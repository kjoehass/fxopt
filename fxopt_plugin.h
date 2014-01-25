#ifndef _FXOPT_PLUGIN_H
#  define _FXOPT_PLUGIN_H

#  include "plugin.h"
#  include "gcc-plugin.h"
#  include <stdlib.h>
#  include "config.h"
#  include "system.h"
#  include "coretypes.h"
#  include "tree.h"
#  include "tree-pass.h"
#  include "tree-flow.h"
#  include "intl.h"
#  include "math.h"
#  include "uthash.h"
#  include "stdint.h"

extern int lastpass;
extern int fxpass;

extern int INTERVAL;
extern int AFFINE;
extern int GUARDING;
extern int ROUNDING;
extern int POSITIVE;
extern int DBL_PRECISION_MULTS;
extern int CONST_DIV_TO_MULT;

#  define REAL_TO_INTEGER_TYPE  long_integer_type_node
//#define REAL_TO_INTEGER_TYPE  short_integer_type_node

// for double_int_lshift and double_int_rshift
#  define ARITH   true
#  define LOGICAL false
// for get_operand_format()
#  define PRINT   true
#  define NOPRINT false
// for aa_assign()
#  define ADD     true
#  define SUB     false

// These should be powers of 2, their product <= 2^16
#  define MAX_PASSES  256     // passes through a BB require to resolve all
#  define MAX_ELEMENTS  256   // elements in an array

#  define NOT_AN_ARRAY (MAX_ELEMENTS - 1) // marks a non-array variable

#  define UID_TO_KEY(x) ((x) * MAX_ELEMENTS * MAX_PASSES)
#  define UID_PASS_TO_KEY(x,y) ((x) * MAX_ELEMENTS * MAX_PASSES + (y))
#  define UID_IDX_TO_KEY(x,y) ((((x) * MAX_ELEMENTS) + (y)) * MAX_PASSES)
#  define UID_PASS_IDX_TO_KEY(x,y,z) (((((x) * MAX_PASSES) + (z)) * MAX_ELEMENTS) + (y))
#  define KEY_TO_UID(x)  ((x) / ( MAX_ELEMENTS * MAX_PASSES ))
#  define KEY_TO_PASS(x)  ((x) % MAX_PASSES )
#  define KEY_TO_IDX(x)  (((x) / MAX_PASSES ) % MAX_ELEMENTS )

#  define PRECISION(x) ((x).I+(x).F+(x).E+(x).sgnd)
#  define INFO_BITS(x) (oprnd_frmt[(x)].I+oprnd_frmt[(x)].F)
#  define BINARY_PT(x) (oprnd_frmt[(x)].F+oprnd_frmt[(x)].E)
#  define LOST_F_BITS(x) (oprnd_frmt[(x)].originalF-oprnd_frmt[(x)].F)

/**
  @brief The structure for a term in an affine definition.
  @details The affine definition of a variable consists of an expression that
    sums some number of <em>terms</em>. Each term is the product of a
    coefficient and a variable. The affine definitions for variables in fxopt
    are implemented as a doubly-linked-list of this AA structure, and each
    list element represents one term in the affine expression.
  
  The <tt>id</tt> value identifies the variable that will be multiplied by
  the coefficient in each term. It is generated from the GIMPLE variable UID
  for the variable, concatenated with the array index (for array elements), and
  the pass number. There are a few special values for the <tt>id</tt>, see
  the documentation of the SIF structure for details.

  The coefficient for the term is stored as a gcc <tt>double_int</tt>. In order
  to determine the corresponding real value we need to know the location of
  the binary point. The binary point location is specified by the <tt>bp</tt>
  variable, which indicates how many bit positions there are to the right of
  the binary point. Therefore, the real value of the coefficient is
  \f[ coeff \times 2^{-bp} \f]

 */ 
struct AA {
  int id;              ///< Hash key of variable
  double_int coeff;    ///< Coefficient for this variable
  uint32_t bp;         ///< Coefficient binary point location, 0 = true integer
  struct AA *next;     ///< Pointer to next variable structure
  struct AA *prev;     ///< Pointer to previous variable structure
};

