/*
 Name        : main.c
 Author      : KJHass
 */
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * A POSIX.1-2001 implementation of rand() and srand(), taken from the linux
 * man page for rand()
 *
 * Not a great random number generator, but at least it is repeatable on
 * different machines
 */
#define RANDBITS 16
#define MAX_RAND ((int32_t) ((1L << RANDBITS) - 1))

static uint32_t next = 1;

uint32_t myrand(void)
{
  next = next * 1103515245UL + 12345;
  return ((next / 65536) % MAX_RAND);
}

void mysrand(uint32_t seed)
{
  next = seed;
}

/*
 * Prototype for the single-precision floating-point version of the
 * DCT algorithm
 */
void dct2dTestFlt(float (*dctBlockFlt)[8]);

/*
 * dct.c is a link to a fixed-point version of single-precision float version
 *
 * Note that the fixed-point versions were generated by the fxopt plugin, and
 * have #defined constants that we need in main
 *
 */
#include "dct.c"

/*
 * dct2dDblRef was derived from a version of the DCT published in
 * "Algorithm Alley" by Tim Kientzle, Dr. Dobb's Journal, March 1, 1999
 *
 * K. Joseph Hass
 */
/*
 * 2-d Forward DCT implemented directly from the formulas.
 * Very accurate, very slow.
 */
static void dct2dDblRef(double (*data)[8])
{
  static const double PI = 3.14159265358979323;
  double output[8][8];
  short x, y, n, m;
  for (y = 0; y < 8; y++) {
    for (x = 0; x < 8; x++) {
      output[y][x] = 0.0;
      for (n = 0; n < 8; n++) {
        for (m = 0; m < 8; m++) {
          output[y][x] += data[n][m]
              * cos(PI * x * (2 * m + 1) / 16.0) * cos(PI * y *
                                                       (2 * n + 1) / 16.0);
        }
      }
    }
  }
  {
    for (y = 0; y < 8; y++) {
      for (x = 0; x < 8; x++) {
        if (x == 0)
          output[y][x] /= sqrt(2);
        if (y == 0)
          output[y][x] /= sqrt(2);
        data[y][x] = output[y][x];
      }
    }
  }
}
/*
 * The scale factor for converting integer input values to reals
 */
#define FLTSCALE (1UL << (RANDBITS - 1))
/*
 * The scale factor for converting fixed-point results to reals
 */
#define FXSCALE (1UL << (DCTBLOCKF + DCTBLOCKE))
/*
 * MNN is the "most negative number" as it would be in the fixed-point
 * format used for the input data. This value is not allowed to appear
 * in the input data blocks
 */
#define MNN (-1L << (RANDBITS - 1))
/*
 * Number of 8x8 blocks to process when gathering error statistics
 */
#define BLOCKS 256

int main()
{
  double sumsq = 0.0;
  double sumerr = 0.0;
  double maxerr = 0.0;
  double minerr = 0.0;
  int32_t points = 0;
  double the_error, rtsumsq, mse;

  int i, j, blocks;
  int32_t t, intdata[8][8];

  mysrand(1);

  for (blocks = 0; blocks < BLOCKS; blocks++) {
    /*
     * The first block has all values equal to the largest possible value
     */
    if (0 == blocks) {
      for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
          intdata[i][j] = (MAX_RAND >> 1);
        }
      }
    /*
     * The second block has all values equal to the largest negative value
     */
    } else if (1 == blocks) {
      for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
          intdata[i][j] = -(MAX_RAND >> 1);
        }
      }
    /*
     * Values in remaining blocks are randomish
     */
    } else {
      for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
          t = myrand() & MAX_RAND;
          if (t > (MAX_RAND >> 1))
            t -= (MAX_RAND + 1);
          if (MNN == t)
            t++;
          intdata[i][j] = t;
        }
      }
    }

    /*
     * dbldata is the result of the very accurate computation; it is used
     * as a reference for determining the error introduced by the other
     * implementations (single-precision float or fixed-point)
     */
    double dbldata[8][8];

    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
        dbldata[i][j] = 2.0 * ((double) intdata[i][j]) / ((double) FLTSCALE);
      }
    }
    dct2dDblRef(dbldata);

#ifdef FLOATING
    /*
     * Analyze error statistics of a single-precision floating-point version
     * of the DCT, with respect to the very accurate double-precision version
     */
    float fltdata[8][8];

    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
        fltdata[i][j] = ((float) intdata[i][j]) / ((float) FLTSCALE);
      }
    }
    dct2dTestFlt(fltdata);
#endif

    /* 
     *  The integer data needs to be shifted left by the number of empty bits
     *  Note that the number of F bits in the input data is RANDBITS-1
     */
    int32_t dctBlockFx[8][8];
    int32_t *dctFx;
    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
#if (RANDBITS > DCTBLOCKF)
        dctBlockFx[i][j] =
            (intdata[i][j] >> (RANDBITS - 1 - DCTBLOCKF)) << DCTBLOCKE;
#else
        dctBlockFx[i][j] =
            intdata[i][j] << (DCTBLOCKE - RANDBITS + 1 + DCTBLOCKF);
#endif
      }
    }

    dctFx = (int32_t *) & dctBlockFx;
    dct(dctFx);

    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
#ifdef FLOATING
        the_error = (dbldata[i][j]) - fltdata[i][j];
#else
        the_error = (dbldata[i][j])
            - (((double) dctBlockFx[i][j]) / (double) FXSCALE);
#endif
        points++;
        sumerr = sumerr + the_error;
        sumsq = sumsq + (the_error * the_error);
        maxerr = (the_error > maxerr ? the_error : maxerr);
        minerr = (the_error < minerr ? the_error : minerr);
      }
    }
  }
  mse = sumsq / (double) points;
  rtsumsq = pow(mse, (double) 0.5);
  sumerr = sumerr / (double) points;
  printf
      ("Pts: %6d  MSE %12.9f RMS %12.9f max %12.9f min %12.9f avg %12.9f\n",
       points, mse, rtsumsq, maxerr, minerr, sumerr);
  printf
      ("                 %12.5e     %12.5e     %12.5e     %12.5e     %12.5e\n",
       mse, rtsumsq, maxerr, minerr, sumerr);

  return 0;
}
