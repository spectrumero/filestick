# FileStick

This is still a very early work in progress.

This project implements an Econet fileserver "onna stick". Acorn had
the FileStore, so this is kind of a play on words on the FileStore.
The system is built on a Lattice UltraPlus 5K FPGA, which includes
128kbyte of static RAM on the chip, as well as a number of dual ported
blockrams, which makes it ideal for a small system project.

The prototype board is based on the UPduino 3.1 board.
See: https://tinyvision.ai/products/upduino-v3-1

The version 1 board contains the econet front end, the up5k FPGA,
an SD card slot, flash memory and a USB interface, implemented with
an FTDI USB UART.

![V1 board](https://raw.githubusercontent.com/spectrumero/filestick/refs/heads/main/filestick-scaled.jpg "V1 board")

The system consists of a RISC-V core (modified FemtoRV core supporting
some extra CSRs, the ecall instruction to handle system calls, ebreak
and some limited illegal instruction trapping). The core is an rv32imc
(RISC-V 32 bit with the multiply/divide and compressed instructions
extensions).

## Features

* USB powered, < 500mA
* Simple RISC-V implementation and early microcomputer style operating
system. System functions via `ecall` interface.
* Fully buffered econet implementation, with econet exposed via standard
open/read/write/close/ioctl operations.
* Programmable timers.
* USB serial interface with hardware FIFO and hardware CTS/RTS flow control.
Has an interactive mode for use as the system console and raw mode for
transferring files over USB etc.
* FAT support using FatFS for both microSD and built-in flash.
* RGB LED, useful for showing system activity.
* C / C++ programming made easy with newlib and GCC riscv cross compiler,
and a lightweight library and specs file for system-specifics.

## Project structure

* hardware: Contains the Verilog source code. Requires a recent version
of yosys, nextpnr, and the icestorm tools. (Debian users will need to
build these from source, as Debian is still on the old arachnepnr).

* system: Contains the operating system. This isn't a multiuser/
multitasking operating system, it's an OS in the vein of a classic
home micro's operating system. It does provide a number of system
calls using the Linux rv32 syscall numbers. Remember there is no
virtual memory on this very small system, however, some simple memory
protection is planned so that user programs can't accidentally
overwrite kernel memory or device address space.

* lib: The filestick library. This is required for most userspace programs
as at the very least it provides the sbrk implementation needed to properly
support libc's malloc.

## Toolchain

The xPack GNU RISC-V embedded GCC project is used to build all source
code. xPack provides ready-to-run toolchains for many platforms including
Linux, Windows and MacOS. Other GCC toolchains will probably work, but xPack
is likely the easiest/most convenient to use.

The xPack toolchain includes newlib (the libc).

See their releases here: 
https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases

The build system uses cmake. If you're running Linux, your distro almost
certainly has cmake. Otherwise see: https://cmake.org/

## TODO

There's much to do. More to come later, including how to write programs
for the FileStick.

