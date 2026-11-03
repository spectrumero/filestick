/*
 * FileStick emulator - Econet interface
 *
 * The register interface follows the RTL:
 *
 *   0x800100-0x8001FF  receive registers (econet_rx_buffered.v)
 *   0x810000-0x81FFFF  receive buffer (2K circular, read only from the CPU)
 *   0x800200-0x8002FF  transmit registers (econet_tx_buffered.v)
 *   0x820000-0x82FFFF  transmit buffer (2K, write only from the CPU)
 *   0x800320           hardware control (econet_hwctl.v)
 *
 * Econet timer A at 0x800300 is a general purpose timer, see timer.c.
 *
 * The network itself is simulated the way BeebEm does it, and as b-em
 * (in its non-AUN mode) and aund's "beebem" mode interoperate with: each
 * Econet frame (scout, ack, data, immediate) is sent as one UDP datagram
 * holding the frame's bytes from the destination address on, without the
 * FCS. The four way handshake is not translated; the system software
 * does it, frame by frame, just as it does on a real network.
 *
 * The stations on the network and their UDP addresses come from a
 * BeebEm style econet.cfg:
 *
 *    # comment
 *    net stn ip-address udp-port
 *    option value
 *
 * As aund does, a transmitted frame is sent to every station in the file
 * except ourselves, since Econet is a broadcast medium. The receiver then
 * filters on the destination address exactly as the hardware does, which
 * means monitor mode sees all traffic.
 *
 * The emulated station's UDP socket is bound to the address listed for the
 * station number the software programs into the receiver, so whatever
 * the FileStick is configured as is the station it appears as. Until it
 * sets one (or if it isn't in the file) frames can still be sent from an
 * unbound socket but nothing can be received, unless --econet-station
 * gave a station to start as.
 *
 * Frames take emulated time on the wire at a fixed bit rate: the line
 * shows as receiving while a frame comes in, the transmitter is busy while
 * one goes out, received frames are held off while we transmit (and vice
 * versa), and the transmitter waits 15 bit times after the line goes idle
 * unless the turnaround bit is set.
 *
 * There is no real Econet clock or line, so the clock output and the
 * termination enable bits in the hardware control register have no
 * effect. Changing them is logged, and they read back as written.
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "emu.h"

#define BUF_BYTES       2048
#define BUF_WORDS       (BUF_BYTES / 4)
#define PTR_MASK        (BUF_BYTES - 1)

#define MAX_DATAGRAM    65536
#define RXQ_SIZE        32       // datagrams held waiting for the line

// receive register numbers
#define RX_REG_START       0
#define RX_REG_END         1
#define RX_REG_COUNT       2
#define RX_REG_ADDRESS     3
#define RX_REG_REPLY       4
#define RX_REG_SCOUT       5
#define RX_REG_OUR_ADDRESS 6
#define RX_REG_STATUS      7

// transmit register numbers
#define TX_REG_START       0
#define TX_REG_END         1
#define TX_REG_CONTROL     2

// hardware control register bits
#define HW_CLKEN           0x001
#define HW_TERMEN          0x002
#define HW_CLKDIV          0x700

//--------------------------------------------------------------------------
// Network configuration (econet.cfg)

struct station {
   uint8_t            net, stn;
   struct sockaddr_in sa;
};

static struct station *stations;
static int nstations;
static const char *cfg_path;
static unsigned bitrate = ECONET_DEFAULT_BITRATE;
static int sock = -1;
static struct station *bound;    // the entry our socket is bound to

// Frames that have arrived from the network and not yet been put on the
// emulated line.
struct rxframe {
   uint8_t *data;
   unsigned len;
};
static struct rxframe rxq[RXQ_SIZE];
static unsigned rxq_head, rxq_count;

//--------------------------------------------------------------------------
// Hardware state

static uint32_t rx_buf[BUF_WORDS];
static uint32_t tx_buf[BUF_WORDS];

// receiver
static uint16_t our_address;
static bool     monitor_mode;
static bool     frame_valid;            // the interrupt
static unsigned rx_ptr;                 // econet_ptr: next byte to write
static uint8_t  frame_address[6];       // first 6 bytes of the last frame
static unsigned valid_start, valid_end, valid_cnt;
static uint32_t valid_address;
static uint16_t valid_scout;

static bool     receiving;
static uint64_t rx_done;                // when the frame on the line ends
static uint64_t line_idle_since;
static struct rxframe rx_cur;

// transmitter
static unsigned tx_start, tx_end;
static bool     turnaround;
static bool     tx_requested;
static bool     transmitting;
static uint64_t tx_done;
static uint8_t  tx_frame[BUF_BYTES];
static unsigned tx_len;

static unsigned hwctl = 7 << 8;         // clock divider resets to 7

//--------------------------------------------------------------------------
static uint64_t frame_cycles(unsigned len)
{
   // opening flag, the bytes, 2 FCS bytes, closing flag (bit stuffing is
   // not accounted for)
   return (uint64_t)(len + 4) * 8 * CPU_HZ / bitrate;
}

static uint64_t bit_cycles(unsigned bits)
{
   return (uint64_t)bits * CPU_HZ / bitrate;
}

static const char *sa_str(const struct sockaddr_in *sa)
{
   static char s[32];
   snprintf(s, sizeof(s), "%s:%u", inet_ntoa(sa->sin_addr), ntohs(sa->sin_port));
   return s;
}

static void dump_frame(const char *what, const uint8_t *p, unsigned len)
{
   if(!verbose) return;
   char hex[3 * 16 + 4];
   unsigned n = len < 16 ? len : 16, o = 0;
   for(unsigned i = 0; i < n; i++)
      o += snprintf(hex + o, sizeof(hex) - o, " %02X", p[i]);
   if(n < len) snprintf(hex + o, sizeof(hex) - o, " ...");
   if(len >= 4)
      emu_log("econet: %s %u bytes, %u.%u -> %u.%u:%s",
              what, len, p[3], p[2], p[1], p[0], hex);
   else
      emu_log("econet: %s %u bytes:%s", what, len, hex);
}

//--------------------------------------------------------------------------
// econet.cfg parsing. The format is BeebEm's, as also read by b-em and
// aund: "net stn ip port" lines, comments starting with '#', and b-em's
// option lines ("name value").
static int read_config(const char *path)
{
   FILE *f = fopen(path, "r");
   if(!f) {
      emu_log("econet: %s: %s", path, strerror(errno));
      return -1;
   }

   char line[512];
   unsigned lineno = 0;
   while(fgets(line, sizeof(line), f)) {
      lineno++;
      char *p = line;
      while(isspace((unsigned char)*p)) p++;
      if(*p == '#' || !*p) continue;

      if(!isdigit((unsigned char)*p)) {
         // option line
         char name[64];
         long value = 0;
         if(sscanf(p, "%63s %ld", name, &value) < 1) continue;
         for(char *c = name; *c; c++) *c = tolower((unsigned char)*c);
         if(!strcmp(name, "aunmode")) {
            if(value)
               emu_log("econet: %s:%u: AUN mode is not supported, using "
                       "BeebEm encapsulation", path, lineno);
         } else if(!strcmp(name, "learn") || !strcmp(name, "aunstrict") ||
                   !strcmp(name, "flagfilltimeout") ||
                   !strcmp(name, "scacktimeout") ||
                   !strcmp(name, "fourwaytimeout") ||
                   !strcmp(name, "massagenets")) {
            VLOG("econet: %s:%u: option %s ignored (only used by b-em/BeebEm)",
                 path, lineno, name);
         } else
            emu_log("econet: %s:%u: unrecognised option %s", path, lineno, name);
         continue;
      }

      char *end;
      long net = strtol(p, &end, 10);
      if(end == p) goto bad;
      p = end;
      long stn = strtol(p, &end, 10);
      if(end == p) goto bad;
      p = end;
      while(isspace((unsigned char)*p)) p++;
      char *ip = p;
      while(*p && !isspace((unsigned char)*p)) p++;
      if(!*p) goto bad;
      *p++ = 0;
      long port = strtol(p, &end, 10);
      if(end == p || port <= 0 || port > 65535 || net < 0 || net > 255 ||
         stn < 0 || stn > 255)
         goto bad;

      struct in_addr addr;
      if(!inet_aton(ip, &addr)) {
         emu_log("econet: %s:%u: invalid IP address '%s'", path, lineno, ip);
         continue;
      }

      bool dup = false;
      for(int i = 0; i < nstations; i++)
         if(stations[i].net == net && stations[i].stn == stn) dup = true;
      if(dup) {
         emu_log("econet: %s:%u: station %ld.%ld listed twice, ignored",
                 path, lineno, net, stn);
         continue;
      }

      struct station *s = realloc(stations, (nstations + 1) * sizeof(*s));
      if(!s) {
         fclose(f);
         emu_log("econet: out of memory");
         return -1;
      }
      stations = s;
      s = &stations[nstations++];
      memset(s, 0, sizeof(*s));
      s->net = net;
      s->stn = stn;
      s->sa.sin_family = AF_INET;
      s->sa.sin_addr = addr;
      s->sa.sin_port = htons(port);
      VLOG("econet: station %ld.%ld is %s", net, stn, sa_str(&s->sa));
      continue;

bad:
      emu_log("econet: %s:%u: malformed line, expected 'net stn ip port'",
              path, lineno);
   }
   fclose(f);

   if(nstations == 0)
      emu_log("econet: %s: no stations listed", path);
   return 0;
}

static struct station *find_station(unsigned net, unsigned stn)
{
   for(int i = 0; i < nstations; i++)
      if(stations[i].net == net && stations[i].stn == stn)
         return &stations[i];
   return NULL;
}

// (Re)create the UDP socket, bound to the entry for st if it's not NULL.
static void open_socket(struct station *st)
{
   if(sock >= 0) close(sock);
   bound = NULL;

   sock = socket(AF_INET, SOCK_DGRAM, 0);
   if(sock < 0) {
      emu_log("econet: socket: %s", strerror(errno));
      return;
   }
   int one = 1;
   setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
   fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

   if(!st) return;
   if(bind(sock, (struct sockaddr *)&st->sa, sizeof(st->sa)) < 0) {
      emu_log("econet: can't bind station %u.%u to %s: %s",
              st->net, st->stn, sa_str(&st->sa), strerror(errno));
      return;
   }
   bound = st;
   emu_log("econet: station %u.%u listening on %s", st->net, st->stn,
           sa_str(&st->sa));
}

// The software has set our station address: become that station on the
// emulated network.
static void address_changed(void)
{
   if(!cfg_path) return;
   unsigned net = our_address >> 8, stn = our_address & 0xFF;
   if(bound && bound->net == net && bound->stn == stn) return;
   if(stn == 0) return;

   struct station *st = find_station(net, stn);
   if(!st) {
      emu_log("econet: station %u.%u is not in %s; frames will be sent but "
              "none can be received", net, stn, cfg_path);
      if(bound) open_socket(NULL);
      return;
   }
   open_socket(st);
}

int econet_open(const char *path, const char *station, unsigned rate)
{
   if(rate) bitrate = rate;
   if(read_config(path) < 0) return -1;
   cfg_path = path;

   struct station *st = NULL;
   if(station) {
      unsigned net = 0, stn;
      char *end;
      stn = strtoul(station, &end, 10);
      if(*end == '.') {
         net = stn;
         stn = strtoul(end + 1, &end, 10);
      }
      if(*end || net > 255 || stn > 255 || stn == 0) {
         emu_log("econet: bad station number '%s', expected [NET.]STN", station);
         return -1;
      }
      st = find_station(net, stn);
      if(!st) {
         emu_log("econet: station %u.%u is not in %s", net, stn, path);
         return -1;
      }
   }
   open_socket(st);
   if(sock < 0) return -1;
   if(station && !bound) return -1;
   return 0;
}

void econet_close(void)
{
   if(sock >= 0) close(sock);
   sock = -1;
   bound = NULL;
}

//--------------------------------------------------------------------------
// Network side

static void send_frame(const uint8_t *p, unsigned len)
{
   dump_frame("sent", p, len);
   if(sock < 0) {
      VLOG("econet: no network configured, frame dropped");
      return;
   }

   // a broadcast medium: everyone but us hears it
   for(int i = 0; i < nstations; i++) {
      struct station *st = &stations[i];
      if(st == bound) continue;
      if(sendto(sock, p, len, 0, (struct sockaddr *)&st->sa, sizeof(st->sa)) < 0 &&
         errno != ECONNREFUSED && errno != EAGAIN)
         VLOG("econet: sending to %u.%u (%s): %s", st->net, st->stn,
              sa_str(&st->sa), strerror(errno));
   }
}

void econet_poll_host(void)
{
   if(sock < 0) return;

   bool got = false;
   while(rxq_count < RXQ_SIZE) {
      static uint8_t buf[MAX_DATAGRAM];
      struct sockaddr_in from;
      socklen_t fromlen = sizeof(from);
      ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&from, &fromlen);
      if(n < 0) {
         if(errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNREFUSED &&
            errno != EINTR)
            VLOG("econet: recvfrom: %s", strerror(errno));
         break;
      }
      if(n == 0) continue;
      if(!bound) continue;    // not a station on the network yet

      uint8_t *data = malloc(n);
      if(!data) break;
      memcpy(data, buf, n);
      struct rxframe *f = &rxq[(rxq_head + rxq_count++) % RXQ_SIZE];
      f->data = data;
      f->len = n;
      got = true;
   }

   if(got) devices_update(cycles);
}

//--------------------------------------------------------------------------
// Emulated line

static uint16_t fcs16(const uint8_t *p, unsigned len)
{
   uint16_t crc = 0xFFFF;
   while(len--) {
      crc ^= *p++;
      for(int i = 0; i < 8; i++)
         crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
   }
   return ~crc;
}

static void rx_store(uint8_t b)
{
   unsigned w = rx_ptr >> 2, sh = 8 * (rx_ptr & 3);
   rx_buf[w] = (rx_buf[w] & ~(0xFFu << sh)) | ((uint32_t)b << sh);
   rx_ptr = (rx_ptr + 1) & PTR_MASK;
}

// The frame on the line has been completely received: it goes into the
// buffer as the receiver would have written it, FCS included.
static void rx_complete(void)
{
   unsigned len = rx_cur.len;
   unsigned start = rx_ptr;

   // the bytes as the receiver sees them: the frame, then its FCS
   uint16_t fcs = fcs16(rx_cur.data, len);
   for(unsigned i = 0; i < len + 2; i++) {
      uint8_t b = i < len ? rx_cur.data[i] : i == len ? (fcs & 0xFF) : fcs >> 8;
      if(i < 6) frame_address[i] = b;     // short frames leave the rest stale
      rx_store(b);
   }

   bool ours = frame_address[0] == (our_address & 0xFF) &&
               frame_address[1] == (our_address >> 8);

   if(ours || monitor_mode) {
      valid_start = start;
      valid_end = rx_ptr;
      valid_cnt = (len + 2) & PTR_MASK;
      valid_address = frame_address[0] | frame_address[1] << 8 |
                      frame_address[2] << 16 | (uint32_t)frame_address[3] << 24;
      valid_scout = frame_address[4] << 8 | frame_address[5];
      frame_valid = true;
      dump_frame(ours ? "received" : "monitored", rx_cur.data, len);
   } else
      dump_frame("ignored (not for us)", rx_cur.data, len);

   free(rx_cur.data);
   rx_cur.data = NULL;
   receiving = false;
}

static void start_tx(uint64_t now)
{
   tx_len = ((tx_end - tx_start) & PTR_MASK) + 1;
   for(unsigned i = 0; i < tx_len; i++) {
      unsigned a = (tx_start + i) & PTR_MASK;
      tx_frame[i] = tx_buf[a >> 2] >> (8 * (a & 3));
   }
   tx_requested = false;
   transmitting = true;
   tx_done = now + frame_cycles(tx_len);
}

void econet_update(uint64_t now)
{
   bool progress = true;
   while(progress) {
      progress = false;

      if(receiving && now >= rx_done) {
         line_idle_since = rx_done;
         rx_complete();
         progress = true;
      }

      if(transmitting && now >= tx_done) {
         transmitting = false;
         line_idle_since = tx_done;
         send_frame(tx_frame, tx_len);
         progress = true;
      }

      if(tx_requested && !transmitting) {
         // 15 bit times of idle line first, unless this is a reply
         if(!receiving && (turnaround || now >= line_idle_since + bit_cycles(15))) {
            start_tx(now);
            progress = true;
         }
      }

      // the receiver is inhibited while we transmit; frames from the
      // network wait for the line
      if(!receiving && !transmitting && !tx_requested && rxq_count) {
         rx_cur = rxq[rxq_head];
         rxq_head = (rxq_head + 1) % RXQ_SIZE;
         rxq_count--;
         receiving = true;
         rx_done = now + frame_cycles(rx_cur.len);
         progress = true;
      }
   }
}

uint64_t econet_next_event(void)
{
   uint64_t n = NEVER;
   if(receiving && rx_done < n) n = rx_done;
   if(transmitting && tx_done < n) n = tx_done;
   if(tx_requested && !transmitting && !receiving) {
      uint64_t t = line_idle_since + bit_cycles(15);
      if(t < n) n = t;
   }
   return n;
}

bool econet_irq(void)
{
   return frame_valid;
}

//--------------------------------------------------------------------------
void econet_reset(void)
{
   memset(rx_buf, 0, sizeof(rx_buf));
   memset(tx_buf, 0, sizeof(tx_buf));
   our_address = 0;
   monitor_mode = false;
   frame_valid = false;
   rx_ptr = 0;
   memset(frame_address, 0, sizeof(frame_address));
   valid_start = valid_end = valid_cnt = 0;
   valid_address = 0;
   valid_scout = 0;
   if(rx_cur.data) free(rx_cur.data);
   rx_cur.data = NULL;
   receiving = false;
   line_idle_since = 0;
   tx_start = tx_end = 0;
   turnaround = false;
   tx_requested = false;
   transmitting = false;
   hwctl = 7 << 8;
}

static void hwctl_write(unsigned v)
{
   unsigned changed = hwctl ^ v;
   hwctl = v;

   if(changed & HW_CLKEN)
      emu_log("econet: clock output %s (no effect: the emulated network "
              "needs no clock)", (v & HW_CLKEN) ? "enabled" : "disabled");
   if(changed & HW_TERMEN)
      emu_log("econet: termination %s (no effect: there is no line to "
              "terminate)", (v & HW_TERMEN) ? "enabled" : "disabled");
   if(changed & HW_CLKDIV) {
      unsigned div = (v & HW_CLKDIV) >> 8;
      emu_log("econet: clock divider set to %u (a %.1fkHz clock on the "
              "hardware; no effect)", div, CPU_HZ / (8000.0 * (1u << div)));
   }
}

uint32_t econet_read(uint32_t addr)
{
   unsigned word = (addr >> 2) & (BUF_WORDS - 1);

   if((addr >> 16) == 0x81)
      return rx_buf[word];
   if((addr >> 16) == 0x82)
      return 0xDEADBEEF;   // no read port on the transmit buffer

   if((addr >> 8) == 0x8001) {
      uint32_t a = valid_address;
      switch((addr >> 2) & 0xF) {
         case RX_REG_START:       return valid_start;
         case RX_REG_END:         return valid_end;
         case RX_REG_COUNT:       return valid_cnt;
         case RX_REG_ADDRESS:     return a;
         case RX_REG_REPLY:       return (a & 0xFF00) << 16 | (a & 0xFF) << 16 |
                                         (a >> 24) << 8 | ((a >> 16) & 0xFF);
         case RX_REG_SCOUT:       return valid_scout;
         case RX_REG_OUR_ADDRESS: return our_address;
         case RX_REG_STATUS:      return monitor_mode << 8 | receiving << 1 |
                                         frame_valid;
      }
      return 0x55555555;
   }

   if((addr >> 8) == 0x8002) {
      bool busy = tx_requested || transmitting;
      switch((addr >> 2) & 3) {
         case TX_REG_START:   return tx_start;
         case TX_REG_END:     return tx_end;
         case TX_REG_CONTROL: return transmitting << 2 | busy << 1 | turnaround;
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
      // any write to the low byte of a receive register acknowledges the
      // frame (clears the interrupt)
      if(wmask & 1) frame_valid = false;

      switch((addr >> 2) & 0xF) {
         case RX_REG_OUR_ADDRESS: {
            uint16_t old = our_address;
            if(wmask & 1) our_address = (our_address & 0xFF00) | (wdata & 0xFF);
            if(wmask & 2) our_address = (our_address & 0x00FF) | (wdata & 0xFF00);
            if(our_address != old) {
               VLOG("econet: our address is %u.%u", our_address >> 8,
                    our_address & 0xFF);
               address_changed();
            }
            break;
         }
         case RX_REG_STATUS:
            if(wmask & 2) {
               bool m = (wdata >> 8) & 1;
               if(m != monitor_mode)
                  VLOG("econet: monitor mode %s", m ? "on" : "off");
               monitor_mode = m;
            }
            break;
      }
      return;
   }

   if((addr >> 8) == 0x8002) {
      if(!wmask) return;
      switch((addr >> 2) & 3) {
         case TX_REG_START:   tx_start = wdata & PTR_MASK; break;
         case TX_REG_END:
            tx_end = wdata & PTR_MASK;
            // setting the end starts transmission, unless already busy
            if(!transmitting && !tx_requested)
               tx_requested = true;
            break;
         case TX_REG_CONTROL: turnaround = wdata & 1; break;
      }
      econet_update(emu_now);
      return;
   }

   if(addr == 0x800320) {
      unsigned v = hwctl;
      if(wmask & 1) v = (v & ~3u) | (wdata & 3);
      if(wmask & 2) v = (v & ~0x700u) | (wdata & 0x700);
      hwctl_write(v);
   }
}
