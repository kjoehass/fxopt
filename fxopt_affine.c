/**
 * @file fxopt_affine.c
 *
 * @brief Functions and macros for affine arithmetic
 *
 * @author K. Joseph Hass
 * @date Created: 2014-01-05T10:33:10-0500
 * @date Last modified: 2014-01-07T16:18:16-0500
 *
 * @details The affine definition for a variable is stored as a linked list of
 * affine elements. Since the list is used for affine arithmetic operations it
 * is often referred to as the <b>AA list</b>. Each element in the list is an
 * ::AA struct.
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
#include "math.h"
#include "fxopt_plugin.h"
#include "utlist.h"

extern struct SIF *var_formats;

/**
 *  @brief  ID number for the next unique error term.
 *  @details Some operations are not strictly affine and introduce error terms
 *  that are not linearly correlated with input variables. When these error
 *  terms are created we need a unique ID value for each one, and this variable
 *  holds the next available unique ID number. It must be incremented after it
 *  is used.
 */
int next_error_id = 1;

/**
 *  @brief  Sorting function for affine elements.
 *
 *  @details The ID values of two affine elements are compared to determine how
 *  the elements are sorted. Elements with smaller GIMPLE UID values precede
 *  elements with higher UID values. Elements with the same UID value are sorted
 *  by the array index. Elements with the same UID and the same index are sorted
 *  by pass number.
 *
 *  A negative return value indicates that element <tt>a</tt> should precede
 *  element <tt>b</tt>, while a return value greater than zero indicates that
 *  element <tt>b</tt> should precede element <tt>a</tt>. This function should
 *  never return a value of zero because two elements must never have the same
 *  ID value.
 *
 *  @param[in] a pointer to AA structure
 *  @param[in] b pointer to AA structure
 *  @return    comparison result     
 */
int aa_id_sort(struct AA *a, struct AA *b) {
  if (KEY_TO_UID(a->id) != KEY_TO_UID(b->id))
    return (KEY_TO_UID(a->id) - KEY_TO_UID(b->id));
  else if (KEY_TO_IDX(a->id) != KEY_TO_IDX(b->id))
    return (KEY_TO_IDX(a->id) - KEY_TO_IDX(b->id));
  else 
    return (KEY_TO_PASS(a->id) - KEY_TO_PASS(b->id));
}

/**
 *  @brief Add a term to an affine definition.
 *
 *  @details Create a new list element and populate it with the hash table key
 *  for the variable being added, its coefficient, and the binary point
 *  location for the coefficient. If the coefficient is zero then this term
 *  is simply discarded. Append the new element to the existing affine
 *  definition. An error occurs if the list pointer is NULL or an element
 *  with the same ID is already in the list. Warnings are printed if the
 *  coefficient is zero and the element is discarded or if it appears that
 *  the list has been deleted.
 *   
 *  @param[in]  aa_list_p pointer to the AA list head pointer
 *  @param[in]  var_key   hash table ID for variable being added to list
 *  @param[in]  coeff     double_int, the coefficient for variable being added
 *  @param[in]  bp        int, binary point location for coefficient
 */
void append_aa_var(struct AA **aa_list_p, int var_key, double_int coeff, int bp)
{
  if (NULL == aa_list_p) {
    error("fxopt: cannot append to list at NULL address");
    return;
  }
  if (NULL != search_aa_var(*aa_list_p, var_key)) {
    error("fxopt: cannot append to list, already has this element");
    return;
  }

  if ((NULL != *aa_list_p) && (*aa_list_p)->id == 0xbad)
    fprintf(stderr, "%p already destroyed, can't append!\n",
            (void *) *aa_list_p);

  struct AA *aa_p = (struct AA *) xmalloc(sizeof(struct AA));

  if (NULL == aa_p) {
    error("fxopt: could not create AA list node");
  }
  if (bp > 63)
    fprintf(stderr, "  append_aa_var -- bp is %d\n", bp);
  // fill its elements
  aa_p->id = var_key;
  aa_p->coeff = coeff;
  aa_p->bp = bp;
  aa_p->next = NULL;
  // append it to the variable's list
  DL_APPEND(*aa_list_p, aa_p);
}

