/*-------------------------------------------------------------------------
 * launcher.c
 *	   PostgreSQL logical replication worker launcher process
 *
 * Copyright (c) 2016-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/launcher.c
 *
 * NOTES
 *	  This module contains the logical replication worker launcher which
 *	  uses the background worker infrastructure to start the logical
 *	  replication workers for every enabled subscription.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"

#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"

#include "libpq/pqsignal.h"

#include "postmaster/bgworker.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"

#include "replication/logicallauncher.h"
#include "replication/logicalworker.h"
#include "replication/slot.h"
#include "replication/walreceiver.h"
#include "replication/worker_internal.h"

#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"

#include "tcop/tcopprot.h"

#include "utils/memutils.h"
#include "utils/pg_lsn.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"
#include "utils/snapmgr.h"

/* max sleep time between cycles (3min) */
#define DEFAULT_NAPTIME_PER_CYCLE 180000L

int max_logical_replication_workers = 4;
int max_sync_workers_per_subscription = 2;

LogicalRepWorker *MyLogicalRepWorker = NULL;

typedef struct LogicalRepCtxStruct
{
  /* Supervisor process. */
  pid_t launcher_pid;

  /* Background workers. */
  LogicalRepWorker workers[FLEXIBLE_ARRAY_MEMBER];
} LogicalRepCtxStruct;

LogicalRepCtxStruct *LogicalRepCtx;

typedef struct LogicalRepWorkerId
{
  Oid subid;
  Oid relid;
} LogicalRepWorkerId;

typedef struct StopWorkersData
{
  int nestDepth;                  /* Sub-transaction nest level */
  List *workers;                  /* List of LogicalRepWorkerId */
  struct StopWorkersData *parent; /* This need not be an immediate
                                   * subtransaction parent */
} StopWorkersData;

/*
 * Stack of StopWorkersData elements. Each stack element contains the workers
 * to be stopped for that subtransaction.
 */
static StopWorkersData *on_commit_stop_workers = NULL;

static void
ApplyLauncherWakeup(void);
static void
logicalrep_launcher_onexit(int code, Datum arg);
static void
logicalrep_worker_onexit(int code, Datum arg);
static void
logicalrep_worker_detach(void);
static void
logicalrep_worker_cleanup(LogicalRepWorker *worker);

/* Flags set by signal handlers */
static volatile sig_atomic_t got_SIGHUP = false;

static bool on_commit_launcher_wakeup = false;

Datum pg_stat_get_subscription(PG_FUNCTION_ARGS);

/*
 * Load the list of subscriptions.
 *
 * Only the fields interesting for worker start/stop functions are filled for
 * each subscription.
 */
