.set SYSCALL_write,     64
.set SYSCALL_read,      63
.set SYSCALL_open,      1024
.set SYSCALL_close,     57
.set SYSCALL_lseek,     62
.set SYSCALL_fstat,     80
.set SYSCALL_exit,      93
.set SYSCALL_hexdump,   32
.set SYSCALL_hexword,   33

.text
.globl start
start:
   la       sp, __stack_top
   call     run_tests

.halt_loop:
   j        .halt_loop

#-----------------
.globl open
open:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_open
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-----------------
.globl close
close:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_close
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-----------------
.globl write
write:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_write
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-----------------
.globl read
read:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_read
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-----------------
.globl lseek
lseek:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_lseek
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-----------------
.globl fstat
fstat:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_fstat
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-------------------
.globl hexdump
hexdump:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_hexdump
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

#-------------------
.globl hexword
hexword:
   addi     sp, sp, -16
   sw       ra, 12(sp)

   li       a7, SYSCALL_hexword
   ecall

   lw       ra, 12(sp)
   addi     sp, sp, 16
   ret

.globl exit
exit:
   li       a7, SYSCALL_exit
   ecall
   ret

