/*
 * ab_udp_client.c
 *
 *  Created on: 2022年1月27日
 *      Author: ljm
 */

#include "ab_udp_client.h"

#include "ab_socket.h"

#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include <stdlib.h>
#include <sys/select.h>

#define T ab_udp_client_t
struct T {
    ab_socket_t sock;
};

T ab_udp_client_new(unsigned short port) {
    T result;
    NEW(result);

    result->sock = ab_socket_new(AB_SOCKET_UDP_INET);
    assert(result->sock);

    ab_socket_reuse_addr(result->sock);

    int ret = ab_socket_bind(result->sock, NULL, port);
    assert(0 == ret);

    return result;
}

void ab_udp_client_free(T *t) {
    assert(t && *t);
    ab_socket_free(&(*t)->sock);
    FREE(*t);
}

int ab_udp_client_recv(T t,
    char *addr_buf, unsigned int addr_buf_size, 
    unsigned short *port,
    unsigned char *buf, unsigned int buf_size, int timeout) {
    assert(t);

    int result = -1;

    if (timeout > 0) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeout * 1000;

        fd_set rfds;
        FD_ZERO(&rfds);

        int fd = ab_socket_fd(t->sock);
        FD_SET(fd, &rfds);
        result = select(fd + 1, &rfds, NULL, NULL, &tv) > 0 && FD_ISSET(fd, &rfds);
        if (result > 0) {
            result = ab_socket_udp_recv(t->sock, 
                addr_buf, addr_buf_size,
                port, buf, buf_size);
        }
    } else {
        result = ab_socket_udp_recv(t->sock, 
            addr_buf, addr_buf_size,
            port, buf, buf_size);
    }

    return result;
}

int  ab_udp_client_send(T t,
    const char *addr, unsigned short port,
    const unsigned char *data, unsigned int data_len) {
    assert(t);

    return ab_socket_udp_send(t->sock, 
        addr, port, data, data_len);
}