/**
 * @brief Search for a variable in an AA list.
 *
 * @details Given the hash key for a variable, search an AA list for that
 * variable. If found, return a pointer to the list element. If not found
 * return NULL.
 *
 * @param[in] aa_list_p head of AA list to be searched
 * @param[in] var_key   hash id for variable that is searched for
 * @return          pointer to AA element if found, else NULL
 */
struct AA *search_aa_var(struct AA *aa_list_p, int var_key)
{
  struct AA *aa_elt_p;

  DL_SEARCH_SCALAR(aa_list_p, aa_elt_p, id, var_key);

  return aa_elt_p;
}

/**
 * @brief Print a single affine list element to stderr.
 *
 * @details Converts the coefficient to a floating-point value and prints it.
 * Determines whether the variable is a declared variable or gcc created
 * it, which pass it was defined in, whether or not it is an array
 * element, and then prints its name accordingly. If the list element
 * represents a constant offset then no variable name will be printed.
 * If the coefficient is zero then the element will not be printed.
 *
 * @param[in] aa_elt_p pointer to an AA list element
 */
void print_aa_element(struct AA *aa_elt_p)
{
  tree var_tree;
  struct SIF *s;
  double fcoeff;

  if (! double_int_fits_in_shwi_p(aa_elt_p->coeff))
    warning(0, G_("fxopt: double_int bigger than SHWI!"));
  fcoeff = ((double) double_int_to_shwi(aa_elt_p->coeff))/(1ULL << aa_elt_p->bp);
  fprintf(stderr, "%6.3f", fcoeff);

  if (aa_elt_p->id != 0) {
    int elt_id = aa_elt_p->id;
    int uid = KEY_TO_UID(elt_id);

    if (uid != 0) {
      // get the SIF struct for the variable in the list element
      HASH_FIND_INT(var_formats, &elt_id, s);

      if (s->alias != 0) {
        var_tree = referenced_var_lookup(cfun, KEY_TO_UID(s->alias));
        elt_id = s->alias;
        uid = KEY_TO_UID(elt_id);
      } else {
        var_tree = referenced_var_lookup(cfun, KEY_TO_UID(elt_id));
      }

      int idx = KEY_TO_IDX(elt_id);
      int pass = KEY_TO_PASS(elt_id);
      if (DECL_NAME(var_tree)) {
        fprintf(stderr, "*%s", IDENTIFIER_POINTER(DECL_NAME(var_tree)));
      } else {
        fprintf(stderr, "*%c%4u", TREE_CODE(var_tree) == CONST_DECL ? 'C' : 'D',
                DECL_UID(var_tree));
      }
      if (idx != NOT_AN_ARRAY)
        fprintf(stderr, "[%1d]", idx);
      fprintf(stderr, "#%d", pass);
    } else {
      fprintf(stderr, "*ERR%d", elt_id);
    }
  }
}

/**
 * @brief Print the equation for an affine definition.
 *
 * @details All elements in the list are printed in the form of an arithmetic
 * expression (a sum of products).
 *
 * @param[in] aa_list_p the AA list to be printed
 */
void print_aa_list(struct AA *aa_list_p)
{
  struct AA *aa_elt_p;

  if (aa_list_p == NULL) 
    return;

  //DL_SORT(aa_list_p, aa_id_sort);  // sort is broken?
  DL_FOREACH(aa_list_p, aa_elt_p) {
    print_aa_element(aa_elt_p);
    if (aa_elt_p->next != NULL)
      fprintf(stderr, "+");
  }
}

/**
 * @brief Get the binary point location for an aa list
 *
 * @details All elements in the list are checked to ensure that the b.p.
 * location is the same in them all. Ignore elements with zero coefficient.
 *
 * @param[in] aa_list_p the AA list to be printed
 */
int get_aa_bp(struct AA *aa_list_p)
{
  struct AA *aa_elt_p;
  int list_bp = 0xbad;

  DL_FOREACH(aa_list_p, aa_elt_p) {
    if (aa_elt_p->bp != list_bp) {
      if (list_bp == 0xbad)
        list_bp = aa_elt_p->bp;
      else
        fprintf(stderr, " Inconsistent binary point in AA list\n");
    }
  }
  return list_bp;
}

