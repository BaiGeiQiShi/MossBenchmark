/*-------------------------------------------------------------------------
 *
 * clog.c
 *		PostgreSQL transaction-commit-log manager
 *
 * This module replaces the old "pg_log" access code, which treated pg_log
 * essentially like a relation, in that it went through the regular buffer
 * manager.  The problem with that was that there wasn't any good way to
 * recycle storage space for transactions so old that they'll never be
 * looked up again.  Now we use specialized access code so that the commit
 * log can be broken into relatively small, independent segments.
 *
 * XLOG interactions: this module generates an XLOG record whenever a new
 * CLOG page is initialized to zeroes.  Other writes of CLOG come from
 * recording of transaction commit or abort in xact.c, which generates its
 * own XLOG records for these events and will re-perform the status update
 * on redo; so we need make no additional XLOG entry here.  For synchronous
 * transaction commits, the XLOG is guaranteed flushed through the XLOG commit
 * record before we are called to log a commit, so the WAL rule "write xlog
 * before data" is satisfied automatically.  However, for async commits we
 * must track the latest LSN affecting each CLOG page, so that we can flush
 * XLOG that far and satisfy the WAL rule.  We don't have to worry about this
 * for aborts (whether sync or async), since the post-crash assumption would
 * be that such transactions failed anyway.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/clog.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/clog.h"
#include "access/slru.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "pg_trace.h"
#include "storage/proc.h"

/*
 * Defines for CLOG page sizes.  A page is the same BLCKSZ as is used
 * everywhere else in Postgres.
 *
 * Note: because TransactionIds are 32 bits and wrap around at 0xFFFFFFFF,
 * CLOG page numbering also wraps around at 0xFFFFFFFF/CLOG_XACTS_PER_PAGE,
 * and CLOG segment numbering at
 * 0xFFFFFFFF/CLOG_XACTS_PER_PAGE/SLRU_PAGES_PER_SEGMENT.  We need take no
 * explicit notice of that fact in this module, except when comparing segment
 * and page numbers in TruncateCLOG (see CLOGPagePrecedes).
 */

/* We need two bits per xact, so four xacts fit in a byte */
#define CLOG_BITS_PER_XACT 2
#define CLOG_XACTS_PER_BYTE 4
#define CLOG_XACTS_PER_PAGE (BLCKSZ * CLOG_XACTS_PER_BYTE)
#define CLOG_XACT_BITMASK ((1 << CLOG_BITS_PER_XACT) - 1)

#define TransactionIdToPage(xid) ((xid) / (TransactionId)CLOG_XACTS_PER_PAGE)
#define TransactionIdToPgIndex(xid) ((xid) % (TransactionId)CLOG_XACTS_PER_PAGE)
#define TransactionIdToByte(xid) (TransactionIdToPgIndex(xid) / CLOG_XACTS_PER_BYTE)
#define TransactionIdToBIndex(xid) ((xid) % (TransactionId)CLOG_XACTS_PER_BYTE)

/* We store the latest async LSN for each group of transactions */
#define CLOG_XACTS_PER_LSN_GROUP 32 /* keep this a power of 2 */
#define CLOG_LSNS_PER_PAGE (CLOG_XACTS_PER_PAGE / CLOG_XACTS_PER_LSN_GROUP)

#define GetLSNIndex(slotno, xid) ((slotno) * CLOG_LSNS_PER_PAGE + ((xid) % (TransactionId)CLOG_XACTS_PER_PAGE) / CLOG_XACTS_PER_LSN_GROUP)

/*
 * The number of subtransactions below which we consider to apply clog group
 * update optimization.  Testing reveals that the number higher than this can
 * hurt performance.
 */
#define THRESHOLD_SUBTRANS_CLOG_OPT 5

/*
 * Link to shared-memory data structures for CLOG control
 */
static SlruCtlData ClogCtlData;

#define ClogCtl (&ClogCtlData)

static int
ZeroCLOGPage(int pageno, bool writeXlog);
static bool
CLOGPagePrecedes(int page1, int page2);
static void
WriteZeroPageXlogRec(int pageno);
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXact, Oid oldestXidDb);
static void
TransactionIdSetPageStatus(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno, bool all_xact_same_page);
static void
TransactionIdSetStatusBit(TransactionId xid, XidStatus status, XLogRecPtr lsn, int slotno);
static void
set_status_by_pages(int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn);
static bool
TransactionGroupUpdateXidStatus(TransactionId xid, XidStatus status, XLogRecPtr lsn, int pageno);
static void
TransactionIdSetPageStatusInternal(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno);

