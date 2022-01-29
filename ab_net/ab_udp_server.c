#include "ab_udp_server.h"

#include "ab_socket.h"

#include "ab_base/ab_mem.h"
#include "ab_base/ab_assert.h"

#include <stdlib.h>

#define T ab_udp_server_t
struct T {
    ab_socket_t sock;
};

T ab_udp_server_new(unsigned short port) {
    T udp_server;
    NEW(udp_server);

    udp_server->sock = ab_socket_new(AB_SOCKET_UDP_INET);
    assert(udp_server->sock);

    ab_socket_reuse_addr(udp_server->sock);

    int ret = ab_socket_bind(udp_server->sock, NULL, port);
    assert(0 == ret);

    return udp_server;
}

void ab_udp_server_free(T *t) {
    assert(t && *t);
    ab_socket_free(&(*t)->sock);
    FREE(*t);
}

int  ab_udp_server_send(T t,
    const char *addr, unsigned short port,
    const unsigned char *data, unsigned int data_len) {
    assert(t);

    return ab_socket_udp_send(t->sock, 
        addr, port, data, data_len);
}

int  ab_udp_server_recv(T t,
    char *addr_buf, unsigned int addr_buf_size, 
    unsigned short *port,
    unsigned char *buf, unsigned int buf_size) {
    assert(t);

    return ab_socket_udp_recv(t->sock, 
        addr_buf, addr_buf_size,
        port, buf, buf_size);
}