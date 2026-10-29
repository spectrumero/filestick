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


// Console, files and operating system commands.

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "basic.h"

#ifdef __riscv
#include <filestick.h>
#include <sys/ioctl.h>
#include <sys/console.h>
#include <sys/dirent.h>
#define CYC_PER_CS   99375       // 9.9375MHz
#else
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <dirent.h>
#define CYC_PER_CS   100000
static bool stdin_eof;
static struct termios saved_tios;
static bool tios_saved;
#endif

int pos, vpos, width;
static char obuf[128];
static int olen;

// Keyboard buffer, for keys read while checking for Escape
static u8 kbuf[32];
static u8 khead, ktail;

static u32 lastcyc, cycrem, ticks;
static i32 time_base;

//------------------------------------------------------------------
void flush_out(void)
{
   if(olen) write(1, obuf, olen);
   olen = 0;
}

static void outc(u8 c)
{
   obuf[olen++] = c;
   if(olen == sizeof(obuf)) flush_out();
}

static void outs(const char *s)
{
   while(*s) outc(*s++);
}

static void outnum(int n)
{
   char b[12];
   int l = fmt_int(b, n, false);
   for(int i = 0; i < l; i++) outc(b[i]);
}

// Number of parameter bytes for each VDU code
static const u8 vdu_args[32] = {
   0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,1,2,5,0,0,1,9, 8,5,0,0,4,4,0,2 };
static u8 vq[10], vqn, vqlen;

// VDU codes are translated to ANSI terminal sequences where there's an
// equivalent.
void vdu(int c)
{
   c &= 255;
   if(vqlen) {
      vq[vqn++] = c;
      if(vqn <= vqlen) return;
      vqlen = 0;
      switch(vq[0]) {
         case 17:
            outs("\x1b[");
            outnum((vq[1] & 128 ? 40 : 30) + (vq[1] & 7));
            outc('m');
            break;
         case 22:
            vdu(12);
            break;
         case 31:
            outs("\x1b[");
            outnum(vq[2] + 1);
            outc(';');
            outnum(vq[1] + 1);
            outc('H');
            pos = vq[1];
            vpos = vq[2];
            break;
      }
      return;
   }
   if(c >= 32) {
      if(c == 127) {
         outs("\b \b");
         if(pos) pos--;
         return;
      }
      if(width && pos >= width) newline();
      outc(c);
      pos++;
      return;
   }
   if(vdu_args[c]) {
      vq[0] = c;
      vqn = 1;
      vqlen = vdu_args[c];
      return;
   }
   switch(c) {
      case 7:
      case 27:
         outc(c);
         break;
      case 8:
         if(pos) pos--;
         outc(8);
         break;
      case 9:
         outs("\x1b[C");
         pos++;
         break;
      case 10:
         outc(10);
         vpos++;
         break;
      case 11:
         outs("\x1b[A");
         if(vpos) vpos--;
         break;
      case 12:
         outs("\x1b[2J");
         // fall through
      case 30:
         outs("\x1b[H");
         pos = vpos = 0;
         break;
      case 13:
         outc(13);
         pos = 0;
         break;
   }
}

void out_str(const char *s, int len)
{
   while(len--) vdu(*s++);
}

void out_cstr(const char *s)
{
   while(*s) vdu(*s++);
}

void newline(void)
{
   vdu(13);
   vdu(10);
}

//------------------------------------------------------------------
// Time, in centiseconds
u32 cycles(void)
{
#ifdef __riscv
   u32 c;
   asm volatile(".option push\n.option arch, +zicsr\ncsrr %0, cycle\n.option pop" : "=r"(c));
   return c;
#else
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 10000000ULL + ts.tv_nsec / 100;
#endif
}

u32 get_time(void)
{
   u32 c = cycles();
   cycrem += c - lastcyc;
   lastcyc = c;
   u32 n = cycrem / CYC_PER_CS;
   ticks += n;
   cycrem -= n * CYC_PER_CS;
   return ticks - time_base;
}

void set_time(i32 t)
{
   get_time();
   time_base = ticks - t;
}

//------------------------------------------------------------------
// Keyboard
static int key_avail(void)
{
#ifdef __riscv
   return fd_peek(0);
#else
   struct pollfd p = { 0, POLLIN, 0 };
   return !stdin_eof && poll(&p, 1, 0) > 0;
#endif
}

void poll_escape(void)
{
   get_time();
   while((u8)(ktail - khead) < sizeof(kbuf) && key_avail() > 0) {
      u8 c;
      if(read(0, &c, 1) != 1) {
#ifndef __riscv
         stdin_eof = true;
#endif
         break;
      }
      if(c == 27) {
         khead = ktail;
         err(E_ESCAPE);
      }
      kbuf[ktail++ & 31] = c;
   }
}

