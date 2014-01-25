/**
 * @file fxopt_range.c
 *
 * @brief  Functions and macros for maintaining range intervals
 *
 * @author K. Joseph Hass
 * @date Created: 2014-01-05T10:07:44-0500
 * @date Last modified: 2014-01-08T09:53:24-0500
 *
 * @details  These functions are used to initialize the range values
 *           (minimum and maximum) for a variable, calculate new range values
 *           after various arithmetic and shift operations, and perform tests
 *           on the range
 *
 * @copyright Copyright (C) 2014 Kenneth Joseph Hass
 *
 * @copyright This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 */
#include "fxopt_plugin.h"

#define UHWI (unsigned HOST_WIDE_INT)

#if HOST_BITS_PER_WIDE_INT == 64
#  define PRINT_DOUBLE_HEX \
     "0x%016" HOST_LONG_FORMAT "x%016" HOST_LONG_FORMAT "x"
#  define PRINT_DOUBLE_HEX_LOW "0x%016" HOST_LONG_FORMAT "x"
#else
#  define PRINT_DOUBLE_HEX \
     "0x%08" HOST_LONG_FORMAT "x%08" HOST_LONG_FORMAT "x"
#  define PRINT_DOUBLE_HEX_LOW "0x%08" HOST_LONG_FORMAT "x"
#endif

#if HOST_BITS_PER_INT == 32
#  define PRINT_DOUBLE_HEX_32 "0x%08x"
#  define UH32 (unsigned int) 
#else
#  define PRINT_DOUBLE_HEX_32 "0x%08" HOST_LONG_FORMAT "x"
#  define UH32 (unsigned long int) 
#endif
#define PRINT_DOUBLE_HEX_16 "0x%04x"

/**
 * @brief Print a gcc double_int
 * @details Print a gcc double_int to stderr, in hexadecimal, using the
 *   appropriate number of characters for the precision of the double_int
 *
 * @param[in] val       double_int to be printed
 * @param[in] precision number of bits of precision in val
 */
void print_double(double_int val, int precision)
{
  if (precision > HOST_BITS_PER_WIDE_INT)
    fprintf(stderr, PRINT_DOUBLE_HEX, UHWI val.high, UHWI val.low);
  else if (precision > 32)
    fprintf(stderr, PRINT_DOUBLE_HEX_LOW, UHWI val.low);
  else if (precision > 16)
    fprintf(stderr, PRINT_DOUBLE_HEX_32, UH32 val.low);
  else
    fprintf(stderr, PRINT_DOUBLE_HEX_16, (unsigned int) (val.low & 0xFFFF));
}


/**
 * @brief Test a double_int for being positive
 * @details Note that positive is not the same as non-negative
 *   (Zero is not considered positive)
 *
 * @param[in] dblint double_int to be tested
 * @return    boolean
 */
bool double_int_positive_p(double_int dblint)
{
  return (!double_int_zero_p(dblint) && !double_int_negative_p(dblint));
}

/**
 * @brief Absolute value function for a double_int
 *
 * @param[in] dblint double_int
 * @return           double_int, absolute value of dblint
 */
double_int double_int_abs(double_int dblint)
{
  if (double_int_negative_p(dblint))
    return double_int_neg(dblint);
  else
    return (dblint);
}

/**
 * @brief Calculate the ceiling of the binary log of the range.
 * 
 * @details Determines the total number of bits required to represent the range
 * of a given SIF structure. Takes absolute value of min and max and ORs their
 * double_int representations, then finds the left-most 1 bit.
 * 
 * @param[in] op_fmt a SIF structure
 * @return    number of I bits required
 */
int ceil_log2_range(struct SIF op_fmt)
{
  double_int onebits;

  onebits = double_int_abs(op_fmt.max);
  onebits = double_int_ior(onebits, double_int_abs(op_fmt.min));

  int ceil = 0;
  while (!(double_int_zero_p(onebits))) {
    onebits =
        double_int_rshift(onebits, 1, HOST_BITS_PER_DOUBLE_INT, LOGICAL);
    ceil++;
  }
  return ceil;
}

