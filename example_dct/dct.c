/**
 * @file
 *
 * @brief Floating-point code for discrete cosine transform
 *
 * From Figure 1 of Loeffler, Ligtenberg, and Moschytz (LL&M).
 * ("Practical Fast 1-D DCT Algorithms with 11 Multiplications,"
 * Acoustics, Speech, and Signal Processing, 1989. ICASSP-89, 1989.
 * pp 988-991.)
 *
 * Note that the 1-D DCT algorithm in LL&M results in the output
 * scaled by 4*sqrt(2) (i.e., 2 1/2 bits).  After two passes,
 * I need to scale the output by 32 (>>5).
 **/
/*
 * This code was derived from a version of the DCT published in
 * "Algorithm Alley" by Tim Kientzle, Dr. Dobb's Journal, March 1, 1999
 *
 * K. Joseph Hass
 */
#ifndef FORMAT
#  define FORMAT ((fxfrmt(1,7,24,0x00FFFFFF,0xFF000001)))
#endif

void dct2dTestFlt(double __attribute__ FORMAT(*dctBlock)[8])
{
  static const double c1 = 0.98078528040323044912;      //cos(PI/16)
  static const double s1 = 0.19509032201612826784;      //sin(PI/16)
  static const double c3 = 0.83146961230254523708;      //cos(3*PI/16)
  static const double s3 = 0.55557023301960222473;      //sin(3*PI/16)
  static const double r2c6 = 0.54119610014619698441;    //sqrt(2)*cos(6*PI/16)
  static const double r2s6 = 1.30656296487637652784;    //sqrt(2)*sin(6*PI/16);
  static const double r2 = 1.41421356237309504880;      //sqrt(2);
  int row, col;

  for (row = 0; row < 8; row++) {
    double x0 = dctBlock[row][0], x1 = dctBlock[row][1],
           x2 = dctBlock[row][2], x3 = dctBlock[row][3],
           x4 = dctBlock[row][4], x5 = dctBlock[row][5],
           x6 = dctBlock[row][6], x7 = dctBlock[row][7], x8;

    /* Stage 1 */
    x8 = x7 + x0;
    x0 -= x7;
    x7 = x1 + x6;
    x1 -= x6;
    x6 = x2 + x5;
    x2 -= x5;
    x5 = x3 + x4;
    x3 -= x4;

    /* Stage 2 */
    x4 = x8 + x5;
    x8 -= x5;
    x5 = x7 + x6;
    x7 -= x6;

    x6 = c1 * (x1 + x2);
    x2 = (-s1 - c1) * x2 + x6;
    x1 = (s1 - c1) * x1 + x6;

    x6 = c3 * (x0 + x3);
    x3 = (-s3 - c3) * x3 + x6;
    x0 = (s3 - c3) * x0 + x6;

    /* Stage 3 */
    x6 = x4 + x5;
    x4 -= x5;
    x5 = x0 + x2;
    x0 -= x2;
    x2 = x3 + x1;
    x3 -= x1;

    x1 = r2c6 * (x7 + x8);
    x7 = (-r2s6 - r2c6) * x7 + x1;
    x8 = (r2s6 - r2c6) * x8 + x1;

    /* Stage 4 and output */
    dctBlock[row][0] = x6;
    dctBlock[row][4] = x4;
    dctBlock[row][2] = x8;
    dctBlock[row][6] = x7;
    dctBlock[row][7] = x2 - x5;
    dctBlock[row][1] = x2 + x5;
    dctBlock[row][3] = x3 * r2;
    dctBlock[row][5] = x0 * r2;
  }

  for (col = 0; col < 8; col++) {
    double x0 = dctBlock[0][col], x1 = dctBlock[1][col],
        x2 = dctBlock[2][col], x3 = dctBlock[3][col],
        x4 = dctBlock[4][col], x5 = dctBlock[5][col],
        x6 = dctBlock[6][col], x7 = dctBlock[7][col], x8;

    /* Stage 1 */
    x8 = x7 + x0;
    x0 -= x7;
    x7 = x1 + x6;
    x1 -= x6;
    x6 = x2 + x5;
    x2 -= x5;
    x5 = x3 + x4;
    x3 -= x4;

    /* Stage 2 */
    x4 = x8 + x5;
    x8 -= x5;
    x5 = x7 + x6;
    x7 -= x6;

    x6 = c1 * (x1 + x2);
    x2 = (-s1 - c1) * x2 + x6;
    x1 = (s1 - c1) * x1 + x6;

    x6 = c3 * (x0 + x3);
    x3 = (-s3 - c3) * x3 + x6;
    x0 = (s3 - c3) * x0 + x6;

    /* Stage 3 */
    x6 = x4 + x5;
    x4 -= x5;
    x5 = x0 + x2;
    x0 -= x2;
    x2 = x3 + x1;
    x3 -= x1;

    x1 = r2c6 * (x7 + x8);
    x7 = (-r2s6 - r2c6) * x7 + x1;
    x8 = (r2s6 - r2c6) * x8 + x1;

    /* Stage 4 and output */
    dctBlock[0][col] = x6;
    dctBlock[4][col] = x4;
    dctBlock[2][col] = x8;
    dctBlock[6][col] = x7;
    dctBlock[7][col] = (x2 - x5);
    dctBlock[1][col] = (x2 + x5);
    dctBlock[3][col] = (x3 * r2);
    dctBlock[5][col] = (x0 * r2);
  }
}
