/*
 * ab_rtsp_client.h
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#ifndef AB_RTSP_CLIENT_H_
#define AB_RTSP_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_rtsp_client_t
typedef struct T *T;

extern T    ab_rtsp_client_new(const char *url, 
    void (*cb)(const unsigned char *, unsigned int, void *), void *user_data);
extern void ab_rtsp_client_free(T *t);

#undef T

#ifdef __cplusplus
}
#endif

#endif // AB_RTSP_CLIENT_H_