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

// file.c: for the simulator just wraps posix i/o

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#include "filesystem.h"
#include "fd.h"

static FDfunction fileio_func = {
   .fd_read          = fileio_read,
   .fd_write         = fileio_write,
   .fd_close         = fileio_close,
   .fd_lseek         = fileio_lseek,
   .fd_fstat         = fileio_fstat
};

int   real_fd[MAX_FD_COUNT];

void init_fileio()
{
   memset(&real_fd, 0, sizeof(real_fd));
}

int fileio_open(const char *path, int flags, mode_t mode)
{
   int fdnum;
   FD *fd = fd_alloc(&fdnum);
   if(!fd)
      return -EMFILE;

   fd->fdfunc = &fileio_func;

   int rfd = open(path, flags, mode);
   if(rfd) {
      real_fd[fdnum] = rfd;
      return fdnum;
   }

   fd_dealloc(fd);
   return errno;
}

ssize_t fileio_read(int fd, void *buf, size_t count)
{
   int rfd = real_fd[fd];
   ssize_t rc = read(rfd, buf, count);
   if(rc < 0) return errno;
   return rc;
}

ssize_t fileio_write(int fd, const void *buf, size_t count)
{
   int rfd = real_fd[fd];
   ssize_t rc = write(rfd, buf, count);
   if(rc < 0) return errno;
   return rc;
}

int fileio_close(int fd) {
   int rfd = real_fd[fd];
   int rc = close(rfd);
   if(rc < 0) return errno;
   return rc;
}

off_t fileio_lseek(int fd, off_t offset, int whence)
{
   int rfd = real_fd[fd];
   off_t rc = lseek(rfd, offset, whence);
   if(rc < 0) return errno;
   return rc;
}

int fileio_fstat(int fd, struct stat *statbuf)
{
   int rfd = real_fd[fd];
   int rc = fstat(rfd, statbuf);
   if(rc < 0) return errno;
   return rc;
}

