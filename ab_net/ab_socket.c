/*
 * ab_socket.c
 *
 *  Created on: 2021年12月25日
 *      Author: ljm
 */

#include "ab_socket.h"

#include "ab_base/ab_assert.h"
#include "ab_base/ab_mem.h"

#include <stdio.h>
#include <string.h>

#ifdef __MINGW32__
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define T ab_socket_t

struct T {
#ifdef __MINGW32__
    SOCKET fd;
#else
    int fd;
#endif
    int type;
    struct sockaddr addr;
};

static int ab_socket_set_addr(int type, struct sockaddr *sock_addr,
        const char *addr, unsigned short port);
static int ab_socket_get_addr(int type, const struct sockaddr *sock_addr,
        char *addr_buf, unsigned int buf_size, unsigned short *port);

T ab_socket_new(int sock_type) {
    int af = 0, type = 0;
    if (AB_SOCKET_TCP_INET == sock_type) {
        af = AF_INET;
        type = SOCK_STREAM;
    } else if (AB_SOCKET_UDP_INET == sock_type) {
        af = AF_INET;
        type = SOCK_DGRAM;
#ifdef __MINGW32__
#else
    } else if (AB_SOCKET_TCP_INET6 == sock_type) {
        af = AF_INET6;
        type = SOCK_STREAM;
    } else if (AB_SOCKET_UDP_INET6 == sock_type) {
        af = AF_INET6;
        type = SOCK_DGRAM;
#endif
    } else
        return NULL;

#ifdef __MINGW32__
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(1, 1), &wsa_data) == SOCKET_ERROR) {
        perror("WSAStartup");
        return NULL;
    }
#endif

#ifdef __MINGW32__
    SOCKET sock_fd = socket(af, type, 0);
    if (INVALID_SOCKET == sock_fd)
#else
    int sock_fd = socket(af, type, 0);
    if (-1 == sock_fd)
#endif
        return NULL;

    T sock;
    NEW(sock);

    sock->fd = sock_fd;
    sock->type = sock_type;
    memset(&sock->addr, 0, sizeof(sock->addr));

    return sock;
}

void ab_socket_free(T *sock) {
    assert(sock && *sock);

#ifdef __MINGW32__
    if ((*sock)->fd != INVALID_SOCKET)
        closesocket((*sock)->fd);
    WSACleanup();
#else
    if ((*sock)->fd > 0)
        close((*sock)->fd);
#endif

    FREE(*sock);
}

int ab_socket_bind(T sock,
    const char *bind_addr, unsigned short bind_port) {
    assert(sock);
    assert(sock->fd > 0);

    if (ab_socket_set_addr(sock->type, &sock->addr,
                        bind_addr, bind_port) != 0)
        return -1;

    return bind(sock->fd, &sock->addr, sizeof(sock->addr));
}

int ab_socket_listen(T sock, int back_log) {
    assert(sock);
    assert(sock->fd > 0);

    return listen(sock->fd, back_log);
}

int ab_socket_connect(T sock,
    const char *conn_addr, unsigned short conn_port) {
    assert(sock);
    assert(sock->fd > 0);

    if (ab_socket_set_addr(sock->type, &sock->addr,
                        conn_addr, conn_port) != 0)
        return -1;

    return connect(sock->fd, &sock->addr, sizeof(sock->addr));
}

T ab_socket_accept(T sock) {
    assert(sock);
    assert(sock->fd > 0);

    struct sockaddr conn_addr;
    memset(&conn_addr, 0, sizeof(conn_addr));
#ifdef __MINGW32__
    int len = sizeof(conn_addr);
#else
    socklen_t len = sizeof(conn_addr);
#endif

    int conn_fd = accept(sock->fd, &conn_addr, &len);
    if (conn_fd > 0) {
        T new_sock;
        NEW(new_sock);
        assert(new_sock);

        new_sock->fd = conn_fd;
        new_sock->type = sock->type;
        memcpy(&new_sock->addr, &conn_addr, len);

        return new_sock;
    }

    return NULL;
}

int ab_socket_send(T sock, const char *data, unsigned int data_len) {
    assert(sock);
    assert(sock->fd > 0);
    assert(data && data_len > 0);

    if (AB_SOCKET_TCP_INET == sock->type ||
        AB_SOCKET_TCP_INET6 == sock->type)
        return send(sock->fd, data, data_len, 0);
    return -1;
}

int ab_socket_recv(T sock, char *buf, unsigned int buf_size) {
    assert(sock);
    assert(sock->fd > 0);
    assert(buf && buf_size > 0);

    if (AB_SOCKET_TCP_INET == sock->type ||
        AB_SOCKET_TCP_INET6 == sock->type)
        return recv(sock->fd, buf, buf_size, 0);
    return -1;
}

