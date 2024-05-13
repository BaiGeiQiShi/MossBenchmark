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
  Size size;

  /* Size of the ProcArray structure itself */
#define PROCARRAY_MAXPROCS (MaxBackends + max_prepared_xacts)

  size = offsetof(ProcArrayStruct, pgprocnos);
  size = add_size(size, mul_size(sizeof(int), PROCARRAY_MAXPROCS));

  /*
   * During Hot Standby processing we have a data structure called
   * KnownAssignedXids, created in shared memory. Local data structures are
   * also created in various backends during GetSnapshotData(),
   * TransactionIdIsInProgress() and GetRunningTransactionData(). All of the
   * main structures created in those functions must be identically sized,
   * since we may at times copy the whole of the data structures around. We
   * refer to this size as TOTAL_MAX_CACHED_SUBXIDS.
   *
   * Ideally we'd only create this structure if we were actually doing hot
   * standby in the current run, but we don't know that yet at the time
   * shared memory is being set up.
   */
#define TOTAL_MAX_CACHED_SUBXIDS ((PGPROC_MAX_CACHED_SUBXIDS + 1) * PROCARRAY_MAXPROCS)

  if (EnableHotStandby)
  {
    size = add_size(size, mul_size(sizeof(TransactionId), TOTAL_MAX_CACHED_SUBXIDS));
    size = add_size(size, mul_size(sizeof(bool), TOTAL_MAX_CACHED_SUBXIDS));
  }

  return size;
}

/*
 * Initialize the shared PGPROC array during postmaster startup.
 */
void
CreateSharedProcArray(void)
{
  bool found;

  /* Create or attach to the ProcArray shared structure */
  procArray = (ProcArrayStruct *)ShmemInitStruct("Proc Array", add_size(offsetof(ProcArrayStruct, pgprocnos), mul_size(sizeof(int), PROCARRAY_MAXPROCS)), &found);

  if (!found)
  {
    /*
     * We're the first - initialize.
     */
    procArray->numProcs = 0;
    procArray->maxProcs = PROCARRAY_MAXPROCS;
    procArray->maxKnownAssignedXids = TOTAL_MAX_CACHED_SUBXIDS;
    procArray->numKnownAssignedXids = 0;
    procArray->tailKnownAssignedXids = 0;
    procArray->headKnownAssignedXids = 0;
    SpinLockInit(&procArray->known_assigned_xids_lck);
    procArray->lastOverflowedXid = InvalidTransactionId;
    procArray->replication_slot_xmin = InvalidTransactionId;
    procArray->replication_slot_catalog_xmin = InvalidTransactionId;
  }

  allProcs = ProcGlobal->allProcs;
  allPgXact = ProcGlobal->allPgXact;

  /* Create or attach to the KnownAssignedXids arrays too, if needed */
  if (EnableHotStandby)
  {
    KnownAssignedXids = (TransactionId *)ShmemInitStruct("KnownAssignedXids", mul_size(sizeof(TransactionId), TOTAL_MAX_CACHED_SUBXIDS), &found);
    KnownAssignedXidsValid = (bool *)ShmemInitStruct("KnownAssignedXidsValid", mul_size(sizeof(bool), TOTAL_MAX_CACHED_SUBXIDS), &found);
  }

  /* Register and initialize fields of ProcLWLockTranche */
  LWLockRegisterTranche(LWTRANCHE_PROC, "proc");
}

/*
 * Add the specified PGPROC to the shared array.
 */
