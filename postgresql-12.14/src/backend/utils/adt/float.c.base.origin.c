/*-------------------------------------------------------------------------
 *
 * float.c
 *	  Functions for the built-in floating-point types.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/float.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include "catalog/pg_type.h"
#include "common/int.h"
#include "common/shortest_dec.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/sortsupport.h"
#include "utils/timestamp.h"

/*
 * Configurable GUC parameter
 *
 * If >0, use shortest-decimal format for output; this is both the default and
 * allows for compatibility with clients that explicitly set a value here to
 * get round-trip-accurate results. If 0 or less, then use the old, slow,
 * decimal rounding method.
 */
int extra_float_digits = 1;

/* Cached constants for degree-based trig functions */
static bool degree_consts_set = false;
static float8 sin_30 = 0;
static float8 one_minus_cos_60 = 0;
static float8 asin_0_5 = 0;
static float8 acos_0_5 = 0;
static float8 atan_1_0 = 0;
static float8 tan_45 = 0;
static float8 cot_45 = 0;

/*
 * These are intentionally not static; don't "fix" them.  They will never
 * be referenced by other files, much less changed; but we don't want the
 * compiler to know that, else it might try to precompute expressions
 * involving them.  See comments for init_degree_constants().
 */
float8 degree_c_thirty = 30.0;
float8 degree_c_forty_five = 45.0;
float8 degree_c_sixty = 60.0;
float8 degree_c_one_half = 0.5;
float8 degree_c_one = 1.0;

/* State for drandom() and setseed() */
static bool drandom_seed_set = false;
static unsigned short drandom_seed[3] = {0, 0, 0};

/* Local function prototypes */
static double
sind_q1(double x);
static double
cosd_q1(double x);
static void
init_degree_constants(void);

#ifndef HAVE_CBRT
/*
 * Some machines (in particular, some versions of AIX) have an extern
 * declaration for cbrt() in <math.h> but fail to provide the actual
 * function, which causes configure to not set HAVE_CBRT.  Furthermore,
 * their compilers spit up at the mismatch between extern declaration
 * and static definition.  We work around that here by the expedient
 * of a #define to make the actual name of the static function different.
 */
#define cbrt my_cbrt
static double
cbrt(double x);
#endif /* HAVE_CBRT */

/*
 * We use these out-of-line ereport() calls to report float overflow,
 * underflow, and zero-divide, because following our usual practice of
 * repeating them at each call site would lead to a lot of code bloat.
 *
 * This does mean that you don't get a useful error location indicator.
 */
pg_noinline void
float_overflow_error(void)
{

}

pg_noinline void
float_underflow_error(void)
{

}

pg_noinline void
float_zero_divide_error(void)
{

}

/*
 * Returns -1 if 'val' represents negative infinity, 1 if 'val'
 * represents (positive) infinity, and 0 otherwise. On some platforms,
 * this is equivalent to the isinf() macro, but not everywhere: C99
 * does not specify that isinf() needs to distinguish between positive
 * and negative infinity.
 */
int
is_infinite(double val)
{














}

/* ========== USER I/O ROUTINES ========== */

/*
 *		float4in		- converts "num" to float4
 *
 * Note that this code now uses strtof(), where it used to use strtod().
 *
 * The motivation for using strtof() is to avoid a double-rounding problem:
 * for certain decimal inputs, if you round the input correctly to a double,
 * and then round the double to a float, the result is incorrect in that it
 * does not match the result of rounding the decimal value to float directly.
 *
 * One of the best examples is 7.038531e-26:
 *
 * 0xAE43FDp-107 = 7.03853069185120912085...e-26
 *      midpoint   7.03853100000000022281...e-26
 * 0xAE43FEp-107 = 7.03853130814879132477...e-26
 *
 * making 0xAE43FDp-107 the correct float result, but if you do the conversion
 * via a double, you get
 *
 * 0xAE43FD.7FFFFFF8p-107 = 7.03853099999999907487...e-26
 *               midpoint   7.03853099999999964884...e-26
 * 0xAE43FD.80000000p-107 = 7.03853100000000022281...e-26
 * 0xAE43FD.80000008p-107 = 7.03853100000000137076...e-26
 *
 * so the value rounds to the double exactly on the midpoint between the two
 * nearest floats, and then rounding again to a float gives the incorrect
 * result of 0xAE43FEp-107.
 *
 */
