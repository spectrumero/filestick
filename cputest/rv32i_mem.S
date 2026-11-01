/*
 * CPU test suite - RV32I loads and stores.
 *
 * ld_data (framework.S) holds the bytes:
 *   +0:  01 7F FF 80     +4:  0F FF F0 00
 *   +8:  78 56 34 12     +12: EF BE AD DE
 *
 * Misaligned accesses aren't tested: the specification leaves it to the
 * execution environment whether they work or trap.
 */
#include "cputest.h"

.option norvc

TEST_FUNC lb
   TEST_LD         1, lb, 0x00000001, 0, ld_data
   TEST_LD         2, lb, 0x0000007F, 1, ld_data
   TEST_LD         3, lb, 0xFFFFFFFF, 2, ld_data
   TEST_LD         4, lb, 0xFFFFFF80, 3, ld_data
   TEST_LD         5, lb, 0x0000000F, 4, ld_data
   TEST_LD         6, lb, 0xFFFFFFFF, 5, ld_data
   TEST_LD         7, lb, 0xFFFFFFF0, 6, ld_data
   TEST_LD         8, lb, 0x00000000, 7, ld_data
   TEST_LD         9, lb, 0xFFFFFFDE, -1, ld_data_end
   TEST_LD        10, lb, 0x00000078, -8, ld_data_end
   TEST_LD        11, lb, 0x0000007F, 2047, ld_data+1-2047
   TEST_LD        12, lb, 0xFFFFFFAD, -2048, ld_data+14+2048
   TEST_LD_RD_RS1 13, lb, 0x00000034, 10, ld_data
   TEST_LD_X0_RD  14, lb, 3, ld_data
TEST_END

TEST_FUNC lbu
   TEST_LD         1, lbu, 0x00000001, 0, ld_data
   TEST_LD         2, lbu, 0x0000007F, 1, ld_data
   TEST_LD         3, lbu, 0x000000FF, 2, ld_data
   TEST_LD         4, lbu, 0x00000080, 3, ld_data
   TEST_LD         5, lbu, 0x000000F0, 6, ld_data
   TEST_LD         6, lbu, 0x00000000, 7, ld_data
   TEST_LD         7, lbu, 0x000000DE, -1, ld_data_end
   TEST_LD         8, lbu, 0x000000BE, -3, ld_data_end
   TEST_LD         9, lbu, 0x000000FF, 2047, ld_data+5-2047
   TEST_LD        10, lbu, 0x000000EF, -2048, ld_data+12+2048
   TEST_LD_RD_RS1 11, lbu, 0x000000AD, 14, ld_data
   TEST_LD_X0_RD  12, lbu, 3, ld_data
TEST_END

TEST_FUNC lh
   TEST_LD         1, lh, 0x00007F01, 0, ld_data
   TEST_LD         2, lh, 0xFFFF80FF, 2, ld_data
   TEST_LD         3, lh, 0xFFFFFF0F, 4, ld_data
   TEST_LD         4, lh, 0x000000F0, 6, ld_data
   TEST_LD         5, lh, 0x00005678, 8, ld_data
   TEST_LD         6, lh, 0xFFFFDEAD, -2, ld_data_end
   TEST_LD         7, lh, 0xFFFFBEEF, -4, ld_data_end
   TEST_LD         8, lh, 0x00001234, 2046, ld_data+10-2046
   TEST_LD         9, lh, 0xFFFF80FF, -2048, ld_data+2+2048
   TEST_LD_RD_RS1 10, lh, 0x00001234, 10, ld_data
   TEST_LD_X0_RD  11, lh, 2, ld_data
TEST_END

TEST_FUNC lhu
   TEST_LD         1, lhu, 0x00007F01, 0, ld_data
   TEST_LD         2, lhu, 0x000080FF, 2, ld_data
   TEST_LD         3, lhu, 0x0000FF0F, 4, ld_data
   TEST_LD         4, lhu, 0x000000F0, 6, ld_data
   TEST_LD         5, lhu, 0x0000DEAD, -2, ld_data_end
   TEST_LD         6, lhu, 0x0000BEEF, -4, ld_data_end
   TEST_LD         7, lhu, 0x0000FF0F, 2046, ld_data+4-2046
   TEST_LD         8, lhu, 0x0000DEAD, -2048, ld_data+14+2048
   TEST_LD_RD_RS1  9, lhu, 0x000080FF, 2, ld_data
   TEST_LD_X0_RD  10, lhu, 2, ld_data
TEST_END

