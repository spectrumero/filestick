#!/usr/bin/env python3
#
# Generates rv32i_alu.S and rv32m.S for the CPU test suite. The expected
# results are computed here from the RISC-V specification's definitions of
# each instruction, rather than worked out by hand.
#
# The generated files are checked in; run this again only if the test cases
# change:  python3 gen_alu.py

import os

M32 = 0xFFFFFFFF

def s32(x):
    x &= M32
    return x - (1 << 32) if x & 0x80000000 else x

def sext12(imm):
    return imm & M32

def trunc_div(a, b):
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q

# --- reference model, straight from the spec --------------------------------

def op_add(a, b):  return (a + b) & M32
def op_sub(a, b):  return (a - b) & M32
def op_sll(a, b):  return (a << (b & 31)) & M32
def op_slt(a, b):  return int(s32(a) < s32(b))
def op_sltu(a, b): return int((a & M32) < (b & M32))
def op_xor(a, b):  return (a ^ b) & M32
def op_srl(a, b):  return (a & M32) >> (b & 31)
def op_sra(a, b):  return (s32(a) >> (b & 31)) & M32
def op_or(a, b):   return (a | b) & M32
def op_and(a, b):  return (a & b) & M32

def op_mul(a, b):    return (a * b) & M32
def op_mulh(a, b):   return ((s32(a) * s32(b)) >> 32) & M32
def op_mulhsu(a, b): return ((s32(a) * (b & M32)) >> 32) & M32
def op_mulhu(a, b):  return (((a & M32) * (b & M32)) >> 32) & M32

def op_div(a, b):
    a, b = s32(a), s32(b)
    if b == 0:
        return M32
    if a == -(1 << 31) and b == -1:
        return a & M32
    return trunc_div(a, b) & M32

