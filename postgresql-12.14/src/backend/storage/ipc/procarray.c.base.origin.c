/*-------------------------------------------------------------------------
 *
 * procarray.c
 *	  POSTGRES process array code.
 *
 *
 * This module maintains arrays of the PGPROC and PGXACT structures for all
 * active backends.  Although there are several uses for this, the principal
 * one is as a means of determining the set of currently running transactions.
 *
 * Because of various subtle race conditions it is critical that a backend
 * hold the correct locks while setting or clearing its MyPgXact->xid field.
 * See notes in src/backend/access/transam/README.
 *
 * The process arrays now also include structures representing prepared
 * transactions.  The xid and subxids fields of these are valid, as are the
 * myProcLocks lists.  They can be distinguished from regular backend PGPROCs
 * at need by checking for pid == 0.
 *
 * During hot standby, we also keep a list of XIDs representing transactions
 * that are known to be running in the master (or more precisely, were running
 * as of the current point in the WAL stream).  This list is kept in the
 * KnownAssignedXids array, and is updated by watching the sequence of
 * arriving XIDs.  This is necessary because if we leave those XIDs out of
 * snapshots taken for standby queries, then they will appear to be already
 * complete, leading to MVCC failures.  Note that in hot standby, the PGPROC
 * array represents standby processes, which by definition are not running
 * transactions that have XIDs.
 *
 * It is perhaps possible for a backend on the master to terminate without
 * writing an abort record for its transaction.  While that shouldn't really
 * happen, it would tie up KnownAssignedXids indefinitely, so we protect
 * ourselves by pruning the array when a valid list of running XIDs arrives.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/ipc/procarray.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>

#include "access/clog.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

#define UINT32_ACCESS_ONCE(var) ((uint32)(*((volatile uint32 *)&(var))))

/* Our shared memory area */
typedef struct ProcArrayStruct
{
  int numProcs; /* number of valid procs entries */
  int maxProcs; /* allocated size of procs array */

  /*
   * Known assigned XIDs handling
   */
  int maxKnownAssignedXids;        /* allocated size of array */
  int numKnownAssignedXids;        /* current # of valid entries */
  int tailKnownAssignedXids;       /* index of oldest valid element */
  int headKnownAssignedXids;       /* index of newest element, + 1 */
  slock_t known_assigned_xids_lck; /* protects head/tail pointers */

  /*
   * Highest subxid that has been removed from KnownAssignedXids array to
   * prevent overflow; or InvalidTransactionId if none.  We track this for
   * similar reasons to tracking overflowing cached subxids in PGXACT
   * entries.  Must hold exclusive ProcArrayLock to change this, and shared
   * lock to read it.
   */
  TransactionId lastOverflowedXid;

  /* oldest xmin of any replication slot */
  TransactionId replication_slot_xmin;
  /* oldest catalog xmin of any replication slot */
  TransactionId replication_slot_catalog_xmin;

  /* indexes into allPgXact[], has PROCARRAY_MAXPROCS entries */
  int pgprocnos[FLEXIBLE_ARRAY_MEMBER];
} ProcArrayStruct;

static ProcArrayStruct *procArray;

static PGPROC *allProcs;
static PGXACT *allPgXact;

/*
 * Reason codes for KnownAssignedXidsCompress().
 */
typedef enum KAXCompressReason
{
  KAX_NO_SPACE,            /* need to free up space at array end */
  KAX_PRUNE,               /* we just pruned old entries */
  KAX_TRANSACTION_END,     /* we just committed/removed some XIDs */
  KAX_STARTUP_PROCESS_IDLE /* startup process is about to sleep */
} KAXCompressReason;

/*
 * Cache to reduce overhead of repeated calls to TransactionIdIsInProgress()
 */
static TransactionId cachedXidIsNotInProgress = InvalidTransactionId;

/*
 * Bookkeeping for tracking emulated transactions in recovery
 */
static TransactionId *KnownAssignedXids;
static bool *KnownAssignedXidsValid;
static TransactionId latestObservedXid = InvalidTransactionId;

/*
 * If we're in STANDBY_SNAPSHOT_PENDING state, standbySnapshotPendingXmin is
 * the highest xid that might still be running that we don't have in
 * KnownAssignedXids.
 */
static TransactionId standbySnapshotPendingXmin;

#ifdef XIDCACHE_DEBUG

/* counters for XidCache measurement */
static long xc_by_recent_xmin = 0;
static long xc_by_known_xact = 0;
static long xc_by_my_xact = 0;
static long xc_by_latest_xid = 0;
static long xc_by_main_xid = 0;
static long xc_by_child_xid = 0;
static long xc_by_known_assigned = 0;
static long xc_no_overflow = 0;
static long xc_slow_answer = 0;

#define xc_by_recent_xmin_inc() (xc_by_recent_xmin++)
#define xc_by_known_xact_inc() (xc_by_known_xact++)
#define xc_by_my_xact_inc() (xc_by_my_xact++)
#define xc_by_latest_xid_inc() (xc_by_latest_xid++)
#define xc_by_main_xid_inc() (xc_by_main_xid++)
#define xc_by_child_xid_inc() (xc_by_child_xid++)
#define xc_by_known_assigned_inc() (xc_by_known_assigned++)
#define xc_no_overflow_inc() (xc_no_overflow++)
#define xc_slow_answer_inc() (xc_slow_answer++)

static void
DisplayXidCache(void);
#else /* !XIDCACHE_DEBUG */

#define xc_by_recent_xmin_inc() ((void)0)
#define xc_by_known_xact_inc() ((void)0)
#define xc_by_my_xact_inc() ((void)0)
#define xc_by_latest_xid_inc() ((void)0)
#define xc_by_main_xid_inc() ((void)0)
#define xc_by_child_xid_inc() ((void)0)
#define xc_by_known_assigned_inc() ((void)0)
#define xc_no_overflow_inc() ((void)0)
#define xc_slow_answer_inc() ((void)0)
#endif /* XIDCACHE_DEBUG */

