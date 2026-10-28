/*
 * FileStick emulator - timers
 *
 * General purpose timer (toplevel.v, GP_TIMER):
 *   0x800004  W   stop value (byte writable), reset value 0xFFFFFFFF
 *   0x800008  W   bit 0: reset count, bit 1: ack interrupt, bit 2: enable
 *             R   bit 0: interrupt pending
 *   While enabled the count goes 0, 1 ... stop+1, 0 ... and the interrupt
 *   latches when the count equals stop.
 *
 * Econet timer A (timer.v), a one shot timer at 0x800300:
 *   +0  R   count
 *   +4  RW  timer value
 *   +8  RW  W: bit 0 reset count, bit 1 enable. R: bit 1 enable, bit 0 irq
 *   Counts up to the timer value and stops; the interrupt is a level that
 *   stays asserted while enabled and count == value.
 *
 * Neither timer is clocked cycle by cycle; the count is derived from the
 * cycle it was last written, and an event is scheduled for when the
 * interrupt will fire.
 */
#include "emu.h"

static struct {
   uint32_t stop;
   bool     enable;
   bool     intr;
   uint32_t count0;     // count at cycle c0
   uint64_t c0;
} gp;

static struct {
   uint32_t value;
   bool     enable;
   uint32_t count0;
   uint64_t c0;
} ta;

static void put_masked(uint32_t *reg, uint32_t v, unsigned wmask)
{
   for(int i = 0; i < 4; i++)
      if(wmask & (1 << i))
         *reg = (*reg & ~(0xFFu << (8 * i))) | (v & (0xFFu << (8 * i)));
}

//--------------------------------------------------------------------------
// General purpose timer
void timer_reset(void)
{
   gp.stop = 0xFFFFFFFF;
   gp.enable = false;
   gp.intr = false;
   gp.count0 = 0;
   gp.c0 = 0;
}

static uint64_t gp_period(void)
{
   return gp.stop == 0xFFFFFFFF ? (1ULL << 32) : (uint64_t)gp.stop + 2;
}

static uint32_t gp_count(uint64_t now)
{
   if(!gp.enable || now <= gp.c0)
      return gp.count0;

   uint64_t n = now - gp.c0;
   uint64_t r = gp.count0;
   if(r > gp.stop) {       // stop was lowered below the count: wraps to 0
      r = 0;
      n--;
   }
   return (r + n) % gp_period();
}

// The cycle on which the count next equals the stop value
static uint64_t gp_hit_time(void)
{
   if(!gp.enable)
      return gp.count0 == gp.stop ? gp.c0 : NEVER;
   if(gp.count0 <= gp.stop)
      return gp.c0 + (gp.stop - gp.count0);
   return gp.c0 + 1 + gp.stop;
}

void timer_update(uint64_t now)
{
   if(!gp.intr) {
      uint64_t hit = gp_hit_time();
      if(hit != NEVER && now > hit)
         gp.intr = true;
   }
}

uint64_t timer_next_event(void)
{
   if(gp.intr) return NEVER;
   uint64_t hit = gp_hit_time();
   return hit == NEVER ? NEVER : hit + 1;
}

bool timer_irq(void)
{
   return gp.intr;
}

static void gp_rebase(uint64_t now)
{
   timer_update(now);
   gp.count0 = gp_count(now);
   gp.c0 = now;
}

uint32_t timer_read(uint32_t addr)
{
   (void)addr;
   timer_update(emu_now);
   return gp.intr;
}

void timer_write(uint32_t addr, uint32_t wdata, unsigned wmask)
{
   if(!wmask) return;
   gp_rebase(emu_now);

   if(addr == DEV_TIMER_SET) {
      put_masked(&gp.stop, wdata, wmask);
   } else {
      if(wdata & 1) gp.count0 = 0;
      gp.enable = (wdata >> 2) & 1;
      if(wdata & 2) gp.intr = false;
   }
}

//--------------------------------------------------------------------------
// Econet timer A
void timera_reset(void)
{
   ta.value = 0;
   ta.enable = false;
   ta.count0 = 0;
   ta.c0 = 0;
}

static uint32_t ta_count(uint64_t now)
{
   if(!ta.enable || ta.count0 >= ta.value || now <= ta.c0)
      return ta.count0;
   uint64_t c = ta.count0 + (now - ta.c0);
   return c > ta.value ? ta.value : (uint32_t)c;
}

static void ta_rebase(uint64_t now)
{
   if(now <= ta.c0) return;   // still being held in reset
   ta.count0 = ta_count(now);
   ta.c0 = now;
}

uint64_t timera_next_event(void)
{
   if(ta.enable && ta.count0 < ta.value)
      return ta.c0 + (ta.value - ta.count0);
   return NEVER;
}

bool timera_irq(uint64_t now)
{
   return ta.enable && ta_count(now) == ta.value;
}

uint32_t timera_read(uint32_t addr)
{
   switch(addr & 0xC) {
      case 0x0: return ta_count(emu_now);
      case 0x4: return ta.value;
      case 0x8: return (ta.enable << 1) | timera_irq(emu_now);
   }
   return 0x55555555;
}

void timera_write(uint32_t addr, uint32_t wdata, unsigned wmask)
{
   ta_rebase(emu_now);

   switch(addr & 0xC) {
      case 0x4:
         put_masked(&ta.value, wdata, wmask);
         break;
      case 0x8:
         if(wmask & 1) {
            if(wdata & 1) {
               // the reset is registered, then holds the count for a cycle
               ta.count0 = 0;
               ta.c0 = emu_now + 2;
            }
            ta.enable = (wdata >> 1) & 1;
         }
         break;
   }
}
