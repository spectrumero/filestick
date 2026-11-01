/*
 * CPU test suite - RV32C compressed instructions (c.ebreak excepted).
 *
 * The instructions under test are written with their c. mnemonics, so the
 * assembler must use the compressed encoding. Setup instructions that the
 * assembler could otherwise compress into the instruction being tested are
 * written with NORVC.
 */
#include "cputest.h"

.option rvc

//---------------------------------------------------------------------------
TEST_FUNC c_li
   NORVC li x8, 0x12345678
   c.li  x8, 0
   CHECK 1, x8, 0
   c.li  x8, 31
   CHECK 2, x8, 31
   c.li  x8, -32
   CHECK 3, x8, 0xFFFFFFE0
   c.li  x31, -1
   CHECK 4, x31, 0xFFFFFFFF, x30
   c.li  x1, 21
   CHECK 5, x1, 21
TEST_END

TEST_FUNC c_lui
   NORVC li x9, 0xFFFFFFFF
   c.lui x9, 1
   CHECK 1, x9, 0x00001000
   c.lui x9, 31
   CHECK 2, x9, 0x0001F000
   c.lui x9, 0xFFFFF
   CHECK 3, x9, 0xFFFFF000
   c.lui x9, 0xFFFE0
   CHECK 4, x9, 0xFFFE0000
   c.lui x31, 0x10
   CHECK 5, x31, 0x00010000, x30
   c.lui x1, 0x15
   CHECK 6, x1, 0x00015000
TEST_END

TEST_FUNC c_addi
   NORVC li x10, 5
   c.addi x10, -32
   CHECK 1, x10, 0xFFFFFFE5
   NORVC li x10, 0x7FFFFFFF
   c.addi x10, 1
   CHECK 2, x10, 0x80000000
   NORVC li x10, 0xFFFFFFF0
   c.addi x10, 31
   CHECK 3, x10, 0x0000000F
   NORVC li x31, 0
   c.addi x31, -1
   CHECK 4, x31, 0xFFFFFFFF, x30
TEST_END

TEST_FUNC c_nop
   NORVC li x5, 0x12345678
   NORVC li x6, 0
   c.nop
   c.nop
   CHECK 1, x5, 0x12345678
   CHECK 2, x6, 0
   CHECK 3, x0, 0
TEST_END

// sp must stay below its value on entry, so lower it first.
TEST_FUNC c_addi16sp
   NORVC addi sp, sp, -1024
   mv    x6, sp
   c.addi16sp sp, -512
   NORVC addi x7, x6, -512
   CHECK_REG 1, sp, x7
   mv    sp, x6
   c.addi16sp sp, 496
   NORVC addi x7, x6, 496
   CHECK_REG 2, sp, x7
   mv    sp, x6
   c.addi16sp sp, 16
   NORVC addi x7, x6, 16
   CHECK_REG 3, sp, x7
   mv    sp, x6
   c.addi16sp sp, -16
   NORVC addi x7, x6, -16
   CHECK_REG 4, sp, x7
   mv    sp, x6
   c.addi16sp sp, 0x150
   NORVC addi x7, x6, 0x150
   CHECK_REG 5, sp, x7
TEST_END

TEST_FUNC c_addi4spn
   NORVC addi sp, sp, -1024
   c.addi4spn x8, sp, 4
   NORVC addi x7, sp, 4
   CHECK_REG 1, x8, x7
   c.addi4spn x9, sp, 1020
   NORVC addi x7, sp, 1020
   CHECK_REG 2, x9, x7
   c.addi4spn x15, sp, 0x2A8
   NORVC addi x7, sp, 0x2A8
   CHECK_REG 3, x15, x7
   c.addi4spn x10, sp, 0x154
   NORVC addi x7, sp, 0x154
   CHECK_REG 4, x10, x7
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC c_lwsp
   NORVC addi sp, sp, -512
   NORVC li x5, 0x11223344
   NORVC sw x5, 0(sp)
   NORVC li x5, 0x55667788
   NORVC sw x5, 4(sp)
   NORVC li x5, 0x8899AABB
   NORVC sw x5, 0x90(sp)
   NORVC li x5, 0xCCDDEEFF
   NORVC sw x5, 252(sp)
   c.lwsp x9, 0(sp)
   CHECK 1, x9, 0x11223344
   c.lwsp x9, 4(sp)
   CHECK 2, x9, 0x55667788
   c.lwsp x31, 252(sp)
   CHECK 3, x31, 0xCCDDEEFF, x30
   c.lwsp x1, 0x90(sp)
   CHECK 4, x1, 0x8899AABB
