/*
 * FileStick emulator - AT25SF081 compatible SPI serial flash, SPI CS 0
 *
 * The flash contents live in a file, which is memory mapped so that erase
 * and program operations go straight to it. The AT25SF081 is 1MB; a larger
 * image file is used at its own size (rounded up to a power of two, up to
 * the 16MB that 24 bit addressing allows).
 *
 * Supported commands:
 *   03 read, 0B fast read, 9F JEDEC id, 90 manufacturer/device id,
 *   AB release from deep power down / device id, B9 deep power down,
 *   05/35 read status register 1/2, 01/31 write status register (ignored),
 *   06 write enable, 04 write disable, 02 page program,
 *   20/52/D8 4K/32K/64K block erase, 60/C7 chip erase, 66/99 reset.
 *
 * Program and erase operations happen immediately, but the busy bit in the
 * status register stays set for a typical device time so software polling
 * it sees realistic timings.
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "emu.h"

#define FLASH_DEFAULT_SIZE (1024 * 1024)
#define FLASH_MAX_SIZE     (16 * 1024 * 1024)
#define PAGE_SIZE          256

#define JEDEC_MFR          0x1F     // Adesto/Atmel
#define JEDEC_DEV1         0x85
#define JEDEC_DEV2         0x01
#define DEVICE_ID          0x13

// typical times, in microseconds
#define T_PP_US            400
#define T_BE4_US           60000
#define T_BE32_US          250000
#define T_BE64_US          450000
#define T_CE_US            10000000

#define SR_BUSY            0x01
#define SR_WEL             0x02

static uint8_t  *mem;
static uint32_t size;
static int      fd = -1;

static struct {
   bool     selected;
   bool     power_down;
   bool     wel;
   uint64_t busy_until;
   uint8_t  cmd;
   unsigned count;         // bytes received since CS asserted
   uint32_t addr;
   uint8_t  page[PAGE_SIZE];
   bool     page_used[PAGE_SIZE];
} fl;

static bool busy(void)
{
   return emu_now < fl.busy_until;
}

static void set_busy(uint64_t us)
{
   fl.busy_until = emu_now + us * (CPU_HZ / 1000000);
}

//--------------------------------------------------------------------------
int flash_open(const char *path)
{
   fd = open(path, O_RDWR | O_CREAT, 0644);
   if(fd < 0) {
      emu_log("flash: %s: %s", path, strerror(errno));
      return -1;
   }

   struct stat st;
   fstat(fd, &st);
   uint32_t fsize = st.st_size > FLASH_MAX_SIZE ? FLASH_MAX_SIZE : st.st_size;
   size = FLASH_DEFAULT_SIZE;
   while(size < fsize) size <<= 1;

   if((uint32_t)st.st_size < size) {
      // pad to full size with erased flash
      static uint8_t ff[4096];
      memset(ff, 0xFF, sizeof(ff));
      lseek(fd, st.st_size, SEEK_SET);
      for(off_t left = size - st.st_size; left > 0; ) {
         ssize_t n = write(fd, ff, left > (off_t)sizeof(ff) ? (off_t)sizeof(ff) : left);
         if(n <= 0) {
            emu_log("flash: cannot extend %s: %s", path, strerror(errno));
            return -1;
         }
         left -= n;
      }
      VLOG("flash: extended %s to %u bytes", path, size);
   }

   mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if(mem == MAP_FAILED) {
      emu_log("flash: mmap %s: %s", path, strerror(errno));
      mem = NULL;
      return -1;
   }

   memset(&fl, 0, sizeof(fl));
   VLOG("flash: %s, %u bytes", path, size);
   return 0;
}

// Copy a file into the flash image, like programming it with a flash tool.
int flash_load(uint32_t offset, const char *path)
{
   int in = open(path, O_RDONLY);
   if(in < 0) {
      emu_log("flash: %s: %s", path, strerror(errno));
      return -1;
   }

   uint32_t pos = offset;
   ssize_t n;
   while(pos < size && (n = read(in, mem + pos, size - pos)) > 0)
      pos += n;
   if(pos >= size && read(in, &n, 1) > 0)
      emu_log("flash: warning: %s doesn't fit at offset 0x%x, truncated", path, offset);
   close(in);

   emu_log("flash: wrote %s at 0x%06x-0x%06x", path, offset, pos - 1);
   return 0;
}

void flash_close(void)
{
   if(mem) {
      msync(mem, size, MS_SYNC);
      munmap(mem, size);
      mem = NULL;
   }
   if(fd >= 0) close(fd);
   fd = -1;
}

//--------------------------------------------------------------------------
static void erase(uint32_t addr, uint32_t len, uint64_t us)
{
   addr &= ~(len - 1) & (size - 1);
   if(len > size) len = size;
   memset(mem + addr, 0xFF, len);
   set_busy(us);
   VLOG("flash: erase 0x%06x-0x%06x", addr, addr + len - 1);
}

// Commands are carried out when CS goes high.
static void end_command(void)
{
   if(fl.count == 0) return;

   switch(fl.cmd) {
      case 0x06:
         if(fl.count == 1) fl.wel = true;
         break;
      case 0x04:
         fl.wel = false;
         break;
      case 0xB9:
         if(fl.count == 1) fl.power_down = true;
         break;
      case 0x01:
      case 0x31:
         fl.wel = false;
         break;

      case 0x02:
         if(fl.wel && fl.count > 4) {
            uint32_t base = fl.addr & ~(PAGE_SIZE - 1) & (size - 1);
            for(int i = 0; i < PAGE_SIZE; i++)
               if(fl.page_used[i])
                  mem[base + i] &= fl.page[i];    // programming only clears bits
            set_busy(T_PP_US);
            VLOG("flash: program page 0x%06x", base);
         }
         fl.wel = false;
         break;

      case 0x20:
         if(fl.wel && fl.count == 4) erase(fl.addr, 4096, T_BE4_US);
         fl.wel = false;
         break;
      case 0x52:
         if(fl.wel && fl.count == 4) erase(fl.addr, 32768, T_BE32_US);
         fl.wel = false;
         break;
      case 0xD8:
         if(fl.wel && fl.count == 4) erase(fl.addr, 65536, T_BE64_US);
         fl.wel = false;
         break;
      case 0x60:
      case 0xC7:
         if(fl.wel && fl.count == 1) erase(0, size, T_CE_US);
         fl.wel = false;
         break;
   }
}

void flash_select(bool selected)
{
   if(!selected && fl.selected)
      end_command();
   fl.selected = selected;
   fl.count = 0;
}

uint8_t flash_xfer(uint8_t out)
{
   if(!mem || !fl.selected) return 0xFF;

   unsigned n = fl.count++;
   if(n == 0) {
      fl.cmd = out;
      fl.addr = 0;
      if(out == 0x02) memset(fl.page_used, 0, sizeof(fl.page_used));
      if(fl.power_down && out != 0xAB) fl.cmd = 0;
      // only status reads work while an operation is in progress
      if(busy() && out != 0x05 && out != 0x35) fl.cmd = 0;
      return 0xFF;
   }

   switch(fl.cmd) {
      case 0x03:     // read
      case 0x0B:     // fast read (one dummy byte)
         if(n <= 3) {
            fl.addr = (fl.addr << 8) | out;
            return 0xFF;
         }
         if(fl.cmd == 0x0B && n == 4)
            return 0xFF;
         return mem[fl.addr++ & (size - 1)];

      case 0x9F: {
         static const uint8_t id[] = { JEDEC_MFR, JEDEC_DEV1, JEDEC_DEV2, 0x00 };
         return n <= sizeof(id) ? id[n - 1] : 0x00;
      }

      case 0x90:
         if(n <= 3) {
            fl.addr = (fl.addr << 8) | out;
            return 0xFF;
         }
         return ((n - 4) ^ fl.addr) & 1 ? DEVICE_ID : JEDEC_MFR;

      case 0xAB:
         fl.power_down = false;
         return n <= 3 ? 0xFF : DEVICE_ID;

      case 0x05:
         return (busy() ? SR_BUSY : 0) | (fl.wel ? SR_WEL : 0);
      case 0x35:
         return 0x00;

      case 0x02:     // page program: data wraps within the page
         if(n <= 3) {
            fl.addr = (fl.addr << 8) | out;
            return 0xFF;
         }
         {
            unsigned i = (fl.addr + n - 4) & (PAGE_SIZE - 1);
            fl.page[i] = fl.page_used[i] ? (fl.page[i] & out) : out;
            fl.page_used[i] = true;
         }
         return 0xFF;

      case 0x20:
      case 0x52:
      case 0xD8:
         if(n <= 3) fl.addr = (fl.addr << 8) | out;
         return 0xFF;

      case 0x66:
         return 0xFF;
      case 0x99:
         fl.wel = false;
         return 0xFF;
   }
   return 0xFF;
}
