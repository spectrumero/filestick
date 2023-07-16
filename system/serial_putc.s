.globl serial_putc
serial_putc:
   #addi  sp, sp, -16       # set up stack frame

.busy:
   la t0, 0x80000C         # uart IO address
   lw t1, 4(t0)            # uart state
   andi t1, t1, 2          # apply write busy mask
   bne t1, zero, .busy     # wait until uart is free

   li t1, 100              # delay
.delay:
   addi t1, t1, -1
   bnez t1, .delay
   
   sw a0, 0(t0)            # send data 

   #addi  sp, sp, 16        # restore stack
   ret

