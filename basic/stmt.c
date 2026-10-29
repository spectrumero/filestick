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


// Statements and the main execution loop.

#include <alloca.h>
#include "basic.h"

// FOR, GOSUB and REPEAT stacks. Each PROC/FN call starts a new base so
// it can't see (and discards on return) loops it didn't start.
typedef union { i32 i; F f; } Num;
typedef struct { void *var; int t; Num lim, step; u8 *tp, *line; } ForFrame;
typedef struct { u8 *tp, *line; } RetFrame;

#define MAXFOR    10
#define MAXGOSUB  26
#define MAXREPEAT 20

static ForFrame forstk[MAXFOR];
static RetFrame gosubstk[MAXGOSUB], repstk[MAXREPEAT];
static int forsp, forbase, gosp, gobase, repsp, repbase;
static u8 pollcount;
static int listo;

void reset_stacks(void)
{
   forsp = forbase = gosp = gobase = repsp = repbase = 0;
}

static bool is_end(u8 c)
{
   return c == ':' || c == 0x0D || c == TK_ELSE;
}

static void need_eq(void)
{
   if(skipsp() != '=') err(E_MISTAKE);
   tp++;
}

static bool cond(int t)
{
   if(t == T_STR) err(E_TYPE);
   return t == T_INT ? acc_i != 0 : acc_f.m != 0;
}

// A line number: either encoded or an expression
static u32 get_lineno(void)
{
   if(skipsp() == TK_LINENO) {
      u32 n = decode_lineno(tp + 1);
      tp += 4;
      return n;
   }
   return expr_int();
}

// Optional line number, returns def if there isn't one
static u32 opt_lineno(u32 def)
{
   u8 c = skipsp();
   if(c == TK_LINENO || (c >= '0' && c <= '9')) return get_lineno();
   return def;
}

// Evaluate a string expression into a NUL terminated file name
static char *get_name(void)
{
   expr_str();
   sbuf[slen] = 0;
   return sbuf;
}

// Skip expressions separated by commas (for unsupported graphics/sound)
static void skip_args(void)
{
   do expr(); while(skipsp() == ',' && tp++);
}

//------------------------------------------------------------------
// Assignment, including the pseudo variables
static void assign(void)
{
   LV lv;
   if(get_lv(&lv, true) <= 0) err(E_MISTAKE);
   need_eq();
   lv_store(&lv, expr());
}

static u8 *addr_expr(void)
{
   need_eq();
   return (u8 *)(uintptr_t)(u32)expr_int();
}

__attribute__((noinline)) static void pseudo_assign(u8 tok)
{
   u8 *a;
   switch(tok) {
      case TK_PTR2: {
         i32 ch = channel();
         need_eq();
         file_setptr(ch, expr_int());
         break;
      }
      case TK_TIME2:
         need_eq();
         set_time(expr_int());
         break;
      case TK_PAGE2:
         a = addr_expr();
         if(a < ws_start || a > himem - 256) err(E_RANGE);
         page = (u8 *)((uintptr_t)a & ~3);
         break;
      case TK_LOMEM2:
         a = addr_expr();
         if(a < top || a > himem - 256) err(E_RANGE);
         lomem = (u8 *)(((uintptr_t)a + sizeof(void *) - 1) & ~(sizeof(void *) - 1));
         clear_vars();
         break;
      case TK_HIMEM2:
         a = addr_expr();
         if(a > ws_end || a < vartop + 1024 || procdepth) err(E_RANGE);
         himem = (u8 *)((uintptr_t)a & ~15);
         himem_restart();
   }
}