static VirtualTransactionId *
GetVirtualXIDsDelayingChkptGuts(int *nvxids, int type);
static bool
HaveVirtualXIDsDelayingChkptGuts(VirtualTransactionId *vxids, int nvxids, int type);

/* Primitives for KnownAssignedXids array handling for standby */
static void
KnownAssignedXidsCompress(KAXCompressReason reason, bool haveLock);
static void
KnownAssignedXidsAdd(TransactionId from_xid, TransactionId to_xid, bool exclusive_lock);
static bool
KnownAssignedXidsSearch(TransactionId xid, bool remove);
static bool
KnownAssignedXidExists(TransactionId xid);
static void
KnownAssignedXidsRemove(TransactionId xid);
static void
KnownAssignedXidsRemoveTree(TransactionId xid, int nsubxids, TransactionId *subxids);
static void
KnownAssignedXidsRemovePreceding(TransactionId xid);
static int
KnownAssignedXidsGet(TransactionId *xarray, TransactionId xmax);
static int
KnownAssignedXidsGetAndSetXmin(TransactionId *xarray, TransactionId *xmin, TransactionId xmax);
static TransactionId
KnownAssignedXidsGetOldestXmin(void);
static void
KnownAssignedXidsDisplay(int trace_level);
static void
KnownAssignedXidsReset(void);
static inline void
ProcArrayEndTransactionInternal(PGPROC *proc, PGXACT *pgxact, TransactionId latestXid);
static void
ProcArrayGroupClearXid(PGPROC *proc, TransactionId latestXid);

/*
 * Report shared-memory space needed by CreateSharedProcArray.
 */
Size
ProcArrayShmemSize(void)
{






























}

/*
 * Initialize the shared PGPROC array during postmaster startup.
 */
void
CreateSharedProcArray(void)
{


































}

/*
 * Add the specified PGPROC to the shared array.
 */
void
ProcArrayAdd(PGPROC *proc)
{










































}

/*
 * Remove the specified PGPROC from the shared array.
 *
 * When latestXid is a valid XID, we are removing a live 2PC gxact from the
 * array, and thus causing it to appear as "not running" anymore.  In this
 * case we must advance latestCompletedXid.  (This is essentially the same
 * as ProcArrayEndTransaction followed by removal of the PGPROC, but we take
 * the ProcArrayLock only once, and don't damage the content of the PGPROC;
 * twophase.c depends on the latter.)
 */
void
ProcArrayRemove(PGPROC *proc, TransactionId latestXid)
{














































}

/*
 * ProcArrayEndTransaction -- mark a transaction as no longer running
 *
 * This is used interchangeably for commit and abort cases.  The transaction
 * commit/abort must already be reported to WAL and pg_xact.
 *
 * proc is currently always MyProc, but we pass it explicitly for flexibility.
 * latestXid is the latest Xid among the transaction's main XID and
 * subtransactions, or InvalidTransactionId if it has no XID.  (We must ask
 * the caller to pass latestXid, instead of computing it from the PGPROC's
 * contents, because the subxid information in the PGPROC might be
 * incomplete.)
 */
void
ProcArrayEndTransaction(PGPROC *proc, TransactionId latestXid)
{


















































}

/*
 * Mark a write transaction as no longer running.
 *
 * We don't do any locking here; caller must handle that.
 */
static inline void
ProcArrayEndTransactionInternal(PGPROC *proc, PGXACT *pgxact, TransactionId latestXid)
{





















}

/*
 * ProcArrayGroupClearXid -- group XID clearing
 *
 * When we cannot immediately acquire ProcArrayLock in exclusive mode at
 * commit time, add ourselves to a list of processes that need their XIDs
 * cleared.  The first process to add itself to the list will acquire
 * ProcArrayLock in exclusive mode and perform ProcArrayEndTransactionInternal
 * on behalf of all group members.  This avoids a great deal of contention
 * around ProcArrayLock when many processes are trying to commit at once,
 * since the lock need not be repeatedly handed off from one committing
 * process to the next.
 */
static void
ProcArrayGroupClearXid(PGPROC *proc, TransactionId latestXid)
{











































































































}

/*
 * ProcArrayClearTransaction -- clear the transaction fields
 *
 * This is used after successfully preparing a 2-phase transaction.  We are
 * not actually reporting the transaction's XID as no longer running --- it
 * will still appear as running because the 2PC's gxact is in the ProcArray
 * too.  We just have to clear out our own PGXACT.
 */
void
ProcArrayClearTransaction(PGPROC *proc)
{




















}

/*
 * ProcArrayInitRecovery -- initialize recovery xid mgmt environment
 *
 * Remember up to where the startup process initialized the CLOG and subtrans
 * so we can ensure it's initialized gaplessly up to the point where necessary
 * while in recovery.
 */
void
ProcArrayInitRecovery(TransactionId initializedUptoXID)
{











}

/*
 * ProcArrayApplyRecoveryInfo -- apply recovery info about xids
 *
 * Takes us through 3 states: Initialized, Pending and Ready.
 * Normal case is to go all the way to Ready straight away, though there
 * are atypical cases where we need to take it in steps.
 *
 * Use the data about running transactions on master to create the initial
 * state of KnownAssignedXids. We also use these records to regularly prune
 * KnownAssignedXids because we know it is possible that some transactions
 * with FATAL errors fail to write abort records, which could cause eventual
 * overflow.
 *
 * See comments for LogStandbySnapshot().
 */
void
ProcArrayApplyRecoveryInfo(RunningTransactions running)
{












































































































































































































































}

/*
 * ProcArrayApplyXidAssignment
 *		Process an XLOG_XACT_ASSIGNMENT WAL record
 */
void
ProcArrayApplyXidAssignment(TransactionId topxid, int nsubxids, TransactionId *subxids)
{


























































}

