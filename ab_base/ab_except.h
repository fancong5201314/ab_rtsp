/*
 * ab_except.h
 *
 *  Created on: 2021年12月25日
 *      Author: ljm
 */

#ifndef AB_EXCEPT_H_
#define AB_EXCEPT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <setjmp.h>

#define T except_t
typedef struct T {
    const char *reason;
} T;

typedef struct except_frame_t except_frame_t;
struct except_frame_t {
    except_frame_t *prev;
    jmp_buf env;
    const char *file;
    int line;
    const T *exception;
};

enum {
    EXCEPT_ENTERED = 0,
    EXCEPT_RAISED,
    EXCEPT_HANDLED,
    EXCEPT_FINALIZED
};

extern except_frame_t *except_stack;
extern const except_t assert_failed;

void except_raise(const T *e, const char *file, int line);

#ifdef WIN32
#include <windows.h>
extern int except_index;
extern void except_init();
extern void except_push(except_frame_t *fp);
extern void except_pop();
#endif

#ifdef WIN32
#define RAISE(e) except_raise(&(e), __FILE__, __LINE__)
#define RERAISE except_raise(except_frame.exception, \
        except_frame.file, except_frame.line)
#define RETURN switch (except_pop(), 0) default: return
#define TRY \
    do { \
        volatile int except_flag; \
        except_frame_t except_frame; \
        if (-1 == except_index) \
            except_init(); \
        except_push(&except_frame); \
        except_flag = setjmp(except_frame.env); \
        if (EXCEPT_ENTERED == except_flag) {
#define EXCEPT(e) \
            if (EXCEPT_ENTERED == except_flag) except_pop(); \
        } else if (except_frame.exception == &(e)) { \
            except_flag = EXCEPT_HANDLED;
#define ELSE \
            if (EXCEPT_ENTERED == except_flag) except_pop(); \
        } else { \
            except_flag = EXCEPT_HANDLED;
#define FINALLY \
            if (EXCEPT_ENTERED == except_flag) except_pop(); \
        } { \
            if (EXCEPT_ENTERED == except_flag) \
                except_flag = EXCEPT_FINALIZED;
#define END_TRY \
            if (EXCEPT_ENTERED == except_flag) except_pop(); \
        } if (EXCEPT_RAISED == except_flag) RERAISE; \
    } while (0)
#else
#define RAISE(e) except_raise(&(e), __FILE__, __LINE__)
#define RERAISE except_raise(except_rame.exception, \
        except_frame.file, except_except_frame.line)
#define RETURN switch (except_stack = except_stack->prev, 0) default: return
#define TRY \
    do { \
        volatile int except_flag; \
        except_frame_t except_frame; \
        except_frame.prev = except_stack; \
        except_stack = &except_frame; \
        except_flag = setjmp(except_frame.env); \
        if (EXCEPT_ENTERED == except_flag) {
#define EXCEPT(e) \
            if (EXCEPT_ENTERED == except_flag) except_stack = except_stack->prev; \
        } else if (except_frame.exception == &(e)) { \
            except_flag = EXCEPT_HANDLED;
#define ELSE \
            if (EXCEPT_ENTERED == except_flag) except_stack = except_stack->prev; \
        } else { \
            except_flag = EXCEPT_HANDLED;
#define FINALLY \
            if (EXCEPT_ENTERED == except_flag) except_stack = except_stack->prev; \
        } { \
            if (EXCEPT_ENTERED == except_flag) \
                except_flag = EXCEPT_FINALIZED;
#define END_TRY \
            if (EXCEPT_ENTERED == except_flag) except_stack = except_stack->prev; \
        } if (EXCEPT_RAISED == except_flag) RERAISE; \
    } while (0)
#endif

#undef T

#ifdef __cplusplus
}
#endif

#endif /* AB_EXCEPT_H_ */
