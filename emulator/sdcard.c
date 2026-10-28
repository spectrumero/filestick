/*
 * FileStick emulator - SD card in SPI mode, SPI CS 1, plus the card detect
 * register (rtl/sdcard.v).
 *
 * The card is an SDHC card (block addressed) whose contents are a file;
 * the capacity is the file size rounded down to a whole number of 512
 * byte blocks.
 *
 * Supported commands: CMD0, CMD8, CMD9, CMD10, CMD12, CMD13, CMD16, CMD17,
 * CMD18, CMD24, CMD25, CMD55, CMD58, CMD59, ACMD13, ACMD23, ACMD41, ACMD51.
 *
 * Card detect, 0x800014:
 *   R  bit 1 card present, bit 0 pin changed (this is an interrupt source)
 *   W  bit 0 = 1 clears the pin changed flag
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "emu.h"

#define BLOCK_SIZE      512
#define OUT_SIZE        1024
#define WRITE_BUSY      8        // bytes of busy after a block write

#define R1_IDLE         0x01
#define R1_ILLEGAL      0x04
#define R1_ADDRESS      0x20
#define R1_PARAMETER    0x40

enum { ST_CMD, ST_WRITE_TOKEN, ST_WRITE_DATA };

static int      fd = -1;
static uint32_t nblocks;
static bool     present;

static struct {
   bool     selected;
   bool     idle;
   bool     app_cmd;
   unsigned acmd41_count;
   uint8_t  cmd[6];
   unsigned cmd_len;

   uint8_t  out[OUT_SIZE];       // bytes queued to send to the host
   unsigned out_head, out_len;

   int      state;
   bool     multi_read, multi_write;
   uint32_t block;
   uint8_t  data[BLOCK_SIZE + 2];
   unsigned data_len;
} sd;

static struct {
   bool     last_state;
   bool     pin_change;
} det;

//--------------------------------------------------------------------------
int sd_open(const char *path)
{
   fd = open(path, O_RDWR);
   if(fd < 0) {
      emu_log("sd: %s: %s", path, strerror(errno));
      return -1;
   }

   struct stat st;
   fstat(fd, &st);
   uint64_t blocks = st.st_size / BLOCK_SIZE;
   nblocks = blocks > UINT32_MAX ? UINT32_MAX : blocks;
   if(nblocks == 0)
      emu_log("sd: warning: %s is smaller than one block", path);

   present = true;
   memset(&sd, 0, sizeof(sd));
   sd.idle = true;
   VLOG("sd: %s, %u blocks", path, nblocks);
   return 0;
}

void sd_close(void)
{
   if(fd >= 0) {
      fsync(fd);
      close(fd);
   }
   fd = -1;
}

//--------------------------------------------------------------------------
static uint8_t crc7(const uint8_t *p, int len)
{
   uint8_t crc = 0;
   for(int i = 0; i < len; i++) {
      uint8_t b = p[i];
      for(int j = 0; j < 8; j++) {
         crc <<= 1;
         if((b ^ crc) & 0x80) crc ^= 0x09;
         b <<= 1;
      }
   }
   return crc & 0x7F;
}

static uint16_t crc16(const uint8_t *p, int len)
{
   uint16_t crc = 0;
   for(int i = 0; i < len; i++) {
      crc ^= (uint16_t)p[i] << 8;
      for(int j = 0; j < 8; j++)
         crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
   }
   return crc;
}

static void push(uint8_t b)
{
   if(sd.out_len < OUT_SIZE)
      sd.out[(sd.out_head + sd.out_len++) % OUT_SIZE] = b;
}

// Queue a data block: Nac gap, start token, data, CRC
static void push_data(const uint8_t *p, int len)
{
   push(0xFF);
   push(0xFE);
   for(int i = 0; i < len; i++) push(p[i]);
   uint16_t crc = crc16(p, len);
   push(crc >> 8);
   push(crc);
}

static bool push_block(uint32_t block)
{
   uint8_t buf[BLOCK_SIZE];

   if(block >= nblocks ||
      pread(fd, buf, BLOCK_SIZE, (off_t)block * BLOCK_SIZE) != BLOCK_SIZE) {
      push(0xFF);
      push(0x08);       // data error token: out of range
      return false;
   }
   push_data(buf, BLOCK_SIZE);
   return true;
}

static void push_csd(void)
{
   uint32_t c_size = nblocks >= 1024 ? nblocks / 1024 - 1 : 0;
   uint8_t csd[16] = {
      0x40, 0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00,
      (c_size >> 16) & 0x3F, c_size >> 8, c_size,
      0x7F, 0x80, 0x0A, 0x40, 0x00, 0x00
   };
   csd[15] = (crc7(csd, 15) << 1) | 1;
   push_data(csd, 16);
}

static void push_cid(void)
{
   uint8_t cid[16] = {
      0x00, 'F', 'S', 'E', 'M', 'U', 'S', 'D', 0x10,
      0x12, 0x34, 0x56, 0x78, 0x01, 0x9A, 0x00
   };
   cid[15] = (crc7(cid, 15) << 1) | 1;
   push_data(cid, 16);
}

//--------------------------------------------------------------------------
static void command(void)
{
   unsigned idx = sd.cmd[0] & 0x3F;
   uint32_t arg = ((uint32_t)sd.cmd[1] << 24) | (sd.cmd[2] << 16) |
                  (sd.cmd[3] << 8) | sd.cmd[4];
   bool app = sd.app_cmd;
   uint8_t r1 = sd.idle ? R1_IDLE : 0;

   sd.app_cmd = false;
   sd.out_len = 0;
   sd.multi_read = false;

   VLOG("sd: %sCMD%u %08x", app ? "A" : "", idx, arg);

   push(0xFF);      // Ncr: one byte before the response

   // most commands aren't accepted until the card is initialised
   bool init_cmd = idx == 0 || idx == 8 || idx == 55 || idx == 58 ||
                   idx == 59 || (app && idx == 41);
   if(sd.idle && !init_cmd) {
      push(r1 | R1_ILLEGAL);
      return;
   }

   if(app) {
      switch(idx) {
         case 41:
            if(++sd.acmd41_count >= 2) sd.idle = false;
            push(sd.idle ? R1_IDLE : 0);
            return;
         case 13: {
            uint8_t status[64] = { 0 };
            push(r1);
            push(0x00);
            push_data(status, sizeof(status));
            return;
         }
         case 23:
            push(r1);
            return;
         case 51: {
            uint8_t scr[8] = { 0x02, 0x35, 0x80, 0x00, 0, 0, 0, 0 };
            push(r1);
            push_data(scr, sizeof(scr));
            return;
         }
      }
      // otherwise treat it as the normal command
   }

   switch(idx) {
      case 0:
         sd.idle = true;
         sd.acmd41_count = 0;
         sd.state = ST_CMD;
         push(R1_IDLE);
         break;

      case 8:
         push(r1);
         push(0x00);
         push(0x00);
         push((arg >> 8) & 0x0F);
         push(arg & 0xFF);
         break;

      case 9:
         push(r1);
         push_csd();
         break;

      case 10:
         push(r1);
         push_cid();
         break;

      case 12:
         push(r1);
         push(0x00);       // busy
         push(0x00);
         break;

      case 13:
         push(r1);
         push(0x00);
         break;

      case 16:
         push(arg == BLOCK_SIZE ? r1 : r1 | R1_PARAMETER);
         break;

      case 17:
      case 18:
         if(arg >= nblocks) {
            push(r1 | R1_ADDRESS);
            break;
         }
         push(r1);
         sd.block = arg;
         if(push_block(sd.block++) && idx == 18)
            sd.multi_read = true;
         break;

      case 24:
      case 25:
         if(arg >= nblocks) {
            push(r1 | R1_ADDRESS);
            break;
         }
         push(r1);
         sd.block = arg;
         sd.multi_write = idx == 25;
         sd.state = ST_WRITE_TOKEN;
         break;

      case 55:
         sd.app_cmd = true;
         push(r1);
         break;

      case 58:
         push(r1);
         push((sd.idle ? 0x00 : 0x80) | 0x40);   // power up done, CCS (SDHC)
         push(0xFF);                             // 2.7 - 3.6V
         push(0x80);
         push(0x00);
         break;

      case 59:
         push(r1);
         break;

      default:
         push(r1 | R1_ILLEGAL);
   }
}

//--------------------------------------------------------------------------
static void write_block(void)
{
   if(sd.block < nblocks &&
      pwrite(fd, sd.data, BLOCK_SIZE, (off_t)sd.block * BLOCK_SIZE) == BLOCK_SIZE) {
      VLOG("sd: wrote block %u", sd.block);
      push(0x05);       // data accepted
   } else {
      push(0x0D);       // write error
   }
   for(int i = 0; i < WRITE_BUSY; i++)
      push(0x00);
   sd.block++;
}

void sd_select(bool selected)
{
   sd.selected = selected;
   sd.cmd_len = 0;
}

uint8_t sd_xfer(uint8_t in)
{
   if(!present || !sd.selected) return 0xFF;

   // what the card sends while this byte comes in
   uint8_t out = 0xFF;
   if(!sd.out_len && sd.multi_read)
      sd.multi_read = push_block(sd.block++);
   if(sd.out_len) {
      out = sd.out[sd.out_head];
      sd.out_head = (sd.out_head + 1) % OUT_SIZE;
      sd.out_len--;
   }

   switch(sd.state) {
      case ST_WRITE_TOKEN:
         if(in == (sd.multi_write ? 0xFC : 0xFE)) {
            sd.state = ST_WRITE_DATA;
            sd.data_len = 0;
         } else if(sd.multi_write && in == 0xFD) {
            sd.state = ST_CMD;      // stop transmission
            sd.out_len = 0;
            push(0xFF);
            for(int i = 0; i < WRITE_BUSY; i++) push(0x00);
         }
         break;

      case ST_WRITE_DATA:
         sd.data[sd.data_len++] = in;
         if(sd.data_len == BLOCK_SIZE + 2) {
            sd.out_len = 0;
            write_block();
            sd.state = sd.multi_write ? ST_WRITE_TOKEN : ST_CMD;
         }
         break;

      default:
         if(sd.cmd_len == 0 && (in & 0xC0) != 0x40)
            break;
         sd.cmd[sd.cmd_len++] = in;
         if(sd.cmd_len == 6) {
            sd.cmd_len = 0;
            command();
         }
   }
   return out;
}

//--------------------------------------------------------------------------
// Card detect switch
void sd_detect_reset(void)
{
   det.last_state = false;
   det.pin_change = false;
}

static void detect_eval(void)
{
   if(present != det.last_state && !det.pin_change) {
      det.pin_change = true;
      det.last_state = present;
   }
}

bool sd_detect_irq(void)
{
   detect_eval();
   return det.pin_change;
}

uint32_t sd_detect_read(void)
{
   detect_eval();
   return (present << 1) | det.pin_change;
}

void sd_detect_write(uint32_t wdata, unsigned wmask)
{
   if((wmask & 1) && (wdata & 1))
      det.pin_change = false;
}
