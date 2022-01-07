/*
 * ab_assert.c
 *
 *  Created on: 2021年12月25日
 *      Author: ljm
 */

#include "ab_assert.h"

const except_t assert_failed = { "Assertion failed" };
void (assert) (int e) {
    assert(e);
}
