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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/console.h>
#include <syscall.h>

#include "init.h"
#include "icommands.h"

#define MAXARGS 7

typedef struct cmdtable {
   char        *cmd;
   void        (*cmdfunc)(int argc, char **argv);
} CmdTable;

CmdTable commands[] = {
   {  .cmd = "ebreak",     .cmdfunc = i_ebreak },
   {  .cmd = "rx",         .cmdfunc = i_receive_xmodem },
   {  .cmd = "hexdump",    .cmdfunc = i_hexdump },
   {  .cmd = "ls",         .cmdfunc = i_ls },
   {  .cmd = "configure",  .cmdfunc = i_configure },
   {  .cmd = "mkdir",      .cmdfunc = i_mkdir },
   {  .cmd = "cd",         .cmdfunc = i_chdir },
   {  .cmd = "rm",         .cmdfunc = i_rm },
   {  .cmd = "poke",       .cmdfunc = i_poke },
   {  .cmd = "peek",       .cmdfunc = i_peek },
   {  .cmd = NULL }
};

static bool comment(const char *cmd);
static ssize_t clean_cmdbuf(char *dest, const char *src, int size);
static CmdTable *internal_command(const char *cmdbuf);
static void run_internal_command(const CmdTable *entry, char *cmdbuf);

// Start the simple command line interpreter
int cli(int fd)
{
   char raw_cmdbuf[256];

   // Make sure stdin is in interactive mode
   if(fd == 0) ioctl(fd, CONSOLE_SET_INTERACTIVE);

   while(true) {
      write(1, "*> ", 3);
     
      memset(raw_cmdbuf, 0, sizeof(raw_cmdbuf));
      ssize_t bytes = read(fd, raw_cmdbuf, sizeof(raw_cmdbuf) - 1);
      if(bytes < 0) {
         printf("Unable to read from fd %d\n", fd);
         return -1;
      }

      parse_cmd(raw_cmdbuf);
   }
   return 0;
}

// --------------------------------------------------------------
// Clean up and parse the command
int parse_cmd(const char *raw_cmdbuf)
{
   char cmdbuf[256];

   memset(cmdbuf, 0, sizeof(cmdbuf)); 
   ssize_t bytes = clean_cmdbuf(cmdbuf, raw_cmdbuf, sizeof(raw_cmdbuf));

   if(bytes && !comment(cmdbuf)) {
      CmdTable *entry = internal_command(cmdbuf);
      if(entry) {
         run_internal_command(entry, cmdbuf);
      }
      else {
         int rc = exec_elf(cmdbuf);
         if(rc < 0) {
            perror("exec_elf");
         }
      }
   }
   return 0;
}

// ----------------------------------------------------------
// Determine if the line submitted is a comment
static bool comment(const char *cmd)
{
   if(cmd[0] == '#') return true;

   return false;
}

// ----------------------------------------------------------
// Clean up the command line
static ssize_t clean_cmdbuf(char *dest, const char *src, int size)
{
   // Find index of first nonwhitespace character
   int i;
   for(i = 0; i < size; i++) {
      if(src[i] != ' ') break;
   }

   // nothing but whitespace
   if(i == size) return 0;

   strcpy(dest, src + i);
   int len = strlen(dest);

   // remove trailing newlines/ctrl chars and spaces
   for(i = len - 1; i >= 0; i--) {
      if(dest[i] > 32) break;

      dest[i] = 0;
      len--;
   }

   return len;
}

// -----------------------------------------------------------
// Check for internal command
static CmdTable *internal_command(const char *cmdbuf)
{
   CmdTable *tptr = commands;
   while(tptr->cmd) {
      int clen = strlen(tptr->cmd);
      if(cmdbuf[clen] == ' ' || cmdbuf[clen] == 0) {
         if(!strncmp(tptr->cmd, cmdbuf, clen)) return tptr;
      }

      tptr++;
   }

   return NULL;
}

// -----------------------------------------------------------
// Run internal command
static void run_internal_command(const CmdTable *entry, char *cmdbuf)
{
   int argc = 0;
   char *args[MAXARGS];

   char *ptr = strtok(cmdbuf, " ");
   while(ptr && argc < MAXARGS) {
      args[argc] = ptr;
      argc++;

      ptr = strtok(NULL, " ");
   }

   if(entry->cmdfunc == NULL) return;

   entry->cmdfunc(argc, args);
}

