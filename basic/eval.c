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


// Variables and expression evaluation.
//
// The result of an expression is left in acc_i (integers), acc_f (reals)
// or sbuf/slen (strings), and the functions return its type.

#include <stddef.h>
#include <alloca.h>
#include "basic.h"

i32 acc_i;
F   acc_f;
char sbuf[256];
int slen;

// Variables are kept in linked lists, one per initial character. The
// name includes any % or $ suffix and a ( for arrays, and PROC and FN
// definitions are cached here too with their token as the first character.
typedef struct Var { struct Var *next; u8 nlen; char name[]; } Var;
static Var *vlist[64];

#define ALIGN(n) (((n) + sizeof(void *) - 1) & ~(sizeof(void *) - 1))

static F last_rnd;

void clear_vars(void)
{
   vartop = lomem;
   memset(vlist, 0, sizeof(vlist));
   dataptr = NULL;
   datastart = page;
}

// Allocate zeroed variable space
void *halloc(u32 n)
{
   u8 *p = vartop;
   n = ALIGN(n);
   check_room(p + n);
   vartop += n;
   memset(p, 0, n);
   return p;
}

static void *var_data(Var *v)
{
   return (u8 *)v + ALIGN(offsetof(Var, name) + v->nlen);
}

static Var *find_var(const u8 *name, int len)
{
   for(Var *v = vlist[name[0] & 63]; v; v = v->next)
      if(v->nlen == len && !memcmp(v->name, name, len)) return v;
   return NULL;
}

static Var *new_var(const u8 *name, int len, int size)
{
   Var *v = halloc(ALIGN(offsetof(Var, name) + len) + size);
   v->nlen = len;
   memcpy(v->name, name, len);
   v->next = vlist[name[0] & 63];
   vlist[name[0] & 63] = v;
   return v;
}

u8 skipsp(void)
{
   while(*tp == ' ') tp++;
   return *tp;
}

static void need(u8 c, int e)
{
   if(skipsp() != c) err(e);
   tp++;
}

void need_comma(void)
{
   need(',', E_COMMA);
}

//------------------------------------------------------------------
// Strings in variables are allocated with some room to grow. If a string
// outgrows its space it's moved to the top of the heap (the old space is
// lost, as on the BBC Micro) unless it's already at the top.
void str_assign(SVar *sv, const char *s, int len)
{
   if(len > sv->cap) {
      int cap = len + 8 > MAXSTR ? MAXSTR : len + 8;
      if(sv->p && (u8 *)sv->p + ALIGN(sv->cap) == vartop) {
         u32 more = ALIGN(cap) - ALIGN(sv->cap);
         check_room(vartop + more);
         vartop += more;
      } else
         sv->p = halloc(cap);
      sv->cap = cap;
   }
   if(len) memmove(sv->p, s, len);
   sv->len = len;
}

//------------------------------------------------------------------
// Conversions of the accumulator
i32 to_int(int t)
{
   if(t == T_INT) return acc_i;
   if(t == T_STR) err(E_TYPE);
   return f_toint(acc_f);
}

F to_real(int t)
{
   if(t == T_INT) return f_fromint(acc_i);
   if(t == T_STR) err(E_TYPE);
   return acc_f;
}

i32 expr_int(void)
{
   return to_int(expr());
}

F expr_real(void)
{
   return to_real(expr());
}

void expr_str(void)
{
   if(expr() != T_STR) err(E_TYPE);
}

static i32 int_factor(void)
{
   return to_int(factor());
}

static int num_factor(void)
{
   int t = factor();
   if(t == T_STR) err(E_TYPE);
   return t;
}

static void str_factor(void)
{
   if(factor() != T_STR) err(E_TYPE);
}

