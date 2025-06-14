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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>

#include "console.h"
#include "fd.h"
#include "raw_econet.h"
#include "sysdefs.h"
#include <sys/econet.h>

#include "printk.h"

extern volatile struct econet_state econet_state_val;
extern volatile uint32_t econet_handshake_state;
extern volatile uint32_t econet_pending_port;
extern volatile uint32_t econet_buf_start;
extern volatile size_t econet_buf_len;
extern volatile uint32_t econet_tx_start;
extern volatile uint32_t econet_tx_end;
extern volatile uint32_t econet_tx_status;
extern volatile uint32_t econet_timeout_state;
extern volatile uint32_t econet_monitor_frames;
extern volatile uint8_t econet_port_list[256];

// Hardware registers
static volatile uint32_t *econet_state    = (uint32_t *)0x80011c;  // reg_status
static volatile uint8_t  *econet_mon      = (uint8_t  *)0x80011d;
static volatile uint32_t *tx_start_offset = (uint32_t *)0x800200;
static volatile uint32_t *tx_end_offset   = (uint32_t *)0x800204;
static volatile uint32_t *tx_flags        = (uint32_t *)0x800208;
static volatile uint32_t *timer_a_val     = (uint32_t *)0x800304;
static volatile uint32_t *timer_a_stat    = (uint32_t *)0x800308;
static volatile uint32_t *econet_clkterm  = (uint32_t *)0x800320;

uint8_t fd_rx_portmap[MAX_FILE_DESCRIPTORS];
struct econet_addr fd_tx_destmap[MAX_FILE_DESCRIPTORS];

uint16_t econet_address;

static FDfunction econet_func = {
   .fd_read = econet_read,
   .fd_write = econet_write,
   .fd_lseek = NULL,
   .fd_fstat = NULL,
   .fd_ioctl = econet_ioctl,
   .fd_close = econet_close,
   .fd_peek  = econet_peek
};

static uint32_t *addr_set = (uint32_t *)0x800118;

static uint32_t last_monitor_frames = 0;

// Internal functions
static int econet_set_rx_port(int fd, uint8_t port);     // sets recvfrom port
static int econet_set_addr(uint16_t netstation);         // sets our net and station number
static int econet_set_tx_addr(int fd, struct econet_addr *dest);
static ssize_t econet_monitor(int fd, void *ptr, size_t count); 
static int econet_set_clkterm(uint16_t flags); 

static uint32_t *led = (uint32_t *)0x800000;

void econet_init() {
   memset(fd_rx_portmap, 0, sizeof(fd_rx_portmap));
   memset(fd_tx_destmap, 0, sizeof(fd_tx_destmap));
   econet_address = 0;
}

int econet_open(const char *devname, int flags, mode_t mode, FD *fd) {
   fd->fdfunc = & econet_func;
   return 0;
}

int econet_ioctl(int fd, unsigned long request, void *ptr) {
   uint32_t req_id = request & 0xFF000000;

   switch(req_id) {
      case ECONET_SET_ADDR:
         return econet_set_addr((uint16_t)(request & 0xFFFF));
      case ECONET_SET_RECV_PORT:
         return econet_set_rx_port(fd, request & 0xFF);
      case ECONET_SET_SEND_ADDR:
         return econet_set_tx_addr(fd, ptr);
      case ECONET_SET_MONITOR:
         econet_monitor_frames = 0;
         last_monitor_frames = 0;
         *econet_mon = (uint8_t)(request & 0xFF);
         return 0;
      case ECONET_SET_CLKTERM:
         return econet_set_clkterm(request & 0xFFFF);
      case ECONET_DBG_BUF:
         memcpy(ptr, (uint8_t *)&econet_state_val, sizeof(struct econet_state));
         return 0;
      default:
         printk("econet_ioctl: bad request\n");
         return -EINVAL;
   }

   return -EINVAL;
}

ssize_t econet_read(int fd, void *ptr, size_t count) {
   if(*econet_mon) return econet_monitor(fd, ptr, count);

   uint8_t port = fd_rx_portmap[fd];
   if(!port)
      return -EINVAL;

   // wait for a valid data frame
   while(!(econet_port_list[port] & 0x80));
   size_t copy_sz = econet_buf_len > count ? count : econet_buf_len;

   // FIXME: define for buffer address
   uint8_t *bufptr = ((uint8_t *)0x810000) + econet_buf_start;
   memcpy(ptr, bufptr, copy_sz);

   if(copy_sz < econet_buf_len) {
      econet_buf_len -= copy_sz;
      econet_buf_start += copy_sz;
   }
   else {
      // this resets 'valid data ready' flag
      econet_port_list[port] = fd;
   }

   return copy_sz;
}