/*
 * TransactionIdIsInProgress -- is given transaction running in some backend
 *
 * Aside from some shortcuts such as checking RecentXmin and our own Xid,
 * there are four possibilities for finding a running transaction:
 *
 * 1. The given Xid is a main transaction Id.  We will find this out cheaply
 * by looking at the PGXACT struct for each backend.
 *
 * 2. The given Xid is one of the cached subxact Xids in the PGPROC array.
 * We can find this out cheaply too.
 *
 * 3. In Hot Standby mode, we must search the KnownAssignedXids list to see
 * if the Xid is running on the master.
 *
 * 4. Search the SubTrans tree to find the Xid's topmost parent, and then see
 * if that is running according to PGXACT or KnownAssignedXids.  This is the
 * slowest way, but sadly it has to be done always if the others failed,
 * unless we see that the cached subxact sets are complete (none have
 * overflowed).
 *
 * ProcArrayLock has to be held while we do 1, 2, 3.  If we save the top Xids
 * while doing 1 and 3, we can release the ProcArrayLock while we do 4.
 * This buys back some concurrency (and we can't retrieve the main Xids from
 * PGXACT again anyway; see GetNewTransactionId).
 */
bool
TransactionIdIsInProgress(TransactionId xid)
{































































































































































































































}

/*
 * TransactionIdIsActive -- is xid the top-level XID of an active backend?
 *
 * This differs from TransactionIdIsInProgress in that it ignores prepared
 * transactions, as well as transactions running on the master if we're in
 * hot standby.  Also, we ignore subtransactions since that's not needed
 * for current uses.
 */
bool
TransactionIdIsActive(TransactionId xid)
{













































}

/*
 * GetOldestXmin -- returns oldest transaction that was running
 *					when any current transaction was
 *started.
 *
 * If rel is NULL or a shared relation, all backends are considered, otherwise
 * only backends running in this database are considered.
 *
 * The flags are used to ignore the backends in calculation when any of the
 * corresponding flags is set. Typically, if you want to ignore ones with
 * PROC_IN_VACUUM flag, you can use PROCARRAY_FLAGS_VACUUM.
 *
 * PROCARRAY_SLOTS_XMIN causes GetOldestXmin to ignore the xmin and
 * catalog_xmin of any replication slots that exist in the system when
 * calculating the oldest xmin.
 *
 * This is used by VACUUM to decide which deleted tuples must be preserved in
 * the passed in table. For shared relations backends in all databases must be
 * considered, but for non-shared relations that's not required, since only
 * backends in my own database could ever see the tuples in them. Also, we can
 * ignore concurrently running lazy VACUUMs because (a) they must be working
 * on other tables, and (b) they don't need to do snapshot-based lookups.
 *
 * This is also used to determine where to truncate pg_subtrans.  For that
 * backends in all databases have to be considered, so rel = NULL has to be
 * passed in.
 *
 * Note: we include all currently running xids in the set of considered xids.
 * This ensures that if a just-started xact has not yet set its snapshot,
 * when it does set the snapshot it cannot set xmin less than what we compute.
 * See notes in src/backend/access/transam/README.
 *
 * Note: despite the above, it's possible for the calculated value to move
 * backwards on repeated calls. The calculated value is conservative, so that
 * anything older is definitely not considered as running by anyone anymore,
 * but the exact value calculated depends on a number of things. For example,
 * if rel = NULL and there are no transactions running in the current
 * database, GetOldestXmin() returns latestCompletedXid. If a transaction
 * begins after that, its xmin will include in-progress transactions in other
 * databases that started earlier, so another call will return a lower value.
 * Nonetheless it is safe to vacuum a table in the current database with the
 * first result.  There are also replication-related effects: a walsender
 * process can set its xmin based on transactions that are no longer running
 * in the master but are still being replayed on the standby, thus possibly
 * making the GetOldestXmin reading go backwards.  In this case there is a
 * possibility that we lose data that the standby would like to have, but
 * unless the standby uses a replication slot to make its xmin persistent
 * there is little we can do about that --- data is only protected if the
 * walsender runs continuously while queries are executed on the standby.
 * (The Hot Standby code deals with such cases by failing standby queries
 * that needed to access already-removed data, so there's no integrity bug.)
 * The return value is also adjusted with vacuum_defer_cleanup_age, so
 * increasing that setting on the fly is another easy way to make
 * GetOldestXmin() move backwards, with no consequences for data integrity.
 */
TransactionId
GetOldestXmin(Relation rel, int flags)
{










































































































































}

/*
 * GetMaxSnapshotXidCount -- get max size for snapshot XID array
 *
 * We have to export this for use by snapmgr.c.
 */
int
GetMaxSnapshotXidCount(void)
{

}

/*
 * GetMaxSnapshotSubxidCount -- get max size for snapshot sub-XID array
 *
 * We have to export this for use by snapmgr.c.
 */
int
GetMaxSnapshotSubxidCount(void)
{

}

/*
 * GetSnapshotData -- returns information about running transactions.
 *
 * The returned snapshot includes xmin (lowest still-running xact ID),
 * xmax (highest completed xact ID + 1), and a list of running xact IDs
 * in the range xmin <= xid < xmax.  It is used as follows:
 *		All xact IDs < xmin are considered finished.
 *		All xact IDs >= xmax are considered still running.
 *		For an xact ID xmin <= xid < xmax, consult list to see whether
 *		it is considered running or not.
 * This ensures that the set of transactions seen as "running" by the
 * current xact will not change after it takes the snapshot.
 *
 * All running top-level XIDs are included in the snapshot, except for lazy
 * VACUUM processes.  We also try to include running subtransaction XIDs,
 * but since PGPROC has only a limited cache area for subxact XIDs, full
 * information may not be available.  If we find any overflowed subxid arrays,
 * we have to mark the snapshot's subxid data as overflowed, and extra work
 * *may* need to be done to determine what's running (see XidInMVCCSnapshot()
 * in heapam_visibility.c).
 *
 * We also update the following backend-global variables:
 *		TransactionXmin: the oldest xmin of any snapshot in use in the
 *			current transaction (this is the same as
 *MyPgXact->xmin). RecentXmin: the xmin computed for the most recent snapshot.
 *XIDs older than this are known not running any more. RecentGlobalXmin: the
 *global xmin (oldest TransactionXmin across all running transactions, except
 *those running LAZY VACUUM).  This is the same computation done by
 *			GetOldestXmin(NULL, PROCARRAY_FLAGS_VACUUM).
 *		RecentGlobalDataXmin: the global xmin for non-catalog tables
 *			>= RecentGlobalXmin
 *
 * Note: this function should probably not be called with an argument that's
 * not statically allocated (see xip allocation below).
 */
