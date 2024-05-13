/*--------------------------------------------------------------------
 * bgworker.c
 *		POSTGRES pluggable background workers implementation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/bgworker.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>

#include "libpq/pqsignal.h"
#include "access/parallel.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "postmaster/bgworker_internals.h"
#include "postmaster/postmaster.h"
#include "replication/logicallauncher.h"
#include "replication/logicalworker.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/shmem.h"
#include "tcop/tcopprot.h"
#include "utils/ascii.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"

/*
 * The postmaster's list of registered background workers, in private memory.
 */
slist_head BackgroundWorkerList = SLIST_STATIC_INIT(BackgroundWorkerList);

/*
 * BackgroundWorkerSlots exist in shared memory and can be accessed (via
 * the BackgroundWorkerArray) by both the postmaster and by regular backends.
 * However, the postmaster cannot take locks, even spinlocks, because this
 * might allow it to crash or become wedged if shared memory gets corrupted.
 * Such an outcome is intolerable.  Therefore, we need a lockless protocol
 * for coordinating access to this data.
 *
 * The 'in_use' flag is used to hand off responsibility for the slot between
 * the postmaster and the rest of the system.  When 'in_use' is false,
 * the postmaster will ignore the slot entirely, except for the 'in_use' flag
 * itself, which it may read.  In this state, regular backends may modify the
 * slot.  Once a backend sets 'in_use' to true, the slot becomes the
 * responsibility of the postmaster.  Regular backends may no longer modify it,
 * but the postmaster may examine it.  Thus, a backend initializing a slot
 * must fully initialize the slot - and insert a write memory barrier - before
 * marking it as in use.
 *
 * As an exception, however, even when the slot is in use, regular backends
 * may set the 'terminate' flag for a slot, telling the postmaster not
 * to restart it.  Once the background worker is no longer running, the slot
 * will be released for reuse.
 *
 * In addition to coordinating with the postmaster, backends modifying this
 * data structure must coordinate with each other.  Since they can take locks,
 * this is straightforward: any backend wishing to manipulate a slot must
 * take BackgroundWorkerLock in exclusive mode.  Backends wishing to read
 * data that might get concurrently modified by other backends should take
 * this lock in shared mode.  No matter what, backends reading this data
 * structure must be able to tolerate concurrent modifications by the
 * postmaster.
 */
typedef struct BackgroundWorkerSlot
{
  bool in_use;
  bool terminate;
  pid_t pid;         /* InvalidPid = not started yet; 0 = dead */
  uint64 generation; /* incremented when slot is recycled */
  BackgroundWorker worker;
} BackgroundWorkerSlot;

/*
 * In order to limit the total number of parallel workers (according to
 * max_parallel_workers GUC), we maintain the number of active parallel
 * workers.  Since the postmaster cannot take locks, two variables are used for
 * this purpose: the number of registered parallel workers (modified by the
 * backends, protected by BackgroundWorkerLock) and the number of terminated
 * parallel workers (modified only by the postmaster, lockless).  The active
 * number of parallel workers is the number of registered workers minus the
 * terminated ones.  These counters can of course overflow, but it's not
 * important here since the subtraction will still give the right number.
 */
typedef struct BackgroundWorkerArray
{
  int total_slots;
  uint32 parallel_register_count;
  uint32 parallel_terminate_count;
  BackgroundWorkerSlot slot[FLEXIBLE_ARRAY_MEMBER];
} BackgroundWorkerArray;

struct BackgroundWorkerHandle
{
  int slot;
  uint64 generation;
};

static BackgroundWorkerArray *BackgroundWorkerData;

/*
 * List of internal background worker entry points.  We need this for
 * reasons explained in LookupBackgroundWorkerFunction(), below.
 */
static const struct
{
  const char *fn_name;
  bgworker_main_type fn_addr;
} InternalBGWorkers[] =

    {{"ParallelWorkerMain", ParallelWorkerMain}, {"ApplyLauncherMain", ApplyLauncherMain}, {"ApplyWorkerMain", ApplyWorkerMain}};

/* Private functions. */
static bgworker_main_type
LookupBackgroundWorkerFunction(const char *libraryname, const char *funcname);

/*
 * Calculate shared memory needed.
 */
Size
BackgroundWorkerShmemSize(void)
{







}

/*
 * Initialize shared memory.
 */
void
BackgroundWorkerShmemInit(void)
{


















































}

