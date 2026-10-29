# Run a function with the stack pointer set to a new value. This is used
# to put BASIC's stack at HIMEM in its workspace, rather than the much
# smaller user stack.
#
# void run_on_stack(void (*fn)(void), void *sp)

.text
.globl run_on_stack
run_on_stack:
   mv    t0, sp
   andi  a1, a1, -16
   addi  sp, a1, -16
   sw    ra, 12(sp)
   sw    t0, 8(sp)
   jalr  a0
   lw    ra, 12(sp)
   lw    sp, 8(sp)
   ret
