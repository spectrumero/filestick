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

#define SYS_peek        21
#define SYS_printk      22
#define SYS_opendir     18
#define SYS_closedir    19
#define SYS_readdir     20
#define SYS_umount      39
#define SYS_mount       40
#define SYS_hexdump     32
#define SYS_exec_elf    24
#define SYS_malloc_init 34
#define SYS_malloc      35
#define SYS_realloc     36
#define SYS_free        37
#define SYS_brk         214
#define SYS_poll        75

// FS ops
#define SYS_mkdir       1030
#define SYS_chdir       49

// Syscall wrappers
.text
.globl poll
poll:
   li    a7, SYS_poll
   ecall
   ret
.globl mkdir
mkdir:
   li    a7, SYS_mkdir
   j     syscall
.globl chdir
chdir:
   li    a7, SYS_chdir
   j     syscall
.globl sys_brk
sys_brk:
   li    a7, SYS_brk
   ecall
   ret
.globl sys_malloc_init
sys_malloc_init:
   li    a7, SYS_malloc_init
   ecall
   ret
.globl sys_malloc
sys_malloc:
   li    a7, SYS_malloc
   ecall
   ret
.globl sys_realloc
sys_realloc:
   li    a7, SYS_realloc
   ecall
   ret
.globl sys_free
sys_free:
   li    a7, SYS_free
   ecall
   ret
.globl hexdump                // called directly, void return
hexdump:
   li       a7, SYS_hexdump   
   ecall
   ret
.globl exec_elf
exec_elf:
   li       a7, SYS_exec_elf
   j        syscall
.globl fd_peek                // called directly
fd_peek:
   li       a7, SYS_peek
   j        syscall
.globl printk                 // called directly
printk:
   li       a7, SYS_printk
   j        syscall
.globl mount                  // called directly
mount:
   li       a7, SYS_mount
   j        syscall
.globl umount
umount:
   li       a7, SYS_umount
   j        syscall
.globl opendir                // called directly
opendir:
   li       a7, SYS_opendir
   j        syscall
.globl closedir               // called directly
closedir:
   li       a7, SYS_closedir
   j        syscall
.globl _readdir               // needs args and return val rearranging
_readdir:
   li       a7, SYS_readdir

.globl syscall
syscall:                      // Syscall number in a7
   ecall
   bgez     a0, .sys_done     // OK if return >= 0
   addi     sp, sp, -16
   sw       ra, 12(sp)
   sw       s0, 8(sp)
   mv       s0, a0            // save error number
   call     __errno           // get address of errno
   neg      s0, s0            // errno is positive, so negate return code
   sw       s0, 0(a0)         // store in errno
   li       a0, -1            // rc = -1

   lw       s0, 8(sp)
   lw       ra, 12(sp)
   addi     sp, sp, 16
.sys_done:
   ret