/**
 * @brief Checks for a constant value range that is a power of 2.
 *
 * @details If the input SIF represents a constant (op_fmt.max == op_fmt.min),
 * then determine whether that constant is an integer power of 2. If so, return
 * the log2 of the absolute value of the constant, else return -1.
 * Note that this function examines the double_int value of the constant,
 * which is an integer. There is no consideration of the actual binary point
 * location.
 *
 * This function checks the bitwise AND of const_val and const_val-1. If
 * the result is all zeros, then const_val has a single bit set to 1
 *
 * @param[in] op_fmt SIF structure for a variable or constant
 * @return    log2 of a constant
 *              @li k if a constant equal to 2^k or -(2^k)
 *              @li -1 otherwise
 */
int log2_range(struct SIF op_fmt)
{
  if (!double_int_equal_p(op_fmt.max, op_fmt.min))
    return -1; // operand is not a constant

  double_int const_val =
      double_int_abs(double_int_sext(op_fmt.max, op_fmt.size));

  if (double_int_equal_p(double_int_zero,
                         double_int_and(const_val,
                                        double_int_sub(const_val,
                                                       double_int_one)))) {
    return double_int_ctz(const_val);
  } else {
    return -1;
  }
}

/**
 * @brief Calculate range max after shift.
 * 
 * @details Positive shift count is right, negative means left, consistent with
 * double_int_rshift. Does not modify the value stored in the SIF struct.
 * If rounding is enabled, the max value is rounded before shifting.
 * 
 * @todo Should GUARDING be considered?
 * 
 * @param[in] op_fmt SIF struct to evaluate
 * @return double_int new max value after applying shift
 */
double_int new_max(struct SIF op_fmt)
{
  double_int maxval = op_fmt.max;

  if (!double_int_zero_p(maxval) && (op_fmt.shift != 0)) {
    if ((op_fmt.shift > 0) && ROUNDING) {
      double_int constant = double_int_lshift(double_int_one,
                                              (op_fmt.shift - 1),
                                              HOST_BITS_PER_DOUBLE_INT,
                                              LOGICAL);
      if (double_int_negative_p(maxval) && !POSITIVE) {
        constant = double_int_sub(constant, double_int_one);
      }
      maxval = double_int_add(maxval, constant);
    }
    maxval =
        double_int_rshift(maxval, op_fmt.shift, HOST_BITS_PER_DOUBLE_INT,
                          ARITH);

    if (AFFINE) {
      double_int aa_max = new_aa_max(op_fmt);
      if ((double_int_scmp(maxval, aa_max))
          && (double_int_scmp(double_int_neg(maxval), aa_max))) {
        fprintf(stderr, "  !!!!!!! aa max [");
        print_double(aa_max, op_fmt.size);
        fprintf(stderr, "] interval max [");
        print_double(maxval, op_fmt.size);
        fprintf(stderr, "]\n");
        maxval = aa_max;
      }
    }
  }
  return maxval;
}

/**
 * @brief Calculate range min after shift.
 * 
 * @details Positive shift count is right, negative means left, consistent with
 * double_int_rshift. Does not modify the value stored in the SIF struct
 * If rounding is enabled, the min value is rounded before shifting.
 * 
 * @todo Should GUARDING be considered?
 * 
 * @param[in] op_fmt SIF struct to evaluate
 * @return double_int new min value after applying shift
 */
double_int new_min(struct SIF op_fmt)
{
  double_int minval = op_fmt.min;
  if (!double_int_zero_p(minval) && (op_fmt.shift != 0)) {
    if ((op_fmt.shift > 0) && ROUNDING) {
      double_int constant = double_int_lshift(double_int_one,
                                              (op_fmt.shift - 1),
                                              HOST_BITS_PER_DOUBLE_INT,
                                              LOGICAL);
      if (double_int_negative_p(minval) && !POSITIVE) {
        constant = double_int_sub(constant, double_int_one);
      }
      minval = double_int_add(minval, constant);
    }
    minval =
        double_int_rshift(minval, op_fmt.shift, HOST_BITS_PER_DOUBLE_INT,
                          ARITH);

    if (AFFINE) {
      double_int aa_min = new_aa_min(op_fmt);
      if ((double_int_scmp(minval, aa_min))
          && (double_int_scmp(double_int_neg(minval), aa_min))) {
        minval = aa_min;
      }
    }
  }
  return minval;
}