/**
 * @brief Delete an entire AA definition
 *
 * @details Each element in the AA list is deleted individually, and its memory
 * is freed. After this function ends, `aa_list_p` will be NULL.
 *
 * @param[in,out] aa_list_pp pointer to a pointer to a head of AA list
 */
void delete_aa_list(struct AA **aa_list_pp)
{
  struct AA *head, *del, *tmp;
  int count;

  if (*aa_list_pp != NULL) {
    head = *aa_list_pp;
    DL_COUNT(head, tmp, count);
    DL_FOREACH_SAFE(head, del, tmp) {
      if (del->id != 0xbad) {
        DL_DELETE(head, del);
        del->id = 0xbad;
      }
    }
    *aa_list_pp = NULL;
  }
}

/**
 * @brief Copy an AA element.
 *
 * @details Create a deep copy of a single AA list element. Note that the next
 * pointer is set to NULL.
 *
 * @param[in] aa_src_elt_p pointer to source AA element
 * @return             pointer to destination AA element
 */
struct AA *copy_aa_element(struct AA *aa_src_elt_p)
{
  struct AA *aa_dest_elt_p = NULL;

  if (aa_src_elt_p == NULL)
    return NULL;

  aa_dest_elt_p = (struct AA *) xmalloc(sizeof(struct AA));
  aa_dest_elt_p->id = aa_src_elt_p->id;
  aa_dest_elt_p->coeff = aa_src_elt_p->coeff;
  aa_dest_elt_p->bp = aa_src_elt_p->bp;
  aa_dest_elt_p->next = NULL;

  return aa_dest_elt_p;
}

/**
 * @brief Copy an AA list.
 *
 * @details Create a deep copy of an AA linked list. A copy of each element
 * of the source list is created and then appended to the destination list.
 *
 * @param[in] aa_src_list_p pointer to source AA struct
 * @return                  pointer to destination AA struct
 */
struct AA *copy_aa_list(struct AA *aa_src_list_p)
{
  struct AA *aa_dest_list_p = NULL;
  struct AA *src_elt_p;
  struct AA *dest_elt_p;

  if (aa_src_list_p == NULL)
    return NULL;

  DL_FOREACH(aa_src_list_p, src_elt_p) {
    dest_elt_p = copy_aa_element(src_elt_p);
    if (src_elt_p->id == 0xbad) {
      return NULL;
    }
    DL_APPEND(aa_dest_list_p, dest_elt_p);
  }

  return aa_dest_list_p;
}

/**
 * @brief Determine affine result of a simple assignment statement.
 *
 * @details Deep copy the AA definition of aa_src_p to result_p.
 * A new list of elements is created, with the same length and the
 * same data in each element. If the assignment is actually a NEGATE
 * operation then the coefficients are negated.
 *
 * @param  aa_src_list_p pointer to existing AA definition list
 * @param  add      boolean, false for negate
 * @return          pointer to the new AA list, NULL if failed
 */
struct AA *affine_assign(struct AA *aa_src_list_p, bool add)
{
  struct AA *src_elt_p;
  struct AA *dest_elt_p;
  struct AA *result_p = NULL;

  if (aa_src_list_p == NULL)
    return NULL;

  DL_FOREACH(aa_src_list_p, src_elt_p) {
    if (src_elt_p->id == 0xbad) {
      fprintf(stderr, "%p already destroyed, can't copy!\n",
              (void *) src_elt_p);
      return NULL;
    }
    dest_elt_p = copy_aa_element(src_elt_p);
    if (!add)
      dest_elt_p->coeff = double_int_neg(dest_elt_p->coeff);
    DL_APPEND(result_p, dest_elt_p);
  }

  return result_p;
}

/**
 * @brief Perform a virtual shift on an AA list.
 *
 * @details Return an new AA list that is a copy of the op_fmt's list, except
 * that each element has been effectively shifted left by k bits. Instead
 * of shifting the coefficients, the binary point location is moved right.
 * This is accomplished by subtracting k from the binary point location of
 * each element and leaving the coefficients unchanged. Note that a virtual
 * shift is equivalent to multiplication by 2^k, and that a negative value
 * for k is a virtual right shift.
 *
 * @todo Test k value. Must have (k <= (F+E)) and (k >= -I)
 *
 * @param[in] op_fmt SIF of the original, unshifted operand
 * @param[in] k  number of bit positions to shift 
 * @return    pointer to new AA list
 */