//------------------------------------------------------------------
// Find an lvalue. Returns 1 if found, 0 if there's no variable name
// here, -1 if it doesn't exist (and create is false).
int get_lv(LV *lv, bool create)
{
   u8 c = skipsp();
   if(c == '?' || c == '!' || c == '$') {
      tp++;
      lv->a = (void *)(uintptr_t)(u32)int_factor();
      lv->t = c == '?' ? L_BYTE : c == '!' ? L_INT : L_SIND;
      return 1;
   }

   u8 *name = tp;
   if(!is_varchar(c) || (c >= '0' && c <= '9')) return 0;
   int n = 1;
   while(is_varchar(name[n])) n++;
   if(name[n] == '%' || name[n] == '$') n++;
   u8 type = name[n - 1];
   bool array = name[n] == '(';
   if(array) n++;
   tp = name + n;
   lv->t = type == '$' ? L_STR : type == '%' ? L_INT : L_REAL;

   if(array) {
      Var *v = find_var(name, n);
      u32 *blk = v ? *(u32 **)var_data(v) : NULL;
      if(!blk) err(E_ARRAY);
      u32 nd = blk[0], idx = 0;
      for(u32 i = 0; i < nd; i++) {
         if(i) need_comma();
         i32 s = expr_int();
         if((u32)s >= blk[i + 1]) err(E_SUBSCRIPT);
         idx = idx * blk[i + 1] + s;
      }
      need(')', E_BRACKET);
      int size = lv->t == L_STR ? sizeof(SVar) : lv->t == L_INT ? 4 : sizeof(F);
      lv->a = (u8 *)blk + ALIGN((nd + 1) * 4) + idx * size;
   } else if(n == 2 && type == '%' && name[0] <= 'Z') {
      lv->a = &resint[name[0] - '@'];
   } else {
      Var *v = find_var(name, n);
      if(!v) {
         if(!create) return -1;
         v = new_var(name, n, lv->t == L_STR ? sizeof(SVar) : sizeof(F));
      }
      lv->a = var_data(v);
   }

   // A?n and A!n
   c = skipsp();
   if((c == '?' || c == '!') && lv->t != L_STR) {
      tp++;
      i32 base = to_int(lv_load(lv));
      lv->a = (void *)(uintptr_t)(u32)(base + int_factor());
      lv->t = c == '?' ? L_BYTE : L_INT;
   }
   return 1;
}

int lv_load(LV *lv)
{
   u8 *a = lv->a;
   switch(lv->t) {
      case L_INT:
         if((uintptr_t)a & 3)
            memcpy(&acc_i, a, 4);
         else
            acc_i = *(volatile i32 *)a;
         return T_INT;
      case L_REAL:
         acc_f = *(F *)a;
         return T_REAL;
      case L_BYTE:
         acc_i = *(volatile u8 *)a;
         return T_INT;
      case L_STR: {
         SVar *sv = lv->a;
         slen = sv->len;
         if(slen) memcpy(sbuf, sv->p, slen);
         return T_STR;
      }
   }
   // $ indirection
   for(slen = 0; slen < MAXSTR && a[slen] != 0x0D; slen++)
      sbuf[slen] = a[slen];
   return T_STR;
}

void lv_store(LV *lv, int t)
{
   u8 *a = lv->a;
   if(lv->t == L_STR || lv->t == L_SIND) {
      if(t != T_STR) err(E_TYPE);
      if(lv->t == L_STR)
         str_assign(lv->a, sbuf, slen);
      else {
         memcpy(a, sbuf, slen);
         a[slen] = 0x0D;
      }
   } else if(lv->t == L_REAL) {
      *(F *)a = to_real(t);
   } else {
      i32 v = to_int(t);
      if(lv->t == L_BYTE)
         *(volatile u8 *)a = v;
      else if((uintptr_t)a & 3)
         memcpy(a, &v, 4);
      else
         *(volatile i32 *)a = v;
   }
}

