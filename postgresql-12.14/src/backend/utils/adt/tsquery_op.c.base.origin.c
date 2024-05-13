/*-------------------------------------------------------------------------
 *
 * tsquery_op.c
 *	  Various operations with tsquery
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsquery_op.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

Datum
tsquery_numnode(PG_FUNCTION_ARGS)
{





}

static QTNode *
join_tsqueries(TSQuery a, TSQuery b, int8 operator, uint16 distance)
{


















}

Datum
tsquery_and(PG_FUNCTION_ARGS)
{

























}

Datum
tsquery_or(PG_FUNCTION_ARGS)
{

























}

Datum
tsquery_phrase_distance(PG_FUNCTION_ARGS)
{






























}

Datum
tsquery_phrase(PG_FUNCTION_ARGS)
{

}

Datum
tsquery_not(PG_FUNCTION_ARGS)
{



























}

static int
CompareTSQ(TSQuery a, TSQuery b)
{





















}

Datum
tsquery_cmp(PG_FUNCTION_ARGS)
{








}

#define CMPFUNC(NAME, CONDITION)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  Datum NAME(PG_FUNCTION_ARGS)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    TSQuery a = PG_GETARG_TSQUERY_COPY(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    TSQuery b = PG_GETARG_TSQUERY_COPY(1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    int res = CompareTSQ(a, b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    PG_FREE_IF_COPY(a, 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    PG_FREE_IF_COPY(b, 1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    PG_RETURN_BOOL(CONDITION);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  /* keep compiler quiet - no extra ; */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  extern int no_such_variable

CMPFUNC(tsquery_lt, res < 0);
CMPFUNC(tsquery_le, res <= 0);
CMPFUNC(tsquery_eq, res == 0);
CMPFUNC(tsquery_ge, res >= 0);
CMPFUNC(tsquery_gt, res > 0);
CMPFUNC(tsquery_ne, res != 0);

TSQuerySign
makeTSQuerySign(TSQuery a)
{














}

static char **
collectTSQueryValues(TSQuery a, int *nvalues_p)
{


























}

static int
cmp_string(const void *a, const void *b)
{




}

static int
remove_duplicates(char **strings, int n)
{




















}

Datum
tsq_mcontains(PG_FUNCTION_ARGS)
{













































}

Datum
tsq_mcontained(PG_FUNCTION_ARGS)
{

}