/*-------------------------------------------------------------------------
 *
 * tsginidx.c
 *	 GIN support functions for tsvector_ops
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsginidx.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/gin.h"
#include "access/stratnum.h"
#include "miscadmin.h"
#include "tsearch/ts_type.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

Datum
gin_cmp_tslexeme(PG_FUNCTION_ARGS)
{









}

Datum
gin_cmp_prefix(PG_FUNCTION_ARGS)
{



















}

Datum
gin_extract_tsvector(PG_FUNCTION_ARGS)
{

























}

Datum
gin_extract_tsquery(PG_FUNCTION_ARGS)
{














































































}

typedef struct
{
  QueryItem *first_item;
  GinTernaryValue *check;
  int *map_item_operand;
  bool *need_recheck;
} GinChkVal;

static GinTernaryValue
checkcondition_gin_internal(GinChkVal *gcv, QueryOperand *val, ExecPhraseData *data)
{
















}

/*
 * Wrapper of check condition function for TS_execute.
 */
static bool
checkcondition_gin(void *checkval, QueryOperand *val, ExecPhraseData *data)
{

}

/*
 * Evaluate tsquery boolean expression using ternary logic.
 *
 * Note: the reason we can't use TS_execute() for this is that its API
 * for the checkcondition callback doesn't allow a MAYBE result to be
 * returned, but we might have MAYBEs in the gcv->check array.
 * Perhaps we should change that API.
 */
static GinTernaryValue
TS_execute_ternary(GinChkVal *gcv, QueryItem *curitem, bool in_phrase)
{






























































































}

Datum
gin_tsquery_consistent(PG_FUNCTION_ARGS)
{































}

Datum
gin_tsquery_triconsistent(PG_FUNCTION_ARGS)
{



































}

/*
 * Formerly, gin_extract_tsvector had only two arguments.  Now it has three,
 * but we still need a pg_proc entry with two args to support reloading
 * pre-9.1 contrib/tsearch2 opclass declarations.  This compatibility
 * function should go away eventually.  (Note: you might say "hey, but the
 * code above is only *using* two args, so let's just declare it that way".
 * If you try that you'll find the opr_sanity regression test complains.)
 */
Datum
gin_extract_tsvector_2args(PG_FUNCTION_ARGS)
{





}

/*
 * Likewise, we need a stub version of gin_extract_tsquery declared with
 * only five arguments.
 */
Datum
gin_extract_tsquery_5args(PG_FUNCTION_ARGS)
{





}

/*
 * Likewise, we need a stub version of gin_tsquery_consistent declared with
 * only six arguments.
 */
Datum
gin_tsquery_consistent_6args(PG_FUNCTION_ARGS)
{





}

/*
 * Likewise, a stub version of gin_extract_tsquery declared with argument
 * types that are no longer considered appropriate.
 */
Datum
gin_extract_tsquery_oldsig(PG_FUNCTION_ARGS)
{

}

/*
 * Likewise, a stub version of gin_tsquery_consistent declared with argument
 * types that are no longer considered appropriate.
 */
Datum
gin_tsquery_consistent_oldsig(PG_FUNCTION_ARGS)
{

}