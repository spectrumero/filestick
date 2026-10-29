/*
;The MIT License
;
;Copyright (c) 2026 Dylan Smith
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


// Startup, the command line, error handling and program editing.

#ifndef __riscv
#define _GNU_SOURCE
#endif
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "basic.h"

#ifndef __riscv
#include <sys/mman.h>
#endif

u8 *tp, *curline;
u8 *page, *top, *lomem, *vartop, *himem, *ws_start, *ws_end;
i32 resint[27];
bool trace_on;
u32 trace_max;
u8 errnum;
u32 errline;
void *onerr;
static u8 *onerr_line;
u8 *dataptr, *datastart;
int procdepth;

static jmp_buf jb;
static int errcode;
static u32 auto_line, auto_step;
static u8 old_byte;
static u8 ibuf[LINEMAX], tbuf[LINEMAX];
static void (*entry)(void);

// Error number then message for each error
static const char errtab[] =
   "\x00" "No room\0"         "\x00" "STOP\0"
   "\x00" "Silly\0"           "\x00" "Bad program\0"
   "\x01" "Out of range\0"    "\x02" "Byte\0"
   "\x04" "Mistake\0"         "\x05" "Missing ,\0"
   "\x06" "Type mismatch\0"   "\x07" "No FN\0"
   "\x08" "$ range\0"         "\x09" "Missing \"\0"
   "\x0A" "Bad DIM\0"         "\x0B" "DIM space\0"
   "\x0C" "Not LOCAL\0"       "\x0D" "No PROC\0"
   "\x0E" "Array\0"           "\x0F" "Subscript\0"
   "\x10" "Syntax error\0"    "\x11" "Escape\0"
   "\x12" "Division by zero\0" "\x13" "String too long\0"
   "\x14" "Too big\0"         "\x15" "-ve root\0"
   "\x16" "Log range\0"       "\x17" "Accuracy lost\0"
   "\x18" "Exp range\0"       "\x1A" "No such variable\0"
   "\x1B" "Missing )\0"       "\x1C" "Bad HEX\0"
   "\x1D" "No such FN/PROC\0" "\x1F" "Arguments\0"
   "\x20" "No FOR\0"          "\x21" "Can't match FOR\0"
   "\x22" "FOR variable\0"    "\x23" "Too many FORs\0"
   "\x24" "No TO\0"           "\x25" "Too many GOSUBs\0"
   "\x26" "No GOSUB\0"        "\x27" "ON syntax\0"
   "\x28" "ON range\0"        "\x29" "No such line\0"
   "\x2A" "Out of DATA\0"     "\x2B" "No REPEAT\0"
   "\x2C" "Too many REPEATs\0" "\x2D" "Missing #\0"
   "\xD6" "File not found\0"  "\xDE" "Channel\0"
   "\xDF" "EOF\0"             "\xFE" "Bad command\0"
   "\xFF" "File error\0";

// Pointer to an error's number, followed by its message
const u8 *errmsg_ptr(int e)
{
   const char *p = errtab;
   while(e--) p += strlen(p + 1) + 2;
   return (const u8 *)p;
}

__attribute__((noreturn)) void err(int e)
{
   errcode = e;
   errnum = *errmsg_ptr(e);
   errline = curline ? line_num(curline) : 0;
   longjmp(jb, 1);
}

// Stop running and return to the prompt
__attribute__((noreturn)) void end_run(void)
{
   longjmp(jb, 2);
}

// Continue running at tp with an empty stack (after HIMEM= or RUN)
__attribute__((noreturn)) void himem_restart(void)
{
   longjmp(jb, 3);
}

// Highest address variables can use. The stack must stay this far above
// them so there's room for the deepest call chain between checks
// (allowing for EVAL's line buffer).
u8 *mem_limit(void)
{
#ifdef __riscv
   return getsp() - 1024;
#else
   return himem - 256;
#endif
}

void check_room(void *p)
{
   if((u8 *)p > mem_limit()) err(E_NOROOM);
}

//------------------------------------------------------------------
// Program lines
u32 line_num(const u8 *p)
{
   return (p[1] << 8) | p[2];
}

// First line numbered n or more
u8 *find_line(u32 n)
{
   u8 *p = page;
   while(line_num(p) < n) p += p[3];
   return p;
}

void jump_line(u32 n)
{
   u8 *p = find_line(n);
   if(line_num(p) != n) err(E_NOLINE);
   tp = curline = p;
}

u8 *skip_line_end(u8 *p)
{
   while(*p != 0x0D) p++;
   return p;
}

static void program_changed(void)
{
   lomem = (u8 *)(((uintptr_t)top + sizeof(void *) - 1) & ~(sizeof(void *) - 1));
   clear_vars();
}

void new_program(void)
{
   old_byte = page[1];
   page[0] = 0x0D;
   page[1] = 0xFF;
   top = page + 2;
   program_changed();
}

// Check the program at PAGE and find TOP
static bool check_program(void)
{
   u8 *p = page;
   while(p < himem - 4 && *p == 0x0D) {
      if(p[1] & 0x80) {
         top = p + 2;
         program_changed();
         return true;
      }
      if(p[3] < 4) break;
      p += p[3];
   }
   return false;
}

bool old_program(void)
{
   page[1] = old_byte;
   if(check_program()) return true;
   new_program();
   return false;
}

// Insert (or delete if text is empty) a program line
static void insert_line(u32 n, const u8 *text, int len)
{
   u8 *p = find_line(n);
   if(line_num(p) == n) {
      int l = p[3];
      memmove(p, p + l, top - p - l);
      top -= l;
   }
   int i = 0;
   while(text[i] == ' ') i++;
   if(text[i] != 0x0D) {
      int l = len + 3;
      if(l > 255) err(E_STRLONG);
      if(top + l + 1024 > mem_limit()) err(E_NOROOM);
      memmove(p + l, p, top - p);
      p[0] = 0x0D;
      p[1] = n >> 8;
      p[2] = n;
      p[3] = l;
      memcpy(p + 4, text, len - 1);
      top += l;
   }
   program_changed();
}

// Parse a line number at s, returns -1 if there isn't one
static i32 parse_lineno(const u8 **s)
{
   const u8 *p = *s;
   i32 n = 0;
   if(*p < '0' || *p > '9') return -1;
   while(*p >= '0' && *p <= '9') {
      n = n * 10 + *p++ - '0';
      if(n > 32767) err(E_SILLY);
   }
   *s = p;
   return n;
}

// Enter a line typed (or loaded from a text file) into the program.
// Returns false if it has no line number.
static bool enter_line(const u8 *s)
{
   while(*s == ' ') s++;
   i32 n = parse_lineno(&s);
   if(n < 0) return false;
   insert_line(n, tbuf, tokenise(s, tbuf, false));
   return true;
}

//------------------------------------------------------------------
// LOAD a program: either tokenised, or text with line numbers
void load_program(const char *name)
{
   int fd = open(name, O_RDONLY);
   if(fd < 0) err(E_NOTFOUND);
   new_program();
   u32 room = mem_limit() - page - 1024;
   int n = read(fd, page, 1);
   if(n == 1 && *page == 0x0D) {
      n = read(fd, page + 1, room);
      close(fd);
      if(n > 0 && check_program()) return;
      new_program();
      err(E_BADPROG);
   }

   // text: read a line at a time into ibuf
   u8 rb[256];
   int rpos = 0, rlen = 0, c = *page;
   u32 last = 0;
   new_program();
   while(c >= 0) {
      int len = 0;
      while(c >= 0 && c != '\n' && c != '\r') {
         if(len < LINEMAX - 16) ibuf[len++] = c;
         if(rpos == rlen) {
            rlen = read(fd, rb, sizeof(rb));
            rpos = 0;
         }
         c = rlen > 0 ? rb[rpos++] : -1;
      }
      ibuf[len] = 0;
      if(len) {
         const u8 *s = ibuf;
         while(*s == ' ') s++;
         if(*s >= '0' && *s <= '9')
            last = parse_lineno(&s);
         else
            last += 10;
         insert_line(last, tbuf, tokenise(s, tbuf, false));
      }
      if(c >= 0) {
         if(rpos == rlen) {
            rlen = read(fd, rb, sizeof(rb));
            rpos = 0;
         }
         c = rlen > 0 ? rb[rpos++] : -1;
      }
   }
   close(fd);
}

void save_program(const char *name)
{
   int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
   if(fd < 0) err(E_FILE);
   int n = write(fd, page, top - page);
   close(fd);
   if(n != top - page) err(E_FILE);
}

void delete_lines(u32 a, u32 b)
{
   u8 *p = find_line(a), *q = find_line(b);
   if(line_num(q) == b) q += q[3];
   memmove(p, q, top - q);
   top -= q - p;
   program_changed();
}

// Line references are fixed first, then the lines renumbered
void renumber(u32 start, u32 step)
{
   u32 n = start;
   for(u8 *p = page; !(p[1] & 0x80); p += p[3], n += step)
      if(n > 32767) err(E_SILLY);
   for(u8 *p = page; !(p[1] & 0x80); p += p[3]) {
      bool q = false;
      for(u8 *s = p + 4; *s != 0x0D; s++) {
         if(*s == '"') q = !q;
         if(q || *s != TK_LINENO) continue;
         u32 old = decode_lineno(s + 1), idx = 0;
         u8 *l;
         for(l = page; !(l[1] & 0x80) && line_num(l) != old; l += l[3]) idx++;
         if(!(l[1] & 0x80)) encode_lineno(s, start + idx * step);
         s += 3;
      }
   }
   n = start;
   for(u8 *p = page; !(p[1] & 0x80); p += p[3], n += step) {
      p[1] = n >> 8;
      p[2] = n;
   }
}

void set_auto(u32 start, u32 step)
{
   auto_line = start;
   auto_step = step;
}

//------------------------------------------------------------------
void run_program(void)
{
   program_changed();
   reset_stacks();
   onerr = NULL;
   datastart = page;
   tp = curline = page;
   himem_restart();
}

static void exec_check(void)
{
   int r = exec();
   if(r != R_EOL) err(r == R_ENDPROC ? E_NOPROC : r == R_FNRET ? E_NOFN : E_NOTLOCAL);
}

// Run from tp, used after RUN, errors trapped by ON ERROR and HIMEM=
static void resume(void)
{
   exec_check();
   end_run();
}

static void immediate(void)
{
   for(;;) {
      curline = NULL;
      procdepth = 0;
      reset_stacks();
      int n = 0;
      if(auto_line) {
         if(auto_line > 32767) {
            auto_line = 0;
            continue;
         }
         n = fmt_int((char *)ibuf, auto_line, false);
         out_str((char *)ibuf, n);
         ibuf[n++] = ' ';
         vdu(' ');
         auto_line += auto_step;
      } else
         vdu('>');
      read_line((char *)ibuf + n, LINEMAX - 16 - n);
      if(enter_line(ibuf)) continue;
      tokenise(ibuf, tbuf, false);
      tp = tbuf;
      exec_check();
   }
}

void report(void)
{
   out_cstr((const char *)errmsg_ptr(errcode) + 1);
}

// Errors arrive here back on the original stack
static void handle_error(void)
{
   procdepth = 0;
   auto_line = 0;
   if(onerr && curline) {
      tp = onerr;
      curline = onerr_line;
      reset_stacks();
      entry = resume;
      return;
   }
   if(pos) newline();
   report();
   if(errline) {
      char num[12];
      out_cstr(" at line ");
      out_str(num, fmt_int(num, errline, false));
   }
   newline();
   entry = immediate;
}

void set_onerr(u8 *p)
{
   onerr = curline ? p : NULL;
   onerr_line = curline;
}

int main(int argc, char **argv)
{
   u32 size;
   u8 *ws;
#ifdef __riscv
   for(size = 0x14000; size >= 0x2000; size -= 256)
      if((ws = malloc(size))) break;
#else
   size = 0x8000;
   ws = mmap(NULL, size, PROT_READ | PROT_WRITE,
         MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
#endif
   ws_start = (u8 *)(((uintptr_t)ws + 3) & ~3);
   ws_end = (u8 *)(((uintptr_t)ws + size) & ~15);
   himem = ws_end;
   page = ws_start;
   resint[0] = 0x90A;
   console_init();
   new_program();

   char num[12];
   out_cstr("BBC BASIC for FileStick\r\n\r\n");
   out_str(num, fmt_int(num, himem - top, false));
   out_cstr(" bytes free\r\n\r\n");

   entry = immediate;
   if(argc > 1) {
      // run the program named on the command line
      strcpy((char *)ibuf, "CHAIN \"");
      strncat((char *)ibuf, argv[1], LINEMAX - 16);
      strcat((char *)ibuf, "\"");
      tokenise(ibuf, tbuf, false);
      tp = tbuf;
      entry = resume;
   }

   switch(setjmp(jb)) {
      case 1: handle_error(); break;
      case 2: entry = immediate; break;
      case 3: entry = resume; break;
   }
   run_on_stack(entry, himem);
   return 0;
}