void flush_keys(void)
{
   khead = ktail;
}

// Wait up to cs centiseconds for a key (forever if cs < 0)
int getkey(i32 cs)
{
   flush_out();
   u32 start = get_time();
   for(;;) {
      poll_escape();
      if(khead != ktail) return kbuf[khead++ & 31];
#ifndef __riscv
      if(stdin_eof) {
         console_restore();
         exit(0);
      }
      usleep(1000);
#endif
      if(cs >= 0 && (i32)(get_time() - start) >= cs) return -1;
   }
}

// Read a line with simple editing. Returns the length.
int read_line(char *buf, int max)
{
   static bool last_cr;
   int n = 0;
   for(;;) {
      int c = getkey(-1);
      if(c == 10 && last_cr) {
         last_cr = false;
         continue;
      }
      last_cr = c == 13;
      if(c == 13 || c == 10) break;
      if(c == 8 || c == 127 || c == 21) {
         do {
            if(!n) break;
            n--;
            vdu(8); vdu(' '); vdu(8);
         } while(c == 21);
      } else if(c >= 32 && n < max) {
         buf[n++] = c;
         vdu(c);
      }
   }
   newline();
   buf[n] = 0;
   return n;
}

void console_init(void)
{
#ifdef __riscv
   ioctl(0, CONSOLE_SET_RAW);
#else
   if(isatty(0) && tcgetattr(0, &saved_tios) == 0) {
      struct termios t = saved_tios;
      t.c_lflag &= ~(ICANON | ECHO | ISIG);
      t.c_iflag &= ~(ICRNL | IXON);
      tcsetattr(0, TCSANOW, &t);
      tios_saved = true;
   }
#endif
   lastcyc = cycles();
}

void console_restore(void)
{
   flush_out();
#ifdef __riscv
   ioctl(0, CONSOLE_SET_INTERACTIVE);
#else
   if(tios_saved) tcsetattr(0, TCSANOW, &saved_tios);
#endif
}

//------------------------------------------------------------------
// Files. Channels are file descriptors.
static u32 open_mask;

i32 open_file(int tok)
{
   sbuf[slen] = 0;
   int flags = tok == TK_OPENIN ? O_RDONLY :
               tok == TK_OPENUP ? O_RDWR : O_WRONLY | O_CREAT | O_TRUNC;
   int fd = open(sbuf, flags, 0666);
   if(fd < 0) return 0;
   if(fd < 3 || fd > 31) {
      close(fd);
      return 0;
   }
   open_mask |= 1u << fd;
   return fd;
}

i32 channel(void)
{
   if(skipsp() != '#') err(E_HASH);
   tp++;
   return to_int(factor());
}

static void check_ch(i32 ch)
{
   if(ch < 0 || ch > 31 || !(open_mask & (1u << ch))) err(E_CHANNEL);
}

void file_close(i32 ch)
{
   if(!ch) {
      for(ch = 3; ch < 32; ch++)
         if(open_mask & (1u << ch)) close(ch);
      open_mask = 0;
      return;
   }
   check_ch(ch);
   close(ch);
   open_mask &= ~(1u << ch);
}

static void file_read(i32 ch, void *buf, int n)
{
   if(read(ch, buf, n) != n) err(E_EOF);
}

static void file_write(i32 ch, const void *buf, int n)
{
   if(write(ch, buf, n) != n) err(E_FILE);
}

i32 file_func(u8 tok, i32 ch)
{
   check_ch(ch);
   if(tok == TK_BGET) {
      u8 c;
      return read(ch, &c, 1) == 1 ? c : -1;
   }
   // the extent is found with lseek, as fstat doesn't return the size
   i32 ptr = lseek(ch, 0, SEEK_CUR);
   if(tok == TK_PTR) return ptr;
   i32 ext = lseek(ch, 0, SEEK_END);
   lseek(ch, ptr, SEEK_SET);
   if(tok == TK_EXT) return ext;
   return ptr >= ext ? -1 : 0;
}

void file_bput(i32 ch, i32 v)
{
   check_ch(ch);
   u8 c = v;
   file_write(ch, &c, 1);
}

void file_setptr(i32 ch, i32 v)
{
   check_ch(ch);
   lseek(ch, v, SEEK_SET);
}

