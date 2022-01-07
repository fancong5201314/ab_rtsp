/*
 * ab_except.c
 *
 *  Created on: 2021年12月25日
 *      Author: ljm
 */

#include "ab_except.h"
#include "ab_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define T except_t
except_frame_t *except_stack = NULL;
void except_raise(const T *e, const char *file, int line) {
#ifdef WIN32
    except_frame_t *p;
    if (-1 == except_index)
        except_init();
    p = TlsGetValue(except_index);
#else
    except_frame_t *p = except_stack;
#endif
    assert(e);
    if (NULL == p) {
        fprintf(stderr, "Uncaught exception");
        if (e->reason)
            fprintf(stderr, " %s", e->reason);
        else
            fprintf(stderr, " at 0x%p", e);
        if (file && line > 0)
            fprintf(stderr, " raised at %s:%d\n", file, line);
        fprintf(stderr, "aborting...\n");
        fflush(stderr);
        abort();
    }
    p->exception = e;
    p->file = file;
    p->line = line;
#ifdef WIN32
    except_pop();
#else
    except_stack = except_stack->prev;
#endif
    longjmp(p->env, EXCEPT_RAISED);
}

#ifdef WIN32
_CRTIMP void _cdecl _assert(void *, void *, unsigned);
#undef assert
#define assert(e) ((e) || (_assert(#e, __FILE__, __LINE__), 0))

int except_index = -1;
void except_init(void) {
    bool cond;

    except_index = TlsAlloc();
    assert(except_index != TLS_OUT_OF_INDEXES);
    cond = TlsSetValue(except_index, NULL);
    assert(cond == true);
}

void except_push(except_frame_t *frame) {
    bool cond;

    frame->prev = TlsGetValue(except_index);
    cond = TlsSetValue(except_index, frame);
    assert(cond == true);
}

void except_pop() {
    bool cond;
    except_frame_t *tos = TlsGetValue(except_index);

    cond = TlsSetValue(except_index, tos->prev);
    assert(cond == true);
}
#endif