/**
  @brief The structure for the SIF definition of a variable.
  @details
  
  The <tt>id</tt> value is generated from the GIMPLE variable UID for the
  underlying variable (ignoring the version number for an SSA name, if
  applicable), concatenated with the array index (for array elements), and
  the pass number. There are a few special values for the <tt>id</tt>:
    - A UID value of zero indicates that the variable is an error term that
      was created as part of an affine estimate of the result of a non-affine
      arithmetic operation. The remaining bits in the id are just a unique
      integer value.
    - An array index value of all one bits (e.g. <tt>0xFF</tt>) signifies
      that the variable is not a member of an array.
    - A pass number of zero indicates a variable definition created before
      processing any GIMPLE statements.
    - An <tt>id</tt> value of exactly <tt>0xdead</tt> marks a definition that
      that has been created but holds no valid information.
    - An <tt>id</tt> value of exactly <tt>0xbad</tt> marks a definition that
      that has been deleted and its memory freed. The implication is that any
      pointers to this element are actually invalid and dangling.

  If <tt>min</tt> is greater than <tt>max</tt> then the range is undefined.
 */ 
struct SIF {
  int id;             //!< Hash key
  int alias;          /*!< If this variable is an alias of another
                          variable, such as a pointer that takes its value from
                          another pointer then the id of the other variable is
                          stored here. If this variable is not an alias, this
                          field is 0 */
  int S;              ///< number of sign bits in variable
  int I;              ///< number of integer bits in variable
  int F;              ///< number of fraction bits in variable
  int E;              ///< number of empty bits to the right of F bits
  int originalF;
  int size;           ///< size of this variable, in bits
  int shift;          /*!< \# bits operand should be shifted right; a negative
                           value indicates a left shift. */
  int ptr_op;         /*!< If a pointer/address operand, the SIZE in bytes of
                           the original data pointed to. */
  int sgnd;           ///< 1 if signed, 0 if unsigned
  int iv;             ///< True if an induction variable.
  int iter;           ///< True if variable used iteratively.
  double_int max;     ///< Maximum possible value
  double_int min;     ///< Minimum possible value
  struct AA *aa;      ///< Pointer to affine variable list
  int has_attribute;  ///< boolean
  int attrS;          ///< S value assigned by an fxfrmt attribute, if any.
  int attrI;          ///< I value assigned by an fxfrmt attribute, if any.
  int attrF;          ///< F value assigned by an fxfrmt attribute, if any.
  int attrE;          ///< E value assigned by an fxfrmt attribute, if any.
  double_int attrmax; ///< Range max value assigned by fxfrmt attribute, if any.
  double_int attrmin; ///< Range min value assigned by fxfrmt attribute, if any.
  UT_hash_handle hh;  ///< Required by uthash functions
};

/* from gimple-pretty-print.c */
void print_gimple_stmt (FILE *, gimple, int, int);

/* from diagnostic-core.h */
extern void fatal_error (const char *, ...);
extern void error (const char *, ...);
extern bool warning (int, const char *, ...);

/* from fxopt_affine.c */
void append_aa_var(struct AA **aa_list_p, int var_key, double_int coeff, int bp);
struct AA *search_aa_var(struct AA *aa_list_p, int var_key);
void print_aa_element(struct AA *aa_elt_p);
void print_aa_list(struct AA *aa_list_p);
void delete_aa_list(struct AA **aa_list_pp);
void fix_aa_bp(struct SIF op_fmt);
double_int new_aa_max(struct SIF op_fmt);
double_int new_aa_min(struct SIF op_fmt);
double_int aa_max(struct AA *aa_list_p);
double_int aa_min(struct AA *aa_list_p);
struct AA *shift_aa_list(struct SIF op_fmt, int k);
struct AA *new_aa_list(struct SIF op_fmt);
struct AA *copy_aa_list(struct AA *aa_src_list_p);
struct AA *affine_assign(struct AA *aa_src_list_p, bool add);
struct AA *affine_add(struct SIF op_fmt[], bool add);
struct AA *affine_multiply(struct AA *op1, struct AA *op2);
struct AA *affine_square(struct AA *op1);
struct AA *affine_divide(struct AA *numerator, struct AA *denominator);

