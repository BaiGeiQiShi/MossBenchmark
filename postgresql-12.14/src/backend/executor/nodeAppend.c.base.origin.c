/*-------------------------------------------------------------------------
 *
 * nodeAppend.c
 *	  routines to handle append nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeAppend.c
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitAppend	- initialize the append node
 *		ExecAppend		- retrieve the next tuple from the node
 *		ExecEndAppend	- shut down the append node
 *		ExecReScanAppend - rescan the append node
 *
 *	 NOTES
 *		Each append node contains a list of one or more subplans which
 *		must be iteratively processed (forwards or backwards).
 *		Tuples are retrieved by executing the 'whichplan'th subplan
 *		until the subplan stops returning tuples, at which point that
 *		plan is shut down and the next started up.
 *
 *		Append nodes don't make use of their left and right
 *		subtrees, rather they maintain a list of subplans so
 *		a typical append node looks like this in the plan tree:
 *
 *				   ...
 *				   /
 *				Append -------+------+------+--- nil
 *				/	\		  |		 |
 *| nil	nil		 ...    ...    ... subplans
 *
 *		Append nodes are currently used for unions, and to support
 *		inheritance queries, where several relations need to be scanned.
 *		For example, in our standard person/student/employee/student-emp
 *		example, where student and employee inherit from person
 *		and student-emp inherits from student and employee, the
 *		query:
 *
 *				select name from person
 *
 *		generates the plan:
 *
 *				  |
 *				Append -------+-------+--------+--------+
 *				/	\		  |		  |
 *|		| nil	nil		 Scan	 Scan	  Scan	   Scan |
 *|		   |		| person employee student student-emp
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/execPartition.h"
#include "executor/nodeAppend.h"
#include "miscadmin.h"

/* Shared state for parallel-aware Append. */
struct ParallelAppendState
{
  LWLock pa_lock;   /* mutual exclusion to choose next subplan */
  int pa_next_plan; /* next plan to choose by any worker */

  /*
   * pa_finished[i] should be true if no more workers should select subplan
   * i.  for a non-partial plan, this should be set to true as soon as a
   * worker selects the plan; for a partial plan, it remains false until
   * some worker executes the plan to completion.
   */
  bool pa_finished[FLEXIBLE_ARRAY_MEMBER];
};

#define INVALID_SUBPLAN_INDEX -1
#define NO_MATCHING_SUBPLANS -2

static TupleTableSlot *
ExecAppend(PlanState *pstate);
static bool
choose_next_subplan_locally(AppendState *node);
static bool
choose_next_subplan_for_leader(AppendState *node);
static bool
choose_next_subplan_for_worker(AppendState *node);
static void
mark_invalid_subplans_as_finished(AppendState *node);

/* ----------------------------------------------------------------
 *		ExecInitAppend
 *
 *		Begin all of the subscans of the append node.
 *
 *	   (This is potentially wasteful, since the entire result of the
 *		append node may not be scanned, but this way all of the
 *		structures get allocated in the executor's top level memory
 *		block instead of that of the call to ExecAppend.)
 * ----------------------------------------------------------------
 */
AppendState *
ExecInitAppend(Append *node, EState *estate, int eflags)
{














































































































































}

/* ----------------------------------------------------------------
 *	   ExecAppend
 *
 *		Handles iteration over multiple subplans.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecAppend(PlanState *pstate)
{






















































}

/* ----------------------------------------------------------------
 *		ExecEndAppend
 *
 *		Shuts down the subscans of the append node.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndAppend(AppendState *node)
{

















}

void
ExecReScanAppend(AppendState *node)
{






































}

/* ----------------------------------------------------------------
 *						Parallel Append Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecAppendEstimate
 *
 *		Compute the amount of space we'll need in the parallel
 *		query DSM, and inform pcxt->estimator about our needs.
 * ----------------------------------------------------------------
 */
void
ExecAppendEstimate(AppendState *node, ParallelContext *pcxt)
{




}

/* ----------------------------------------------------------------
 *		ExecAppendInitializeDSM
 *
 *		Set up shared state for Parallel Append.
 * ----------------------------------------------------------------
 */
void
ExecAppendInitializeDSM(AppendState *node, ParallelContext *pcxt)
{









}

/* ----------------------------------------------------------------
 *		ExecAppendReInitializeDSM
 *
 *		Reset shared state before beginning a fresh scan.
 * ----------------------------------------------------------------
 */
void
ExecAppendReInitializeDSM(AppendState *node, ParallelContext *pcxt)
{




}

/* ----------------------------------------------------------------
 *		ExecAppendInitializeWorker
 *
 *		Copy relevant information from TOC into planstate, and
 *initialize whatever is required to choose and execute the optimal subplan.
 * ----------------------------------------------------------------
 */
void
ExecAppendInitializeWorker(AppendState *node, ParallelWorkerContext *pwcxt)
{


}

/* ----------------------------------------------------------------
 *		choose_next_subplan_locally
 *
 *		Choose next subplan for a non-parallel-aware Append,
 *		returning false if there are no more.
 * ----------------------------------------------------------------
 */
static bool
choose_next_subplan_locally(AppendState *node)
{











































}

/* ----------------------------------------------------------------
 *		choose_next_subplan_for_leader
 *
 *      Try to pick a plan which doesn't commit us to doing much
 *      work locally, so that as much work as possible is done in
 *      the workers.  Cheapest subplans are at the end.
 * ----------------------------------------------------------------
 */
static bool
choose_next_subplan_for_leader(AppendState *node)
{
































































}

/* ----------------------------------------------------------------
 *		choose_next_subplan_for_worker
 *
 *		Choose next subplan for a parallel-aware Append, returning
 *		false if there are no more.
 *
 *		We start from the first plan and advance through the list;
 *		when we get back to the end, we loop back to the first
 *		partial plan.  This assigns the non-partial plans first in
 *		order of descending cost and then spreads out the workers
 *		as evenly as possible across the remaining partial plans.
 * ----------------------------------------------------------------
 */
static bool
choose_next_subplan_for_worker(AppendState *node)
{















































































































}

/*
 * mark_invalid_subplans_as_finished
 *		Marks the ParallelAppendState's pa_finished as true for each
 *invalid subplan.
 *
 * This function should only be called for parallel Append with run-time
 * pruning enabled.
 */
static void
mark_invalid_subplans_as_finished(AppendState *node)
{






















}