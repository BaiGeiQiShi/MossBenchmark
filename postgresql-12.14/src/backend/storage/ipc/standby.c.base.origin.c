/*-------------------------------------------------------------------------
 *
 * standby.c
 *	  Misc functions used in Hot Standby mode.
 *
 *	All functions for handling RM_STANDBY_ID, which relate to
 *	AccessExclusiveLocks and starting snapshots for Hot Standby mode.
 *	Plus conflict recovery processing.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/ipc/standby.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/standby.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"

/* User-settable GUC parameters */
int vacuum_defer_cleanup_age;
int max_standby_archive_delay = 30 * 1000;
int max_standby_streaming_delay = 30 * 1000;

static HTAB *RecoveryLockLists;

/* Flags set by timeout handlers */
static volatile sig_atomic_t got_standby_deadlock_timeout = false;
static volatile sig_atomic_t got_standby_delay_timeout = false;
static volatile sig_atomic_t got_standby_lock_timeout = false;

static void
ResolveRecoveryConflictWithVirtualXIDs(VirtualTransactionId *waitlist, ProcSignalReason reason, bool report_waiting);
static void
SendRecoveryConflictWithBufferPin(ProcSignalReason reason);
static XLogRecPtr
LogCurrentRunningXacts(RunningTransactions CurrRunningXacts);
static void
LogAccessExclusiveLocks(int nlocks, xl_standby_lock *locks);

/*
 * Keep track of all the locks owned by a given transaction.
 */
typedef struct RecoveryLockListsEntry
{
  TransactionId xid;
  List *locks;
} RecoveryLockListsEntry;

/*
 * InitRecoveryTransactionEnvironment
 *		Initialize tracking of in-progress transactions in master
 *
 * We need to issue shared invalidations and hold locks. Holding locks
 * means others may want to wait on us, so we need to make a lock table
 * vxact entry like a real transaction. We could create and delete
 * lock table entries for each transaction but its simpler just to create
 * one permanent entry and leave it there all the time. Locks are then
 * acquired and released as needed. Yes, this means you can see the
 * Startup process in pg_locks once we have run this.
 */
void
InitRecoveryTransactionEnvironment(void)
{





































}

/*
 * ShutdownRecoveryTransactionEnvironment
 *		Shut down transaction tracking
 *
 * Prepare to switch from hot standby mode to normal operation. Shut down
 * recovery-time transaction tracking.
 *
 * This must be called even in shutdown of startup process if transaction
 * tracking has been initialized. Otherwise some locks the tracked
 * transactions were holding will not be released and and may interfere with
 * the processes still running (but will exit soon later) at the exit of
 * startup process.
 */
void
ShutdownRecoveryTransactionEnvironment(void)
{























}

/*
 * -----------------------------------------------------
 *		Standby wait timers and backend cancel logic
 * -----------------------------------------------------
 */

/*
 * Determine the cutoff time at which we want to start canceling conflicting
 * transactions.  Returns zero (a time safely in the past) if we are willing
 * to wait forever.
 */
static TimestampTz
GetStandbyLimitTime(void)
{
























}

#define STANDBY_INITIAL_WAIT_US 1000
static int standbyWait_us = STANDBY_INITIAL_WAIT_US;

/*
 * Standby wait logic for ResolveRecoveryConflictWithVirtualXIDs.
 * We wait here for a while then return. If we decide we can't wait any
 * more then we return true, if we can wait some more return false.
 */
static bool
WaitExceedsMaxStandbyDelay(void)
{



























}

/*
 * This is the main executioner for any query backend that conflicts with
 * recovery processing. Judgement has already been passed on it within
 * a specific rmgr. Here we just issue the orders to the procs. The procs
 * then throw the required error as instructed.
 *
 * If report_waiting is true, "waiting" is reported in PS display if necessary.
 * If the caller has already reported that, report_waiting should be false.
 * Otherwise, "waiting" is reported twice unexpectedly.
 */
static void
ResolveRecoveryConflictWithVirtualXIDs(VirtualTransactionId *waitlist, ProcSignalReason reason, bool report_waiting)
{








































































}

void
ResolveRecoveryConflictWithSnapshot(TransactionId latestRemovedXid, RelFileNode node)
{





















}

void
ResolveRecoveryConflictWithTablespace(Oid tsid)
{





















}

void
ResolveRecoveryConflictWithDatabase(Oid dbid)
{





















}

/*
 * ResolveRecoveryConflictWithLock is called from ProcSleep()
 * to resolve conflicts with other backends holding relation locks.
 *
 * The WaitLatch sleep normally done in ProcSleep()
 * (when not InHotStandby) is performed here, for code clarity.
 *
 * We either resolve conflicts immediately or set a timeout to wake us at
 * the limit of our patience.
 *
 * Resolve conflicts by canceling to all backends holding a conflicting
 * lock.  As we are already queued to be granted the lock, no new lock
 * requests conflicting with ours will be granted in the meantime.
 *
 * We also must check for deadlocks involving the Startup process and
 * hot-standby backend processes. If deadlock_timeout is reached in
 * this function, all the backends holding the conflicting locks are
 * requested to check themselves for deadlocks.
 */
