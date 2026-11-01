/*
 * CPU test suite - assembler macros shared by the test files.
 *
 * Each test is an assembler function "int test_<name>(void)" called from
 * main.c. It returns 0 if every case passed, otherwise the number of the
 * first case that failed, with the value the CPU produced and the value
 * expected in test_got and test_exp.
 *
 * Tests may use any register, including sp, gp and tp: test_prologue saves
 * the callee saved registers and the stack pointer, and test_exit puts them
 * back. The one rule is that sp must always point at memory that can be
 * used as stack, because the interrupt handler pushes onto the main bank's
 * sp. Lower it as much as you like, but don't raise it above its value on
 * entry.
 *
 * A test function looks like:
 *
 *    TEST_FUNC add
 *       TEST_RR  1, add, 0x00000003, 1, 2
 *       ...
 *    TEST_END
 *
 * Case numbers are written out in each macro call, so a reported failure
 * can be found by searching for the test name and the case number.
 */
#ifndef CPUTEST_H
#define CPUTEST_H

// Never let the linker rewrite instruction sequences, otherwise the
// instructions run are not necessarily those written.
.option norelax

// Assemble one instruction with its full size (32 bit) encoding, even in a
// file where the assembler may otherwise compress instructions. The
// instruction under test in each macro below is written this way.
.macro NORVC insn:vararg
   .option push
   .option norvc
   \insn
   .option pop
.endm

//---------------------------------------------------------------------------
// Function entry and exit.

.macro TEST_FUNC name
   .text
   .balign 4
   .globl test_\name
   .type test_\name, @function
test_\name:
   jal   t0, test_prologue
.endm

.macro TEST_END
   j     test_pass
.endm

//---------------------------------------------------------------------------
// Failure reporting.

// Report failure of case n: got is in register got, expected in register exp.
// Neither may be a1.
.macro FAIL_REG n, got, exp
   mv    a1, \got
   mv    a2, \exp
   li    a0, \n
   j     test_fail
.endm

// Report failure of case n with constant got/expected values. Used for
// control flow tests: e.g. a branch that wasn't taken (got 0, expected 1).
.macro FAIL_CONST n, got, exp
   li    a1, \got
   li    a2, \exp
   li    a0, \n
   j     test_fail
.endm

// Check that register reg holds the constant exp. tmp is overwritten, and
// can't be a1, a2 or reg.
.macro CHECK n, reg, exp, tmp=x31
   li    \tmp, \exp
   beq   \reg, \tmp, .Lcheck_ok\@
   FAIL_REG \n, \reg, \tmp
.Lcheck_ok\@:
.endm

// Check that register reg holds the same value as register exp.
.macro CHECK_REG n, reg, exp
   beq   \reg, \exp, .Lcheck_ok\@
   FAIL_REG \n, \reg, \exp
.Lcheck_ok\@:
.endm

//---------------------------------------------------------------------------
// Register-register ALU operations: inst rd, rs1, rs2

.macro TEST_RR n, inst, exp, a, b
   li    x11, \a
   li    x12, \b
   NORVC \inst x13, x11, x12
   CHECK \n, x13, \exp
.endm

// rd is the same register as rs1
.macro TEST_RR_RD_RS1 n, inst, exp, a, b
   li    x14, \a
   li    x15, \b
   NORVC \inst x14, x14, x15
   CHECK \n, x14, \exp
.endm

// rd is the same register as rs2
.macro TEST_RR_RD_RS2 n, inst, exp, a, b
   li    x16, \a
   li    x17, \b
   NORVC \inst x17, x16, x17
   CHECK \n, x17, \exp
.endm

// rd, rs1 and rs2 all the same register
.macro TEST_RR_ALL_SAME n, inst, exp, a
   li    x18, \a
   NORVC \inst x18, x18, x18
   CHECK \n, x18, \exp
.endm

