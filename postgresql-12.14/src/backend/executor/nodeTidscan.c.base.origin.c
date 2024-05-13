/*-------------------------------------------------------------------------
 *
 * nodeTidscan.c
 *	  Routines to support direct tid scans of relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeTidscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *
 *		ExecTidScan			scans a relation using tids
 *		ExecInitTidScan		creates and initializes state info.
 *		ExecReScanTidScan	rescans the tid relation.
 *		ExecEndTidScan		releases all storage.
 */
#include "postgres.h"

#include "access/sysattr.h"
#include "access/tableam.h"
#include "catalog/pg_type.h"
#include "executor/execdebug.h"
#include "executor/nodeTidscan.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "storage/bufmgr.h"
#include "utils/array.h"
#include "utils/rel.h"

#define IsCTIDVar(node) ((node) != NULL && IsA((node), Var) && ((Var *)(node))->varattno == SelfItemPointerAttributeNumber && ((Var *)(node))->varlevelsup == 0)

/* one element in tss_tidexprs */
typedef struct TidExpr
{
  ExprState *exprstate; /* ExprState for a TID-yielding subexpr */
  bool isarray;         /* if true, it yields tid[] not just tid */
  CurrentOfExpr *cexpr; /* alternatively, we can have CURRENT OF */
} TidExpr;

static void
TidExprListCreate(TidScanState *tidstate);
static void
TidListEval(TidScanState *tidstate);
static int
itemptr_comparator(const void *a, const void *b);
static TupleTableSlot *
TidNext(TidScanState *node);

/*
 * Extract the qual subexpressions that yield TIDs to search for,
 * and compile them into ExprStates if they're ordinary expressions.
 *
 * CURRENT OF is a special case that we can't compile usefully;
 * just drop it into the TidExpr list as-is.
 */
static void
TidExprListCreate(TidScanState *tidstate)
{

























































}

/*
 * Compute the list of TIDs to be visited, by evaluating the expressions
 * for them.
 *
 * (The result is actually an array, not a list.)
 */
static void
TidListEval(TidScanState *tidstate)
{


















































































































































}

/*
 * qsort comparator for ItemPointerData items
 */
static int
itemptr_comparator(const void *a, const void *b)
{
























}

/* ----------------------------------------------------------------
 *		TidNext
 *
 *		Retrieve a tuple from the TidScan node's currentRelation
 *		using the tids in the TidScanState information.
 *
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
TidNext(TidScanState *node)
{

































































































}

/*
 * TidRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
TidRecheck(TidScanState *node, TupleTableSlot *slot)
{






}

/* ----------------------------------------------------------------
 *		ExecTidScan(node)
 *
 *		Scans the relation using tids and returns
 *		   the next qualifying tuple in the direction specified.
 *		We call the ExecScan() routine and pass it the appropriate
 *		access method functions.
 *
 *		Conditions:
 *		  -- the "cursor" maintained by the AMI is positioned at the
 *tuple returned previously.
 *
 *		Initial States:
 *		  -- the relation indicated is opened for scanning so that the
 *			 "cursor" is positioned before the first qualifying
 *tuple.
 *		  -- tidPtr is -1.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecTidScan(PlanState *pstate)
{



}

/* ----------------------------------------------------------------
 *		ExecReScanTidScan(node)
 * ----------------------------------------------------------------
 */
void
ExecReScanTidScan(TidScanState *node)
{















}

/* ----------------------------------------------------------------
 *		ExecEndTidScan
 *
 *		Releases any storage allocated through C routines.
 *		Returns nothing.
 * ----------------------------------------------------------------
 */
void
ExecEndTidScan(TidScanState *node)
{


















}

/* ----------------------------------------------------------------
 *		ExecInitTidScan
 *
 *		Initializes the tid scan's state information, creates
 *		scan keys, and opens the base and tid relations.
 *
 *		Parameters:
 *		  node: TidNode node produced by the planner.
 *		  estate: the execution state initialized in InitPlan.
 * ----------------------------------------------------------------
 */
TidScanState *
ExecInitTidScan(TidScan *node, EState *estate, int eflags)
{























































}