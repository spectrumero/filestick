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
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include "console.h"
#include "fd.h"
#include "sysdefs.h"

static FDfunction cons_func = {
   .fd_read  = console_read,
   .fd_write = console_write,
   .fd_lseek = NULL,
   .fd_fstat = console_fstat,
   .fd_close = NULL,
};

//------------------------------------------------------------------
// Open the console
int open_console(const char *devname, int flags, mode_t mode, FD *fd) {
   fd->fdfunc = &cons_func;
   return 0;
}

//-----------------------------------------------------------------
// Read the console
ssize_t console_read(int fd, void *buf, size_t count) {
   return read(0, buf, count);
}

//------------------------------------------------------------------
// Write to the system console.
ssize_t console_write(int fd, const void *buf, size_t count) {
   return write(0, buf, count);
}

//------------------------------------------------------------------
// stat the console
int console_fstat(int fd, struct stat *statbuf) {
   memset(statbuf, 0, sizeof(struct stat));
   statbuf->st_mode = S_IFCHR | 0777;
   statbuf->st_nlink = 1;
   return 0;
}

