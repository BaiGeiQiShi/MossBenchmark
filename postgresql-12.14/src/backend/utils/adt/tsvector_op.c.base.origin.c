/*-------------------------------------------------------------------------
 *
 * tsvector_op.c
 *	  operations over tsvector
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsvector_op.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "parser/parse_coerce.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/regproc.h"
#include "utils/rel.h"

typedef struct
{
  WordEntry *arrb;
  WordEntry *arre;
  char *values;
  char *operand;
} CHKVAL;

typedef struct StatEntry
{
  uint32 ndoc; /* zero indicates that we were already here
                * while walking through the tree */
  uint32 nentry;
  struct StatEntry *left;
  struct StatEntry *right;
  uint32 lenlexeme;
  char lexeme[FLEXIBLE_ARRAY_MEMBER];
} StatEntry;

#define STATENTRYHDRSZ (offsetof(StatEntry, lexeme))

typedef struct
{
  int32 weight;

  uint32 maxdepth;

  StatEntry **stack;
  uint32 stackpos;

  StatEntry *root;
} TSVectorStat;

/* TS_execute requires ternary logic to handle NOT with phrase matches */
typedef enum
{
  TS_NO,   /* definitely no match */
  TS_YES,  /* definitely does match */
  TS_MAYBE /* can't verify match for lack of pos data */
} TSTernaryValue;

static TSTernaryValue
TS_execute_recurse(QueryItem *curitem, void *arg, uint32 flags, TSExecuteCallback chkcond);
static int
tsvector_bsearch(const TSVector tsv, char *lexeme, int lexeme_len);
static Datum
tsvector_update_trigger(PG_FUNCTION_ARGS, bool config_column);

/*
 * Order: haspos, len, word, for all positions (pos, weight)
 */
static int
silly_cmp_tsvector(const TSVector a, const TSVector b)
{
































































}

#define TSVECTORCMPFUNC(type, action, ret)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  Datum tsvector_##type(PG_FUNCTION_ARGS)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    TSVector a = PG_GETARG_TSVECTOR(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    TSVector b = PG_GETARG_TSVECTOR(1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    int res = silly_cmp_tsvector(a, b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    PG_FREE_IF_COPY(a, 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    PG_FREE_IF_COPY(b, 1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    PG_RETURN_##ret(res action 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  /* keep compiler quiet - no extra ; */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  extern int no_such_variable

TSVECTORCMPFUNC(lt, <, BOOL);
TSVECTORCMPFUNC(le, <=, BOOL);
TSVECTORCMPFUNC(eq, ==, BOOL);
TSVECTORCMPFUNC(ge, >=, BOOL);
TSVECTORCMPFUNC(gt, >, BOOL);
TSVECTORCMPFUNC(ne, !=, BOOL);
TSVECTORCMPFUNC(cmp, +, INT32);

Datum
tsvector_strip(PG_FUNCTION_ARGS)
{




























}

Datum
tsvector_length(PG_FUNCTION_ARGS)
{





}

Datum
tsvector_setweight(PG_FUNCTION_ARGS)
{



















































}

/*
 * setweight(tsin tsvector, char_weight "char", lexemes "text"[])
 *
 * Assign weight w to elements of tsin that are listed in lexemes.
 */
Datum
tsvector_setweight_by_filter(PG_FUNCTION_ARGS)
{










































































}

#define compareEntry(pa, a, pb, b) tsCompareString((pa) + (a)->pos, (a)->len, (pb) + (b)->pos, (b)->len, false)

/*
 * Add positions from src to dest after offsetting them by maxpos.
 * Return the number added (might be less than expected due to overflow)
 */
static int32
add_pos(TSVector src, WordEntry *srcptr, TSVector dest, WordEntry *destptr, int32 maxpos)
{























}

/*
 * Perform binary search of given lexeme in TSVector.
 * Returns lexeme position in TSVector's entry array or -1 if lexeme wasn't
 * found.
 */
static int
tsvector_bsearch(const TSVector tsv, char *lexeme, int lexeme_len)
{
























}

/*
 * qsort comparator functions
 */

static int
compare_int(const void *va, const void *vb)
{








}

static int
compare_text_lexemes(const void *va, const void *vb)
{








}

/*
 * Internal routine to delete lexemes from TSVector by array of offsets.
 *
 * int *indices_to_delete -- array of lexeme offsets to delete (modified here!)
 * int indices_count -- size of that array
 *
 * Returns new TSVector without given lexemes along with their positions
 * and weights.
 */
static TSVector
tsvector_delete_by_indices(TSVector tsv, int *indices_to_delete, int indices_count)
{




















































































}

/*
 * Delete given lexeme from tsvector.
 * Implementation of user-level ts_delete(tsvector, text).
 */
Datum
tsvector_delete_str(PG_FUNCTION_ARGS)
{















}

