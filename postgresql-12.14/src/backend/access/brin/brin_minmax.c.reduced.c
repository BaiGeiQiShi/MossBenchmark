/*
 * brin_minmax.c
 *		Implementation of Min/Max opclass for BRIN
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_minmax.c
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/brin_internal.h"
#include "access/brin_tuple.h"
#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "catalog/pg_amop.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct MinmaxOpaque {
  Oid cached_subtype;
  FmgrInfo strategy_procinfos[BTMaxStrategyNumber];
} MinmaxOpaque;

static FmgrInfo *
minmax_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum);

Datum
brin_minmax_opcinfo(PG_FUNCTION_ARGS)
{














}

/*
 * Examine the given index tuple (which contains partial status of a certain
 * page range) by comparing it to the given value that comes from another heap
 * tuple.  If the new value is outside the min/max range specified by the
 * existing tuple values, update the index tuple and return true.  Otherwise,
 * return false and do not modify in this case.
 */
Datum
brin_minmax_add_value(PG_FUNCTION_ARGS)
{



































































}

/*
 * Given an index tuple corresponding to a certain page range and a scan key,
 * return whether the scan key is consistent with the index tuple's min/max
 * values.  Return true if so, false otherwise.
 */
Datum
brin_minmax_consistent(PG_FUNCTION_ARGS)
{














































































}

/*
 * Given two BrinValues, update the first of them as a union of the summary
 * values contained in both.  The second one is untouched.
 */
Datum
brin_minmax_union(PG_FUNCTION_ARGS)
{


























































}

/*
 * Cache and return the procedure for the given strategy.
 *
 * Note: this function mirrors inclusion_get_strategy_procinfo; see notes
 * there.  If changes are made here, see that function too.
 */
static FmgrInfo *
minmax_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum)
{










































}
