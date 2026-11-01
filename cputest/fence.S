/*
 * CPU test suite - FENCE.
 *
 * On a single hart with no caches or other bus masters, a FENCE has no
 * visible effect, so this just checks that each form executes as an ordinary
 * instruction and memory accesses either side of it behave.
 */
#include "cputest.h"

.option norvc

TEST_FUNC fence
   la    x11, st_buf
   li    x12, 0x5A5A5A5A
   sw    x12, 0(x11)
   fence
   lw    x13, 0(x11)
   CHECK 1, x13, 0x5A5A5A5A

   li    x12, 0x3C3C3C3C
   sw    x12, 0(x11)
   fence rw, rw
   lw    x13, 0(x11)
   CHECK 2, x13, 0x3C3C3C3C

   li    x12, 0x12345678
   sw    x12, 4(x11)
   fence w, r
   lw    x13, 4(x11)
   CHECK 3, x13, 0x12345678

   fence r, w
   fence iorw, iorw
   fence i, o
   fence.tso
   li    x13, 0x77
   CHECK 4, x13, 0x77
TEST_END
