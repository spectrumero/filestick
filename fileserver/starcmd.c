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
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/econet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "message.h"
#include "starcmd.h"

#define MAXARGS   8

typedef struct cmdtable {
   char *cmd;
   void (*cmdfunc)(NetFSMsg *msg, int argc, char **argv);
} CmdTable;

static void cmd_i_am(NetFSMsg *msg, int argc, char **argv);
static void cmd_echo(NetFSMsg *msg, int argc, char **argv);

CmdTable commands[] = {
   { .cmd = "i",     .cmdfunc = cmd_i_am },
   { .cmd = "echo",  .cmdfunc = cmd_echo },
   { .cmd = NULL }
};

//-------------------------------------------------------------
//
void
handle_starcmd(NetFSMsg *msg)
{
   char *args[MAXARGS];
   int argc = 0;

   if(msg->paysize > 0) {
      char *ptr = strtok(msg->payload, " ");
      while(ptr && argc < MAXARGS) {
         args[argc] = ptr;
         argc++;

         ptr = strtok(NULL, " ");
      }
   }

   if(argc > 0) {
      CmdTable *cptr = commands;
      bool handled = false;
      while(cptr->cmd != NULL) {
         if(!strcasecmp(cptr->cmd, args[0])) {
            if(cptr->cmdfunc == NULL) break;

            cptr->cmdfunc(msg, argc, args);
            handled = true;
            break;
         }

         cptr++;
      }

      if(!handled) {
         printf("cmd not handled\n");
      }
   }
}

//-------------------------------------------------------------
// Star commands
static void
cmd_i_am(NetFSMsg *msg, int argc, char **argv)
{
   // test 
   uint8_t iambuf[6];
   iambuf[0] = 0x05;    // I AM
   iambuf[1] = 0x00;    // Result code
   iambuf[2] = 3;       // URD
   iambuf[3] = 5;       // CSD
   iambuf[4] = 6;       // LIB
   iambuf[5] = 0;       // boot opts
   econet_send(msg, iambuf, sizeof(iambuf));
}

static void
cmd_echo(NetFSMsg *msg, int argc, char **argv)
{
   uint8_t buf[64];
   buf[0] = 0x00;       // no action, cmd complete
   buf[1] = 0x01;       // result
   strcpy(buf+2, "Test\n");
   econet_send(msg, buf, 8);
}

