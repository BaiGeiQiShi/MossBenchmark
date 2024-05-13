/*
 * brin_inclusion.c
 *		Implementation of inclusion opclasses for BRIN
 *
 * This module provides framework BRIN support functions for the "inclusion"
 * operator classes.  A few SQL-level support functions are also required for
 * each opclass.
 *
 * The "inclusion" BRIN strategy is useful for types that support R-Tree
 * operations.  This implementation is a straight mapping of those operations
 * to the block-range nature of BRIN, with two exceptions: (a) we explicitly
 * support "empty" elements: at least with range types, we need to consider
 * emptiness separately from regular R-Tree strategies; and (b) we need to
 * consider "unmergeable" elements, that is, a set of elements for whose union
 * no representation exists.  The only case where that happens as of this
 * writing is the INET type, where IPv6 values cannot be merged with IPv4
 * values.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_inclusion.c
 */
#include "postgres.h"

#include "access/brin_internal.h"
#include "access/brin_tuple.h"
#include "access/genam.h"
#include "access/skey.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/*
 * Additional SQL level support functions
 *
 * Procedure numbers must not use values reserved for BRIN itself; see
 * brin_internal.h.
 */
#define INCLUSION_MAX_PROCNUMS 4 /* maximum support procs we need */
#define PROCNUM_MERGE 11         /* required */
#define PROCNUM_MERGEABLE 12     /* optional */
#define PROCNUM_CONTAINS 13      /* optional */
#define PROCNUM_EMPTY 14         /* optional */

/*
 * Subtract this from procnum to obtain index in InclusionOpaque arrays
 * (Must be equal to minimum of private procnums).
 */
#define PROCNUM_BASE 11

/*-
 * The values stored in the bv_values arrays correspond to:
 *
 * INCLUSION_UNION
 *		the union of the values in the block range
 * INCLUSION_UNMERGEABLE
 *		whether the values in the block range cannot be merged
 *		(e.g. an IPv6 address amidst IPv4 addresses)
 * INCLUSION_CONTAINS_EMPTY
 *		whether an empty value is present in any tuple
 *		in the block range
 */
#define INCLUSION_UNION 0
#define INCLUSION_UNMERGEABLE 1
#define INCLUSION_CONTAINS_EMPTY 2

typedef struct InclusionOpaque
{
  FmgrInfo extra_procinfos[INCLUSION_MAX_PROCNUMS];
  bool extra_proc_missing[INCLUSION_MAX_PROCNUMS];
  Oid cached_subtype;
  FmgrInfo strategy_procinfos[RTMaxStrategyNumber];
} InclusionOpaque;

static FmgrInfo *
inclusion_get_procinfo(BrinDesc *bdesc, uint16 attno, uint16 procnum);
static FmgrInfo *
inclusion_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum);

/*
 * BRIN inclusion OpcInfo function
 */
Datum
brin_inclusion_opcinfo(PG_FUNCTION_ARGS)
{



























}

/*
 * BRIN inclusion add value function
 *
 * Examine the given index tuple (which contains partial status of a certain
 * page range) by comparing it to the given value that comes from another heap
 * tuple.  If the new value is outside the union specified by the existing
 * tuple values, update the index tuple and return true.  Otherwise, return
 * false and do not modify in this case.
 */
Datum
brin_inclusion_add_value(PG_FUNCTION_ARGS)
{















































































































}

/*
 * BRIN inclusion consistent function
 *
 * All of the strategies are optional.
 */
Datum
brin_inclusion_consistent(PG_FUNCTION_ARGS)
{
















































































































































































































































}

/*
 * BRIN inclusion union function
 *
 * Given two BrinValues, update the first of them as a union of the summary
 * values contained in both.  The second one is untouched.
 */
Datum
brin_inclusion_union(PG_FUNCTION_ARGS)
{




















































































}

/*
 * Cache and return inclusion opclass support procedure
 *
 * Return the procedure corresponding to the given function support number
 * or null if it is not exists.
 */
static FmgrInfo *
inclusion_get_procinfo(BrinDesc *bdesc, uint16 attno, uint16 procnum)
{
































}

/*
 * Cache and return the procedure of the given strategy
 *
 * Return the procedure corresponding to the given sub-type and strategy
 * number.  The data type of the index will be used as the left hand side of
 * the operator and the given sub-type will be used as the right hand side.
 * Throws an error if the pg_amop row does not exist, but that should not
 * happen with a properly configured opclass.
 *
 * It always throws an error when the data type of the opclass is different
 * from the data type of the column or the expression.  That happens when the
 * column data type has implicit cast to the opclass data type.  We don't
 * bother casting types, because this situation can easily be avoided by
 * setting storage data type to that of the opclass.  The same problem does not
 * apply to the data type of the right hand side, because the type in the
 * ScanKey always matches the opclass' one.
 *
 * Note: this function mirrors minmax_get_strategy_procinfo; if changes are
 * made here, see that function too.
 */
static FmgrInfo *
inclusion_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum)
{














































}