/*-------------------------------------------------------------------------
 *
 * xlogutils.c
 *
 * PostgreSQL write-ahead log manager utility routines
 *
 * This file contains support routines that are used by XLOG replay functions.
 * None of this code is used during normal system operation.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/xlogutils.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "access/timeline.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/rel.h"

/*
 * During XLOG replay, we may see XLOG records for incremental updates of
 * pages that no longer exist, because their relation was later dropped or
 * truncated.  (Note: this is only possible when full_page_writes = OFF,
 * since when it's ON, the first reference we see to a page should always
 * be a full-page rewrite not an incremental update.)  Rather than simply
 * ignoring such records, we make a note of the referenced page, and then
 * complain if we don't actually see a drop or truncate covering the page
 * later in replay.
 */
typedef struct xl_invalid_page_key
{
  RelFileNode node;  /* the relation */
  ForkNumber forkno; /* the fork number */
  BlockNumber blkno; /* the page */
} xl_invalid_page_key;

typedef struct xl_invalid_page
{
  xl_invalid_page_key key; /* hash key ... must be first */
  bool present;            /* page existed but contained zeroes */
} xl_invalid_page;

static HTAB *invalid_page_tab = NULL;

/* Report a reference to an invalid page */
static void
report_invalid_page(int elevel, RelFileNode node, ForkNumber forkno, BlockNumber blkno, bool present)
{











}

/* Log a reference to an invalid page */
static void
log_invalid_page(RelFileNode node, ForkNumber forkno, BlockNumber blkno, bool present)
{























































}

/* Forget any invalid pages >= minblkno, because they've been dropped */
static void
forget_invalid_pages(RelFileNode node, ForkNumber forkno, BlockNumber minblkno)
{




























}

/* Forget any invalid pages in a whole database */
static void
forget_invalid_pages_db(Oid dbid)
{




























}

/* Are there any unresolved references to invalid pages? */
bool
XLogHaveInvalidPages(void)
{





}

/* Complain about any remaining invalid-page entries */
void
XLogCheckInvalidPages(void)
{




























}

/*
 * XLogReadBufferForRedo
 *		Read a page during XLOG replay
 *
 * Reads a block referenced by a WAL record into shared buffer cache, and
 * determines what needs to be done to redo the changes to it.  If the WAL
 * record includes a full-page image of the page, it is restored.
 *
 * 'lsn' is the LSN of the record being replayed.  It is compared with the
 * page's LSN to determine if the record has already been replayed.
 * 'block_id' is the ID number the block was registered with, when the WAL
 * record was created.
 *
 * Returns one of the following:
 *
 *	BLK_NEEDS_REDO	- changes from the WAL record need to be applied
 *	BLK_DONE		- block doesn't need replaying
 *	BLK_RESTORED	- block was restored from a full-page image included in
 *					  the record
 *	BLK_NOTFOUND	- block was not found (because it was truncated away by
 *					  an operation later in the WAL stream)
 *
 * On return, the buffer is locked in exclusive-mode, and returned in *buf.
 * Note that the buffer is locked and returned even if it doesn't need
 * replaying.  (Getting the buffer lock is not really necessary during
 * single-process crash recovery, but some subroutines such as MarkBufferDirty
 * will complain if we don't have the lock.  In hot standby mode it's
 * definitely necessary.)
 *
 * Note: when a backup block is available in XLOG with the BKPIMAGE_APPLY flag
 * set, we restore it, even if the page in the database appears newer.  This
 * is to protect ourselves against database pages that were partially or
 * incorrectly written during a crash.  We assume that the XLOG data must be
 * good because it has passed a CRC check, while the database page might not
 * be.  This will force us to replay all subsequent modifications of the page
 * that appear in XLOG, rather than possibly ignoring them as already
 * applied, but that's not a huge drawback.
 */
XLogRedoAction
XLogReadBufferForRedo(XLogReaderState *record, uint8 block_id, Buffer *buf)
{

}

/*
 * Pin and lock a buffer referenced by a WAL record, for the purpose of
 * re-initializing it.
 */
Buffer
XLogInitBufferForRedo(XLogReaderState *record, uint8 block_id)
{




}

/*
 * XLogReadBufferForRedoExtended
 *		Like XLogReadBufferForRedo, but with extra options.
 *
 * In RBM_ZERO_* modes, if the page doesn't exist, the relation is extended
 * with all-zeroes pages up to the referenced block number.  In
 * RBM_ZERO_AND_LOCK and RBM_ZERO_AND_CLEANUP_LOCK modes, the return value
 * is always BLK_NEEDS_REDO.
 *
 * (The RBM_ZERO_AND_CLEANUP_LOCK mode is redundant with the get_cleanup_lock
 * parameter. Do not use an inconsistent combination!)
 *
 * If 'get_cleanup_lock' is true, a "cleanup lock" is acquired on the buffer
 * using LockBufferForCleanup(), instead of a regular exclusive lock.
 */
XLogRedoAction
XLogReadBufferForRedoExtended(XLogReaderState *record, uint8 block_id, ReadBufferMode mode, bool get_cleanup_lock, Buffer *buf)
{






























































































}

/*
 * XLogReadBufferExtended
 *		Read a page during XLOG replay
 *
 * This is functionally comparable to ReadBufferExtended. There's some
 * differences in the behavior wrt. the "mode" argument:
 *
 * In RBM_NORMAL mode, if the page doesn't exist, or contains all-zeroes, we
 * return InvalidBuffer. In this case the caller should silently skip the
 * update on this page. (In this situation, we expect that the page was later
 * dropped or truncated. If we don't see evidence of that later in the WAL
 * sequence, we'll complain at the end of WAL replay.)
 *
 * In RBM_ZERO_* modes, if the page doesn't exist, the relation is extended
 * with all-zeroes pages up to the given block number.
 *
 * In RBM_NORMAL_NO_LOG mode, we return InvalidBuffer if the page doesn't
 * exist, and we don't check for all-zeroes.  Thus, no log entry is made
 * to imply that the page should be dropped or truncated later.
 *
 * NB: A redo function should normally not call this directly. To get a page
 * to modify, use XLogReadBufferForRedoExtended instead. It is important that
 * all pages modified by a WAL record are registered in the WAL records, or
 * they will be invisible to tools that that need to know which pages are
 * modified.
 */
