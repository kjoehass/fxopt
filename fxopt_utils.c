/**
 * @file fxopt_utils.c
 *
 * @brief Assorted utilities.
 *
 * @author K. Joseph Hass
 * @date Created: 2014-01-05T11:19:29-0500
 * @date Last modified: 2014-01-05T12:44:22-0500
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
 * @brief  Sorting function for variable ::SIF definitions.
 *
 * @details The ID values of two ::SIF definitions are compared to determine
 * how the ::SIF definitions are sorted. ::SIF definitions with smaller GIMPLE
 * UID values precede ::SIF definitions with higher UID values. ::SIF definitions
 * with the same UID value are sorted by the array index. ::SIF definitions with
 * the same UID and the same index are sorted by pass number.
 *
 * A negative return value indicates that ::SIF definition <tt>a</tt> should
 * precede ::SIF definition <tt>b</tt>, while a return value greater than zero
 * indicates that ::SIF definition <tt>b</tt> should precede ::SIF definition
 * <tt>a</tt>. This function should never return a value of zero because two
 * ::SIF definitions must never have the same ID value.
 *
 * This function is strictly for cosmetic purposes and is used when printing
 * all of the variables' ::SIF definitions.
 *
 * @param[in] a pointer to ::SIF structure
 * @param[in] b pointer to ::SIF structure
 * @return    comparison result     
 */
int id_sort(struct SIF *a, struct SIF *b) {
  if (KEY_TO_UID(a->id) != KEY_TO_UID(b->id))
    return (KEY_TO_UID(a->id) - KEY_TO_UID(b->id));
  else if (KEY_TO_IDX(a->id) != KEY_TO_IDX(b->id))
    return (KEY_TO_IDX(a->id) - KEY_TO_IDX(b->id));
  else 
    return (KEY_TO_PASS(a->id) - KEY_TO_PASS(b->id));
}
/**
 * @brief Calculate the hash key for the ::SIF formats.
 *
 * @details The key is calculated from the UID of the variable declaration,
 * the array index (if this variable is an element in an array), and the pass
 * number.
 * 
 * @param[in] var_tree the gcc tree for the underlying variable declaration
 * @param[in] pass     pass number
 * @param[in] index    index value for array elements
 * @return             the calculated unique hash key
 */
int calc_hash_key(tree var_tree, int pass, int index)
{
  var_tree = get_operand_decl(var_tree);
  if ((TREE_CODE(var_tree) == VAR_DECL)
      || (TREE_CODE(var_tree) == PARM_DECL)
      || (TREE_CODE(var_tree) == RESULT_DECL)) {
    if (0 == DECL_UID(var_tree))
      error("fxopt: variable UID is null");
    if ((index >= MAX_ELEMENTS) || (index < 0))
      error("fxopt: Array index is too large or is negative");
    if ((pass >= MAX_PASSES) || (pass < 0))
      error("fxopt: Pass number is too large or is negative");
    int key = UID_PASS_IDX_TO_KEY(DECL_UID(var_tree),pass,index);
    return key;
  } else {
    error("fxopt: calc_hash_key parameter not a DECL");
    return 0;
  }
}

/**
 * @brief Pointer to the hash table for the variables' ::SIF formats.
 */
struct SIF *var_formats = NULL;
/**
 * @brief Counter for number of defined variables, just for diagnostics.
 */
static int vars = 0;

/**
 * @brief Add a new variable to the ::SIF format hash.
 * 
 * @details Given the hash key for a variable, create a new hash table entry
 * for the variable and initialize the structure elements.
 * 
 * @param[in] var_id the calculated unique hash key for the variable
 */
void add_var_format(int var_id)
{
  struct SIF *s;
  int uid = KEY_TO_UID(var_id);
  if (uid == 0)
    return; // an affine error term, not a real variable

  vars++;
  s = (struct SIF *) xmalloc(sizeof(struct SIF));
  initialize_format(s);
  s->id = var_id;
  //
  // Look for an existing format definition for this variable where the
  // pass count is 0 and the array index is ignored. If found, copy the
  // format information from that variable's format.
  //
  int declid = UID_IDX_TO_KEY(uid, NOT_AN_ARRAY);
  if (var_id != declid) {
    struct SIF *declfmt;
    HASH_FIND_INT(var_formats, &declid, declfmt);
    if (NULL != declfmt) {
      s->size = declfmt->size;
      s->sgnd = declfmt->sgnd;
      s->has_attribute = declfmt->has_attribute;
      s->attrS = declfmt->attrS;
      s->attrI = declfmt->attrI;
      s->attrF = declfmt->attrF;
      s->attrE = declfmt->attrE;
      s->attrmax = declfmt->attrmax;
      s->attrmin = declfmt->attrmin;
      s->ptr_op = declfmt->ptr_op;
      s->iv = declfmt->iv;
      s->iter = declfmt->iter;
    }
  }
  HASH_ADD_INT(var_formats, id, s);
}


/**
 * @brief Get a pointer to a ::SIF, given its hash key.
 *
 * @details Given a variable ID, look for the hash table entry (::SIF struct)
 * for that variable. If found, return a pointer to the ::SIF. Else, return
 * NULL.
 *  
 * @param[in] var_id variable ID (hash key) of interest
 * @return    pointer to the ::SIF struct for the variable
 */
struct SIF *get_format_ptr(int var_id)
{
  struct SIF *s;
  HASH_FIND_INT(var_formats, &var_id, s);
  return s;
}

/**
 * @brief Find/create a pointer to a ::SIF, given its hash key.
 *
 * @details Given a variable ID, look for the hash table entry (::SIF struct)
 * for that variable. If found, return a pointer to the ::SIF. Else, create a
 * new ::SIF struct entry and return a pointer to it.
 *
 * @param[in] var_id variable ID (hash key) of interest
 * @return    pointer to the ::SIF struct for the variable
 */
struct SIF *find_var_format(int var_id)
{
  struct SIF *s;
  HASH_FIND_INT(var_formats, &var_id, s);
  if (NULL == s) {
    add_var_format(var_id);
    HASH_FIND_INT(var_formats, &var_id, s);
  }
  return s;
}

/**
 * @brief Delete the ::SIF definition for a variable.
 * @details Removes the ::SIF struct from the <tt>var_formats</tt> hash table,
 * deletes the linked list for its affine definition, and frees the memory
 * that was allocated for those structures.
 *
 * @param[in] var_format pointer to ::SIF structure
 */
void delete_var_format(struct SIF *var_format)
{
  delete_aa_list(&(var_format->aa));
  HASH_DEL(var_formats, var_format);
  free(var_format);
}

/**
 * @brief Deletes all variable data structures.
 * @details Removes the ::SIF and ::AA structures for all variables.
 */
void delete_all_formats()
{
  struct SIF *current_var_format, *tmp;
  HASH_ITER(hh, var_formats, current_var_format, tmp) {
    delete_var_format(current_var_format);
  }
}

/**
 * @brief Initialize the ::SIF definition for a variable.
 * @details Most of the elements of the ::SIF structure are given default
 * values. Setting the <tt>I</tt> and <tt>F</tt> elements to zero indicates
 * that the SIFE values are undefined. Setting the <tt>max</tt> value to zero
 * and the <tt>min</tt> value to one marks the range as undefined. Setting the
 * <tt>aa</tt> to NULL indicates that the affine equation is undefined.
 *
 * @param[in] s pointer to ::SIF structure
 */
void initialize_format(struct SIF *s)
{
  s->id = 0xdead;
  s->S = s->I = s->F = s->E = s->originalF = s->size = s->shift = s->sgnd = 0;
  s->max = double_int_zero; // special "uninitialized" values
  s->min = double_int_one;
  s->has_attribute = s->ptr_op = s->iv = 0;
  s->alias = s->iter = 0;
  s->aa = NULL;
}

/**
 * @brief Make a copy of a variable specification.
 * @details Everything is copied except the variable's id value and the special
 * fields needed to maintain the hash tree.
 *
 * @param[in] src pointer to ::SIF structure
 * @param[in,out] dest pointer to ::SIF structure
 */
void copy_format(struct SIF *src, struct SIF *dest)
{
  // DO NOT copy the alias element
  if (dest == src) {
    fprintf(stderr, "trying to copy a format to itself\n");
    return;
  }
  dest->S = src->S;
  dest->I = src->I;
  dest->F = src->F;
  dest->E = src->E;
  dest->originalF = src->originalF;
  dest->size = src->size;
  dest->shift = src->shift;
  dest->sgnd = src->sgnd;
  dest->max = src->max;
  dest->min = src->min;
  dest->has_attribute = src->has_attribute;
  dest->attrS = src->attrS;
  dest->attrI = src->attrI;
  dest->attrF = src->attrF;
  dest->attrE = src->attrE;
  dest->attrmax = src->attrmax;
  dest->attrmin = src->attrmin;
  dest->ptr_op = src->ptr_op;
  dest->iv = src->iv;
  dest->iter = src->iter;
  if (dest->aa != src->aa) {
    delete_aa_list(&(dest->aa));
    dest->aa = copy_aa_list(src->aa);
  }
}

/**
 * @brief Copy the ::SIF format of one variable to another.
 * @details Only the parameters related to the variable's fixed-point format
 * are copied.
 *
 * @param[in] src pointer to ::SIF structure
 * @param[in, out] dest pointer to ::SIF structure
 */
void copy_SIF(struct SIF *src, struct SIF *dest)
{
  if (dest == src) {
    fprintf(stderr, "trying to copy a SIF to itself\n");
    return;
  }
  dest->S = src->S;
  dest->I = src->I;
  dest->F = src->F;
  dest->E = src->E;
  dest->max = src->max;
  dest->min = src->min;
  dest->iv = src->iv;
  if (dest->aa != src->aa) {
    delete_aa_list(&(dest->aa));
    dest->aa = copy_aa_list(src->aa);
  }
}

/**
 * @brief Return range maximum as a floating-point value.
 *
 * @param[in] op_fmt pointer to ::SIF structure
 * @return    floating-point value of max
 */
