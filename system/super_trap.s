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
   sw       a7, 8(sp)

   addi     a7, a7, -SYSCALL_lowest
   bltz     a7, .invalid_syscall
   li       t0, syscall_table_sz
   bge      a7, t0, .syscall_high

   la       t0, syscall_table
.find_syscall:
   add      t0, t0, a7           # get table entry
   lb       t1, 0(t0)
   beqz     t1, .invalid_syscall # must be nonzero
   slli     t1, t1, 2            # multiply by 4 to get call offset
   la       t2, syscall_address
   add      t2, t2, t1           # add offset
   lw       t3, 0(t2)            # get table entry
   jalr     ra, 0(t3)            # Make syscall
   j        .syscall_done

.syscall_high:
   lw       a7, 8(sp)
   addi     a7, a7, -SYSCALL_hi_lowest
   bltz     a7, .nontable_syscall   # between the tables
   li       t0, syscall_high_table_sz
   bge      a7, t0, .invalid_syscall
   la       t0, syscall_high_table
   j        .find_syscall

.nontable_syscall:
   lw       a7, 8(sp)
   li       t0, 214              # brk
   bne      a7, t0, .next_nontable_1
   call     SYS_brk
   j        .syscall_done
.next_nontable_1:

.invalid_syscall:
   li       a0, -2000

.syscall_done:
   lw       a7, 8(sp)
   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

