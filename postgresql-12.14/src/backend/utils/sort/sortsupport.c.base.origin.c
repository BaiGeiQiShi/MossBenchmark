/*-------------------------------------------------------------------------
 *
 * sortsupport.c
 *	  Support routines for accelerated sorting.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/sort/sortsupport.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/nbtree.h"
#include "catalog/pg_am.h"
#include "fmgr.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/sortsupport.h"

/* Info needed to use an old-style comparison function as a sort comparator */
typedef struct
{
  FmgrInfo flinfo;                 /* lookup data for comparison function */
  FunctionCallInfoBaseData fcinfo; /* reusable callinfo structure */
} SortShimExtra;

#define SizeForSortShimExtra(nargs) (offsetof(SortShimExtra, fcinfo) + SizeForFunctionCallInfo(nargs))

/*
 * Shim function for calling an old-style comparator
 *
 * This is essentially an inlined version of FunctionCall2Coll(), except
 * we assume that the FunctionCallInfoBaseData was already mostly set up by
 * PrepareSortSupportComparisonShim.
 */
static int
comparison_shim(Datum x, Datum y, SortSupport ssup)
{


















}

/*
 * Set up a shim function to allow use of an old-style btree comparison
 * function as if it were a sort support comparator.
 */
void
PrepareSortSupportComparisonShim(Oid cmpFunc, SortSupport ssup)
{














}

/*
 * Look up and call sortsupport function to setup SortSupport comparator;
 * or if no such function exists or it declines to set up the appropriate
 * state, prepare a suitable shim.
 */
static void
FinishSortSupportFunction(Oid opfamily, Oid opcintype, SortSupport ssup)
{



























}

/*
 * Fill in SortSupport given an ordering operator (btree "<" or ">" operator).
 *
 * Caller must previously have zeroed the SortSupportData structure and then
 * filled in ssup_cxt, ssup_collation, and ssup_nulls_first.  This will fill
 * in ssup_reverse as well as the comparator function pointer.
 */
void
PrepareSortSupportFromOrderingOp(Oid orderingOp, SortSupport ssup)
{














}

/*
 * Fill in SortSupport given an index relation, attribute, and strategy.
 *
 * Caller must previously have zeroed the SortSupportData structure and then
 * filled in ssup_cxt, ssup_attno, ssup_collation, and ssup_nulls_first.  This
 * will fill in ssup_reverse (based on the supplied strategy), as well as the
 * comparator function pointer.
 */
void
PrepareSortSupportFromIndexRel(Relation indexRel, int16 strategy, SortSupport ssup)
{
















}