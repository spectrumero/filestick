#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "ff.h"
#include "diskio.h"
#include "flashdisc.h"
#include "printk.h"
#include "spi_flashdev.h"  // for spiflash_sync

#include "fd.h"
//#define FLASH_DEBUG 1

static int flash_fd = -1;
static LBA_t sectorCount = SECTOR_COUNT;
static WORD secsz = SECTOR_SIZE;
static DWORD blocksz = BLOCK_SIZE;

DSTATUS intflash_init() {
   if(flash_fd < 0)
      flash_fd = SYS_open("/dev/spiflash", O_RDWR, 0);

#ifdef FLASH_DEBUG
   printk("intflash_init: fd open: %d\n", flash_fd);
#endif
   return (flash_fd > 0) ? 0 : STA_NOINIT;
}

//-------------------------------------------------------------
// Read a flash sector. Calculate the byte offset, lseek to
// the offset, and perform a read.
DRESULT intflash_read(BYTE *buf, LBA_t sector, UINT count)
{
#ifdef FLASH_DEBUG
   printk("Read sector %d count %d\n", sector, count);
#endif
   uint32_t flashaddress;
   uint32_t bytecount;
   if(flash_fd < 0) return RES_NOTRDY;

   flashaddress = FS_START + (sector * SECTOR_SIZE);
   bytecount = count * SECTOR_SIZE;

   off_t rc = SYS_lseek(flash_fd, flashaddress, SEEK_SET);
#ifdef FLASH_DEBUG
   printk("seek: offset=%x result=%x\n", flashaddress, rc);
#endif
   if(rc < 0) return RES_ERROR;

   int bytes = SYS_read(flash_fd, buf, bytecount);
#ifdef FLASH_DEBUG
   printk("read: bytes = %d\n", bytes);
#endif
   return rc > -1 ? RES_OK : RES_ERROR;
}

//---------------------------------------------------------------
// ioctl for the internal flash.
DRESULT intflash_ioctl(BYTE cmd, void *buf)
{
#ifdef FLASH_DEBUG
   printk("intflash_ioctl: %d\n", cmd);
#endif
   switch(cmd) {
      case CTRL_SYNC:
         return intflash_sync();
      case GET_SECTOR_COUNT:
         buf = &sectorCount;
         return RES_OK;
      case GET_SECTOR_SIZE:
         buf = &secsz;
         return RES_OK;
      case GET_BLOCK_SIZE:
         buf = &blocksz;
         return RES_OK;
      case CTRL_TRIM:
         return RES_OK;
   }

   return RES_PARERR;
}

DRESULT intflash_write(const BYTE *buf, LBA_t sector, UINT count)
{
#ifdef FLASH_DEBUG
   printk("Write sector %d count %d\n", sector, count);
#endif
   uint32_t flashaddress;
   uint32_t bytecount;
   if(flash_fd < 0) return RES_NOTRDY;

   flashaddress = FS_START + (sector * SECTOR_SIZE);
   bytecount = count * SECTOR_SIZE;

   off_t rc = SYS_lseek(flash_fd, flashaddress, SEEK_SET);
#ifdef FLASH_DEBUG
   printk("seek: offset=%x result=%x\n", flashaddress, rc);
#endif
   if(rc < 0) return RES_ERROR;

   int bytes = SYS_write(flash_fd, buf, bytecount);
#ifdef FLASH_DEBUG
   printk("write: bytes = %d\n", bytes);
#endif

   return rc > -1 ? RES_OK : RES_ERROR;
}

DRESULT intflash_sync()
{
   spiflash_sync();
   return RES_OK;
}