float real_max(struct SIF *op_fmt)
{
  return ((float) double_int_to_shwi(op_fmt->max) /
          (1ULL << (op_fmt->F + op_fmt->E)));
}

/**
 * @brief Return range minimum as a floating-point value.
 *
 * @param[in] op_fmt pointer to ::SIF structure
 * @return    floating-point value of min
 */
float real_min(struct SIF *op_fmt)
{
  return ((float) double_int_to_shwi(op_fmt->min) /
          (1ULL << (op_fmt->F + op_fmt->E)));
}

/**
 * @brief Remove unimportant variables from the ::SIF hash table.
 * @details Before printing the fixed-point information about all of the
 * variables we delete the hash table entries for variables that are not
 * important.
 *
 * This is done by iterating through gcc's list of declared variables, not by
 * iterating through the ::SIF hash table. Information for induction variables
 * is discarded, as is that of any variable where the fixed-point format is
 * undefined. If there is an entry for a version of a variable (meaning that
 * it has an array index and/or a non-zero pass number) then compare the SIF
 * values for the version and the underlying variable...delete the version if
 * they are the same.
 *
 * We also look for some error conditions. All versions of a function parameter
 * declaration or the function's return value should have the same SIF format.
 * All versions of a pointer should be the same, and alias variables should
 * have the same format as the aliased variable.
 */
void cleanup_formats()
{
  tree var;
  referenced_var_iterator rvi;
  struct SIF *var_fmt;     //< points to SIF struct for declared variable
  struct SIF *vers_fmt;    //< points to ::SIF struct for a version of variable
  struct SIF *is_array;    //< a fake pointer, tests for arrays
  int i, j, key, elements;

  FOR_EACH_REFERENCED_VAR(cfun, var, rvi) {
    key = calc_hash_key(var, 0, NOT_AN_ARRAY);
    HASH_FIND_INT(var_formats, &key, var_fmt);
    if (NULL != var_fmt) {
      key = calc_hash_key(var, 0, 0);
      HASH_FIND_INT(var_formats, &key, is_array);
      if (is_array)
        elements = MAX_ELEMENTS;
      else
        elements = 1;
      for (i = 0; i < MAX_PASSES; i++) {
        for (j = 0; j < elements; j++) {
          key = calc_hash_key(var, i, j);
          HASH_FIND_INT(var_formats, &key, vers_fmt);
          if ((NULL != vers_fmt) && (var_fmt != vers_fmt)) {
            if ((var_fmt->iv) || (!format_initialized(*vers_fmt))) {
              delete_aa_list(&(vers_fmt->aa));
              HASH_DEL(var_formats, vers_fmt);
              free(vers_fmt);
            } else if (format_initialized(*var_fmt)) {
              if ((var_fmt->S != vers_fmt->S) || (var_fmt->I != vers_fmt->I) ||
                  (var_fmt->F != vers_fmt->F) || (var_fmt->E != vers_fmt->E)) {
                var_fmt->iv = 1;        // use iv as a "don't print" flag
                if (!var_fmt->has_attribute) {
                  delete_aa_list(&(vers_fmt->aa));
                  HASH_DEL(var_formats, vers_fmt);
                  free(vers_fmt);
                }
              } else {
                delete_aa_list(&(vers_fmt->aa));
                HASH_DEL(var_formats, vers_fmt);
                free(vers_fmt);
              }
            } else {
              copy_format(vers_fmt, var_fmt);
              // don't delete aa list, shallow copy to var_fmt
              HASH_DEL(var_formats, vers_fmt);
              free(vers_fmt);
            }
          }                     // if vers_fmt is initialized
        }                       // for each element of var
      }                         // for each pass through the BB
      if (((var_fmt->iv) || (!format_initialized(*var_fmt)))
          && (!var_fmt->alias)) {
        if (PARM_DECL == TREE_CODE(var)) {
          warning(0, G_("Inconsistent format of a function parameter"));
          var_fmt->S = var_fmt->I = var_fmt->F = var_fmt->iv = 0;
          var_fmt->E = var_fmt->size;
        } else if (DECL_RESULT(current_function_decl) == var) {
          warning(0, G_("Inconsistent format of function return value"));
          var_fmt->S = var_fmt->I = var_fmt->F = var_fmt->iv = 0;
          var_fmt->E = var_fmt->size;
        } else if (var_fmt->has_attribute) {
          tree var_tree = referenced_var_lookup(cfun, KEY_TO_UID(var_fmt->id));
          if (DECL_NAME(var_tree)) {
            warning(0,
                    G_
                    ("Inconsistent format of variable %s (with attribute)"),
                    IDENTIFIER_POINTER(DECL_NAME(var_tree)));
          } else {
            warning(0,
                    G_
                    ("Inconsistent format of variable %c%4u (with attribute)"),
                    TREE_CODE(var_tree) == CONST_DECL ? 'C' : 'D',
                    DECL_UID(var_tree));
          }
          var_fmt->iv = 0;
        } else if (var_fmt->ptr_op) {
          tree var_tree = referenced_var_lookup(cfun, KEY_TO_UID(var_fmt->id));
          if (DECL_NAME(var_tree)) {
            warning(0,
                    G_("Inconsistent format of pointer %s"),
                    IDENTIFIER_POINTER(DECL_NAME(var_tree)));
          } else {
            fprintf(stderr, "Inconsistent format of pointer %c%4u\n",
                    TREE_CODE(var_tree) == CONST_DECL ? 'C' : 'D',
                    DECL_UID(var_tree));
          }
          var_fmt->S = var_fmt->I = var_fmt->F = var_fmt->iv = 0;
          var_fmt->E = var_fmt->size;
        } else {
          delete_aa_list(&(var_fmt->aa));
          HASH_DEL(var_formats, var_fmt);
          free(var_fmt);
        }
      }
    }                           // if var_fmt is not NULL
  }                             // for each referenced var
}

/**
 * @brief Force consistency of ::SIF formats for versions of pointers.
 * @details We assume that memory locations accessed via pointers will only
 * be modified once, if at all, in any given GIMPLE basic block. At the end
 * of each basic block we look for aliased variables and recompute the range
 * for them by finding the widest range that would include the ranges of
 * all aliases of the variable. At this point we've lost any valid affine
 * definition so the variable is given a new affine definition that just
 * specifies the range of the variable, without correlations to any input
 * parameters.
 */
void force_ptr_consistency()
{
  struct SIF *s;
  struct SIF *ss;
  double_int max, min, aamax, aamin, x0, x1;
  int uid, j, bp, key, index;

  for (s = var_formats; s != NULL; s = (struct SIF *) (s->hh.next)) {
    //
    // Look for aliased variables that were modified in this pass, push
    // the new range information to the target
    //
    if ((s->alias) && (KEY_TO_PASS(s->id) == fxpass) && (NULL != s->aa)) {
      index = KEY_TO_IDX(s->id);
      //
      // Look for the target in this pass
      //
      key = UID_PASS_IDX_TO_KEY(KEY_TO_UID(s->alias), fxpass, index);
      ss = get_format_ptr(key);
      if (NULL == ss) {
        add_var_format(key);
        ss = get_format_ptr(key);
        if (ss->has_attribute) {
          ss->S = ss->attrS;
          ss->I = ss->attrI;
          ss->F = ss->attrF;
          ss->E = ss->attrE;
          ss->max = ss->attrmax;
          ss->min = ss->attrmin;
        }
      }
      //
      // Expect the same binary point location
      //
      bp = s->F + s->E;
      if (bp != (ss->F + ss->E))
        fprintf(stderr, "  Inconsistent binary point locations");

      ss->max = double_int_smax(s->max, ss->max);
      ss->min = double_int_smin(s->min, ss->min);
      aamax = double_int_smax(aa_max(ss->aa), aa_max(s->aa));
      aamin = double_int_smin(aa_min(ss->aa), aa_min(s->aa));

      x0 = double_int_rshift(double_int_add(aamax, aamin), 1,
                             HOST_BITS_PER_DOUBLE_INT, ARITH);
      x1 = double_int_rshift(double_int_sub(aamax, aamin), 1,
                             HOST_BITS_PER_DOUBLE_INT, ARITH);

      key = UID_PASS_IDX_TO_KEY(KEY_TO_UID(ss->id), fxpass, index);
      delete_aa_list(&(ss->aa));
      if (!double_int_zero_p(x0))
        append_aa_var(&(ss->aa), 0, x0, bp);
      if (!double_int_zero_p(x1))
        append_aa_var(&(ss->aa), key, x1, bp);
      s->alias = key;
    }                           // found an aliased variable in this pass
  }                             // all formats

  HASH_SORT(var_formats, id_sort);
  uid = 0;
  for (s = var_formats; s != 0; s = (struct SIF *) (s->hh.next)) {
    //
    // If the uid has changed and the saved uid is nonzero then we have
    // been accumulating the min and max for the saved uid. Store them
    // in the formats for every index of that uid in this pass
    //
    if ((0 != uid) && (KEY_TO_UID(s->id) != uid)) {
      for (j = 0; j < MAX_ELEMENTS; j++) {
        key = UID_PASS_IDX_TO_KEY(uid, fxpass, j);
        HASH_FIND_INT(var_formats, &key, ss);
        if (NULL != ss) {
          ss->max = max;
          ss->min = min;
          x0 = double_int_rshift(double_int_add(aamax, aamin), 1,
                                 HOST_BITS_PER_DOUBLE_INT, ARITH);
          x1 = double_int_rshift(double_int_sub(aamax, aamin), 1,
                                 HOST_BITS_PER_DOUBLE_INT, ARITH);
          delete_aa_list(&(ss->aa));
          if (!double_int_zero_p(x0))
            append_aa_var(&(ss->aa), 0, x0, bp);
          if (!double_int_zero_p(x1))
            append_aa_var(&(ss->aa), key, x1, bp);
        }
      }
      uid = 0;
      max = double_int_zero;
      min = double_int_zero;
      aamax = double_int_zero;
      aamin = double_int_zero;
    }
    //
    // This variable is a pointer that we've already seen, in this pass
    //
    if ((KEY_TO_UID(s->id) == uid) && (KEY_TO_PASS(s->id) == fxpass)) {
      if (NOT_AN_ARRAY != KEY_TO_IDX(s->id)) {
        max = double_int_smax(max, s->max);
        min = double_int_smin(min, s->min);
        aamax = double_int_smax(aamax, aa_max(s->aa));
        aamin = double_int_smin(aamin, aa_min(s->aa));
      }
      //
      // Look for pointer variables that were modified in this pass
      // Since the hash is sorted, this must be the lowest index 
      //
    } else if ((s->ptr_op) && !(s->alias) && (KEY_TO_PASS(s->id) == fxpass)) {
      uid = KEY_TO_UID(s->id);
      max = s->max;
      min = s->min;
      aamax = aa_max(s->aa);
      aamin = aa_min(s->aa);
      bp = s->F + s->E;
    }
  }
}

