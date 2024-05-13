/*-------------------------------------------------------------------------
 *
 * numeric.c
 *	  An exact numeric data type for the Postgres database system
 *
 * Original coding 1998, Jan Wieck.  Heavily revised 2003, Tom Lane.
 *
 * Many of the algorithmic ideas are borrowed from David M. Smith's "FM"
 * multiple-precision math library, most recently published as Algorithm
 * 786: Multiple-Precision Complex Arithmetic and Functions, ACM
 * Transactions on Mathematical Software, Vol. 24, No. 4, December 1998,
 * pages 359-367.
 *
 * Copyright (c) 1998-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/numeric.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "catalog/pg_type.h"
#include "common/int.h"
#include "funcapi.h"
#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/guc.h"
#include "utils/hashutils.h"
#include "utils/int8.h"
#include "utils/numeric.h"
#include "utils/sortsupport.h"

/* ----------
 * Uncomment the following to enable compilation of dump_numeric()
 * and dump_var() and to get a dump of any result produced by make_result().
 * ----------
#define NUMERIC_DEBUG
 */

/* ----------
 * Local data types
 *
 * Numeric values are represented in a base-NBASE floating point format.
 * Each "digit" ranges from 0 to NBASE-1.  The type NumericDigit is signed
 * and wide enough to store a digit.  We assume that NBASE*NBASE can fit in
 * an int.  Although the purely calculational routines could handle any even
 * NBASE that's less than sqrt(INT_MAX), in practice we are only interested
 * in NBASE a power of ten, so that I/O conversions and decimal rounding
 * are easy.  Also, it's actually more efficient if NBASE is rather less than
 * sqrt(INT_MAX), so that there is "headroom" for mul_var and div_var_fast to
 * postpone processing carries.
 *
 * Values of NBASE other than 10000 are considered of historical interest only
 * and are no longer supported in any sense; no mechanism exists for the client
 * to discover the base, so every client supporting binary mode expects the
 * base-10000 format.  If you plan to change this, also note the numeric
 * abbreviation code, which assumes NBASE=10000.
 * ----------
 */

#if 0
#define NBASE 10
#define HALF_NBASE 5
#define DEC_DIGITS 1       /* decimal digits per NBASE digit */
#define MUL_GUARD_DIGITS 4 /* these are measured in NBASE digits */
#define DIV_GUARD_DIGITS 8

typedef signed char NumericDigit;
#endif

#if 0
#define NBASE 100
#define HALF_NBASE 50
#define DEC_DIGITS 2       /* decimal digits per NBASE digit */
#define MUL_GUARD_DIGITS 3 /* these are measured in NBASE digits */
#define DIV_GUARD_DIGITS 6

typedef signed char NumericDigit;
#endif

#if 1
#define NBASE 10000
#define HALF_NBASE 5000
#define DEC_DIGITS 4       /* decimal digits per NBASE digit */
#define MUL_GUARD_DIGITS 2 /* these are measured in NBASE digits */
#define DIV_GUARD_DIGITS 4

typedef int16 NumericDigit;
#endif

/*
 * The Numeric type as stored on disk.
 *
 * If the high bits of the first word of a NumericChoice (n_header, or
 * n_short.n_header, or n_long.n_sign_dscale) are NUMERIC_SHORT, then the
 * numeric follows the NumericShort format; if they are NUMERIC_POS or
 * NUMERIC_NEG, it follows the NumericLong format.  If they are NUMERIC_NAN,
 * it is a NaN.  We currently always store a NaN using just two bytes (i.e.
 * only n_header), but previous releases used only the NumericLong format,
 * so we might find 4-byte NaNs on disk if a database has been migrated using
 * pg_upgrade.  In either case, when the high bits indicate a NaN, the
 * remaining bits are never examined.  Currently, we always initialize these
 * to zero, but it might be possible to use them for some other purpose in
 * the future.
 *
 * In the NumericShort format, the remaining 14 bits of the header word
 * (n_short.n_header) are allocated as follows: 1 for sign (positive or
 * negative), 6 for dynamic scale, and 7 for weight.  In practice, most
 * commonly-encountered values can be represented this way.
 *
 * In the NumericLong format, the remaining 14 bits of the header word
 * (n_long.n_sign_dscale) represent the display scale; and the weight is
 * stored separately in n_weight.
 *
 * NOTE: by convention, values in the packed form have been stripped of
 * all leading and trailing zero digits (where a "digit" is of base NBASE).
 * In particular, if the value is zero, there will be no digits at all!
 * The weight is arbitrary in that case, but we normally set it to zero.
 */

struct NumericShort
{
  uint16 n_header;                            /* Sign + display scale + weight */
  NumericDigit n_data[FLEXIBLE_ARRAY_MEMBER]; /* Digits */
};

struct NumericLong
{
  uint16 n_sign_dscale;                       /* Sign + display scale */
  int16 n_weight;                             /* Weight of 1st digit	*/
  NumericDigit n_data[FLEXIBLE_ARRAY_MEMBER]; /* Digits */
};

union NumericChoice
{
  uint16 n_header;             /* Header word */
  struct NumericLong n_long;   /* Long form (4-byte header) */
  struct NumericShort n_short; /* Short form (2-byte header) */
};

struct NumericData
{
  int32 vl_len_;              /* varlena header (do not touch directly!) */
  union NumericChoice choice; /* choice of format */
};

/*
 * Interpretation of high bits.
 */

#define NUMERIC_SIGN_MASK 0xC000
#define NUMERIC_POS 0x0000
#define NUMERIC_NEG 0x4000
#define NUMERIC_SHORT 0x8000
#define NUMERIC_NAN 0xC000

#define NUMERIC_FLAGBITS(n) ((n)->choice.n_header & NUMERIC_SIGN_MASK)
#define NUMERIC_IS_NAN(n) (NUMERIC_FLAGBITS(n) == NUMERIC_NAN)
#define NUMERIC_IS_SHORT(n) (NUMERIC_FLAGBITS(n) == NUMERIC_SHORT)

#define NUMERIC_HDRSZ (VARHDRSZ + sizeof(uint16) + sizeof(int16))
#define NUMERIC_HDRSZ_SHORT (VARHDRSZ + sizeof(uint16))

/*
 * If the flag bits are NUMERIC_SHORT or NUMERIC_NAN, we want the short header;
 * otherwise, we want the long one.  Instead of testing against each value, we
 * can just look at the high bit, for a slight efficiency gain.
 */
#define NUMERIC_HEADER_IS_SHORT(n) (((n)->choice.n_header & 0x8000) != 0)
#define NUMERIC_HEADER_SIZE(n) (VARHDRSZ + sizeof(uint16) + (NUMERIC_HEADER_IS_SHORT(n) ? 0 : sizeof(int16)))

/*
 * Short format definitions.
 */

#define NUMERIC_SHORT_SIGN_MASK 0x2000
#define NUMERIC_SHORT_DSCALE_MASK 0x1F80
#define NUMERIC_SHORT_DSCALE_SHIFT 7
#define NUMERIC_SHORT_DSCALE_MAX (NUMERIC_SHORT_DSCALE_MASK >> NUMERIC_SHORT_DSCALE_SHIFT)
#define NUMERIC_SHORT_WEIGHT_SIGN_MASK 0x0040
#define NUMERIC_SHORT_WEIGHT_MASK 0x003F
#define NUMERIC_SHORT_WEIGHT_MAX NUMERIC_SHORT_WEIGHT_MASK
#define NUMERIC_SHORT_WEIGHT_MIN (-(NUMERIC_SHORT_WEIGHT_MASK + 1))

/*
 * Extract sign, display scale, weight.
 */

#define NUMERIC_DSCALE_MASK 0x3FFF
#define NUMERIC_DSCALE_MAX NUMERIC_DSCALE_MASK

#define NUMERIC_SIGN(n) (NUMERIC_IS_SHORT(n) ? (((n)->choice.n_short.n_header & NUMERIC_SHORT_SIGN_MASK) ? NUMERIC_NEG : NUMERIC_POS) : NUMERIC_FLAGBITS(n))
#define NUMERIC_DSCALE(n) (NUMERIC_HEADER_IS_SHORT((n)) ? ((n)->choice.n_short.n_header & NUMERIC_SHORT_DSCALE_MASK) >> NUMERIC_SHORT_DSCALE_SHIFT : ((n)->choice.n_long.n_sign_dscale & NUMERIC_DSCALE_MASK))
#define NUMERIC_WEIGHT(n) (NUMERIC_HEADER_IS_SHORT((n)) ? (((n)->choice.n_short.n_header & NUMERIC_SHORT_WEIGHT_SIGN_MASK ? ~NUMERIC_SHORT_WEIGHT_MASK : 0) | ((n)->choice.n_short.n_header & NUMERIC_SHORT_WEIGHT_MASK)) : ((n)->choice.n_long.n_weight))

