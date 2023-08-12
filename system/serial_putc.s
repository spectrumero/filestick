.globl serial_putc
serial_putc:
   li    t0, '\n'                # newline?
   bne   t0, a0, .write_byte     # no

   addi  sp, sp, -16             # Send \r\n
   sw    ra, 12(sp)
   li    a0, '\r'
   call  .write_byte
   li    a0, '\n'
   call  .write_byte
   lw    ra, 12(sp)
   addi  sp, sp, 16
   ret
   
.write_byte:
   #addi  sp, sp, -16       # set up stack frame

.busy:
   la t0, 0x80000C         # uart IO address
   lw t1, 4(t0)            # uart state
   andi t1, t1, 2          # apply write busy mask
   bne t1, zero, .busy     # wait until uart is free

   li t1, 100              # delay - there's a bug in the UART that means it prematurely
                           # is not busy!
.delay:
   addi t1, t1, -1
   bnez t1, .delay
   
   sw a0, 0(t0)            # send data 

   #addi  sp, sp, 16        # restore stack
   ret

