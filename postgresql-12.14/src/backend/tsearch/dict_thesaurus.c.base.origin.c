/*-------------------------------------------------------------------------
 *
 * dict_thesaurus.c
 *		Thesaurus dictionary: phrase to phrase substitution
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/dict_thesaurus.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/namespace.h"
#include "commands/defrem.h"
#include "tsearch/ts_cache.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/regproc.h"

/*
 * Temporary we use TSLexeme.flags for inner use...
 */
#define DT_USEASIS 0x1000

typedef struct LexemeInfo
{
  uint32 idsubst;    /* entry's number in DictThesaurus->subst */
  uint16 posinsubst; /* pos info in entry */
  uint16 tnvariant;  /* total num lexemes in one variant */
  struct LexemeInfo *nextentry;
  struct LexemeInfo *nextvariant;
} LexemeInfo;

typedef struct
{
  char *lexeme;
  LexemeInfo *entries;
} TheLexeme;

typedef struct
{
  uint16 lastlexeme; /* number lexemes to substitute */
  uint16 reslen;
  TSLexeme *res; /* prepared substituted result */
} TheSubstitute;

typedef struct
{
  /* subdictionary to normalize lexemes */
  Oid subdictOid;
  TSDictionaryCacheEntry *subdict;

  /* Array to search lexeme by exact match */
  TheLexeme *wrds;
  int nwrds;  /* current number of words */
  int ntwrds; /* allocated array length */

  /*
   * Storage of substituted result, n-th element is for n-th expression
   */
  TheSubstitute *subst;
  int nsubst;
} DictThesaurus;

static void
newLexeme(DictThesaurus *d, char *b, char *e, uint32 idsubst, uint16 posinsubst)
{





























}

static void
addWrd(DictThesaurus *d, char *b, char *e, uint32 idsubst, uint16 nwrd, uint16 posinsubst, bool useasis)
{
























































}

#define TR_WAITLEX 1
#define TR_INLEX 2
#define TR_WAITSUBS 3
#define TR_INSUBS 4

static void
thesaurusRead(const char *filename, DictThesaurus *d)
{









































































































































}

static TheLexeme *
addCompiledLexeme(TheLexeme *newwrds, int *nnw, int *tnm, TSLexeme *lexeme, LexemeInfo *src, uint16 tnvariant)
{


























}

static int
cmpLexemeInfo(LexemeInfo *a, LexemeInfo *b)
{





















}

static int
cmpLexeme(const TheLexeme *a, const TheLexeme *b)
{

















}

static int
cmpLexemeQ(const void *a, const void *b)
{

}

static int
cmpTheLexeme(const void *a, const void *b)
{










}

static void
compileTheLexeme(DictThesaurus *d)
{








































































































}

static void
compileTheSubstitute(DictThesaurus *d)
{















































































}

Datum
thesaurus_init(PG_FUNCTION_ARGS)
{



















































}

static LexemeInfo *
findTheLexeme(DictThesaurus *d, char *lexeme)
{

















}

static bool
matchIdSubst(LexemeInfo *stored, uint32 idsubst)
{

















}

static LexemeInfo *
findVariant(LexemeInfo *in, LexemeInfo *stored, uint16 curpos, LexemeInfo **newin, int newn)
{




























































}

static TSLexeme *
copyTSLexeme(TheSubstitute *ts)
{













}

static TSLexeme *
checkMatch(DictThesaurus *d, LexemeInfo *info, uint16 curpos, bool *moreres)
{
















}

Datum
thesaurus_lexize(PG_FUNCTION_ARGS)
{






























































































}