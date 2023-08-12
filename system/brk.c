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
#include <console.h>

#include "brk.h"

#define GP_SPACE     0x800

static void *program_brk = (void *)0x10000;
static void *min_brk = (void *)0x10000;
static uint8_t **user_sp = (uint8_t **)0xFEFC;  // FIXME export linker symbol

//--------------------------------------------------------------------------
// Set when loading an ELF file from memory size information
void set_min_brk(void *addr) {
   min_brk = addr;
}

//--------------------------------------------------------------------------
// Set the program break
void *SYS_brk(void *addr) {

   // Space must be left for the global pointer, if used.
   register void *gp asm("gp");
   void *min_gp = gp + GP_SPACE;
   if(min_brk < min_gp) min_brk = min_gp;
   if(program_brk < min_brk) program_brk = min_brk;

   // Maximum program break should always leave some space for the stack
   void *max_brk = (*user_sp) - 1024;

   // Break request must be within a valid range which is: above the GP
   // or min brk, and sufficiently below the current user stack pointer.
   if(addr > max_brk || addr < min_brk) return program_brk;

   program_brk = addr;
   return addr;   
}