void
ResolveRecoveryConflictWithLock(LOCKTAG locktag)
{










































































































}

/*
 * ResolveRecoveryConflictWithBufferPin is called from LockBufferForCleanup()
 * to resolve conflicts with other backends holding buffer pins.
 *
 * The ProcWaitForSignal() sleep normally done in LockBufferForCleanup()
 * (when not InHotStandby) is performed here, for code clarity.
 *
 * We either resolve conflicts immediately or set a timeout to wake us at
 * the limit of our patience.
 *
 * Resolve conflicts by sending a PROCSIG signal to all backends to check if
 * they hold one of the buffer pins that is blocking Startup process. If so,
 * those backends will take an appropriate error action, ERROR or FATAL.
 *
 * We also must check for deadlocks.  Deadlocks occur because if queries
 * wait on a lock, that must be behind an AccessExclusiveLock, which can only
 * be cleared if the Startup process replays a transaction completion record.
 * If Startup process is also waiting then that is a deadlock. The deadlock
 * can occur if the query is waiting and then the Startup sleeps, or if
 * Startup is sleeping and the query waits on a lock. We protect against
 * only the former sequence here, the latter sequence is checked prior to
 * the query sleeping, in CheckRecoveryConflictDeadlock().
 *
 * Deadlocks are extremely rare, and relatively expensive to check for,
 * so we don't do a deadlock check right away ... only if we have had to wait
 * at least deadlock_timeout.
 */
void
ResolveRecoveryConflictWithBufferPin(void)
{












































































}

static void
SendRecoveryConflictWithBufferPin(ProcSignalReason reason)
{









}

/*
 * In Hot Standby perform early deadlock detection.  We abort the lock
 * wait if we are about to sleep while holding the buffer pin that Startup
 * process is waiting for.
 *
 * Note: this code is pessimistic, because there is no way for it to
 * determine whether an actual deadlock condition is present: the lock we
 * need to wait for might be unrelated to any held by the Startup process.
 * Sooner or later, this mechanism should get ripped out in favor of somehow
 * accounting for buffer locks in DeadLockCheck().  However, errors here
 * seem to be very low-probability in practice, so for now it's not worth
 * the trouble.
 */
void
CheckRecoveryConflictDeadlock(void)
{















}

/* --------------------------------
 *		timeout handler routines
 * --------------------------------
 */

/*
 * StandbyDeadLockHandler() will be called if STANDBY_DEADLOCK_TIMEOUT is
 * exceeded.
 */
void
StandbyDeadLockHandler(void)
{

}

/*
 * StandbyTimeoutHandler() will be called if STANDBY_TIMEOUT is exceeded.
 */
void
StandbyTimeoutHandler(void)
{

}

/*
 * StandbyLockTimeoutHandler() will be called if STANDBY_LOCK_TIMEOUT is
 * exceeded.
 */
void
StandbyLockTimeoutHandler(void)
{

}

/*
 * -----------------------------------------------------
 * Locking in Recovery Mode
 * -----------------------------------------------------
 *
 * All locks are held by the Startup process using a single virtual
 * transaction. This implementation is both simpler and in some senses,
 * more correct. The locks held mean "some original transaction held
 * this lock, so query access is not allowed at this time". So the Startup
 * process is the proxy by which the original locks are implemented.
 *
 * We only keep track of AccessExclusiveLocks, which are only ever held by
 * one transaction on one relation.
 *
 * We keep a hash table of lists of locks in local memory keyed by xid,
 * RecoveryLockLists, so we can keep track of the various entries made by
 * the Startup process's virtual xid in the shared lock table.
 *
 * List elements use type xl_standby_lock, since the WAL record type exactly
 * matches the information that we need to keep track of.
 *
 * We use session locks rather than normal locks so we don't need
 * ResourceOwners.
 */

void
StandbyAcquireAccessExclusiveLock(TransactionId xid, Oid dbOid, Oid relOid)
{

































}

static void
StandbyReleaseLockList(List *locks)
{















}

static void
StandbyReleaseLocks(TransactionId xid)
{














}

/*
 * Release locks for a transaction tree, starting at xid down, from
 * RecoveryLockLists.
 *
 * Called during WAL replay of COMMIT/ROLLBACK when in hot standby mode,
 * to remove any AccessExclusiveLocks requested by a transaction.
 */
void
StandbyReleaseLockTree(TransactionId xid, int nsubxids, TransactionId *subxids)
{








}

/*
 * Called at end of recovery and when we see a shutdown checkpoint.
 */
void
StandbyReleaseAllLocks(void)
{











}

/*
 * StandbyReleaseOldLocks
 *		Release standby locks held by top-level XIDs that aren't
 *running, as long as they're not prepared transactions.
 */
