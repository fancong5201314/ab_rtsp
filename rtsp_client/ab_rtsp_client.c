/*
 * ab_rtsp_client.c
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#include "ab_rtsp_client.h"

#include "ip_check.h"

#include "ab_net/ab_tcp_client.h"
#include "ab_net/ab_udp_client.h"
#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include <arpa/inet.h>

#define RTP_CLIENT_PORT     30001
#define RTCP_CLIENT_PORT    30002

#define T ab_rtsp_client_t

enum ab_rtsp_over_method_t {
    AB_RTSP_OVER_NONE = 0,
    AB_RTSP_OVER_TCP,
    AB_RTSP_OVER_UDP
};

struct T {
    int rtp_over_opt;

    char url[128];
    char srv_addr[64];

    void *user_data;
    void (*callback)(const unsigned char *, unsigned int, void *);

    ab_tcp_client_t tcp_client;

    unsigned short udp_rtp_srv_port;
    unsigned short udp_rtcp_srv_port;
    ab_udp_client_t udp_rtp_client;
    ab_udp_client_t udp_rtcp_client;

    unsigned int seq;
    char session[16];

    bool quit;
    pthread_t child_thd;
};

static bool parse_rtsp_addr(const char *rtsp_addr, 
    char *host_buf, unsigned int host_buf_size, unsigned short *port);
static void *child_thd_callback(void *arg);

static bool send_cmd_options(T t);
static bool send_cmd_describe(T t);
static bool send_cmd_setup(T t);
static bool send_cmd_play(T t);
static bool send_cmd_teardown(T t);

T ab_rtsp_client_new(int rtp_over_opt, const char *url,
    void (*cb)(const unsigned char *, unsigned int, void *), void *user_data) {
    char host_buf[64] = {0};
    unsigned short port = 0;

    if (!parse_rtsp_addr(url, host_buf, sizeof(host_buf), &port)) {
        return NULL;
    }

    if (ip_check(host_buf) != IP_VERSION_4 || 0 == port) {
        return NULL;
    }

    T result;
    NEW(result);

    result->rtp_over_opt = rtp_over_opt;
    int url_len = strlen(url);
    if (url_len < sizeof(result->url)) {
        memcpy(result->url, url, url_len);
    }

    int host_len = strlen(host_buf);
    memset(result->srv_addr, 0, sizeof(result->srv_addr));
    if (host_len < sizeof(result->srv_addr)) {
        memcpy(result->srv_addr, host_buf, host_len);
    }

    result->user_data = user_data;
    result->callback = cb;

    result->tcp_client = ab_tcp_client_new(host_buf, port);

    if (AB_RTSP_OVER_UDP == result->rtp_over_opt) {
        result->udp_rtp_client = ab_udp_client_new(RTP_CLIENT_PORT);
        result->udp_rtcp_client = ab_udp_client_new(RTCP_CLIENT_PORT);
    }

    result->seq = 1;
    memset(result->session, 0, sizeof(result->session));

    if (result->tcp_client != NULL) {
        send_cmd_options(result);
        send_cmd_describe(result);
        send_cmd_setup(result);
        send_cmd_play(result);
    }

    result->quit = false;
    pthread_create(&result->child_thd, NULL, child_thd_callback, result);

    return result;
}

void ab_rtsp_client_free(T *t) {
    assert(t && *t);

    send_cmd_teardown(*t);
    sleep(1);

    (*t)->quit = true;
    pthread_join((*t)->child_thd, NULL);

    if (AB_RTSP_OVER_UDP == (*t)->rtp_over_opt) {
        ab_udp_client_free(&(*t)->udp_rtp_client);
        ab_udp_client_free(&(*t)->udp_rtcp_client);
    }

    ab_tcp_client_free(&(*t)->tcp_client);

    FREE(*t);
}

bool parse_rtsp_addr(const char *rtsp_addr, 
    char *host_buf, unsigned int host_buf_size, unsigned short *port) {
    char *pos_start = strstr(rtsp_addr, "rtsp://");
    if (NULL == pos_start) {
        return false;
    }

    pos_start += 7;

    char *pos_end = strchr(pos_start, '/');
    char buf[128] = {0};
    if (pos_end) {
        memcpy(buf, pos_start, pos_end - pos_start);
    } else {
        strcpy(buf, pos_start);
    }

    pos_end = strchr(buf, ':');
    if (pos_end) {
        if (host_buf && host_buf_size > pos_end - buf) {
            memcpy(host_buf, buf, pos_end - buf);
        } else {
            return false;
        }

        char port_buf[8] = {0};
        strcpy(port_buf, pos_end + 1);
        if (port) {
            *port = atoi(port_buf);
        } else {
            return false;
        }
    } else {
        unsigned int buf_len = strlen(buf);
        if (host_buf && host_buf_size > buf_len) {
            strcpy(host_buf, buf);
        }

        if (port) {
            *port = 554;
        }
    }

    return true;
}

bool send_cmd_options(T t) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "OPTIONS %s RTSP/1.0\r\n"
        "CSeq: %u\r\n\r\n", t->url, ++t->seq);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    printf("%s\n",buf);
    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf), -1) <= 0) {
        return false;
    }

    printf("%s\n", buf);
    return true;
}

bool send_cmd_describe(T t) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "DESCRIBE %s RTSP/1.0\r\n"
        "CSeq: %u\r\n"
        "Accept: application/sdp\r\n\r\n", t->url, ++t->seq);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    printf("%s\n", (const char *) buf);

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf), -1) <= 0) {
        return false;
    }

    printf("%s\n", buf);
    return true;
}

bool send_cmd_setup(T t) {
    assert(t);

    char buf[1024];
    if (AB_RTSP_OVER_TCP == t->rtp_over_opt) {
        snprintf(buf, sizeof(buf), 
            "SETUP %s RTSP/1.0\r\n"
            "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
            "CSeq: %u\r\n\r\n", t->url, ++t->seq);
    } else if (AB_RTSP_OVER_UDP == t->rtp_over_opt) {
        snprintf(buf, sizeof(buf), 
            "SETUP %s RTSP/1.0\r\n"
            "Transport: RTP/AVP;unicast;client_port=%u-%u\r\n"
            "CSeq: %u\r\n\r\n", t->url, RTP_CLIENT_PORT, RTCP_CLIENT_PORT, ++t->seq);
    } else {
        return false;
    }

    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    printf("%s\n", buf);

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf), -1) <= 0) {
        return false;
    }

    printf("%s\n", buf);
    char *pos = strstr(buf, "server_port");
    if (pos != NULL) {
        sscanf(pos, "server_port=%hu-%hu\r\n", 
            &t->udp_rtp_srv_port, &t->udp_rtcp_srv_port);
    }

    pos = strstr(buf, "Session:");
    if (pos != NULL) {
        sscanf(pos, "Session: %s\r\n", t->session);
    }

    return true;
}

bool send_cmd_play(T t) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "PLAY %s RTSP/1.0\r\n"
        "CSeq: %u\r\n"
        "Session: %s\r\n"
        "Range: npt=0.000-\r\n\r\n", t->url, ++t->seq, t->session);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    printf("%s\n", (const char *) buf);

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf), -1) <= 0) {
        return false;
    }

    printf("%s\n", buf);
    return true;
}

bool send_cmd_teardown(T t) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "TEARDOWN %s RTSP/1.0\r\n"
        "CSeq: %u\r\n"
        "Session: %s\r\n\r\n", t->url, ++t->seq, t->session);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    printf("%s\n", (const char *) buf);

    return true;
}

static void process_rtp_over_tcp(T t) {
    unsigned int recv_buf_size = 512 * 1024;
    unsigned char *recv_buf = (unsigned char *) ALLOC(recv_buf_size);

    unsigned int start_pos = 0;

    while (!t->quit) {
        int nrecv = ab_tcp_client_recv(t->tcp_client, recv_buf, recv_buf_size, -1);
        if (nrecv <= 0) {
            continue;
        }

        start_pos = 0;
        while (start_pos < nrecv) {
            if (recv_buf[start_pos] != 0x24) {
                break;
            }

            unsigned short rtp_len = ntohs(*(unsigned short *)(recv_buf + start_pos + 2));

            unsigned char nal_type = recv_buf[start_pos + 16] & 0x1F;
            unsigned int slice = 0x1000000;
            if  (0x1C == nal_type || 0x1D == nal_type) {
                unsigned char flag = recv_buf[start_pos + 17] & 0xE0;
                if (0x80 == flag) { // start
                    if (t->callback) {
                        unsigned char nal_fua = (recv_buf[start_pos + 16] & 0xE0) | (recv_buf[start_pos + 17] & 0x1F);
                        t->callback((unsigned char *) &slice, sizeof(slice), t->user_data);
                        t->callback(&nal_fua, 1, t->user_data);
                    }
                }

                if (t->callback) {
                    t->callback(recv_buf + start_pos + 18, rtp_len - 14, t->user_data);
                }
            } else {
                if (t->callback) {
                    t->callback((unsigned char *) &slice, sizeof(slice), t->user_data);
                    t->callback(recv_buf + start_pos + 16, rtp_len - 12, t->user_data);
                }
            } 

            start_pos += rtp_len + 4;
        }
    }

    FREE(recv_buf);
}

static void process_rtp_over_udp(T t) {
    unsigned int recv_buf_size = 10 * 1024;
    unsigned char *recv_buf = (unsigned char *) ALLOC(recv_buf_size);

    char recv_addr[64];
    unsigned short recv_port = 0;

    while (!t->quit) {
        memset(recv_addr, 0, sizeof(recv_addr));
        recv_port = 0;

        int nrecv = ab_udp_client_recv(t->udp_rtp_client, 
            recv_addr, sizeof(recv_addr), &recv_port, recv_buf, recv_buf_size, -1);
        if (nrecv <= 0) {
            continue;
        }

        if (strcmp(recv_addr, t->srv_addr) != 0 || 
            recv_port != t->udp_rtp_srv_port) {
            continue;
        }

        unsigned char nal_type = recv_buf[12] & 0x1F;
        unsigned int slice = 0x1000000;
        if  (0x1C == nal_type || 0x1D == nal_type) {
            unsigned char flag = recv_buf[13] & 0xE0;
            if (0x80 == flag) {
                if (t->callback) {
                    unsigned char nal_fua = (recv_buf[12] & 0xE0) | (recv_buf[13] & 0x1F);
                    t->callback((unsigned char *) &slice, sizeof(slice), t->user_data);
                    t->callback(&nal_fua, 1, t->user_data);
                }
            }

            if (t->callback) {
                t->callback(recv_buf + 14, nrecv - 14, t->user_data);
            }
        } else {
            if (t->callback) {
                t->callback((unsigned char *) &slice, sizeof(slice), t->user_data);
                t->callback(recv_buf + 12, nrecv - 12, t->user_data);
            }
        }
    }

    FREE(recv_buf);
}

void *child_thd_callback(void *arg) {
    assert(arg);

    T t = (T) arg;

    if (AB_RTSP_OVER_TCP == t->rtp_over_opt) {
        process_rtp_over_tcp(t);
    } else if (AB_RTSP_OVER_UDP == t->rtp_over_opt) {
        process_rtp_over_udp(t);
    }

    return NULL;
}