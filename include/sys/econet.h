#ifndef SYS_ECONET_H
#define SYS_ECONET_H
/*
;The MIT License
;
;Copyright (c) 2023 Dylan Smith
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;THE SOFTWARE.
*/
#ifndef ASM
#include <stdint.h>

struct econet_addr {
   uint8_t        port;
   uint8_t        net;
   uint8_t        station;
};

struct econet_state {
   uint32_t       handshake_state;
   uint32_t       pending_port;
   uint32_t       rx_buf_start;
   uint32_t       rx_buf_len;
   uint32_t       tx_buf_start;
   uint32_t       tx_buf_end;
   uint32_t       tx_status;
   uint32_t       timeout_state;
};
#endif

#define ECONET_SET_ADDR       0x01000000
#define ECONET_SET_RECV_PORT  0x02000000
#define ECONET_SET_SEND_ADDR  0x03000000
#define ECONET_SET_MONITOR    0x04000000
#define ECONET_DBG_BUF        0xF0000000

// Low-level states
#define ECONET_STATE_WAITSCOUT   0
#define ECONET_STATE_WAITDATA    1
#define ECONET_STATE_TXSCOUT     2
#define ECONET_STATE_TXDATA      3

// Bit fields
#define ECONET_STATUS_TXDONE     1
#define ECONET_STATUS_TXNETERR   2

#endif

