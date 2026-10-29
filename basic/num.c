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

// Floating point arithmetic, maths functions and number <-> text
// conversion.
//
// Reals are BBC BASIC style: a 32 bit mantissa and an exponent in the
// range -127..127, giving the same range and precision as BBC BASIC II.
// Doing this in software here is several K smaller (and faster) than
// using double with libgcc's soft float routines.

#include "basic.h"

const F F_ZERO = {0, 0, 0};
const F F_ONE  = {0x80000000, 1, 0};
static const F F_HALF     = {0x80000000, 0, 0};
static const F F_PI       = {0xC90FDAA2, 2, 0};
static const F F_PI2HI    = {0xC90FDAA2, 1, 0};
static const F F_PI2LO    = {0x85A308D3, -33, 0};
static const F F_2OVPI    = {0xA2F9836E, 0, 0};
static const F F_PI6      = {0x860A91C1, 0, 0};
static const F F_LN2      = {0xB17217F8, 0, 0};
static const F F_INVLN2   = {0xB8AA3B29, 1, 0};
static const F F_INVLN10  = {0xDE5BD8A9, -1, 0};
static const F F_SQRT3    = {0xDDB3D743, 1, 0};
static const F F_TAN15    = {0x8930A2F5, -1, 0};
static const F F_SQRTHALF = {0xB504F334, 0, 0};
static const F F_DEG      = {0xE52EE0D3, 6, 0};
static const F F_RAD      = {0x8EFA3513, -5, 0};

// Series coefficients, highest power last
static const F sin_c[] = {     // (-1)^k/(2k+1)!
   {0x80000000,1,0},{0xAAAAAAAB,-2,1},{0x88888889,-6,0},{0xD00D00D0,-12,1},
   {0xB8EF1D2B,-18,0},{0xD7322B40,-25,1} };
static const F cos_c[] = {     // (-1)^k/(2k)!
   {0x80000000,1,0},{0x80000000,0,1},{0xAAAAAAAB,-4,0},{0xB60B60B6,-9,1},
   {0xD00D00D0,-15,0},{0x93F27DBC,-21,1},{0x8F76C780,-28,0} };
static const F exp_c[] = {     // 1/k!
   {0x80000000,1,0},{0x80000000,1,0},{0x80000000,0,0},{0xAAAAAAAB,-2,0},
   {0xAAAAAAAB,-4,0},{0x88888889,-6,0},{0xB60B60B6,-9,0},{0xD00D00D0,-12,0},
   {0xD00D00D0,-15,0},{0xB8EF1D2B,-18,0},{0x93F27DBC,-21,0} };
static const F atn_c[] = {     // (-1)^k/(2k+1); ln uses the absolute values
   {0x80000000,1,0},{0xAAAAAAAB,-1,1},{0xCCCCCCCD,-2,0},{0x92492492,-2,1},
   {0xE38E38E4,-3,0},{0xBA2E8BA3,-3,1},{0x9D89D89E,-3,0},{0x88888889,-3,1},
   {0xF0F0F0F1,-4,0} };

// Extended precision (64 bit mantissa) values for decimal conversion
typedef struct { u64 m; int e; } X;
static const X pow10p[] = {
   {0xA000000000000000ULL,4},{0xC800000000000000ULL,7},{0x9C40000000000000ULL,14},
   {0xBEBC200000000000ULL,27},{0x8E1BC9BF04000000ULL,54},{0x9DC5ADA82B70B59EULL,107} };
static const X pow10n[] = {
   {0xCCCCCCCCCCCCCCCDULL,-3},{0xA3D70A3D70A3D70AULL,-6},{0xD1B71758E219652CULL,-13},
   {0xABCC77118461CEFDULL,-26},{0xE69594BEC44DE15BULL,-53},{0xCFB11EAD453994BAULL,-106} };

