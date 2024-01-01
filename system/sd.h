#ifndef _SD_H
#define _SD_H

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
#include "ff.h"
#include "diskio.h"

#define SD_SLAVE_ID     1     // SPI slave id

typedef struct _sdcmd {
   uint8_t cmd;
   uint8_t arg[4];
   uint8_t crc;
} SDCmd;

typedef struct _sd_r37 {
   uint8_t r1;
   uint8_t r7data[4];
} SD_R37;

#define CMD0      0x40     // reset/SPI mode on
#define CMD0SEQ   { CMD0, { 0x00, 0x00, 0x00, 0x00 }, 0x95 }
#define CMD8      0x48     // SEND_IF_COND send interface condition
#define CMD8SEQ   { CMD8, { 0x00, 0x00, 0x01, 0xAA }, 0x87 }
#define CMD17     0x51     // READ_SINGLE_BLOCK
#define CMD24     0x58     // WRITE_SINGLE_BLOCK
#define CMD55     0x77     // Next command is an ACMD
#define CMD55SEQ  { CMD55, {0x00, 0x00, 0x00, 0x00 }, 0x00 }
#define CMD58     0x7A     // READ_OCR, read operations condition register
#define CMD58SEQ  { CMD58, { 0x00, 0x00, 0x00, 0x00 }, 0x00 }

#define ACMD41    0x69     // SD_SEND_OP_COND, send operating condition
#define ACMD41SEQ { ACMD41, { 0x40, 0x00, 0x00, 0x00 }, 0x00 }
#define ACMD41V1SEQ { ACMD41, { 0x00, 0x00, 0x00, 0x00 }, 0x00 }

// SD defines
#define SD_READY 0x00
#define SD_START_TOKEN 0xFE
#define SD_ERROR_TOKEN 0x00
#define SD_DATA_ACCEPTED 0x05
#define SD_DATA_REJECTED_CRC 0x08
#define SD_DATA_REJECTED_WRITE 0x0D

// Low level return codes
#define SD_SUCCESS 0
#define SD_IDLEFAIL -1        // CMD0 failed
#define SD_IFCONDFAIL -2      // CMD8 failed
#define SD_READOCRFAIL -3     // CMD58 failed
#define SD_READFAILED -4      // READ failed
#define SD_INITFAIL -5        // ACMD41 failed

// Initialize the sd card.
// Returns 0 for success or negative error number.
int sd_init();

// Read a single 512 byte block
uint8_t sd_readsingleblk(uint32_t addr, uint8_t *buf, uint8_t *token); 

// Write a single 512 byte block
uint8_t sd_writesingleblk(uint32_t addr, const uint8_t *buf, uint8_t *token); 

// Deassert SD slave select
void  sd_done();

// Logical disc functions for FatFS
// Status indicator
extern DSTATUS sd_status;

// Read the partition table and initialize
DRESULT sd_readPT();

// Read a logical sector
DRESULT sd_read(BYTE *buf, LBA_t sector, UINT count);

// Write a logical sector
DRESULT sd_write(const BYTE *buf, LBA_t sector, UINT count);

// Do an ioctl
DRESULT sd_ioctl(BYTE cmd, void *buf);

#endif

