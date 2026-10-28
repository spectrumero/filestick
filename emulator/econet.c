/*
 * FileStick emulator - Econet interface stub
 *
 * The econet hardware isn't emulated yet. This provides the register
 * interface so that the system software sees an idle network: nothing is
 * ever received, and frames written to the transmitter vanish (the
 * transmitter immediately reports that it is not busy).
 *
 *   0x800100-0x8001FF  receive registers (econet_rx_buffered.v)
 *   0x810000-0x81FFFF  receive buffer (2K, read only from the CPU)
 *   0x800200-0x8002FF  transmit registers (econet_tx_buffered.v)
 *   0x820000-0x82FFFF  transmit buffer (2K, write only from the CPU)
 *   0x800320           hardware control (econet_hwctl.v)
 *
 * Econet timer A at 0x800300 is a general purpose timer, see timer.c.
 */
#include <string.h>

#include "emu.h"

#define BUF_WORDS       512      // 2K buffers

// receive register numbers
#define RX_REG_OUR_ADDRESS 6
#define RX_REG_STATUS      7

// transmit register numbers
#define TX_REG_START       0
#define TX_REG_END         1
#define TX_REG_CONTROL     2

static uint32_t rx_buf[BUF_WORDS];
static uint32_t tx_buf[BUF_WORDS];

static uint16_t our_address;
static bool     monitor_mode;
static uint32_t tx_start, tx_end;
static bool     turnaround;
static unsigned hwctl = 7 << 8;  // clock divider resets to 7

void econet_reset(void)
{
   memset(rx_buf, 0, sizeof(rx_buf));
   memset(tx_buf, 0, sizeof(tx_buf));
   our_address = 0;
   monitor_mode = false;
   tx_start = tx_end = 0;
   turnaround = false;
   hwctl = 7 << 8;
}

bool econet_irq(void)
{
   return false;     // no frames are ever received
}

uint32_t econet_read(uint32_t addr)
{
   unsigned word = (addr >> 2) & (BUF_WORDS - 1);

   if((addr >> 16) == 0x81)
      return rx_buf[word];
   if((addr >> 16) == 0x82)
      return 0xDEADBEEF;   // no read port on the transmit buffer

   if((addr >> 8) == 0x8001) {
      switch((addr >> 2) & 0xF) {
         case RX_REG_OUR_ADDRESS: return our_address;
         case RX_REG_STATUS:      return monitor_mode << 8;
      }
      return 0;
   }

   if((addr >> 8) == 0x8002) {
      switch((addr >> 2) & 3) {
         case TX_REG_START:   return tx_start;
         case TX_REG_END:     return tx_end;
         case TX_REG_CONTROL: return turnaround;   // never busy/transmitting
      }
      return 0x55555555;
   }

   if(addr == 0x800320)
      return hwctl;

   return 0xDEADBEEF;
}

void econet_write(uint32_t addr, uint32_t wdata, unsigned wmask)
{
   unsigned word = (addr >> 2) & (BUF_WORDS - 1);

   if((addr >> 16) == 0x82) {
      for(int i = 0; i < 4; i++)
         if(wmask & (1 << i))
            tx_buf[word] = (tx_buf[word] & ~(0xFFu << (8 * i))) |
                           (wdata & (0xFFu << (8 * i)));
      return;
   }

   if((addr >> 8) == 0x8001) {
      switch((addr >> 2) & 0xF) {
         case RX_REG_OUR_ADDRESS:
            if(wmask & 1) our_address = (our_address & 0xFF00) | (wdata & 0xFF);
            if(wmask & 2) our_address = (our_address & 0x00FF) | (wdata & 0xFF00);
            break;
         case RX_REG_STATUS:
            if(wmask & 2) monitor_mode = (wdata >> 8) & 1;
            break;
      }
      return;
   }

   if((addr >> 8) == 0x8002) {
      switch((addr >> 2) & 3) {
         case TX_REG_START:   tx_start = wdata & 0x7FF; break;
         case TX_REG_END:
            tx_end = wdata & 0x7FF;
            VLOG("econet: frame of %u bytes dropped (not emulated)",
                 (tx_end - tx_start + 1) & 0x7FF);
            break;
         case TX_REG_CONTROL: turnaround = wdata & 1; break;
      }
      return;
   }

   if(addr == 0x800320) {
      if(wmask & 1) hwctl = (hwctl & ~3u) | (wdata & 3);
      if(wmask & 2) hwctl = (hwctl & ~0x700u) | (wdata & 0x700);
   }
}
