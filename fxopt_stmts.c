/**
 * @file fxopt_stmts.c
 *
 * @brief Functions and macros for processing GIMPLE assignment statements
 *
 * @author K. Joseph Hass
 * @date Created: 2014-01-05T10:20:35-0500
 * @date Last modified: 2014-01-08T12:19:47-0500
 *
 * @details These functions process the operands for the various GIMPLE
 * assignment statements, determining whether any shifts are needed for the
 * operands as well as the format of the result operand.
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

/**
 * @brief Process unary assignment statements.
 * 
 * @details Includes NOP, NEGATE, truncation, casts, variable declarations,
 * and simple assignments from a declared variable to an SSA name.
 * If the size of operand[0] (the LHS) is different from the size of operand[1]
 * (the RHS) then we have a problem...issue a gcc warning.
 * In general, the format of the result is set to be the same as the format
 * of the RHS operand. For memory loads and stores, the pointer is assigned the
 * format of the data that it points to. For NEGATE instructions the minimum
 * value of the result is set to the negative of operand[1]'s maximum, and
 * vice-versa. When storing to memory try to preserve the location of the
 * binary point. If the operands are an entire array, make the same changes
 * to the SIF format structs for every element of the array.
 *
 * Examples of GIMPLE statements processed in this function:
 * @code
 * s1.11_58 = s1;
 * accum_20 = (float) D.1992_19;
 * *D.1985_78[0] = x6_63;
 * D.2004_53 = -s1.9_52;
 * *D.2104_204[col_2] = x4_185;
 * data[0] = InVal_4(D);
 * @endcode
 * 
 * @warning Casts from unsigned to signed may be untested
 * 
 * @param[in] gsi_p statement iterator, points to statement being processed
 * @param[in,out] oprnd_frmt[] array of SIF format structs for all operands
 * @param[in,out] oprnd_tree[] array of gcc trees for the operands
 * @return    SIF format struct for the result
 */
struct SIF nop(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
               tree oprnd_tree[])
{
  bool AA_mode = ADD;
  struct SIF result_frmt;
  initialize_format(&result_frmt);

  gimple stmt = gsi_stmt(*gsi_p);
  oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
  if (! format_initialized(oprnd_frmt[1]))
      return result_frmt;
  //
  // If the RHS is an entire array then the result_frmt is the format
  // of the first element. Doesn't matter, since shift is zero nothing
  // will be done to the elements
  //
  result_frmt = oprnd_frmt[0];
  result_frmt.shift = 0;
  result_frmt.aa = NULL;

  if (oprnd_frmt[0].size == oprnd_frmt[1].size) {
    if (gimple_assign_cast_p(stmt) ||
        (oprnd_frmt[0].sgnd == oprnd_frmt[1].sgnd) ||
        (oprnd_frmt[0].sgnd && (oprnd_frmt[1].ptr_op != 0)
                           && (oprnd_frmt[0].ptr_op == 0)) ||
        (oprnd_frmt[1].sgnd && (oprnd_frmt[0].ptr_op != 0)
                                             && (oprnd_frmt[1].ptr_op == 0))) {
      copy_SIF(&oprnd_frmt[1],&result_frmt);
    } else {
      if (oprnd_frmt[1].sgnd) {
        // Cast signed to unsigned
        // FIXME There be dragons here...just discarding sign bits
        // shift_left(oprnd_frmt, oprnd_tree, 1, oprnd_frmt[1].S);
        warning(0, G_("fxopt: casting signed to unsigned"));
        // FIXME update range
      } else {
        warning(0, G_("fxopt: casting unsigned to signed"));
        shift_right(oprnd_frmt, oprnd_tree, 1, 1);
        // FIXME update range
      }
    }
  } else {
    warning(0,
            G_("fxopt: NOP, LHS has %d bits, RHS has %d bits"),
            oprnd_frmt[0].size, oprnd_frmt[1].size);
  }
  if (NEGATE_EXPR == gimple_assign_rhs_code(stmt)) {
    double_int temp = result_frmt.max;
    result_frmt.max = double_int_neg(result_frmt.min);
    result_frmt.min = double_int_neg(temp);
    AA_mode = SUB;
  }
  //
  // If store to memory, try to do trivial fixes to preserve the pointer
  //   format but copy the new min/max
  //
  if (oprnd_frmt[0].ptr_op && (!oprnd_frmt[1].ptr_op)) {
    //
    // Need to move binary point?
    //
    int bp_diff = (oprnd_frmt[0].S + oprnd_frmt[0].I) -
        (result_frmt.S + result_frmt.I);
    if (bp_diff > 0) {          // move right
      shift_right(oprnd_frmt, oprnd_tree, 1, bp_diff);
    } else if (bp_diff < 0) {   // move left, if extra S bits
      if ((bp_diff + oprnd_frmt[1].S) > 0) {
        shift_left(oprnd_frmt, oprnd_tree, 1, bp_diff);
      } else {
        error("fxopt: could not preserve pointer format");
      }
    }
    result_frmt.S = oprnd_frmt[1].S;
    result_frmt.I = oprnd_frmt[1].I;
    result_frmt.F = oprnd_frmt[1].F;
    result_frmt.E = oprnd_frmt[1].E;
    result_frmt.min = new_min(oprnd_frmt[1]);
    result_frmt.max = new_max(oprnd_frmt[1]);
  }

  result_frmt.E =
      result_frmt.size - result_frmt.S - result_frmt.I - result_frmt.F;

  delete_aa_list(&(result_frmt.aa));
  result_frmt.aa = affine_assign(oprnd_frmt[1].aa, AA_mode);

  //
  // Deal with the other members of an array
  //
  if ((TREE_CODE(TREE_TYPE(oprnd_tree[1])) == ARRAY_TYPE) &&
      (gimple_assign_rhs_code(stmt) == VAR_DECL)) {
    if (NEGATE_EXPR == gimple_assign_rhs_code(stmt))
      fprintf(stderr, "  YIKES, a negated array!\n");
    int low_bound =
        TREE_INT_CST_LOW(TYPE_MIN_VALUE
                         (TYPE_DOMAIN(oprnd_tree[1]->common.type)));
    int high_bound =
        TREE_INT_CST_LOW(TYPE_MAX_VALUE
                         (TYPE_DOMAIN(oprnd_tree[1]->common.type)));
    int elements = high_bound - low_bound + 1;
    //
    // Deal with arrays...create a format for every element
    //
    int element_size = DECL_SIZE(oprnd_tree[1])
        ? (TREE_INT_CST_LOW(DECL_SIZE(oprnd_tree[1])) / elements) : 0;
    if (element_size > 0) {
      int index;
      struct SIF *var_fmt, *op_fmt;
      for (index = low_bound; index <= high_bound; index++) {
        var_fmt = find_var_format(calc_hash_key(oprnd_tree[1], 0, index));
        op_fmt = find_var_format(calc_hash_key(oprnd_tree[0], 0, index));
        op_fmt->S = var_fmt->S;
        op_fmt->I = var_fmt->I;
        op_fmt->F = var_fmt->F;
        op_fmt->E = op_fmt->size - op_fmt->S - op_fmt->I - op_fmt->F;
        op_fmt->min = var_fmt->min;
        op_fmt->max = var_fmt->max;
        op_fmt->sgnd = var_fmt->sgnd;
        op_fmt->shift = 0;
        delete_aa_list(&(op_fmt->aa));
        op_fmt->aa = copy_aa_list(result_frmt.aa);

        fprintf(stderr, "     [%2d]  (%2d/%2d/%2d/%2d)  [%+5.3f,%+5.3f]\n",
                index, op_fmt->S, op_fmt->I, op_fmt->F, op_fmt->E,
                real_min(op_fmt), real_max(op_fmt));
      }
    }
  }
  return result_frmt;
}

