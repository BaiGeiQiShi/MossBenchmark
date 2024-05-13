/*-------------------------------------------------------------------------
 *
 * spgscan.c
 *	  routines for scanning SP-GiST indexes
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/spgist/spgscan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/spgist_private.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/datum.h"
#include "utils/float.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef void (*storeRes_func)(SpGistScanOpaque so, ItemPointer heapPtr, Datum leafValue, bool isNull, bool recheck, bool recheckDistances, double *distances);

/*
 * Pairing heap comparison function for the SpGistSearchItem queue.
 * KNN-searches currently only support NULLS LAST.  So, preserve this logic
 * here.
 */
static int
pairingheap_SpGistSearchItem_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{



















































}

static void
spgFreeSearchItem(SpGistScanOpaque so, SpGistSearchItem *item)
{











}

/*
 * Add SpGistSearchItem to queue
 *
 * Called in queue context
 */
static void
spgAddSearchItemToQueue(SpGistScanOpaque so, SpGistSearchItem *item)
{

}

static SpGistSearchItem *
spgAllocSearchItem(SpGistScanOpaque so, bool isnull, double *distances)
{











}

static void
spgAddStartItem(SpGistScanOpaque so, bool isnull)
{











}

/*
 * Initialize queue to search the root page, resetting
 * any previously active scan
 */
static void
resetSpGistScanOpaque(SpGistScanOpaque so)
{





















































}

/*
 * Prepare scan keys in SpGistScanOpaque from caller-given scan keys
 *
 * Sets searchNulls, searchNonNulls, numberOfKeys, keyData fields of *so.
 *
 * The point here is to eliminate null-related considerations from what the
 * opclass consistent functions need to deal with.  We assume all SPGiST-
 * indexable operators are strict, so any null RHS value makes the scan
 * condition unsatisfiable.  We also pull out any IS NULL/IS NOT NULL
 * conditions; their effect is reflected into searchNulls/searchNonNulls.
 */
static void
spgPrepareScanKeys(IndexScanDesc scan)
{







































































































}

IndexScanDesc
spgbeginscan(Relation rel, int keysz, int orderbysz)
{






















































}

void
spgrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{











































}

void
spgendscan(IndexScanDesc scan)
{


























}

/*
 * Leaf SpGistSearchItem constructor, called in queue context
 */
static SpGistSearchItem *
spgNewHeapItem(SpGistScanOpaque so, int level, ItemPointer heapPtr, Datum leafValue, bool recheck, bool recheckDistances, bool isnull, double *distances)
{




























}

/*
 * Test whether a leaf tuple satisfies all the scan keys
 *
 * *reportedSome is set to true if:
 *		the scan is not ordered AND the item satisfies the scankeys
 */
static bool
spgLeafTest(SpGistScanOpaque so, SpGistSearchItem *item, SpGistLeafTuple leafTuple, bool isnull, bool *reportedSome, storeRes_func storeRes)
{







































































}

/* A bundle initializer for inner_consistent methods */
static void
spgInitInnerConsistentIn(spgInnerConsistentIn *in, SpGistScanOpaque so, SpGistSearchItem *item, SpGistInnerTuple innerTuple)
{














}

static SpGistSearchItem *
spgMakeInnerItem(SpGistScanOpaque so, SpGistSearchItem *parentItem, SpGistNodeTuple tuple, spgInnerConsistentOut *out, int i, bool isnull, double *distances)
{




















}

static void
spgInnerTest(SpGistScanOpaque so, SpGistSearchItem *item, SpGistInnerTuple innerTuple, bool isnull)
{







































































}

/* Returns a next item in an (ordered) scan or null if the index is exhausted */
static SpGistSearchItem *
spgGetNextQueueItem(SpGistScanOpaque so)
{







}

enum SpGistSpecialOffsetNumbers
{
  SpGistBreakOffsetNumber = InvalidOffsetNumber,
  SpGistRedirectOffsetNumber = MaxOffsetNumber + 1,
  SpGistErrorOffsetNumber = MaxOffsetNumber + 2
};

static OffsetNumber
spgTestLeafTuple(SpGistScanOpaque so, SpGistSearchItem *item, Page page, OffsetNumber offset, bool isnull, bool isroot, bool *reportedSome, storeRes_func storeRes)
{




































}

/*
 * Walk the tree and report all tuples passing the scan quals to the storeRes
 * subroutine.
 *
 * If scanWholeIndex is true, we'll do just that.  If not, we'll stop at the
 * next page boundary once we have reported at least one tuple.
 */
static void
spgWalk(Relation index, SpGistScanOpaque so, bool scanWholeIndex, storeRes_func storeRes, Snapshot snapshot)
{










































































































}

/* storeRes subroutine for getbitmap case */
static void
storeBitmap(SpGistScanOpaque so, ItemPointer heapPtr, Datum leafValue, bool isnull, bool recheck, bool recheckDistances, double *distances)
{



}

int64
spggetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{











}

/* storeRes subroutine for gettuple case */
static void
storeGettuple(SpGistScanOpaque so, ItemPointer heapPtr, Datum leafValue, bool isnull, bool recheck, bool recheckDistances, double *nonNullDistances)
{















































}

bool
spggettuple(IndexScanDesc scan, ScanDirection dir)
{






























































}

bool
spgcanreturn(Relation index, int attno)
{






}