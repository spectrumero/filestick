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
#include <unistd.h>

#include "console.h"

static size_t console_hexline(void *buf, size_t count);
static void console_nibble(uint8_t nibble);

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

