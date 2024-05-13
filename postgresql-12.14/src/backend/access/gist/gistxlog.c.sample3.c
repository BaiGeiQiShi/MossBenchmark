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
  gistxlogPageSplit xlrec;
  SplitedPageLayout *ptr;
  int npage = 0;
  XLogRecPtr recptr;
  int i;

  for (ptr = dist; ptr; ptr = ptr->next)
  {
    npage++;
  }

  xlrec.origrlink = origrlink;
  xlrec.orignsn = orignsn;
  xlrec.origleaf = page_is_leaf;
  xlrec.npage = (uint16)npage;
  xlrec.markfollowright = markfollowright;

  XLogBeginInsert();

  /*
   * Include a full page image of the child buf. (only necessary if a
   * checkpoint happened since the child page was split)
   */
  if (BufferIsValid(leftchildbuf))
  {
    XLogRegisterBuffer(0, leftchildbuf, REGBUF_STANDARD);
  }

  /*
   * NOTE: We register a lot of data. The caller must've called
   * XLogEnsureRecordSpace() to prepare for that. We cannot do it here,
   * because we're already in a critical section. If you change the number
   * of buffer or data registrations here, make sure you modify the
   * XLogEnsureRecordSpace() calls accordingly!
   */
  XLogRegisterData((char *)&xlrec, sizeof(gistxlogPageSplit));

  i = 1;
  for (ptr = dist; ptr; ptr = ptr->next)
  {
    XLogRegisterBuffer(i, ptr->buffer, REGBUF_WILL_INIT);
    XLogRegisterBufData(i, (char *)&(ptr->block.num), sizeof(int));
    XLogRegisterBufData(i, (char *)ptr->list, ptr->lenlist);
    i++;
  }

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_SPLIT);

  return recptr;
}

/*
 * Write XLOG record describing a page deletion. This also includes removal of
 * downlink from the parent page.
 */
XLogRecPtr
gistXLogPageDelete(Buffer buffer, FullTransactionId xid, Buffer parentBuffer, OffsetNumber downlinkOffset)
{
  gistxlogPageDelete xlrec;
  XLogRecPtr recptr;

  xlrec.deleteXid = xid;
  xlrec.downlinkOffset = downlinkOffset;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfGistxlogPageDelete);

  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
  XLogRegisterBuffer(1, parentBuffer, REGBUF_STANDARD);

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_DELETE);

  return recptr;
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
  gistxlogPageUpdate xlrec;
  int i;
  XLogRecPtr recptr;

  xlrec.ntodelete = ntodelete;
  xlrec.ntoinsert = ituplen;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, sizeof(gistxlogPageUpdate));

  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
  XLogRegisterBufData(0, (char *)todelete, sizeof(OffsetNumber) * ntodelete);

  /* new tuples */
  for (i = 0; i < ituplen; i++)
  {
    XLogRegisterBufData(0, (char *)(itup[i]), IndexTupleSize(itup[i]));
  }

  /*
   * Include a full page image of the child buf. (only necessary if a
   * checkpoint happened since the child page was split)
   */
  if (BufferIsValid(leftchildbuf))
  {
    XLogRegisterBuffer(1, leftchildbuf, REGBUF_STANDARD);
  }

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_UPDATE);

  return recptr;
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