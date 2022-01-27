/*
 * ab_tcp_server.h
 *
 *  Created on: 2021年12月26日
 *      Author: ljm
 */

#ifndef AB_TCP_SERVER_H_
#define AB_TCP_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_tcp_server_t
typedef struct T *T;

extern T    ab_tcp_server_new(unsigned short port, 
    void new_conn_cb(void *sock, void *user_data), void *user_data);

extern void ab_tcp_server_free(T *tcp_server);

#undef T

#ifdef __cplusplus
}
#endif

#endif /* AB_TCP_SERVER_H_ */