TEST_FUNC lw
   TEST_LD         1, lw, 0x80FF7F01, 0, ld_data
   TEST_LD         2, lw, 0x00F0FF0F, 4, ld_data
   TEST_LD         3, lw, 0x12345678, 8, ld_data
   TEST_LD         4, lw, 0xDEADBEEF, 12, ld_data
   TEST_LD         5, lw, 0xDEADBEEF, -4, ld_data_end
   TEST_LD         6, lw, 0x80FF7F01, -16, ld_data_end
   TEST_LD         7, lw, 0x80FF7F01, -1, ld_data+1      // unaligned base
   TEST_LD         8, lw, 0x12345678, 3, ld_data+5
   TEST_LD         9, lw, 0x00F0FF0F, 2044, ld_data+4-2044
   TEST_LD        10, lw, 0xDEADBEEF, -2048, ld_data+12+2048
   TEST_LD_RD_RS1 11, lw, 0x12345678, 8, ld_data
   TEST_LD_X0_RD  12, lw, 0, ld_data
TEST_END

// TEST_ST stores to st_buf+4+offset; the word at st_buf+4 starts as
// 0xAAAAAAAA.

TEST_FUNC sb
   TEST_ST  1, sb, 0x12345678, 0, 0xAAAAAA78
   TEST_ST  2, sb, 0x12345678, 1, 0xAAAA78AA
   TEST_ST  3, sb, 0x12345678, 2, 0xAA78AAAA
   TEST_ST  4, sb, 0x12345678, 3, 0x78AAAAAA
   TEST_ST  5, sb, 0xFFFFFF80, 0, 0xAAAAAA80
   TEST_ST  6, sb, 0x00000000, 3, 0x00AAAAAA
   TEST_ST  7, sb, 0x000000FF, 1, 0xAAAAFFAA

   // 8: negative offset
   la    x11, st_buf+4+2048
   li    x12, 0x55555555
   sw    x12, -2048(x11)
   li    x13, 0x000000C3
   sb    x13, -2046(x11)
   lw    x14, -2048(x11)
   CHECK 8, x14, 0x55C35555

   // 9: store x0
   la    x11, st_buf
   li    x12, 0xFFFFFFFF
   sw    x12, 0(x11)
   sb    x0, 1(x11)
   lw    x14, 0(x11)
   CHECK 9, x14, 0xFFFF00FF
TEST_END

TEST_FUNC sh
   TEST_ST  1, sh, 0x12345678, 0, 0xAAAA5678
   TEST_ST  2, sh, 0x12345678, 2, 0x5678AAAA
   TEST_ST  3, sh, 0xFFFF8001, 0, 0xAAAA8001
   TEST_ST  4, sh, 0x0000FFFF, 2, 0xFFFFAAAA
   TEST_ST  5, sh, 0x00000000, 0, 0xAAAA0000

   // 6: negative offset
   la    x11, st_buf+4+2048
   li    x12, 0x55555555
   sw    x12, -2048(x11)
   li    x13, 0x0000C3D2
   sh    x13, -2046(x11)
   lw    x14, -2048(x11)
   CHECK 6, x14, 0xC3D25555

   // 7: store x0
   la    x11, st_buf
   li    x12, 0xFFFFFFFF
   sw    x12, 0(x11)
   sh    x0, 2(x11)
   lw    x14, 0(x11)
   CHECK 7, x14, 0x0000FFFF
TEST_END

TEST_FUNC sw
   TEST_ST  1, sw, 0x12345678, 0, 0x12345678
   TEST_ST  2, sw, 0x00000000, 0, 0x00000000
   TEST_ST  3, sw, 0xFFFFFFFF, 0, 0xFFFFFFFF
   TEST_ST  4, sw, 0x80000001, 0, 0x80000001

   // 5: negative offset
   la    x11, st_buf+4+2048
   li    x13, 0x89ABCDEF
   sw    x13, -2048(x11)
   la    x12, st_buf
   lw    x14, 4(x12)
   CHECK 5, x14, 0x89ABCDEF

   // 6: large positive offset, unaligned base
   la    x11, st_buf+8-2047
   li    x13, 0x13579BDF
   sw    x13, 2047(x11)
   la    x12, st_buf
   lw    x14, 8(x12)
   CHECK 6, x14, 0x13579BDF

   // 7: store x0
   la    x11, st_buf
   li    x12, 0xFFFFFFFF
   sw    x12, 0(x11)
   sw    x0, 0(x11)
   lw    x14, 0(x11)
   CHECK 7, x14, 0

   // 8: store the base register itself
   la    x11, st_buf
   sw    x11, 0(x11)
   lw    x14, 0(x11)
   CHECK_REG 8, x14, x11
TEST_END
