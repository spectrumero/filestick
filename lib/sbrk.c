#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define HEAPSIZE 4096
/*
static uint8_t mem[HEAPSIZE];
static uint8_t *memptr = mem;
static uint8_t *mem_end=mem + HEAPSIZE;

void *_sbrk(ptrdiff_t incr) {
   uint8_t *ptr = memptr;
   if(memptr + incr > mem_end) {
      errno = ENOMEM;
      return (caddr_t)-1;
   }

   memptr += incr;
   return (caddr_t)ptr; 
}*/
#define SYS_BRK   214

static uint8_t *memptr = NULL;

void *_sbrk(ptrdiff_t incr) {

   if(memptr == NULL) {
      register uint32_t  syscall asm("a7") = SYS_BRK;
      register uint32_t  arg     asm("a0") = 0;
      register uint8_t  *addr    asm("a0");
      asm volatile("ecall"
            : "=r"(addr)
            : "r"(arg), "r"(syscall));
      memptr = addr;
   }

   register uint32_t  syscall asm("a7") = SYS_BRK;
   register uint8_t  *arg     asm("a0") = memptr + incr;
   register uint8_t  *addr    asm("a0");
   asm volatile("ecall"
         : "=r"(addr)
         : "r"(arg), "r"(syscall));

   if(addr == memptr) {
      errno = ENOMEM;
      return (caddr_t)-1;
   }

   uint8_t *ptr = memptr;
   memptr = addr;
   return ptr;
}
