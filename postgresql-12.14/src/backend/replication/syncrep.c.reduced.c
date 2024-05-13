/*-------------------------------------------------------------------------
 *
 * syncrep.c
 *
 * Synchronous replication is new as of PostgreSQL 9.1.
 *
 * If requested, transaction commits wait until their commit LSN are
 * acknowledged by the synchronous standbys.
 *
 * This module contains the code for waiting and release of backends.
 * All code in this module executes on the primary. The core streaming
 * replication transport remains within WALreceiver/WALsender modules.
 *
 * The essence of this design is that it isolates all logic about
 * waiting/releasing onto the primary. The primary defines which standbys
 * it wishes to wait for. The standbys are completely unaware of the
 * durability requirements of transactions on the primary, reducing the
 * complexity of the code and streamlining both standby operations and
 * network bandwidth because there is no requirement to ship
 * per-transaction state information.
 *
 * Replication is either synchronous or not synchronous (async). If it is
 * async, we just fastpath out of here. If it is sync, then we wait for
 * the write, flush or apply location on the standby before releasing
 * the waiting backend. Further complexity in that interaction is
 * expected in later releases.
 *
 * The best performing way to manage the waiting backends is to have a
 * single ordered queue of waiting backends, so that we can avoid
 * searching the through all waiters each time we receive a reply.
 *
 * In 9.5 or before only a single standby could be considered as
 * synchronous. In 9.6 we support a priority-based multiple synchronous
 * standbys. In 10.0 a quorum-based multiple synchronous standbys is also
 * supported. The number of synchronous standbys that transactions
 * must wait for replies from is specified in synchronous_standby_names.
 * This parameter also specifies a list of standby names and the method
 * (FIRST and ANY) to choose synchronous standbys from the listed ones.
 *
 * The method FIRST specifies a priority-based synchronous replication
 * and makes transaction commits wait until their WAL records are
 * replicated to the requested number of synchronous standbys chosen based
 * on their priorities. The standbys whose names appear earlier in the list
 * are given higher priority and will be considered as synchronous.
 * Other standby servers appearing later in this list represent potential
 * synchronous standbys. If any of the current synchronous standbys
 * disconnects for whatever reason, it will be replaced immediately with
 * the next-highest-priority standby.
 *
 * The method ANY specifies a quorum-based synchronous replication
 * and makes transaction commits wait until their WAL records are
 * replicated to at least the requested number of synchronous standbys
 * in the list. All the standbys appearing in the list are considered as
 * candidates for quorum synchronous standbys.
 *
 * If neither FIRST nor ANY is specified, FIRST is used as the method.
 * This is for backward compatibility with 9.6 or before where only a
 * priority-based sync replication was supported.
 *
 * Before the standbys chosen from synchronous_standby_names can
 * become the synchronous standbys they must have caught up with
 * the primary; that may take some time. Once caught up,
 * the standbys which are considered as synchronous at that moment
 * will release waiters from the queue.
 *
 * Portions Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/syncrep.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "access/xact.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/ps_status.h"

/* User-settable parameters for sync rep */
char *SyncRepStandbyNames;

#define SyncStandbysDefined()                                                  \
  (SyncRepStandbyNames != NULL && SyncRepStandbyNames[0] != '\0')

static bool announce_next_takeover = true;

SyncRepConfigData *SyncRepConfig = NULL;
static int SyncRepWaitMode = SYNC_REP_NO_WAIT;

static void
SyncRepQueueInsert(int mode);
static void
SyncRepCancelWait(void);
static int
SyncRepWakeQueue(bool all, int mode);

static bool
SyncRepGetSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr,
                     XLogRecPtr *applyPtr, bool *am_sync);
static void
SyncRepGetOldestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr,
                           XLogRecPtr *applyPtr,
                           SyncRepStandbyData *sync_standbys, int num_standbys);
static void
SyncRepGetNthLatestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr,
                              XLogRecPtr *applyPtr,
                              SyncRepStandbyData *sync_standbys,
                              int num_standbys, uint8 nth);
static int
SyncRepGetStandbyPriority(void);
static List *
SyncRepGetSyncStandbysPriority(bool *am_sync);
static List *
SyncRepGetSyncStandbysQuorum(bool *am_sync);
static int
standby_priority_comparator(const void *a, const void *b);
static int
cmp_lsn(const void *a, const void *b);

#ifdef USE_ASSERT_CHECKING
static bool
SyncRepQueueIsOrderedByLSN(int mode);
#endif

/*
 * ===========================================================
 * Synchronous Replication functions for normal user backends
 * ===========================================================
 */