/*
 * Search the postmaster's backend-private list of RegisteredBgWorker objects
 * for the one that maps to the given slot number.
 */
static RegisteredBgWorker *
FindRegisteredWorkerBySlotNumber(int slotno)
{














}

/*
 * Notice changes to shared memory made by other backends.
 * Accept new worker requests only if allow_new_workers is true.
 *
 * This code runs in the postmaster, so we must be very careful not to assume
 * that shared memory contents are sane.  Otherwise, a rogue backend could
 * take out the postmaster.
 */
void
BackgroundWorkerStateChange(bool allow_new_workers)
{






































































































































































}

/*
 * Forget about a background worker that's no longer needed.
 *
 * The worker must be identified by passing an slist_mutable_iter that
 * points to it.  This convention allows deletion of workers during
 * searches of the worker list, and saves having to search the list again.
 *
 * Caller is responsible for notifying bgw_notify_pid, if appropriate.
 *
 * This function must be invoked only in the postmaster.
 */
void
ForgetBackgroundWorker(slist_mutable_iter *cur)
{

























}

/*
 * Report the PID of a newly-launched background worker in shared memory.
 *
 * This function should only be called from the postmaster.
 */
void
ReportBackgroundWorkerPID(RegisteredBgWorker *rw)
{










}

/*
 * Report that the PID of a background worker is now zero because a
 * previously-running background worker has exited.
 *
 * This function should only be called from the postmaster.
 */
void
ReportBackgroundWorkerExit(slist_mutable_iter *cur)
{



























}

/*
 * Cancel SIGUSR1 notifications for a PID belonging to an exiting backend.
 *
 * This function should only be called from the postmaster.
 */
void
BackgroundWorkerStopNotifications(pid_t pid)
{












}

/*
 * Cancel any not-yet-started worker requests that have waiting processes.
 *
 * This is called during a normal ("smart" or "fast") database shutdown.
 * After this point, no new background workers will be started, so anything
 * that might be waiting for them needs to be kicked off its wait.  We do
 * that by cancelling the bgworker registration entirely, which is perhaps
 * overkill, but since we're shutting down it does not matter whether the
 * registration record sticks around.
 *
 * This function should only be called from the postmaster.
 */
void
ForgetUnstartedBackgroundWorkers(void)
{
























}

/*
 * Reset background worker crash state.
 *
 * We assume that, after a crash-and-restart cycle, background workers without
 * the never-restart flag should be restarted immediately, instead of waiting
 * for bgw_restart_time to elapse.  On the other hand, workers with that flag
 * should be forgotten immediately, since we won't ever restart them.
 *
 * This function should only be called from the postmaster.
 */
void
ResetBackgroundWorkerCrashTimes(void)
{










































}

#ifdef EXEC_BACKEND
/*
 * In EXEC_BACKEND mode, workers use this to retrieve their details from
 * shared memory.
 */
BackgroundWorker *
BackgroundWorkerEntry(int slotno)
{
  static BackgroundWorker myEntry;
  BackgroundWorkerSlot *slot;

  Assert(slotno < BackgroundWorkerData->total_slots);
  slot = &BackgroundWorkerData->slot[slotno];
  Assert(slot->in_use);

  /* must copy this in case we don't intend to retain shmem access */
  memcpy(&myEntry, &slot->worker, sizeof myEntry);
  return &myEntry;
}
#endif

/*
 * Complain about the BackgroundWorker definition using error level elevel.
 * Return true if it looks ok, false if not (unless elevel >= ERROR, in
 * which case we won't return at all in the not-OK case).
 */
static bool
SanityCheckBackgroundWorker(BackgroundWorker *worker, int elevel)
{












































}

static void
bgworker_quickdie(SIGNAL_ARGS)
{















}

/*
 * Standard SIGTERM handler for background workers
 */
static void
bgworker_die(SIGNAL_ARGS)
{



}

/*
 * Standard SIGUSR1 handler for unconnected workers
 *
 * Here, we want to make sure an unconnected worker will at least heed
 * latch activity.
 */
static void
bgworker_sigusr1_handler(SIGNAL_ARGS)
{





}

/*
 * Start a new background worker
 *
 * This is the main entry point for background worker, to be called from
 * postmaster.
 */
void
StartBackgroundWorker(void)
{

















































































































































}

/*
 * Register a new static background worker.
 *
 * This can only be called directly from postmaster or in the _PG_init
 * function of a module library that's loaded by shared_preload_libraries;
 * otherwise it will have no effect.
 */
