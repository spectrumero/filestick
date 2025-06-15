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

/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "flashdisc.h"        // our SPI flash chip
#include "sd.h"               // our SD card interface

/* Definitions of physical drive number for each drive */
#define DEV_SPIFLASH    0
#define DEV_SDCARD      1

//#define DEBUG 1

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
   switch(pdrv) {
      case DEV_SPIFLASH:
         return 0;
      case DEV_SDCARD:
         return sd_status;
   }

   return STA_NOINIT;         
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
   int rc;
   switch(pdrv) {
      case DEV_SPIFLASH:
         return intflash_init();
      case DEV_SDCARD:
         // Low level init
         if((rc=sd_init()) == SD_SUCCESS) {
            // Read partition table to complete initialization
            return sd_readPT();
         }
         return STA_NOINIT;
   }

   return STA_NOINIT;         
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
#ifdef DEBUG
   printk("disk_read: pdrv=%d sector=%ld count=%d\n", pdrv, sector, count);
#endif
   switch(pdrv) {
      case DEV_SPIFLASH:
         return intflash_read(buff, sector, count);
      case DEV_SDCARD:
         return sd_read(buff, sector, count);
   }

   return RES_PARERR;         
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
   switch(pdrv) {
      case DEV_SPIFLASH:
         return intflash_write(buff, sector, count);
      case DEV_SDCARD:
         return sd_write(buff, sector, count);
   }
   return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
#ifdef DEBUG
   printk("disk_ioctl: pdrv=%d cmd=%d\n", pdrv, cmd);
#endif
   switch(pdrv) {
      case DEV_SPIFLASH:
         return intflash_ioctl(cmd, buff);
      case DEV_SDCARD:
         return sd_ioctl(cmd, buff);
   }

   return RES_PARERR;
}

