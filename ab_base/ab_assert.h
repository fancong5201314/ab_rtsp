/*
 * ab_assert.h
 *
 *  Created on: 2021年12月25日
 *      Author: ljm
 */

#ifndef AB_ASSERT_H_
#define AB_ASSERT_H_

#ifdef __cplusplus
extern "C" {
#endif

#undef assert

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#include "ab_except.h"
extern void assert(int e);
#define assert(e) ((void)((e) || (RAISE(assert_failed), 0)))
#endif

#ifdef __cplusplus
}
#endif

#endif /* AB_ASSERT_H_ */
