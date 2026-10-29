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


// Tokenising and listing. The keyword table is the one from BBC BASIC II:
// each entry is the keyword, its token and a flags byte.

#include "basic.h"

#define KF_COND   0x01     // not tokenised if followed by an alphanumeric
#define KF_MID    0x02     // now in the middle of a statement
#define KF_START  0x04     // now at the start of a statement
#define KF_FNPROC 0x08     // followed by an FN/PROC name
#define KF_LINE   0x10     // line numbers follow
#define KF_REM    0x20     // don't tokenise the rest of the line
#define KF_PSEUDO 0x40     // pseudo variable: +&40 at start of statement

static const char keywords[] =
   "AND" "\x80\x00"
   "ABS" "\x94\x00"
   "ACS" "\x95\x00"
   "ADVAL" "\x96\x00"
   "ASC" "\x97\x00"
   "ASN" "\x98\x00"
   "ATN" "\x99\x00"
   "AUTO" "\xC6\x10"
   "BGET" "\x9A\x01"
   "BPUT" "\xD5\x03"
   "COLOUR" "\xFB\x02"
   "CALL" "\xD6\x02"
   "CHAIN" "\xD7\x02"
   "CHR$" "\xBD\x00"
   "CLEAR" "\xD8\x01"
   "CLOSE" "\xD9\x03"
   "CLG" "\xDA\x01"
   "CLS" "\xDB\x01"
   "COS" "\x9B\x00"
   "COUNT" "\x9C\x01"
   "DATA" "\xDC\x20"
   "DEG" "\x9D\x00"
   "DEF" "\xDD\x00"
   "DELETE" "\xC7\x10"
   "DIV" "\x81\x00"
   "DIM" "\xDE\x02"
   "DRAW" "\xDF\x02"
   "ENDPROC" "\xE1\x01"
   "END" "\xE0\x01"
   "ENVELOPE" "\xE2\x02"
   "ELSE" "\x8B\x14"
   "EVAL" "\xA0\x00"
   "ERL" "\x9E\x01"
   "ERROR" "\x85\x04"
   "EOF" "\xC5\x01"
   "EOR" "\x82\x00"
   "ERR" "\x9F\x01"
   "EXP" "\xA1\x00"
   "EXT" "\xA2\x01"
   "FOR" "\xE3\x02"
   "FALSE" "\xA3\x01"
   "FN" "\xA4\x08"
   "GOTO" "\xE5\x12"
   "GET$" "\xBE\x00"
   "GET" "\xA5\x00"
   "GOSUB" "\xE4\x12"
   "GCOL" "\xE6\x02"
   "HIMEM" "\x93\x43"
   "INPUT" "\xE8\x02"
   "IF" "\xE7\x02"
   "INKEY$" "\xBF\x00"
   "INKEY" "\xA6\x00"
   "INT" "\xA8\x00"
   "INSTR(" "\xA7\x00"
   "LIST" "\xC9\x10"
   "LINE" "\x86\x00"
   "LOAD" "\xC8\x02"
   "LOMEM" "\x92\x43"
   "LOCAL" "\xEA\x02"
   "LEFT$(" "\xC0\x00"
   "LEN" "\xA9\x00"
   "LET" "\xE9\x04"
   "LOG" "\xAB\x00"
   "LN" "\xAA\x00"
   "MID$(" "\xC1\x00"
   "MODE" "\xEB\x02"
   "MOD" "\x83\x00"
   "MOVE" "\xEC\x02"
   "NEXT" "\xED\x02"
   "NEW" "\xCA\x01"
   "NOT" "\xAC\x00"
   "OLD" "\xCB\x01"
   "ON" "\xEE\x02"
   "OFF" "\x87\x00"
   "OR" "\x84\x00"
   "OPENIN" "\x8E\x00"
   "OPENOUT" "\xAE\x00"
   "OPENUP" "\xAD\x00"
   "OSCLI" "\xFF\x02"
   "PRINT" "\xF1\x02"
   "PAGE" "\x90\x43"
   "PTR" "\x8F\x43"
   "PI" "\xAF\x01"
   "PLOT" "\xF0\x02"
   "POINT(" "\xB0\x00"
   "PROC" "\xF2\x0A"
   "POS" "\xB1\x01"
   "RETURN" "\xF8\x01"
   "REPEAT" "\xF5\x00"
   "REPORT" "\xF6\x01"
   "READ" "\xF3\x02"
   "REM" "\xF4\x20"
   "RUN" "\xF9\x01"
   "RAD" "\xB2\x00"
   "RESTORE" "\xF7\x12"
   "RIGHT$(" "\xC2\x00"
   "RND" "\xB3\x01"
   "RENUMBER" "\xCC\x10"
   "STEP" "\x88\x00"
   "SAVE" "\xCD\x02"
   "SGN" "\xB4\x00"
   "SIN" "\xB5\x00"
   "SQR" "\xB6\x00"
   "SPC" "\x89\x00"
   "STR$" "\xC3\x00"
   "STRING$(" "\xC4\x00"
   "SOUND" "\xD4\x02"
   "STOP" "\xFA\x01"
   "TAN" "\xB7\x00"
   "THEN" "\x8C\x14"
   "TO" "\xB8\x00"
   "TAB(" "\x8A\x00"
   "TRACE" "\xFC\x12"
   "TIME" "\x91\x43"
   "TRUE" "\xB9\x01"
   "UNTIL" "\xFD\x02"
   "USR" "\xBA\x00"
   "VDU" "\xEF\x02"
   "VAL" "\xBB\x00"
   "VPOS" "\xBC\x01"
   "WIDTH" "\xFE\x02"
   "PAGE" "\xD0\x00"
   "PTR" "\xCF\x00"
   "TIME" "\xD1\x00"
   "LOMEM" "\xD2\x00"
   "HIMEM" "\xD3\x00"
   ;

