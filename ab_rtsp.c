/*
 * ab_rtsp.c
 *
 *  Created on: 2022年1月7日
 *      Author: ljm
 */

#include "ab_rtsp.h"
#include "ab_rtp_def.h"

#include "ab_base/ab_list.h"
#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include "ab_log/ab_logger.h"

#include "ab_net/ab_socket.h"
#include "ab_net/ab_tcp_server.h"
#include "ab_net/ab_udp_server.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>

#include <arpa/inet.h>

#define T ab_rtsp_t

static unsigned short rtp_udp_server_port = 20001;
static unsigned short rtcp_udp_server_port = 20002;
static unsigned short rtsp_port = 554;

enum ab_rtsp_over_method_t {
    AB_RTSP_OVER_NONE = 0,
    AB_RTSP_OVER_TCP,
    AB_RTSP_OVER_UDP
};

typedef struct ab_rtsp_buffer_t {
    void *data;
    int size;
    int used;
} ab_rtsp_buffer_t;

typedef struct ab_select_args_t {
    int max_fd;
    fd_set rfds;
} ab_select_args_t;

typedef struct ab_rtsp_client_t *ab_rtsp_client_t;
struct ab_rtsp_client_t {
    ab_socket_t sock;
    ab_socket_t rtp_sock;

    int status;
    int method;

    unsigned short rtp_chn_port;
    unsigned short rtcp_chn_port;
};

struct T {
    ab_tcp_server_t tcp_srv;
    list_t clients;

    ab_udp_server_t udp_rtp_srv;
    ab_udp_server_t udp_rtcp_srv;

    bool quit;
    pthread_mutex_t mutex;
    pthread_t rtsp_thd;

    ab_rtsp_buffer_t buffer;
    ab_rtp_packet_t *rtp_pkt;
    unsigned int rtp_pkt_len;
};

static void get_sock_info(ab_socket_t sock,
    char *buf, unsigned int buf_size) {
    char addr_buf[64];
    unsigned short port = 0;

    memset(addr_buf, 0, buf_size);
    if (ab_socket_addr(sock, addr_buf, sizeof(addr_buf)) == 0 &&
        ab_socket_port(sock, &port) == 0) {
        snprintf(buf, buf_size, "%s:%d", addr_buf, port);
    }
}

static int handle_cmd_options(char *buf, unsigned int buf_size,
        unsigned int cseq) {
    snprintf(buf, buf_size, 
        "RTSP/1.0 200 OK\r\n"
        "CSeq: %u\r\n"
        "Public: OPTIONS, DESCRIBE, SETUP, "
        "PLAY, TEARDOWN\r\n\r\n", cseq);
    return strlen(buf);
}

static int handle_cmd_describe(char *buf, unsigned int buf_size,
    const char *url, unsigned int cseq) {
    char sdp[256];
    char local_ip[32];
    sscanf(url, "rtsp://%[^:]:", local_ip);

    snprintf(sdp, sizeof(sdp), 
        "v=0\r\n"
        "o=- 9%ld 1 IN IP4 %s\r\n"
        "t=0 0\r\n"
        "a=control:*\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=control:track0\r\n", time(NULL), local_ip);

    snprintf(buf, buf_size, 
        "RTSP/1.0 200 OK\r\n"
        "CSeq: %u\r\n"
        "Content-Base: %s\r\n"
        "Content-type: application/sdp\r\n"
        "Content-length: %lu\r\n\r\n"
        "%s", cseq, url, strlen(sdp), sdp);
    return strlen(buf);
}

static int handle_cmd_setup(char *buf, unsigned int buf_size,
    unsigned int cseq, int rtsp_over,
    unsigned short rtp, unsigned short rtcp) {
   if (AB_RTSP_OVER_UDP == rtsp_over) {
        snprintf(buf, buf_size, 
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %u\r\n"
            "Transport: RTP/AVP;unicast;"
            "client_port=%u-%u;server_port=%u-%u\r\n"
            "Session: 66334873\r\n\r\n",
            cseq, rtp, rtcp, rtp_udp_server_port, rtcp_udp_server_port);
   } else if (AB_RTSP_OVER_TCP == rtsp_over) {
        snprintf(buf, buf_size, 
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %u\r\n"
            "Transport: RTP/AVP/TCP;unicast;"
            "interleaved=%u-%u\r\n"
            "Session: 66334873\r\n\r\n", cseq, rtp, rtcp);
    } else {
        buf[0] = '\0';
    }

    return strlen(buf);
}

