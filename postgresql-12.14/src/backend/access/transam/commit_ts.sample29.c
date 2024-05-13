/*-------------------------------------------------------------------------
 *
 * commit_ts.c
 *		PostgreSQL commit timestamp manager
 *
 * This module is a pg_xact-like system that stores the commit timestamp
 * for each transaction.
 *
 * XLOG interactions: this module generates an XLOG record whenever a new
 * CommitTs page is initialized to zeroes.  Also, one XLOG record is
 * generated for setting of values when the caller requests it; this allows
 * us to support values coming from places other than transaction commit.
 * Other writes of CommitTS come from recording of transaction commit in
 * xact.c, which generates its own XLOG records for these events and will
 * re-perform the status update on redo; so we need make no additional XLOG
 * entry here.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/commit_ts.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/commit_ts.h"
#include "access/htup_details.h"
#include "access/slru.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "storage/shmem.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

/*
 * Defines for CommitTs page sizes.  A page is the same BLCKSZ as is used
 * everywhere else in Postgres.
 *
 * Note: because TransactionIds are 32 bits and wrap around at 0xFFFFFFFF,
 * CommitTs page numbering also wraps around at
 * 0xFFFFFFFF/COMMIT_TS_XACTS_PER_PAGE, and CommitTs segment numbering at
 * 0xFFFFFFFF/COMMIT_TS_XACTS_PER_PAGE/SLRU_PAGES_PER_SEGMENT.  We need take no
 * explicit notice of that fact in this module, except when comparing segment
 * and page numbers in TruncateCommitTs (see CommitTsPagePrecedes).
 */

/*
 * We need 8+2 bytes per xact.  Note that enlarging this struct might mean
 * the largest possible file name is more than 5 chars long; see
 * SlruScanDirectory.
 */
typedef struct CommitTimestampEntry
{
  TimestampTz time;
  RepOriginId nodeid;
} CommitTimestampEntry;

#define SizeOfCommitTimestampEntry (offsetof(CommitTimestampEntry, nodeid) + sizeof(RepOriginId))

#define COMMIT_TS_XACTS_PER_PAGE (BLCKSZ / SizeOfCommitTimestampEntry)

#define TransactionIdToCTsPage(xid) ((xid) / (TransactionId)COMMIT_TS_XACTS_PER_PAGE)
#define TransactionIdToCTsEntry(xid) ((xid) % (TransactionId)COMMIT_TS_XACTS_PER_PAGE)

/*
 * Link to shared-memory data structures for CommitTs control
 */
static SlruCtlData CommitTsCtlData;

#define CommitTsCtl (&CommitTsCtlData)

/*
 * We keep a cache of the last value set in shared memory.
 *
 * This is also good place to keep the activation status.  We keep this
 * separate from the GUC so that the standby can activate the module if the
 * primary has it active independently of the value of the GUC.
 *
 * This is protected by CommitTsLock.  In some places, we use commitTsActive
 * without acquiring the lock; where this happens, a comment explains the
 * rationale for it.
 */
typedef struct CommitTimestampShared
{
  TransactionId xidLastCommit;
  CommitTimestampEntry dataLastCommit;
  bool commitTsActive;
} CommitTimestampShared;

CommitTimestampShared *commitTsShared;

/* GUC variable */
bool track_commit_timestamp;

static void
SetXidCommitTsInPage(TransactionId xid, int nsubxids, TransactionId *subxids, TimestampTz ts, RepOriginId nodeid, int pageno);
static void
TransactionIdSetCommitTs(TransactionId xid, TimestampTz ts, RepOriginId nodeid, int slotno);
static void
error_commit_ts_disabled(void);
static int
ZeroCommitTsPage(int pageno, bool writeXlog);
static bool
CommitTsPagePrecedes(int page1, int page2);
static void
ActivateCommitTs(void);
static void
DeactivateCommitTs(void);
static void
WriteZeroPageXlogRec(int pageno);
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXid);
static void
WriteSetTimestampXlogRec(TransactionId mainxid, int nsubxids, TransactionId *subxids, TimestampTz timestamp, RepOriginId nodeid);

/*
 * TransactionTreeSetCommitTsData
 *
 * Record the final commit timestamp of transaction entries in the commit log
 * for a transaction and its subtransaction tree, as efficiently as possible.
 *
 * xid is the top level transaction id.
 *
 * subxids is an array of xids of length nsubxids, representing subtransactions
 * in the tree of xid. In various cases nsubxids may be zero.
 * The reason why tracking just the parent xid commit timestamp is not enough
 * is that the subtrans SLRU does not stay valid across crashes (it's not
 * permanent) so we need to keep the information about them here. If the
 * subtrans implementation changes in the future, we might want to revisit the
 * decision of storing timestamp info for each subxid.
 *
 * The write_xlog parameter tells us whether to include an XLog record of this
 * or not.  Normally, this is called from transaction commit routines (both
 * normal and prepared) and the information will be stored in the transaction
 * commit XLog record, and so they should pass "false" for this.  The XLog redo
 * code should use "false" here as well.  Other callers probably want to pass
 * true, so that the given values persist in case of crashes.
 */
