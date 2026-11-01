/*
 * CPU test suite - RV32I control transfer, LUI, AUIPC, and register file.
 */
#include "cputest.h"

.option norvc

//---------------------------------------------------------------------------
TEST_FUNC lui
   li    x13, 0xFFFFFFFF
   lui   x13, 0x00000
   CHECK 1, x13, 0x00000000
   li    x13, 0xFFFFFFFF
   lui   x13, 0x00001
   CHECK 2, x13, 0x00001000
   lui   x13, 0x7FFFF
   CHECK 3, x13, 0x7FFFF000
   lui   x13, 0x80000
   CHECK 4, x13, 0x80000000
   lui   x13, 0xFFFFF
   CHECK 5, x13, 0xFFFFF000
   lui   x13, 0x12345
   CHECK 6, x13, 0x12345000
   lui   x31, 0xABCDE
   CHECK 7, x31, 0xABCDE000, x30
   lui   x0, 0x12345
   CHECK 8, x0, 0
TEST_END

//---------------------------------------------------------------------------
// AUIPC: x5 gets the address of the auipc from the jal link value, so the
// result doesn't depend on AUIPC working.
.macro TEST_AUIPC n, imm
   jal   x5, .Lauipc\@
.Lauipc\@:
   auipc x6, \imm
   li    x7, (\imm << 12)
   add   x7, x7, x5
   CHECK_REG \n, x6, x7
.endm

TEST_FUNC auipc
   TEST_AUIPC  1, 0x00000
   TEST_AUIPC  2, 0x00001
   TEST_AUIPC  3, 0x007FF
   TEST_AUIPC  4, 0x00800
   TEST_AUIPC  5, 0xFFFFF        // pc - 4096
   TEST_AUIPC  6, 0x7FFFF
   TEST_AUIPC  7, 0x80000
   TEST_AUIPC  8, 0x12345
   TEST_AUIPC  9, 0xFFF00        // pc - 1M
   jal   x5, .Lauipc_x0
.Lauipc_x0:
   auipc x0, 0x12345
   CHECK 10, x0, 0
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC jal
   // 1-2: forwards, link is the following instruction
   la    x5, 1f
   jal   x6, 2f
1: FAIL_CONST 1, 0, 1
2: CHECK_REG 2, x6, x5

   // 3-4: backwards
   j     4f
3: j     5f
4: la    x5, 1f
   jal   x7, 3b
1: FAIL_CONST 3, 0, 1
5: CHECK_REG 4, x7, x5

   // 5-6: rd = x0
   jal   x0, 1f
   FAIL_CONST 5, 0, 1
1: CHECK 6, x0, 0

   // 7-8: rd = ra
   la    x5, 1f
   jal   ra, 2f
1: FAIL_CONST 7, 0, 1
2: CHECK_REG 8, ra, x5

   // 9-10: rd = x31
   la    x5, 1f
   jal   x31, 2f
1: FAIL_CONST 9, 0, 1
2: CHECK_REG 10, x31, x5
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC jalr
   // 1-2: zero offset
   la    x5, 2f
   la    x6, 1f
   jalr  x7, 0(x5)
1: FAIL_CONST 1, 0, 1
2: CHECK_REG 2, x7, x6

   // 3-4: positive offset
   la    x5, 2f-12
   la    x6, 1f
   jalr  x7, 12(x5)
1: FAIL_CONST 3, 0, 1
2: CHECK_REG 4, x7, x6

   // 5-6: negative offset
   la    x5, 2f+2000
   la    x6, 1f
   jalr  x7, -2000(x5)
1: FAIL_CONST 5, 0, 1
2: CHECK_REG 6, x7, x6

   // 7-8: bit 0 of the target address is cleared (odd base)
   la    x5, 2f+1
   la    x6, 1f
   jalr  x7, 0(x5)
1: FAIL_CONST 7, 0, 1
2: CHECK_REG 8, x7, x6

   // 9-10: bit 0 of the target address is cleared (odd offset)
   la    x5, 2f
   la    x6, 1f
   jalr  x7, 1(x5)
