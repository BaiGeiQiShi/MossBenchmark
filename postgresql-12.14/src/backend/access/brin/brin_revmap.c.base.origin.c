/*
 * brin_revmap.c
 *		Range map for BRIN indexes
 *
 * The range map (revmap) is a translation structure for BRIN indexes: for each
 * page range there is one summary tuple, and its location is tracked by the
 * revmap.  Whenever a new tuple is inserted into a table that violates the
 * previously recorded summary values, a new tuple is inserted into the index
 * and the revmap is updated to point to it.
 *
 * The revmap is stored in the first pages of the index, immediately following
 * the metapage.  When the revmap needs to be expanded, all tuples on the
 * regular BRIN page at that block (if any) are moved out of the way.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_revmap.c
 */
#include "postgres.h"

#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_revmap.h"
#include "access/brin_tuple.h"
#include "access/brin_xlog.h"
#include "access/rmgr.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/rel.h"

/*
 * In revmap pages, each item stores an ItemPointerData.  These defines let one
 * find the logical revmap page number and index number of the revmap item for
 * the given heap block number.
 */
#define HEAPBLK_TO_REVMAP_BLK(pagesPerRange, heapBlk) ((heapBlk / pagesPerRange) / REVMAP_PAGE_MAXITEMS)
#define HEAPBLK_TO_REVMAP_INDEX(pagesPerRange, heapBlk) ((heapBlk / pagesPerRange) % REVMAP_PAGE_MAXITEMS)

struct BrinRevmap
{
  Relation rm_irel;
  BlockNumber rm_pagesPerRange;
  BlockNumber rm_lastRevmapPage; /* cached from the metapage */
  Buffer rm_metaBuf;
  Buffer rm_currBuf;
};

/* typedef appears in brin_revmap.h */

static BlockNumber
revmap_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk);
static Buffer
revmap_get_buffer(BrinRevmap *revmap, BlockNumber heapBlk);
static BlockNumber
revmap_extend_and_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk);
static void
revmap_physical_extend(BrinRevmap *revmap);

/*
 * Initialize an access object for a range map.  This must be freed by
 * brinRevmapTerminate when caller is done with it.
 */
BrinRevmap *
brinRevmapInitialize(Relation idxrel, BlockNumber *pagesPerRange, Snapshot snapshot)
{























}

/*
 * Release resources associated with a revmap access object.
 */
void
brinRevmapTerminate(BrinRevmap *revmap)
{






}

/*
 * Extend the revmap to cover the given heap block number.
 */
void
brinRevmapExtend(BrinRevmap *revmap, BlockNumber heapBlk)
{






}

/*
 * Prepare to insert an entry into the revmap; the revmap buffer in which the
 * entry is to reside is locked and returned.  Most callers should call
 * brinRevmapExtend beforehand, as this routine does not extend the revmap if
 * it's not long enough.
 *
 * The returned buffer is also recorded in the revmap struct; finishing that
 * releases the buffer, therefore the caller needn't do it explicitly.
 */
Buffer
brinLockRevmapPageForUpdate(BrinRevmap *revmap, BlockNumber heapBlk)
{






}

/*
 * In the given revmap buffer (locked appropriately by caller), which is used
 * in a BRIN index of pagesPerRange pages per range, set the element
 * corresponding to heap block number heapBlk to the given TID.
 *
 * Once the operation is complete, the caller must update the LSN on the
 * returned buffer.
 *
 * This is used both in regular operation and during WAL replay.
 */
void
brinSetHeapBlockItemptr(Buffer buf, BlockNumber pagesPerRange, BlockNumber heapBlk, ItemPointerData tid)
{


















}

/*
 * Fetch the BrinTuple for a given heap block.
 *
 * The buffer containing the tuple is locked, and returned in *buf.  The
 * returned tuple points to the shared buffer and must not be freed; if caller
 * wants to use it after releasing the buffer lock, it must create its own
 * palloc'ed copy.  As an optimization, the caller can pass a pinned buffer
 * *buf on entry, which will avoid a pin-unpin cycle when the next tuple is on
 * the same page as a previous one.
 *
 * If no tuple is found for the given heap range, returns NULL. In that case,
 * *buf might still be updated (and pin must be released by caller), but it's
 * not locked.
 *
 * The output tuple offset within the buffer is returned in *off, and its size
 * is returned in *size.
 */
BrinTuple *
brinGetTupleForHeapBlock(BrinRevmap *revmap, BlockNumber heapBlk, Buffer *buf, OffsetNumber *off, Size *size, int mode, Snapshot snapshot)
{

























































































































}

/*
 * Delete an index tuple, marking a page range as unsummarized.
 *
 * Index must be locked in ShareUpdateExclusiveLock mode.
 *
 * Return false if caller should retry.
 */
bool
brinRevmapDesummarizeRange(Relation idxrel, BlockNumber heapBlk)
{
















































































































}

/*
 * Given a heap block number, find the corresponding physical revmap block
 * number and return it.  If the revmap page hasn't been allocated yet, return
 * InvalidBlockNumber.
 */
static BlockNumber
revmap_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk)
{












}

/*
 * Obtain and return a buffer containing the revmap page for the given heap
 * page.  The revmap must have been previously extended to cover that page.
 * The returned buffer is also recorded in the revmap struct; finishing that
 * releases the buffer, therefore the caller needn't do it explicitly.
 */
static Buffer
revmap_get_buffer(BrinRevmap *revmap, BlockNumber heapBlk)
{





























}

/*
 * Given a heap block number, find the corresponding physical revmap block
 * number and return it. If the revmap page hasn't been allocated yet, extend
 * the revmap until it is.
 */
static BlockNumber
revmap_extend_and_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk)
{













}

/*
 * Try to extend the revmap by one page.  This might not happen for a number of
 * reasons; caller is expected to retry until the expected outcome is obtained.
 */
static void
revmap_physical_extend(BrinRevmap *revmap)
{




































































































































}