/*-------------------------------------------------------------------------
 *
 * wparser.c
 *		Standard interface to word parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/wparser.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "tsearch/ts_cache.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/jsonapi.h"
#include "utils/varlena.h"

/******sql-level interface******/

typedef struct
{
  int cur;
  LexDescr *list;
} TSTokenTypeStorage;

/* state for ts_headline_json_* */
typedef struct HeadlineJsonState
{
  HeadlineParsedText *prs;
  TSConfigCacheEntry *cfg;
  TSParserCacheEntry *prsobj;
  TSQuery query;
  List *prsoptions;
  bool transformed;
} HeadlineJsonState;

static text *
headline_json_value(void *_state, char *elem_value, int elem_len);

static void
tt_setup_firstcall(FuncCallContext *funcctx, Oid prsid)
{

























}

static Datum
tt_process_call(FuncCallContext *funcctx)
{





























}

Datum
ts_token_type_byid(PG_FUNCTION_ARGS)
{
















}

Datum
ts_token_type_byname(PG_FUNCTION_ARGS)
{




















}

typedef struct
{
  int type;
  char *lexeme;
} LexemeEntry;

typedef struct
{
  int cur;
  int len;
  LexemeEntry *list;
} PrsStorage;

static void
prs_setup_firstcall(FuncCallContext *funcctx, Oid prsid, text *txt)
{











































}

static Datum
prs_process_call(FuncCallContext *funcctx)
{





























}

Datum
ts_parse_byid(PG_FUNCTION_ARGS)
{



















}

Datum
ts_parse_byname(PG_FUNCTION_ARGS)
{





















}

Datum
ts_headline_byid_opt(PG_FUNCTION_ARGS)
{
















































}

Datum
ts_headline_byid(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_opt(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_jsonb_byid_opt(PG_FUNCTION_ARGS)
{

















































}

Datum
ts_headline_jsonb(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_jsonb_byid(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_jsonb_opt(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_json_byid_opt(PG_FUNCTION_ARGS)
{

















































}

Datum
ts_headline_json(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_json_byid(PG_FUNCTION_ARGS)
{

}

Datum
ts_headline_json_opt(PG_FUNCTION_ARGS)
{

}

/*
 * Return headline in text from, generated from a json(b) element
 */
static text *
headline_json_value(void *_state, char *elem_value, int elem_len)
{














}