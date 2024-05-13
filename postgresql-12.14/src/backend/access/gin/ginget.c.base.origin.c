/*-------------------------------------------------------------------------
 *
 * ginget.c
 *	  fetch tuples from a GIN scan.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginget.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "storage/predicate.h"
#include "utils/datum.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/* GUC parameter */
int GinFuzzySearchLimit = 0;

typedef struct pendingPosition
{
  Buffer pendingBuffer;
  OffsetNumber firstOffset;
  OffsetNumber lastOffset;
  ItemPointerData item;
  bool *hasMatchKey;
} pendingPosition;

/*
 * Goes to the next page if current offset is outside of bounds
 */
static bool
moveRightIfItNeeded(GinBtreeData *btree, GinBtreeStack *stack, Snapshot snapshot)
{



















}

/*
 * Scan all pages of a posting tree and save all its heap ItemPointers
 * in scanEntry->matchBitmap
 */
static void
scanPostingTree(Relation index, GinScanEntry scanEntry, BlockNumber rootPostingTree, Snapshot snapshot)
{



































}

/*
 * Collects TIDs into scanEntry->matchBitmap for all heap tuples that
 * match the search entry.  This supports three different match modes:
 *
 * 1. Partial-match support: scan from current point until the
 *	  comparePartialFn says we're done.
 * 2. SEARCH_MODE_ALL: scan from current point (which should be first
 *	  key for the current attnum) until we hit null items or end of attnum
 * 3. SEARCH_MODE_EVERYTHING: scan from current point (which should be first
 *	  key for the current attnum) until we hit end of attnum
 *
 * Returns true if done, false if it's necessary to restart scan from scratch
 */
static bool
collectMatchBitmap(GinBtreeData *btree, GinBtreeStack *stack, GinScanEntry scanEntry, Snapshot snapshot)
{




































































































































































































}

/*
 * Start* functions setup beginning state of searches: finds correct buffer and
 * pins it.
 */
static void
startScanEntry(GinState *ginstate, GinScanEntry entry, Snapshot snapshot)
{





























































































































































}

/*
 * Comparison function for scan entry indexes. Sorts by predictNumberResult,
 * least frequent items first.
 */
static int
entryIndexByFrequencyCmp(const void *a1, const void *a2, void *arg)
{


















}

static void
startScanKey(GinState *ginstate, GinScanOpaque so, GinScanKey key)
{


























































































}

static void
startScan(IndexScanDesc scan)
{













































}

/*
 * Load the next batch of item pointers from a posting tree.
 *
 * Note that we copy the page into GinScanEntry->list array and unlock it, but
 * keep it pinned to prevent interference with vacuum.
 */
static void
entryLoadMoreItems(GinState *ginstate, GinScanEntry entry, ItemPointerData advancePast, Snapshot snapshot)
{































































































































}

#define gin_rand() (((double)random()) / ((double)MAX_RANDOM_VALUE))
#define dropItem(e) (gin_rand() > ((double)GinFuzzySearchLimit) / ((double)((e)->predictNumberResult)))

/*
 * Sets entry->curItem to next heap item pointer > advancePast, for one entry
 * of one scan key, or sets entry->isFinished to true if there are no more.
 *
 * Item pointers are returned in ascending order.
 *
 * Note: this can return a "lossy page" item pointer, indicating that the
 * entry potentially matches all items on that heap page.  However, it is
 * not allowed to return both a lossy page pointer and exact (regular)
 * item pointers for the same page.  (Doing so would break the key-combination
 * logic in keyGetItem and scanGetItem; see comment in scanGetItem.)  In the
 * current implementation this is guaranteed by the behavior of tidbitmaps.
 */
static void
entryGetItem(GinState *ginstate, GinScanEntry entry, ItemPointerData advancePast, Snapshot snapshot)
{
































































































































































}

/*
 * Identify the "current" item among the input entry streams for this scan key
 * that is greater than advancePast, and test whether it passes the scan key
 * qual condition.
 *
 * The current item is the smallest curItem among the inputs.  key->curItem
 * is set to that value.  key->curItemMatches is set to indicate whether that
 * TID passes the consistentFn test.  If so, key->recheckCurItem is set true
 * iff recheck is needed for this item pointer (including the case where the
 * item pointer is a lossy page pointer).
 *
 * If all entry streams are exhausted, sets key->isFinished to true.
 *
 * Item pointers must be returned in ascending order.
 *
 * Note: this can return a "lossy page" item pointer, indicating that the
 * key potentially matches all items on that heap page.  However, it is
 * not allowed to return both a lossy page pointer and exact (regular)
 * item pointers for the same page.  (Doing so would break the key-combination
 * logic in scanGetItem.)
 */
static void
keyGetItem(GinState *ginstate, MemoryContext tempCtx, GinScanKey key, ItemPointerData advancePast, Snapshot snapshot)
{




























































































































































































































































































}

/*
 * Get next heap item pointer (after advancePast) from scan.
 * Returns true if anything found.
 * On success, *item and *recheck are set.
 *
 * Note: this is very nearly the same logic as in keyGetItem(), except
 * that we know the keys are to be combined with AND logic, whereas in
 * keyGetItem() the combination logic is known only to the consistentFn.
 */
static bool
scanGetItem(IndexScanDesc scan, ItemPointerData advancePast, ItemPointerData *item, bool *recheck)
{




























































































































}

/*
 * Functions for scanning the pending list
 */

/*
 * Get ItemPointer of next heap row to be checked from pending list.
 * Returns false if there are no more. On pages with several heap rows
 * it returns each row separately, on page with part of heap row returns
 * per page data.  pos->firstOffset and pos->lastOffset are set to identify
 * the range of pending-list tuples belonging to this heap row.
 *
 * The pendingBuffer is presumed pinned and share-locked on entry, and is
 * pinned and share-locked on success exit.  On failure exit it's released.
 */
static bool
scanGetCandidate(IndexScanDesc scan, pendingPosition *pos)
{











































































}

/*
 * Scan pending-list page from current tuple (off) up till the first of:
 * - match is found (then returns true)
 * - no later match is possible
 * - tuple's attribute number is not equal to entry's attrnum
 * - reach end of page
 *
 * datum[]/category[]/datumExtracted[] arrays are used to cache the results
 * of gintuple_get_key() on the current page.
 */
static bool
matchPartialInPendingList(GinState *ginstate, Page page, OffsetNumber off, OffsetNumber maxoff, GinScanEntry entry, Datum *datum, GinNullCategory *category, bool *datumExtracted)
{



















































}

/*
 * Set up the entryRes array for each key by looking at
 * every entry for current heap row in pending list.
 *
 * Returns true if each scan key has at least one entryRes match.
 * This corresponds to the situations where the normal index search will
 * try to apply the key's consistentFn.  (A tuple not meeting that requirement
 * cannot be returned by the normal search since no entry stream will
 * source its TID.)
 *
 * The pendingBuffer is presumed pinned and share-locked on entry.
 */
static bool
collectMatchesForHeapRow(IndexScanDesc scan, pendingPosition *pos)
{



































































































































































































}

/*
 * Collect all matched rows from pending list into bitmap.
 */
static void
scanPendingInsert(IndexScanDesc scan, TIDBitmap *tbm, int64 *ntids)
{
























































































}

#define GinIsVoidRes(s) (((GinScanOpaque)scan->opaque)->isVoidRes)

int64
gingetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{



























































}