//------------------------------------------------------------------
// PRINT
__attribute__((noinline)) static void print_stmt(void)
{
   if(skipsp() == '#') {
      print_file();
      return;
   }
   bool justify = true, hex = false;
   u32 fmt = resint[0];
   int fw = fmt & 0xFF;
   char buf[40];

   for(;;) {
      u8 c = skipsp();
      if(is_end(c)) {
         newline();
         break;
      }
      tp++;
      if(c == ';' || c == ',') {
         justify = c == ',';
         hex = false;
         if(c == ',' && fw)
            while(pos % fw) vdu(' ');
         if(c == ';' && is_end(skipsp())) break;
      } else if(c == '\'') {
         newline();
      } else if(c == '~') {
         hex = true;
      } else if(c == TK_TAB) {
         i32 x = expr_int();
         if(skipsp() == ',') {
            tp++;
            i32 y = expr_int();
            vdu(31); vdu(x); vdu(y);
         } else {
            if(pos > x) newline();
            while(pos < x) vdu(' ');
         }
         if(skipsp() != ')') err(E_BRACKET);
         tp++;
      } else if(c == TK_SPC) {
         for(i32 n = to_int(factor()); n > 0; n--) vdu(' ');
      } else {
         tp--;
         int t = expr();
         if(t == T_STR) {
            out_str(sbuf, slen);
         } else {
            int n = hex ? fmt_int(buf, to_int(t), true) : fmt_num(buf, t, fmt);
            if(justify)
               for(int i = n; i < fw; i++) vdu(' ');
            out_str(buf, n);
         }
      }
   }
   flush_out();
}

//------------------------------------------------------------------
// INPUT
__attribute__((noinline)) static void input_stmt(void)
{
   if(skipsp() == '#') {
      input_file();
      return;
   }
   bool line = skipsp() == TK_LINE;
   if(line) tp++;
   char *buf = alloca(LINEMAX);
   int bpos = 0, blen = -1;
   bool q = true;

   for(;;) {
      u8 c = skipsp();
      if(is_end(c)) break;
      if(c == ',' || c == ';') {
         tp++;
         q = true;
         continue;
      }
      if(c == '\'') {
         tp++;
         newline();
         continue;
      }
      if(c == '"') {
         factor();
         out_str(sbuf, slen);
         q = false;
         continue;
      }
      LV lv;
      if(get_lv(&lv, true) <= 0) err(E_MISTAKE);
      if(bpos > blen) {
         if(q) vdu('?');
         blen = read_line(buf, LINEMAX - 16);
         bpos = 0;
      }
      q = true;

      // the next field
      int s = bpos, e;
      if(line) {
         e = blen;
      } else {
         while(buf[s] == ' ') s++;
         for(e = s; e < blen && buf[e] != ','; e++);
      }
      bpos = e + 1;
      buf[e] = 0;
      if(lv.t == L_STR || lv.t == L_SIND) {
         slen = e - s;
         memcpy(sbuf, buf + s, slen);
         lv_store(&lv, T_STR);
      } else {
         const u8 *p = (u8 *)buf + s;
         lv_store(&lv, str_to_num(&p));
      }
   }
}

//------------------------------------------------------------------
// IF: tp is left where execution continues
static void if_stmt(void)
{
   bool c = cond(expr());
   if(skipsp() == TK_THEN) tp++;
   if(!c) {
      bool q = false;
      for(;; tp++) {
         if(*tp == 0x0D) return;
         if(*tp == '"') q = !q;
         if(!q && *tp == TK_ELSE) break;
      }
      tp++;
   }
   if(skipsp() == TK_LINENO) jump_line(get_lineno());
}

//------------------------------------------------------------------
// ON ... GOTO/GOSUB and ON ERROR
__attribute__((noinline)) static void on_stmt(void)
{
   u8 c = skipsp();
   if(c == TK_ERROR) {
      tp++;
      if(skipsp() == TK_OFF)
         onerr = NULL;
      else
         set_onerr(tp);
      tp = skip_line_end(tp);
      return;
   }
   i32 n = expr_int();
   c = skipsp();
   if(c != TK_GOTO && c != TK_GOSUB) err(E_ONSYNTAX);
   tp++;
   u32 target = 0;
   bool found = false;
   for(i32 i = 1;; i++) {
      u32 l = get_lineno();
      if(i == n) {
         target = l;
         found = true;
      }
      if(skipsp() != ',') break;
      tp++;
   }
   if(!found) {
      if(skipsp() != TK_ELSE) err(E_ONRANGE);
      tp++;
      if(skipsp() == TK_LINENO) jump_line(get_lineno());
      return;
   }
   if(skipsp() == TK_ELSE) tp = skip_line_end(tp);
   if(c == TK_GOSUB) {
      if(gosp == MAXGOSUB) err(E_MANYGOSUB);
      gosubstk[gosp].tp = tp;
      gosubstk[gosp++].line = curline;
   }
   jump_line(target);
}