void
ProcArrayAdd(PGPROC *proc)
{
  ProcArrayStruct *arrayP = procArray;
  int index;

  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  if (arrayP->numProcs >= arrayP->maxProcs)
  {
    /*
     * Oops, no room.  (This really shouldn't happen, since there is a
     * fixed supply of PGPROC structs too, and so we should have failed
     * earlier.)
     */


  }

  /*
   * Keep the procs array sorted by (PGPROC *) so that we can utilize
   * locality of references much better. This is useful while traversing the
   * ProcArray because there is an increased likelihood of finding the next
   * PGPROC structure in the cache.
   *
   * Since the occurrence of adding/removing a proc is much lower than the
   * access to the ProcArray itself, the overhead should be marginal
   */
  for (index = 0; index < arrayP->numProcs; index++)
  {
    /*
     * If we are the first PGPROC or if we have found our right position
     * in the array, break
     */
    if ((arrayP->pgprocnos[index] == -1) || (arrayP->pgprocnos[index] > proc->pgprocno))
    {
      break;
    }
  }

  memmove(&arrayP->pgprocnos[index + 1], &arrayP->pgprocnos[index], (arrayP->numProcs - index) * sizeof(int));
  arrayP->pgprocnos[index] = proc->pgprocno;
  arrayP->numProcs++;

  LWLockRelease(ProcArrayLock);
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
  ProcArrayStruct *arrayP = procArray;
  int index;

#ifdef XIDCACHE_DEBUG
  /* dump stats at backend shutdown, but not prepared-xact end */
  if (proc->pid != 0)
  {
    DisplayXidCache();
  }
#endif

  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  if (TransactionIdIsValid(latestXid))
  {


    /* Advance global latestCompletedXid while holding the lock */




  }
  else
  {
    /* Shouldn't be trying to remove a live transaction here */
    Assert(!TransactionIdIsValid(allPgXact[proc->pgprocno].xid));
  }

  for (index = 0; index < arrayP->numProcs; index++)
  {
    if (arrayP->pgprocnos[index] == proc->pgprocno)
    {
      /* Keep the PGPROC array sorted. See notes above */
      memmove(&arrayP->pgprocnos[index], &arrayP->pgprocnos[index + 1], (arrayP->numProcs - index - 1) * sizeof(int));
      arrayP->pgprocnos[arrayP->numProcs - 1] = -1; /* for debugging */
      arrayP->numProcs--;
      LWLockRelease(ProcArrayLock);
      return;
    }
  }

  /* Oops */
  LWLockRelease(ProcArrayLock);


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
  PGXACT *pgxact = &allPgXact[proc->pgprocno];

  if (TransactionIdIsValid(latestXid))
  {
    /*
     * We must lock ProcArrayLock while clearing our advertised XID, so
     * that we do not exit the set of "running" transactions while someone
     * else is taking a snapshot.  See discussion in
     * src/backend/access/transam/README.
     */
    Assert(TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

    /*
     * If we can immediately acquire ProcArrayLock, we clear our own XID
     * and release the lock.  If not, use group XID clearing to improve
     * efficiency.
     */
    if (LWLockConditionalAcquire(ProcArrayLock, LW_EXCLUSIVE))
    {
      ProcArrayEndTransactionInternal(proc, pgxact, latestXid);
      LWLockRelease(ProcArrayLock);
    }
    else
    {
      ProcArrayGroupClearXid(proc, latestXid);
    }
  }
  else
  {
    /*
     * If we have no XID, we don't need to lock, since we won't affect
     * anyone else's calculation of a snapshot.  We might change their
     * estimate of global xmin, but that's OK.
     */
    Assert(!TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

    proc->lxid = InvalidLocalTransactionId;
    pgxact->xmin = InvalidTransactionId;
    /* must be cleared with xid/xmin: */
    pgxact->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;

    /* be sure these are cleared in abort */
    pgxact->delayChkpt = false;
    proc->delayChkptEnd = false;

    proc->recoveryConflictPending = false;

    Assert(pgxact->nxids == 0);
    Assert(pgxact->overflowed == false);
  }
}

/*
 * Mark a write transaction as no longer running.
 *
 * We don't do any locking here; caller must handle that.
 */
static inline void
ProcArrayEndTransactionInternal(PGPROC *proc, PGXACT *pgxact, TransactionId latestXid)
{
  pgxact->xid = InvalidTransactionId;
  proc->lxid = InvalidLocalTransactionId;
  pgxact->xmin = InvalidTransactionId;
  /* must be cleared with xid/xmin: */
  pgxact->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;

  /* be sure these are cleared in abort */
  pgxact->delayChkpt = false;
  proc->delayChkptEnd = false;

  proc->recoveryConflictPending = false;

  /* Clear the subtransaction-XID cache too while holding the lock */
  pgxact->nxids = 0;
  pgxact->overflowed = false;

  /* Also advance global latestCompletedXid while holding the lock */
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, latestXid))
  {
    ShmemVariableCache->latestCompletedXid = latestXid;
  }
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
  PROC_HDR *procglobal = ProcGlobal;
  uint32 nextidx;
  uint32 wakeidx;

  /* We should definitely have an XID to clear. */
  Assert(TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

  /* Add ourselves to the list of processes needing a group XID clear. */
  proc->procArrayGroupMember = true;
  proc->procArrayGroupMemberXid = latestXid;
  while (true)
  {
    nextidx = pg_atomic_read_u32(&procglobal->procArrayGroupFirst);
    pg_atomic_write_u32(&proc->procArrayGroupNext, nextidx);

    if (pg_atomic_compare_exchange_u32(&procglobal->procArrayGroupFirst, &nextidx, (uint32)proc->pgprocno))
    {
      break;
    }
  }

  /*
   * If the list was not empty, the leader will clear our XID.  It is
   * impossible to have followers without a leader because the first process
   * that has added itself to the list will always have nextidx as
   * INVALID_PGPROCNO.
   */
  if (nextidx != INVALID_PGPROCNO)
  {
    int extraWaits = 0;

    /* Sleep until the leader clears our XID. */
    pgstat_report_wait_start(WAIT_EVENT_PROCARRAY_GROUP_UPDATE);
    for (;;)
    {
      /* acts as a read barrier */
      PGSemaphoreLock(proc->sem);
      if (!proc->procArrayGroupMember)
      {
        break;
      }

    }
    pgstat_report_wait_end();

    Assert(pg_atomic_read_u32(&proc->procArrayGroupNext) == INVALID_PGPROCNO);

    /* Fix semaphore count for any absorbed wakeups */
    while (extraWaits-- > 0)
    {

    }
    return;
  }

  /* We are the leader.  Acquire the lock on behalf of everyone. */
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  /*
   * Now that we've got the lock, clear the list of processes waiting for
   * group XID clearing, saving a pointer to the head of the list.  Trying
   * to pop elements one at a time could lead to an ABA problem.
   */
  nextidx = pg_atomic_exchange_u32(&procglobal->procArrayGroupFirst, INVALID_PGPROCNO);

  /* Remember head of list so we can perform wakeups after dropping lock. */
  wakeidx = nextidx;

  /* Walk the list and clear all XIDs. */
  while (nextidx != INVALID_PGPROCNO)
  {
    PGPROC *nextproc = &allProcs[nextidx];
    PGXACT *pgxact = &allPgXact[nextidx];

    ProcArrayEndTransactionInternal(nextproc, pgxact, nextproc->procArrayGroupMemberXid);

    /* Move to next proc in list. */
    nextidx = pg_atomic_read_u32(&nextproc->procArrayGroupNext);
  }

  /* We're done with the lock now. */
  LWLockRelease(ProcArrayLock);

  /*
   * Now that we've released the lock, go back and wake everybody up.  We
   * don't do this under the lock so as to keep lock hold times to a
   * minimum.  The system calls we need to perform to wake other processes
   * up are probably much slower than the simple memory writes we did while
   * holding the lock.
   */
  while (wakeidx != INVALID_PGPROCNO)
  {
    PGPROC *nextproc = &allProcs[wakeidx];

    wakeidx = pg_atomic_read_u32(&nextproc->procArrayGroupNext);
    pg_atomic_write_u32(&nextproc->procArrayGroupNext, INVALID_PGPROCNO);

    /* ensure all previous writes are visible before follower continues. */
    pg_write_barrier();

    nextproc->procArrayGroupMember = false;

    if (nextproc != MyProc)
    {
      PGSemaphoreUnlock(nextproc->sem);
    }
  }
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
  static TransactionId *xids = NULL;
  int nxids = 0;
  ProcArrayStruct *arrayP = procArray;
  TransactionId topxid;
  int i, j;

  /*
   * Don't bother checking a transaction older than RecentXmin; it could not
   * possibly still be running.  (Note: in particular, this guarantees that
   * we reject InvalidTransactionId, FrozenTransactionId, etc as not
   * running.)
   */
  if (TransactionIdPrecedes(xid, RecentXmin))
  {
    xc_by_recent_xmin_inc();
    return false;
  }

  /*
   * We may have just checked the status of this transaction, so if it is
   * already known to be completed, we can fall out without any access to
   * shared memory.
   */
  if (TransactionIdEquals(cachedXidIsNotInProgress, xid))
  {
    xc_by_known_xact_inc();
    return false;
  }

  /*
   * Also, we can handle our own transaction (and subtransactions) without
   * any access to shared memory.
   */
  if (TransactionIdIsCurrentTransactionId(xid))
  {
    xc_by_my_xact_inc();
    return true;
  }

  /*
   * If first time through, get workspace to remember main XIDs in. We
   * malloc it permanently to avoid repeated palloc/pfree overhead.
   */
  if (xids == NULL)
  {
    /*
     * In hot standby mode, reserve enough space to hold all xids in the
     * known-assigned list. If we later finish recovery, we no longer need
     * the bigger array, but we don't bother to shrink it.
     */
    int maxxids = RecoveryInProgress() ? TOTAL_MAX_CACHED_SUBXIDS : arrayP->maxProcs;

    xids = (TransactionId *)malloc(maxxids * sizeof(TransactionId));
    if (xids == NULL)
    {

    }
  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  /*
   * Now that we have the lock, we can check latestCompletedXid; if the
   * target Xid is after that, it's surely still running.
   */
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, xid))
  {
    LWLockRelease(ProcArrayLock);
    xc_by_latest_xid_inc();
    return true;
  }

  /* No shortcuts, gotta grovel through the array */
  for (i = 0; i < arrayP->numProcs; i++)
  {
    int pgprocno = arrayP->pgprocnos[i];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId pxid;
    int pxids;

    /* Ignore my own proc --- dealt with it above */
    if (proc == MyProc)
    {
      continue;
    }

    /* Fetch xid just once - see GetNewTransactionId */
    pxid = UINT32_ACCESS_ONCE(pgxact->xid);

    if (!TransactionIdIsValid(pxid))
    {
      continue;
    }

    /*
     * Step 1: check the main Xid
     */
    if (TransactionIdEquals(pxid, xid))
    {
      LWLockRelease(ProcArrayLock);
      xc_by_main_xid_inc();
      return true;
    }

    /*
     * We can ignore main Xids that are younger than the target Xid, since
     * the target could not possibly be their child.
     */
    if (TransactionIdPrecedes(xid, pxid))
    {
      continue;
    }

    /*
     * Step 2: check the cached child-Xids arrays
     */
    pxids = pgxact->nxids;
    pg_read_barrier(); /* pairs with barrier in GetNewTransactionId() */
    for (j = pxids - 1; j >= 0; j--)
    {
      /* Fetch xid just once - see GetNewTransactionId */
      TransactionId cxid = UINT32_ACCESS_ONCE(proc->subxids.xids[j]);







    }

    /*
     * Save the main Xid for step 4.  We only need to remember main Xids
     * that have uncached children.  (Note: there is no race condition
     * here because the overflowed flag cannot be cleared, only set, while
     * we hold ProcArrayLock.  So we can't miss an Xid that we need to
     * worry about.)
     */
    if (pgxact->overflowed)
    {

    }
  }

  /*
   * Step 3: in hot standby mode, check the known-assigned-xids list.  XIDs
   * in the list must be treated as running.
   */
  if (RecoveryInProgress())
  {
    /* none of the PGXACT entries should have XIDs in hot standby mode */









    /*
     * If the KnownAssignedXids overflowed, we have to check pg_subtrans
     * too.  Fetch all xids from KnownAssignedXids that are lower than
     * xid, since if xid is a subtransaction its parent will always have a
     * lower value.  Note we will collect both main and subXIDs here, but
     * there's no help for it.
     */




  }

  LWLockRelease(ProcArrayLock);

  /*
   * If none of the relevant caches overflowed, we know the Xid is not
   * running without even looking at pg_subtrans.
   */
  if (nxids == 0)
  {
    xc_no_overflow_inc();
    cachedXidIsNotInProgress = xid;
    return false;
  }

  /*
   * Step 4: have to check pg_subtrans.
   *
   * At this point, we know it's either a subtransaction of one of the Xids
   * in xids[], or it's not running.  If it's an already-failed
   * subtransaction, we want to say "not running" even though its parent may
   * still be running.  So first, check pg_xact to see if it's been aborted.
   */








  /*
   * It isn't aborted, so check whether the transaction tree it belongs to
   * is still running (or, more precisely, whether it was running when we
   * held ProcArrayLock).
   */















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
  ProcArrayStruct *arrayP = procArray;
  TransactionId result;
  int index;
  bool allDbs;

  TransactionId replication_slot_xmin = InvalidTransactionId;
  TransactionId replication_slot_catalog_xmin = InvalidTransactionId;

  /*
   * If we're not computing a relation specific limit, or if a shared
   * relation has been passed in, backends in all databases have to be
   * considered.
   */
  allDbs = rel == NULL || rel->rd_rel->relisshared;

  /* Cannot look for individual databases during recovery */
  Assert(allDbs || !RecoveryInProgress());

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  /*
   * We initialize the MIN() calculation with latestCompletedXid + 1. This
   * is a lower bound for the XIDs that might appear in the ProcArray later,
   * and so protects us against overestimating the result due to future
   * additions.
   */
  result = ShmemVariableCache->latestCompletedXid;
  Assert(TransactionIdIsNormal(result));
  TransactionIdAdvance(result);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (pgxact->vacuumFlags & (flags & PROCARRAY_PROC_FLAGS_MASK))
    {
      continue;
    }

    if (allDbs || proc->databaseId == MyDatabaseId || proc->databaseId == 0) /* always include WalSender */
    {
      /* Fetch xid just once - see GetNewTransactionId */
      TransactionId xid = UINT32_ACCESS_ONCE(pgxact->xid);

      /* First consider the transaction's own Xid, if any */
      if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, result))
      {
        result = xid;
      }

      /*
       * Also consider the transaction's Xmin, if set.
       *
       * We must check both Xid and Xmin because a transaction might
       * have an Xmin but not (yet) an Xid; conversely, if it has an
       * Xid, that could determine some not-yet-set Xmin.
       */
      xid = UINT32_ACCESS_ONCE(pgxact->xmin);
      if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, result))
      {
        result = xid;
      }
    }
  }

  /*
   * Fetch into local variable while ProcArrayLock is held - the
   * LWLockRelease below is a barrier, ensuring this happens inside the
   * lock.
   */
  replication_slot_xmin = procArray->replication_slot_xmin;
  replication_slot_catalog_xmin = procArray->replication_slot_catalog_xmin;

  if (RecoveryInProgress())
  {
    /*
     * Check to see whether KnownAssignedXids contains an xid value older
     * than the main procarray.
     */
    TransactionId kaxmin = KnownAssignedXidsGetOldestXmin();







  }
  else
  {
    /*
     * No other information needed, so release the lock immediately.
     */
    LWLockRelease(ProcArrayLock);

    /*
     * Compute the cutoff XID by subtracting vacuum_defer_cleanup_age,
     * being careful not to generate a "permanent" XID.
     *
     * vacuum_defer_cleanup_age provides some additional "slop" for the
     * benefit of hot standby queries on standby servers.  This is quick
     * and dirty, and perhaps not all that useful unless the master has a
     * predictable transaction rate, but it offers some protection when
     * there's no walsender connection.  Note that we are assuming
     * vacuum_defer_cleanup_age isn't large enough to cause wraparound ---
     * so guc.c should limit it to no more than the xidStopLimit threshold
     * in varsup.c.  Also note that we intentionally don't apply
     * vacuum_defer_cleanup_age on standby servers.
     */
    result -= vacuum_defer_cleanup_age;
    if (!TransactionIdIsNormal(result))
    {

    }
  }

  /*
   * Check whether there are replication slots requiring an older xmin.
   */
  if (!(flags & PROCARRAY_SLOTS_XMIN) && TransactionIdIsValid(replication_slot_xmin) && NormalTransactionIdPrecedes(replication_slot_xmin, result))
  {

  }

  /*
   * After locks have been released and defer_cleanup_age has been applied,
   * check whether we need to back up further to make logical decoding
   * possible. We need to do so if we're computing the global limit (rel =
   * NULL) or if the passed relation is a catalog relation of some kind.
   */
  if (!(flags & PROCARRAY_SLOTS_XMIN) && (rel == NULL || RelationIsAccessibleInLogicalDecoding(rel)) && TransactionIdIsValid(replication_slot_catalog_xmin) && NormalTransactionIdPrecedes(replication_slot_catalog_xmin, result))
  {

  }

  return result;
}