/* ----------
 * NumericVar is the format we use for arithmetic.  The digit-array part
 * is the same as the NumericData storage format, but the header is more
 * complex.
 *
 * The value represented by a NumericVar is determined by the sign, weight,
 * ndigits, and digits[] array.
 *
 * Note: the first digit of a NumericVar's value is assumed to be multiplied
 * by NBASE ** weight.  Another way to say it is that there are weight+1
 * digits before the decimal point.  It is possible to have weight < 0.
 *
 * buf points at the physical start of the palloc'd digit buffer for the
 * NumericVar.  digits points at the first digit in actual use (the one
 * with the specified weight).  We normally leave an unused digit or two
 * (preset to zeroes) between buf and digits, so that there is room to store
 * a carry out of the top digit without reallocating space.  We just need to
 * decrement digits (and increment weight) to make room for the carry digit.
 * (There is no such extra space in a numeric value stored in the database,
 * only in a NumericVar in memory.)
 *
 * If buf is NULL then the digit buffer isn't actually palloc'd and should
 * not be freed --- see the constants below for an example.
 *
 * dscale, or display scale, is the nominal precision expressed as number
 * of digits after the decimal point (it must always be >= 0 at present).
 * dscale may be more than the number of physically stored fractional digits,
 * implying that we have suppressed storage of significant trailing zeroes.
 * It should never be less than the number of stored digits, since that would
 * imply hiding digits that are present.  NOTE that dscale is always expressed
 * in *decimal* digits, and so it may correspond to a fractional number of
 * base-NBASE digits --- divide by DEC_DIGITS to convert to NBASE digits.
 *
 * rscale, or result scale, is the target precision for a computation.
 * Like dscale it is expressed as number of *decimal* digits after the decimal
 * point, and is always >= 0 at present.
 * Note that rscale is not stored in variables --- it's figured on-the-fly
 * from the dscales of the inputs.
 *
 * While we consistently use "weight" to refer to the base-NBASE weight of
 * a numeric value, it is convenient in some scale-related calculations to
 * make use of the base-10 weight (ie, the approximate log10 of the value).
 * To avoid confusion, such a decimal-units weight is called a "dweight".
 *
 * NB: All the variable-level functions are written in a style that makes it
 * possible to give one and the same variable as argument and destination.
 * This is feasible because the digit buffer is separate from the variable.
 * ----------
 */
typedef struct NumericVar
{
  int ndigits;          /* # of digits in digits[] - can be 0! */
  int weight;           /* weight of first digit */
  int sign;             /* NUMERIC_POS, NUMERIC_NEG, or NUMERIC_NAN */
  int dscale;           /* display scale */
  NumericDigit *buf;    /* start of palloc'd space for digits[] */
  NumericDigit *digits; /* base-NBASE digits */
} NumericVar;

/* ----------
 * Data for generate_series
 * ----------
 */
typedef struct
{
  NumericVar current;
  NumericVar stop;
  NumericVar step;
} generate_series_numeric_fctx;

/* ----------
 * Sort support.
 * ----------
 */
typedef struct
{
  void *buf;         /* buffer for short varlenas */
  int64 input_count; /* number of non-null values seen */
  bool estimating;   /* true if estimating cardinality */

  hyperLogLogState abbr_card; /* cardinality estimator */
} NumericSortSupport;

/* ----------
 * Fast sum accumulator.
 *
 * NumericSumAccum is used to implement SUM(), and other standard aggregates
 * that track the sum of input values.  It uses 32-bit integers to store the
 * digits, instead of the normal 16-bit integers (with NBASE=10000).  This
 * way, we can safely accumulate up to NBASE - 1 values without propagating
 * carry, before risking overflow of any of the digits.  'num_uncarried'
 * tracks how many values have been accumulated without propagating carry.
 *
 * Positive and negative values are accumulated separately, in 'pos_digits'
 * and 'neg_digits'.  This is simpler and faster than deciding whether to add
 * or subtract from the current value, for each new value (see sub_var() for
 * the logic we avoid by doing this).  Both buffers are of same size, and
 * have the same weight and scale.  In accum_sum_final(), the positive and
 * negative sums are added together to produce the final result.
 *
 * When a new value has a larger ndigits or weight than the accumulator
 * currently does, the accumulator is enlarged to accommodate the new value.
 * We normally have one zero digit reserved for carry propagation, and that
 * is indicated by the 'have_carry_space' flag.  When accum_sum_carry() uses
 * up the reserved digit, it clears the 'have_carry_space' flag.  The next
 * call to accum_sum_add() will enlarge the buffer, to make room for the
 * extra digit, and set the flag again.
 *
 * To initialize a new accumulator, simply reset all fields to zeros.
 *
 * The accumulator does not handle NaNs.
 * ----------
 */
typedef struct NumericSumAccum
{
  int ndigits;
  int weight;
  int dscale;
  int num_uncarried;
  bool have_carry_space;
  int32 *pos_digits;
  int32 *neg_digits;
} NumericSumAccum;

/*
 * We define our own macros for packing and unpacking abbreviated-key
 * representations for numeric values in order to avoid depending on
 * USE_FLOAT8_BYVAL.  The type of abbreviation we use is based only on
 * the size of a datum, not the argument-passing convention for float8.
 */
#define NUMERIC_ABBREV_BITS (SIZEOF_DATUM * BITS_PER_BYTE)
#if SIZEOF_DATUM == 8
#define NumericAbbrevGetDatum(X) ((Datum)(X))
#define DatumGetNumericAbbrev(X) ((int64)(X))
#define NUMERIC_ABBREV_NAN NumericAbbrevGetDatum(PG_INT64_MIN)
#else
#define NumericAbbrevGetDatum(X) ((Datum)(X))
#define DatumGetNumericAbbrev(X) ((int32)(X))
#define NUMERIC_ABBREV_NAN NumericAbbrevGetDatum(PG_INT32_MIN)
#endif

/* ----------
 * Some preinitialized constants
 * ----------
 */
static const NumericDigit const_zero_data[1] = {0};
static const NumericVar const_zero = {0, 0, NUMERIC_POS, 0, NULL, (NumericDigit *)const_zero_data};

static const NumericDigit const_one_data[1] = {1};
static const NumericVar const_one = {1, 0, NUMERIC_POS, 0, NULL, (NumericDigit *)const_one_data};

static const NumericDigit const_two_data[1] = {2};
static const NumericVar const_two = {1, 0, NUMERIC_POS, 0, NULL, (NumericDigit *)const_two_data};

#if DEC_DIGITS == 4
static const NumericDigit const_zero_point_five_data[1] = {5000};
#elif DEC_DIGITS == 2
static const NumericDigit const_zero_point_five_data[1] = {50};
#elif DEC_DIGITS == 1
static const NumericDigit const_zero_point_five_data[1] = {5};
#endif
static const NumericVar const_zero_point_five = {1, -1, NUMERIC_POS, 1, NULL, (NumericDigit *)const_zero_point_five_data};

#if DEC_DIGITS == 4
static const NumericDigit const_zero_point_nine_data[1] = {9000};
#elif DEC_DIGITS == 2
static const NumericDigit const_zero_point_nine_data[1] = {90};
#elif DEC_DIGITS == 1
static const NumericDigit const_zero_point_nine_data[1] = {9};
#endif
static const NumericVar const_zero_point_nine = {1, -1, NUMERIC_POS, 1, NULL, (NumericDigit *)const_zero_point_nine_data};

#if DEC_DIGITS == 4
static const NumericDigit const_one_point_one_data[2] = {1, 1000};
#elif DEC_DIGITS == 2
static const NumericDigit const_one_point_one_data[2] = {1, 10};
#elif DEC_DIGITS == 1
static const NumericDigit const_one_point_one_data[2] = {1, 1};
#endif
static const NumericVar const_one_point_one = {2, 0, NUMERIC_POS, 1, NULL, (NumericDigit *)const_one_point_one_data};

static const NumericVar const_nan = {0, 0, NUMERIC_NAN, 0, NULL, NULL};

#if DEC_DIGITS == 4
static const int round_powers[4] = {0, 1000, 100, 10};
#endif

/* ----------
 * Local functions
 * ----------
 */

#ifdef NUMERIC_DEBUG
static void
dump_numeric(const char *str, Numeric num);
static void
dump_var(const char *str, NumericVar *var);
#else
#define dump_numeric(s, n)
#define dump_var(s, v)
#endif

#define digitbuf_alloc(ndigits) ((NumericDigit *)palloc((ndigits) * sizeof(NumericDigit)))
#define digitbuf_free(buf)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((buf) != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      pfree(buf);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define init_var(v) MemSetAligned(v, 0, sizeof(NumericVar))

