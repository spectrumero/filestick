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
#include <errno.h>

#include "console.h"
#include "fd.h"
#include "raw_econet.h"

extern volatile uint32_t econet_handshake_state;
extern volatile uint32_t econet_pending_port;
extern volatile uint32_t econet_buf_start;
extern volatile size_t econet_buf_len;
extern volatile uint8_t econet_port_list[256];

uint8_t fd_portmap[MAX_FILE_DESCRIPTORS];
uint16_t econet_address;

static FDfunction econet_func = {
   .fd_read = econet_read,
   .fd_write = NULL,
   .fd_lseek = NULL,
   .fd_fstat = NULL,
   .fd_ioctl = econet_ioctl,
   .fd_close = econet_close
};

static uint32_t *addr_set = (uint32_t *)0x800118;

// Internal functions
static int econet_set_port(int fd, uint8_t port);
static int econet_set_addr(uint16_t netstation);

void econet_init() {
   memset(fd_portmap, 0, sizeof(fd_portmap));
   econet_address = 0;
}

int econet_open(const char *devname, int flags, mode_t mode, FD *fd) {
   fd->fdfunc = & econet_func;
   return 0;
}

int econet_ioctl(int fd, unsigned long request, void *ptr) {
   uint32_t req_id = request & 0xFF000000;

   switch(req_id) {
      case SET_ADDR:
         return econet_set_addr((uint16_t)(request & 0xFFFF));
      case SET_RECV_PORT:
         return econet_set_port(fd, request & 0xFF);
      default:
         kerr_puts("econet_ioctl: bad request");
         return -EINVAL;
   }

   return -EINVAL;
}

int econet_read(int fd, void *ptr, size_t count) {
   uint8_t port = fd_portmap[fd];
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

int econet_close(int fd) {
   uint8_t port = fd_portmap[fd];
   if(port) {
      econet_port_list[port] = 0;
      fd_portmap[fd] = 0;
   }
   return 0;
}

static int econet_set_port(int fd, uint8_t port) {
   // TODO: error handling
   fd_portmap[fd] = port;

   // indicates to the ISR that the port is being listened
   econet_port_list[port] = fd;
   return 0;
}

// netstation is MSB=network LSB=station
static int econet_set_addr(uint16_t netstation) {
   econet_address = netstation;
   *addr_set = netstation;       // sets address in receiver hardware
   return 0;
}

