#
#The MIT License
#
#Copyright (c) 2023 Dylan Smith
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.
#

# FIXME: There will probably be more stuff in here, but scause is not yet
# implemented, so arriving here will always be from an ecall instruction
.include "syscalls.inc"

.option arch, +zicsr
.text
.globl super_trap
super_trap:
   csrw     sscratch, sp      # store userland stack pointer
   la       sp, __stack_top   # set up supervisor stack

   call     syscall_handler

   csrr     sp, sscratch      # restore userland stack pointer
   sret

##-------------------------------------------------------------------------
## Handle syscalls. Syscall number is in a7.
.globl syscall_handler
syscall_handler:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   la       ra, .syscall_done # always return here
   
   # TODO: syscalls should be table based, but there aren't many yet
   li       t0, SYSCALL_write 
   bne      t0, a7, .L0
   j        SYS_write
.L0:
.bad_syscall:
   li       a0, -2000         # TODO: error number
.syscall_done:
   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

