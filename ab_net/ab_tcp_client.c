/*
 * ab_tcp_client.c
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */


#include "ab_tcp_client.h"

#include "ab_socket.h"

#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include <stdlib.h>
#include <sys/select.h>

#define T ab_tcp_client_t

struct T {
    ab_socket_t sock;
};

T ab_tcp_client_new(const char *addr, unsigned short port) {
    T tcp_client;
    NEW(tcp_client);
    assert(tcp_client);
    tcp_client->sock = ab_socket_new(AB_SOCKET_TCP_INET);
    assert(tcp_client->sock);

    if (ab_socket_connect(tcp_client->sock, addr, port) != 0) {
        ab_socket_free(&tcp_client->sock);
        FREE(tcp_client);
    }

    return tcp_client;
}

void ab_tcp_client_free(T *t) {
    assert(t && *t);
    ab_socket_free(&(*t)->sock);
    FREE(*t);
}

int ab_tcp_client_recv(T t, unsigned char *buf, unsigned int buf_size) {
    assert(t);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000;

    fd_set rfds;
    FD_ZERO(&rfds);

    int fd = ab_socket_fd(t->sock);
    FD_SET(fd, &rfds);
    if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0 && FD_ISSET(fd, &rfds)) {
        return ab_socket_recv(t->sock, buf, buf_size);
    }

    return -1;
}

int  ab_tcp_client_send(T t, const unsigned char *data, unsigned int data_len) {
    assert(t);
    return ab_socket_send(t->sock, data, data_len);
}