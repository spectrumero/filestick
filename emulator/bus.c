/*
 * FileStick emulator - memory map (rtl/toplevel.v)
 *
 * 0x000000 - 0x01FFFF  SPRAM (128K)
 * 0x020000 - 0x021BFF  Block RAM (boot loader, 7K)
 * 0x800000             RGB LED (write)
 * 0x800004             GP timer stop value (write)
 * 0x800008             GP timer control/status
 * 0x80000C             UART data
 * 0x800010             UART status
 * 0x800014             SD card detect
 * 0x800020 - 0x80002F  SPI controller
 * 0x800100 - 0x8001FF  Econet receive registers
 * 0x800200 - 0x8002FF  Econet transmit registers
 * 0x800300 - 0x80030F  Econet timer A
 * 0x800320             Econet hardware control
 * 0x810000 - 0x81FFFF  Econet receive buffer
 * 0x820000 - 0x82FFFF  Econet transmit buffer
 *
 * Unmapped addresses read as 0xDEADBEEF. All accesses are 32 bit aligned
 * words with a byte write mask, as on the real bus.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "emu.h"

uint8_t spram[SPRAM_SIZE];
uint8_t blkram[BLKRAM_SIZE];

static unsigned rgbled;

uint64_t next_event;
bool irq_line;

static inline uint32_t get32(const uint8_t *p)
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void put32_masked(uint8_t *p, uint32_t v, unsigned wmask)
{
   for(int i = 0; i < 4; i++)
      if(wmask & (1 << i))
         p[i] = v >> (8 * i);
}

//--------------------------------------------------------------------------
// Instruction fetch: no side effects, only memory makes sense here.
uint16_t bus_fetch16(uint32_t addr)
{
   addr &= ADDR_MASK & ~1u;
   if(addr < SPRAM_SIZE)
      return spram[addr] | (spram[addr + 1] << 8);
   if((addr >> 17) == 1) {
      uint32_t off = addr - BLKRAM_BASE;
      if(off < BLKRAM_SIZE)
         return blkram[off] | (blkram[off + 1] << 8);
      return 0;
   }
   return (addr & 2) ? 0xDEAD : 0xBEEF;
}

//--------------------------------------------------------------------------
static void rgbled_write(uint32_t wdata, unsigned wmask)
{
   if(!(wmask & 1)) return;
   unsigned v = wdata & 7;
   if(v != rgbled) {
      rgbled = v;
      VLOG("[led: %s%s%s%s]", v & 4 ? "R" : "", v & 2 ? "G" : "",
           v & 1 ? "B" : "", v ? "" : "off");
   }
}

//--------------------------------------------------------------------------
uint32_t bus_read(uint32_t addr)
{
   if(addr < SPRAM_SIZE)
      return get32(&spram[addr]);

   if((addr >> 17) == 1) {
      uint32_t off = addr - BLKRAM_BASE;
      return off < BLKRAM_SIZE ? get32(&blkram[off]) : 0;
   }

   uint32_t v;
   switch(addr) {
      case DEV_TIMER_CTL:  v = timer_read(addr); break;
      case DEV_UART_DATA:
      case DEV_UART_STATE: v = uart_read(addr); break;
      case DEV_SD_STATUS:  v = sd_detect_read(); break;
      default:
         if((addr >> 4) == 0x80002)
            v = spi_read(addr);
         else if((addr >> 4) == 0x80030)
            v = timera_read(addr);
         else if((addr >> 8) == 0x8001 || (addr >> 8) == 0x8002 ||
                 addr == 0x800320 ||
                 (addr >> 16) == 0x81 || (addr >> 16) == 0x82)
            v = econet_read(addr);
         else
            return 0xDEADBEEF;
   }

   // reads can have side effects too (e.g. popping the UART FIFO)
   devices_update(emu_now);
   return v;
}

//--------------------------------------------------------------------------
void bus_write(uint32_t addr, uint32_t wdata, unsigned wmask)
{
   if(addr < SPRAM_SIZE) {
      put32_masked(&spram[addr], wdata, wmask);
      return;
   }

   if((addr >> 17) == 1) {
      uint32_t off = addr - BLKRAM_BASE;
      if(off < BLKRAM_SIZE)
         put32_masked(&blkram[off], wdata, wmask);
      return;
   }

   switch(addr) {
      case DEV_RGBLED:     rgbled_write(wdata, wmask); return;
      case DEV_TIMER_SET:
      case DEV_TIMER_CTL:  timer_write(addr, wdata, wmask); break;
      case DEV_UART_DATA:  uart_write(addr, wdata, wmask); break;
      case DEV_SD_STATUS:  sd_detect_write(wdata, wmask); break;
      default:
         if((addr >> 4) == 0x80002)
            spi_write(addr, wdata, wmask);
         else if((addr >> 4) == 0x80030)
            timera_write(addr, wdata, wmask);
         else if((addr >> 8) == 0x8001 || (addr >> 8) == 0x8002 ||
                 addr == 0x800320 ||
                 (addr >> 16) == 0x81 || (addr >> 16) == 0x82)
            econet_write(addr, wdata, wmask);
         else
            return;
   }

   // a device register changed: interrupt and event state may differ now
   devices_update(emu_now);
}

//--------------------------------------------------------------------------
void devices_update(uint64_t now)
{
   timer_update(now);
   uart_update(now);

   irq_line = timer_irq() || timera_irq(now) || sd_detect_irq() || econet_irq();

   uint64_t n = timer_next_event();
   uint64_t t = timera_next_event();
   if(t < n) n = t;
   t = uart_next_event();
   if(t < n) n = t;
   next_event = n;
}

//--------------------------------------------------------------------------
// Load the boot ROM image into block RAM. Accepts a raw binary, or a
// $readmemh style hex file of 32 bit words (as used by the RTL) if the
// name ends in .hex.
int bus_load_boot(const char *path)
{
   FILE *f = fopen(path, "rb");
   if(!f) {
      emu_log("boot: %s: %s", path, strerror(errno));
      return -1;
   }

   memset(blkram, 0, sizeof(blkram));
   size_t len = strlen(path);
   size_t loaded = 0;

   if(len > 4 && !strcmp(path + len - 4, ".hex")) {
      char tok[64];
      uint32_t word = 0;
      int c;
      while(fscanf(f, "%63s", tok) == 1) {
         if(tok[0] == '/' && tok[1] == '/') {
            while((c = fgetc(f)) != EOF && c != '\n');
            continue;
         }
         if(tok[0] == '@') {
            word = strtoul(tok + 1, NULL, 16);
            continue;
         }
         if(!isxdigit((unsigned char)tok[0])) continue;
         uint32_t v = strtoul(tok, NULL, 16);
         if(word * 4 + 4 <= BLKRAM_SIZE) {
            put32_masked(&blkram[word * 4], v, 0xF);
            if(word * 4 + 4 > loaded) loaded = word * 4 + 4;
         }
         word++;
      }
   } else {
      loaded = fread(blkram, 1, BLKRAM_SIZE, f);
      if(fgetc(f) != EOF)
         emu_log("boot: warning: %s is larger than block RAM (%d bytes), truncated",
                 path, BLKRAM_SIZE);
   }
   fclose(f);

   VLOG("boot: loaded %zu bytes from %s", loaded, path);
   return 0;
}
