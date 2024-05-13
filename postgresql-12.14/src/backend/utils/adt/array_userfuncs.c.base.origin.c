/*-------------------------------------------------------------------------
 *
 * array_userfuncs.c
 *	  Misc user-visible array support functions
 *
 * Copyright (c) 2003-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/array_userfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "common/int.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

static Datum
array_position_common(FunctionCallInfo fcinfo);

/*
 * fetch_array_arg_replace_nulls
 *
 * Fetch an array-valued argument in expanded form; if it's null, construct an
 * empty array value of the proper data type.  Also cache basic element type
 * information in fn_extra.
 *
 * Caution: if the input is a read/write pointer, this returns the input
 * argument; so callers must be sure that their changes are "safe", that is
 * they cannot leave the array in a corrupt state.
 *
 * If we're being called as an aggregate function, make sure any newly-made
 * expanded array is allocated in the aggregate state context, so as to save
 * copying operations.
 */
static ExpandedArrayHeader *
fetch_array_arg_replace_nulls(FunctionCallInfo fcinfo, int argno)
{















































}

/*-----------------------------------------------------------------------------
 * array_append :
 *		push an element onto the end of a one-dimensional array
 *----------------------------------------------------------------------------
 */
Datum
array_append(PG_FUNCTION_ARGS)
{














































}

/*-----------------------------------------------------------------------------
 * array_prepend :
 *		push an element onto the front of a one-dimensional array
 *----------------------------------------------------------------------------
 */
Datum
array_prepend(PG_FUNCTION_ARGS)
{























































}

/*-----------------------------------------------------------------------------
 * array_cat :
 *		concatenate two nD arrays to form an nD array, or
 *		push an (n-1)D array onto the end of an nD array
 *----------------------------------------------------------------------------
 */
Datum
array_cat(PG_FUNCTION_ARGS)
{









































































































































































































}

/*
 * ARRAY_AGG(anynonarray) aggregate function
 */
Datum
array_agg_transfn(PG_FUNCTION_ARGS)
{









































}

Datum
array_agg_finalfn(PG_FUNCTION_ARGS)
{



























}

/*
 * ARRAY_AGG(anyarray) aggregate function
 */
Datum
array_agg_array_transfn(PG_FUNCTION_ARGS)
{






































}

Datum
array_agg_array_finalfn(PG_FUNCTION_ARGS)
{






















}

/*-----------------------------------------------------------------------------
 * array_position, array_position_start :
 *			return the offset of a value in an array.
 *
 * IS NOT DISTINCT FROM semantics are used for comparisons.  Return NULL when
 * the value is not found.
 *-----------------------------------------------------------------------------
 */
Datum
array_position(PG_FUNCTION_ARGS)
{

}

Datum
array_position_start(PG_FUNCTION_ARGS)
{

}

/*
 * array_position_common
 *		Common code for array_position and array_position_start
 *
 * These are separate wrappers for the sake of opr_sanity regression test.
 * They are not strict so we have to test for null inputs explicitly.
 */
static Datum
array_position_common(FunctionCallInfo fcinfo)
{










































































































































}

/*-----------------------------------------------------------------------------
 * array_positions :
 *			return an array of positions of a value in an array.
 *
 * IS NOT DISTINCT FROM semantics are used for comparisons.  Returns NULL when
 * the input array is NULL.  When the value is not found in the array, returns
 * an empty array.
 *
 * This is not strict so we have to test for null inputs explicitly.
 *-----------------------------------------------------------------------------
 */
Datum
array_positions(PG_FUNCTION_ARGS)
{

















































































































}