struct AA *shift_aa_list(struct SIF op_fmt, int k)
{
  struct AA *new_list_p = NULL;
  struct AA *aa_elt_p;

  if (op_fmt.aa == NULL)
    return NULL;

  DL_FOREACH(op_fmt.aa, aa_elt_p) {
    append_aa_var(&new_list_p, aa_elt_p->id, aa_elt_p->coeff,
                  (aa_elt_p->bp - k));
  }

  return new_list_p;
}

/**
 * @brief Apply shift and round operations to an AA list.
 *
 * @details A new AA list is created by applying a shift, and possibly a
 * rounding operation, to the existing AA list in a operand's SIF structure.
 * The size and direction of the shift is determined by the `shift`
 * element value in the SIF structure, and the type of rounding is determined
 * by the global option flags.
 *
 * @todo Verify that the shift size and direction are valid with respect to
 * the operand's format.
 *
 * @param[in] op_fmt SIF structure
 * @return    pointer to new_aa_list structure
 */
struct AA *new_aa_list(struct SIF op_fmt)
{
  struct AA *new_list_p = NULL;
  struct AA *aa_elt_p;
  double_int constant, coeff;
  int org_bp, shift;

  if (op_fmt.aa == NULL)
    return NULL;

  org_bp = op_fmt.F + op_fmt.E;

  DL_FOREACH(op_fmt.aa, aa_elt_p) {
    coeff = aa_elt_p->coeff;
    shift = aa_elt_p->bp - org_bp;
    if (shift != 0) {
      if ((shift > 0) && ROUNDING) {
        constant = double_int_lshift(double_int_one,
                                              (shift - 1),
                                              HOST_BITS_PER_DOUBLE_INT,
                                              LOGICAL);
        if (double_int_negative_p(coeff) && !POSITIVE) {
          constant = double_int_sub(constant, double_int_one);
        }
        coeff = double_int_add(coeff, constant);
      }
      coeff =
        double_int_rshift(coeff, shift, HOST_BITS_PER_DOUBLE_INT, ARITH);
    }
    append_aa_var(&new_list_p, aa_elt_p->id, coeff, org_bp);
  }

  return new_list_p;
}

/**
 * @brief Force the AA binary point to be consistent with format
 *
 * @details If the binary point location of the ::AA list is different than the
 * binary point location in the ::SIF format, then shift the ::AA coefficients
 * as necessary and change the value of the binary point location in each list
 * element.
 *
 * @param[in] op_fmt SIF structure
 */
void fix_aa_bp(struct SIF op_fmt)
{
  struct AA *aa_elt_p;
  double_int constant, coeff;
  int format_bp, shift;

  if (op_fmt.aa == NULL)
    return;

  format_bp = op_fmt.F + op_fmt.E;
  shift = get_aa_bp(op_fmt.aa) - format_bp;

  DL_FOREACH(op_fmt.aa, aa_elt_p) {
    coeff = aa_elt_p->coeff;
    aa_elt_p->bp = format_bp;
    if (shift != 0) {
      if ((shift > 0) && ROUNDING) {
        constant = double_int_lshift(double_int_one,
                                              (shift - 1),
                                              HOST_BITS_PER_DOUBLE_INT,
                                              LOGICAL);
        if (double_int_negative_p(coeff) && !POSITIVE) {
          constant = double_int_sub(constant, double_int_one);
        }
        coeff = double_int_add(coeff, constant);
      }
      coeff =
        double_int_rshift(coeff, shift, HOST_BITS_PER_DOUBLE_INT, ARITH);
    }
    aa_elt_p->coeff = coeff;
  }
}

/**
 * @brief Return the constant term from an AA list.
 *
 * @details If the AA list has a constant (or "center") term, which does not
 * depend on the value of any variable, then the ID value for that term will
 * be zero. Return the coefficient of this AA list element, if it exists, or
 * a value of zero if there no constant element in the list.
 *
 * @param[in] aa_list_p pointer to head of AA list
 * @return    double_int coefficient of constant term
 */