#define NUMERIC_DIGITS(num) (NUMERIC_HEADER_IS_SHORT(num) ? (num)->choice.n_short.n_data : (num)->choice.n_long.n_data)
#define NUMERIC_NDIGITS(num) ((VARSIZE(num) - NUMERIC_HEADER_SIZE(num)) / sizeof(NumericDigit))
#define NUMERIC_CAN_BE_SHORT(scale, weight) ((scale) <= NUMERIC_SHORT_DSCALE_MAX && (weight) <= NUMERIC_SHORT_WEIGHT_MAX && (weight) >= NUMERIC_SHORT_WEIGHT_MIN)

static void
alloc_var(NumericVar *var, int ndigits);
static void
free_var(NumericVar *var);
static void
zero_var(NumericVar *var);

static const char *
set_var_from_str(const char *str, const char *cp, NumericVar *dest);
static void
set_var_from_num(Numeric value, NumericVar *dest);
static void
init_var_from_num(Numeric num, NumericVar *dest);
static void
set_var_from_var(const NumericVar *value, NumericVar *dest);
static char *
get_str_from_var(const NumericVar *var);
static char *
get_str_from_var_sci(const NumericVar *var, int rscale);

static Numeric
make_result(const NumericVar *var);
static Numeric
make_result_opt_error(const NumericVar *var, bool *error);

static void
apply_typmod(NumericVar *var, int32 typmod);

static bool
numericvar_to_int32(const NumericVar *var, int32 *result);
static bool
numericvar_to_int64(const NumericVar *var, int64 *result);
static void
int64_to_numericvar(int64 val, NumericVar *var);
#ifdef HAVE_INT128
static bool
numericvar_to_int128(const NumericVar *var, int128 *result);
static void
int128_to_numericvar(int128 val, NumericVar *var);
#endif
static double
numeric_to_double_no_overflow(Numeric num);
static double
numericvar_to_double_no_overflow(const NumericVar *var);

static Datum
numeric_abbrev_convert(Datum original_datum, SortSupport ssup);
static bool
numeric_abbrev_abort(int memtupcount, SortSupport ssup);
static int
numeric_fast_cmp(Datum x, Datum y, SortSupport ssup);
static int
numeric_cmp_abbrev(Datum x, Datum y, SortSupport ssup);

static Datum
numeric_abbrev_convert_var(const NumericVar *var, NumericSortSupport *nss);

static int
cmp_numerics(Numeric num1, Numeric num2);
static int
cmp_var(const NumericVar *var1, const NumericVar *var2);
static int
cmp_var_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, int var1sign, const NumericDigit *var2digits, int var2ndigits, int var2weight, int var2sign);
static void
add_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
sub_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
mul_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale);
static void
div_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round);
static void
div_var_fast(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round);
static int
select_div_scale(const NumericVar *var1, const NumericVar *var2);
static void
mod_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
ceil_var(const NumericVar *var, NumericVar *result);
static void
floor_var(const NumericVar *var, NumericVar *result);

static void
sqrt_var(const NumericVar *arg, NumericVar *result, int rscale);
static void
exp_var(const NumericVar *arg, NumericVar *result, int rscale);
static int
estimate_ln_dweight(const NumericVar *var);
static void
ln_var(const NumericVar *arg, NumericVar *result, int rscale);
static void
log_var(const NumericVar *base, const NumericVar *num, NumericVar *result);
static void
power_var(const NumericVar *base, const NumericVar *exp, NumericVar *result);
static void
power_var_int(const NumericVar *base, int exp, NumericVar *result, int rscale);
static void
power_ten_int(int exp, NumericVar *result);

static int
cmp_abs(const NumericVar *var1, const NumericVar *var2);
static int
cmp_abs_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, const NumericDigit *var2digits, int var2ndigits, int var2weight);
static void
add_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
sub_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
round_var(NumericVar *var, int rscale);
static void
trunc_var(NumericVar *var, int rscale);
static void
strip_var(NumericVar *var);
static void
compute_bucket(Numeric operand, Numeric bound1, Numeric bound2, const NumericVar *count_var, NumericVar *result_var);

static void
accum_sum_add(NumericSumAccum *accum, const NumericVar *var1);
static void
accum_sum_rescale(NumericSumAccum *accum, const NumericVar *val);
static void
accum_sum_carry(NumericSumAccum *accum);
static void
accum_sum_reset(NumericSumAccum *accum);
static void
accum_sum_final(NumericSumAccum *accum, NumericVar *result);
static void
accum_sum_copy(NumericSumAccum *dst, NumericSumAccum *src);
static void
accum_sum_combine(NumericSumAccum *accum, NumericSumAccum *accum2);

/* ----------------------------------------------------------------------
 *
 * Input-, output- and rounding-functions
 *
 * ----------------------------------------------------------------------
 */

/*
 * numeric_in() -
 *
 *	Input function for numeric data type
 */
Datum
numeric_in(PG_FUNCTION_ARGS)
{







































































}

/*
 * numeric_out() -
 *
 *	Output function for numeric data type
 */
Datum
numeric_out(PG_FUNCTION_ARGS)
{




















}

/*
 * numeric_is_nan() -
 *
 *	Is Numeric value a NaN?
 */
bool
numeric_is_nan(Numeric num)
{

}

/*
 * numeric_maximum_size() -
 *
 *	Maximum size of a numeric with given typmod, or -1 if unlimited/unknown.
 */
int32
numeric_maximum_size(int32 typmod)
{































}

/*
 * numeric_out_sci() -
 *
 *	Output function for numeric data type in scientific notation.
 */
char *
numeric_out_sci(Numeric num, int scale)
{
















}

/*
 * numeric_normalize() -
 *
 *	Output function for numeric data type, suppressing insignificant
 *trailing zeroes and then any trailing decimal point.  The intent of this is to
 *	produce strings that are equal if and only if the input numeric values
 *	compare equal.
 */
char *
numeric_normalize(Numeric num)
{








































}

/*
 *		numeric_recv			- converts external binary
 *format to numeric
 *
 * External format is a sequence of int16's:
 * ndigits, weight, sign, dscale, NumericDigits.
 */
Datum
numeric_recv(PG_FUNCTION_ARGS)
{
























































}

/*
 *		numeric_send			- converts numeric to binary
 *format
 */
Datum
numeric_send(PG_FUNCTION_ARGS)
{



















}

/*
 * numeric_support()
 *
 * Planner support function for the numeric() length coercion function.
 *
 * Flatten calls that solely represent increases in allowable precision.
 * Scale changes mutate every datum, so they are unoptimizable.  Some values,
 * e.g. 1E-1001, can only fit into an unconstrained numeric, so a change from
 * an unconstrained numeric to any constrained numeric is also unoptimizable.
 */
Datum
numeric_support(PG_FUNCTION_ARGS)
{






































}

/*
 * numeric() -
 *
 *	This is a special function called by the Postgres database system
 *	before a value is stored in a tuple's attribute. The precision and
 *	scale of the attribute have to be applied on the value.
 */
Datum
numeric(PG_FUNCTION_ARGS)
{









































































}

Datum
numerictypmodin(PG_FUNCTION_ARGS)
{



































}

Datum
numerictypmodout(PG_FUNCTION_ARGS)
{













}

/* ----------------------------------------------------------------------
 *
 * Sign manipulation, rounding and the like
 *
 * ----------------------------------------------------------------------
 */

Datum
numeric_abs(PG_FUNCTION_ARGS)
{



























}

Datum
numeric_uminus(PG_FUNCTION_ARGS)
{








































}

Datum
numeric_uplus(PG_FUNCTION_ARGS)
{







}

/*
 * numeric_sign() -
 *
 * returns -1 if the argument is less than 0, 0 if the argument is equal
 * to 0, and 1 if the argument is greater than zero.
 */
Datum
numeric_sign(PG_FUNCTION_ARGS)
{




































}

/*
 * numeric_round() -
 *
 *	Round a value to have 'scale' digits after the decimal point.
 *	We allow negative 'scale', implying rounding before the decimal
 *	point --- Oracle interprets rounding that way.
 */
Datum
numeric_round(PG_FUNCTION_ARGS)
{








































}

/*
 * numeric_trunc() -
 *
 *	Truncate a value to have 'scale' digits after the decimal point.
 *	We allow negative 'scale', implying a truncation before the decimal
 *	point --- Oracle interprets truncation that way.
 */
Datum
numeric_trunc(PG_FUNCTION_ARGS)
{








































}

/*
 * numeric_ceil() -
 *
 *	Return the smallest integer greater than or equal to the argument
 */
Datum
numeric_ceil(PG_FUNCTION_ARGS)
{
















}

/*
 * numeric_floor() -
 *
 *	Return the largest integer equal to or less than the argument
 */
Datum
numeric_floor(PG_FUNCTION_ARGS)
{
















}

