/*-------------------------------------------------------------------------
 *
 * procsignal.c
 *	  Routines for interprocess signalling
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/ipc/procsignal.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/parallel.h"
#include "commands/async.h"
#include "miscadmin.h"
#include "replication/walsender.h"
#include "storage/latch.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/sinval.h"
#include "tcop/tcopprot.h"

/*
 * The SIGUSR1 signal is multiplexed to support signalling multiple event
 * types. The specific reason is communicated via flags in shared memory.
 * We keep a boolean flag for each possible "reason", so that different
 * reasons can be signaled to a process concurrently.  (However, if the same
 * reason is signaled more than once nearly simultaneously, the process may
 * observe it only once.)
 *
 * Each process that wants to receive signals registers its process ID
 * in the ProcSignalSlots array. The array is indexed by backend ID to make
 * slot allocation simple, and to avoid having to search the array when you
 * know the backend ID of the process you're signalling.  (We do support
 * signalling without backend ID, but it's a bit less efficient.)
 *
 * The flags are actually declared as "volatile sig_atomic_t" for maximum
 * portability.  This should ensure that loads and stores of the flag
 * values are atomic, allowing us to dispense with any explicit locking.
 */
typedef struct
{
  pid_t pss_pid;
  sig_atomic_t pss_signalFlags[NUM_PROCSIGNALS];
} ProcSignalSlot;

/*
 * We reserve a slot for each possible BackendId, plus one for each
 * possible auxiliary process type.  (This scheme assumes there is not
 * more than one of any auxiliary process type at a time.)
 */
#define NumProcSignalSlots (MaxBackends + NUM_AUXPROCTYPES)

static ProcSignalSlot *ProcSignalSlots = NULL;
static volatile ProcSignalSlot *MyProcSignalSlot = NULL;

static bool
CheckProcSignal(ProcSignalReason reason);
static void
CleanupProcSignalState(int status, Datum arg);

/*
 * ProcSignalShmemSize
 *		Compute space needed for procsignal's shared memory
 */
Size
ProcSignalShmemSize(void)
{

}

/*
 * ProcSignalShmemInit
 *		Allocate and initialize procsignal's shared memory
 */
void
ProcSignalShmemInit(void)
{










}

/*
 * ProcSignalInit
 *		Register the current process in the procsignal array
 *
 * The passed index should be my BackendId if the process has one,
 * or MaxBackends + aux process type if not.
 */
void
ProcSignalInit(int pss_idx)
{























}

/*
 * CleanupProcSignalState
 *		Remove current process from ProcSignalSlots
 *
 * This function is called via on_shmem_exit() during backend shutdown.
 */
static void
CleanupProcSignalState(int status, Datum arg)
{

























}

/*
 * SendProcSignal
 *		Send a signal to a Postgres process
 *
 * Providing backendId is optional, but it will speed up the operation.
 *
 * On success (a signal was sent), zero is returned.
 * On error, -1 is returned, and errno is set (typically to ESRCH or EPERM).
 *
 * Not to be confused with ProcSendSignal
 */
int
SendProcSignal(pid_t pid, ProcSignalReason reason, BackendId backendId)
{


















































}

/*
 * CheckProcSignal - check to see if a particular reason has been
 * signaled, and clear the signal flag.  Should be called after receiving
 * SIGUSR1.
 */
static bool
CheckProcSignal(ProcSignalReason reason)
{













}

/*
 * procsignal_sigusr1_handler - handle SIGUSR1 signal.
 */
void
procsignal_sigusr1_handler(SIGNAL_ARGS)
{

























































}