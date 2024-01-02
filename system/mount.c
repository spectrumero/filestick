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

#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "filesystem.h"
#include "ff.h"         // FatFS
#include "cust_errno.h"

static FATFS fs;        // FatFS filesystem TODO: array of these

// Implements the mount syscall
int SYS_mount(const char *src, const char *target, const char *fstype,
              unsigned long mountflags, const void *data)
{
   // It's quite likely we'll only ever support fatfs but never say never.
   if(strcmp(fstype, "fatfs"))
      return -EINVAL;

   FRESULT res = f_mount(&fs, "", 1);

   // convert results to standard errno types
   return fatfs_to_errno(res);
}

int fatfs_to_errno(FRESULT res) {
   switch(res) {
      case FR_OK:
         return 0;
      case FR_NO_FILE:
         return -ENOENT;
      case FR_NO_PATH:
         return -ENOENT;
      case FR_NOT_ENOUGH_CORE:
         return -ENOMEM;
      case FR_TOO_MANY_OPEN_FILES:
         return -ENFILE;
      default:
         return -(res + EFATFS_START);
   }
}
