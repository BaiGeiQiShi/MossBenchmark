/*-------------------------------------------------------------------------
 *
 * execExprInterp.c
 *	  Interpreted evaluation of an expression step list.
 *
 * This file provides either a "direct threaded" (for gcc, clang and
 * compatible) or a "switch threaded" (for all compilers) implementation of
 * expression evaluation.  The former is amongst the fastest known methods
 * of interpreting programs without resorting to assembly level work, or
 * just-in-time compilation, but it requires support for computed gotos.
 * The latter is amongst the fastest approaches doable in standard C.
 *
 * In either case we use ExprEvalStep->opcode to dispatch to the code block
 * within ExecInterpExpr() that implements the specific opcode type.
 *
 * Switch-threading uses a plain switch() statement to perform the
 * dispatch.  This has the advantages of being plain C and allowing the
 * compiler to warn if implementation of a specific opcode has been forgotten.
 * The disadvantage is that dispatches will, as commonly implemented by
 * compilers, happen from a single location, requiring more jumps and causing
 * bad branch prediction.
 *
 * In direct threading, we use gcc's label-as-values extension - also adopted
 * by some other compilers - to replace ExprEvalStep->opcode with the address
 * of the block implementing the instruction. Dispatch to the next instruction
 * is done by a "computed goto".  This allows for better branch prediction
 * (as the jumps are happening from different locations) and fewer jumps
 * (as no preparatory jump to a common dispatch location is needed).
 *
 * When using direct threading, ExecReadyInterpretedExpr will replace
 * each step's opcode field with the address of the relevant code block and
 * ExprState->flags will contain EEO_FLAG_DIRECT_THREADED to remember that
 * that's been done.
 *
 * For very simple instructions the overhead of the full interpreter
 * "startup", as minimal as it is, is noticeable.  Therefore
 * ExecReadyInterpretedExpr will choose to implement certain simple
 * opcode patterns using special fast-path routines (ExecJust*).
 *
 * Complex or uncommon instructions are not implemented in-line in
 * ExecInterpExpr(), rather we call out to a helper function appearing later
 * in this file.  For one reason, there'd not be a noticeable performance
 * benefit, but more importantly those complex routines are intended to be
 * shared between different expression evaluation approaches.  For instance
 * a JIT compiler would generate calls to them.  (This is why they are
 * exported rather than being "static" in this file.)
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/execExprInterp.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "commands/sequence.h"
#include "executor/execExpr.h"
#include "executor/nodeSubplan.h"
#include "funcapi.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datum.h"
#include "utils/expandedrecord.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"
#include "utils/xml.h"

/*
 * Use computed-goto-based opcode dispatch when computed gotos are available.
 * But use a separate symbol so that it's easy to adjust locally in this file
 * for development and testing.
 */
#ifdef HAVE_COMPUTED_GOTO
#define EEO_USE_COMPUTED_GOTO
#endif /* HAVE_COMPUTED_GOTO */

/*
 * Macros for opcode dispatch.
 *
 * EEO_SWITCH - just hides the switch if not in use.
 * EEO_CASE - labels the implementation of named expression step type.
 * EEO_DISPATCH - jump to the implementation of the step type for 'op'.
 * EEO_OPCODE - compute opcode required by used expression evaluation method.
 * EEO_NEXT - increment 'op' and jump to correct next step type.
 * EEO_JUMP - jump to the specified step number within the current expression.
 */
#if defined(EEO_USE_COMPUTED_GOTO)

/* struct for jump target -> opcode lookup table */
typedef struct ExprEvalOpLookup
{
  const void *opcode;
  ExprEvalOp op;
} ExprEvalOpLookup;

/* to make dispatch_table accessible outside ExecInterpExpr() */
static const void **dispatch_table = NULL;

/* jump target -> opcode lookup table */
static ExprEvalOpLookup reverse_dispatch_table[EEOP_LAST];

#define EEO_SWITCH()
#define EEO_CASE(name) CASE_##name:
#define EEO_DISPATCH() goto *((void *)op->opcode)
#define EEO_OPCODE(opcode) ((intptr_t)dispatch_table[opcode])

#else /* !EEO_USE_COMPUTED_GOTO */

#define EEO_SWITCH()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  starteval:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  switch ((ExprEvalOp)op->opcode)
#define EEO_CASE(name) case name:
#define EEO_DISPATCH() goto starteval
#define EEO_OPCODE(opcode) (opcode)

#endif /* EEO_USE_COMPUTED_GOTO */

#define EEO_NEXT()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    op++;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    EEO_DISPATCH();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

#define EEO_JUMP(stepno)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    op = &state->steps[stepno];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    EEO_DISPATCH();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

