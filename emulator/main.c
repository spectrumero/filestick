/*
 * FileStick emulator - main program
 *
 * Runs the emulated system in 1ms time slices (10000 cycles at 10MHz),
 * sleeping after each one to keep emulated time in step with real time.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "emu.h"

#define SLICE_CYCLES    (CPU_HZ / 1000)            // 1ms
#define MAX_LAG_NS      50000000LL                 // give up catching up after 50ms

uint64_t cycles;
uint64_t emu_now;
unsigned bus_wait;
int verbose;

static volatile sig_atomic_t quit_signal;
static volatile sig_atomic_t dump_signal;

void emu_log(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fputs("\r\n", stderr);
}

static void on_signal(int sig)
{
   if(sig == SIGUSR1)
      dump_signal = 1;
   else
      quit_signal = 1;
}

static int64_t now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_until(int64_t t)
{
   struct timespec ts = { .tv_sec = t / 1000000000LL, .tv_nsec = t % 1000000000LL };
   while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR)
      if(quit_signal) break;
}

// Power on reset of the CPU and devices. Memory and the flash and SD card
// contents are left alone.
void emu_reset(void)
{
   cpu_reset();
   timer_reset();
   timera_reset();
   uart_reset();
   spi_reset();
   sd_detect_reset();
   econet_reset();
   devices_update(cycles);
}

static void usage(const char *prog)
{
   fprintf(stderr,
      "Usage: %s -b BOOT -f FLASH [options]\n"
      "\n"
      "  -b, --boot FILE        boot ROM loaded into block RAM (.bin, or .hex in\n"
      "                         $readmemh format)\n"
      "  -f, --flash FILE       SPI flash image (created/padded to 1MB with 0xFF)\n"
      "  -s, --sd FILE          SD card image (no card inserted if not given)\n"
      "  -l, --load OFFS:FILE   write FILE into the flash image at OFFS before\n"
      "                         starting (can be repeated), e.g.\n"
      "                         -l 0x20000:filestick-system.bin -l 0x30000:init.elf\n"
      "  -u, --uart-link PATH   make a symlink at PATH to the UART's pty\n"
      "      --stdio            use this terminal as the UART instead of a pty\n"
      "                         (ctrl-] quits)\n"
      "  -g, --gdb [HOST:]PORT  listen for gdb on PORT (localhost unless HOST is\n"
      "                         given), with the CPU stopped at reset until gdb\n"
      "                         connects and continues it\n"
      "      --gdb-nowait       with --gdb, start running straight away; gdb\n"
      "                         can attach at any time\n"
      "  -t, --turbo            run as fast as possible, not at 10MHz\n"
      "  -c, --cycles N         stop after N cycles\n"
      "  -v, --verbose          log device activity to stderr\n"
      "      --trace            trace every instruction to stderr\n"
      "  -h, --help             this help\n"
      "\n"
      "Send SIGUSR1 to dump the CPU registers.\n",
      prog);
}

int main(int argc, char **argv)
{
   static const struct option opts[] = {
      { "boot",      required_argument, NULL, 'b' },
      { "flash",     required_argument, NULL, 'f' },
      { "sd",        required_argument, NULL, 's' },
      { "load",      required_argument, NULL, 'l' },
      { "uart-link", required_argument, NULL, 'u' },
      { "stdio",     no_argument,       NULL, 'S' },
      { "gdb",       required_argument, NULL, 'g' },
      { "gdb-nowait", no_argument,      NULL, 'N' },
      { "turbo",     no_argument,       NULL, 't' },
      { "cycles",    required_argument, NULL, 'c' },
      { "verbose",   no_argument,       NULL, 'v' },
      { "trace",     no_argument,       NULL, 'T' },
      { "help",      no_argument,       NULL, 'h' },
      { NULL, 0, NULL, 0 }
   };

   const char *boot_path = NULL, *flash_path = NULL, *sd_path = NULL;
   const char *uart_link = NULL, *gdb_spec = NULL;
   bool gdb_wait = true;
   char **loads = calloc(argc, sizeof(char *));
   int nloads = 0;
   bool use_stdio = false, turbo = false;
   uint64_t max_cycles = 0;
   int c;

   while((c = getopt_long(argc, argv, "b:f:s:l:u:g:tc:vh", opts, NULL)) != -1) {
      switch(c) {
         case 'b': boot_path = optarg; break;
         case 'f': flash_path = optarg; break;
         case 's': sd_path = optarg; break;
         case 'l': loads[nloads++] = optarg; break;
         case 'u': uart_link = optarg; break;
         case 'S': use_stdio = true; break;
         case 'g': gdb_spec = optarg; break;
         case 'N': gdb_wait = false; break;
         case 't': turbo = true; break;
         case 'c': max_cycles = strtoull(optarg, NULL, 0); break;
         case 'v': verbose++; break;
         case 'T': cpu_trace = true; break;
         case 'h': usage(argv[0]); return 0;
         default:  usage(argv[0]); return 1;
      }
   }

   if(!boot_path || !flash_path || optind != argc) {
      usage(argv[0]);
      return 1;
   }

   if(bus_load_boot(boot_path) < 0) return 1;
   if(flash_open(flash_path) < 0) return 1;
   for(int i = 0; i < nloads; i++) {
      char *sep = strchr(loads[i], ':');
      if(!sep) {
         emu_log("--load needs OFFSET:FILE, got %s", loads[i]);
         return 1;
      }
      *sep = 0;
      if(flash_load(strtoul(loads[i], NULL, 0), sep + 1) < 0) return 1;
   }
   free(loads);
   if(sd_path && sd_open(sd_path) < 0) return 1;

   if((use_stdio ? uart_open_stdio() : uart_open_pty(uart_link)) < 0)
      return 1;

   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = on_signal;
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
   sigaction(SIGHUP, &sa, NULL);
   sigaction(SIGUSR1, &sa, NULL);

   emu_reset();
   if(gdb_spec && gdb_listen(gdb_spec, gdb_wait) < 0) {
      uart_close();
      return 1;
   }

   int64_t start_ns = now_ns();
   int64_t base_ns = start_ns;
   uint64_t base_cycles = 0;

   while(!quit_signal && !uart_quit_requested && !gdb_quit_requested) {
      if(gdb_halted()) {
         // stopped in the debugger: emulated time stands still
         gdb_poll(100);
         uart_poll_host();
         base_ns = now_ns();
         base_cycles = cycles;
         continue;
      }

      uint64_t target = cycles + SLICE_CYCLES;
      if(max_cycles && target > max_cycles) target = max_cycles;

      if(gdb_enabled()) {
         gdb_run(target);
         gdb_poll(0);
      } else
         cpu_run(target);
      uart_poll_host();

      if(dump_signal) {
         dump_signal = 0;
         cpu_dump();
      }
      if(max_cycles && cycles >= max_cycles) break;
      if(turbo) continue;

      int64_t due = base_ns + (int64_t)((cycles - base_cycles) * (1000000000ULL / CPU_HZ));
      int64_t now = now_ns();
      if(due > now)
         sleep_until(due);
      else if(now - due > MAX_LAG_NS) {
         // the host can't keep up (or we were stopped): don't try to catch up
         base_ns = now;
         base_cycles = cycles;
      }
   }

   uart_poll_host();
   double secs = (now_ns() - start_ns) / 1e9;
   uart_close();
   flash_close();
   sd_close();

   if(verbose || max_cycles)
      emu_log("emulator: %" PRIu64 " cycles (%.3fs emulated) in %.3fs",
              cycles, (double)cycles / CPU_HZ, secs);
   if(verbose > 1 || max_cycles)
      cpu_dump();
   return 0;
}
