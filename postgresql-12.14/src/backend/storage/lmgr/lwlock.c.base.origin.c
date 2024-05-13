/*-------------------------------------------------------------------------
 *
 * lwlock.c
 *	  Lightweight lock manager
 *
 * Lightweight locks are intended primarily to provide mutual exclusion of
 * access to shared-memory data structures.  Therefore, they offer both
 * exclusive and shared lock modes (to support read/write and read-only
 * access to a shared object).  There are few other frammishes.  User-level
 * locking should be done with the full lock manager --- which depends on
 * LWLocks to protect its shared state.
 *
 * In addition to exclusive and shared modes, lightweight locks can be used to
 * wait until a variable changes value.  The variable is initially not set
 * when the lock is acquired with LWLockAcquire, i.e. it remains set to the
 * value it was set to when the lock was released last, and can be updated
 * without releasing the lock by calling LWLockUpdateVar.  LWLockWaitForVar
 * waits for the variable to be updated, or until the lock is free.  When
 * releasing the lock with LWLockReleaseClearVar() the value can be set to an
 * appropriate value for a free lock.  The meaning of the variable is up to
 * the caller, the lightweight lock code just assigns and compares it.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/lmgr/lwlock.c
 *
 * NOTES:
 *
 * This used to be a pretty straight forward reader-writer lock
 * implementation, in which the internal state was protected by a
 * spinlock. Unfortunately the overhead of taking the spinlock proved to be
 * too high for workloads/locks that were taken in shared mode very
 * frequently. Often we were spinning in the (obviously exclusive) spinlock,
 * while trying to acquire a shared lock that was actually free.
 *
 * Thus a new implementation was devised that provides wait-free shared lock
 * acquisition for locks that aren't exclusively locked.
 *
 * The basic idea is to have a single atomic variable 'lockcount' instead of
 * the formerly separate shared and exclusive counters and to use atomic
 * operations to acquire the lock. That's fairly easy to do for plain
 * rw-spinlocks, but a lot harder for something like LWLocks that want to wait
 * in the OS.
 *
 * For lock acquisition we use an atomic compare-and-exchange on the lockcount
 * variable. For exclusive lock we swap in a sentinel value
 * (LW_VAL_EXCLUSIVE), for shared locks we count the number of holders.
 *
 * To release the lock we use an atomic decrement to release the lock. If the
 * new value is zero (we get that atomically), we know we can/have to release
 * waiters.
 *
 * Obviously it is important that the sentinel value for exclusive locks
 * doesn't conflict with the maximum number of possible share lockers -
 * luckily MAX_BACKENDS makes that easily possible.
 *
 *
 * The attentive reader might have noticed that naively doing the above has a
 * glaring race condition: We try to lock using the atomic operations and
 * notice that we have to wait. Unfortunately by the time we have finished
 * queuing, the former locker very well might have already finished it's
 * work. That's problematic because we're now stuck waiting inside the OS.

 * To mitigate those races we use a two phased attempt at locking:
 *	 Phase 1: Try to do it atomically, if we succeed, nice
 *	 Phase 2: Add ourselves to the waitqueue of the lock
 *	 Phase 3: Try to grab the lock again, if we succeed, remove ourselves
 from *			  the queue *	 Phase 4: Sleep till wake-up, goto Phase
 1
 *
 * This protects us against the problem from above as nobody can release too
 *	  quick, before we're queued, since after Phase 2 we're already queued.
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "pg_trace.h"
#include "postmaster/postmaster.h"
#include "replication/slot.h"
#include "storage/ipc.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/proclist.h"
#include "storage/spin.h"
#include "utils/memutils.h"

#ifdef LWLOCK_STATS
#include "utils/hsearch.h"
#endif

/* We use the ShmemLock spinlock to protect LWLockCounter */
extern slock_t *ShmemLock;

#define LW_FLAG_HAS_WAITERS ((uint32)1 << 30)
#define LW_FLAG_RELEASE_OK ((uint32)1 << 29)
#define LW_FLAG_LOCKED ((uint32)1 << 28)

#define LW_VAL_EXCLUSIVE ((uint32)1 << 24)
#define LW_VAL_SHARED 1