//------------------------------------------------------------------
// Make a real from value x * 2^(e-64), normalising and rounding.
F f_make(u64 x, int e, int s)
{
   F r = F_ZERO;
   if(!x) return r;
   if(!(x >> 32)) { x <<= 32; e -= 32; }
   if(!(x >> 48)) { x <<= 16; e -= 16; }
   if(!(x >> 56)) { x <<= 8; e -= 8; }
   while(!(x >> 63)) { x <<= 1; e--; }

   u64 y = x + 0x80000000;
   if(y < x) { y = 1ULL << 63; e++; }
   if(e > 127) err(E_TOOBIG);
   if(e < -127) return r;
   r.m = y >> 32;
   r.e = e;
   r.s = s;
   return r;
}

F f_fromint(i32 v)
{
   u32 u = v < 0 ? -(u32)v : (u32)v;
   return f_make((u64)u << 32, 32, v < 0);
}

//------------------------------------------------------------------
// Convert to integer, truncating towards zero
i32 f_toint(F a)
{
   if(!a.m || a.e <= 0) return 0;
   if(a.e > 31) {
      if(a.e == 32 && a.s && a.m == 0x80000000) return INT32_MIN;
      err(E_TOOBIG);
   }
   u32 u = a.m >> (32 - a.e);
   return a.s ? -(i32)u : (i32)u;
}

//------------------------------------------------------------------
// Floor: returns false if the number is too big for an integer (in
// which case it's already integral).
int f_floor(F a, i32 *out)
{
   if(a.m && a.e > 31) return 0;
   i32 i = f_toint(a);
   if(a.s && a.e < 32) {
      u32 frac = a.e <= 0 ? a.m : a.m << a.e;
      if(frac) i--;
   }
   *out = i;
   return 1;
}

F f_neg(F a)
{
   if(a.m) a.s ^= 1;
   return a;
}

F f_add(F a, F b)
{
   if(!b.m) return a;
   if(!a.m) return b;
   if(a.e < b.e) { F t = a; a = b; b = t; }

   int d = a.e - b.e;
   u64 x = (u64)a.m << 31;
   u64 y = d < 63 ? ((u64)b.m << 31) >> d : 0;
   int s = a.s;
   if(a.s == b.s)
      x += y;
   else if(x >= y)
      x -= y;
   else {
      x = y - x;
      s = b.s;
   }
   return f_make(x, a.e + 1, s);
}

F f_sub(F a, F b)
{
   return f_add(a, f_neg(b));
}

F f_mul(F a, F b)
{
   if(!a.m || !b.m) return F_ZERO;
   return f_make((u64)a.m * b.m, a.e + b.e, a.s ^ b.s);
}

F f_div(F a, F b)
{
   if(!b.m) err(E_DIVZERO);
   if(!a.m) return F_ZERO;
   u64 r = a.m, q = 0;
   for(int i = 0; i < 34; i++) {
      q <<= 1;
      if(r >= b.m) { r -= b.m; q |= 1; }
      r <<= 1;
   }
   return f_make(q, a.e - b.e + 31, a.s ^ b.s);
}

// Returns <0, 0, >0
int f_cmp(F a, F b)
{
   F d = f_sub(a, b);
   if(!d.m) return 0;
   return d.s ? -1 : 1;
}

//------------------------------------------------------------------
// Evaluate the polynomial c[0] + c[1]x + ... + c[n-1]x^(n-1)
static F poly(F x, const F *c, int n)
{
   F r = c[--n];
   while(n--)
      r = f_add(f_mul(r, x), c[n]);
   return r;
}

F f_sqrt(F a)
{
   if(!a.m) return a;
   if(a.s) err(E_ROOT);
   u64 n = (u64)a.m << ((a.e & 1) ? 31 : 32);
   u64 rem = 0, root = 0;
   for(int i = 0; i < 32; i++) {
      root <<= 1;
      rem = (rem << 2) | (n >> 62);
      n <<= 2;
      u64 t = (root << 1) | 1;
      if(rem >= t) { rem -= t; root |= 1; }
   }
   return f_make(root << 32, (a.e + 1) >> 1, 0);
}

F f_ln(F a)
{
   if(!a.m || a.s) err(E_LOGRANGE);
   int e = a.e;
   a.e = 0;
   if(f_cmp(a, F_SQRTHALF) < 0) { a.e = 1; e--; }

   // ln(a) = 2s(1 + s^2/3 + s^4/5 ...), s = (a-1)/(a+1)
   F s = f_div(f_sub(a, F_ONE), f_add(a, F_ONE));
   F s2 = f_mul(s, s);
   F r = F_ZERO;
   for(int i = 6; i >= 0; i--) {
      F c = atn_c[i];
      c.s = 0;
      r = f_add(f_mul(r, s2), c);
   }
   r = f_mul(r, s);
   r.e++;
   return f_add(r, f_mul(f_fromint(e), F_LN2));
}

