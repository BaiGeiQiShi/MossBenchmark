/*-------------------------------------------------------------------------
 *
 * int8.c
 *	  Internal 64-bit integer operations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/int8.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "common/int.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/optimizer.h"
#include "utils/int8.h"
#include "utils/builtins.h"

#define MAXINT8LEN 25

typedef struct
{
  int64 current;
  int64 finish;
  int64 step;
} generate_series_fctx;

/***********************************************************************
 **
 **		Routines for 64-bit integers.
 **
 ***********************************************************************/

/*----------------------------------------------------------
 * Formatting and conversion routines.
 *---------------------------------------------------------*/

/*
 * scanint8 --- try to parse a string into an int8.
 *
 * If errorOK is false, ereport a useful error message if the string is bad.
 * If errorOK is true, just return "false" for bad input.
 */
bool
scanint8(const char *str, bool errorOK, int64 *result)
{



















































































}

/* int8in()
 */
Datum
int8in(PG_FUNCTION_ARGS)
{





}

/* int8out()
 */
Datum
int8out(PG_FUNCTION_ARGS)
{







}

/*
 *		int8recv			- converts external binary
 *format to int8
 */
Datum
int8recv(PG_FUNCTION_ARGS)
{



}

/*
 *		int8send			- converts int8 to binary format
 */
Datum
int8send(PG_FUNCTION_ARGS)
{






}

/*----------------------------------------------------------
 *	Relational operators for int8s, including cross-data-type comparisons.
 *---------------------------------------------------------*/

/* int8relop()
 * Is val1 relop val2?
 */
Datum
int8eq(PG_FUNCTION_ARGS)
{




}

Datum
int8ne(PG_FUNCTION_ARGS)
{




}

Datum
int8lt(PG_FUNCTION_ARGS)
{




}

Datum
int8gt(PG_FUNCTION_ARGS)
{




}

Datum
int8le(PG_FUNCTION_ARGS)
{




}

Datum
int8ge(PG_FUNCTION_ARGS)
{




}

/* int84relop()
 * Is 64-bit val1 relop 32-bit val2?
 */
Datum
int84eq(PG_FUNCTION_ARGS)
{




}

Datum
int84ne(PG_FUNCTION_ARGS)
{




}

Datum
int84lt(PG_FUNCTION_ARGS)
{




}

Datum
int84gt(PG_FUNCTION_ARGS)
{




}

Datum
int84le(PG_FUNCTION_ARGS)
{




}

Datum
int84ge(PG_FUNCTION_ARGS)
{




}

/* int48relop()
 * Is 32-bit val1 relop 64-bit val2?
 */
Datum
int48eq(PG_FUNCTION_ARGS)
{




}

Datum
int48ne(PG_FUNCTION_ARGS)
{




}

Datum
int48lt(PG_FUNCTION_ARGS)
{




}

Datum
int48gt(PG_FUNCTION_ARGS)
{




}

Datum
int48le(PG_FUNCTION_ARGS)
{




}

Datum
int48ge(PG_FUNCTION_ARGS)
{




}

/* int82relop()
 * Is 64-bit val1 relop 16-bit val2?
 */
Datum
int82eq(PG_FUNCTION_ARGS)
{




}

Datum
int82ne(PG_FUNCTION_ARGS)
{




}

Datum
int82lt(PG_FUNCTION_ARGS)
{




}

Datum
int82gt(PG_FUNCTION_ARGS)
{




}

Datum
int82le(PG_FUNCTION_ARGS)
{




}

Datum
int82ge(PG_FUNCTION_ARGS)
{




}

/* int28relop()
 * Is 16-bit val1 relop 64-bit val2?
 */
Datum
int28eq(PG_FUNCTION_ARGS)
{




}

Datum
int28ne(PG_FUNCTION_ARGS)
{




}

Datum
int28lt(PG_FUNCTION_ARGS)
{




}

Datum
int28gt(PG_FUNCTION_ARGS)
{




}

Datum
int28le(PG_FUNCTION_ARGS)
{




}

Datum
int28ge(PG_FUNCTION_ARGS)
{




}

/*
 * in_range support function for int8.
 *
 * Note: we needn't supply int8_int4 or int8_int2 variants, as implicit
 * coercion of the offset value takes care of those scenarios just as well.
 */
Datum
in_range_int8_int8(PG_FUNCTION_ARGS)
{



































}

/*----------------------------------------------------------
 *	Arithmetic operators on 64-bit integers.
 *---------------------------------------------------------*/

Datum
int8um(PG_FUNCTION_ARGS)
{









}

Datum
int8up(PG_FUNCTION_ARGS)
{



}

Datum
int8pl(PG_FUNCTION_ARGS)
{









}

