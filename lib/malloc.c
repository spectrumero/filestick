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

// Malloc using the built-in malloc in system, rather than the libc
// supplied malloc.

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include "syscall.h"

#define MEMTOP       0x20800
#define DEFAULT_BRK  MEMTOP - 2048

// Internal malloc functions
void *sys_brk(void *addr);
void *sys_malloc_init(void *mem, size_t bytes);
void *sys_malloc(void *mem, size_t bytes);
void *sys_realloc(void *mem, void *ptr, size_t bytes);
void sys_free(void *mem, void *ptr);

uint8_t *mempool = NULL;
//#define DEBUG_MALLOC 
// ---------------------------------------------------------
// Set up the allocator
static bool setup_malloc(void)
{
   uint8_t *requested_brk = (uint8_t *)DEFAULT_BRK;
   mempool = sys_brk(0);
   uint8_t *actual_brk = sys_brk(requested_brk);
   size_t bytes = actual_brk - mempool;

   if(actual_brk > mempool) {
      void *pool = sys_malloc_init(mempool, bytes);
      if(pool) {
#ifdef DEBUG_MALLOC
         printk("mempool: %x brk=%x size=%d bytes, init=%x\n", mempool, actual_brk, bytes, pool);
#endif
         return true;
      }
      else {
         mempool = NULL;
         return false;
      }
   }
   
   mempool = NULL;
   return false;
}

// --------------------------------------------------------
// Setup malloc with a user-defined pool
bool setup_malloc_pool(void *mem, size_t size)
{
   if(sys_malloc_init(mem, size)) {
      mempool = mem;
      return true;
   }
   return false;
}

// --------------------------------------------------------
// malloc
void *__wrap__malloc_r(struct _reent *r, size_t size)
{
#ifdef DEBUG_MALLOC
   printk("wrap_malloc_r mempool = %x\n", mempool);
#endif
   if(!mempool) {
      if(!setup_malloc()) return NULL;
   }
   void *ptr = sys_malloc(mempool,size);
#ifdef DEBUG_MALLOC
   printk("malloc size = %d ptr = %x\n", size, ptr);
#endif
   return ptr;
}

// --------------------------------------------------------
// realloc
void *__wrap__realloc_r(struct _reent *r, void *ptr, size_t size)
{
   if(!mempool) {
      if(!setup_malloc()) return NULL;
   }
   return sys_realloc(mempool, ptr, size);
}

// --------------------------------------------------------
// calloc
void *__wrap__calloc_r(struct _reent *r, size_t nmemb, size_t size)
{
   if(!mempool) {
      if(!setup_malloc()) return NULL;
   }
   
   size_t bytes = nmemb * size;
   void *m = sys_malloc(mempool, bytes);
   if(m) {
      memset(m, 0, bytes);
   }

   return m;
}

// --------------------------------------------------------
// free
void __wrap__free_r(struct _reent *r, void *ptr)
{
   sys_free(mempool, ptr);
}