double_int aa_center(struct AA *aa_list_p)
{
  struct AA *aa_elt_p = NULL;

  if (aa_list_p == NULL)
    return double_int_zero;

  aa_elt_p = search_aa_var(aa_list_p, 0);

  if (NULL != aa_elt_p)
    return aa_elt_p->coeff;
  else
    return double_int_zero;
}

/**
 * @brief Calculate the maximum value of an affine definition.
 *
 * @details Add the absolute values of all coefficients and return this
 * value as the maximum value of the affine definition. If the affine
 * element list is uninitialized return a value of zero.
 *
 * @param[in] aa_list_p pointer to head of AA list
 * @return    double_int maximum value
 */
double_int aa_max(struct AA *aa_list_p)
{
  struct AA *aa_elt_p;

  double_int constant, maxval, coeff;

  if (aa_list_p == NULL)
    return double_int_zero; // flag for uninitialized

  maxval = double_int_zero;
  constant = double_int_zero;
  DL_FOREACH(aa_list_p, aa_elt_p) {
    coeff = aa_elt_p->coeff;
    if (aa_elt_p->id == 0)
      constant = coeff;
    else
      if (double_int_negative_p(coeff))
        maxval = double_int_sub(maxval, coeff);
      else
        maxval = double_int_add(maxval, coeff);
  }
  maxval = double_int_add(constant, maxval);
  return maxval;
}

/**
 * @brief Calculate the affine maximum after shifting.

 * @details Add the absolute values of all shifted coefficients and return this
 * value as the maximum value of the affine definition. The size and direction
 * of the shift is determined by the `shift` element in the operand
 * format SIF structure. If the affine element list is uninitialized return
 * a value of zero.

 * @todo Verify that the shift size and direction are valid with respect to
 * the operand's format.

 * @param[in] op_fmt operand format to be evaluated
 * @return    double_int maximum value
 */
double_int new_aa_max(struct SIF op_fmt)
{
  double_int maxval, constant;
  int shift;

  maxval = double_int_zero;
  constant = double_int_zero;

  if (op_fmt.aa == NULL)
    return maxval;

  maxval = aa_max(op_fmt.aa);

  shift = op_fmt.shift;
  if (shift != 0) {
    if ((shift > 0) && ROUNDING) {
      constant = double_int_lshift(double_int_one,
                                   (shift - 1),
                                   HOST_BITS_PER_DOUBLE_INT, LOGICAL);
      if (double_int_negative_p(maxval) && !POSITIVE) {
        constant = double_int_sub(constant, double_int_one);
      }
      maxval = double_int_add(maxval, constant);
    }
    maxval = double_int_rshift(maxval, shift, HOST_BITS_PER_DOUBLE_INT, ARITH);
  }
  return maxval;
}

/**
 * @brief Calculate the minimum value of an affine definition.
 *
 * @details The minimum value of the affine definition is equal to the
 * coefficient of the constant term minus the absolute values of all other
 * coefficients. Return this value as the minimum value of the affine
 * definition. If the affine element list is uninitialized return a value of
 * zero.
 *
 * @param[in] aa_list_p pointer to head of AA list
 * @return    double_int minimum value
 */
double_int aa_min(struct AA *aa_list_p)
{
  struct AA *aa_elt_p;

  double_int constant, minval, coeff;

  if (aa_list_p == NULL)
    return double_int_one; // flag for uninitialized

  minval = double_int_zero;
  constant = double_int_zero;
  DL_FOREACH(aa_list_p, aa_elt_p) {
    coeff = aa_elt_p->coeff;
    if (aa_elt_p->id == 0)
      constant = coeff;
    else
      if (double_int_negative_p(coeff))
        minval = double_int_add(minval, coeff);
      else
        minval = double_int_sub(minval, coeff);
  }
  minval = double_int_add(constant, minval);
  return minval;
}

/**
 * @brief Calculate the affine minimum after shifting.
 *
 * @details Shifts all of the coefficients according to the `shift` element
 * value in the SIF structure, then calculates the affine minimum. If the affine
 * element list is uninitialized return a value of zero.
 *
 * @todo Verify that the shift size and direction are valid with respect to the
 * operand's format.
 *
 * @param[in] op_fmt operand format to be evaluated
 * @return double_int minimum value
 */
