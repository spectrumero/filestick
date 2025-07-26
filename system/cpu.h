#ifndef CPU_H
#define CPU_H
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

// CPU specifics.

// Custom CSRs for our RISC-V core.

// Custom supervisor level read only CSRs
// The following CSR addresses allow reading of the normal bank of CPU
// registers when using the interrupt bank.
#define CSR_R_BASE   0xDC0
#define CSR_ZERO     CSR_R_BASE + 0
#define CSR_RA       CSR_R_BASE + 1
#define CSR_SP       CSR_R_BASE + 2    
#define CSR_GP       CSR_R_BASE + 3    
#define CSR_TP       CSR_R_BASE + 4    
#define CSR_T0       CSR_R_BASE + 5    
#define CSR_T1       CSR_R_BASE + 6    
#define CSR_T2       CSR_R_BASE + 7    
#define CSR_S0       CSR_R_BASE + 8    
#define CSR_S1       CSR_R_BASE + 9    
#define CSR_A0       CSR_R_BASE + 10   
#define CSR_A1       CSR_R_BASE + 11   
#define CSR_A2       CSR_R_BASE + 12   
#define CSR_A3       CSR_R_BASE + 13   
#define CSR_A4       CSR_R_BASE + 14   
#define CSR_A5       CSR_R_BASE + 15   
#define CSR_A6       CSR_R_BASE + 16   
#define CSR_A7       CSR_R_BASE + 17   
#define CSR_S2       CSR_R_BASE + 18   
#define CSR_S3       CSR_R_BASE + 19   
#define CSR_S4       CSR_R_BASE + 20   
#define CSR_S5       CSR_R_BASE + 21   
#define CSR_S6       CSR_R_BASE + 22   
#define CSR_S7       CSR_R_BASE + 23   
#define CSR_S8       CSR_R_BASE + 24   
#define CSR_S9       CSR_R_BASE + 25   
#define CSR_S10      CSR_R_BASE + 26   
#define CSR_S11      CSR_R_BASE + 27   
#define CSR_T3       CSR_R_BASE + 28   
#define CSR_T4       CSR_R_BASE + 29   
#define CSR_T5       CSR_R_BASE + 30   
#define CSR_T6       CSR_R_BASE + 31   

#define CSR_REGBANK  0x5c0
#define CSR_PRIVMODE 0x5c1

#endif

