/*-------------------------------------------------------------------------
 *
 * deadlock.c
 *	  POSTGRES deadlock detection code
 *
 * See src/backend/storage/lmgr/README for a description of the deadlock
 * detection and resolution algorithms.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/lmgr/deadlock.c
 *
 *	Interface:
 *
 *	DeadLockCheck()
 *	DeadLockReport()
 *	RememberSimpleDeadLock()
 *	InitDeadLockChecking()
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "utils/memutils.h"

/*
 * One edge in the waits-for graph.
 *
 * waiter and blocker may or may not be members of a lock group, but if either
 * is, it will be the leader rather than any other member of the lock group.
 * The group leaders act as representatives of the whole group even though
 * those particular processes need not be waiting at all.  There will be at
 * least one member of the waiter's lock group on the wait queue for the given
 * lock, maybe more.
 */
typedef struct
{
  PGPROC *waiter;  /* the leader of the waiting lock group */
  PGPROC *blocker; /* the leader of the group it is waiting for */
  LOCK *lock;      /* the lock being waited for */
  int pred;        /* workspace for TopoSort */
  int link;        /* workspace for TopoSort */
} EDGE;

/* One potential reordering of a lock's wait queue */
typedef struct
{
  LOCK *lock;     /* the lock whose wait queue is described */
  PGPROC **procs; /* array of PGPROC *'s in new wait order */
  int nProcs;
} WAIT_ORDER;

/*
 * Information saved about each edge in a detected deadlock cycle.  This
 * is used to print a diagnostic message upon failure.
 *
 * Note: because we want to examine this info after releasing the lock
 * manager's partition locks, we can't just store LOCK and PGPROC pointers;
 * we must extract out all the info we want to be able to print.
 */
typedef struct
{
  LOCKTAG locktag;   /* ID of awaited lock object */
  LOCKMODE lockmode; /* type of lock we're waiting for */
  int pid;           /* PID of blocked backend */
} DEADLOCK_INFO;

static bool
DeadLockCheckRecurse(PGPROC *proc);
static int
TestConfiguration(PGPROC *startProc);
static bool
FindLockCycle(PGPROC *checkProc, EDGE *softEdges, int *nSoftEdges);
static bool
FindLockCycleRecurse(PGPROC *checkProc, int depth, EDGE *softEdges, int *nSoftEdges);
static bool
FindLockCycleRecurseMember(PGPROC *checkProc, PGPROC *checkProcLeader, int depth, EDGE *softEdges, int *nSoftEdges);
static bool
ExpandConstraints(EDGE *constraints, int nConstraints);
static bool
TopoSort(LOCK *lock, EDGE *constraints, int nConstraints, PGPROC **ordering);

#ifdef DEBUG_DEADLOCK
static void
PrintLockQueue(LOCK *lock, const char *info);
#endif

/*
 * Working space for the deadlock detector
 */

/* Workspace for FindLockCycle */
static PGPROC **visitedProcs; /* Array of visited procs */
static int nVisitedProcs;

/* Workspace for TopoSort */
static PGPROC **topoProcs;     /* Array of not-yet-output procs */
static int *beforeConstraints; /* Counts of remaining before-constraints */
static int *afterConstraints;  /* List head for after-constraints */

/* Output area for ExpandConstraints */
static WAIT_ORDER *waitOrders; /* Array of proposed queue rearrangements */
static int nWaitOrders;
static PGPROC **waitOrderProcs; /* Space for waitOrders queue contents */

/* Current list of constraints being considered */
static EDGE *curConstraints;
static int nCurConstraints;
static int maxCurConstraints;

/* Storage space for results from FindLockCycle */
static EDGE *possibleConstraints;
static int nPossibleConstraints;
static int maxPossibleConstraints;
static DEADLOCK_INFO *deadlockDetails;
static int nDeadlockDetails;

/* PGPROC pointer of any blocking autovacuum worker found */
static PGPROC *blocking_autovacuum_proc = NULL;

/*
 * InitDeadLockChecking -- initialize deadlock checker during backend startup
 *
 * This does per-backend initialization of the deadlock checker; primarily,
 * allocation of working memory for DeadLockCheck.  We do this per-backend
 * since there's no percentage in making the kernel do copy-on-write
 * inheritance of workspace from the postmaster.  We want to allocate the
 * space at startup because (a) the deadlock checker might be invoked when
 * there's no free memory left, and (b) the checker is normally run inside a
 * signal handler, which is a very dangerous place to invoke palloc from.
 */
void
InitDeadLockChecking(void)
{




















































}

/*
 * DeadLockCheck -- Checks for deadlocks for a given process
 *
 * This code looks for deadlocks involving the given process.  If any
 * are found, it tries to rearrange lock wait queues to resolve the
 * deadlock.  If resolution is impossible, return DS_HARD_DEADLOCK ---
 * the caller is then expected to abort the given proc's transaction.
 *
 * Caller must already have locked all partitions of the lock tables.
 *
 * On failure, deadlock details are recorded in deadlockDetails[] for
 * subsequent printing by DeadLockReport().  That activity is separate
 * because (a) we don't want to do it while holding all those LWLocks,
 * and (b) we are typically invoked inside a signal handler.
 */
DeadLockState
DeadLockCheck(PGPROC *proc)
{









































































}

/*
 * Return the PGPROC of the autovacuum that's blocking a process.
 *
 * We reset the saved pointer as soon as we pass it back.
 */
PGPROC *
GetBlockingAutoVacuumPgproc(void)
{






}