/*
 * generate_series_numeric() -
 *
 *	Generate series of numeric.
 */
Datum
generate_series_numeric(PG_FUNCTION_ARGS)
{

}

Datum
generate_series_step_numeric(PG_FUNCTION_ARGS)
{
































































































}

/*
 * Implements the numeric version of the width_bucket() function
 * defined by SQL2003. See also width_bucket_float8().
 *
 * 'bound1' and 'bound2' are the lower and upper bounds of the
 * histogram's range, respectively. 'count' is the number of buckets
 * in the histogram. width_bucket() returns an integer indicating the
 * bucket number that 'operand' belongs to in an equiwidth histogram
 * with the specified characteristics. An operand smaller than the
 * lower bound is assigned to bucket 0. An operand greater than the
 * upper bound is assigned to an additional bucket (with number
 * count+1). We don't allow "NaN" for any of the numeric arguments.
 */
Datum
width_bucket_numeric(PG_FUNCTION_ARGS)
{









































































}

/*
 * If 'operand' is not outside the bucket range, determine the correct
 * bucket for it to go. The calculations performed by this function
 * are derived directly from the SQL2003 spec.
 */
static void
compute_bucket(Numeric operand, Numeric bound1, Numeric bound2, const NumericVar *count_var, NumericVar *result_var)
{




























}

/* ----------------------------------------------------------------------
 *
 * Comparison functions
 *
 * Note: btree indexes need these routines not to leak memory; therefore,
 * be careful to free working copies of toasted datums.  Most places don't
 * need to be so careful.
 *
 * Sort support:
 *
 * We implement the sortsupport strategy routine in order to get the benefit of
 * abbreviation. The ordinary numeric comparison can be quite slow as a result
 * of palloc/pfree cycles (due to detoasting packed values for alignment);
 * while this could be worked on itself, the abbreviation strategy gives more
 * speedup in many common cases.
 *
 * Two different representations are used for the abbreviated form, one in
 * int32 and one in int64, whichever fits into a by-value Datum.  In both cases
 * the representation is negated relative to the original value, because we use
 * the largest negative value for NaN, which sorts higher than other values. We
 * convert the absolute value of the numeric to a 31-bit or 63-bit positive
 * value, and then negate it if the original number was positive.
 *
 * We abort the abbreviation process if the abbreviation cardinality is below
 * 0.01% of the row count (1 per 10k non-null rows).  The actual break-even
 * point is somewhat below that, perhaps 1 per 30k (at 1 per 100k there's a
 * very small penalty), but we don't want to build up too many abbreviated
 * values before first testing for abort, so we take the slightly pessimistic
 * number.  We make no attempt to estimate the cardinality of the real values,
 * since it plays no part in the cost model here (if the abbreviation is equal,
 * the cost of comparing equal and unequal underlying values is comparable).
 * We discontinue even checking for abort (saving us the hashing overhead) if
 * the estimated cardinality gets to 100k; that would be enough to support many
 * billions of rows while doing no worse than breaking even.
 *
 * ----------------------------------------------------------------------
 */

/*
 * Sort support strategy routine.
 */
Datum
numeric_sortsupport(PG_FUNCTION_ARGS)
{
































}

/*
 * Abbreviate a numeric datum, handling NaNs and detoasting
 * (must not leak memory!)
 */
static Datum
numeric_abbrev_convert(Datum original_datum, SortSupport ssup)
{
















































}

/*
 * Consider whether to abort abbreviation.
 *
 * We pay no attention to the cardinality of the non-abbreviated data. There is
 * no reason to do so: unlike text, we have no fast check for equal values, so
 * we pay the full overhead whenever the abbreviations are equal regardless of
 * whether the underlying values are also equal.
 */
static bool
numeric_abbrev_abort(int memtupcount, SortSupport ssup)
{
























































}

/*
 * Non-fmgr interface to the comparison routine to allow sortsupport to elide
 * the fmgr call.  The saving here is small given how slow numeric comparisons
 * are, but it is a required part of the sort support API when abbreviations
 * are performed.
 *
 * Two palloc/pfree cycles could be saved here by using persistent buffers for
 * aligning short-varlena inputs, but this has not so far been considered to
 * be worth the effort.
 */
static int
numeric_fast_cmp(Datum x, Datum y, SortSupport ssup)
{
















}

/*
 * Compare abbreviations of values. (Abbreviations may be equal where the true
 * values differ, but if the abbreviations differ, they must reflect the
 * ordering of the true values.)
 */
static int
numeric_cmp_abbrev(Datum x, Datum y, SortSupport ssup)
{













}

/*
 * Abbreviate a NumericVar according to the available bit size.
 *
 * The 31-bit value is constructed as:
 *
 *	0 + 7bits digit weight + 24 bits digit value
 *
 * where the digit weight is in single decimal digits, not digit words, and
 * stored in excess-44 representation[1]. The 24-bit digit value is the 7 most
 * significant decimal digits of the value converted to binary. Values whose
 * weights would fall outside the representable range are rounded off to zero
 * (which is also used to represent actual zeros) or to 0x7FFFFFFF (which
 * otherwise cannot occur). Abbreviation therefore fails to gain any advantage
 * where values are outside the range 10^-44 to 10^83, which is not considered
 * to be a serious limitation, or when values are of the same magnitude and
 * equal in the first 7 decimal digits, which is considered to be an
 * unavoidable limitation given the available bits. (Stealing three more bits
 * to compare another digit would narrow the range of representable weights by
 * a factor of 8, which starts to look like a real limiting factor.)
 *
 * (The value 44 for the excess is essentially arbitrary)
 *
 * The 63-bit value is constructed as:
 *
 *	0 + 7bits weight + 4 x 14-bit packed digit words
 *
 * The weight in this case is again stored in excess-44, but this time it is
 * the original weight in digit words (i.e. powers of 10000). The first four
 * digit words of the value (if present; trailing zeros are assumed as needed)
 * are packed into 14 bits each to form the rest of the value. Again,
 * out-of-range values are rounded off to 0 or 0x7FFFFFFFFFFFFFFF. The
 * representable range in this case is 10^-176 to 10^332, which is considered
 * to be good enough for all practical purposes, and comparison of 4 words
 * means that at least 13 decimal digits are compared, which is considered to
 * be a reasonable compromise between effectiveness and efficiency in computing
 * the abbreviation.
 *
 * (The value 44 for the excess is even more arbitrary here, it was chosen just
 * to match the value used in the 31-bit case)
 *
 * [1] - Excess-k representation means that the value is offset by adding 'k'
 * and then treated as unsigned, so the smallest representable value is stored
 * with all bits zero. This allows simple comparisons to work on the composite
 * value.
 */

#if NUMERIC_ABBREV_BITS == 64

static Datum
numeric_abbrev_convert_var(const NumericVar *var, NumericSortSupport *nss)
{















































}

#endif /* NUMERIC_ABBREV_BITS == 64 */

#if NUMERIC_ABBREV_BITS == 32

static Datum
numeric_abbrev_convert_var(const NumericVar *var, NumericSortSupport *nss)
{
  int ndigits = var->ndigits;
  int weight = var->weight;
  int32 result;

  if (ndigits == 0 || weight < -11)
  {
    result = 0;
  }
  else if (weight > 20)
  {
    result = PG_INT32_MAX;
  }
  else
  {
    NumericDigit nxt1 = (ndigits > 1) ? var->digits[1] : 0;

    weight = (weight + 11) * 4;

    result = var->digits[0];

    /*
     * "result" now has 1 to 4 nonzero decimal digits. We pack in more
     * digits to make 7 in total (largest we can fit in 24 bits)
     */

    if (result > 999)
    {
      /* already have 4 digits, add 3 more */
      result = (result * 1000) + (nxt1 / 10);
      weight += 3;
    }
    else if (result > 99)
    {
      /* already have 3 digits, add 4 more */
      result = (result * 10000) + nxt1;
      weight += 2;
    }
    else if (result > 9)
    {
      NumericDigit nxt2 = (ndigits > 2) ? var->digits[2] : 0;

      /* already have 2 digits, add 5 more */
      result = (result * 100000) + (nxt1 * 10) + (nxt2 / 1000);
      weight += 1;
    }
    else
    {
      NumericDigit nxt2 = (ndigits > 2) ? var->digits[2] : 0;

      /* already have 1 digit, add 6 more */
      result = (result * 1000000) + (nxt1 * 100) + (nxt2 / 100);
    }

    result = result | (weight << 24);
  }

  /* the abbrev is negated relative to the original */
  if (var->sign == NUMERIC_POS)
  {
    result = -result;
  }

  if (nss->estimating)
  {
    uint32 tmp = (uint32)result;

    addHyperLogLog(&nss->abbr_card, DatumGetUInt32(hash_uint32(tmp)));
  }

  return NumericAbbrevGetDatum(result);
}

