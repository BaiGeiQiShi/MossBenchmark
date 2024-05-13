/*-------------------------------------------------------------------------
 *
 * nodeSubplan.c
 *	  routines to support sub-selects appearing in expressions
 *
 * This module is concerned with executing SubPlan expression nodes, which
 * should not be confused with sub-SELECTs appearing in FROM.  SubPlans are
 * divided into "initplans", which are those that need only one evaluation per
 * query (among other restrictions, this requires that they don't use any
 * direct correlation variables from the parent plan level), and "regular"
 * subplans, which are re-evaluated every time their result is required.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeSubplan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 *	 INTERFACE ROUTINES
 *		ExecSubPlan  - process a subselect
 *		ExecInitSubPlan - initialize a subselect
 */
#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/htup_details.h"
#include "executor/executor.h"
#include "executor/nodeSubplan.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static Datum
ExecHashSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull);
static Datum
ExecScanSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull);
static void
buildSubPlanHash(SubPlanState *node, ExprContext *econtext);
static bool
findPartialMatch(TupleHashTable hashtable, TupleTableSlot *slot, FmgrInfo *eqfunctions);
static bool
slotAllNulls(TupleTableSlot *slot);
static bool
slotNoNulls(TupleTableSlot *slot);

/* ----------------------------------------------------------------
 *		ExecSubPlan
 *
 * This is the main entry point for execution of a regular SubPlan.
 * ----------------------------------------------------------------
 */
Datum
ExecSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull)
{





































}

/*
 * ExecHashSubPlan: store subselect result in an in-memory hash table
 */
static Datum
ExecHashSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull)
{















































































































}

/*
 * ExecScanSubPlan: default case where we have to rescan subplan each time
 */
static Datum
ExecScanSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull)
{




























































































































































































































































}

/*
 * buildSubPlanHash: load hash table by scanning subplan output.
 */
static void
buildSubPlanHash(SubPlanState *node, ExprContext *econtext)
{













































































































































}

/*
 * execTuplesUnequal
 *		Return true if two tuples are definitely unequal in the
 *indicated fields.
 *
 * Nulls are neither equal nor unequal to anything else.  A true result
 * is obtained only if there are non-null fields that compare not-equal.
 *
 * slot1, slot2: the tuples to compare (must have same columns!)
 * numCols: the number of attributes to be examined
 * matchColIdx: array of attribute column numbers
 * eqFunctions: array of fmgr lookup info for the equality functions to use
 * evalContext: short-term memory context for executing the functions
 */
static bool
execTuplesUnequal(TupleTableSlot *slot1, TupleTableSlot *slot2, int numCols, AttrNumber *matchColIdx, FmgrInfo *eqfunctions, const Oid *collations, MemoryContext evalContext)
{















































}

/*
 * findPartialMatch: does the hashtable contain an entry that is not
 * provably distinct from the tuple?
 *
 * We have to scan the whole hashtable; we can't usefully use hashkeys
 * to guide probing, since we might get partial matches on tuples with
 * hashkeys quite unrelated to what we'd get from the given tuple.
 *
 * Caller must provide the equality functions to use, since in cross-type
 * cases these are different from the hashtable's internal functions.
 */
static bool
findPartialMatch(TupleHashTable hashtable, TupleTableSlot *slot, FmgrInfo *eqfunctions)
{



















}

/*
 * slotAllNulls: is the slot completely NULL?
 *
 * This does not test for dropped columns, which is OK because we only
 * use it on projected tuples.
 */
static bool
slotAllNulls(TupleTableSlot *slot)
{











}

/*
 * slotNoNulls: is the slot entirely not NULL?
 *
 * This does not test for dropped columns, which is OK because we only
 * use it on projected tuples.
 */
static bool
slotNoNulls(TupleTableSlot *slot)
{











}

/* ----------------------------------------------------------------
 *		ExecInitSubPlan
 *
 * Create a SubPlanState for a SubPlan; this is the SubPlan-specific part
 * of ExecInitExpr().  We split it out so that it can be used for InitPlans
 * as well as regular SubPlans.  Note that we don't link the SubPlan into
 * the parent's subPlan list, because that shouldn't happen for InitPlans.
 * Instead, ExecInitExpr() does that one part.
 * ----------------------------------------------------------------
 */
SubPlanState *
ExecInitSubPlan(SubPlan *subplan, PlanState *parent)
{

















































































































































































































}

/* ----------------------------------------------------------------
 *		ExecSetParamPlan
 *
 *		Executes a subplan and sets its output parameters.
 *
 * This is called from ExecEvalParamExec() when the value of a PARAM_EXEC
 * parameter is requested and the param's execPlan field is set (indicating
 * that the param has not yet been evaluated).  This allows lazy evaluation
 * of initplans: we don't run the subplan until/unless we need its output.
 * Note that this routine MUST clear the execPlan fields of the plan's
 * output parameters after evaluating them!
 *
 * The results of this function are stored in the EState associated with the
 * ExprContext (particularly, its ecxt_param_exec_vals); any pass-by-ref
 * result Datums are allocated in the EState's per-query memory.  The passed
 * econtext can be any ExprContext belonging to that EState; which one is
 * important only to the extent that the ExprContext's per-tuple memory
 * context is used to evaluate any parameters passed down to the subplan.
 * (Thus in principle, the shorter-lived the ExprContext the better, since
 * that data isn't needed after we return.  In practice, because initplan
 * parameters are never more complex than Vars, Aggrefs, etc, evaluating them
 * currently never leaks any memory anyway.)
 * ----------------------------------------------------------------
 */
void
ExecSetParamPlan(SubPlanState *node, ExprContext *econtext)
{
















































































































































































}

/*
 * ExecSetParamPlanMulti
 *
 * Apply ExecSetParamPlan to evaluate any not-yet-evaluated initplan output
 * parameters whose ParamIDs are listed in "params".  Any listed params that
 * are not initplan outputs are ignored.
 *
 * As with ExecSetParamPlan, any ExprContext belonging to the current EState
 * can be used, but in principle a shorter-lived ExprContext is better than a
 * longer-lived one.
 */
void
ExecSetParamPlanMulti(const Bitmapset *params, ExprContext *econtext)
{















}

/*
 * Mark an initplan as needing recalculation
 */
void
ExecReScanSetParamPlan(SubPlanState *node, PlanState *parent)
{











































}

/*
 * ExecInitAlternativeSubPlan
 *
 * Initialize for execution of one of a set of alternative subplans.
 */
AlternativeSubPlanState *
ExecInitAlternativeSubPlan(AlternativeSubPlan *asplan, PlanState *parent)
{





















































}

/*
 * ExecAlternativeSubPlan
 *
 * Execute one of a set of alternative subplans.
 *
 * Note: in future we might consider changing to different subplans on the
 * fly, in case the original rowcount estimate turns out to be way off.
 */
Datum
ExecAlternativeSubPlan(AlternativeSubPlanState *node, ExprContext *econtext, bool *isNull)
{




}