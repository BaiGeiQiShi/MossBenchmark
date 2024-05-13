/*-------------------------------------------------------------------------
 *
 * nodeCtescan.c
 *	  routines to handle CteScan nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeCtescan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeCtescan.h"
#include "miscadmin.h"

static TupleTableSlot *
CteScanNext(CteScanState *node);

/* ----------------------------------------------------------------
 *		CteScanNext
 *
 *		This is a workhorse for ExecCteScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
CteScanNext(CteScanState *node)
{
















































































































}

/*
 * CteScanRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
CteScanRecheck(CteScanState *node, TupleTableSlot *slot)
{


}

/* ----------------------------------------------------------------
 *		ExecCteScan(node)
 *
 *		Scans the CTE sequentially and returns the next qualifying
 *tuple. We call the ExecScan() routine and pass it the appropriate access
 *method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecCteScan(PlanState *pstate)
{



}

/* ----------------------------------------------------------------
 *		ExecInitCteScan
 * ----------------------------------------------------------------
 */
CteScanState *
ExecInitCteScan(CteScan *node, EState *estate, int eflags)
{































































































}

/* ----------------------------------------------------------------
 *		ExecEndCteScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndCteScan(CteScanState *node)
{






















}

/* ----------------------------------------------------------------
 *		ExecReScanCteScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanCteScan(CteScanState *node)
{
































}