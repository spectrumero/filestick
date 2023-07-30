.set SYSCALL_write,     64
.set SYSCALL_read,      63
.set SYSCALL_open,      1024
.set SYSCALL_close,     57
.set SYSCALL_exit,      93

.text
.globl start
start:
   la       sp, __stack_top
   call     run_tests

.halt_loop:
   j        .halt_loop

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

.globl exit
exit:
   li       a7, SYSCALL_exit
   ecall
   ret

