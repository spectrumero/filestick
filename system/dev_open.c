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

// Open devices
#include <string.h>

#include "dev_open.h"
#include "fd.h"
#include "console.h"
#include "rgbled.h"
#include "spi_flashdev.h"
#include "raw_econet.h"
#include "errno.h"

extern FD fdtable[MAX_FILE_DESCRIPTORS];

DevOpen open_table[] = {
   { .devname = "console",    .open_device_impl = open_console },
   { .devname = "rgbled",     .open_device_impl = rgbled_open },
   { .devname = "spiflash",   .open_device_impl = spiflash_open },
   { .devname = "econet",     .open_device_impl = econet_open },
   { .devname = NULL,         .open_device_impl = NULL }
};

//------------------------------------------------------------
// Open a device
//
int open_device(const char *devname, int flags, mode_t mode) {
   int fdnum;
   FD *fd = fd_alloc(&fdnum);
   if(!fd)
      return -EMFILE;

   DevOpen *table_ent = open_table;
   while(table_ent->devname != NULL) {
      if(!strcmp(table_ent->devname, devname)) {
         int rc = table_ent->open_device_impl(devname, flags, mode, fd);
         if(!rc) {
            fd->flags |= flags;
            return fdnum;
         }
         else {
            fd_dealloc(fd);
            return rc;
         }
      } 
      table_ent++;
   }

   // Device not found
   fd_dealloc(fd);
   return -ENOENT;
}

