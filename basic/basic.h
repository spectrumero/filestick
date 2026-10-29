#ifndef BASIC_H
#define BASIC_H
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

// BBC BASIC for the FileStick.
//
// Programs are held in memory in the same tokenised format as BBC BASIC II,
// so programs SAVEd by a BBC Micro can be LOADed and vice versa.
//
// Memory is one block allocated with malloc at startup, laid out as on
// the BBC Micro:
//
//   PAGE           program, ending with &0D &FF
//   TOP            end of program
//   LOMEM          variables, arrays and strings, growing upwards to vartop
//   ...            free space
//   HIMEM          the C stack, growing downwards from HIMEM
//
// so the "No room" check is simply the stack pointer meeting vartop.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef int32_t  i32;
typedef uint32_t u32;
typedef uint64_t u64;

// Floating point: BBC BASIC style 40 bit float, value = m/2^32 * 2^e,
// m normalised so bit 31 is set (or m == 0 for zero), s = sign.
typedef struct { u32 m; int16_t e; u8 s; } F;

// Expression types
#define T_STR     0
#define T_INT     1
#define T_REAL    2

// Lvalue types
#define L_STR     0     // string variable (SVar)
#define L_INT     1     // 4 byte integer (variable or !)
#define L_REAL    2     // real variable
#define L_BYTE    3     // ? indirection
#define L_SIND    4     // $ indirection

typedef struct { void *a; int t; } LV;

// A string variable
typedef struct { char *p; u8 len; u8 cap; } SVar;

// Tokens
enum {
   TK_AND = 0x80, TK_DIV, TK_EOR, TK_MOD, TK_OR, TK_ERROR, TK_LINE, TK_OFF,
   TK_STEP, TK_SPC, TK_TAB, TK_ELSE, TK_THEN, TK_LINENO, TK_OPENIN, TK_PTR,
   TK_PAGE, TK_TIME, TK_LOMEM, TK_HIMEM, TK_ABS, TK_ACS, TK_ADVAL, TK_ASC,
   TK_ASN, TK_ATN, TK_BGET, TK_COS, TK_COUNT, TK_DEG, TK_ERL, TK_ERR,
   TK_EVAL, TK_EXP, TK_EXT, TK_FALSE, TK_FN, TK_GET, TK_INKEY, TK_INSTR,
   TK_INT, TK_LEN, TK_LN, TK_LOG, TK_NOT, TK_OPENUP, TK_OPENOUT, TK_PI,
   TK_POINT, TK_POS, TK_RAD, TK_RND, TK_SGN, TK_SIN, TK_SQR, TK_TAN,
   TK_TO, TK_TRUE, TK_USR, TK_VAL, TK_VPOS, TK_CHRS, TK_GETS, TK_INKEYS,
   TK_LEFTS, TK_MIDS, TK_RIGHTS, TK_STRS, TK_STRINGS, TK_EOF, TK_AUTO, TK_DELETE,
   TK_LOAD, TK_LIST, TK_NEW, TK_OLD, TK_RENUMBER, TK_SAVE, TK_CE, TK_PTR2,
   TK_PAGE2, TK_TIME2, TK_LOMEM2, TK_HIMEM2, TK_SOUND, TK_BPUT, TK_CALL, TK_CHAIN,
   TK_CLEAR, TK_CLOSE, TK_CLG, TK_CLS, TK_DATA, TK_DEF, TK_DIM, TK_DRAW,
   TK_END, TK_ENDPROC, TK_ENVELOPE, TK_FOR, TK_GOSUB, TK_GOTO, TK_GCOL, TK_IF,
   TK_INPUT, TK_LET, TK_LOCAL, TK_MODE, TK_MOVE, TK_NEXT, TK_ON, TK_VDU,
   TK_PLOT, TK_PRINT, TK_PROC, TK_READ, TK_REM, TK_REPEAT, TK_REPORT, TK_RESTORE,
   TK_RETURN, TK_RUN, TK_STOP, TK_COLOUR, TK_TRACE, TK_UNTIL, TK_WIDTH, TK_OSCLI
};

// Errors: index into the error table in main.c
enum {
   E_NOROOM, E_STOP, E_SILLY, E_BADPROG, E_RANGE, E_BYTE, E_MISTAKE, E_COMMA,
   E_TYPE, E_NOFN, E_SRANGE, E_QUOTE, E_BADDIM, E_DIMSPACE, E_NOTLOCAL,
   E_NOPROC, E_ARRAY, E_SUBSCRIPT, E_SYNTAX, E_ESCAPE, E_DIVZERO, E_STRLONG,
   E_TOOBIG, E_ROOT, E_LOGRANGE, E_ACCURACY, E_EXPRANGE, E_NOVAR, E_BRACKET,
   E_BADHEX, E_NOSUCHFN, E_ARGS, E_NOFOR, E_CANTMATCH, E_FORVAR,
   E_MANYFOR, E_NOTO, E_MANYGOSUB, E_NOGOSUB, E_ONSYNTAX, E_ONRANGE,
   E_NOLINE, E_DATA, E_NOREPEAT, E_MANYREPEAT, E_HASH, E_NOTFOUND,
   E_CHANNEL, E_EOF, E_BADCMD, E_FILE
};

