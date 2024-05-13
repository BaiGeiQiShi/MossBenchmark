/*-------------------------------------------------------------------------
 *
 * parallel.c
 *	  Infrastructure for launching parallel workers
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/transam/parallel.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/nbtree.h"
#include "access/parallel.h"
#include "access/session.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_enum.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "commands/async.h"
#include "executor/execParallel.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqmq.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "storage/ipc.h"
#include "storage/predicate.h"
#include "storage/sinval.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/combocid.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/typcache.h"

/*
 * We don't want to waste a lot of memory on an error queue which, most of
 * the time, will process only a handful of small messages.  However, it is
 * desirable to make it large enough that a typical ErrorResponse can be sent
 * without blocking.  That way, a worker that errors out can write the whole
 * message into the queue and terminate without waiting for the user backend.
 */
#define PARALLEL_ERROR_QUEUE_SIZE 16384

/* Magic number for parallel context TOC. */
#define PARALLEL_MAGIC 0x50477c7c

/*
 * Magic numbers for per-context parallel state sharing.  Higher-level code
 * should use smaller values, leaving these very large ones for use by this
 * module.
 */
#define PARALLEL_KEY_FIXED UINT64CONST(0xFFFFFFFFFFFF0001)
#define PARALLEL_KEY_ERROR_QUEUE UINT64CONST(0xFFFFFFFFFFFF0002)
#define PARALLEL_KEY_LIBRARY UINT64CONST(0xFFFFFFFFFFFF0003)
#define PARALLEL_KEY_GUC UINT64CONST(0xFFFFFFFFFFFF0004)
#define PARALLEL_KEY_COMBO_CID UINT64CONST(0xFFFFFFFFFFFF0005)
#define PARALLEL_KEY_TRANSACTION_SNAPSHOT UINT64CONST(0xFFFFFFFFFFFF0006)
#define PARALLEL_KEY_ACTIVE_SNAPSHOT UINT64CONST(0xFFFFFFFFFFFF0007)
#define PARALLEL_KEY_TRANSACTION_STATE UINT64CONST(0xFFFFFFFFFFFF0008)
#define PARALLEL_KEY_ENTRYPOINT UINT64CONST(0xFFFFFFFFFFFF0009)
#define PARALLEL_KEY_SESSION_DSM UINT64CONST(0xFFFFFFFFFFFF000A)
#define PARALLEL_KEY_REINDEX_STATE UINT64CONST(0xFFFFFFFFFFFF000B)
#define PARALLEL_KEY_RELMAPPER_STATE UINT64CONST(0xFFFFFFFFFFFF000C)
#define PARALLEL_KEY_ENUMBLACKLIST UINT64CONST(0xFFFFFFFFFFFF000D)

/* Fixed-size parallel state. */
typedef struct FixedParallelState
{
  /* Fixed-size state that workers must restore. */
  Oid database_id;
  Oid authenticated_user_id;
  Oid current_user_id;
  Oid outer_user_id;
  Oid temp_namespace_id;
  Oid temp_toast_namespace_id;
  int sec_context;
  bool is_superuser;
  PGPROC *parallel_master_pgproc;
  pid_t parallel_master_pid;
  BackendId parallel_master_backend_id;
  TimestampTz xact_ts;
  TimestampTz stmt_ts;
  SerializableXactHandle serializable_xact_handle;

  /* Mutex protects remaining fields. */
  slock_t mutex;

  /* Maximum XactLastRecEnd of any worker. */
  XLogRecPtr last_xlog_end;
} FixedParallelState;

/*
 * Our parallel worker number.  We initialize this to -1, meaning that we are
 * not a parallel worker.  In parallel workers, it will be set to a value >= 0
 * and < the number of workers before any user code is invoked; each parallel
 * worker will get a different parallel worker number.
 */
int ParallelWorkerNumber = -1;

/* Is there a parallel message pending which we need to receive? */
volatile bool ParallelMessagePending = false;

