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

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "spi_flashdev.h"
#include "fd.h"
#include "sysdefs.h"

static FDfunction spiflash_func = {
   .fd_read    = spiflash_read,
   .fd_write   = NULL,
   .fd_lseek   = spiflash_lseek,
   .fd_fstat   = spiflash_fstat,
   .fd_close   = spiflash_close
};

static uint32_t   fileptr = 0;
static bool       is_open = false;

//------------------------------------------------------------------------
// Open the SPI flash
// TODO: simulator
int spiflash_open(const char *devname, int flags, mode_t mode, FD *fd) {
   return -ENOENT;
}

//------------------------------------------------------------------------
// Read
// TODO: simulator
ssize_t spiflash_read(int fd, void *buf, size_t count) {
   return 0;
}

//------------------------------------------------------------------------
// Seek
// TODO: simulator
off_t spiflash_lseek(int fd, off_t offset, int whence) {
   return 0;
}

//------------------------------------------------------------------------
// Stat
// FIXME: return size based on flash chip
int spiflash_fstat(int fd, struct stat *statbuf) {
   memset(statbuf, 0, sizeof(struct stat));
   statbuf->st_mode = S_IFBLK | 0555;
   statbuf->st_size = 0xFFFFFF;
   statbuf->st_nlink = 1;

   return 0;
}

//------------------------------------------------------------------------
// Close
int spiflash_close(int fd) {
   is_open = false;
   return 0;
}

