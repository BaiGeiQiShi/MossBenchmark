/*-------------------------------------------------------------------------
 *
 * bool.c
 *	  Functions for the built-in type "bool".
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/bool.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>

#include "libpq/pqformat.h"
#include "utils/builtins.h"

/*
 * Try to interpret value as boolean value.  Valid values are: true,
 * false, yes, no, on, off, 1, 0; as well as unique prefixes thereof.
 * If the string parses okay, return true, else false.
 * If okay and result is not NULL, return the value in *result.
 */
bool
parse_bool(const char *value, bool *result)
{

}

bool
parse_bool_with_len(const char *value, size_t len, bool *result)
{































































































}

/*****************************************************************************
 *	 USER I/O ROUTINES
 **
 *****************************************************************************/

/*
 *		boolin			- converts "t" or "f" to 1 or 0
 *
 * Check explicitly for "true/false" and TRUE/FALSE, 1/0, YES/NO, ON/OFF.
 * Reject other values.
 *
 * In the switch statement, check the most-used possibilities first.
 */
Datum
boolin(PG_FUNCTION_ARGS)
{





























}

/*
 *		boolout			- converts 1 or 0 to "t" or "f"
 */
Datum
boolout(PG_FUNCTION_ARGS)
{






}

/*
 *		boolrecv			- converts external binary
 *format to bool
 *
 * The external representation is one byte.  Any nonzero value is taken
 * as "true".
 */
Datum
boolrecv(PG_FUNCTION_ARGS)
{





}

/*
 *		boolsend			- converts bool to binary format
 */
Datum
boolsend(PG_FUNCTION_ARGS)
{






}

/*
 *		booltext			- cast function for bool => text
 *
 * We need this because it's different from the behavior of boolout();
 * this function follows the SQL-spec result (except for producing lower case)
 */
Datum
booltext(PG_FUNCTION_ARGS)
{













}

/*****************************************************************************
 *	 PUBLIC ROUTINES
 **
 *****************************************************************************/

Datum
booleq(PG_FUNCTION_ARGS)
{




}

Datum
boolne(PG_FUNCTION_ARGS)
{




}

Datum
boollt(PG_FUNCTION_ARGS)
{




}

Datum
boolgt(PG_FUNCTION_ARGS)
{




}

Datum
boolle(PG_FUNCTION_ARGS)
{




}

Datum
boolge(PG_FUNCTION_ARGS)
{




}

/*
 * boolean-and and boolean-or aggregates.
 */

/*
 * Function for standard EVERY aggregate conforming to SQL 2003.
 * The aggregate is also named bool_and for consistency.
 *
 * Note: this is only used in plain aggregate mode, not moving-aggregate mode.
 */
Datum
booland_statefunc(PG_FUNCTION_ARGS)
{

}

/*
 * Function for standard ANY/SOME aggregate conforming to SQL 2003.
 * The aggregate is named bool_or, because ANY/SOME have parsing conflicts.
 *
 * Note: this is only used in plain aggregate mode, not moving-aggregate mode.
 */
Datum
boolor_statefunc(PG_FUNCTION_ARGS)
{

}

typedef struct BoolAggState
{
  int64 aggcount; /* number of non-null values aggregated */
  int64 aggtrue;  /* number of values aggregated that are true */
} BoolAggState;

static BoolAggState *
makeBoolAggState(FunctionCallInfo fcinfo)
{













}

Datum
bool_accum(PG_FUNCTION_ARGS)
{




















}

Datum
bool_accum_inv(PG_FUNCTION_ARGS)
{




















}

Datum
bool_alltrue(PG_FUNCTION_ARGS)
{












}

Datum
bool_anytrue(PG_FUNCTION_ARGS)
{












}