#endif /* NUMERIC_ABBREV_BITS == 32 */

/*
 * Ordinary (non-sortsupport) comparisons follow.
 */

Datum
numeric_cmp(PG_FUNCTION_ARGS)
{










}

Datum
numeric_eq(PG_FUNCTION_ARGS)
{










}

Datum
numeric_ne(PG_FUNCTION_ARGS)
{










}

Datum
numeric_gt(PG_FUNCTION_ARGS)
{










}

Datum
numeric_ge(PG_FUNCTION_ARGS)
{










}

Datum
numeric_lt(PG_FUNCTION_ARGS)
{










}

Datum
numeric_le(PG_FUNCTION_ARGS)
{










}

static int
cmp_numerics(Numeric num1, Numeric num2)
{




























}

/*
 * in_range support function for numeric.
 */
Datum
in_range_numeric_numeric(PG_FUNCTION_ARGS)
{















































































}

Datum
hash_numeric(PG_FUNCTION_ARGS)
{














































































}

/*
 * Returns 64-bit value by hashing a value to a 64-bit value, with a seed.
 * Otherwise, similar to hash_numeric.
 */
Datum
hash_numeric_extended(PG_FUNCTION_ARGS)
{
























































}

/* ----------------------------------------------------------------------
 *
 * Basic arithmetic functions
 *
 * ----------------------------------------------------------------------
 */

/*
 * numeric_add() -
 *
 *	Add two numerics
 */
Datum
numeric_add(PG_FUNCTION_ARGS)
{







}

/*
 * numeric_add_opt_error() -
 *
 *	Internal version of numeric_add().  If "*have_error" flag is provided,
 *	on error it's set to true, NULL returned.  This is helpful when caller
 *	need to handle errors by itself.
 */
Numeric
numeric_add_opt_error(Numeric num1, Numeric num2, bool *have_error)
{



























}

/*
 * numeric_sub() -
 *
 *	Subtract one numeric from another
 */
Datum
numeric_sub(PG_FUNCTION_ARGS)
{







}

/*
 * numeric_sub_opt_error() -
 *
 *	Internal version of numeric_sub().  If "*have_error" flag is provided,
 *	on error it's set to true, NULL returned.  This is helpful when caller
 *	need to handle errors by itself.
 */
Numeric
numeric_sub_opt_error(Numeric num1, Numeric num2, bool *have_error)
{



























}

/*
 * numeric_mul() -
 *
 *	Calculate the product of two numerics
 */
Datum
numeric_mul(PG_FUNCTION_ARGS)
{







}

/*
 * numeric_mul_opt_error() -
 *
 *	Internal version of numeric_mul().  If "*have_error" flag is provided,
 *	on error it's set to true, NULL returned.  This is helpful when caller
 *	need to handle errors by itself.
 */
Numeric
numeric_mul_opt_error(Numeric num1, Numeric num2, bool *have_error)
{








































}

/*
 * numeric_div() -
 *
 *	Divide one numeric into another
 */
Datum
numeric_div(PG_FUNCTION_ARGS)
{







}

/*
 * numeric_div_opt_error() -
 *
 *	Internal version of numeric_div().  If "*have_error" flag is provided,
 *	on error it's set to true, NULL returned.  This is helpful when caller
 *	need to handle errors by itself.
 */
Numeric
numeric_div_opt_error(Numeric num1, Numeric num2, bool *have_error)
{














































}

/*
 * numeric_div_trunc() -
 *
 *	Divide one numeric into another, truncating the result to an integer
 */
Datum
numeric_div_trunc(PG_FUNCTION_ARGS)
{

































}

/*
 * numeric_mod() -
 *
 *	Calculate the modulo of two numerics
 */
Datum
numeric_mod(PG_FUNCTION_ARGS)
{







}

/*
 * numeric_mod_opt_error() -
 *
 *	Internal version of numeric_mod().  If "*have_error" flag is provided,
 *	on error it's set to true, NULL returned.  This is helpful when caller
 *	need to handle errors by itself.
 */
Numeric
numeric_mod_opt_error(Numeric num1, Numeric num2, bool *have_error)
{































}

/*
 * numeric_inc() -
 *
 *	Increment a number by one
 */
Datum
numeric_inc(PG_FUNCTION_ARGS)
{
























}

/*
 * numeric_smaller() -
 *
 *	Return the smaller of two numbers
 */
Datum
numeric_smaller(PG_FUNCTION_ARGS)
{















}

/*
 * numeric_larger() -
 *
 *	Return the larger of two numbers
 */
Datum
numeric_larger(PG_FUNCTION_ARGS)
{















}

/* ----------------------------------------------------------------------
 *
 * Advanced math functions
 *
 * ----------------------------------------------------------------------
 */

/*
 * numeric_fac()
 *
 * Compute factorial
 */
Datum
numeric_fac(PG_FUNCTION_ARGS)
{





































}

/*
 * numeric_sqrt() -
 *
 *	Compute the square root of a numeric.
 */
Datum
numeric_sqrt(PG_FUNCTION_ARGS)
{










































}

/*
 * numeric_exp() -
 *
 *	Raise e to the power of x
 */
Datum
numeric_exp(PG_FUNCTION_ARGS)
{




















































}

/*
 * numeric_ln() -
 *
 *	Compute the natural logarithm of x
 */
Datum
numeric_ln(PG_FUNCTION_ARGS)
{

































}

/*
 * numeric_log() -
 *
 *	Compute the logarithm of x in a given base
 */
Datum
numeric_log(PG_FUNCTION_ARGS)
{

































}

/*
 * numeric_power() -
 *
 *	Raise b to the power of x
 */
Datum
numeric_power(PG_FUNCTION_ARGS)
{






































































}

/*
 * numeric_scale() -
 *
 *	Returns the scale, i.e. the count of decimal digits in the fractional
 *part
 */
Datum
numeric_scale(PG_FUNCTION_ARGS)
{








}

/* ----------------------------------------------------------------------
 *
 * Type conversion functions
 *
 * ----------------------------------------------------------------------
 */

Datum
int4_numeric(PG_FUNCTION_ARGS)
{













}

int32
numeric_int4_opt_error(Numeric num, bool *have_error)
{


































}

Datum
numeric_int4(PG_FUNCTION_ARGS)
{



}

/*
 * Given a NumericVar, convert it to an int32. If the NumericVar
 * exceeds the range of an int32, false is returned, otherwise true is returned.
 * The input NumericVar is *not* free'd.
 */
static bool
numericvar_to_int32(const NumericVar *var, int32 *result)
{
















}

Datum
int8_numeric(PG_FUNCTION_ARGS)
{













}

Datum
numeric_int8(PG_FUNCTION_ARGS)
{



















}

Datum
int2_numeric(PG_FUNCTION_ARGS)
{













}

Datum
numeric_int2(PG_FUNCTION_ARGS)
{




























}

Datum
float8_numeric(PG_FUNCTION_ARGS)
{



























}

Datum
numeric_float8(PG_FUNCTION_ARGS)
{
















}

/*
 * Convert numeric to float8; if out of range, return +/- HUGE_VAL
 *
 * (internal helper function, not directly callable from SQL)
 */
Datum
numeric_float8_no_overflow(PG_FUNCTION_ARGS)
{











}

Datum
float4_numeric(PG_FUNCTION_ARGS)
{



























}

Datum
numeric_float4(PG_FUNCTION_ARGS)
{
















}

/* ----------------------------------------------------------------------
 *
 * Aggregate functions
 *
 * The transition datatype for all these aggregates is declared as INTERNAL.
 * Actually, it's a pointer to a NumericAggState allocated in the aggregate
 * context.  The digit buffers for the NumericVars will be there too.
 *
 * On platforms which support 128-bit integers some aggregates instead use a
 * 128-bit integer based transition datatype to speed up calculations.
 *
 * ----------------------------------------------------------------------
 */

typedef struct NumericAggState
{
  bool calcSumX2;            /* if true, calculate sumX2 */
  MemoryContext agg_context; /* context we're calculating in */
  int64 N;                   /* count of processed numbers */
  NumericSumAccum sumX;      /* sum of processed numbers */
  NumericSumAccum sumX2;     /* sum of squares of processed numbers */
  int maxScale;              /* maximum scale seen so far */
  int64 maxScaleCount;       /* number of values seen with maximum scale */
  int64 NaNcount;            /* count of NaN values (not included in N!) */
} NumericAggState;

/*
 * Prepare state data for a numeric aggregate function that needs to compute
 * sum, count and optionally sum of squares of the input.
 */
static NumericAggState *
makeNumericAggState(FunctionCallInfo fcinfo, bool calcSumX2)
{


















}

/*
 * Like makeNumericAggState(), but allocate the state in the current memory
 * context.
 */
static NumericAggState *
makeNumericAggStateCurrentContext(bool calcSumX2)
{







}

