/*-------------------------------------------------------------------------
 *
 * ginutil.c
 *	  Utility routines for the Postgres inverted index access method.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginutil.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/reloptions.h"
#include "access/xloginsert.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/typcache.h"

/*
 * GIN handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
ginhandler(PG_FUNCTION_ARGS)
{










































}

/*
 * initGinState: fill in an empty GinState struct to describe the index
 *
 * Note: assorted subsidiary data is allocated in the CurrentMemoryContext.
 */
void
initGinState(GinState *state, Relation index)
{







































































































}

/*
 * Extract attribute (column) number of stored entry from GIN tuple
 */
OffsetNumber
gintuple_get_attrnum(GinState *ginstate, IndexTuple tuple)
{
























}

/*
 * Extract stored datum (and possible null category) from GIN tuple
 */
Datum
gintuple_get_key(GinState *ginstate, IndexTuple tuple, GinNullCategory *category)
{































}

/*
 * Allocate a new page (either by recycling, or by extending the index file)
 * The returned buffer is already pinned and exclusive-locked
 * Caller is responsible for initializing the page by calling GinInitBuffer
 */
Buffer
GinNewBuffer(Relation index)
{

















































}

void
GinInitPage(Page page, uint32 f, Size pageSize)
{








}

void
GinInitBuffer(Buffer b, uint32 f)
{

}

void
GinInitMetabuffer(Buffer b)
{























}

/*
 * Compare two keys of the same index column
 */
int
ginCompareEntries(GinState *ginstate, OffsetNumber attnum, Datum a, GinNullCategory categorya, Datum b, GinNullCategory categoryb)
{














}

/*
 * Compare two keys of possibly different index columns
 */
int
ginCompareAttEntries(GinState *ginstate, OffsetNumber attnuma, Datum a, GinNullCategory categorya, OffsetNumber attnumb, Datum b, GinNullCategory categoryb)
{







}

/*
 * Support for sorting key datums in ginExtractEntries
 *
 * Note: we only have to worry about null and not-null keys here;
 * ginExtractEntries never generates more than one placeholder null,
 * so it doesn't have to sort those.
 */
typedef struct
{
  Datum datum;
  bool isnull;
} keyEntryData;

typedef struct
{
  FmgrInfo *cmpDatumFunc;
  Oid collation;
  bool haveDups;
} cmpEntriesArg;

static int
cmpEntries(const void *a, const void *b, void *arg)
{




































}

/*
 * Extract the index key values from an indexable item
 *
 * The resulting key values are sorted, and any duplicates are removed.
 * This avoids generating redundant index entries.
 */
Datum *
ginExtractEntries(GinState *ginstate, OffsetNumber attnum, Datum value, bool isNull, int32 *nentries, GinNullCategory **categories)
{














































































































}

bytea *
ginoptions(Datum reloptions, bool validate)
{




















}

/*
 * Fetch index's statistical data into *stats
 *
 * Note: in the result, nPendingPages can be trusted to be up-to-date,
 * as can ginVersion; but the other fields are as of the last VACUUM.
 */
void
ginGetStats(Relation index, GinStatsData *stats)
{

















}

/*
 * Write the given statistics to the index's metapage
 *
 * Note: nPendingPages and ginVersion are *not* copied over
 */
void
ginUpdateStats(Relation index, const GinStatsData *stats, bool is_build)
{
















































}