double_int new_aa_min(struct SIF op_fmt)
{
  double_int minval, constant;
  int shift;

  minval = double_int_zero;
  constant = double_int_zero;

  if (op_fmt.aa == NULL)
    return minval;

  minval = aa_min(op_fmt.aa);

  shift = op_fmt.shift;
  if (shift != 0) {
    if ((shift > 0) && ROUNDING) {
      constant = double_int_lshift(double_int_one,
                                   (shift - 1),
                                   HOST_BITS_PER_DOUBLE_INT, LOGICAL);
      if (double_int_negative_p(minval) && !POSITIVE) {
        constant = double_int_sub(constant, double_int_one);
      }
      minval = double_int_add(minval, constant);
    }
    minval = double_int_rshift(minval, shift, HOST_BITS_PER_DOUBLE_INT, ARITH);
  }
  return minval;
}

/**
 * @brief Add or subtract statement affine result processing.
 *
 * @details AA elements that are unique to either operand 1 or operand 2 are
 * simply copied to the result AA list. For variables that are found in both
 * the AA lists of operands 1 and 2, the coefficients are added.
 * If the operation to be performed is subtraction rather than addition, then
 * the coefficient of operand 2 is negated first. If the affine definition
 * of either operand is missing return a NULL pointer.
 *
 * @note Assumes that operands are already aligned (have the same binary
 * point location). A warning is issued if this is not the case.
 *   
 * @param[in]  oprnd_frmt array of SIF structs for the operands
 * @param[in]  add    boolean, true for addition false for subtraction
 * @return        pointer to the new AA list, NULL if failed
 */
struct AA *affine_add(struct SIF oprnd_frmt[], bool add)
{
  struct AA *aa1_elt_p;
  struct AA *aa2_elt_p;
  struct AA *result_p = NULL;
  double_int op1_coeff, op2_coeff;

  if ((oprnd_frmt[1].aa == NULL) || (oprnd_frmt[2].aa == NULL))
    return NULL;

  struct AA *aa1_list_p = new_aa_list(oprnd_frmt[1]);
  struct AA *aa2_list_p = new_aa_list(oprnd_frmt[2]);

  DL_FOREACH(aa1_list_p, aa1_elt_p) {
    aa2_elt_p = search_aa_var(aa2_list_p, aa1_elt_p->id);
    if (aa2_elt_p == NULL) {    // in aa1 but not in aa2
      append_aa_var(&result_p, aa1_elt_p->id, aa1_elt_p->coeff, aa1_elt_p->bp);
    } else {                    // in both aa1 and aa2
      op1_coeff = aa1_elt_p->coeff;
      op2_coeff = aa2_elt_p->coeff;
      if (aa1_elt_p->bp != aa2_elt_p->bp)
        fprintf(stderr, "  affine_add : binary points not equal\n");
      if (!add)
        op2_coeff = double_int_neg(double_int_add(double_int_one,op2_coeff));
      append_aa_var(&result_p, aa1_elt_p->id,
                    double_int_add(op1_coeff, op2_coeff), aa1_elt_p->bp);
    }
  }

  DL_FOREACH(aa2_list_p, aa2_elt_p) {
    aa1_elt_p = search_aa_var(aa1_list_p, aa2_elt_p->id);
    if (aa1_elt_p == NULL) {    // in aa2 but not in aa1
      op2_coeff = aa2_elt_p->coeff;
      if (!add)
        op2_coeff = double_int_neg(double_int_add(double_int_one,op2_coeff));
      append_aa_var(&result_p, aa2_elt_p->id, op2_coeff, aa2_elt_p->bp);
    }
  }

  delete_aa_list(&aa1_list_p);
  delete_aa_list(&aa2_list_p);
  return result_p;
}

