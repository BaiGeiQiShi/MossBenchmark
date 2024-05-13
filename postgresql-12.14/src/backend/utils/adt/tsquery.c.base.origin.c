/*-------------------------------------------------------------------------
 *
 * tsquery.c
 *	  I/O functions for tsquery
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsquery.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "tsearch/ts_type.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/pg_crc.h"

/* FTS operator priorities, see ts_type.h */
const int tsearch_op_priority[OP_COUNT] = {
    4, /* OP_NOT */
    2, /* OP_AND */
    1, /* OP_OR */
    3  /* OP_PHRASE */
};

/*
 * parser's states
 */
typedef enum
{
  WAITOPERAND = 1,
  WAITOPERATOR = 2,
  WAITFIRSTOPERAND = 3
} ts_parserstate;

/*
 * token types for parsing
 */
typedef enum
{
  PT_END = 0,
  PT_ERR = 1,
  PT_VAL = 2,
  PT_OPR = 3,
  PT_OPEN = 4,
  PT_CLOSE = 5
} ts_tokentype;

/*
 * get token from query string
 *
 * *operator is filled in with OP_* when return values is PT_OPR,
 * but *weight could contain a distance value in case of phrase operator.
 * *strval, *lenval and *weight are filled in when return value is PT_VAL
 *
 */
typedef ts_tokentype (*ts_tokenizer)(TSQueryParserState state, int8 *operator, int * lenval, char **strval, int16 *weight, bool *prefix);

struct TSQueryParserStateData
{
  /* Tokenizer used for parsing tsquery */
  ts_tokenizer gettoken;

  /* State of tokenizer function */
  char *buffer;   /* entire string we are scanning */
  char *buf;      /* current scan point */
  int count;      /* nesting count, incremented by (,
                   * decremented by ) */
  bool in_quotes; /* phrase in quotes "" */
  ts_parserstate state;

  /* polish (prefix) notation in list, filled in by push* functions */
  List *polstr;

  /*
   * Strings from operands are collected in op. curop is a pointer to the
   * end of used space of op.
   */
  char *op;
  char *curop;
  int lenop;  /* allocated size of op */
  int sumlen; /* used size of op */

  /* state for value's parser */
  TSVectorParseState valstate;
};

/*
 * subroutine to parse the modifiers (weight and prefix flag currently)
 * part, like ':AB*' of a query.
 */
static char *
get_modifiers(char *buf, int16 *weight, bool *prefix)
{







































}

/*
 * Parse phrase operator. The operator
 * may take the following forms:
 *
 *		a <N> b (distance is exactly N lexemes)
 *		a <-> b (default distance = 1)
 *
 * The buffer should begin with '<' char
 */
static bool
parse_phrase_operator(TSQueryParserState pstate, int16 *distance)
{













































































}

/*
 * Parse OR operator used in websearch_to_tsquery(), returns true if we
 * believe that "OR" literal could be an operator OR
 */
static bool
parse_or_operator(TSQueryParserState pstate)
{




















































}

static ts_tokentype
gettoken_query_standard(TSQueryParserState state, int8 *operator, int * lenval, char **strval, int16 *weight, bool *prefix)
{




























































































}

static ts_tokentype
gettoken_query_websearch(TSQueryParserState state, int8 *operator, int * lenval, char **strval, int16 *weight, bool *prefix)
{







































































































































}

static ts_tokentype
gettoken_query_plain(TSQueryParserState state, int8 *operator, int * lenval, char **strval, int16 *weight, bool *prefix)
{













}

/*
 * Push an operator to state->polstr
 */
void
pushOperator(TSQueryParserState state, int8 oper, int16 distance)
{











}

static void
pushValue_internal(TSQueryParserState state, pg_crc32 valcrc, int distance, int lenval, int weight, bool prefix)
{




















}

/*
 * Push an operand to state->polstr.
 *
 * strval must point to a string equal to state->curop. lenval is the length
 * of the string.
 */
