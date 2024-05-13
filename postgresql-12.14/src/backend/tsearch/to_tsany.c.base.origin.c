/*-------------------------------------------------------------------------
 *
 * to_tsany.c
 *		to_ts* function definitions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/to_tsany.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "tsearch/ts_cache.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/jsonapi.h"

typedef struct MorphOpaque
{
  Oid cfg_id;
  int qoperator; /* query operator */
} MorphOpaque;

typedef struct TSVectorBuildState
{
  ParsedText *prs;
  Oid cfgId;
} TSVectorBuildState;

static void
add_to_tsvector(void *_state, char *elem_value, int elem_len);

Datum
get_current_ts_config(PG_FUNCTION_ARGS)
{

}

/*
 * to_tsvector
 */
static int
compareWORD(const void *a, const void *b)
{















}

static int
uniqueWORD(ParsedWord *a, int32 l)
{











































































}

/*
 * make value of tsvector, given parsed text
 *
 * Note: frees prs->words and subsidiary data.
 */
TSVector
make_tsvector(ParsedText *prs)
{














































































}

Datum
to_tsvector_byid(PG_FUNCTION_ARGS)
{






















}

Datum
to_tsvector(PG_FUNCTION_ARGS)
{





}

/*
 * Worker function for jsonb(_string)_to_tsvector(_byid)
 */
static TSVector
jsonb_to_tsvector_worker(Oid cfgId, Jsonb *jb, uint32 flags)
{











}

Datum
jsonb_string_to_tsvector_byid(PG_FUNCTION_ARGS)
{








}

Datum
jsonb_string_to_tsvector(PG_FUNCTION_ARGS)
{









}

Datum
jsonb_to_tsvector_byid(PG_FUNCTION_ARGS)
{











}

Datum
jsonb_to_tsvector(PG_FUNCTION_ARGS)
{












}

/*
 * Worker function for json(_string)_to_tsvector(_byid)
 */
static TSVector
json_to_tsvector_worker(Oid cfgId, text *json, uint32 flags)
{











}

Datum
json_string_to_tsvector_byid(PG_FUNCTION_ARGS)
{








}

Datum
json_string_to_tsvector(PG_FUNCTION_ARGS)
{









}

Datum
json_to_tsvector_byid(PG_FUNCTION_ARGS)
{











}

Datum
json_to_tsvector(PG_FUNCTION_ARGS)
{












}

/*
 * Parse lexemes in an element of a json(b) value, add to TSVectorBuildState.
 */
static void
add_to_tsvector(void *_state, char *elem_value, int elem_len)
{






























}

/*
 * to_tsquery
 */

/*
 * This function is used for morph parsing.
 *
 * The value is passed to parsetext which will call the right dictionary to
 * lexize the word. If it turns out to be a stopword, we push a QI_VALSTOP
 * to the stack.
 *
 * All words belonging to the same variant are pushed as an ANDed list,
 * and different variants are ORed together.
 */
static void
pushval_morph(Datum opaque, TSQueryParserState state, char *strval, int lenval, int16 weight, bool prefix)
{
















































































}

Datum
to_tsquery_byid(PG_FUNCTION_ARGS)
{










}

Datum
to_tsquery(PG_FUNCTION_ARGS)
{





}

Datum
plainto_tsquery_byid(PG_FUNCTION_ARGS)
{










}

Datum
plainto_tsquery(PG_FUNCTION_ARGS)
{





}

Datum
phraseto_tsquery_byid(PG_FUNCTION_ARGS)
{










}

Datum
phraseto_tsquery(PG_FUNCTION_ARGS)
{





}

Datum
websearch_to_tsquery_byid(PG_FUNCTION_ARGS)
{











}

Datum
websearch_to_tsquery(PG_FUNCTION_ARGS)
{





}