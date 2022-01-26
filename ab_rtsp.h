/*
 * ab_rtsp.h
 *
 *  Created on: 2022年1月7日
 *      Author: ljm
 */

#ifndef AB_RTSP_H_
#define AB_RTSP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_rtsp_t
typedef struct T *T;

enum ab_rtsp_over_method_t {
    AB_RTSP_OVER_NONE = 0,
    AB_RTSP_OVER_TCP,
    AB_RTSP_OVER_UDP
};

extern T    ab_rtsp_new();
extern void ab_rtsp_free(T *rtsp);

extern int  ab_rtsp_send(T rtsp, const char *data, unsigned int data_size);

#undef T

#ifdef __cplusplus
}
#endif

#endif // AB_RTSP_H_
