/*-------------------------------------------------------------------------
 *
 * execExpr.c
 *	  Expression evaluation infrastructure.
 *
 *	During executor startup, we compile each expression tree (which has
 *	previously been processed by the parser and planner) into an ExprState,
 *	using ExecInitExpr() et al.  This converts the tree into a flat array
 *	of ExprEvalSteps, which may be thought of as instructions in a program.
 *	At runtime, we'll execute steps, starting with the first, until we reach
 *	an EEOP_DONE opcode.
 *
 *	This file contains the "compilation" logic.  It is independent of the
 *	specific execution technology we use (switch statement, computed goto,
 *	JIT compilation, etc).
 *
 *	See src/backend/executor/README for some background, specifically the
 *	"Expression Trees and ExprState nodes", "Expression Initialization",
 *	and "Expression Evaluation" sections.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execExpr.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/nbtree.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_type.h"
#include "executor/execExpr.h"
#include "executor/nodeSubplan.h"
#include "funcapi.h"
#include "jit/jit.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

typedef struct LastAttnumInfo
{
  AttrNumber last_inner;
  AttrNumber last_outer;
  AttrNumber last_scan;
} LastAttnumInfo;

static void
ExecReadyExpr(ExprState *state);
static void
ExecInitExprRec(Expr *node, ExprState *state, Datum *resv, bool *resnull);
static void
ExecInitFunc(ExprEvalStep *scratch, Expr *node, List *args, Oid funcid, Oid inputcollid, ExprState *state);
static void
ExecInitExprSlots(ExprState *state, Node *node);
static void
ExecPushExprSlots(ExprState *state, LastAttnumInfo *info);
static bool
get_last_attnums_walker(Node *node, LastAttnumInfo *info);
static void
ExecComputeSlotInfo(ExprState *state, ExprEvalStep *op);
static void
ExecInitWholeRowVar(ExprEvalStep *scratch, Var *variable, ExprState *state);
static void
ExecInitSubscriptingRef(ExprEvalStep *scratch, SubscriptingRef *sbsref, ExprState *state, Datum *resv, bool *resnull);
static bool
isAssignmentIndirectionExpr(Expr *expr);
static void
ExecInitCoerceToDomain(ExprEvalStep *scratch, CoerceToDomain *ctest, ExprState *state, Datum *resv, bool *resnull);
static void
ExecBuildAggTransCall(ExprState *state, AggState *aggstate, ExprEvalStep *scratch, FunctionCallInfo fcinfo, AggStatePerTrans pertrans, int transno, int setno, int setoff, bool ishash);

/*
 * ExecInitExpr: prepare an expression tree for execution
 *
 * This function builds and returns an ExprState implementing the given
 * Expr node tree.  The return ExprState can then be handed to ExecEvalExpr
 * for execution.  Because the Expr tree itself is read-only as far as
 * ExecInitExpr and ExecEvalExpr are concerned, several different executions
 * of the same plan tree can occur concurrently.  (But note that an ExprState
 * does mutate at runtime, so it can't be re-used concurrently.)
 *
 * This must be called in a memory context that will last as long as repeated
 * executions of the expression are needed.  Typically the context will be
 * the same as the per-query context of the associated ExprContext.
 *
 * Any Aggref, WindowFunc, or SubPlan nodes found in the tree are added to
 * the lists of such nodes held by the parent PlanState (or more accurately,
 * the AggrefExprState etc. nodes created for them are added).
 *
 * Note: there is no ExecEndExpr function; we assume that any resource
 * cleanup needed will be handled by just releasing the memory context
 * in which the state tree is built.  Functions that require additional
 * cleanup work can register a shutdown callback in the ExprContext.
 *
 *	'node' is the root of the expression tree to compile.
 *	'parent' is the PlanState node that owns the expression.
 *
 * 'parent' may be NULL if we are preparing an expression that is not
 * associated with a plan tree.  (If so, it can't have aggs or subplans.)
 * Such cases should usually come through ExecPrepareExpr, not directly here.
 *
 * Also, if 'node' is NULL, we just return NULL.  This is convenient for some
 * callers that may or may not have an expression that needs to be compiled.
 * Note that a NULL ExprState pointer *cannot* be handed to ExecEvalExpr,
 * although ExecQual and ExecCheck will accept one (and treat it as "true").
 */
