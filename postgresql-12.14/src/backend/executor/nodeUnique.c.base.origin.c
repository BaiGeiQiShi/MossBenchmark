/*-------------------------------------------------------------------------
 *
 * nodeUnique.c
 *	  Routines to handle unique'ing of queries where appropriate
 *
 * Unique is a very simple node type that just filters out duplicate
 * tuples from a stream of sorted tuples from its subplan.  It's essentially
 * a dumbed-down form of Group: the duplicate-removal functionality is
 * identical.  However, Unique doesn't do projection nor qual checking,
 * so it's marginally more efficient for cases where neither is needed.
 * (It's debatable whether the savings justifies carrying two plan node
 * types, though.)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeUnique.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecUnique		- generate a unique'd temporary relation
 *		ExecInitUnique	- initialize node and subnodes
 *		ExecEndUnique	- shutdown node and subnodes
 *
 * NOTES
 *		Assumes tuples returned from subplan arrive in
 *		sorted order.
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeUnique.h"
#include "miscadmin.h"
#include "utils/memutils.h"

/* ----------------------------------------------------------------
 *		ExecUnique
 * ----------------------------------------------------------------
 */
static TupleTableSlot * /* return: a tuple or NULL */
ExecUnique(PlanState *pstate)
{




























































}

/* ----------------------------------------------------------------
 *		ExecInitUnique
 *
 *		This initializes the unique node state structures and
 *		the node's subplan.
 * ----------------------------------------------------------------
 */
UniqueState *
ExecInitUnique(Unique *node, EState *estate, int eflags)
{




































}

/* ----------------------------------------------------------------
 *		ExecEndUnique
 *
 *		This shuts down the subplan and frees resources allocated
 *		to this node.
 * ----------------------------------------------------------------
 */
void
ExecEndUnique(UniqueState *node)
{






}

void
ExecReScanUnique(UniqueState *node)
{











}