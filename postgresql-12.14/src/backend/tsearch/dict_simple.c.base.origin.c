/*-------------------------------------------------------------------------
 *
 * dict_simple.c
 *		Simple dictionary: just lowercase and check for stopword
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/dict_simple.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "commands/defrem.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

typedef struct
{
  StopList stoplist;
  bool accept;
} DictSimple;

Datum
dsimple_init(PG_FUNCTION_ARGS)
{




































}

Datum
dsimple_lexize(PG_FUNCTION_ARGS)
{




























}