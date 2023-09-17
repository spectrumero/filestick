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

# Interrupt service routine
.include "devices.inc"

.text
.globl isr_trap
isr_trap:
   addi     sp, sp, -16
   sw       ra, 12(sp)
   sw       a1, 8(sp)
   sw       a0, 4(sp)

   la       ra, .isr_exit     # return here
   la       a0, device_base

   lw       a1, 0x11c(a0)     # econet receive status offset
   andi     a1, a1, 1         # frame valid waiting bit
   bnez     a1, econet_rx
   lw       a1, 16(a0)        # console uart state
   andi     a1, a1, 1         # uart_valid bit
   bnez     a1, console_rx

   lw       a1, 0x308(a0)     # econet timer A status
   andi     a1, a1, 1
   bnez     a1, econet_timeout

   lw       a0, 8(a1)         # timer state
   bnez     a0, timer_done 

.isr_fell_through:
   la       a0, device_base
   li       a1, 7
   sw       a1, 0(a0)
   
.isr_exit:   
   lw       a0, 4(sp)
   lw       a1, 8(sp)
   lw       ra, 12(sp)
   addi     sp, sp, 16
   mret

