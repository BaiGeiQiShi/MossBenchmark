/*
 * cash.c
 * Written by D'Arcy J.M. Cain
 * darcy@druid.net
 * http://www.druid.net/darcy/
 *
 * Functions to allow input and output of money normally but store
 * and handle it as 64 bit ints
 *
 * A slightly modified version of this file and a discussion of the
 * workings can be found in the book "Software Solutions in C" by
 * Dale Schumacher, Academic Press, ISBN: 0-12-632360-7 except that
 * this version handles 64 bit numbers and so can hold values up to
 * $92,233,720,368,547,758.07.
 *
 * src/backend/utils/adt/cash.c
 */

#include "postgres.h"

#include <limits.h>
#include <ctype.h>
#include <math.h>

#include "common/int.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/cash.h"
#include "utils/int8.h"
#include "utils/numeric.h"
#include "utils/pg_locale.h"

/*************************************************************************
 * Private routines
 ************************************************************************/

static const char *
num_word(Cash value)
{





















































} /* num_word() */

/* cash_in()
 * Convert a string to a cash data type.
 * Format is [$]###[,]###[.##]
 * Examples: 123.45 $123.45 $123,456.78
 *
 */
Datum
cash_in(PG_FUNCTION_ARGS)
{








































































































































































































































}

/* cash_out()
 * Function to convert cash to a dollars and cents representation, using
 * the lc_monetary locale's formatting.
 */
Datum
cash_out(PG_FUNCTION_ARGS)
{















































































































































































}

/*
 *		cash_recv			- converts external binary
 *format to cash
 */
Datum
cash_recv(PG_FUNCTION_ARGS)
{



}

/*
 *		cash_send			- converts cash to binary format
 */
Datum
cash_send(PG_FUNCTION_ARGS)
{






}

/*
 * Comparison functions
 */

Datum
cash_eq(PG_FUNCTION_ARGS)
{




}

Datum
cash_ne(PG_FUNCTION_ARGS)
{




}

Datum
cash_lt(PG_FUNCTION_ARGS)
{




}

Datum
cash_le(PG_FUNCTION_ARGS)
{




}

Datum
cash_gt(PG_FUNCTION_ARGS)
{




}

Datum
cash_ge(PG_FUNCTION_ARGS)
{




}

Datum
cash_cmp(PG_FUNCTION_ARGS)
{















}

/* cash_pl()
 * Add two cash values.
 */
Datum
cash_pl(PG_FUNCTION_ARGS)
{







}

/* cash_mi()
 * Subtract two cash values.
 */
Datum
cash_mi(PG_FUNCTION_ARGS)
{







}

/* cash_div_cash()
 * Divide cash by cash, returning float8.
 */
Datum
cash_div_cash(PG_FUNCTION_ARGS)
{











}

/* cash_mul_flt8()
 * Multiply cash by float8.
 */
Datum
cash_mul_flt8(PG_FUNCTION_ARGS)
{






}

/* flt8_mul_cash()
 * Multiply float8 by cash.
 */
Datum
flt8_mul_cash(PG_FUNCTION_ARGS)
{






}

/* cash_div_flt8()
 * Divide cash by float8.
 */
Datum
cash_div_flt8(PG_FUNCTION_ARGS)
{











}

/* cash_mul_flt4()
 * Multiply cash by float4.
 */
Datum
cash_mul_flt4(PG_FUNCTION_ARGS)
{






}

/* flt4_mul_cash()
 * Multiply float4 by cash.
 */
Datum
flt4_mul_cash(PG_FUNCTION_ARGS)
{






}

/* cash_div_flt4()
 * Divide cash by float4.
 *
 */
Datum
cash_div_flt4(PG_FUNCTION_ARGS)
{











}

/* cash_mul_int8()
 * Multiply cash by int8.
 */
Datum
cash_mul_int8(PG_FUNCTION_ARGS)
{






}

/* int8_mul_cash()
 * Multiply int8 by cash.
 */
Datum
int8_mul_cash(PG_FUNCTION_ARGS)
{






}

/* cash_div_int8()
 * Divide cash by 8-byte integer.
 */
Datum
cash_div_int8(PG_FUNCTION_ARGS)
{












}

/* cash_mul_int4()
 * Multiply cash by int4.
 */
Datum
cash_mul_int4(PG_FUNCTION_ARGS)
{






}

/* int4_mul_cash()
 * Multiply int4 by cash.
 */
Datum
int4_mul_cash(PG_FUNCTION_ARGS)
{






}

/* cash_div_int4()
 * Divide cash by 4-byte integer.
 *
 */
Datum
cash_div_int4(PG_FUNCTION_ARGS)
{












}

/* cash_mul_int2()
 * Multiply cash by int2.
 */
Datum
cash_mul_int2(PG_FUNCTION_ARGS)
{






}

/* int2_mul_cash()
 * Multiply int2 by cash.
 */
Datum
int2_mul_cash(PG_FUNCTION_ARGS)
{






}

/* cash_div_int2()
 * Divide cash by int2.
 *
 */
Datum
cash_div_int2(PG_FUNCTION_ARGS)
{











}

/* cashlarger()
 * Return larger of two cash values.
 */
Datum
cashlarger(PG_FUNCTION_ARGS)
{







}

/* cashsmaller()
 * Return smaller of two cash values.
 */
Datum
cashsmaller(PG_FUNCTION_ARGS)
{







}

/* cash_words()
 * This converts an int4 as well but to a representation using words
 * Obviously way North American centric - sorry
 */
Datum
cash_words(PG_FUNCTION_ARGS)
{




















































































}

/* cash_numeric()
 * Convert cash to numeric.
 */
Datum
cash_numeric(PG_FUNCTION_ARGS)
{
















































}

/* numeric_cash()
 * Convert numeric to cash.
 */
Datum
numeric_cash(PG_FUNCTION_ARGS)
{






























}

/* int4_cash()
 * Convert int4 (int) to cash
 */
Datum
int4_cash(PG_FUNCTION_ARGS)
{

























}

/* int8_cash()
 * Convert int8 (bigint) to cash
 */
Datum
int8_cash(PG_FUNCTION_ARGS)
{

























}