/*
 * Accumulate a new input value for numeric aggregate functions.
 */
static void
do_numeric_accum(NumericAggState *state, Numeric newval)
{

















































}

/*
 * Attempt to remove an input value from the aggregated state.
 *
 * If the value cannot be removed then the function will return false; the
 * possible reasons for failing are described below.
 *
 * If we aggregate the values 1.01 and 2 then the result will be 3.01.
 * If we are then asked to un-aggregate the 1.01 then we must fail as we
 * won't be able to tell what the new aggregated value's dscale should be.
 * We don't want to return 2.00 (dscale = 2), since the sum's dscale would
 * have been zero if we'd really aggregated only 2.
 *
 * Note: alternatively, we could count the number of inputs with each possible
 * dscale (up to some sane limit).  Not yet clear if it's worth the trouble.
 */
static bool
do_numeric_discard(NumericAggState *state, Numeric newval)
{


















































































}

/*
 * Generic transition function for numeric aggregates that require sumX2.
 */
Datum
numeric_accum(PG_FUNCTION_ARGS)
{
















}

/*
 * Generic combine function for numeric aggregates which require sumX2
 */
Datum
numeric_combine(PG_FUNCTION_ARGS)
{


































































}

/*
 * Generic transition function for numeric aggregates that don't require sumX2.
 */
Datum
numeric_avg_accum(PG_FUNCTION_ARGS)
{
















}

/*
 * Combine function for numeric aggregates which don't require sumX2
 */
Datum
numeric_avg_combine(PG_FUNCTION_ARGS)
{
































































}

/*
 * numeric_avg_serialize
 *		Serialize NumericAggState for numeric aggregates that don't
 *require sumX2.
 */
Datum
numeric_avg_serialize(PG_FUNCTION_ARGS)
{
















































}

/*
 * numeric_avg_deserialize
 *		Deserialize bytea into NumericAggState for numeric aggregates
 *that don't require sumX2.
 */
Datum
numeric_avg_deserialize(PG_FUNCTION_ARGS)
{











































}

/*
 * numeric_serialize
 *		Serialization function for NumericAggState for numeric
 *aggregates that require sumX2.
 */
Datum
numeric_serialize(PG_FUNCTION_ARGS)
{

























































}

/*
 * numeric_deserialize
 *		Deserialization function for NumericAggState for numeric
 *aggregates that require sumX2.
 */
Datum
numeric_deserialize(PG_FUNCTION_ARGS)
{

















































}

/*
 * Generic inverse transition function for numeric aggregates
 * (with or without requirement for X^2).
 */
Datum
numeric_accum_inv(PG_FUNCTION_ARGS)
{




















}

/*
 * Integer data types in general use Numeric accumulators to share code
 * and avoid risk of overflow.
 *
 * However for performance reasons optimized special-purpose accumulator
 * routines are used when possible.
 *
 * On platforms with 128-bit integer support, the 128-bit routines will be
 * used when sum(X) or sum(X*X) fit into 128-bit.
 *
 * For 16 and 32 bit inputs, the N and sum(X) fit into 64-bit so the 64-bit
 * accumulators will be used for SUM and AVG of these data types.
 */

#ifdef HAVE_INT128
typedef struct Int128AggState
{
  bool calcSumX2; /* if true, calculate sumX2 */
  int64 N;        /* count of processed numbers */
  int128 sumX;    /* sum of processed numbers */
  int128 sumX2;   /* sum of squares of processed numbers */
} Int128AggState;

/*
 * Prepare state data for a 128-bit aggregate function that needs to compute
 * sum, count and optionally sum of squares of the input.
 */
static Int128AggState *
makeInt128AggState(FunctionCallInfo fcinfo, bool calcSumX2)
{

















}

/*
 * Like makeInt128AggState(), but allocate the state in the current memory
 * context.
 */
static Int128AggState *
makeInt128AggStateCurrentContext(bool calcSumX2)
{






}

/*
 * Accumulate a new input value for 128-bit aggregate functions.
 */
static void
do_int128_accum(Int128AggState *state, int128 newval)
{







}

/*
 * Remove an input value from the aggregated state.
 */
static void
do_int128_discard(Int128AggState *state, int128 newval)
{







}

typedef Int128AggState PolyNumAggState;
#define makePolyNumAggState makeInt128AggState
#define makePolyNumAggStateCurrentContext makeInt128AggStateCurrentContext
#else
typedef NumericAggState PolyNumAggState;
#define makePolyNumAggState makeNumericAggState
#define makePolyNumAggStateCurrentContext makeNumericAggStateCurrentContext
#endif

Datum
int2_accum(PG_FUNCTION_ARGS)
{























}

Datum
int4_accum(PG_FUNCTION_ARGS)
{























}

Datum
int8_accum(PG_FUNCTION_ARGS)
{



















}

/*
 * Combine function for numeric aggregates which require sumX2
 */
Datum
numeric_poly_combine(PG_FUNCTION_ARGS)
{


























































}

/*
 * numeric_poly_serialize
 *		Serialize PolyNumAggState into bytea for aggregate functions
 *which require sumX2.
 */
Datum
numeric_poly_serialize(PG_FUNCTION_ARGS)
{





























































}

/*
 * numeric_poly_deserialize
 *		Deserialize PolyNumAggState from bytea for aggregate functions
 *which require sumX2.
 */
Datum
numeric_poly_deserialize(PG_FUNCTION_ARGS)
{



















































}

/*
 * Transition function for int8 input when we don't need sumX2.
 */
Datum
int8_avg_accum(PG_FUNCTION_ARGS)
{























}

/*
 * Combine function for PolyNumAggState for aggregates which don't require
 * sumX2
 */
Datum
int8_avg_combine(PG_FUNCTION_ARGS)
{





















































}

/*
 * int8_avg_serialize
 *		Serialize PolyNumAggState into bytea using the standard
 *		recv-function infrastructure.
 */
Datum
int8_avg_serialize(PG_FUNCTION_ARGS)
{

















































}

/*
 * int8_avg_deserialize
 *		Deserialize bytea back into PolyNumAggState.
 */
Datum
int8_avg_deserialize(PG_FUNCTION_ARGS)
{






































}

/*
 * Inverse transition functions to go with the above.
 */

Datum
int2_accum_inv(PG_FUNCTION_ARGS)
{




























}

Datum
int4_accum_inv(PG_FUNCTION_ARGS)
{




























}

Datum
int8_accum_inv(PG_FUNCTION_ARGS)
{
























}

Datum
int8_avg_accum_inv(PG_FUNCTION_ARGS)
{




























}

Datum
numeric_poly_sum(PG_FUNCTION_ARGS)
{

























}

Datum
numeric_poly_avg(PG_FUNCTION_ARGS)
{


























}

Datum
numeric_avg(PG_FUNCTION_ARGS)
{


























}

Datum
numeric_sum(PG_FUNCTION_ARGS)
{























}

/*
 * Workhorse routine for the standard deviance and variance
 * aggregates. 'state' is aggregate's transition state.
 * 'variance' specifies whether we should calculate the
 * variance or the standard deviation. 'sample' indicates whether the
 * caller is interested in the sample or the population
 * variance/stddev.
 *
 * If appropriate variance statistic is undefined for the input,
 * *is_null is set to true and NULL is returned.
 */
static Numeric
numeric_stddev_internal(NumericAggState *state, bool variance, bool sample, bool *is_null)
{






















































































}

Datum
numeric_var_samp(PG_FUNCTION_ARGS)
{
















}

Datum
numeric_stddev_samp(PG_FUNCTION_ARGS)
{
















}

Datum
numeric_var_pop(PG_FUNCTION_ARGS)
{
















}

Datum
numeric_stddev_pop(PG_FUNCTION_ARGS)
{
















}

#ifdef HAVE_INT128
static Numeric
numeric_poly_stddev_internal(Int128AggState *state, bool variance, bool sample, bool *is_null)
{





































}
#endif

Datum
numeric_poly_var_samp(PG_FUNCTION_ARGS)
{




















}

Datum
numeric_poly_stddev_samp(PG_FUNCTION_ARGS)
{




















}

Datum
numeric_poly_var_pop(PG_FUNCTION_ARGS)
{




















}

Datum
numeric_poly_stddev_pop(PG_FUNCTION_ARGS)
{




















}

/*
 * SUM transition functions for integer datatypes.
 *
 * To avoid overflow, we use accumulators wider than the input datatype.
 * A Numeric accumulator is needed for int8 input; for int4 and int2
 * inputs, we use int8 accumulators which should be sufficient for practical
 * purposes.  (The latter two therefore don't really belong in this file,
 * but we keep them here anyway.)
 *
 * Because SQL defines the SUM() of no values to be NULL, not zero,
 * the initial condition of the transition data value needs to be NULL. This
 * means we can't rely on ExecAgg to automatically insert the first non-null
 * data value into the transition data: it doesn't know how to do the type
 * conversion.  The upshot is that these routines have to be marked non-strict
 * and handle substitution of the first non-null input themselves.
 *
 * Note: these functions are used only in plain aggregation mode.
 * In moving-aggregate mode, we use intX_avg_accum and intX_avg_accum_inv.
 */

