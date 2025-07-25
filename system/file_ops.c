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
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#include "fd.h"
#include "sysdefs.h"
#include "ff.h"
#include "filesystem.h"
#include "printk.h"

int SYS_unlink(const char *pathname)
{
   FRESULT res = f_unlink(pathname);
   return fatfs_to_errno(res);
}

int SYS_mkdir(const char *pathname, mode_t mode)
{
   FRESULT res = f_mkdir(pathname);
   return fatfs_to_errno(res);
}

int SYS_chdir(const char *pathname)
{
   FRESULT res = f_chdir(pathname);
   return fatfs_to_errno(res);
}

int SYS_stat(const char *pathname, struct stat *statbuf)
{
   FILINFO fno;
   memset(statbuf, 0, sizeof(struct stat));

   FRESULT res = f_stat(pathname, &fno);
   if(res == FR_OK) {
      statbuf->st_size = fno.fsize;
      if(fno.fattrib & AM_DIR) statbuf->st_mode = S_IFDIR;
      else                     statbuf->st_mode = S_IFREG;

      if(fno.fattrib & AM_RDO) statbuf->st_mode |= S_IRUSR;
      else                     statbuf->st_mode |= (S_IRUSR|S_IWUSR);

      // TODO: timestamps
   }
   return fatfs_to_errno(res);
}