1: FAIL_CONST 9, 0, 1
2: CHECK_REG 10, x7, x6

   // 11-12: rd = rs1, the target is the old value of rs1
   la    x8, 2f
   la    x6, 1f
   jalr  x8, 0(x8)
1: FAIL_CONST 11, 0, 1
2: CHECK_REG 12, x8, x6

   // 13-14: rd = x0
   la    x5, 1f
   jalr  x0, 0(x5)
   FAIL_CONST 13, 0, 1
1: CHECK 14, x0, 0

   // 15-16: backwards
   j     4f
3: j     5f
4: la    x5, 3b
   la    x6, 1f
   jalr  x9, 0(x5)
1: FAIL_CONST 15, 0, 1
5: CHECK_REG 16, x9, x6
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC beq
   TEST_BR_TAKEN       1, beq, 0, 0
   TEST_BR_TAKEN       2, beq, 1, 1
   TEST_BR_TAKEN       3, beq, 0xFFFFFFFF, 0xFFFFFFFF
   TEST_BR_TAKEN       4, beq, 0x80000000, 0x80000000
   TEST_BR_TAKEN_BACK  5, beq, 0x12345678, 0x12345678
   TEST_BR_NOT_TAKEN   6, beq, 0, 1
   TEST_BR_NOT_TAKEN   7, beq, 1, 0
   TEST_BR_NOT_TAKEN   8, beq, 0xFFFFFFFF, 1
   TEST_BR_NOT_TAKEN   9, beq, 0x80000000, 0
   TEST_BR_NOT_TAKEN  10, beq, 0x12345678, 0x12345679
   TEST_BR_NOT_TAKEN  11, beq, 0x00000001, 0x80000001
   TEST_BR_SAME       12, beq, 1, 0x5A5A5A5A
   TEST_BR_X0_RS1     13, beq, 1, 0
   TEST_BR_X0_RS1     14, beq, 0, 1
   TEST_BR_X0_RS2     15, beq, 1, 0
   TEST_BR_X0_RS2     16, beq, 0, 0x80000000
TEST_END

TEST_FUNC bne
   TEST_BR_NOT_TAKEN   1, bne, 0, 0
   TEST_BR_NOT_TAKEN   2, bne, 1, 1
   TEST_BR_NOT_TAKEN   3, bne, 0xFFFFFFFF, 0xFFFFFFFF
   TEST_BR_NOT_TAKEN   4, bne, 0x80000000, 0x80000000
   TEST_BR_TAKEN_BACK  5, bne, 0x12345678, 0x12345670
   TEST_BR_TAKEN       6, bne, 0, 1
   TEST_BR_TAKEN       7, bne, 1, 0
   TEST_BR_TAKEN       8, bne, 0xFFFFFFFF, 1
   TEST_BR_TAKEN       9, bne, 0x80000000, 0
   TEST_BR_TAKEN      10, bne, 0x12345678, 0x12345679
   TEST_BR_TAKEN      11, bne, 0x00000001, 0x80000001
   TEST_BR_SAME       12, bne, 0, 0x5A5A5A5A
   TEST_BR_X0_RS1     13, bne, 0, 0
   TEST_BR_X0_RS1     14, bne, 1, 1
   TEST_BR_X0_RS2     15, bne, 0, 0
   TEST_BR_X0_RS2     16, bne, 1, 0x80000000
TEST_END

