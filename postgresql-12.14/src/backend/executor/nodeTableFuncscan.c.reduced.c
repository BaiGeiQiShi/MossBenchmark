/*-------------------------------------------------------------------------
 *
 * nodeTableFuncscan.c
 *	  Support routines for scanning RangeTableFunc (XMLTABLE like
 *functions).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeTableFuncscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecTableFuncscan		scans a function.
 *		ExecFunctionNext		retrieve next tuple in
 *sequential order. ExecInitTableFuncscan	creates and initializes a
 *TableFuncscan node. ExecEndTableFuncscan		releases any storage
 *allocated. ExecReScanTableFuncscan rescans the function
 */
#include "postgres.h"

#include "nodes/execnodes.h"
#include "executor/executor.h"
#include "executor/nodeTableFuncscan.h"
#include "executor/tablefunc.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/xml.h"

static TupleTableSlot *
TableFuncNext(TableFuncScanState *node);
static bool
TableFuncRecheck(TableFuncScanState *node, TupleTableSlot *slot);

static void
tfuncFetchRows(TableFuncScanState *tstate, ExprContext *econtext);
static void
tfuncInitialize(TableFuncScanState *tstate, ExprContext *econtext, Datum doc);
static void
tfuncLoadRows(TableFuncScanState *tstate, ExprContext *econtext);

/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *		TableFuncNext
 *
 *		This is a workhorse for ExecTableFuncscan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
TableFuncNext(TableFuncScanState *node)
{

















}

/*
 * TableFuncRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
TableFuncRecheck(TableFuncScanState *node, TupleTableSlot *slot)
{


}

/* ----------------------------------------------------------------
 *		ExecTableFuncscan(node)
 *
 *		Scans the function sequentially and returns the next qualifying
 *		tuple.
 *		We call the ExecScan() routine and pass it the appropriate
 *		access method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecTableFuncScan(PlanState *pstate)
{



}

/* ----------------------------------------------------------------
 *		ExecInitTableFuncscan
 * ----------------------------------------------------------------
 */
TableFuncScanState *
ExecInitTableFuncScan(TableFuncScan *node, EState *estate, int eflags)
{
  TableFuncScanState *scanstate;
  TableFunc *tf = node->tablefunc;
  TupleDesc tupdesc;
  int i;

  /* check for unsupported flags */
  Assert(!(eflags & EXEC_FLAG_MARK));

  /*
   * TableFuncscan should not have any children.
   */
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

  /*
   * create new ScanState for node
   */
  scanstate = makeNode(TableFuncScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecTableFuncScan;

  /*
   * Miscellaneous initialization
   *
   * create expression context for node
   */
  ExecAssignExprContext(estate, &scanstate->ss.ps);

  /*
   * initialize source tuple type
   */
  tupdesc = BuildDescFromLists(tf->colnames, tf->coltypes, tf->coltypmods, tf->colcollations);
  /* and the corresponding scan slot */
  ExecInitScanTupleSlot(estate, &scanstate->ss, tupdesc, &TTSOpsMinimalTuple);

  /*
   * Initialize result type and projection.
   */
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

  /*
   * initialize child expressions
   */
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, &scanstate->ss.ps);

  /* Only XMLTABLE is supported currently */
  scanstate->routine = &XmlTableRoutine;

  scanstate->perTableCxt = AllocSetContextCreate(CurrentMemoryContext, "TableFunc per value context", ALLOCSET_DEFAULT_SIZES);
  scanstate->opaque = NULL; /* initialized at runtime */

  scanstate->ns_names = tf->ns_names;

  scanstate->ns_uris = ExecInitExprList(tf->ns_uris, (PlanState *)scanstate);
  scanstate->docexpr = ExecInitExpr((Expr *)tf->docexpr, (PlanState *)scanstate);
  scanstate->rowexpr = ExecInitExpr((Expr *)tf->rowexpr, (PlanState *)scanstate);
  scanstate->colexprs = ExecInitExprList(tf->colexprs, (PlanState *)scanstate);
  scanstate->coldefexprs = ExecInitExprList(tf->coldefexprs, (PlanState *)scanstate);

  scanstate->notnulls = tf->notnulls;

  /* these are allocated now and initialized later */
  scanstate->in_functions = palloc(sizeof(FmgrInfo) * tupdesc->natts);
  scanstate->typioparams = palloc(sizeof(Oid) * tupdesc->natts);

  /*
   * Fill in the necessary fmgr infos.
   */
  for (i = 0; i < tupdesc->natts; i++) {
    Oid in_funcid;

    getTypeInputInfo(TupleDescAttr(tupdesc, i)->atttypid, &in_funcid, &scanstate->typioparams[i]);
    fmgr_info(in_funcid, &scanstate->in_functions[i]);
  }

  return scanstate;
}

/* ----------------------------------------------------------------
 *		ExecEndTableFuncscan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndTableFuncScan(TableFuncScanState *node)
{
  /*
   * Free the exprcontext
   */
  ExecFreeExprContext(&node->ss.ps);

  /*
   * clean out the tuple table
   */
  if (node->ss.ps.ps_ResultTupleSlot) {

  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

  /*
   * Release tuplestore resources
   */
  if (node->tupstore != NULL) {

  }
  node->tupstore = NULL;
}

/* ----------------------------------------------------------------
 *		ExecReScanTableFuncscan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanTableFuncScan(TableFuncScanState *node)
{




















}

/* ----------------------------------------------------------------
 *		tfuncFetchRows
 *
 *		Read rows from a TableFunc producer
 * ----------------------------------------------------------------
 */
static void
tfuncFetchRows(TableFuncScanState *tstate, ExprContext *econtext)
{






























































}

/*
 * Fill in namespace declarations, the row filter, and column filters in a
 * table expression builder context.
 */
static void
tfuncInitialize(TableFuncScanState *tstate, ExprContext *econtext, Datum doc)
{







































































}

/*
 * Load all the rows from the TableFunc table builder into a tuplestore.
 */
static void
tfuncLoadRows(TableFuncScanState *tstate, ExprContext *econtext)
{











































































}