//------------------------------------------------------------------
// DIM
void dim_stmt(void)
{
   do {
      u8 *name = tp;
      u8 c = skipsp();
      name = tp;
      if(!is_varchar(c) || (c >= '0' && c <= '9')) err(E_BADDIM);
      int n = 1;
      while(is_varchar(name[n])) n++;
      if(name[n] == '%' || name[n] == '$') n++;
      u8 type = name[n - 1];

      if(name[n] != '(') {
         // DIM var size: reserve bytes
         LV lv;
         get_lv(&lv, true);
         if(lv.t == L_STR) err(E_BADDIM);
         i32 sz = expr_int();
         if(sz < -1) err(E_BADDIM);
         acc_i = (i32)(uintptr_t)vartop;
         if(sz >= 0) {
            if((u32)sz >= (u32)(mem_limit() - vartop)) err(E_DIMSPACE);
            halloc(sz + 1);
         }
         lv_store(&lv, T_INT);
         continue;
      }

      tp = name + ++n;
      Var *v = find_var(name, n);
      if(v && *(void **)var_data(v)) err(E_BADDIM);
      u32 dims[10], total = 1;
      int nd = 0;
      do {
         i32 d = expr_int();
         if(d < 0 || nd == 10) err(E_BADDIM);
         dims[nd++] = d + 1;
         total *= d + 1;
         if(total > 0x100000) err(E_DIMSPACE);
      } while(skipsp() == ',' && tp++);
      need(')', E_BRACKET);

      int size = type == '$' ? sizeof(SVar) : type == '%' ? 4 : sizeof(F);
      u32 hdr = ALIGN((nd + 1) * 4);
      u32 bytes = hdr + total * size;
      if(!v) v = new_var(name, n, sizeof(void *));
      if(bytes >= (u32)(mem_limit() - vartop)) err(E_DIMSPACE);
      u32 *blk = halloc(bytes);
      blk[0] = nd;
      memcpy(blk + 1, dims, nd * 4);
      *(u32 **)var_data(v) = blk;
   } while(skipsp() == ',' && tp++);
}

//------------------------------------------------------------------
// Find a PROC or FN definition. name starts with the token. Returns a
// pointer to the cached entry: the text after the name in the DEF, and
// its line.
u8 **find_proc(const u8 *name, int n)
{
   Var *v = find_var(name, n);
   if(!v) {
      for(u8 *l = page; !(l[1] & 0x80); l += l[3]) {
         u8 *s = l + 4;
         while(*s == ' ') s++;
         if(*s++ != TK_DEF) continue;
         while(*s == ' ') s++;
         if(!memcmp(s, name, n) && !is_varchar(s[n])) {
            v = new_var(name, n, 2 * sizeof(u8 *));
            u8 **d = var_data(v);
            d[0] = s + n;
            d[1] = l;
            return d;
         }
      }
      err(E_NOSUCHFN);
   }
   return var_data(v);
}

//------------------------------------------------------------------
// Expressions, highest precedence first. A saved left hand side:
typedef struct { int t; i32 i; F f; char *s; int len; } Lhs;

static void save_lhs(Lhs *l, int t)
{
   l->t = t;
   l->i = acc_i;
   l->f = acc_f;
}

#define SAVE_LHS(l, t) do { save_lhs(&(l), t); \
   if(t == T_STR) { (l).s = alloca(slen); (l).len = slen; memcpy((l).s, sbuf, slen); } \
   } while(0)

// Arithmetic on the left hand side and the accumulator
static int arith(u8 op, Lhs *l, int tb)
{
   if(l->t == T_STR || tb == T_STR) err(E_TYPE);
   if(op == TK_DIV || op == TK_MOD) {
      i32 b = to_int(tb);
      i32 a = l->t == T_INT ? l->i : f_toint(l->f);
      if(!b) err(E_DIVZERO);
      if(b == -1)
         acc_i = op == TK_DIV ? -(u32)a : 0;
      else
         acc_i = op == TK_DIV ? a / b : a % b;
      return T_INT;
   }
   if(l->t == T_INT && tb == T_INT && op != '/' && op != '^') {
      i32 a = l->i, b = acc_i;
      if(op == '+') acc_i = (u32)a + (u32)b;
      else if(op == '-') acc_i = (u32)a - (u32)b;
      else {
         int64_t p = (int64_t)a * b;
         if(p != (i32)p) {
            acc_f = f_mul(f_fromint(a), f_fromint(b));
            return T_REAL;
         }
         acc_i = p;
      }
      return T_INT;
   }
   F a = l->t == T_INT ? f_fromint(l->i) : l->f;
   F b = to_real(tb);
   switch(op) {
      case '+': acc_f = f_add(a, b); break;
      case '-': acc_f = f_sub(a, b); break;
      case '*': acc_f = f_mul(a, b); break;
      case '/': acc_f = f_div(a, b); break;
      default:  acc_f = f_pow(a, b); break;
   }
   return T_REAL;
}