bool is_varchar(u8 c)
{
   return (c >= '0' && c <= '9') || (c >= '@' && c <= 'Z') ||
          (c >= '_' && c <= 'z');
}

static bool is_digit(u8 c)
{
   return c >= '0' && c <= '9';
}

//------------------------------------------------------------------
// Line numbers after GOTO etc. are stored as &8D and three bytes
int encode_lineno(u8 *d, u32 n)
{
   d[0] = TK_LINENO;
   d[1] = (((n & 0xC0) >> 2) | ((n & 0xC000) >> 12)) ^ 0x54;
   d[2] = (n & 0x3F) | 0x40;
   d[3] = ((n >> 8) & 0x3F) | 0x40;
   return 4;
}

// p points after the &8D
u32 decode_lineno(const u8 *p)
{
   u8 b = (p[0] ^ 0x54) << 2;
   u32 lo = (p[1] & 0x3F) | (b & 0xC0);
   u32 hi = (p[2] & 0x3F) | ((b << 2) & 0xC0);
   return (hi << 8) | lo;
}

//------------------------------------------------------------------
// Tokenise the text s into d. Returns the length of the tokenised text,
// which is terminated with &0D. lnmode starts in line number mode (for
// AUTO, DELETE etc. typed at the prompt this isn't needed).
int tokenise(const u8 *s, u8 *d, bool lnmode)
{
   u8 *d0 = d, *dend = d + LINEMAX - 5;
   bool start = true;

   // output is truncated rather than overflowing the buffer
#define PUT(c) do { if(d < dend) *d++ = (c); } while(0)
#define COPY() do { PUT(*s); s++; } while(0)

   while(*s) {
      u8 c = *s;

      if(c == '"') {
         do COPY(); while(*s && *s != '"');
         if(*s) COPY();
         continue;
      }
      if(lnmode && is_digit(c)) {
         u32 n = 0;
         while(is_digit(*s)) n = n * 10 + *s++ - '0';
         if(n > 65279) err(E_SILLY);
         if(d < dend - 4) d += encode_lineno(d, n);
         continue;
      }
      if(c == ',' || c == ' ') {
         COPY();
         continue;
      }
      lnmode = false;
      if(c == ':') {
         start = true;
         COPY();
         continue;
      }
      if(c == '*' && start) {
         while(*s) COPY();
         break;
      }
      if(c == '&') {
         do COPY(); while(is_digit(*s) || (*s >= 'A' && *s <= 'F'));
         continue;
      }
      if(is_digit(c) || c == '.') {
         while(is_digit(*s) || *s == '.') COPY();
         if(*s == 'E') {
            COPY();
            if(*s == '-' || *s == '+') COPY();
            while(is_digit(*s)) COPY();
         }
         start = false;
         continue;
      }
      if(c >= 'A' && c <= 'Z') {
         // look for a keyword
         const char *k = keywords;
         while(*k) {
            int i = 0;
            while((u8)k[i] < 0x80 && k[i] == s[i]) i++;
            const char *e = k;
            while((u8)*e < 0x80) e++;
            u8 tok = e[0], fl = e[1];
            bool match = false;
            if((u8)k[i] >= 0x80) {
               // whole keyword
               match = !((fl & KF_COND) && is_varchar(s[i]));
            } else if(i && s[i] == '.') {
               match = true;
               i++;
            }
            if(match) {
               s += i;
               if((fl & KF_PSEUDO) && start) tok += 0x40;
               PUT(tok);
               if(fl & KF_FNPROC)
                  while(is_varchar(*s)) COPY();
               if(fl & KF_LINE) lnmode = true;
               if(fl & KF_START) start = true;
               if(fl & KF_MID) start = false;
               if(fl & KF_REM)
                  while(*s) COPY();
               break;
            }
            k = e + 2;
         }
         if(*k) continue;
      }
      if(is_varchar(c)) {
         while(is_varchar(*s)) COPY();
         start = false;
         continue;
      }
      COPY();
   }
   *d++ = 0x0D;
   return d - d0;
#undef PUT
#undef COPY
}

