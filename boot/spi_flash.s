.text
.globl flash_id
flash_id:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    s1, 8(sp)

   li    a0, 0x9f          # flash id
   call  flash_byte
   li    a0, 0xFF
   call  flash_byte
   mv    s1, a0
   li    a0, 0xFF
   call  flash_byte
   slli  s1, s1, 8
   or    s1, s1, a0
   li    a0, 0xFF
   call  flash_byte
   slli  s1, s1, 8
   or    s1, s1, a0
   li    a0, 0xFF
   call  flash_byte
   slli  s1, s1, 8
   or    s1, s1, a0
   mv    a0, s1
   
   # t0 will already be at 800014
   sw    a0, 4(t0)         # touching 0x800018 will reset flash_ss

   lw    s1, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

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
  
 