F f_log(F a)
{
   return f_mul(f_ln(a), F_INVLN10);
}

F f_exp(F a)
{
   if(a.m && a.e > 7) {
      if(a.s) return F_ZERO;
      err(E_EXPRANGE);
   }
   // a = k ln2 + r
   F t = f_mul(a, F_INVLN2);
   t = f_add(t, a.s ? f_neg(F_HALF) : F_HALF);
   i32 k = f_toint(t);
   F r = f_sub(a, f_mul(f_fromint(k), F_LN2));
   r = poly(r, exp_c, 11);
   int e = r.e + k;
   if(e > 127) err(E_EXPRANGE);
   if(e < -127) return F_ZERO;
   r.e = e;
   return r;
}

F f_pow(F a, F b)
{
   i32 n;
   if(f_floor(b, &n) && f_cmp(b, f_fromint(n)) == 0 && n > -256 && n < 256) {
      F r = F_ONE;
      int k = n < 0 ? -n : n;
      F x = a;
      while(k) {
         if(k & 1) r = f_mul(r, x);
         x = f_mul(x, x);
         k >>= 1;
      }
      return n < 0 ? f_div(F_ONE, r) : r;
   }
   if(!a.m) return F_ZERO;
   return f_exp(f_mul(b, f_ln(a)));
}

// sin (cosine if c is set)
static F sin_cos(F a, int c)
{
   // reduce to -pi/4..pi/4
   if(a.m && a.e > 24) err(E_ACCURACY);
   F q = f_add(f_mul(a, F_2OVPI), a.s ? f_neg(F_HALF) : F_HALF);
   i32 k = f_toint(q);
   F kf = f_fromint(k);
   F r = f_sub(f_sub(a, f_mul(kf, F_PI2HI)), f_mul(kf, F_PI2LO));

   k += c;
   F r2 = f_mul(r, r);
   if(k & 1)
      r = poly(r2, cos_c, 7);
   else
      r = f_mul(r, poly(r2, sin_c, 6));
   if(k & 2) r = f_neg(r);
   return r;
}

F f_sin(F a) { return sin_cos(a, 0); }
F f_cos(F a) { return sin_cos(a, 1); }
F f_tan(F a) { return f_div(f_sin(a), f_cos(a)); }

F f_atn(F a)
{
   int s = a.s, inv = 0;
   F add = F_ZERO;
   a.s = 0;
   if(f_cmp(a, F_ONE) > 0) {
      a = f_div(F_ONE, a);
      inv = 1;
   }
   if(f_cmp(a, F_TAN15) > 0) {
      // atn(x) = pi/6 + atn((x*sqrt3 - 1) / (x + sqrt3))
      a = f_div(f_sub(f_mul(a, F_SQRT3), F_ONE), f_add(a, F_SQRT3));
      add = F_PI6;
   }
   F r = f_add(f_mul(a, poly(f_mul(a, a), atn_c, 9)), add);
   if(inv) r = f_sub(F_PI2HI, r);
   if(s) r = f_neg(r);
   return r;
}

F f_asn(F a)
{
   F d = f_sub(F_ONE, f_mul(a, a));
   if(!d.m) {
      F r = F_PI2HI;
      r.s = a.s;
      return r;
   }
   return f_atn(f_div(a, f_sqrt(d)));
}

F f_acs(F a) { return f_sub(F_PI2HI, f_asn(a)); }
F f_deg(F a) { return f_mul(a, F_DEG); }
F f_rad(F a) { return f_mul(a, F_RAD); }
F f_pi(void) { return F_PI; }

//------------------------------------------------------------------
// Random numbers: xorshift32
static u32 rnd_state = 0x12345678;

i32 rnd_next(void)
{
   u32 x = rnd_state;
   x ^= x << 13;
   x ^= x >> 17;
   x ^= x << 5;
   rnd_state = x;
   return x;
}