TEST_FUNC blt
   TEST_BR_TAKEN       1, blt, 0, 1
   TEST_BR_TAKEN       2, blt, 0xFFFFFFFF, 0
   TEST_BR_TAKEN       3, blt, 0x80000000, 0x7FFFFFFF
   TEST_BR_TAKEN       4, blt, 0x80000000, 0xFFFFFFFF
   TEST_BR_TAKEN       5, blt, 0xFFFFFFFE, 0xFFFFFFFF
   TEST_BR_TAKEN_BACK  6, blt, 0x80000000, 0x80000001
   TEST_BR_NOT_TAKEN   7, blt, 1, 0
   TEST_BR_NOT_TAKEN   8, blt, 0, 0
   TEST_BR_NOT_TAKEN   9, blt, 0, 0xFFFFFFFF
   TEST_BR_NOT_TAKEN  10, blt, 0x7FFFFFFF, 0x80000000
   TEST_BR_NOT_TAKEN  11, blt, 0xFFFFFFFF, 0xFFFFFFFE
   TEST_BR_NOT_TAKEN  12, blt, 0x80000000, 0x80000000
   TEST_BR_SAME       13, blt, 0, 0xFFFFFFFF
   TEST_BR_X0_RS1     14, blt, 1, 1
   TEST_BR_X0_RS1     15, blt, 0, 0xFFFFFFFF
   TEST_BR_X0_RS2     16, blt, 1, 0x80000000
   TEST_BR_X0_RS2     17, blt, 0, 1
TEST_END

TEST_FUNC bge
   TEST_BR_NOT_TAKEN   1, bge, 0, 1
   TEST_BR_NOT_TAKEN   2, bge, 0xFFFFFFFF, 0
   TEST_BR_NOT_TAKEN   3, bge, 0x80000000, 0x7FFFFFFF
   TEST_BR_NOT_TAKEN   4, bge, 0x80000000, 0xFFFFFFFF
   TEST_BR_NOT_TAKEN   5, bge, 0xFFFFFFFE, 0xFFFFFFFF
   TEST_BR_TAKEN_BACK  6, bge, 0x80000001, 0x80000000
   TEST_BR_TAKEN       7, bge, 1, 0
   TEST_BR_TAKEN       8, bge, 0, 0
   TEST_BR_TAKEN       9, bge, 0, 0xFFFFFFFF
   TEST_BR_TAKEN      10, bge, 0x7FFFFFFF, 0x80000000
   TEST_BR_TAKEN      11, bge, 0xFFFFFFFF, 0xFFFFFFFE
   TEST_BR_TAKEN      12, bge, 0x80000000, 0x80000000
   TEST_BR_SAME       13, bge, 1, 0xFFFFFFFF
   TEST_BR_X0_RS1     14, bge, 0, 1
   TEST_BR_X0_RS1     15, bge, 1, 0xFFFFFFFF
   TEST_BR_X0_RS2     16, bge, 0, 0x80000000
   TEST_BR_X0_RS2     17, bge, 1, 1
TEST_END

TEST_FUNC bltu
   TEST_BR_TAKEN       1, bltu, 0, 1
   TEST_BR_TAKEN       2, bltu, 1, 0xFFFFFFFF
   TEST_BR_TAKEN       3, bltu, 0x7FFFFFFF, 0x80000000
   TEST_BR_TAKEN       4, bltu, 0xFFFFFFFE, 0xFFFFFFFF
   TEST_BR_TAKEN_BACK  5, bltu, 0, 0x80000000
   TEST_BR_NOT_TAKEN   6, bltu, 1, 0
   TEST_BR_NOT_TAKEN   7, bltu, 0xFFFFFFFF, 0
   TEST_BR_NOT_TAKEN   8, bltu, 0x80000000, 0x7FFFFFFF
   TEST_BR_NOT_TAKEN   9, bltu, 0xFFFFFFFF, 0xFFFFFFFE
   TEST_BR_NOT_TAKEN  10, bltu, 5, 5
   TEST_BR_SAME       11, bltu, 0, 0x80000000
   TEST_BR_X0_RS1     12, bltu, 1, 1
   TEST_BR_X0_RS1     13, bltu, 0, 0
   TEST_BR_X0_RS2     14, bltu, 0, 0xFFFFFFFF
   TEST_BR_X0_RS2     15, bltu, 0, 0
TEST_END