/*
 * TransactionIdSetTreeStatus
 *
 * Record the final state of transaction entries in the commit log for
 * a transaction and its subtransaction tree. Take care to ensure this is
 * efficient, and as atomic as possible.
 *
 * xid is a single xid to set status for. This will typically be
 * the top level transactionid for a top level commit or abort. It can
 * also be a subtransaction when we record transaction aborts.
 *
 * subxids is an array of xids of length nsubxids, representing subtransactions
 * in the tree of xid. In various cases nsubxids may be zero.
 *
 * lsn must be the WAL location of the commit record when recording an async
 * commit.  For a synchronous commit it can be InvalidXLogRecPtr, since the
 * caller guarantees the commit record is already flushed in that case.  It
 * should be InvalidXLogRecPtr for abort cases, too.
 *
 * In the commit case, atomicity is limited by whether all the subxids are in
 * the same CLOG page as xid.  If they all are, then the lock will be grabbed
 * only once, and the status will be set to committed directly.  Otherwise
 * we must
 *	 1. set sub-committed all subxids that are not on the same page as the
 *		main xid
 *	 2. atomically set committed the main xid and the subxids on the same
 page *	 3. go over the first bunch again and set them committed
 * Note that as far as concurrent checkers are concerned, main transaction
 * commit as a whole is still atomic.
 *
 * Example:
 *		TransactionId t commits and has subxids t1, t2, t3, t4
 *		t is on page p1, t1 is also on p1, t2 and t3 are on p2, t4 is on
 p3 *		1. update pages2-3: *					page2:
 set t2,t3 as sub-committed *					page3: set t4 as
 sub-committed *		2. update page1: *
 set t1 as sub-committed, *					then set t as
 committed, then set t1 as committed *		3. update pages2-3: *
 page2: set t2,t3 as committed *					page3:
 set t4 as committed
 *
 * NB: this is a low-level routine and is NOT the preferred entry point
 * for most uses; functions in transam.c are the intended callers.
 *
 * XXX Think about issuing FADVISE_WILLNEED on pages that we will need,
 * but aren't yet in cache, as well as hinting pages not to fall out of
 * cache yet.
 */
void
TransactionIdSetTreeStatus(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn)
{



























































}

/*
 * Helper for TransactionIdSetTreeStatus: set the status for a bunch of
 * transactions, chunking in the separate CLOG pages involved. We never
 * pass the whole transaction tree to this function, only subtransactions
 * that are on different pages to the top level transaction id.
 */
static void
set_status_by_pages(int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn)
{


























}

/*
 * Record the final state of transaction entries in the commit log for all
 * entries on a single page.  Atomic only on this page.
 */
static void
TransactionIdSetPageStatus(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno, bool all_xact_same_page)
{













































}

/*
 * Record the final state of transaction entry in the commit log
 *
 * We don't do any locking here; caller must handle that.
 */
static void
TransactionIdSetPageStatusInternal(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno)
{


















































}

/*
 * When we cannot immediately acquire CLogControlLock in exclusive mode at
 * commit time, add ourselves to a list of processes that need their XIDs
 * status update.  The first process to add itself to the list will acquire
 * CLogControlLock in exclusive mode and set transaction status as required
 * on behalf of all group members.  This avoids a great deal of contention
 * around CLogControlLock when many processes are trying to commit at once,
 * since the lock need not be repeatedly handed off from one committing
 * process to the next.
 *
 * Returns true when transaction status has been updated in clog; returns
 * false if we decided against applying the optimization because the page
 * number we need to update differs from those processes already waiting.
 */
static bool
TransactionGroupUpdateXidStatus(TransactionId xid, XidStatus status, XLogRecPtr lsn, int pageno)
{

















































































































































}

/*
 * Sets the commit status of a single transaction.
 *
 * Must be called with CLogControlLock held
 */
static void
TransactionIdSetStatusBit(TransactionId xid, XidStatus status, XLogRecPtr lsn, int slotno)
{

















































}

/*
 * Interrogate the state of a transaction in the commit log.
 *
 * Aside from the actual commit status, this function returns (into *lsn)
 * an LSN that is late enough to be able to guarantee that if we flush up to
 * that LSN then we will have flushed the transaction's commit record to disk.
 * The result is not necessarily the exact LSN of the transaction's commit
 * record!	For example, for long-past transactions (those whose clog pages
 * already migrated to disk), we'll return InvalidXLogRecPtr.  Also, because
 * we group transactions on the same clog page to conserve storage, we might
 * return the LSN of a later transaction that falls into the same group.
 *
 * NB: this is a low-level routine and is NOT the preferred entry point
 * for most uses; TransactionLogFetch() in transam.c is the intended caller.
 */
XidStatus
TransactionIdGetStatus(TransactionId xid, XLogRecPtr *lsn)
{





















}