TEST_END

TEST_FUNC c_swsp
   NORVC addi sp, sp, -512
   NORVC li x5, 0xAAAAAAAA
   NORVC sw x5, 0(sp)
   NORVC sw x5, 4(sp)
   NORVC sw x5, 8(sp)
   NORVC sw x5, 0x64(sp)
   NORVC sw x5, 252(sp)
   NORVC li x10, 0x12345678
   c.swsp x10, 4(sp)
   NORVC lw x6, 4(sp)
   CHECK 1, x6, 0x12345678
   NORVC lw x6, 0(sp)
   CHECK 2, x6, 0xAAAAAAAA
   NORVC lw x6, 8(sp)
   CHECK 3, x6, 0xAAAAAAAA
   NORVC li x31, 0x87654321
   c.swsp x31, 252(sp)
   NORVC lw x6, 252(sp)
   CHECK 4, x6, 0x87654321
   c.swsp x0, 0x64(sp)
   NORVC lw x6, 0x64(sp)
   CHECK 5, x6, 0
TEST_END

TEST_FUNC c_lw
   la    x8, test_buf
   NORVC li x5, 0x10203040
   NORVC sw x5, 0(x8)
   NORVC li x5, 0x50607080
   NORVC sw x5, 4(x8)
   NORVC li x5, 0x90A0B0C0
   NORVC sw x5, 0x54(x8)
   NORVC li x5, 0xD0E0F000
   NORVC sw x5, 124(x8)
   c.lw  x9, 0(x8)
   CHECK 1, x9, 0x10203040
   c.lw  x15, 124(x8)
   CHECK 2, x15, 0xD0E0F000
   c.lw  x10, 0x54(x8)
   CHECK 3, x10, 0x90A0B0C0
   mv    x15, x8
   c.lw  x8, 4(x15)
   CHECK 4, x8, 0x50607080
   c.lw  x15, 4(x15)
   CHECK 5, x15, 0x50607080
TEST_END

TEST_FUNC c_sw
   la    x8, test_buf
   NORVC li x5, 0xAAAAAAAA
   NORVC sw x5, 0(x8)
   NORVC sw x5, 4(x8)
   NORVC sw x5, 8(x8)
   NORVC sw x5, 0x28(x8)
   NORVC sw x5, 124(x8)
   NORVC li x9, 0x12345678
   c.sw  x9, 4(x8)
   NORVC lw x6, 4(x8)
   CHECK 1, x6, 0x12345678
   NORVC lw x6, 0(x8)
   CHECK 2, x6, 0xAAAAAAAA
   NORVC lw x6, 8(x8)
   CHECK 3, x6, 0xAAAAAAAA
   la    x15, test_buf
   NORVC li x14, 0xFEDCBA98
   c.sw  x14, 124(x15)
   NORVC lw x6, 124(x8)
   CHECK 4, x6, 0xFEDCBA98
   c.sw  x15, 0x28(x15)             // store the base register
   NORVC lw x6, 0x28(x8)
   CHECK_REG 5, x6, x15
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC c_srli
   NORVC li x8, 0x80000001
   c.srli x8, 1
   CHECK 1, x8, 0x40000000
   NORVC li x8, 0x80000001
   c.srli x8, 31
   CHECK 2, x8, 0x00000001
   NORVC li x15, 0xFFFFFFFF
   c.srli x15, 16
   CHECK 3, x15, 0x0000FFFF
   NORVC li x12, 0x12345678
   c.srli x12, 4
   CHECK 4, x12, 0x01234567, x31
