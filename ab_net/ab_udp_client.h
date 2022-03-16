/*
 * ab_udp_client.h
 *
 *  Created on: 2022年1月27日
 *      Author: ljm
 */

#ifndef AB_UDP_CLIENT_H_
#define AB_UDP_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_udp_client_t
typedef struct T *T;

extern T    ab_udp_client_new(unsigned short port);
extern void ab_udp_client_free(T *t);

extern int  ab_udp_client_recv(T t,
    char *addr_buf, unsigned int addr_buf_size, unsigned short *port,
    unsigned char *buf, unsigned int buf_size, int timeout);
extern int  ab_udp_client_send(T t,
    const char *addr, unsigned short port,
    const unsigned char *data, unsigned int data_len);

#undef T

#ifdef __cplusplus
}
#endif

#endif // AB_UDP_CLIENT_H_