/*
 * Wait for synchronous replication, if requested by user.
 *
 * Initially backends start in state SYNC_REP_NOT_WAITING and then
 * change that state to SYNC_REP_WAITING before adding ourselves
 * to the wait queue. During SyncRepWakeQueue() a WALSender changes
 * the state to SYNC_REP_WAIT_COMPLETE once replication is confirmed.
 * This backend then resets its state to SYNC_REP_NOT_WAITING.
 *
 * 'lsn' represents the LSN to wait for.  'commit' indicates whether this LSN
 * represents a commit record.  If it doesn't, then we wait only for the WAL
 * to be flushed if synchronous_commit is set to the higher level of
 * remote_apply, because only commit records provide apply feedback.
 */
void
SyncRepWaitForLSN(XLogRecPtr lsn, bool commit)
{
  char *new_status = NULL;
  const char *old_status;
  int mode;

  /* Cap the level for anything other than commit to remote flush only. */
  if (commit) {
    mode = SyncRepWaitMode;
  } else {



  /*
   * Fast exit if user has not requested sync replication.
   */
  if (!SyncRepRequested()) {



  Assert(SHMQueueIsDetached(&(MyProc->syncRepLinks)));
  Assert(WalSndCtl != NULL);

  LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);
  Assert(MyProc->syncRepState == SYNC_REP_NOT_WAITING);

  /*
   * We don't wait for sync rep if WalSndCtl->sync_standbys_defined is not
   * set.  See SyncRepUpdateSyncStandbysDefined.
   *
   * Also check that the standby hasn't already replied. Unlikely race
   * condition but we'll be fetching that cache line anyway so it's likely
   * to be a low cost check.
   */
  if (!WalSndCtl->sync_standbys_defined || lsn <= WalSndCtl->lsn[mode]) {
    LWLockRelease(SyncRepLock);
    return;
  }































































































































/*
 * Insert MyProc into the specified SyncRepQueue, maintaining sorted invariant.
 *
 * Usually we will go at tail of queue, though it's possible that we arrive
 * here out of order, so start at tail and work back to insertion point.
 */
static void
SyncRepQueueInsert(int mode)
{



























}

/*
 * Acquire SyncRepLock and cancel any wait currently in progress.
 */
static void
SyncRepCancelWait(void)
{






}

void
SyncRepCleanupAtProcExit(void)
{
  /*
   * First check if we are removed from the queue without the lock to not
   * slow down backend exit.
   */
  if (!SHMQueueIsDetached(&(MyProc->syncRepLinks))) {









}

/*
 * ===========================================================
 * Synchronous Replication functions for wal sender processes
 * ===========================================================
 */

/*
 * Take any action required to initialise sync rep state from config
 * data. Called at WALSender startup and after each SIGHUP.
 */
void
SyncRepInitConfig(void)
{
















}

/*
 * Update the LSNs on each queue based upon our latest state. This
 * implements a simple policy of first-valid-sync-standby-releases-waiter.
 *
 * Other policies are possible, which would change what we do here and
 * perhaps also which information we store as well.
 */
void
SyncRepReleaseWaiters(void)
{
































































































}

/*
 * Calculate the synced Write, Flush and Apply positions among sync standbys.
 *
 * Return false if the number of sync standbys is less than
 * synchronous_standby_names specifies. Otherwise return true and
 * store the positions into *writePtr, *flushPtr and *applyPtr.
 *
 * On return, *am_sync is set to true if this walsender is connecting to
 * sync standby. Otherwise it's set to false.
 */
static bool
SyncRepGetSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr,
                     XLogRecPtr *applyPtr, bool *am_sync)
{


























































}

/*
 * Calculate the oldest Write, Flush and Apply positions among sync standbys.
 */
static void
SyncRepGetOldestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr,
                           XLogRecPtr *applyPtr,
                           SyncRepStandbyData *sync_standbys, int num_standbys)
{






















}

/*
 * Calculate the Nth latest Write, Flush and Apply positions among sync
 * standbys.
 */
static void
SyncRepGetNthLatestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr,
                              XLogRecPtr *applyPtr,
                              SyncRepStandbyData *sync_standbys,
                              int num_standbys, uint8 nth)
{































}

/*
 * Compare lsn in order to sort array in descending order.
 */
static int
cmp_lsn(const void *a, const void *b)
{










}

/*
 * Return data about walsenders that are candidates to be sync standbys.
 *
 * *standbys is set to a palloc'd array of structs of per-walsender data,
 * and the number of valid entries (candidate sync senders) is returned.
 * (This might be more or fewer than num_sync; caller must check.)
 */
int
SyncRepGetCandidateStandbys(SyncRepStandbyData **standbys)
{









































































}

/*
 * qsort comparator to sort SyncRepStandbyData entries by priority
 */
static int
standby_priority_comparator(const void *a, const void *b)
{














}

