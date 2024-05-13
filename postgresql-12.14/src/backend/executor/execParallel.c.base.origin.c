/*-------------------------------------------------------------------------
 *
 * execParallel.c
 *	  Support routines for parallel execution.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * This file contains routines that are intended to support setting up,
 * using, and tearing down a ParallelContext from within the PostgreSQL
 * executor.  The ParallelContext machinery will handle starting the
 * workers and ensuring that their state generally matches that of the
 * leader; see src/backend/access/transam/README.parallel for details.
 * However, we must save and restore relevant executor state, such as
 * any ParamListInfo associated with the query, buffer usage info, and
 * the actual plan to be passed down to the worker.
 *
 * IDENTIFICATION
 *	  src/backend/executor/execParallel.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/execParallel.h"
#include "executor/executor.h"
#include "executor/nodeAppend.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeCustom.h"
#include "executor/nodeForeignscan.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeSeqscan.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/tqueue.h"
#include "jit/jit.h"
#include "nodes/nodeFuncs.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/datum.h"
#include "utils/dsa.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "pgstat.h"

/*
 * Magic numbers for parallel executor communication.  We use constants
 * greater than any 32-bit integer here so that values < 2^32 can be used
 * by individual parallel nodes to store their own state.
 */
#define PARALLEL_KEY_EXECUTOR_FIXED UINT64CONST(0xE000000000000001)
#define PARALLEL_KEY_PLANNEDSTMT UINT64CONST(0xE000000000000002)
#define PARALLEL_KEY_PARAMLISTINFO UINT64CONST(0xE000000000000003)
#define PARALLEL_KEY_BUFFER_USAGE UINT64CONST(0xE000000000000004)
#define PARALLEL_KEY_TUPLE_QUEUE UINT64CONST(0xE000000000000005)
#define PARALLEL_KEY_INSTRUMENTATION UINT64CONST(0xE000000000000006)
#define PARALLEL_KEY_DSA UINT64CONST(0xE000000000000007)
#define PARALLEL_KEY_QUERY_TEXT UINT64CONST(0xE000000000000008)
#define PARALLEL_KEY_JIT_INSTRUMENTATION UINT64CONST(0xE000000000000009)

#define PARALLEL_TUPLE_QUEUE_SIZE 65536

/*
 * Fixed-size random stuff that we need to pass to parallel workers.
 */
typedef struct FixedParallelExecutorState
{
  int64 tuples_needed; /* tuple bound, see ExecSetTupleBound */
  dsa_pointer param_exec;
  int eflags;
  int jit_flags;
} FixedParallelExecutorState;

/*
 * DSM structure for accumulating per-PlanState instrumentation.
 *
 * instrument_options: Same meaning here as in instrument.c.
 *
 * instrument_offset: Offset, relative to the start of this structure,
 * of the first Instrumentation object.  This will depend on the length of
 * the plan_node_id array.
 *
 * num_workers: Number of workers.
 *
 * num_plan_nodes: Number of plan nodes.
 *
 * plan_node_id: Array of plan nodes for which we are gathering instrumentation
 * from parallel workers.  The length of this array is given by num_plan_nodes.
 */
struct SharedExecutorInstrumentation
{
  int instrument_options;
  int instrument_offset;
  int num_workers;
  int num_plan_nodes;
  int plan_node_id[FLEXIBLE_ARRAY_MEMBER];
  /* array of num_plan_nodes * num_workers Instrumentation objects follows */
};
#define GetInstrumentationArray(sei) (AssertVariableIsOfTypeMacro(sei, SharedExecutorInstrumentation *), (Instrumentation *)(((char *)sei) + sei->instrument_offset))

/* Context object for ExecParallelEstimate. */
typedef struct ExecParallelEstimateContext
{
  ParallelContext *pcxt;
  int nnodes;
} ExecParallelEstimateContext;

/* Context object for ExecParallelInitializeDSM. */
typedef struct ExecParallelInitializeDSMContext
{
  ParallelContext *pcxt;
  SharedExecutorInstrumentation *instrumentation;
  int nnodes;
} ExecParallelInitializeDSMContext;

/* Helper functions that run in the parallel leader. */
static char *
ExecSerializePlan(Plan *plan, EState *estate);
static bool
ExecParallelEstimate(PlanState *node, ExecParallelEstimateContext *e);
static bool
ExecParallelInitializeDSM(PlanState *node, ExecParallelInitializeDSMContext *d);
static shm_mq_handle **
ExecParallelSetupTupleQueues(ParallelContext *pcxt, bool reinitialize);
static bool
ExecParallelReInitializeDSM(PlanState *planstate, ParallelContext *pcxt);
static bool
ExecParallelRetrieveInstrumentation(PlanState *planstate, SharedExecutorInstrumentation *instrumentation);

/* Helper function that runs in the parallel worker. */
static DestReceiver *
ExecParallelGetReceiver(dsm_segment *seg, shm_toc *toc);

/*
 * Create a serialized representation of the plan to be sent to each worker.
 */
static char *
ExecSerializePlan(Plan *plan, EState *estate)
{






































































}

/*
 * Parallel-aware plan nodes (and occasionally others) may need some state
 * which is shared across all parallel workers.  Before we size the DSM, give
 * them a chance to call shm_toc_estimate_chunk or shm_toc_estimate_keys on
 * &pcxt->estimator.
 *
 * While we're at it, count the number of PlanState nodes in the tree, so
 * we know how many Instrumentation structures we need.
 */