// Binary operator precedence, highest binds tightest
static int prec(u8 op)
{
   switch(op) {
      case TK_OR: case TK_EOR:   return 1;
      case TK_AND:               return 2;
      case '=': case '<': case '>': return 3;
      case '+': case '-':        return 4;
      case '*': case '/': case TK_DIV: case TK_MOD: return 5;
      case '^':                  return 6;
   }
   return 0;
}

// Apply binary operator op of precedence p to the saved left hand side
// and the accumulator
static __attribute__((noinline)) int combine(u8 op, int p, int rel, Lhs *l, int t2)
{
   int t = l->t;
   if(p <= 2) {
      i32 a = t == T_INT ? l->i : t == T_REAL ? f_toint(l->f) : to_int(T_STR);
      i32 b = to_int(t2);
      acc_i = op == TK_AND ? a & b : op == TK_OR ? a | b : a ^ b;
      return T_INT;
   }
   if(p == 3) {
      int c;
      if(t == T_STR || t2 == T_STR) {
         if(t != t2) err(E_TYPE);
         int n = l->len < slen ? l->len : slen;
         c = memcmp(l->s, sbuf, n);
         if(!c) c = l->len - slen;
      } else if(t == T_INT && t2 == T_INT)
         c = l->i < acc_i ? -1 : l->i > acc_i;
      else
         c = f_cmp(t == T_INT ? f_fromint(l->i) : l->f, to_real(t2));
      acc_i = rel & (c < 0 ? 1 : c ? 4 : 2) ? -1 : 0;
      return T_INT;
   }
   if(t == T_STR && op == '+') {
      if(t2 != T_STR) err(E_TYPE);
      if(l->len + slen > MAXSTR) err(E_STRLONG);
      memmove(sbuf + l->len, sbuf, slen);
      memcpy(sbuf, l->s, l->len);
      slen += l->len;
      return T_STR;
   }
   return arith(op, l, t2);
}

// Evaluate operators of precedence minp and above, by precedence climbing
static int binary(int minp)
{
   int t = factor();
   for(;;) {
      u8 op = skipsp();
      int p = prec(op);
      if(!p || p < minp) return t;
      tp++;

      // relational operators: bit 0 <, bit 1 =, bit 2 >
      int rel = op == '<' ? 1 : op == '=' ? 2 : 4;
      if(p == 3 && op != '=') {
         if(*tp == '=') { rel |= 2; tp++; }
         else if(op == '<' && *tp == '>') { rel = 5; tp++; }
      }

      Lhs l;
      SAVE_LHS(l, t);
      t = combine(op, p, rel, &l, binary(p + 1));
   }
}

int expr(void)
{
   check_room(vartop);
   return binary(1);
}

//------------------------------------------------------------------
// Functions of one real argument
static F (*const realfn[])(F) = {
   f_sqrt, f_ln, f_log, f_exp, f_sin, f_cos, f_tan, f_atn, f_asn, f_acs,
   f_deg, f_rad };
static const u8 realtok[] = {
   TK_SQR, TK_LN, TK_LOG, TK_EXP, TK_SIN, TK_COS, TK_TAN, TK_ATN, TK_ASN,
   TK_ACS, TK_DEG, TK_RAD };

// Evaluate a string number (VAL and INPUT)
int str_to_num(const u8 **pp)
{
   const u8 *p = *pp;
   while(*p == ' ') p++;
   bool neg = *p == '-';
   if(*p == '-' || *p == '+') p++;
   int t = T_INT;
   acc_i = 0;
   if((*p >= '0' && *p <= '9') || *p == '.') t = parse_num(&p);
   if(neg) {
      if(t == T_INT) acc_i = -(u32)acc_i;
      else acc_f = f_neg(acc_f);
   }
   *pp = p;
   return t;
}