TEST_END

TEST_FUNC c_srai
   NORVC li x8, 0x80000001
   c.srai x8, 1
   CHECK 1, x8, 0xC0000000
   NORVC li x8, 0x80000001
   c.srai x8, 31
   CHECK 2, x8, 0xFFFFFFFF
   NORVC li x15, 0x7FFFFFFF
   c.srai x15, 16
   CHECK 3, x15, 0x00007FFF
   NORVC li x9, 0x87654321
   c.srai x9, 4
   CHECK 4, x9, 0xF8765432
TEST_END

TEST_FUNC c_slli
   NORVC li x8, 0x80000001
   c.slli x8, 1
   CHECK 1, x8, 0x00000002
   NORVC li x31, 1
   c.slli x31, 31
   CHECK 2, x31, 0x80000000, x30
   NORVC li x1, 0x12345678
   c.slli x1, 4
   CHECK 3, x1, 0x23456780
   NORVC li x20, 0xFFFFFFFF
   c.slli x20, 16
   CHECK 4, x20, 0xFFFF0000
TEST_END

TEST_FUNC c_andi
   NORVC li x8, 0x12345678
   c.andi x8, -32
   CHECK 1, x8, 0x12345660
   NORVC li x8, 0x12345678
   c.andi x8, 31
   CHECK 2, x8, 0x00000018
   NORVC li x15, 0x12345678
   c.andi x15, 0
   CHECK 3, x15, 0
   NORVC li x9, 0x87654321
   c.andi x9, -1
   CHECK 4, x9, 0x87654321
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC c_sub
   NORVC li x8, 5
   NORVC li x9, 7
   c.sub x8, x9
   CHECK 1, x8, 0xFFFFFFFE
   CHECK 2, x9, 7
   NORVC li x15, 0x80000000
   NORVC li x14, 1
   c.sub x15, x14
   CHECK 3, x15, 0x7FFFFFFF
   NORVC li x10, 0x12345678
   c.sub x10, x10
   CHECK 4, x10, 0
TEST_END

TEST_FUNC c_xor
   NORVC li x8, 0xFF00FF00
   NORVC li x9, 0x0FF00FF0
   c.xor x8, x9
   CHECK 1, x8, 0xF0F0F0F0
   CHECK 2, x9, 0x0FF00FF0
   NORVC li x15, 0x12345678
   c.xor x15, x15
   CHECK 3, x15, 0
TEST_END

TEST_FUNC c_or
   NORVC li x8, 0xFF00FF00
   NORVC li x9, 0x0FF00FF0
   c.or  x8, x9
   CHECK 1, x8, 0xFFF0FFF0
   CHECK 2, x9, 0x0FF00FF0
   NORVC li x15, 0x12345678
   NORVC li x13, 0
   c.or  x15, x13
   CHECK 3, x15, 0x12345678
TEST_END

TEST_FUNC c_and
   NORVC li x8, 0xFF00FF00
   NORVC li x9, 0x0FF00FF0
   c.and x8, x9
   CHECK 1, x8, 0x0F000F00
   CHECK 2, x9, 0x0FF00FF0
   NORVC li x15, 0x12345678
   NORVC li x13, 0xFFFFFFFF
   c.and x15, x13
   CHECK 3, x15, 0x12345678
TEST_END

TEST_FUNC c_mv
   NORVC li x30, 0x12345678
   NORVC li x5, 0
   c.mv  x5, x30
   CHECK 1, x5, 0x12345678
   CHECK 2, x30, 0x12345678
   NORVC li x8, 0x87654321
   c.mv  x31, x8
   CHECK 3, x31, 0x87654321, x30
   c.mv  x1, x31
   CHECK 4, x1, 0x87654321, x30
TEST_END

TEST_FUNC c_add
   NORVC li x5, 0x12345678
   NORVC li x6, 0x11111111
   c.add x5, x6
   CHECK 1, x5, 0x23456789
   CHECK 2, x6, 0x11111111
   NORVC li x31, 0xFFFFFFFF
   NORVC li x8, 1
   c.add x31, x8
   CHECK 3, x31, 0, x30
   NORVC li x20, 0x40000001
   c.add x20, x20
   CHECK 4, x20, 0x80000002
