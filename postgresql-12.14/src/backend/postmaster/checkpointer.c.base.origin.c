/*-------------------------------------------------------------------------
 *
 * checkpointer.c
 *
 * The checkpointer is new as of Postgres 9.2.  It handles all checkpoints.
 * Checkpoints are automatically dispatched after a certain amount of time has
 * elapsed since the last one, and it can be signaled to perform requested
 * checkpoints as well.  (The GUC parameter that mandates a checkpoint every
 * so many WAL segments is implemented by having backends signal when they
 * fill WAL segments; the checkpointer itself doesn't watch for the
 * condition.)
 *
 * The checkpointer is started by the postmaster as soon as the startup
 * subprocess finishes, or as soon as recovery begins if we are doing archive
 * recovery.  It remains alive until the postmaster commands it to terminate.
 * Normal termination is by SIGUSR2, which instructs the checkpointer to
 * execute a shutdown checkpoint and then exit(0).  (All backends must be
 * stopped before SIGUSR2 is issued!)  Emergency termination is by SIGQUIT;
 * like any backend, the checkpointer will simply abort and exit on SIGQUIT.
 *
 * If the checkpointer exits unexpectedly, the postmaster treats that the same
 * as a backend crash: shared memory may be corrupted, so remaining backends
 * should be killed by SIGQUIT and then a recovery cycle started.  (Even if
 * shared memory isn't corrupted, we have lost information about which
 * files need to be fsync'd for the next checkpoint, and so a system
 * restart needs to be forced.)
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/checkpointer.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "replication/syncrep.h"
#include "storage/bufmgr.h"
#include "storage/condition_variable.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

/*----------
 * Shared memory area for communication between checkpointer and backends
 *
 * The ckpt counters allow backends to watch for completion of a checkpoint
 * request they send.  Here's how it works:
 *	* At start of a checkpoint, checkpointer reads (and clears) the request
 *	  flags and increments ckpt_started, while holding ckpt_lck.
 *	* On completion of a checkpoint, checkpointer sets ckpt_done to
 *	  equal ckpt_started.
 *	* On failure of a checkpoint, checkpointer increments ckpt_failed
 *	  and sets ckpt_done to equal ckpt_started.
 *
 * The algorithm for backends is:
 *	1. Record current values of ckpt_failed and ckpt_started, and
 *	   set request flags, while holding ckpt_lck.
 *	2. Send signal to request checkpoint.
 *	3. Sleep until ckpt_started changes.  Now you know a checkpoint has
 *	   begun since you started this algorithm (although *not* that it was
 *	   specifically initiated by your signal), and that it is using your
 *flags.
 *	4. Record new value of ckpt_started.
 *	5. Sleep until ckpt_done >= saved value of ckpt_started.  (Use modulo
 *	   arithmetic here in case counters wrap around.)  Now you know a
 *	   checkpoint has started and completed, but not whether it was
 *	   successful.
 *	6. If ckpt_failed is different from the originally saved value,
 *	   assume request failed; otherwise it was definitely successful.
 *
 * ckpt_flags holds the OR of the checkpoint request flags sent by all
 * requesting backends since the last checkpoint start.  The flags are
 * chosen so that OR'ing is the correct way to combine multiple requests.
 *
 * num_backend_writes is used to count the number of buffer writes performed
 * by user backend processes.  This counter should be wide enough that it
 * can't overflow during a single processing cycle.  num_backend_fsync
 * counts the subset of those writes that also had to do their own fsync,
 * because the checkpointer failed to absorb their request.
 *
 * The requests array holds fsync requests sent by backends and not yet
 * absorbed by the checkpointer.
 *
 * Unlike the checkpoint fields, num_backend_writes, num_backend_fsync, and
 * the requests fields are protected by CheckpointerCommLock.
 *----------
 */
typedef struct
{
  SyncRequestType type; /* request type */
  FileTag ftag;         /* file identifier */
} CheckpointerRequest;

typedef struct
{
  pid_t checkpointer_pid; /* PID (0 if not started) */

  slock_t ckpt_lck; /* protects all the ckpt_* fields */

  int ckpt_started; /* advances when checkpoint starts */
  int ckpt_done;    /* advances when checkpoint done */
  int ckpt_failed;  /* advances when checkpoint fails */

  int ckpt_flags; /* checkpoint flags, as defined in xlog.h */

  ConditionVariable start_cv; /* signaled when ckpt_started advances */
  ConditionVariable done_cv;  /* signaled when ckpt_done advances */

  uint32 num_backend_writes; /* counts user backend buffer writes */
  uint32 num_backend_fsync;  /* counts user backend fsync calls */

  int num_requests; /* current # of requests */
  int max_requests; /* allocated array size */
  CheckpointerRequest requests[FLEXIBLE_ARRAY_MEMBER];
} CheckpointerShmemStruct;

