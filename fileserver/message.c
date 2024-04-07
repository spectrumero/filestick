/*
;Copyright (c) 2024 Dylan Smith
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
#include <sys/ioctl.h>
#include <sys/econet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "message.h"
#include "starcmd.h"

static int econet_fd;
static uint8_t econet_buf[2048];

static void econet_handle_message(NetFSMsg *msg);

//------------------------------------------------------------
// Initialize econet.
int
econet_init(uint8_t station)
{
   econet_fd = open("/dev/econet", O_RDWR);
   if(econet_fd < 0) return econet_fd;

   // set station number
   int rc = ioctl(econet_fd, ECONET_SET_ADDR | station);
   if(rc < 0) return rc;

   rc = ioctl(econet_fd, ECONET_SET_RECV_PORT | NETFS_PORT);
   if(rc < 0) return rc;

   printf("Econet initialized, station: %d\n", station);
   return 0;
}

//------------------------------------------------------------
// Wait for messages and dispatch them.
void
econet_msgloop()
{
   ssize_t rxbytes;
   NetFSMsg msg; 

   while(1) {
      rxbytes = read(econet_fd, econet_buf, sizeof(econet_buf));
      printf("read msg: %d bytes\n", rxbytes);

      if(rxbytes < 7) continue;

      msg.reply_net = *(econet_buf + 1);
      msg.reply_station = *econet_buf;
      msg.reply_port = *(econet_buf + 2);
      msg.function_code = *(econet_buf + 3);
      msg.urd = *(econet_buf + 4);
      msg.csd = *(econet_buf + 5);
      msg.lib = *(econet_buf + 6);
      msg.paysize = rxbytes - 7;
      msg.payload = econet_buf + 7;
     
      // guarantee null terminator for strings 
      *(econet_buf + rxbytes) = 0;

      econet_handle_message(&msg);
   }
}

//---------------------------------------------------------------
// Send a message to the source.
void
econet_send(NetFSMsg *origin, uint8_t *msg, size_t msgsize)
{
   struct econet_addr dest;
   dest.port = origin->reply_port;
   dest.net = origin->reply_net;
   dest.station = origin->reply_station;

   int rc = ioctl(econet_fd, ECONET_SET_SEND_ADDR, &dest);
   if(rc < 0) {
      printf("econet_send: ioctl failed\n");
      return;
   }

   ssize_t bytes = write(econet_fd, msg, msgsize);
   if(bytes < 0) {
      printf("econet_send: write failed\n");
   }
}

//---------------------------------------------------------------
// Handle a message
static void
econet_handle_message(NetFSMsg *msg)
{
   switch(msg->function_code) {
      case FC_COMMANDLINE:
         handle_starcmd(msg);
         break;
      default:
         printf("station %d.%d sent unknown function code %d\n",
               msg->reply_net, msg->reply_station, msg->function_code);
   }
}