Datum
float4in(PG_FUNCTION_ARGS)
{







































































































































}

/*
 *		float4out		- converts a float4 number to a string
 *						  using a standard output format
 */
Datum
float4out(PG_FUNCTION_ARGS)
{












}

/*
 *		float4recv			- converts external binary
 *format to float4
 */
Datum
float4recv(PG_FUNCTION_ARGS)
{



}

/*
 *		float4send			- converts float4 to binary
 *format
 */
Datum
float4send(PG_FUNCTION_ARGS)
{






}

/*
 *		float8in		- converts "num" to float8
 */
Datum
float8in(PG_FUNCTION_ARGS)
{



}

/* Convenience macro: set *have_error flag (if provided) or throw error */
#define RETURN_ERROR(throw_error)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (have_error)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *have_error = true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      return 0.0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      throw_error;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/*
 * float8in_internal_opt_error - guts of float8in()
 *
 * This is exposed for use by functions that want a reasonably
 * platform-independent way of inputting doubles.  The behavior is
 * essentially like strtod + ereport on error, but note the following
 * differences:
 * 1. Both leading and trailing whitespace are skipped.
 * 2. If endptr_p is NULL, we throw error if there's trailing junk.
 * Otherwise, it's up to the caller to complain about trailing junk.
 * 3. In event of a syntax error, the report mentions the given type_name
 * and prints orig_string as the input; this is meant to support use of
 * this function with types such as "box" and "point", where what we are
 * parsing here is just a substring of orig_string.
 *
 * "num" could validly be declared "const char *", but that results in an
 * unreasonable amount of extra casting both here and in callers, so we don't.
 *
 * When "*have_error" flag is provided, it's set instead of throwing an
 * error.  This is helpful when caller need to handle errors by itself.
 */
double
float8in_internal_opt_error(char *num, char **endptr_p, const char *type_name, const char *orig_string, bool *have_error)
{

































































































































}

/*
 * Interface to float8in_internal_opt_error() without "have_error" argument.
 */
double
float8in_internal(char *num, char **endptr_p, const char *type_name, const char *orig_string)
{

}

/*
 *		float8out		- converts float8 number to a string
 *						  using a standard output format
 */
Datum
float8out(PG_FUNCTION_ARGS)
{



}

/*
 * float8out_internal - guts of float8out()
 *
 * This is exposed for use by functions that want a reasonably
 * platform-independent way of outputting doubles.
 * The result is always palloc'd.
 */
char *
float8out_internal(double num)
{











}

/*
 *		float8recv			- converts external binary
 *format to float8
 */
Datum
float8recv(PG_FUNCTION_ARGS)
{



}

/*
 *		float8send			- converts float8 to binary
 *format
 */
Datum
float8send(PG_FUNCTION_ARGS)
{






}

/* ========== PUBLIC ROUTINES ========== */

/*
 *		======================
 *		FLOAT4 BASE OPERATIONS
 *		======================
 */

/*
 *		float4abs		- returns |arg1| (absolute value)
 */
Datum
float4abs(PG_FUNCTION_ARGS)
{



}

/*
 *		float4um		- returns -arg1 (unary minus)
 */
Datum
float4um(PG_FUNCTION_ARGS)
{





}

Datum
float4up(PG_FUNCTION_ARGS)
{



}

Datum
float4larger(PG_FUNCTION_ARGS)
{













}

Datum
float4smaller(PG_FUNCTION_ARGS)
{













}

/*
 *		======================
 *		FLOAT8 BASE OPERATIONS
 *		======================
 */

/*
 *		float8abs		- returns |arg1| (absolute value)
 */
Datum
float8abs(PG_FUNCTION_ARGS)
{



}

/*
 *		float8um		- returns -arg1 (unary minus)
 */
Datum
float8um(PG_FUNCTION_ARGS)
{





}

