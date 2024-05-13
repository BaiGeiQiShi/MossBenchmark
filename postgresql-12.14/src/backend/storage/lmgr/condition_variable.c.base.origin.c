/*-------------------------------------------------------------------------
 *
 * condition_variable.c
 *	  Implementation of condition variables.  Condition variables provide
 *	  a way for one process to wait until a specific condition occurs,
 *	  without needing to know the specific identity of the process for
 *	  which they are waiting.  Waits for condition variables can be
 *	  interrupted, unlike LWLock waits.  Condition variables are safe
 *	  to use within dynamic shared memory segments.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/storage/lmgr/condition_variable.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "storage/condition_variable.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/proclist.h"
#include "storage/spin.h"
#include "utils/memutils.h"

/* Initially, we are not prepared to sleep on any condition variable. */
static ConditionVariable *cv_sleep_target = NULL;

/* Reusable WaitEventSet. */
static WaitEventSet *cv_wait_event_set = NULL;

/*
 * Initialize a condition variable.
 */
void
ConditionVariableInit(ConditionVariable *cv)
{


}

/*
 * Prepare to wait on a given condition variable.
 *
 * This can optionally be called before entering a test/sleep loop.
 * Doing so is more efficient if we'll need to sleep at least once.
 * However, if the first test of the exit condition is likely to succeed,
 * it's more efficient to omit the ConditionVariablePrepareToSleep call.
 * See comments in ConditionVariableSleep for more detail.
 *
 * Caution: "before entering the loop" means you *must* test the exit
 * condition between calling ConditionVariablePrepareToSleep and calling
 * ConditionVariableSleep.  If that is inconvenient, omit calling
 * ConditionVariablePrepareToSleep.
 */
void
ConditionVariablePrepareToSleep(ConditionVariable *cv)
{











































}

/*
 * Wait for the given condition variable to be signaled.
 *
 * This should be called in a predicate loop that tests for a specific exit
 * condition and otherwise sleeps, like so:
 *
 *	 ConditionVariablePrepareToSleep(cv);  // optional
 *	 while (condition for which we are waiting is not true)
 *		 ConditionVariableSleep(cv, wait_event_info);
 *	 ConditionVariableCancelSleep();
 *
 * wait_event_info should be a value from one of the WaitEventXXX enums
 * defined in pgstat.h.  This controls the contents of pg_stat_activity's
 * wait_event_type and wait_event columns while waiting.
 */
void
ConditionVariableSleep(ConditionVariable *cv, uint32 wait_event_info)
{




























































}

/*
 * Cancel any pending sleep operation.
 *
 * We just need to remove ourselves from the wait queue of any condition
 * variable for which we have previously prepared a sleep.
 *
 * Do nothing if nothing is pending; this allows this function to be called
 * during transaction abort to clean up any unfinished CV sleep.
 */
void
ConditionVariableCancelSleep(void)
{















}

/*
 * Wake up the oldest process sleeping on the CV, if there is any.
 *
 * Note: it's difficult to tell whether this has any real effect: we know
 * whether we took an entry off the list, but the entry might only be a
 * sentinel.  Hence, think twice before proposing that this should return
 * a flag telling whether it woke somebody.
 */
void
ConditionVariableSignal(ConditionVariable *cv)
{















}

/*
 * Wake up all processes sleeping on the given CV.
 *
 * This guarantees to wake all processes that were sleeping on the CV
 * at time of call, but processes that add themselves to the list mid-call
 * will typically not get awakened.
 */
void
ConditionVariableBroadcast(ConditionVariable *cv)
{




















































































}