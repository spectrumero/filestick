.globl serial_write

# a0 = pointer to data
# a1 = bytes to write
serial_write:
   addi  sp, sp, -16    # 16 bytes on the stack
   sw    ra, 12(sp)
   sw    a0, 8(sp)

   beqz  a1, .done      # zero length?
   mv    t2, a0
   li    t3, 0
.loop:
   lb    a0, 0(t2)
   call  serial_putc    # uses t0, t1
   addi  t2, t2, 1
   addi  t3, t3, 1
   blt   t3, a1, .loop

.done:
   lw    a0, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

##-------------------------------------------------
# Print the value in a0 as a binary string
.globl serial_binprint
serial_binprint:
   addi  sp, sp, -16    # allocate stack
   sw    ra, 12(sp)
   sw    s2, 8(sp)
   sw    a0, 4(sp)
   sw    s1, 0(sp)

   li    s2, 0x80000000 # bit mask
   mv    s1, a0
.binprint:
   and   t0, s1, s2
   beqz  t0, .printzero
   li    a0, '1'
   call  serial_putc
   j     .nextdigit
.printzero:
   li    a0, '0'
   call  serial_putc
.nextdigit:
   srli  s2, s2, 1
   bnez  s2, .binprint  # s2 = 0 when we're out of bits

   lw    s1, 0(sp)
   lw    a0, 4(sp)
   lw    s2, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret

##-------------------------------------------------
# Print the value in a0 as a hex string
.globl serial_hexprint
serial_hexprint:
   addi  sp, sp, -16    # allocate stack
   sw    ra, 12(sp)
   sw    s1, 8(sp)
   sw    s2, 4(sp)
   sw    a0, 0(sp)

   li    s2, 0xF0000000 # bit mask
   li    s1, 28         # number of shift bits
.hexprint:
   and   t0, a0, s2     # mask out current hex digit
   srl   t0, t0, s1     # shift to end
   slti  t1, t0, 10     # numeric or letter?
   beqz  t1, .letter
   addi  a0, t0, '0'    # a0 = ascii digit 0-9
   call  serial_putc
   j     .nexthex
.letter:
   addi  a0, t0, 'A'-10 # a0 = ascii digit A-F
   call  serial_putc
.nexthex:
   lw    a0, 0(sp)      # get a0 back off the stack
   srli  s2, s2, 4      # move the bit mask
   addi  s1, s1, -4     # next shift
   bnez  s2, .hexprint

   lw    s2, 4(sp)
   lw    s1, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret
   
   
