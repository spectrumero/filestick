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

# UART low level driver

#include "devices.h"
#include "sysdefs.h"

.globl serial_putc
serial_putc:
   li    t0, '\n'                # newline?
   bne   t0, a0, .write_byte     # no

   addi  sp, sp, -16             # Send \r\n
   sw    ra, 12(sp)
   li    a0, '\r'
   call  .write_byte
   li    a0, '\n'
   call  .write_byte
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret
   
.write_byte:

.busy:
   la t0, DEV_BASE         
   lw t1, OFFS_UARTSTATE(t0)  # uart state
   andi t1, t1, 2             # apply write busy mask
   bne t1, zero, .busy        # wait until uart is free

//   li t1, 100                 # delay - there's a bug in the UART that means it prematurely
                              # is not busy!
//.delay:
//   addi t1, t1, -1
//   bnez t1, .delay
   
   sw a0, OFFS_UART(t0)       # send data 
   ret

