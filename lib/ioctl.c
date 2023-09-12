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

#include <sys/ioctl.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

#define SYS_IOCTL    29

// Syscall wrapper for ioctl
int ioctl(int fd, unsigned long request, ...) {
   va_list ap;
   va_start(ap, request);
   uint8_t *p = va_arg(ap, uint8_t *);
   va_end(ap);

   register uint32_t    syscall  asm("a7") = SYS_IOCTL;
   register int         _fd      asm("a0");
   register uint32_t    _req     asm("a1");
   register uint8_t     *_ptr    asm("a2");
   register int         _rc      asm("a0");
   _fd = fd;
   _req = request;
   _ptr = p;
   asm volatile("ecall"
         : "=r"(_rc)
         : "r"(_fd), "r"(_req), "r"(_ptr), "r"(syscall));
   if(_rc < 0) {
      errno = _rc;
      return -1;
   }
   return 0;
}

