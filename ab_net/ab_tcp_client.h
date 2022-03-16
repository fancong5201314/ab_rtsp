/*
 * ab_tcp_client.h
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#ifndef AB_TCP_CLIENT_H_
#define AB_TCP_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_tcp_client_t
typedef struct T *T;

extern T    ab_tcp_client_new(const char *addr, unsigned short port);
extern void ab_tcp_client_free(T *t);

extern int  ab_tcp_client_recv(T t, unsigned char *buf, unsigned int buf_size, int timeout);
extern int  ab_tcp_client_send(T t, const unsigned char *data, unsigned int data_len);

#undef T

#ifdef __cplusplus
}
#endif

#endif // AB_TCP_CLIENT_H_