/**
 * @brief Print formats of ::SIF hash table variables used in this pass.
 * @details Sort them by id first, just for cosmetic reasons. Don't print
 * if the SIF format is undefined.
 */
void print_var_formats()
{
  struct SIF *s;

  HASH_SORT(var_formats, id_sort);
  for (s = var_formats; s != NULL; s = (struct SIF *) (s->hh.next)) {
    if (format_initialized(*s) && (fxpass == KEY_TO_PASS(s->id)))
      print_one_format(s);
  }
}

/**
 * @brief Print the S, I, F, and E values for one ::SIF hash table entry.
 *
 * @param[in] s pointer to ::SIF structure
 */
void print_one_format(struct SIF *s)
{
  if (NULL == s)
    return;

  int uid = KEY_TO_UID(s->id);
  int idx = KEY_TO_IDX(s->id);
  tree var_tree = referenced_var_lookup(cfun, uid);
  if (s->alias != 0) {
    fprintf(stderr, "//@(%2d/%2d/%2d/%2d)", s->S, s->I, s->F, s->E);
  }
  else if (s->iv) {
    fprintf(stderr, "// (%2d/%2d/--/--)", s->S, s->I);
  } else {
    fprintf(stderr, "// (%2d/%2d/%2d/%2d)", s->S, s->I, s->F, s->E);
  }
  if (s->has_attribute != 0)
    fprintf(stderr, "=");
  else
    fprintf(stderr, " ");
  if (s->ptr_op != 0)
    fprintf(stderr, "*");
  else
    fprintf(stderr, " ");
  if (DECL_NAME(var_tree)) {
    fprintf(stderr, "%s", IDENTIFIER_POINTER(DECL_NAME(var_tree)));
  } else {
    fprintf(stderr, "%c%4u", TREE_CODE(var_tree) == CONST_DECL ? 'C' : 'D',
            DECL_UID(var_tree));
  }
  if (idx != NOT_AN_ARRAY)
    fprintf(stderr, "[%2d]", idx);

  fprintf(stderr, "\n");
}

/**
 * @brief Print the S, I, F, and E values and affine definition for a ::SIF
 * struct.
 *
 * @param[in] s pointer to ::SIF structure
 */
void print_one_aa_format(struct SIF *s)
{
  if (NULL == s)
    return;

  int uid = KEY_TO_UID(s->id);
  int idx = KEY_TO_IDX(s->id);
  int pass = KEY_TO_PASS(s->id);
  tree var_tree = referenced_var_lookup(cfun, uid);
  if (s->alias != 0) {
    fprintf(stderr, "//@(%2d/%2d/%2d/%2d)", s->S, s->I, s->F, s->E);
  }
  else if (s->iv) {
    fprintf(stderr, "// (%2d/%2d/--/--)", s->S, s->I);
  } else {
    fprintf(stderr, "// (%2d/%2d/%2d/%2d)", s->S, s->I, s->F, s->E);
  }
  if (s->has_attribute != 0)
    fprintf(stderr, "=");
  else
    fprintf(stderr, " ");
  if (s->ptr_op != 0)
    fprintf(stderr, "*");
  else
    fprintf(stderr, " ");
  if (DECL_NAME(var_tree)) {
    fprintf(stderr, "%s", IDENTIFIER_POINTER(DECL_NAME(var_tree)));
  } else {
    fprintf(stderr, "%c%4u", TREE_CODE(var_tree) == CONST_DECL ? 'C' : 'D',
            DECL_UID(var_tree));
  }
  if (idx != NOT_AN_ARRAY)
    fprintf(stderr, "[%2d]", idx);

  fprintf(stderr, "#%d", pass);
  if ((s->alias != 0) && (s->alias != s->id)) {
    tree var_tree = referenced_var_lookup(cfun, KEY_TO_UID(s->alias));
    if (NULL != var_tree) 
      fprintf(stderr, "->%s = ", IDENTIFIER_POINTER(DECL_NAME(var_tree)));
    else
      fprintf(stderr, "->?????? = ");
    print_aa_list(s->aa);
    fprintf(stderr, "\n");
  } else {
    fprintf(stderr, " = ");
    print_aa_list(s->aa);
    fprintf(stderr, "\n");
  }
}

/**
 * @brief Print the S, I, F, and E values and range min/max for a ::SIF struct.
 *
 * @param[in] op_fmt an operands ::SIF structure
 */
void print_format(struct SIF op_fmt)
{
  if (op_fmt.iv)
    fprintf(stderr, "(%2d/%2d/--/--)", op_fmt.S, op_fmt.I);
  else
    fprintf(stderr, "(%2d/%2d/%2d/%2d)", op_fmt.S, op_fmt.I, op_fmt.F,
            op_fmt.E);
  if (op_fmt.sgnd) {
    fprintf(stderr, "s");
  } else {
    fprintf(stderr, "u");
  }
  print_min_max(op_fmt);
}

/**
 * @brief Restore the fxfrmt attribute values for all variables.
 * @details Before each fxopt pass through a function we restore any
 * SIF parameters that were defined by an fxfrmt attribute. The affine
 * definition is created based on the specified min and max range values.
 *
 * @todo This should also be done for variables with an initial value.
 */
void restore_attributes()
{
  struct SIF *s;
  for (s = var_formats; s != NULL; s = (struct SIF *) (s->hh.next)) {
    if (s->has_attribute != 0) {
      s->S = s->attrS;
      s->I = s->attrI;
      s->F = s->attrF;
      s->E = s->attrE;
      s->min = s->attrmin;
      s->max = s->attrmax;
      double_int x0 = double_int_rshift(double_int_add(s->max, s->min), 1,
                                          HOST_BITS_PER_DOUBLE_INT, ARITH);
      double_int x1 = double_int_rshift(double_int_sub(s->max, s->min), 1,
                                           HOST_BITS_PER_DOUBLE_INT, ARITH);
      delete_aa_list(&(s->aa));
      if (!double_int_zero_p(x0))
         append_aa_var(&(s->aa), 0, x0, (s->F + s->E));
      if (!double_int_zero_p(x1))
         append_aa_var(&(s->aa), s->id, x1, (s->F + s->E));
    }
  }
}

/**
 * @brief Convert floating operations to integer operations.
 *
 * @details There are some GIMPLE assignment operations that are defined only
 * for floating-point operands, and for these operations the GIMPLE statement
 * is modified to use an appropriate integer operation instead.
 *
 * @note Only the operation is modified, the operands are not affected.
 *
 * @note This only works for binary expressions, for unary expressions
 * the "rhs_code" is actually the code for operand 1, which gets fixed when
 * we call get_operand_format.
 *
 * @param[in] stmt a GIMPLE assignment statement
 */
void real_expr_to_integer(gimple stmt)
{
  if (! lastpass) return;

  enum tree_code stmt_rhs_code = gimple_assign_rhs_code(stmt);
  switch (stmt_rhs_code) {
    case RDIV_EXPR:
      gimple_assign_set_rhs_code(stmt, TRUNC_DIV_EXPR);
      break;
    case FIX_TRUNC_EXPR:
      gimple_assign_set_rhs_code(stmt, NOP_EXPR);
      break;
    case FLOAT_EXPR:
      gimple_assign_set_rhs_code(stmt, NOP_EXPR);
      break;
    default:
      return;
  }
  update_stmt(stmt);
}

/**
 * @brief Convert floating variables to integers.
 * @details This function changes only the type of the variable, not its value.
 * In all cases we change the type of the innermost underlying variable but
 * the type of a variable may be specified multiple places, depending on the
 * kind of variable.
 *
 *   - If the var is a function parameter, change the function's parameter
 *     type as well.
 *
 *   - If the var is a pointer, change the pointer's type as well.
 *
 *   - If the var is an array, also change the size of whole array and
 *     the size of each array element. The mode, constant, and public
 *     parameters for the array are configured.
 *
 * @note The specific integer type used to replace floating types is defined
 * by REAL_TO_INTEGER_TYPE.
 *
 * @param[in, out] var gcc tree for a variable
 */
