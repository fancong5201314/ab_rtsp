/*
 * ab_rtsp_client.c
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#include "ab_rtsp_client.h"

#include "ip_check.h"

#include "ab_net/ab_tcp_client.h"
#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include <arpa/inet.h>

#define T ab_rtsp_client_t
struct T {
    void *user_data;
    void (*callback)(const unsigned char *, unsigned int, void *);

    ab_tcp_client_t tcp_client;

    unsigned int seq;
    unsigned long long session;

    unsigned int buf_size;
    unsigned int buf_used;
    unsigned char *buf;

    bool quit;
    pthread_t child_thd;
};

static bool parse_rtsp_addr(const char *rtsp_addr, 
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

static bool send_cmd_options(T t, const char *url) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "OPTIONS %s RTSP/1.0\r\n"
        "CSeq: %u\r\n\r\n", url, t->seq);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf)) <= 0) {
        return false;
    }

    printf("%s\n", buf);
    return true;
}

static bool send_cmd_describe(T t, const char *url) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "DESCRIBE %s RTSP/1.0\r\n"
        "CSeq: %u\r\n"
        "Accept: application/sdp\r\n\r\n", url, t->seq);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf)) <= 0) {
        return false;
    }

    printf("%s\n", buf);
    return true;
}

static bool send_cmd_setup(T t, const char *url) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "SETUP %s RTSP/1.0\r\n"
        "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
        "CSeq: %u\r\n\r\n", url, t->seq);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf)) <= 0) {
        return false;
    }

    printf("%s\n", (const char *) buf);
    char *pos = strstr(buf, "Session");
    sscanf(pos, "Session: %llu\r\n", &t->session);
    return true;
}

static bool send_cmd_play(T t, const char *url) {
    assert(t);

    char buf[1024];
    snprintf(buf, sizeof(buf), 
        "PLAY %s RTSP/1.0\r\n"
        "CSeq: %u\r\n"
        "Session: %llu\r\n"
        "Range: npt=0.000-\n\r\n\r\n", url, t->seq, t->session);
    unsigned int buf_len =strlen(buf);
    if (ab_tcp_client_send(t->tcp_client, (unsigned char *) buf, buf_len) <= 0) {
        return false;
    }

    memset(buf, 0, sizeof(buf));
    if (ab_tcp_client_recv(t->tcp_client, (unsigned char *) buf, sizeof(buf)) <= 0) {
        return false;
    }

    printf("%s\n", (const char *) buf);
    return true;
}

static void *child_thd_callback(void *arg) {
    assert(arg);

    T t = (T) arg;

    unsigned int recv_buf_size = 512 * 1024;
    unsigned char *recv_buf = (unsigned char *) ALLOC(recv_buf_size);

    unsigned int start_pos = 0;

    while (!t->quit) {
        int nrecv = ab_tcp_client_recv(t->tcp_client, recv_buf, recv_buf_size);
        if (nrecv > 0) {
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
    }

    FREE(recv_buf);

    return NULL;
}

T ab_rtsp_client_new(const char *url,
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

    result->user_data = user_data;
    result->callback = cb;

    result->buf_size = 512 * 1024;
    result->buf_used = 0;
    result->buf = (unsigned char *) ALLOC(result->buf_size);

    result->tcp_client = ab_tcp_client_new(host_buf, port);

    if (result->tcp_client) {
        send_cmd_options(result, url);
        send_cmd_describe(result, url);
        send_cmd_setup(result, url);
        send_cmd_play(result, url);
    }

    result->quit = false;
    pthread_create(&result->child_thd, NULL, child_thd_callback, result);

    return result;
}

void ab_rtsp_client_free(T *t) {
    assert(t && *t);
    (*t)->quit = true;
    pthread_join((*t)->child_thd, NULL);
    ab_tcp_client_free(&(*t)->tcp_client);

    FREE((*t)->buf);
    (*t)->buf_size = 0;
    (*t)->buf_used = 0;

    FREE(*t);
}