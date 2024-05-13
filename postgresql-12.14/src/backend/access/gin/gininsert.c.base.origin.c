/*-------------------------------------------------------------------------
 *
 * gininsert.c
 *	  insert routines for the postgres inverted index access method.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/gininsert.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "access/tableam.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "storage/indexfsm.h"
#include "storage/predicate.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct
{
  GinState ginstate;
  double indtuples;
  GinStatsData buildStats;
  MemoryContext tmpCtx;
  MemoryContext funcCtx;
  BuildAccumulator accum;
} GinBuildState;

/*
 * Adds array of item pointers to tuple's posting list, or
 * creates posting tree and tuple pointing to tree in case
 * of not enough space.  Max size of tuple is defined in
 * GinFormTuple().  Returns a new, modified index tuple.
 * items[] must be in sorted order with no duplicates.
 */
static IndexTuple
addItemPointersToLeafTuple(GinState *ginstate, IndexTuple old, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats, Buffer buffer)
{

















































}

/*
 * Build a fresh leaf tuple, either posting-list or posting-tree format
 * depending on whether the given items list will fit.
 * items[] must be in sorted order with no duplicates.
 *
 * This is basically the same logic as in addItemPointersToLeafTuple,
 * but working from slightly different input.
 */
static IndexTuple
buildFreshLeafTuple(GinState *ginstate, OffsetNumber attnum, Datum key, GinNullCategory category, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats, Buffer buffer)
{































}

/*
 * Insert one or more heap TIDs associated with the given key value.
 * This will either add a single key entry, or enlarge a pre-existing entry.
 *
 * During an index build, buildStats is non-null and the counters
 * it contains should be incremented as needed.
 */
void
ginEntryInsert(GinState *ginstate, OffsetNumber attnum, Datum key, GinNullCategory category, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats)
{
























































}

/*
 * Extract index entries for a single indexable item, and add them to the
 * BuildAccumulator's state.
 *
 * This function is used only during initial index creation.
 */
static void
ginHeapTupleBulkInsert(GinBuildState *buildstate, OffsetNumber attnum, Datum value, bool isNull, ItemPointer heapptr)
{














}

static void
ginBuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{

































}

IndexBuildResult *
ginbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{


































































































}

/*
 *	ginbuildempty() -- build an empty gin index in the initialization fork
 */
void
ginbuildempty(Relation index)
{





















}

/*
 * Insert index entries for a single indexable item during "normal"
 * (non-fast-update) insertion
 */
static void
ginHeapTupleInsert(GinState *ginstate, OffsetNumber attnum, Datum value, bool isNull, ItemPointer item)
{










}

bool
gininsert(Relation index, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{












































}