TEST_FUNC bgeu
   TEST_BR_NOT_TAKEN   1, bgeu, 0, 1
   TEST_BR_NOT_TAKEN   2, bgeu, 1, 0xFFFFFFFF
   TEST_BR_NOT_TAKEN   3, bgeu, 0x7FFFFFFF, 0x80000000
   TEST_BR_NOT_TAKEN   4, bgeu, 0xFFFFFFFE, 0xFFFFFFFF
   TEST_BR_TAKEN_BACK  5, bgeu, 0x80000000, 0
   TEST_BR_TAKEN       6, bgeu, 1, 0
   TEST_BR_TAKEN       7, bgeu, 0xFFFFFFFF, 0
   TEST_BR_TAKEN       8, bgeu, 0x80000000, 0x7FFFFFFF
   TEST_BR_TAKEN       9, bgeu, 0xFFFFFFFF, 0xFFFFFFFE
   TEST_BR_TAKEN      10, bgeu, 5, 5
   TEST_BR_SAME       11, bgeu, 1, 0x80000000
   TEST_BR_X0_RS1     12, bgeu, 0, 1
   TEST_BR_X0_RS1     13, bgeu, 1, 0
   TEST_BR_X0_RS2     14, bgeu, 1, 0xFFFFFFFF
   TEST_BR_X0_RS2     15, bgeu, 1, 0
TEST_END

//---------------------------------------------------------------------------
// Branches and jumps across a 2.7K gap, forwards and backwards, so the
// offsets use immediate bit 11 (which is encoded separately from the
// other bits in both the B and J formats).
TEST_FUNC branch_far
   li    x11, 1
   li    x12, 2
   beq   x11, x11, .Lfar_a1
   FAIL_CONST 1, 0, 1
.Lfar_b1:
   blt   x11, x12, .Lfar_a2
   FAIL_CONST 3, 0, 1
.Lfar_b2:
   bltu  x11, x12, .Lfar_a3
   FAIL_CONST 5, 0, 1
.Lfar_b3:
   jal   x5, .Lfar_a4
.Lfar_b3_ret:
   FAIL_CONST 7, 0, 1
.Lfar_b4:
   j     .Lfar_done

   .skip 0xAA8

.Lfar_a1:
   bne   x11, x12, .Lfar_b1
   FAIL_CONST 2, 0, 1
.Lfar_a2:
   bge   x12, x11, .Lfar_b2
   FAIL_CONST 4, 0, 1
.Lfar_a3:
   bgeu  x12, x11, .Lfar_b3
   FAIL_CONST 6, 0, 1
.Lfar_a4:
   jal   x6, .Lfar_b4
.Lfar_a4_ret:
   FAIL_CONST 8, 0, 1

.Lfar_done:
   la    x7, .Lfar_b3_ret
   CHECK_REG 9, x5, x7
   la    x7, .Lfar_a4_ret
   CHECK_REG 10, x6, x7
TEST_END

//---------------------------------------------------------------------------
// Every register (except sp) holds its own value: write a different value
// to each one, then store them all and check what was stored. Also checks
// x0 can't be written.
TEST_FUNC regfile
   addi  sp, sp, -128
   .irp r, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
   li    x\r, (\r * 0x01010101) ^ 0xA5000000
   .endr
   li    x0, 0x12345678
   .irp r, 0,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
   sw    x\r, (\r * 4)(sp)
   .endr

   lw    x5, 0(sp)
   CHECK 100, x5, 0
   .irp r, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
   lw    x5, (\r * 4)(sp)
   CHECK \r, x5, (\r * 0x01010101) ^ 0xA5000000
   .endr

   // Now clear every register, load each one back from memory (the value
   // stored above) and check it.
   .irp r, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
   li    x\r, 0
   .endr
   .irp r, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
   lw    x\r, (\r * 4)(sp)
   .endr
   CHECK 63, x31, (31 * 0x01010101) ^ 0xA5000000, x30
   lw    x30, (30 * 4)(sp)
   .irp r, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
   CHECK (\r + 32), x\r, (\r * 0x01010101) ^ 0xA5000000
   .endr
TEST_END
