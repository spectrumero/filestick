#ifndef REGDUMP_H
#define REGDUMP_H
/*
;The MIT License
;
;Copyright (c) 2023 Dylan Smith
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;THE SOFTWARE.
*/

enum Registerfile {
   REG_A0 = 0,       // 0
   REG_A1,           // 4
   REG_A2,           // 8
   REG_A3,           // 12
   REG_A4,           // 16
   REG_A5,           // 20
   REG_A6,           // 24
   REG_A7,           // 28
   REG_S0,           // 32
   REG_S1,           // 36
   REG_S2,           // 40
   REG_S3,           // 44
   REG_S4,           // 48
   REG_S5,           // 52
   REG_S6,           // 56
   REG_S7,           // 60
   REG_S8,           // 64
   REG_S9,           // 68
   REG_S10,          // 72
   REG_S11,          // 76
   REG_T0,           // 80
   REG_T1,           // 84
   REG_T2,           // 88
   REG_T3,           // 92
   REG_T4,           // 96
   REG_T5,           // 100
   REG_T6,           // 104
   REG_GP,           // 108
   REG_TP,           // 112
   REG_RA,           // 116
   REG_SP,           // 120
   REG_SEPC          // 124
};

void dump_registers(uint32_t *registers);
void dump_reg(char *regname, uint32_t regval);

#endif
