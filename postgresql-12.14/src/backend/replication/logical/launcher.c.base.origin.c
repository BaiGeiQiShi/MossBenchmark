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









}

/*
 * ApplyLauncherRegister
 *		Register a background worker running the logical replication
 *launcher.
 */
void
ApplyLauncherRegister(void)
{



















}

/*
 * ApplyLauncherShmemInit
 *		Allocate and initialize replication launcher shared memory
 */
void
ApplyLauncherShmemInit(void)
{



















}

/*
 * Check whether current transaction has manipulated logical replication
 * workers.
 */
bool
XactManipulatesLogicalReplicationWorkers(void)
{

}

/*
 * Wakeup the launcher on commit if requested.
 */
void
AtEOXact_ApplyLauncher(bool isCommit)
{































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




}

static void
ApplyLauncherWakeup(void)
{




}

/*
 * Main loop for the apply launcher process.
 */
void
ApplyLauncherMain(Datum main_arg)
{






































































































}

/*
 * Is current process the logical replication launcher?
 */
bool
IsLogicalLauncher(void)
{

}

/*
 * Returns state of the subscriptions.
 */
Datum
pg_stat_get_subscription(PG_FUNCTION_ARGS)
{



































































































































}