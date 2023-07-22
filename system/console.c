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

#include "console.h"
#include "fd.h"
#include "sysdefs.h"

static size_t console_hexline(void *buf, size_t count);
static void console_nibble(uint8_t nibble);

static FDfunction cons_func = {
   .fd_read  = console_read,
   .fd_write = console_write,
   .fd_close = NULL,
};

extern volatile uint32_t bufindex;
extern volatile uint32_t bufstart;
extern volatile uint32_t cr_index;
extern volatile uint32_t cons_rx_count;

uint8_t *cons_buf = (uint8_t *)0x20700; // FIXME FIXME FIXME!

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
// Write to the system console.
ssize_t console_write(int fd, void *buf, size_t count) {
   ssize_t rc = count;
   char *bufptr = (char *)buf;

   while(count--) {
      serial_putc(*bufptr++);
   }

   return rc;
}

//------------------------------------------------------------------
// Debug routines
void console_hexdump(void *buf, size_t count) {
   while(count) {
      count = console_hexline(buf, count);
      buf += 16;
      serial_putc('\r');
      serial_putc('\n');
   }
}

static size_t console_hexline(void *buf, size_t count) {
   size_t bytes = count > 16 ? 16 : count;
   uint8_t *ptr = (uint8_t *)buf;

   // Address
   console_hexword((uint32_t)buf);
   serial_putc(' ');
   serial_putc(' ');

   for(int i=0; i < bytes; i++) {
      console_hexbyte(*ptr++);
      serial_putc(' ');
      if(i == 7) serial_putc(' ');
   }

   serial_putc(' ');
   serial_putc('|');
   ptr -= bytes;
   for(int i=0; i < bytes; i++) {
      if(*ptr > 31 && *ptr < 128)
         serial_putc(*ptr);
      else
         serial_putc('.');
      ptr++;
   }
   serial_putc('|');

   return count-bytes;
}

void console_hexword(uint32_t word) {
   uint32_t mask = 0xF0000000;
   uint8_t shift = 28;
   while(mask) {
      uint8_t nibble=(word & mask) >> shift;
      shift -= 4;
      mask >>= 4;
      console_nibble(nibble);
   }
}

void console_hexbyte(uint8_t byte) {
   uint8_t hi=(byte & 0xF0) >> 4;
   uint8_t lo=byte & 0x0F;

   console_nibble(hi);
   console_nibble(lo);
}

static void console_nibble(uint8_t val) {
   if(val > 9)
      val='a'-10 + val;
   else
      val='0' + val;

   serial_putc(val);
}

