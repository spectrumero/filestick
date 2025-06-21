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

#include "printk.h"
#include "spi_flashdev.h"
#include "fd.h"
#include "sysdefs.h"
#include "kmalloc.h"
#include "devices.h"

//#define DEBUG_FLASHWRITE

static FDfunction spiflash_func = {
   .fd_read    = spiflash_read,
   .fd_write   = spiflash_write,
   .fd_lseek   = spiflash_lseek,
   .fd_fstat   = spiflash_fstat,
   .fd_close   = spiflash_close
};

static uint32_t   fileptr = 0;
static bool       is_open = false;

// Flash write low level things
#define WRITE_SECTOR_SIZE           4096
#define WRITE_OFFSET_MASK           0xFFFFF000
#define WRITE_FILEPTR_OFFSET_MASK   0x00000FFF
#define WRITE_ERASE_SEC_SHIFT       12

static uint8_t    *writebuf = NULL;
static uint32_t   write_blk_offset = 0;
static uint32_t   write_blk_end = 0;

volatile uint8_t  *spi_reg_ss       = (uint8_t *)(DEV_BASE + OFFS_SPI_REG_SS);
volatile uint8_t  *spi_reg_active   = (uint8_t *)(DEV_BASE + OFFS_SPI_REG_ACTIVE);
#define SPI_SS                0
#define FLASH_CMD_WREN        0x06     // write enable
#define FLASH_CMD_SE          0x20     // 4k sector erase
#define FLASH_CMD_PP          0x02     // page program
#define FLASH_CMD_RDSR        0x05     // read status register

static ssize_t spiflash_write_to_sector(const uint8_t *buf, size_t count);
static ssize_t spiflash_load_sector(void);
static void spiflash_writebuffer(void);

//------------------------------------------------------------------------
// Open the SPI flash
// FIXME: limitation - open is exclusive
int spiflash_open(const char *devname, int flags, mode_t mode, FD *fd) {
   if(is_open) 
      return -EBUSY;

   fd->fdfunc = &spiflash_func;
   is_open = true;
   fileptr = 0;
   return 0;
}

//------------------------------------------------------------------------
// Read
// FIXME: bounds check the flash chip depending on type
ssize_t spiflash_read(int fd, void *buf, size_t count) {
   if(writebuf != NULL) spiflash_writebuffer();

   flash_memcpy(fileptr, buf, count);
   fileptr += count;
   return count;
}

//------------------------------------------------------------------------
// Write
// The erase sector size is 4k so writes get buffered and written out
// when this buffer is filled.
ssize_t spiflash_write(int fd, const void *buf, size_t count) {
   size_t remain = count;

#ifdef DEBUG_FLASHWRITE
   printk("writing %d bytes\n", count);
#endif 
   do {
      ssize_t bytes = spiflash_write_to_sector(buf, remain);
      if(bytes < 0) return bytes;

      fileptr += bytes;
      buf += bytes;
      remain -= bytes;
#ifdef DEBUG_FLASHWRITE
      printk("%d bytes remaining\n", remain);
#endif
   } while(remain);

   return count;
}

static ssize_t spiflash_write_to_sector(const uint8_t *buf, size_t count) {
   uint32_t end_addr = count + fileptr;

   if(writebuf == NULL) {
#ifdef DEBUG_FLASHWRITE
      printk("load new sector\n");
#endif
      int rc = spiflash_load_sector();
      if(rc < 0) return rc;
   }
   else if(end_addr >= write_blk_end) {
#ifdef DEBUG_FLASHWRITE
      printk("flush prev sector and load new sector, fileptr = %x end_addr = %x, count = %x\n",
            fileptr, end_addr, count);
#endif
      spiflash_writebuffer();
      int rc = spiflash_load_sector();
      if(rc < 0) return rc;
   }

   uint32_t fileptr_in_blk = fileptr & WRITE_FILEPTR_OFFSET_MASK;
   if(end_addr >= write_blk_end) {
      count = write_blk_end - fileptr;
#ifdef DEBUG_FLASHWRITE
      printk("reducing count to %d: write_blk_end = %x fileptr = %x\n",
            count, write_blk_end, fileptr);
#endif
   }

   uint8_t *bufptr = writebuf + fileptr_in_blk;
   memcpy(bufptr, buf, count);

   // if the block end was hit, write it out
   if(end_addr >= write_blk_end) {
#ifdef DEBUG_FLASHWRITE
      printk("write out sector\n");
#endif
      spiflash_writebuffer();
   }

   return count;
}

static ssize_t spiflash_load_sector(void)
{
   writebuf = kmalloc(WRITE_SECTOR_SIZE);
   if(writebuf == NULL) return -ENOMEM;

   // calculate the start byte of the erase sector
   write_blk_offset = fileptr & WRITE_OFFSET_MASK;
   write_blk_end = write_blk_offset + WRITE_SECTOR_SIZE;

   // get what's currently in the erase sector block
   flash_memcpy(write_blk_offset, writebuf, WRITE_SECTOR_SIZE);
   return 0;
}

//-------------------------------------------------------------------------
// write a 4k erase block's worth of data
static void spiflash_writebuffer(void)
{
   uint8_t *pageptr = writebuf;
   uint8_t status;

   // set SPI slave select to flash
   *spi_reg_ss = SPI_SS;

   // Write enable
   flash_byte(FLASH_CMD_WREN);
   *spi_reg_active = 0;    // slave select high

   // Sector erase
   flash_byte(FLASH_CMD_SE);
   flash_byte((uint8_t)(write_blk_offset >> 16));
   flash_byte((uint8_t)(write_blk_offset >> 8));
   flash_byte((uint8_t)(write_blk_offset));
   *spi_reg_active = 0;    // SS high completes erase

   // wait for erase sector
   flash_byte(FLASH_CMD_RDSR);
   do {
      status = flash_byte(0xFF);
   } while(status & 1);
   *spi_reg_active = 0;

   // Write 16 x 4k blocks
   for(int i = 0; i < 16; i++) {
      flash_byte(FLASH_CMD_WREN);
      *spi_reg_active = 0;
      flash_byte(FLASH_CMD_PP);
      flash_byte((uint8_t)(write_blk_offset >> 16));
      flash_byte((uint8_t)(write_blk_offset >> 8));
      flash_byte((uint8_t)(write_blk_offset));

      for(int j = 0; j < 256; j++) {
         flash_byte(*pageptr++);
      }
      *spi_reg_active = 0;

      // wait for write to complete
      flash_byte(FLASH_CMD_RDSR);
      do {
         status = flash_byte(0xFF);
      } while(status & 1);
      *spi_reg_active = 0;
      write_blk_offset += 256;
   }

   // turn off slave select
   *spi_reg_active = 0;

   // clear buffers
   write_blk_offset = 0;
   kfree(writebuf);
   writebuf = NULL;
}

//------------------------------------------------------------------------
// Seek
// FIXME: bounds check the flash chip depending on type
off_t spiflash_lseek(int fd, off_t offset, int whence) {
   switch(whence) {
      case SEEK_SET:
         fileptr = offset;
         break;
      case SEEK_CUR:
         fileptr += offset;
         break;
      case SEEK_END:
         fileptr = 0xFFFFFF + offset;
         break;
      default:
         return -EINVAL;
   }
   return fileptr;
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
   if(writebuf) spiflash_writebuffer();
   is_open = false;
   return 0;
}