/**
 * @brief Determine whether rounding before a shift could cause an overflow.
 * 
 * @details If the shift will only consume empty bits then no rounding will be
 * done, so return false. Check to see if either end of the interval changes
 * sign after being shifted, when sign extended for the actual precision of the
 * variable.
 * 
 * @param[inout] op_fmt SIF struct of variable
 * @return       boolean true if overflow could occur
 */
int rounding_may_overflow(struct SIF op_fmt)
{
  if (op_fmt.E >= op_fmt.shift)
    return 0;

  op_fmt.max =
      double_int_sext(op_fmt.max, PRECISION(op_fmt) + op_fmt.shift);
  op_fmt.min =
      double_int_sext(op_fmt.min, PRECISION(op_fmt) + op_fmt.shift);
  double_int newmax = double_int_sext(new_max(op_fmt), PRECISION(op_fmt));
  double_int newmin = double_int_sext(new_min(op_fmt), PRECISION(op_fmt));
  return ((double_int_positive_p(op_fmt.max)
           && double_int_negative_p(newmax))
          || (double_int_negative_p(op_fmt.min)
              && double_int_positive_p(newmin)));
}

/**
 * @brief Check for valid range.
 * 
 * @details Verify that the min and max value do not change value after being
 * sign extended to fit in the variable, and that they can be represented in the
 * specified variable size.
 * 
 * @param[in] op_fmt SIF format to check
 * @return none
 */
void check_range(struct SIF op_fmt)
{

  if ((double_int_scmp(op_fmt.max, op_fmt.min) == -1)  // uninitialized range
      || op_fmt.iv                                     // induction variable
      || op_fmt.ptr_op                                 // is a pointer
      || op_fmt.has_attribute)                         // fxfrmt attribute
    return;

  if (format_initialized(op_fmt)) {
    double_int max = double_int_sext(new_max(op_fmt), op_fmt.size);
    if (!double_int_equal_p(max, op_fmt.max)) {
      if (!GUARDING)
        warning(0,
                G_("Maximum value flipped sign when extended, not guarded"));
      else
        warning(0,
                G_("Maximum value flipped sign when extended, guarded"));
    } else if (ceil_log2_range(new_range(op_fmt)) >
               ceil_log2_range(op_fmt))
      warning(0, G_("Maximum value too big for operand size"));

    double_int min = double_int_sext(new_min(op_fmt), op_fmt.size);
    if (!double_int_equal_p(min, op_fmt.min)) {
      if (!GUARDING)
        warning(0,
                G_("Minimum value flipped sign when extended, not guarded"));
      else
        warning(0,
                G_("Minimum value flipped sign when extended, guarded"));
    } else if (ceil_log2_range(new_range(op_fmt)) >
               ceil_log2_range(op_fmt))
      warning(0, G_("Minimum value too big for operand size"));

    if (pessimistic_format(op_fmt))
      fprintf(stderr, "  *** Result format is pessimistic ***\n");

  }
}

/**
 * @brief Print a range's min and max as floating-point and hexadecimal values.
 * 
 * @details Apply any pending shift operations and then convert the double_ints
 * to floating-point values. The values are printed to stderr and followed by
 * a linefeed.
 * 
 * @param[in] op_fmt SIF struct containing range to print
 * @return none
 */
