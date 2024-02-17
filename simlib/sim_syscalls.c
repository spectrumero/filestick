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

// Simulated syscall wrapper.
// Essentially the interface between the simulator library and the program
// under test.

#include <errno.h>
#include "fd.h"
#include "simulator.h"

ssize_t 
SIM_write(int fd, const void *buf, size_t count)
{
   ssize_t rc = SYS_write(fd, buf, count);
   if(rc < 0) errno = rc;
   return rc;
}

ssize_t
SIM_read(int fd, void *buf, size_t count)
{
   ssize_t rc = SYS_read(fd, buf, count);
   if(rc < 0) errno = rc;
   return rc;
}

int
SIM_fstat(int fd, struct stat *statbuf)
{
   int rc = SYS_fstat(fd, statbuf);
   if(rc < 0) errno = rc;
   return rc;
}

off_t
SIM_lseek(int fd, off_t offset, int whence)
{
   off_t rc= SYS_lseek(fd, offset, whence);
   if(rc < 0) errno = rc;
   return rc;
}

int
SIM_close(int fd)
{
   int rc = SYS_close(fd);
   if(rc < 0) errno = rc;
   return rc;
}

int
SIM_open(const char *pathname, int flags, mode_t mode)
{
   int rc = SYS_open(pathname, flags, mode);
   if(rc < 0) errno = rc;
   return rc;
}