/*
 * Delete given array of lexemes from tsvector.
 * Implementation of user-level ts_delete(tsvector, text[]).
 */
Datum
tsvector_delete_arr(PG_FUNCTION_ARGS)
{









































}

/*
 * Expand tsvector as table with following columns:
 *	   lexeme: lexeme text
 *	   positions: integer array of lexeme positions
 *	   weights: char array of weights corresponding to positions
 */
Datum
tsvector_unnest(PG_FUNCTION_ARGS)
{










































































}

/*
 * Convert tsvector to array of lexemes.
 */
Datum
tsvector_to_array(PG_FUNCTION_ARGS)
{


















}

/*
 * Build tsvector from array of lexemes.
 */
Datum
array_to_tsvector(PG_FUNCTION_ARGS)
{






























































}

/*
 * ts_filter(): keep only lexemes with given weights in tsvector.
 */
Datum
tsvector_filter(PG_FUNCTION_ARGS)
{




































































































}

Datum
tsvector_concat(PG_FUNCTION_ARGS)
{



























































































































































































































}

/*
 * Compare two strings by tsvector rules.
 *
 * if isPrefix = true then it returns zero value iff b has prefix a
 */
int32
tsCompareString(char *a, int lena, char *b, int lenb, bool prefix)
{



































}

/*
 * Check weight info or/and fill 'data' with the required positions
 */
static bool
checkclass_str(CHKVAL *chkval, WordEntry *entry, QueryOperand *val, ExecPhraseData *data)
{









































































}

/*
 * Removes duplicate pos entries. We can't use uniquePos() from
 * tsvector.c because array might be longer than MAXENTRYPOS
 *
 * Returns new length.
 */
static int
uniqueLongPos(WordEntryPos *pos, int npos)
{























}

/*
 * is there value 'val' in array or not ?
 */
static bool
checkcondition_str(void *checkval, QueryOperand *val, ExecPhraseData *data)
{
































































































}

/*
 * Compute output position list for a tsquery operator in phrase mode.
 *
 * Merge the position lists in Ldata and Rdata as specified by "emit",
 * returning the result list into *data.  The input position lists must be
 * sorted and unique, and the output will be as well.
 *
 * data: pointer to initially-all-zeroes output struct, or NULL
 * Ldata, Rdata: input position lists
 * emit: bitmask of TSPO_XXX flags
 * Loffset: offset to be added to Ldata positions before comparing/outputting
 * Roffset: offset to be added to Rdata positions before comparing/outputting
 * max_npos: maximum possible required size of output position array
 *
 * Loffset and Roffset should not be negative, else we risk trying to output
 * negative positions, which won't fit into WordEntryPos.
 *
 * The result is boolean (TS_YES or TS_NO), but for the caller's convenience
 * we return it as TSTernaryValue.
 *
 * Returns TS_YES if any positions were emitted to *data; or if data is NULL,
 * returns TS_YES if any positions would have been emitted.
 */
#define TSPO_L_ONLY 0x01 /* emit positions appearing only in L */
#define TSPO_R_ONLY 0x02 /* emit positions appearing only in R */
#define TSPO_BOTH 0x04   /* emit positions appearing in both L&R */

static TSTernaryValue
TS_phrase_output(ExecPhraseData *data, ExecPhraseData *Ldata, ExecPhraseData *Rdata, int emit, int Loffset, int Roffset, int max_npos)
{




































































































}

/*
 * Execute tsquery at or below an OP_PHRASE operator.
 *
 * This handles tsquery execution at recursion levels where we need to care
 * about match locations.
 *
 * In addition to the same arguments used for TS_execute, the caller may pass
 * a preinitialized-to-zeroes ExecPhraseData struct, to be filled with lexeme
 * match position info on success.  data == NULL if no position data need be
 * returned.  (In practice, outside callers pass NULL, and only the internal
 * recursion cases pass a data pointer.)
 * Note: the function assumes data != NULL for operators other than OP_PHRASE.
 * This is OK because an outside call always starts from an OP_PHRASE node.
 *
 * The detailed semantics of the match data, given that the function returned
 * TS_YES (successful match), are:
 *
 * npos > 0, negate = false:
 *	 query is matched at specified position(s) (and only those positions)
 * npos > 0, negate = true:
 *	 query is matched at all positions *except* specified position(s)
 * npos = 0, negate = true:
 *	 query is matched at all positions
 * npos = 0, negate = false:
 *	 disallowed (this should result in TS_NO or TS_MAYBE, as appropriate)
 *
 * Successful matches also return a "width" value which is the match width in
 * lexemes, less one.  Hence, "width" is zero for simple one-lexeme matches,
 * and is the sum of the phrase operator distances for phrase matches.  Note
 * that when width > 0, the listed positions represent the ends of matches not
 * the starts.  (This unintuitive rule is needed to avoid possibly generating
 * negative positions, which wouldn't fit into the WordEntryPos arrays.)
 *
 * If the TSExecuteCallback function reports that an operand is present
 * but fails to provide position(s) for it, we will return TS_MAYBE when
 * it is possible but not certain that the query is matched.
 *
 * When the function returns TS_NO or TS_MAYBE, it must return npos = 0,
 * negate = false (which is the state initialized by the caller); but the
 * "width" output in such cases is undefined.
 */