void
TransactionTreeSetCommitTsData(TransactionId xid, int nsubxids, TransactionId *subxids, TimestampTz timestamp, RepOriginId nodeid, bool write_xlog)
{
  int i;
  TransactionId headxid;
  TransactionId newestXact;

  /*
   * No-op if the module is not active.
   *
   * An unlocked read here is fine, because in a standby (the only place
   * where the flag can change in flight) this routine is only called by the
   * recovery process, which is also the only process which can change the
   * flag.
   */
  if (!commitTsShared->commitTsActive)
  {
    return;
  }

  /*
   * Comply with the WAL-before-data rule: if caller specified it wants this
   * value to be recorded in WAL, do so before touching the data.
   */





  /*
   * Figure out the latest Xid in this batch: either the last subxid if
   * there's any, otherwise the parent xid.
   */









  /*
   * We split the xids to set the timestamp to in groups belonging to the
   * same SLRU page; the first element in each such set is its head.  The
   * first group has the main XID as the head; subsequent sets use the first
   * subxid not on the previous page as head.  This way, we only have to
   * lock/modify each SLRU page once.
   */
































  /* update the cached value in shared memory */





  /* and move forwards our endpoint, if needed */





}

/*
 * Record the commit timestamp of transaction entries in the commit log for all
 * entries on a single page.  Atomic only on this page.
 */
static void
SetXidCommitTsInPage(TransactionId xid, int nsubxids, TransactionId *subxids, TimestampTz ts, RepOriginId nodeid, int pageno)
{
















}

/*
 * Sets the commit timestamp of a single transaction.
 *
 * Must be called with CommitTsControlLock held
 */
static void
TransactionIdSetCommitTs(TransactionId xid, TimestampTz ts, RepOriginId nodeid, int slotno)
{









}

/*
 * Interrogate the commit timestamp of a transaction.
 *
 * The return value indicates whether a commit timestamp record was found for
 * the given xid.  The timestamp value is returned in *ts (which may not be
 * null), and the origin node for the Xid is returned in *nodeid, if it's not
 * null.
 */
bool
TransactionIdGetCommitTsData(TransactionId xid, TimestampTz *ts, RepOriginId *nodeid)
{













































































}

/*
 * Return the Xid of the latest committed transaction.  (As far as this module
 * is concerned, anyway; it's up to the caller to ensure the value is useful
 * for its purposes.)
 *
 * ts and extra are filled with the corresponding data; they can be passed
 * as NULL if not wanted.
 */
TransactionId
GetLatestCommitTsData(TimestampTz *ts, RepOriginId *nodeid)
{






















}

static void
error_commit_ts_disabled(void)
{

}

/*
 * SQL-callable wrapper to obtain commit time of a transaction
 */
Datum
pg_xact_commit_timestamp(PG_FUNCTION_ARGS)
{












}

Datum
pg_last_committed_xact(PG_FUNCTION_ARGS)
{



































}

/*
 * Number of shared CommitTS buffers.
 *
 * We use a very similar logic as for the number of CLOG buffers; see comments
 * in CLOGShmemBuffers.
 */
Size
CommitTsShmemBuffers(void)
{
  return Min(16, Max(4, NBuffers / 1024));
}

/*
 * Shared memory sizing for CommitTs
 */
Size
CommitTsShmemSize(void)
{
  return SimpleLruShmemSize(CommitTsShmemBuffers(), 0) + sizeof(CommitTimestampShared);
}

/*
 * Initialize CommitTs at system startup (postmaster start or standalone
 * backend)
 */
void
CommitTsShmemInit(void)
{
  bool found;

  CommitTsCtl->PagePrecedes = CommitTsPagePrecedes;
  SimpleLruInit(CommitTsCtl, "commit_timestamp", CommitTsShmemBuffers(), 0, CommitTsControlLock, "pg_commit_ts", LWTRANCHE_COMMITTS_BUFFERS);
  SlruPagePrecedesUnitTests(CommitTsCtl, COMMIT_TS_XACTS_PER_PAGE);

  commitTsShared = ShmemInitStruct("CommitTs shared", sizeof(CommitTimestampShared), &found);

  if (!IsUnderPostmaster)
  {
    Assert(!found);

    commitTsShared->xidLastCommit = InvalidTransactionId;
    TIMESTAMP_NOBEGIN(commitTsShared->dataLastCommit.time);
    commitTsShared->dataLastCommit.nodeid = InvalidRepOriginId;
    commitTsShared->commitTsActive = false;
  }
  else
  {

  }
}