//------------------------------------------------------------------
// FOR/NEXT
__attribute__((noinline)) static void for_stmt(void)
{
   LV lv;
   if(get_lv(&lv, true) <= 0 || (lv.t != L_INT && lv.t != L_REAL)) err(E_FORVAR);
   need_eq();
   lv_store(&lv, expr());
   if(skipsp() != TK_TO) err(E_NOTO);
   tp++;
   int t = expr();
   ForFrame f;
   f.var = lv.a;
   f.t = lv.t;
   if(lv.t == L_INT) {
      f.lim.i = to_int(t);
      f.step.i = 1;
   } else {
      f.lim.f = to_real(t);
      f.step.f = F_ONE;
   }
   if(skipsp() == TK_STEP) {
      tp++;
      t = expr();
      if(lv.t == L_INT) f.step.i = to_int(t);
      else f.step.f = to_real(t);
   }
   f.tp = tp;
   f.line = curline;

   // a loop using the same variable replaces it (and any inside it)
   for(int i = forbase; i < forsp; i++)
      if(forstk[i].var == f.var) forsp = i;
   if(forsp == MAXFOR) err(E_MANYFOR);
   forstk[forsp++] = f;
}

// Returns true if execution jumped back to the loop
__attribute__((noinline)) static bool next_stmt(void)
{
   for(;;) {
      if(forsp <= forbase) err(E_NOFOR);
      u8 c = skipsp();
      if(!is_end(c) && c != ',') {
         LV lv;
         if(get_lv(&lv, true) <= 0) err(E_CANTMATCH);
         int i = forsp;
         while(--i >= forbase && forstk[i].var != lv.a);
         if(i < forbase) err(E_CANTMATCH);
         forsp = i + 1;
      }
      ForFrame *f = &forstk[forsp - 1];
      bool done;
      if(f->t == L_INT) {
         i32 v = *(i32 *)f->var + f->step.i;
         *(i32 *)f->var = v;
         done = f->step.i >= 0 ? v > f->lim.i : v < f->lim.i;
      } else {
         F v = f_add(*(F *)f->var, f->step.f);
         *(F *)f->var = v;
         int r = f_cmp(v, f->lim.f);
         done = f->step.f.s ? r < 0 : r > 0;
      }
      if(!done) {
         tp = f->tp;
         curline = f->line;
         return true;
      }
      forsp--;
      if(skipsp() != ',') return false;
      tp++;
   }
}

//------------------------------------------------------------------
// READ/DATA. dataptr points at the next item, or NULL to start
// searching from datastart.
static u8 *find_data(u8 *l)
{
   for(; !(l[1] & 0x80); l += l[3]) {
      bool q = false;
      for(u8 *s = l + 4; *s != 0x0D; s++) {
         if(*s == '"') q = !q;
         if(q) continue;
         if(*s == TK_REM) break;
         if(*s == TK_DATA) return s + 1;
      }
   }
   err(E_DATA);
}

