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

#include <unistd.h>
#include <console.h>
#include <string.h>

#include "fd.h"
#include "regdump.h"

void dump_registers(uint32_t *registers) {
   dump_reg("sp", registers[REG_SP]);
   dump_reg("ra", registers[REG_RA]);
   dump_reg("gp", registers[REG_GP]);
   dump_reg("tp", registers[REG_TP]);
   dump_reg("sepc", registers[REG_SEPC]);
   SYS_write(1, "\r\n", 2);
}

void dump_reg(char *regname, uint32_t regval) {
   SYS_write(1, regname, strlen(regname));
   SYS_write(1, ": ", 2);

   console_hexword(regval);
   SYS_write(1, "  ", 2);
}
