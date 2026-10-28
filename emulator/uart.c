/*
 * FileStick emulator - FIFO'd UART (rtl/fifo_uart.v, uart_picosoc_shrunk.v)
 *
 *   0x80000C  R   pop a byte from the receive FIFO
 *             W   transmit a byte (bits 7:0)
 *   0x800010  R   bits 31:16 bytes in FIFO, bit 2 FIFO full (CTS),
 *                 bit 1 transmitter busy, bit 0 data ready
 *
 * The serial line is a Linux pseudo terminal (or stdin/stdout). Received
 * bytes go into the 512 byte FIFO at the 115200 baud byte rate; when the
 * FIFO is full we stop reading the host side, which is the emulated
 * equivalent of the hardware flow control.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "emu.h"

#define BAUD               115200
#define DIVIDER            (CPU_HZ / BAUD)            // 86
#define TX_BUSY_CYCLES     (10 * (DIVIDER + 2))       // 10 bits of the baud counter
#define RX_BYTE_CYCLES     (10 * CPU_HZ / BAUD)       // byte time on the wire

#define FIFO_SIZE          512
#define FIFO_MASK          (FIFO_SIZE - 1)

#define ESCAPE_CHAR        0x1D                       // ctrl-]

bool uart_quit_requested;

static struct {
   uint8_t  fifo[FIFO_SIZE];
   unsigned in_ptr, out_ptr;  // 9 bit pointers, as in fifo.v
   uint8_t  data_out;
   uint64_t tx_busy_until;
   uint64_t rx_next;          // earliest cycle the next byte can arrive
} u;

// host side
static int in_fd = -1, out_fd = -1;
static int slave_fd = -1;
static bool stdio_mode;
static struct termios saved_tios;
static bool tios_saved;
static char *link_path;

static uint8_t pending[FIFO_SIZE];  // read from the host, not yet in the FIFO
static unsigned pend_head, pend_count;

static uint8_t txbuf[65536];
static unsigned tx_count;

static inline unsigned fifo_avail(void)
{
   return (u.in_ptr - u.out_ptr) & FIFO_MASK;
}

static inline bool fifo_full(void)
{
   return fifo_avail() == FIFO_SIZE - 1;
}

//--------------------------------------------------------------------------
void uart_reset(void)
{
   memset(&u, 0, sizeof(u));
}

void uart_update(uint64_t now)
{
   while(pend_count && !fifo_full() && now >= u.rx_next) {
      u.fifo[u.in_ptr] = pending[pend_head];
      u.in_ptr = (u.in_ptr + 1) & FIFO_MASK;
      pend_head = (pend_head + 1) % sizeof(pending);
      pend_count--;

      // keep to the line rate, but don't build up credit while idle
      if(now - u.rx_next > RX_BYTE_CYCLES) u.rx_next = now;
      u.rx_next += RX_BYTE_CYCLES;
   }
}

uint64_t uart_next_event(void)
{
   return (pend_count && !fifo_full()) ? u.rx_next : NEVER;
}

uint32_t uart_read(uint32_t addr)
{
   if(addr == DEV_UART_DATA) {
      // the hardware pops even when empty
      u.data_out = u.fifo[u.out_ptr];
      u.out_ptr = (u.out_ptr + 1) & FIFO_MASK;
      return u.data_out;
   }

   unsigned avail = fifo_avail();
   return (avail << 16) |
          (fifo_full() << 2) |
          ((emu_now < u.tx_busy_until) << 1) |
          (avail != 0);
}

static void flush_tx(void)
{
   unsigned done = 0;
   while(done < tx_count) {
      ssize_t n = write(out_fd, txbuf + done, tx_count - done);
      if(n <= 0) break;
      done += n;
   }
   if(done && done < tx_count)
      memmove(txbuf, txbuf + done, tx_count - done);
   tx_count -= done;
}

void uart_write(uint32_t addr, uint32_t wdata, unsigned wmask)
{
   (void)addr;
   if(!(wmask & 1)) return;

   u.tx_busy_until = emu_now + TX_BUSY_CYCLES;
   if(out_fd < 0) return;

   if(tx_count == sizeof(txbuf))
      flush_tx();
   if(tx_count < sizeof(txbuf))      // otherwise nobody is listening: drop
      txbuf[tx_count++] = wdata;
}

//--------------------------------------------------------------------------
// Called between time slices: exchange data with the host.
void uart_poll_host(void)
{
   if(tx_count)
      flush_tx();

   if(in_fd < 0) return;

   // only take as much as the FIFO could hold, leave the rest in the pty
   unsigned space = sizeof(pending) - pend_count;
   unsigned room = FIFO_SIZE - 1 - fifo_avail();
   if(space > room) space = room;
   if(space == 0) return;

   uint8_t buf[sizeof(pending)];
   ssize_t n = read(in_fd, buf, space);
   if(n <= 0) {
      if(n == 0 && stdio_mode) in_fd = -1;   // EOF on stdin: no more input
      return;
   }

   for(ssize_t i = 0; i < n; i++) {
      if(stdio_mode && isatty(in_fd) && buf[i] == ESCAPE_CHAR) {
         uart_quit_requested = true;
         return;
      }
      pending[(pend_head + pend_count) % sizeof(pending)] = buf[i];
      pend_count++;
   }
   devices_update(cycles);
}

int uart_host_fd(void)
{
   return in_fd;
}

//--------------------------------------------------------------------------
int uart_open_pty(const char *link)
{
   int master = posix_openpt(O_RDWR | O_NOCTTY);
   if(master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
      emu_log("uart: cannot create pseudo terminal: %s", strerror(errno));
      return -1;
   }

   const char *name = ptsname(master);

   // Keep the slave side open ourselves, so data written while no terminal
   // program is attached is kept, and the master doesn't see hangups when
   // one disconnects. Start it in raw mode so nothing gets translated.
   slave_fd = open(name, O_RDWR | O_NOCTTY);
   if(slave_fd >= 0) {
      struct termios t;
      if(tcgetattr(slave_fd, &t) == 0) {
         cfmakeraw(&t);
         cfsetispeed(&t, B115200);
         cfsetospeed(&t, B115200);
         tcsetattr(slave_fd, TCSANOW, &t);
      }
   }

   fcntl(master, F_SETFL, fcntl(master, F_GETFL) | O_NONBLOCK);
   in_fd = out_fd = master;

   emu_log("uart: serial port is %s", name);
   if(link) {
      unlink(link);
      if(symlink(name, link) < 0)
         emu_log("uart: cannot create symlink %s: %s", link, strerror(errno));
      else {
         link_path = strdup(link);
         emu_log("uart: linked to %s", link);
      }
   }
   return 0;
}

int uart_open_stdio(void)
{
   stdio_mode = true;
   in_fd = STDIN_FILENO;
   out_fd = STDOUT_FILENO;

   if(isatty(in_fd) && tcgetattr(in_fd, &saved_tios) == 0) {
      struct termios t = saved_tios;
      cfmakeraw(&t);
      tcsetattr(in_fd, TCSANOW, &t);
      tios_saved = true;
      emu_log("uart: console on this terminal, press ctrl-] to quit");
   }
   fcntl(in_fd, F_SETFL, fcntl(in_fd, F_GETFL) | O_NONBLOCK);
   return 0;
}

void uart_close(void)
{
   if(out_fd >= 0 && tx_count) {
      // give a final chance for the output to drain
      fcntl(out_fd, F_SETFL, fcntl(out_fd, F_GETFL) & ~O_NONBLOCK);
      if(stdio_mode) flush_tx();
   }
   if(stdio_mode) {
      fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
      if(tios_saved) tcsetattr(STDIN_FILENO, TCSANOW, &saved_tios);
   } else {
      if(in_fd >= 0) close(in_fd);
      if(slave_fd >= 0) close(slave_fd);
   }
   if(link_path) {
      unlink(link_path);
      free(link_path);
   }
   in_fd = out_fd = slave_fd = -1;
}
