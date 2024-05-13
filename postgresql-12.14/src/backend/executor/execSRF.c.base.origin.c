/*-------------------------------------------------------------------------
 *
 * execSRF.c
 *	  Routines implementing the API for set-returning functions
 *
 * This file serves nodeFunctionscan.c and nodeProjectSet.c, providing
 * common code for calling set-returning functions according to the
 * ReturnSetInfo API.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execSRF.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/objectaccess.h"
#include "executor/execdebug.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "pgstat.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

/* static function decls */
static void
init_sexpr(Oid foid, Oid input_collation, Expr *node, SetExprState *sexpr, PlanState *parent, MemoryContext sexprCxt, bool allowSRF, bool needDescForSRF);
static void
ShutdownSetExpr(Datum arg);
static void
ExecEvalFuncArgs(FunctionCallInfo fcinfo, List *argList, ExprContext *econtext);
static void
ExecPrepareTuplestoreResult(SetExprState *sexpr, ExprContext *econtext, Tuplestorestate *resultStore, TupleDesc resultDesc);
static void
tupledesc_match(TupleDesc dst_tupdesc, TupleDesc src_tupdesc);

/*
 * Prepare function call in FROM (ROWS FROM) for execution.
 *
 * This is used by nodeFunctionscan.c.
 */
SetExprState *
ExecInitTableFunctionResult(Expr *expr, ExprContext *econtext, PlanState *parent)
{






























}

/*
 *		ExecMakeTableFunctionResult
 *
 * Evaluate a table function, producing a materialized result in a Tuplestore
 * object.
 *
 * This is used by nodeFunctionscan.c.
 */
Tuplestorestate *
ExecMakeTableFunctionResult(SetExprState *setexpr, ExprContext *econtext, MemoryContext argContext, TupleDesc expectedDesc, bool randomAccess)
{






















































































































































































































































































































}

/*
 * Prepare targetlist SRF function call for execution.
 *
 * This is used by nodeProjectSet.c.
 */
SetExprState *
ExecInitFunctionResultSet(Expr *expr, ExprContext *econtext, PlanState *parent)
{

































}

/*
 *		ExecMakeFunctionResultSet
 *
 * Evaluate the arguments to a set-returning function and then call the
 * function itself.  The argument expressions may not contain set-returning
 * functions (the planner is supposed to have separated evaluation for those).
 *
 * This should be called in a short-lived (per-tuple) context, argContext
 * needs to live until all rows have been returned (i.e. *isDone set to
 * ExprEndResult or ExprSingleResult).
 *
 * This is used by nodeProjectSet.c.
 */
Datum
ExecMakeFunctionResultSet(SetExprState *fcache, ExprContext *econtext, MemoryContext argContext, bool *isNull, ExprDoneCond *isDone)
{



















































































































































































}

/*
 * init_sexpr - initialize a SetExprState node during first use
 */
static void
init_sexpr(Oid foid, Oid input_collation, Expr *node, SetExprState *sexpr, PlanState *parent, MemoryContext sexprCxt, bool allowSRF, bool needDescForSRF)
{



























































































}

/*
 * callback function in case a SetExprState needs to be shut down before it
 * has been run to completion
 */
static void
ShutdownSetExpr(Datum arg)
{




















}

/*
 * Evaluate arguments for a function.
 */
static void
ExecEvalFuncArgs(FunctionCallInfo fcinfo, List *argList, ExprContext *econtext)
{













}

/*
 *		ExecPrepareTuplestoreResult
 *
 * Subroutine for ExecMakeFunctionResultSet: prepare to extract rows from a
 * tuplestore function result.  We must set up a funcResultSlot (unless
 * already done in a previous call cycle) and verify that the function
 * returned the expected tuple descriptor.
 */
static void
ExecPrepareTuplestoreResult(SetExprState *sexpr, ExprContext *econtext, Tuplestorestate *resultStore, TupleDesc resultDesc)
{




























































}

/*
 * Check that function result tuple type (src_tupdesc) matches or can
 * be considered to match what the query expects (dst_tupdesc). If
 * they don't match, ereport.
 *
 * We really only care about number of attributes and data type.
 * Also, we can ignore type mismatch on columns that are dropped in the
 * destination type, so long as the physical storage matches.  This is
 * helpful in some cases involving out-of-date cached plans.
 */
static void
tupledesc_match(TupleDesc dst_tupdesc, TupleDesc src_tupdesc)
{


























}