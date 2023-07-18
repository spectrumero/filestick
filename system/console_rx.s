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
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    a2, 8(sp)

   lb    a1, 12(a0)     # get uart byte - resets data received flag
   sw    a1, 4(sp)      # store it

   la    a0, 0x20700    # FIXME
   la    a1, bufindex
   lw    a2, 0(a1)      # get current buffer index
   add   a0, a0, a2     # set buffer pointer
   addi  a2, a2, 1      # increment buffer index
   andi  a2, a2, 0xFF   # make sure it wraps around
   sw    a2, 0(a1)      # store it
   lw    a1, 4(sp)      # get the uart byte back
   sb    a1, 0(a0)      # store it in the console buffer

   lw    a2, 8(sp)
   lw    ra, 12(sp)   
   addi  sp, sp, 16
   ret

.data
.align 4
.globl bufindex
bufindex: .word 0
.globl curidx
curidx:  .word 0