/*
 * Return the list of sync standbys, or NIL if no sync standby is connected.
 *
 * The caller must hold SyncRepLock.
 *
 * On return, *am_sync is set to true if this walsender is connecting to
 * sync standby. Otherwise it's set to false.
 *
 * XXX This function is BROKEN and should not be used in new code.  It has
 * an inherent race condition, since the returned list of integer indexes
 * might no longer correspond to reality.
 */
List *
SyncRepGetSyncStandbys(bool *am_sync)
{













}

/*
 * Return the list of all the candidates for quorum sync standbys,
 * or NIL if no such standby is connected.
 *
 * The caller must hold SyncRepLock. This function must be called only in
 * a quorum-based sync replication.
 *
 * On return, *am_sync is set to true if this walsender is connecting to
 * sync standby. Otherwise it's set to false.
 */
static List *
SyncRepGetSyncStandbysQuorum(bool *am_sync)
{



















































}

/*
 * Return the list of sync standbys chosen based on their priorities,
 * or NIL if no sync standby is connected.
 *
 * If there are multiple standbys with the same priority,
 * the first one found is selected preferentially.
 *
 * The caller must hold SyncRepLock. This function must be called only in
 * a priority-based sync replication.
 *
 * On return, *am_sync is set to true if this walsender is connecting to
 * sync standby. Otherwise it's set to false.
 */
static List *
SyncRepGetSyncStandbysPriority(bool *am_sync)
{













































































































































































}

/*
 * Check if we are in the list of sync standbys, and if so, determine
 * priority sequence. Return priority if set, or zero to indicate that
 * we are not a potential sync standby.
 *
 * Compare the parameter SyncRepStandbyNames against the application_name
 * for this WALSender, or allow any name if we find a wildcard "*".
 */
static int
SyncRepGetStandbyPriority(void)
{



































}

/*
 * Walk the specified queue from head.  Set the state of any backends that
 * need to be woken, remove them from the queue, and then wake them.
 * Pass all = true to wake whole queue; otherwise, just wake up to
 * the walsender's LSN.
 *
 * Must hold SyncRepLock.
 */
static int
SyncRepWakeQueue(bool all, int mode)
{
























































}

/*
 * The checkpointer calls this as needed to update the shared
 * sync_standbys_defined flag, so that backends don't remain permanently wedged
 * if synchronous_standby_names is unset.  It's safe to check the current value
 * without the lock, because it's only ever updated by one process.  But we
 * must take the lock to change it.
 */
void
SyncRepUpdateSyncStandbysDefined(void)
{
  bool sync_standbys_defined = SyncStandbysDefined();

  if (sync_standbys_defined != WalSndCtl->sync_standbys_defined) {


























}

#ifdef USE_ASSERT_CHECKING
static bool
SyncRepQueueIsOrderedByLSN(int mode)
{
  PGPROC *proc = NULL;
  XLogRecPtr lastLSN;

  Assert(mode >= 0 && mode < NUM_SYNC_REP_WAIT_MODE);

  lastLSN = 0;

  proc = (PGPROC *)SHMQueueNext(&(WalSndCtl->SyncRepQueue[mode]),
                                &(WalSndCtl->SyncRepQueue[mode]),
                                offsetof(PGPROC, syncRepLinks));

  while (proc) {
    /*
     * Check the queue is ordered by LSN and that multiple procs don't
     * have matching LSNs
     */
    if (proc->waitLSN <= lastLSN) {
      return false;
    }

    lastLSN = proc->waitLSN;

    proc = (PGPROC *)SHMQueueNext(&(WalSndCtl->SyncRepQueue[mode]),
                                  &(proc->syncRepLinks),
                                  offsetof(PGPROC, syncRepLinks));
  }

  return true;
}
#endif

/*
 * ===========================================================
 * Synchronous Replication functions executed by any process
 * ===========================================================
 */

bool
check_synchronous_standby_names(char **newval, void **extra, GucSource source)
{
  if (*newval != NULL && (*newval)[0] != '\0') {













































  } else {
    *extra = NULL;
  }

  return true;
}

void
assign_synchronous_standby_names(const char *newval, void *extra)
{
  SyncRepConfig = (SyncRepConfigData *)extra;
}

void
assign_synchronous_commit(int newval, void *extra)
{
  switch (newval) {
  case SYNCHRONOUS_COMMIT_REMOTE_WRITE:


  case SYNCHRONOUS_COMMIT_REMOTE_FLUSH:
    SyncRepWaitMode = SYNC_REP_WAIT_FLUSH;
    break;
  case SYNCHRONOUS_COMMIT_REMOTE_APPLY:


  default:;
    SyncRepWaitMode = SYNC_REP_NO_WAIT;
    break;
  }
}