Snapshot
GetSnapshotData(Snapshot snapshot)
{

































































































































































































































































































}

/*
 * ProcArrayInstallImportedXmin -- install imported xmin into MyPgXact->xmin
 *
 * This is called when installing a snapshot imported from another
 * transaction.  To ensure that OldestXmin doesn't go backwards, we must
 * check that the source transaction is still running, and we'd better do
 * that atomically with installing the new xmin.
 *
 * Returns true if successful, false if source xact is no longer running.
 */
bool
ProcArrayInstallImportedXmin(TransactionId xmin, VirtualTransactionId *sourcevxid)
{







































































}

/*
 * ProcArrayInstallRestoredXmin -- install restored xmin into MyPgXact->xmin
 *
 * This is like ProcArrayInstallImportedXmin, but we have a pointer to the
 * PGPROC of the transaction from which we imported the snapshot, rather than
 * an XID.
 *
 * Returns true if successful, false if source xact is no longer running.
 */
bool
ProcArrayInstallRestoredXmin(TransactionId xmin, PGPROC *proc)
{




























}

/*
 * GetRunningTransactionData -- returns information about running transactions.
 *
 * Similar to GetSnapshotData but returns more information. We include
 * all PGXACTs with an assigned TransactionId, even VACUUM processes and
 * prepared transactions.
 *
 * We acquire XidGenLock and ProcArrayLock, but the caller is responsible for
 * releasing them. Acquiring XidGenLock ensures that no new XIDs enter the proc
 * array until the caller has WAL-logged this snapshot, and releases the
 * lock. Acquiring ProcArrayLock ensures that no transactions commit until the
 * lock is released.
 *
 * The returned data structure is statically allocated; caller should not
 * modify it, and must not assume it is valid past the next call.
 *
 * This is never executed during recovery so there is no need to look at
 * KnownAssignedXids.
 *
 * Dummy PGXACTs from prepared transaction are included, meaning that this
 * may return entries with duplicated TransactionId values coming from
 * transaction finishing to prepare.  Nothing is done about duplicated
 * entries here to not hold on ProcArrayLock more than necessary.
 *
 * We don't worry about updating other counters, we want to keep this as
 * simple as possible and leave GetSnapshotData() as the primary code for
 * that bookkeeping.
 *
 * Note that if any transaction has overflowed its cached subtransactions
 * then there is no real need include any subtransactions.
 */
RunningTransactions
GetRunningTransactionData(void)
{






























































































































































}

/*
 * GetOldestActiveTransactionId()
 *
 * Similar to GetSnapshotData but returns just oldestActiveXid. We include
 * all PGXACTs with an assigned TransactionId, even VACUUM processes.
 * We look at all databases, though there is no need to include WALSender
 * since this has no effect on hot standby conflicts.
 *
 * This is never executed during recovery so there is no need to look at
 * KnownAssignedXids.
 *
 * We don't worry about updating other counters, we want to keep this as
 * simple as possible and leave GetSnapshotData() as the primary code for
 * that bookkeeping.
 */
TransactionId
GetOldestActiveTransactionId(void)
{

















































}

/*
 * GetOldestSafeDecodingTransactionId -- lowest xid not affected by vacuum
 *
 * Returns the oldest xid that we can guarantee not to have been affected by
 * vacuum, i.e. no rows >= that xid have been vacuumed away unless the
 * transaction aborted. Note that the value can (and most of the time will) be
 * much more conservative than what really has been affected by vacuum, but we
 * currently don't have better data available.
 *
 * This is useful to initialize the cutoff xid after which a new changeset
 * extraction replication slot can start decoding changes.
 *
 * Must be called with ProcArrayLock held either shared or exclusively,
 * although most callers will want to use exclusive mode since it is expected
 * that the caller will immediately use the xid to peg the xmin horizon.
 */
TransactionId
GetOldestSafeDecodingTransactionId(bool catalogOnly)
{












































































}

/*
 * GetVirtualXIDsDelayingChkptGuts -- Get the VXIDs of transactions that are
 * delaying the start or end of a checkpoint because they have critical
 * actions in progress.
 *
 * Constructs an array of VXIDs of transactions that are currently in commit
 * critical sections, as shown by having specified delayChkpt or delayChkptEnd
 * set.
 *
 * Returns a palloc'd array that should be freed by the caller.
 * *nvxids is the number of valid entries.
 *
 * Note that because backends set or clear delayChkpt and delayChkptEnd
 * without holding any lock, the result is somewhat indeterminate, but we
 * don't really care.  Even in a multiprocessor with delayed writes to
 * shared memory, it should be certain that setting of delayChkpt will
 * propagate to shared memory when the backend takes a lock, so we cannot
 * fail to see a virtual xact as delayChkpt if it's already inserted its
 * commit record.  Whether it takes a little while for clearing of
 * delayChkpt to propagate is unimportant for correctness.
 */
static VirtualTransactionId *
GetVirtualXIDsDelayingChkptGuts(int *nvxids, int type)
{


































}

/*
 * GetVirtualXIDsDelayingChkpt - Get the VXIDs of transactions that are
 * delaying the start of a checkpoint.
 */
VirtualTransactionId *
GetVirtualXIDsDelayingChkpt(int *nvxids)
{

}

