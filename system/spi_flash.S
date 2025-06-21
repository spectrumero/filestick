#
#The MIT License
#
#Copyright (c) 2023 Dylan Smith
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.
#

#include "devices.h"

.text

.globl flash_reset
flash_reset:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    s1,  8(sp)

   li    t0, DEV_BASE

   sb    zero, OFFS_SPI_REG_SS(t0)        # select SPI slave select 0
   li    a0, 0xFFFFFFFF                   # send some 1s
   call  flash_word
   sb    zero, OFFS_SPI_REG_ACTIVE(t0)    # reset ss

   sb    zero, OFFS_SPI_REG_ENDIAN(t0)    # set little endian so the low byte gets written out
   li    s1, 10                           # Send 10 sequences
.rst_loop:
   call  flash_byte
   addi  s1, s1, -1
   bnez  s1, .rst_loop

   sb    zero, OFFS_SPI_REG_ACTIVE(t0)    # reset flash_ss

   li    a0, 0xAB                         # release power down
   call  flash_byte
   li    a0, 0xFFFFFFFF
   call  flash_word                       # send dummy bytes

   sb    zero, OFFS_SPI_REG_ACTIVE(t0)    # reset flash_ss
   
   lw    s1, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

# Writes and reads from flash. Data in in a0, out in a0.
.globl flash_byte
flash_byte:
   li    t0, DEV_BASE
   sb    zero, OFFS_SPI_REG_BITCOUNT(t0)    # set SPI to write 1 byte
   sb    a0, OFFS_SPI_DAT(t0)               # write to SPI
   lb    a0, OFFS_SPI_DAT(t0)               # read back value shifted in
   ret

.globl flash_word
flash_word:
   li    t0, DEV_BASE
   li    t1, 3
   sb    t1, OFFS_SPI_REG_BITCOUNT(t0)      # set SPI to write 4 bytes
   sw    a0, OFFS_SPI_DAT(t0)
   lw    a0, OFFS_SPI_DAT(t0)
   ret

# void flash_memcpy(void *src, void *dst, uint32_t size);
# Copies data from flash to memory.
# Size must be 24 bits or smaller.
# a0 = flash source
# a1 = mem dest
# a2 = size
.globl flash_memcpy
flash_memcpy:
   li    t2, DEV_BASE
   andi  t3, a2, 0x3                      # remainder after div by 4

   sb    zero, OFFS_SPI_REG_SS(t2)        # set SPI SS
   srli  a2, a2, 2                        # ensure size is in whole words
   li    t0, 1
   sb    t0, OFFS_SPI_REG_ENDIAN(t2)      # set endianness
   li    t0, 3
   sb    t0, OFFS_SPI_REG_BITCOUNT(t2)    # set length to words
   li    t0, 0x03000000                   # flash read in MSB
   or    t0, t0, a0                       # set the flash src address
   sw    t0, OFFS_SPI_DAT(t2)             # send request
   lw    t0, OFFS_SPI_DAT(t2)             # consume 4 bytes from flash
   li    t0, 0xFFFFFFFF                   # write 1s while reading flash
   sb    zero, OFFS_SPI_REG_ENDIAN(t2)    # set endianness for transfer
.copy_loop:
   sw    t0, OFFS_SPI_DAT(t2)             # move 32 bits of data
   lw    t1, OFFS_SPI_DAT(t2)
   sw    t1, 0(a1)                        # result -> memory
   addi  a1, a1, 4                        # next word
   addi  a2, a2, -1                       # dec word count
   bnez  a2, .copy_loop                   # continue if there's stuff left to do
   beqz  t3, .copy_done                   # no remainder to deal with?

   sb    zero, OFFS_SPI_REG_BITCOUNT(t2)  # byte at a time
   li    t1, 1
   sb    t1, OFFS_SPI_REG_ENDIAN(t2)      # set endianness
.remainder_loop:
   sw    t0, OFFS_SPI_DAT(t2)
   lb    t1, OFFS_SPI_DAT(t2)
   sb    t1, 0(a1)
   addi  a1, a1, 1
   addi  t3, t3, -1                       # remainder
   bnez  t3, .remainder_loop
   
.copy_done:
   sb    zero, OFFS_SPI_REG_ACTIVE(t2)    # reset spi_ss    
   ret