/*
 * GetMaxSnapshotXidCount -- get max size for snapshot XID array
 *
 * We have to export this for use by snapmgr.c.
 */
int
GetMaxSnapshotXidCount(void)
{
  return procArray->maxProcs;
}

/*
 * GetMaxSnapshotSubxidCount -- get max size for snapshot sub-XID array
 *
 * We have to export this for use by snapmgr.c.
 */
int
GetMaxSnapshotSubxidCount(void)
{
  return TOTAL_MAX_CACHED_SUBXIDS;
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
  ProcArrayStruct *arrayP = procArray;
  TransactionId xmin;
  TransactionId xmax;
  TransactionId globalxmin;
  int index;
  int count = 0;
  int subcount = 0;
  bool suboverflowed = false;
  TransactionId replication_slot_xmin = InvalidTransactionId;
  TransactionId replication_slot_catalog_xmin = InvalidTransactionId;

  Assert(snapshot != NULL);

  /*
   * Allocating space for maxProcs xids is usually overkill; numProcs would
   * be sufficient.  But it seems better to do the malloc while not holding
   * the lock, so we can't look at numProcs.  Likewise, we allocate much
   * more subxip storage than is probably needed.
   *
   * This does open a possibility for avoiding repeated malloc/free: since
   * maxProcs does not change at runtime, we can simply reuse the previous
   * xip arrays if any.  (This relies on the fact that all callers pass
   * static SnapshotData structs.)
   */
  if (snapshot->xip == NULL)
  {
    /*
     * First call for this snapshot. Snapshot is same size whether or not
     * we are in recovery, see later comments.
     */
    snapshot->xip = (TransactionId *)malloc(GetMaxSnapshotXidCount() * sizeof(TransactionId));
    if (snapshot->xip == NULL)
    {

    }
    Assert(snapshot->subxip == NULL);
    snapshot->subxip = (TransactionId *)malloc(GetMaxSnapshotSubxidCount() * sizeof(TransactionId));
    if (snapshot->subxip == NULL)
    {

    }
  }

  /*
   * It is sufficient to get shared lock on ProcArrayLock, even if we are
   * going to set MyPgXact->xmin.
   */
  LWLockAcquire(ProcArrayLock, LW_SHARED);

  /* xmax is always latestCompletedXid + 1 */
  xmax = ShmemVariableCache->latestCompletedXid;
  Assert(TransactionIdIsNormal(xmax));
  TransactionIdAdvance(xmax);

  /* initialize xmin calculation with xmax */
  globalxmin = xmin = xmax;

  snapshot->takenDuringRecovery = RecoveryInProgress();

  if (!snapshot->takenDuringRecovery)
  {
    int *pgprocnos = arrayP->pgprocnos;
    int numProcs;

    /*
     * Spin over procArray checking xid, xmin, and subxids.  The goal is
     * to gather all active xids, find the lowest xmin, and try to record
     * subxids.
     */
    numProcs = arrayP->numProcs;
    for (index = 0; index < numProcs; index++)
    {
      int pgprocno = pgprocnos[index];
      PGXACT *pgxact = &allPgXact[pgprocno];
      TransactionId xid;

      /*
       * Skip over backends doing logical decoding which manages xmin
       * separately (check below) and ones running LAZY VACUUM.
       */
      if (pgxact->vacuumFlags & (PROC_IN_LOGICAL_DECODING | PROC_IN_VACUUM))
      {
        continue;
      }

      /* Update globalxmin to be the smallest valid xmin */
      xid = UINT32_ACCESS_ONCE(pgxact->xmin);
      if (TransactionIdIsNormal(xid) && NormalTransactionIdPrecedes(xid, globalxmin))
      {
        globalxmin = xid;
      }

      /* Fetch xid just once - see GetNewTransactionId */
      xid = UINT32_ACCESS_ONCE(pgxact->xid);

      /*
       * If the transaction has no XID assigned, we can skip it; it
       * won't have sub-XIDs either.  If the XID is >= xmax, we can also
       * skip it; such transactions will be treated as running anyway
       * (and any sub-XIDs will also be >= xmax).
       */
      if (!TransactionIdIsNormal(xid) || !NormalTransactionIdPrecedes(xid, xmax))
      {
        continue;
      }

      /*
       * We don't include our own XIDs (if any) in the snapshot, but we
       * must include them in xmin.
       */
      if (NormalTransactionIdPrecedes(xid, xmin))
      {
        xmin = xid;
      }
      if (pgxact == MyPgXact)
      {
        continue;
      }

      /* Add XID to snapshot. */
      snapshot->xip[count++] = xid;

      /*
       * Save subtransaction XIDs if possible (if we've already
       * overflowed, there's no point).  Note that the subxact XIDs must
       * be later than their parent, so no need to check them against
       * xmin.  We could filter against xmax, but it seems better not to
       * do that much work while holding the ProcArrayLock.
       *
       * The other backend can add more subxids concurrently, but cannot
       * remove any.  Hence it's important to fetch nxids just once.
       * Should be safe to use memcpy, though.  (We needn't worry about
       * missing any xids added concurrently, because they must postdate
       * xmax.)
       *
       * Again, our own XIDs are not included in the snapshot.
       */
      if (!suboverflowed)
      {
        if (pgxact->overflowed)
        {

        }
        else
        {
          int nxids = pgxact->nxids;

          if (nxids > 0)
          {
            PGPROC *proc = &allProcs[pgprocno];





          }
        }
      }
    }
  }
  else
  {
    /*
     * We're in hot standby, so get XIDs from KnownAssignedXids.
     *
     * We store all xids directly into subxip[]. Here's why:
     *
     * In recovery we don't know which xids are top-level and which are
     * subxacts, a design choice that greatly simplifies xid processing.
     *
     * It seems like we would want to try to put xids into xip[] only, but
     * that is fairly small. We would either need to make that bigger or
     * to increase the rate at which we WAL-log xid assignment; neither is
     * an appealing choice.
     *
     * We could try to store xids into xip[] first and then into subxip[]
     * if there are too many xids. That only works if the snapshot doesn't
     * overflow because we do not search subxip[] in that case. A simpler
     * way is to just store all xids in the subxact array because this is
     * by far the bigger array. We just leave the xip array empty.
     *
     * Either way we need to change the way XidInMVCCSnapshot() works
     * depending upon when the snapshot was taken, or change normal
     * snapshot processing so it matches.
     *
     * Note: It is possible for recovery to end before we finish taking
     * the snapshot, and for newly assigned transaction ids to be added to
     * the ProcArray.  xmax cannot change while we hold ProcArrayLock, so
     * those newly added transaction ids would be filtered away, so we
     * need not be concerned about them.
     */






  }

  /*
   * Fetch into local variable while ProcArrayLock is held - the
   * LWLockRelease below is a barrier, ensuring this happens inside the
   * lock.
   */
  replication_slot_xmin = procArray->replication_slot_xmin;
  replication_slot_catalog_xmin = procArray->replication_slot_catalog_xmin;

  if (!TransactionIdIsValid(MyPgXact->xmin))
  {
    MyPgXact->xmin = TransactionXmin = xmin;
  }

  LWLockRelease(ProcArrayLock);

  /*
   * Update globalxmin to include actual process xids.  This is a slightly
   * different way of computing it than GetOldestXmin uses, but should give
   * the same result.
   */
  if (TransactionIdPrecedes(xmin, globalxmin))
  {
    globalxmin = xmin;
  }

  /* Update global variables too */
  RecentGlobalXmin = globalxmin - vacuum_defer_cleanup_age;
  if (!TransactionIdIsNormal(RecentGlobalXmin))
  {

  }

  /* Check whether there's a replication slot requiring an older xmin. */
  if (TransactionIdIsValid(replication_slot_xmin) && NormalTransactionIdPrecedes(replication_slot_xmin, RecentGlobalXmin))
  {

  }

  /* Non-catalog tables can be vacuumed if older than this xid */
  RecentGlobalDataXmin = RecentGlobalXmin;

  /*
   * Check whether there's a replication slot requiring an older catalog
   * xmin.
   */
  if (TransactionIdIsNormal(replication_slot_catalog_xmin) && NormalTransactionIdPrecedes(replication_slot_catalog_xmin, RecentGlobalXmin))
  {

  }

  RecentXmin = xmin;

  snapshot->xmin = xmin;
  snapshot->xmax = xmax;
  snapshot->xcnt = count;
  snapshot->subxcnt = subcount;
  snapshot->suboverflowed = suboverflowed;

  snapshot->curcid = GetCurrentCommandId(false);

  /*
   * This is a new snapshot, so set both refcounts are zero, and mark it as
   * not copied in persistent memory.
   */
  snapshot->active_count = 0;
  snapshot->regd_count = 0;
  snapshot->copied = false;

  if (old_snapshot_threshold < 0)
  {
    /*
     * If not using "snapshot too old" feature, fill related fields with
     * dummy values that don't require any locking.
     */
    snapshot->lsn = InvalidXLogRecPtr;
    snapshot->whenTaken = 0;
  }
  else
  {
    /*
     * Capture the current time and WAL stream location in case this
     * snapshot becomes old enough to need to fall back on the special
     * "old snapshot" logic.
     */



  }

  return snapshot;
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
  bool result = false;
  TransactionId xid;
  PGXACT *pgxact;

  Assert(TransactionIdIsNormal(xmin));
  Assert(proc != NULL);

  /* Get lock so source xact can't end while we're doing this */
  LWLockAcquire(ProcArrayLock, LW_SHARED);

  pgxact = &allPgXact[proc->pgprocno];

  /*
   * Be certain that the referenced PGPROC has an advertised xmin which is
   * no later than the one we're installing, so that the system-wide xmin
   * can't go backwards.  Also, make sure it's running in the same database,
   * so that the per-database xmin cannot go backwards.
   */
  xid = UINT32_ACCESS_ONCE(pgxact->xmin);
  if (proc->databaseId == MyDatabaseId && TransactionIdIsNormal(xid) && TransactionIdPrecedesOrEquals(xid, xmin))
  {
    MyPgXact->xmin = TransactionXmin = xmin;
    result = true;
  }

  LWLockRelease(ProcArrayLock);

  return result;
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
  /* result workspace */
  static RunningTransactionsData CurrentRunningXactsData;

  ProcArrayStruct *arrayP = procArray;
  RunningTransactions CurrentRunningXacts = &CurrentRunningXactsData;
  TransactionId latestCompletedXid;
  TransactionId oldestRunningXid;
  TransactionId *xids;
  int index;
  int count;
  int subcount;
  bool suboverflowed;

  Assert(!RecoveryInProgress());

  /*
   * Allocating space for maxProcs xids is usually overkill; numProcs would
   * be sufficient.  But it seems better to do the malloc while not holding
   * the lock, so we can't look at numProcs.  Likewise, we allocate much
   * more subxip storage than is probably needed.
   *
   * Should only be allocated in bgwriter, since only ever executed during
   * checkpoints.
   */
  if (CurrentRunningXacts->xids == NULL)
  {
    /*
     * First call
     */
    CurrentRunningXacts->xids = (TransactionId *)malloc(TOTAL_MAX_CACHED_SUBXIDS * sizeof(TransactionId));
    if (CurrentRunningXacts->xids == NULL)
    {

    }
  }

  xids = CurrentRunningXacts->xids;

  count = subcount = 0;
  suboverflowed = false;

  /*
   * Ensure that no xids enter or leave the procarray while we obtain
   * snapshot.
   */
  LWLockAcquire(ProcArrayLock, LW_SHARED);
  LWLockAcquire(XidGenLock, LW_SHARED);

  latestCompletedXid = ShmemVariableCache->latestCompletedXid;

  oldestRunningXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);

  /*
   * Spin over procArray collecting all xids
   */
  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId xid;

    /* Fetch xid just once - see GetNewTransactionId */
    xid = UINT32_ACCESS_ONCE(pgxact->xid);

    /*
     * We don't need to store transactions that don't have a TransactionId
     * yet because they will not show as running on a standby server.
     */
    if (!TransactionIdIsValid(xid))
    {
      continue;
    }

    /*
     * Be careful not to exclude any xids before calculating the values of
     * oldestRunningXid and suboverflowed, since these are used to clean
     * up transaction information held on standbys.
     */
    if (TransactionIdPrecedes(xid, oldestRunningXid))
    {
      oldestRunningXid = xid;
    }

    if (pgxact->overflowed)
    {

    }

    /*
     * If we wished to exclude xids this would be the right place for it.
     * Procs with the PROC_IN_VACUUM flag set don't usually assign xids,
     * but they do during truncation at the end when they get the lock and
     * truncate, so it is not much of a problem to include them if they
     * are seen and it is cleaner to include them.
     */

    xids[count++] = xid;
  }

  /*
   * Spin over procArray collecting all subxids, but only if there hasn't
   * been a suboverflow.
   */
  if (!suboverflowed)
  {
    for (index = 0; index < arrayP->numProcs; index++)
    {
      int pgprocno = arrayP->pgprocnos[index];
      PGPROC *proc = &allProcs[pgprocno];
      PGXACT *pgxact = &allPgXact[pgprocno];
      int nxids;

      /*
       * Save subtransaction XIDs. Other backends can't add or remove
       * entries while we're holding XidGenLock.
       */
      nxids = pgxact->nxids;
      if (nxids > 0)
      {
        /* barrier not really required, as XidGenLock is held, but ... */






        /*
         * Top-level XID of a transaction is always less than any of
         * its subxids, so we don't need to check if any of the
         * subxids are smaller than oldestRunningXid
         */
      }
    }
  }

  /*
   * It's important *not* to include the limits set by slots here because
   * snapbuild.c uses oldestRunningXid to manage its xmin horizon. If those
   * were to be included here the initial value could never increase because
   * of a circular dependency where slots only increase their limits when
   * running xacts increases oldestRunningXid and running xacts only
   * increases if slots do.
   */

  CurrentRunningXacts->xcnt = count - subcount;
  CurrentRunningXacts->subxcnt = subcount;
  CurrentRunningXacts->subxid_overflow = suboverflowed;
  CurrentRunningXacts->nextXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  CurrentRunningXacts->oldestRunningXid = oldestRunningXid;
  CurrentRunningXacts->latestCompletedXid = latestCompletedXid;

  Assert(TransactionIdIsValid(CurrentRunningXacts->nextXid));
  Assert(TransactionIdIsValid(CurrentRunningXacts->oldestRunningXid));
  Assert(TransactionIdIsNormal(CurrentRunningXacts->latestCompletedXid));

  /* We don't release the locks here, the caller is responsible for that */

  return CurrentRunningXacts;
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
  ProcArrayStruct *arrayP = procArray;
  TransactionId oldestRunningXid;
  int index;

  Assert(!RecoveryInProgress());

  /*
   * Read nextXid, as the upper bound of what's still active.
   *
   * Reading a TransactionId is atomic, but we must grab the lock to make
   * sure that all XIDs < nextXid are already present in the proc array (or
   * have already completed), when we spin over it.
   */
  LWLockAcquire(XidGenLock, LW_SHARED);
  oldestRunningXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  LWLockRelease(XidGenLock);

  /*
   * Spin over procArray collecting all xids and subxids.
   */
  LWLockAcquire(ProcArrayLock, LW_SHARED);
  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId xid;

    /* Fetch xid just once - see GetNewTransactionId */
    xid = UINT32_ACCESS_ONCE(pgxact->xid);

    if (!TransactionIdIsNormal(xid))
    {
      continue;
    }

    if (TransactionIdPrecedes(xid, oldestRunningXid))
    {
      oldestRunningXid = xid;
    }

    /*
     * Top-level XID of a transaction is always less than any of its
     * subxids, so we don't need to check if any of the subxids are
     * smaller than oldestRunningXid
     */
  }
  LWLockRelease(ProcArrayLock);

  return oldestRunningXid;
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
  VirtualTransactionId *vxids;
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

  Assert(type != 0);

  /* allocate what's certainly enough result space */
  vxids = (VirtualTransactionId *)palloc(sizeof(VirtualTransactionId) * arrayP->maxProcs);

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (((type & DELAY_CHKPT_START) && pgxact->delayChkpt) || ((type & DELAY_CHKPT_COMPLETE) && proc->delayChkptEnd))
    {
      VirtualTransactionId vxid;






    }
  }

  LWLockRelease(ProcArrayLock);

  *nvxids = count;
  return vxids;
}

