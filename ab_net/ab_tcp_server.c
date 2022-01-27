/*
 * ab_tcp_server.c
 *
 *  Created on: 2021年12月26日
 *      Author: ljm
 */

#include "ab_tcp_server.h"

#include "ab_socket.h"

#include "ab_base/ab_assert.h"
#include "ab_base/ab_mem.h"

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef WIN32
#else
#include <sys/select.h>
#endif

#define T   ab_tcp_server_t

struct T {
    ab_socket_t sock;
    void (*new_conn_cb)(void *sock, void *user_data);
    void *user_data;
    bool quit;
    pthread_t acceptor_thd;

    int error_num;
};

static void *accept_event_loop_func(void *arg);

T ab_tcp_server_new(unsigned short port,
    void new_conn_cb(void *sock, void *user_data), void *user_data) {
    assert(port != 0);

    T tcp_server;
    NEW(tcp_server);
    assert(tcp_server);
    tcp_server->sock = ab_socket_new(AB_SOCKET_TCP_INET);
    assert(tcp_server->sock);

    ab_socket_reuse_addr(tcp_server->sock);

    int ret = ab_socket_bind(tcp_server->sock, NULL, port);
    assert(0 == ret);

    ret = ab_socket_listen(tcp_server->sock, 10);
    assert(0 == ret);

    tcp_server->new_conn_cb = new_conn_cb;
    tcp_server->user_data = user_data;

    tcp_server->error_num = 0;

    tcp_server->quit = false;
    pthread_create(&tcp_server->acceptor_thd, NULL,
                   accept_event_loop_func, tcp_server);

    return tcp_server;
}

void ab_tcp_server_free(T *tcp_server) {
    assert(tcp_server && *tcp_server);

    (*tcp_server)->quit = true;
    pthread_join((*tcp_server)->acceptor_thd, NULL);

    if ((*tcp_server)->error_num != 0) {
        // TODO TCP Server acceptor isn't normal exit.
    }

    ab_socket_free(&(*tcp_server)->sock);
    FREE(*tcp_server);
}

void *accept_event_loop_func(void *arg) {
    assert(arg);

    T tcp_server = (T) arg;

    fd_set rfds;
    int fd = ab_socket_fd(tcp_server->sock);

    while (!tcp_server->quit) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;
        int num = select(fd + 1, &rfds, NULL, NULL, &timeout);
        if (num < 0) {
            // TODO select return error
            tcp_server->error_num = -1;
            break;
        } else if (0 == num) {
            continue;
        } else {
            ab_socket_t conn_sock = ab_socket_accept(tcp_server->sock);
            if (tcp_server->new_conn_cb != NULL) {
                tcp_server->new_conn_cb(conn_sock, tcp_server->user_data);
            }
        }
    }

    return NULL;
}
