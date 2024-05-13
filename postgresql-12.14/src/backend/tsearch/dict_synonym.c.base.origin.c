/*-------------------------------------------------------------------------
 *
 * dict_synonym.c
 *		Synonym dictionary: replace word by its synonym
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/dict_synonym.c
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
  char *in;
  char *out;
  int outlen;
  uint16 flags;
} Syn;

typedef struct
{
  int len; /* length of syn array */
  Syn *syn;
  bool case_sensitive;
} DictSyn;

/*
 * Finds the next whitespace-delimited word within the 'in' string.
 * Returns a pointer to the first character of the word, and a pointer
 * to the next byte after the last character in the word (in *end).
 * Character '*' at the end of word will not be threated as word
 * character if flags is not null.
 */
static char *
findwrd(char *in, char **end, uint16 *flags)
{








































}

static int
compareSyn(const void *a, const void *b)
{

}

Datum
dsynonym_init(PG_FUNCTION_ARGS)
{

















































































































}

Datum
dsynonym_lexize(PG_FUNCTION_ARGS)
{




































}