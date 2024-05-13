/*-------------------------------------------------------------------------
 *
 * tsvector_parser.c
 *	  Parser for tsvector
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsvector_parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"

/*
 * Private state of tsvector parser.  Note that tsquery also uses this code to
 * parse its input, hence the boolean flags.  The two flags are both true or
 * both false in current usage, but we keep them separate for clarity.
 * is_tsquery affects *only* the content of error messages.
 */
struct TSVectorParseStateData
{
  char *prsbuf;    /* next input character */
  char *bufstart;  /* whole string (used only for errors) */
  char *word;      /* buffer to hold the current word */
  int len;         /* size in bytes allocated for 'word' */
  int eml;         /* max bytes per character */
  bool oprisdelim; /* treat ! | * ( ) as delimiters? */
  bool is_tsquery; /* say "tsquery" not "tsvector" in errors? */
  bool is_web;     /* we're in websearch_to_tsquery() */
};

/*
 * Initializes parser for the input string. If oprisdelim is set, the
 * following characters are treated as delimiters in addition to whitespace:
 * ! | & ( )
 */
TSVectorParseState
init_tsvector_parser(char *input, int flags)
{













}

/*
 * Reinitializes parser to parse 'input', instead of previous input.
 */
void
reset_tsvector_parser(TSVectorParseState state, char *input)
{

}

/*
 * Shuts down a tsvector parser.
 */
void
close_tsvector_parser(TSVectorParseState state)
{


}

/* increase the size of 'word' if needed to hold one more character */
#define RESIZEPRSBUF                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int clen = curpos - state->word;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (clen + state->eml >= state->len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      state->len *= 2;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      state->word = (char *)repalloc(state->word, state->len);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      curpos = state->word + clen;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/* Fills gettoken_tsvector's output parameters, and returns true */
#define RETURN_TOKEN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (pos_ptr != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *pos_ptr = pos;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *poslen = npos;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else if (pos != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      pfree(pos);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (strval != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      *strval = state->word;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (lenval != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      *lenval = curpos - state->word;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    if (endptr != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      *endptr = state->prsbuf;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    return true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

/* State codes used in gettoken_tsvector */
#define WAITWORD 1
#define WAITENDWORD 2
#define WAITNEXTCHAR 3
#define WAITENDCMPLX 4
#define WAITPOSINFO 5
#define INPOSINFO 6
#define WAITPOSDELIM 7
#define WAITCHARCMPLX 8

#define PRSSYNTAXERROR prssyntaxerror(state)

static void
prssyntaxerror(TSVectorParseState state)
{

}

/*
 * Get next token from string being parsed. Returns true if successful,
 * false if end of input string is reached.  On success, these output
 * parameters are filled in:
 *
 * *strval		pointer to token
 * *lenval		length of *strval
 * *pos_ptr		pointer to a palloc'd array of positions and weights
 *				associated with the token. If the caller is not
 *interested in the information, NULL can be supplied. Otherwise the caller is
 *responsible for pfreeing the array. *poslen		number of elements in
 **pos_ptr *endptr		scan resumption point
 *
 * Pass NULL for unwanted output parameters.
 */
bool
gettoken_tsvector(TSVectorParseState state, char **strval, int *lenval, WordEntryPos **pos_ptr, int *poslen, char **endptr)
{


















































































































































































































































}