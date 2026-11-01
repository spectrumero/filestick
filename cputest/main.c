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

// CPU test suite: checks the unprivileged RV32IMC instructions against the
// RISC-V specification.
//
// cputest.elf [-v] [-f] [test ...]
//    -v    list every test run, not just the failures
//    -f    also test FENCE (traps on a core that doesn't implement it)
//    test  run only the named tests, e.g. "cputest.elf mulhsu c.lw"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

enum group { GRP_I, GRP_FENCE, GRP_M, GRP_C };

static const char *group_names[] = {
   [GRP_I]     = "RV32I",
   [GRP_FENCE] = "RV32I",
   [GRP_M]     = "RV32M",
   [GRP_C]     = "RV32C",
};

#define TEST(fn, name, grp) int test_##fn(void);
#include "tests.def"
#undef TEST

struct test {
   int (*fn)(void);
   const char *name;
   enum group grp;
};

static const struct test tests[] = {
#define TEST(fn, name, grp) { test_##fn, name, grp },
#include "tests.def"
#undef TEST
};

#define NTESTS (sizeof(tests) / sizeof(tests[0]))

extern uint32_t test_got, test_exp;

static void usage(void)
{
   printf("usage: cputest.elf [-v] [-f] [test ...]\n"
          "  -v  list every test, not just failures\n"
          "  -f  include FENCE (traps if not implemented)\n");
}

static int selected(const struct test *t, int argc, char **argv, int first,
                    int fence)
{
   if(first >= argc)
      return t->grp != GRP_FENCE || fence;

   for(int i = first; i < argc; i++)
      if(!strcmp(argv[i], t->name))
         return 1;
   return 0;
}

int main(int argc, char **argv)
{
   int verbose = 0, fence = 0, first = 1;

   for(; first < argc && argv[first][0] == '-'; first++) {
      if(!strcmp(argv[first], "-v"))
         verbose = 1;
      else if(!strcmp(argv[first], "-f"))
         fence = 1;
      else {
         usage();
         return 1;
      }
   }

   for(int i = first; i < argc; i++) {
      size_t j;
      for(j = 0; j < NTESTS; j++)
         if(!strcmp(argv[i], tests[j].name))
            break;
      if(j == NTESTS) {
         printf("No test called %s\n", argv[i]);
         return 1;
      }
   }

   printf("RV32IMC CPU tests\n");

   int run = 0, failed = 0;
   const char *group = NULL;
   for(size_t i = 0; i < NTESTS; i++) {
      const struct test *t = &tests[i];
      if(!selected(t, argc, argv, first, fence))
         continue;

      if(verbose && group != group_names[t->grp]) {
         group = group_names[t->grp];
         printf("%s\n", group);
      }

      // print the name first, so it's on screen if the test crashes
      if(verbose) {
         printf("  %-12s ", t->name);
         fflush(stdout);
      }

      int rc = t->fn();
      run++;
      if(rc == 0) {
         if(verbose)
            printf("ok\n");
      } else {
         failed++;
         if(!verbose)
            printf("  %-12s ", t->name);
         printf("FAIL case %d: got 0x%08" PRIx32 ", expected 0x%08" PRIx32 "\n",
                rc, test_got, test_exp);
      }
   }

   printf("%d tests, %d passed, %d failed\n", run, run - failed, failed);
   if(!fence && first >= argc)
      printf("(FENCE not tested, use -f)\n");
   return failed ? 1 : 0;
}