/*
 * GetVirtualXIDsDelayingChkptEnd - Get the VXIDs of transactions that are
 * delaying the end of a checkpoint.
 */
VirtualTransactionId *
GetVirtualXIDsDelayingChkptEnd(int *nvxids)
{

}

/*
 * HaveVirtualXIDsDelayingChkpt -- Are any of the specified VXIDs delaying?
 *
 * This is used with the results of GetVirtualXIDsDelayingChkpt to see if any
 * of the specified VXIDs are still in critical sections of code.
 *
 * Note: this is O(N^2) in the number of vxacts that are/were delaying, but
 * those numbers should be small enough for it not to be a problem.
 */
static bool
HaveVirtualXIDsDelayingChkptGuts(VirtualTransactionId *vxids, int nvxids, int type)
{







































}

/*
 * HaveVirtualXIDsDelayingChkpt -- Are any of the specified VXIDs delaying
 * the start of a checkpoint?
 */
bool
HaveVirtualXIDsDelayingChkpt(VirtualTransactionId *vxids, int nvxids)
{

}

/*
 * HaveVirtualXIDsDelayingChkptEnd -- Are any of the specified VXIDs delaying
 * the end of a checkpoint?
 */
bool
HaveVirtualXIDsDelayingChkptEnd(VirtualTransactionId *vxids, int nvxids)
{

}

/*
 * BackendPidGetProc -- get a backend's PGPROC given its PID
 *
 * Returns NULL if not found.  Note that it is up to the caller to be
 * sure that the question remains meaningful for long enough for the
 * answer to be used ...
 */
PGPROC *
BackendPidGetProc(int pid)
{














}

/*
 * BackendPidGetProcWithLock -- get a backend's PGPROC given its PID
 *
 * Same as above, except caller must be holding ProcArrayLock.  The found
 * entry, if any, can be assumed to be valid as long as the lock remains held.
 */
PGPROC *
BackendPidGetProcWithLock(int pid)
{





















}

/*
 * BackendXidGetPid -- get a backend's pid given its XID
 *
 * Returns 0 if not found or it's a prepared transaction.  Note that
 * it is up to the caller to be sure that the question remains
 * meaningful for long enough for the answer to be used ...
 *
 * Only main transaction Ids are considered.  This function is mainly
 * useful for determining what backend owns a lock.
 *
 * Beware that not every xact has an XID assigned.  However, as long as you
 * only call this using an XID found on disk, you're safe.
 */
int
BackendXidGetPid(TransactionId xid)
{



























}

/*
 * IsBackendPid -- is a given pid a running backend
 *
 * This is not called by the backend, but is called by external modules.
 */
bool
IsBackendPid(int pid)
{

}

/*
 * GetCurrentVirtualXIDs -- returns an array of currently active VXIDs.
 *
 * The array is palloc'd. The number of valid entries is returned into *nvxids.
 *
 * The arguments allow filtering the set of VXIDs returned.  Our own process
 * is always skipped.  In addition:
 *	If limitXmin is not InvalidTransactionId, skip processes with
 *		xmin > limitXmin.
 *	If excludeXmin0 is true, skip processes with xmin = 0.
 *	If allDbs is false, skip processes attached to other databases.
 *	If excludeVacuum isn't zero, skip processes for which
 *		(vacuumFlags & excludeVacuum) is not zero.
 *
 * Note: the purpose of the limitXmin and excludeXmin0 parameters is to
 * allow skipping backends whose oldest live snapshot is no older than
 * some snapshot we have.  Since we examine the procarray with only shared
 * lock, there are race conditions: a backend could set its xmin just after
 * we look.  Indeed, on multiprocessors with weak memory ordering, the
 * other backend could have set its xmin *before* we look.  We know however
 * that such a backend must have held shared ProcArrayLock overlapping our
 * own hold of ProcArrayLock, else we would see its xmin update.  Therefore,
 * any snapshot the other backend is taking concurrently with our scan cannot
 * consider any transactions as still running that we think are committed
 * (since backends must hold ProcArrayLock exclusive to commit).
 */
VirtualTransactionId *
GetCurrentVirtualXIDs(TransactionId limitXmin, bool excludeXmin0, bool allDbs, int excludeVacuum, int *nvxids)
{

























































}

/*
 * GetConflictingVirtualXIDs -- returns an array of currently active VXIDs.
 *
 * Usage is limited to conflict resolution during recovery on standby servers.
 * limitXmin is supplied as either latestRemovedXid, or InvalidTransactionId
 * in cases where we cannot accurately determine a value for latestRemovedXid.
 *
 * If limitXmin is InvalidTransactionId then we want to kill everybody,
 * so we're not worried if they have a snapshot or not, nor does it really
 * matter what type of lock we hold.
 *
 * All callers that are checking xmins always now supply a valid and useful
 * value for limitXmin. The limitXmin is always lower than the lowest
 * numbered KnownAssignedXid that is not already a FATAL error. This is
 * because we only care about cleanup records that are cleaning up tuple
 * versions from committed transactions. In that case they will only occur
 * at the point where the record is less than the lowest running xid. That
 * allows us to say that if any backend takes a snapshot concurrently with
 * us then the conflict assessment made here would never include the snapshot
 * that is being derived. So we take LW_SHARED on the ProcArray and allow
 * concurrent snapshots when limitXmin is valid. We might think about adding
 *	 Assert(limitXmin < lowest(KnownAssignedXids))
 * but that would not be true in the case of FATAL errors lagging in array,
 * but we already know those are bogus anyway, so we skip that test.
 *
 * If dbOid is valid we skip backends attached to other databases.
 *
 * Be careful to *not* pfree the result from this function. We reuse
 * this array sufficiently often that we use malloc for the result.
 */
VirtualTransactionId *
GetConflictingVirtualXIDs(TransactionId limitXmin, Oid dbOid)
{


































































}

