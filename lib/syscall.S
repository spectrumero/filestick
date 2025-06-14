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
#define SYS_mount       40
#define SYS_hexdump     32

// Syscall wrappers
.text
.globl hexdump                // called directly, void return
hexdump:
   li       a7, SYS_hexdump   
   ecall
   ret
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