static ssize_t econet_monitor(int fd, void *ptr, size_t count) {
   // wait for a frame
   while(econet_monitor_frames == last_monitor_frames);

   size_t copy_sz = econet_buf_len > count ? count : econet_buf_len;

   // FIXME: define for buffer address
   uint8_t *bufptr = ((uint8_t *)0x810000) + econet_buf_start;
   memcpy(ptr, bufptr, copy_sz);

   if(copy_sz < econet_buf_len) {
      econet_buf_len -= copy_sz;
      econet_buf_start += copy_sz;
   }
   else {
      // done with this frame, account for it
      last_monitor_frames++;
   }

   return copy_sz;
}

ssize_t econet_peek(int fd)
{
   uint8_t port = fd_rx_portmap[fd];
   if(!port)
      return -EINVAL;

   // no data
   if(!(econet_port_list[port] & 0x80)) return 0;

   return econet_buf_len; 
}

ssize_t econet_write(int fd, const void *ptr, size_t count) {
   uint8_t *scout_buf = (uint8_t *)0x820000;
   uint8_t *data_buf  = (uint8_t *)0x820008;

   // Validate the size
   if(count > ECONET_TXBUFSZ - 8) return -EMSGSIZE;

   // Reset transmit flags
   econet_tx_status = 0;
   econet_timeout_state = 0;

   // This is not a turnaround (reply)
   *tx_flags = 0;

   // Validate that there is a valid destination
   struct econet_addr *dest = &fd_tx_destmap[fd];
   if(dest->station == 0) return -EDESTADDRREQ;

   // wait until idle:
   // check receiving and frame_valid flags (the latter is reset only once the ISR
   // has acknowledged a frame), and that handshake state is idle
   *led = 0;
   while(*econet_state & 3 || econet_handshake_state > STATE_WAITSCOUT);
   DISABLE_INTERRUPTS

   // Create the address word
   uint32_t addr = dest->station | dest->net << 8 | econet_address << 16;

   // Set up the scout frame
   *((uint32_t *)scout_buf) = addr;
   *(scout_buf + 4) = 0x80;            // scout flag
   *(scout_buf + 5) = dest->port;

   // Set up the data frame
   *((uint32_t *)data_buf)  = addr;
   memcpy(data_buf + 4, ptr, count);
   econet_tx_start = 8;          // start offset in transmit buffer for data frame
   econet_tx_end   = 11 + count; // index of last byte of transmit buffer for data frame

   // Set up the timeout for the scout frame
   *timer_a_val = TIMER_HUNDRED_MS;
   *timer_a_stat = TIMER_ENABLE|TIMER_RESET;

   // Transmit the scout frame. The ISR will handle the handshaking.
   // Setting the end offset will trigger the transmission of the scout frame.
   econet_handshake_state = STATE_TXSCOUT;
   *tx_start_offset = 0;
   *tx_end_offset   = 5;      // index of last byte of scout frame
   ENABLE_INTERRUPTS

   // TODO: nonblocking writes
   while(econet_handshake_state >= STATE_TXSCOUT);

   if(econet_tx_status != STATUS_TXDONE) {
      // Not listening
      if(econet_timeout_state == ECONET_STATE_TXSCOUT) 
         return -EHOSTUNREACH;

      // got scout ack but no data ack
      return -ETIMEDOUT;
   }

   return count;
}

int econet_close(int fd) {
   uint8_t port = fd_rx_portmap[fd];
   if(port) {
      econet_port_list[port] = 0;
      fd_rx_portmap[fd] = 0;
   }
   return 0;
}

static int econet_set_rx_port(int fd, uint8_t port) {
   // TODO: error handling
   fd_rx_portmap[fd] = port;

   // indicates to the ISR that the port is being listened
   econet_port_list[port] = fd;
   return 0;
}

static int econet_set_tx_addr(int fd, struct econet_addr *dest) {
   fd_tx_destmap[fd].port = dest->port;
   fd_tx_destmap[fd].net = dest->net;
   fd_tx_destmap[fd].station = dest->station;

   return 0;
}

// netstation is MSB=network LSB=station
static int econet_set_addr(uint16_t netstation) {
   econet_address = netstation;
   *addr_set = netstation;       // sets address in receiver hardware
   return 0;
}

// bits 10-8 set the clock divider
// bit 1 enables/disables the terminator
// bit 0 enables/disables the clock
static int econet_set_clkterm(uint16_t flags) {
   *econet_clkterm = flags;
   return 0;
}
