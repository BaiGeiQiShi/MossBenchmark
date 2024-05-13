/*-------------------------------------------------------------------------
 *
 * nbtcompare.c
 *	  Comparison functions for btree access method.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtcompare.c
 *
 * NOTES
 *
 *	These functions are stored in pg_amproc.  For each operator class
 *	defined on btrees, they compute
 *
 *				compare(a, b):
 *						< 0 if a < b,
 *						= 0 if a == b,
 *						> 0 if a > b.
 *
 *	The result is always an int32 regardless of the input datatype.
 *
 *	Although any negative int32 is acceptable for reporting "<",
 *	and any positive int32 is acceptable for reporting ">", routines
 *	that work on 32-bit or wider datatypes can't just return "a - b".
 *	That could overflow and give the wrong answer.
 *
 *	NOTE: it is critical that the comparison function impose a total order
 *	on all non-NULL values of the data type, and that the datatype's
 *	boolean comparison operators (= < >= etc) yield results consistent
 *	with the comparison routine.  Otherwise bad behavior may ensue.
 *	(For example, the comparison operators must NOT punt when faced with
 *	NAN or other funny values; you must devise some collation sequence for
 *	all such values.)  If the datatype is not trivial, this is most
 *	reliably done by having the boolean operators invoke the same
 *	three-way comparison code that the btree function does.  Therefore,
 *	this file contains only btree support for "trivial" datatypes ---
 *	all others are in the /utils/adt/ files that implement their datatypes.
 *
 *	NOTE: these routines must not leak memory, since memory allocated
 *	during an index access won't be recovered till end of query.  This
 *	primarily affects comparison routines for toastable datatypes;
 *	they have to be careful to free any detoasted copy of an input datum.
 *
 *	NOTE: we used to forbid comparison functions from returning INT_MIN,
 *	but that proves to be too error-prone because some platforms' versions
 *	of memcmp() etc can return INT_MIN.  As a means of stress-testing
 *	callers, this file can be compiled with STRESS_SORT_INT_MIN defined
 *	to cause many of these functions to return INT_MIN or INT_MAX instead of
 *	their customary -1/+1.  For production, though, that's not a good idea
 *	since users or third-party code might expect the traditional results.
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "utils/builtins.h"
#include "utils/sortsupport.h"

#ifdef STRESS_SORT_INT_MIN
#define A_LESS_THAN_B INT_MIN
#define A_GREATER_THAN_B INT_MAX
#else
#define A_LESS_THAN_B (-1)
#define A_GREATER_THAN_B 1
#endif

Datum
btboolcmp(PG_FUNCTION_ARGS)
{




}

Datum
btint2cmp(PG_FUNCTION_ARGS)
{




}

static int
btint2fastcmp(Datum x, Datum y, SortSupport ssup)
{




}

Datum
btint2sortsupport(PG_FUNCTION_ARGS)
{




}

Datum
btint4cmp(PG_FUNCTION_ARGS)
{















}

static int
btint4fastcmp(Datum x, Datum y, SortSupport ssup)
{















}

Datum
btint4sortsupport(PG_FUNCTION_ARGS)
{




}

Datum
btint8cmp(PG_FUNCTION_ARGS)
{















}

static int
btint8fastcmp(Datum x, Datum y, SortSupport ssup)
{















}

Datum
btint8sortsupport(PG_FUNCTION_ARGS)
{




}

Datum
btint48cmp(PG_FUNCTION_ARGS)
{















}

Datum
btint84cmp(PG_FUNCTION_ARGS)
{















}

Datum
btint24cmp(PG_FUNCTION_ARGS)
{















}

Datum
btint42cmp(PG_FUNCTION_ARGS)
{















}

Datum
btint28cmp(PG_FUNCTION_ARGS)
{















}

Datum
btint82cmp(PG_FUNCTION_ARGS)
{















}

Datum
btoidcmp(PG_FUNCTION_ARGS)
{















}

static int
btoidfastcmp(Datum x, Datum y, SortSupport ssup)
{















}

Datum
btoidsortsupport(PG_FUNCTION_ARGS)
{




}

Datum
btoidvectorcmp(PG_FUNCTION_ARGS)
{

























}

Datum
btcharcmp(PG_FUNCTION_ARGS)
{





}