def op_divu(a, b):
    return M32 if b == 0 else (a // b) & M32

def op_rem(a, b):
    a, b = s32(a), s32(b)
    if b == 0:
        return a & M32
    if a == -(1 << 31) and b == -1:
        return 0
    return (a - b * trunc_div(a, b)) & M32

def op_remu(a, b):
    return a & M32 if b == 0 else (a % b) & M32

# --- test values -------------------------------------------------------------

EDGE = [0x00000000, 0x00000001, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000]
EXTRA = [
    (0x12345678, 0x9ABCDEF0),
    (0xAAAAAAAA, 0x55555555),
    (0xEDCBA987, 0x00000003),
    (0x00000007, 0xFFFFFFF9),
    (0x0000FFFF, 0xFFFF0000),
    (0x80000001, 0xFFFFFFFE),
]
ARITH_PAIRS = [(a, b) for a in EDGE for b in EDGE] + EXTRA

SHIFT_A = [0x80000001, 0x12345678, 0xFFFFFFFF]
SHIFT_B = [0, 1, 7, 31, 32, 33, 0xFFFFFFE1, 0x00000010]
SHIFT_PAIRS = [(a, b) for a in SHIFT_A for b in SHIFT_B]

IMM_A = [0x00000000, 0x00000001, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000,
         0x12345678]
IMMS = [0, 1, -1, 2047, -2048, 0x555, -0x556]
IMM_PAIRS = [(a, i) for a in IMM_A for i in IMMS]

SHAMT_A = [0x80000001, 0x12345678, 0xFFFFFFFF]
SHAMTS = [0, 1, 4, 15, 16, 31]
SHAMT_PAIRS = [(a, s) for a in SHAMT_A for s in SHAMTS]

DIV_PAIRS = ARITH_PAIRS + [
    (7, 2), (s32(-7) & M32, 2), (7, s32(-2) & M32),
    (s32(-7) & M32, s32(-2) & M32), (0x80000000, 2), (100, 7),
    (0xFFFFFF9C, 7), (0x12345678, 0), (0x87654321, 0),
    (0x80000000, 0xFFFFFFFF), (0xFFFFFFFF, 0x80000000),
    (0x12345678, 0x00001234), (0xFEDCBA98, 0x00000010),
]

MUL_PAIRS = ARITH_PAIRS + [
    (2, 0x80000000), (0x80000000, 2), (0xFFFFFFFF, 2), (2, 0xFFFFFFFF),
    (0x00010000, 0x00010000), (0xFFFF0000, 0x0000FFFF),
    (0x89ABCDEF, 0x01234567), (0x01234567, 0x89ABCDEF),
]

def h(x):
    return "0x%08X" % (x & M32)

# --- emit --------------------------------------------------------------------

# Operand tables, shared by all the instructions that use them.
def operand_table(label, pairs):
    out = ["   .balign 4", "%s:" % label]
    for n, (a, b) in enumerate(pairs, 1):
        out.append("   .word %s, %s   // %d" % (h(a), h(b), n))
    return out

def word_table(label, words):
    out = ["   .balign 4", "%s:" % label]
    for w in words:
        out.append("   .word %s" % h(w))
    return out

def rr_test(name, fn, pairs, ops_label, data):
    out = ["TEST_FUNC %s" % name]
    exp_label = "exp_%s" % name
    out.append("   TEST_RR_TABLE       1, %-6s, %s, %s, %d"
               % (name, ops_label, exp_label, len(pairs)))
    data += ["   .balign 4", "%s:" % exp_label]
    for n, (a, b) in enumerate(pairs, 1):
        data.append("   .word %s   // %d: %s, %s" % (h(fn(a, b)), n, h(a), h(b)))
    n = len(pairs) + 1
    # register usage variants
    a, b = 0x12345678, 0x00000013
    out.append("   TEST_RR_RD_RS1    %3d, %-6s, %s, %s, %s" % (n, name, h(fn(a, b)), h(a), h(b))); n += 1
    out.append("   TEST_RR_RD_RS2    %3d, %-6s, %s, %s, %s" % (n, name, h(fn(a, b)), h(a), h(b))); n += 1
    out.append("   TEST_RR_ALL_SAME  %3d, %-6s, %s, %s" % (n, name, h(fn(a, a)), h(a))); n += 1
    out.append("   TEST_RR_X0_RS1    %3d, %-6s, %s, %s" % (n, name, h(fn(0, b)), h(b))); n += 1
    out.append("   TEST_RR_X0_RS2    %3d, %-6s, %s, %s" % (n, name, h(fn(a, 0)), h(a))); n += 1
    out.append("   TEST_RR_X0_RD     %3d, %-6s, %s, %s" % (n, name, h(a), h(b))); n += 1
    a, b = 0x87654321, 0x00000005
    out.append("   TEST_RR_HIGH      %3d, %-6s, %s, %s, %s" % (n, name, h(fn(a, b)), h(a), h(b))); n += 1
    out.append("TEST_END")
    out.append("")
    return out

def ri_test(name, fn, avals, ops_label, imms, data, is_shift=False):
    out = ["TEST_FUNC %s" % name]
    n = 1
    for imm in imms:
        b = imm if is_shift else sext12(imm)
        exp_label = "exp_%s_%s" % (name, ("%d" % imm).replace("-", "m"))
        out.append("   TEST_RI_TABLE     %3d, %-6s, %5d, %s, %s, %d"
                   % (n, name, imm, ops_label, exp_label, len(avals)))
        data += ["   .balign 4", "%s:" % exp_label]
        for a in avals:
            data.append("   .word %s   // %d: %s, %d" % (h(fn(a, b)), n, h(a), imm))
            n += 1
    a = 0x12345678
    imm = 5 if is_shift else -3
    b = imm if is_shift else sext12(imm)
    out.append("   TEST_RI_RD_RS1    %3d, %-6s, %s, %s, %d" % (n, name, h(fn(a, b)), h(a), imm)); n += 1
    out.append("   TEST_RI_X0_RS1    %3d, %-6s, %s, %d" % (n, name, h(fn(0, b)), imm)); n += 1
    out.append("   TEST_RI_X0_RD     %3d, %-6s, %s, %d" % (n, name, h(a), imm)); n += 1
    a = 0x87654321
    out.append("   TEST_RI_HIGH      %3d, %-6s, %s, %s, %d" % (n, name, h(fn(a, b)), h(a), imm)); n += 1
    out.append("TEST_END")
    out.append("")
    return out

HEADER = """\
/*
 * CPU test suite - %s
 *
 * GENERATED by gen_alu.py, do not edit. Expected values are computed from
 * the RISC-V specification's definition of each instruction.
 *
 * Most cases are table driven: the comment by each expected value gives the
 * case number and the operands.
 */
#include "cputest.h"

// The instruction under test is always assembled at full size (see NORVC in
// cputest.h); the code around it may be compressed.
.option rvc

"""

def write(fname, desc, body, data):
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), fname)
    with open(path, "w") as f:
        f.write(HEADER % desc)
        f.write("\n".join(body))
        f.write("\n.section .rodata\n")
        f.write("\n".join(data))
        f.write("\n")