static CheckpointerShmemStruct *CheckpointerShmem;

/* interval for calling AbsorbSyncRequests in CheckpointWriteDelay */
#define WRITES_PER_ABSORB 1000

/*
 * GUC parameters
 */
int CheckPointTimeout = 300;
int CheckPointWarning = 30;
double CheckPointCompletionTarget = 0.5;

/*
 * Flags set by interrupt handlers for later service in the main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;

/*
 * Private state
 */
static bool ckpt_active = false;

/* these values are valid when ckpt_active is true: */
static pg_time_t ckpt_start_time;
static XLogRecPtr ckpt_start_recptr;
static double ckpt_cached_elapsed;

static pg_time_t last_checkpoint_time;
static pg_time_t last_xlog_switch_time;

/* Prototypes for private functions */

static void
CheckArchiveTimeout(void);
static bool
IsCheckpointOnSchedule(double progress);
static bool
ImmediateCheckpointRequested(void);
static bool
CompactCheckpointerRequestQueue(void);
static void
UpdateSharedMemoryConfig(void);

/* Signal handlers */

static void chkpt_quickdie(SIGNAL_ARGS);
static void ChkptSigHupHandler(SIGNAL_ARGS);
static void ReqCheckpointHandler(SIGNAL_ARGS);
static void chkpt_sigusr1_handler(SIGNAL_ARGS);
static void ReqShutdownHandler(SIGNAL_ARGS);

/*
 * Main entry point for checkpointer process
 *
 * This is invoked from AuxiliaryProcessMain, which has already created the
 * basic execution environment, but not enabled signals yet.
 */
void
CheckpointerMain(void)
{
















































































































































































































































































































































































}

/*
 * CheckArchiveTimeout -- check for archive_timeout and switch xlog files
 *
 * This will switch to a new WAL file and force an archive file write if
 * meaningful activity is recorded in the current WAL file. This includes most
 * writes, including just a single checkpoint record, but excludes WAL records
 * that were inserted with the XLOG_MARK_UNIMPORTANT flag being set (like
 * snapshots of running transactions).  Such records, depending on
 * configuration, occur on regular intervals and don't contain important
 * information.  This avoids generating archives with a few unimportant
 * records.
 */
static void
CheckArchiveTimeout(void)
{
























































}

/*
 * Returns true if an immediate checkpoint request is pending.  (Note that
 * this does not check the *current* checkpoint's IMMEDIATE flag, but whether
 * there is one pending behind it.)
 */
static bool
ImmediateCheckpointRequested(void)
{











}

/*
 * CheckpointWriteDelay -- control rate of checkpoint
 *
 * This function is called after each page write performed by BufferSync().
 * It is responsible for throttling BufferSync()'s write rate to hit
 * checkpoint_completion_target.
 *
 * The checkpoint request flags should be passed in; currently the only one
 * examined is CHECKPOINT_IMMEDIATE, which disables delays between writes.
 *
 * 'progress' is an estimate of how much of the work has been done, as a
 * fraction between 0.0 meaning none, and 1.0 meaning all done.
 */
void
CheckpointWriteDelay(int flags, double progress)
{


















































}

/*
 * IsCheckpointOnSchedule -- are we on schedule to finish this checkpoint
 *		 (or restartpoint) in time?
 *
 * Compares the current progress against the time/segments elapsed since last
 * checkpoint, and returns true if the progress we've made this far is greater
 * than the elapsed time/segments.
 */
static bool
IsCheckpointOnSchedule(double progress)
{






































































}

/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */

/*
 * chkpt_quickdie() occurs when signalled SIGQUIT by the postmaster.
 *
 * Some backend has bought the farm,
 * so we need to stop what we're doing and exit.
 */
static void
chkpt_quickdie(SIGNAL_ARGS)
{















}

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
ChkptSigHupHandler(SIGNAL_ARGS)
{






}

/* SIGINT: set flag to run a normal checkpoint right away */
static void
ReqCheckpointHandler(SIGNAL_ARGS)
{









}

