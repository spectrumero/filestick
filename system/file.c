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

// file.c: mostly a wrapper around FatFS to provide POSIX-style open/read/write
// close operations, and abstract away FatFS, integrating it with the other
// open/read/write/close etc. operations we have on devices.

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#include "spi_flashdev.h"
#include "fd.h"
#include "sysdefs.h"
#include "ff.h"
#include "filesystem.h"
#include "printk.h"

static FDfunction fileio_func = {
   .fd_read          = fileio_read,
   .fd_write         = fileio_write,
   .fd_close         = fileio_close,
   .fd_lseek         = fileio_lseek,
   .fd_fstat         = fileio_fstat
};

FIL   fhnd[MAX_FD_COUNT];

void init_fileio()
{
   memset(&fhnd, 0, sizeof(fhnd));
}

int fileio_open(const char *path, int flags, mode_t mode)
{
   int fdnum;
   FD *fd = fd_alloc(&fdnum);
   if(!fd)
      return -EMFILE;

   fd->fdfunc = &fileio_func;

   FIL *fp = &fhnd[fdnum - MIN_FD_NUMBER];

   // translate posix open flags to FatFS flags
   uint8_t fatfs_flags = 0;
   if((flags + 1) & _FREAD)   fatfs_flags |= FA_READ;
   if((flags + 1) & _FWRITE)  fatfs_flags |= FA_WRITE;
   if(flags & _FAPPEND)       fatfs_flags |= FA_OPEN_APPEND;
   if(flags & _FCREAT)        fatfs_flags |= FA_CREATE_ALWAYS;
   if(flags & _FTRUNC)        fatfs_flags |= FA_CREATE_ALWAYS;

   FRESULT res = f_open(fp, path, fatfs_flags);

   if(res == FR_OK) 
      return fdnum;

   fd_dealloc(fd);
   return fatfs_to_errno(res);
}

ssize_t fileio_read(int fd, void *buf, size_t count)
{
   FIL *fp = &fhnd[fd - MIN_FD_NUMBER];
   UINT bytes_read;

   FRESULT res = f_read(fp, buf, count, &bytes_read);
   if(res == FR_OK)
      return bytes_read;

   return fatfs_to_errno(res);
}

ssize_t fileio_write(int fd, const void *buf, size_t count)
{
   FIL *fp = &fhnd[fd - MIN_FD_NUMBER];
   UINT bytes_written;

   FRESULT res = f_write(fp, buf, count, &bytes_written);
   if(res == FR_OK)
      return bytes_written;

   return fatfs_to_errno(res);
}

int fileio_close(int fd) {
   FIL *fp = &fhnd[fd - MIN_FD_NUMBER];
   FRESULT res = f_close(fp);

   return fatfs_to_errno(res);
}

off_t fileio_lseek(int fd, off_t offset, int whence)
{
   FRESULT res;
   off_t target;
   FIL *fp = &fhnd[fd - MIN_FD_NUMBER];

   switch(whence) {
      case SEEK_CUR:
         target = f_tell(fp) + offset;
         res = f_lseek(fp, target);
         break;
      case SEEK_END:
         target = f_size(fp) + offset;
         res = f_lseek(fp, target);
         break;
      default:       // SEEK_SET
         res = f_lseek(fp, offset);
   }

   if(res == FR_OK) 
      return f_tell(fp);

   return fatfs_to_errno(res);
}

// Warning: this is partial, because FatFS doesn't have fstat
// that works on a file pointer. Really it just returns the size.
int fileio_fstat(int fd, struct stat *statbuf)
{
   FRESULT res;
   FIL *fp = &fhnd[fd - MIN_FD_NUMBER];
   memset(statbuf, 0, sizeof(struct stat));
   statbuf->st_mode = S_IFREG;
   statbuf->st_size = f_size(fp);

   return 0;
}