static TSTernaryValue
TS_phrase_execute(QueryItem *curitem, void *arg, uint32 flags, TSExecuteCallback chkcond, ExecPhraseData *data)
{



































































































































































































































}

/*
 * Evaluate tsquery boolean expression.
 *
 * curitem: current tsquery item (initially, the first one)
 * arg: opaque value to pass through to callback function
 * flags: bitmask of flag bits shown in ts_utils.h
 * chkcond: callback function to check whether a primitive value is present
 */
bool
TS_execute(QueryItem *curitem, void *arg, uint32 flags, TSExecuteCallback chkcond)
{






}

/*
 * TS_execute recursion for operators above any phrase operator.  Here we do
 * not need to worry about lexeme positions.  As soon as we hit an OP_PHRASE
 * operator, we pass it off to TS_phrase_execute which does worry.
 */
static TSTernaryValue
TS_execute_recurse(QueryItem *curitem, void *arg, uint32 flags, TSExecuteCallback chkcond)
{





























































































}

/*
 * Detect whether a tsquery boolean expression requires any positive matches
 * to values shown in the tsquery.
 *
 * This is needed to know whether a GIN index search requires full index scan.
 * For example, 'x & !y' requires a match of x, so it's sufficient to scan
 * entries for x; but 'x | !y' could match rows containing neither x nor y.
 */
bool
tsquery_requires_match(QueryItem *curitem)
{




















































}

/*
 * boolean operations
 */
Datum
ts_match_qv(PG_FUNCTION_ARGS)
{

}

Datum
ts_match_vq(PG_FUNCTION_ARGS)
{






















}

Datum
ts_match_tt(PG_FUNCTION_ARGS)
{













}

Datum
ts_match_tq(PG_FUNCTION_ARGS)
{












}

/*
 * ts_stat statistic function support
 */

/*
 * Returns the number of positions in value 'wptr' within tsvector 'txt',
 * that have a weight equal to one of the weights in 'weight' bitmask.
 */
static int
check_weight(TSVector txt, WordEntry *wptr, int8 weight)
{













}

#define compareStatWord(a, e, t) tsCompareString((a)->lexeme, (a)->lenlexeme, STRPTR(t) + (e)->pos, (e)->len, false)

static void
insertStatEntry(MemoryContext persistentContext, TSVectorStat *stat, TSVector txt, uint32 off)
{






































































}

static void
chooseNextStatEntry(MemoryContext persistentContext, TSVectorStat *stat, TSVector txt, uint32 low, uint32 high, uint32 offset)
{






















}

/*
 * This is written like a custom aggregate function, because the
 * original plan was to do just that. Unfortunately, an aggregate function
 * can't return a set, so that plan was abandoned. If that limitation is
 * lifted in the future, ts_stat could be a real aggregate function so that
 * you could use it like this:
 *
 *	 SELECT ts_stat(vector_column) FROM vector_table;
 *
 *	where vector_column is a tsvector-type column in vector_table.
 */

static TSVectorStat *
ts_accum(MemoryContext persistentContext, TSVectorStat *stat, Datum data)
{
































}

static void
ts_setup_firstcall(FunctionCallInfo fcinfo, FuncCallContext *funcctx, TSVectorStat *stat)
{











































}

static StatEntry *
walkStatEntryTree(TSVectorStat *stat)
{















































}

static Datum
ts_process_call(FuncCallContext *funcctx)
{



































}

static TSVectorStat *
ts_stat_sql(MemoryContext persistentContext, text *txt, text *ws)
{























































































}

Datum
ts_stat1(PG_FUNCTION_ARGS)
{






















}

Datum
ts_stat2(PG_FUNCTION_ARGS)
{
























}

/*
 * Triggers for automatic update of a tsvector column from text column(s)
 *
 * Trigger arguments are either
 *		name of tsvector col, name of tsconfig to use, name(s) of text
 *col(s) name of tsvector col, name of regconfig col, name(s) of text col(s) ie,
 *tsconfig can either be specified by name, or indirectly as the contents of a
 *regconfig field in the row.  If the name is used, it must be explicitly
 *schema-qualified.
 */
Datum
tsvector_update_trigger_byid(PG_FUNCTION_ARGS)
{

}

Datum
tsvector_update_trigger_bycolumn(PG_FUNCTION_ARGS)
{

}

static Datum
tsvector_update_trigger(PG_FUNCTION_ARGS, bool config_column)
{














































































































































}