/*
 * ip_check.h
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */


#ifndef IP_CHECK_H_
#define IP_CHECK_H_

#ifdef __cplusplus
extern "C" {
#endif

enum IPVersion {
    IP_VERSION_NONE = 0,
    IP_VERSION_4,
    IP_VERSION_6
};

extern int  ip_check(const char *ip);

#ifdef __cplusplus
}
#endif

#endif // IP_CHECK_H_