static int handle_cmd_play(char *buf, unsigned int buf_size,
    unsigned int cseq) {
    snprintf(buf, buf_size, 
        "RTSP/1.0 200 OK\r\n"
        "CSeq: %u\r\n"
        "Range: npt=0.000-\r\n"
        "Session: 66334873; timeout=60\r\n\r\n", cseq);
    return strlen(buf);
}

static int handle_cmd_teardown(char *buf, unsigned int buf_size, 
    unsigned int cseq) {
    snprintf(buf, buf_size,
            "RTSP/1.0 551 Option not supported\r\n"
            "CSeq: %u\r\n"
            "Session: 66334873\r\n\r\n", cseq);
    return strlen(buf);
}

static int handle_cmd_not_supported(char *buf, unsigned int buf_size,
    unsigned int cseq) {
    snprintf(buf, buf_size,
            "RTSP/1.0 551 Option not supported\r\n"
            "CSeq: %u\r\n"
            "Session: 66334873\r\n\r\n", cseq);
    return strlen(buf);
}

static int process_client_request(ab_rtsp_client_t client, 
    const char *request, unsigned int request_len, 
    char *response, unsigned int response_size) {
    char method[16];
    char url[128];
    char version[16];
    memset(method, 0, sizeof(method));
    memset(url, 0, sizeof(url));
    memset(version, 0, sizeof(version));

    sscanf(request, "%s %s %s\r\n", method, url, version);

    unsigned int cseq = 0;
    const char *line = strstr(request, "CSeq");
    if (NULL == line)
        return 0;
    else
        sscanf(line, "CSeq: %u\r\n", &cseq);

    int len = 0;
    if (strcmp(method, "OPTIONS") == 0) {
        len = handle_cmd_options(response, response_size, cseq);
    } else if (strcmp(method, "DESCRIBE") == 0) {
        len = handle_cmd_describe(response, response_size, url, cseq);
    } else if (strcmp(method, "SETUP") == 0) {
        line = strstr(request, "Transport");

        if (strstr(line, "RTP/AVP/TCP") != NULL)
            client->method = AB_RTSP_OVER_TCP;
        else if (strstr(line, "RTP/AVP") != NULL)
            client->method = AB_RTSP_OVER_UDP;
        else
            return 0;

        if (AB_RTSP_OVER_UDP == client->method)
            sscanf(line, "Transport: RTP/AVP;unicast;client_port=%hu-%hu\r\n", 
                &client->rtp_chn_port, &client->rtcp_chn_port);
        else if (AB_RTSP_OVER_TCP == client->method)
            sscanf(line, "Transport: RTP/AVP/TCP;unicast;interleaved=%hu-%hu\r\n", 
                &client->rtp_chn_port, &client->rtcp_chn_port);

        len = handle_cmd_setup(response, response_size, cseq, client->method, 
            client->rtp_chn_port, client->rtcp_chn_port);
    } else if (strcmp(method, "PLAY") == 0) {
        len = handle_cmd_play(response, response_size, cseq);
        client->status = 1;
    } else {
        len = handle_cmd_not_supported(response, response_size, cseq);
    }

    return len;
}

static void recv_client_msg(ab_rtsp_client_t client) {
    assert(client);

    char request[4096];
    memset(request, 0, sizeof(request));
    int nread = ab_socket_recv(client->sock, (unsigned char *) request, sizeof(request));
    if (nread < 0) {
        AB_LOGGER_ERROR("return %d.\n", nread);
    } else if (0 == nread) {
        char sock_info[64];
        memset(sock_info, 0, sizeof(sock_info));
        get_sock_info(client->sock, sock_info, sizeof(sock_info));
        AB_LOGGER_DEBUG("Close connection[%s]\n", sock_info);
        ab_socket_free(&client->sock);
    } else {
        AB_LOGGER_DEBUG("request:\n%s\n", request);
        const unsigned int response_size = 1024;
        char response[response_size];
        memset(response, 0, response_size);
        int len = process_client_request(client, request, nread,
            response, response_size);
        AB_LOGGER_DEBUG("response:\n%s\n", response);
        if (len > 0)
            ab_socket_send(client->sock, (unsigned char *) response, len);
    }
}

static void check_rtsp_event(void **x, void *user_data) {
    assert(x && *x);
    assert(user_data);

    ab_rtsp_client_t client = (ab_rtsp_client_t) *x;
    ab_select_args_t *slt_args = (ab_select_args_t *) user_data;

    int fd = ab_socket_fd(client->sock);
    if (FD_ISSET(fd, &slt_args->rfds))
        recv_client_msg(client);
}

