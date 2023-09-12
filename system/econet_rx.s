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

# Handle reception of econet frames.
# An interrupt is only triggered if the frame has a valid FCS (checksum)
# and is for our station.

.set     state_waitscout,  0
.set     state_waitdata,   1

.text

# ISR has already pushed a0, a1

# On entry a0 = device_base address
.globl econet_rx
econet_rx:
   addi     sp, sp, -32
   sw       ra, 28(sp)
   sw       s1, 24(sp)
   sw       s2, 20(sp)
   sw       s3, 16(sp)
   sw       a2, 12(sp)

   la       a1, econet_handshake_state
   lw       s1, 0(a1)            # s1 = state
   beqz     s1, .scout_ack       # state == state_waitscout
   addi     s1, s1, -1           # next state = state_waitdata
   beqz     s1, .data_ack        # state == state_waitdata, s1 reset to state_waitscout

.econet_rx_done:
   lw       a2, 12(sp)
   lw       s3, 16(sp)
   lw       s2, 20(sp)
   lw       s1, 24(sp)
   lw       ra, 28(sp)
   addi     sp, sp, 32
   ret

.scout_ack:
   lw       s2, 0x108(a0)        # get the frame size
   addi     s2, s2, -8           # which should be 8 bytes
   bnez     s2, .econet_rx_done  # if not bale out now TODO: send some kind of nack?
   lb       s2, 0x114(a0)        # get the port from the scout frame
   la       a2, econet_port_list
   add      a2, a2, s2
   lb       s3, 0(a2)            # get the file descriptor for the port
   beqz     s3, .econet_rx_done  # fd=0, port not open

   li       s1, state_waitdata   # update state
   sw       s1, 0(a1)
   sw       s2, 4(a1)            # set econet_pending_port

.econet_ack:
   lw       s1, 0x110(a0)        # get reply address word
   la       a2, 0x820000         # transmit buffer
   sw       s1, 0(a2)            # reply address word makes the scout ack frame
   sw       zero, 0x200(a0)      # tx buf start
   li       s1, 3                # tx buf end
   sw       s1, 0x204(a0)        # tx buf end, setting this causes transmission
   j        .econet_rx_done

.data_ack:
   la       a2, econet_port_list
   lw       s2, 4(a1)            # get econet_pending_port
   add      a2, a2, s2           # set a2 = econet_port_list entry
   lb       s3, 0(a2)            # get file descriptor
   ori      s3, s3, 0x80         # set high bit to flag is_ready
   sb       s3, 0(a2)
   sw       s1, 0(a1)            # s1 = state_waitscout
   lw       s1, 0x100(a0)        # get buffer start offset
   sw       s1, 8(a1)            # save in econet_buf_start
   lw       s1, 0x108(a0)        # get bytes received length
   sw       s1, 12(a1)           # save in econet_buf_len
   j        .econet_ack          # send ack frame
   
.data
.align 4
.globl econet_handshake_state       # offset 0
econet_handshake_state: .word 0
.globl econet_pending_port          # offset 4
econet_pending_port:    .word 0
.globl econet_buf_start             # offset 8
econet_buf_start:       .word 0
.globl econet_buf_len               # offset 12
econet_buf_len:         .word 0
.globl econet_port_list
econet_port_list:
.fill 256, 1, 0