TEST_END

//---------------------------------------------------------------------------
TEST_FUNC c_j
   // forwards
   c.j   1f
   FAIL_CONST 1, 0, 1
1:
   // backwards
   c.j   3f
2: c.j   4f
3: c.j   2b
   FAIL_CONST 2, 0, 1
4:
   // close to the maximum offset, forwards and backwards
   c.j   6f
   FAIL_CONST 3, 0, 1
5: c.j   7f
   .skip 0x7C0
6: c.j   5b
   FAIL_CONST 4, 0, 1
7:
TEST_END

TEST_FUNC c_jal
   // forwards: link is the following instruction, pc + 2
   la    x5, 1f
   c.jal 2f
1: FAIL_CONST 1, 0, 1
2: CHECK_REG 2, ra, x5

   // backwards
   j     4f
3: c.j   5f
4: la    x5, 1f
   c.jal 3b
1: FAIL_CONST 3, 0, 1
5: CHECK_REG 4, ra, x5
TEST_END

TEST_FUNC c_jr
   la    x5, 1f
   c.jr  x5
   FAIL_CONST 1, 0, 1
1:
   la    x31, 1f
   c.jr  x31
   FAIL_CONST 2, 0, 1
1:
   la    ra, 1f
   c.jr  ra
   FAIL_CONST 3, 0, 1
1:
TEST_END

TEST_FUNC c_jalr
   la    x5, 2f
   la    x6, 1f
   c.jalr x5
1: FAIL_CONST 1, 0, 1
2: CHECK_REG 2, ra, x6

   // rs1 = ra: the target is the old value
   la    ra, 2f
   la    x6, 1f
   c.jalr ra
1: FAIL_CONST 3, 0, 1
2: CHECK_REG 4, ra, x6

   la    x31, 2f
   la    x6, 1f
   c.jalr x31
1: FAIL_CONST 5, 0, 1
2: CHECK_REG 6, ra, x6
TEST_END

//---------------------------------------------------------------------------
.macro TEST_CBR_TAKEN n, inst, reg, val
   NORVC li \reg, \val
   \inst \reg, .Lcbr_ok\@
   FAIL_CONST \n, 0, 1
.Lcbr_ok\@:
.endm

.macro TEST_CBR_NOT_TAKEN n, inst, reg, val
   NORVC li \reg, \val
   \inst \reg, .Lcbr_bad\@
   j     .Lcbr_ok\@
.Lcbr_bad\@:
   FAIL_CONST \n, 1, 0
.Lcbr_ok\@:
.endm

.macro TEST_CBR_BACK n, inst, reg, val
   NORVC li \reg, \val
   j     .Lcbr_go\@
.Lcbr_back\@:
   j     .Lcbr_ok\@
.Lcbr_go\@:
   \inst \reg, .Lcbr_back\@
   FAIL_CONST \n, 0, 1
.Lcbr_ok\@:
.endm

.macro TEST_CBR_FAR n, inst, reg, val
   NORVC li \reg, \val
   \inst \reg, .Lcbr_fwd\@
   FAIL_CONST \n, 0, 1
.Lcbr_back\@:
   j     .Lcbr_ok\@
   .skip 0xE0
.Lcbr_fwd\@:
   \inst \reg, .Lcbr_back\@
   FAIL_CONST \n, 0, 2
.Lcbr_ok\@:
.endm

TEST_FUNC c_beqz
   TEST_CBR_TAKEN      1, c.beqz, x8, 0
   TEST_CBR_TAKEN      2, c.beqz, x15, 0
   TEST_CBR_NOT_TAKEN  3, c.beqz, x8, 1
   TEST_CBR_NOT_TAKEN  4, c.beqz, x9, 0x80000000
   TEST_CBR_NOT_TAKEN  5, c.beqz, x10, 0xFFFFFFFF
   TEST_CBR_BACK       6, c.beqz, x11, 0
   TEST_CBR_FAR        7, c.beqz, x12, 0