/**
 * @brief Process ARRAY_REF (array reference) statements.
 * 
 * @details In general, the format of the result is set to be the same as the
 * format of the RHS operand, considering the array index.
 * 
 * Examples of GIMPLE statements processed by this function:
 * @code
 * D.1989_16 = data[3];
 * x5_28 = *D.1985_27[5];
 * x2_133 = *D.2065_132[col_2];
 * @endcode
 * 
 * @warning Casts from unsigned to signed may be untested
 * 
 * @param[in] gsi_p statement iterator, points to statement being processed
 * @param[in,out] oprnd_frmt array of SIF format structs for all operands
 * @param[in,out] oprnd_tree array of gcc trees for the operands
 * @return    SIF format struct for the result
 */
struct SIF array_ref(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
               tree oprnd_tree[])
{
  bool AA_mode = ADD;
  struct SIF result_frmt;
  initialize_format(&result_frmt);

  gimple stmt = gsi_stmt(*gsi_p);
  oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);

  if (! format_initialized(oprnd_frmt[1]))
      return result_frmt;
  //
  // If the RHS is an entire array then the result_frmt is the format
  // of the first element. Doesn't matter, since shift is zero nothing
  // will be done to the elements
  //
  result_frmt = oprnd_frmt[0];  // shallow copy
  result_frmt.aa = NULL;        // don't let copy_SIF trash op0 aa list
  copy_SIF(&oprnd_frmt[1], &result_frmt);
  //
  // If store to memory, try to do trivial fixes to preserve the pointer
  //   format but copy the new min/max
  //
  if (oprnd_frmt[0].ptr_op && (!oprnd_frmt[1].ptr_op)) {
    //
    // Need to move binary point?
    //
    int bp_diff = (oprnd_frmt[0].S + oprnd_frmt[0].I) -
        (result_frmt.S + result_frmt.I);
    if (bp_diff > 0) {          // move right
      shift_right(oprnd_frmt, oprnd_tree, 1, bp_diff);
    } else if (bp_diff < 0) {   // move left, if extra S bits
      if ((bp_diff + oprnd_frmt[1].S) > 0) {
        shift_left(oprnd_frmt, oprnd_tree, 1, bp_diff);
      } else {
        error("fxopt: could not preserve pointer format");
      }
    }
    result_frmt.min = new_min(oprnd_frmt[1]);
    result_frmt.max = new_max(oprnd_frmt[1]);
  }

  result_frmt.E =
      result_frmt.size - result_frmt.S - result_frmt.I - result_frmt.F;

  delete_aa_list(&(result_frmt.aa));
  result_frmt.aa = affine_assign(oprnd_frmt[1].aa, AA_mode);

  //
  // Deal with the other members of an array
  //
  if ((TREE_CODE(TREE_TYPE(oprnd_tree[1])) == ARRAY_TYPE) &&
      (gimple_assign_rhs_code(stmt) == VAR_DECL)) {
    int low_bound =
        TREE_INT_CST_LOW(TYPE_MIN_VALUE
                         (TYPE_DOMAIN(oprnd_tree[1]->common.type)));
    int high_bound =
        TREE_INT_CST_LOW(TYPE_MAX_VALUE
                         (TYPE_DOMAIN(oprnd_tree[1]->common.type)));
    int elements = high_bound - low_bound + 1;
    //
    // Deal with arrays...create a format for every element
    //
    int element_size = DECL_SIZE(oprnd_tree[1])
        ? (TREE_INT_CST_LOW(DECL_SIZE(oprnd_tree[1])) / elements) : 0;
    if (element_size > 0) {
      int index;
      struct SIF *var_fmt, *op_fmt;
      for (index = low_bound; index <= high_bound; index++) {
        var_fmt = find_var_format(calc_hash_key(oprnd_tree[1], 0, index));
        op_fmt = find_var_format(calc_hash_key(oprnd_tree[0], 0, index));
        op_fmt->S = var_fmt->S;
        op_fmt->I = var_fmt->I;
        op_fmt->F = var_fmt->F;
        op_fmt->E = op_fmt->size - op_fmt->S - op_fmt->I - op_fmt->F;
        op_fmt->min = var_fmt->min;
        op_fmt->max = var_fmt->max;
        op_fmt->sgnd = var_fmt->sgnd;
        op_fmt->shift = 0;
        delete_aa_list(&(op_fmt->aa));
        op_fmt->aa = copy_aa_list(result_frmt.aa);

        fprintf(stderr, "     [%2d]  (%2d/%2d/%2d/%2d)  [%+5.3f,%+5.3f]\n",
                index, op_fmt->S, op_fmt->I, op_fmt->F, op_fmt->E,
                real_min(op_fmt), real_max(op_fmt));
      }
    }
  }
  return result_frmt;
}