ExprState *
ExecInitExpr(Expr *node, PlanState *parent)
{




























}

/*
 * ExecInitExprWithParams: prepare a standalone expression tree for execution
 *
 * This is the same as ExecInitExpr, except that there is no parent PlanState,
 * and instead we may have a ParamListInfo describing PARAM_EXTERN Params.
 */
ExprState *
ExecInitExprWithParams(Expr *node, ParamListInfo ext_params)
{




























}

/*
 * ExecInitQual: prepare a qual for execution by ExecQual
 *
 * Prepares for the evaluation of a conjunctive boolean expression (qual list
 * with implicit AND semantics) that returns true if none of the
 * subexpressions are false.
 *
 * We must return true if the list is empty.  Since that's a very common case,
 * we optimize it a bit further by translating to a NULL ExprState pointer
 * rather than setting up an ExprState that computes constant TRUE.  (Some
 * especially hot-spot callers of ExecQual detect this and avoid calling
 * ExecQual at all.)
 *
 * If any of the subexpressions yield NULL, then the result of the conjunction
 * is false.  This makes ExecQual primarily useful for evaluating WHERE
 * clauses, since SQL specifies that tuples with null WHERE results do not
 * get selected.
 */
ExprState *
ExecInitQual(List *qual, PlanState *parent)
{









































































}

/*
 * ExecInitCheck: prepare a check constraint for execution by ExecCheck
 *
 * This is much like ExecInitQual/ExecQual, except that a null result from
 * the conjunction is treated as TRUE.  This behavior is appropriate for
 * evaluating CHECK constraints, since SQL specifies that NULL constraint
 * conditions are not failures.
 *
 * Note that like ExecInitQual, this expects input in implicit-AND format.
 * Users of ExecCheck that have expressions in normal explicit-AND format
 * can just apply ExecInitExpr to produce suitable input for ExecCheck.
 */
ExprState *
ExecInitCheck(List *qual, PlanState *parent)
{














}

/*
 * Call ExecInitExpr() on a list of expressions, return a list of ExprStates.
 */
List *
ExecInitExprList(List *nodes, PlanState *parent)
{











}

/*
 *		ExecBuildProjectionInfo
 *
 * Build a ProjectionInfo node for evaluating the given tlist in the given
 * econtext, and storing the result into the tuple slot.  (Caller must have
 * ensured that tuple slot has a descriptor matching the tlist!)
 *
 * inputDesc can be NULL, but if it is not, we check to see whether simple
 * Vars in the tlist match the descriptor.  It is important to provide
 * inputDesc for relation-scan plan nodes, as a cross check that the relation
 * hasn't been changed since the plan was made.  At higher levels of a plan,
 * there is no need to recheck.
 *
 * This is implemented by internally building an ExprState that performs the
 * whole projection in one go.
 *
 * Caution: before PG v10, the targetList was a list of ExprStates; now it
 * should be the planner-created targetlist, since we do the compilation here.
 */
ProjectionInfo *
ExecBuildProjectionInfo(List *targetList, ExprContext *econtext, TupleTableSlot *slot, PlanState *parent, TupleDesc inputDesc)
{

}

/*
 *		ExecBuildProjectionInfoExt
 *
 * As above, with one additional option.
 *
 * If assignJunkEntries is true (the usual case), resjunk entries in the tlist
 * are not handled specially: they are evaluated and assigned to the proper
 * column of the result slot.  If assignJunkEntries is false, resjunk entries
 * are evaluated, but their result is discarded without assignment.
 */
ProjectionInfo *
ExecBuildProjectionInfoExt(List *targetList, ExprContext *econtext, TupleTableSlot *slot, bool assignJunkEntries, PlanState *parent, TupleDesc inputDesc)
{
































































































































}

