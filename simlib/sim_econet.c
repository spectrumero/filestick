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

// Internal functions
static int econet_set_rx_port(int fd, uint8_t port);     // sets recvfrom port
static int econet_set_addr(uint16_t netstation);         // sets our net and station number
static int econet_set_tx_addr(int fd, struct econet_addr *dest);

static FDfunction econet_func = {
   .fd_read = econet_read,
   .fd_write = econet_write,
   .fd_lseek = NULL,
   .fd_fstat = NULL,
   .fd_ioctl = econet_ioctl,
   .fd_close = econet_close
};

void
econet_init()
{
}

int 
econet_open(const char *devname, int flags, mode_t mode, FD *fd)
{
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
      case ECONET_DBG_BUF:
         //memcpy(ptr, (uint8_t *)&econet_state_val, sizeof(struct econet_state));
         return 0;
      default:
         printk("econet_ioctl: bad request\n");
         return -EINVAL;
   }

   return -EINVAL;
}

ssize_t econet_read
(int fd, void *ptr, size_t count) 
{
   return 0;
}

ssize_t 
econet_write(int fd, const void *ptr, size_t count) 
{
   return 0;
}

int econet_close(int fd) {
   /*
   uint8_t port = fd_rx_portmap[fd];
   if(port) {
      econet_port_list[port] = 0;
      fd_rx_portmap[fd] = 0;
   }*/
   return 0;
}

static int econet_set_rx_port(int fd, uint8_t port) {
   /*
   // TODO: error handling
   fd_rx_portmap[fd] = port;

   // indicates to the ISR that the port is being listened
   econet_port_list[port] = fd;*/
   return 0;
}

static int econet_set_tx_addr(int fd, struct econet_addr *dest) {
   /*
   fd_tx_destmap[fd].port = dest->port;
   fd_tx_destmap[fd].net = dest->net;
   fd_tx_destmap[fd].station = dest->station;*/

   return 0;
}

// netstation is MSB=network LSB=station
static int econet_set_addr(uint16_t netstation) {
   /*
   econet_address = netstation;
   *addr_set = netstation;       // sets address in receiver hardware
   */
   return 0;
}