void convert_real_var_to_integer(tree var)
{
  tree orig_inner_type = get_innermost_type(var);

  if (REAL_TYPE == TREE_CODE(orig_inner_type)) {
    int constant = orig_inner_type->base.constant_flag;
    if (TREE_CODE(var) == PARM_DECL) {
      DECL_ARG_TYPE(var) = REAL_TO_INTEGER_TYPE;
    }
    if ((TREE_CODE(var) == VAR_DECL) || (TREE_CODE(var) == PARM_DECL)
        || (TREE_CODE(var) == RESULT_DECL)) {
      if (POINTER_TYPE == TREE_CODE(TREE_TYPE(var))) {
        tree base_var = TREE_TYPE(TREE_TYPE(var));
        if (REAL_TYPE == TREE_CODE(base_var)) {
          TREE_TYPE(TREE_TYPE(var)) = REAL_TO_INTEGER_TYPE;
        } else if (ARRAY_TYPE == TREE_CODE(base_var)) {
          int elements =
              TREE_INT_CST_LOW(TYPE_SIZE(base_var)) /
              TREE_INT_CST_LOW(TYPE_SIZE(orig_inner_type));
          int type_size =
              elements * TREE_INT_CST_LOW(TYPE_SIZE(REAL_TO_INTEGER_TYPE));
          int type_size_unit =
              elements *
              TREE_INT_CST_LOW(TYPE_SIZE_UNIT(REAL_TO_INTEGER_TYPE));
          TYPE_SIZE(base_var) = build_int_cst(integer_type_node, type_size);
          TYPE_SIZE_UNIT(base_var) =
              build_int_cst(integer_type_node, type_size_unit);
          base_var->type.mode = mode_for_size(type_size, MODE_INT, 0);
          TREE_TYPE(base_var) = REAL_TO_INTEGER_TYPE;
          TREE_TYPE(base_var)->base.constant_flag =
              orig_inner_type->base.constant_flag;
          TREE_TYPE(base_var)->base.public_flag =
              orig_inner_type->base.public_flag;
        } else {
          fprintf(stderr, " Trying to convert pointer to unknown real DECL.\n");
        }
      } else if (ARRAY_TYPE == TREE_CODE(TREE_TYPE(var))) {
        int elements =
            TREE_INT_CST_LOW(DECL_SIZE(var)) /
            TREE_INT_CST_LOW(TYPE_SIZE(orig_inner_type));
        int decl_size =
            elements * TREE_INT_CST_LOW(TYPE_SIZE(REAL_TO_INTEGER_TYPE));
        int decl_size_unit =
            elements *
            TREE_INT_CST_LOW(TYPE_SIZE_UNIT(REAL_TO_INTEGER_TYPE));
        DECL_SIZE(var) = build_int_cst(integer_type_node, decl_size);
        DECL_SIZE_UNIT(var) =
            build_int_cst(integer_type_node, decl_size_unit);
        DECL_MODE(var) = mode_for_size(decl_size, MODE_INT, 0);
        TREE_TYPE(TREE_TYPE(var)) = REAL_TO_INTEGER_TYPE;
        TREE_TYPE(TREE_TYPE(var))->base.constant_flag =
            orig_inner_type->base.constant_flag;
        TREE_TYPE(TREE_TYPE(var))->base.public_flag =
            orig_inner_type->base.public_flag;
      } else if (TREE_TYPE(var) == orig_inner_type) {
        DECL_SIZE(var) = TYPE_SIZE(REAL_TO_INTEGER_TYPE);
        DECL_SIZE_UNIT(var) = TYPE_SIZE_UNIT(REAL_TO_INTEGER_TYPE);
        DECL_MODE(var) = TYPE_MODE(REAL_TO_INTEGER_TYPE);
        TREE_TYPE(var) = REAL_TO_INTEGER_TYPE;
        TREE_TYPE(var)->base.constant_flag = constant;
        TREE_TYPE(var)->base.public_flag =
            orig_inner_type->base.public_flag;
      } else {
        fprintf(stderr, " Trying to convert unknown real DECL.\n");
      }
    } else {
      fprintf(stderr, " Trying to convert unknown real var to integer.\n");
    }
  }
}

/**
 * @brief Changes a function declaration from floating to integer.
 * @details This modifies the declaration for the function itself as well as
 * for the function arguments.
 *
 * @param[in] decl tree for a function declaration
 */
void convert_real_func_to_integer(tree decl)
{
  function_args_iterator args_iter;
  tree *an_arg;

  FOREACH_FUNCTION_ARGS_PTR(TREE_TYPE(decl), an_arg, args_iter) {
    if (TREE_CODE(*an_arg) == REAL_TYPE)
      *an_arg = REAL_TO_INTEGER_TYPE;
  }
  if (SCALAR_FLOAT_TYPE_P(TREE_TYPE(TREE_TYPE(decl)))) {
    TREE_TYPE(TREE_TYPE(decl)) = REAL_TO_INTEGER_TYPE;
  }
}

/**
 * @brief Replace a constant operand with its reciprocal.
 * @details Integer constants are first converted to a real constant and then
 * the reciprocal is calculated. The statement is modified to replace the
 * original constant with its reciprocal.
 *
 * @param[in] stmt GIMPLE statement to be modified
 * @param[in] opnumber operand number for the constant operand
 */
void invert_constant_operand(gimple stmt, int opnumber)
{
  REAL_VALUE_TYPE real_const, inv_const;

  tree operand = gimple_op(stmt, opnumber);

  if (REAL_CST == TREE_CODE(operand)) {
    real_const = TREE_REAL_CST(operand);
  } else if (INTEGER_CST == TREE_CODE(operand)) {
    real_const = real_value_from_int_cst(double_type_node, operand);
  } else {
    warning(0, G_("fxopt: can't invert this constant type"));
  }

  real_arithmetic(&inv_const, RDIV_EXPR, &dconst1, &real_const);

  gimple_set_op(stmt, opnumber, build_real(double_type_node, inv_const));
  update_stmt(stmt);
}

/**
 * @brief Determine I, F, and E values for a real constant.
 * @details First determine the required number of integer bits. If the constant
 * is exactly zero then use one I bit, and never use a negative value for I.
 * Assume that the constant is signed and has one S bit. Calculate the
 * number of available fraction bits by subtracting S and I from the operand
 * size. Round the constant to the available precision, convert to an integer,
 * and count the number of zeros at the left end. Set the value of E and
 * subtract E from F. Finally, the range and affine definition of the constant
 * are defined appropriately.
 *
 * @todo The integer variables used here are `HOST_WIDE_INT`s. Make sure
 * this function works if the integer type used to replace floats has more
 * bits than a `HOST_WIDE_INT`. May need to replace the
 * `real_to_integer` function calls.
 *
 * @param[in] real_const a GIMPLE REAL_VALUE_TYPE
 * @return    ::SIF structure for the constant
 */
struct SIF get_format_real_value_type(REAL_VALUE_TYPE real_const)
{
  REAL_VALUE_TYPE scaled_real, rounded_real;
  HOST_WIDE_INT integer_constant;

  struct SIF op_fmt;
  initialize_format(&op_fmt);

  if (real_compare(EQ_EXPR, &real_const, &dconst0))
    op_fmt.I = 1;
  else
    op_fmt.I = MAX(real_exponent(&real_const), 0);
  op_fmt.size = TREE_INT_CST_LOW(TYPE_SIZE(REAL_TO_INTEGER_TYPE));
  op_fmt.sgnd = op_fmt.S = 1;
  op_fmt.F = op_fmt.size - op_fmt.S - op_fmt.I;

  if ((op_fmt.S + op_fmt.I) > op_fmt.size)
    error("fxopt: real constant is too big");

  real_ldexp(&scaled_real, &real_const, op_fmt.F);
  real_round(&rounded_real, VOIDmode, &scaled_real);
  integer_constant = real_to_integer(&rounded_real);

  op_fmt.E = MIN(op_fmt.F, ctz_hwi(integer_constant));
  op_fmt.F -= op_fmt.E;
  // 
  // If op_fmt.F is zero this is an exact integer, right justify it.
  // Ignore the scaling and rounding.
  // 
  if (0 == op_fmt.F) {
    op_fmt.S += op_fmt.E;
    op_fmt.E = 0;
    integer_constant = real_to_integer(&real_const);
  }
  op_fmt.min = op_fmt.max = shwi_to_double_int(integer_constant);

  append_aa_var(&(op_fmt.aa), 0, op_fmt.max, (op_fmt.F + op_fmt.E));
  
  return op_fmt;
}

/**
 * @brief Convert a real constant tree to an integer constant tree.
 *
 * @details Also modifies the operand format structure passed as a parameter.
 *
 * @note The type used for the integer constant tree is specified by
 * `REAL_TO_INTEGER_TYPE`.
 *
 * @todo This function is sometimes called without using the return value.
 * That's a memory leak that should be eliminated.
 *
 * @param[in] real_cst gcc tree for the real constant
 * @param[in,out] op_fmt_p pointer to ::SIF structure for the operand
 * @return    gcc tree for the integer (fixed-point) constant
 */
tree convert_real_constant(tree real_cst, struct SIF *op_fmt_p)
{
  REAL_VALUE_TYPE real_const;
  tree integer_constant;
  struct SIF temp_fmt;
  initialize_format(&temp_fmt);

  real_const = TREE_REAL_CST(real_cst);
  temp_fmt = get_format_real_value_type(real_const);
  op_fmt_p->S = temp_fmt.S;
  op_fmt_p->I = temp_fmt.I;
  op_fmt_p->F = temp_fmt.F;
  op_fmt_p->E = temp_fmt.E;
  op_fmt_p->size = temp_fmt.size;
  op_fmt_p->sgnd = temp_fmt.sgnd;
  op_fmt_p->min = temp_fmt.min;
  op_fmt_p->max = temp_fmt.max;
  op_fmt_p->aa = temp_fmt.aa;

  integer_constant = double_int_to_tree(REAL_TO_INTEGER_TYPE,op_fmt_p->max);

  return integer_constant;
}

/**
 * @brief Determine the fixed-point format for an integer constant.
 *
 * @details The constant is assumed to be a true integer with no F or E bits.
 * The number of I bits is determined by the magnitude of the integer unless
 * the integer is exactly zero, then it gets an I value of 1. If the constant
 * is negative the format is assumed to be signed with one sign bit, else
 * the value of S is zero. The range min and max are set to the constant's
 * value, as is the center element of the affine definition.
 *
 * @note Constant operands are not the same as variables and don't have a
 * specified size in GIMPLE. The size used in the SIF format is just the
 * value of I+S.
 *
 * @param[in] integer_cst gcc tree for the integer constant
 * @param[in,out] op_fmt_p pointer to ::SIF structure for the constant
 */
