/*
 * mem.c
 *
 *  Created on: 2021年12月26日
 *      Author: ljm
 */

#include "ab_mem.h"
#include "ab_assert.h"

#include <stdlib.h>
#include <stddef.h>

const except_t mem_failed = { "Allocation failed" };
void *mem_alloc(long nbytes, const char *file, int line) {
    void *ptr;
    assert(nbytes > 0);
    ptr = malloc(nbytes);
    if (NULL == ptr) {
        if (NULL == file)
            RAISE(mem_failed);
        else
            except_raise(&mem_failed, file, line);
    }

    return ptr;
}

void *mem_calloc(long count, long nbytes, const char *file, int line) {
    void *ptr;
    assert(count > 0);
    assert(nbytes > 0);
    ptr = calloc(count, nbytes);
    if (NULL == ptr) {
        if (NULL == file)
            RAISE(mem_failed);
        else
            except_raise(&mem_failed, file, line);
    }

    return ptr;
}

void mem_free(void *ptr, const char *file, int lien) {
    if (ptr)
        free(ptr);
}

void *mem_resize(void *ptr, long nbytes, const char *file, int line) {
    assert(ptr);
    assert(nbytes > 0);
    ptr = realloc(ptr, nbytes);
    if (NULL == ptr) {
        if (NULL == file)
            RAISE(mem_failed);
        else
            except_raise(&mem_failed, file, line);
    }

    return ptr;
}
