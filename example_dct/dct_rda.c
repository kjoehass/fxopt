#define DCTBLOCKF 24
#define DCTBLOCKE 0
#define RETURNF 24
#define RETURNE 0
#include <stdint.h>
#include <stdio.h>
#define MAX_EXPR(X,Y) ((X) > (Y) ? (X) : (Y))
#define MIN_EXPR(X,Y) ((X) < (Y) ? (X) : (Y))
#define DUMPVAR(A,B) printf(#A" 0X%08X %10.6f\n", A, ((float)A)/(1UL << B));
#define DUMPDBLVAR(A,B) printf(#A" 0X%016llX %10.6f\n", (long long unsigned int) A, ((float)A)/(1UL << B));
int32_t dct (int32_t *dctBlock)
{
  int64_t shft0LL;
  int64_t sround0LL;
  int64_t signbit0LL;
  int64_t round0LL;
  int64_t wide0LL;
  int32_t shft1L;
  int32_t sround1L;
  int32_t signbit1L;
  int32_t round1L;
  int32_t shft2L;
  int32_t sround2L;
  int32_t signbit2L;
  int32_t round2L;
  uint32_t shft2UL;
  int32_t x8;
  int32_t x7;
  int32_t x6;
  int32_t x5;
  int32_t x4;
  int32_t x3;
  int32_t x2;
  int32_t x1;
  int32_t x0;
  int32_t col;
  int32_t row;
  int32_t D2012;
  int32_t D2011;
  int32_t D2010;
  int32_t D2009;
  int32_t D2008;
  int32_t D2007;
  int32_t D2006;
  int32_t D2005;
  int32_t D2004;
  int32_t D2003;
  int32_t D2002;
  int32_t D2001;
  int32_t D2000;
  int32_t *D1999;
  int32_t *D1998;
  int32_t *D1997;
  int32_t *D1996;
  int32_t *D1995;
  int32_t *D1994;
  int32_t *D1993;
  int32_t D1992;
  int32_t D1991;
  int32_t D1990;
  int32_t D1989;
  int32_t D1988;
  int32_t D1987;
  int32_t D1986;
  int32_t D1985;
  int32_t D1984;
  int32_t D1983;
  int32_t D1982;
  int32_t D1981;
  int32_t D1980;
  int32_t *D1979;
  uint32_t D1978;
  uint32_t row_0;
bb2:
  row = 0;
  // DEBUG row => row_3
  goto bb4;
bb3:
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x0 = D1979[0];
  // DEBUG x0 => x0_8
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x1 = D1979[1];
  // DEBUG x1 => x1_12
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x2 = D1979[2];
  // DEBUG x2 => x2_16
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x3 = D1979[3];
  // DEBUG x3 => x3_20
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x4 = D1979[4];
  // DEBUG x4 => x4_24
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x5 = D1979[5];
  // DEBUG x5 => x5_28
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x6 = D1979[6];
  // DEBUG x6 => x6_32
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  x7 = D1979[7];
  // DEBUG x7 => x7_36
  x8 = x7 + x0;
  // DEBUG x8 => x8_37
  x0 = x0 - x7;
  // DEBUG x0 => x0_38
  x7 = x1 + x6;
  // DEBUG x7 => x7_39
  x1 = x1 - x6;
  // DEBUG x1 => x1_40
  x6 = x2 + x5;
  // DEBUG x6 => x6_41
  x2 = x2 - x5;
  // DEBUG x2 => x2_42
  x5 = x3 + x4;
  // DEBUG x5 => x5_43
  x3 = x3 - x4;
  // DEBUG x3 => x3_44
  x4 = x8 + x5;
  // DEBUG x4 => x4_45
  x8 = x8 - x5;
  // DEBUG x8 => x8_46
  x5 = x7 + x6;
  // DEBUG x5 => x5_47
  x7 = x7 - x6;
  // DEBUG x7 => x7_48
  D1980 = x1 + x2;
  wide0LL = (int64_t) D1980 * 2106220352LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  x6 = (int32_t) shft0LL;
  // DEBUG x6 => x6_50
  wide0LL = (int64_t) x2 * -1262586814;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1981 = (int32_t) shft0LL;
  signbit2L = x6 >> 31;
  sround2L = signbit2L + 1;
  round2L = x6 + sround2L;
  shft2L = round2L >> 1;
  x2 = D1981 + shft2L;
  // DEBUG x2 => x2_52
  wide0LL = (int64_t) x1 * -1687267075;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1982 = (int32_t) shft0LL;
  x1 = D1982 + x6;
  // DEBUG x1 => x1_54
  D1983 = x0 + x3;
  wide0LL = (int64_t) D1983 * 1785567396LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  x6 = (int32_t) shft0LL;
  // DEBUG x6 => x6_56
  wide0LL = (int64_t) x3 * -1489322693;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1984 = (int32_t) shft0LL;
  signbit2L = x6 >> 31;
  sround2L = signbit2L + 1;
  round2L = x6 + sround2L;
  shft2L = round2L >> 1;
  x3 = D1984 + shft2L;
  // DEBUG x3 => x3_58
  wide0LL = (int64_t) x0 * -592489406;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 268435456LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 29;
  D1985 = (int32_t) shft0LL;
  signbit1L = D1985 >> 31;
  sround1L = signbit1L + 1;
  round1L = D1985 + sround1L;
  shft1L = round1L >> 1;
  x0 = shft1L + x6;
  // DEBUG x0 => x0_60
  x6 = x4 + x5;
  // DEBUG x6 => x6_61
  x4 = x4 - x5;
  // DEBUG x4 => x4_62
  signbit1L = x0 >> 31;
  sround1L = signbit1L + 1;
  round1L = x0 + sround1L;
  shft1L = round1L >> 1;
  x5 = shft1L + x2;
  // DEBUG x5 => x5_63
  signbit1L = x0 >> 31;
  sround1L = signbit1L + 1;
  round1L = x0 + sround1L;
  shft1L = round1L >> 1;
  x0 = shft1L - x2;
  // DEBUG x0 => x0_64
  signbit2L = x1 >> 31;
  sround2L = signbit2L + 1;
  round2L = x1 + sround2L;
  shft2L = round2L >> 1;
  x2 = x3 + shft2L;
  // DEBUG x2 => x2_65
  signbit2L = x1 >> 31;
  sround2L = signbit2L + 1;
  round2L = x1 + sround2L;
  shft2L = round2L >> 1;
  x3 = x3 - shft2L;
  // DEBUG x3 => x3_66
  D1986 = x7 + x8;
  wide0LL = (int64_t) D1986 * 1162209775LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  x1 = (int32_t) shft0LL;
  // DEBUG x1 => x1_68
  wide0LL = (int64_t) x7 * -1984016189;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1987 = (int32_t) shft0LL;
  signbit2L = x1 >> 31;
  sround2L = signbit2L + 1;
  round2L = x1 + sround2L;
  shft2L = round2L >> 1;
  x7 = D1987 + shft2L;
  // DEBUG x7 => x7_70
  wide0LL = (int64_t) x8 * 1643612827LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1988 = (int32_t) shft0LL;
  x8 = D1988 + x1;
  // DEBUG x8 => x8_72
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  D1979[0] = x6;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  D1979[4] = x4;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  signbit1L = x8 >> 31;
  sround1L = signbit1L + 1;
  round1L = x8 + sround1L;
  shft1L = round1L >> 1;
  D1979[2] = shft1L;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  D1979[6] = x7;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  D1989 = x2 - x5;
  D1979[7] = D1989;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  D1990 = x2 + x5;
  D1979[1] = D1990;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  wide0LL = (int64_t) x3 * 1518500250LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1991 = (int32_t) shft0LL;
  D1979[3] = D1991;
  row_0 = (uint32_t) row;
  D1978 = row_0 * 64;
  shft2UL = D1978 >> 1;
  D1979 = dctBlock + (shft2UL/sizeof(*dctBlock));
  wide0LL = (int64_t) x0 * 1518500250LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D1992 = (int32_t) shft0LL;
  D1979[5] = D1992;
  row = row + 1;
  // DEBUG row => row_101
bb4:
  // row = PHI <row(2), row(3)>
  // DEBUG row => row_1
  if (row <= 7)
    goto bb3;
  else
    goto bb5;
bb5:
  col = 0;
  // DEBUG col => col_102
  goto bb7;
bb6:
  x0 = dctBlock[col];
  // DEBUG x0 => x0_103
  D1993 = dctBlock + (32/sizeof(*dctBlock));
  x1 = D1993[col];
  // DEBUG x1 => x1_105
  D1994 = dctBlock + (64/sizeof(*dctBlock));
  x2 = D1994[col];
  // DEBUG x2 => x2_107
  D1995 = dctBlock + (96/sizeof(*dctBlock));
  x3 = D1995[col];
  // DEBUG x3 => x3_109
  D1996 = dctBlock + (128/sizeof(*dctBlock));
  x4 = D1996[col];
  // DEBUG x4 => x4_111
  D1997 = dctBlock + (160/sizeof(*dctBlock));
  x5 = D1997[col];
  // DEBUG x5 => x5_113
  D1998 = dctBlock + (192/sizeof(*dctBlock));
  x6 = D1998[col];
  // DEBUG x6 => x6_115
  D1999 = dctBlock + (224/sizeof(*dctBlock));
  x7 = D1999[col];
  // DEBUG x7 => x7_117
  x8 = x7 + x0;
  // DEBUG x8 => x8_118
  x0 = x0 - x7;
  // DEBUG x0 => x0_119
  x7 = x1 + x6;
  // DEBUG x7 => x7_120
  x1 = x1 - x6;
  // DEBUG x1 => x1_121
  x6 = x2 + x5;
  // DEBUG x6 => x6_122
  x2 = x2 - x5;
  // DEBUG x2 => x2_123
  x5 = x3 + x4;
  // DEBUG x5 => x5_124
  x3 = x3 - x4;
  // DEBUG x3 => x3_125
  x4 = x8 + x5;
  // DEBUG x4 => x4_126
  x8 = x8 - x5;
  // DEBUG x8 => x8_127
  x5 = x7 + x6;
  // DEBUG x5 => x5_128
  x7 = x7 - x6;
  // DEBUG x7 => x7_129
  D2000 = x1 + x2;
  wide0LL = (int64_t) D2000 * 2106220352LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  x6 = (int32_t) shft0LL;
  // DEBUG x6 => x6_131
  wide0LL = (int64_t) x2 * -1262586814;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2001 = (int32_t) shft0LL;
  signbit2L = x6 >> 31;
  sround2L = signbit2L + 1;
  round2L = x6 + sround2L;
  shft2L = round2L >> 1;
  x2 = D2001 + shft2L;
  // DEBUG x2 => x2_133
  wide0LL = (int64_t) x1 * -1687267075;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2002 = (int32_t) shft0LL;
  x1 = D2002 + x6;
  // DEBUG x1 => x1_135
  D2003 = x0 + x3;
  wide0LL = (int64_t) D2003 * 1785567396LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  x6 = (int32_t) shft0LL;
  // DEBUG x6 => x6_137
  wide0LL = (int64_t) x3 * -1489322693;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2004 = (int32_t) shft0LL;
  signbit2L = x6 >> 31;
  sround2L = signbit2L + 1;
  round2L = x6 + sround2L;
  shft2L = round2L >> 1;
  x3 = D2004 + shft2L;
  // DEBUG x3 => x3_139
  wide0LL = (int64_t) x0 * -592489406;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 268435456LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 29;
  D2005 = (int32_t) shft0LL;
  signbit1L = D2005 >> 31;
  sround1L = signbit1L + 1;
  round1L = D2005 + sround1L;
  shft1L = round1L >> 1;
  x0 = shft1L + x6;
  // DEBUG x0 => x0_141
  x6 = x4 + x5;
  // DEBUG x6 => x6_142
  x4 = x4 - x5;
  // DEBUG x4 => x4_143
  signbit1L = x0 >> 31;
  sround1L = signbit1L + 1;
  round1L = x0 + sround1L;
  shft1L = round1L >> 1;
  x5 = shft1L + x2;
  // DEBUG x5 => x5_144
  signbit1L = x0 >> 31;
  sround1L = signbit1L + 1;
  round1L = x0 + sround1L;
  shft1L = round1L >> 1;
  x0 = shft1L - x2;
  // DEBUG x0 => x0_145
  signbit2L = x1 >> 31;
  sround2L = signbit2L + 1;
  round2L = x1 + sround2L;
  shft2L = round2L >> 1;
  x2 = x3 + shft2L;
  // DEBUG x2 => x2_146
  signbit2L = x1 >> 31;
  sround2L = signbit2L + 1;
  round2L = x1 + sround2L;
  shft2L = round2L >> 1;
  x3 = x3 - shft2L;
  // DEBUG x3 => x3_147
  D2006 = x7 + x8;
  wide0LL = (int64_t) D2006 * 1162209775LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  x1 = (int32_t) shft0LL;
  // DEBUG x1 => x1_149
  wide0LL = (int64_t) x7 * -1984016189;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2007 = (int32_t) shft0LL;
  signbit2L = x1 >> 31;
  sround2L = signbit2L + 1;
  round2L = x1 + sround2L;
  shft2L = round2L >> 1;
  x7 = D2007 + shft2L;
  // DEBUG x7 => x7_151
  wide0LL = (int64_t) x8 * 1643612827LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2008 = (int32_t) shft0LL;
  x8 = D2008 + x1;
  // DEBUG x8 => x8_153
  dctBlock[col] = x6;
  D1996 = dctBlock + (128/sizeof(*dctBlock));
  D1996[col] = x4;
  D1994 = dctBlock + (64/sizeof(*dctBlock));
  signbit1L = x8 >> 31;
  sround1L = signbit1L + 1;
  round1L = x8 + sround1L;
  shft1L = round1L >> 1;
  D1994[col] = shft1L;
  D1998 = dctBlock + (192/sizeof(*dctBlock));
  D1998[col] = x7;
  D1999 = dctBlock + (224/sizeof(*dctBlock));
  D2009 = x2 - x5;
  D1999[col] = D2009;
  D1993 = dctBlock + (32/sizeof(*dctBlock));
  D2010 = x2 + x5;
  D1993[col] = D2010;
  D1995 = dctBlock + (96/sizeof(*dctBlock));
  wide0LL = (int64_t) x3 * 1518500250LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2011 = (int32_t) shft0LL;
  D1995[col] = D2011;
  D1997 = dctBlock + (160/sizeof(*dctBlock));
  wide0LL = (int64_t) x0 * 1518500250LL;
  signbit0LL = wide0LL >> 63;
  sround0LL = signbit0LL + 536870912LL;
  round0LL = wide0LL + sround0LL;
  shft0LL = round0LL >> 30;
  D2012 = (int32_t) shft0LL;
  D1997[col] = D2012;
  col = col + 1;
  // DEBUG col => col_165
bb7:
  // col = PHI <col(5), col(6)>
  // DEBUG col => col_2
  if (col <= 7)
    goto bb6;
  else
    goto bb8;
bb8:
  return 0;
}