/*
 * CancelVirtualTransaction - used in recovery conflict processing
 *
 * Returns pid of the process signaled, or 0 if not found.
 */
pid_t
CancelVirtualTransaction(VirtualTransactionId vxid, ProcSignalReason sigmode)
{

}

pid_t
SignalVirtualTransaction(VirtualTransactionId vxid, ProcSignalReason sigmode, bool conflictPending)
{

































}

/*
 * MinimumActiveBackends --- count backends (other than myself) that are
 *		in active transactions.  Return true if the count exceeds the
 *		minimum threshold passed.  This is used as a heuristic to decide
 *if a pre-XLOG-flush delay is worthwhile during commit.
 *
 * Do not count backends that are blocked waiting for locks, since they are
 * not going to get to run until someone else commits.
 */
bool
MinimumActiveBackends(int min)
{




























































}

/*
 * CountDBBackends --- count backends that are using specified database
 */
int
CountDBBackends(Oid databaseid)
{
























}

/*
 * CountDBConnections --- counts database backends ignoring any background
 *		worker processes
 */
int
CountDBConnections(Oid databaseid)
{




























}

/*
 * CancelDBBackends --- cancel backends that are using specified database
 */
void
CancelDBBackends(Oid databaseid, ProcSignalReason sigmode, bool conflictPending)
{
































}

/*
 * CountUserBackends --- count backends that are used by specified user
 */
int
CountUserBackends(Oid roleid)
{




























}

/*
 * CountOtherDBBackends -- check for other backends running in the given DB
 *
 * If there are other backends in the DB, we will wait a maximum of 5 seconds
 * for them to exit.  Autovacuum backends are encouraged to exit early by
 * sending them SIGTERM, but normal user backends are just waited for.
 *
 * The current backend is always ignored; it is caller's responsibility to
 * check whether the current backend uses the given DB, if it's important.
 *
 * Returns true if there are (still) other backends in the DB, false if not.
 * Also, *nbackends and *nprepared are set to the number of other backends
 * and prepared transactions in the DB, respectively.
 *
 * This function is used to interlock DROP DATABASE and related commands
 * against there being any active backends in the target DB --- dropping the
 * DB while active backends remain would be a Bad Thing.  Note that we cannot
 * detect here the possibility of a newly-started backend that is trying to
 * connect to the doomed database, so additional interlocking is needed during
 * backend startup.  The caller should normally hold an exclusive lock on the
 * target DB before calling this, which is one reason we mustn't wait
 * indefinitely.
 */
bool
CountOtherDBBackends(Oid databaseId, int *nbackends, int *nprepared)
{









































































}

/*
 * ProcArraySetReplicationSlotXmin
 *
 * Install limits to future computations of the xmin horizon to prevent vacuum
 * and HOT pruning from removing affected rows still needed by clients with
 * replication slots.
 */
void
ProcArraySetReplicationSlotXmin(TransactionId xmin, TransactionId catalog_xmin, bool already_locked)
{














}

/*
 * ProcArrayGetReplicationSlotXmin
 *
 * Return the current slot xmin limits. That's useful to be able to remove
 * data that's older than those limits.
 */
void
ProcArrayGetReplicationSlotXmin(TransactionId *xmin, TransactionId *catalog_xmin)
{













}

#define XidCacheRemove(i)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    MyProc->subxids.xids[i] = MyProc->subxids.xids[MyPgXact->nxids - 1];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    pg_write_barrier();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    MyPgXact->nxids--;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

/*
 * XidCacheRemoveRunningXids
 *
 * Remove a bunch of TransactionIds from the list of known-running
 * subtransactions for my backend.  Both the specified xid and those in
 * the xids[] array (of length nxids) are removed from the subxids cache.
 * latestXid must be the latest XID among the group.
 */
void
XidCacheRemoveRunningXids(TransactionId xid, int nxids, const TransactionId *xids, TransactionId latestXid)
{






































































}

#ifdef XIDCACHE_DEBUG

/*
 * Print stats about effectiveness of XID cache
 */
static void
DisplayXidCache(void)
{
  fprintf(stderr, "XidCache: xmin: %ld, known: %ld, myxact: %ld, latest: %ld, mainxid: %ld, childxid: %ld, knownassigned: %ld, nooflo: %ld, slow: %ld\n", xc_by_recent_xmin, xc_by_known_xact, xc_by_my_xact, xc_by_latest_xid, xc_by_main_xid, xc_by_child_xid, xc_by_known_assigned, xc_no_overflow, xc_slow_answer);
}
#endif /* XIDCACHE_DEBUG */

/* ----------------------------------------------
 *		KnownAssignedTransactions sub-module
 * ----------------------------------------------
 */