/*
 * Number of shared CLOG buffers.
 *
 * On larger multi-processor systems, it is possible to have many CLOG page
 * requests in flight at one time which could lead to disk access for CLOG
 * page if the required page is not found in memory.  Testing revealed that we
 * can get the best performance by having 128 CLOG buffers, more than that it
 * doesn't improve performance.
 *
 * Unconditionally keeping the number of CLOG buffers to 128 did not seem like
 * a good idea, because it would increase the minimum amount of shared memory
 * required to start, which could be a problem for people running very small
 * configurations.  The following formula seems to represent a reasonable
 * compromise: people with very low values for shared_buffers will get fewer
 * CLOG buffers as well, and everyone else will get 128.
 */
Size
CLOGShmemBuffers(void)
{

}

/*
 * Initialization of shared memory for CLOG
 */
Size
CLOGShmemSize(void)
{

}

void
CLOGShmemInit(void)
{



}

/*
 * This func must be called ONCE on system install.  It creates
 * the initial CLOG segment.  (The CLOG directory is assumed to
 * have been created by initdb, and CLOGShmemInit must have been
 * called already.)
 */
void
BootStrapCLOG(void)
{












}

/*
 * Initialize (or reinitialize) a page of CLOG to zeroes.
 * If writeXlog is true, also emit an XLOG record saying we did this.
 *
 * The page is not actually written, just set up in shared memory.
 * The slot number of the new page is returned.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
ZeroCLOGPage(int pageno, bool writeXlog)
{










}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after StartupXLOG has initialized ShmemVariableCache->nextFullXid.
 */
void
StartupCLOG(void)
{











}

/*
 * This must be called ONCE at the end of startup/recovery.
 */
void
TrimCLOG(void)
{









































}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
ShutdownCLOG(void)
{











}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 */
void
CheckPointCLOG(void)
{




}

/*
 * Make sure that CLOG has room for a newly-allocated XID.
 *
 * NB: this is called while holding XidGenLock.  We want it to be very fast
 * most of the time; even when it's not so fast, no actual I/O need happen
 * unless we're forced to write out a dirty clog or xlog page to make room
 * in shared memory.
 */
void
ExtendCLOG(TransactionId newestXact)
{



















}

/*
 * Remove all CLOG segments before the one holding the passed transaction ID
 *
 * Before removing any CLOG data, we must flush XLOG to disk, to ensure
 * that any recently-emitted HEAP_FREEZE records have reached disk; otherwise
 * a crash and restart might leave us with some unfrozen tuples referencing
 * removed CLOG data.  We choose to emit a special TRUNCATE XLOG record too.
 * Replaying the deletion from XLOG is not critical, since the files could
 * just as well be removed later, but doing so prevents a long-running hot
 * standby server from acquiring an unreasonably bloated CLOG directory.
 *
 * Since CLOG segments hold a large number of transactions, the opportunity to
 * actually remove a segment is fairly rare, and so it seems best not to do
 * the XLOG flush unless we have confirmed that there is a removable segment.
 */
void
TruncateCLOG(TransactionId oldestXact, Oid oldestxid_datoid)
{

































}

/*
 * Decide whether a CLOG page number is "older" for truncation purposes.
 *
 * We need to use comparison of TransactionIds here in order to do the right
 * thing with wraparound XID arithmetic.  However, TransactionIdPrecedes()
 * would get weird about permanent xact IDs.  So, offset both such that xid1,
 * xid2, and xid2 + CLOG_XACTS_PER_PAGE - 1 are all normal XIDs; this offset
 * is relevant to page 0 and to the page preceding page 0.
 *
 * The page containing oldestXact-2^31 is the important edge case.  The
 * portion of that page equaling or following oldestXact-2^31 is expendable,
 * but the portion preceding oldestXact-2^31 is not.  When oldestXact-2^31 is
 * the first XID of a page and segment, the entire page and segment is
 * expendable, and we could truncate the segment.  Recognizing that case would
 * require making oldestXact, not just the page containing oldestXact,
 * available to this callback.  The benefit would be rare and small, so we
 * don't optimize that edge case.
 */
static bool
CLOGPagePrecedes(int page1, int page2)
{









}

/*
 * Write a ZEROPAGE xlog record
 */
static void
WriteZeroPageXlogRec(int pageno)
{



}

/*
 * Write a TRUNCATE xlog record
 *
 * We must flush the xlog record to disk before returning --- see notes
 * in TruncateCLOG().
 */
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXact, Oid oldestXactDb)
{











}

/*
 * CLOG resource manager's routines
 */
void
clog_redo(XLogReaderState *record)
{








































}