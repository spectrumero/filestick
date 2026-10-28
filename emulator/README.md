# FileStick emulator

An emulator for the FileStick hardware: the modified FemtoRV32 core, memory
map and peripherals in `rtl/`, running at the real system's 10MHz.

## Building

```
$ cd emulator
$ make
```

It needs only a C compiler and Linux (it uses a pseudo terminal for the UART).

## Running

```
$ ./filestick-emu -b ../build/boot/filestick-boot.bin -f flash.img \
      -l 0x20000:../build/system/filestick-system.bin \
      -l 0x30000:../build/init/init.elf \
      -s sdcard.img -u /tmp/filestick
uart: serial port is /dev/pts/5
uart: linked to /tmp/filestick
```

Then connect a terminal program to the pty, e.g. `picocom /tmp/filestick`
or `screen /tmp/filestick`. Use `--stdio` to have the console on the
emulator's own terminal instead (ctrl-] quits).

| Option | |
|---|---|
| `-b FILE` | Boot ROM loaded into block RAM at 0x20000. A raw binary, or a `$readmemh` hex file (e.g. `rtl/filestick-boot.hex`) if the name ends in `.hex`. |
| `-f FILE` | SPI flash image. Created if it doesn't exist, and padded to 1MB with 0xFF if shorter. Writes by the emulated system go to this file. |
| `-l OFFS:FILE` | Write FILE into the flash image at OFFS before starting, as a flash programmer would. Can be repeated. This modifies the image file. |
| `-s FILE` | SD card image. Without it, no card is inserted. |
| `-u PATH` | Create a symlink to the UART's pty at PATH. |
| `--stdio` | Use stdin/stdout as the UART. |
| `-t` | Turbo: run as fast as the host allows instead of at 10MHz. |
| `-c N` | Stop after N cycles, then print the CPU registers. |
| `-v` | Log device activity (LED, flash erase/program, SD commands). |
| `--trace` | Log every instruction executed, with the register it wrote. |

`kill -USR1` the emulator to dump the CPU registers while it runs.

The flash layout the boot loader and system expect is:

| Offset | Contents |
|---|---|
| 0x00000 | FPGA bitstream (not used by the emulator) |
| 0x20000 | `filestick-system.bin`, 64K copied to address 0 by the boot loader |
| 0x30000 | `init.elf`, loaded by the system |
| 0x40000 | FAT filesystem, 256K |

The boot loader always copies a full 64K, so the system's `.bss` starts out
holding whatever the flash contains after `filestick-system.bin`. On erased
flash that is 0xFF, not zero, and FatFS then can't mount the flash
filesystem. Padding the image with zeros avoids this:
`truncate -s 64K system.bin` before loading it.

## What is emulated

* **CPU**: RV32IMC as implemented by `femtorv32_gracilis.v`, including its
  quirks: 24 bit PC and addresses, the second register bank (CSR 0x5C0 and
  the 0xDC0-0xDFF register peek CSRs), the ecall/ebreak/illegal instruction
  traps through stvec/sepc/scause, interrupts through mtvec/mepc/mcause,
  and memory protection (loads and stores below 0x10000 need S or M mode;
  the trap is taken one instruction late, like the hardware). Cycle counts
  follow the core's state machine, including the one word fetch cache,
  divide timing and SPI wait states.
* **Memory**: 128K SPRAM at 0, 7K block RAM at 0x20000.
* **UART**: the 512 byte receive FIFO, status register and transmit busy
  timing at 115200 baud, connected to a Linux pty.
* **SPI controller**: all registers, byte order and transfer sizes, slave
  select handling and CPU stalls.
* **Serial flash** (CS 0): AT25SF081 compatible, read, fast read, IDs,
  status, write enable, page program, 4K/32K/64K/chip erase, power down.
  Erase and program take typical device times (the busy bit stays set).
* **SD card** (CS 1): SDHC card in SPI mode with single and multiple block
  read and write, plus the card detect register and its interrupt.
* **Timers**: the general purpose timer and the econet "timer A".
* **RGB LED**: changes are logged with `-v`.
* **Econet**: a stub. The registers and buffers exist, but no frames are
  received and transmitted frames are dropped.

## Source files

| File | |
|---|---|
| `main.c` | options, setup, and the 1ms time slice loop |
| `cpu.c` | the CPU core |
| `bus.c` | memory map, RAM, boot ROM loading, device event scheduling |
| `timer.c` | the GP timer and econet timer A |
| `uart.c` | UART and the pty |
| `spi.c` | SPI controller |
| `flash.c` | SPI flash |
| `sdcard.c` | SD card and card detect |
| `econet.c` | econet stub |
