#ifndef _CONSOLE_H
#define _CONSOLE_H
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

#include <stdint.h>
#include <unistd.h>

#include "fd.h"

ssize_t console_write(int fd, void *buf, size_t count);
void serial_putc(uint8_t ch);

// Debugging routings
void console_hexdump(void *buf, size_t count);
void console_hexword(uint32_t word);
void console_hexbyte(uint8_t byte);

// Syscall support
int open_console(const char *devname, int flags, mode_t mode, FD *fd);

#endif

