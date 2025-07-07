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
#include <string.h>

#include <sys/econet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "icommands.h"

typedef struct cfgtable {
   char        *item;
   void        (*configfunc)(int argc, char **argv);
} ConfigTable;

static void cfg_station(int argc, char **argv);
static void cfg_netclock(int argc, char **argv);
static void cfg_netterminate(int argc, char **argv);

ConfigTable cfg[] = {
   {  .item = "netstation",    .configfunc = cfg_station },
   {  .item = "netclock",      .configfunc = cfg_netclock },
   {  .item = "netterminate",  .configfunc = cfg_netterminate },
   {  .item = NULL }
};

// ------------------------------------------------------------
// Finds and calls the configuration function.
void i_configure(int argc, char **argv)
{
   if(argc < 2) {
      printf("usage: configure <item> <params> ...\n");
      return;
   }

   int cfg_argc = argc - 1;
   char **cfg_argv = argv + 1;

   ConfigTable *cfgptr = cfg;
   while(cfgptr->item != NULL) {
      if(!strcmp(cfgptr->item, cfg_argv[0])) {
         cfgptr->configfunc(cfg_argc, cfg_argv);
         return;
      }
      cfgptr++;
   }

   printf("No such configuration item\n");
}

// -----------------------------------------
// Set the econet station
static void cfg_station(int argc, char **argv)
{
   uint8_t station;
   int fd = open("/dev/econet", O_RDWR);
   if(fd < 0) {
      perror("econet: open");
      return;
   }

   if(argc == 1) {
      int rc = ioctl(fd, ECONET_GET_ADDR, &station);
      if(rc < 0) {
         perror("econet: ioctl");
         goto cleanup;
      }

      if(station == 0)
         printf("Econet station not configured\n");
      else
         printf("Econet station %d\n", station);
   }
   else {
      station = atoi(argv[1]);
      if(station == 0) {
         printf("Invalid station number\n");
      }
      else {
         int rc = ioctl(fd, ECONET_SET_ADDR | station);
         if(rc < 0) perror("econet: set_addr: ioctl");
      }
   }

cleanup:   
   close(fd);
}

// -----------------------------------------
// Fetch clkterm value
static uint16_t get_clkterm(int fd)
{
   uint16_t clkterm;

   int rc = ioctl(fd, ECONET_GET_CLKTERM, &clkterm);
   if(rc < 0) {
      perror("econet: ioctl");
      return 0xFFFF;
   }

   return clkterm;
}

//------------------------------------------
// Enable/disable and set the econet network clock
static void cfg_netclock(int argc, char **argv)
{
   int fd = open("/dev/econet", O_RDWR);
   if(fd < 0) {
      perror("econet: open");
      return;
   }

   uint16_t clkterm = get_clkterm(fd);
   if(clkterm == 0xFFFF) goto cleanup;

   if(argc == 1) {
      printf("Econet clock divider = %d\n", clkterm >> 8);
      printf("Econet clock %s\n",
            clkterm & CLOCK_ENABLE ? "enabled" : "disabled");
   }
   else {
      if(argc > 2 && !strcmp(argv[2], "enable"))
         clkterm |= CLOCK_ENABLE;
      else
         clkterm &= (0xFFFF ^ CLOCK_ENABLE);

      int div = atoi(argv[1]);
      if(div > 0 && div < 8) {
         clkterm &= 0xFF;
         clkterm |= div << 8;
      }
      else {
         printf("Invalid clock divider value\n");
         goto cleanup;
      }

      int rc = ioctl(fd, ECONET_SET_CLKTERM | clkterm);
      if(rc < 0) {
         perror("econet: setting clock: ioctl");
      }
   }
cleanup:
   close(fd);
}

//------------------------------------------
// Enable/disable the network terminator
static void cfg_netterminate(int argc, char **argv)
{
   int fd = open("/dev/econet", O_RDWR);
   if(fd < 0) {
      perror("econet: open");
      return;
   }

   uint16_t clkterm = get_clkterm(fd);
   if(clkterm == 0xFFFF) goto cleanup;

   if(argc == 1) {
      printf("Econet termination %s\n",
            clkterm & TERM_ENABLE ? "on" : "off");
   }
   else {
      if(!strcmp(argv[1], "on")) {
         clkterm |= TERM_ENABLE;
      }
      else {
         clkterm &= (0xFFFF ^ TERM_ENABLE);
      }

      int rc = ioctl(fd, ECONET_SET_CLKTERM | clkterm);
      if(rc < 0) {
         perror("econet: setting termination: ioctl");
      }
   }

cleanup:
   close(fd);
}

