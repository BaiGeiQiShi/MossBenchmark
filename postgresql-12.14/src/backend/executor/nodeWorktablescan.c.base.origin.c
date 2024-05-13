/*-------------------------------------------------------------------------
 *
 * nodeWorktablescan.c
 *	  routines to handle WorkTableScan nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeWorktablescan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeWorktablescan.h"

static TupleTableSlot *
WorkTableScanNext(WorkTableScanState *node);

/* ----------------------------------------------------------------
 *		WorkTableScanNext
 *
 *		This is a workhorse for ExecWorkTableScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
WorkTableScanNext(WorkTableScanState *node)
{




























}

/*
 * WorkTableScanRecheck -- access method routine to recheck a tuple in
 * EvalPlanQual
 */
static bool
WorkTableScanRecheck(WorkTableScanState *node, TupleTableSlot *slot)
{


}

/* ----------------------------------------------------------------
 *		ExecWorkTableScan(node)
 *
 *		Scans the worktable sequentially and returns the next qualifying
 *tuple. We call the ExecScan() routine and pass it the appropriate access
 *method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecWorkTableScan(PlanState *pstate)
{




































}

/* ----------------------------------------------------------------
 *		ExecInitWorkTableScan
 * ----------------------------------------------------------------
 */
WorkTableScanState *
ExecInitWorkTableScan(WorkTableScan *node, EState *estate, int eflags)
{

















































}

/* ----------------------------------------------------------------
 *		ExecEndWorkTableScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndWorkTableScan(WorkTableScanState *node)
{













}

/* ----------------------------------------------------------------
 *		ExecReScanWorkTableScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanWorkTableScan(WorkTableScanState *node)
{












}