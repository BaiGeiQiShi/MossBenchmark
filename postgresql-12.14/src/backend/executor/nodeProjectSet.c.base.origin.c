/*-------------------------------------------------------------------------
 *
 * nodeProjectSet.c
 *	  support for evaluating targetlists containing set-returning functions
 *
 * DESCRIPTION
 *
 *		ProjectSet nodes are inserted by the planner to evaluate
 *set-returning functions in the targetlist.  It's guaranteed that all
 *set-returning functions are directly at the top level of the targetlist, i.e.
 *they can't be inside more-complex expressions.  If that'd otherwise be the
 *case, the planner adds additional ProjectSet nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeProjectSet.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeProjectSet.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/memutils.h"

static TupleTableSlot *
ExecProjectSRF(ProjectSetState *node, bool continuing);

/* ----------------------------------------------------------------
 *		ExecProjectSet(node)
 *
 *		Return tuples after evaluating the targetlist (which contains
 *set returning functions).
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecProjectSet(PlanState *pstate)
{














































































}

/* ----------------------------------------------------------------
 *		ExecProjectSRF
 *
 *		Project a targetlist containing one or more set-returning
 *functions.
 *
 *		'continuing' indicates whether to continue projecting rows for
 *the same input tuple; or whether a new input tuple is being projected.
 *
 *		Returns NULL if no output tuple has been produced.
 *
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecProjectSRF(ProjectSetState *node, bool continuing)
{













































































}

/* ----------------------------------------------------------------
 *		ExecInitProjectSet
 *
 *		Creates the run-time state information for the ProjectSet node
 *		produced by the planner and initializes outer relations
 *		(child nodes).
 * ----------------------------------------------------------------
 */
ProjectSetState *
ExecInitProjectSet(ProjectSet *node, EState *estate, int eflags)
{



















































































}

/* ----------------------------------------------------------------
 *		ExecEndProjectSet
 *
 *		frees up storage allocated through C routines
 * ----------------------------------------------------------------
 */
void
ExecEndProjectSet(ProjectSetState *node)
{














}

void
ExecReScanProjectSet(ProjectSetState *node)
{











}