Datum
int8mi(PG_FUNCTION_ARGS)
{









}

Datum
int8mul(PG_FUNCTION_ARGS)
{









}

Datum
int8div(PG_FUNCTION_ARGS)
{
































}

/* int8abs()
 * Absolute value
 */
Datum
int8abs(PG_FUNCTION_ARGS)
{









}

/* int8mod()
 * Modulo operation.
 */
Datum
int8mod(PG_FUNCTION_ARGS)
{























}

Datum
int8inc(PG_FUNCTION_ARGS)
{

































}

Datum
int8dec(PG_FUNCTION_ARGS)
{
































}

/*
 * These functions are exactly like int8inc/int8dec but are used for
 * aggregates that count only non-null values.  Since the functions are
 * declared strict, the null checks happen before we ever get here, and all we
 * need do is increment the state value.  We could actually make these pg_proc
 * entries point right at int8inc/int8dec, but then the opr_sanity regression
 * test would complain about mismatched entries for a built-in function.
 */

Datum
int8inc_any(PG_FUNCTION_ARGS)
{

}

Datum
int8inc_float8_float8(PG_FUNCTION_ARGS)
{

}

Datum
int8dec_any(PG_FUNCTION_ARGS)
{

}

Datum
int8larger(PG_FUNCTION_ARGS)
{







}

Datum
int8smaller(PG_FUNCTION_ARGS)
{







}

Datum
int84pl(PG_FUNCTION_ARGS)
{









}

Datum
int84mi(PG_FUNCTION_ARGS)
{









}

Datum
int84mul(PG_FUNCTION_ARGS)
{









}

Datum
int84div(PG_FUNCTION_ARGS)
{
































}

Datum
int48pl(PG_FUNCTION_ARGS)
{









}

Datum
int48mi(PG_FUNCTION_ARGS)
{









}

Datum
int48mul(PG_FUNCTION_ARGS)
{









}

Datum
int48div(PG_FUNCTION_ARGS)
{












}

Datum
int82pl(PG_FUNCTION_ARGS)
{









}

Datum
int82mi(PG_FUNCTION_ARGS)
{









}

Datum
int82mul(PG_FUNCTION_ARGS)
{









}

Datum
int82div(PG_FUNCTION_ARGS)
{
































}

Datum
int28pl(PG_FUNCTION_ARGS)
{









}

Datum
int28mi(PG_FUNCTION_ARGS)
{









}

Datum
int28mul(PG_FUNCTION_ARGS)
{









}

Datum
int28div(PG_FUNCTION_ARGS)
{












}

/* Binary arithmetics
 *
 *		int8and		- returns arg1 & arg2
 *		int8or		- returns arg1 | arg2
 *		int8xor		- returns arg1 # arg2
 *		int8not		- returns ~arg1
 *		int8shl		- returns arg1 << arg2
 *		int8shr		- returns arg1 >> arg2
 */

Datum
int8and(PG_FUNCTION_ARGS)
{




}

Datum
int8or(PG_FUNCTION_ARGS)
{




}

Datum
int8xor(PG_FUNCTION_ARGS)
{




}

Datum
int8not(PG_FUNCTION_ARGS)
{



}

Datum
int8shl(PG_FUNCTION_ARGS)
{




}

Datum
int8shr(PG_FUNCTION_ARGS)
{




}

/*----------------------------------------------------------
 *	Conversion operators.
 *---------------------------------------------------------*/

Datum
int48(PG_FUNCTION_ARGS)
{



}

Datum
int84(PG_FUNCTION_ARGS)
{








}

Datum
int28(PG_FUNCTION_ARGS)
{



}

Datum
int82(PG_FUNCTION_ARGS)
{








}

Datum
i8tod(PG_FUNCTION_ARGS)
{






}

/* dtoi8()
 * Convert float8 to 8-byte integer.
 */
Datum
dtoi8(PG_FUNCTION_ARGS)
{
















}

Datum
i8tof(PG_FUNCTION_ARGS)
{






}

/* ftoi8()
 * Convert float4 to 8-byte integer.
 */
Datum
ftoi8(PG_FUNCTION_ARGS)
{
















}

Datum
i8tooid(PG_FUNCTION_ARGS)
{








}

Datum
oidtoi8(PG_FUNCTION_ARGS)
{



}

/*
 * non-persistent numeric series generator
 */
Datum
generate_series_int8(PG_FUNCTION_ARGS)
{

}

Datum
generate_series_step_int8(PG_FUNCTION_ARGS)
{









































































}

/*
 * Planner support function for generate_series(int8, int8 [, int8])
 */
Datum
generate_series_int8_support(PG_FUNCTION_ARGS)
{























































}