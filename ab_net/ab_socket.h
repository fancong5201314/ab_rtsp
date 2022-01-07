/*
 * ab_socket.h
 *
 *  Created on: 2021年12月25日
 *      Author: ljm
 */

#ifndef AB_SOCKET_H_
#define AB_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_socket_t
typedef struct T *T;

enum {
    AB_SOCKET_NONE         = 0,
    AB_SOCKET_TCP_INET,
    AB_SOCKET_TCP_INET6,
    AB_SOCKET_UDP_INET,
    AB_SOCKET_UDP_INET6
};

extern T    ab_socket_new(int sock_type);
extern void ab_socket_free(T *sock);

extern int  ab_socket_bind(T sock, const char *bind_addr, unsigned short bind_port);
extern int  ab_socket_listen(T sock, int back_log);
extern int  ab_socket_connect(T sock, const char *conn_addr, unsigned short conn_port);
extern T    ab_socket_accept(T sock);

extern int  ab_socket_send(T sock, const char *data, unsigned int data_len);
extern int  ab_socket_recv(T sock, char *buf, unsigned int buf_size);
extern int  ab_socket_udp_send(T sock, const char *to_addr, unsigned short to_port,
        const char *data, unsigned int data_len);
extern int  ab_socket_udp_recv(T sock, char *from_addr_buf, unsigned int addr_buf_size,
        unsigned short *from_port, char *buf, unsigned int buf_size);

extern int  ab_socket_fd(T sock);
extern int  ab_socket_addr(T sock, char *buf, unsigned int buf_size);
extern int  ab_socket_port(T sock, unsigned short *port);

extern int  ab_socket_reuse_addr(T sock);

#undef T

#ifdef __cplusplus
}
#endif

#endif /* AB_SOCKET_H_ */