void
pushValue(TSQueryParserState state, char *strval, int lenval, int16 weight, bool prefix)
{


























}

/*
 * Push a stopword placeholder to state->polstr
 */
void
pushStop(TSQueryParserState state)
{






}

#define STACKDEPTH 32

typedef struct OperatorElement
{
  int8 op;
  int16 distance;
} OperatorElement;

static void
pushOpStack(OperatorElement *stack, int *lenstack, int8 op, int16 distance)
{









}

static void
cleanOpStack(TSQueryParserState state, OperatorElement *stack, int *lenstack, int8 op)
{













}

/*
 * Make polish (prefix) notation of query.
 *
 * See parse_tsquery for explanation of pushval.
 */
static void
makepol(TSQueryParserState state, PushFunction pushval, Datum opaque)
{




































}

static void
findoprnd_recurse(QueryItem *ptr, uint32 *pos, int nnodes, bool *needcleanup)
{















































}

/*
 * Fill in the left-fields previously left unfilled.
 * The input QueryItems must be in polish (prefix) notation.
 * Also, set *needcleanup to true if there are any QI_VALSTOP nodes.
 */
static void
findoprnd(QueryItem *ptr, int size, bool *needcleanup)
{










}

/*
 * Each value (operand) in the query is passed to pushval. pushval can
 * transform the simple value to an arbitrarily complex expression using
 * pushValue and pushOperator. It must push a single value with pushValue,
 * a complete expression with all operands, or a stopword placeholder
 * with pushStop, otherwise the prefix notation representation will be broken,
 * having an operator with no operand.
 *
 * opaque is passed on to pushval as is, pushval can use it to store its
 * private state.
 */
TSQuery
parse_tsquery(char *buf, PushFunction pushval, Datum opaque, int flags)
{
















































































































}

static void
pushval_asis(Datum opaque, TSQueryParserState state, char *strval, int lenval, int16 weight, bool prefix)
{

}

/*
 * in without morphology
 */
Datum
tsqueryin(PG_FUNCTION_ARGS)
{



}

/*
 * out function
 */
typedef struct
{
  QueryItem *curpol;
  char *buf;
  char *cur;
  char *op;
  int buflen;
} INFIX;

/* Makes sure inf->buf is large enough for adding 'addsize' bytes */
#define RESIZEBUF(inf, addsize)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  while (((inf)->cur - (inf)->buf) + (addsize) + 1 >= (inf)->buflen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int len = (inf)->cur - (inf)->buf;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    (inf)->buflen *= 2;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    (inf)->buf = (char *)repalloc((void *)(inf)->buf, (inf)->buflen);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    (inf)->cur = (inf)->buf + len;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  }

/*
 * recursively traverse the tree and
 * print it in infix (human-readable) form
 */
static void
infix(INFIX *in, int parentPriority, bool rightPhraseOp)
{


























































































































































}

Datum
tsqueryout(PG_FUNCTION_ARGS)
{



















}

/*
 * Binary Input / Output functions. The binary format is as follows:
 *
 * uint32	 number of operators/operands in the query
 *
 * Followed by the operators and operands, in prefix notation. For each
 * operand:
 *
 * uint8	type, QI_VAL
 * uint8	weight
 *			operand text in client encoding, null-terminated
 * uint8	prefix
 *
 * For each operator:
 * uint8	type, QI_OPR
 * uint8	operator, one of OP_AND, OP_PHRASE OP_OR, OP_NOT.
 * uint16	distance (only for OP_PHRASE)
 */
Datum
tsquerysend(PG_FUNCTION_ARGS)
{



































}

Datum
tsqueryrecv(PG_FUNCTION_ARGS)
{













































































































































}

/*
 * debug function, used only for view query
 * which will be executed in non-leaf pages in index
 */
Datum
tsquerytree(PG_FUNCTION_ARGS)
{


































}