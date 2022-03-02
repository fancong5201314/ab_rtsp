/*
 * ab_rtsp_pull_stream.c
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#include "ab_rtsp_pull_stream.h"

#include "ip_check.h"

#include "ab_net/ab_tcp_client.h"
#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define T ab_rtsp_pull_stream_t
struct T {
    void *user_data;
    void (*callback)(const unsigned char *, unsigned int, void *);

    ab_tcp_client_t tcp_client;

    unsigned int seq;
    unsigned long long session;

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

    ab_rtsp_pull_stream_t t = (ab_rtsp_pull_stream_t) arg;
    unsigned int data_size = 512 * 1024;
    unsigned char *data = (unsigned char *) malloc(data_size);

    while (!t->quit) {
        int nrecv = ab_tcp_client_recv(t->tcp_client, data, data_size);
        if (nrecv > 0 && nrecv <= data_size) {
            if (t->callback) {
                t->callback(data, nrecv, t->user_data);
            }
        }
    }

    free(data);
    data = NULL;

    return NULL;
}

T ab_rtsp_pull_stream_new(const char *url,
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

void ab_rtsp_pull_stream_free(T *t) {
    assert(t && *t);
    (*t)->quit = true;
    pthread_join((*t)->child_thd, NULL);
    ab_tcp_client_free(&(*t)->tcp_client);
    FREE(*t);
}