static void fill_set(void **x, void *user_data) {
    assert(x && *x);
    assert(user_data);

    ab_rtsp_client_t client = (ab_rtsp_client_t) *x;
    ab_select_args_t *slt_args = (ab_select_args_t *) user_data;

    int fd = ab_socket_fd(client->sock);
    FD_SET(fd, &slt_args->rfds);
    if (fd > slt_args->max_fd)
        slt_args->max_fd = fd;
}

static list_t clients_clean_up(list_t head) {
    while (head) {
        ab_rtsp_client_t client = head->first;
        if (NULL == client->sock) {
            list_t del_node = head;
            head = head->rest;
            FREE(del_node->first);
            FREE(del_node);
        } else
            break;
    }

    list_t result = head;
    while (head && head->rest) {
        ab_rtsp_client_t client = head->rest->first;
        if (NULL == client->sock) {
            list_t del_node = head->rest;
            head->rest = head->rest->rest;
            FREE(del_node->first);
            FREE(del_node);
        } else
            head = head->rest;
    }

    return result;
}

static void *rtsp_event_start_routine(void *arg) {
    assert(arg);

    T rtsp = (T) arg;

    while (!rtsp->quit) {
        ab_select_args_t slt_args;
        FD_ZERO(&slt_args.rfds);
        slt_args.max_fd = -1;

        pthread_mutex_lock(&rtsp->mutex);
        list_map(rtsp->clients, fill_set, &slt_args);
        pthread_mutex_unlock(&rtsp->mutex);

        if (-1 == slt_args.max_fd) {
            usleep(50 * 1000);
        } else {
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 50 * 1000;
            int nums = select(slt_args.max_fd + 1, &slt_args.rfds, NULL, NULL, &timeout);
            if (nums < 0)
                break;
            else if (0 == nums)
                continue;

            pthread_mutex_lock(&rtsp->mutex);
            list_map(rtsp->clients, check_rtsp_event, &slt_args);
            rtsp->clients = clients_clean_up(rtsp->clients);
            pthread_mutex_unlock(&rtsp->mutex);
        }
    }

    return NULL;
}

static void accept_func(void *sock, void *user_data) {
    assert(sock);
    assert(user_data);

    char sock_info[64];
    memset(sock_info, 0, sizeof(sock_info));
    get_sock_info(sock, sock_info, sizeof(sock_info));
    AB_LOGGER_DEBUG("New connection[%s]\n", sock_info);

    T rtsp = (T) user_data;

    ab_rtsp_client_t new_client;
    NEW(new_client);

    new_client->sock = sock;
    new_client->status = 0;
    new_client->method = AB_RTSP_OVER_NONE;

    pthread_mutex_lock(&rtsp->mutex);
    rtsp->clients = list_push(rtsp->clients, new_client);
    pthread_mutex_unlock(&rtsp->mutex);
}

static void rtp_sender_apply(void **x, void *user_data) {
    assert(x && *x);
    assert(user_data);

    ab_rtsp_client_t client = (ab_rtsp_client_t) *x;
    ab_rtsp_buffer_t *buf = (ab_rtsp_buffer_t *) user_data;

    if (client->status) {
        if (AB_RTSP_OVER_UDP == client->method) {
            char addr_buf[32];
            ab_socket_addr(client->sock, addr_buf, sizeof(addr_buf));
            ab_socket_udp_send(client->rtp_sock, addr_buf, client->rtp_chn_port,
                buf->data + 4, buf->used - 4);
        } else if (AB_RTSP_OVER_TCP == client->method)
            ab_socket_send(client->sock, buf->data, buf->used);
    }
}

static void fill_rtp_packet_tcp_header(unsigned char *header, unsigned int header_size,
                                       unsigned int data_size) {
    if (header_size < 4)
        return;

    header[0]    = 0x24;
    header[1]    = 0x0;
    header[2]    = (data_size & 0xff00) >> 8;
    header[3]    = data_size & 0xff;
}

static void rtp_header_hton(ab_rtp_header_t *rtp_header) {
    rtp_header->seq         = htons(rtp_header->seq);
    rtp_header->timestamp   = htonl(rtp_header->timestamp);
    rtp_header->ssrc        = htonl(rtp_header->ssrc);
}

static void rtp_header_ntoh(ab_rtp_header_t *rtp_header) {
    rtp_header->seq         = ntohs(rtp_header->seq);
    rtp_header->timestamp   = ntohl(rtp_header->timestamp);
    rtp_header->ssrc        = ntohl(rtp_header->ssrc);
}

