/*
 * ab_rtsp_pull_stream.h
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#ifndef AB_RTSP_PULL_STREAM_H_
#define AB_RTSP_PULL_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_rtsp_pull_stream_t
typedef struct T *T;

extern T    ab_rtsp_pull_stream_new(const char *url, 
    void (*cb)(const unsigned char *, unsigned int, void *), void *user_data);
extern void ab_rtsp_pull_stream_free(T *t);

#undef T

#ifdef __cplusplus
}
#endif

#endif // AB_RTSP_PULL_STREAM_H_