#include "devices.h"

# Prints character in a0
.globl serial_putc
serial_putc:

.busy:
   lw    t1, OFFS_UARTSTATE(gp)  # uart state
   andi  t1, t1, 2               # apply write busy mask
   bne   t1, zero, .busy         # wait until uart is free

   sw a0, OFFS_UART(gp)          # send data 

   ret

