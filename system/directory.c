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

// Directory access syscalls. These are somewhat minimal wrappers around
// FatFS calls.

#include <stdbool.h>
#include <stdint.h>
#include <sys/dirent.h>
#include <string.h>

#include "ff.h"
#include "filesystem.h"

int SYS_opendir(DIR *dir, const char *path)
{
   FRESULT res = f_opendir(dir, path);
   return fatfs_to_errno(res);
}

int SYS_closedir(DIR *dir)
{
   FRESULT res = f_closedir(dir);
   return fatfs_to_errno(res);
}

int SYS_readdir(DIR *dir, struct dirent *d)
{
   FILINFO fno;

   FRESULT res = f_readdir(dir, &fno);
   if(res = FR_OK) {
      strcpy(d->d_name, fno.fname);
      d->d_isdir = fno.fattrib & AM_DIR;
      d->d_size = fno.fsize;
   }

   return fatfs_to_errno(res);
}

