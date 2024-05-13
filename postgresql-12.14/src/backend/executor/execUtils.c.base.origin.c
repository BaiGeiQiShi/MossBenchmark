/*-------------------------------------------------------------------------
 *
 * execUtils.c
 *	  miscellaneous executor utility routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execUtils.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		CreateExecutorState		Create/delete executor working
 *state FreeExecutorState CreateExprContext CreateStandaloneExprContext
 *		FreeExprContext
 *		ReScanExprContext
 *
 *		ExecAssignExprContext	Common code for plan node init routines.
 *		etc
 *
 *		ExecOpenScanRelation	Common code for scan node init routines.
 *
 *		ExecInitRangeTable		Set up executor's
 *range-table-related data.
 *
 *		ExecGetRangeTableRelation		Fetch Relation for a
 *rangetable entry.
 *
 *		executor_errposition	Report syntactic position of an error.
 *
 *		RegisterExprContextCallback    Register function shutdown
 *callback UnregisterExprContextCallback  Deregister function shutdown callback
 *
 *		GetAttributeByName		Runtime extraction of columns
 *from tuples. GetAttributeByNum
 *
 *	 NOTES
 *		This file has traditionally been the place to stick misc.
 *		executor support stuff that doesn't really go anyplace else.
 */

#include "postgres.h"

#include "access/parallel.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "executor/executor.h"
#include "executor/execPartition.h"
#include "jit/jit.h"
#include "mb/pg_wchar.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/typcache.h"

static bool
tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc);
static void
ShutdownExprContext(ExprContext *econtext, bool isCommit);

/* ----------------------------------------------------------------
 *				 Executor state and memory management functions
 * ----------------------------------------------------------------
 */

/* ----------------
 *		CreateExecutorState
 *
 *		Create and initialize an EState node, which is the root of
 *		working storage for an entire Executor invocation.
 *
 * Principally, this creates the per-query memory context that will be
 * used to hold all working data that lives till the end of the query.
 * Note that the per-query context will become a child of the caller's
 * CurrentMemoryContext.
 * ----------------
 */
EState *
CreateExecutorState(void)
{

















































































}

/* ----------------
 *		FreeExecutorState
 *
 *		Release an EState along with all remaining working storage.
 *
 * Note: this is not responsible for releasing non-memory resources, such as
 * open relations or buffer pins.  But it will shut down any still-active
 * ExprContexts within the EState and deallocate associated JITed expressions.
 * That is sufficient cleanup for situations where the EState has only been
 * used for expression evaluation, and not to run a complete Plan.
 *
 * This can be called in any memory context ... so long as it's not one
 * of the ones to be freed.
 * ----------------
 */
void
FreeExecutorState(EState *estate)
{



































}

/* ----------------
 *		CreateExprContext
 *
 *		Create a context for expression evaluation within an EState.
 *
 * An executor run may require multiple ExprContexts (we usually make one
 * for each Plan node, and a separate one for per-output-tuple processing
 * such as constraint checking).  Each ExprContext has its own "per-tuple"
 * memory context.
 *
 * Note we make no assumption about the caller's memory context.
 * ----------------
 */
ExprContext *
CreateExprContext(EState *estate)
{














































}

/* ----------------
 *		CreateStandaloneExprContext
 *
 *		Create a context for standalone expression evaluation.
 *
 * An ExprContext made this way can be used for evaluation of expressions
 * that contain no Params, subplans, or Var references (it might work to
 * put tuple references into the scantuple field, but it seems unwise).
 *
 * The ExprContext struct is allocated in the caller's current memory
 * context, which also becomes its "per query" context.
 *
 * It is caller's responsibility to free the ExprContext when done,
 * or at least ensure that any shutdown callbacks have been called
 * (ReScanExprContext() is suitable).  Otherwise, non-memory resources
 * might be leaked.
 * ----------------
 */
ExprContext *
CreateStandaloneExprContext(void)
{


































}

/* ----------------
 *		FreeExprContext
 *
 *		Free an expression context, including calling any remaining
 *		shutdown callbacks.
 *
 * Since we free the temporary context used for expression evaluation,
 * any previously computed pass-by-reference expression result will go away!
 *
 * If isCommit is false, we are being called in error cleanup, and should
 * not call callbacks but only release memory.  (It might be better to call
 * the callbacks and pass the isCommit flag to them, but that would require
 * more invasive code changes than currently seems justified.)
 *
 * Note we make no assumption about the caller's memory context.
 * ----------------
 */
void
FreeExprContext(ExprContext *econtext, bool isCommit)
{














}

