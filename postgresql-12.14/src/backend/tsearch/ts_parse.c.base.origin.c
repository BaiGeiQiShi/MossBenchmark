/*-------------------------------------------------------------------------
 *
 * ts_parse.c
 *		main parse functions for tsearch
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/ts_parse.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "tsearch/ts_cache.h"
#include "tsearch/ts_utils.h"

#define IGNORE_LONGLEXEME 1

/*
 * Lexize subsystem
 */

typedef struct ParsedLex
{
  int type;
  char *lemm;
  int lenlemm;
  struct ParsedLex *next;
} ParsedLex;

typedef struct ListParsedLex
{
  ParsedLex *head;
  ParsedLex *tail;
} ListParsedLex;

typedef struct
{
  TSConfigCacheEntry *cfg;
  Oid curDictId;
  int posDict;
  DictSubState dictState;
  ParsedLex *curSub;
  ListParsedLex towork; /* current list to work */
  ListParsedLex waste;  /* list of lexemes that already lexized */

  /*
   * fields to store last variant to lexize (basically, thesaurus or similar
   * to, which wants	several lexemes
   */

  ParsedLex *lastRes;
  TSLexeme *tmpRes;
} LexizeData;

static void
LexizeInit(LexizeData *ld, TSConfigCacheEntry *cfg)
{







}

static void
LPLAddTail(ListParsedLex *list, ParsedLex *newpl)
{










}

static ParsedLex *
LPLRemoveHead(ListParsedLex *list)
{













}

static void
LexizeAddLemm(LexizeData *ld, int type, char *lemm, int lenlemm)
{







}

static void
RemoveHead(LexizeData *ld)
{



}

static void
setCorrLex(LexizeData *ld, ParsedLex **correspondLexem)
{
















}

static void
moveToWaste(LexizeData *ld, ParsedLex *stop)
{











}

static void
setNewTmpRes(LexizeData *ld, ParsedLex *lex, TSLexeme *res)
{












}

static TSLexeme *
LexizeExec(LexizeData *ld, ParsedLex **correspondLexem)
{














































































































































































}

/*
 * Parse string and lexize words.
 *
 * prs will be filled in.
 */
void
parsetext(Oid cfgId, ParsedText *prs, char *buf, int buflen)
{































































}

/*
 * Headline framework
 */

/* Add a word to prs->words[] */
static void
hladdword(HeadlineParsedText *prs, char *buf, int buflen, int type)
{











}

/*
 * Add pos and matching-query-item data to the just-added word.
 * Here, buf/buflen represent a processed lexeme, not raw token text.
 *
 * If the query contains more than one matching item, we replicate
 * the last-added word so that each item can be pointed to.  The
 * duplicate entries are marked with repeated = 1.
 */
static void
hlfinditem(HeadlineParsedText *prs, TSQuery query, int32 pos, char *buf, int buflen)
{






























}

static void
addHLParsedLex(HeadlineParsedText *prs, TSQuery query, ParsedLex *lexs, TSLexeme *norms)
{










































}

void
hlparsetext(Oid cfgId, HeadlineParsedText *prs, TSQuery query, char *buf, int buflen)
{
















































}

/*
 * Generate the headline, as a text object, from HeadlineParsedText.
 */
text *
generateHeadline(HeadlineParsedText *prs)
{








































































}