/*
 * In Hot Standby mode, we maintain a list of transactions that are (or were)
 * running in the master at the current point in WAL.  These XIDs must be
 * treated as running by standby transactions, even though they are not in
 * the standby server's PGXACT array.
 *
 * We record all XIDs that we know have been assigned.  That includes all the
 * XIDs seen in WAL records, plus all unobserved XIDs that we can deduce have
 * been assigned.  We can deduce the existence of unobserved XIDs because we
 * know XIDs are assigned in sequence, with no gaps.  The KnownAssignedXids
 * list expands as new XIDs are observed or inferred, and contracts when
 * transaction completion records arrive.
 *
 * During hot standby we do not fret too much about the distinction between
 * top-level XIDs and subtransaction XIDs. We store both together in the
 * KnownAssignedXids list.  In backends, this is copied into snapshots in
 * GetSnapshotData(), taking advantage of the fact that XidInMVCCSnapshot()
 * doesn't care about the distinction either.  Subtransaction XIDs are
 * effectively treated as top-level XIDs and in the typical case pg_subtrans
 * links are *not* maintained (which does not affect visibility).
 *
 * We have room in KnownAssignedXids and in snapshots to hold maxProcs *
 * (1 + PGPROC_MAX_CACHED_SUBXIDS) XIDs, so every master transaction must
 * report its subtransaction XIDs in a WAL XLOG_XACT_ASSIGNMENT record at
 * least every PGPROC_MAX_CACHED_SUBXIDS.  When we receive one of these
 * records, we mark the subXIDs as children of the top XID in pg_subtrans,
 * and then remove them from KnownAssignedXids.  This prevents overflow of
 * KnownAssignedXids and snapshots, at the cost that status checks for these
 * subXIDs will take a slower path through TransactionIdIsInProgress().
 * This means that KnownAssignedXids is not necessarily complete for subXIDs,
 * though it should be complete for top-level XIDs; this is the same situation
 * that holds with respect to the PGPROC entries in normal running.
 *
 * When we throw away subXIDs from KnownAssignedXids, we need to keep track of
 * that, similarly to tracking overflow of a PGPROC's subxids array.  We do
 * that by remembering the lastOverflowedXID, ie the last thrown-away subXID.
 * As long as that is within the range of interesting XIDs, we have to assume
 * that subXIDs are missing from snapshots.  (Note that subXID overflow occurs
 * on primary when 65th subXID arrives, whereas on standby it occurs when 64th
 * subXID arrives - that is not an error.)
 *
 * Should a backend on primary somehow disappear before it can write an abort
 * record, then we just leave those XIDs in KnownAssignedXids. They actually
 * aborted but we think they were running; the distinction is irrelevant
 * because either way any changes done by the transaction are not visible to
 * backends in the standby.  We prune KnownAssignedXids when
 * XLOG_RUNNING_XACTS arrives, to forestall possible overflow of the
 * array due to such dead XIDs.
 */

/*
 * RecordKnownAssignedTransactionIds
 *		Record the given XID in KnownAssignedXids, as well as any
 *preceding unobserved XIDs.
 *
 * RecordKnownAssignedTransactionIds() should be run for *every* WAL record
 * associated with a transaction. Must be called for each record after we
 * have executed StartupCLOG() et al, since we must ExtendCLOG() etc..
 *
 * Called during recovery in analogy with and in place of GetNewTransactionId()
 */
void
RecordKnownAssignedTransactionIds(TransactionId xid)
{



























































}

/*
 * ExpireTreeKnownAssignedTransactionIds
 *		Remove the given XIDs from KnownAssignedXids.
 *
 * Called during recovery in analogy with and in place of
 *ProcArrayEndTransaction()
 */
void
ExpireTreeKnownAssignedTransactionIds(TransactionId xid, int nsubxids, TransactionId *subxids, TransactionId max_xid)
{
















}

/*
 * ExpireAllKnownAssignedTransactionIds
 *		Remove all entries in KnownAssignedXids and reset
 *lastOverflowedXid.
 */
void
ExpireAllKnownAssignedTransactionIds(void)
{










}

/*
 * ExpireOldKnownAssignedTransactionIds
 *		Remove KnownAssignedXids entries preceding the given XID and
 *		potentially reset lastOverflowedXid.
 */
void
ExpireOldKnownAssignedTransactionIds(TransactionId xid)
{














}

/*
 * KnownAssignedTransactionIdsIdleMaintenance
 *		Opportunistically do maintenance work when the startup process
 *		is about to go idle.
 */
void
KnownAssignedTransactionIdsIdleMaintenance(void)
{

}

/*
 * Private module functions to manipulate KnownAssignedXids
 *
 * There are 5 main uses of the KnownAssignedXids data structure:
 *
 *	* backends taking snapshots - all valid XIDs need to be copied out
 *	* backends seeking to determine presence of a specific XID
 *	* startup process adding new known-assigned XIDs
 *	* startup process removing specific XIDs as transactions end
 *	* startup process pruning array when special WAL records arrive
 *
 * This data structure is known to be a hot spot during Hot Standby, so we
 * go to some lengths to make these operations as efficient and as concurrent
 * as possible.
 *
 * The XIDs are stored in an array in sorted order --- TransactionIdPrecedes
 * order, to be exact --- to allow binary search for specific XIDs.  Note:
 * in general TransactionIdPrecedes would not provide a total order, but
 * we know that the entries present at any instant should not extend across
 * a large enough fraction of XID space to wrap around (the master would
 * shut down for fear of XID wrap long before that happens).  So it's OK to
 * use TransactionIdPrecedes as a binary-search comparator.
 *
 * It's cheap to maintain the sortedness during insertions, since new known
 * XIDs are always reported in XID order; we just append them at the right.
 *
 * To keep individual deletions cheap, we need to allow gaps in the array.
 * This is implemented by marking array elements as valid or invalid using
 * the parallel boolean array KnownAssignedXidsValid[].  A deletion is done
 * by setting KnownAssignedXidsValid[i] to false, *without* clearing the
 * XID entry itself.  This preserves the property that the XID entries are
 * sorted, so we can do binary searches easily.  Periodically we compress
 * out the unused entries; that's much cheaper than having to compress the
 * array immediately on every deletion.
 *
 * The actually valid items in KnownAssignedXids[] and KnownAssignedXidsValid[]
 * are those with indexes tail <= i < head; items outside this subscript range
 * have unspecified contents.  When head reaches the end of the array, we
 * force compression of unused entries rather than wrapping around, since
 * allowing wraparound would greatly complicate the search logic.  We maintain
 * an explicit tail pointer so that pruning of old XIDs can be done without
 * immediately moving the array contents.  In most cases only a small fraction
 * of the array contains valid entries at any instant.
 *
 * Although only the startup process can ever change the KnownAssignedXids
 * data structure, we still need interlocking so that standby backends will
 * not observe invalid intermediate states.  The convention is that backends
 * must hold shared ProcArrayLock to examine the array.  To remove XIDs from
 * the array, the startup process must hold ProcArrayLock exclusively, for
 * the usual transactional reasons (compare commit/abort of a transaction
 * during normal running).  Compressing unused entries out of the array
 * likewise requires exclusive lock.  To add XIDs to the array, we just insert
 * them into slots to the right of the head pointer and then advance the head
 * pointer.  This wouldn't require any lock at all, except that on machines
 * with weak memory ordering we need to be careful that other processors
 * see the array element changes before they see the head pointer change.
 * We handle this by using a spinlock to protect reads and writes of the
 * head/tail pointers.  (We could dispense with the spinlock if we were to
 * create suitable memory access barrier primitives and use those instead.)
 * The spinlock must be taken to read or write the head/tail pointers unless
 * the caller holds ProcArrayLock exclusively.
 *
 * Algorithmic analysis:
 *
 * If we have a maximum of M slots, with N XIDs currently spread across
 * S elements then we have N <= S <= M always.
 *
 *	* Adding a new XID is O(1) and needs little locking (unless compression
 *		must happen)
 *	* Compressing the array is O(S) and requires exclusive lock
 *	* Removing an XID is O(logS) and requires exclusive lock
 *	* Taking a snapshot is O(S) and requires shared lock
 *	* Checking for an XID is O(logS) and requires shared lock
 *
 * In comparison, using a hash table for KnownAssignedXids would mean that
 * taking snapshots would be O(M). If we can maintain S << M then the
 * sorted array technique will deliver significantly faster snapshots.
 * If we try to keep S too small then we will spend too much time compressing,
 * so there is an optimal point for any workload mix. We use a heuristic to
 * decide when to compress the array, though trimming also helps reduce
 * frequency of compressing. The heuristic requires us to track the number of
 * currently valid XIDs in the array (N).  Except in special cases, we'll
 * compress when S >= 2N.  Bounding S at 2N in turn bounds the time for
 * taking a snapshot to be O(N), which it would have to be anyway.
 */