Datum
int2_sum(PG_FUNCTION_ARGS)
{


















































}

Datum
int4_sum(PG_FUNCTION_ARGS)
{


















































}

/*
 * Note: this function is obsolete, it's no longer used for SUM(int8).
 */
Datum
int8_sum(PG_FUNCTION_ARGS)
{

































}

/*
 * Routines for avg(int2) and avg(int4).  The transition datatype
 * is a two-element int8 array, holding count and sum.
 *
 * These functions are also used for sum(int2) and sum(int4) when
 * operating in moving-aggregate mode, since for correct inverse transitions
 * we need to count the inputs.
 */

typedef struct Int8TransTypeData
{
  int64 count;
  int64 sum;
} Int8TransTypeData;

Datum
int2_avg_accum(PG_FUNCTION_ARGS)
{




























}

Datum
int4_avg_accum(PG_FUNCTION_ARGS)
{




























}

Datum
int4_avg_combine(PG_FUNCTION_ARGS)
{






























}

Datum
int2_avg_accum_inv(PG_FUNCTION_ARGS)
{




























}

Datum
int4_avg_accum_inv(PG_FUNCTION_ARGS)
{




























}

Datum
int8_avg(PG_FUNCTION_ARGS)
{




















}

/*
 * SUM(int2) and SUM(int4) both return int8, so we can use this
 * final function for both.
 */
Datum
int2int4_sum(PG_FUNCTION_ARGS)
{
















}

/* ----------------------------------------------------------------------
 *
 * Debug support
 *
 * ----------------------------------------------------------------------
 */

#ifdef NUMERIC_DEBUG

/*
 * dump_numeric() - Dump a value in the db storage format for debugging
 */
static void
dump_numeric(const char *str, Numeric num)
{
  NumericDigit *digits = NUMERIC_DIGITS(num);
  int ndigits;
  int i;

  ndigits = NUMERIC_NDIGITS(num);

  printf("%s: NUMERIC w=%d d=%d ", str, NUMERIC_WEIGHT(num), NUMERIC_DSCALE(num));
  switch (NUMERIC_SIGN(num))
  {
  case NUMERIC_POS:
    printf("POS");
    break;
  case NUMERIC_NEG:
    printf("NEG");
    break;
  case NUMERIC_NAN:
    printf("NaN");
    break;
  default:;
    printf("SIGN=0x%x", NUMERIC_SIGN(num));
    break;
  }

  for (i = 0; i < ndigits; i++)
  {
    printf(" %0*d", DEC_DIGITS, digits[i]);
  }
  printf("\n");
}

/*
 * dump_var() - Dump a value in the variable format for debugging
 */
static void
dump_var(const char *str, NumericVar *var)
{
  int i;

  printf("%s: VAR w=%d d=%d ", str, var->weight, var->dscale);
  switch (var->sign)
  {
  case NUMERIC_POS:
    printf("POS");
    break;
  case NUMERIC_NEG:
    printf("NEG");
    break;
  case NUMERIC_NAN:
    printf("NaN");
    break;
  default:;
    printf("SIGN=0x%x", var->sign);
    break;
  }

  for (i = 0; i < var->ndigits; i++)
  {
    printf(" %0*d", DEC_DIGITS, var->digits[i]);
  }

  printf("\n");
}
#endif /* NUMERIC_DEBUG */

/* ----------------------------------------------------------------------
 *
 * Local functions follow
 *
 * In general, these do not support NaNs --- callers must eliminate
 * the possibility of NaN first.  (make_result() is an exception.)
 *
 * ----------------------------------------------------------------------
 */

/*
 * alloc_var() -
 *
 *	Allocate a digit buffer of ndigits digits (plus a spare digit for
 *rounding)
 */
static void
alloc_var(NumericVar *var, int ndigits)
{





}

/*
 * free_var() -
 *
 *	Return the digit buffer of a variable to the free pool
 */
static void
free_var(NumericVar *var)
{




}

/*
 * zero_var() -
 *
 *	Set a variable to ZERO.
 *	Note: its dscale is not touched.
 */
static void
zero_var(NumericVar *var)
{






}

/*
 * set_var_from_str()
 *
 *	Parse a string and put the number into a variable
 *
 * This function does not handle leading or trailing spaces, and it doesn't
 * accept "NaN" either.  It returns the end+1 position so that caller can
 * check for trailing spaces/garbage if deemed necessary.
 *
 * cp is the place to actually start parsing; str is what to use in error
 * reports.  (Typically cp would be the same except advanced over spaces.)
 */
static const char *
set_var_from_str(const char *str, const char *cp, NumericVar *dest)
{































































































































































}

/*
 * set_var_from_num() -
 *
 *	Convert the packed db format into a variable
 */
static void
set_var_from_num(Numeric num, NumericVar *dest)
{











}

/*
 * init_var_from_num() -
 *
 *	Initialize a variable from packed db format. The digits array is not
 *	copied, which saves some cycles when the resulting var is not modified.
 *	Also, there's no need to call free_var(), as long as you don't assign
 *any other value to it (with set_var_* functions, or by using the var as the
 *	destination of a function like add_var())
 *
 *	CAUTION: Do not modify the digits buffer of a var initialized with this
 *	function, e.g by calling round_var() or trunc_var(), as the changes will
 *	propagate to the original Numeric! It's OK to use it as the destination
 *	argument of one of the calculational functions, though.
 */
static void
init_var_from_num(Numeric num, NumericVar *dest)
{






}

/*
 * set_var_from_var() -
 *
 *	Copy one variable into another
 */
static void
set_var_from_var(const NumericVar *value, NumericVar *dest)
{














}

/*
 * get_str_from_var() -
 *
 *	Convert a var to text representation (guts of numeric_out).
 *	The var is displayed to the number of digits indicated by its dscale.
 *	Returns a palloc'd string.
 */
static char *
get_str_from_var(const NumericVar *var)
{










































































































































}

/*
 * get_str_from_var_sci() -
 *
 *	Convert a var to a normalised scientific notation text representation.
 *	This function does the heavy lifting for numeric_out_sci().
 *
 *	This notation has the general form a * 10^b, where a is known as the
 *	"significand" and b is known as the "exponent".
 *
 *	Because we can't do superscript in ASCII (and because we want to copy
 *	printf's behaviour) we display the exponent using E notation, with a
 *	minimum of two exponent digits.
 *
 *	For example, the value 1234 could be output as 1.2e+03.
 *
 *	We assume that the exponent can fit into an int32.
 *
 *	rscale is the number of decimal digits desired after the decimal point
 *in the output, negative values will be treated as meaning zero.
 *
 *	Returns a palloc'd string.
 */
static char *
get_str_from_var_sci(const NumericVar *var, int rscale)
{

































































}

/*
 * make_result_opt_error() -
 *
 *	Create the packed db numeric format in palloc()'d memory from
 *	a variable.  If "*have_error" flag is provided, on error it's set to
 *	true, NULL returned.  This is helpful when caller need to handle errors
 *	by itself.
 */
static Numeric
make_result_opt_error(const NumericVar *var, bool *have_error)
{
















































































}

/*
 * make_result() -
 *
 *	An interface to make_result_opt_error() without "have_error" argument.
 */
static Numeric
make_result(const NumericVar *var)
{

}

/*
 * apply_typmod() -
 *
 *	Do bounds checking and rounding according to the attributes
 *	typmod field.
 */
static void
apply_typmod(NumericVar *var, int32 typmod)
{






































































}

/*
 * Convert numeric to int8, rounding if needed.
 *
 * If overflow, return false (no error is raised).  Return true if okay.
 */
static bool
numericvar_to_int64(const NumericVar *var, int64 *result)
{





































































}

/*
 * Convert int8 value to numeric.
 */
static void
int64_to_numericvar(int64 val, NumericVar *var)
{




































}

#ifdef HAVE_INT128
/*
 * Convert numeric to int128, rounding if needed.
 *
 * If overflow, return false (no error is raised).  Return true if okay.
 */
static bool
numericvar_to_int128(const NumericVar *var, int128 *result)
{
































































}

/*
 * Convert 128 bit integer to numeric.
 */
static void
int128_to_numericvar(int128 val, NumericVar *var)
{




































}
#endif

/*
 * Convert numeric to float8; if out of range, return +/- HUGE_VAL
 */
static double
numeric_to_double_no_overflow(Numeric num)
{

















}

