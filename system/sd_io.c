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

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "spi.h"
#include "devices.h"
#include "sd.h"

static uint8_t sd_readres1(void); 
static void sd_readres37(SD_R37 *res); 

//----------------------------------------------------------
// Initialize the SD card.
int sd_init() {
   uint16_t i;
   uint8_t rx, attempts;
   SDCmd idle=CMD0SEQ;
   SDCmd if_cond=CMD8SEQ;
   SDCmd read_ocr=CMD58SEQ;
   SDCmd acmd=CMD55SEQ;
   SDCmd op_cond=ACMD41SEQ;
   SD_R37 r37resp;            // responses 3 and 7

   // set SPI slave = 1
   spi_set_slave(SD_SLAVE_ID);

   // Send 10 dummy bytes
   for(i=0; i < 10; i++) {
      spi_byte(0xFF);
   }

   spi_write(&idle, sizeof(idle), false);
   rx=sd_readres1();
   if(rx != 1)
      return SD_IDLEFAIL;

   // Send CMD8
   spi_write(&if_cond, sizeof(if_cond), false);
   sd_readres37(&r37resp); 
   if(r37resp.r1 > 1)
      return SD_IFCONDFAIL;

   // Attempt initialization
   attempts=0;
   for(;;) {
      if(attempts++ > 50) {
         return SD_INITFAIL;
      }

      spi_write(&acmd, sizeof(acmd), false);
      rx = sd_readres1();

      if(rx < 2) {
         spi_write(&op_cond, sizeof(op_cond), false);
         rx = sd_readres1();
      }

      if(rx == SD_READY)
         break;

      // delay a few ms
      for(i=0; i < 32767; i++) {
         asm volatile("nop");
      }
   } 

   // send CMD58
   spi_write(&read_ocr, sizeof(read_ocr), false);
   sd_readres37(&r37resp);
   if(r37resp.r1 > 1)
      return SD_READOCRFAIL;

   return SD_SUCCESS;
}

static uint8_t sd_readres1() {
   uint8_t i = 0;
   uint8_t res1;

   while((res1 = spi_byte(0xFF)) == 0xFF) {
      i++;
      if(i > 254) break;
   }
   return res1;
}

static void sd_readres37(SD_R37 *res) {
   res->r1 = sd_readres1();

   if(res->r1 > 1) return;
   res->r7data[0] = spi_byte(0xFF);
   res->r7data[1] = spi_byte(0xFF);
   res->r7data[2] = spi_byte(0xFF);
   res->r7data[3] = spi_byte(0xFF);
}

static SDCmd rsb={ CMD17, { 0x00, 0x00, 0x00, 0x00 }, 0x00 };

//---------------------------------------------------------------------
// Read a 512 byte block.
uint8_t sd_readsingleblk(uint32_t addr, uint8_t *buf, uint8_t *token) {
   uint8_t rx, read;
   uint16_t readAttempts, i;

   spi_set_slave(SD_SLAVE_ID);

   *token=0xFF;
   rsb.arg[0] = (uint8_t)(addr >> 24);
   rsb.arg[1] = (uint8_t)(addr >> 16);
   rsb.arg[2] = (uint8_t)(addr >> 8);
   rsb.arg[3] = (uint8_t)addr;

   spi_write(&rsb, sizeof(rsb), false);

   rx = sd_readres1();
   if(rx != 0xFF) {
      readAttempts = 0;
      while(++readAttempts != 65535)
         if((read = spi_byte(0xFF)) != 0xFF) break;

      if(read == 0xFE) {
         spi_read(buf, 512, false, 0xFFFFFFFF);

         // CRC bytes
         spi_byte(0xFF);
         spi_byte(0xFF);
      }
      *token = read;
   }

   return rx;
}

//----------------------------------------------------------------------------
// Write a 512 byte block
static SDCmd wsb={ CMD24, { 0x00, 0x00, 0x00, 0x00 }, 0x00 };

uint8_t sd_writesingleblk(uint32_t addr, const uint8_t *buf, uint8_t *token) {
   uint8_t read, rx;
   uint16_t i, readAttempts;

   spi_set_slave(SD_SLAVE_ID);

   wsb.arg[0] = (uint8_t)(addr >> 24);
   wsb.arg[1] = (uint8_t)(addr >> 16);
   wsb.arg[2] = (uint8_t)(addr >> 8);
   wsb.arg[3] = (uint8_t)addr;

   spi_write(&wsb, sizeof(wsb), false);
   rx = sd_readres1();

   if(rx ==  SD_READY) {
      spi_byte(SD_START_TOKEN);

      spi_write(buf, 512, false);

      for(readAttempts = 0; readAttempts < 65535; readAttempts++) {
         read = spi_byte(0xFF);
         if(read != 0xFF) {
            break;
         }
      }

      *token = read & 0x1F;
      if(*token == 0x05) {
         // wait for write to finish
         readAttempts=0;
         while(spi_byte(0xFF) == 0x00) {
            if(++readAttempts == 65535) {
               // timed out
               *token=0;
               break;
            }
         }
      }
   }

   return rx;
}