/**
 * @brief Multiply affine definitions
 *
 * @details Determine (or estimate) the affine result of multiplying two
 * affine definitions.
 * 
 * Multiplication can result in both affine and non-affine terms in the
 * product's affine definition. The only source of affine product terms is
 * multiplication by a constant, so the constant term in the affine list for
 * each operand is multiplied by all of the terms in the other affine list and
 * the resulting affine terms are collected by summing the coefficients of
 * terms that have the same ID value (i.e. depend on the same variable).
 *
 * If the same variable exists in the AA list of both multiplicands then we get
 * a term for that variable squared. Since an affine definition cannot have
 * a quadratic term then we create a new unique error term. However, since
 * this is a squared term it can never be negative. We find the product of the
 * coefficients and half of that product is added to the constant term in the
 * product's affine definition while the other half is added to the coefficient
 * of the unique error term for all squared variable terms.
 *
 * In all other cases we have the product of two affine terms that depend on
 * different variables. Such a product cannot be represented in affine form so
 * it becomes a new uncorrelated error term. However, if the same two variables
 * exist in the affine definitions for both multiplicands we will effectively
 * have the same product term twice. In this special case we should cancel the
 * two coefficient products before lumping the result into the uncorrelated
 * error term. Note that since all of these error terms are uncorrelated we
 * must sum the absolute values of the products of their coefficients to get a
 * coefficient for the lumped error term.
 *
 * @param  aa1_list_p head of AA linked list for first operand
 * @param  aa2_list_p head of AA linked list for second operand
 * @return     pointer to the product's AA list, NULL if failed
 */
struct AA *affine_multiply(struct AA *aa1_list_p, struct AA *aa2_list_p)
{
  struct AA *result_p = NULL;
  struct AA *aa1_elt_p;
  struct AA *aa2_elt_p;
  struct AA *res_elt_p;

  if ((aa1_list_p == NULL) || (aa2_list_p == NULL))
    return NULL;

  int new_bp = get_aa_bp(aa1_list_p) + get_aa_bp(aa2_list_p);

  double_int aa_err = double_int_zero;
  DL_FOREACH(aa1_list_p, aa1_elt_p) {
    DL_FOREACH(aa2_list_p, aa2_elt_p) {
      double_int product = double_int_mul(aa1_elt_p->coeff, aa2_elt_p->coeff);
      if (aa1_elt_p->id == 0) { // op1 constant term
        res_elt_p = search_aa_var(result_p, aa2_elt_p->id);
        if (NULL != res_elt_p)
          res_elt_p->coeff = double_int_add(res_elt_p->coeff, product);
        else
          append_aa_var(&result_p, aa2_elt_p->id, product, new_bp);
      } else if (aa2_elt_p->id == 0) {  // op2 constant term
        res_elt_p = search_aa_var(result_p, aa1_elt_p->id);
        if (NULL != res_elt_p)
          res_elt_p->coeff = double_int_add(res_elt_p->coeff, product);
        else
          append_aa_var(&result_p, aa1_elt_p->id, product, new_bp);
      //
      // This is an error term that must be positive because the input is
      // squared. Add half of the coefficient to the 'center' term. Since
      // this error is uncorrelated with other squared errors, the absolute
      // value of half of the coefficient is added to a new error term.
      //
      } else if (aa1_elt_p->id == aa2_elt_p->id) {
        product = double_int_rshift(product, 1, HOST_BITS_PER_DOUBLE_INT,
                                    ARITH);
        res_elt_p = search_aa_var(result_p, 0);
        if (NULL == res_elt_p)
          append_aa_var(&result_p, 0, product, new_bp);
        else
          res_elt_p->coeff = double_int_add(res_elt_p->coeff, product);
        aa_err = double_int_add(aa_err, double_int_abs(product));
      } else {                  // uncorrelated error term
        //
        // if this product will occur twice then allow cancellation
        // divide the sum of the product terms by 2 since we will see it twice
        //
        struct AA *other1 = search_aa_var(aa2_list_p, aa1_elt_p->id);
        struct AA *other2 = search_aa_var(aa1_list_p, aa2_elt_p->id);
        if (other1 && other2) {
          double_int otherp = double_int_mul(other1->coeff, other2->coeff);
          product = double_int_abs(double_int_add(product, otherp));
          product = double_int_rshift(product, 1, HOST_BITS_PER_DOUBLE_INT,
                                        ARITH);
          aa_err = double_int_add(aa_err, product);
        } else {
          aa_err = double_int_add(aa_err, double_int_abs(product));
        }
      }
    }
  }

  if (!double_int_zero_p(aa_err)) {
    append_aa_var(&result_p, next_error_id++, aa_err, new_bp);
  }
  return result_p;
}

