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
   REG_A0 = 0,
   REG_A1,
   REG_A2,
   REG_A3,
   REG_A4,
   REG_A5,
   REG_A6,
   REG_A7,
   REG_S0,
   REG_S1,
   REG_S2,
   REG_S3,
   REG_S4,
   REG_S5,
   REG_S6,
   REG_S7,
   REG_S8,
   REG_S9,
   REG_S10,
   REG_S11,
   REG_T0,
   REG_T1,
   REG_T2,
   REG_T3,
   REG_T4,
   REG_T5,
   REG_T6,
   REG_GP,
   REG_TP,
   REG_RA,
   REG_SP,
   REG_SEPC
};

void dump_registers(uint32_t *registers);
void dump_reg(char *regname, uint32_t regval);

#endif