/**
 * @brief Process statements that include pointer arithmetic.
 * 
 * @details Includes POINTER_PLUS_EXPR, which adds an offset to a pointer, and
 * MEM_REF, which uses a pointer to access a memory location. The RHS will
 * include a pointer and (possibly) an offset that must be added to the pointer.
 *
 * In GIMPLE the pointer offset is always some number of bytes, regardless
 * of the actual size of the pointee's data type. If the pointer is to a real
 * type then we may need to modify the offset's value for the new size of the
 * replacement real-to-integer type.
 * 
 * The LHS may be a pointer (POINTER_PLUS_EXPR) or a normal variable (MEM_REF)
 * 
 * Examples of GIMPLE statements processed by this function:
 * @code
 * D.1985_27 = dctBlock_6(D) + D.1984_26;
 * D.2065_132 = dctBlock_6(D) + 128;
 * @endcode
 * 
 * @param[in] gsi_p statement iterator, points to statement being processed
 * @param[in,out] oprnd_frmt array of SIF format structs for all operands
 * @param[in,out] oprnd_tree array of gcc trees for the operands
 * @return    SIF format struct for the result
 */
struct SIF pointer_math(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
                        tree oprnd_tree[])
{
  int shift = 0;
  struct SIF result_frmt;
  initialize_format(&result_frmt);

  gimple stmt = gsi_stmt(*gsi_p);
  oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
  if (! format_initialized(oprnd_frmt[1]))
      return result_frmt;

  if (oprnd_frmt[1].ptr_op == 0)
    fatal_error("fxopt: pointer math, first operand not a pointer");

  result_frmt = oprnd_frmt[0];
  result_frmt.aa = NULL;        // don't let copy_SIF trash op0 aa list
  copy_SIF(&oprnd_frmt[1], &result_frmt);
  //
  // Calculate how the offset must be adjusted to account for actual size
  // of fixed-point operand
  //
  if (oprnd_frmt[1].ptr_op > oprnd_frmt[1].size) {
    shift =
        exact_log2((HOST_WIDE_INT) (oprnd_frmt[1].ptr_op / oprnd_frmt[1].size));
  } else if (oprnd_frmt[1].ptr_op < oprnd_frmt[1].size) {
    shift = -exact_log2((HOST_WIDE_INT)
                        (oprnd_frmt[1].size / oprnd_frmt[1].ptr_op));
  }

  if (gimple_assign_rhs_code(stmt) == POINTER_PLUS_EXPR) {
    oprnd_frmt[2] = get_operand_format(stmt, 2, 0, NOPRINT);
    oprnd_frmt[2].shift = shift;
    result_frmt.alias = oprnd_frmt[1].id;
  } else if (gimple_assign_rhs_code(stmt) == MEM_REF) {
    //
    // If the RHS is a MEM_REF then there are really two operands on the RHS
    // although gimple_num_ops() says there is just one. Operand 1 has two
    // arguments: a pointer and an integer constant. The second argument of
    // operand 1 is used as a fake operand 2. Since it is known to be an
    // integer constant the associated SIF format not necessary.
    //
    double_int constant_double = mem_ref_offset(oprnd_tree[1]);
    if (double_int_zero_p(constant_double))
      shift = 0;
    oprnd_frmt[1].shift = shift;
    result_frmt.ptr_op = 0;
    delete_aa_list(&(result_frmt.aa));
    result_frmt.aa = affine_assign(oprnd_frmt[1].aa, ADD);
  } else {
    error("fxopt: pointer math, unexpected operation");
  }

  if (shift != 0)
    fprintf(stderr, "  Pointer stride will be shifted %d\n", shift);

  return result_frmt;
}

/**
 * @brief Process addition and subtraction statements.
 *
 * 
 * Examples of GIMPLE statements processed by this function:
 * @code
 * D.1979_6 = D.1978_5 + c1_2;
 * D.1984_11 = D.1980_7 + D.1983_10;
 * D.2010_60 = s1.11_58 - c1.12_59;
 * @endcode
 * 
 * @param[in] gsi_p statement iterator, points to statement being processed
 * @param[in,out] oprnd_frmt array of SIF format structs for all operands
 * @param[in,out] oprnd_tree array of gcc trees for the operands
 * @return    SIF format struct for the result
 */
