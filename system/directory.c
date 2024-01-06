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
// FatFS calls. The intention is to hide the filesystem implementation from
// userland (which shouldn't ever need to know about FatFS).

#include <stdbool.h>
#include <stdint.h>
#include <sys/dirent.h>
#include <string.h>
#include <errno.h>

#include "ff.h"
#include "filesystem.h"
#include "strlcpy.h"

#define MAX_DHND     8

static DIRHND dhnd[MAX_DHND];

//--------------------------------------------------
// Do any required initialization.
void init_dirs() 
{
   for(int i = 0; i < MAX_DHND; i++)
      dhnd[i].open = false;
}

//-------------------------------------------------
// Allocate a directory handle and open a dir.
int SYS_opendir(const char *path)
{
   DIRHND *dirp = NULL;
   int i;

   for(i = 0; i < MAX_DHND; i++) {
      if(dhnd[i].open == false) {
         dirp = &dhnd[i];
         break;
      }
   }

   if(!dirp) return -ENFILE;

   FRESULT res = f_opendir(&dirp->dir, path);

   if(res == FR_OK) {
      dirp->open = true;
      return i;            // directory handle number
   }

   return fatfs_to_errno(res);
}

//------------------------------------------------
// Close dir and deallocate handle
int SYS_closedir(int dh)
{
   if(dh > MAX_DHND) return -EMFILE;
   if(dhnd[dh].open == false) return -EBADF;

   FRESULT res = f_closedir(&dhnd[dh].dir);
   if(res == FR_OK) dhnd[dh].open = false;

   return fatfs_to_errno(res);
}

static FILINFO fno;
//------------------------------------------------
// Read a dir.
int SYS_readdir(int dh, struct dirent *d)
{
   if(dh > MAX_DHND) return -EMFILE;
   if(dhnd[dh].open == false) return -EBADF;


   FRESULT res = f_readdir(&dhnd[dh].dir, &fno);
   if(res == FR_OK) {
      strlcpy(d->d_name, fno.fname, sizeof(d->d_name));
      d->d_isdir = fno.fattrib & AM_DIR;
      d->d_size = fno.fsize;
   }

   return fatfs_to_errno(res);
}

