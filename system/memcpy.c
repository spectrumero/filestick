/*
;The MIT License
;
;Copyright (c) 2023 Dylan Smith
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;THE SOFTWARE.
*/

#include <string.h>
#include <stdint.h>

#define UNALIGNED(x,y) \
    (((uint32_t)x & 3) | ((uint32_t)y & 3))

// note: the use of volatile prevents the optimiser from trying to use
// libgcc's memcpy (which will cause a lock up, as it doesn't exist so we'll
// just get stuck in an infinite loop...)
void *memcpy(void *__restrict dest, const void *__restrict src, size_t n) {
    volatile uint8_t *ud = dest;
    volatile const uint8_t *us = src;

    if(!UNALIGNED(dest,src) && n >= 4) {
        size_t aligned = n >> 2;
        volatile uint32_t *d = (uint32_t *)dest;
        volatile const uint32_t *s = (uint32_t *)src;

        while(aligned--) {
            *d++ = *s++;
        }

        // pick up remainder
        n &= 3;
        ud = (uint8_t *)d;
        us = (uint8_t *)s;
    }

    while(n--) {
        *ud++ = *us++;
    }
    return dest;
}

