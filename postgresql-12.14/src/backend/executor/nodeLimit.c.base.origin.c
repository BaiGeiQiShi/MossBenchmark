/*-------------------------------------------------------------------------
 *
 * nodeLimit.c
 *	  Routines to handle limiting of query results where appropriate
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeLimit.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecLimit		- extract a limited range of tuples
 *		ExecInitLimit	- initialize node and subnodes..
 *		ExecEndLimit	- shutdown node and subnodes
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeLimit.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"

static void
recompute_limits(LimitState *node);
static int64
compute_tuples_needed(LimitState *node);

/* ----------------------------------------------------------------
 *		ExecLimit
 *
 *		This is a very simple node which just performs LIMIT/OFFSET
 *		filtering on the stream of tuples returned by a subplan.
 * ----------------------------------------------------------------
 */
static TupleTableSlot * /* return: a tuple or NULL */
ExecLimit(PlanState *pstate)
{











































































































































































































}

/*
 * Evaluate the limit/offset expressions --- done at startup or rescan.
 *
 * This is also a handy place to reset the current-position state info.
 */
static void
recompute_limits(LimitState *node)
{



































































}

/*
 * Compute the maximum number of tuples needed to satisfy this Limit node.
 * Return a negative value if there is not a determinable limit.
 */
static int64
compute_tuples_needed(LimitState *node)
{






}

/* ----------------------------------------------------------------
 *		ExecInitLimit
 *
 *		This initializes the limit node state structures and
 *		the node's subplan.
 * ----------------------------------------------------------------
 */
LimitState *
ExecInitLimit(Limit *node, EState *estate, int eflags)
{



















































}

/* ----------------------------------------------------------------
 *		ExecEndLimit
 *
 *		This shuts down the subplan and frees resources allocated
 *		to this node.
 * ----------------------------------------------------------------
 */
void
ExecEndLimit(LimitState *node)
{


}

void
ExecReScanLimit(LimitState *node)
{















}