static bool
ExecParallelEstimate(PlanState *planstate, ExecParallelEstimateContext *e)
{








































































}

/*
 * Estimate the amount of space required to serialize the indicated parameters.
 */
static Size
EstimateParamExecSpace(EState *estate, Bitmapset *params)
{






























}

/*
 * Serialize specified PARAM_EXEC parameters.
 *
 * We write the number of parameters first, as a 4-byte integer, and then
 * write details for each parameter in turn.  The details for each parameter
 * consist of a 4-byte paramid (location of param in execution time internal
 * parameter array) and then the datum as serialized by datumSerialize().
 */
static dsa_pointer
SerializeParamExecParams(EState *estate, Bitmapset *params, dsa_area *area)
{















































}

/*
 * Restore specified PARAM_EXEC parameters.
 */
static void
RestoreParamExecParams(char *start_address, EState *estate)
{




















}

/*
 * Initialize the dynamic shared memory segment that will be used to control
 * parallel execution.
 */
static bool
ExecParallelInitializeDSM(PlanState *planstate, ExecParallelInitializeDSMContext *d)
{























































































}

/*
 * It sets up the response queues for backend workers to return tuples
 * to the main backend and start the workers.
 */
static shm_mq_handle **
ExecParallelSetupTupleQueues(ParallelContext *pcxt, bool reinitialize)
{













































}

/*
 * Sets up the required infrastructure for backend workers to perform
 * execution and return results to the main backend.
 */
ParallelExecutorInfo *
ExecInitParallelPlan(PlanState *planstate, EState *estate, Bitmapset *sendParams, int nworkers, int64 tuples_needed)
{




















































































































































































































































}

/*
 * Set up tuple queue readers to read the results of a parallel subplan.
 *
 * This is separate from ExecInitParallelPlan() because we can launch the
 * worker processes and let them start doing something before we do this.
 */
void
ExecParallelCreateReaders(ParallelExecutorInfo *pei)
{















}

/*
 * Re-initialize the parallel executor shared memory state before launching
 * a fresh batch of workers.
 */
void
ExecParallelReinitialize(PlanState *planstate, ParallelExecutorInfo *pei, Bitmapset *sendParams)
{






































}

/*
 * Traverse plan tree to reinitialize per-node dynamic shared memory state
 */
static bool
ExecParallelReInitializeDSM(PlanState *planstate, ParallelContext *pcxt)
{




































































}

/*
 * Copy instrumentation information about this node and its descendants from
 * dynamic shared memory.
 */
static bool
ExecParallelRetrieveInstrumentation(PlanState *planstate, SharedExecutorInstrumentation *instrumentation)
{

























































}

/*
 * Add up the workers' JIT instrumentation from dynamic shared memory.
 */
static void
ExecParallelRetrieveJitInstrumentation(PlanState *planstate, SharedJitInstrumentation *shared_jit)
{































}

/*
 * Finish parallel execution.  We wait for parallel workers to finish, and
 * accumulate their buffer usage.
 */
void
ExecParallelFinish(ParallelExecutorInfo *pei)
{


















































}

/*
 * Accumulate instrumentation, and then clean up whatever ParallelExecutorInfo
 * resources still exist after ExecParallelFinish.  We separate these
 * routines because someone might want to examine the contents of the DSM
 * after ExecParallelFinish and before calling this routine.
 */
void
ExecParallelCleanup(ParallelExecutorInfo *pei)
{





























}

/*
 * Create a DestReceiver to write tuples we produce to the shm_mq designated
 * for that purpose.
 */
static DestReceiver *
ExecParallelGetReceiver(dsm_segment *seg, shm_toc *toc)
{








}

/*
 * Create a QueryDesc for the PlannedStmt we are to execute, and return it.
 */
static QueryDesc *
ExecParallelGetQueryDesc(shm_toc *toc, DestReceiver *receiver, int instrument_options)
{



















}

/*
 * Copy instrumentation information from this node and its descendants into
 * dynamic shared memory, so that the parallel leader can retrieve it.
 */
static bool
ExecParallelReportInstrumentation(PlanState *planstate, SharedExecutorInstrumentation *instrumentation)
{



































}

/*
 * Initialize the PlanState and its descendants with the information
 * retrieved from shared memory.  This has to be done once the PlanState
 * is allocated and initialized by executor; that is, after ExecutorStart().
 */
static bool
ExecParallelInitializeWorker(PlanState *planstate, ParallelWorkerContext *pwcxt)
{





































































}

/*
 * Main entrypoint for parallel query worker processes.
 *
 * We reach this function from ParallelWorkerMain, so the setup necessary to
 * create a sensible parallel environment has already been done;
 * ParallelWorkerMain worries about stuff like the transaction state, combo
 * CID mappings, and GUC values, so we don't need to deal with any of that
 * here.
 *
 * Our job is to deal with concerns specific to the executor.  The parallel
 * group leader will have stored a serialized PlannedStmt, and it's our job
 * to execute that plan and write the resulting tuples to the appropriate
 * tuple queue.  Various bits of supporting information that we need in order
 * to do this are also stored in the dsm_segment and can be accessed through
 * the shm_toc.
 */
void
ParallelQueryMain(dsm_segment *seg, shm_toc *toc)
{
































































































}