/*
 * ReScanExprContext
 *
 *		Reset an expression context in preparation for a rescan of its
 *		plan node.  This requires calling any registered shutdown
 *callbacks, since any partially complete set-returning-functions must be
 *canceled.
 *
 * Note we make no assumption about the caller's memory context.
 */
void
ReScanExprContext(ExprContext *econtext)
{




}

/*
 * Build a per-output-tuple ExprContext for an EState.
 *
 * This is normally invoked via GetPerTupleExprContext() macro,
 * not directly.
 */
ExprContext *
MakePerTupleExprContext(EState *estate)
{






}

/* ----------------------------------------------------------------
 *				 miscellaneous node-init support functions
 *
 * Note: all of these are expected to be called with CurrentMemoryContext
 * equal to the per-query memory context.
 * ----------------------------------------------------------------
 */

/* ----------------
 *		ExecAssignExprContext
 *
 *		This initializes the ps_ExprContext field.  It is only necessary
 *		to do this for nodes which use ExecQual or ExecProject
 *		because those routines require an econtext. Other nodes that
 *		don't have to evaluate expressions don't need to do this.
 * ----------------
 */
void
ExecAssignExprContext(EState *estate, PlanState *planstate)
{

}

/* ----------------
 *		ExecGetResultType
 * ----------------
 */
TupleDesc
ExecGetResultType(PlanState *planstate)
{

}

/*
 * ExecGetResultSlotOps - information about node's type of result slot
 */
const TupleTableSlotOps *
ExecGetResultSlotOps(PlanState *planstate, bool *isfixed)
{































}

/* ----------------
 *		ExecAssignProjectionInfo
 *
 * forms the projection information from the node's targetlist
 *
 * Notes for inputDesc are same as for ExecBuildProjectionInfo: supply it
 * for a relation-scan node, can pass NULL for upper-level nodes
 * ----------------
 */
void
ExecAssignProjectionInfo(PlanState *planstate, TupleDesc inputDesc)
{

}

/* ----------------
 *		ExecConditionalAssignProjectionInfo
 *
 * as ExecAssignProjectionInfo, but store NULL rather than building projection
 * info if no projection is required
 * ----------------
 */
void
ExecConditionalAssignProjectionInfo(PlanState *planstate, TupleDesc inputDesc, Index varno)
{


















}

static bool
tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc)
{



























































}

/* ----------------
 *		ExecFreeExprContext
 *
 * A plan node's ExprContext should be freed explicitly during executor
 * shutdown because there may be shutdown callbacks to call.  (Other resources
 * made by the above routines, such as projection info, don't need to be freed
 * explicitly because they're just memory in the per-query memory context.)
 *
 * However ... there is no particular need to do it during ExecEndNode,
 * because FreeExecutorState will free any remaining ExprContexts within
 * the EState.  Letting FreeExecutorState do it allows the ExprContexts to
 * be freed in reverse order of creation, rather than order of creation as
 * will happen if we delete them here, which saves O(N^2) work in the list
 * cleanup inside FreeExprContext.
 * ----------------
 */
void
ExecFreeExprContext(PlanState *planstate)
{





}

/* ----------------------------------------------------------------
 *				  Scan node support
 * ----------------------------------------------------------------
 */

/* ----------------
 *		ExecAssignScanType
 * ----------------
 */
void
ExecAssignScanType(ScanState *scanstate, TupleDesc tupDesc)
{



}

/* ----------------
 *		ExecCreateScanSlotFromOuterPlan
 * ----------------
 */
void
ExecCreateScanSlotFromOuterPlan(EState *estate, ScanState *scanstate, const TupleTableSlotOps *tts_ops)
{







}

/* ----------------------------------------------------------------
 *		ExecRelationIsTargetRelation
 *
 *		Detect whether a relation (identified by rangetable index)
 *		is one of the target relations of the query.
 *
 * Note: This is currently no longer used in core.  We keep it around
 * because FDWs may wish to use it to determine if their foreign table
 * is a target relation.
 * ----------------------------------------------------------------
 */
bool
ExecRelationIsTargetRelation(EState *estate, Index scanrelid)
{












}

/* ----------------------------------------------------------------
 *		ExecOpenScanRelation
 *
 *		Open the heap relation to be scanned by a base-level scan plan
 *node. This should be called during the node's ExecInit routine.
 * ----------------------------------------------------------------
 */
Relation
ExecOpenScanRelation(EState *estate, Index scanrelid, int eflags)
{
















}

/*
 * ExecInitRangeTable
 *		Set up executor's range-table-related data
 *
 * We build an array from the range table list to allow faster lookup by RTI.
 * (The es_range_table field is now somewhat redundant, but we keep it to
 * avoid breaking external code unnecessarily.)
 * This is also a convenient place to set up the parallel es_relations array.
 */