static List *
get_subscription_list(void)
{
  List *res = NIL;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tup;
  MemoryContext resultcxt;

  /* This is the context that we will allocate our output data in */
  resultcxt = CurrentMemoryContext;

  /*
   * Start a transaction so we can access pg_database, and get a snapshot.
   * We don't have a use for the snapshot itself, but we're interested in
   * the secondary effect that it sets RecentGlobalXmin.  (This is critical
   * for anything that reads heap pages, because HOT may decide to prune
   * them even if the process doesn't attempt to modify any tuples.)
   */
  StartTransactionCommand();
  (void)GetTransactionSnapshot();

  rel = table_open(SubscriptionRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);

  while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
  {
    Form_pg_subscription subform = (Form_pg_subscription)GETSTRUCT(tup);
    Subscription *sub;
    MemoryContext oldcxt;

    /*
     * Allocate our results in the caller's context, not the
     * transaction's. We do this inside the loop, and restore the original
     * context at the end, so that leaky things like heap_getnext() are
     * not called in a potentially long-lived context.
     */
    oldcxt = MemoryContextSwitchTo(resultcxt);

    sub = (Subscription *)palloc0(sizeof(Subscription));
    sub->oid = subform->oid;
    sub->dbid = subform->subdbid;
    sub->owner = subform->subowner;
    sub->enabled = subform->subenabled;
    sub->name = pstrdup(NameStr(subform->subname));
    /* We don't fill fields we are not interested in. */

    res = lappend(res, sub);
    MemoryContextSwitchTo(oldcxt);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  CommitTransactionCommand();

  return res;
}

/*
 * Wait for a background worker to start up and attach to the shmem context.
 *
 * This is only needed for cleaning up the shared memory in case the worker
 * fails to attach.
 */
static void
WaitForReplicationWorkerAttach(LogicalRepWorker *worker, uint16 generation, BackgroundWorkerHandle *handle)
{

















































}

/*
 * Walks the workers array and searches for one that matches given
 * subscription id and relid.
 */
LogicalRepWorker *
logicalrep_worker_find(Oid subid, Oid relid, bool only_running)
{


















}

/*
 * Similar to logicalrep_worker_find(), but returns list of all workers for
 * the subscription, instead just one.
 */
List *
logicalrep_workers_find(Oid subid, bool only_running)
{
  int i;
  List *res = NIL;

  Assert(LWLockHeldByMe(LogicalRepWorkerLock));

  /* Search for attached worker for a given subscription id. */
  for (i = 0; i < max_logical_replication_workers; i++)
  {
    LogicalRepWorker *w = &LogicalRepCtx->workers[i];

    if (w->in_use && w->subid == subid && (!only_running || w->proc))
    {

    }
  }

  return res;
}

/*
 * Start new apply background worker, if possible.
 */
void
logicalrep_worker_launch(Oid dbid, Oid subid, const char *subname, Oid userid, Oid relid)
{























































































































































}

/*
 * Stop the logical replication worker for subid/relid, if any, and wait until
 * it detaches from the slot.
 */
void
logicalrep_worker_stop(Oid subid, Oid relid)
{

























































































}

/*
 * Request worker for specified sub/rel to be stopped on commit.
 */
void
logicalrep_worker_stop_at_commit(Oid subid, Oid relid)
{


































}

/*
 * Wake up (using latch) any logical replication worker for specified sub/rel.
 */
void
logicalrep_worker_wakeup(Oid subid, Oid relid)
{












}

/*
 * Wake up (using latch) the specified logical replication worker.
 *
 * Caller must hold lock, else worker->proc could change under us.
 */
void
logicalrep_worker_wakeup_ptr(LogicalRepWorker *worker)
{



}

/*
 * Attach to a slot.
 */
void
logicalrep_worker_attach(int slot)
{






















}

/*
 * Detach the worker (cleans up the worker info).
 */
static void
logicalrep_worker_detach(void)
{






}

/*
 * Clean up worker info.
 */
static void
logicalrep_worker_cleanup(LogicalRepWorker *worker)
{








}

/*
 * Cleanup function for logical replication launcher.
 *
 * Called on logical replication launcher exit.
 */
static void
logicalrep_launcher_onexit(int code, Datum arg)
{
  LogicalRepCtx->launcher_pid = 0;
}

/*
 * Cleanup function.
 *
 * Called on logical replication worker exit.
 */
static void
logicalrep_worker_onexit(int code, Datum arg)
{









}

/* SIGHUP: set flag to reload configuration at next convenient time */
static void
logicalrep_launcher_sighup(SIGNAL_ARGS)
{








}

/*
 * Count the number of registered (not necessarily running) sync workers
 * for a subscription.
 */
int
logicalrep_sync_worker_count(Oid subid)
{

















}

/*
 * ApplyLauncherShmemSize
 *		Compute space needed for replication launcher shared memory
 */
Size
ApplyLauncherShmemSize(void)
{
  Size size;

  /*
   * Need the fixed struct and the array of LogicalRepWorker.
   */
  size = sizeof(LogicalRepCtxStruct);
  size = MAXALIGN(size);
  size = add_size(size, mul_size(max_logical_replication_workers, sizeof(LogicalRepWorker)));
  return size;
}

/*
 * ApplyLauncherRegister
 *		Register a background worker running the logical replication
 *launcher.
 */
void
ApplyLauncherRegister(void)
{
  BackgroundWorker bgw;

  if (max_logical_replication_workers == 0)
  {

  }

  memset(&bgw, 0, sizeof(bgw));
  bgw.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
  snprintf(bgw.bgw_library_name, BGW_MAXLEN, "postgres");
  snprintf(bgw.bgw_function_name, BGW_MAXLEN, "ApplyLauncherMain");
  snprintf(bgw.bgw_name, BGW_MAXLEN, "logical replication launcher");
  snprintf(bgw.bgw_type, BGW_MAXLEN, "logical replication launcher");
  bgw.bgw_restart_time = 5;
  bgw.bgw_notify_pid = 0;
  bgw.bgw_main_arg = (Datum)0;

  RegisterBackgroundWorker(&bgw);
}

/*
 * ApplyLauncherShmemInit
 *		Allocate and initialize replication launcher shared memory
 */
void
ApplyLauncherShmemInit(void)
{
  bool found;

  LogicalRepCtx = (LogicalRepCtxStruct *)ShmemInitStruct("Logical Replication Launcher Data", ApplyLauncherShmemSize(), &found);

  if (!found)
  {
    int slot;

    memset(LogicalRepCtx, 0, ApplyLauncherShmemSize());

    /* Initialize memory and spin locks for each worker slot. */
    for (slot = 0; slot < max_logical_replication_workers; slot++)
    {
      LogicalRepWorker *worker = &LogicalRepCtx->workers[slot];

      memset(worker, 0, sizeof(LogicalRepWorker));
      SpinLockInit(&worker->relmutex);
    }
  }
}

/*
 * Check whether current transaction has manipulated logical replication
 * workers.
 */
bool
XactManipulatesLogicalReplicationWorkers(void)
{
  return (on_commit_stop_workers != NULL);
}

/*
 * Wakeup the launcher on commit if requested.
 */
void
AtEOXact_ApplyLauncher(bool isCommit)
{

  Assert(on_commit_stop_workers == NULL || (on_commit_stop_workers->nestDepth == 1 && on_commit_stop_workers->parent == NULL));

  if (isCommit)
  {
    ListCell *lc;

    if (on_commit_stop_workers != NULL)
    {
      List *workers = on_commit_stop_workers->workers;







    }

    if (on_commit_launcher_wakeup)
    {
      ApplyLauncherWakeup();
    }
  }

  /*
   * No need to pfree on_commit_stop_workers.  It was allocated in
   * transaction memory context, which is going to be cleaned soon.
   */
  on_commit_stop_workers = NULL;
  on_commit_launcher_wakeup = false;
}

/*
 * On commit, merge the current on_commit_stop_workers list into the
 * immediate parent, if present.
 * On rollback, discard the current on_commit_stop_workers list.
 * Pop out the stack.
 */
void
AtEOSubXact_ApplyLauncher(bool isCommit, int nestDepth)
{
  StopWorkersData *parent;

  /* Exit immediately if there's no work to do at this level. */
  if (on_commit_stop_workers == NULL || on_commit_stop_workers->nestDepth < nestDepth)
  {
    return;
  }






























  /*
   * We have taken care of the current subtransaction workers list for both
   * abort or commit. So we are ready to pop the stack.
   */


}

/*
 * Request wakeup of the launcher on commit of the transaction.
 *
 * This is used to send launcher signal to stop sleeping and process the
 * subscriptions when current transaction commits. Should be used when new
 * tuple was added to the pg_subscription catalog.
 */
void
ApplyLauncherWakeupAtCommit(void)
{
  if (!on_commit_launcher_wakeup)
  {
    on_commit_launcher_wakeup = true;
  }
}

static void
ApplyLauncherWakeup(void)
{
  if (LogicalRepCtx->launcher_pid != 0)
  {
    kill(LogicalRepCtx->launcher_pid, SIGUSR1);
  }
}

/*
 * Main loop for the apply launcher process.
 */
void
ApplyLauncherMain(Datum main_arg)
{
  TimestampTz last_start_time = 0;

  ereport(DEBUG1, (errmsg("logical replication launcher started")));

  before_shmem_exit(logicalrep_launcher_onexit, (Datum)0);

  Assert(LogicalRepCtx->launcher_pid == 0);
  LogicalRepCtx->launcher_pid = MyProcPid;

  /* Establish signal handlers. */
  pqsignal(SIGHUP, logicalrep_launcher_sighup);
  pqsignal(SIGTERM, die);
  BackgroundWorkerUnblockSignals();

  /*
   * Establish connection to nailed catalogs (we only ever access
   * pg_subscription).
   */
  BackgroundWorkerInitializeConnection(NULL, NULL, 0);

  /* Enter main loop */
  for (;;)
  {
    int rc;
    List *sublist;
    ListCell *lc;
    MemoryContext subctx;
    MemoryContext oldctx;
    TimestampTz now;
    long wait_time = DEFAULT_NAPTIME_PER_CYCLE;

    CHECK_FOR_INTERRUPTS();

    now = GetCurrentTimestamp();

    /* Limit the start retry to once a wal_retrieve_retry_interval */
    if (TimestampDifferenceExceeds(last_start_time, now, wal_retrieve_retry_interval))
    {
      /* Use temporary context for the database list and worker info. */
      subctx = AllocSetContextCreate(TopMemoryContext, "Logical Replication Launcher sublist", ALLOCSET_DEFAULT_SIZES);
      oldctx = MemoryContextSwitchTo(subctx);

      /* search for subscriptions to start or stop. */
      sublist = get_subscription_list();

      /* Start the missing workers for enabled subscriptions. */
      foreach (lc, sublist)
      {
        Subscription *sub = (Subscription *)lfirst(lc);
        LogicalRepWorker *w;

        if (!sub->enabled)
        {
          continue;
        }












      }

      /* Switch back to original memory context. */
      MemoryContextSwitchTo(oldctx);
      /* Clean the temporary memory. */
      MemoryContextDelete(subctx);
    }
    else
    {
      /*
       * The wait in previous cycle was interrupted in less than
       * wal_retrieve_retry_interval since last worker was started, this
       * usually means crash of the worker, so we should retry in
       * wal_retrieve_retry_interval again.
       */

    }

    /* Wait for more work. */
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, wait_time, WAIT_EVENT_LOGICAL_LAUNCHER_MAIN);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }

    if (got_SIGHUP)
    {


    }
  }

  /* Not reachable */
}

/*
 * Is current process the logical replication launcher?
 */
bool
IsLogicalLauncher(void)
{
  return LogicalRepCtx->launcher_pid == MyProcPid;
}

/*
 * Returns state of the subscriptions.
 */
Datum
pg_stat_get_subscription(PG_FUNCTION_ARGS)
{



































































































































}