Datum
float8up(PG_FUNCTION_ARGS)
{



}

Datum
float8larger(PG_FUNCTION_ARGS)
{













}

Datum
float8smaller(PG_FUNCTION_ARGS)
{













}

/*
 *		====================
 *		ARITHMETIC OPERATORS
 *		====================
 */

/*
 *		float4pl		- returns arg1 + arg2
 *		float4mi		- returns arg1 - arg2
 *		float4mul		- returns arg1 * arg2
 *		float4div		- returns arg1 / arg2
 */
Datum
float4pl(PG_FUNCTION_ARGS)
{




}

Datum
float4mi(PG_FUNCTION_ARGS)
{




}

Datum
float4mul(PG_FUNCTION_ARGS)
{




}

Datum
float4div(PG_FUNCTION_ARGS)
{




}

/*
 *		float8pl		- returns arg1 + arg2
 *		float8mi		- returns arg1 - arg2
 *		float8mul		- returns arg1 * arg2
 *		float8div		- returns arg1 / arg2
 */
Datum
float8pl(PG_FUNCTION_ARGS)
{




}

Datum
float8mi(PG_FUNCTION_ARGS)
{




}

Datum
float8mul(PG_FUNCTION_ARGS)
{




}

Datum
float8div(PG_FUNCTION_ARGS)
{




}

/*
 *		====================
 *		COMPARISON OPERATORS
 *		====================
 */

/*
 *		float4{eq,ne,lt,le,gt,ge}		- float4/float4
 *comparison operations
 */
int
float4_cmp_internal(float4 a, float4 b)
{









}

Datum
float4eq(PG_FUNCTION_ARGS)
{




}

Datum
float4ne(PG_FUNCTION_ARGS)
{




}

Datum
float4lt(PG_FUNCTION_ARGS)
{




}

Datum
float4le(PG_FUNCTION_ARGS)
{




}

Datum
float4gt(PG_FUNCTION_ARGS)
{




}

Datum
float4ge(PG_FUNCTION_ARGS)
{




}

Datum
btfloat4cmp(PG_FUNCTION_ARGS)
{




}

static int
btfloat4fastcmp(Datum x, Datum y, SortSupport ssup)
{




}

Datum
btfloat4sortsupport(PG_FUNCTION_ARGS)
{




}

/*
 *		float8{eq,ne,lt,le,gt,ge}		- float8/float8
 *comparison operations
 */
int
float8_cmp_internal(float8 a, float8 b)
{









}

Datum
float8eq(PG_FUNCTION_ARGS)
{




}

Datum
float8ne(PG_FUNCTION_ARGS)
{




}

Datum
float8lt(PG_FUNCTION_ARGS)
{




}

Datum
float8le(PG_FUNCTION_ARGS)
{




}

Datum
float8gt(PG_FUNCTION_ARGS)
{




}

Datum
float8ge(PG_FUNCTION_ARGS)
{




}

Datum
btfloat8cmp(PG_FUNCTION_ARGS)
{




}

static int
btfloat8fastcmp(Datum x, Datum y, SortSupport ssup)
{




}

Datum
btfloat8sortsupport(PG_FUNCTION_ARGS)
{




}

Datum
btfloat48cmp(PG_FUNCTION_ARGS)
{





}

Datum
btfloat84cmp(PG_FUNCTION_ARGS)
{





}

/*
 * in_range support function for float8.
 *
 * Note: we needn't supply a float8_float4 variant, as implicit coercion
 * of the offset value takes care of that scenario just as well.
 */
Datum
in_range_float8_float8(PG_FUNCTION_ARGS)
{






































































}

/*
 * in_range support function for float4.
 *
 * We would need a float4_float8 variant in any case, so we supply that and
 * let implicit coercion take care of the float4_float4 case.
 */
Datum
in_range_float4_float8(PG_FUNCTION_ARGS)
{






































































}

/*
 *		===================
 *		CONVERSION ROUTINES
 *		===================
 */

/*
 *		ftod			- converts a float4 number to a float8
 *number
 */
Datum
ftod(PG_FUNCTION_ARGS)
{



}