void rnd_seed(i32 s)
{
   rnd_state = s ? s : 1;
}

F rnd_float(void)
{
   return f_make((u64)(u32)rnd_next() << 32, 0, 0);
}

//------------------------------------------------------------------
// Extended precision multiply, used for decimal conversion
static X xmul(X a, X b)
{
   u32 ah = a.m >> 32, al = a.m, bh = b.m >> 32, bl = b.m;
   u64 hl = (u64)ah * bl, lh = (u64)al * bh;
   u64 mid = (((u64)al * bl) >> 32) + (u32)hl + (u32)lh;
   u64 m = (u64)ah * bh + (hl >> 32) + (lh >> 32) + (mid >> 32);
   a.e += b.e;
   if(!(m >> 63)) { m <<= 1; a.e--; }
   a.m = m;
   return a;
}

// Multiply by 10^n
static X xscale(X x, int n)
{
   const X *t = n < 0 ? pow10n : pow10p;
   if(n < 0) n = -n;
   for(int i = 0; n && i < 6; i++, n >>= 1)
      if(n & 1) x = xmul(x, t[i]);
   if(n) x.e = n > 0 && t == pow10p ? 9999 : -9999;
   return x;
}

//------------------------------------------------------------------
// Parse a number. Sets acc_i or acc_f and returns T_INT or T_REAL.
int parse_num(const u8 **pp)
{
   const u8 *p = *pp;
   u64 m = 0;
   int dexp = 0, real = 0;

   if(*p == '&') {
      u32 v = 0;
      int n = 0;
      for(p++;; p++, n++) {
         u8 c = *p;
         if(c >= '0' && c <= '9') c -= '0';
         else if(c >= 'A' && c <= 'F') c -= 'A' - 10;
         else break;
         v = (v << 4) | c;
      }
      if(!n) err(E_BADHEX);
      *pp = p;
      acc_i = v;
      return T_INT;
   }

   for(;; p++) {
      u8 c = *p;
      if(c == '.') {
         if(real) break;
         real = 1;
         continue;
      }
      if(c < '0' || c > '9') break;
      if(m < 100000000000000000ULL)
         m = m * 10 + (c - '0'), dexp -= real;
      else
         dexp += !real;
   }
   if(*p == 'E') {
      int es = 0, ev = 0;
      p++;
      if(*p == '-') { es = 1; p++; }
      else if(*p == '+') p++;
      while(*p >= '0' && *p <= '9') {
         if(ev < 1000) ev = ev * 10 + *p - '0';
         p++;
      }
      dexp += es ? -ev : ev;
      real = 1;
   }
   *pp = p;

   if(!real && m <= INT32_MAX) {
      acc_i = m;
      return T_INT;
   }

   X x = { m, 64 };
   if(m) {
      while(!(x.m >> 63)) { x.m <<= 1; x.e--; }
      x = xscale(x, dexp);
   }
   if(x.e > 200) err(E_TOOBIG);
   acc_f = x.e < -200 ? F_ZERO : f_make(x.m, x.e, 0);
   return T_REAL;
}