Buffer
XLogReadBufferExtended(RelFileNode rnode, ForkNumber forknum, BlockNumber blkno, ReadBufferMode mode)
{





















































































}

/*
 * Struct actually returned by XLogFakeRelcacheEntry, though the declared
 * return type is Relation.
 */
typedef struct
{
  RelationData reldata; /* Note: this must be first */
  FormData_pg_class pgc;
} FakeRelCacheEntryData;

typedef FakeRelCacheEntryData *FakeRelCacheEntry;

/*
 * Create a fake relation cache entry for a physical relation
 *
 * It's often convenient to use the same functions in XLOG replay as in the
 * main codepath, but those functions typically work with a relcache entry.
 * We don't have a working relation cache during XLOG replay, but this
 * function can be used to create a fake relcache entry instead. Only the
 * fields related to physical storage, like rd_rel, are initialized, so the
 * fake entry is only usable in low-level operations like ReadBuffer().
 *
 * Caller must free the returned entry with FreeFakeRelcacheEntry().
 */
Relation
CreateFakeRelcacheEntry(RelFileNode rnode)
{

































}

/*
 * Free a fake relation cache entry.
 */
void
FreeFakeRelcacheEntry(Relation fakerel)
{






}

/*
 * Drop a relation during XLOG replay
 *
 * This is called when the relation is about to be deleted; we need to remove
 * any open "invalid-page" records for the relation.
 */
void
XLogDropRelation(RelFileNode rnode, ForkNumber forknum)
{

}

/*
 * Drop a whole database during XLOG replay
 *
 * As above, but for DROP DATABASE instead of dropping a single rel
 */
void
XLogDropDatabase(Oid dbid)
{









}

/*
 * Truncate a relation during XLOG replay
 *
 * We need to clean up any open "invalid-page" records for the dropped pages.
 */
void
XLogTruncateRelation(RelFileNode rnode, ForkNumber forkNum, BlockNumber nblocks)
{

}

/*
 * Read 'count' bytes from WAL into 'buf', starting at location 'startptr'
 * in timeline 'tli'.
 *
 * Will open, and keep open, one WAL segment stored in the static file
 * descriptor 'sendFile'. This means if XLogRead is used once, there will
 * always be one descriptor left open until the process ends, but never
 * more than one.
 *
 * XXX This is very similar to pg_waldump's XLogDumpXLogRead and to XLogRead
 * in walsender.c but for small differences (such as lack of elog() in
 * frontend).  Probably these should be merged at some point.
 */
static void
XLogRead(char *buf, int segsize, TimeLineID tli, XLogRecPtr startptr, Size count)
{




































































































}

/*
 * Determine which timeline to read an xlog page from and set the
 * XLogReaderState's currTLI to that timeline ID.
 *
 * We care about timelines in xlogreader when we might be reading xlog
 * generated prior to a promotion, either if we're currently a standby in
 * recovery or if we're a promoted master reading xlogs generated by the old
 * master before our promotion.
 *
 * wantPage must be set to the start address of the page to read and
 * wantLength to the amount of the page that will be read, up to
 * XLOG_BLCKSZ. If the amount to be read isn't known, pass XLOG_BLCKSZ.
 *
 * We switch to an xlog segment from the new timeline eagerly when on a
 * historical timeline, as soon as we reach the start of the xlog segment
 * containing the timeline switch.  The server copied the segment to the new
 * timeline so all the data up to the switch point is the same, but there's no
 * guarantee the old segment will still exist. It may have been deleted or
 * renamed with a .partial suffix so we can't necessarily keep reading from
 * the old TLI even though tliSwitchPoint says it's OK.
 *
 * We can't just check the timeline when we read a page on a different segment
 * to the last page. We could've received a timeline switch from a cascading
 * upstream, so the current segment ends abruptly (possibly getting renamed to
 * .partial) and we have to switch to a new one.  Even in the middle of reading
 * a page we could have to dump the cached page and switch to a new TLI.
 *
 * Because of this, callers MAY NOT assume that currTLI is the timeline that
 * will be in a page's xlp_tli; the page may begin on an older timeline or we
 * might be reading from historical timeline data on a segment that's been
 * copied to a new timeline.
 *
 * The caller must also make sure it doesn't read past the current replay
 * position (using GetWalRcvWriteRecPtr) if executing in recovery, so it
 * doesn't fail to notice that the current timeline became historical. The
 * caller must also update ThisTimeLineID with the result of
 * GetWalRcvWriteRecPtr and must check RecoveryInProgress().
 */
void
XLogReadDetermineTimeline(XLogReaderState *state, XLogRecPtr wantPage, uint32 wantLength)
{


















































































}

/*
 * read_page callback for reading local xlog files
 *
 * Public because it would likely be very helpful for someone writing another
 * output method outside walsender, e.g. in a bgworker.
 *
 * TODO: The walsender has its own version of this, but it relies on the
 * walsender's latch being set whenever WAL is flushed. No such infrastructure
 * exists for normal backends, so we have to do a check/sleep/repeat style of
 * loop for now.
 */
int
read_local_xlog_page(XLogReaderState *state, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *cur_page, TimeLineID *pageTLI)
{























































































































}