void
StandbyReleaseOldLocks(TransactionId oldxid)
{
























}

/*
 * --------------------------------------------------------------------
 *		Recovery handling for Rmgr RM_STANDBY_ID
 *
 * These record types will only be created if XLogStandbyInfoActive()
 * --------------------------------------------------------------------
 */

void
standby_redo(XLogReaderState *record)
{














































}

/*
 * Log details of the current snapshot to WAL. This allows the snapshot state
 * to be reconstructed on the standby and for logical decoding.
 *
 * This is used for Hot Standby as follows:
 *
 * We can move directly to STANDBY_SNAPSHOT_READY at startup if we
 * start from a shutdown checkpoint because we know nothing was running
 * at that time and our recovery snapshot is known empty. In the more
 * typical case of an online checkpoint we need to jump through a few
 * hoops to get a correct recovery snapshot and this requires a two or
 * sometimes a three stage process.
 *
 * The initial snapshot must contain all running xids and all current
 * AccessExclusiveLocks at a point in time on the standby. Assembling
 * that information while the server is running requires many and
 * various LWLocks, so we choose to derive that information piece by
 * piece and then re-assemble that info on the standby. When that
 * information is fully assembled we move to STANDBY_SNAPSHOT_READY.
 *
 * Since locking on the primary when we derive the information is not
 * strict, we note that there is a time window between the derivation and
 * writing to WAL of the derived information. That allows race conditions
 * that we must resolve, since xids and locks may enter or leave the
 * snapshot during that window. This creates the issue that an xid or
 * lock may start *after* the snapshot has been derived yet *before* the
 * snapshot is logged in the running xacts WAL record. We resolve this by
 * starting to accumulate changes at a point just prior to when we derive
 * the snapshot on the primary, then ignore duplicates when we later apply
 * the snapshot from the running xacts record. This is implemented during
 * CreateCheckpoint() where we use the logical checkpoint location as
 * our starting point and then write the running xacts record immediately
 * before writing the main checkpoint WAL record. Since we always start
 * up from a checkpoint and are immediately at our starting point, we
 * unconditionally move to STANDBY_INITIALIZED. After this point we
 * must do 4 things:
 *	* move shared nextFullXid forwards as we see new xids
 *	* extend the clog and subtrans with each new xid
 *	* keep track of uncommitted known assigned xids
 *	* keep track of uncommitted AccessExclusiveLocks
 *
 * When we see a commit/abort we must remove known assigned xids and locks
 * from the completing transaction. Attempted removals that cannot locate
 * an entry are expected and must not cause an error when we are in state
 * STANDBY_INITIALIZED. This is implemented in StandbyReleaseLocks() and
 * KnownAssignedXidsRemove().
 *
 * Later, when we apply the running xact data we must be careful to ignore
 * transactions already committed, since those commits raced ahead when
 * making WAL entries.
 *
 * The loose timing also means that locks may be recorded that have a
 * zero xid, since xids are removed from procs before locks are removed.
 * So we must prune the lock list down to ensure we hold locks only for
 * currently running xids, performed by StandbyReleaseOldLocks().
 * Zero xids should no longer be possible, but we may be replaying WAL
 * from a time when they were possible.
 *
 * For logical decoding only the running xacts information is needed;
 * there's no need to look at the locking information, but it's logged anyway,
 * as there's no independent knob to just enable logical decoding. For
 * details of how this is used, check snapbuild.c's introductory comment.
 *
 *
 * Returns the RecPtr of the last inserted record.
 */
XLogRecPtr
LogStandbySnapshot(void)
{




















































}

/*
 * Record an enhanced snapshot of running transactions into WAL.
 *
 * The definitions of RunningTransactionsData and xl_xact_running_xacts are
 * similar. We keep them separate because xl_xact_running_xacts is a
 * contiguous chunk of memory and never exists fully until it is assembled in
 * WAL. The inserted records are marked as not being important for durability,
 * to avoid triggering superfluous checkpoint / archiving activity.
 */
static XLogRecPtr
LogCurrentRunningXacts(RunningTransactions CurrRunningXacts)
{












































}

/*
 * Wholesale logging of AccessExclusiveLocks. Other lock types need not be
 * logged, as described in backend/storage/lmgr/README.
 */
static void
LogAccessExclusiveLocks(int nlocks, xl_standby_lock *locks)
{










}

/*
 * Individual logging of AccessExclusiveLocks for use during LockAcquire()
 */
void
LogAccessExclusiveLock(Oid dbOid, Oid relOid)
{









}

/*
 * Prepare to log an AccessExclusiveLock, for use during LockAcquire()
 */
void
LogAccessExclusiveLockPrepare(void)
{













}

/*
 * Emit WAL for invalidations. This currently is only used for commits without
 * an xid but which contain invalidations.
 */
void
LogStandbyInvalidations(int nmsgs, SharedInvalidationMessage *msgs, bool relcacheInitFileInval)
{














}