int ab_socket_udp_send(T sock, const char *to_addr, unsigned short to_port,
        const char *data, unsigned int data_len) {
    assert(sock);
    assert(sock->fd > 0);
    assert(data && data_len > 0);

    struct sockaddr addr;
    if (ab_socket_set_addr(sock->type, &addr, to_addr, to_port) != 0)
        return -1;
    return sendto(sock->fd, data, data_len, 0, &addr, sizeof(addr));
}

int ab_socket_udp_recv(T sock, char *from_addr_buf, unsigned int addr_buf_size,
        unsigned short *from_port, char *buf, unsigned int buf_size) {
    assert(sock);
    assert(sock->fd > 0);
    assert(buf && buf_size > 0);

    struct sockaddr addr;
#ifdef __MINGW32__
    int addr_len = sizeof(addr);
#else
    socklen_t addr_len = sizeof(addr);
#endif
    int ret = recvfrom(sock->fd, buf, buf_size, 0, &addr, &addr_len);
    if (-1 == ret)
        return -1;

    if (ab_socket_get_addr(sock->type, &addr, from_addr_buf, addr_buf_size,
            from_port) != 0)
        return -1;

    return ret;
}

int ab_socket_set_addr(int type, struct sockaddr *sock_addr,
        const char *addr, unsigned short port) {
    assert(sock_addr);
    if (AB_SOCKET_TCP_INET == type ||
        AB_SOCKET_UDP_INET == type) {
        struct sockaddr_in *inaddr = (struct sockaddr_in *)sock_addr;
        inaddr->sin_family = AF_INET;
        if (NULL == addr)
            inaddr->sin_addr.s_addr = INADDR_ANY;
        else
#ifdef __MINGW32__
            inaddr->sin_addr.s_addr = inet_addr(addr);
#else
            inet_pton(AF_INET, addr, &inaddr->sin_addr);
#endif
        inaddr->sin_port = htons(port);
    } else if (AB_SOCKET_TCP_INET6 == type ||
               AB_SOCKET_UDP_INET6 == type) {
#ifdef __MINGW32__
#else
        struct sockaddr_in6 *in6addr =  (struct sockaddr_in6 *)&addr;
        in6addr->sin6_family = AF_INET6;
        if (NULL == addr)
            in6addr->sin6_addr = in6addr_any;
        else
            inet_pton(AF_INET, addr, &in6addr->sin6_addr);
        in6addr->sin6_port = htons(port);
#endif
    } else
        return -1;

    return 0;
}

int ab_socket_get_addr(int type, const struct sockaddr *sock_addr,
        char *addr_buf, unsigned int buf_size, unsigned short *port) {
    assert(sock_addr);

    if (AB_SOCKET_TCP_INET == type ||
        AB_SOCKET_UDP_INET == type) {
        const struct sockaddr_in *inaddr =
                (const struct sockaddr_in *) sock_addr;
        if (addr_buf != NULL && buf_size != 0) {
#ifdef __MINGW32__
            char *addr = inet_ntoa(inaddr->sin_addr);
            strncpy(addr_buf, addr, buf_size);
#else
            inet_ntop(AF_INET, &inaddr->sin_addr, addr_buf, buf_size);
#endif
        }

        if (port != NULL)
            *port = ntohs(inaddr->sin_port);
    } else if (AB_SOCKET_TCP_INET6 == type ||
               AB_SOCKET_UDP_INET6 == type) {
#ifdef __MINGW32__
#else
        const struct sockaddr_in6 *in6addr =
                (const struct sockaddr_in6 *) &sock_addr;
        if (addr_buf != NULL && buf_size != 0)
            inet_ntop(AF_INET6, &in6addr->sin6_addr, addr_buf, buf_size);
        if (port != NULL)
            *port = ntohs(in6addr->sin6_port);
#endif
    } else
        return -1;

    return 0;
}

int ab_socket_fd(T sock) {
    assert(sock);
    return sock->fd;
}

int ab_socket_addr(T sock, char *buf, unsigned int buf_size) {
    assert(sock);
    assert(sock->fd > 0);
    assert(buf && buf_size > 0);
    return ab_socket_get_addr(sock->type, &sock->addr, buf, buf_size, NULL);
}

int ab_socket_port(T sock, unsigned short *port) {
    assert(sock);
    assert(sock->fd > 0);
    assert(port);
    return ab_socket_get_addr(sock->type, &sock->addr, NULL, 0, port);
}

int ab_socket_reuse_addr(T sock) {
    assert(sock);
    assert(sock->fd > 0);

    int opt_val = 0x1;
#ifdef __MINGW32__
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR,
            (const char *) &opt_val, sizeof(opt_val)) == -1)
#else
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt_val, sizeof(opt_val)) == -1)
#endif
        return -1;

    return 0;
}

int ab_socket_reuse_port(T sock) {
    assert(sock);
    assert(sock->fd > 0);

    int opt_val = 0x1;
#ifdef __APPLE_CC__
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEPORT,
                   &opt_val, sizeof(opt_val)) == -1)
        return -1;
#endif
    return 0;
}