/*
 *		dtof			- converts a float8 number to a float4
 *number
 */
Datum
dtof(PG_FUNCTION_ARGS)
{














}

/*
 *		dtoi4			- converts a float8 number to an int4
 *number
 */
Datum
dtoi4(PG_FUNCTION_ARGS)
{
















}

/*
 *		dtoi2			- converts a float8 number to an int2
 *number
 */
Datum
dtoi2(PG_FUNCTION_ARGS)
{
















}

/*
 *		i4tod			- converts an int4 number to a float8
 *number
 */
Datum
i4tod(PG_FUNCTION_ARGS)
{



}

/*
 *		i2tod			- converts an int2 number to a float8
 *number
 */
Datum
i2tod(PG_FUNCTION_ARGS)
{



}

/*
 *		ftoi4			- converts a float4 number to an int4
 *number
 */
Datum
ftoi4(PG_FUNCTION_ARGS)
{
















}

/*
 *		ftoi2			- converts a float4 number to an int2
 *number
 */
Datum
ftoi2(PG_FUNCTION_ARGS)
{
















}

/*
 *		i4tof			- converts an int4 number to a float4
 *number
 */
Datum
i4tof(PG_FUNCTION_ARGS)
{



}

/*
 *		i2tof			- converts an int2 number to a float4
 *number
 */
Datum
i2tof(PG_FUNCTION_ARGS)
{



}

/*
 *		=======================
 *		RANDOM FLOAT8 OPERATORS
 *		=======================
 */

/*
 *		dround			- returns	ROUND(arg1)
 */
Datum
dround(PG_FUNCTION_ARGS)
{



}

/*
 *		dceil			- returns the smallest integer greater
 *than or equal to the specified float
 */
Datum
dceil(PG_FUNCTION_ARGS)
{



}

/*
 *		dfloor			- returns the largest integer lesser
 *than or equal to the specified float
 */
Datum
dfloor(PG_FUNCTION_ARGS)
{



}

/*
 *		dsign			- returns -1 if the argument is less
 *than 0, 0 if the argument is equal to 0, and 1 if the argument is greater than
 *zero.
 */
Datum
dsign(PG_FUNCTION_ARGS)
{

















}

/*
 *		dtrunc			- returns truncation-towards-zero of
 *arg1, arg1 >= 0 ... the greatest integer less than or equal to arg1 arg1 < 0
 *... the least integer greater than or equal to arg1
 */
Datum
dtrunc(PG_FUNCTION_ARGS)
{













}

/*
 *		dsqrt			- returns square root of arg1
 */
Datum
dsqrt(PG_FUNCTION_ARGS)
{



















}

/*
 *		dcbrt			- returns cube root of arg1
 */
Datum
dcbrt(PG_FUNCTION_ARGS)
{














}

/*
 *		dpow			- returns pow(arg1,arg2)
 */
Datum
dpow(PG_FUNCTION_ARGS)
{



















































































}

/*
 *		dexp			- returns the exponential function of
 *arg1
 */
Datum
dexp(PG_FUNCTION_ARGS)
{




















}

/*
 *		dlog1			- returns the natural logarithm of arg1
 */
Datum
dlog1(PG_FUNCTION_ARGS)
{



























}

/*
 *		dlog10			- returns the base 10 logarithm of arg1
 */
Datum
dlog10(PG_FUNCTION_ARGS)
{




























}

/*
 *		dacos			- returns the arccos of arg1 (radians)
 */
Datum
dacos(PG_FUNCTION_ARGS)
{


























}

/*
 *		dasin			- returns the arcsin of arg1 (radians)
 */
Datum
dasin(PG_FUNCTION_ARGS)
{


























}

/*
 *		datan			- returns the arctan of arg1 (radians)
 */
Datum
datan(PG_FUNCTION_ARGS)
{





















}

/*
 *		atan2			- returns the arctan of arg1/arg2
 *(radians)
 */
Datum
datan2(PG_FUNCTION_ARGS)
{





















}

/*
 *		dcos			- returns the cosine of arg1 (radians)
 */
