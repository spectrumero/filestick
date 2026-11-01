/*
 * CPU test suite - function entry/exit shared by all the tests, and the
 * data areas used by the load/store tests.
 */
#include "cputest.h"

.option norvc

.data
.balign 4
.globl test_got, test_exp
test_got:      .word 0
test_exp:      .word 0
test_saved_sp: .word 0

// Known data for the load tests.
.balign 4
.globl ld_data, ld_data_end
ld_data:
   .word 0x80FF7F01        // bytes 01 7F FF 80
   .word 0x00F0FF0F        // bytes 0F FF F0 00
   .word 0x12345678
   .word 0xDEADBEEF
ld_data_end:

.bss
.balign 4
.globl st_buf, test_buf
st_buf:     .skip 16
test_buf:   .skip 256

.text
.balign 4

// Called by TEST_FUNC with jal t0: save the registers the C caller expects
// to be preserved, and the stack pointer so the test can change it freely.
.globl test_prologue
test_prologue:
   addi  sp, sp, -64
   sw    ra,  0(sp)
   sw    s0,  4(sp)
   sw    s1,  8(sp)
   sw    s2, 12(sp)
   sw    s3, 16(sp)
   sw    s4, 20(sp)
   sw    s5, 24(sp)
   sw    s6, 28(sp)
   sw    s7, 32(sp)
   sw    s8, 36(sp)
   sw    s9, 40(sp)
   sw    s10, 44(sp)
   sw    s11, 48(sp)
   sw    gp, 52(sp)
   sw    tp, 56(sp)
   la    t1, test_saved_sp
   sw    sp, 0(t1)
   jr    t0

// All cases passed.
.globl test_pass
test_pass:
   li    a0, 0
   j     test_exit

// Case a0 failed, got a1, expected a2.
.globl test_fail
test_fail:
   la    t1, test_got
   sw    a1, 0(t1)
   la    t1, test_exp
   sw    a2, 0(t1)

test_exit:
   la    t1, test_saved_sp
   lw    sp, 0(t1)
   lw    ra,  0(sp)
   lw    s0,  4(sp)
   lw    s1,  8(sp)
   lw    s2, 12(sp)
   lw    s3, 16(sp)
   lw    s4, 20(sp)
   lw    s5, 24(sp)
   lw    s6, 28(sp)
   lw    s7, 32(sp)
   lw    s8, 36(sp)
   lw    s9, 40(sp)
   lw    s10, 44(sp)
   lw    s11, 48(sp)
   lw    gp, 52(sp)
   lw    tp, 56(sp)
   addi  sp, sp, 64
   ret
