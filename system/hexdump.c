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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "printk.h"
#include "console.h"
#include "hexdump.h"

// Supervisor debugging commands
void
super_hexdump(int argc, char **argv)
{
   uint32_t start;
   uint32_t len;

   if(argc != 3) {
      printk("usage: hexdump [hex address] [hex length]\n");
      return;
   }

   if(!hextoint(argv[1], &start)) {
      printk("invalid start address\n");
      return;
   }

   if(!hextoint(argv[2], &len)) {
      printk("invalid length\n");
      return;
   }

   hexdump((void *)start, len, start);
}

void
super_peek(int argc, char **argv)
{
   uint32_t addr;
   if(argc != 3) {
      printk("usage: peek [w|h|b] [address]\n");
      return;
   }

   if(!hextoint(argv[2], &addr)) {
      printk("invalid address\n");
      return;
   }

   char sz = argv[1][0];
   switch(sz) {
      case 'w':
      {
         uint32_t *v = (uint32_t *)addr;
         printk("%08x\n", *v);
         break;
      }
      case 'h':
      {
         uint16_t *v = (uint16_t *)addr;
         printk("%04x\n", *v);
         break;
      }
      case 'b':
      {
         uint8_t *v = (uint8_t *)addr;
         printk("%02x\n", *v);
         break;
      }
      default:
         printk("invalid size\n");
   }
}

bool 
hextoint(const char *hexstr, uint32_t *val)
{
   *val = 0;
   int shift = 0;
   uint32_t tmpval;

   for(int i = strlen(hexstr) - 1; i >=0; i--) {
      char c = hexstr[i];
      if(c >= '0' && c <= '9')
         tmpval = c - '0';
      else if(c >= 'a' && c <= 'f')
         tmpval = c - 'a' + 10;
      else
         return false;

      *val |= (tmpval << shift);
      shift += 4;
   }

   return true;
}

//----------------------------------------------------------------------------
// Hexdump some memory
// addr is the address to start printing on the left side.
static void 
hexline(uint8_t *buf, int size) 
{
    int i;

    for(i = 0; i < size; i++) {
        printk("%02x ", *buf++);
        if(i == 7) serial_putc(' ');
    }

    if(size < 16) {
        for(; i < 16; i++) {
            printk("   ");
            if(i == 7) serial_putc(' ');
        }
    }
}

static void 
printchars(uint8_t *buf, int size) 
{
   uint8_t i;
   serial_putc('|');
   for(i=0; i<size; i++) {
      if(*buf > 31 && *buf < 127) 
         serial_putc(*buf);
      else
         serial_putc('.');
      buf++;
   }
   printk("|\n");
}

void 
hexdump(const void *ptr, int size, uint32_t addr) 
{
   uint16_t linelen;
   //uint16_t count=0;
   uint8_t *buf = (uint8_t *)ptr;

   do {
       printk("%08x   ", addr);
       linelen = size >= 16 ? 16 : size;
       hexline(buf, linelen);
       printchars(buf, linelen);

       size -= 16;
       buf += 16;
       addr += 16;
   } while(size > 0);
}

