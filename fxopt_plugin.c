/**
 * @file fxopt_plugin.c
 *
 * @brief The main file for the fxopt plugin
 *
 * @author K. Joseph Hass
 * @date Created: 2014-01-04T17:17:55-0500
 * @date Last modified: 2014-01-08T12:06:58-0500
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
 * @def   INFO_BITS
 * @brief Number of bits in format that convey magnitude information.
 */
#define INFO_BITS(x) (oprnd_frmt[(x)].I+oprnd_frmt[(x)].F)
/**
 * @def   BINARY_PT
 * @brief Number of bits to the right of the binary point.
 */
#define BINARY_PT(x) (oprnd_frmt[(x)].F+oprnd_frmt[(x)].E)
/**
 * @var   lastpass
 * @brief Set to true when all formats are resolved and code can be generated.
 */
int lastpass = 0;
/**
 * @var   fxpass
 * @brief Counts number of passes through the function in attempt to resolve
 *   all formats.
 */
int fxpass = 0;

//
// global variables to indicate which optimizations have been selected
//
int INTERVAL = 0;
int AFFINE   = 0;
int GUARDING = 0;
int ROUNDING = 0;
int POSITIVE = 0;
int DBL_PRECISION_MULTS = 0;
int CONST_DIV_TO_MULT = 0;

int plugin_is_GPL_compatible;

// Attribute handler callback

static tree
handle_user_attribute(tree * node, tree name, tree args,
                      int flags, bool * no_add_attrs)
{
  return NULL_TREE;
}

// Attribute definition
//
// The fxfrmt attribute specifies the SIF format of a variable
// The five attributes are S bits, I bits, F bits, maximum integer value,
// and minimum integer value 
//
static struct attribute_spec frmt_attr =
    { "fxfrmt", 1, 5, false, false, false, handle_user_attribute };
//
// The fxiter attribute marks a variable as being used iteratively.
// If the format of an iterative variable changes, or its range gets wider,
//   during one pass through a function then we upate the variable's format
//   and range information then repeat processing in the hope that the format
//   and range will converge to a stable value.
//
static struct attribute_spec iter_attr =
    { "fxiter", 0, 0, false, false, false, handle_user_attribute };


/// Register the fxopt attributes so gcc will recognize them

///
/// \param event_data pointer to void
/// \param data       pointer to  void
///
static void register_attributes(void *event_data, void *data)
{
  register_attribute(&frmt_attr);
  register_attribute(&iter_attr);
}


/**
 * @brief The main function that is invoked for fxopt
 * @details 
 *
 * @return boolean, success or failure
 */
