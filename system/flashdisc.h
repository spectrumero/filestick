#ifndef _FLASHDISC_H
#define _FLASHDISC_H

#include "diskio.h"     // FatFs

#define SECTOR_COUNT 512      // 256k
#define SECTOR_SIZE  512
#define BLOCK_SIZE   4096     // erase block size
#define FS_START     0x40000  // flash start address for filesystem

#define CMD_READ     0x03     // flash command: read
#define CMD_WREN     0x06     // flash command: write enable
#define CMD_SE       0x20     // flash command: sector erase (4k)
#define CMD_PP       0x02     // flash command: page program
#define CMD_RDSR     0x05     // flash command: read status register

// Onboard SPI flash functions
DSTATUS intflash_init();
DRESULT intflash_read(BYTE *buf, LBA_t sector, UINT count);
DRESULT intflash_ioctl(BYTE cmd, void *buf);
DRESULT intflash_write(const BYTE *buf, LBA_t sector, UINT count);
DRESULT intflash_sync();
void intflash_release();
#endif