void print_min_max(struct SIF op_fmt)
{
  if (format_initialized(op_fmt)) {
    double_int maxval = new_max(op_fmt);
    double_int minval = new_min(op_fmt);
    HOST_WIDE_INT scale = 1ULL << (op_fmt.F + op_fmt.E);
    if (double_int_fits_in_shwi_p(minval) && double_int_fits_in_shwi_p(maxval)) {
      fprintf(stderr, "  [%+5.3f,%+5.3f]",
              ((float) double_int_to_shwi(minval) / scale),
              ((float) double_int_to_shwi(maxval) / scale));
    }
    fprintf(stderr, " [");
    print_double(minval, op_fmt.size);
    fprintf(stderr, ",");
    print_double(maxval, op_fmt.size);
    fprintf(stderr, "]");
  }
  fprintf(stderr, "\n");
}

/**
 * @brief Compare the ranges of two formats.
 * 
 * @details Apply any pending shift operations and then align the binary points
 * of the min and max values. Compare the min and max values of the two ranges
 * to see how range 1 compares to range 2.
 * 
 * @param[in] fmt1 SIF struct containing first range
 * @param[in] fmt2 SIF struct containing second range
 * @return integer comparison result
 *   @li +1 if range 1 is (at least) partially outside range 2
 *   @li -1 if range 1 is inside range 2
 *   @li  0 if range 1 and range 2 are equal
 */
