#ifndef SPI_FLASHDEV_H
#define SPI_FLASHDEV_H
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
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#include "fd.h"

void spiflash_init(void);
int spiflash_open(const char *devname, int flags, mode_t mode, FD *fd);
ssize_t spiflash_read(int fd, void *buf, size_t count);
ssize_t spiflash_write(int fd, const void *buf, size_t count);
int spiflash_fstat(int fd, struct stat *statbuf);
off_t spiflash_lseek(int fd, off_t offset, int whence);
int spiflash_close(int fd);

void spiflash_sync(void);

// From spi_flash.s
void flash_memcpy(uint32_t srcaddr, void *destptr, size_t count);
uint8_t flash_byte(uint8_t byte);

#endif
