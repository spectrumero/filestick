/*
;The MIT License
;
;Copyright (c) 2025 Dylan Smith
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <syscall.h>

#include "icommands.h"

static bool hextoint(const char *hexstr, uint32_t *val);

void
i_peek(int argc, char **argv)
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

void
i_poke(int argc, char **argv)
{
   uint32_t addr;
   uint32_t val;

   if(argc != 4) {
      printk("usage: poke [w|h|b] [address] [value]\n");
      return;
   }

   if(!hextoint(argv[2], &addr)) {
      printk("invalid address\n");
      return;
   }
   if(!hextoint(argv[3], &val)) {
      printk("invalid value\n");
      return;
   }

   char sz = argv[1][0];
   switch(sz) {
      case 'w':
      {
         uint32_t *a = (uint32_t *)addr;
         *a = val;
         break;
      }
      case 'h':
      {
         uint16_t *a = (uint16_t *)addr;
         *a = (uint16_t)val;
         break;
      }
      case 'b':
      {
         uint8_t *a= (uint8_t *)addr;
         *a = (uint8_t)val;
         break;
      }
      default:
         printk("invalid size\n");
   }
}

static bool 
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

