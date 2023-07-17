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

#include "rgbled.h"
#include "fd.h"

static FDfunction led_func = {
   .fd_read  = rgbled_read,
   .fd_write = rgbled_write,
   .fd_close = rgbled_close,
};

static uint32_t ledval;
static uint32_t *led_dev = (uint32_t *)0x800000;

int rgbled_open(const char *devname, int flags, mode_t mode, FD *fd) {
   fd->fdfunc = &led_func;
   return 0;   
}

ssize_t rgbled_write(int fd, void *buf, size_t count) {
   uint8_t *bufptr = (uint8_t *)buf;
   size_t rc = count;

   while(count--) {
      uint8_t byte = *bufptr++;
      ledval = byte;
      *led_dev = ledval;
   }
   return rc;
}

ssize_t rgbled_read(int fd, void *buf, size_t count) {
   uint8_t *bufptr = (uint8_t *)buf;
   size_t rc = count;

   while(count--) {
      *bufptr++ = (uint8_t)ledval;
   }
   return rc;
}

int rgbled_close(int fd) {
   return 0;
}

