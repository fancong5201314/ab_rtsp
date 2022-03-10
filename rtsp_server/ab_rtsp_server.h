/*
 * ab_rtsp_server.h
 *
 *  Created on: 2022年1月7日
 *      Author: ljm
 */

#ifndef AB_RTSP_SERVER_H_
#define AB_RTSP_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define T ab_rtsp_server_t
typedef struct T *T;

/*
 * video_codec: 1(H.264)、2(H.265)  
 */
extern T    ab_rtsp_server_new(unsigned short port, int video_codec);
extern void ab_rtsp_server_free(T *rtsp);

extern int  ab_rtsp_server_send(T rtsp, const char *data, unsigned int data_len);

#undef T

#ifdef __cplusplus
}
#endif

#endif // AB_RTSP_SERVER_H_
