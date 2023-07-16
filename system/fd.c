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

#include "fd.h"
#include "console.h"

FD fdtable[MAX_FILE_DESCRIPTORS];

//------------------------------------------------------------------------
// Initialize file descriptors
//
void fd_init() {
   memset(fdtable, 0, sizeof(fdtable));

   // set up stdin, stdout, stderr
   // stdout
   fdtable[1].fd_write = console_write;
   fdtable[1].flags    = FD_FLAG_OPEN;
   fdtable[2].fd_write = console_write;
   fdtable[2].flags    = FD_FLAG_OPEN;
}

//------------------------------------------------------------------------
// Write to an fd
//
ssize_t SYS_write(int fd, void *buf, size_t count) {
   if(fd > MAX_FILE_DESCRIPTORS || fd < 0)
      return -1;

   FD fd_ent = fdtable[fd];
   if(fd_ent.flags & FD_FLAG_OPEN && fd_ent.fd_write != NULL) {
      return fd_ent.fd_write(fd, buf, count);
   }

   return -1;
}