// Common to LEFT$ and RIGHT$
static int left_right(bool right)
{
   expr_str();
   need_comma();
   char *s = alloca(slen);
   int len = slen;
   memcpy(s, sbuf, len);
   i32 n = expr_int();
   need(')', E_BRACKET);
   if((u32)n > (u32)len) n = len;
   memcpy(sbuf, right ? s + len - n : s, n);
   slen = n;
   return T_STR;
}

static int func(u8 tok)
{
   int t;
   i32 n;

   for(unsigned i = 0; i < sizeof(realtok); i++) {
      if(tok == realtok[i]) {
         acc_f = realfn[i](to_real(num_factor()));
         return T_REAL;
      }
   }

   switch(tok) {
      case TK_PI:    acc_f = f_pi(); return T_REAL;
      case TK_TRUE:  acc_i = -1; return T_INT;
      case TK_FALSE: acc_i = 0; return T_INT;
      case TK_ERL:   acc_i = errline; return T_INT;
      case TK_ERR:   acc_i = errnum; return T_INT;
      case TK_TIME:  acc_i = get_time(); return T_INT;
      case TK_PAGE:  acc_i = (i32)(uintptr_t)page; return T_INT;
      case TK_LOMEM: acc_i = (i32)(uintptr_t)lomem; return T_INT;
      case TK_HIMEM: acc_i = (i32)(uintptr_t)himem; return T_INT;
      case TK_COUNT:
      case TK_POS:   acc_i = pos; return T_INT;
      case TK_VPOS:  acc_i = vpos; return T_INT;
      case TK_TO:
         if(*tp != 'P') err(E_SYNTAX);
         tp++;
         acc_i = (i32)(uintptr_t)top;
         return T_INT;
      case TK_GET:   acc_i = getkey(-1); return T_INT;
      case TK_GETS:
      case TK_INKEYS:
         n = tok == TK_GETS ? getkey(-1) : getkey(int_factor());
         sbuf[0] = n;
         slen = n >= 0;
         return T_STR;
      case TK_INKEY:
         n = int_factor();
         acc_i = n < 0 ? 0 : getkey(n);
         return T_INT;
      case TK_RND:
         if(*tp != '(') {
            acc_i = rnd_next();
            return T_INT;
         }
         tp++;
         n = expr_int();
         need(')', E_BRACKET);
         if(n < 0) {
            rnd_seed(n);
            acc_i = n;
            return T_INT;
         }
         if(n == 1) last_rnd = rnd_float();
         if(n <= 1) {
            acc_f = last_rnd;
            return T_REAL;
         }
         acc_i = 1 + (u32)rnd_next() % (u32)n;
         return T_INT;
      case TK_ABS:
      case TK_SGN:
         t = num_factor();
         if(tok == TK_SGN) {
            acc_i = t == T_INT ? (acc_i > 0) - (acc_i < 0) :
                    !acc_f.m ? 0 : acc_f.s ? -1 : 1;
            return T_INT;
         }
         if(t == T_INT) {
            if(acc_i < 0) acc_i = -(u32)acc_i;
         } else
            acc_f.s = 0;
         return t;
      case TK_INT:
         t = num_factor();
         if(t == T_REAL && f_floor(acc_f, &n)) {
            acc_i = n;
            t = T_INT;
         }
         return t;
      case TK_NOT:
         acc_i = ~int_factor();
         return T_INT;
      case TK_LEN:
         str_factor();
         acc_i = slen;
         return T_INT;
      case TK_ASC:
         str_factor();
         acc_i = slen ? (u8)sbuf[0] : -1;
         return T_INT;
      case TK_VAL: {
         str_factor();
         sbuf[slen] = 0;
         const u8 *p = (u8 *)sbuf;
         return str_to_num(&p);
      }
      case TK_EVAL: {
         str_factor();
         sbuf[slen] = 0;
         u8 *buf = alloca(LINEMAX);
         tokenise((u8 *)sbuf, buf, false);
         u8 *save = tp;
         tp = buf;
         t = expr();
         tp = save;
         return t;
      }
      case TK_CHRS:
         sbuf[0] = int_factor();
         slen = 1;
         return T_STR;
      case TK_STRS: {
         bool hex = skipsp() == '~';
         if(hex) tp++;
         t = num_factor();
         if(hex)
            slen = fmt_int(sbuf, to_int(t), true);
         else
            slen = fmt_num(sbuf, t, (resint[0] & 0xFF000000) ? resint[0] : 0x90A);
         return T_STR;
      }
      case TK_LEFTS:
      case TK_RIGHTS:
         return left_right(tok == TK_RIGHTS);
      case TK_MIDS: {
         expr_str();
         need_comma();
         char *s = alloca(slen);
         int len = slen;
         memcpy(s, sbuf, len);
         i32 start = expr_int(), cnt = MAXSTR;
         if(skipsp() == ',') {
            tp++;
            cnt = expr_int();
         }
         need(')', E_BRACKET);
         if(start < 1) start = 1;
         if(start > len) start = len + 1;
         if((u32)cnt > (u32)(len - start + 1)) cnt = len - start + 1;
         memcpy(sbuf, s + start - 1, cnt);
         slen = cnt;
         return T_STR;
      }
      case TK_STRINGS: {
         n = expr_int();
         need_comma();
         expr_str();
         need(')', E_BRACKET);
         if(n < 0) n = 0;
         if(n * slen > MAXSTR) err(E_STRLONG);
         for(int i = 1; i < n; i++) memcpy(sbuf + i * slen, sbuf, slen);
         slen *= n;
         return T_STR;
      }
      case TK_INSTR: {
         expr_str();
         need_comma();
         char *s = alloca(slen);
         int len = slen;
         memcpy(s, sbuf, len);
         expr_str();
         i32 start = 1;
         if(skipsp() == ',') {
            tp++;
            start = expr_int();
         }
         need(')', E_BRACKET);
         if(start < 1) start = 1;
         acc_i = 0;
         for(int i = start - 1; i + slen <= len; i++) {
            if(!memcmp(s + i, sbuf, slen)) {
               acc_i = i + 1;
               break;
            }
         }
         return T_INT;
      }
      case TK_USR: {
         n = int_factor();
         i32 regs[4] = { resint[1], resint[24], resint[25], resint[3] };
         call_code(n, regs);
         acc_i = regs[0];
         return T_INT;
      }
      case TK_OPENIN:
      case TK_OPENOUT:
      case TK_OPENUP:
         str_factor();
         acc_i = open_file(tok);
         return T_INT;
      case TK_EOF:
      case TK_EXT:
      case TK_PTR:
      case TK_BGET:
         acc_i = file_func(tok, channel());
         return T_INT;
      case TK_FN:
         return call_fn(true);
      case TK_ADVAL:
         int_factor();
         acc_i = 0;
         return T_INT;
      case TK_POINT:
         expr_int();
         need_comma();
         expr_int();
         need(')', E_BRACKET);
         acc_i = 0;
         return T_INT;
   }
   err(E_SYNTAX);
}

int factor(void)
{
   u8 c = skipsp();
   int t;

   if(c >= 0x80) {
      tp++;
      return func(c);
   }
   if((c >= '0' && c <= '9') || c == '.' || c == '&') {
      const u8 *p = tp;
      t = parse_num(&p);
      tp = (u8 *)p;
      return t;
   }
   switch(c) {
      case '"':
         slen = 0;
         for(tp++;; tp++) {
            c = *tp;
            if(c == 0x0D) err(E_QUOTE);
            if(c == '"' && *++tp != '"') break;
            if(slen == MAXSTR) err(E_STRLONG);
            sbuf[slen++] = c;
         }
         return T_STR;
      case '(':
         tp++;
         t = expr();
         need(')', E_BRACKET);
         return t;
      case '-':
         tp++;
         t = num_factor();
         if(t == T_INT) acc_i = -(u32)acc_i;
         else acc_f = f_neg(acc_f);
         return t;
      case '+':
         tp++;
         return num_factor();
   }
   LV lv;
   int r = get_lv(&lv, false);
   if(!r) err(E_MISTAKE);
   if(r < 0) err(E_NOVAR);
   return lv_load(&lv);
}
