/*-------------------------------------------------------------------------
 *
 * nodeNamedtuplestorescan.c
 *	  routines to handle NamedTuplestoreScan nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeNamedtuplestorescan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeNamedtuplestorescan.h"
#include "miscadmin.h"
#include "utils/queryenvironment.h"

static TupleTableSlot *
NamedTuplestoreScanNext(NamedTuplestoreScanState *node);

/* ----------------------------------------------------------------
 *		NamedTuplestoreScanNext
 *
 *		This is a workhorse for ExecNamedTuplestoreScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
NamedTuplestoreScanNext(NamedTuplestoreScanState *node)
{












}

/*
 * NamedTuplestoreScanRecheck -- access method routine to recheck a tuple in
 * EvalPlanQual
 */
static bool
NamedTuplestoreScanRecheck(NamedTuplestoreScanState *node, TupleTableSlot *slot)
{


}

/* ----------------------------------------------------------------
 *		ExecNamedTuplestoreScan(node)
 *
 *		Scans the CTE sequentially and returns the next qualifying
 *tuple. We call the ExecScan() routine and pass it the appropriate access
 *method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecNamedTuplestoreScan(PlanState *pstate)
{



}

/* ----------------------------------------------------------------
 *		ExecInitNamedTuplestoreScan
 * ----------------------------------------------------------------
 */
NamedTuplestoreScanState *
ExecInitNamedTuplestoreScan(NamedTuplestoreScan *node, EState *estate, int eflags)
{





































































}

/* ----------------------------------------------------------------
 *		ExecEndNamedTuplestoreScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndNamedTuplestoreScan(NamedTuplestoreScanState *node)
{













}

/* ----------------------------------------------------------------
 *		ExecReScanNamedTuplestoreScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanNamedTuplestoreScan(NamedTuplestoreScanState *node)
{














}