void int_constant_format(tree integer_cst, struct SIF *op_fmt_p)
{
  if (integer_zerop(integer_cst))
    op_fmt_p->I = 1;
  else
    op_fmt_p->I = tree_int_cst_min_precision(integer_cst, TRUE);
  op_fmt_p->F = op_fmt_p->E = 0;
  op_fmt_p->S = op_fmt_p->sgnd = tree_int_cst_sgn(integer_cst) < 0 ? 1 : 0;
  op_fmt_p->size = op_fmt_p->I + op_fmt_p->S;
  op_fmt_p->min = tree_to_double_int(integer_cst);
  op_fmt_p->max = tree_to_double_int(integer_cst);
  op_fmt_p->shift = op_fmt_p->has_attribute = op_fmt_p->ptr_op = 0;
  op_fmt_p->iv = op_fmt_p->alias = 0;
  delete_aa_list(&(op_fmt_p->aa));
  append_aa_var(&(op_fmt_p->aa), 0, op_fmt_p->max, 0);
}

/**
 * @brief Determine the format of a constant's reciprocal.
 *
 * @details The constant itself is not modified, we just determine what the
 * fixed-point format of the reciprocal would be. Integer constants are
 * converted to reals and the math is done on the real constant's reciprocal.
 *
 * @param[in] stmt GIMPLE statement with a constant operand
 * @param[in] op_number operand number of the constant
 * @return    ::SIF structure for the constant's reciprocal
 */
struct SIF get_inverted_operand_format(gimple stmt, int op_number)
{
  REAL_VALUE_TYPE real_const, inv_const;

  struct SIF op_fmt;
  initialize_format(&op_fmt);

  tree operand = gimple_op(stmt, op_number);

  if (REAL_CST == TREE_CODE(operand)) {
    real_const = TREE_REAL_CST(operand);
  } else if (INTEGER_CST == TREE_CODE(operand)) {
    real_const = real_value_from_int_cst(double_type_node, operand);
  } else {
    warning(0, G_("fxopt: can't invert this constant type"));
  }

  real_arithmetic(&inv_const, RDIV_EXPR, &dconst1, &real_const);

  op_fmt = get_format_real_value_type(inv_const);

  return op_fmt;
}
/**
 * @brief Find the innermost type for an operand.
 *
 * @details If a variable is a memory reference, an array element, or an SSA
 * name then drill down until we find the core type of the variable.
 *
 * @param[in] vardecl operand's GIMPLE tree
 * @return    GIMPLE tree of the innermost type
 */
tree get_innermost_type(tree vardecl)
{
  tree innertype;
  innertype = vardecl;
  while (TREE_TYPE(innertype) != NULL) {
    if (ARRAY_REF == TREE_CODE(innertype) || MEM_REF == TREE_CODE(innertype)) {
      innertype = TREE_OPERAND(innertype, 0);
    } else if (SSA_NAME == TREE_CODE(innertype)) {
      innertype = SSA_NAME_VAR(innertype);
    } else {
      innertype = TREE_TYPE(innertype);
    }
  }
  return innertype;
}

/**
 * @brief Find the underlying variable declaration tree for an operand.
 * @details Operands can be a composite of pointers, array elements, memory
 * references and SSA names. Drill down to the core declaration.
 *
 * @param[in] operand GIMPLE tree for an operand
 * @return    GIMPLE tree for the underlying variable declaration
 */
tree get_operand_decl(tree operand)
{
  tree var = NULL;

  if (TREE_CONSTANT(operand)) {
    return var;
  } else {
    if ((VAR_DECL == TREE_CODE(operand))
        || (PARM_DECL == TREE_CODE(operand))
        || (RESULT_DECL == TREE_CODE(operand))
        || (SSA_NAME == TREE_CODE(operand))) {
      var = operand;
    } else if ((ARRAY_REF == TREE_CODE(operand))
               || (MEM_REF == TREE_CODE(operand))) {
      var = TREE_OPERAND(operand, 0);
      if (MEM_REF == TREE_CODE(var)) {
        var = TREE_OPERAND(var, 0);
      }
    }
    if (SSA_NAME == TREE_CODE(var)) {
      var = SSA_NAME_VAR(var);
    }
  }
  if ((VAR_DECL != TREE_CODE(var)) && (PARM_DECL != TREE_CODE(var))
      && (RESULT_DECL != TREE_CODE(var))) {
    error("fxopt: Error getting operand declaration");
  }
  return var;
}

/**
 * @brief Determine the SIF format of a statement operand.
 *
 * @details Given a statement gimple, an operand number, and possibly the
 * element number for an array, return the ::SIF format for the operand If
 * parameter print is true, pretty print the format to stderr
 *
 * @param[in] stmt           gimple statement being processed
 * @param[in] op_number      operand number being retrieved
 * @param[in] element_number array element number of the operand
 * @param[in] print          true causes format to be printed
 * @return                   ::SIF structure for the operand
 */