void
ExecInitRangeTable(EState *estate, List *rangeTable)
{



























}

/*
 * ExecGetRangeTableRelation
 *		Open the Relation for a range table entry, if not already done
 *
 * The Relations will be closed again in ExecEndPlan().
 */
Relation
ExecGetRangeTableRelation(EState *estate, Index rti)
{






































}

/*
 * UpdateChangedParamSet
 *		Add changed parameters to a plan node's chgParam set
 */
void
UpdateChangedParamSet(PlanState *node, Bitmapset *newchg)
{




















}

/*
 * executor_errposition
 *		Report an execution-time cursor position, if possible.
 *
 * This is expected to be used within an ereport() call.  The return value
 * is a dummy (always 0, in fact).
 *
 * The locations stored in parsetrees are byte offsets into the source string.
 * We have to convert them to 1-based character indexes for reporting to
 * clients.  (We do things this way to avoid unnecessary overhead in the
 * normal non-error case: computing character indexes would be much more
 * expensive than storing token offsets.)
 */
int
executor_errposition(EState *estate, int location)
{
















}

/*
 * Register a shutdown callback in an ExprContext.
 *
 * Shutdown callbacks will be called (in reverse order of registration)
 * when the ExprContext is deleted or rescanned.  This provides a hook
 * for functions called in the context to do any cleanup needed --- it's
 * particularly useful for functions returning sets.  Note that the
 * callback will *not* be called in the event that execution is aborted
 * by an error.
 */
void
RegisterExprContextCallback(ExprContext *econtext, ExprContextCallbackFunction function, Datum arg)
{











}

/*
 * Deregister a shutdown callback in an ExprContext.
 *
 * Any list entries matching the function and arg will be removed.
 * This can be used if it's no longer necessary to call the callback.
 */
void
UnregisterExprContextCallback(ExprContext *econtext, ExprContextCallbackFunction function, Datum arg)
{

















}

/*
 * Call all the shutdown callbacks registered in an ExprContext.
 *
 * The callback list is emptied (important in case this is only a rescan
 * reset, and not deletion of the ExprContext).
 *
 * If isCommit is false, just clean the callback list but don't call 'em.
 * (See comment for FreeExprContext.)
 */
static void
ShutdownExprContext(ExprContext *econtext, bool isCommit)
{





























}

/*
 *		GetAttributeByName
 *		GetAttributeByNum
 *
 *		These functions return the value of the requested attribute
 *		out of the given tuple Datum.
 *		C functions which take a tuple as an argument are expected
 *		to use these.  Ex: overpaid(EMP) might call GetAttributeByNum().
 *		Note: these are actually rather slow because they do a typcache
 *		lookup on each call.
 */
Datum
GetAttributeByName(HeapTupleHeader tuple, const char *attname, bool *isNull)
{





























































}

Datum
GetAttributeByNum(HeapTupleHeader tuple, AttrNumber attrno, bool *isNull)
{










































}

/*
 * Number of items in a tlist (including any resjunk items!)
 */
int
ExecTargetListLength(List *targetlist)
{


}

/*
 * Number of items in a tlist, not including any resjunk items
 */
int
ExecCleanTargetListLength(List *targetlist)
{













}

/*
 * Return a relInfo's tuple slot for a trigger's OLD tuples.
 */
TupleTableSlot *
ExecGetTriggerOldSlot(EState *estate, ResultRelInfo *relInfo)
{











}

/*
 * Return a relInfo's tuple slot for a trigger's NEW tuples.
 */
TupleTableSlot *
ExecGetTriggerNewSlot(EState *estate, ResultRelInfo *relInfo)
{











}

/*
 * Return a relInfo's tuple slot for processing returning tuples.
 */
TupleTableSlot *
ExecGetReturningSlot(EState *estate, ResultRelInfo *relInfo)
{











}

/* Return a bitmap representing columns being inserted */
Bitmapset *
ExecGetInsertedCols(ResultRelInfo *relinfo, EState *estate)
{





































}

/* Return a bitmap representing columns being updated */
Bitmapset *
ExecGetUpdatedCols(ResultRelInfo *relinfo, EState *estate)
{


























}

/* Return a bitmap representing generated columns being updated */
Bitmapset *
ExecGetExtraUpdatedCols(ResultRelInfo *relinfo, EState *estate)
{


























}

/* Return columns being updated, including generated columns */
Bitmapset *
ExecGetAllUpdatedCols(ResultRelInfo *relinfo, EState *estate)
{

}