/*
 * ExecPrepareExpr --- initialize for expression execution outside a normal
 * Plan tree context.
 *
 * This differs from ExecInitExpr in that we don't assume the caller is
 * already running in the EState's per-query context.  Also, we run the
 * passed expression tree through expression_planner() to prepare it for
 * execution.  (In ordinary Plan trees the regular planning process will have
 * made the appropriate transformations on expressions, but for standalone
 * expressions this won't have happened.)
 */
ExprState *
ExecPrepareExpr(Expr *node, EState *estate)
{












}

/*
 * ExecPrepareQual --- initialize for qual execution outside a normal
 * Plan tree context.
 *
 * This differs from ExecInitQual in that we don't assume the caller is
 * already running in the EState's per-query context.  Also, we run the
 * passed expression tree through expression_planner() to prepare it for
 * execution.  (In ordinary Plan trees the regular planning process will have
 * made the appropriate transformations on expressions, but for standalone
 * expressions this won't have happened.)
 */
ExprState *
ExecPrepareQual(List *qual, EState *estate)
{












}

/*
 * ExecPrepareCheck -- initialize check constraint for execution outside a
 * normal Plan tree context.
 *
 * See ExecPrepareExpr() and ExecInitCheck() for details.
 */
ExprState *
ExecPrepareCheck(List *qual, EState *estate)
{












}

/*
 * Call ExecPrepareExpr() on each member of a list of Exprs, and return
 * a list of ExprStates.
 *
 * See ExecPrepareExpr() for details.
 */
List *
ExecPrepareExprList(List *nodes, EState *estate)
{

















}

/*
 * ExecCheck - evaluate a check constraint
 *
 * For check constraints, a null result is taken as TRUE, ie the constraint
 * passes.
 *
 * The check constraint may have been prepared with ExecInitCheck
 * (possibly via ExecPrepareCheck) if the caller had it in implicit-AND
 * format, but a regular boolean expression prepared with ExecInitExpr or
 * ExecPrepareExpr works too.
 */
bool
ExecCheck(ExprState *state, ExprContext *econtext)
{




















}

/*
 * Prepare a compiled expression for execution.  This has to be called for
 * every ExprState before it can be executed.
 *
 * NB: While this currently only calls ExecReadyInterpretedExpr(),
 * this will likely get extended to further expression evaluation methods.
 * Therefore this should be used instead of directly calling
 * ExecReadyInterpretedExpr().
 */
static void
ExecReadyExpr(ExprState *state)
{






}

/*
 * Append the steps necessary for the evaluation of node to ExprState->steps,
 * possibly recursing into sub-expressions of node.
 *
 * node - expression to evaluate
 * state - ExprState to whose ->steps to append the necessary operations
 * resv / resnull - where to store the result of the node into
 */
static void
ExecInitExprRec(Expr *node, ExprState *state, Datum *resv, bool *resnull)
{





















































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































}

/*
 * Add another expression evaluation step to ExprState->steps.
 *
 * Note that this potentially re-allocates es->steps, therefore no pointer
 * into that array may be used while the expression is still being built.
 */
void
ExprEvalPushStep(ExprState *es, const ExprEvalStep *s)
{












}

/*
 * Perform setup necessary for the evaluation of a function-like expression,
 * appending argument evaluation steps to the steps list in *state, and
 * setting up *scratch so it is ready to be pushed.
 *
 * *scratch is not pushed here, so that callers may override the opcode,
 * which is useful for function-like cases like DISTINCT.
 */
static void
ExecInitFunc(ExprEvalStep *scratch, Expr *node, List *args, Oid funcid, Oid inputcollid, ExprState *state)
{
































































































}

/*
 * Add expression steps deforming the ExprState's inner/outer/scan slots
 * as much as required by the expression.
 */
static void
ExecInitExprSlots(ExprState *state, Node *node)
{








}

/*
 * Add steps deforming the ExprState's inner/out/scan slots as much as
 * indicated by info. This is useful when building an ExprState covering more
 * than one expression.
 */
static void
ExecPushExprSlots(ExprState *state, LastAttnumInfo *info)
{




































}

