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

// mount syscall wrapper

#include <stdint.h>
#include <errno.h>

#define SYS_MOUNT    40

int mount(const char *source, const char *target, const char *filesystemtype,
      unsigned long mountflags, const void *data)
{
   register uint32_t       syscall  asm("a7") = SYS_MOUNT;
   register const char *   _src     asm("a0");
   register const char *   _tgt     asm("a1");
   register const char *   _fstype  asm("a2");
   register uint32_t       _mntflg  asm("a3");
   register const void *   _data    asm("a4");
   register int            _rc      asm("a0");
   _src = source;
   _tgt = target;
   _fstype = filesystemtype;
   _mntflg = mountflags;
   _data = data;
   asm volatile("ecall"
         : "=r"(_rc)
         : "r"(_src), "r"(_tgt), "r"(_fstype), "r"(_mntflg), "r"(_data), "r"(syscall));
   if(_rc < 0) {
      errno = _rc;
      return -1;
   }
   return 0;
}