struct SIF addition(gimple_stmt_iterator *gsi_p, struct SIF oprnd_frmt[],
                    tree oprnd_tree[])
{
  struct SIF result_frmt;
  initialize_format(&result_frmt);

  gimple stmt = gsi_stmt(*gsi_p);

  //
  // Don't evaluate constants until we really need them, because their formats
  // are not stored and float constants will look like pure integers if
  // evaluated twice
  //
  if (TREE_CONSTANT(oprnd_tree[1])) {
    oprnd_frmt[2] = get_operand_format(stmt, 2, 0, PRINT);
    if (format_initialized(oprnd_frmt[2])) {
      oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
      oprnd_frmt[1].S =
          oprnd_frmt[2].size - oprnd_frmt[1].I - oprnd_frmt[1].F -
          oprnd_frmt[1].E;
    } else {
      return result_frmt;
    }
  } else if (TREE_CONSTANT(oprnd_tree[2])) {
    oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
    if (format_initialized(oprnd_frmt[1])) {
      oprnd_frmt[2] = get_operand_format(stmt, 2, 0, PRINT);
      oprnd_frmt[2].S =
          oprnd_frmt[1].size - oprnd_frmt[2].I - oprnd_frmt[2].F -
          oprnd_frmt[2].E;
    } else {
      return result_frmt;
    }
  } else {
    if (!format_initialized(oprnd_frmt[1])) {
      oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
    }
    if (!format_initialized(oprnd_frmt[2])) {
      oprnd_frmt[2] = get_operand_format(stmt, 2, 0, PRINT);
    }
    if ((!format_initialized(oprnd_frmt[1]))
        || (!format_initialized(oprnd_frmt[2])))
      return result_frmt;
  }
  //
  // Both operands are defined...here we go
  //
  result_frmt = oprnd_frmt[0];
  result_frmt.aa = NULL;
  //
  // Need at least two sign bits for signed addition, at least one for unsigned
  // (oprnd_frmt.sgnd = 1 for signed, so the minimum number of sign bits is 
  // oprnd_frmt[0].sgnd+1). Signed variables are guaranteed to have S > 0.
  //
  int min_s_bits = oprnd_frmt[0].sgnd + 1;
  if (oprnd_frmt[1].S < min_s_bits) {
    fprintf(stderr, "  OP1 needs another sign bit\n");
    shift_right(oprnd_frmt, oprnd_tree, 1, 1);
  }
  if (oprnd_frmt[2].S < min_s_bits) {
    fprintf(stderr, "  OP2 needs another sign bit\n");
    shift_right(oprnd_frmt, oprnd_tree, 2, 1);
  }
  // 
  // Align binary points
  // 
  // need to shift op2 left
  if (BINARY_PT(1) > BINARY_PT(2)) {
    // consume redundant sign bits but keep 1 if unsigned, 2 if signed
    if (oprnd_frmt[2].S > min_s_bits) {
      int shift = MIN((oprnd_frmt[2].S - min_s_bits),
                      (BINARY_PT(1) - BINARY_PT(2)));
      shift_left(oprnd_frmt, oprnd_tree, 2, shift);
    }
    // still not aligned, must shift op1 right
    if (BINARY_PT(1) > BINARY_PT(2)) {
      shift_right(oprnd_frmt, oprnd_tree, 1,
                  (BINARY_PT(1) - BINARY_PT(2)));
    }
  }
  // need to shift op1 left
  if (BINARY_PT(2) > BINARY_PT(1)) {
    // consume redundant sign bits but keep 1 if unsigned, 2 if signed
    if (oprnd_frmt[1].S > min_s_bits) {
      int shift = MIN((oprnd_frmt[1].S - min_s_bits),
                      (BINARY_PT(2) - BINARY_PT(1)));
      shift_left(oprnd_frmt, oprnd_tree, 1, shift);
    }
    // must shift op2 right
    if (BINARY_PT(2) > BINARY_PT(1)) {
      shift_right(oprnd_frmt, oprnd_tree, 2,
                  (BINARY_PT(2) - BINARY_PT(1)));
    }
  }
  if (ROUNDING && GUARDING) {
    if (rounding_may_overflow(oprnd_frmt[1])
        || rounding_may_overflow(oprnd_frmt[2])) {
      // Need one more sign bit in both operands to allow for overflow
      // from rounding.
      fprintf(stderr, "  Rounding may overflow, add another sign bit\n");
      shift_right(oprnd_frmt, oprnd_tree, 1, 1);
      shift_right(oprnd_frmt, oprnd_tree, 2, 1);
    }
  }
  result_frmt.S = MIN(oprnd_frmt[1].S, oprnd_frmt[2].S) - 1;
  result_frmt.I = MAX(oprnd_frmt[1].I, oprnd_frmt[2].I) + 1;
  result_frmt.F = MAX(oprnd_frmt[1].F, oprnd_frmt[2].F);
  result_frmt.E =
      oprnd_frmt[0].size - result_frmt.S - result_frmt.I - result_frmt.F;
  struct SIF temp_frmt;
  initialize_format(&temp_frmt);
  if (gimple_assign_rhs_code(stmt) == MINUS_EXPR)
    temp_frmt = new_range_sub(oprnd_frmt, result_frmt);
  else
    temp_frmt = new_range_add(oprnd_frmt, result_frmt);
  if (pessimistic_format(temp_frmt) && (! oprnd_frmt[0].ptr_op)) {
    fprintf(stderr,
            "  *** Pessimistic addition axiom, ");
    if ((oprnd_frmt[1].shift > 0) && (oprnd_frmt[1].shift > 0)) {
      fprintf(stderr, "giving back a right shift *** \n");
      result_frmt.I -= 1;
      if ((LOST_F_BITS(1) > 0) || (LOST_F_BITS(2) > 0)) 
        result_frmt.F += 1;
      else
        result_frmt.E += 1;
      shift_left(oprnd_frmt, oprnd_tree, 1, 1);
      shift_left(oprnd_frmt, oprnd_tree, 2, 1);
    } else {
      fprintf(stderr, "converting an I to S in result *** \n");
      result_frmt.S += 1;
      result_frmt.I -= 1;
    }
  }                             // axioms are pessimistic
  delete_aa_list(&(result_frmt.aa));
  if (gimple_assign_rhs_code(stmt) == MINUS_EXPR)
    result_frmt = new_range_sub(oprnd_frmt, result_frmt);
  else
    result_frmt = new_range_add(oprnd_frmt, result_frmt);

  fix_aa_bp(result_frmt);
  check_range(result_frmt);
  delete_aa_list(&(temp_frmt.aa));
  return result_frmt;
}

