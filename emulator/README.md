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
| `-g [HOST:]PORT` | Listen for gdb on PORT, see below. The CPU waits at the reset address until gdb connects and continues it. Only local connections are accepted unless HOST is given. |
| `--gdb-nowait` | With `-g`, start running immediately; gdb can attach at any time. |
| `-e FILE` | Connect the Econet interface to the emulated network described by FILE, see below. |
| `--econet-station [NET.]STN` | With `-e`, be this station from startup instead of waiting for the software to set the station number. |
| `--econet-bitrate N` | Econet line speed in bits/s used for frame timing (default 250000). |
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

## Econet

Econet is emulated the way BeebEm does it, so the FileStick can share a
network with BeebEm, b-em (`sf/econet` branches, with `aunmode` off) and
aund (with `beebem` in `aund.conf`). Every Econet frame (scout, ack, data,
immediate) goes out as one UDP datagram holding the frame from the
destination address onwards, without the FCS. The system software does
the four way handshake frame by frame, just as it does on a real network.

`-e` takes the same `econet.cfg` file as BeebEm, b-em and aund, which maps
each station to an IP address and UDP port:

```
# net stn ip port
0 254 127.0.0.1 10254
0 101 127.0.0.1 10101
```

b-em's option lines (`aunmode`, `learn` and so on) are accepted but
ignored. AUN mode is not supported, so you get a warning if `aunmode` is on.

* The emulator becomes whichever station the software programs into the
  receiver (`configure netstation`, or the fileserver's station 254). It
  binds the UDP port listed for that station. Until then it can send but
  not receive. `--econet-station` binds a station at startup instead.
* Econet is a broadcast medium, so (as aund does) each frame goes to every
  station in the file except this one. The receiver filters on the
  destination address as the hardware does, so monitor mode sees all
  traffic, including frames between other stations.
* Frames take time on the emulated line at `--econet-bitrate`. Receiving
  shows in the status register, the transmitter is busy while it sends,
  received frames wait while we transmit, and the transmitter waits 15
  bit times of idle line unless turnaround is set. Received frames land
  in the circular buffer with their FCS, as on the hardware.
* There is no real line, so enabling the clock output or the termination,
  or changing the clock divider, does nothing. Each change is logged, and
  the hardware control register reads back what was written.

`-v` logs every frame sent and received.

## Debugging with gdb

Start the emulator with `-g PORT`, then connect `riscv-none-elf-gdb`:

```
$ ./filestick-emu -b ../build/boot/filestick-boot.bin -f flash.img -g 3333 ...
$ riscv-none-elf-gdb ../build/system/filestick-system.elf
(gdb) target remote :3333
(gdb) break disk_initialize
(gdb) continue
```

All the usual commands work: breakpoints, `step`/`next`/`stepi`,
`finish`, backtraces, reading and writing registers and memory,
`watch`/`rwatch`/`awatch`, ctrl-C to stop the running system, and `load`
to put an ELF file straight into RAM (it sets the pc to the entry point).
Build with `-g` for source level debugging; the normal builds have no
debug information.

* While gdb has the CPU stopped, emulated time stops too: timers don't
  advance and no interrupts arrive. Input typed at the UART is held until
  the CPU runs again.
* Breakpoints are kept by the emulator rather than written into memory,
  so there is no limit on how many, and they can be set anywhere,
  including in the boot ROM or in a program that hasn't been loaded yet.
  `break` and `hbreak` behave the same.
* Watchpoints (up to 16) stop the CPU before the load or store that
  touches the watched memory, as RISC-V hardware triggers do. Watching a
  device register works, e.g. `rwatch *(int *)0x80000c` stops on every
  UART read.
* Interrupts are masked while single stepping, so `stepi` doesn't land
  in the interrupt handler. A pending interrupt is taken on the next
  `continue`. `monitor maskisr off` changes this.
* Memory reads by gdb outside RAM go to the devices, with the same side
  effects a load would have (reading 0x80000C pops the UART FIFO).
* `x0`-`x31` are the registers of the current bank. `info registers
  bank0` and `info registers bank1` show both banks, which can also be
  read and written as `$bank0_a0`, `$bank1_sp` and so on. `info registers
  csr` shows the CSRs, `$regbank` (CSR 0x5C0) and `$privmode` (bit 0 S
  mode, bit 1 M mode, bit 2 privilege violation trap pending).
* `monitor reset` resets the CPU and devices (memory, flash and SD card
  contents are kept). `monitor cycles` shows the cycle count. `monitor
  quit` and gdb's `kill` exit the emulator. Quitting gdb or `detach`
  leaves the system running, and another gdb can attach later.

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
* **Econet**: receiver, transmitter and hardware control registers and
  buffers, networked over UDP like BeebEm (see above). Without `-e`,
  transmitted frames take their time on the line and are then dropped,
  and nothing is received.

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
| `econet.c` | Econet interface and BeebEm style UDP network |
| `gdbstub.c` | gdb remote protocol server |