struct SIF get_operand_format(gimple stmt, int op_number,
                              int element_number, bool print)
{
  struct SIF op_fmt;
  initialize_format(&op_fmt);

  int uid, pass, index;
  uid = pass = 0;
  index = NOT_AN_ARRAY;

  tree operand = gimple_op(stmt, op_number);

  tree inner_type = get_innermost_type(operand);

  if (TREE_CONSTANT(operand)) {
    if (REAL_CST == TREE_CODE(operand)) {
      if (lastpass) {
        gimple_set_op(stmt, op_number, convert_real_constant(operand, &op_fmt));
        update_stmt(stmt);
        print_gimple_stmt(stderr, stmt, 2, 0);
      } else {
        convert_real_constant(operand, &op_fmt);
      }
    } else if (INTEGER_CST == TREE_CODE(operand)) {
      int_constant_format(operand, &op_fmt);
    } else {
      warning(0, G_("Unexpected constant operand encountered"));
    }
  } else {
    tree var = NULL;
    tree index_var = NULL;
    if (SSA_NAME == TREE_CODE(operand)) {
      var = SSA_NAME_VAR(operand);
      // operand tree must have same type as underlying variable
      TREE_TYPE(operand) = TREE_TYPE(var);
    } else if (VAR_DECL == TREE_CODE(operand)) {
      var = operand;
    } else if (ARRAY_REF == TREE_CODE(operand)) {
      // find the declaration of the underlying variable
      var = TREE_OPERAND(operand, 0);
      index_var = TREE_OPERAND(operand, 1);
      if (INTEGER_CST == TREE_CODE(index_var)) {
        index = TREE_INT_CST_LOW(index_var);
      } else if (SSA_NAME == TREE_CODE(index_var)) {
        index_var = SSA_NAME_VAR(index_var);
        uid = DECL_UID(index_var);
        struct SIF *idx_fmt = get_format_ptr(
            UID_PASS_IDX_TO_KEY(uid,0,NOT_AN_ARRAY));
        index = idx_fmt->shift;
        if (op_number > 0) {
          idx_fmt->shift = idx_fmt->shift + 1;
          if (idx_fmt->shift > (int) double_int_to_uhwi(idx_fmt->max))
            idx_fmt->shift = 0;
        }
      } else {
        error("fxopt: Unexpected index operand for ARRAY_REF");
      }
      if (MEM_REF == TREE_CODE(var)) {
        var = TREE_OPERAND(var, 0);
        if (SSA_NAME == TREE_CODE(var)) {
          var = SSA_NAME_VAR(var);
        } else {
          error("fxopt: Unexpected operand of a MEM_REF");
        }
      } else {
        if (INTEGER_CST != TREE_CODE(TREE_OPERAND(operand, 1))) {
          error("fxopt: get_operand_format: Error getting array index");
        }
      }
      if ((VAR_DECL != TREE_CODE(var)) && (PARM_DECL != TREE_CODE(var))) {
        error
            ("fxopt: get_operand_format: Error getting array operand VAR_DECL/PARM_DECL");
      }
      // operand tree must have same type as underlying variable
      if (ARRAY_TYPE == TREE_CODE(TREE_TYPE(var))) {
        TREE_TYPE(operand) = inner_type;
      } else if (POINTER_TYPE == TREE_CODE(TREE_TYPE(var))) {
        TREE_TYPE(operand) = inner_type;
        if (ARRAY_TYPE == TREE_CODE(TREE_TYPE(TREE_TYPE(var)))) {
          tree array_tree = TREE_TYPE(TREE_TYPE(var));
          TREE_TYPE(array_tree) = strip_array_types(TREE_TYPE(array_tree));
          if (TREE_CODE(inner_type) != TREE_CODE(TREE_TYPE(array_tree))) {
            fprintf(stderr, "Error setting array type!\n");
          }
        } else {
          fprintf(stderr, "Do something with pointer type!\n");
        }
      } else {
        error("fxopt: Error setting array element type");
      }

    } else if (MEM_REF == TREE_CODE(operand)) {
      TREE_TYPE(operand) = inner_type;
      // find the declaration of the underlying variable
      var = TREE_OPERAND(operand, 0);
      if (SSA_NAME == TREE_CODE(var)) {
        TREE_TYPE(var) = inner_type;
        var = SSA_NAME_VAR(var);        // var is now a _DECL
      }
      if ((VAR_DECL != TREE_CODE(var)) && (PARM_DECL != TREE_CODE(var))) {
        error("fxopt: Error getting mem_ref operand VAR_DECL/PARM_DECL");
      }
    } else {
      warning(0, G_("Unexpected operand code, operand %d"), op_number);
    }                           // finding underlying declared variable

    //
    // Try to fetch a format for this exact operand, pass, and index.
    //
    struct SIF *op_fmt_p = NULL;
    uid = DECL_UID(var);        // only variable declarations have a UID
    int var_key = UID_PASS_IDX_TO_KEY(uid, fxpass, index);
    HASH_FIND_INT(var_formats, &var_key, op_fmt_p);

    //
    // If the operand is on the RHS and is aliased then we need to get
    // the format of the alias instead.
    //
    // Try to fetch a format for this operand with index of NOT_AN_ARRAY.
    // If this operand is aliased, fetch the format for the aliased variable
    //   using the index for the current operand (needed for ARRAY_REF)
    //
    if (op_number > 0) {
      struct SIF *var_fmt_p = NULL;
      var_key = UID_PASS_IDX_TO_KEY(uid, fxpass, NOT_AN_ARRAY);
      HASH_FIND_INT(var_formats, &var_key, var_fmt_p);
      if ((var_fmt_p != NULL) && (var_fmt_p->ptr_op) && (var_fmt_p->alias)) {
        int alias_uid = KEY_TO_UID(var_fmt_p->alias);
        pass = fxpass;
        do {
          var_key = UID_PASS_IDX_TO_KEY(alias_uid, pass, index);
          HASH_FIND_INT(var_formats, &var_key, var_fmt_p);
          if (var_fmt_p != NULL) {
            op_fmt_p = var_fmt_p;
          }
          pass--;
        } while ((pass >= 0) && (op_fmt_p == NULL));
      }
    }
    //
    // Function parameters get updated every pass
    // They should only have themselves in their aa list, but the pass #
    //   needs to be updated so that the aa list points to the current
    //   SIF format
    //
    if ((op_fmt_p == NULL) && (fxpass > 0) && (PARM_DECL == TREE_CODE(var))) {
      op_fmt_p = find_var_format(UID_PASS_IDX_TO_KEY(uid, fxpass, index));
      struct SIF *prev_fmt_p =
          find_var_format(UID_PASS_IDX_TO_KEY(uid, (fxpass - 1), index));
      copy_SIF(prev_fmt_p, op_fmt_p);
      if (op_fmt_p->aa) {
        if (op_fmt_p->aa->id == UID_PASS_IDX_TO_KEY(uid, (fxpass - 1), index))
          op_fmt_p->aa->id = op_fmt_p->id;
        else if (op_fmt_p->aa->next->id ==
                 UID_PASS_IDX_TO_KEY(uid, (fxpass - 1), index))
          op_fmt_p->aa->next->id = op_fmt_p->id;
        else
          fprintf(stderr, "  !!!!! Unexpected id\n");
      }
    }
    //
    // Else, try to find the operand in an earlier pass
    //
    if ((op_fmt_p == NULL) && (fxpass > 0)) {
      pass = fxpass;
      do {
        pass--;
        var_key = UID_PASS_IDX_TO_KEY(uid, pass, index);
        HASH_FIND_INT(var_formats, &var_key, op_fmt_p);
        if (op_fmt_p != NULL) {
        }
      } while ((pass >= 0) && (op_fmt_p == NULL));
    }
    //
    // Else, try to find the operand without an index
    //
    if ((op_fmt_p == NULL) && (index > 0)) {
      var_key = UID_PASS_IDX_TO_KEY(uid, fxpass, NOT_AN_ARRAY);
      HASH_FIND_INT(var_formats, &var_key, op_fmt_p);
      if ((op_fmt_p == NULL) && (fxpass > 0)) {
        pass = fxpass;
        do {
          pass--;
          var_key = UID_PASS_IDX_TO_KEY(uid, pass, index);
          HASH_FIND_INT(var_formats, &var_key, op_fmt_p);
          if (op_fmt_p != NULL) {
          }
        } while ((pass >= 0) && (op_fmt_p == NULL));
      } else {
      }
    }

    if (op_fmt_p != NULL) {
      delete_aa_list(&(op_fmt.aa));
      op_fmt = *op_fmt_p;
      //
      // If a pointer on the LHS still doesn't have an initialized format,
      // try to use the operand in this pass without an index
      //
      if (!format_initialized(op_fmt) && (op_fmt.ptr_op != 0) &&
                                         (op_number == 0)) {
        var_key = UID_PASS_IDX_TO_KEY(uid, fxpass, NOT_AN_ARRAY);
        if (var_key != op_fmt.id) {
          HASH_FIND_INT(var_formats, &var_key, op_fmt_p);
          if (op_fmt_p != NULL) {
            op_fmt = *op_fmt_p;
            if (op_fmt_p->alias != 0) {
              op_fmt.alias = UID_PASS_IDX_TO_KEY(KEY_TO_UID(op_fmt_p->alias),
                                               fxpass, index);
            }
          }
        }
      }
    }
  }                             // not a constant


  if (format_initialized(op_fmt)) {
    // marker for uninitialized: min > max
    if (double_int_scmp(op_fmt.max, op_fmt.min) == -1) {
      op_fmt.max = double_int_mask(op_fmt.I + op_fmt.F);
      op_fmt.min = double_int_neg(op_fmt.max);
      op_fmt.max =
          double_int_lshift(op_fmt.max, op_fmt.E,
                            HOST_BITS_PER_DOUBLE_INT, ARITH);
      op_fmt.min =
          double_int_lshift(op_fmt.min, op_fmt.E,
                            HOST_BITS_PER_DOUBLE_INT, ARITH);
    }
    //
    // Turn off interval arithmetic (for experimental purposes)
    //
    if (!INTERVAL) {
      op_fmt.max = double_int_mask(op_fmt.I + op_fmt.F + op_fmt.E);
      op_fmt.min = double_int_neg(op_fmt.max);
    }
    //
    // Using affine range instead of interval arithmetic
    //
    if ((AFFINE) && (NULL != op_fmt.aa)) {
      op_fmt.max = aa_max(op_fmt.aa);
      op_fmt.min = aa_min(op_fmt.aa);
    }

    op_fmt.originalF = op_fmt.F;
    //
    // If desired, print the operand's format
    //
    if (print) {
      if (op_fmt.alias != 0) {
        fprintf(stderr, "  OP%d  @", op_number);
      } else if (op_fmt.ptr_op != 0) {
        fprintf(stderr, "  OP%d  *", op_number);
      } else {
        fprintf(stderr, "  OP%d   ", op_number);
      }
      if (op_fmt.iv)
        fprintf(stderr, "(%2d/%2d/--/--)", op_fmt.S, op_fmt.I);
      else
        fprintf(stderr, "(%2d/%2d/%2d/%2d)", op_fmt.S, op_fmt.I, op_fmt.F,
                op_fmt.E);
      if (op_fmt.sgnd) {
        fprintf(stderr, "s ");
      } else {
        fprintf(stderr, "u ");
      }
      if (AFFINE) {
        print_aa_list(op_fmt.aa);
      }
      if (INTERVAL)
        print_min_max(op_fmt);
      else
        fprintf(stderr, "\n");
    }
  }                             // found some kind of format to use

  return op_fmt;
}


/**
 * @brief Stores the calculated ::SIF format for an operand.
 *
 * @details Given the tree for a statement LHS operand, set the format of the
 * underlying declared variable.
 *
 * Only the SIF-related parameters can be changed. These are the ::SIF structure
 * elements for
 *  - S
 *  - I
 *  - F
 *  - E
 *  - min
 *  - max
 *  - iv
 *  - aa
 * The other ::SIF parameters (e.g. size, signed) are inherited from the
 * underlying variable
 *
 * @param[in] operand the operand's gcc tree
 * @param[in] result_frmt  ::SIF struct
 * @return    int     error flag, nonzero if errors found
 */
int set_var_format(tree operand, struct SIF result_frmt)
{
  if (result_frmt.F < 0) {
    fatal_error("fxopt: result has negative # of fraction bits");
  }
  if (result_frmt.I < 0) {
    fatal_error("fxopt: result has negative # of integer bits");
  }
  tree var = NULL;
  struct SIF *var_fmt;
  int index = NOT_AN_ARRAY;
  //
  // Need to find the VAR declaration for the declared variable that
  // underlies this operand
  //
  if (SSA_NAME == TREE_CODE(operand)) {
    var = SSA_NAME_VAR(operand);
  } else if ((PARM_DECL == TREE_CODE(operand))
             || (VAR_DECL == TREE_CODE(operand))) {
    var = operand;
  } else if (ARRAY_REF == TREE_CODE(operand)) {
    var = TREE_OPERAND(operand, 0);
    tree index_var = TREE_OPERAND(operand, 1);
    if (INTEGER_CST == TREE_CODE(index_var)) {
      index = TREE_INT_CST_LOW(index_var);
    } else if (SSA_NAME == TREE_CODE(index_var)) {
      index_var = SSA_NAME_VAR(index_var);
      int uid = DECL_UID(index_var);
      struct SIF *idx_fmt =
          get_format_ptr(UID_PASS_IDX_TO_KEY(uid, 0, NOT_AN_ARRAY));
      index = idx_fmt->shift;
      fprintf(stderr, "  Using index %d\n", index);
      idx_fmt->shift = idx_fmt->shift + 1;
      if (idx_fmt->shift > (int) double_int_to_uhwi(idx_fmt->max))
        idx_fmt->shift = 0;
    }
    if (MEM_REF == TREE_CODE(var)) {
      var = TREE_OPERAND(var, 0);
      if (SSA_NAME == TREE_CODE(var)) {
        var = SSA_NAME_VAR(var);
      } else {
        error("fxopt: set_var_format: Unexpected operand of a MEM_REF");
      }
    } else {
      if (INTEGER_CST != TREE_CODE(TREE_OPERAND(operand, 1))) {
        error("fxopt: set_var_format: Error getting array index");
      }
    }
  } else if (MEM_REF == TREE_CODE(operand)) {
    var = TREE_OPERAND(operand, 0);
    if (SSA_NAME == TREE_CODE(var)) {
      var = SSA_NAME_VAR(var);
    }
  } else {
    error("fxopt: Unexpected operand code encountered");
  }
  if ((VAR_DECL != TREE_CODE(var)) && (PARM_DECL != TREE_CODE(var))) {
    error("fxopt: Error setting operand VAR_DECL/PARM_DECL");
  }
  //
  // Get a pointer to the hash table entry for this var. Note that this
  // is a pointer to the actual variable under operand 0, not to an alias
  //
  if (result_frmt.iv)
    var_fmt = get_format_ptr(calc_hash_key(var, 0, NOT_AN_ARRAY));
  else
    var_fmt = get_format_ptr(calc_hash_key(var, fxpass, index));
  if (NULL == var_fmt) {
    add_var_format(calc_hash_key(var, fxpass, index));
    var_fmt = get_format_ptr(calc_hash_key(var, fxpass, index));
    struct SIF *prior_var_fmt;
    prior_var_fmt = get_format_ptr(calc_hash_key(var, (fxpass - 1), index));
    if (NULL != prior_var_fmt)
      copy_format(prior_var_fmt, var_fmt);
  }
  //
  // If the operand is aliased, adjust the alias for the index of the LHS
  //
  if ((result_frmt.alias != 0) && (result_frmt.alias != var_fmt->id)) {
    result_frmt.alias = UID_PASS_IDX_TO_KEY(KEY_TO_UID(result_frmt.alias),
                                            fxpass, index);
  }
  //
  // Do some error checking
  //
  int errors = 0;
  if (var_fmt->size < result_frmt.size)
    error("fxopt: new format has more bits than variable");
  if ((PARM_DECL == TREE_CODE(var)) &&
      ((var_fmt->S != result_frmt.S) || (var_fmt->I != result_frmt.I))) {
    warning(0, G_("Changing format of a function parameter"));
    errors = 1;
  }
  //
  // If the variable is a pointer or is used iteratively, check to see if the
  // new format is different or the new range is wider. If so, issue a warning.
  // If the variable is used iteratively then we also need to re-evaluate all
  // of the assignments that used the variable, so return an error flag.
  //
  if (format_initialized(*var_fmt) && (var_fmt->ptr_op || var_fmt->iter)) {
    if ((var_fmt->S != result_frmt.S) || (var_fmt->I != result_frmt.I)) {
      //warning(0, G_("Changing format of a pointer/iterative target"));
      errors = var_fmt->iter;
    }
    if (INTERVAL) {
      if (range_compare(result_frmt, *var_fmt) == 1) {
        warning(0, G_("Expanding range of a pointer/iterative target"));
        errors = var_fmt->iter;
      }
      result_frmt.max = range_max(result_frmt, *var_fmt);
      result_frmt.min = range_min(result_frmt, *var_fmt);
    }
  }
  //
  // Store the desired format info in the hash table
  //
  copy_SIF(&result_frmt, var_fmt);
  var_fmt->shift = 0;
  var_fmt->alias = result_frmt.alias;

  return errors;
}

