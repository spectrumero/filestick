/*
;The MIT License
;
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

// Simple supervisor shell for debugging after a crash.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sys/console.h"
#include "super_shell.h"
#include "elfload.h"
#include "console.h"
#include "printk.h"

#include "hexdump.h"

#define MAXARGS      4

typedef struct cmdtable {
   char        *cmd;
   void        (*cmdfunc)(int argc, char **argv);
} CmdTable;

CmdTable commands[] = {
   {.cmd = "hexdump",   .cmdfunc = super_hexdump},
   {.cmd = "peek",      .cmdfunc = super_peek},
   {.cmd = "poke",      .cmdfunc = super_poke},
   {.cmd = "boot",      .cmdfunc = super_elf},
   {.cmd = "run",       .cmdfunc = super_elf},
   {.cmd = "ret",       .cmdfunc = NULL },
   {.cmd = NULL }
};

void super_shell()
{
   size_t bytes;
   char cmdbuf[48];
   char *args[MAXARGS];
   int argc;

   printk("Supervisor\n");

   // Make sure we're in interactive mode
   SYS_ioctl(0, CONSOLE_SET_INTERACTIVE, NULL);

   while(true) {
      SYS_write(1, "S> ", 3);

      memset(cmdbuf, 0, sizeof(cmdbuf));
      bytes = SYS_read(0, cmdbuf, sizeof(cmdbuf) - 1);

      if(bytes) {
         argc = 0;
         char *ptr = strtok(cmdbuf, " ");
         while(ptr && argc < MAXARGS) {
            args[argc] = ptr;
            argc++;

            ptr = strtok(NULL, " ");
         }

         if(argc > 0) {
            CmdTable *cptr = commands;
            bool handled = false;
            while(cptr->cmd != NULL) {
               if(!strcmp(cptr->cmd, args[0])) {
                  if(cptr->cmdfunc == NULL)
                     return;

                  cptr->cmdfunc(argc, args);
                  handled = true;
                  break;
               }

               cptr++;
            }

            if(!handled) 
               printk("Bad command\n");
         }
      }
   }
}

