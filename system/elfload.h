#ifndef ELFLOAD_H
#define ELFLOAD_H
/*
;The MIT License
;
;Copyright (c) 2023 Dylan Smith
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;THE SOFTWARE.
*/
#include <stdint.h>

#define ELF_MAGIC       0x464c457f     // 0x7f,E,L,F
#define FLASH_OFFSET    0x30000        // Where in flash the startup is
#define USRMEM_START    0x10000
#define USRMEM_SIZE     0x10800

// Loads boot file from flash. Returns entry address or 0 on failure.
uint32_t elf_boot();

// Loads from a file descriptor. The offset is how many bytes into the
// open file the ELF data starts (normally 0). Returns the entry address
// or 0 on failure.
uint32_t elf_load(int fd, uint32_t offset);

#endif