/**
 * @brief Update the F and E values for a shifted constant.
 * @details As a constant is shifted it is possible that new empty bits will
 * appear. Determine the correct number of F and E bits that would result from
 * a specified shift, <em>without</em> actually shifting the constant itself.
 *
 * @param[in,out] oprnd_frmt operand ::SIF structure array
 * @param[in] oprnd_tree operand tree array
 * @param[in] opnumber   number of operand to be evaluated
 */
void fix_f_e_bits(struct SIF oprnd_frmt[], tree oprnd_tree[], int opnumber)
{
  REAL_VALUE_TYPE real_const;
  struct SIF tmp_fmt;
  initialize_format(&tmp_fmt);

  double_int constant;

  if (TREE_CONSTANT(oprnd_tree[opnumber])) {
    // get the full constant value, unshifted
    if (REAL_CST == TREE_CODE(oprnd_tree[opnumber])) {
      real_const = TREE_REAL_CST(oprnd_tree[opnumber]);
      tmp_fmt = get_format_real_value_type(real_const); // maximum precision
      constant = tmp_fmt.max;
    } else if (INTEGER_CST == TREE_CODE(oprnd_tree[opnumber])) {
      constant = TREE_INT_CST(oprnd_tree[opnumber]);
    } else {
      warning(0, G_("fxopt: Unexpected constant type, fix_f_e_bits"));
    }

    if (tree_int_cst_sgn(oprnd_tree[opnumber]) < 0)
      constant = double_int_neg(constant);
    // right shift with rounding
    if (oprnd_frmt[opnumber].shift > 0) {
      constant =
          double_int_rshift(constant, (oprnd_frmt[opnumber].shift - 1),
                            HOST_BITS_PER_DOUBLE_INT, ARITH);
      constant = double_int_add(constant, double_int_one);
      constant =
          double_int_rshift(constant, 1, HOST_BITS_PER_DOUBLE_INT, ARITH);
    }
    // left shift (rshift with negative count is left shift)
    if (oprnd_frmt[opnumber].shift < 0) {
      constant =
          double_int_rshift(constant, (oprnd_frmt[opnumber].shift),
                            HOST_BITS_PER_DOUBLE_INT, ARITH);
    }
    // combine F and E
    oprnd_frmt[opnumber].F += oprnd_frmt[opnumber].E;
    // determine correct E
    oprnd_frmt[opnumber].E =
        MIN(double_int_ctz(constant), oprnd_frmt[opnumber].F);
    // determine correct F
    oprnd_frmt[opnumber].F -= oprnd_frmt[opnumber].E;
    oprnd_frmt[opnumber].originalF = oprnd_frmt[opnumber].F;
  }
  delete_aa_list(&(tmp_fmt.aa));
}

/**
 * @brief Perform a right shift on an SIF format.
 * 
 * @details Adds sign bits on the left and discards fraction and empty bits
 * Don't actually shift the operand, just modify the format to what it would be
 * after the shift and keep track of the cumulative shifts If the operand is a
 * constant, determine the true number of empty bits in the shifted and rounded
 * value.
 * 
 * @param[in] oprnd_frmt operand ::SIF structure array
 * @param[in] oprnd_tree operand tree array
 * @param[in] opnumber   operand number to be shifted
 * @param[in] count      number of positions to shift
 */
void shift_right(struct SIF oprnd_frmt[], tree oprnd_tree[], int opnumber,
                 int count)
{
  if (count == 0)
    return;
  if (count < 0)
    warning(0, "fxopt: Negative right shift");

  oprnd_frmt[opnumber].shift += count;
  oprnd_frmt[opnumber].S += count;

  if (count <= oprnd_frmt[opnumber].E) {
    oprnd_frmt[opnumber].E -= count;
  } else {
    oprnd_frmt[opnumber].F += oprnd_frmt[opnumber].E;
    oprnd_frmt[opnumber].F -= count;
    oprnd_frmt[opnumber].E = 0;
  }
  fix_f_e_bits(oprnd_frmt, oprnd_tree, opnumber);
}

/**
 * @brief Shift an SIF format left.
 * 
 * @details Discards sign bits. Keeps track of whether a bit "shifted in" at
 * the LSB is an empty bit or a valid fraction bit. This can be important if we
 * shift something right and then shift it back left.
 * 
 * @param[in] oprnd_frmt operand ::SIF structure array
 * @param[in] oprnd_tree operand tree array
 * @param[in] opnumber   operand number to be shifted
 * @param[in] count      number of positions to shift
 */
void shift_left(struct SIF oprnd_frmt[], tree oprnd_tree[], int opnumber,
                int count)
{
  if (count < 0)
    warning(0, "fxopt: Negative left shift");

  oprnd_frmt[opnumber].shift -= count;
  oprnd_frmt[opnumber].S -= count;
  if ((oprnd_frmt[opnumber].originalF - oprnd_frmt[opnumber].F) > count) {
    oprnd_frmt[opnumber].F += count;
  } else {
    // next two statements must be in this order
    oprnd_frmt[opnumber].E +=
        count - (oprnd_frmt[opnumber].originalF - oprnd_frmt[opnumber].F);
    oprnd_frmt[opnumber].F = oprnd_frmt[opnumber].originalF;
  }
  fix_f_e_bits(oprnd_frmt, oprnd_tree, opnumber);
}

/**
 * @brief Verify that a desired operand shift is valid.
 * 
 * @details Generate an error message if the shift would result in the loss of
 * integer bits, or if the sign information would be lost.
 * 
 * @param[in] oprnd_frmt operand's ::SIF structure
 */
void check_shift(struct SIF oprnd_frmt)
{
  if (!format_initialized(oprnd_frmt))
    return;
  if (oprnd_frmt.F < 0) {
    error("fxopt: invalid right shift, lost I bits");
  }
  if (oprnd_frmt.I < 0) {
    error("fxopt: invalid shift, negative I bits");
  }
  if (oprnd_frmt.sgnd) {
    if (oprnd_frmt.S < 1) {
      error("fxopt: invalid left shift, signed operand");
    }
  } else if (oprnd_frmt.S < 0) {
    error("fxopt: invalid left shift, unsigned operand");
  }
}

/**
 * @brief Predicate for an initialized format specification.
 * @details If the format has been initialized then either I or F or both
 * must be non-zero.
 * 
 * @param[in] oprnd_frmt the ::SIF structure to check
 * @return    true if the format has been initialized
 */
int format_initialized(struct SIF oprnd_frmt)
{
  return (oprnd_frmt.I || oprnd_frmt.F);
}

/**
 * @brief Calculate log2 of a power-of-2 integer constant tree.
 *
 * @details Modified version of tree_log2() from tree.c, but works for
 * positive and negative constants. If int_const equals 2^k or -(2^k),
 * return k else return -1.
 *
 * @param[in] int_const pointer to tree of the integer constant
 * @return    log base 2 of absolute value of constant
 */
int abs_tree_log2(tree int_const)
{
  int prec;
  HOST_WIDE_INT high, low;

  STRIP_NOPS(int_const);

  prec = TYPE_PRECISION(TREE_TYPE(int_const));
  high = TREE_INT_CST_HIGH(int_const);
  low = TREE_INT_CST_LOW(int_const);

  if (tree_int_cst_sign_bit(int_const)) {
    high = -high;
    low = -low;
  }

  // First clear all bits that are beyond the type's precision in case we've
  // been sign extended.
  if (prec == 2 * HOST_BITS_PER_WIDE_INT);
  else if (prec > HOST_BITS_PER_WIDE_INT)
    high &= ~((HOST_WIDE_INT) (-1) << (prec - HOST_BITS_PER_WIDE_INT));
  else {
    high = 0;
    if (prec < HOST_BITS_PER_WIDE_INT)
      low &= ~((HOST_WIDE_INT) (-1) << prec);
  }

  return (high != 0 ? HOST_BITS_PER_WIDE_INT + exact_log2(high)
          : exact_log2(low));
}