Datum
dcos(PG_FUNCTION_ARGS)
{




































}

/*
 *		dcot			- returns the cotangent of arg1
 *(radians)
 */
Datum
dcot(PG_FUNCTION_ARGS)
{





















}

/*
 *		dsin			- returns the sine of arg1 (radians)
 */
Datum
dsin(PG_FUNCTION_ARGS)
{






















}

/*
 *		dtan			- returns the tangent of arg1 (radians)
 */
Datum
dtan(PG_FUNCTION_ARGS)
{



















}

/* ========== DEGREE-BASED TRIGONOMETRIC FUNCTIONS ========== */

/*
 * Initialize the cached constants declared at the head of this file
 * (sin_30 etc).  The fact that we need those at all, let alone need this
 * Rube-Goldberg-worthy method of initializing them, is because there are
 * compilers out there that will precompute expressions such as sin(constant)
 * using a sin() function different from what will be used at runtime.  If we
 * want exact results, we must ensure that none of the scaling constants used
 * in the degree-based trig functions are computed that way.  To do so, we
 * compute them from the variables degree_c_thirty etc, which are also really
 * constants, but the compiler cannot assume that.
 *
 * Other hazards we are trying to forestall with this kluge include the
 * possibility that compilers will rearrange the expressions, or compute
 * some intermediate results in registers wider than a standard double.
 *
 * In the places where we use these constants, the typical pattern is like
 *		volatile float8 sin_x = sin(x * RADIANS_PER_DEGREE);
 *		return (sin_x / sin_30);
 * where we hope to get a value of exactly 1.0 from the division when x = 30.
 * The volatile temporary variable is needed on machines with wide float
 * registers, to ensure that the result of sin(x) is rounded to double width
 * the same as the value of sin_30 has been.  Experimentation with gcc shows
 * that marking the temp variable volatile is necessary to make the store and
 * reload actually happen; hopefully the same trick works for other compilers.
 * (gcc's documentation suggests using the -ffloat-store compiler switch to
 * ensure this, but that is compiler-specific and it also pessimizes code in
 * many places where we don't care about this.)
 */
static void
init_degree_constants(void)
{








}

#define INIT_DEGREE_CONSTANTS()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!degree_consts_set)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      init_degree_constants();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

/*
 *		asind_q1		- returns the inverse sine of x in
 *degrees, for x in the range [0, 1].  The result is an angle in the first
 *quadrant --- [0, 90] degrees.
 *
 *						  For the 3 special case inputs
 *(0, 0.5 and 1), this function will return exact values (0, 30 and 90 degrees
 *respectively).
 */
static double
asind_q1(double x)
{


















}

/*
 *		acosd_q1		- returns the inverse cosine of x in
 *degrees, for x in the range [0, 1].  The result is an angle in the first
 *quadrant --- [0, 90] degrees.
 *
 *						  For the 3 special case inputs
 *(0, 0.5 and 1), this function will return exact values (0, 60 and 90 degrees
 *respectively).
 */
static double
acosd_q1(double x)
{


















}

/*
 *		dacosd			- returns the arccos of arg1 (degrees)
 */
Datum
dacosd(PG_FUNCTION_ARGS)
{




































}

/*
 *		dasind			- returns the arcsin of arg1 (degrees)
 */
Datum
dasind(PG_FUNCTION_ARGS)
{




































}

/*
 *		datand			- returns the arctan of arg1 (degrees)
 */
Datum
datand(PG_FUNCTION_ARGS)
{



























}

/*
 *		atan2d			- returns the arctan of arg1/arg2
 *(degrees)
 */
Datum
datan2d(PG_FUNCTION_ARGS)
{































}

/*
 *		sind_0_to_30	- returns the sine of an angle that lies between
 *0 and 30 degrees.  This will return exactly 0 when x is 0, and exactly 0.5
 *when x is 30 degrees.
 */
static double
sind_0_to_30(double x)
{



}

/*
 *		cosd_0_to_60	- returns the cosine of an angle that lies
 *between 0 and 60 degrees.  This will return exactly 1 when x is 0, and exactly
 *0.5 when x is 60 degrees.
 */
