.text

#--------------------------------------------------------------------
# Copy from the address in a0, to the destination in a1, for a2 bytes
.globl flash_memcpy
flash_memcpy:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    a0, 8(sp)
   sw    a1, 4(sp)
   sw    a2, 0(sp)

   li    a0, 0x03          # command = 0x03
   call  flash_byte
   lw    a0, 8(sp)         # get src arg
   srli  a0, a0, 16        # 24-16 bit address
   call  flash_byte
   lw    a0, 8(sp)
   srli  a0, a0, 8         # 15-8
   call  flash_byte
   lw    a0, 8(sp)         # 7-0
   call  flash_byte   
.copy_loop:
   li    a0, 0x00
   call  flash_byte
   sb    a0, 0(a1)         # dest
   addi  a1, a1, 1
   addi  a2, a2, -1        # counter
   bnez  a2, .copy_loop

   sw    a0, 4(t0)         # deassert spi_ss

   lw    a2, 0(sp)
   lw    a1, 4(sp)
   lw    a0, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

##-----------------------------------------------------------------------
## Resets the SPI flash
.globl flash_reset
flash_reset:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    s1, 8(sp)

   li    s1, 10            # send 10 sequences
.rst_loop:
   li    a0, 0xFF
   call  flash_byte
   addi  s1, s1, -1
   bnez  s1, .rst_loop

   sw    a0, 4(t0)         # touching 0x800018 will reset flash_ss

   li    a0, 0xAB          # release power down
   call  flash_byte
   li    a0, 0xFF
   call  flash_byte        # dummy bytes
   li    a0, 0xFF
   call  flash_byte
   li    a0, 0xFF
   call  flash_byte
   
   sw    a0, 4(t0)         # touching 0x800018 will reset flash_ss

   lw    s1, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

#-----------------------------------------------------------------------
# Writes and reads from flash. Data in in a0, out in a0.
.globl flash_byte
flash_byte:
   la    t0, 0x800014      # device address
   sw    a0, 0(t0)         # write to flash (low 8 bits only)
.fl_loop:
   lw    a0, 0(t0)         # test/read
   andi  t1, a0, 0x100     # ready mask
   beqz  t1, .fl_loop      # if not ready read again
   andi  a0, a0, 0xFF      # remove ready bit
   ret
  
 