/**
 * @brief Calculate the floor of the log2 of an integer constant tree.
 * @details Takes the absolute value of the integer constant, then calculates
 * the base 2 log of its value. Returns the floor of the log as an integer.
 *
 * @param[in] int_const integer constant tree 
 * @return              integer
 */
int abs_tree_floor_log2(tree int_const)
{
  int prec;
  HOST_WIDE_INT high, low;

  STRIP_NOPS(int_const);

  prec = TYPE_PRECISION(TREE_TYPE(int_const));
  high = TREE_INT_CST_HIGH(int_const);
  low = TREE_INT_CST_LOW(int_const);

  if (tree_int_cst_sign_bit(int_const)) {
    high = -high;
    low = -low;
  }

  // First clear all bits that are beyond the type's precision in case we've
  // been sign extended.

  if (prec == 2 * HOST_BITS_PER_WIDE_INT);
  else if (prec > HOST_BITS_PER_WIDE_INT)
    high &= ~((HOST_WIDE_INT) (-1) << (prec - HOST_BITS_PER_WIDE_INT));
  else {
    high = 0;
    if (prec < HOST_BITS_PER_WIDE_INT)
      low &= ~((HOST_WIDE_INT) (-1) << prec);
  }

  return (high != 0 ? HOST_BITS_PER_WIDE_INT + floor_log2(high)
          : floor_log2(low));
}

/**
 * @brief Apply any fxfrmt attribute to the statement LHS.
 *
 * @details If the format of the result (operand 0) was assigned by a fxfrmt
 * attribute, saturate the current result to the desired number of integer
 * bits.
 * 
 * @param[in] gsi_p pointer to gimple_stmt_iterator
 * @param[in] oprnd_frmt ::SIF structure array for all operands
 * @param[in] oprnd_tree tree array for all operands
 * @param[in] result_frmt calculated ::SIF structure for result
 * @param[in] result_var_p pointer to result variable's tree 
 * @return    ::SIF structure for result format, after applying fxfrmt
 */
struct SIF apply_fxfrmt(gimple_stmt_iterator * gsi_p,
                        struct SIF oprnd_frmt[], tree oprnd_tree[],
                        struct SIF result_frmt, tree * result_var_p)
{
  struct SIF new_frmt = result_frmt;
  int temp;
  gimple new_stmt;

  if (oprnd_frmt[0].has_attribute) {

    fprintf(stderr, "  Unsaturated RESULT is (%2d/%2d/%2d/%2d) shft %2d",
            result_frmt.S, result_frmt.I, result_frmt.F, result_frmt.E,
            result_frmt.shift);
    temp = result_frmt.shift;
    result_frmt.shift = 0;
    print_min_max(result_frmt);
    result_frmt.shift = temp;

    if (oprnd_frmt[0].I < result_frmt.I) {
      //
      // Saturation converts I bits to S bits. Adding these redundant bits
      //   decreases the size of the right shift that may be needed to convert
      //   a double-precision result back to single precision with S=1
      //
      new_frmt.S = result_frmt.S + (result_frmt.I - oprnd_frmt[0].I);
      new_frmt.shift = result_frmt.shift - (result_frmt.I - oprnd_frmt[0].I);

      new_frmt.shift = MAX(new_frmt.shift, 0);
      new_frmt.I = oprnd_frmt[0].I;

      // the desired number of info bits in the saturated value
      int sat_bits = oprnd_frmt[0].I + result_frmt.F + result_frmt.E;

      //
      // sat_value is the largest positive value that will fit in the
      //   final format, after being shifted to single precision
      // When updating the min/max values, force the empty bits to
      //   zeros in the maximum value
      //
      double_int sat_value = double_int_mask(sat_bits);
      if (new_frmt.shift != 0)
        sat_value =
            double_int_and_not(sat_value, double_int_mask(new_frmt.shift));
      if (new_frmt.E > 0)
        sat_value = double_int_and_not(sat_value, double_int_mask(new_frmt.E));
      new_frmt.max = double_int_smin(new_frmt.max, sat_value);
      new_frmt.min =
          double_int_smax(new_frmt.min,
                          double_int_sext(double_int_neg(new_frmt.max),
                                          PRECISION(new_frmt)));

      if (lastpass) {
        tree max_pos =
            double_int_to_tree(TREE_TYPE(*result_var_p), new_frmt.max);
        tree maxpos_var =
            make_rename_temp(TREE_TYPE(*result_var_p), "_fx_maxpos0");
        new_stmt =
            gimple_build_assign_with_ops(INTEGER_CST, maxpos_var, max_pos,
                                         NULL);
        print_gimple_stmt(stderr, new_stmt, 2, 0);
        //gimple_set_visited(new_stmt, true);
        gsi_insert_after(gsi_p, new_stmt, GSI_NEW_STMT);

        // only saturate positive if value can be positive
        if (double_int_positive_p(new_frmt.max)) {
          tree satpos_var =
              make_rename_temp(TREE_TYPE(*result_var_p), "_fx_satpos0");
          new_stmt =
              gimple_build_assign_with_ops(MIN_EXPR, satpos_var,
                                           *result_var_p, maxpos_var);
          print_gimple_stmt(stderr, new_stmt, 2, 0);
          //gimple_set_visited(new_stmt, true);
          gsi_insert_after(gsi_p, new_stmt, GSI_NEW_STMT);
          *result_var_p = satpos_var;
        }
        // only saturate negative if value can be negative
        if (double_int_negative_p(new_frmt.min)) {
          tree minneg_var =
              make_rename_temp(TREE_TYPE(*result_var_p), "_fx_minneg0");
          new_stmt =
              gimple_build_assign_with_ops(NEGATE_EXPR, minneg_var,
                                           maxpos_var, NULL);
          print_gimple_stmt(stderr, new_stmt, 2, 0);
          //gimple_set_visited(new_stmt, true);
          gsi_insert_after(gsi_p, new_stmt, GSI_NEW_STMT);

          tree sat_var = make_rename_temp(TREE_TYPE(*result_var_p), "_fx_sat0");
          new_stmt =
              gimple_build_assign_with_ops(MAX_EXPR, sat_var, *result_var_p,
                                           minneg_var);
          print_gimple_stmt(stderr, new_stmt, 2, 0);
          //gimple_set_visited(new_stmt, true);
          gsi_insert_after(gsi_p, new_stmt, GSI_NEW_STMT);
          *result_var_p = sat_var;
        }
      }                         // if lastpass
      //
      // Print out the final result format
      //
      fprintf(stderr, "    Saturated RESULT is (%2d/%2d/%2d/%2d) shft %2d",
              new_frmt.S, new_frmt.I, new_frmt.F, new_frmt.E, new_frmt.shift);
      temp = new_frmt.shift;
      new_frmt.shift = 0;
      print_min_max(new_frmt);
      new_frmt.shift = temp;
    }                           // result needs more I bits than oprnd0 has
  }                             // oprnd 0 has an attribute
  //
  // If the original LHS is a pointer with a fxfrmt attribute then
  //   we can't change the binary point location, so we need to make sure
  //   that both S and I have the same values as the attribute
  //
  // shift left to discard extra
  // sign bits so that both S and I are the same
  //
  if (oprnd_frmt[0].ptr_op != 0) {
    int shift_size = 0;
    if (oprnd_frmt[0].I > result_frmt.I) {
      //
      // Convert S bits to fake I bits if possible
      //
      int bits2convert = MIN((oprnd_frmt[0].S - 1),
                             (oprnd_frmt[0].I - result_frmt.I));
      if (bits2convert > 0) {
        fprintf(stderr, "    Converting %d S bits to I bits\n", bits2convert);
        new_frmt.S = result_frmt.S - bits2convert;
        new_frmt.I = result_frmt.I + bits2convert;
      }
      //
      // Do a right shift, adding I bits instead of S bits
      //
      shift_size = oprnd_frmt[0].I - result_frmt.I - bits2convert;
      fprintf(stderr, "    shift_size %d to equalize I bits\n", shift_size);
      new_frmt.I += shift_size;
    }
    if (oprnd_frmt[0].S != new_frmt.S) {
      shift_size += oprnd_frmt[0].S - new_frmt.S;
      fprintf(stderr, "    shift_size %d to equalize S bits\n", shift_size);
    }
    new_frmt.shift += shift_size;
    if (new_frmt.E > shift_size) {
      new_frmt.E -= shift_size;
    } else {
      new_frmt.F -= (shift_size - new_frmt.E);
      new_frmt.E = 0;
    }
    new_frmt.S = oprnd_frmt[0].S;
    //
    // Print out the final result format
    //
    fprintf(stderr, "   Saturated pointer is (%2d/%2d/%2d/%2d) shft %2d",
            new_frmt.S, new_frmt.I, new_frmt.F, new_frmt.E, new_frmt.shift);
    temp = new_frmt.shift;
    new_frmt.shift = shift_size;
    print_min_max(new_frmt);
    new_frmt.shift = temp;
  }
  if (new_frmt.shift != 0) {
    // 
    // Do the required shift
    // 
    if (lastpass) {
      tree shifted_var = make_rename_temp(TREE_TYPE(*result_var_p),
                                          "_fx_shft0");
      if (new_frmt.shift > 0) {
        tree shift_constant =
            build_int_cst(integer_type_node, new_frmt.shift);
        new_stmt =
            gimple_build_assign_with_ops(RSHIFT_EXPR, shifted_var,
                                         *result_var_p, shift_constant);
      } else {
        tree shift_constant =
            build_int_cst(integer_type_node, -new_frmt.shift);
        new_stmt =
            gimple_build_assign_with_ops(LSHIFT_EXPR, shifted_var,
                                         *result_var_p, shift_constant);
      }
      //gimple_set_visited(new_stmt, true);
      gsi_insert_after(gsi_p, new_stmt, GSI_NEW_STMT);
      print_gimple_stmt(stderr, new_stmt, 2, 0);
      *result_var_p = shifted_var;
    }                         // lastpass
  }                           // need to shift after saturation
  return new_frmt;
}

// vim:syntax=c.doxygen