//------------------------------------------------------------------
// Unsigned decimal or hex
static int fmt_uns(char *out, u32 v, int base)
{
   char tmp[12];
   int n = 0;
   do {
      int d = v % base;
      tmp[n++] = d < 10 ? '0' + d : 'A' - 10 + d;
      v /= base;
   } while(v);
   for(int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
   return n;
}

int fmt_int(char *out, i32 v, bool hex)
{
   if(hex) return fmt_uns(out, v, 16);
   if(v < 0) {
      *out = '-';
      return fmt_uns(out + 1, -(u32)v, 10) + 1;
   }
   return fmt_uns(out, v, 10);
}

// Round x*10^n to an integer, returns -1 if it won't fit in 32 bits
static i32 xround(F f, int n)
{
   if(!f.m) return 0;
   X x = { (u64)f.m << 32, f.e };
   x = xscale(x, n);
   if(x.e > 32) return -1;
   if(x.e < 0) return 0;
   u64 v = x.e ? (x.m >> (64 - x.e)) : 0;
   if(x.e < 64 && ((x.m >> (63 - x.e)) & 1)) v++;
   return v > 0xFFFFFFFFULL ? -1 : (i32)v;
}

static const u32 pow10tab[] = {
   1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
   1000000000 };

//------------------------------------------------------------------
// Format a number according to an @% style format word (field width is
// ignored here). Returns the length.
int fmt_num(char *out, int t, u32 fmt)
{
   int mode = (fmt >> 16) & 0xFF;
   int nd = (fmt >> 8) & 0xFF;
   char *o = out;

   F f = t == T_INT ? f_fromint(acc_i) : acc_f;

   if(mode > 2) mode = 0;
   if(nd > 9 || (nd == 0 && mode != 2)) nd = 9;
   if(f.s) *o++ = '-';
   f.s = 0;

   // get the significant digits n, with the first at 10^d. F format
   // rounds from 9 significant digits, like BBC BASIC.
   int sig = mode == 2 ? 9 : nd;
   u32 n = 0;
   int d = 0;
   if(f.m) {
      // estimate the decimal exponent, then correct it
      d = ((f.e - 1) * 77) >> 8;
      for(int i = 0; i < 3; i++) {
         n = xround(f, sig - 1 - d);
         if(n >= pow10tab[sig]) d++;
         else if(n < pow10tab[sig - 1]) d--;
         else break;
      }
   }

   if(mode == 2) {
      // v = n * 10^k has nd decimal places
      int k = nd - 8 + d;
      if(k <= 9 && (k < 0 || (u64)n * pow10tab[k] <= 0xFFFFFFFF)) {
         u32 v = k >= 0 ? n * pow10tab[k] : k < -9 ? 0 :
                 (n + pow10tab[-k] / 2) / pow10tab[-k];
         char dig[12];
         int len = fmt_uns(dig, v, 10);
         int ip = len - nd;
         if(ip <= 0) {
            *o++ = '0';
         } else {
            memcpy(o, dig, ip);
            o += ip;
         }
         if(nd) {
            *o++ = '.';
            for(int i = ip; i < 0; i++) *o++ = '0';
            int skip = ip < 0 ? 0 : ip;
            memcpy(o, dig + skip, len - skip);
            o += len - skip;
         }
         if(out[0] == '-' && v == 0) {
            memmove(out, out + 1, o - out - 1);
            o--;
         }
         return o - out;
      }
      mode = 0;
   }

   char dig[12];
   int len = fmt_uns(dig, n, 10);
   if(!f.m) {
      len = nd;
      memset(dig, '0', nd);
   }
   if(mode == 0) {
      while(len > 1 && dig[len - 1] == '0') len--;
      if(d >= -1 && d < nd) {
         if(d < 0) {
            *o++ = '0';
            *o++ = '.';
            memcpy(o, dig, len);
            return o + len - out;
         }
         for(int i = 0; i <= d || i < len; i++) {
            if(i == d + 1) *o++ = '.';
            *o++ = i < len ? dig[i] : '0';
         }
         return o - out;
      }
   }
   // exponent format
   *o++ = dig[0];
   if(len > 1) {
      *o++ = '.';
      memcpy(o, dig + 1, len - 1);
      o += len - 1;
   }
   *o++ = 'E';
   int el = fmt_int(o, d, false);
   o += el;
   if(mode == 1)
      while(el++ < 3) *o++ = ' ';
   return o - out;
}

//------------------------------------------------------------------
// Convert to and from the 5 byte format used in files by PRINT# (mantissa
// lsb first, then excess 128 exponent).
void f_to_bbc(F f, u8 *b)
{
   u32 m = f.m;
   if(m) m = (m & 0x7FFFFFFF) | ((u32)f.s << 31);
   for(int i = 0; i < 4; i++) b[i] = m >> (i * 8);
   b[4] = f.m ? f.e + 128 : 0;
}

F bbc_to_f(const u8 *b)
{
   F f = F_ZERO;
   u32 m = b[0] | (b[1] << 8) | (b[2] << 16) | ((u32)b[3] << 24);
   if(b[4]) {
      f.s = m >> 31;
      f.m = m | 0x80000000;
      f.e = b[4] - 128;
   }
   return f;
}
