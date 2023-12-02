#include "devices.h"

# Prints character in a0
.globl serial_putc
serial_putc:

.busy:
   lw    t1, UART_STAT(gp)    # uart state
   andi  t1, t1, 2            # apply write busy mask
   bne   t1, zero, .busy      # wait until uart is free

   li    t1, 100              # delay
.delay:
   addi  t1, t1, -1
   bnez  t1, .delay
   
   sw a0, UART_DAT(gp)        # send data 

   ret