static Datum
ExecInterpExpr(ExprState *state, ExprContext *econtext, bool *isnull);
static void
ExecInitInterpreter(void);

/* support functions */
static void
CheckVarSlotCompatibility(TupleTableSlot *slot, int attnum, Oid vartype);
static void
CheckOpSlotCompatibility(ExprEvalStep *op, TupleTableSlot *slot);
static TupleDesc
get_cached_rowtype(Oid type_id, int32 typmod, ExprEvalRowtypeCache *rowcache, bool *changed);
static void
ExecEvalRowNullInt(ExprState *state, ExprEvalStep *op, ExprContext *econtext, bool checkisnull);

/* fast-path evaluation functions */
static Datum
ExecJustInnerVar(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustOuterVar(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustScanVar(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustConst(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustAssignInnerVar(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustAssignOuterVar(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustAssignScanVar(ExprState *state, ExprContext *econtext, bool *isnull);
static Datum
ExecJustApplyFuncToCase(ExprState *state, ExprContext *econtext, bool *isnull);

/*
 * Prepare ExprState for interpreted execution.
 */
void
ExecReadyInterpretedExpr(ExprState *state)
{












































































































}

/*
 * Evaluate expression identified by "state" in the execution context
 * given by "econtext".  *isnull is set to the is-null flag for the result,
 * and the Datum value is the function result.
 *
 * As a special case, return the dispatch table's address if state is NULL.
 * This is used by ExecInitInterpreter to set up the dispatch_table global.
 * (Only applies when EEO_USE_COMPUTED_GOTO is defined.)
 */
static Datum
ExecInterpExpr(ExprState *state, ExprContext *econtext, bool *isnull)
{









































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































}

/*
 * Expression evaluation callback that performs extra checks before executing
 * the expression. Declared extern so other methods of execution can use it
 * too.
 */
Datum
ExecInterpExprStillValid(ExprState *state, ExprContext *econtext, bool *isNull)
{











}

/*
 * Check that an expression is still valid in the face of potential schema
 * changes since the plan has been created.
 */
void
CheckExprStillValid(ExprState *state, ExprContext *econtext)
{










































}

/*
 * Check whether a user attribute in a slot can be referenced by a Var
 * expression.  This should succeed unless there have been schema changes
 * since the expression tree has been created.
 */
static void
CheckVarSlotCompatibility(TupleTableSlot *slot, int attnum, Oid vartype)
{







































}

/*
 * Verify that the slot is compatible with a EEOP_*_FETCHSOME operation.
 */
static void
CheckOpSlotCompatibility(ExprEvalStep *op, TupleTableSlot *slot)
{































}

/*
 * get_cached_rowtype: utility function to lookup a rowtype tupdesc
 *
 * type_id, typmod: identity of the rowtype
 * rowcache: space for caching identity info
 *		(rowcache->cacheptr must be initialized to NULL)
 * changed: if not NULL, *changed is set to true on any update
 *
 * The returned TupleDesc is not guaranteed pinned; caller must pin it
 * to use it across any operation that might incur cache invalidation.
 * (The TupleDesc is always refcounted, so just use IncrTupleDescRefCount.)
 *
 * NOTE: because composite types can change contents, we must be prepared
 * to re-do this during any node execution; cannot call just once during
 * expression initialization.
 */
static TupleDesc
get_cached_rowtype(Oid type_id, int32 typmod, ExprEvalRowtypeCache *rowcache, bool *changed)
{



















































}

/*
 * Fast-path functions, for very simple expressions
 */

/* Simple reference to inner Var */
static Datum
ExecJustInnerVar(ExprState *state, ExprContext *econtext, bool *isnull)
{












}

/* Simple reference to outer Var */
static Datum
ExecJustOuterVar(ExprState *state, ExprContext *econtext, bool *isnull)
{








}

/* Simple reference to scan Var */
static Datum
ExecJustScanVar(ExprState *state, ExprContext *econtext, bool *isnull)
{








}

/* Simple Const expression */
static Datum
ExecJustConst(ExprState *state, ExprContext *econtext, bool *isnull)
{




}

/* Evaluate inner Var and assign to appropriate column of result tuple */
static Datum
ExecJustAssignInnerVar(ExprState *state, ExprContext *econtext, bool *isnull)
{




















}

/* Evaluate outer Var and assign to appropriate column of result tuple */
static Datum
ExecJustAssignOuterVar(ExprState *state, ExprContext *econtext, bool *isnull)
{












}

/* Evaluate scan Var and assign to appropriate column of result tuple */
static Datum
ExecJustAssignScanVar(ExprState *state, ExprContext *econtext, bool *isnull)
{












}

/* Evaluate CASE_TESTVAL and apply a strict function to it */
static Datum
ExecJustApplyFuncToCase(ExprState *state, ExprContext *econtext, bool *isnull)
{































}

#if defined(EEO_USE_COMPUTED_GOTO)
/*
 * Comparator used when building address->opcode lookup table for
 * ExecEvalStepOp() in the threaded dispatch case.
 */
static int
dispatch_compare_ptr(const void *a, const void *b)
{












}
#endif

/*
 * Do one-time initialization of interpretation machinery.
 */
static void
ExecInitInterpreter(void)
{



















}

/*
 * Function to return the opcode of an expression step.
 *
 * When direct-threading is in use, ExprState->opcode isn't easily
 * decipherable. This function returns the appropriate enum member.
 */
ExprEvalOp
ExecEvalStepOp(ExprState *state, ExprEvalStep *op)
{













}

/*
 * Out-of-line helper functions for complex instructions.
 */

/*
 * Evaluate EEOP_FUNCEXPR_FUSAGE
 */
void
ExecEvalFuncExprFusage(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{












}

/*
 * Evaluate EEOP_FUNCEXPR_STRICT_FUSAGE
 */
void
ExecEvalFuncExprStrictFusage(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{

























}

/*
 * Evaluate a PARAM_EXEC parameter.
 *
 * PARAM_EXEC params (internal executor parameters) are stored in the
 * ecxt_param_exec_vals array, and can be accessed by array index.
 */
void
ExecEvalParamExec(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{












}

/*
 * Evaluate a PARAM_EXTERN parameter.
 *
 * PARAM_EXTERN parameters must be sought in ecxt_param_list_info.
 */
void
ExecEvalParamExtern(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{
































}

/*
 * Evaluate a SQLValueFunction expression.
 */
void
ExecEvalSQLValueFunction(ExprState *state, ExprEvalStep *op)
{





















































}

/*
 * Raise error if a CURRENT OF expression is evaluated.
 *
 * The planner should convert CURRENT OF into a TidScan qualification, or some
 * other special handling in a ForeignScan node.  So we have to be able to do
 * ExecInitExpr on a CurrentOfExpr, but we shouldn't ever actually execute it.
 * If we get here, we suppose we must be dealing with CURRENT OF on a foreign
 * table whose FDW doesn't handle it, and complain accordingly.
 */
void
ExecEvalCurrentOfExpr(ExprState *state, ExprEvalStep *op)
{

}

/*
 * Evaluate NextValueExpr.
 */
void
ExecEvalNextValueExpr(ExprState *state, ExprEvalStep *op)
{

















}

/*
 * Evaluate NullTest / IS NULL for rows.
 */
void
ExecEvalRowNull(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{

}

/*
 * Evaluate NullTest / IS NOT NULL for rows.
 */
void
ExecEvalRowNotNull(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{

}

/* Common code for IS [NOT] NULL on a row value */
static void
ExecEvalRowNullInt(ExprState *state, ExprEvalStep *op, ExprContext *econtext, bool checkisnull)
{












































































}

/*
 * Evaluate an ARRAY[] expression.
 *
 * The individual array elements (or subarrays) have already been evaluated
 * into op->d.arrayexpr.elemvalues[]/elemnulls[].
 */
void
ExecEvalArrayExpr(ExprState *state, ExprEvalStep *op)
{























































































































































































}

/*
 * Evaluate an ArrayCoerceExpr expression.
 *
 * Source array is in step's result variable.
 */
void
ExecEvalArrayCoerce(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{




























}

/*
 * Evaluate a ROW() expression.
 *
 * The individual columns have already been evaluated into
 * op->d.row.elemvalues[]/elemnulls[].
 */
void
ExecEvalRow(ExprState *state, ExprEvalStep *op)
{







}

/*
 * Evaluate GREATEST() or LEAST() expression (note this is *not* MIN()/MAX()).
 *
 * All of the to-be-compared expressions have already been evaluated into
 * op->d.minmax.values[]/nulls[].
 */
void
ExecEvalMinMax(ExprState *state, ExprEvalStep *op)
{




















































}

/*
 * Evaluate a FieldSelect node.
 *
 * Source record is in step's result variable.
 */
void
ExecEvalFieldSelect(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{










































































































}

/*
 * Deform source tuple, filling in the step's values/nulls arrays, before
 * evaluating individual new values as part of a FieldStore expression.
 * Subsequent steps will overwrite individual elements of the values/nulls
 * arrays with the new field values, and then FIELDSTORE_FORM will build the
 * new tuple value.
 *
 * Source record is in step's result variable.
 */
void
ExecEvalFieldStoreDeForm(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{


































}

/*
 * Compute the new composite datum after each individual field value of a
 * FieldStore expression has been evaluated.
 */
void
ExecEvalFieldStoreForm(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{










}

/*
 * Process a subscript in a SubscriptingRef expression.
 *
 * If subscript is NULL, throw error in assignment case, or in fetch case
 * set result to NULL and return false (instructing caller to skip the rest
 * of the SubscriptingRef sequence).
 *
 * Subscript expression result is in subscriptvalue/subscriptnull.
 * On success, integer subscript value has been saved in upperindex[] or
 * lowerindex[] for use later.
 */
bool
ExecEvalSubscriptingRef(ExprState *state, ExprEvalStep *op)
{





























}

/*
 * Evaluate SubscriptingRef fetch.
 *
 * Source container is in step's result variable.
 */
void
ExecEvalSubscriptingRefFetch(ExprState *state, ExprEvalStep *op)
{















}

/*
 * Compute old container element/slice value for a SubscriptingRef assignment
 * expression. Will only be generated if the new-value subexpression
 * contains SubscriptingRef or FieldStore. The value is stored into the
 * SubscriptingRefState's prevvalue/prevnull fields.
 */
void
ExecEvalSubscriptingRefOld(ExprState *state, ExprEvalStep *op)
{




















}

/*
 * Evaluate SubscriptingRef assignment.
 *
 * Input container (possibly null) is in result area, replacement value is in
 * SubscriptingRefState's replacevalue/replacenull.
 */
void
ExecEvalSubscriptingRefAssign(ExprState *state, ExprEvalStep *op)
{





































}

/*
 * Evaluate a rowtype coercion operation.
 * This may require rearranging field positions.
 *
 * Source record is in step's result variable.
 */
void
ExecEvalConvertRowtype(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{












































































}

/*
 * Evaluate "scalar op ANY/ALL (array)".
 *
 * Source array is in our result area, scalar arg is already evaluated into
 * fcinfo->args[0].
 *
 * The operator always yields boolean, and we combine the results across all
 * array elements using OR and AND (for ANY and ALL respectively).  Of course
 * we short-circuit as soon as the result is known.
 */
void
ExecEvalScalarArrayOp(ExprState *state, ExprEvalStep *op)
{
















































































































































}

/*
 * Evaluate a NOT NULL domain constraint.
 */
void
ExecEvalConstraintNotNull(ExprState *state, ExprEvalStep *op)
{




}

/*
 * Evaluate a CHECK domain constraint.
 */
void
ExecEvalConstraintCheck(ExprState *state, ExprEvalStep *op)
{




}

/*
 * Evaluate the various forms of XmlExpr.
 *
 * Arguments have been evaluated into named_argvalue/named_argnull
 * and/or argvalue/argnull arrays.
 */
void
ExecEvalXmlExpr(ExprState *state, ExprEvalStep *op)
{
















































































































































































































}

/*
 * ExecEvalGroupingFunc
 *
 * Computes a bitmask with a bit for each (unevaluated) argument expression
 * (rightmost arg is least significant bit).
 *
 * A bit is set if the corresponding expression is NOT part of the set of
 * grouping expressions in the current grouping set.
 */
void
ExecEvalGroupingFunc(ExprState *state, ExprEvalStep *op)
{


















}

/*
 * Hand off evaluation of a subplan to nodeSubplan.c
 */
void
ExecEvalSubPlan(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{






}

/*
 * Hand off evaluation of an alternative subplan to nodeSubplan.c
 */
void
ExecEvalAlternativeSubPlan(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{






}

/*
 * Evaluate a wholerow Var expression.
 *
 * Returns a Datum whose value is the value of a whole-row range variable
 * with respect to given expression context.
 */
void
ExecEvalWholeRowVar(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{


























































































































































































































}

void
ExecEvalSysVar(ExprState *state, ExprEvalStep *op, ExprContext *econtext, TupleTableSlot *slot)
{










}

/*
 * Transition value has not been initialized. This is the first non-NULL input
 * value for a group. We use it as the initial value for transValue.
 */
void
ExecAggInitGroup(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroup)
{














}

/*
 * Ensure that the current transition value is a child of the aggcontext,
 * rather than the per-tuple context.
 *
 * NB: This can change the current memory context.
 */
Datum
ExecAggTransReparent(AggState *aggstate, AggStatePerTrans pertrans, Datum newValue, bool newValueIsNull, Datum oldValue, bool oldValueIsNull)
{



































}

/*
 * Invoke ordered transition function, with a datum argument.
 */
void
ExecEvalAggOrderedTransDatum(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{




}

/*
 * Invoke ordered transition function, with a tuple argument.
 */
void
ExecEvalAggOrderedTransTuple(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{







}