int range_compare(struct SIF fmt1, struct SIF fmt2)
{
  double_int max1, min1, max2, min2;
  int bp1, bp2, maxcmp, mincmp;

  if ((double_int_scmp(fmt1.max, fmt1.min) == -1) ||
      (double_int_scmp(fmt2.max, fmt2.min) == -1))
    error("fxopt: comparing an undefined range");

  max1 = fmt1.max;
  min1 = fmt1.min;
  bp1 = fmt1.F + fmt1.E;
  max2 = fmt2.max;
  min2 = fmt2.min;
  bp2 = fmt2.F + fmt2.E;

  if (bp1 > bp2) {
    max2 = double_int_lshift(max2, (bp1 - bp2), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
    min2 = double_int_lshift(min2, (bp1 - bp2), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
  } else if (bp2 > bp1) {
    max1 = double_int_lshift(max1, (bp2 - bp1), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
    min1 = double_int_lshift(min1, (bp2 - bp1), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
  }

  //
  // Note comparing minimums in opposite order
  //
  maxcmp = double_int_scmp(max1, max2);
  mincmp = double_int_scmp(min2, min1);

  // Either maxcmp or mincmp = 1 means range 1 is not inside range 2
  if ((maxcmp == 1) || (mincmp == 1))
    return 1;
  // Either maxcmp or mincmp = -1, and the other is 0 or -1, means
  // range 1 is inside range 2
  if ((maxcmp == -1) || (mincmp == -1))
    return -1;
  else
    return 0;
}

/**
 * @brief Determine the larger range max value of two formats.
 * 
 * @details Find the larger maximum value and return it with the bp location
 * of the first format.
 * 
 * @param[in] fmt1 SIF struct containing first range
 * @param[in] fmt2 SIF struct containing second range
 * @return    maximum range with fmt1's binary point
 */
double_int range_max(struct SIF fmt1, struct SIF fmt2)
{
  double_int max1, max2;
  int bp1, bp2, maxcmp;

  if ((double_int_scmp(fmt1.max, fmt1.min) == -1) ||
      (double_int_scmp(fmt2.max, fmt2.min) == -1))
    error("fxopt: comparing an undefined range");

  max1 = fmt1.max;
  bp1 = fmt1.F + fmt1.E;
  max2 = fmt2.max;
  bp2 = fmt2.F + fmt2.E;

  if (bp1 > bp2) {
    max2 = double_int_lshift(max2, (bp1 - bp2), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
  } else if (bp2 > bp1) {
    max1 = double_int_lshift(max1, (bp2 - bp1), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
  }

  maxcmp = double_int_scmp(max1, max2);

  // max2 is larger
  if (maxcmp == -1) {
    if (bp1 > bp2) {
      return max2;
    } else if (bp2 > bp1) {
      max2 = fmt2.max;
      if (ROUNDING) {
        double_int constant = double_int_lshift(double_int_one,
                                              (bp2 - bp2 - 1),
                                              HOST_BITS_PER_DOUBLE_INT,
                                              LOGICAL);
        if (double_int_negative_p(max2) && !POSITIVE)
          constant = double_int_sub(constant, double_int_one);
        max2 = double_int_add(max2, constant);
      }
      return double_int_rshift(max2, (bp2 - bp1), HOST_BITS_PER_DOUBLE_INT,
                             ARITH);
    } else {
      return fmt2.max;
    }
  }

  return fmt1.max;
}

/**
 * @brief Determine the smaller range minimum value of two formats.
 * 
 * @details Find the smaller minimum value and return it with the bp location
 * of the first format.
 * 
 * @param[in] fmt1 SIF struct containing first range
 * @param[in] fmt2 SIF struct containing second range
 * @return    minimum range with fmt1's binary point
 */
double_int range_min(struct SIF fmt1, struct SIF fmt2)
{
  double_int min1, min2;
  int bp1, bp2, mincmp;

  if ((double_int_scmp(fmt1.min, fmt1.min) == -1) ||
      (double_int_scmp(fmt2.min, fmt2.min) == -1))
    error("fxopt: comparing an undefined range");

  min1 = fmt1.min;
  bp1 = fmt1.F + fmt1.E;
  min2 = fmt2.min;
  bp2 = fmt2.F + fmt2.E;

  if (bp1 > bp2) {
    min2 = double_int_lshift(min2, (bp1 - bp2), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
  } else if (bp2 > bp1) {
    min1 = double_int_lshift(min1, (bp2 - bp1), HOST_BITS_PER_DOUBLE_INT,
                             LOGICAL);
  }

  mincmp = double_int_scmp(min2, min1);

  // min2 is smaller
  if (mincmp == -1) {
    if (bp1 > bp2) {
      return min2;
    } else if (bp2 > bp1) {
      min2 = fmt2.min;
      if (ROUNDING) {
        double_int constant = double_int_lshift(double_int_one,
                                              (bp2 - bp2 - 1),
                                              HOST_BITS_PER_DOUBLE_INT,
                                              LOGICAL);
        if (double_int_negative_p(min2) && !POSITIVE)
          constant = double_int_sub(constant, double_int_one);
        min2 = double_int_add(min2, constant);
      }
      return double_int_rshift(min2, (bp2 - bp1), HOST_BITS_PER_DOUBLE_INT,
                             ARITH);
    } else {
      return fmt2.min;
    }
  }

  return fmt1.min;
}

/**
 * @brief Check a SIF format for excess integer bits.
 * 
 * @details Return false if the format has no integer bits or represents the
 * constant zero. Otherwise, calculate the range of the format after applying
 * any shifts, then determine how many integer bits are needed to represent it.
 * If that value is less than the number of bits provided in the format then
 * return true.
 * 
 * @param[in] op_fmt SIF format to check
 * @return boolean
 */
int pessimistic_format(struct SIF op_fmt)
{
  if (op_fmt.I == 0)
    return 0;  // no integer bits to give up
  if (double_int_zero_p(op_fmt.max) && double_int_zero_p(op_fmt.max))
    return 0;  // identically zero
  return (MAX
          (0,
           ((op_fmt.I + op_fmt.F + op_fmt.E) -
            ceil_log2_range(new_range(op_fmt)))));
}

/**
 * @brief Check the range max for most negative number, 0x8000...
 * 
 * @details Create a double_int that is equal to the MNN for the variable's
 * size. Return the result of comparing the MNN value to the actual max value
 * after any shifts have been applied.
 * 
 * @param[in] op_fmt SIF format to be checked
 * @result boolean
 */
int max_is_mnn(struct SIF op_fmt)
{
  double_int mnn = double_int_lshift(double_int_one,
                                     (op_fmt.size - 1),
                                     HOST_BITS_PER_DOUBLE_INT,
                                     LOGICAL);
  double_int maxpos = new_max(op_fmt);
  return (double_int_equal_p(maxpos, mnn));
}

/**
 * @brief Calculate the new min and max after shifting and rounding.
 * 
 * @details Updates min, max and returns a SIF struct with the new values.
 * 
 * @param[in] op_fmt original SIF struct
 * @return SIF copy of op_fmt with updated range
 */
struct SIF new_range(struct SIF op_fmt)
{
  struct SIF new_frmt = op_fmt;

  new_frmt.max = new_max(op_fmt);
  new_frmt.min = new_min(op_fmt);
  return new_frmt;
}

/**
 * @brief Determine the range for the result of addition.
 * 
 * @details Any pending shifts are applied to the operands. The maximum value
 * of the result is the sum of the maximum values of the operands. The minimum
 * value of the result is the sum of the minimum values of the operands. The
 * returned SIF struct is a copy of the input result_frmt with the newly
 * calculated range.
 * 
 * @param[in] oprnd_frmt[] array of SIF format structs for the operands
 * @param[in] result_frmt the original SIF format for the result value
 * @return SIF format struct for the result with updated range
 */
struct SIF new_range_add(struct SIF oprnd_frmt[], struct SIF result_frmt)
{
  struct SIF new_frmt = result_frmt;

  if (AFFINE) {
    new_frmt.aa = affine_add(oprnd_frmt, ADD);
    new_frmt.max = new_aa_max(new_frmt);
    new_frmt.min = new_aa_min(new_frmt);
  } else {
    new_frmt.max =
        double_int_add(new_max(oprnd_frmt[1]), new_max(oprnd_frmt[2]));
    new_frmt.min =
        double_int_add(new_min(oprnd_frmt[1]), new_min(oprnd_frmt[2]));
  }
  return new_frmt;
}

/**
 * @brief Determine the range for the result of subtraction.
 * 
 * @details Any pending shifts are applied to the operands. The maximum value
 * of the result is the maximum value of the first operand minus the minimum
 * value of the second operand. The minimum value of the result is the minimum
 * value of the first operand minus the maximum value of the second operand.
 * The returned SIF struct is a copy of the input result_frmt with the newly
 * calculated range.
 * 
 * @param[in] oprnd_frmt[] array of SIF format structs for the operands
 * @param[in] result_frmt the original SIF format for the result value
 * @return SIF format struct for the result with updated range
 */
struct SIF new_range_sub(struct SIF oprnd_frmt[], struct SIF result_frmt)
{
  struct SIF new_frmt = result_frmt;

  if (AFFINE) {
    new_frmt.aa = affine_add(oprnd_frmt, SUB);
    new_frmt.max = new_aa_max(new_frmt);
    new_frmt.min = new_aa_min(new_frmt);
  } else {
    new_frmt.max =
        double_int_sub(new_max(oprnd_frmt[1]), new_min(oprnd_frmt[2]));
    new_frmt.min =
        double_int_sub(new_min(oprnd_frmt[1]), new_max(oprnd_frmt[2]));
  }
  return new_frmt;
}

/**
 * @brief Determine the range for the result of multiplication.
 * 
 * @details Any pending shifts are applied to the operands. A brute force
 * approach: compute all four products of the min and max values of the two
 * operands and select the minimum and maximum values of those products.
 * 
 * The returned SIF struct is a copy of the input result_frmt with the newly
 * calculated range.
 * 
 * @param[in] oprnd_frmt[] array of SIF format structs for the operands
 * @param[in] result_frmt original SIF format for the result value
 * @return SIF format struct for the result with updated range
 */
struct SIF new_range_mul(struct SIF oprnd_frmt[], struct SIF result_frmt)
{
  struct SIF new_frmt = result_frmt;



  if (AFFINE) {
    struct AA *aa1_list_p = new_aa_list(oprnd_frmt[1]);
    struct AA *aa2_list_p = new_aa_list(oprnd_frmt[2]);
    new_frmt.aa = affine_multiply(aa1_list_p, aa2_list_p);

    delete_aa_list(&(aa1_list_p));
    delete_aa_list(&(aa2_list_p));
    new_frmt.max = new_aa_max(new_frmt);
    new_frmt.min = new_aa_min(new_frmt);
  } else {
    double_int max1 = new_max(oprnd_frmt[1]);
    double_int min1 = new_min(oprnd_frmt[1]);
    double_int max2 = new_max(oprnd_frmt[2]);
    double_int min2 = new_min(oprnd_frmt[2]);

    double_int temp = double_int_mul(max1, max2);
    new_frmt.max = temp;
    new_frmt.min = temp;
    temp = double_int_mul(max1, min2);
    new_frmt.max = double_int_smax(new_frmt.max, temp);
    new_frmt.min = double_int_smin(new_frmt.min, temp);
    temp = double_int_mul(min1, max2);
    new_frmt.max = double_int_smax(new_frmt.max, temp);
    new_frmt.min = double_int_smin(new_frmt.min, temp);
    temp = double_int_mul(min1, min2);
    new_frmt.max = double_int_smax(new_frmt.max, temp);
    new_frmt.min = double_int_smin(new_frmt.min, temp);
  }

  return new_frmt;
}

/**
 * @brief Determine the range for the result of division
 *
 * @details Any pending shifts are applied to the operands, and they
 * are sign-extended. If the range of the divisor includes zero then a
 * divide-by-zero error is possible: warning message is send to stderr and the
 * min/max values of the result are set to the limits of the result's format. If
 * divide-by-zero is not possible, use a brute force approach and calculate the
 * four possible results of dividing the min and max dividend by the min and max
 * divisor. Select the min and max of these results for the min and max of the
 * result.
 * 
 * The returned SIF struct is a copy of the input result_frmt with the newly
 * calculated range.
 *
 * @param[in] oprnd_frmt[] array of SIF format structs for the operands
 * @param[in] result_frmt the original SIF format for the result value
 * @return SIF format struct for the result with updated range
 */
struct SIF new_range_div(struct SIF oprnd_frmt[], struct SIF result_frmt)
{
  struct SIF new_frmt = result_frmt;

  if (AFFINE) {
    struct AA *aa1_list_p = new_aa_list(oprnd_frmt[1]);
    struct AA *aa2_list_p = new_aa_list(oprnd_frmt[2]);
    new_frmt.aa = affine_divide(aa1_list_p, aa2_list_p);
    fix_aa_bp(new_frmt);
    new_frmt.max = new_aa_max(new_frmt);
    new_frmt.min = new_aa_min(new_frmt);

    delete_aa_list(&(aa1_list_p));
    delete_aa_list(&(aa2_list_p));
  } else {
    double_int max1 =
        double_int_sext(new_max(oprnd_frmt[1]), oprnd_frmt[1].size);
    double_int min1 =
        double_int_sext(new_min(oprnd_frmt[1]), oprnd_frmt[1].size);
    double_int max2 =
        double_int_sext(new_max(oprnd_frmt[2]), oprnd_frmt[2].size);
    double_int min2 =
        double_int_sext(new_min(oprnd_frmt[2]), oprnd_frmt[2].size);

    if (double_int_zero_p(min2) || double_int_zero_p(max2) ||
        (double_int_positive_p(max2) && double_int_negative_p(min2))) {
      fprintf(stderr, "  *** Divide by zero possible! *** \n");
      new_frmt.max = double_int_mask(new_frmt.size - 1);
      new_frmt.min = double_int_not(new_frmt.max);
    } else {
      double_int temp = double_int_sdiv(max1, max2, TRUNC_DIV_EXPR);
      new_frmt.max = temp;
      new_frmt.min = temp;
      temp = double_int_sdiv(max1, min2, TRUNC_DIV_EXPR);
      new_frmt.max = double_int_smax(new_frmt.max, temp);
      new_frmt.min = double_int_smin(new_frmt.min, temp);
      temp = double_int_sdiv(min1, max2, TRUNC_DIV_EXPR);
      new_frmt.max = double_int_smax(new_frmt.max, temp);
      new_frmt.min = double_int_smin(new_frmt.min, temp);
      temp = double_int_sdiv(min1, min2, TRUNC_DIV_EXPR);
      new_frmt.max = double_int_smax(new_frmt.max, temp);
      new_frmt.min = double_int_smin(new_frmt.min, temp);
    }
  }

  return new_frmt;
}