static void rtp_sender_func(T rtsp, const char *data, unsigned int data_size) {
    assert(rtsp);
    assert(data);
    assert(data_size > 0);

    pthread_mutex_lock(&rtsp->mutex);
    if (list_length(rtsp->clients) == 0) {
        pthread_mutex_unlock(&rtsp->mutex);
        return;
    }
    pthread_mutex_unlock(&rtsp->mutex);

    int nalu_type = data[0];
    ab_rtsp_buffer_t buf;
    if (data_size < RTP_MAX_PACKET_SIZE) {
        fill_rtp_packet_tcp_header(rtsp->rtp_pkt->header, sizeof(rtsp->rtp_pkt->header),
            data_size + sizeof(ab_rtp_header_t));
        rtp_header_hton(&rtsp->rtp_pkt->rtp_header);

        memcpy(rtsp->rtp_pkt->payload, data, data_size);
        buf.data = rtsp->rtp_pkt;
        buf.size = buf.used = data_size + sizeof(ab_rtp_header_t) + 4;

        pthread_mutex_lock(&rtsp->mutex);
        list_map(rtsp->clients, rtp_sender_apply, &buf);
        pthread_mutex_unlock(&rtsp->mutex);

        rtp_header_ntoh(&rtsp->rtp_pkt->rtp_header);

        rtsp->rtp_pkt->rtp_header.seq++;
    } else {
        unsigned send_data_size = data_size - 1;

        int pkt_num = send_data_size / RTP_MAX_PACKET_SIZE;
        if (send_data_size % RTP_MAX_PACKET_SIZE != 0)
            pkt_num++;

        for (int i = 0; i < pkt_num; i++) {
            unsigned short rtp_pkt_size = 0;
            if (i < pkt_num - 1 || send_data_size % RTP_MAX_PACKET_SIZE == 0)
                rtp_pkt_size = RTP_MAX_PACKET_SIZE + sizeof(ab_rtp_header_t) + 2;
            else
                rtp_pkt_size = (send_data_size % RTP_MAX_PACKET_SIZE) + sizeof(ab_rtp_header_t) + 2;

            fill_rtp_packet_tcp_header(rtsp->rtp_pkt->header, sizeof(rtsp->rtp_pkt->header),
                    rtp_pkt_size);

            rtp_header_hton(&rtsp->rtp_pkt->rtp_header);

            rtsp->rtp_pkt->payload[0] = (nalu_type & 0x60) | 0x1c;
            rtsp->rtp_pkt->payload[1] = nalu_type & 0x1f;

            if (0 == i)
                rtsp->rtp_pkt->payload[1] |= 0x80;
            else if (i == pkt_num - 1)
                rtsp->rtp_pkt->payload[1] |= 0x40;

            unsigned int pos = i * RTP_MAX_PACKET_SIZE + 1;
            if (i < pkt_num - 1 || send_data_size % RTP_MAX_PACKET_SIZE == 0)
                memcpy(rtsp->rtp_pkt->payload + 2, data + pos, RTP_MAX_PACKET_SIZE);
            else
                memcpy(rtsp->rtp_pkt->payload + 2, data + pos, send_data_size % RTP_MAX_PACKET_SIZE);

            buf.data = rtsp->rtp_pkt;
            buf.size = buf.used = rtp_pkt_size + 4;

            pthread_mutex_lock(&rtsp->mutex);
            list_map(rtsp->clients, rtp_sender_apply, &buf);
            pthread_mutex_unlock(&rtsp->mutex);

            rtp_header_ntoh(&rtsp->rtp_pkt->rtp_header);

            rtsp->rtp_pkt->rtp_header.seq++;
        }
    }

    if ((nalu_type & 0x1f) != 7 && (nalu_type & 0x1f) != 8)
        rtsp->rtp_pkt->rtp_header.timestamp += 90000 / 25;
}

static bool start_code3(const char *data, unsigned int data_size) {
    if (data_size >= 3 &&
        0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2])
        return true;
    return false;
}

static bool start_code4(const char *data, unsigned int data_size) {
    if (data_size >= 4 &&
        0x00 == data[0] && 0x00 == data[1] &&
        0x00 == data[2] && 0x01 == data[3])
        return true;
    return false;
}

static int find_start_code(const char *data, unsigned int data_size) {
    if (data_size < 4)
        return -1;

    unsigned int i;
    for (i = 0; i < data_size - 4; i++)
        if (start_code3(data + i, 3) || start_code4(data + i, 4))
            break;

    if ((i < data_size - 4) || start_code3(data + i, 3))
        return i;
    return -1;
}