// rs1 is x0
.macro TEST_RR_X0_RS1 n, inst, exp, b
   li    x19, \b
   NORVC \inst x20, x0, x19
   CHECK \n, x20, \exp
.endm

// rs2 is x0
.macro TEST_RR_X0_RS2 n, inst, exp, a
   li    x21, \a
   NORVC \inst x22, x21, x0
   CHECK \n, x22, \exp
.endm

// rd is x0: the result must be discarded
.macro TEST_RR_X0_RD n, inst, a, b
   li    x23, \a
   li    x24, \b
   NORVC \inst x0, x23, x24
   CHECK \n, x0, 0
.endm

// High numbered registers, and the source registers must be unchanged.
.macro TEST_RR_HIGH n, inst, exp, a, b
   li    x29, \a
   li    x30, \b
   NORVC \inst x28, x29, x30
   CHECK \n, x28, \exp
   CHECK \n, x29, \a
   CHECK \n, x30, \b
.endm

// Table driven: run inst on count pairs of operands from the table ops
// (.word rs1, rs2 for each case), checking each result against the table
// exp (one .word per case). Cases are numbered from first.
.macro TEST_RR_TABLE first, inst, ops, exp, count
   la    x8, \ops
   la    x9, \exp
   li    x10, \first
   li    x5, \count
.Ltbl_loop\@:
   lw    x11, 0(x8)
   lw    x12, 4(x8)
   lw    x31, 0(x9)
   NORVC \inst x13, x11, x12
   bne   x13, x31, .Ltbl_fail\@
   addi  x8, x8, 8
   addi  x9, x9, 4
   addi  x10, x10, 1
   addi  x5, x5, -1
   bnez  x5, .Ltbl_loop\@
   j     .Ltbl_done\@
.Ltbl_fail\@:
   mv    a1, x13
   mv    a2, x31
   j     test_fail            // a0 (x10) is the case number
.Ltbl_done\@:
.endm

//---------------------------------------------------------------------------
// Register-immediate ALU operations: inst rd, rs1, imm

.macro TEST_RI n, inst, exp, a, imm
   li    x11, \a
   NORVC \inst x13, x11, \imm
   CHECK \n, x13, \exp
.endm

.macro TEST_RI_RD_RS1 n, inst, exp, a, imm
   li    x14, \a
   NORVC \inst x14, x14, \imm
   CHECK \n, x14, \exp
.endm

.macro TEST_RI_X0_RS1 n, inst, exp, imm
   NORVC \inst x20, x0, \imm
   CHECK \n, x20, \exp
.endm

.macro TEST_RI_X0_RD n, inst, a, imm
   li    x23, \a
   NORVC \inst x0, x23, \imm
   CHECK \n, x0, 0
.endm

.macro TEST_RI_HIGH n, inst, exp, a, imm
   li    x29, \a
   NORVC \inst x28, x29, \imm
   CHECK \n, x28, \exp
   CHECK \n, x29, \a
.endm

// Table driven: run "inst rd, rs1, imm" with rs1 taken from each word of
// the table ops, checking against each word of the table exp.
.macro TEST_RI_TABLE first, inst, imm, ops, exp, count
   la    x8, \ops
   la    x9, \exp
   li    x10, \first
   li    x5, \count
.Ltbl_loop\@:
   lw    x11, 0(x8)
   lw    x31, 0(x9)
   NORVC \inst x13, x11, \imm
   bne   x13, x31, .Ltbl_fail\@
   addi  x8, x8, 4
   addi  x9, x9, 4
   addi  x10, x10, 1
   addi  x5, x5, -1
   bnez  x5, .Ltbl_loop\@
   j     .Ltbl_done\@
.Ltbl_fail\@:
   mv    a1, x13
   mv    a2, x31
   j     test_fail            // a0 (x10) is the case number
.Ltbl_done\@:
.endm

//---------------------------------------------------------------------------
// Conditional branches: inst rs1, rs2, target

