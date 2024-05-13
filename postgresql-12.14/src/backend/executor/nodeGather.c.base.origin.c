/*-------------------------------------------------------------------------
 *
 * nodeGather.c
 *	  Support routines for scanning a plan via multiple workers.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * A Gather executor launches parallel workers to run multiple copies of a
 * plan.  It can also run the plan itself, if the workers are not available
 * or have not started up yet.  It then merges all of the results it produces
 * and the results from the workers into a single output stream.  Therefore,
 * it will normally be used with a plan where running multiple copies of the
 * same plan does not produce duplicate output, such as parallel-aware
 * SeqScan.
 *
 * Alternatively, a Gather node can be configured to use just one worker
 * and the single-copy flag can be set.  In this case, the Gather node will
 * run the plan in one worker and will not execute the plan itself.  In
 * this case, it simply returns whatever tuples were returned by the worker.
 * If a worker cannot be obtained, then it will run the plan itself and
 * return the results.  Therefore, a plan used with a single-copy Gather
 * node need not be parallel-aware.
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeGather.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/relscan.h"
#include "access/xact.h"
#include "executor/execdebug.h"
#include "executor/execParallel.h"
#include "executor/nodeGather.h"
#include "executor/nodeSubplan.h"
#include "executor/tqueue.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
ExecGather(PlanState *pstate);
static TupleTableSlot *
gather_getnext(GatherState *gatherstate);
static HeapTuple
gather_readnext(GatherState *gatherstate);
static void
ExecShutdownGatherWorkers(GatherState *node);

/* ----------------------------------------------------------------
 *		ExecInitGather
 * ----------------------------------------------------------------
 */
GatherState *
ExecInitGather(Gather *node, EState *estate, int eflags)
{






































































}

/* ----------------------------------------------------------------
 *		ExecGather(node)
 *
 *		Scans the relation via multiple workers and returns
 *		the next qualifying tuple.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecGather(PlanState *pstate)
{































































































}

/* ----------------------------------------------------------------
 *		ExecEndGather
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndGather(GatherState *node)
{







}

/*
 * Read the next tuple.  We might fetch a tuple from one of the tuple queues
 * using gather_readnext, or if no tuple queue contains a tuple and the
 * single_copy flag is not set, we might generate one locally instead.
 */
static TupleTableSlot *
gather_getnext(GatherState *gatherstate)
{









































}

/*
 * Attempt to read a tuple from one of our parallel workers.
 */
static HeapTuple
gather_readnext(GatherState *gatherstate)
{


















































































}

/* ----------------------------------------------------------------
 *		ExecShutdownGatherWorkers
 *
 *		Stop all the parallel workers.
 * ----------------------------------------------------------------
 */
static void
ExecShutdownGatherWorkers(GatherState *node)
{











}

/* ----------------------------------------------------------------
 *		ExecShutdownGather
 *
 *		Destroy the setup for parallel workers including parallel
 *context.
 * ----------------------------------------------------------------
 */
void
ExecShutdownGather(GatherState *node)
{








}

/* ----------------------------------------------------------------
 *						Join Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecReScanGather
 *
 *		Prepare to re-scan the result of a Gather.
 * ----------------------------------------------------------------
 */
void
ExecReScanGather(GatherState *node)
{



































}