static double
cosd_0_to_60(double x)
{



}

/*
 *		sind_q1			- returns the sine of an angle in the
 *first quadrant (0 to 90 degrees).
 */
static double
sind_q1(double x)
{














}

/*
 *		cosd_q1			- returns the cosine of an angle in the
 *first quadrant (0 to 90 degrees).
 */
static double
cosd_q1(double x)
{














}

/*
 *		dcosd			- returns the cosine of arg1 (degrees)
 */
Datum
dcosd(PG_FUNCTION_ARGS)
{


















































}

/*
 *		dcotd			- returns the cotangent of arg1
 *(degrees)
 */
Datum
dcotd(PG_FUNCTION_ARGS)
{





























































}

/*
 *		dsind			- returns the sine of arg1 (degrees)
 */
Datum
dsind(PG_FUNCTION_ARGS)
{



















































}

/*
 *		dtand			- returns the tangent of arg1 (degrees)
 */
Datum
dtand(PG_FUNCTION_ARGS)
{





























































}

/*
 *		degrees		- returns degrees converted from radians
 */
Datum
degrees(PG_FUNCTION_ARGS)
{



}

/*
 *		dpi				- returns the constant PI
 */
Datum
dpi(PG_FUNCTION_ARGS)
{

}

/*
 *		radians		- returns radians converted from degrees
 */
Datum
radians(PG_FUNCTION_ARGS)
{



}

/* ========== HYPERBOLIC FUNCTIONS ========== */

/*
 *		dsinh			- returns the hyperbolic sine of arg1
 */
Datum
dsinh(PG_FUNCTION_ARGS)
{
























}

/*
 *		dcosh			- returns the hyperbolic cosine of arg1
 */
Datum
dcosh(PG_FUNCTION_ARGS)
{





















}

/*
 *		dtanh			- returns the hyperbolic tangent of arg1
 */
Datum
dtanh(PG_FUNCTION_ARGS)
{














}

/*
 *		dasinh			- returns the inverse hyperbolic sine of
 *arg1
 */
Datum
dasinh(PG_FUNCTION_ARGS)
{









}

/*
 *		dacosh			- returns the inverse hyperbolic cosine
 *of arg1
 */
Datum
dacosh(PG_FUNCTION_ARGS)
{

















}

/*
 *		datanh			- returns the inverse hyperbolic tangent
 *of arg1
 */
Datum
datanh(PG_FUNCTION_ARGS)
{
































}

/*
 *		drandom		- returns a random number
 */
Datum
drandom(PG_FUNCTION_ARGS)
{




























}

/*
 *		setseed		- set seed for the random number generator
 */
Datum
setseed(PG_FUNCTION_ARGS)
{
















}

/*
 *		=========================
 *		FLOAT AGGREGATE OPERATORS
 *		=========================
 *
 *		float8_accum		- accumulate for AVG(), variance
 *aggregates, etc. float4_accum		- same, but input data is float4
 *		float8_avg			- produce final result for float
 *AVG() float8_var_samp		- produce final result for float VAR_SAMP()
 *		float8_var_pop		- produce final result for float
 *VAR_POP() float8_stddev_samp	- produce final result for float STDDEV_SAMP()
 *		float8_stddev_pop	- produce final result for float
 *STDDEV_POP()
 *
 * The naive schoolbook implementation of these aggregates works by
 * accumulating sum(X) and sum(X^2).  However, this approach suffers from
 * large rounding errors in the final computation of quantities like the
 * population variance (N*sum(X^2) - sum(X)^2) / N^2, since each of the
 * intermediate terms is potentially very large, while the difference is often
 * quite small.
 *
 * Instead we use the Youngs-Cramer algorithm [1] which works by accumulating
 * Sx=sum(X) and Sxx=sum((X-Sx/N)^2), using a numerically stable algorithm to
 * incrementally update those quantities.  The final computations of each of
 * the aggregate values is then trivial and gives more accurate results (for
 * example, the population variance is just Sxx/N).  This algorithm is also
 * fairly easy to generalize to allow parallel execution without loss of
 * precision (see, for example, [2]).  For more details, and a comparison of
 * this with other algorithms, see [3].
 *
 * The transition datatype for all these aggregates is a 3-element array
 * of float8, holding the values N, Sx, Sxx in that order.
 *
 * Note that we represent N as a float to avoid having to build a special
 * datatype.  Given a reasonable floating-point implementation, there should
 * be no accuracy loss unless N exceeds 2 ^ 52 or so (by which time the
 * user will have doubtless lost interest anyway...)
 *
 * [1] Some Results Relevant to Choice of Sum and Sum-of-Product Algorithms,
 * E. A. Youngs and E. M. Cramer, Technometrics Vol 13, No 3, August 1971.
 *
 * [2] Updating Formulae and a Pairwise Algorithm for Computing Sample
 * Variances, T. F. Chan, G. H. Golub & R. J. LeVeque, COMPSTAT 1982.
 *
 * [3] Numerically Stable Parallel Computation of (Co-)Variance, Erich
 * Schubert and Michael Gertz, Proceedings of the 30th International
 * Conference on Scientific and Statistical Database Management, 2018.
 */