/**
 * @brief Process multiplication statements.
 *
 * @details For multiplication...
 *
 * Examples of GIMPLE statements processed by this function:
 * @code
 * D.1977_4 = D.1976_3 * 2.44140625e-4;
 * D.1978_5 = c2_1 * InVal_4(D);
 * D.2022_75 = D.2021_74 * x0_38;
 * @endcode
 * 
 * @param[in] gsi_p statement iterator, points to statement being processed
 * @param[in,out] oprnd_frmt array of SIF format structs for all operands
 * @param[in,out] oprnd_tree array of gcc trees for the operands
 * @return    SIF format struct for the result
 */
struct SIF multiplication(gimple_stmt_iterator *gsi_p,
                          struct SIF oprnd_frmt[], tree oprnd_tree[])
{
  gimple stmt, new_stmt;
  struct SIF result_frmt;
  initialize_format(&result_frmt);

  stmt = gsi_stmt(*gsi_p);

  oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
  if (format_initialized(oprnd_frmt[1])) {
    //
    // If oprnd_frmt[2] is already initialized then this function was called
    // from the division function, after inverting a constant divisor
    //
    if (!format_initialized(oprnd_frmt[2]))
      oprnd_frmt[2] = get_operand_format(stmt, 2, 0, PRINT);
    if (format_initialized(oprnd_frmt[2])) {
      //
      // Both operands are initialized...here we go
      //
      result_frmt = oprnd_frmt[0];
      result_frmt.aa = NULL;
      /**
       * Special case of operand is a constant == 2^K:
       * Will cause a "virtual shift" of other RHS operand, moving binary
       * point K bits to the right (K > 0) or left (K < 0).
       */ 
      // If log2_range returns -1 then operand is not 2^K
      // This k value is for the double_int stored for the constant and
      // therefore must be non-negative
      //
      int k = log2_range(oprnd_frmt[2]);

      if (k >= 0) {
        // 
        // If the constant has a format with no integer bits then it must
        // have been converted from a real fraction, so the effective K
        // is negative. The virtual shift will remove integer bits, so
        // make sure the other operand has enough of them
        // 
        if ((oprnd_frmt[2].I == 0) && (oprnd_frmt[2].F != 0)) {
          k = k - oprnd_frmt[2].F - oprnd_frmt[2].E;
          if ((k + oprnd_frmt[1].I) < 0)
            error("fxopt: multiply by 2^(-K), K too big");
        }
        if (k > (oprnd_frmt[1].F + oprnd_frmt[1].E))
          error("fxopt: multiply by 2^K, K too big");
        fprintf(stderr, "  virtual shift, binary point moved %d bits\n", k);
        if (lastpass) {
          if (tree_int_cst_sign_bit(oprnd_tree[2])) {
            // constant is -2^k, must negate 
            new_stmt =
                gimple_build_assign_with_ops(NEGATE_EXPR,
                                             oprnd_tree[0], oprnd_tree[1],
                                             NULL);
          } else {
            new_stmt =
                gimple_build_assign_with_ops(NOP_EXPR, oprnd_tree[0],
                                             oprnd_tree[1], NULL);
          }
          //gimple_set_visited(new_stmt, true);
          print_gimple_stmt(stderr, new_stmt, 2, 0);
          gsi_replace(gsi_p, new_stmt, false);
          oprnd_tree[2] = NULL;
        }

        result_frmt.I = oprnd_frmt[1].I + k;
        if (oprnd_frmt[1].F >= k) {
          result_frmt.F = oprnd_frmt[1].F - k;
          result_frmt.E = oprnd_frmt[1].E;
        } else {
          result_frmt.F = 0;
          result_frmt.E = oprnd_frmt[1].E - k;
        }
        delete_aa_list(&(result_frmt.aa));
        result_frmt.aa = shift_aa_list(oprnd_frmt[1], k);
        result_frmt.S = oprnd_frmt[1].S;
        result_frmt.max = oprnd_frmt[1].max;
        result_frmt.min = oprnd_frmt[1].min;
        result_frmt.shift = 0;
      } else {
        /**
         * If either operand is less than 1 (no I bits) and there are X known
         * sign bits to the right of the binary point, then X integer bits in
         * the result will actually be redundant sign bits. But X can't be
         * greater than the number of integer bits in the other operand.
         */
        int fraction_zeros = 0;
        if ((oprnd_frmt[1].I > 0) && (oprnd_frmt[2].I == 0)) {
          fraction_zeros =
              oprnd_frmt[2].F + oprnd_frmt[2].E -
              ceil_log2_range(oprnd_frmt[2]);
          fraction_zeros = MIN(fraction_zeros, oprnd_frmt[1].I);
        }
        if ((oprnd_frmt[2].I > 0) && (oprnd_frmt[1].I == 0)) {
          fraction_zeros =
              oprnd_frmt[1].F + oprnd_frmt[1].E -
              ceil_log2_range(oprnd_frmt[1]);
          fraction_zeros = MIN(fraction_zeros, oprnd_frmt[2].I);
        }
        if (fraction_zeros) {
          fprintf(stderr,
                  "  *** Zeros right of b.p., %d I bits changed to S bits in RSLT\n",
                  fraction_zeros);
        }
        /**
         * If the double precision multiply is enabled, double the operand
         * sizes for the multiplication (unless the operands are so small
         * that the result will fit in current size). This will cause the rhs
         * operands to be cast to the wider type but there is no need to do any
         * shifts to the operands.
         *
         * Don't do a double-precision multiply if it provides no benefit
         * (if the number of info bits in the product will fit in a single
         * precision result). Note that the # of info bits in the operand
         * formats may be larger than necessary if an operand has an fxfrmt
         * attribute.
         */
        int result_info_bits =
            result_frmt.size - result_frmt.sgnd + fraction_zeros;
        int oprnds_info_bits = INFO_BITS(1) + INFO_BITS(2);
        struct SIF tmp_fmt = new_range_mul(oprnd_frmt, result_frmt);
        int new_frmt_info_bits = ceil_log2_range(tmp_fmt);
        delete_aa_list(&(tmp_fmt.aa));
        if (DBL_PRECISION_MULTS) {
          if ((!INTERVAL && (oprnds_info_bits > result_info_bits))
              || (INTERVAL && (new_frmt_info_bits > result_info_bits))) {
            result_frmt.size = 2 * oprnd_frmt[0].size;
            result_info_bits =
                result_frmt.size - result_frmt.sgnd + fraction_zeros;
            oprnd_frmt[1].S += oprnd_frmt[1].size;
            oprnd_frmt[1].size = 2 * oprnd_frmt[1].size;
            oprnd_frmt[2].S += oprnd_frmt[2].size;
            oprnd_frmt[2].size = 2 * oprnd_frmt[2].size;
            //
            // Do implicit cast of RHS to double size of LHS
            //
            if (lastpass)
              gimple_assign_set_rhs_code(gsi_stmt(*gsi_p), WIDEN_MULT_EXPR);
          } else {
            fprintf(stderr,
                    "  *** Double-precision multiply unnecessary ***\n");
          }
        } else {
          /**
           * If there are so many integer bits that the product can't possibly
           * fit in the LHS, we're hosed...can't even do double precision
           * because we can't cast the result back to single
           */
          if ((oprnd_frmt[1].I + oprnd_frmt[2].I) > result_info_bits) {
            error("fxopt: Multiplication impossible, too many I bits");
          }
          // 
          // If product won't fit in LHS, we have to discard bits. Sacrifice
          // one fraction bit at a time from each operand until it fits or we
          // run out of fraction bits
          // 
          while (((INFO_BITS(1) + INFO_BITS(2)) > result_info_bits)
                 && (oprnd_frmt[1].F > 0 || oprnd_frmt[2].F > 0)) {
            // op1 has more information bits
            if (INFO_BITS(1) > INFO_BITS(2)) {
              if (oprnd_frmt[1].F > 0) {
                shift_right(oprnd_frmt, oprnd_tree, 1, 1);
              } else if (oprnd_frmt[2].F > 0) { // no choice!
                shift_right(oprnd_frmt, oprnd_tree, 2, 1);
              }
            }
            // op2 has more or same information bits
            else {
              if (oprnd_frmt[2].F > 0) {
                shift_right(oprnd_frmt, oprnd_tree, 2, 1);
              } else if (oprnd_frmt[1].F > 0) { // no choice!
                shift_right(oprnd_frmt, oprnd_tree, 1, 1);
              }
            }
          }                     // while too many info bits

          if ((INFO_BITS(1) + INFO_BITS(2)) < result_info_bits) {
            //
            // Sometimes we get an unexpected empty bit when shifting
            // a constant right. Give back an info bit.
            //
            fprintf(stderr, "  *** Overoptimized, ");
            if ((INFO_BITS(1) < INFO_BITS(2)) && (LOST_F_BITS(1) > 0)) {
              fprintf(stderr, "shifting op1 left, info bits\n");
              shift_left(oprnd_frmt, oprnd_tree, 1, 1);
            } else if (LOST_F_BITS(2) > 0) {
              fprintf(stderr, "shifting op2 left\n");
              shift_left(oprnd_frmt, oprnd_tree, 2, 1);
            } else if (LOST_F_BITS(1) > 0) {
              fprintf(stderr, "shifting op1 left\n");
              shift_left(oprnd_frmt, oprnd_tree, 1, 1);
            } else {
              fprintf(stderr, "no lost info bits to return\n");
            }

          }                     // over optimized
        }                       // operands must be shifted
        // 
        // Product info bits will fit in result, but we may need to discard
        // empty bits in the operands (i.e. right justify the operands).
        // Start with the operand that has more empty bits.
        // 
        while (((INFO_BITS(1) + oprnd_frmt[1].E +
                 INFO_BITS(2) + oprnd_frmt[2].E) > result_info_bits)
               && (oprnd_frmt[1].E > 0 || oprnd_frmt[2].E > 0)) {
          if (oprnd_frmt[1].E > oprnd_frmt[2].E) {
            shift_right(oprnd_frmt, oprnd_tree, 1, oprnd_frmt[1].E);
          } else {
            shift_right(oprnd_frmt, oprnd_tree, 2, oprnd_frmt[2].E);
          }
        }
        //
        // Check if result could be invalid (most negative number)
        //
        int result_sign_bits =
            result_frmt.size -
            (oprnd_frmt[1].I + oprnd_frmt[2].I - fraction_zeros) -
            (oprnd_frmt[1].F + oprnd_frmt[2].F) -
            (oprnd_frmt[1].E + oprnd_frmt[2].E);
        tmp_fmt = new_range_mul(oprnd_frmt, result_frmt);
        if ((INTERVAL && max_is_mnn(tmp_fmt))
            || ((!INTERVAL) && ROUNDING && (result_sign_bits == 1))) {
          fprintf(stderr, "  *** Adding a sign bit to prevent MNN *** \n");
          if ((INFO_BITS(1) > INFO_BITS(2))
              && ((oprnd_frmt[1].F + oprnd_frmt[1].E) > 0))
            shift_right(oprnd_frmt, oprnd_tree, 1, 1);
          else if ((oprnd_frmt[2].F + oprnd_frmt[2].E) > 0)
            shift_right(oprnd_frmt, oprnd_tree, 2, 1);
          else
            fprintf(stderr, "  *** FAILED to add a sign bit *** \n");
        }
        delete_aa_list(&(tmp_fmt.aa));
        // 
        // Determine product format.
        // 
        result_frmt.I = oprnd_frmt[1].I + oprnd_frmt[2].I - fraction_zeros;
        result_frmt.F = oprnd_frmt[1].F + oprnd_frmt[2].F;
        result_frmt.E = oprnd_frmt[1].E + oprnd_frmt[2].E;
        result_frmt.S =
            result_frmt.size - result_frmt.I - result_frmt.F - result_frmt.E;
        if (result_frmt.S < result_frmt.sgnd)
          error("fxopt: Multiplication FAILED, sign bit is lost");
        //
        // This can happen if the product is less than half the full
        // scale result format range
        //
        tmp_fmt = new_range_mul(oprnd_frmt, result_frmt);
        if ((pessimistic_format(tmp_fmt) >
             fraction_zeros) && (result_frmt.I > 0)) {
          fprintf(stderr, "  *** Pessimistic multiplication axiom, ");
          //
          // Try to give back a fraction bit, but if impossible then convert
          // an integer bit to a redundant sign bit
          //
          if ((INFO_BITS(1) < INFO_BITS(2)) && (oprnd_frmt[1].shift > 0)) {
            fprintf(stderr, "shifting op1 left, info bits\n");
            shift_left(oprnd_frmt, oprnd_tree, 1, 1);
            result_frmt.F += 1;
          } else if (oprnd_frmt[2].shift > 0) {
            fprintf(stderr, "shifting op2 left\n");
            shift_left(oprnd_frmt, oprnd_tree, 2, 1);
            result_frmt.F += 1;
          } else if (oprnd_frmt[1].shift > 0) {
            fprintf(stderr, "shifting op1 left\n");
            shift_left(oprnd_frmt, oprnd_tree, 1, 1);
            result_frmt.F += 1;
          } else {
            fprintf(stderr, "converting I to S in result\n");
            result_frmt.S += 1;
          }
          result_frmt.I -= 1;
        }
        delete_aa_list(&(tmp_fmt.aa));
        tmp_fmt = new_range_mul(oprnd_frmt, result_frmt);
        delete_aa_list(&(result_frmt.aa));
        result_frmt = tmp_fmt; // shallow copy, don't delete tmp_fmt.aa
        result_frmt.shift = 0;
      }                         // not a multiply by 2^K
    }                           // op2 format is initialized
  }                             // op1 format is initialized
  fix_aa_bp(result_frmt);
  check_range(result_frmt);
  return result_frmt;
}
/**
 * @brief  Integer division.
 * @details Determine what formatting is needed for the dividend and divisor
 * before the divsion can be performed, and determine the format for the
 * resulting format.
 *
 * If the option for converting constant division to multiplication by
 * the inverse is enabled, then just calculate the reciprocal of the constant,
 * change the operation to multiplication, and call the function that processes
 * multiplication statements.
 *
 * If the divisor is a constant equal to an integer power of 2, then a virtual
 * shift is performed. The binary point is moved as needed and the statement
 * is converted to a NOP assignment.
 *
 * Examples of GIMPLE statements processed by this function:
 * @code
 * D.1977_4 = D.1976_3 / 4.096e+3;
 * D.1979_8 = D.1978_7 / 5.0e+0;
 * D.1983_11 = InVal_4(D) / D.1982_10;
 * @endcode
 * 
 * @param[in] gsi_p statement iterator, points to statement being processed
 * @param[in,out] oprnd_frmt array of SIF format structs for all operands
 * @param[in,out] oprnd_tree array of gcc trees for the operands
 * @return    SIF format struct for the result
 */