/*
 * GetVirtualXIDsDelayingChkpt - Get the VXIDs of transactions that are
 * delaying the start of a checkpoint.
 */
VirtualTransactionId *
GetVirtualXIDsDelayingChkpt(int *nvxids)
{
  return GetVirtualXIDsDelayingChkptGuts(nvxids, DELAY_CHKPT_START);
}

/*
 * GetVirtualXIDsDelayingChkptEnd - Get the VXIDs of transactions that are
 * delaying the end of a checkpoint.
 */
VirtualTransactionId *
GetVirtualXIDsDelayingChkptEnd(int *nvxids)
{
  return GetVirtualXIDsDelayingChkptGuts(nvxids, DELAY_CHKPT_COMPLETE);
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
  PGPROC *result;

  if (pid == 0)
  { /* never match dummy PGPROCs */

  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  result = BackendPidGetProcWithLock(pid);

  LWLockRelease(ProcArrayLock);

  return result;
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
  PGPROC *result = NULL;
  ProcArrayStruct *arrayP = procArray;
  int index;

  if (pid == 0)
  { /* never match dummy PGPROCs */

  }

  for (index = 0; index < arrayP->numProcs; index++)
  {
    PGPROC *proc = &allProcs[arrayP->pgprocnos[index]];

    if (proc->pid == pid)
    {
      result = proc;
      break;
    }
  }

  return result;
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
  VirtualTransactionId *vxids;
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

  /* allocate what's certainly enough result space */
  vxids = (VirtualTransactionId *)palloc(sizeof(VirtualTransactionId) * arrayP->maxProcs);

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (proc == MyProc)
    {
      continue;
    }

    if (excludeVacuum & pgxact->vacuumFlags)
    {
      continue;
    }

    if (allDbs || proc->databaseId == MyDatabaseId)
    {
      /* Fetch xmin just once - might change on us */
      TransactionId pxmin = UINT32_ACCESS_ONCE(pgxact->xmin);

      if (excludeXmin0 && !TransactionIdIsValid(pxmin))
      {
        continue;
      }

      /*
       * InvalidTransactionId precedes all other XIDs, so a proc that
       * hasn't set xmin yet will not be rejected by this test.
       */
      if (!TransactionIdIsValid(limitXmin) || TransactionIdPrecedesOrEquals(pxmin, limitXmin))
      {
        VirtualTransactionId vxid;

        GET_VXID_FROM_PGPROC(vxid, *proc);
        if (VirtualTransactionIdIsValid(vxid))
        {
          vxids[count++] = vxid;
        }
      }
    }
  }

  LWLockRelease(ProcArrayLock);

  *nvxids = count;
  return vxids;
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
  ProcArrayStruct *arrayP = procArray;

#define MAXAUTOVACPIDS 10 /* max autovacs to SIGTERM per iteration */
  int autovac_pids[MAXAUTOVACPIDS];
  int tries;

  /* 50 tries with 100ms sleep between tries makes 5 sec total wait */
  for (tries = 0; tries < 50; tries++)
  {
    int nautovacs = 0;
    bool found = false;
    int index;

    CHECK_FOR_INTERRUPTS();

    *nbackends = *nprepared = 0;

    LWLockAcquire(ProcArrayLock, LW_SHARED);

    for (index = 0; index < arrayP->numProcs; index++)
    {
      int pgprocno = arrayP->pgprocnos[index];
      PGPROC *proc = &allProcs[pgprocno];
      PGXACT *pgxact = &allPgXact[pgprocno];

      if (proc->databaseId != databaseId)
      {
        continue;
      }
      if (proc == MyProc)
      {
        continue;
      }















    }

    LWLockRelease(ProcArrayLock);

    if (!found)
    {
      return false; /* no conflicting backends, so done */
    }

    /*
     * Send SIGTERM to any conflicting autovacuums before sleeping. We
     * postpone this step until after the loop because we don't want to
     * hold ProcArrayLock while issuing kill(). We have no idea what might
     * block kill() inside the kernel...
     */





    /* sleep, then try again */

  }

  return true; /* timed out, still conflicts */
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
  Assert(!already_locked || LWLockHeldByMe(ProcArrayLock));

  if (!already_locked)
  {
    LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  }

  procArray->replication_slot_xmin = xmin;
  procArray->replication_slot_catalog_xmin = catalog_xmin;

  if (!already_locked)
  {
    LWLockRelease(ProcArrayLock);
  }
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
  int i, j;

  Assert(TransactionIdIsValid(xid));

  /*
   * We must hold ProcArrayLock exclusively in order to remove transactions
   * from the PGPROC array.  (See src/backend/access/transam/README.)  It's
   * possible this could be relaxed since we know this routine is only used
   * to abort subtransactions, but pending closer analysis we'd best be
   * conservative.
   *
   * Note that we do not have to be careful about memory ordering of our own
   * reads wrt. GetNewTransactionId() here - only this process can modify
   * relevant fields of MyProc/MyPgXact.  But we do have to be careful about
   * our own writes being well ordered.
   */
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  /*
   * Under normal circumstances xid and xids[] will be in increasing order,
   * as will be the entries in subxids.  Scan backwards to avoid O(N^2)
   * behavior when removing a lot of xids.
   */
  for (i = nxids - 1; i >= 0; i--)
  {
    TransactionId anxid = xids[i];

    for (j = MyPgXact->nxids - 1; j >= 0; j--)
    {
      if (TransactionIdEquals(MyProc->subxids.xids[j], anxid))
      {
        XidCacheRemove(j);
        break;
      }
    }

    /*
     * Ordinarily we should have found it, unless the cache has
     * overflowed. However it's also possible for this routine to be
     * invoked multiple times for the same subtransaction, in case of an
     * error during AbortSubTransaction.  So instead of Assert, emit a
     * debug warning.
     */
    if (j < 0 && !MyPgXact->overflowed)
    {

    }
  }

  for (j = MyPgXact->nxids - 1; j >= 0; j--)
  {
    if (TransactionIdEquals(MyProc->subxids.xids[j], xid))
    {
      XidCacheRemove(j);
      break;
    }
  }
  /* Ordinarily we should have found it, unless the cache has overflowed */
  if (j < 0 && !MyPgXact->overflowed)
  {

  }

  /* Also advance global latestCompletedXid while holding the lock */
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, latestXid))
  {
    ShmemVariableCache->latestCompletedXid = latestXid;
  }

  LWLockRelease(ProcArrayLock);
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