static float8 *
check_float8_array(ArrayType *transarray, const char *caller, int n)
{










}

/*
 * float8_combine
 *
 * An aggregate combine function used to combine two 3 fields
 * aggregate transition data into a single transition data.
 * This function is used only in two stage aggregation and
 * shouldn't be called outside aggregate context.
 */
Datum
float8_combine(PG_FUNCTION_ARGS)
{
















































































}

Datum
float8_accum(PG_FUNCTION_ARGS)
{













































































}

Datum
float4_accum(PG_FUNCTION_ARGS)
{















































































}

Datum
float8_avg(PG_FUNCTION_ARGS)
{
















}

Datum
float8_var_pop(PG_FUNCTION_ARGS)
{


















}

Datum
float8_var_samp(PG_FUNCTION_ARGS)
{


















}

Datum
float8_stddev_pop(PG_FUNCTION_ARGS)
{


















}

Datum
float8_stddev_samp(PG_FUNCTION_ARGS)
{


















}

/*
 *		=========================
 *		SQL2003 BINARY AGGREGATES
 *		=========================
 *
 * As with the preceding aggregates, we use the Youngs-Cramer algorithm to
 * reduce rounding errors in the aggregate final functions.
 *
 * The transition datatype for all these aggregates is a 6-element array of
 * float8, holding the values N, Sx=sum(X), Sxx=sum((X-Sx/N)^2), Sy=sum(Y),
 * Syy=sum((Y-Sy/N)^2), Sxy=sum((X-Sx/N)*(Y-Sy/N)) in that order.
 *
 * Note that Y is the first argument to all these aggregates!
 *
 * It might seem attractive to optimize this by having multiple accumulator
 * functions that only calculate the sums actually needed.  But on most
 * modern machines, a couple of extra floating-point multiplies will be
 * insignificant compared to the other per-tuple overhead, so I've chosen
 * to minimize code space instead.
 */

Datum
float8_regr_accum(PG_FUNCTION_ARGS)
{











































































































}

/*
 * float8_regr_combine
 *
 * An aggregate combine function used to combine two 6 fields
 * aggregate transition data into a single transition data.
 * This function is used only in two stage aggregation and
 * shouldn't be called outside aggregate context.
 */
Datum
float8_regr_combine(PG_FUNCTION_ARGS)
{

















































































































}

Datum
float8_regr_sxx(PG_FUNCTION_ARGS)
{

















}

Datum
float8_regr_syy(PG_FUNCTION_ARGS)
{

















}

Datum
float8_regr_sxy(PG_FUNCTION_ARGS)
{

















}

Datum
float8_regr_avgx(PG_FUNCTION_ARGS)
{















}

Datum
float8_regr_avgy(PG_FUNCTION_ARGS)
{















}

Datum
float8_covar_pop(PG_FUNCTION_ARGS)
{















}

Datum
float8_covar_samp(PG_FUNCTION_ARGS)
{















}

Datum
float8_corr(PG_FUNCTION_ARGS)
{

























}

