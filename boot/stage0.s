.data
flashstring:
.ascii "boot: Flash id: "
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
   la    sp, __stack_top   # initialize stack

   la    s0, 0x800000      # device base
   li    s1, 1
   sw    s1, 0(s0)         # turn on RGB LED blue

   la    a0, flashstring
   li    a1, flashstring_sz
   call  serial_write

   call  flash_reset       # put flash in a known state
   call  flash_id
   call  serial_hexprint
   
   li    a1, 0xFFFFFFFF
   beq   a1, a0, .badflash

   la    a0, ok
   li    a1, ok_sz
   call  serial_write

   li    s1, 5
   sw    s1, 0(s0)         # RGB magenta

   la    a0, booting
   la    a1, booting_sz
   call  serial_write 

.flash_copy:
   li    s1, 0x10000       # destination address
   li    s2, 0x10000       # bytes to copy

   li    a0, 0x03          # cmd = 3
   call  flash_byte
   li    a0, 0x02          # start at flash 0x020000
   call  flash_byte
   li    a0, 0x00
   call  flash_byte
   li    a0, 0x00
   call  flash_byte
.flash_copy_loop:
   li    a0, 0x00
   call  flash_byte        # byte now in lowest 8 bits of a0
   sb    a0, 0(s1)         # store it
   addi  s1, s1, 1
   addi  s2, s2, -1
   bnez  s2, .flash_copy_loop

   la    a0, ok
   li    a1, ok_sz
   call  serial_write

   j     0x10000           # run

.badflash:
   la    a0, bad
   li    a1, bad_sz
   call  serial_write

   li    s1, 4
   sw    s1, 0(s0)         # RGB LED red
.halt:
   j     .halt

   

