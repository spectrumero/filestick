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

// SD card logical disk operations

#include <stdint.h>

#include "sd.h"
#include "partition.h"
#include "ff.h"      // FatFS defines and types
#include "diskio.h"  // FatFS diskio defines
#include "printk.h"

#include "console.h"

// SD card logical disc IO

// Offset and size in sectors of current partition
LBA_t lba_offset;
LBA_t lba_sectors;
DSTATUS sd_status=STA_NOINIT;

static void fill_pte(const uint8_t *ptestart, PTEntry *pte);

// Read the partition table
DRESULT sd_readPT() {
   uint8_t bootsect[512];
   uint8_t token, rx;
   PTEntry pte;

   rx = sd_readsingleblk(0x00, bootsect, &token);
   if(token != 0xFE || rx > 1)
      return RES_ERROR;

   // offset to first partition table entry
   fill_pte(bootsect + 0x1BE, &pte);
   lba_offset = pte.startlba;
   lba_sectors = pte.sectors;

   sd_status=0;
   return RES_OK;
}

// FatFS support
// Read a logical sector
DRESULT sd_read(BYTE *buf, LBA_t sector, UINT count) {
   uint8_t rx, token;
   UINT i;
   LBA_t phys=sector + lba_offset;

   for(i = 0; i < count; i++) {
      rx = sd_readsingleblk(phys, buf, &token);
      if(token != 0xFE || rx > 1)
         return RES_ERROR;

      buf+=512;
      phys++;
   }

   return RES_OK;
}

// FatFS support
// Write a logical sector
DRESULT sd_write(const BYTE *buf, LBA_t sector, UINT count) {
   uint8_t rx, token;
   UINT i;
   LBA_t phys=sector + lba_offset;

   for(i = 0; i < count; i++) {
      rx = sd_writesingleblk(phys, buf, &token);
      if(token != 0x05 || rx != 0)
         return RES_ERROR;
      buf += 512;
      phys++;
   }
}

// FatFS support
// Do an ioctl
DRESULT sd_ioctl(BYTE cmd, void *buf) {
   switch(cmd) {
      case CTRL_SYNC:
         return RES_OK;
      case GET_SECTOR_COUNT:
         *(LBA_t *)buf = lba_sectors;
         return RES_OK;
      case GET_SECTOR_SIZE:
         *(WORD *)buf=512;
         return RES_OK;
      case GET_BLOCK_SIZE:
         *(DWORD *)buf=512;
         return RES_OK;
      case CTRL_TRIM:
         return RES_OK;
   }
   return RES_PARERR;
}

// the partition table entry isn't usually conveniently word aligned.
#define SET_UINT32(x, ptr) \
    x = (*ptr) | (*(ptr+1) << 8) | (*(ptr+2) << 16) | (*(ptr+3) << 24)
   
static void fill_pte(const uint8_t *ptebytes, PTEntry *pte)
{
   pte->status = *ptebytes++;
   pte->starthead = *ptebytes++;
   pte->startsec = *ptebytes++;
   pte->startcyl = *ptebytes++;
   pte->type = *ptebytes++;
   pte->endhead = *ptebytes++;
   pte->endsec = *ptebytes++;
   pte->endcyl = *ptebytes++;
   SET_UINT32(pte->startlba, ptebytes);
   ptebytes += 4;
   SET_UINT32(pte->sectors, ptebytes);

}