/*
 * This function must be called ONCE on system install.
 *
 * (The CommitTs directory is assumed to have been created by initdb, and
 * CommitTsShmemInit must have been called already.)
 */
void
BootStrapCommitTs(void)
{
  /*
   * Nothing to do here at present, unlike most other SLRU modules; segments
   * are created when the server is started with this module enabled. See
   * ActivateCommitTs.
   */
}

/*
 * Initialize (or reinitialize) a page of CommitTs to zeroes.
 * If writeXlog is true, also emit an XLOG record saying we did this.
 *
 * The page is not actually written, just set up in shared memory.
 * The slot number of the new page is returned.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
ZeroCommitTsPage(int pageno, bool writeXlog)
{










}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after StartupXLOG has initialized ShmemVariableCache->nextFullXid.
 */
void
StartupCommitTs(void)
{

}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after recovery has finished.
 */
void
CompleteCommitTsInitialization(void)
{
  /*
   * If the feature is not enabled, turn it off for good.  This also removes
   * any leftover data.
   *
   * Conversely, we activate the module if the feature is enabled.  This is
   * necessary for primary and standby as the activation depends on the
   * control file contents at the beginning of recovery or when a
   * XLOG_PARAMETER_CHANGE is replayed.
   */
  if (!track_commit_timestamp)
  {
    DeactivateCommitTs();
  }
  else
  {

  }
}

/*
 * Activate or deactivate CommitTs' upon reception of a XLOG_PARAMETER_CHANGE
 * XLog record during recovery.
 */
void
CommitTsParameterChange(bool newvalue, bool oldvalue)
{
























}

/*
 * Activate this module whenever necessary.
 *		This must happen during postmaster or standalone-backend
 *startup, or during WAL replay anytime the track_commit_timestamp setting is
 *		changed in the master.
 *
 * The reason why this SLRU needs separate activation/deactivation functions is
 * that it can be enabled/disabled during start and the activation/deactivation
 * on master is propagated to standby via replay. Other SLRUs don't have this
 * property and they can be just initialized during normal startup.
 *
 * This is in charge of creating the currently active segment, if it's not
 * already there.  The reason for this is that the server might have been
 * running with this module disabled for a while and thus might have skipped
 * the normal creation point.
 */
static void
ActivateCommitTs(void)
{


























































}

/*
 * Deactivate this module.
 *
 * This must be called when the track_commit_timestamp parameter is turned off.
 * This happens during postmaster or standalone-backend startup, or during WAL
 * replay.
 *
 * Resets CommitTs into invalid state to make sure we don't hand back
 * possibly-invalid data; also removes segments of old data.
 */
static void
DeactivateCommitTs(void)
{
  /*
   * Cleanup the status in the shared memory.
   *
   * We reset everything in the commitTsShared record to prevent user from
   * getting confusing data about last committed transaction on the standby
   * when the module was activated repeatedly on the primary.
   */
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);

  commitTsShared->commitTsActive = false;
  commitTsShared->xidLastCommit = InvalidTransactionId;
  TIMESTAMP_NOBEGIN(commitTsShared->dataLastCommit.time);
  commitTsShared->dataLastCommit.nodeid = InvalidRepOriginId;

  ShmemVariableCache->oldestCommitTsXid = InvalidTransactionId;
  ShmemVariableCache->newestCommitTsXid = InvalidTransactionId;

  LWLockRelease(CommitTsLock);

  /*
   * Remove *all* files.  This is necessary so that there are no leftover
   * files; in the case where this feature is later enabled after running
   * with it disabled for some time there may be a gap in the file sequence.
   * (We can probably tolerate out-of-sequence files, as they are going to
   * be overwritten anyway when we wrap around, but it seems better to be
   * tidy.)
   */
  LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);
  (void)SlruScanDirectory(CommitTsCtl, SlruScanDirCbDeleteAll, NULL);
  LWLockRelease(CommitTsControlLock);
}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
ShutdownCommitTs(void)
{
  /* Flush dirty CommitTs pages to disk */
  SimpleLruFlush(CommitTsCtl, false);

  /*
   * fsync pg_commit_ts to ensure that any files flushed previously are
   * durably on disk.
   */
  fsync_fname("pg_commit_ts", true);
}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 */
void
CheckPointCommitTs(void)
{
  /* Flush dirty CommitTs pages to disk */
  SimpleLruFlush(CommitTsCtl, true);
}

