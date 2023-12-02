#include "devices.h"
.data
flashstring:
.ascii "Test: Flash id: "
.set flashstring_sz, .-flashstring
crlf:
.ascii "\r\n"
ok:
.ascii "...OK\r\n"
.set ok_sz, .-ok
bad:
.ascii "...Bad!\r\n"
.set bad_sz, .-bad
booting:
.ascii "Booting..."
.set booting_sz, .-booting


.option arch, +zicsr
.text
   la       sp, __stack_top
   
   # global pointer used as IO address space pointer during
   # bootup sequence.
   la       gp, DEVICE_BASE   

   li       a0, 1
   sw       a0, RGB_LED(gp)            # RGB led -> blue

   la       a0, flashstring
   li       a1, flashstring_sz
   call     serial_write

   call     flash_reset       # put flash in a known state
.loop:
   call     flash_id
   call     serial_hexprint

   la       a0, crlf
   li       a1, 2
   call     serial_write

   la       a0, 0x20000
   li       a1, 32
   call     serial_hexdump

   # flash address 0x20000, mem addr 0x0000, size 64k
   li       a0, 0x20000
   li       a1, 0
   li       a2, 0x10000
   call     flash_memcpy

   li       a0, 0
   li       a1, 32
   call     serial_hexdump

.hltloop:
   j        .hltloop