/*
 * get_last_attnums_walker: expression walker for ExecInitExprSlots
 */
static bool
get_last_attnums_walker(Node *node, LastAttnumInfo *info)
{















































}

/*
 * Compute additional information for EEOP_*_FETCHSOME ops.
 *
 * The goal is to determine whether a slot is 'fixed', that is, every
 * evaluation of the expression will have the same type of slot, with an
 * equivalent descriptor.
 */
static void
ExecComputeSlotInfo(ExprState *state, ExprEvalStep *op)
{


















































































}

/*
 * Prepare step for the evaluation of a whole-row variable.
 * The caller still has to push the step.
 */
static void
ExecInitWholeRowVar(ExprEvalStep *scratch, Var *variable, ExprState *state)
{






























































}

/*
 * Prepare evaluation of a SubscriptingRef expression.
 */
static void
ExecInitSubscriptingRef(ExprEvalStep *scratch, SubscriptingRef *sbsref, ExprState *state, Datum *resv, bool *resnull)
{




















































































































































































}

/*
 * Helper for preparing SubscriptingRef expressions for evaluation: is expr
 * a nested FieldStore or SubscriptingRef that needs the old element value
 * passed down?
 *
 * (We could use this in FieldStore too, but in that case passing the old
 * value is so cheap there's no need.)
 *
 * Note: it might seem that this needs to recurse, but in most cases it does
 * not; the CaseTestExpr, if any, will be directly the arg or refexpr of the
 * top-level node.  Nested-assignment situations give rise to expression
 * trees in which each level of assignment has its own CaseTestExpr, and the
 * recursive structure appears within the newvals or refassgnexpr field.
 * There is an exception, though: if the array is an array-of-domain, we will
 * have a CoerceToDomain as the refassgnexpr, and we need to be able to look
 * through that.
 */
static bool
isAssignmentIndirectionExpr(Expr *expr)
{





























}

/*
 * Prepare evaluation of a CoerceToDomain expression.
 */
static void
ExecInitCoerceToDomain(ExprEvalStep *scratch, CoerceToDomain *ctest, ExprState *state, Datum *resv, bool *resnull)
{




























































































































}

/*
 * Build transition/combine function invocations for all aggregate transition
 * / combination function invocations in a grouping sets phase. This has to
 * invoke all sort based transitions in a phase (if doSort is true), all hash
 * based transitions (if doHash is true), or both (both true).
 *
 * The resulting expression will, for each set of transition values, first
 * check for filters, evaluate aggregate input, check that that input is not
 * NULL for a strict transition function, and then finally invoke the
 * transition for each of the concurrently computed grouping sets.
 */
ExprState *
ExecBuildAggTrans(AggState *aggstate, AggStatePerPhase phase, bool doSort, bool doHash)
{
























































































































































































































































































}

/*
 * Build transition/combine function invocation for a single transition
 * value. This is separated from ExecBuildAggTrans() because there are
 * multiple callsites (hash and sort in some grouping set cases).
 */
static void
ExecBuildAggTransCall(ExprState *state, AggState *aggstate, ExprEvalStep *scratch, FunctionCallInfo fcinfo, AggStatePerTrans pertrans, int transno, int setno, int setoff, bool ishash)
{































































































}

/*
 * Build equality expression that can be evaluated using ExecQual(), returning
 * true if the expression context's inner/outer tuple are NOT DISTINCT. I.e
 * two nulls match, a null and a not-null don't match.
 *
 * desc: tuple descriptor of the to-be-compared tuples
 * numCols: the number of attributes to be examined
 * keyColIdx: array of attribute column numbers
 * eqFunctions: array of function oids of the equality functions to use
 * parent: parent executor node
 */
ExprState *
ExecBuildGroupingEqual(TupleDesc ldesc, TupleDesc rdesc, const TupleTableSlotOps *lops, const TupleTableSlotOps *rops, int numCols, const AttrNumber *keyColIdx, const Oid *eqfunctions, const Oid *collations, PlanState *parent)
{








































































































































}