__attribute__((noinline)) static void read_stmt(void)
{
   do {
      LV lv;
      if(get_lv(&lv, true) <= 0) err(E_MISTAKE);
      u8 *p = dataptr ? dataptr : find_data(datastart);
      while(*p == ' ') p++;
      if(*p == 0x0D) {
         p = find_data(p);
         while(*p == ' ') p++;
      }
      // copy the item to sbuf
      slen = 0;
      if(*p == '"') {
         for(p++; *p != 0x0D; p++) {
            if(*p == '"' && *++p != '"') break;
            sbuf[slen++] = *p;
         }
         while(*p != ',' && *p != 0x0D) p++;
      } else {
         while(*p != ',' && *p != 0x0D) sbuf[slen++] = *p++;
      }
      if(*p == ',') p++;
      dataptr = p;
      if(lv.t == L_STR || lv.t == L_SIND)
         lv_store(&lv, T_STR);
      else {
         sbuf[slen] = 0;
         const u8 *s = (u8 *)sbuf;
         lv_store(&lv, str_to_num(&s));
      }
   } while(skipsp() == ',' && tp++);
}

//------------------------------------------------------------------
// PROC and FN calls. Parameters and LOCALs are saved here on the C
// stack and restored on return.
typedef struct Save { struct Save *next; LV lv; int t; i32 i; F f; char *s; int len; } Save;

#define SAVE_VAR(lvp) do { Save *s_ = alloca(sizeof(Save)); \
   s_->lv = *(lvp); \
   s_->t = lv_load(lvp); \
   s_->i = acc_i; \
   s_->f = acc_f; \
   if(s_->t == T_STR) { s_->s = alloca(slen); memcpy(s_->s, sbuf, slen); s_->len = slen; } \
   s_->next = saves; \
   saves = s_; } while(0)

static void set_acc(Save *s)
{
   acc_i = s->i;
   acc_f = s->f;
   if(s->t == T_STR) {
      slen = s->len;
      memcpy(sbuf, s->s, slen);
   }
}

// Put back a saved variable without disturbing the accumulator
static void restore(Save *s)
{
   u8 *a = s->lv.a;
   switch(s->lv.t) {
      case L_STR:
         str_assign(s->lv.a, s->s, s->len);
         break;
      case L_SIND:
         memcpy(a, s->s, s->len);
         a[s->len] = 0x0D;
         break;
      case L_REAL:
         *(F *)a = s->f;
         break;
      case L_BYTE:
         *a = s->i;
         break;
      default:
         memcpy(a, &s->i, 4);
   }
}

int call_fn(bool isfn)
{
   u8 *name = tp - 1;
   int n = 1;
   while(is_varchar(name[n])) n++;
   tp = name + n;
   u8 **def = find_proc(name, n);
   u8 *d = def[0];
   Save *saves = NULL, *args = NULL;

   check_room(vartop);
   while(*d == ' ') d++;
   if(*d == '(') {
      if(skipsp() != '(') err(E_ARGS);
      for(;;) {
         // the formal parameter in the DEF, then the actual one
         u8 *ctp = tp + 1;
         tp = d + 1;
         LV lv;
         if(get_lv(&lv, true) <= 0) err(E_ARGS);
         d = tp;
         tp = ctp;
         Save *a = alloca(sizeof(Save));
         a->lv = lv;
         a->t = expr();
         a->i = acc_i;
         a->f = acc_f;
         if(a->t == T_STR) {
            a->s = alloca(slen);
            a->len = slen;
            memcpy(a->s, sbuf, slen);
         }
         a->next = args;
         args = a;
         check_room(vartop);
         while(*d == ' ') d++;
         u8 c = skipsp();
         if(c != *d || (c != ',' && c != ')')) err(E_ARGS);
         if(c == ')') break;
      }
      tp++;
      d++;
      for(Save *a = args; a; a = a->next) {
         SAVE_VAR(&a->lv);
         set_acc(a);
         lv_store(&a->lv, a->t);
      }
   }

   u8 *ret_tp = tp, *ret_line = curline;
   int ofb = forbase, ofs = forsp, ogb = gobase, ogs = gosp, orb = repbase, ors = repsp;
   forbase = forsp;
   gobase = gosp;
   repbase = repsp;
   procdepth++;
   tp = d;
   curline = def[1];

   int r, t = T_INT;
   while((r = exec()) == R_LOCAL) {
      do {
         LV lv;
         if(get_lv(&lv, true) <= 0) err(E_NOTLOCAL);
         SAVE_VAR(&lv);
         acc_i = 0;
         acc_f = F_ZERO;
         slen = 0;
         lv_store(&lv, lv.t == L_STR || lv.t == L_SIND ? T_STR : T_INT);
      } while(skipsp() == ',' && tp++);
   }
   if(r != (isfn ? R_FNRET : R_ENDPROC)) err(isfn ? E_NOFN : E_NOPROC);
   if(isfn) t = expr();

   for(; saves; saves = saves->next) restore(saves);

   forbase = ofb; forsp = ofs;
   gobase = ogb; gosp = ogs;
   repbase = orb; repsp = ors;
   procdepth--;
   tp = ret_tp;
   curline = ret_line;
   return t;
}