// Return codes from exec()
#define R_EOL     0     // end of immediate mode line
#define R_ENDPROC 1
#define R_FNRET   2     // '=' at start of statement
#define R_LOCAL   3

#define MAXSTR    255
#define LINEMAX   256   // size of buffers for a tokenised line

// main.c
extern u8 *tp, *curline;
extern u8 *page, *top, *lomem, *vartop, *himem, *ws_start, *ws_end;
extern i32 resint[27];   // @%, A%..Z%
extern bool trace_on;
extern u32 trace_max;
extern u8 errnum;
extern u32 errline;
extern void *onerr;
extern u8 *dataptr, *datastart;
extern int procdepth;
__attribute__((noreturn)) void err(int e);
__attribute__((noreturn)) void end_run(void);
__attribute__((noreturn)) void himem_restart(void);
__attribute__((noreturn)) void run_program(void);
u8 *mem_limit(void);
void check_room(void *p);
u32 line_num(const u8 *p);
u8 *find_line(u32 n);
void jump_line(u32 n);
u8 *skip_line_end(u8 *p);
void new_program(void);
bool old_program(void);
void load_program(const char *name);
void save_program(const char *name);
void delete_lines(u32 a, u32 b);
void renumber(u32 start, u32 step);
void set_auto(u32 start, u32 step);
void set_onerr(u8 *p);
void report(void);

// token.c
bool is_varchar(u8 c);
int tokenise(const u8 *s, u8 *d, bool lnmode);
u32 decode_lineno(const u8 *p);
int encode_lineno(u8 *d, u32 n);
void list_line(u8 *p, int *indent, int listo);

// eval.c
extern i32 acc_i;
extern F   acc_f;
extern char sbuf[256];
extern int slen;
void clear_vars(void);
void *halloc(u32 n);
u8 skipsp(void);
void need_comma(void);
void str_assign(SVar *sv, const char *s, int len);
i32 to_int(int t);
F to_real(int t);
int expr(void);
int factor(void);
i32 expr_int(void);
F expr_real(void);
void expr_str(void);
int get_lv(LV *lv, bool create);
int lv_load(LV *lv);
void lv_store(LV *lv, int t);
void dim_stmt(void);
u8 **find_proc(const u8 *name, int n);
int str_to_num(const u8 **pp);

// stmt.c
int exec(void);
void reset_stacks(void);
int call_fn(bool isfn);

// num.c
extern const F F_ZERO, F_ONE;
F f_make(u64 x, int e, int s);
F f_fromint(i32 v);
i32 f_toint(F a);
int f_floor(F a, i32 *out);
F f_neg(F a);
F f_add(F a, F b);
F f_sub(F a, F b);
F f_mul(F a, F b);
F f_div(F a, F b);
int f_cmp(F a, F b);
F f_sqrt(F a);
F f_ln(F a);
F f_log(F a);
F f_exp(F a);
F f_pow(F a, F b);
F f_sin(F a);
F f_cos(F a);
F f_tan(F a);
F f_atn(F a);
F f_asn(F a);
F f_acs(F a);
F f_deg(F a);
F f_rad(F a);
F f_pi(void);
i32 rnd_next(void);
void rnd_seed(i32 s);
F rnd_float(void);
int parse_num(const u8 **pp);
int fmt_int(char *out, i32 v, bool hex);
int fmt_num(char *out, int t, u32 fmt);
void f_to_bbc(F f, u8 *b);
F bbc_to_f(const u8 *b);

// io.c
extern int pos, vpos, width;
void flush_out(void);
void vdu(int c);
void out_str(const char *s, int len);
void out_cstr(const char *s);
void newline(void);
u32 cycles(void);
u32 get_time(void);
void set_time(i32 t);
void poll_escape(void);
void flush_keys(void);
int getkey(i32 cs);
int read_line(char *buf, int max);
void console_init(void);
void console_restore(void);
i32 open_file(int tok);
i32 channel(void);
void file_close(i32 ch);
i32 file_func(u8 tok, i32 ch);
void file_bput(i32 ch, i32 v);
void file_setptr(i32 ch, i32 v);
void print_file(void);
void input_file(void);
void oscli(const char *cmd, int len);
void call_code(u32 addr, i32 *regs);

// platform
void run_on_stack(void (*fn)(void), void *sp);

static inline u8 *getsp(void) { return (u8 *)__builtin_frame_address(0); }

#endif
