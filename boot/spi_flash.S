#include "devices.h"

.text

// Call with gp = DEVICE_BASE address
// Returns flash id in a0
.globl flash_id
flash_id:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   
   sb    zero, OFFS_SPI_REG_ENDIAN(gp)   # set little endian so the low byte gets written out
   sb    zero, OFFS_SPI_REG_SS(gp)       # select SPI slave select 0
   li    a0, 0x9f                   # flash id
   call  flash_byte
   li    a0, 0xFFFFFFFF             # write 1s while getting response
   call  flash_word                 
   sb    zero, OFFS_SPI_REG_ACTIVE(gp)   # reset slave select

   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

// Call with gp = DEVICE_BASE address
.globl flash_reset
flash_reset:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    s1,  8(sp)

   sb    zero, OFFS_SPI_REG_SS(gp)       # select SPI slave select 0
   li    a0, 0xFFFFFFFF             # send some 1s
   call  flash_word
   sb    zero, OFFS_SPI_REG_ACTIVE(gp)   # reset ss

   sb    zero, OFFS_SPI_REG_ENDIAN(gp)   # set little endian so the low byte gets written out
   li    s1, 10                     # Send 10 sequences
.rst_loop:
   call  flash_byte
   addi  s1, s1, -1
   bnez  s1, .rst_loop

   sb    zero, OFFS_SPI_REG_ACTIVE(gp)   # reset flash_ss

   li    a0, 0xAB                   # release power down
   call  flash_byte
   li    a0, 0xFFFFFFFF
   call  flash_word                 # send dummy bytes

   sb    zero, OFFS_SPI_REG_ACTIVE(gp)   # reset flash_ss
   
   lw    s1, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

# Writes and reads from flash. Data in in a0, out in a0.
.globl flash_byte
flash_byte:
   sb    zero, OFFS_SPI_REG_BITCOUNT(gp)    # set SPI to write 1 byte
   sb    a0, OFFS_SPI_DAT(gp)               # write to SPI
   lb    a0, OFFS_SPI_DAT(gp)               # read back value shifted in
   ret

.globl flash_word
flash_word:
   li    t1, 3
   sb    t1, OFFS_SPI_REG_BITCOUNT(gp)      # set SPI to write 4 bytes
   sw    a0, OFFS_SPI_DAT(gp)
   lw    a0, OFFS_SPI_DAT(gp)
   ret

# Copies data from flash. Note expects size to be a whole number of words.
# a0 = flash source
# a1 = mem dest
# a2 = size
# gp = device base
.globl flash_memcpy
flash_memcpy:
 
   srli  a2, a2, 2                     # ensure size is in whole words
   li    t0, 1
   sb    t0, OFFS_SPI_REG_ENDIAN(gp)        # set endianness
   li    t0, 3
   sb    t0, OFFS_SPI_REG_BITCOUNT(gp)      # set length to words
   li    t0, 0x03000000                # flash read in MSB
   or    t0, t0, a0                    # set the flash src address
   sw    t0, OFFS_SPI_DAT(gp)               # send request
   lw    t0, OFFS_SPI_DAT(gp)               # consume 4 bytes from flash
   li    t0, 0xFFFFFFFF                # write 1s while reading flash
   sb    zero, OFFS_SPI_REG_ENDIAN(gp)      # set endianness for transfer
.copy_loop:
   sw    t0, OFFS_SPI_DAT(gp)               # move 32 bits of data
   lw    t1, OFFS_SPI_DAT(gp)
   sw    t1, 0(a1)                     # result -> memory
   addi  a1, a1, 4                     # next word
   addi  a2, a2, -1                    # dec word count
   bnez  a2, .copy_loop                # continue if there's stuff left to do

   sb    zero, OFFS_SPI_REG_ACTIVE(gp)      # reset spi_ss    
   ret
 
