# Testbed for system calls
.data
startup:   .ascii "Started OK\r\n"
.set startup_sz, .-startup
returncode: .ascii "Return code: "
.set returncode_sz, .-returncode
crlf: .ascii "\r\n"
stdoutstr:  .ascii "Wrote this to stdout\r\n"
.set stdoutstr_sz, .-stdoutstr

.set SYSCALL_write, 64

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
   
#
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

.halt_loop:
   la    a0, 0x800000
   li    a1, 7
   sw    a1, 0(a0)
   j .halt_loop

