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

.set SCAUSE_ECALL, 0x8
.set SCAUSE_EBREAK, 0x3
.set SCAUSE_ILLEGAL, 0x2

.option arch, +zicsr
.text

##---------------------------------------------------------------------------
## supervisor trap entry point: don't use the temp registers because we could
## have got here via an illegal instruction
.globl super_trap
super_trap:
   csrw     sscratch, sp      # store userland stack pointer
   la       sp, __stack_top - 16   # set up supervisor stack
   sw       s1, 0(sp)         # save s1
   sw       s2, 4(sp)         # save s2
   sw       ra, 8(sp)
   csrr     s1, sscratch      # retrieve user stack ptr
   sw       s1, 12(sp)        # save it on the stack

   la       ra, .trap_done
   csrr     s1, scause
   li       s2, SCAUSE_ECALL
   beq      s1, s2, syscall_handler

   # save all the registers
   addi     sp, sp, -124
   sw       a0, 0(sp)
   sw       a1, 4(sp)
   sw       a2, 8(sp)
   sw       a3, 12(sp)
   sw       a4, 16(sp)
   sw       a5, 20(sp)
   sw       a6, 24(sp)
   sw       a7, 28(sp)
   sw       s0, 32(sp)
   lw       a0, 124(sp)       # original s1 
   sw       a0, 36(sp)
   lw       a0, 128(sp)       # original s2
   sw       a0, 40(sp)
   sw       s3, 44(sp)
   sw       s4, 48(sp)
   sw       s5, 52(sp)
   sw       s6, 56(sp)
   sw       s7, 60(sp)
   sw       s8, 64(sp)
   sw       s9, 68(sp)
   sw       s10, 72(sp)
   sw       s11, 76(sp)
   sw       t0, 80(sp)
   sw       t1, 84(sp)
   sw       t2, 88(sp)
   sw       t3, 92(sp)
   sw       t4, 96(sp)
   sw       t5, 100(sp)
   sw       t6, 104(sp)
   sw       gp, 108(sp)
   sw       tp, 112(sp)
   lw       a0, 132(sp)       # original ra
   sw       a0, 116(sp)
   lw       a0, 136(sp)       # user sp
   sw       a0, 120(sp)

   mv       a0, sp            # pass pointer to saved registers
   la       ra, .restore_stack

   li       s2, SCAUSE_EBREAK
   beq      s1, s2, ebreak_handler
   call     illegal_handler
.restore_stack:
   addi     sp, sp, 124

.trap_done:
   lw       s1, 0(sp)
   lw       s2, 4(sp)
   lw       ra, 8(sp)
   lw       sp, 12(sp)
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