TEST_END

TEST_FUNC c_bnez
   TEST_CBR_NOT_TAKEN  1, c.bnez, x8, 0
   TEST_CBR_NOT_TAKEN  2, c.bnez, x15, 0
   TEST_CBR_TAKEN      3, c.bnez, x8, 1
   TEST_CBR_TAKEN      4, c.bnez, x9, 0x80000000
   TEST_CBR_TAKEN      5, c.bnez, x10, 0xFFFFFFFF
   TEST_CBR_BACK       6, c.bnez, x11, 0x00010000
   TEST_CBR_FAR        7, c.bnez, x12, 2
TEST_END

//---------------------------------------------------------------------------
// Mixing 16 and 32 bit instructions: 32 bit instructions at addresses that
// are 2 mod 4, so they straddle a word boundary, and jumps to them.
TEST_FUNC rvc_align
   // 1: a run of straddling ALU instructions
   .balign 4
   c.li  x9, 0
   NORVC addi x9, x9, 0x123
   NORVC addi x9, x9, 0x111
   NORVC xori x9, x9, 0x7FF
   CHECK 1, x9, 0x000005CB

   // 2: straddling load
   la    x11, ld_data
   .balign 4
   c.nop
   NORVC lw x10, 8(x11)
   CHECK 2, x10, 0x12345678

   // 3: straddling store
   la    x11, test_buf
   NORVC li x10, 0x13572468
   .balign 4
   c.nop
   NORVC sw x10, 0(x11)
   NORVC lw x12, 0(x11)
   CHECK 3, x12, 0x13572468

   // 4-5: straddling jal, to a straddling target; link is pc + 4
   la    x5, 1f
   .balign 4
   c.nop
   NORVC jal x6, 2f
1: FAIL_CONST 4, 0, 1
   .balign 4
   c.nop
2: NORVC addi x7, x0, 0x2A5
   CHECK_REG 5, x6, x5
   CHECK 6, x7, 0x2A5

   // 7-8: jalr to a straddling target, from a straddling jalr
   la    x5, 2f
   la    x8, 1f
   .balign 4
   c.nop
   NORVC jalr x6, 0(x5)
1: FAIL_CONST 7, 0, 1
   .balign 4
   c.nop
2: NORVC addi x7, x0, 0x15A
   CHECK_REG 8, x6, x8
   CHECK 9, x7, 0x15A

   // 10: straddling taken branch to a straddling target
   NORVC li x8, 3
   .balign 4
   c.nop
   NORVC bne x8, x0, 2f
   FAIL_CONST 10, 0, 1
   .balign 4
   c.nop
2: NORVC addi x7, x0, 0x0F0
   CHECK 11, x7, 0x0F0

   // 12: straddling branch, not taken, falls through to a straddling
   // instruction
   .balign 4
   c.nop
   NORVC beq x8, x0, 1f
   NORVC addi x7, x0, 0x00F
   j     2f
1: FAIL_CONST 12, 1, 0
2: CHECK 13, x7, 0x00F

   // 14: compressed jump to a 32 bit instruction at 2 mod 4, and
   // compressed jalr to the same
   c.j   1f
   FAIL_CONST 14, 0, 1
   .balign 4
   c.nop
1: NORVC addi x7, x0, 0x777
   CHECK 15, x7, 0x777
   la    x5, 1f
   c.jalr x5
   FAIL_CONST 16, 0, 1
   .balign 4
   c.nop
1: NORVC addi x7, x0, 0x555
   CHECK 17, x7, 0x555

   // 18: 32 bit instruction at 0 mod 4, compressed at 0 and 2 mod 4, then
   // 32 bit at 0 mod 4 again
   .balign 4
   NORVC addi x7, x0, 0x100
   c.addi x7, 1
   c.addi x7, 2
   NORVC addi x7, x7, 0x10
   CHECK 18, x7, 0x113
TEST_END