// PRINT# writes values in the BBC Micro's format
void print_file(void)
{
   i32 ch = channel();
   check_ch(ch);
   while(skipsp() == ',') {
      tp++;
      u8 b[MAXSTR + 2];
      int n, t = expr();
      if(t == T_INT) {
         b[0] = 0x40;
         for(int i = 0; i < 4; i++) b[i + 1] = acc_i >> (24 - i * 8);
         n = 5;
      } else if(t == T_REAL) {
         b[0] = 0xFF;
         f_to_bbc(acc_f, b + 1);
         n = 6;
      } else {
         b[0] = 0;
         b[1] = slen;
         for(int i = 0; i < slen; i++) b[i + 2] = sbuf[slen - 1 - i];
         n = slen + 2;
      }
      file_write(ch, b, n);
   }
}

void input_file(void)
{
   i32 ch = channel();
   check_ch(ch);
   while(skipsp() == ',') {
      tp++;
      LV lv;
      if(get_lv(&lv, true) <= 0) err(E_MISTAKE);
      u8 b[MAXSTR], type;
      int t;
      file_read(ch, &type, 1);
      if(type == 0x40) {
         file_read(ch, b, 4);
         acc_i = ((u32)b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
         t = T_INT;
      } else if(type == 0xFF) {
         file_read(ch, b, 5);
         acc_f = bbc_to_f(b);
         t = T_REAL;
      } else {
         file_read(ch, &type, 1);
         file_read(ch, b, type);
         slen = type;
         for(int i = 0; i < slen; i++) sbuf[i] = b[slen - 1 - i];
         t = T_STR;
      }
      lv_store(&lv, t);
   }
}

//------------------------------------------------------------------
// *commands
static void cat(const char *path)
{
   const char *name;
#ifdef __riscv
   struct dirent d;
   int h = opendir(*path ? path : ".");
   if(h < 0) err(E_NOTFOUND);
   while(_readdir(h, &d) >= 0 && d.d_name[0]) {
      name = d.d_name;
      bool dir = d.d_isdir;
#else
   DIR *h = opendir(*path ? path : ".");
   struct dirent *d;
   if(!h) err(E_NOTFOUND);
   while((d = readdir(h))) {
      name = d->d_name;
      if(*name == '.') continue;
      bool dir = d->d_type == DT_DIR;
#endif
      int len = strlen(name) + dir;
      if(pos && pos + len >= 78) newline();
      out_cstr(name);
      if(dir) vdu('/');
      do vdu(' '); while(pos % 16);
      poll_escape();
   }
   closedir(h);
   if(pos) newline();
}

static bool cmd_is(const char *cmd, int len, const char *name)
{
   for(int i = 0; i < len; i++) {
      char c = cmd[i];
      if(c == '.') return i > 0;
      if((c & ~0x20) != name[i]) return false;
   }
   return !name[len];
}

void oscli(const char *cmd, int len)
{
   char buf[LINEMAX];
   if(len > LINEMAX - 1) len = LINEMAX - 1;
   memcpy(buf, cmd, len);
   buf[len] = 0;

   char *c = buf;
   while(*c == '*' || *c == ' ') c++;
   int n = 0;
   while(c[n] && c[n] != ' ' && c[n] != '.') n++;
   if(c[n] == '.') n++;
   char *arg = c + n;
   while(*arg == ' ') arg++;
   for(char *e = arg + strlen(arg); e > arg && e[-1] == ' '; ) *--e = 0;

   if((n == 1 && *c == '.') || cmd_is(c, n, "CAT")) cat(arg);
   else if(cmd_is(c, n, "CD") || cmd_is(c, n, "DIR")) {
      if(chdir(arg)) err(E_NOTFOUND);
   }
   else if(cmd_is(c, n, "DELETE")) {
      if(unlink(arg)) err(E_NOTFOUND);
   }
   else if(cmd_is(c, n, "MKDIR")) {
      if(mkdir(arg, 0777)) err(E_FILE);
   }
   else if(cmd_is(c, n, "BYE") || cmd_is(c, n, "QUIT")) {
      file_close(0);
      console_restore();
      _exit(0);
   }
   else if(!cmd_is(c, n, "FX") && n) err(E_BADCMD);
}

//------------------------------------------------------------------
// CALL and USR: the code is called with A%, X%, Y% and C% in a0-a3, and
// a0 is returned.
void call_code(u32 addr, i32 *regs)
{
#ifdef __riscv
   i32 (*fn)(i32, i32, i32, i32) = (void *)addr;
   regs[0] = fn(regs[0], regs[1], regs[2], regs[3]);
#else
   (void)addr;
   (void)regs;
   err(E_BADCMD);
#endif
}

#ifndef __riscv
void run_on_stack(void (*fn)(void), void *sp)
{
   (void)sp;
   fn();
}
#endif