void
RegisterBackgroundWorker(BackgroundWorker *worker)
{


























































}

/*
 * Register a new background worker from a regular backend.
 *
 * Returns true on success and false on failure.  Failure typically indicates
 * that no background worker slots are currently available.
 *
 * If handle != NULL, we'll set *handle to a pointer that can subsequently
 * be used as an argument to GetBackgroundWorkerPid().  The caller can
 * free this pointer using pfree(), if desired.
 */
bool
RegisterDynamicBackgroundWorker(BackgroundWorker *worker, BackgroundWorkerHandle **handle)
{





























































































}

/*
 * Get the PID of a dynamically-registered background worker.
 *
 * If the worker is determined to be running, the return value will be
 * BGWH_STARTED and *pidp will get the PID of the worker process.  If the
 * postmaster has not yet attempted to start the worker, the return value will
 * be BGWH_NOT_YET_STARTED.  Otherwise, the return value is BGWH_STOPPED.
 *
 * BGWH_STOPPED can indicate either that the worker is temporarily stopped
 * (because it is configured for automatic restart and exited non-zero),
 * or that the worker is permanently stopped (because it exited with exit
 * code 0, or was not configured for automatic restart), or even that the
 * worker was unregistered without ever starting (either because startup
 * failed and the worker is not configured for automatic restart, or because
 * TerminateBackgroundWorker was used before the worker was successfully
 * started).
 */
BgwHandleStatus
GetBackgroundWorkerPid(BackgroundWorkerHandle *handle, pid_t *pidp)
{














































}

/*
 * Wait for a background worker to start up.
 *
 * This is like GetBackgroundWorkerPid(), except that if the worker has not
 * yet started, we wait for it to do so; thus, BGWH_NOT_YET_STARTED is never
 * returned.  However, if the postmaster has died, we give up and return
 * BGWH_POSTMASTER_DIED, since it that case we know that startup will not
 * take place.
 *
 * The caller *must* have set our PID as the worker's bgw_notify_pid,
 * else we will not be awoken promptly when the worker's state changes.
 */
BgwHandleStatus
WaitForBackgroundWorkerStartup(BackgroundWorkerHandle *handle, pid_t *pidp)
{































}

/*
 * Wait for a background worker to stop.
 *
 * If the worker hasn't yet started, or is running, we wait for it to stop
 * and then return BGWH_STOPPED.  However, if the postmaster has died, we give
 * up and return BGWH_POSTMASTER_DIED, because it's the postmaster that
 * notifies us when a worker's state changes.
 *
 * The caller *must* have set our PID as the worker's bgw_notify_pid,
 * else we will not be awoken promptly when the worker's state changes.
 */
BgwHandleStatus
WaitForBackgroundWorkerShutdown(BackgroundWorkerHandle *handle)
{



























}

/*
 * Instruct the postmaster to terminate a background worker.
 *
 * Note that it's safe to do this without regard to whether the worker is
 * still running, or even if the worker may already have existed and been
 * unregistered.
 */
void
TerminateBackgroundWorker(BackgroundWorkerHandle *handle)
{




















}

/*
 * Look up (and possibly load) a bgworker entry point function.
 *
 * For functions contained in the core code, we use library name "postgres"
 * and consult the InternalBGWorkers array.  External functions are
 * looked up, and loaded if necessary, using load_external_function().
 *
 * The point of this is to pass function names as strings across process
 * boundaries.  We can't pass actual function addresses because of the
 * possibility that the function has been loaded at a different address
 * in a different process.  This is obviously a hazard for functions in
 * loadable libraries, but it can happen even for functions in the core code
 * on platforms using EXEC_BACKEND (e.g., Windows).
 *
 * At some point it might be worthwhile to get rid of InternalBGWorkers[]
 * in favor of applying load_external_function() for core functions too;
 * but that raises portability issues that are not worth addressing now.
 */
static bgworker_main_type
LookupBackgroundWorkerFunction(const char *libraryname, const char *funcname)
{






















}

/*
 * Given a PID, get the bgw_type of the background worker.  Returns NULL if
 * not a valid background worker.
 *
 * The return value is in static memory belonging to this function, so it has
 * to be used before calling this function again.  This is so that the caller
 * doesn't have to worry about the background worker locking protocol.
 */
const char *
GetBackgroundWorkerTypeByPid(pid_t pid)
{


























}