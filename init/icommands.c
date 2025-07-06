/*
;The MIT License
;
;Copyright (c) 2025 Dylan Smith
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/console.h>
#include <sys/dirent.h>
#include <syscall.h>
#include <errno.h>

#include "init.h"
#include "icommands.h"
#include "xmodem_server.h"

void i_ebreak(int argc, char **argv)
{
   asm("ebreak");
}

// ----------------------------------------------------------------------------
// Hexdump a afile
void i_hexdump(int argc, char **argv)
{
   uint32_t bytecount = 0;
   uint8_t buf[256];
   if(argc != 2) {
      printf("usage: hexdump <filename>\n");
      return;
   }

   int fd = open(argv[1], O_RDONLY);
   if(fd < 0) {
      perror("open");
      return;
   }

   ssize_t bytes;
   do {
      bytes = read(fd, buf, sizeof(buf));
      if(bytes < 0) {
         int e = errno;
         perror("read");
         printf("read errno = %d\n", e);
      }
      if(bytes > 0)
         hexdump(buf, bytes, bytecount);
      bytecount += bytes;
   } while(bytes > 0);
   close(fd);
}

// ----------------------------------------------------------------------------
// List files
void i_ls(int argc, char **argv)
{
   struct dirent entry;
   int dhnd = opendir("");
   if(dhnd < 0) {
      perror("opendir");
      return;
   }

   while(_readdir(dhnd, &entry) >= 0) {
      if(strlen(entry.d_name) == 0) break;
      printf("%s\t%ld\t%s\n", entry.d_isdir ? "d" : "f", entry.d_size, entry.d_name);
   }

   closedir(dhnd);
}

// ----------------------------------------------------------------------------
// XMODEM receive
static void xm_tx_byte(struct xmodem_server *xdm, uint8_t byte, void *cb_data)
{
   write(1, &byte, 1);
}

static uint8_t xm_rx_byte()
{
   uint8_t buf[2];
   read(0, buf, 1);
   return buf[0];
}

void i_receive_xmodem(int argc, char **argv)
{
   struct xmodem_server xdm;
   uint32_t faketime = 0;
   if(argc != 2) {
      printf("usage: rx <filename>\n");
      return;
   }

   int fd = open(argv[1], O_CREAT|O_WRONLY);
   if(fd < 0) {
      perror("open");
      return;
   }

   printf("Starting xmodem receive to %s\n", argv[1]);

   ioctl(0, CONSOLE_SET_RAW);

   xmodem_server_init(&xdm, xm_tx_byte, NULL);
   while(!xmodem_server_is_done(&xdm)) {
      uint8_t resp[XMODEM_MAX_PACKET_SIZE];
      uint32_t block_nr;
      int rx_data_len;

      if(fd_peek(0) > 0)
         xmodem_server_rx_byte(&xdm, xm_rx_byte());

      rx_data_len = xmodem_server_process(&xdm, resp, &block_nr, (faketime >> 2));
      if(rx_data_len > 0) {
         write(fd, resp, rx_data_len);
      }
      faketime++;
   }

   close(fd);
   ioctl(0, CONSOLE_SET_INTERACTIVE);
   if(xmodem_server_get_state(&xdm) == XMODEM_STATE_FAILURE) {
      printf("xmodem transfer failed\n");
   }
}
