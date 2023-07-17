# Testbed for system calls
.data
startup:   .ascii "Started OK\r\n"
.set startup_sz, .-startup
returncode: .ascii "Return code: "
.set returncode_sz, .-returncode
crlf: .ascii "\r\n"
stdoutstr:  .ascii "Wrote this to stdout\r\n"
.set stdoutstr_sz, .-stdoutstr
openstr:    .asciz "/dev/rgbled"
rgbled:     .byte  1,1,1,1
ledfailed:  .ascii "RGB led open failed\r\n"
.set ledfailed_sz, .-ledfailed
filedes:    .ascii "File descriptor: "
.set filedes_sz, .-filedes

.set SYSCALL_write, 64
.set SYSCALL_read,  63
.set SYSCALL_open,  1024
.set SYSCALL_close, 57

.text
.global start
start:
   la    sp, __stack_top
   addi  sp, sp, -16

   la    a0, 0x800000
   li    a1, 1
   sw    a1, 0(a0)

   la    a0, startup
   la    a1, startup_sz
   call  serial_write
   
   li    a7, SYSCALL_write
   li    a0, 1             # stdout
   la    a1, stdoutstr     # ptr
   li    a2, stdoutstr_sz  # count
   ecall

   sw    a0, 12(sp)
   la    a0, returncode
   la    a1, returncode_sz
   call  serial_write
   lw    a0, 12(sp)
   call  serial_hexprint
   la    a0, crlf
   la    a1, 2
   call  serial_write

   # Test open
   la    a0, openstr       # path
   li    a1, 1             # flags
   li    a2, 0666          # mode
   li    a7, SYSCALL_open
   ecall
   blez  a0, .rgbled_failed

   sw    a0, 0(sp)
   la    a0, filedes
   li    a1, filedes_sz
   call  serial_write
   lw    a0, 0(sp)
   call  serial_hexprint
   la    a0, crlf
   li    a1, 2
   call  serial_write

   lw    a0, 0(sp)
   la    a1, rgbled        # byte to send
   li    a2, 4             # size             
   li    a7, SYSCALL_write
   ecall

   sw    a0, 12(sp)
   la    a0, returncode
   la    a1, returncode_sz
   call  serial_write
   lw    a0, 12(sp)
   call  serial_hexprint
   la    a0, crlf
   la    a1, 2
   call  serial_write

.halt_loop:
   j .halt_loop

.rgbled_failed:
   sw    a0, 0(sp)
   la    a0, ledfailed
   li    a1, ledfailed_sz
   call  serial_write
   lw    a0, 0(sp)
   call  serial_hexprint
   j     .halt_loop 