Datum
float8_regr_r2(PG_FUNCTION_ARGS)
{































}

Datum
float8_regr_slope(PG_FUNCTION_ARGS)
{
























}

Datum
float8_regr_intercept(PG_FUNCTION_ARGS)
{


























}

/*
 *		====================================
 *		MIXED-PRECISION ARITHMETIC OPERATORS
 *		====================================
 */

/*
 *		float48pl		- returns arg1 + arg2
 *		float48mi		- returns arg1 - arg2
 *		float48mul		- returns arg1 * arg2
 *		float48div		- returns arg1 / arg2
 */
Datum
float48pl(PG_FUNCTION_ARGS)
{




}

Datum
float48mi(PG_FUNCTION_ARGS)
{




}

Datum
float48mul(PG_FUNCTION_ARGS)
{




}

Datum
float48div(PG_FUNCTION_ARGS)
{




}

/*
 *		float84pl		- returns arg1 + arg2
 *		float84mi		- returns arg1 - arg2
 *		float84mul		- returns arg1 * arg2
 *		float84div		- returns arg1 / arg2
 */
Datum
float84pl(PG_FUNCTION_ARGS)
{




}

Datum
float84mi(PG_FUNCTION_ARGS)
{




}

Datum
float84mul(PG_FUNCTION_ARGS)
{




}

Datum
float84div(PG_FUNCTION_ARGS)
{




}

/*
 *		====================
 *		COMPARISON OPERATORS
 *		====================
 */

/*
 *		float48{eq,ne,lt,le,gt,ge}		- float4/float8
 *comparison operations
 */
Datum
float48eq(PG_FUNCTION_ARGS)
{




}

Datum
float48ne(PG_FUNCTION_ARGS)
{




}

Datum
float48lt(PG_FUNCTION_ARGS)
{




}

Datum
float48le(PG_FUNCTION_ARGS)
{




}

Datum
float48gt(PG_FUNCTION_ARGS)
{




}

Datum
float48ge(PG_FUNCTION_ARGS)
{




}

/*
 *		float84{eq,ne,lt,le,gt,ge}		- float8/float4
 *comparison operations
 */
Datum
float84eq(PG_FUNCTION_ARGS)
{




}

Datum
float84ne(PG_FUNCTION_ARGS)
{




}

Datum
float84lt(PG_FUNCTION_ARGS)
{




}

Datum
float84le(PG_FUNCTION_ARGS)
{




}

Datum
float84gt(PG_FUNCTION_ARGS)
{




}

Datum
float84ge(PG_FUNCTION_ARGS)
{




}

/*
 * Implements the float8 version of the width_bucket() function
 * defined by SQL2003. See also width_bucket_numeric().
 *
 * 'bound1' and 'bound2' are the lower and upper bounds of the
 * histogram's range, respectively. 'count' is the number of buckets
 * in the histogram. width_bucket() returns an integer indicating the
 * bucket number that 'operand' belongs to in an equiwidth histogram
 * with the specified characteristics. An operand smaller than the
 * lower bound is assigned to bucket 0. An operand greater than the
 * upper bound is assigned to an additional bucket (with number
 * count+1). We don't allow "NaN" for any of the float8 inputs, and we
 * don't allow either of the histogram bounds to be +/- infinity.
 */
Datum
width_bucket_float8(PG_FUNCTION_ARGS)
{

































































}

/* ========== PRIVATE ROUTINES ========== */

#ifndef HAVE_CBRT

static double
cbrt(double x)
{
  int isneg = (x < 0.0);
  double absx = fabs(x);
  double tmpres = pow(absx, (double)1.0 / (double)3.0);

  /*
   * The result is somewhat inaccurate --- not really pow()'s fault, as the
   * exponent it's handed contains roundoff error.  We can improve the
   * accuracy by doing one iteration of Newton's formula.  Beware of zero
   * input however.
   */
  if (tmpres > 0.0)
  {
    tmpres -= (tmpres - absx / (tmpres * tmpres)) / (double)3.0;
  }

  return isneg ? -tmpres : tmpres;
}

#endif /* !HAVE_CBRT */