#define LW_LOCK_MASK ((uint32)((1 << 25) - 1))
/* Must be greater than MAX_BACKENDS - which is 2^23-1, so we're fine. */
#define LW_SHARED_MASK ((uint32)((1 << 24) - 1))

/*
 * This is indexed by tranche ID and stores the names of all tranches known
 * to the current backend.
 */
static const char **LWLockTrancheArray = NULL;
static int LWLockTranchesAllocated = 0;

#define T_NAME(lock) (LWLockTrancheArray[(lock)->tranche])

/*
 * This points to the main array of LWLocks in shared memory.  Backends inherit
 * the pointer by fork from the postmaster (except in the EXEC_BACKEND case,
 * where we have special measures to pass it down).
 */
LWLockPadded *MainLWLockArray = NULL;

/*
 * We use this structure to keep track of locked LWLocks for release
 * during error recovery.  Normally, only a few will be held at once, but
 * occasionally the number can be much higher; for example, the pg_buffercache
 * extension locks all buffer partitions simultaneously.
 */
#define MAX_SIMUL_LWLOCKS 200

/* struct representing the LWLocks we're holding */
typedef struct LWLockHandle
{
  LWLock *lock;
  LWLockMode mode;
} LWLockHandle;

static int num_held_lwlocks = 0;
static LWLockHandle held_lwlocks[MAX_SIMUL_LWLOCKS];

/* struct representing the LWLock tranche request for named tranche */
typedef struct NamedLWLockTrancheRequest
{
  char tranche_name[NAMEDATALEN];
  int num_lwlocks;
} NamedLWLockTrancheRequest;

NamedLWLockTrancheRequest *NamedLWLockTrancheRequestArray = NULL;
static int NamedLWLockTrancheRequestsAllocated = 0;
int NamedLWLockTrancheRequests = 0;

NamedLWLockTranche *NamedLWLockTrancheArray = NULL;

static bool lock_named_request_allowed = true;

static void
InitializeLWLocks(void);
static void
RegisterLWLockTranches(void);

static inline void
LWLockReportWaitStart(LWLock *lock);
static inline void
LWLockReportWaitEnd(void);

#ifdef LWLOCK_STATS
typedef struct lwlock_stats_key
{
  int tranche;
  void *instance;
} lwlock_stats_key;

typedef struct lwlock_stats
{
  lwlock_stats_key key;
  int sh_acquire_count;
  int ex_acquire_count;
  int block_count;
  int dequeue_self_count;
  int spin_delay_count;
} lwlock_stats;

static HTAB *lwlock_stats_htab;
static lwlock_stats lwlock_stats_dummy;
#endif

#ifdef LOCK_DEBUG
bool Trace_lwlocks = false;

inline static void
PRINT_LWDEBUG(const char *where, LWLock *lock, LWLockMode mode)
{
  /* hide statement & context here, otherwise the log is just too verbose */
  if (Trace_lwlocks)
  {
    uint32 state = pg_atomic_read_u32(&lock->state);

    ereport(LOG, (errhidestmt(true), errhidecontext(true), errmsg_internal("%d: %s(%s %p): excl %u shared %u haswaiters %u waiters %u rOK %d", MyProcPid, where, T_NAME(lock), lock, (state & LW_VAL_EXCLUSIVE) != 0, state & LW_SHARED_MASK, (state & LW_FLAG_HAS_WAITERS) != 0, pg_atomic_read_u32(&lock->nwaiters), (state & LW_FLAG_RELEASE_OK) != 0)));
  }
}

inline static void
LOG_LWDEBUG(const char *where, LWLock *lock, const char *msg)
{
  /* hide statement & context here, otherwise the log is just too verbose */
  if (Trace_lwlocks)
  {
    ereport(LOG, (errhidestmt(true), errhidecontext(true), errmsg_internal("%s(%s %p): %s", where, T_NAME(lock), lock, msg)));
  }
}

#else /* not LOCK_DEBUG */
#define PRINT_LWDEBUG(a, b, c) ((void)0)
#define LOG_LWDEBUG(a, b, c) ((void)0)
#endif /* LOCK_DEBUG */

#ifdef LWLOCK_STATS

