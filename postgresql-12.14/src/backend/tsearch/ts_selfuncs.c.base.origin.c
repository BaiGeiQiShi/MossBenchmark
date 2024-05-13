/*-------------------------------------------------------------------------
 *
 * ts_selfuncs.c
 *	  Selectivity estimation functions for text search operators.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/ts_selfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/nodes.h"
#include "tsearch/ts_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

/*
 * The default text search selectivity is chosen to be small enough to
 * encourage indexscans for typical table densities.  See selfuncs.h and
 * DEFAULT_EQ_SEL for details.
 */
#define DEFAULT_TS_MATCH_SEL 0.005

/* lookup table type for binary searching through MCELEMs */
typedef struct
{
  text *element;
  float4 frequency;
} TextFreq;

/* type of keys for bsearch'ing through an array of TextFreqs */
typedef struct
{
  char *lexeme;
  int length;
} LexemeKey;

static Selectivity
tsquerysel(VariableStatData *vardata, Datum constval);
static Selectivity
mcelem_tsquery_selec(TSQuery query, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers);
static Selectivity
tsquery_opr_selec(QueryItem *item, char *operand, TextFreq *lookup, int length, float4 minfreq);
static int
compare_lexeme_textfreq(const void *e1, const void *e2);

#define tsquery_opr_selec_no_stats(query) tsquery_opr_selec(GETQUERY(query), GETOPERAND(query), NULL, 0, 0)

/*
 *	tsmatchsel -- Selectivity of "@@"
 *
 * restriction selectivity function for tsvector @@ tsquery and
 * tsquery @@ tsvector
 */
Datum
tsmatchsel(PG_FUNCTION_ARGS)
{






























































}

/*
 *	tsmatchjoinsel -- join selectivity of "@@"
 *
 * join selectivity function for tsvector @@ tsquery and tsquery @@ tsvector
 */
Datum
tsmatchjoinsel(PG_FUNCTION_ARGS)
{


}

/*
 * @@ selectivity for tsvector var vs tsquery constant
 */
static Selectivity
tsquerysel(VariableStatData *vardata, Datum constval)
{
















































}

/*
 * Extract data from the pg_statistic arrays into useful format.
 */
static Selectivity
mcelem_tsquery_selec(TSQuery query, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers)
{












































}

/*
 * Traverse the tsquery in preorder, calculating selectivity as:
 *
 *	 selec(left_oper) * selec(right_oper) in AND & PHRASE nodes,
 *
 *	 selec(left_oper) + selec(right_oper) -
 *		selec(left_oper) * selec(right_oper) in OR nodes,
 *
 *	 1 - select(oper) in NOT nodes
 *
 *	 histogram-based estimation in prefix VAL nodes
 *
 *	 freq[val] in exact VAL nodes, if the value is in MCELEM
 *	 min(freq[MCELEM]) / 2 in VAL nodes, if it is not
 *
 * The MCELEM array is already sorted (see ts_typanalyze.c), so we can use
 * binary search for determining freq[MCELEM].
 *
 * If we don't have stats for the tsvector, we still use this logic,
 * except we use default estimates for VAL nodes.  This case is signaled
 * by lookup == NULL.
 */
static Selectivity
tsquery_opr_selec(QueryItem *item, char *operand, TextFreq *lookup, int length, float4 minfreq)
{








































































































































}

/*
 * bsearch() comparator for a lexeme (non-NULL terminated string with length)
 * and a TextFreq. Use length, then byte-for-byte comparison, because that's
 * how ANALYZE code sorted data before storing it in a statistic tuple.
 * See ts_typanalyze.c for details.
 */
static int
compare_lexeme_textfreq(const void *e1, const void *e2)
{



















}