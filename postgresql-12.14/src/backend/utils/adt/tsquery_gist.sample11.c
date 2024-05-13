/*-------------------------------------------------------------------------
 *
 * tsquery_gist.c
 *	  GiST index support for tsquery
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsquery_gist.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/stratnum.h"
#include "access/gist.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

#define GETENTRY(vec, pos) DatumGetTSQuerySign((vec)->vector[pos].key)

Datum
gtsquery_compress(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *retval = entry;

  if (entry->leafkey)
  {
    TSQuerySign sign;

    retval = (GISTENTRY *)palloc(sizeof(GISTENTRY));
    sign = makeTSQuerySign(DatumGetTSQuery(entry->key));

    gistentryinit(*retval, TSQuerySignGetDatum(sign), entry->rel, entry->page, entry->offset, false);
  }

  PG_RETURN_POINTER(retval);
}

/*
 * We do not need a decompress function, because the other gtsquery
 * support functions work with the compressed representation.
 */

Datum
gtsquery_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  TSQuery query = PG_GETARG_TSQUERY(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

  /* Oid		subtype = PG_GETARG_OID(3); */
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  TSQuerySign key = DatumGetTSQuerySign(entry->key);
  TSQuerySign sq = makeTSQuerySign(query);
  bool retval;

  /* All cases served by this function are inexact */
  *recheck = true;

  switch (strategy)
  {
  case RTContainsStrategyNumber:;;
    if (GIST_LEAF(entry))
    {
      retval = (key & sq) == sq;
    }
    else
    {

    }
    break;
  case RTContainedByStrategyNumber:;;
    if (GIST_LEAF(entry))
    {
      retval = (key & sq) == key;
    }
    else
    {

    }
    break;
  default:;;;

  }
  PG_RETURN_BOOL(retval);
}

Datum
gtsquery_union(PG_FUNCTION_ARGS)
{















}

Datum
gtsquery_same(PG_FUNCTION_ARGS)
{







}

static int
sizebitvec(TSQuerySign sign)
{








}

static int
hemdist(TSQuerySign a, TSQuerySign b)
{



}

Datum
gtsquery_penalty(PG_FUNCTION_ARGS)
{







}

typedef struct
{
  OffsetNumber pos;
  int32 cost;
} SPLITCOST;

static int
comparecost(const void *a, const void *b)
{








}

#define WISH_F(a, b, c) (double)(-(double)(((a) - (b)) * ((a) - (b)) * ((a) - (b))) * (c))

Datum
gtsquery_picksplit(PG_FUNCTION_ARGS)
{

























































































}

/*
 * Formerly, gtsquery_consistent was declared in pg_proc.h with arguments
 * that did not match the documented conventions for GiST support functions.
 * We fixed that, but we still need a pg_proc entry with the old signature
 * to support reloading pre-9.6 contrib/tsearch2 opclass declarations.
 * This compatibility function should go away eventually.
 */
Datum
gtsquery_consistent_oldsig(PG_FUNCTION_ARGS)
{

}