struct SIF division(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
                    tree oprnd_tree[])
{
  gimple stmt, new_stmt;
  struct SIF result_frmt;
  initialize_format(&result_frmt);

  stmt = gsi_stmt(*gsi_p);
  //
  // If divisor is a constant, multiply by the inverse instead
  //
  if (CONST_DIV_TO_MULT && TREE_CONSTANT(oprnd_tree[2])) {
    invert_constant_operand(stmt, 2);
    gimple_assign_set_rhs_code(stmt, MULT_EXPR);
    update_stmt(stmt);
    print_gimple_stmt(stderr, stmt, 2, 0);
    result_frmt = multiplication(gsi_p, oprnd_frmt, oprnd_tree);
  } else {
    oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
    if (format_initialized(oprnd_frmt[1])) {
      oprnd_frmt[2] = get_operand_format(stmt, 2, 0, PRINT);
      if (format_initialized(oprnd_frmt[2])) {
        //
        // Both operands initialized...here we go
        //
        result_frmt = oprnd_frmt[0];
        result_frmt.aa = NULL;
        //
        // Special case of operand is 2^K
        // Will cause a "virtual shift" of other RHS operand
        // 
        // If log2_range returns -1 then operand is not 2^K
        // This k value is for the double_int stored for the constant and
        // therefore must be non-negative
        //
        int k = log2_range(oprnd_frmt[2]);
        if (k >= 0) {
          // 
          // Determine effective exponent for a constant with the given
          // SIF format
          //
          k = k - oprnd_frmt[2].F - oprnd_frmt[2].E;
          //
          // If k > 0, division will move the binary point to the left.
          // Make sure we have enough integer bits in the dividend.
          //
          if (k > oprnd_frmt[1].I)
              error("fxopt: divide by 2^K, K too big");

          if ((k + oprnd_frmt[1].F + oprnd_frmt[1].E) < 0)
            error("fxopt: divide by 2^(-K), K too big");

          fprintf(stderr, "  virtual shift, binary point moved %d bits\n", k);
          if (lastpass) {
            if (tree_int_cst_sign_bit(oprnd_tree[2])) {
              // constant is -2^k, must negate 
              new_stmt =
                  gimple_build_assign_with_ops(NEGATE_EXPR,
                                               oprnd_tree[0], oprnd_tree[1],
                                               NULL);
            } else {
              new_stmt =
                  gimple_build_assign_with_ops(NOP_EXPR, oprnd_tree[0],
                                               oprnd_tree[1], NULL);
            }
            print_gimple_stmt(stderr, new_stmt, 2, 0);
            gsi_replace(gsi_p, new_stmt, false);
            oprnd_tree[2] = NULL;
          }

          result_frmt.I = oprnd_frmt[1].I - k;
          if ((oprnd_frmt[1].F + k) >= 0) {
            result_frmt.F = oprnd_frmt[1].F + k;
            result_frmt.E = oprnd_frmt[1].E;
          } else {
            result_frmt.F = 0;
            result_frmt.E = oprnd_frmt[1].E + k - oprnd_frmt[1].F;
          }
          result_frmt.aa = shift_aa_list(oprnd_frmt[1], -k);
          result_frmt.S = oprnd_frmt[1].S;
          result_frmt.max = oprnd_frmt[1].max;
          result_frmt.min = oprnd_frmt[1].min;
          result_frmt.shift = 0;
        } else {
          // 
          // Extra sign bits in the numerator create extra sign bits in the
          // result, which reduces the number of info bits, so discard them
          // 
          if (oprnd_frmt[1].S > 1)
            shift_left(oprnd_frmt, oprnd_tree, 1, (oprnd_frmt[1].S - 1));
          // 
          // Adjust the number of fraction bits in the numerator to reflect 
          // the number of fraction bit _positions_ rather than actual
          // information fraction bits
          // 
          if (oprnd_frmt[1].F !=
              (oprnd_frmt[1].size - oprnd_frmt[1].S - oprnd_frmt[1].I)) {
            oprnd_frmt[1].F =
                oprnd_frmt[1].size - oprnd_frmt[1].S - oprnd_frmt[1].I;
            oprnd_frmt[1].E = 0;
            fprintf(stderr, "  OP1 cast to (%2d/%2d/%2d/%2d)\n",
                    oprnd_frmt[1].S, oprnd_frmt[1].I, oprnd_frmt[1].F,
                    oprnd_frmt[1].E);
          }
          // 
          // Right-justify the divisor, discarding empty bits
          // 
          shift_right(oprnd_frmt, oprnd_tree, 2, oprnd_frmt[2].E);
          // 
          // Set the number of information bits in the denominator
          // to be no greater than half of the number of information bits
          // in the numerator.
          //
          if ((oprnd_frmt[2].F + oprnd_frmt[2].I) >
              ((oprnd_frmt[1].F + oprnd_frmt[1].I) / 2)) {
            int shiftconst =
                ((oprnd_frmt[2].F + oprnd_frmt[2].I) -
                 (oprnd_frmt[1].F + oprnd_frmt[1].I) / 2);
            shiftconst = MIN(shiftconst, oprnd_frmt[2].F);
            shift_right(oprnd_frmt, oprnd_tree, 2, shiftconst);
            fprintf(stderr, "  OP2 heuristically adjusted for division\n");
          }
          // 
          // The denominator must not have more fraction bits than the
          // numerator...discard fraction bits from the denominator
          // 
          if (oprnd_frmt[2].F > oprnd_frmt[1].F) {
            int shift = oprnd_frmt[2].F - oprnd_frmt[1].F;
            if (shift > 0) {
              shift_right(oprnd_frmt, oprnd_tree, 2, shift);
              fprintf(stderr, "  OP2 truncated to (%2d/%2d/%2d/%2d)\n",
                      oprnd_frmt[2].S, oprnd_frmt[2].I, oprnd_frmt[2].F,
                      oprnd_frmt[2].E);
            } else {
              error("fxopt plugin FAILED to process a division");
              return result_frmt;
            }
          }
          // 
          // The sum of the integer bits in the numerator plus the fraction
          // bits in the denominator must be less than the size of the LHS.
          // The only option is to discard denominator fraction bits
          // 
          if ((oprnd_frmt[1].I + oprnd_frmt[2].F) >= oprnd_frmt[0].size) {
            int shift = oprnd_frmt[0].size - oprnd_frmt[1].I + 1;
            if ((shift > 0) & (shift <= oprnd_frmt[2].F)) {
              shift_right(oprnd_frmt, oprnd_tree, 2, shift);
            } else {
              error("fxopt plugin FAILED to process a division");
              return result_frmt;
            }
          }
          result_frmt.I = oprnd_frmt[1].I + oprnd_frmt[2].F;
          result_frmt.F = oprnd_frmt[1].F - oprnd_frmt[2].F;
          result_frmt.S = oprnd_frmt[1].S;
          result_frmt.E =
              result_frmt.size - result_frmt.S - result_frmt.I -
              result_frmt.F;
      

          struct SIF temp_frmt = new_range_div(oprnd_frmt, result_frmt);
          delete_aa_list(&(result_frmt.aa));
          result_frmt = temp_frmt; // shallow, do not delete temp_frmt.aa
          //
          // Correct the number of true I bits in the result based on the max
          // value of the result
          //
          if (pessimistic_format(result_frmt)) {
            int extraI =
                result_frmt.I + result_frmt.F + result_frmt.E -
                ceil_log2_range(result_frmt);
            extraI = MIN(result_frmt.I, extraI);
            fprintf(stderr, "  %d I bits changed to S bits\n", extraI);
            result_frmt.S += extraI;
            result_frmt.I -= extraI;
          }                       // axioms are pessimistic
          //
          // Check if result could be invalid (most negative number)
          // If so, convert an I bit to an S bit for greater range
          //
          if (max_is_mnn(result_frmt)) {
            fprintf(stderr,
                    "  *** 1 S bit changed to I to prevent MNN *** \n");
            result_frmt.S -= 1;
            result_frmt.I += 1;
          }
        } // op2 is not 2^k
      }                         // op2 format is initialized
    }                           // op1 format is initialized
  }                             // was not converted to a multiplication
  fix_aa_bp(result_frmt);
  check_range(result_frmt);
  return result_frmt;
}