/* Are we initializing a parallel worker? */
bool InitializingParallelWorker = false;

/* Pointer to our fixed parallel state. */
static FixedParallelState *MyFixedParallelState;

/* List of active parallel contexts. */
static dlist_head pcxt_list = DLIST_STATIC_INIT(pcxt_list);

/* Backend-local copy of data from FixedParallelState. */
static pid_t ParallelMasterPid;

/*
 * List of internal parallel worker entry points.  We need this for
 * reasons explained in LookupParallelWorkerFunction(), below.
 */
static const struct
{
  const char *fn_name;
  parallel_worker_main_type fn_addr;
} InternalParallelWorkers[] =

    {{"ParallelQueryMain", ParallelQueryMain}, {"_bt_parallel_build_main", _bt_parallel_build_main}};

/* Private functions. */
static void
HandleParallelMessage(ParallelContext *pcxt, int i, StringInfo msg);
static void
WaitForParallelWorkersToExit(ParallelContext *pcxt);
static parallel_worker_main_type
LookupParallelWorkerFunction(const char *libraryname, const char *funcname);
static void
ParallelWorkerShutdown(int code, Datum arg);

/*
 * Establish a new parallel context.  This should be done after entering
 * parallel mode, and (unless there is an error) the context should be
 * destroyed before exiting the current subtransaction.
 */
ParallelContext *
CreateParallelContext(const char *library_name, const char *function_name, int nworkers)
{


























}

/*
 * Establish the dynamic shared memory segment for a parallel context and
 * copy state and other bookkeeping information that will be needed by
 * parallel workers into it.
 */
void
InitializeParallelDSM(ParallelContext *pcxt)
{
















































































































































































































































}

/*
 * Reinitialize the dynamic shared memory segment for a parallel context such
 * that we could launch workers for it again.
 */
void
ReinitializeParallelDSM(ParallelContext *pcxt)
{






































}

/*
 * Launch parallel workers.
 */
void
LaunchParallelWorkers(ParallelContext *pcxt)
{














































































}

/*
 * Wait for all workers to attach to their error queues, and throw an error if
 * any worker fails to do this.
 *
 * Callers can assume that if this function returns successfully, then the
 * number of workers given by pcxt->nworkers_launched have initialized and
 * attached to their error queues.  Whether or not these workers are guaranteed
 * to still be running depends on what code the caller asked them to run;
 * this function does not guarantee that they have not exited.  However, it
 * does guarantee that any workers which exited must have done so cleanly and
 * after successfully performing the work with which they were tasked.
 *
 * If this function is not called, then some of the workers that were launched
 * may not have been started due to a fork() failure, or may have exited during
 * early startup prior to attaching to the error queue, so nworkers_launched
 * cannot be viewed as completely reliable.  It will never be less than the
 * number of workers which actually started, but it might be more.  Any workers
 * that failed to start will still be discovered by
 * WaitForParallelWorkersToFinish and an error will be thrown at that time,
 * provided that function is eventually reached.
 *
 * In general, the leader process should do as much work as possible before
 * calling this function.  fork() failures and other early-startup failures
 * are very uncommon, and having the leader sit idle when it could be doing
 * useful work is undesirable.  However, if the leader needs to wait for
 * all of its workers or for a specific worker, it may want to call this
 * function before doing so.  If not, it must make some other provision for
 * the failure-to-start case, lest it wait forever.  On the other hand, a
 * leader which never waits for a worker that might not be started yet, or
 * at least never does so prior to WaitForParallelWorkersToFinish(), need not
 * call this function at all.
 */
void
WaitForParallelWorkersToAttach(ParallelContext *pcxt)
{


























































































}

/*
 * Wait for all workers to finish computing.
 *
 * Even if the parallel operation seems to have completed successfully, it's
 * important to call this function afterwards.  We must not miss any errors
 * the workers may have thrown during the parallel operation, or any that they
 * may yet throw while shutting down.
 *
 * Also, we want to update our notion of XactLastRecEnd based on worker
 * feedback.
 */
