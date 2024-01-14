/*
;The MIT License
;
;Copyright (c) 2024 Dylan Smith
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

// printk implementation - uses nanoprintf
// See https://github.com/charlesnicholson/nanoprintf

#include <stdarg.h>

#include "console.h"
#include "printk.h"

// Configure nanoprintf
// NOTE: enabling binary specifiers requires __clzsi2.c from libgcc
#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS  1     // Enables field width specifiers
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS    0     // Set to 0 or 1. Enables precision specifiers.
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS        0     // Set to 0 or 1. Enables floating-point specifiers.
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS        0     // Set to 0 or 1. Enables oversized modifiers.
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS       0     // Set to 0 or 1. Enables binary specifiers.
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS    0     // Set to 0 or 1. Enables %n for write-back.
#include "nanoprintf.h"

static void _npf_putc(int c, void *ctx);
void printk(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   npf_vpprintf(_npf_putc, NULL, fmt, args);
   va_end(args);
}

static void _npf_putc(int c, void *ctx)
{   
   if(c == '\n') {
      serial_putc('\r');
      serial_putc('\n');
   }
   else
      serial_putc(c);
}
