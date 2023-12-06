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
# Generic SPI routines.

#include "devices.h"

.text

// void spi_deassert_ss()
.globl spi_deassert_ss
spi_deassert_ss:
   li    t0, DEV_BASE
   sb    zero, OFFS_SPI_REG_ACTIVE(t0)
   ret

// void spi_set_slave(uint8_t slave)
.globl spi_set_slave
spi_set_slave:
   li    t0, DEV_BASE
   andi  a0, a0, 3         # ensure value is valid
   sb    a0, OFFS_SPI_REG_SS(t0)
   ret

.globl spi_write
// void spi_write(void *src, size_t size, bool deassert_ss_when_done);
// Writes size bytes from src to SPI. The src pointer should be 
// aligned on a 32-bit boundary for best performance.
spi_write:
   li    t0, DEV_BASE
   beqz  a1, .write_done         # make sure there's something to do

   sb    zero, OFFS_SPI_REG_ENDIAN(t0)
   andi  t2, a0, 3               # check for word alignment
   beqz  t2, .spi_write_cont1    # aligned
   li    t3, 4
   sub   t2, t3, t2              # bytes to transfer
   sub   a1, a1, t2              # subtract these bytes from remaining byte count
   li    t1, 1                   # set bytes per xfer = 1
   sb    zero, OFFS_SPI_REG_BITCOUNT(t0)
.wr_un_loop1:                    # t2 = how many unaligned bytes to write
   lb    t3, 0(a0)
   sb    t3, OFFS_SPI_DAT(t0)
   addi  a0, a0, 1               # increment address
   addi  t2, t2, -1              # decrement byte count
   bnez  t2, .wr_un_loop1
   beqz  a1, .write_done         # leave if all bytes written out

.spi_write_cont1:
   andi  t2, a1, 3               # get any unaligned remainder bytes
   sub   a1, a1, t2              # subtract them from the aligned write
   beqz  a1, .wr_unaligned_2     # any aligned bytes to write out?
   li    t1, 3                   # set 32 bit transfers
   sb    t1, OFFS_SPI_REG_BITCOUNT(t0)
.wr_aligned_loop:
   lw    t3, 0(a0)
   sw    t3, OFFS_SPI_DAT(t0)    # write 32 bits
   addi  a0, a0, 4               # next address
   addi  a1, a1, -4              # decrement count
   bnez  a1, .wr_aligned_loop
   beqz  t2, .write_done         # if no unaligned remainder, we're done

.wr_unaligned_2:
   li    t1, 1                   # set bytes per xfer = 1
   sb    zero, OFFS_SPI_REG_BITCOUNT(t0)
.wr_un_loop2:                    # t2 = how many unaligned bytes to write
   lb    t3, 0(a0)
   sb    t3, OFFS_SPI_DAT(t0)
   addi  a0, a0, 1               # increment address
   addi  t2, t2, -1              # decrement byte count
   bnez  t2, .wr_un_loop2

.write_done:
   beqz  a2, .write_exit         # Don't deassert slave select if a2=0
   sb    zero, OFFS_SPI_REG_ACTIVE(t0)

.write_exit:
   ret

.globl spi_read
// void spi_read(void *dst, size_t size, bool deassert_ss_when_done, uint32_t wr_word);
spi_read:
   li    t0, DEV_BASE
   beqz  a1, .read_done         # make sure there's something to do

   sb    zero, OFFS_SPI_REG_ENDIAN(t0)
   andi  t2, a0, 3               # check for word alignment
   beqz  t2, .spi_read_cont1    # aligned
   li    t3, 4
   sub   t2, t3, t2              # bytes to transfer
   sub   a1, a1, t2              # subtract these bytes from remaining byte count
   li    t1, 1                   # set bytes per xfer = 1
   sb    zero, OFFS_SPI_REG_BITCOUNT(t0)
.rd_un_loop1:                    # t2 = how many unaligned bytes to read
   sb    a3, OFFS_SPI_DAT(t0)    # write out pattern while reading
   lb    t3, OFFS_SPI_DAT+3(t0)
   sb    t3, 0(a0)               # result -> memory
   addi  a0, a0, 1               # increment address
   addi  t2, t2, -1              # decrement byte count
   bnez  t2, .rd_un_loop1
   beqz  a1, .read_done         # leave if all bytes written out

.spi_read_cont1:
   andi  t2, a1, 3               # get any unaligned remainder bytes
   sub   a1, a1, t2              # subtract them from the aligned read
   beqz  a1, .rd_unaligned_2     # any aligned bytes to read out?
   li    t1, 3                   # set 32 bit transfers
   sb    t1, OFFS_SPI_REG_BITCOUNT(t0)
.rd_aligned_loop:
   sw    a3, OFFS_SPI_DAT(t0)    # write out pattern
   lw    t3, OFFS_SPI_DAT(t0)    # read 32 bits
   sw    t3, 0(a0)               # result -> memory
   addi  a0, a0, 4               # next address
   addi  a1, a1, -4              # decrement count
   bnez  a1, .rd_aligned_loop
   beqz  t2, .read_done         # if no unaligned remainder, we're done

.rd_unaligned_2:
   li    t1, 1                   # set bytes per xfer = 1
   sb    zero, OFFS_SPI_REG_BITCOUNT(t0)
.rd_un_loop2:                    # t2 = how many unaligned bytes to read
   sb    a3, OFFS_SPI_DAT(t0)    # write out pattern
   lb    t3, OFFS_SPI_DAT+3(t0)    
   sb    t3, 0(a0)               # result -> memory
   addi  a0, a0, 1               # increment address
   addi  t2, t2, -1              # decrement byte count
   bnez  t2, .rd_un_loop2

.read_done:
   beqz  a2, .read_exit         # Don't deassert slave select if a2=0
   sb    zero, OFFS_SPI_REG_ACTIVE(t0)

.read_exit:
   ret
