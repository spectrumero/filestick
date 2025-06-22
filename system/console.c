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
   .fd_ioctl = console_ioctl,
   .fd_peek  = console_peek
};

extern volatile uint32_t bufindex;
extern volatile uint32_t bufstart;
extern volatile uint32_t cr_index;
extern volatile uint32_t cons_rx_count;
extern volatile uint32_t cons_control;

// This should be able to pick up the symbol, but it doesn't
uint8_t *cons_buf = (uint8_t *)0xff00; // FIXME system.ld symbol

//------------------------------------------------------------------
// Open the console
int open_console(const char *devname, int flags, mode_t mode, FD *fd) {
   fd->fdfunc = &cons_func;
   return 0;
}

//-----------------------------------------------------------------
// Read the console
ssize_t console_read(int fd, void *buf, size_t count) {
   size_t bytes_read = 0;
   uint8_t *bufptr = (uint8_t *)buf;

   if(!count) return 0;

   // FIXME will need a way to break out (eg Ctrl-C)
   while(cons_rx_count == 0);

   DISABLE_INTERRUPTS
   cons_rx_count--;
   ENABLE_INTERRUPTS

   while(count && bufstart != bufindex) {
      uint8_t byte = *(cons_buf + bufstart);
      *bufptr++ = byte;

      bufstart++;
      bufstart &= 0xFF;
      count--;
      bytes_read++;

      // FIXME console driver should do this echo back
      if(byte == 0x0d) {
         serial_putc(0x0d);
         serial_putc(0x0a);
         break;
      }
   }

   return bytes_read;
}

//------------------------------------------------------------------
// Return the number of bytes available
ssize_t console_peek(int fd) {
   return cons_rx_count;
}

//------------------------------------------------------------------
// Write to the system console.
ssize_t console_write(int fd, const void *buf, size_t count) {
   ssize_t rc = count;
   char *bufptr = (char *)buf;

   while(count--) {
      serial_putc(*bufptr++);
   }

   return rc;
}

//------------------------------------------------------------------
// stat the console
int console_fstat(int fd, struct stat *statbuf) {
   memset(statbuf, 0, sizeof(struct stat));
   statbuf->st_mode = S_IFCHR | 0777;
   statbuf->st_nlink = 1;
   return 0;
}

//------------------------------------------------------------------
// console ioctl
int console_ioctl(int fd, unsigned long request, void *ptr) {
   return 0;
}