/*
 * Make sure that CommitTs has room for a newly-allocated XID.
 *
 * NB: this is called while holding XidGenLock.  We want it to be very fast
 * most of the time; even when it's not so fast, no actual I/O need happen
 * unless we're forced to write out a dirty CommitTs or xlog page to make room
 * in shared memory.
 *
 * NB: the current implementation relies on track_commit_timestamp being
 * PGC_POSTMASTER.
 */
void
ExtendCommitTs(TransactionId newestXact)
{
  int pageno;

  /*
   * Nothing to do if module not enabled.  Note we do an unlocked read of
   * the flag here, which is okay because this routine is only called from
   * GetNewTransactionId, which is never called in a standby.
   */
  Assert(!InRecovery);
  if (!commitTsShared->commitTsActive)
  {
    return;
  }

  /*
   * No work except at first XID of a page.  But beware: just after
   * wraparound, the first XID of page zero is FirstNormalTransactionId.
   */









  /* Zero the page and make an XLOG entry about it */



}

/*
 * Remove all CommitTs segments before the one holding the passed
 * transaction ID.
 *
 * Note that we don't need to flush XLOG here.
 */
void
TruncateCommitTs(TransactionId oldestXact)
{
  int cutoffPage;

  /*
   * The cutoff point is the start of the segment containing oldestXact. We
   * pass the *page* containing oldestXact to SimpleLruTruncate.
   */
  cutoffPage = TransactionIdToCTsPage(oldestXact);

  /* Check to see if there's any files that could be removed */
  if (!SlruScanDirectory(CommitTsCtl, SlruScanDirCbReportPresence, &cutoffPage))
  {
    return; /* nothing to remove */
  }

  /* Write XLOG record */


  /* Now we can remove the old CommitTs segment(s) */

}

/*
 * Set the limit values between which commit TS can be consulted.
 */
void
SetCommitTsLimit(TransactionId oldestXact, TransactionId newestXact)
{
  /*
   * Be careful not to overwrite values that are either further into the
   * "future" or signal a disabled committs.
   */
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  if (ShmemVariableCache->oldestCommitTsXid != InvalidTransactionId)
  {








  }
  else
  {
    Assert(ShmemVariableCache->newestCommitTsXid == InvalidTransactionId);
    ShmemVariableCache->oldestCommitTsXid = oldestXact;
    ShmemVariableCache->newestCommitTsXid = newestXact;
  }
  LWLockRelease(CommitTsLock);
}

/*
 * Move forwards the oldest commitTS value that can be consulted
 */
void
AdvanceOldestCommitTsXid(TransactionId oldestXact)
{
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  if (ShmemVariableCache->oldestCommitTsXid != InvalidTransactionId && TransactionIdPrecedes(ShmemVariableCache->oldestCommitTsXid, oldestXact))
  {

  }
  LWLockRelease(CommitTsLock);
}

/*
 * Decide whether a commitTS page number is "older" for truncation purposes.
 * Analogous to CLOGPagePrecedes().
 *
 * At default BLCKSZ, (1 << 31) % COMMIT_TS_XACTS_PER_PAGE == 128.  This
 * introduces differences compared to CLOG and the other SLRUs having (1 <<
 * 31) % per_page == 0.  This function never tests exactly
 * TransactionIdPrecedes(x-2^31, x).  When the system reaches xidStopLimit,
 * there are two possible counts of page boundaries between oldestXact and the
 * latest XID assigned, depending on whether oldestXact is within the first
 * 128 entries of its page.  Since this function doesn't know the location of
 * oldestXact within page2, it returns false for one page that actually is
 * expendable.  This is a wider (yet still negligible) version of the
 * truncation opportunity that CLOGPagePrecedes() cannot recognize.
 *
 * For the sake of a worked example, number entries with decimal values such
 * that page1==1 entries range from 1.0 to 1.999.  Let N+0.15 be the number of
 * pages that 2^31 entries will span (N is an integer).  If oldestXact=N+2.1,
 * then the final safe XID assignment leaves newestXact=1.95.  We keep page 2,
 * because entry=2.85 is the border that toggles whether entries precede the
 * last entry of the oldestXact page.  While page 2 is expendable at
 * oldestXact=N+2.1, it would be precious at oldestXact=N+2.9.
 */
static bool
CommitTsPagePrecedes(int page1, int page2)
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
 */
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXid)
{








}

/*
 * Write a SETTS xlog record
 */
static void
WriteSetTimestampXlogRec(TransactionId mainxid, int nsubxids, TransactionId *subxids, TimestampTz timestamp, RepOriginId nodeid)
{










}

/*
 * CommitTS resource manager's routines
 */
void
commit_ts_redo(XLogReaderState *record)
{





























































}