//------------------------------------------------------------------
// Name of a token, as a pointer to its entry in the keyword table
const u8 *token_name(u8 t)
{
   const char *k = keywords;
   while(*k) {
      const char *e = k;
      while((u8)*e < 0x80) e++;
      if((u8)*e == t) return (const u8 *)k;
      k = e + 2;
   }
   return NULL;
}

//------------------------------------------------------------------
// Print tokenised text up to &0D
void print_tokens(const u8 *s)
{
   bool quote = false, rem = false;
   char num[12];

   for(; *s != 0x0D; s++) {
      u8 c = *s;
      if(c == '"') quote = !quote;
      if(c < 0x80 || quote || rem) {
         vdu(c);
      } else if(c == TK_LINENO) {
         out_str(num, fmt_int(num, decode_lineno(s + 1), false));
         s += 3;
      } else {
         const u8 *k = token_name(c);
         if(k) while(*k < 0x80) vdu(*k++);
         if(c == TK_REM || c == TK_DATA) rem = true;
      }
   }
}

//------------------------------------------------------------------
// List a program line, with LISTO style indentation
void list_line(u8 *p, int *indent, int listo)
{
   char num[12];
   int n = fmt_int(num, line_num(p), false);
   for(int i = n; i < 5; i++) vdu(' ');
   out_str(num, n);
   if(listo & 1) vdu(' ');

   // indentation: NEXT and UNTIL at the start of a line take effect
   // before the line is printed
   u8 *s = p + 4;
   while(*s == ' ') s++;
   bool q = false;
   int pre = 0, post = 0;
   for(u8 *t = s; *t != 0x0D; t++) {
      if(*t == '"') q = !q;
      if(q) continue;
      if(*t == TK_REM || *t == TK_DATA) break;
      if(*t == TK_LINENO) { t += 3; continue; }
      int d = 0;
      if((listo & 2) && *t == TK_FOR) d = 2;
      if((listo & 2) && *t == TK_NEXT) d = -2;
      if((listo & 4) && *t == TK_REPEAT) d = 2;
      if((listo & 4) && *t == TK_UNTIL) d = -2;
      if(d < 0 && t == s) pre += d;
      else post += d;
   }
   *indent += pre;
   if(*indent < 0) *indent = 0;
   for(int i = 0; i < *indent; i++) vdu(' ');
   *indent += post;
   if(*indent < 0) *indent = 0;
   print_tokens(listo ? s : p + 4);
   newline();
}