data = []
data += operand_table("ops_arith", ARITH_PAIRS)
data += operand_table("ops_shift", SHIFT_PAIRS)
data += word_table("ops_imm", IMM_A)
data += word_table("ops_shamt", SHAMT_A)
alu = []
alu += rr_test("add",  op_add,  ARITH_PAIRS, "ops_arith", data)
alu += rr_test("sub",  op_sub,  ARITH_PAIRS, "ops_arith", data)
alu += rr_test("sll",  op_sll,  SHIFT_PAIRS, "ops_shift", data)
alu += rr_test("slt",  op_slt,  ARITH_PAIRS, "ops_arith", data)
alu += rr_test("sltu", op_sltu, ARITH_PAIRS, "ops_arith", data)
alu += rr_test("xor",  op_xor,  ARITH_PAIRS, "ops_arith", data)
alu += rr_test("srl",  op_srl,  SHIFT_PAIRS, "ops_shift", data)
alu += rr_test("sra",  op_sra,  SHIFT_PAIRS, "ops_shift", data)
alu += rr_test("or",   op_or,   ARITH_PAIRS, "ops_arith", data)
alu += rr_test("and",  op_and,  ARITH_PAIRS, "ops_arith", data)
alu += ri_test("addi",  op_add,  IMM_A, "ops_imm", IMMS, data)
alu += ri_test("slti",  op_slt,  IMM_A, "ops_imm", IMMS, data)
alu += ri_test("sltiu", op_sltu, IMM_A, "ops_imm", IMMS, data)
alu += ri_test("xori",  op_xor,  IMM_A, "ops_imm", IMMS, data)
alu += ri_test("ori",   op_or,   IMM_A, "ops_imm", IMMS, data)
alu += ri_test("andi",  op_and,  IMM_A, "ops_imm", IMMS, data)
alu += ri_test("slli",  op_sll,  SHAMT_A, "ops_shamt", SHAMTS, data, True)
alu += ri_test("srli",  op_srl,  SHAMT_A, "ops_shamt", SHAMTS, data, True)
alu += ri_test("srai",  op_sra,  SHAMT_A, "ops_shamt", SHAMTS, data, True)
write("rv32i_alu.S", "RV32I integer computational instructions", alu, data)

data = []
data += operand_table("ops_mul", MUL_PAIRS)
data += operand_table("ops_div", DIV_PAIRS)
m = []
m += rr_test("mul",    op_mul,    MUL_PAIRS, "ops_mul", data)
m += rr_test("mulh",   op_mulh,   MUL_PAIRS, "ops_mul", data)
m += rr_test("mulhsu", op_mulhsu, MUL_PAIRS, "ops_mul", data)
m += rr_test("mulhu",  op_mulhu,  MUL_PAIRS, "ops_mul", data)
m += rr_test("div",    op_div,    DIV_PAIRS, "ops_div", data)
m += rr_test("divu",   op_divu,   DIV_PAIRS, "ops_div", data)
m += rr_test("rem",    op_rem,    DIV_PAIRS, "ops_div", data)
m += rr_test("remu",   op_remu,   DIV_PAIRS, "ops_div", data)
write("rv32m.S", "RV32M multiply and divide instructions", m, data)
