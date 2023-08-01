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

// Implement top level file descriptor functions

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fd.h"
#include "console.h"
#include "errno.h"
#include "dev_open.h"

FD fdtable[MAX_FILE_DESCRIPTORS];

//------------------------------------------------------------------------
// Initialize file descriptors
//
void fd_init() {
   memset(fdtable, 0, sizeof(fdtable));

   // Open stdin, stdout, stderr
   open_device("console", O_RDONLY, 0);
   open_device("console", O_WRONLY, 0);
   open_device("console", O_WRONLY, 0);
}

//------------------------------------------------------------------------
// Write to an fd
//
ssize_t SYS_write(int fd, const void *buf, size_t count) {
   if(fd > MAX_FILE_DESCRIPTORS || fd < 0)
      return -EBADF;

   FD fd_ent = fdtable[fd];
   if(fd_ent.flags && fd_ent.fdfunc->fd_write != NULL) {
      return fd_ent.fdfunc->fd_write(fd, buf, count);
   }

   return -EBADF;
}

//------------------------------------------------------------------------
// Read from an fd
//
ssize_t SYS_read(int fd, void *buf, size_t count) {
   if(fd > MAX_FILE_DESCRIPTORS || fd < 0)
      return -EBADF;

   FD fd_ent = fdtable[fd];
   if(fd_ent.flags && fd_ent.fdfunc->fd_read != NULL) {
      return fd_ent.fdfunc->fd_read(fd, buf, count);
   }

   return -EBADF;
}

//------------------------------------------------------------------------
// Stat an fd
//
int SYS_fstat(int fd, struct stat *statbuf) {
   if(fd > MAX_FILE_DESCRIPTORS || fd < 0)
      return -EBADF;

   FD fd_ent = fdtable[fd];
   if(fd_ent.flags && fd_ent.fdfunc->fd_fstat != NULL) {
      return fd_ent.fdfunc->fd_fstat(fd, statbuf);
   }

   return -EBADF;
}

//------------------------------------------------------------------------
// seek on an fd
//
off_t SYS_lseek(int fd, off_t offset, int whence) {
   if(fd > MAX_FILE_DESCRIPTORS || fd < 0)
      return -EBADF;

   FD fd_ent = fdtable[fd];
   if(fd_ent.flags && fd_ent.fdfunc->fd_lseek != NULL) {
      return fd_ent.fdfunc->fd_lseek(fd, offset, whence);
   }

   return -ESPIPE;
}

//------------------------------------------------------------------------
// Close an fd
//
int SYS_close(int fd) {
   if(fd > MAX_FILE_DESCRIPTORS || fd < 0)
      return -EBADF;

   // No-op if trying to close stdout/stderr/stdin
   if(fd < 3)
      return 0;

   FD fd_ent = fdtable[fd];
   if(fd_ent.flags && fd_ent.fdfunc->fd_close != NULL) {
      int rc=fd_ent.fdfunc->fd_close(fd);
      fd_dealloc(&fd_ent);

      return rc;
   }

   return -EBADF;
}

//-----------------------------------------------------------------------
// Open an fd
//
int SYS_open(const char *pathname, int flags, mode_t mode) {
   // Device or file?
   if(!strncmp(pathname, "/dev/", 5)) {
      return open_device(pathname + 5, flags, mode);
   }
   return -ENOENT;
}

//-----------------------------------------------------------------------
// Dealloc an fd
//
void fd_dealloc(FD *fd) {
   memset(fd, 0, sizeof(FD));
}

//-----------------------------------------------------------------------
// Allocate a new fd
//
FD *fd_alloc(int *fdnum) {
   for(int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
      if(!fdtable[i].flags) {
         fdtable[i].flags = FLAG_ALLOCATED;
         *fdnum = i;
         return &fdtable[i];
      }
   }

   return NULL;
}

//-----------------------------------------------------------------------
// Get an fd entry from its integer value
FD *get_fdentry(int fd) {
   if(fd < 0 || fd > MAX_FILE_DESCRIPTORS)
      return NULL;

   return &fdtable[fd];
}