/**
 * @brief Affine division, numerator/denominator
 *
 * @details Division of affine definitions is non-trivial. It is actually done
 * in two steps: first we find the reciprocal of the denominator, second we
 * multiply the reciprocal times the numerator.
 *
 * @param  numerator_p    head of AA linked list for numerator
 * @param  denominator_p  head of AA linked list for denominator
 * @return                pointer to the quotient's AA list, NULL if failed
 */
struct AA *affine_divide(struct AA *numerator_p, struct AA *denominator_p)
{
  struct AA *reciprocal_p = NULL;
  struct AA *quotient_p = NULL;
  struct AA *temp_p;
  double_int den_min, den_max, a, b, double_1, alpha, dmax, dmin, zeta, delta;
  int den_bp, num_bp;

  if ((numerator_p == NULL) || (denominator_p == NULL))
    return NULL;
  //
  // numerator_p (dividend)
  //
  num_bp = get_aa_bp(numerator_p);
  //
  // denominator_p (divisor)
  //
  den_bp = get_aa_bp(denominator_p);
  den_min = aa_min(denominator_p);
  den_max = aa_max(denominator_p);

  a = double_int_smin(double_int_abs(den_min), double_int_abs(den_max));
  b = double_int_smax(double_int_abs(den_min), double_int_abs(den_max));

  //
  // Create a double_int identically 1 with a binary point at numerator_p bp
  //
  double_1 = double_int_lshift(double_int_one, num_bp, (num_bp + 2), ARITH);

  alpha = double_int_sdiv(double_1, b, TRUNC_DIV_EXPR); // bp = num_bp - den_bp
  alpha = double_int_mul(alpha, alpha); // bp = 2(num_bp - den_bp)
  alpha = double_int_neg(alpha);        // bp = 2(num_bp - den_bp

  double_int dmax_1 = double_int_sdiv(double_1, a, TRUNC_DIV_EXPR);
  double_int dmax_2 = double_int_rshift(double_int_mul(alpha, a), num_bp,
                                        HOST_BITS_PER_DOUBLE_INT, ARITH);
  dmax = double_int_sub(dmax_1, dmax_2);

  double_int dmin_1 = double_int_sdiv(double_1, b, TRUNC_DIV_EXPR);
  double_int dmin_2 = double_int_rshift(double_int_mul(alpha, b), num_bp,
                                        HOST_BITS_PER_DOUBLE_INT, ARITH);
  dmin = double_int_sub(dmin_1, dmin_2);

  zeta = double_int_add(dmin, dmax);
  zeta = double_int_rshift(zeta, 1, HOST_BITS_PER_DOUBLE_INT, ARITH);
  delta =
      double_int_smax(double_int_sub(zeta, dmin), double_int_sub(dmax, zeta));

  // FIXME round down first term?

  if (double_int_negative_p(den_min))
    zeta = double_int_neg(zeta);

  alpha = double_int_rshift(alpha, (num_bp - den_bp), HOST_BITS_PER_DOUBLE_INT,
                            ARITH);

  append_aa_var(&reciprocal_p, 0, alpha, (num_bp - den_bp));
  temp_p = affine_multiply(denominator_p, reciprocal_p);
  delete_aa_list(&(reciprocal_p));
  reciprocal_p = temp_p;

  if (num_bp != get_aa_bp(reciprocal_p))
    fprintf(stderr, "  !!!!! expected bp of num_bp\n");

  if (!double_int_zero_p(zeta)) {
    zeta = double_int_lshift(zeta, den_bp, HOST_BITS_PER_DOUBLE_INT, ARITH);

    struct AA *elt_p;
    elt_p = search_aa_var(reciprocal_p, 0);
    if (NULL == elt_p)
      append_aa_var(&reciprocal_p, 0, zeta, num_bp);
    else if (num_bp != elt_p->bp)
      fprintf(stderr, " incorrect bp location ");
    elt_p->coeff = double_int_add(elt_p->coeff, zeta);
  }

  if (!double_int_zero_p(delta)) {
    delta = double_int_lshift(delta, den_bp, HOST_BITS_PER_DOUBLE_INT, ARITH);

    append_aa_var(&reciprocal_p, next_error_id, delta, num_bp);
    next_error_id++;
  }

  quotient_p = affine_multiply(numerator_p, reciprocal_p);
  delete_aa_list(&(reciprocal_p));
  return quotient_p;
}