/* SIGUSR1: used for latch wakeups */
static void
chkpt_sigusr1_handler(SIGNAL_ARGS)
{





}

/* SIGUSR2: set flag to run a shutdown checkpoint and exit */
static void
ReqShutdownHandler(SIGNAL_ARGS)
{






}

/* --------------------------------
 *		communication with backends
 * --------------------------------
 */

/*
 * CheckpointerShmemSize
 *		Compute space needed for checkpointer-related shared memory
 */
Size
CheckpointerShmemSize(void)
{










}

/*
 * CheckpointerShmemInit
 *		Allocate and initialize checkpointer-related shared memory
 */
void
CheckpointerShmemInit(void)
{


















}

/*
 * RequestCheckpoint
 *		Called in backend processes to request a checkpoint
 *
 * flags is a bitwise OR of the following:
 *	CHECKPOINT_IS_SHUTDOWN: checkpoint is for database shutdown.
 *	CHECKPOINT_END_OF_RECOVERY: checkpoint is for end of WAL recovery.
 *	CHECKPOINT_IMMEDIATE: finish the checkpoint ASAP,
 *		ignoring checkpoint_completion_target parameter.
 *	CHECKPOINT_FORCE: force a checkpoint even if no XLOG activity has
 *occurred since the last one (implied by CHECKPOINT_IS_SHUTDOWN or
 *		CHECKPOINT_END_OF_RECOVERY).
 *	CHECKPOINT_WAIT: wait for completion before returning (otherwise,
 *		just signal checkpointer to do it, and return).
 *	CHECKPOINT_CAUSE_XLOG: checkpoint is requested due to xlog filling.
 *		(This affects logging, and in particular enables
 *CheckPointWarning.)
 */
void
RequestCheckpoint(int flags)
{


































































































































}

/*
 * ForwardSyncRequest
 *		Forward a file-fsync request from a backend to the checkpointer
 *
 * Whenever a backend is compelled to write directly to a relation
 * (which should be seldom, if the background writer is getting its job done),
 * the backend calls this routine to pass over knowledge that the relation
 * is dirty and must be fsync'd before next checkpoint.  We also use this
 * opportunity to count such writes for statistical purposes.
 *
 * To avoid holding the lock for longer than necessary, we normally write
 * to the requests[] queue without checking for duplicates.  The checkpointer
 * will have to eliminate dups internally anyway.  However, if we discover
 * that the queue is full, we make a pass over the entire queue to compact
 * it.  This is somewhat expensive, but the alternative is for the backend
 * to perform its own fsync, which is far more expensive in practice.  It
 * is theoretically possible a backend fsync might still be necessary, if
 * the queue is full and contains no duplicate entries.  In that case, we
 * let the backend know by returning false.
 */
bool
ForwardSyncRequest(const FileTag *ftag, SyncRequestType type)
{

























































}

/*
 * CompactCheckpointerRequestQueue
 *		Remove duplicates from the request queue to avoid backend
 *fsyncs. Returns "true" if any entries were removed.
 *
 * Although a full fsync request queue is not common, it can lead to severe
 * performance problems when it does happen.  So far, this situation has
 * only been observed to occur when the system is under heavy write load,
 * and especially during the "sync" phase of a checkpoint.  Without this
 * logic, each backend begins doing an fsync for every block written, which
 * gets very expensive and can slow down the whole system.
 *
 * Trying to do this every time the queue is full could lose if there
 * aren't any removable entries.  But that should be vanishingly rare in
 * practice: there's one queue entry per shared buffer.
 */
static bool
CompactCheckpointerRequestQueue(void)
{

























































































}

/*
 * AbsorbSyncRequests
 *		Retrieve queued sync requests and pass them to sync mechanism.
 *
 * This is exported because it must be called during CreateCheckPoint;
 * we have to be sure we have accepted all pending requests just before
 * we start fsync'ing.  Since CreateCheckPoint sometimes runs in
 * non-checkpointer processes, do nothing if not checkpointer.
 */
void
AbsorbSyncRequests(void)
{




















































}

/*
 * Update any shared memory configurations based on config parameters
 */
static void
UpdateSharedMemoryConfig(void)
{










}

/*
 * FirstCallSinceLastCheckpoint allows a process to take an action once
 * per checkpoint cycle by asynchronously checking for checkpoint completion.
 */
bool
FirstCallSinceLastCheckpoint(void)
{
















}