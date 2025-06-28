//
//The MIT License
//
//Copyright (c) 2023 Dylan Smith
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.
//

#include "devices.h"
#include "sys/console.h"

#define OFFS_BUFINDEX      0
#define OFFS_BUFSTART      4
#define OFFS_CRINDEX       8
#define OFFS_CONS_RX_COUNT 12
#define OFFS_CONS_CONTROL  16

// max bytes in buffer before sending flow ctl
#define FLOW_CONTROL_MAX   250   

// a0 = device base, a1 = console uart valid bit
.text
.globl console_rx
console_rx:
   addi  sp, sp, -32
   sw    ra, 28(sp)
   sw    s1, 12(sp)
   sw    a2, 8(sp)
   sw    a3, 4(sp)
   sw    a1, 0(sp)

   lb    s1, OFFS_UART(a0)          # get uart byte - resets data received flag

   la    a0, __cons_buff
   la    a1, bufindex
   lw    a2, OFFS_BUFINDEX(a1)      # get current buffer index
   andi  t0, a2, 0xFF               # get wrapped ringbuffer index

   lw    t1, OFFS_CONS_CONTROL(a1)  # get control word
   andi  t1, t1, CONSOLE_RAW        # raw set?
   bnez  t1, .console_raw    

   li    a3, 127                    # backspace?
   beq   a3, s1, .bkspc

   li    a3, 0x0d                   # carriage return?
   beq   a3, s1, .cr

   add   a0, a0, t0                 # set memory address for byte store
   addi  a2, a2, 1                  # increment buffer index
   lw    a3, OFFS_BUFSTART(a1)      # get starting index
   andi  t0, a2, 0xFF               # wrap the new buffer index
   andi  t1, a3, 0xFF               # wrap the starting index
   beq   t0, t1, .bell              # buffer is full

   sw    a2, OFFS_BUFINDEX(a1)      # store new buffer index
   sb    s1, 0(a0)                  # store byte in the console buffer

   mv    a0, s1
   call  raw_putc                   # echo character

.cons_rx_done:
   lw    a1, 0(sp)
   lw    a3, 4(sp)
   lw    a2, 8(sp)
   lw    s1, 12(sp)
   lw    ra, 28(sp)   
   addi  sp, sp, 32
   ret

.bkspc:
   lw    a3, OFFS_BUFSTART(a1)      # bufstart
   beq   a2, a3, .bell              # nothing to do, at buffer start
   addi  a2, a2, -1                 # go back a space
   sw    a2, OFFS_BUFINDEX(a1)      # store it in bufindex

   la    a0, DEV_BASE
   li    a3, 8                      # backspace
   sw    a3, OFFS_UART(a0)
   j     .cons_rx_done

.cr:
   sw    t0, OFFS_CRINDEX(a1)       # cr_index - t0 still points at the wrapped buffer index
   lw    a0, OFFS_CONS_RX_COUNT(a1) # get current cons_rx_count
   addi  a0, a0, 1                  # increment received count
   sw    a0, OFFS_CONS_RX_COUNT(a1) # cons_rx_count
   li    a0, '\n'                   # echo CR
   call  serial_putc
   j     .cons_rx_done

.bell:
   li    a0, 7                      # ctrl-G
   call  raw_putc
   j     .cons_rx_done

.console_raw:
   add   a0, a0, t0                 # set memory address for byte store
   addi  a2, a2, 1                  # increment buffer index
   lw    a3, OFFS_BUFSTART(a1)      # get starting index
   andi  t0, a2, 0xFF               # wrap the new buffer index
   andi  t1, a3, 0xFF               # wrap the starting index
   beq   t0, t1, .flow_control      # buffer is full - all we can do is throw the byte out

   sw    a2, OFFS_BUFINDEX(a1)      # store new buffer index
   sb    s1, 0(a0)                  # store received byte in the buffer

   addi  a3, a3, FLOW_CONTROL_MAX
   bgeu  a3, a2, .cons_rx_done      # not in the danger zone, no flow control needed

.flow_control:
   lw    t0, OFFS_CONS_CONTROL(a1)  # get control word
   andi  t1, t0, CONSOLE_FLOW_XOFF  # xon/xoff flow control?
   bnez  t1, .xoff                  # note: other half of driver handles XON
                                    # TODO: more flow control options

   j     .cons_rx_done              # no flow control

.xoff:
   li    a0, FLOW_XOFF
   call  raw_putc
   j     .cons_rx_done
   

.data
.align 4
.globl bufindex         # where next byte should be written (offset 0)
bufindex: .word 0
.globl bufstart         # where the ring buffer starts      (offset 4)
bufstart:  .word 0
.globl cr_index         # where the carriage return is      (offset 8)
cr_index:   .word 0
.globl cons_rx_count    # console receive count             (offset C)
cons_rx_count: .word 0
.globl cons_control     # console control flags             (offset 10)
cons_control: .word 0

