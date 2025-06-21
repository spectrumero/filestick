#include "devices.h"
.data
flashstring:
.ascii "boot: Flash id: "
.set flashstring_sz, .-flashstring
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
   la       gp, DEV_BASE
   li       a0, 2
   sw       a0, 0(gp)

   la       a0, flashstring
   li       a1, flashstring_sz
   call     serial_write

   call     flash_reset          # put flash in a known state
.loop:
   call     flash_id
   call     serial_hexprint
   li       a1, 0xFFFFFFFF       # test that flash is present
   beq      a1, a0, .badflash    

   # flash address 0x20000, mem addr 0x0000, size 64k
   li       a0, 0x20000
   li       a1, 0
   li       a2, 0x10000
   call     flash_memcpy

   la       a0, ok
   li       a1, ok_sz
   call     serial_write

   # dump out first 32 bytes so we can check that things look OK
   li       a0, 0
   li       a1, 32
   call     serial_hexdump

   # run
   j        0x00000

.badflash:
   la    a0, bad
   li    a1, bad_sz
   call  serial_write

   li    s1, 4
   sw    s1, 0(s0)         # RGB LED red
.halt:
   j     .halt

