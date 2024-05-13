/*-------------------------------------------------------------------------
 *
 * gistxlog.c
 *	  WAL replay logic for GiST.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			 src/backend/access/gist/gistxlog.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/bufmask.h"
#include "access/gist_private.h"
#include "access/gistxlog.h"
#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "storage/procarray.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static MemoryContext opCtx; /* working memory for operations */

/*
 * Replay the clearing of F_FOLLOW_RIGHT flag on a child page.
 *
 * Even if the WAL record includes a full-page image, we have to update the
 * follow-right flag, because that change is not included in the full-page
 * image.  To be sure that the intermediate state with the wrong flag value is
 * not visible to concurrent Hot Standby queries, this function handles
 * restoring the full-page image as well as updating the flag.  (Note that
 * we never need to do anything else to the child page in the current WAL
 * action.)
 */
static void
gistRedoClearFollowRight(XLogReaderState *record, uint8 block_id)
{
























}

/*
 * redo any page update (except page split)
 */
static void
gistRedoPageUpdateRecord(XLogReaderState *record)
{



































































































}

/*
 * redo delete on gist index page to remove tuples marked as DEAD during index
 * tuple insertion
 */
static void
gistRedoDeleteRecord(XLogReaderState *record)
{

















































}

/*
 * Returns an array of index pointers.
 */
static IndexTuple *
decodePageSplitRecord(char *begin, int len, int *n)
{



















}

static void
gistRedoPageSplitRecord(XLogReaderState *record)
{








































































































}

/* redo page deletion */
static void
gistRedoPageDelete(XLogReaderState *record)
{

































}

static void
gistRedoPageReuse(XLogReaderState *record)
{


































}

void
gist_redo(XLogReaderState *record)
{

































}

void
gist_xlog_startup(void)
{

}

void
gist_xlog_cleanup(void)
{

}

/*
 * Mask a Gist page before running consistency checks on it.
 */
void
gist_mask(char *pagedata, BlockNumber blkno)
{


































}

/*
 * Write WAL record of a page split.
 */
XLogRecPtr
gistXLogSplit(bool page_is_leaf, SplitedPageLayout *dist, BlockNumber origrlink, GistNSN orignsn, Buffer leftchildbuf, bool markfollowright)
{

















































}

/*
 * Write XLOG record describing a page deletion. This also includes removal of
 * downlink from the parent page.
 */
XLogRecPtr
gistXLogPageDelete(Buffer buffer, FullTransactionId xid, Buffer parentBuffer, OffsetNumber downlinkOffset)
{















}

/*
 * Write XLOG record about reuse of a deleted page.
 */
void
gistXLogPageReuse(Relation rel, BlockNumber blkno, FullTransactionId latestRemovedXid)
{

















}

/*
 * Write XLOG record describing a page update. The update can include any
 * number of deletions and/or insertions of tuples on a single index page.
 *
 * If this update inserts a downlink for a split page, also record that
 * the F_FOLLOW_RIGHT flag on the child page is cleared and NSN set.
 *
 * Note that both the todelete array and the tuples are marked as belonging
 * to the target buffer; they need not be stored in XLOG if XLogInsert decides
 * to log the whole buffer contents instead.
 */
XLogRecPtr
gistXLogUpdate(Buffer buffer, OffsetNumber *todelete, int ntodelete, IndexTuple *itup, int ituplen, Buffer leftchildbuf)
{































}

/*
 * Write XLOG record describing a delete of leaf index tuples marked as DEAD
 * during new tuple insertion.  One may think that this case is already covered
 * by gistXLogUpdate().  But deletion of index tuples might conflict with
 * standby queries and needs special handling.
 */
XLogRecPtr
gistXLogDelete(Buffer buffer, OffsetNumber *todelete, int ntodelete, TransactionId latestRemovedXid)
{




















}