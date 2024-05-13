/*-------------------------------------------------------------------------
 *
 * uuid.c
 *	  Functions for the built-in type "uuid".
 *
 * Copyright (c) 2007-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/uuid.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "port/pg_bswap.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/hashutils.h"
#include "utils/sortsupport.h"
#include "utils/uuid.h"

/* sortsupport for uuid */
typedef struct
{
  int64 input_count; /* number of non-null values seen */
  bool estimating;   /* true if estimating cardinality */

  hyperLogLogState abbr_card; /* cardinality estimator */
} uuid_sortsupport_state;

static void
string_to_uuid(const char *source, pg_uuid_t *uuid);
static int
uuid_internal_cmp(const pg_uuid_t *arg1, const pg_uuid_t *arg2);
static int
uuid_fast_cmp(Datum x, Datum y, SortSupport ssup);
static int
uuid_cmp_abbrev(Datum x, Datum y, SortSupport ssup);
static bool
uuid_abbrev_abort(int memtupcount, SortSupport ssup);
static Datum
uuid_abbrev_convert(Datum original, SortSupport ssup);

Datum
uuid_in(PG_FUNCTION_ARGS)
{






}

Datum
uuid_out(PG_FUNCTION_ARGS)
{





























}

/*
 * We allow UUIDs as a series of 32 hexadecimal digits with an optional dash
 * after each group of 4 hexadecimal digits, and optionally surrounded by {}.
 * (The canonical format 8x-4x-4x-4x-12x, where "nx" means n hexadecimal
 * digits, is the only one used for output.)
 */
static void
string_to_uuid(const char *source, pg_uuid_t *uuid)
{



















































}

Datum
uuid_recv(PG_FUNCTION_ARGS)
{






}

Datum
uuid_send(PG_FUNCTION_ARGS)
{






}

/* internal uuid compare function */
static int
uuid_internal_cmp(const pg_uuid_t *arg1, const pg_uuid_t *arg2)
{

}

Datum
uuid_lt(PG_FUNCTION_ARGS)
{




}

Datum
uuid_le(PG_FUNCTION_ARGS)
{




}

Datum
uuid_eq(PG_FUNCTION_ARGS)
{




}

Datum
uuid_ge(PG_FUNCTION_ARGS)
{




}

Datum
uuid_gt(PG_FUNCTION_ARGS)
{




}

Datum
uuid_ne(PG_FUNCTION_ARGS)
{




}

/* handler for btree index operator */
Datum
uuid_cmp(PG_FUNCTION_ARGS)
{




}

/*
 * Sort support strategy routine
 */
Datum
uuid_sortsupport(PG_FUNCTION_ARGS)
{




























}

/*
 * SortSupport comparison func
 */
static int
uuid_fast_cmp(Datum x, Datum y, SortSupport ssup)
{




}

/*
 * Abbreviated key comparison func
 */
static int
uuid_cmp_abbrev(Datum x, Datum y, SortSupport ssup)
{












}

/*
 * Callback for estimating effectiveness of abbreviated key optimization.
 *
 * We pay no attention to the cardinality of the non-abbreviated data, because
 * there is no equality fast-path within authoritative uuid comparator.
 */
static bool
uuid_abbrev_abort(int memtupcount, SortSupport ssup)
{





















































}

/*
 * Conversion routine for sortsupport.  Converts original uuid representation
 * to abbreviated key representation.  Our encoding strategy is simple -- pack
 * the first `sizeof(Datum)` bytes of uuid data into a Datum (on little-endian
 * machines, the bytes are stored in reverse order), and treat it as an
 * unsigned integer.
 */
static Datum
uuid_abbrev_convert(Datum original, SortSupport ssup)
{































}

/* hash index support */
Datum
uuid_hash(PG_FUNCTION_ARGS)
{



}

Datum
uuid_hash_extended(PG_FUNCTION_ARGS)
{



}