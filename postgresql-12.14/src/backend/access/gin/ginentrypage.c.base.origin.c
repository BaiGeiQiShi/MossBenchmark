/*-------------------------------------------------------------------------
 *
 * ginentrypage.c
 *	  routines for handling GIN entry tree pages.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginentrypage.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "utils/rel.h"

static void
entrySplitPage(GinBtree btree, Buffer origbuf, GinBtreeStack *stack, GinBtreeEntryInsertData *insertData, BlockNumber updateblkno, Page *newlpage, Page *newrpage);

/*
 * Form a tuple for entry tree.
 *
 * If the tuple would be too big to be stored, function throws a suitable
 * error if errorTooBig is true, or returns NULL if errorTooBig is false.
 *
 * See src/backend/access/gin/README for a description of the index tuple
 * format that is being built here.  We build on the assumption that we
 * are making a leaf-level key entry containing a posting list of nipd items.
 * If the caller is actually trying to make a posting-tree entry, non-leaf
 * entry, or pending-list entry, it should pass dataSize = 0 and then overwrite
 * the t_tid fields as necessary.  In any case, 'data' can be NULL to skip
 * filling in the posting list; the caller is responsible for filling it
 * afterwards if data = NULL and nipd > 0.
 */
IndexTuple
GinFormTuple(GinState *ginstate, OffsetNumber attnum, Datum key, GinNullCategory category, Pointer data, Size dataSize, int nipd, bool errorTooBig)
{





































































































}

/*
 * Read item pointers from leaf entry tuple.
 *
 * Returns a palloc'd array of ItemPointers. The number of items is returned
 * in *nitems.
 */
ItemPointer
ginReadTuple(GinState *ginstate, OffsetNumber attnum, IndexTuple itup, int *nitems)
{



























}

/*
 * Form a non-leaf entry tuple by copying the key data from the given tuple,
 * which can be either a leaf or non-leaf entry tuple.
 *
 * Any posting list in the source tuple is not copied.  The specified child
 * block number is inserted into t_tid.
 */
static IndexTuple
GinFormInteriorTuple(IndexTuple itup, Page page, BlockNumber childblk)
{

























}

/*
 * Entry tree is a "static", ie tuple never deletes from it,
 * so we don't use right bound, we use rightmost key instead.
 */
static IndexTuple
getRightMostTuple(Page page)
{



}

static bool
entryIsMoveRight(GinBtree btree, Page page)
{




















}

/*
 * Find correct tuple in non-leaf page. It supposed that
 * page correctly chosen and searching value SHOULD be on page
 */
static BlockNumber
entryLocateEntry(GinBtree btree, GinBtreeStack *stack)
{
































































}

/*
 * Searches correct position for value on leaf page.
 * Page should be correctly chosen.
 * Returns true if value found on page.
 */
static bool
entryLocateLeafEntry(GinBtree btree, GinBtreeStack *stack)
{





















































}

static OffsetNumber
entryFindChildPtr(GinBtree btree, Page page, BlockNumber blkno, OffsetNumber storedOff)
{









































}

static BlockNumber
entryGetLeftMostPage(GinBtree btree, Page page)
{








}

static bool
entryIsEnoughSpace(GinBtree btree, Buffer buf, OffsetNumber off, GinBtreeEntryInsertData *insertData)
{






















}

/*
 * Delete tuple on leaf page if tuples existed and we
 * should update it, update old child blkno to new right page
 * if child split occurred
 */
static void
entryPreparePage(GinBtree btree, Page page, OffsetNumber off, GinBtreeEntryInsertData *insertData, BlockNumber updateblkno)
{















}

/*
 * Prepare to insert data on an entry page.
 *
 * If it will fit, return GPTP_INSERT after doing whatever setup is needed
 * before we enter the insertion critical section.  *ptp_workspace can be
 * set to pass information along to the execPlaceToPage function.
 *
 * If it won't fit, perform a page split and return two temporary page
 * images into *newlpage and *newrpage, with result GPTP_SPLIT.
 *
 * In neither case should the given page buffer be modified here.
 *
 * Note: on insertion to an internal node, in addition to inserting the given
 * item, the downlink of the existing item at stack->off will be updated to
 * point to updateblkno.
 */
static GinPlaceToPageRC
entryBeginPlaceToPage(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertPayload, BlockNumber updateblkno, void **ptp_workspace, Page *newlpage, Page *newrpage)
{












}

/*
 * Perform data insertion after beginPlaceToPage has decided it will fit.
 *
 * This is invoked within a critical section, and XLOG record creation (if
 * needed) is already started.  The target buffer is registered in slot 0.
 */
static void
entryExecPlaceToPage(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertPayload, BlockNumber updateblkno, void *ptp_workspace)
{




























}

/*
 * Split entry page and insert new data.
 *
 * Returns new temp pages to *newlpage and *newrpage.
 * The original buffer is left untouched.
 */
static void
entrySplitPage(GinBtree btree, Buffer origbuf, GinBtreeStack *stack, GinBtreeEntryInsertData *insertData, BlockNumber updateblkno, Page *newlpage, Page *newrpage)
{
























































































}

/*
 * Construct insertion payload for inserting the downlink for given buffer.
 */
static void *
entryPrepareDownlink(GinBtree btree, Buffer lbuf)
{












}

/*
 * Fills new root by rightest values from child.
 * Also called from ginxlog, should not use btree
 */
void
ginEntryFillRoot(GinBtree btree, Page root, BlockNumber lblkno, Page lpage, BlockNumber rblkno, Page rpage)
{















}

/*
 * Set up GinBtree for entry page access
 *
 * Note: during WAL recovery, there may be no valid data in ginstate
 * other than a faked-up Relation pointer; the key datum is bogus too.
 */
void
ginPrepareEntryScan(GinBtree btree, OffsetNumber attnum, Datum key, GinNullCategory category, GinState *ginstate)
{























}