/*
 * DeadLockCheckRecurse -- recursively search for valid orderings
 *
 * curConstraints[] holds the current set of constraints being considered
 * by an outer level of recursion.  Add to this each possible solution
 * constraint for any cycle detected at this level.
 *
 * Returns true if no solution exists.  Returns false if a deadlock-free
 * state is attainable, in which case waitOrders[] shows the required
 * rearrangements of lock wait queues (if any).
 */
static bool
DeadLockCheckRecurse(PGPROC *proc)
{























































}

/*--------------------
 * Test a configuration (current set of constraints) for validity.
 *
 * Returns:
 *		0: the configuration is good (no deadlocks)
 *	   -1: the configuration has a hard deadlock or is not self-consistent
 *		>0: the configuration has one or more soft deadlocks
 *
 * In the soft-deadlock case, one of the soft cycles is chosen arbitrarily
 * and a list of its soft edges is returned beginning at
 * possibleConstraints+nPossibleConstraints.  The return value is the
 * number of soft edges.
 *--------------------
 */
static int
TestConfiguration(PGPROC *startProc)
{























































}

/*
 * FindLockCycle -- basic check for deadlock cycles
 *
 * Scan outward from the given proc to see if there is a cycle in the
 * waits-for graph that includes this proc.  Return true if a cycle
 * is found, else false.  If a cycle is found, we return a list of
 * the "soft edges", if any, included in the cycle.  These edges could
 * potentially be eliminated by rearranging wait queues.  We also fill
 * deadlockDetails[] with information about the detected cycle; this info
 * is not used by the deadlock algorithm itself, only to print a useful
 * message after failing.
 *
 * Since we need to be able to check hypothetical configurations that would
 * exist after wait queue rearrangement, the routine pays attention to the
 * table of hypothetical queue orders in waitOrders[].  These orders will
 * be believed in preference to the actual ordering seen in the locktable.
 */
static bool
FindLockCycle(PGPROC *checkProc, EDGE *softEdges, /* output argument */
    int *nSoftEdges)                              /* output argument */
{




}

static bool
FindLockCycleRecurse(PGPROC *checkProc, int depth, EDGE *softEdges, /* output argument */
    int *nSoftEdges)                                                /* output argument */
{








































































}

static bool
FindLockCycleRecurseMember(PGPROC *checkProc, PGPROC *checkProcLeader, int depth, EDGE *softEdges, /* output argument */
    int *nSoftEdges)                                                                               /* output argument */
{
















































































































































































































































}

/*
 * ExpandConstraints -- expand a list of constraints into a set of
 *		specific new orderings for affected wait queues
 *
 * Input is a list of soft edges to be reversed.  The output is a list
 * of nWaitOrders WAIT_ORDER structs in waitOrders[], with PGPROC array
 * workspace in waitOrderProcs[].
 *
 * Returns true if able to build an ordering that satisfies all the
 * constraints, false if not (there are contradictory constraints).
 */
static bool
ExpandConstraints(EDGE *constraints, int nConstraints)
{












































}

/*
 * TopoSort -- topological sort of a wait queue
 *
 * Generate a re-ordering of a lock's wait queue that satisfies given
 * constraints about certain procs preceding others.  (Each such constraint
 * is a fact of a partial ordering.)  Minimize rearrangement of the queue
 * not needed to achieve the partial ordering.
 *
 * This is a lot simpler and slower than, for example, the topological sort
 * algorithm shown in Knuth's Volume 1.  However, Knuth's method doesn't
 * try to minimize the damage to the existing order.  In practice we are
 * not likely to be working with more than a few constraints, so the apparent
 * slowness of the algorithm won't really matter.
 *
 * The initial queue ordering is taken directly from the lock's wait queue.
 * The output is an array of PGPROC pointers, of length equal to the lock's
 * wait queue length (the caller is responsible for providing this space).
 * The partial order is specified by an array of EDGE structs.  Each EDGE
 * is one that we need to reverse, therefore the "waiter" must appear before
 * the "blocker" in the output array.  The EDGE array may well contain
 * edges associated with other locks; these should be ignored.
 *
 * Returns true if able to build an ordering that satisfies all the
 * constraints, false if not (there are contradictory constraints).
 */
static bool
TopoSort(LOCK *lock, EDGE *constraints, int nConstraints, PGPROC **ordering) /* output argument */
{
































































































































































































}

#ifdef DEBUG_DEADLOCK
static void
PrintLockQueue(LOCK *lock, const char *info)
{
  PROC_QUEUE *waitQueue = &(lock->waitProcs);
  int queue_size = waitQueue->size;
  PGPROC *proc;
  int i;

  printf("%s lock %p queue ", info, lock);
  proc = (PGPROC *)waitQueue->links.next;
  for (i = 0; i < queue_size; i++)
  {
    printf(" %d", proc->pid);
    proc = (PGPROC *)proc->links.next;
  }
  printf("\n");
  fflush(stdout);
}
#endif

/*
 * Report a detected deadlock, with available details.
 */
void
DeadLockReport(void)
{






















































}

/*
 * RememberSimpleDeadLock: set up info for DeadLockReport when ProcSleep
 * detects a trivial (two-way) deadlock.  proc1 wants to block for lockmode
 * on lock, but proc2 is already waiting and would be blocked by proc1.
 */
void
RememberSimpleDeadLock(PGPROC *proc1, LOCKMODE lockmode, LOCK *lock, PGPROC *proc2)
{










}