// Branch must be taken, forwards.
.macro TEST_BR_TAKEN n, inst, a, b
   li    x11, \a
   li    x12, \b
   NORVC \inst x11, x12, .Lbr_ok\@
   FAIL_CONST \n, 0, 1
.Lbr_ok\@:
.endm

// Branch must be taken, backwards.
.macro TEST_BR_TAKEN_BACK n, inst, a, b
   li    x11, \a
   li    x12, \b
   j     .Lbr_go\@
.Lbr_back\@:
   j     .Lbr_ok\@
.Lbr_go\@:
   NORVC \inst x11, x12, .Lbr_back\@
   FAIL_CONST \n, 0, 1
.Lbr_ok\@:
.endm

// Branch must not be taken.
.macro TEST_BR_NOT_TAKEN n, inst, a, b
   li    x11, \a
   li    x12, \b
   NORVC \inst x11, x12, .Lbr_bad\@
   j     .Lbr_ok\@
.Lbr_bad\@:
   FAIL_CONST \n, 1, 0
.Lbr_ok\@:
.endm

// Both operands the same register; taken is 1 or 0.
.macro TEST_BR_SAME n, inst, taken, a
   li    x13, \a
   NORVC \inst x13, x13, .Lbr_dest\@
   .if \taken
   FAIL_CONST \n, 0, 1
   .else
   j     .Lbr_ok\@
   .endif
.Lbr_dest\@:
   .if !\taken
   FAIL_CONST \n, 1, 0
   .endif
.Lbr_ok\@:
.endm

// x0 as the first or second operand; taken is 1 or 0.
.macro TEST_BR_X0_RS1 n, inst, taken, b
   li    x14, \b
   NORVC \inst x0, x14, .Lbr_dest\@
   .if \taken
   FAIL_CONST \n, 0, 1
   .else
   j     .Lbr_ok\@
   .endif
.Lbr_dest\@:
   .if !\taken
   FAIL_CONST \n, 1, 0
   .endif
.Lbr_ok\@:
.endm

.macro TEST_BR_X0_RS2 n, inst, taken, a
   li    x15, \a
   NORVC \inst x15, x0, .Lbr_dest\@
   .if \taken
   FAIL_CONST \n, 0, 1
   .else
   j     .Lbr_ok\@
   .endif
.Lbr_dest\@:
   .if !\taken
   FAIL_CONST \n, 1, 0
   .endif
.Lbr_ok\@:
.endm

//---------------------------------------------------------------------------
// Loads and stores

// Load from base_label + offset.
.macro TEST_LD n, inst, exp, offset, base
   la    x11, \base
   NORVC \inst x13, \offset(x11)
   CHECK \n, x13, \exp
.endm

// Load with the destination the same as the base register.
.macro TEST_LD_RD_RS1 n, inst, exp, offset, base
   la    x14, \base
   NORVC \inst x14, \offset(x14)
   CHECK \n, x14, \exp
.endm

// Load into x0: the value must be discarded.
.macro TEST_LD_X0_RD n, inst, offset, base
   la    x15, \base
   NORVC \inst x0, \offset(x15)
   CHECK \n, x0, 0
.endm

// Fill the three words at st_buf with 0xAAAAAAAA, store val at offset
// from st_buf+4, then check the word at st_buf+4 is exp and the words
// either side are unchanged.
.macro TEST_ST n, inst, val, offset, exp
   la    x11, st_buf
   li    x12, 0xAAAAAAAA
   sw    x12, 0(x11)
   sw    x12, 4(x11)
   sw    x12, 8(x11)
   addi  x11, x11, 4
   li    x13, \val
   NORVC \inst x13, \offset(x11)
   lw    x14, -4(x11)
   CHECK \n, x14, 0xAAAAAAAA
   lw    x14, 0(x11)
   CHECK \n, x14, \exp
   lw    x14, 4(x11)
   CHECK \n, x14, 0xAAAAAAAA
.endm

#endif