/*
 * Compress KnownAssignedXids by shifting valid data down to the start of the
 * array, removing any gaps.
 *
 * A compression step is forced if "reason" is KAX_NO_SPACE, otherwise
 * we do it only if a heuristic indicates it's a good time to do it.
 *
 * Compression requires holding ProcArrayLock in exclusive mode.
 * Caller must pass haveLock = true if it already holds the lock.
 */
static void
KnownAssignedXidsCompress(KAXCompressReason reason, bool haveLock)
{














































































































}

/*
 * Add xids into KnownAssignedXids at the head of the array.
 *
 * xids from from_xid to to_xid, inclusive, are added to the array.
 *
 * If exclusive_lock is true then caller already holds ProcArrayLock in
 * exclusive mode, so we need no extra locking here.  Else caller holds no
 * lock, so we need to be sure we maintain sufficient interlocks against
 * concurrent readers.  (Only the startup process ever calls this, so no need
 * to worry about concurrent writers.)
 */
static void
KnownAssignedXidsAdd(TransactionId from_xid, TransactionId to_xid, bool exclusive_lock)
{




































































































}

/*
 * KnownAssignedXidsSearch
 *
 * Searches KnownAssignedXids for a specific xid and optionally removes it.
 * Returns true if it was found, false if not.
 *
 * Caller must hold ProcArrayLock in shared or exclusive mode.
 * Exclusive lock must be held for remove = true.
 */
static bool
KnownAssignedXidsSearch(TransactionId xid, bool remove)
{




























































































}

/*
 * Is the specified XID present in KnownAssignedXids[]?
 *
 * Caller must hold ProcArrayLock in shared or exclusive mode.
 */
static bool
KnownAssignedXidExists(TransactionId xid)
{



}

/*
 * Remove the specified XID from KnownAssignedXids[].
 *
 * Caller must hold ProcArrayLock in exclusive mode.
 */
static void
KnownAssignedXidsRemove(TransactionId xid)
{















}

/*
 * KnownAssignedXidsRemoveTree
 *		Remove xid (if it's not InvalidTransactionId) and all the
 *subxids.
 *
 * Caller must hold ProcArrayLock in exclusive mode.
 */
static void
KnownAssignedXidsRemoveTree(TransactionId xid, int nsubxids, TransactionId *subxids)
{














}

/*
 * Prune KnownAssignedXids up to, but *not* including xid. If xid is invalid
 * then clear the whole table.
 *
 * Caller must hold ProcArrayLock in exclusive mode.
 */
static void
KnownAssignedXidsRemovePreceding(TransactionId removeXid)
{


































































}

/*
 * KnownAssignedXidsGet - Get an array of xids by scanning KnownAssignedXids.
 * We filter out anything >= xmax.
 *
 * Returns the number of XIDs stored into xarray[].  Caller is responsible
 * that array is large enough.
 *
 * Caller must hold ProcArrayLock in (at least) shared mode.
 */
static int
KnownAssignedXidsGet(TransactionId *xarray, TransactionId xmax)
{



}

/*
 * KnownAssignedXidsGetAndSetXmin - as KnownAssignedXidsGet, plus
 * we reduce *xmin to the lowest xid value seen if not already lower.
 *
 * Caller must hold ProcArrayLock in (at least) shared mode.
 */
static int
KnownAssignedXidsGetAndSetXmin(TransactionId *xarray, TransactionId *xmin, TransactionId xmax)
{

















































}

/*
 * Get oldest XID in the KnownAssignedXids array, or InvalidTransactionId
 * if nothing there.
 */
static TransactionId
KnownAssignedXidsGetOldestXmin(void)
{





















}

/*
 * Display KnownAssignedXids to provide debug trail
 *
 * Currently this is only called within startup process, so we need no
 * special locking.
 *
 * Note this is pretty expensive, and much of the expense will be incurred
 * even if the elog message will get discarded.  It's not currently called
 * in any performance-critical places, however, so no need to be tenser.
 */
static void
KnownAssignedXidsDisplay(int trace_level)
{






















}

/*
 * KnownAssignedXidsReset
 *		Resets KnownAssignedXids to be empty
 */
static void
KnownAssignedXidsReset(void)
{









}