static void
init_lwlock_stats(void);
static void
print_lwlock_stats(int code, Datum arg);
static lwlock_stats *
get_lwlock_stats_entry(LWLock *lockid);

static void
init_lwlock_stats(void)
{
  HASHCTL ctl;
  static MemoryContext lwlock_stats_cxt = NULL;
  static bool exit_registered = false;

  if (lwlock_stats_cxt != NULL)
  {
    MemoryContextDelete(lwlock_stats_cxt);
  }

  /*
   * The LWLock stats will be updated within a critical section, which
   * requires allocating new hash entries. Allocations within a critical
   * section are normally not allowed because running out of memory would
   * lead to a PANIC, but LWLOCK_STATS is debugging code that's not normally
   * turned on in production, so that's an acceptable risk. The hash entries
   * are small, so the risk of running out of memory is minimal in practice.
   */
  lwlock_stats_cxt = AllocSetContextCreate(TopMemoryContext, "LWLock stats", ALLOCSET_DEFAULT_SIZES);
  MemoryContextAllowInCriticalSection(lwlock_stats_cxt, true);

  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(lwlock_stats_key);
  ctl.entrysize = sizeof(lwlock_stats);
  ctl.hcxt = lwlock_stats_cxt;
  lwlock_stats_htab = hash_create("lwlock stats", 16384, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
  if (!exit_registered)
  {
    on_shmem_exit(print_lwlock_stats, 0);
    exit_registered = true;
  }
}

static void
print_lwlock_stats(int code, Datum arg)
{
  HASH_SEQ_STATUS scan;
  lwlock_stats *lwstats;

  hash_seq_init(&scan, lwlock_stats_htab);

  /* Grab an LWLock to keep different backends from mixing reports */
  LWLockAcquire(&MainLWLockArray[0].lock, LW_EXCLUSIVE);

  while ((lwstats = (lwlock_stats *)hash_seq_search(&scan)) != NULL)
  {
    fprintf(stderr, "PID %d lwlock %s %p: shacq %u exacq %u blk %u spindelay %u dequeue self %u\n", MyProcPid, LWLockTrancheArray[lwstats->key.tranche], lwstats->key.instance, lwstats->sh_acquire_count, lwstats->ex_acquire_count, lwstats->block_count, lwstats->spin_delay_count, lwstats->dequeue_self_count);
  }

  LWLockRelease(&MainLWLockArray[0].lock);
}

static lwlock_stats *
get_lwlock_stats_entry(LWLock *lock)
{
  lwlock_stats_key key;
  lwlock_stats *lwstats;
  bool found;

  /*
   * During shared memory initialization, the hash table doesn't exist yet.
   * Stats of that phase aren't very interesting, so just collect operations
   * on all locks in a single dummy entry.
   */
  if (lwlock_stats_htab == NULL)
  {
    return &lwlock_stats_dummy;
  }

  /* Fetch or create the entry. */
  MemSet(&key, 0, sizeof(key));
  key.tranche = lock->tranche;
  key.instance = lock;
  lwstats = hash_search(lwlock_stats_htab, &key, HASH_ENTER, &found);
  if (!found)
  {
    lwstats->sh_acquire_count = 0;
    lwstats->ex_acquire_count = 0;
    lwstats->block_count = 0;
    lwstats->dequeue_self_count = 0;
    lwstats->spin_delay_count = 0;
  }
  return lwstats;
}
#endif /* LWLOCK_STATS */

/*
 * Compute number of LWLocks required by named tranches.  These will be
 * allocated in the main array.
 */
static int
NumLWLocksByNamedTranches(void)
{









}

/*
 * Compute shmem space needed for LWLocks and named tranches.
 */
Size
LWLockShmemSize(void)
{

























}

/*
 * Allocate shmem space for the main LWLock array and all tranches and
 * initialize it.  We also register all the LWLock tranches here.
 */
void
CreateLWLocks(void)
{


































}

/*
 * Initialize LWLocks that are fixed and those belonging to named tranches.
 */
static void
InitializeLWLocks(void)
{
































































}

/*
 * Register named tranches and tranches for fixed LWLocks.
 */
static void
RegisterLWLockTranches(void)
{
































}

/*
 * InitLWLockAccess - initialize backend-local state needed to hold LWLocks
 */
void
InitLWLockAccess(void)
{



}

/*
 * GetNamedLWLockTranche - returns the base address of LWLock from the
 *		specified tranche.
 *
 * Caller needs to retrieve the requested number of LWLocks starting from
 * the base lock address returned by this API.  This can be used for
 * tranches that are requested by using RequestNamedLWLockTranche() API.
 */
LWLockPadded *
GetNamedLWLockTranche(const char *tranche_name)
{


























}

/*
 * Allocate a new tranche ID.
 */
int
LWLockNewTrancheId(void)
{









}

/*
 * Register a tranche ID in the lookup table for the current process.  This
 * routine will save a pointer to the tranche name passed as an argument,
 * so the name should be allocated in a backend-lifetime context
 * (TopMemoryContext, static variable, or similar).
 */
void
LWLockRegisterTranche(int tranche_id, const char *tranche_name)
{





















}

/*
 * RequestNamedLWLockTranche
 *		Request that extra LWLocks be allocated during postmaster
 *		startup.
 *
 * This is only useful for extensions if called from the _PG_init hook
 * of a library that is loaded into the postmaster via
 * shared_preload_libraries.  Once shared memory has been allocated, calls
 * will be ignored.  (We could raise an error, but it seems better to make
 * it a no-op, so that libraries containing such calls can be reloaded if
 * needed.)
 */
void
RequestNamedLWLockTranche(const char *tranche_name, int num_lwlocks)
{































}

/*
 * LWLockInitialize - initialize a new lwlock; it's initially unlocked
 */
void
LWLockInitialize(LWLock *lock, int tranche_id)
{






}

/*
 * Report start of wait event for light-weight locks.
 *
 * This function will be used by all the light-weight lock calls which
 * needs to wait to acquire the lock.  This function distinguishes wait
 * event based on tranche and lock id.
 */
static inline void
LWLockReportWaitStart(LWLock *lock)
{

}

/*
 * Report end of wait event for light-weight locks.
 */
static inline void
LWLockReportWaitEnd(void)
{

}

/*
 * Return an identifier for an LWLock based on the wait class and event.
 */
const char *
GetLWLockIdentifier(uint32 classId, uint16 eventId)
{













}

/*
 * Internal function that tries to atomically acquire the lwlock in the passed
 * in mode.
 *
 * This function will not block waiting for a lock to become free - that's the
 * callers job.
 *
 * Returns true if the lock isn't free and we need to wait.
 */
static bool
LWLockAttemptLock(LWLock *lock, LWLockMode mode)
{

































































}

/*
 * Lock the LWLock's wait list against concurrent activity.
 *
 * NB: even though the wait list is locked, non-conflicting lock operations
 * may still happen concurrently.
 *
 * Time spent holding mutex should be short!
 */
static void
LWLockWaitListLock(LWLock *lock)
{











































}

/*
 * Unlock the LWLock's wait list.
 *
 * Note that it can be more efficient to manipulate flags and release the
 * locks in a single atomic operation.
 */
static void
LWLockWaitListUnlock(LWLock *lock)
{





}

/*
 * Wakeup all the lockers that currently have a chance to acquire the lock.
 */
static void
LWLockWakeup(LWLock *lock)
{












































































































}

/*
 * Add ourselves to the end of the queue.
 *
 * NB: Mode can be LW_WAIT_UNTIL_FREE here!
 */
static void
LWLockQueueSelf(LWLock *lock, LWLockMode mode)
{







































}

/*
 * Remove ourselves from the waitlist.
 *
 * This is used if we queued ourselves because we thought we needed to sleep
 * but, after further checking, we discovered that we don't actually need to
 * do so.
 */
static void
LWLockDequeueSelf(LWLock *lock)
{























































































}

/*
 * LWLockAcquire - acquire a lightweight lock in the specified mode
 *
 * If the lock is not available, sleep until it is.  Returns true if the lock
 * was available immediately, false if we had to sleep.
 *
 * Side effect: cancel/die interrupts are held off until lock release.
 */
bool
LWLockAcquire(LWLock *lock, LWLockMode mode)
{






































































































































































}

/*
 * LWLockConditionalAcquire - acquire a lightweight lock in the specified mode
 *
 * If the lock is not available, return false with no side-effects.
 *
 * If successful, cancel/die interrupts are held off until lock release.
 */
bool
LWLockConditionalAcquire(LWLock *lock, LWLockMode mode)
{






































}

/*
 * LWLockAcquireOrWait - Acquire lock, or wait until it's free
 *
 * The semantics of this function are a bit funky.  If the lock is currently
 * free, it is acquired in the given mode, and the function returns true.  If
 * the lock isn't immediately free, the function waits until it is released
 * and returns false, but does not acquire the lock.
 *
 * This is currently used for WALWriteLock: when a backend flushes the WAL,
 * holding WALWriteLock, it can flush the commit records of many other
 * backends as a side-effect.  Those other backends need to wait until the
 * flush finishes, but don't need to acquire the lock anymore.  They can just
 * wake up, observe that their records have already been flushed, and return.
 */
bool
LWLockAcquireOrWait(LWLock *lock, LWLockMode mode)
{



















































































































}

/*
 * Does the lwlock in its current state need to wait for the variable value to
 * change?
 *
 * If we don't need to wait, and it's because the value of the variable has
 * changed, store the current value in newval.
 *
 * *result is set to true if the lock was free, and false otherwise.
 */
static bool
LWLockConflictsWithVar(LWLock *lock, uint64 *valptr, uint64 oldval, uint64 *newval, bool *result)
{








































}

/*
 * LWLockWaitForVar - Wait until lock is free, or a variable is updated.
 *
 * If the lock is held and *valptr equals oldval, waits until the lock is
 * either freed, or the lock holder updates *valptr by calling
 * LWLockUpdateVar.  If the lock is free on exit (immediately or after
 * waiting), returns true.  If the lock is still held, but *valptr no longer
 * matches oldval, returns false and sets *newval to the current value in
 * *valptr.
 *
 * Note: this function ignores shared lock holders; if the lock is held
 * in shared mode, returns 'true'.
 */
bool
LWLockWaitForVar(LWLock *lock, uint64 *valptr, uint64 oldval, uint64 *newval)
{



























































































































}

/*
 * LWLockUpdateVar - Update a variable and wake up waiters atomically
 *
 * Sets *valptr to 'val', and wakes up all processes waiting for us with
 * LWLockWaitForVar().  Setting the value and waking up the processes happen
 * atomically so that any process calling LWLockWaitForVar() on the same lock
 * is guaranteed to see the new value, and act accordingly.
 *
 * The caller must be holding the lock in exclusive mode.
 */
void
LWLockUpdateVar(LWLock *lock, uint64 *valptr, uint64 val)
{















































}

/*
 * LWLockRelease - release a previously acquired lock
 */
void
LWLockRelease(LWLock *lock)
{














































































}

/*
 * LWLockReleaseClearVar - release a previously acquired lock, reset variable
 */
void
LWLockReleaseClearVar(LWLock *lock, uint64 *valptr, uint64 val)
{











}

/*
 * LWLockReleaseAll - release all currently-held locks
 *
 * Used to clean up after ereport(ERROR). An important difference between this
 * function and retail LWLockRelease calls is that InterruptHoldoffCount is
 * unchanged by this operation.  This is necessary since InterruptHoldoffCount
 * has been set to an appropriate level earlier in error recovery. We could
 * decrement it below zero if we allow it to drop for each released lock!
 */
void
LWLockReleaseAll(void)
{






}

/*
 * LWLockHeldByMe - test whether my process holds a lock in any mode
 *
 * This is meant as debug support only.
 */
bool
LWLockHeldByMe(LWLock *l)
{










}

/*
 * LWLockHeldByMe - test whether my process holds any of an array of locks
 *
 * This is meant as debug support only.
 */
bool
LWLockAnyHeldByMe(LWLock *l, int nlocks, size_t stride)
{
















}

/*
 * LWLockHeldByMeInMode - test whether my process holds a lock in given mode
 *
 * This is meant as debug support only.
 */
bool
LWLockHeldByMeInMode(LWLock *l, LWLockMode mode)
{










}