//------------------------------------------------------------------
static void trace_stmt(void)
{
   u8 c = skipsp();
   if(c == TK_ON || c == TK_OFF) {
      tp++;
      trace_on = c == TK_ON;
      trace_max = 65535;
   } else {
      trace_max = get_lineno();
      trace_on = true;
   }
}

__attribute__((noinline)) static void list_stmt(void)
{
   if(*tp == 'O') {
      tp++;
      listo = expr_int();
      return;
   }
   u32 a = opt_lineno(0), b = a ? a : 65279;
   if(skipsp() == ',') {
      tp++;
      b = opt_lineno(65279);
   }
   int indent = 0;
   for(u8 *p = find_line(a); line_num(p) <= b; p += p[3]) {
      poll_escape();
      list_line(p, &indent, listo);
   }
   end_run();
}

static void vdu_stmt(void)
{
   while(!is_end(skipsp())) {
      i32 v = expr_int();
      vdu(v);
      u8 c = skipsp();
      if(c == ';') vdu(v >> 8);
      if(c == ',' || c == ';') tp++;
   }
   flush_out();
}

//------------------------------------------------------------------
// The main loop: execute statements from tp until the end of an immediate
// mode line, or ENDPROC, '=' or LOCAL for call_fn() to deal with.
int exec(void)
{
   for(;;) {
      u8 c = skipsp();
      if(c == ':') {
         tp++;
         continue;
      }
      if(c == 0x0D) {
         if(!curline) return R_EOL;
         if(tp[1] & 0x80) end_run();
         curline = tp;
         tp += 4;
         if(trace_on && line_num(curline) <= trace_max) {
            char num[12];
            vdu('[');
            out_str(num, fmt_int(num, line_num(curline), false));
            vdu(']');
         }
         continue;
      }
      if(!(++pollcount & 15)) poll_escape();
      tp++;

      switch(c) {
         case TK_REM:
         case TK_DATA:
         case TK_DEF:
         case TK_ELSE:
            tp = skip_line_end(tp);
            continue;
         case TK_IF:
            if_stmt();
            continue;
         case TK_GOTO:
            jump_line(get_lineno());
            continue;
         case TK_GOSUB: {
            u32 n = get_lineno();
            if(gosp == MAXGOSUB) err(E_MANYGOSUB);
            gosubstk[gosp].tp = tp;
            gosubstk[gosp++].line = curline;
            jump_line(n);
            continue;
         }
         case TK_RETURN:
            if(gosp <= gobase) err(E_NOGOSUB);
            gosp--;
            tp = gosubstk[gosp].tp;
            curline = gosubstk[gosp].line;
            continue;
         case TK_ON:
            on_stmt();
            continue;
         case TK_FOR:
            for_stmt();
            break;
         case TK_NEXT:
            if(next_stmt()) continue;
            break;
         case TK_REPEAT:
            if(repsp == MAXREPEAT) err(E_MANYREPEAT);
            repstk[repsp].tp = tp;
            repstk[repsp++].line = curline;
            continue;
         case TK_UNTIL:
            if(repsp <= repbase) err(E_NOREPEAT);
            if(!cond(expr())) {
               tp = repstk[repsp - 1].tp;
               curline = repstk[repsp - 1].line;
               continue;
            }
            repsp--;
            break;
         case TK_PROC:
            call_fn(false);
            break;
         case TK_ENDPROC:
            return R_ENDPROC;
         case TK_LOCAL:
            return R_LOCAL;
         case TK_END:
            end_run();
         case TK_STOP:
            err(E_STOP);
         case TK_LET:
            assign();
            break;
         case TK_PTR2:
         case TK_PAGE2:
         case TK_TIME2:
         case TK_LOMEM2:
         case TK_HIMEM2:
            pseudo_assign(c);
            break;
         case TK_PRINT:
            print_stmt();
            break;
         case TK_INPUT:
            input_stmt();
            break;
         case TK_DIM:
            dim_stmt();
            break;
         case TK_READ:
            read_stmt();
            break;
         case TK_RESTORE:
            datastart = page;
            if(!is_end(skipsp())) datastart = find_line(get_lineno());
            dataptr = NULL;
            break;
         case TK_REPORT:
            report();
            break;
         case TK_TRACE:
            trace_stmt();
            break;
         case TK_VDU:
            vdu_stmt();
            break;
         case TK_CLS:
            vdu(12);
            break;
         case TK_MODE:
            expr_int();
            vdu(12);
            break;
         case TK_COLOUR:
            vdu(17);
            vdu(expr_int());
            break;
         case TK_CLG:
            break;
         case TK_GCOL:
         case TK_MOVE:
         case TK_DRAW:
         case TK_PLOT:
         case TK_SOUND:
         case TK_ENVELOPE:
            skip_args();
            break;
         case TK_WIDTH:
            width = expr_int();
            break;
         case TK_CALL: {
            i32 a = expr_int();
            while(skipsp() == ',') {
               tp++;
               expr();
            }
            i32 regs[4] = { resint[1], resint[24], resint[25], resint[3] };
            call_code(a, regs);
            break;
         }
         case TK_BPUT: {
            i32 ch = channel();
            need_comma();
            file_bput(ch, expr_int());
            break;
         }
         case TK_CLOSE:
            file_close(channel());
            break;
         case TK_OSCLI:
            expr_str();
            oscli(sbuf, slen);
            break;
         case TK_CLEAR:
            clear_vars();
            break;
         case TK_RUN:
            run_program();
         case TK_CHAIN:
            load_program(get_name());
            run_program();
         case TK_LOAD:
            load_program(get_name());
            end_run();
         case TK_SAVE:
            save_program(get_name());
            break;
         case TK_NEW:
            new_program();
            end_run();
         case TK_OLD:
            if(!old_program()) err(E_BADPROG);
            end_run();
         case TK_LIST:
            list_stmt();
            break;
         case TK_DELETE: {
            u32 a = get_lineno();
            need_comma();
            delete_lines(a, get_lineno());
            end_run();
         }
         case TK_RENUMBER: {
            u32 a = opt_lineno(10), b = 10;
            if(skipsp() == ',') {
               tp++;
               b = get_lineno();
            }
            renumber(a, b ? b : 10);
            end_run();
         }
         case TK_AUTO: {
            u32 a = opt_lineno(10), b = 10;
            if(skipsp() == ',') {
               tp++;
               b = get_lineno();
            }
            set_auto(a, b ? b : 10);
            end_run();
         }
         case '*':
            c = skipsp();
            oscli((char *)tp, skip_line_end(tp) - tp);
            tp = skip_line_end(tp);
            continue;
         case '=':
            return R_FNRET;
         default:
            tp--;
            assign();
            break;
      }

      // the statement should end here
      c = skipsp();
      if(c == TK_ELSE)
         tp = skip_line_end(tp);
      else if(c != ':' && c != 0x0D)
         err(E_SYNTAX);
   }
}
