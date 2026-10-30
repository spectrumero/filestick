/*
 * FileStick emulator - shared definitions.
 *
 * The emulated system is a modified FemtoRV32 "Gracilis" core (RV32IMC)
 * running at 10MHz with 128K of SPRAM, 7K of block RAM holding the boot
 * loader, and a handful of memory mapped peripherals. See rtl/toplevel.v
 * for the hardware this models.
 */
#ifndef EMU_H
#define EMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define CPU_HZ             10000000ULL
#define ADDR_MASK          0xFFFFFF       // the core has a 24 bit address bus
#define NEVER              UINT64_MAX

// Memory map
#define SPRAM_BASE         0x000000
#define SPRAM_SIZE         0x20000        // 128K
#define BLKRAM_BASE        0x020000
#define BLKRAM_SIZE        (14 * 512)     // 14 up5k block RAMs of 512 bytes
#define RESET_ADDR         0x020000

#define DEV_BASE           0x800000
#define DEV_RGBLED         0x800000
#define DEV_TIMER_SET      0x800004
#define DEV_TIMER_CTL      0x800008
#define DEV_UART_DATA      0x80000C
#define DEV_UART_STATE     0x800010
#define DEV_SD_STATUS      0x800014

// Cycle counter. This is the number of 10MHz clocks since reset.
extern uint64_t cycles;

// The cycle on which the instruction currently being executed is in the
// EXECUTE state, i.e. when its memory access happens. Devices use this
// as "now" when they are accessed.
extern uint64_t emu_now;

// Number of WAIT_ALU_OR_MEM cycles the current load/store needs. Set to 1
// by the CPU before each access; slow devices (SPI) increase it.
extern unsigned bus_wait;

extern int verbose;

// Logging to stderr. In stdio console mode stderr may be the terminal in
// raw mode, so these use \r\n.
void emu_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define VLOG(...) do { if(verbose) emu_log(__VA_ARGS__); } while(0)

// cpu.c
void cpu_reset(void);
void cpu_step(void);
void cpu_run(uint64_t until);
void cpu_dump(void);
extern bool cpu_trace;
extern bool cpu_irq_mask;          // don't take interrupts (while stepping)

uint32_t cpu_get_pc(void);
void cpu_set_pc(uint32_t pc);
unsigned cpu_bank(void);
uint32_t cpu_get_reg(unsigned n);
void cpu_set_reg(unsigned n, uint32_t v);
uint32_t cpu_get_mode(void);
void cpu_set_mode(uint32_t v);
bool cpu_get_csr(unsigned csr, uint32_t *v);
bool cpu_set_csr(unsigned csr, uint32_t v);
bool cpu_next_access(uint32_t *addr, unsigned *size, bool *store);

// gdbstub.c - GDB remote protocol server
int gdb_listen(const char *spec, bool wait);
bool gdb_enabled(void);
bool gdb_halted(void);
void gdb_run(uint64_t until);
void gdb_poll(int timeout_ms);
extern bool gdb_quit_requested;

// main.c
void emu_reset(void);

// bus.c
extern uint8_t spram[SPRAM_SIZE];
extern uint8_t blkram[BLKRAM_SIZE];
uint32_t bus_read(uint32_t addr);
void bus_write(uint32_t addr, uint32_t wdata, unsigned wmask);
uint16_t bus_fetch16(uint32_t addr);
int bus_load_boot(const char *path);

// Device event scheduling. Devices that change state on their own as time
// passes (timers, UART receive) report when they next need attention, and
// the CPU loop calls devices_update() once that cycle has been reached.
// devices_update() also recomputes the interrupt line.
extern uint64_t next_event;
extern bool irq_line;
void devices_update(uint64_t now);

// timer.c - general purpose timer and the econet "timer A"
void timer_reset(void);
void timer_update(uint64_t now);
uint64_t timer_next_event(void);
bool timer_irq(void);
uint32_t timer_read(uint32_t addr);
void timer_write(uint32_t addr, uint32_t wdata, unsigned wmask);

void timera_reset(void);
uint64_t timera_next_event(void);
bool timera_irq(uint64_t now);
uint32_t timera_read(uint32_t addr);
void timera_write(uint32_t addr, uint32_t wdata, unsigned wmask);

// uart.c
int uart_open_pty(const char *link);
int uart_open_stdio(void);
void uart_close(void);
void uart_reset(void);
void uart_update(uint64_t now);
uint64_t uart_next_event(void);
uint32_t uart_read(uint32_t addr);
void uart_write(uint32_t addr, uint32_t wdata, unsigned wmask);
void uart_poll_host(void);
int uart_host_fd(void);
extern bool uart_quit_requested;

// spi.c - SPI controller. CS 0 = flash, CS 1 = SD card.
void spi_reset(void);
uint32_t spi_read(uint32_t addr);
void spi_write(uint32_t addr, uint32_t wdata, unsigned wmask);

// flash.c - AT25SF081 compatible serial flash
int flash_open(const char *path);
int flash_load(uint32_t offset, const char *path);
void flash_close(void);
void flash_select(bool selected);
uint8_t flash_xfer(uint8_t out);

// sdcard.c - SD card in SPI mode, plus the card detect switch
int sd_open(const char *path);
void sd_close(void);
void sd_select(bool selected);
uint8_t sd_xfer(uint8_t out);
void sd_detect_reset(void);
bool sd_detect_irq(void);
uint32_t sd_detect_read(void);
void sd_detect_write(uint32_t wdata, unsigned wmask);

// econet.c - stub
void econet_reset(void);
bool econet_irq(void);
uint32_t econet_read(uint32_t addr);
void econet_write(uint32_t addr, uint32_t wdata, unsigned wmask);

#endif
