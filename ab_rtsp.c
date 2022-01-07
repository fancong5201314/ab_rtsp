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

#include "ab_net/ab_tcp_server.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>

#define T ab_rtsp_t

struct T {
    int method;

    ab_tcp_server_t srv;

    list_t clients_all;
    list_t clients_already;

    pthread_mutex_t mutex;

    bool quit;
    pthread_t rtcp_thd;
};

static unsigned short rtsp_port = 9527;

typedef struct ab_select_args_t {
    int max_fd;
    fd_set rfds;
} ab_select_args_t;

static void get_sock_info(ab_socket_t sock, char *buf, unsigned int buf_size) {
    char addr_buf[64];
    unsigned short port = 0;

    memset(addr_buf, 0, buf_size);
    if (ab_socket_addr(sock, addr_buf, sizeof(addr_buf)) == 0 &&
        ab_socket_port(sock, &port) == 0) {
        snprintf(buf, buf_size, "%s:%d", addr_buf, port);
    }
}

static void process_rtcp_msg(ab_socket_t *sock) {
    assert(sock && *sock);

    char buf[4096];
    memset(buf, 0, sizeof(buf));
    int nread = ab_socket_recv(*sock, buf, sizeof(buf));
    if (nread < 0)
        AB_LOGGER_ERROR("return %d.\n", nread);
    else if (0 == nread) {
        char sock_info[64];
        memset(sock_info, 0, sizeof(sock_info));
        get_sock_info(*sock, sock_info, sizeof(sock_info));
        AB_LOGGER_DEBUG("Close connection[%s]\n", sock_info);
        ab_socket_free(sock);
    } else
        AB_LOGGER_INFO("%s\n", buf);
}

static void check_rtcp_event(void **x, void *user_data) {
    assert(x && *x);
    assert(user_data);

    ab_socket_t sock = (ab_socket_t) *x;
    fd_set *fds = (fd_set *) user_data;

    int fd = ab_socket_fd(sock);
    if (FD_ISSET(fd, fds))
        process_rtcp_msg((ab_socket_t *) x);
}

static void fill_set(void **x, void *user_data) {
    assert(x && *x);
    assert(user_data);

    ab_socket_t sock = (ab_socket_t) *x;
    ab_select_args_t *slt_args = (ab_select_args_t *) user_data;

    int fd = ab_socket_fd((ab_socket_t) *x);
    FD_SET(fd, &slt_args->rfds);
    if (fd > slt_args->max_fd)
        slt_args->max_fd = fd;
}

static list_t clients_clean_up(list_t head) {
    while (head && NULL == head->first) {
        list_t del_node = head;
        head = head->rest;
        FREE(del_node);
    }

    list_t result = head;
    while (head && head->rest) {
        if (NULL == head->rest->first) {
            list_t del_node = head->rest;
            head->rest = head->rest->rest;
            FREE(del_node);
        } else
            head = head->rest;
    }

    return result;
}

static void *rtcp_event_func(void *arg) {
    assert(arg);

    T rtsp = (T) arg;

    ab_select_args_t slt_args;
    struct timeval timeout;
    while (0 == rtsp->quit) {
        FD_ZERO(&slt_args.rfds);
        slt_args.max_fd = -1;

        pthread_mutex_lock(&rtsp->mutex);
        list_map(rtsp->clients_all, fill_set, &slt_args);
        pthread_mutex_unlock(&rtsp->mutex);

        if (-1 == slt_args.max_fd) {
            usleep(50 * 1000);
        } else {
            timeout.tv_sec = 0;
            timeout.tv_usec = 50 * 1000;
            int nums = select(slt_args.max_fd + 1,
                    &slt_args.rfds, NULL, NULL, &timeout);
            if (nums < 0)
                break;
            else if (0 == nums)
                continue;

            pthread_mutex_lock(&rtsp->mutex);
            list_map(rtsp->clients_all, check_rtcp_event, &slt_args.rfds);
            rtsp->clients_all = clients_clean_up(rtsp->clients_all);
            rtsp->clients_already = clients_clean_up(rtsp->clients_already);
            pthread_mutex_unlock(&rtsp->mutex);
        }
    }

    return NULL;
}

static void accept_func(ab_socket_t sock, void *user_data) {
    assert(sock);
    assert(user_data);

    char sock_info[64];
    memset(sock_info, 0, sizeof(sock_info));
    get_sock_info(sock, sock_info, sizeof(sock_info));
    AB_LOGGER_DEBUG("New connection[%s]\n", sock_info);

    T rtsp = (T) user_data;

    pthread_mutex_lock(&rtsp->mutex);
    rtsp->clients_all = list_push(rtsp->clients_all, sock);
    pthread_mutex_unlock(&rtsp->mutex);
}

T ab_rtsp_new(int rtsp_over_method) {
    T rtsp;
    NEW(rtsp);
    assert(rtsp);

    rtsp->method = rtsp_over_method;
    rtsp->srv = ab_tcp_server_new(rtsp_port, accept_func, rtsp);

    rtsp->clients_all = NULL;
    rtsp->clients_already = NULL;

    pthread_mutex_init(&rtsp->mutex, NULL);

    rtsp->quit = false;
    pthread_create(&rtsp->rtcp_thd, NULL, rtcp_event_func, rtsp);

    return rtsp;
}

void ab_rtsp_free(T *rtsp) {
    assert(rtsp && *rtsp);

    (*rtsp)->quit = true;
    pthread_join((*rtsp)->rtcp_thd, NULL);
    pthread_mutex_destroy(&(*rtsp)->mutex);

    while ((*rtsp)->clients_all) {
        ab_socket_t sock = NULL;
        (*rtsp)->clients_all = list_pop((*rtsp)->clients_all, (void **) &sock);
        ab_socket_free(&sock);
    }

    list_free(&(*rtsp)->clients_already);

    ab_tcp_server_free(&(*rtsp)->srv);

    FREE(*rtsp);
}

int ab_rtsp_send(T rtsp, const char *data, unsigned int data_size) {
    return 0;
}