static unsigned int execute_fxopt_plugin(void)
{
  gimple_stmt_iterator gsi;
  gimple stmt;
  basic_block bb;
  tree var, innertype;
  referenced_var_iterator rvi;
  struct SIF *new_format_p = NULL;

  fprintf(stderr, "  ===== Setting formats of declared vars =====\n");
  FOR_EACH_REFERENCED_VAR(cfun, var, rvi) {
    int iter, has_attr, is_ptr, s_bits, i_bits, f_bits, low_bound, high_bound,
        elements, element_size, i;
    double_int max, min;

    low_bound = high_bound = 0;
    iter = has_attr = is_ptr = s_bits = i_bits = f_bits = 0;
    max = double_int_zero;      // special "uninitialized" values
    min = double_int_one;
    // 
    // Set the fixed-point format according to the fxfrmt attribute, if
    // present, or leave it uninitialized.
    // 
    /**
     * @todo Do error checking on fxfrmt attribute parameters. Verify that
     * S+I+F fits in the variable. Verify that max and min are reasonable
     * for given format.
     */
    tree attrlist = DECL_ATTRIBUTES(var);
    if (attrlist != NULL_TREE) {
      tree attr, next_tree;
      for (attr = lookup_attribute("fxfrmt", attrlist);
           attr != NULL_TREE;
           attr = lookup_attribute("fxfrmt", TREE_CHAIN(attr))) {
        s_bits = TREE_INT_CST_LOW(TREE_VALUE(TREE_VALUE(attr)));
        next_tree = TREE_CHAIN(TREE_VALUE(attr));
        if (next_tree != NULL_TREE) {
          i_bits = TREE_INT_CST_LOW(TREE_VALUE(next_tree));
          next_tree = TREE_CHAIN(next_tree);
          if (next_tree != NULL_TREE) {
            f_bits = TREE_INT_CST_LOW(TREE_VALUE(next_tree));
            has_attr = 1;
            next_tree = TREE_CHAIN(next_tree);
            if (next_tree != NULL_TREE) {
              max = tree_to_double_int(TREE_VALUE(next_tree));
              next_tree = TREE_CHAIN(next_tree);
              if (next_tree != NULL_TREE) {
                min = tree_to_double_int(TREE_VALUE(next_tree));
              }
            }
          }
        }
      }
      for (attr = lookup_attribute("fxiter", attrlist);
           attr != NULL_TREE;
           attr = lookup_attribute("fxiter", TREE_CHAIN(attr))) {
        iter = 1;
      }
    }
    innertype = get_innermost_type(var);
    //
    // For arrays we need to know the range of the index so we can
    //   create a format for each element
    // A scalar is just an array with one element
    //
    if (TREE_CODE(TREE_TYPE(var)) == ARRAY_TYPE) {
      low_bound =
          TREE_INT_CST_LOW(TYPE_MIN_VALUE(TYPE_DOMAIN(var->common.type)));
      high_bound =
          TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(var->common.type)));
    }
    //
    // The is_ptr field gets the size of the data being referenced, before
    //   any conversion from real to integer. We need this to fix the stride
    //   for pointer references later.
    //
    if ((TREE_CODE(TREE_TYPE(var)) == POINTER_TYPE) ||
        (TREE_CODE(TREE_TYPE(var)) == REFERENCE_TYPE)) {
      is_ptr = TREE_INT_CST_LOW(TYPE_SIZE(innertype));
      //
      // if it's a pointer to an array, determine array bounds
      //
      if (TREE_CODE(TREE_TYPE(TREE_TYPE(var))) == ARRAY_TYPE) {
        tree array_tree = TREE_TYPE(var);
        low_bound =
            TREE_INT_CST_LOW(TYPE_MIN_VALUE
                             (TYPE_DOMAIN(array_tree->common.type)));
        high_bound =
            TREE_INT_CST_LOW(TYPE_MAX_VALUE
                             (TYPE_DOMAIN(array_tree->common.type)));
      }
    }
    //
    // If the variable has an initial value, convert it from real to integer
    // (if necessary) and set its format according to the initial value
    //
    if ((TREE_CODE(var) == VAR_DECL) && (DECL_INITIAL(var) != NULL_TREE)) {
      tree initial = DECL_INITIAL(var); // a constant or constructor tree
      new_format_p = find_var_format(calc_hash_key(var, 0, NOT_AN_ARRAY));
      if (TREE_CODE(initial) == REAL_CST) {
        convert_real_constant(initial, new_format_p);
      } else if (TREE_CODE(initial) == INTEGER_CST) {
        int_constant_format(initial, new_format_p);
      } else if (TREE_CODE(initial) == CONSTRUCTOR) {
        unsigned HOST_WIDE_INT ix;
        tree field, val;
        FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(initial), ix, field, val) {
          i = TREE_INT_CST_LOW(field);  // index into the vector
          new_format_p = find_var_format(calc_hash_key(var, 0, i));
          if (TREE_CODE(val) == REAL_CST) {
            convert_real_constant(val, new_format_p);
          } else if (TREE_CODE(val) == INTEGER_CST) {
            int_constant_format(val, new_format_p);
          } else {
            fprintf(stderr, " *** Unexpected initial constructor element\n");
          }
        }
      } else {
        fprintf(stderr, " *** Unexpected initial value tree\n");
      }
    }
    //
    // If there was not an initial value, set the variable's format here
    //
    elements = high_bound - low_bound + 1;
    if (TREE_CODE(innertype) == REAL_TYPE) {
      element_size = TREE_INT_CST_LOW(TYPE_SIZE(REAL_TO_INTEGER_TYPE));
    } else if ((TREE_CODE(TREE_TYPE(var)) == POINTER_TYPE) ||
               (TREE_CODE(TREE_TYPE(var)) == REFERENCE_TYPE)) {
      if (TREE_CODE(TREE_TYPE(TREE_TYPE(var))) == ARRAY_TYPE) {
        tree array_tree = TREE_TYPE(TREE_TYPE(var));
        element_size =
            TYPE_SIZE(array_tree) ? (TREE_INT_CST_LOW(TYPE_SIZE(array_tree)) /
                                     elements) : 0;
      }
    } else if ((TREE_CODE(var) == VAR_DECL) || (TREE_CODE(var) == PARM_DECL)) {
      element_size =
          DECL_SIZE(var) ? (TREE_INT_CST_LOW(DECL_SIZE(var)) / elements) : 0;
    } else {
      fprintf(stderr, " *** Can't compute element size!\n");
    }

    int is_signed = TYPE_UNSIGNED(innertype) ? 0 : 1;
    if ((! has_attr) && iter) { // assumed format for iterative variables
      s_bits = 1;
      i_bits = 0;
      f_bits = element_size - 1;
    }
    int e_bits = element_size - s_bits - i_bits - f_bits;
    //
    // If the variable is a parameter declaration, document its format
    // in the transcript. This is useful for generating the fixed-point
    // C program.
    //
    if ((TREE_CODE(var) == PARM_DECL) && has_attr) {
      fprintf(stderr, "/// %sF %d\n", IDENTIFIER_POINTER(DECL_NAME(var)),
          f_bits);
      fprintf(stderr, "/// %sE %d\n", IDENTIFIER_POINTER(DECL_NAME(var)),
          e_bits);
    }
    //
    // If the variable had an fxfrmt or fxiter attribute but the min and max
    // were not specified, calculate default values for them
    //
    if (double_int_zero_p(max) && double_int_one_p(min) && (has_attr || iter)) {
      max = double_int_mask(i_bits + f_bits);

      if (is_signed) {
        min = double_int_neg(max);
        min = double_int_lshift(min, e_bits, HOST_BITS_PER_DOUBLE_INT, ARITH);
      } else {
        min = double_int_zero;
      }

      max = double_int_lshift(max, e_bits, HOST_BITS_PER_DOUBLE_INT, ARITH);
    }

    if (is_signed) {
      max = double_int_sext(max, element_size);
      min = double_int_sext(min, element_size);
    }

    if (element_size > 0) {     // ignore void types, like .MEM
      double_int x0 = double_int_rshift(double_int_add(max, min), 1,
                                          HOST_BITS_PER_DOUBLE_INT, ARITH);
      double_int x1 = double_int_rshift(double_int_sub(max, min), 1,
                                           HOST_BITS_PER_DOUBLE_INT, ARITH);
      int bp = f_bits + e_bits;
      for (i = low_bound; i < NOT_AN_ARRAY; i++) {
        if ((i > high_bound) || (low_bound == high_bound))
          i = NOT_AN_ARRAY; // hack!
        new_format_p = find_var_format(calc_hash_key(var, 0, i));
        if (!format_initialized(*new_format_p)) {
          new_format_p->attrS = new_format_p->S = s_bits;
          new_format_p->attrI = new_format_p->I = i_bits;
          new_format_p->attrF = new_format_p->F = f_bits;
          new_format_p->attrE = new_format_p->E = e_bits;
          new_format_p->attrmax = new_format_p->max = max;
          new_format_p->attrmin = new_format_p->min = min;
          if (has_attr || iter) {
            if (!double_int_zero_p(x0))
              append_aa_var(&(new_format_p->aa), 0, x0, bp);
            if (!double_int_zero_p(x1))
              append_aa_var(&(new_format_p->aa), new_format_p->id, x1, bp);
          }
          new_format_p->has_attribute = has_attr;
          new_format_p->ptr_op = is_ptr;  // not boolean, an integer
          new_format_p->size = element_size;
          new_format_p->sgnd = is_signed;
          new_format_p->shift = 0;
          new_format_p->iv = 0;
          new_format_p->alias = 0;
          new_format_p->iter = iter;
        }
      }
    }                           // if element_size > 0
  }                             // for each VAR

  //
  //   Look for conditional statements. If the conditional has a RHS
  //   that is an integer constant then we want to mark the underlying 
  //   variable as an induction variable and set has_attribute to force this
  //   format to be used for all SSA_NAMES associated with the var
  //
  fprintf(stderr, "  ===== Marking induction variables =====\n");
  FOR_EACH_BB(bb) {
    for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
      stmt = gsi_stmt(gsi);
      if (gimple_code(stmt) == GIMPLE_COND) {
        if (SSA_NAME == TREE_CODE(gimple_cond_lhs(stmt))) {
          var = SSA_NAME_VAR(gimple_cond_lhs(stmt));
          if (INTEGER_CST == TREE_CODE(gimple_cond_rhs(stmt))) {
            new_format_p = find_var_format(calc_hash_key(var, 0, NOT_AN_ARRAY));
            int varsize = new_format_p->size;
            int_constant_format(gimple_cond_rhs(stmt), new_format_p);
            new_format_p->iv = 1;
            new_format_p->size = varsize;
          } else {
            fprintf(stderr, "RHS of a GIMPLE_COND not an integer!\n");
          }
        } else {
          fprintf(stderr, "LHS of a GIMPLE_COND not an SSA_NAME!\n");
        }
      }
    }
  }

  fprintf(stderr, "  ===== Marking induction variable statements =====\n");
  FOR_EACH_BB(bb) {
    for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
      stmt = gsi_stmt(gsi);
      if (gimple_code(stmt) == GIMPLE_ASSIGN) {
        int num_operands = gimple_num_ops(stmt);
        struct SIF oprnd_frmt[num_operands];
        tree *oprnd_tree = gimple_ops(stmt);

        int i;
        for (i = 0; i < num_operands; i++)
          initialize_format(&oprnd_frmt[i]);

        oprnd_frmt[0] = get_operand_format(stmt, 0, 0, NOPRINT);
        if (oprnd_frmt[0].ptr_op == 0) {
          if (!TREE_CONSTANT(oprnd_tree[1])) {
            oprnd_frmt[1] = get_operand_format(stmt, 1, 0, NOPRINT);
          }
          if (oprnd_frmt[1].iv) {
            oprnd_frmt[0].iv = 1;
            set_var_format(oprnd_tree[0], oprnd_frmt[0]);
          }
        }
        if (oprnd_frmt[0].iv) {
          gimple_set_plf(stmt, GF_PLF_1, true);
        } else {
          gimple_set_plf(stmt, GF_PLF_1, false);
        }
      }
    }
  }
  fprintf(stderr, "  ===== Marking induction variable statements 2 =====\n");
  FOR_EACH_BB(bb) {
    for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
      stmt = gsi_stmt(gsi);
      if ((gimple_code(stmt) == GIMPLE_ASSIGN)
          && (!gimple_plf(stmt, GF_PLF_1))) {
        int num_operands = gimple_num_ops(stmt);
        struct SIF oprnd_frmt[num_operands];
        tree *oprnd_tree = gimple_ops(stmt);

        int i;
        for (i = 0; i < num_operands; i++)
          initialize_format(&oprnd_frmt[i]);

        oprnd_frmt[0] = get_operand_format(stmt, 0, 0, NOPRINT);
        if (oprnd_frmt[0].ptr_op == 0) {
          if (!TREE_CONSTANT(oprnd_tree[1])) {
            oprnd_frmt[1] = get_operand_format(stmt, 1, 0, NOPRINT);
          }
          if (oprnd_frmt[1].iv) {
            oprnd_frmt[0].iv = 1;

            fprintf(stderr, "  ========== Changed iv =====\n");
            set_var_format(oprnd_tree[0], oprnd_frmt[0]);
          }
        }
        if (oprnd_frmt[0].iv) {
          gimple_set_plf(stmt, GF_PLF_1, true);
          fprintf(stderr, "  ========== Changed PLF =====\n");
        } else {
          gimple_set_plf(stmt, GF_PLF_1, false);
        }
      }
    }
  }

  lastpass = 0;
  do {  // until lastpass
    if (lastpass) {
      fprintf(stderr, "  ===== Beginning lastpass =====\n");
      FOR_EACH_REFERENCED_VAR(cfun, var, rvi) {
        //
        // If the variable has an initial value, convert it from real to integer
        // (if necessary) and set its format according to the initial value
        //
        if ((TREE_CODE(var) == VAR_DECL) && (DECL_INITIAL(var) != NULL_TREE)) {
          tree initial = DECL_INITIAL(var);
          new_format_p = find_var_format(calc_hash_key(var, 0, NOT_AN_ARRAY));
          if (TREE_CODE(initial) == REAL_CST) {
            DECL_INITIAL(var) = convert_real_constant(initial, new_format_p);
          } else if (TREE_CODE(initial) == CONSTRUCTOR) {
            VEC(constructor_elt, gc) * element_vec = CONSTRUCTOR_ELTS(initial);
            unsigned HOST_WIDE_INT ix;
            tree field, val;
            FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(initial), ix, field, val)
            {
              int i = TREE_INT_CST_LOW(field);  // index into the vector
              new_format_p = find_var_format(calc_hash_key(var, 0, i));
              if (TREE_CODE(val) == REAL_CST) {
                element_vec->base.vec[i].value =
                    convert_real_constant(val, new_format_p);
              }
            }
            innertype = get_innermost_type(var);
            if (TREE_CODE(innertype) == REAL_TYPE)
              innertype = REAL_TO_INTEGER_TYPE;
          }
        }
        // convert the TYPE of the var from float to integer
        convert_real_var_to_integer(var);
      }                         // for each VAR
      // Fix the type of the function itself, if necessary
      convert_real_var_to_integer(DECL_RESULT(current_function_decl));
      convert_real_func_to_integer(current_function_decl);
    }                           // if lastpass

    //
    // "visited" bit is set when a statement was successfully processed and
    // the format of its result was determined...start by clearing them all
    // except when pass local flag 1 is set...in that case the statement is
    // calculating an induction variable and should not be converted to
    // fixed-point
    //
    fprintf(stderr, "  ===== Marking statements not visited =====\n");
    FOR_EACH_BB(bb) {
      for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
        stmt = gsi_stmt(gsi);
        gimple_set_visited(stmt, 0);
        gimple_set_visited(stmt, gimple_plf(stmt, GF_PLF_1));
      }
    }
    restore_attributes();

    int undefined_result_format;        // at least one format still undefined
    int statements_updated;     // at least one statement was defined
    int formats_changed = 0;    // at least one variable format changed
    //
    // Loop through the statements until EITHER all of the variable
    //   formats have been defined OR no statements were updated
    // The former is a success condition, the latter is a fail condition
    //
    do {
      fxpass++;
      if (fxpass >= MAX_PASSES)
        fatal_error("fxopt: Too many passes\n");

      fprintf(stderr, "  ===== Starting pass %d =====\n", fxpass);
      undefined_result_format = 0;
      statements_updated = 0;
      int bbnumber = 0;
      //
      // If the format of an iterated variable changed on the last iteration
      //   of the loop, revisit all statements. This is not elegant...better
      //   to find the actual statements affected by a new format and revisit
      //   them only.
      //
      if (formats_changed)
        FOR_EACH_BB(bb) {
          for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
            stmt = gsi_stmt(gsi);
            gimple_set_visited(stmt, gimple_plf(stmt, GF_PLF_1));
          }
        }
      formats_changed = 0;
      FOR_EACH_BB(bb) {
        bbnumber++;
        fprintf(stderr, "  ======= Starting basic block %d =====\n", bbnumber);

        for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
          stmt = gsi_stmt(gsi);
          print_gimple_stmt(stderr, stmt, 2, 0);

          if (is_gimple_assign(stmt) && (!(gimple_visited_p(stmt)))) {
            gimple new_stmt;
            struct SIF result_frmt;
            initialize_format(&result_frmt);

            // 
            // Get the trees for all operands, get the format of the LHS,
            // set the RHS formats to be uninitialized
            // 
            int num_operands = gimple_num_ops(stmt);
            struct SIF oprnd_frmt[num_operands];
            tree *oprnd_tree = gimple_ops(stmt);

            int i;
            for (i = 0; i < num_operands; i++) {
              initialize_format(&oprnd_frmt[i]);
            }

            oprnd_frmt[0] = get_operand_format(stmt, 0, 0, NOPRINT);
            //
            // Since we modify the statement but still need access to the
            // original LHS operand, save the original operand 0 tree
            //
            tree oprnd0_tree = oprnd_tree[0];
            // 
            // Change floating expressions to integer expressions, then get
            // the code and class for the right hand side
            // 
            real_expr_to_integer(stmt);
            enum tree_code rhs_code = gimple_assign_rhs_code(stmt);

            switch (rhs_code) {
              case INTEGER_CST:
                oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
                if (format_initialized(oprnd_frmt[0])) {
                  copy_format(&oprnd_frmt[1], &result_frmt);
                  if (result_frmt.iv)
                    result_frmt.shift =
                      (int) double_int_to_uhwi(oprnd_frmt[1].max);
                } else {
                  copy_format(&oprnd_frmt[1], &result_frmt);
                  result_frmt.S = oprnd_frmt[0].size - result_frmt.I;
                }
                break;
              case REAL_CST:
                oprnd_frmt[1] = get_operand_format(stmt, 1, 0, PRINT);
                copy_format(&oprnd_frmt[1], &result_frmt);
                break;
              case FIX_TRUNC_EXPR:
              case FLOAT_EXPR:
              case NOP_EXPR:
              case CONVERT_EXPR:
              case SSA_NAME:
              case VAR_DECL:
              case NEGATE_EXPR:
                result_frmt = nop(&gsi, oprnd_frmt, oprnd_tree);
                break;
              case ARRAY_REF:
                result_frmt = array_ref(&gsi, oprnd_frmt, oprnd_tree);
                break;
              case MEM_REF:
              case POINTER_PLUS_EXPR:
                result_frmt = pointer_math(&gsi, oprnd_frmt, oprnd_tree);
                break;
              case PLUS_EXPR:
              case MINUS_EXPR:
                result_frmt = addition(&gsi, oprnd_frmt, oprnd_tree);
                break;
              case MULT_EXPR:
                result_frmt = multiplication(&gsi, oprnd_frmt, oprnd_tree);
                break;
              case RDIV_EXPR:
              case TRUNC_DIV_EXPR:
                result_frmt = division(&gsi, oprnd_frmt, oprnd_tree);
                break;
              default:
                // 
                // The GIMPLE statement was either trivial or unknown.
                // 
                fprintf(stderr, "*** Found a gimple assign statement ***\n");
                enum gimple_rhs_class xclass = get_gimple_rhs_class(rhs_code);
                fprintf(stderr, "RHS TREE CLASS: %s   CODE: %s\n",
                        TREE_CODE_CLASS_STRING(xclass),
                        tree_code_name[rhs_code]);
            }
            ///////////////////////////////////////////////////////////////
            //  Done analyzing an assignment statement 
            ///////////////////////////////////////////////////////////////
            if (format_initialized(result_frmt)) {
              // 
              // Now construct and insert any necessary shift operations. This
              // section is executed after each GIMPLE arithmetic operator is
              // analyzed.
              // 
              // Build a shift statement for each RHS operand. A negative shift
              // is left, a positive shift is right. The right shift operator is
              // a true arithmetic shift.
              // 
              int i;
              for (i = 1; i < num_operands; i++) {
                if (!lastpass)
                  break;
                tree current_oprnd_tree = oprnd_tree[i];
                char var_name[32];
                if (oprnd_frmt[i].shift != 0) {
                  check_shift(oprnd_frmt[i]);
                  if (!oprnd_frmt[1].ptr_op) {
                    if (oprnd_frmt[i].shift > 0)
                      fprintf(stderr, "  OP%d >>%-2d", i, oprnd_frmt[i].shift);
                    else
                      fprintf(stderr, "  OP%d <<%-2d", i, -oprnd_frmt[i].shift);
                    fprintf(stderr, " (%2d/%2d/%2d/%2d)", oprnd_frmt[i].S,
                            oprnd_frmt[i].I, oprnd_frmt[i].F, oprnd_frmt[i].E);
                    print_min_max(oprnd_frmt[i]);
                  }

                  if (TREE_CONSTANT(oprnd_tree[i])) {
                    //
                    // If this operand is a constant, just change its value by
                    // shifting and rounding at compile time, then replace the
                    // old value with the new value
                    //
                    double_int constant_double =
                        tree_to_double_int(oprnd_tree[i]);

                    if (oprnd_frmt[i].shift < 0) {      // shift left
                      oprnd_frmt[i].shift = -oprnd_frmt[i].shift;
                      constant_double = double_int_lshift(constant_double,
                                                          (HOST_WIDE_INT)
                                                          oprnd_frmt[i].shift,
                                                          oprnd_frmt[i].size,
                                                          false);
                    } else {    // shift right 
                      double_int round_const = double_int_lshift(double_int_one,
                                                                 (HOST_WIDE_INT)
                                                                 (oprnd_frmt
                                                                  [i].shift -
                                                                  1),
                                                                 oprnd_frmt
                                                                 [i].size,
                                                                 false);
                      if (double_int_negative_p(constant_double))
                        round_const =
                            double_int_add(round_const, double_int_minus_one);

                      constant_double =
                          double_int_add(constant_double, round_const);
                      constant_double =
                          double_int_rshift(constant_double,
                                            (HOST_WIDE_INT) oprnd_frmt[i].shift,
                                            oprnd_frmt[i].size, true);
                    }
                    current_oprnd_tree =
                        double_int_to_tree(TREE_TYPE(oprnd_tree[i]),
                                           constant_double);
                  } else if (MEM_REF == TREE_CODE(oprnd_tree[i])) {
                    //
                    // MEM_REF operands are actually expressions and they have
                    //   two operands. The first is a pointer and the second is
                    //   an offset. The offset is an integer number of bytes,
                    //   and is a multiple of the size of the data pointed to
                    //   by the pointer.
                    // If a real type is replaced by an integer type it may be
                    //   necessary to adjust the offset for the new size of the
                    //   data. The amount of the adjustment is determined in
                    //   pointer_math() and passed as the shift element in the
                    //   SIF format for the MEM_REF
                    //
                    double_int constant_double = mem_ref_offset(oprnd_tree[i]);
                    if (oprnd_frmt[i].shift < 0) {      // shift left
                      oprnd_frmt[i].shift = -oprnd_frmt[i].shift;
                      constant_double =
                          double_int_lshift(constant_double,
                                            (HOST_WIDE_INT) oprnd_frmt[i].shift,
                                            oprnd_frmt[i].size, false);
                    } else {    // shift right 
                      constant_double =
                          double_int_rshift(constant_double,
                                            (HOST_WIDE_INT) oprnd_frmt[i].shift,
                                            oprnd_frmt[i].size, true);
                    }
                    tree const_tree =
                        double_int_to_tree(TREE_TYPE
                                           (TREE_OPERAND(oprnd_tree[i], 1)),
                                           constant_double);
                    current_oprnd_tree = build2(MEM_REF,
                                                TREE_TYPE(oprnd_tree[i]),
                                                TREE_OPERAND(oprnd_tree[i], 0),
                                                const_tree);
                  } else {
                    //
                    // Add explicit statements to perform the shift, possibly
                    // with rounding
                    //
                    // var_to_shift is the variable that will actually, finally,
                    // be shifted, after any rounding is done
                    //
                    tree var_to_shift = oprnd_tree[i];
                    // shift_expr will be right for shift>0, left for shift<0
                    enum tree_code shift_expr;
                    if (oprnd_frmt[i].shift < 0) {
                      oprnd_frmt[i].shift = -oprnd_frmt[i].shift;
                      shift_expr = LSHIFT_EXPR;
                    } else {
                      shift_expr = RSHIFT_EXPR;
                      //
                      // Only round if the shift will cause F bits to be lost
                      //
                      if (ROUNDING
                          && (oprnd_frmt[i].originalF > oprnd_frmt[i].F)) {
                        //
                        // Check to see if rounding this operand could cause an
                        // overflow. If so, and if guarding is enabled, then do
                        // the guarding.
                        //
                        if (GUARDING && rounding_may_overflow(oprnd_frmt[i])) {
                          //
                          // When guarding, shift the operand right one place
                          // before rounding. If overflow occurs when rounding 
                          // then we have 1 bit of headroom, although the result
                          // format doesn't take this into account...expect to
                          // be lucky that overflow doesn't occur with _both_
                          // operands and corrupt the result
                          //
                          sprintf(var_name, "_fx_guard%d", i);
                          tree guarded_var =
                              make_rename_temp(long_long_integer_type_node,
                                               var_name);
                          new_stmt =
                              gimple_build_assign_with_ops(RSHIFT_EXPR,
                                                           guarded_var,
                                                           var_to_shift,
                                                           build_one_cst
                                                           (integer_type_node));
                          print_gimple_stmt(stderr, new_stmt, 2, 0);
                          gsi_insert_before(&gsi, new_stmt, GSI_SAME_STMT);
                          var_to_shift = guarded_var;
                          oprnd_frmt[i].S++;
                          if (oprnd_frmt[i].E > 0)
                            oprnd_frmt[i].E--;
                          else
                            oprnd_frmt[i].F--;
                          oprnd_frmt[i].shift--;
                          current_oprnd_tree = guarded_var;
                        }

                        if (oprnd_frmt[i].shift > 0) {
                          // shift_constant is (1 << n-1) for n-bit shift
                          sprintf(var_name, "_fx_round%d", i);
                          tree rounded_var =
                              make_rename_temp(TREE_TYPE(oprnd_tree[i]),
                                               var_name);
                          tree rounding_const =
                            build_int_cst(integer_type_node,
                              (1UL << (oprnd_frmt[i].shift - 1)));
                          //
                          // do a signed round if both positive and negative
                          // values are possible
                          // *** this requires a run-time evaluation of sign 
                          //
                          if (double_int_negative_p(oprnd_frmt[i].min) &&
                              double_int_positive_p(oprnd_frmt[i].max) &&
                              !POSITIVE) {
                            // sign_bit_var is -1 if op is negative, else 0
                            sprintf(var_name, "_fx_signbit%d", i);
                            tree sign_bit_var =
                                make_rename_temp(TREE_TYPE(oprnd_tree[i]),
                                                 var_name);
                            tree sign_shift_cst =
                                build_int_cst(integer_type_node,
                                              (oprnd_frmt[i].size - 1));
                            new_stmt =
                                gimple_build_assign_with_ops(RSHIFT_EXPR,
                                                             sign_bit_var,
                                                             var_to_shift,
                                                             sign_shift_cst);
                            print_gimple_stmt(stderr, new_stmt, 2, 0);
                            gsi_insert_before(&gsi, new_stmt, GSI_SAME_STMT);
                            // Now we add the rounding_const and sign_bit_var
                            sprintf(var_name, "_fx_sround%d", i);
                            tree s_round_var =
                                make_rename_temp(TREE_TYPE(oprnd_tree[i]),
                                                 var_name);
                            new_stmt =
                                gimple_build_assign_with_ops(PLUS_EXPR,
                                                             s_round_var,
                                                             sign_bit_var,
                                                             rounding_const);
                            print_gimple_stmt(stderr, new_stmt, 2, 0);
                            //gimple_set_visited(new_stmt, true);
                            gsi_insert_before(&gsi, new_stmt, GSI_SAME_STMT);
                            // Add the adjusted round value to op
                            new_stmt =
                                gimple_build_assign_with_ops(PLUS_EXPR,
                                                             rounded_var,
                                                             var_to_shift,
                                                             s_round_var);
                            // 
                            // Only positive values are possible
                            //
                          } else if (double_int_positive_p(oprnd_frmt[i].max)) {
                            new_stmt =
                                gimple_build_assign_with_ops(PLUS_EXPR,
                                                             rounded_var,
                                                             var_to_shift,
                                                             rounding_const);
                            // 
                            // Only negative values are possible
                            //
                          } else {
                            rounding_const =
                                build_int_cst(integer_type_node,
                                              (1UL <<
                                               (oprnd_frmt[i].shift - 1)) - 1);
                            new_stmt =
                                gimple_build_assign_with_ops(PLUS_EXPR,
                                                             rounded_var,
                                                             var_to_shift,
                                                             rounding_const);
                          }
                          print_gimple_stmt(stderr, new_stmt, 2, 0);
                          gsi_insert_before(&gsi, new_stmt, GSI_SAME_STMT);
                          var_to_shift = rounded_var;
                        }       // shift > 0 after guarding
                      }         // if rounding
                    }           // if right shift
                    if (oprnd_frmt[i].shift != 0) {
                      sprintf(var_name, "_fx_shft%d", i);
                      tree shift_var =
                          make_rename_temp(TREE_TYPE(oprnd_tree[i]), var_name);
                      tree shift_constant = build_int_cst(integer_type_node,
                                                          oprnd_frmt[i].shift);
                      new_stmt =
                          gimple_build_assign_with_ops(shift_expr, shift_var,
                                                       var_to_shift,
                                                       shift_constant);
                      print_gimple_stmt(stderr, new_stmt, 2, 0);
                      gsi_insert_before(&gsi, new_stmt, GSI_SAME_STMT);
                      current_oprnd_tree = shift_var;
                    }
                  }
                }
                // 
                // If the current operand was modified, modify the statement
                //
                if (current_oprnd_tree != oprnd_tree[i]) {
                  gimple_set_op(stmt, i, current_oprnd_tree);
                  gimple_set_modified(stmt, true);
                }
              }
              // 
              // //////// Assign a format to the result variable //////////
              // 
              // If the desired result format is larger than the actual oprnd0
              // variable, create a new variable for the desired result and
              // update the original statement accordingly.
              // 
              tree result_var = oprnd_tree[0];
              if (result_frmt.size > oprnd_frmt[0].size) {
                //
                // Replace the original LHS with a wider variable
                //
                tree wider_var; // need this name in scope later
                if (lastpass) {
                  wider_var =
                      make_rename_temp(long_long_integer_type_node,
                                       "_fx_wide0");
                  gimple_set_op(stmt, 0, wider_var);
                  gimple_set_modified(stmt, true);
                  update_stmt(stmt);
                  print_gimple_stmt(stderr, stmt, 2, 0);
                  result_var = wider_var;
                }

                if (ROUNDING && GUARDING && (result_frmt.S == 1)) {
                  if (lastpass) {
                    tree guarded_var =
                        make_rename_temp(long_long_integer_type_node,
                                         "_fx_guard0");
                    new_stmt =
                        gimple_build_assign_with_ops(RSHIFT_EXPR, guarded_var,
                                                     result_var,
                                                     build_one_cst
                                                     (integer_type_node));
                    print_gimple_stmt(stderr, new_stmt, 2, 0);
                    gsi_insert_after(&gsi, new_stmt, GSI_NEW_STMT);
                    result_var = guarded_var;
                  }
                  result_frmt.S++;
                  if (result_frmt.E > 0)
                    result_frmt.E--;
                  else
                    result_frmt.F--;
                }
                // 
                // Explicitly cast the new wider LHS to the width of the
                // original LHS. Shift right so there remains just 1 sign bit in
                // the lower half of the double-precision result
                // 
                result_frmt.shift = oprnd_frmt[0].size - result_frmt.S + 1;

                if (result_frmt.shift > 0) {
                  // don't round if there are enough empty bits
                  if (ROUNDING && lastpass) {
                    tree rounded_var =
                        make_rename_temp(TREE_TYPE(wider_var), "_fx_round0");
                    // shift_constant is (1 << n-1) for n-bit shift
                    // unless guarding, then it is (1 << n-2) FIXME?
                    // if saturating to fxfrmt, adjust shift
                    int half_lsb_shift = result_frmt.shift - 1;
                    if (format_initialized(oprnd_frmt[0]) &&
                        (oprnd_frmt[0].I < result_frmt.I)) {
                      half_lsb_shift -= result_frmt.I - oprnd_frmt[0].I;
                      half_lsb_shift = MAX(half_lsb_shift, 0);
                    }
                    tree rounding_const;
                    if (result_frmt.shift > HOST_BITS_PER_WIDE_INT) {
                      half_lsb_shift -= HOST_BITS_PER_WIDE_INT;
                      rounding_const =
                          build_int_cst_wide(integer_type_node, 0,
                                             (1UL << half_lsb_shift));
                    } else {
                      rounding_const =
                          build_int_cst_wide(integer_type_node,
                                             (1UL << half_lsb_shift), 0);
                    }
                    if (double_int_negative_p(result_frmt.min) && !POSITIVE) {
                      // round signed
                      // sign_bit_var is -1 if op is negative, else 0
                      // this must be a run-time check
                      tree sign_bit_var = make_rename_temp(TREE_TYPE(wider_var),
                                                           "_fx_signbit0");
                      tree sign_shifting_const =
                          build_int_cst(integer_type_node,
                                        result_frmt.size - 1);
                      new_stmt =
                          gimple_build_assign_with_ops(RSHIFT_EXPR,
                                                       sign_bit_var,
                                                       result_var,
                                                       sign_shifting_const);
                      print_gimple_stmt(stderr, new_stmt, 2, 0);
                      gsi_insert_after(&gsi, new_stmt, GSI_NEW_STMT);
                      // Now we add the rounding_const and the sign_bit_var
                      tree s_round_var = make_rename_temp(TREE_TYPE(wider_var),
                                                          "_fx_sround0");
                      new_stmt =
                          gimple_build_assign_with_ops(PLUS_EXPR,
                                                       s_round_var,
                                                       sign_bit_var,
                                                       rounding_const);
                      gsi_insert_after(&gsi, new_stmt, GSI_NEW_STMT);
                      print_gimple_stmt(stderr, new_stmt, 2, 0);
                      // Add the adjusted round value to op
                      new_stmt =
                          gimple_build_assign_with_ops(PLUS_EXPR, rounded_var,
                                                       result_var, s_round_var);
                    } else {
                      // round unsigned
                      new_stmt =
                          gimple_build_assign_with_ops(PLUS_EXPR, rounded_var,
                                                       result_var,
                                                       rounding_const);
                    }
                    gsi_insert_after(&gsi, new_stmt, GSI_NEW_STMT);
                    print_gimple_stmt(stderr, new_stmt, 2, 0);
                    result_var = rounded_var;
                  }             // end of rounding
                  //
                  // Saturate if necessary, after rounding
                  // Necessary means that the orignal LHS has an fxfrmt
                  //   attribute AND either (the number of I bits in the result
                  //   is different) OR (the LHS is a pointer and the number of
                  //   sign bits is different)
                  // For non-pointers we only saturate the magnitude but for
                  //   pointers also force the binary point to the same location
                  //
                  result_frmt =
                      apply_fxfrmt(&gsi, oprnd_frmt, oprnd_tree, result_frmt,
                                   &result_var);
                }               // end of shift to cast wide to narrow
                //
                // Saturate if necessary...FIXME is it ever necessary?
                //
                if (oprnd_frmt[0].has_attribute
                    && ((oprnd_frmt[0].I != result_frmt.I)
                        || (oprnd_frmt[0].ptr_op
                            && (result_frmt.S != oprnd_frmt[0].S)))) {
                  fprintf(stderr, " !! Second call to apply_fxfrmt\n");
                  result_frmt =
                      apply_fxfrmt(&gsi, oprnd_frmt, oprnd_tree,
                                   result_frmt, &result_var);
                }
                //
                // Do the actual cast to the original, smaller size
                //
                //FIXME fails if original stmt was not a NOP??
                if (lastpass) {
                  new_stmt =
                      gimple_build_assign_with_ops(CONVERT_EXPR,
                                                   oprnd0_tree, result_var,
                                                   NULL);
                  gsi_insert_after(&gsi, new_stmt, GSI_NEW_STMT);
                  print_gimple_stmt(stderr, new_stmt, 2, 0);
                }
                //
                // The result now fits in the LHS operand
                // Update the format based on the actual shift that was done
                //
                result_frmt = new_range(result_frmt);
                result_frmt.size = oprnd_frmt[0].size;
                result_frmt.S = 1;
                if (result_frmt.shift > result_frmt.E) {
                  result_frmt.F =
                      result_frmt.F + result_frmt.E - result_frmt.shift;
                  result_frmt.E = 0;
                } else {
                  result_frmt.E -= result_frmt.shift;
                }
                result_frmt.shift = 0;
              } else {
                //
                // Original operand0 and result are same size
                // If necessary, saturate to the desired oprnd0 format
                //
                //FIXME if original stmt was unary, should this stmt be NOP
                //  instead of, say an array reference or SSA_NAME?
                if (oprnd_frmt[0].has_attribute
                    && ((oprnd_frmt[0].I != result_frmt.I)
                        || (oprnd_frmt[0].S != result_frmt.S))) {
                  if (lastpass) {
                    tree unsat_var =
                        make_rename_temp(TREE_TYPE(oprnd0_tree), "_fx_unsat0");
                    gimple_set_op(stmt, 0, unsat_var);
                    gimple_set_modified(stmt, true);
                    update_stmt(stmt);
                    print_gimple_stmt(stderr, stmt, 2, 0);
                    result_var = unsat_var;
                  }

                  result_frmt =
                      apply_fxfrmt(&gsi, oprnd_frmt, oprnd_tree, result_frmt,
                                   &result_var);

                  if (lastpass && (oprnd0_tree != result_var)) {
                    if ((rhs_code == SSA_NAME) || (rhs_code == CONVERT_EXPR) ||
                        (rhs_code == ARRAY_REF)) {
                      new_stmt =
                          gimple_build_assign_with_ops(rhs_code, oprnd0_tree,
                                                       result_var, NULL);
                    } else {
                      new_stmt =
                          gimple_build_assign_with_ops(NOP_EXPR, oprnd0_tree,
                                                       result_var, NULL);
                    }
                    gsi_insert_after(&gsi, new_stmt, GSI_NEW_STMT);
                    print_gimple_stmt(stderr, new_stmt, 2, 0);
                  }
                }               // oprnd0 has fxfrmt attribute, must saturate
              }                 // result and oprnd0 are same size
              check_shift(result_frmt);

              if (AFFINE) {
                struct AA *new_aa = new_aa_list(result_frmt);
                delete_aa_list(&(result_frmt.aa));
                result_frmt.aa = new_aa;
                result_frmt.max = aa_max(result_frmt.aa);
                result_frmt.min = aa_min(result_frmt.aa);
              }

              // 
              // Update and dump the statement, if modified
              // 
              if (gimple_modified_p(stmt)) {
                update_stmt(stmt);
                print_gimple_stmt(stderr, stmt, 2, 0);
              }
              if (result_frmt.alias) {
                fprintf(stderr, "  RSLT @");
              } else if (result_frmt.ptr_op) {
                fprintf(stderr, "  RSLT *");
              } else {
                fprintf(stderr, "  RSLT  ");
              }
              fprintf(stderr, "(%2d/%2d/%2d/%2d)",
                      result_frmt.S, result_frmt.I, result_frmt.F,
                      result_frmt.E);
              if (result_frmt.sgnd) {
                fprintf(stderr, "s ");
              } else {
                fprintf(stderr, "u ");
              }
              if (AFFINE) {
                print_aa_list(result_frmt.aa);
              }
              if (INTERVAL)
                print_min_max(result_frmt);
              else
                fprintf(stderr, "\n");

              statements_updated++;
            }                   // result format is initialized
            else {
              undefined_result_format++;
              fprintf(stderr, "  RESULT is uninitialized\n");
            }                   // result format is not initialized
            //
            // If there was a problem setting the LHS format, and the
            // LHS is a variable that is used iteratively, try again
            //
            if (set_var_format(oprnd0_tree, result_frmt) && oprnd_frmt[0].iter)
            {
              statements_updated++;
              undefined_result_format++;
              formats_changed++;
            }                   // LHS format changed
            delete_aa_list(&(result_frmt.aa));
            for (i = 0; i < num_operands; i++) {
              if (0xdead == oprnd_frmt[i].id) 
                delete_aa_list(&(oprnd_frmt[i].aa));
            }
          }                     // an assign statement
          fprintf(stderr, "-------------------------------- %d %d\n",
                    bbnumber, fxpass);
          if (gimple_code(stmt) == GIMPLE_RETURN) {
            tree return_val = gimple_return_retval(stmt);
            if (return_val != NULL_TREE) {
              if (SSA_NAME == TREE_CODE(return_val)) {
                if (lastpass) {
                  tree return_var_tree = SSA_NAME_VAR(return_val);
                  struct SIF *var_fmt;
                  var_fmt = find_var_format(calc_hash_key(return_var_tree,
                                                         fxpass, NOT_AN_ARRAY));
                  if (format_initialized(*var_fmt)) {
                    fprintf(stderr, "/// RETURNS %2d\n", var_fmt->S);
                    fprintf(stderr, "/// RETURNI %2d\n", var_fmt->I);
                    fprintf(stderr, "/// RETURNF %2d\n", var_fmt->F);
                    fprintf(stderr, "/// RETURNE %2d\n", var_fmt->E);
                  }
                }
              } else {
                fprintf(stderr, "  Unexpected non-void return type");
              }
            }                   // non-void return value
          }                     // return statement
        }                       // each statement
        //
        // Make the min/max of pointer variables be consistent, and convert
        // their AA lists to a simple range.
        //
        fprintf(stderr, "  End of a basic block\n");
        force_ptr_consistency();
      }                         // each bb
    } while (undefined_result_format && statements_updated);

    if (undefined_result_format)
      error("fxopt: couldn't resolve all formats");
    lastpass++;
  } while (lastpass < 2);

  print_var_formats();
  delete_all_formats();
  return 0;
}

