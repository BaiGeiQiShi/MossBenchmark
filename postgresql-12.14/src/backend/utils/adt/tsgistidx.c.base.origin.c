/*-------------------------------------------------------------------------
 *
 * tsgistidx.c
 *	  GiST support functions for tsvector_ops
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsgistidx.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gist.h"
#include "access/tuptoaster.h"
#include "port/pg_bitutils.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/pg_crc.h"

#define SIGLENINT                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  31 /* >121 => key will toast, so it will not work                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      * !!! */

#define SIGLEN (sizeof(int32) * SIGLENINT)
#define SIGLENBIT (SIGLEN * BITS_PER_BYTE)

typedef char BITVEC[SIGLEN];
typedef char *BITVECP;

#define LOOPBYTE for (i = 0; i < SIGLEN; i++)

#define GETBYTE(x, i) (*((BITVECP)(x) + (int)((i) / BITS_PER_BYTE)))
#define GETBITBYTE(x, i) (((char)(x)) >> (i) & 0x01)
#define CLRBIT(x, i) GETBYTE(x, i) &= ~(0x01 << ((i) % BITS_PER_BYTE))
#define SETBIT(x, i) GETBYTE(x, i) |= (0x01 << ((i) % BITS_PER_BYTE))
#define GETBIT(x, i) ((GETBYTE(x, i) >> ((i) % BITS_PER_BYTE)) & 0x01)

#define HASHVAL(val) (((unsigned int)(val)) % SIGLENBIT)
#define HASH(sign, val) SETBIT((sign), HASHVAL(val))

#define GETENTRY(vec, pos) ((SignTSVector *)DatumGetPointer((vec)->vector[(pos)].key))

/*
 * type of GiST index key
 */

typedef struct
{
  int32 vl_len_; /* varlena header (do not touch directly!) */
  int32 flag;
  char data[FLEXIBLE_ARRAY_MEMBER];
} SignTSVector;

#define ARRKEY 0x01
#define SIGNKEY 0x02
#define ALLISTRUE 0x04

#define ISARRKEY(x) (((SignTSVector *)(x))->flag & ARRKEY)
#define ISSIGNKEY(x) (((SignTSVector *)(x))->flag & SIGNKEY)
#define ISALLTRUE(x) (((SignTSVector *)(x))->flag & ALLISTRUE)

#define GTHDRSIZE (VARHDRSZ + sizeof(int32))
#define CALCGTSIZE(flag, len) (GTHDRSIZE + (((flag) & ARRKEY) ? ((len) * sizeof(int32)) : (((flag) & ALLISTRUE) ? 0 : SIGLEN)))

#define GETSIGN(x) ((BITVECP)((char *)(x) + GTHDRSIZE))
#define GETARR(x) ((int32 *)((char *)(x) + GTHDRSIZE))
#define ARRNELEM(x) ((VARSIZE(x) - GTHDRSIZE) / sizeof(int32))

static int32
sizebitvec(BITVECP sign);

Datum
gtsvectorin(PG_FUNCTION_ARGS)
{


}

#define SINGOUTSTR "%d true bits, %d false bits"
#define ARROUTSTR "%d unique words"
#define EXTRALEN (2 * 13)

static int outbuf_maxlen = 0;

Datum
gtsvectorout(PG_FUNCTION_ARGS)
{






















}

static int
compareint(const void *va, const void *vb)
{








}

/*
 * Removes duplicates from an array of int32. 'l' is
 * size of the input array. Returns the new size of the array.
 */
static int
uniqueint(int32 *a, int32 l)
{























}

static void
makesign(BITVECP sign, SignTSVector *a)
{








}

Datum
gtsvector_compress(PG_FUNCTION_ARGS)
{


















































































}

Datum
gtsvector_decompress(PG_FUNCTION_ARGS)
{

















}

typedef struct
{
  int32 *arrb;
  int32 *arre;
} CHKVAL;

/*
 * is there value 'val' in array or not ?
 */
static bool
checkcondition_arr(void *checkval, QueryOperand *val, ExecPhraseData *data)
{
































}

static bool
checkcondition_bit(void *checkval, QueryOperand *val, ExecPhraseData *data)
{








}

Datum
gtsvector_consistent(PG_FUNCTION_ARGS)
{


































}

static int32
unionkey(BITVECP sbase, SignTSVector *add)
{
























}

Datum
gtsvector_union(PG_FUNCTION_ARGS)
{





























}

Datum
gtsvector_same(PG_FUNCTION_ARGS)
{




























































}

static int32
sizebitvec(BITVECP sign)
{

}

static int
hemdistsign(BITVECP a, BITVECP b)
{









}

static int
hemdist(SignTSVector *a, SignTSVector *b)
{

















}

Datum
gtsvector_penalty(PG_FUNCTION_ARGS)
{





























}

typedef struct
{
  bool allistrue;
  BITVEC sign;
} CACHESIGN;

static void
fillcache(CACHESIGN *item, SignTSVector *key)
{













}

#define WISH_F(a, b, c) (double)(-(double)(((a) - (b)) * ((a) - (b)) * ((a) - (b))) * (c))
typedef struct
{
  OffsetNumber pos;
  int32 cost;
} SPLITCOST;

static int
comparecost(const void *va, const void *vb)
{











}

static int
hemdistcache(CACHESIGN *a, CACHESIGN *b)
{

















}

Datum
gtsvector_picksplit(PG_FUNCTION_ARGS)
{




























































































































































































}

/*
 * Formerly, gtsvector_consistent was declared in pg_proc.h with arguments
 * that did not match the documented conventions for GiST support functions.
 * We fixed that, but we still need a pg_proc entry with the old signature
 * to support reloading pre-9.6 contrib/tsearch2 opclass declarations.
 * This compatibility function should go away eventually.
 */
Datum
gtsvector_consistent_oldsig(PG_FUNCTION_ARGS)
{

}