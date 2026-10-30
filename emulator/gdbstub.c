/*
 * FileStick emulator - GDB remote serial protocol server
 *
 * Lets riscv-none-elf-gdb connect with "target remote :PORT" to inspect
 * and control the emulated CPU: registers (both banks and the CSRs),
 * memory, breakpoints, watchpoints, single step and ctrl-C.
 *
 * While gdb has the CPU stopped, emulated time stops too: devices don't
 * advance and no interrupts arrive.
 *
 * Breakpoints are held here rather than written into memory as ebreak
 * instructions, so they work anywhere (including the boot ROM, and in
 * code that hasn't been loaded yet) and the software never sees them.
 *
 * Watchpoints stop the CPU before the load or store that would hit them
 * executes, as RISC-V debug triggers do; gdb then steps over the access
 * itself.
 *
 * The register layout follows gdb's RISC-V numbering: x0-x31 are 0-31
 * (the current register bank), pc is 32, CSR n is 65 + n. Extra registers
 * after those give the register bank select, the privilege state and all
 * 64 registers of both banks.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "emu.h"

#define PACKET_SIZE     0x4000
#define MAX_WATCH       16

#define REG_PC          32
#define REG_CSR0        65
#define REG_PRIV        (REG_CSR0 + 4096)    // privilege state, cpu_get_mode()
#define REG_BANKS       (REG_PRIV + 1)       // bank0 x0 - bank1 x31
#define REG_END         (REG_BANKS + 64)

#define SIGINT_GDB      2
#define SIGTRAP_GDB     5

bool gdb_quit_requested;

static int listen_fd = -1;
static int client_fd = -1;
static bool halted;
static bool no_ack;
static bool mask_isr_step = true;

// packet receive state
static enum { RX_IDLE, RX_DATA, RX_CSUM1, RX_CSUM2 } rx_state;
static char rx_buf[PACKET_SIZE + 1];
static unsigned rx_len;

// stop reason, sent in reply to "?" too
static char stop_reply[64] = "S05";

// one bit per halfword of the 24 bit address space
static uint8_t bp_map[(ADDR_MASK + 1) / 16];
static unsigned nbreak;

static struct watch {
   uint32_t addr, len;
   int type;                // 2 write, 3 read, 4 access, as in Z2-Z4
} watches[MAX_WATCH];
static unsigned nwatch;

static const char *const reg_names[32] = {
   "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
   "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
   "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
   "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

static const struct { const char *name; unsigned csr; } csrs[] = {
   { "sscratch", 0x140 }, { "stvec", 0x105 }, { "sepc", 0x141 },
   { "scause", 0x142 }, { "stval", 0x143 },
   { "mstatus", 0x300 }, { "mtvec", 0x305 }, { "mscratch", 0x340 },
   { "mepc", 0x341 }, { "mcause", 0x342 },
   { "regbank", 0x5C0 },
   { "cycle", 0xC00 }, { "cycleh", 0xC80 },
};

static char *target_xml;

//--------------------------------------------------------------------------
static bool bp_test(uint32_t pc)
{
   pc = (pc & ADDR_MASK) >> 1;
   return bp_map[pc >> 3] & (1 << (pc & 7));
}

static void bp_set(uint32_t pc, bool on)
{
   pc = (pc & ADDR_MASK) >> 1;
   bool was = bp_map[pc >> 3] & (1 << (pc & 7));
   if(on && !was) {
      bp_map[pc >> 3] |= 1 << (pc & 7);
      nbreak++;
   } else if(!on && was) {
      bp_map[pc >> 3] &= ~(1 << (pc & 7));
      nbreak--;
   }
}

static void clear_all_points(void)
{
   memset(bp_map, 0, sizeof(bp_map));
   nbreak = 0;
   nwatch = 0;
}

//--------------------------------------------------------------------------
static void build_target_xml(void)
{
   size_t size = 16384, len = 0;
   target_xml = malloc(size);

#define ADD(...) (len += snprintf(target_xml + len, size - len, __VA_ARGS__))
   ADD("<?xml version=\"1.0\"?>\n"
       "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n"
       "<target version=\"1.0\">\n"
       "<architecture>riscv:rv32</architecture>\n"
       "<feature name=\"org.gnu.gdb.riscv.cpu\">\n");
   for(int i = 0; i < 32; i++)
      ADD("<reg name=\"%s\" bitsize=\"32\" type=\"%s\" regnum=\"%d\"/>\n",
          reg_names[i], i == 1 ? "code_ptr" : (i == 2 || i == 8) ? "data_ptr" : "int", i);
   ADD("<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\" regnum=\"%d\"/>\n"
       "</feature>\n"
       "<feature name=\"org.gnu.gdb.riscv.csr\">\n", REG_PC);
   for(size_t i = 0; i < sizeof(csrs) / sizeof(csrs[0]); i++)
      if(csrs[i].csr != 0x5C0)
         ADD("<reg name=\"%s\" bitsize=\"32\" type=\"int\" regnum=\"%u\" "
             "save-restore=\"no\" group=\"csr\"/>\n",
             csrs[i].name, REG_CSR0 + csrs[i].csr);
   ADD("</feature>\n"
       "<feature name=\"org.filestick.emulator\">\n"
       "<reg name=\"regbank\" bitsize=\"32\" type=\"int\" regnum=\"%u\" "
       "save-restore=\"no\" group=\"csr\"/>\n"
       "<reg name=\"privmode\" bitsize=\"32\" type=\"int\" regnum=\"%u\" "
       "save-restore=\"no\" group=\"csr\"/>\n",
       REG_CSR0 + 0x5C0, REG_PRIV);
   for(int i = 0; i < 64; i++)
      ADD("<reg name=\"bank%d_%s\" bitsize=\"32\" type=\"int\" regnum=\"%d\" "
          "save-restore=\"no\" group=\"bank%d\"/>\n",
          i / 32, reg_names[i % 32], REG_BANKS + i, i / 32);
   ADD("</feature>\n</target>\n");
#undef ADD
}

//--------------------------------------------------------------------------
// Registers, in gdb's numbering
static bool reg_get(unsigned n, uint32_t *v)
{
   if(n < 32)
      *v = cpu_get_reg(cpu_bank() * 32 + n);
   else if(n == REG_PC)
      *v = cpu_get_pc();
   else if(n == REG_PRIV)
      *v = cpu_get_mode();
   else if(n >= REG_BANKS && n < REG_END)
      *v = cpu_get_reg(n - REG_BANKS);
   else if(n >= REG_CSR0 && n < REG_CSR0 + 4096)
      return cpu_get_csr(n - REG_CSR0, v);
   else
      return false;
   return true;
}

static bool reg_set(unsigned n, uint32_t v)
{
   if(n < 32)
      cpu_set_reg(cpu_bank() * 32 + n, v);
   else if(n == REG_PC)
      cpu_set_pc(v);
   else if(n == REG_PRIV)
      cpu_set_mode(v);
   else if(n >= REG_BANKS && n < REG_END)
      cpu_set_reg(n - REG_BANKS, v);
   else if(n >= REG_CSR0 && n < REG_CSR0 + 4096)
      return cpu_set_csr(n - REG_CSR0, v);
   else
      return false;
   return true;
}

//--------------------------------------------------------------------------
// Memory. RAM is accessed directly; anything else goes through the bus,
// so device registers read as the software would see them (including any
// side effects, such as popping the UART FIFO).
static bool is_ram(uint32_t addr)
{
   return addr < SPRAM_SIZE ||
          (addr >= BLKRAM_BASE && addr < BLKRAM_BASE + BLKRAM_SIZE);
}

static uint8_t *ram_ptr(uint32_t addr)
{
   return addr < SPRAM_SIZE ? &spram[addr] : &blkram[addr - BLKRAM_BASE];
}

static uint8_t mem_read8(uint32_t addr, uint32_t *word_addr, uint32_t *word)
{
   addr &= ADDR_MASK;
   if(is_ram(addr))
      return *ram_ptr(addr);
   if(*word_addr != (addr & ~3u)) {
      *word_addr = addr & ~3u;
      emu_now = cycles;
      bus_wait = 1;
      *word = bus_read(*word_addr);
   }
   return *word >> (8 * (addr & 3));
}

static void mem_write8(uint32_t addr, uint8_t v)
{
   addr &= ADDR_MASK;
   if(is_ram(addr)) {
      *ram_ptr(addr) = v;
      return;
   }
   emu_now = cycles;
   bus_wait = 1;
   bus_write(addr & ~3u, (uint32_t)v << (8 * (addr & 3)), 1u << (addr & 3));
}

//--------------------------------------------------------------------------
// Packet I/O
static void send_raw(const char *buf, size_t len)
{
   while(len && client_fd >= 0) {
      ssize_t n = send(client_fd, buf, len, MSG_NOSIGNAL);
      if(n < 0) {
         if(errno == EINTR) continue;
         emu_log("gdb: send: %s", strerror(errno));
         return;
      }
      buf += n;
      len -= n;
   }
}

static void put_packet_len(const char *data, size_t len)
{
   static char out[PACKET_SIZE * 2 + 8];
   unsigned char sum = 0;
   size_t o = 0;

   out[o++] = '$';
   for(size_t i = 0; i < len && o < sizeof(out) - 4; i++) {
      out[o++] = data[i];
      sum += (unsigned char)data[i];
   }
   o += snprintf(out + o, sizeof(out) - o, "#%02x", sum);
   send_raw(out, o);
}

static void put_packet(const char *data)
{
   put_packet_len(data, strlen(data));
}

static const char hexdigits[] = "0123456789abcdef";

static int hexval(int c)
{
   if(c >= '0' && c <= '9') return c - '0';
   if(c >= 'a' && c <= 'f') return c - 'a' + 10;
   if(c >= 'A' && c <= 'F') return c - 'A' + 10;
   return -1;
}

static char *put_hex32le(char *p, uint32_t v)
{
   for(int i = 0; i < 4; i++, v >>= 8) {
      *p++ = hexdigits[(v >> 4) & 15];
      *p++ = hexdigits[v & 15];
   }
   return p;
}

static bool get_hex32le(const char **pp, uint32_t *v)
{
   const char *p = *pp;
   *v = 0;
   for(int i = 0; i < 4; i++) {
      int hi = hexval(p[0]), lo = hexval(p[1]);
      if(hi < 0 || lo < 0) return false;
      *v |= (uint32_t)(hi << 4 | lo) << (8 * i);
      p += 2;
   }
   *pp = p;
   return true;
}

// Text to hex, for console output and monitor command replies
static void put_console(const char *text)
{
   char buf[PACKET_SIZE];
   size_t o = 0;
   buf[o++] = 'O';
   for(; *text && o < sizeof(buf) - 3; text++) {
      buf[o++] = hexdigits[(unsigned char)*text >> 4];
      buf[o++] = hexdigits[*text & 15];
   }
   put_packet_len(buf, o);
}

//--------------------------------------------------------------------------
static void stop(int sig, const char *extra)
{
   halted = true;
   cpu_irq_mask = false;
   snprintf(stop_reply, sizeof(stop_reply), "T%02x%s", sig, extra ? extra : "");
   put_packet(stop_reply);
}

// Single step one instruction (with interrupts held off unless the user
// asked for them).
static void step(void)
{
   cpu_irq_mask = mask_isr_step;
   cpu_step();
   cpu_irq_mask = false;
   stop(SIGTRAP_GDB, NULL);
}

static void resume(void)
{
   halted = false;
}

// Would the next instruction trigger a watchpoint? If so stop before it.
static bool watch_check(void)
{
   uint32_t addr;
   unsigned size;
   bool store;

   if(!cpu_next_access(&addr, &size, &store))
      return false;

   for(unsigned i = 0; i < nwatch; i++) {
      struct watch *w = &watches[i];
      if(w->type == 2 && !store) continue;
      if(w->type == 3 && store) continue;
      if(addr >= w->addr + w->len || w->addr >= addr + size) continue;

      char extra[48];
      uint32_t hit = addr > w->addr ? addr : w->addr;
      snprintf(extra, sizeof(extra), "%s:%x;",
               w->type == 2 ? "watch" : w->type == 3 ? "rwatch" : "awatch", hit);
      stop(SIGTRAP_GDB, extra);
      return true;
   }
   return false;
}

void gdb_run(uint64_t until)
{
   if(!nbreak && !nwatch) {
      cpu_run(until);
      return;
   }

   while(cycles < until) {
      if(nbreak && bp_test(cpu_get_pc())) {
         stop(SIGTRAP_GDB, "swbreak:;");
         return;
      }
      if(nwatch && watch_check())
         return;
      cpu_step();
   }
}

//--------------------------------------------------------------------------
// Commands
static void monitor(const char *cmd)
{
   char line[256];

   if(!strcmp(cmd, "reset")) {
      emu_reset();
      put_console("CPU and devices reset, pc = 0x020000\n");
   } else if(!strncmp(cmd, "maskisr", 7)) {
      const char *arg = cmd + 7;
      while(*arg == ' ') arg++;
      if(!strcmp(arg, "on")) mask_isr_step = true;
      else if(!strcmp(arg, "off")) mask_isr_step = false;
      else if(*arg) {
         put_packet("E01");
         return;
      }
      snprintf(line, sizeof(line), "Interrupts while single stepping: %s\n",
               mask_isr_step ? "masked" : "allowed");
      put_console(line);
   } else if(!strcmp(cmd, "cycles")) {
      snprintf(line, sizeof(line), "%llu cycles (%.6fs)\n",
               (unsigned long long)cycles, (double)cycles / CPU_HZ);
      put_console(line);
   } else if(!strcmp(cmd, "quit")) {
      gdb_quit_requested = true;
   } else {
      put_console("FileStick emulator monitor commands:\n"
                  "  reset             reset the CPU and devices\n"
                  "  maskisr [on|off]  mask interrupts while single stepping (default on)\n"
                  "  cycles            show the cycle count\n"
                  "  quit              exit the emulator\n");
   }
   put_packet("OK");
}

static void handle_query(char *p)
{
   if(!strncmp(p, "qSupported", 10)) {
      char buf[128];
      snprintf(buf, sizeof(buf), "PacketSize=%x;qXfer:features:read+;"
               "QStartNoAckMode+;swbreak+;hwbreak+;vContSupported+", PACKET_SIZE);
      put_packet(buf);
   } else if(!strncmp(p, "qXfer:features:read:target.xml:", 31)) {
      unsigned long off = strtoul(p + 31, &p, 16);
      unsigned long len = (*p == ',') ? strtoul(p + 1, NULL, 16) : 0;
      size_t total = strlen(target_xml);
      if(len > PACKET_SIZE - 1) len = PACKET_SIZE - 1;
      if(off >= total) {
         put_packet("l");
         return;
      }
      static char buf[PACKET_SIZE];
      size_t n = total - off < len ? total - off : len;
      buf[0] = (off + n >= total) ? 'l' : 'm';
      memcpy(buf + 1, target_xml + off, n);
      put_packet_len(buf, n + 1);
   } else if(!strncmp(p, "qXfer:", 6)) {
      put_packet("E00");
   } else if(!strcmp(p, "qAttached")) {
      put_packet("1");       // gdb detaches rather than kills on quit
   } else if(!strcmp(p, "qC")) {
      put_packet("QC1");
   } else if(!strcmp(p, "qfThreadInfo")) {
      put_packet("m1");
   } else if(!strcmp(p, "qsThreadInfo")) {
      put_packet("l");
   } else if(!strncmp(p, "qRcmd,", 6)) {
      char cmd[256];
      size_t n = 0;
      for(p += 6; p[0] && p[1] && n < sizeof(cmd) - 1; p += 2)
         cmd[n++] = hexval(p[0]) << 4 | hexval(p[1]);
      cmd[n] = 0;
      monitor(cmd);
   } else if(!strcmp(p, "QStartNoAckMode")) {
      put_packet("OK");
      no_ack = true;
   } else {
      put_packet("");
   }
}

// Z/z: breakpoints and watchpoints
static void handle_point(char *p)
{
   bool insert = p[0] == 'Z';
   int type = p[1] - '0';
   char *e;
   uint32_t addr = strtoul(p + 3, &e, 16) & ADDR_MASK;
   uint32_t len = (*e == ',') ? strtoul(e + 1, NULL, 16) : 1;

   if(p[2] != ',' || type < 0 || type > 4) {
      put_packet("");
      return;
   }

   if(type <= 1) {
      bp_set(addr, insert);
   } else if(insert) {
      if(nwatch == MAX_WATCH) {
         put_packet("E01");
         return;
      }
      watches[nwatch++] = (struct watch){ addr, len ? len : 1, type };
   } else {
      for(unsigned i = 0; i < nwatch; i++)
         if(watches[i].addr == addr && watches[i].len == (len ? len : 1) &&
            watches[i].type == type) {
            watches[i] = watches[--nwatch];
            break;
         }
   }
   put_packet("OK");
}

static void handle_vcont(char *p)
{
   if(!strcmp(p, "vCont?")) {
      put_packet("vCont;c;C;s;S");
      return;
   }
   // Only one thread: the first action decides
   p += 5;
   if(*p == ';') p++;
   if(*p == 's' || *p == 'S')
      step();
   else if(*p == 'c' || *p == 'C')
      resume();
   else
      put_packet("E01");
}

static void handle_packet(char *p, size_t len)
{
   char buf[PACKET_SIZE];
   char *o = buf;
   uint32_t v;

   switch(p[0]) {
      case '?':
         put_packet(stop_reply);
         break;

      case 'g':
         for(unsigned i = 0; i <= REG_PC; i++) {
            reg_get(i, &v);
            o = put_hex32le(o, v);
         }
         put_packet_len(buf, o - buf);
         break;

      case 'G': {
         const char *q = p + 1;
         for(unsigned i = 0; i <= REG_PC && get_hex32le(&q, &v); i++)
            reg_set(i, v);
         put_packet("OK");
         break;
      }

      case 'p':
         if(reg_get(strtoul(p + 1, NULL, 16), &v)) {
            o = put_hex32le(o, v);
            put_packet_len(buf, o - buf);
         } else
            put_packet("E01");
         break;

      case 'P': {
         char *e;
         unsigned n = strtoul(p + 1, &e, 16);
         const char *q = e + 1;
         if(*e == '=' && get_hex32le(&q, &v) && reg_set(n, v))
            put_packet("OK");
         else
            put_packet("E01");
         break;
      }

      case 'm': {
         char *e;
         uint32_t addr = strtoul(p + 1, &e, 16);
         uint32_t n = (*e == ',') ? strtoul(e + 1, NULL, 16) : 0;
         uint32_t word_addr = 1, word = 0;
         if(n > PACKET_SIZE / 2) n = PACKET_SIZE / 2;
         for(uint32_t i = 0; i < n; i++) {
            uint8_t b = mem_read8(addr + i, &word_addr, &word);
            *o++ = hexdigits[b >> 4];
            *o++ = hexdigits[b & 15];
         }
         put_packet_len(buf, o - buf);
         break;
      }

      case 'M': {
         char *e;
         uint32_t addr = strtoul(p + 1, &e, 16);
         uint32_t n = (*e == ',') ? strtoul(e + 1, &e, 16) : 0;
         if(*e++ != ':') {
            put_packet("E01");
            break;
         }
         for(uint32_t i = 0; i < n && e[0] && e[1]; i++, e += 2)
            mem_write8(addr + i, hexval(e[0]) << 4 | hexval(e[1]));
         put_packet("OK");
         break;
      }

      case 'X': {
         // binary data, with '}' escapes
         char *e;
         uint32_t addr = strtoul(p + 1, &e, 16);
         uint32_t n = (*e == ',') ? strtoul(e + 1, &e, 16) : 0;
         if(*e++ != ':') {
            put_packet("E01");
            break;
         }
         char *end = p + len;
         for(uint32_t i = 0; i < n && e < end; i++) {
            uint8_t b = *e++;
            if(b == '}' && e < end) b = *e++ ^ 0x20;
            mem_write8(addr + i, b);
         }
         put_packet("OK");
         break;
      }

      case 'c':
      case 'C':
         if(p[0] == 'c' && p[1])
            cpu_set_pc(strtoul(p + 1, NULL, 16));
         resume();
         break;

      case 's':
      case 'S':
         if(p[0] == 's' && p[1])
            cpu_set_pc(strtoul(p + 1, NULL, 16));
         step();
         break;

      case 'Z':
      case 'z':
         handle_point(p);
         break;

      case 'H':
      case 'T':
         put_packet("OK");
         break;

      case 'q':
      case 'Q':
         handle_query(p);
         break;

      case 'v':
         if(!strncmp(p, "vCont", 5))
            handle_vcont(p);
         else
            put_packet("");
         break;

      case 'D':
         put_packet("OK");
         clear_all_points();
         resume();
         break;

      case 'k':
         gdb_quit_requested = true;
         break;

      default:
         put_packet("");
   }
}

//--------------------------------------------------------------------------
// Connection handling
static void disconnect(void)
{
   emu_log("gdb: disconnected");
   close(client_fd);
   client_fd = -1;
   clear_all_points();
   cpu_irq_mask = false;
   resume();
}

static void accept_client(void)
{
   int fd = accept(listen_fd, NULL, NULL);
   if(fd < 0) return;
   if(client_fd >= 0) {
      close(fd);           // one debugger at a time
      return;
   }

   int one = 1;
   setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
   client_fd = fd;
   no_ack = false;
   rx_state = RX_IDLE;
   halted = true;
   strcpy(stop_reply, "S05");
   emu_log("gdb: connected, CPU stopped at %06x", cpu_get_pc());
}

static void rx_byte(char c)
{
   static unsigned char sum, rx_sum;

   switch(rx_state) {
      case RX_IDLE:
         if(c == '$') {
            rx_state = RX_DATA;
            rx_len = 0;
            sum = 0;
         } else if(c == 0x03 && !halted) {
            stop(SIGINT_GDB, NULL);
         }
         // '+' and '-' acks are ignored
         break;

      case RX_DATA:
         if(c == '#')
            rx_state = RX_CSUM1;
         else {
            if(rx_len < PACKET_SIZE) rx_buf[rx_len++] = c;
            sum += (unsigned char)c;
         }
         break;

      case RX_CSUM1:
         rx_sum = hexval(c) << 4;
         rx_state = RX_CSUM2;
         break;

      case RX_CSUM2:
         rx_sum |= hexval(c);
         rx_state = RX_IDLE;
         if(!no_ack) {
            if(rx_sum != sum) {
               send_raw("-", 1);
               break;
            }
            send_raw("+", 1);
         }
         rx_buf[rx_len] = 0;
         handle_packet(rx_buf, rx_len);
         break;
   }
}

void gdb_poll(int timeout_ms)
{
   if(listen_fd < 0) return;

   struct pollfd fds[2] = {
      { .fd = listen_fd, .events = POLLIN },
      { .fd = client_fd, .events = POLLIN },
   };
   if(poll(fds, client_fd >= 0 ? 2 : 1, timeout_ms) <= 0)
      return;

   if(fds[0].revents & POLLIN)
      accept_client();

   if(client_fd >= 0 && fds[1].fd == client_fd && fds[1].revents) {
      char buf[4096];
      ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
      if(n <= 0) {
         if(n == 0 || errno != EINTR)
            disconnect();
         return;
      }
      for(ssize_t i = 0; i < n && client_fd >= 0; i++)
         rx_byte(buf[i]);
   }
}

bool gdb_enabled(void)
{
   return listen_fd >= 0;
}

bool gdb_halted(void)
{
   return halted;
}

// spec is PORT or HOST:PORT. Without a host only local connections are
// accepted. If wait is set, the CPU stays stopped at reset until gdb
// connects and continues it.
int gdb_listen(const char *spec, bool wait)
{
   char host[256] = "127.0.0.1";
   const char *port = spec;
   const char *colon = strrchr(spec, ':');
   if(colon) {
      size_t n = colon - spec;
      if(n >= sizeof(host)) n = sizeof(host) - 1;
      memcpy(host, spec, n);
      host[n] = 0;
      port = colon + 1;
      if(!*host) strcpy(host, "127.0.0.1");
   }

   struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM,
                             .ai_flags = AI_PASSIVE };
   struct addrinfo *res;
   int err = getaddrinfo(host, port, &hints, &res);
   if(err) {
      emu_log("gdb: %s: %s", spec, gai_strerror(err));
      return -1;
   }

   for(struct addrinfo *ai = res; ai; ai = ai->ai_next) {
      int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if(fd < 0) continue;
      int one = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
      if(bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && listen(fd, 1) == 0) {
         listen_fd = fd;
         break;
      }
      close(fd);
   }
   freeaddrinfo(res);

   if(listen_fd < 0) {
      emu_log("gdb: can't listen on %s: %s", spec, strerror(errno));
      return -1;
   }

   build_target_xml();
   halted = wait;
   emu_log("gdb: listening on %s:%s%s", host, port,
           wait ? ", waiting for connection" : "");
   return 0;
}
