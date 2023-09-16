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

# a0 = device base, a1 = console uart valid bit
.text
.globl console_rx
console_rx:
   addi  sp, sp, -32
   sw    ra, 28(sp)
   sw    s1, 12(sp)
   sw    a2, 8(sp)
   sw    a3, 4(sp)
   sw    a1, 0(sp)

   lb    s1, 12(a0)     # get uart byte - resets data received flag

   la    a0, __cons_buff
   la    a1, bufindex
   lw    a2, 0(a1)      # get current buffer index

   li    a3, 127        # backspace?
   beq   a3, s1, .bkspc

   li    a3, 0x0d       # carriage return?
   beq   a3, s1, .cr

   add   a0, a0, a2     # set memory address for byte store
   addi  a2, a2, 1      # increment buffer index
   andi  a2, a2, 0xFF   # make sure it wraps around
   lw    a3, 4(a1)      # get starting index
   beq   a3, a2, .cons_buffer_full # buffer is full

   sw    a2, 0(a1)      # store new buffer index
   sb    s1, 0(a0)      # store byte in the console buffer

   la    a0, 0x800000   # FIXME
   sw    s1, 0xC(a0)    # echo character

.cons_rx_done:
   lw    a1, 0(sp)
   lw    a3, 4(sp)
   lw    a2, 8(sp)
   lw    s1, 12(sp)
   lw    ra, 28(sp)   
   addi  sp, sp, 32
   ret

.cons_buffer_full:
   la    a0, 0x800000
   li    s1, 7
   sw    s1, 0(a0)
   j .cons_rx_done

.bkspc:
   lw    a3, 4(a1)      # bufstart
   beq   a2, a3, .cons_rx_done # nothing to do, at buffer start
   addi  a2, a2, -1     # go back a space
   andi  a2, a2, 0xFF   # wrap
   sw    a2, 0(a1)      # store it in bufindex

   la    a0, 0x80000C   # FIXME
   li    a3, 8          # backspace
   sw    a3, 0(a0)
   j     .cons_rx_done

.cr:
   addi  a2, a2, -1     # index of the CR is one byte before the bufindex
   andi  a2, a2, 0xFF
   sw    a2, 8(a1)      # cr_index
   lw    a0, 12(a1)     # get current cons_rx_count
   addi  a0, a0, 1      # increment received count
   sw    a0, 12(a1)     # cons_rx_count
   li    a0, '\n'       # echo CR
   call  serial_putc
   j     .cons_rx_done

.data
.align 4
.globl bufindex         # where next byte should be written
bufindex: .word 0
.globl bufstart         # where the ring buffer starts
bufstart:  .word 0
.globl cr_index         # where the carriage return is
cr_index:   .word 0
.globl cons_rx_count    # console receive count
cons_rx_count: .word 0