void
WaitForParallelWorkersToFinish(ParallelContext *pcxt)
{






































































































}

/*
 * Wait for all workers to exit.
 *
 * This function ensures that workers have been completely shutdown.  The
 * difference between WaitForParallelWorkersToFinish and this function is
 * that former just ensures that last message sent by worker backend is
 * received by master backend whereas this ensures the complete shutdown.
 */
static void
WaitForParallelWorkersToExit(ParallelContext *pcxt)
{





























}

/*
 * Destroy a parallel context.
 *
 * If expecting a clean exit, you should use WaitForParallelWorkersToFinish()
 * first, before calling this function.  When this function is invoked, any
 * remaining workers are forcibly killed; the dynamic shared memory segment
 * is unmapped; and we then wait (uninterruptibly) for the workers to exit.
 */
void
DestroyParallelContext(ParallelContext *pcxt)
{


































































}

/*
 * Are there any parallel contexts currently active?
 */
bool
ParallelContextActive(void)
{

}

/*
 * Handle receipt of an interrupt indicating a parallel worker message.
 *
 * Note: this is called within a signal handler!  All we can do is set
 * a flag that will cause the next CHECK_FOR_INTERRUPTS() to invoke
 * HandleParallelMessages().
 */
void
HandleParallelMessageInterrupt(void)
{



}

/*
 * Handle any queued protocol messages received from parallel workers.
 */
void
HandleParallelMessages(void)
{






















































































}

/*
 * Handle a single protocol message received from a single parallel worker.
 */
static void
HandleParallelMessage(ParallelContext *pcxt, int i, StringInfo msg)
{




































































































}

/*
 * End-of-subtransaction cleanup for parallel contexts.
 *
 * Currently, it's forbidden to enter or leave a subtransaction while
 * parallel mode is in effect, so we could just blow away everything.  But
 * we may want to relax that restriction in the future, so this code
 * contemplates that there may be multiple subtransaction IDs in pcxt_list.
 */
void
AtEOSubXact_Parallel(bool isCommit, SubTransactionId mySubId)
{















}

/*
 * End-of-transaction cleanup for parallel contexts.
 */
void
AtEOXact_Parallel(bool isCommit)
{











}

/*
 * Main entrypoint for parallel workers.
 */
void
ParallelWorkerMain(Datum main_arg)
{



















































































































































































































































}

/*
 * Update shared memory with the ending location of the last WAL record we
 * wrote, if it's greater than the value already stored there.
 */
void
ParallelWorkerReportLastRecEnd(XLogRecPtr last_xlog_end)
{









}

/*
 * Make sure the leader tries to read from our error queue one more time.
 * This guards against the case where we exit uncleanly without sending an
 * ErrorResponse to the leader, for example because some code calls proc_exit
 * directly.
 */
static void
ParallelWorkerShutdown(int code, Datum arg)
{

}

/*
 * Look up (and possibly load) a parallel worker entry point function.
 *
 * For functions contained in the core code, we use library name "postgres"
 * and consult the InternalParallelWorkers array.  External functions are
 * looked up, and loaded if necessary, using load_external_function().
 *
 * The point of this is to pass function names as strings across process
 * boundaries.  We can't pass actual function addresses because of the
 * possibility that the function has been loaded at a different address
 * in a different process.  This is obviously a hazard for functions in
 * loadable libraries, but it can happen even for functions in the core code
 * on platforms using EXEC_BACKEND (e.g., Windows).
 *
 * At some point it might be worthwhile to get rid of InternalParallelWorkers[]
 * in favor of applying load_external_function() for core functions too;
 * but that raises portability issues that are not worth addressing now.
 */
static parallel_worker_main_type
LookupParallelWorkerFunction(const char *libraryname, const char *funcname)
{






















}