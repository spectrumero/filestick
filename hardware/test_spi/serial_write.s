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

# dump memory as hex
# a0 = memory address
# a1 = length
.globl serial_hexdump
serial_hexdump:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    s1, 8(sp)
   sw    s2, 4(sp)
   sw    s3, 0(sp)
  
   mv    s1, a0            # use s1 and s2 for curr addr and remaining len
   mv    s2, a1
   li    s3, 0

.dump_loop:
   lb    a0, 0(s1)         # get byte at address
   call  serial_hexbyte    # print it
   li    a0, ' '
   call  serial_putc

   # after 8th put an extra space
   andi  a0, s3, 7
   slti  a0, a0, 7
   bnez  a0, .nospace
   li    a0, ' '
   call  serial_putc

.nospace:
   # after 16th put CR
   andi  a0, s3, 0xF
   slti  a0, a0, 0xF
   bnez  a0, .nocr
   call  serial_crlf

.nocr:
   addi  s1, s1, 1         # next address
   addi  s3, s3, 1
   bne   s3, s2, .dump_loop

   call  serial_crlf

   lw    s3, 0(sp)
   lw    s2, 4(sp)
   lw    s1, 8(sp)
   lw    ra, 12(sp)
   ret

.globl serial_crlf
serial_crlf:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   li    a0, '\r'
   call  serial_putc
   li    a0, '\n'
   call  serial_putc
   lw    ra, 12(sp)
   addi  sp, sp, 16  
   ret

# print a byte as a 2 digit hex string. value in a0 LSB
.globl serial_hexbyte
serial_hexbyte:
   addi  sp, sp, -16
   sw    ra, 12(sp)
   sw    s1, 8(sp)

   mv    s1, a0         # save passed value
   li    t0, 0xF0       # first digit
   and   t1, a0, t0     # mask it out
   srli  t1, t1, 4      # put it in t1 LSN
   call  .print_nibble  # print it
   
   li    t0, 0x0F       # second digit
   and   t1, s1, t0     # mask it out
   call  .print_nibble  # print it
  
   lw    s1, 8(sp)
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret 

.print_nibble:
   sw    ra, 4(sp)
   slti  t2, t1, 10     # less than 10?
   beqz  t2, .print_letter
   addi  a0, t1, '0'    # a0 = ascii 0-9
   call  serial_putc
   lw    ra, 4(sp)
   ret
.print_letter:
   addi  a0, t1, 'A'-10 # ascii digit A-F
   call  serial_putc
   lw    ra, 4(sp)
   ret
    
