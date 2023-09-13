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
.set     state_txscout,    2
.set     state_txdata,     3

# bitfield values
.set     status_txdone,    1        # transmit completed OK
.set     status_txneterr,  2        # network error

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
   addi     s1, s1, -1           # next state = state_txscout
   beqz     s1, .rx_scout_ack    # remote station acked our scout
   addi     s1, s1, -1           # next state = state_txdata
   beqz     s1, .rx_data_ack

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
   addi     s1, s1, 2            # advance past our address
   sw       s1, 8(a1)            # save in econet_buf_start
   lw       s1, 0x108(a0)        # get bytes received length
   addi     s1, s1, -4           # remove FCS byte length and our addr byte len
   sw       s1, 12(a1)           # save in econet_buf_len
   j        .econet_ack          # send ack frame

# these routines are to handle acknowledgements when we are doing a 4 way handshake
.rx_scout_ack:
   lw       s2, 0x108(a0)        # get the frame size
   addi     s2, s2, -4           # which should be 4 bytes
   bnez     s2, .tx_error        # if not, bale out now. 
   # TODO: verify the ack is from the correct station!
   li       s1, state_txdata     # next state - transmit data packet
   sw       s1, 0(a1)
   lw       a2, 16(a1)           # data packet start offset
   sw       a2, 0x200(a0)
   lw       a2, 20(a1)           # data packet end offset
   sw       a2, 0x204(a0)        # kicks off transmission
   j        .econet_rx_done

.rx_data_ack:
   lw       s2, 0x108(a0)        # get the frame size
   addi     s2, s2, -4           # which should be 4 bytes
   bnez     s2, .tx_error        # if not, bale out now.
   sw       zero, 0(a1)          # set state back to idle (state_waitscout)
   la       s1, status_txdone
   sw       s1, 24(a1)           # set TX done status bit
   j        .econet_rx_done

# TODO: retry sending the 4 way handshake sequence
.tx_error:
   sw       zero, 0(a1)          # reset state
   la       s1, status_txneterr
   sw       s1, 24(a1)           # set status bit
   j        .econet_rx_done

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
.globl econet_tx_start              # offset 16
econet_tx_start:        .word 0    
.globl econet_tx_end                # offset 20
econet_tx_end:          .word 0
.globl econet_tx_status             # offset 24
econet_tx_status:       .word 0

.globl econet_port_list
econet_port_list:
.fill 256, 1, 0

