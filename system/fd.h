#ifndef _FD_H
#define _FD_H

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


#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>

#define MAX_FILE_DESCRIPTORS  16

// Temporary flag to reserve a file descriptor
#define FLAG_ALLOCATED  0x80000000

typedef struct _FDfunction {
   ssize_t (*fd_read)(int fd, void *ptr, size_t count);
   ssize_t (*fd_write)(int fd, const void *ptr, size_t count);
   off_t   (*fd_lseek)(int fd, off_t offset, int whence);
   int     (*fd_fstat)(int fd, struct stat *statbuf);
   int     (*fd_close)(int fd);
   int     (*fd_ioctl)(int fd, unsigned long request, void *ptr);
} FDfunction;

typedef struct _FD {
   uint32_t       flags;
   FDfunction     *fdfunc;
   void           *data;
} FD;

// File descriptor table management
void fd_init();
FD *fd_alloc(int *fdnum);
FD *get_fdentry(int fd);
void fd_dealloc(FD *fd);

// System calls
ssize_t SYS_write(int fd, const void *buf, size_t count);
ssize_t SYS_read (int fd, void *buf, size_t count);
int     SYS_fstat(int fd, struct stat *statbuf);
off_t   SYS_lseek(int fd, off_t offset, int whence);
int     SYS_ioctl(int fd, unsigned long request, void *ptr);
int     SYS_close(int fd);
int     SYS_open(const char *pathname, int flags, mode_t mode);


#endif