T ab_rtsp_new() {
    T rtsp;
    NEW(rtsp);
    assert(rtsp);

    rtsp->tcp_srv = ab_tcp_server_new(rtsp_port, accept_func, rtsp);

    rtsp->udp_rtp_srv = ab_udp_server_new(rtp_udp_server_port);
    rtsp->udp_rtcp_srv = ab_udp_server_new(rtcp_udp_server_port);

    rtsp->clients = NULL;

    pthread_mutex_init(&rtsp->mutex, NULL);

    rtsp->quit = false;
    pthread_create(&rtsp->rtsp_thd, NULL, rtsp_event_start_routine, rtsp);

    rtsp->buffer.size   = 1024 * 1024;
    rtsp->buffer.used   = 0;
    rtsp->buffer.data   = ALLOC(rtsp->buffer.size);

    rtsp->rtp_pkt =
        (ab_rtp_packet_t *) ALLOC(sizeof(ab_rtp_packet_t) + RTP_MAX_PACKET_SIZE);

    rtsp->rtp_pkt->rtp_header.csrc_len      = 0;
    rtsp->rtp_pkt->rtp_header.extension     = 0;
    rtsp->rtp_pkt->rtp_header.padding       = 0;
    rtsp->rtp_pkt->rtp_header.version       = RTP_VERSION;
    rtsp->rtp_pkt->rtp_header.payload_type  = RTP_PAYLOAD_TYPE_H264;
    rtsp->rtp_pkt->rtp_header.marker        = 0;
    rtsp->rtp_pkt->rtp_header.seq           = 0;
    rtsp->rtp_pkt->rtp_header.timestamp     = 0;
    rtsp->rtp_pkt->rtp_header.ssrc          = 0x88923423;

    rtsp->rtp_pkt_len                       = 0;

    return rtsp;
}

void ab_rtsp_free(T *rtsp) {
    assert(rtsp && *rtsp);

    (*rtsp)->quit = true;

    pthread_join((*rtsp)->rtsp_thd, NULL);
    pthread_mutex_destroy(&(*rtsp)->mutex);

    FREE((*rtsp)->rtp_pkt);
    FREE((*rtsp)->buffer.data);

    while ((*rtsp)->clients) {
        ab_rtsp_client_t client;
        (*rtsp)->clients = list_pop((*rtsp)->clients, (void **) &client);
        ab_socket_free(&(client->sock));
        FREE(client);
    }

    ab_udp_server_free(&(*rtsp)->udp_rtcp_srv);
    ab_udp_server_free(&(*rtsp)->udp_rtp_srv);
    ab_tcp_server_free(&(*rtsp)->tcp_srv);

    FREE(*rtsp);
}

int ab_rtsp_send(T rtsp, const char *data, unsigned int data_size) {
    if (NULL == data || 0 == data_size)
        if (rtsp->buffer.used > 0) {
            int first_start_code_pos = find_start_code(rtsp->buffer.data, rtsp->buffer.used);
            if (0 == first_start_code_pos) {
                int start_code = 0;
                if (start_code3(rtsp->buffer.data, rtsp->buffer.used))
                    start_code = 3;
                else if (start_code4(rtsp->buffer.data, rtsp->buffer.used))
                    start_code = 4;

                rtp_sender_func(rtsp, rtsp->buffer.data + start_code, rtsp->buffer.used - start_code);
                rtsp->buffer.used = 0;
            }

        return 0;
    }

    if (rtsp->buffer.size - rtsp->buffer.used >= data_size) {
        memcpy(rtsp->buffer.data + rtsp->buffer.used, data, data_size);
        rtsp->buffer.used += data_size;
    } else {
        AB_LOGGER_WARN("Not enough spaces.\n");
        return 0;
    }

    unsigned int start_pos = 0;
    while (start_pos < rtsp->buffer.used) {
        int first_start_code_pos =
            find_start_code(rtsp->buffer.data + start_pos, rtsp->buffer.used - start_pos);
        if (0 == first_start_code_pos) {
            unsigned int start_code = 0;
            if (start_code3(rtsp->buffer.data + start_pos, rtsp->buffer.used - start_pos))
                start_code = 3;
            else if (start_code4(rtsp->buffer.data + start_pos, rtsp->buffer.used - start_pos))
                start_code = 4;
            else
                continue;

            int next_start_code_pos =
                find_start_code(rtsp->buffer.data + start_pos + start_code,
                                rtsp->buffer.used - start_pos - start_code);
            if (-1 == next_start_code_pos) {
                if (start_pos != 0) {
                    rtsp->buffer.used -= start_pos;
                    memcpy(rtsp->buffer.data, rtsp->buffer.data + start_pos, rtsp->buffer.used);
                }
                break;
            } else {
                rtp_sender_func(rtsp, rtsp->buffer.data + start_pos + start_code, next_start_code_pos);
                start_pos += start_code + next_start_code_pos;
            }
        } else
            break;
    }

    return data_size;
}