/* from fxopt_range.c */
bool double_int_positive_p(double_int dblint);
int ceil_log2_format(struct SIF op_fmt);
int ceil_log2_range(struct SIF op_fmt);
int log2_range(struct SIF op_fmt);
int pessimistic_format(struct SIF op_fmt);
void check_range(struct SIF op_fmt);
void print_min_max(struct SIF op_fmt);
void print_double(double_int val, int precision);
int range_compare(struct SIF fmt1, struct SIF fmt2);
int rounding_may_overflow(struct SIF op_fmt);
int max_is_mnn(struct SIF op_fmt);
double_int double_int_abs(double_int dblint);
double_int new_max(struct SIF op_fmt);
double_int new_min(struct SIF op_fmt);
double_int range_max(struct SIF fmt1, struct SIF fmt2);
double_int range_min(struct SIF fmt1, struct SIF fmt2);
struct SIF new_range(struct SIF op_fmt);
struct SIF new_range_add(struct SIF oprnd_frmt[], struct SIF result_frmt);
struct SIF new_range_sub(struct SIF oprnd_frmt[], struct SIF result_frmt);
struct SIF new_range_mul(struct SIF oprnd_frmt[], struct SIF result_frmt);
struct SIF new_range_div(struct SIF oprnd_frmt[], struct SIF result_frmt);
struct SIF int_const_to_range(tree int_const, struct SIF result_frmt);

/* from fxopt_utils.c */
int calc_hash_key(tree var_tree, int version, int index);
void add_var_format(int var_id);
struct SIF *get_format_ptr(int var_id);
struct SIF *find_var_format(int var_id);
void copy_format(struct SIF *src, struct SIF *dest);
void copy_SIF(struct SIF *src, struct SIF *dest);
void delete_var_format(struct SIF *var_format);
void delete_all_formats();
void force_ptr_consistency();
void initialize_format(struct SIF *op_fmt);
float real_max(struct SIF *op_fmt);
float real_min(struct SIF *op_fmt);
void print_var_formats();
void print_one_format(struct SIF *op_fmt);
void print_format(struct SIF op_fmt);
void restore_attributes();
void real_expr_to_integer(gimple stmt);
void convert_real_var_to_integer(tree var);
void convert_real_func_to_integer(tree decl);
void invert_constant_operand(gimple stmt, int opnumber);
tree convert_real_constant(tree real_cst, struct SIF *op_fmt);
void int_constant_format(tree integer_cst, struct SIF *op_fmt);
int set_var_format(tree operand, struct SIF op_fmt);
void shift_right(struct SIF oprnd_frmt[], tree oprnd_tree[], int opnumber,
                 int count);
void shift_left(struct SIF oprnd_frmt[], tree oprnd_tree[], int opnumber,
                int count);
int format_initialized(struct SIF oprnd_frmt);
void check_shift(struct SIF oprnd_frmt);
int abs_tree_log2(tree int_const);
int abs_tree_floor_log2(tree int_const);
tree get_innermost_type(tree vardecl);
struct SIF get_inverted_operand_format(gimple stmt, int op_number);
tree get_operand_decl(tree operand);
struct SIF get_operand_format(gimple stmt, int op_number,
                              int element_number, bool print);
struct SIF apply_fxfrmt(gimple_stmt_iterator * gsi_p,
                        struct SIF oprnd_frmt[], tree oprnd_tree[],
                        struct SIF result_frmt, tree * result_var_p);


/* from fxopt_stmts.c */
struct SIF nop(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
               tree oprnd_tree[]);
struct SIF array_ref(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
               tree oprnd_tree[]);
struct SIF pointer_math(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
                        tree oprnd_tree[]);
struct SIF addition(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
                    tree oprnd_tree[]);
struct SIF multiplication(gimple_stmt_iterator * gsi_p,
                          struct SIF oprnd_frmt[], tree oprnd_tree[]);
struct SIF division(gimple_stmt_iterator * gsi_p, struct SIF oprnd_frmt[],
                    tree oprnd_tree[]);

#endif
