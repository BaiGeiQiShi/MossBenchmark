/*-------------------------------------------------------------------------
 *
 * tsrank.c
 *		rank tsvector by tsquery
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsrank.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "tsearch/ts_utils.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "miscadmin.h"

static const float weights[] = {0.1f, 0.2f, 0.4f, 1.0f};

#define wpos(wep) (w[WEP_GETWEIGHT(wep)])

#define RANK_NO_NORM 0x00
#define RANK_NORM_LOGLENGTH 0x01
#define RANK_NORM_LENGTH 0x02
#define RANK_NORM_EXTDIST 0x04
#define RANK_NORM_UNIQ 0x08
#define RANK_NORM_LOGUNIQ 0x10
#define RANK_NORM_RDIVRPLUS1 0x20
#define DEF_NORM_METHOD RANK_NO_NORM

static float
calc_rank_or(const float *w, TSVector t, TSQuery q);
static float
calc_rank_and(const float *w, TSVector t, TSQuery q);

/*
 * Returns a weight of a word collocation
 */
static float4
word_distance(int32 w)
{






}

static int
cnt_length(TSVector t)
{




















}

#define WordECompareQueryItem(e, q, p, i, m) tsCompareString((q) + (i)->distance, (i)->length, (e) + (p)->pos, (p)->len, (m))

/*
 * Returns a pointer to a WordEntry's array corresponding to 'item' from
 * tsvector 't'. 'q' is the TSQuery containing 'item'.
 * Returns NULL if not found.
 */
static WordEntry *
find_wordentry(TSVector t, TSQuery q, QueryOperand *item, int32 *nitem)
{













































}

/*
 * sort QueryOperands by (length, word)
 */
static int
compareQueryOperand(const void *a, const void *b, void *arg)
{





}

/*
 * Returns a sorted, de-duplicated array of QueryOperands in a query.
 * The returned QueryOperands are pointers to the original QueryOperands
 * in the query.
 *
 * Length of the returned array is stored in *size
 */
static QueryOperand **
SortAndUniqItems(TSQuery q, int *size)
{









































}

static float
calc_rank_and(const float *w, TSVector t, TSQuery q)
{
















































































}

static float
calc_rank_or(const float *w, TSVector t, TSQuery q)
{




































































}

static float
calc_rank(const float *w, TSVector t, TSQuery q, int32 method)
{

















































}

static const float *
getWeights(ArrayType *win)
{



































}

Datum
ts_rank_wttf(PG_FUNCTION_ARGS)
{












}

Datum
ts_rank_wtt(PG_FUNCTION_ARGS)
{











}

Datum
ts_rank_ttf(PG_FUNCTION_ARGS)
{










}

Datum
ts_rank_tt(PG_FUNCTION_ARGS)
{









}

typedef struct
{
  union
  {
    struct
    { /* compiled doc representation */
      QueryItem **items;
      int16 nitem;
    } query;
    struct
    { /* struct is used for preparing doc
       * representation */
      QueryItem *item;
      WordEntry *entry;
    } map;
  } data;
  WordEntryPos pos;
} DocRepresentation;

static int
compareDocR(const void *va, const void *vb)
{



















}

#define MAXQROPOS MAXENTRYPOS
typedef struct
{
  bool operandexists;
  bool reverseinsert; /* indicates insert order, true means
                       * descending order */
  uint32 npos;
  WordEntryPos pos[MAXQROPOS];
} QueryRepresentationOperand;

typedef struct
{
  TSQuery query;
  QueryRepresentationOperand *operandData;
} QueryRepresentation;

#define QR_GET_OPERAND_DATA(q, v) ((q)->operandData + (((QueryItem *)(v)) - GETQUERY((q)->query)))

static bool
checkcondition_QueryOperand(void *checkval, QueryOperand *val, ExecPhraseData *data)
{



















}

typedef struct
{
  int pos;
  int p;
  int q;
  DocRepresentation *begin;
  DocRepresentation *end;
} CoverExt;

static void
resetQueryRepresentation(QueryRepresentation *qr, bool reverseinsert)
{








}

static void
fillQueryRepresentationData(QueryRepresentation *qr, DocRepresentation *entry)
{

































}

static bool
Cover(DocRepresentation *doc, int len, QueryRepresentation *qr, CoverExt *ext)
{












































































}

static DocRepresentation *
get_docrep(TSVector txt, QueryRepresentation *qr, int *doclen)
{


















































































































}

static float4
calc_rank_cd(const float4 *arrdata, TSVector txt, TSQuery query, int method)
{












































































































}

Datum
ts_rankcd_wttf(PG_FUNCTION_ARGS)
{












}

Datum
ts_rankcd_wtt(PG_FUNCTION_ARGS)
{











}

Datum
ts_rankcd_ttf(PG_FUNCTION_ARGS)
{










}

Datum
ts_rankcd_tt(PG_FUNCTION_ARGS)
{









}