/*
 * FileStick emulator - SPI controller (rtl/spi.v)
 *
 *   0x800020  data: a write loads the shift register and starts a transfer,
 *             a read starts a transfer of the last written data (unless one
 *             is already in progress) and returns the shifted in data.
 *             The CPU is stalled until the read data is available, or until
 *             a write can start when a transfer is already in progress.
 *   0x800024  immediate data: read/write the data registers, no transfer.
 *   0x800028  control:
 *               byte 0  bits 1:0  transfer size 0=8, 1=16, 2=24, 3=32 bits
 *               byte 1  bits 9:8  slave select
 *               byte 2  bit 16    big endian
 *               byte 3  bit 24    slave select active
 *
 * Data is shifted out MSB first from bit 31 of the shift register. In
 * little endian mode, the data is byte swapped on the way in and out so
 * that the low byte goes first. The SPI clock is the 10MHz system clock,
 * one bit per cycle.
 *
 * Transfers are carried out on the byte level at the moment they start;
 * only the timing is tracked cycle by cycle.
 *
 * Slave select 0 is the flash, 1 is the SD card.
 */
#include "emu.h"

static struct {
   unsigned bitcount;      // bits per transfer - 1
   unsigned ss;
   bool     big_endian;
   bool     ss_active;
   uint32_t reg_write;
   uint32_t reg_read;
   uint32_t shift_in;
   uint64_t busy_until;    // first cycle the controller is idle again
   int      selected;      // device currently selected, -1 for none
} spi;

static inline uint32_t bswap32(uint32_t v)
{
   return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

void spi_reset(void)
{
   spi.bitcount = 31;
   spi.ss = 0;
   spi.big_endian = true;
   spi.ss_active = false;
   spi.reg_write = 0;
   spi.reg_read = 0;
   spi.shift_in = 0;
   spi.busy_until = 0;
   spi.selected = -1;
}

static void select_device(int dev, bool sel)
{
   switch(dev) {
      case 0: flash_select(sel); break;
      case 1: sd_select(sel); break;
   }
}

static void update_select(void)
{
   int dev = spi.ss_active ? (int)spi.ss : -1;
   if(dev != spi.selected) {
      if(spi.selected >= 0) select_device(spi.selected, false);
      spi.selected = dev;
      if(dev >= 0) select_device(dev, true);
   }
}

static uint8_t device_xfer(uint8_t out)
{
   switch(spi.selected) {
      case 0: return flash_xfer(out);
      case 1: return sd_xfer(out);
   }
   return 0xFF;      // MISO pulled up
}

static void transfer(uint64_t start, uint32_t shift_out)
{
   spi.ss_active = true;
   update_select();

   unsigned bits = spi.bitcount + 1;
   uint32_t in = 0;
   for(unsigned i = 0; i < bits / 8; i++)
      in = (in << 8) | device_xfer(shift_out >> (24 - 8 * i));

   spi.shift_in = bits == 32 ? in : (spi.shift_in << bits) | in;
   spi.reg_read = spi.shift_in;

   // IDLE -> SHIFTING for bitcount+1 cycles -> IDLE
   spi.busy_until = start + spi.bitcount + 2;
}

static uint32_t read_data(void)
{
   return spi.big_endian ? spi.reg_read : bswap32(spi.reg_read);
}

uint32_t spi_read(uint32_t addr)
{
   switch((addr >> 2) & 3) {
      case 0:
         if(emu_now >= spi.busy_until)
            transfer(emu_now, spi.reg_write);
         bus_wait = spi.busy_until - emu_now;
         return read_data();
      case 1:
         return read_data();
      case 2:
         return (spi.ss_active << 24) | (spi.big_endian << 16) |
                (spi.ss << 8) | spi.bitcount;
   }
   return 0xBBBBBBBB;
}

void spi_write(uint32_t addr, uint32_t wdata, unsigned wmask)
{
   unsigned reg = (addr >> 2) & 3;

   if(reg == 2) {
      static const unsigned sizes[4] = { 7, 15, 23, 31 };
      if(wmask & 1) spi.bitcount = sizes[wdata & 3];
      if(wmask & 2) spi.ss = (wdata >> 8) & 3;
      if(wmask & 4) spi.big_endian = (wdata >> 16) & 1;
      // (the hardware defers a change made mid transfer until the end of
      // it, but our transfers are already complete at this point)
      if(wmask & 8) spi.ss_active = (wdata >> 24) & 1;
      update_select();
      return;
   }
   if(reg == 3) return;

   uint32_t data = spi.big_endian ? wdata : bswap32(wdata);
   unsigned mask = spi.big_endian ? wmask :
                   ((wmask & 1) << 3) | ((wmask & 2) << 1) |
                   ((wmask & 4) >> 1) | ((wmask & 8) >> 3);
   for(int i = 0; i < 4; i++)
      if(mask & (1 << i))
         spi.reg_write = (spi.reg_write & ~(0xFFu << (8 * i))) |
                         (data & (0xFFu << (8 * i)));

   if(reg == 0) {
      if(emu_now >= spi.busy_until) {
         // idle: the shift register is loaded directly from the bus
         transfer(emu_now, data);
      } else {
         // busy: held until the current transfer ends, then it is sent
         // from the write register
         uint64_t start = spi.busy_until;
         bus_wait = start - emu_now + 1;
         transfer(start, spi.reg_write);
      }
   }
}