/**
  @brief A require function for plugins.

  @return  always true
 */
static bool gate_fxopt_plugin(void)
{
  return true;
}

static struct gimple_opt_pass pass_fxopt_plugin = {
  {
   GIMPLE_PASS,
   "fxopt",                     ///<  plugin name 
   gate_fxopt_plugin,           ///<  gate function name
   execute_fxopt_plugin,        ///<  main function name
   NULL,                        ///<  sub 
   NULL,                        ///<  next 
   0,                           ///<  static_pass_number 
   0,                           ///<  tv_id 
   PROP_cfg | PROP_ssa,         ///<  properties_required 
   0,                           ///<  properties_provided 
   0,                           ///<  properties_destroyed 
   0,                           ///<  todo_flags_start 
   TODO_dump_func | TODO_verify_ssa | TODO_update_ssa   ///<  todo_flags_finish 
   }
};

/**
 @brief Initializes the fxopt plugin

 @details Process all of the command line switches that affect fxopt. Register
 the callbacks for all of the functions necessary to run the plugin.

 @param plugin_info pointer to a plugin_name_args struct
 @param version     pointer to a plugin_gcc_version struct
 @return            0 for success
 */
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version)
{
  struct register_pass_info pass_info;
  const char *plugin_name = plugin_info->base_name;
  int argc = plugin_info->argc;
  struct plugin_argument *argv = plugin_info->argv;
  char *ref_pass_name = NULL;
  int ref_instance_number = 0;
  int i;

// 
// Process the plugin arguments.
//
  for (i = 0; i < argc; ++i) {
    if (!strcmp(argv[i].key, "ref-pass-name")) {
      if (argv[i].value)
        ref_pass_name = argv[i].value;
      else
        warning
            (0, "option -fplugin-arg-%s-ref-pass-name requires a pass name",
             plugin_name);
    } else if (!strcmp(argv[i].key, "ref-pass-instance-num")) {
      if (argv[i].value)
        ref_instance_number = strtol(argv[i].value, NULL, 0);
      else
        warning
            (0, "option -fplugin-arg-%s-ref-pass-instance-num requires integer",
             plugin_name);
    } else if (!strcmp(argv[i].key, "round")) {
      ROUNDING = 1;
      fprintf(stderr, "fxopt: rounding enabled\n");
    } else if (!strcmp(argv[i].key, "round-positive")) {
      ROUNDING = 1;
      POSITIVE = 1;
      fprintf(stderr, "fxopt: positive rounding enabled\n");
    } else if (!strcmp(argv[i].key, "guard")) {
      GUARDING = 1;
      fprintf(stderr, "fxopt: guarding enabled\n");
    } else if (!strcmp(argv[i].key, "dpmult")) {
      DBL_PRECISION_MULTS = 1;
      fprintf(stderr, "fxopt: double-precision multiplication enabled\n");
    } else if (!strcmp(argv[i].key, "div2mult")) {
      CONST_DIV_TO_MULT = 1;
      fprintf(stderr,
              "fxopt: constant division converted to multiplication\n");
    } else if (!strcmp(argv[i].key, "interval")) {
      INTERVAL = 1;
      fprintf(stderr, "fxopt: using interval arithmetic\n");
    } else if (!strcmp(argv[i].key, "affine")) {
      AFFINE = 1;
      INTERVAL = 1;
      fprintf(stderr, "fxopt: using affine arithmetic\n");
    } else
      warning(0, G_("plugin %qs: unrecognized argument %qs ignored"),
              plugin_name, argv[i].key);
  }

  if (!ref_pass_name) {
    error("plugin %qs requires a reference pass name", plugin_name);
    return 1;
  }

  pass_info.pass = &pass_fxopt_plugin.pass;
  pass_info.reference_pass_name = ref_pass_name;
  pass_info.ref_pass_instance_number = ref_instance_number;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                    &pass_info);

  register_callback(plugin_name, PLUGIN_ATTRIBUTES, register_attributes,
                    NULL);
  return 0;
}