/* As above, but work from a NumericVar */
static double
numericvar_to_double_no_overflow(const NumericVar *var)
{

















}

/*
 * cmp_var() -
 *
 *	Compare two values on variable level.  We assume zeroes have been
 *	truncated to no digits.
 */
static int
cmp_var(const NumericVar *var1, const NumericVar *var2)
{

}

/*
 * cmp_var_common() -
 *
 *	Main routine of cmp_var(). This function can be used by both
 *	NumericVar and Numeric.
 */
static int
cmp_var_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, int var1sign, const NumericDigit *var2digits, int var2ndigits, int var2weight, int var2sign)
{




































}

/*
 * add_var() -
 *
 *	Full version of add functionality on variable level (handling signs).
 *	result might point to one of the operands too without danger.
 */
static void
add_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{









































































































}

/*
 * sub_var() -
 *
 *	Full version of sub functionality on variable level (handling signs).
 *	result might point to one of the operands too without danger.
 */
static void
sub_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{













































































































}

/*
 * mul_var() -
 *
 *	Multiplication on variable level. Product of var1 * var2 is stored
 *	in result.  Result is rounded to no more than rscale fractional digits.
 */
static void
mul_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale)
{






























































































































































































}

/*
 * div_var() -
 *
 *	Division on variable level. Quotient of var1 / var2 is stored in result.
 *	The quotient is figured to exactly rscale fractional digits.
 *	If round is true, it is rounded at the rscale'th digit; if false, it
 *	is truncated (towards zero) at that digit.
 */
static void
div_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round)
{


















































































































































































































































































}

/*
 * div_var_fast() -
 *
 *	This has the same API as div_var, but is implemented using the division
 *	algorithm from the "FM" library, rather than Knuth's schoolbook-division
 *	approach.  This is significantly faster but can produce inaccurate
 *	results, because it sometimes has to propagate rounding to the left,
 *	and so we can never be entirely sure that we know the requested digits
 *	exactly.  We compute DIV_GUARD_DIGITS extra digits, but there is
 *	no certainty that that's enough.  We use this only in the transcendental
 *	function calculation routines, where everything is approximate anyway.
 *
 *	Although we provide a "round" argument for consistency with div_var,
 *	it is unwise to use this function with round=false.  In truncation mode
 *	it is possible to get a result with no significant digits, for example
 *	with rscale=0 we might compute 0.99999... and truncate that to 0 when
 *	the correct answer is 1.
 */
static void
div_var_fast(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round)
{

















































































































































































































































































































}

/*
 * Default scale selection for division
 *
 * Returns the appropriate result scale for the division result.
 */
static int
select_div_scale(const NumericVar *var1, const NumericVar *var2)
{
























































}

/*
 * mod_var() -
 *
 *	Calculate the modulo of two numerics at variable level
 */
static void
mod_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{

















}

/*
 * ceil_var() -
 *
 *	Return the smallest integer greater than or equal to the argument
 *	on variable level
 */
static void
ceil_var(const NumericVar *var, NumericVar *result)
{














}

/*
 * floor_var() -
 *
 *	Return the largest integer equal to or less than the argument
 *	on variable level
 */
static void
floor_var(const NumericVar *var, NumericVar *result)
{














}

/*
 * sqrt_var() -
 *
 *	Compute the square root of x using Newton's algorithm
 */
static void
sqrt_var(const NumericVar *arg, NumericVar *result, int rscale)
{


































































}

/*
 * exp_var() -
 *
 *	Raise e to the power of x, computed to rscale fractional digits
 */
static void
exp_var(const NumericVar *arg, NumericVar *result, int rscale)
{

























































































































}

/*
 * Estimate the dweight of the most significant decimal digit of the natural
 * logarithm of a number.
 *
 * Essentially, we're approximating log10(abs(ln(var))).  This is used to
 * determine the appropriate rscale when computing natural logarithms.
 *
 * Note: many callers call this before range-checking the input.  Therefore,
 * we must be robust against values that are invalid to apply ln() to.
 * We don't wish to throw an error here, so just return zero in such cases.
 */
static int
estimate_ln_dweight(const NumericVar *var)
{








































































}

/*
 * ln_var() -
 *
 *	Compute the natural log of x
 */
static void
ln_var(const NumericVar *arg, NumericVar *result, int rscale)
{


































































































}

/*
 * log_var() -
 *
 *	Compute the logarithm of num in a given base.
 *
 *	Note: this routine chooses dscale of the result.
 */
static void
log_var(const NumericVar *base, const NumericVar *num, NumericVar *result)
{















































}

/*
 * power_var() -
 *
 *	Raise base to the power of exp
 *
 *	Note: this routine chooses dscale of the result.
 */
static void
power_var(const NumericVar *base, const NumericVar *exp, NumericVar *result)
{

































































































































































}

/*
 * power_var_int() -
 *
 *	Raise base to the power of exp, where exp is an integer.
 */
static void
power_var_int(const NumericVar *base, int exp, NumericVar *result, int rscale)
{












































































































































































}

/*
 * power_ten_int() -
 *
 *	Raise ten to the power of exp, where exp is an integer.  Note that
 *unlike power_var_int(), this does no overflow/underflow checking or rounding.
 */
static void
power_ten_int(int exp, NumericVar *result)
{























}

/* ----------------------------------------------------------------------
 *
 * Following are the lowest level functions that operate unsigned
 * on the variable level
 *
 * ----------------------------------------------------------------------
 */

/* ----------
 * cmp_abs() -
 *
 *	Compare the absolute values of var1 and var2
 *	Returns:	-1 for ABS(var1) < ABS(var2)
 *				0  for ABS(var1) == ABS(var2)
 *				1  for ABS(var1) > ABS(var2)
 * ----------
 */
static int
cmp_abs(const NumericVar *var1, const NumericVar *var2)
{

}

/* ----------
 * cmp_abs_common() -
 *
 *	Main routine of cmp_abs(). This function can be used by both
 *	NumericVar and Numeric.
 * ----------
 */
static int
cmp_abs_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, const NumericDigit *var2digits, int var2ndigits, int var2weight)
{





























































}

/*
 * add_abs() -
 *
 *	Add the absolute values of two variables into result.
 *	result might point to one of the operands without danger.
 */
static void
add_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{








































































}

/*
 * sub_abs()
 *
 *	Subtract the absolute value of var2 from the absolute value of var1
 *	and store in result. result might point to one of the operands
 *	without danger.
 *
 *	ABS(var1) MUST BE GREATER OR EQUAL ABS(var2) !!!
 */
static void
sub_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{








































































}

/*
 * round_var
 *
 * Round the value of a variable to no more than rscale decimal digits
 * after the decimal point.  NOTE: we allow rscale < 0 here, implying
 * rounding before the decimal point.
 */
static void
round_var(NumericVar *var, int rscale)
{






























































































}

/*
 * trunc_var
 *
 * Truncate (towards zero) the value of a variable at rscale decimal digits
 * after the decimal point.  NOTE: we allow rscale < 0 here, implying
 * truncation before the decimal point.
 */
static void
trunc_var(NumericVar *var, int rscale)
{



















































}

/*
 * strip_var
 *
 * Strip any leading and trailing zeroes from a numeric variable
 */
static void
strip_var(NumericVar *var)
{


























}

/* ----------------------------------------------------------------------
 *
 * Fast sum accumulator functions
 *
 * ----------------------------------------------------------------------
 */

/*
 * Reset the accumulator's value to zero.  The buffers to hold the digits
 * are not free'd.
 */
static void
accum_sum_reset(NumericSumAccum *accum)
{








}

/*
 * Accumulate a new value.
 */
static void
accum_sum_add(NumericSumAccum *accum, const NumericVar *val)
{













































}

/*
 * Propagate carries.
 */
static void
accum_sum_carry(NumericSumAccum *accum)
{








































































}

/*
 * Re-scale accumulator to accommodate new value.
 *
 * If the new value has more digits than the current digit buffers in the
 * accumulator, enlarge the buffers.
 */
static void
accum_sum_rescale(NumericSumAccum *accum, const NumericVar *val)
{














































































}

/*
 * Return the current value of the accumulator.  This perform final carry
 * propagation, and adds together the positive and negative sums.
 *
 * Unlike all the other routines, the caller is not required to switch to
 * the memory context that holds the accumulator.
 */
static void
accum_sum_final(NumericSumAccum *accum, NumericVar *result)
{








































}

/*
 * Copy an accumulator's state.
 *
 * 'dst' is assumed to be uninitialized beforehand.  No attempt is made at
 * freeing old values.
 */
static void
accum_sum_copy(NumericSumAccum *dst, NumericSumAccum *src)
{









}

/*
 * Add the current value of 'accum2' into 'accum'.
 */
static void
accum_sum_combine(NumericSumAccum *accum, NumericSumAccum *accum2)
{








}