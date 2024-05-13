/*-------------------------------------------------------------------------
 *
 * funcapi.c
 *	  Utility and convenience functions for fmgr functions that return
 *	  sets and/or composite types, or deal with VARIADIC inputs.
 *
 * Copyright (c) 2002-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/fmgr/funcapi.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "nodes/nodeFuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/regproc.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

typedef struct polymorphic_actuals
{
  Oid anyelement_type; /* anyelement mapping, if known */
  Oid anyarray_type;   /* anyarray mapping, if known */
  Oid anyrange_type;   /* anyrange mapping, if known */
} polymorphic_actuals;

static void
shutdown_MultiFuncCall(Datum arg);
static TypeFuncClass
internal_get_result_type(Oid funcid, Node *call_expr, ReturnSetInfo *rsinfo, Oid *resultTypeId, TupleDesc *resultTupleDesc);
static void
resolve_anyelement_from_others(polymorphic_actuals *actuals);
static void
resolve_anyarray_from_others(polymorphic_actuals *actuals);
static void
resolve_anyrange_from_others(polymorphic_actuals *actuals);
static bool
resolve_polymorphic_tupdesc(TupleDesc tupdesc, oidvector *declared_args, Node *call_expr);
static TypeFuncClass
get_type_func_class(Oid typid, Oid *base_typeid);

/*
 * init_MultiFuncCall
 * Create an empty FuncCallContext data structure
 * and do some other basic Multi-function call setup
 * and error checking
 */
FuncCallContext *
init_MultiFuncCall(PG_FUNCTION_ARGS)
{



























































}

/*
 * per_MultiFuncCall
 *
 * Do Multi-function per-call setup
 */
FuncCallContext *
per_MultiFuncCall(PG_FUNCTION_ARGS)
{



}

/*
 * end_MultiFuncCall
 * Clean up after init_MultiFuncCall
 */
void
end_MultiFuncCall(PG_FUNCTION_ARGS, FuncCallContext *funcctx)
{







}

/*
 * shutdown_MultiFuncCall
 * Shutdown function to clean up after init_MultiFuncCall
 */
static void
shutdown_MultiFuncCall(Datum arg)
{











}

/*
 * get_call_result_type
 *		Given a function's call info record, determine the kind of
 *datatype it is supposed to return.  If resultTypeId isn't NULL, *resultTypeId
 *		receives the actual datatype OID (this is mainly useful for
 *scalar result types).  If resultTupleDesc isn't NULL, *resultTupleDesc
 *		receives a pointer to a TupleDesc when the result is of a
 *composite type, or NULL when it's a scalar result.
 *
 * One hard case that this handles is resolution of actual rowtypes for
 * functions returning RECORD (from either the function's OUT parameter
 * list, or a ReturnSetInfo context node).  TYPEFUNC_RECORD is returned
 * only when we couldn't resolve the actual rowtype for lack of information.
 *
 * The other hard case that this handles is resolution of polymorphism.
 * We will never return polymorphic pseudotypes (ANYELEMENT etc), either
 * as a scalar result type or as a component of a rowtype.
 *
 * This function is relatively expensive --- in a function returning set,
 * try to call it only the first time through.
 */
TypeFuncClass
get_call_result_type(FunctionCallInfo fcinfo, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{

}

/*
 * get_expr_result_type
 *		As above, but work from a calling expression node tree
 */
TypeFuncClass
get_expr_result_type(Node *expr, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{
































}

/*
 * get_func_result_type
 *		As above, but work from a function's OID only
 *
 * This will not be able to resolve pure-RECORD results nor polymorphism.
 */
TypeFuncClass
get_func_result_type(Oid functionId, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{

}

/*
 * internal_get_result_type -- workhorse code implementing all the above
 *
 * funcid must always be supplied.  call_expr and rsinfo can be NULL if not
 * available.  We will return TYPEFUNC_RECORD, and store NULL into
 * *resultTupleDesc, if we cannot deduce the complete result rowtype from
 * the available information.
 */
static TypeFuncClass
internal_get_result_type(Oid funcid, Node *call_expr, ReturnSetInfo *rsinfo, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{

















































































































}

/*
 * get_expr_result_tupdesc
 *		Get a tupdesc describing the result of a composite-valued
 *expression
 *
 * If expression is not composite or rowtype can't be determined, returns NULL
 * if noError is true, else throws error.
 *
 * This is a simpler version of get_expr_result_type() for use when the caller
 * is only interested in determinate rowtype results.
 */
TupleDesc
get_expr_result_tupdesc(Node *expr, bool noError)
{

























}

/*
 * Resolve actual type of ANYELEMENT from other polymorphic inputs
 *
 * Note: the error cases here and in the sibling functions below are not
 * really user-facing; they could only occur if the function signature is
 * incorrect or the parser failed to enforce consistency of the actual
 * argument types.  Hence, we don't sweat too much over the error messages.
 */
static void
resolve_anyelement_from_others(polymorphic_actuals *actuals)
{




























}

/*
 * Resolve actual type of ANYARRAY from other polymorphic inputs
 */
static void
resolve_anyarray_from_others(polymorphic_actuals *actuals)
{





















}

/*
 * Resolve actual type of ANYRANGE from other polymorphic inputs
 */
static void
resolve_anyrange_from_others(polymorphic_actuals *actuals)
{





}

/*
 * Given the result tuple descriptor for a function with OUT parameters,
 * replace any polymorphic column types (ANYELEMENT etc) in the tupdesc
 * with concrete data types deduced from the input arguments.
 * declared_args is an oidvector of the function's declared input arg types
 * (showing which are polymorphic), and call_expr is the call expression.
 * Returns true if able to deduce all types, false if not.
 */
static bool
resolve_polymorphic_tupdesc(TupleDesc tupdesc, oidvector *declared_args, Node *call_expr)
{



































































































































































}

/*
 * Given the declared argument types and modes for a function, replace any
 * polymorphic types (ANYELEMENT etc) in argtypes[] with concrete data types
 * deduced from the input arguments found in call_expr.
 * Returns true if able to deduce all types, false if not.
 *
 * This is the same logic as resolve_polymorphic_tupdesc, but with a different
 * argument representation, and slightly different output responsibilities.
 *
 * argmodes may be NULL, in which case all arguments are assumed to be IN mode.
 */
bool
resolve_polymorphic_argtypes(int numargs, Oid *argtypes, char *argmodes, Node *call_expr)
{





































































































































}

/*
 * get_type_func_class
 *		Given the type OID, obtain its TYPEFUNC classification.
 *		Also, if it's a domain, return the base type OID.
 *
 * This is intended to centralize a bunch of formerly ad-hoc code for
 * classifying types.  The categories used here are useful for deciding
 * how to handle functions returning the datatype.
 */
static TypeFuncClass
get_type_func_class(Oid typid, Oid *base_typeid)
{








































}

/*
 * get_func_arg_info
 *
 * Fetch info about the argument types, names, and IN/OUT modes from the
 * pg_proc tuple.  Return value is the total number of arguments.
 * Other results are palloc'd.  *p_argtypes is always filled in, but
 * *p_argnames and *p_argmodes will be set NULL in the default cases
 * (no names, and all IN arguments, respectively).
 *
 * Note that this function simply fetches what is in the pg_proc tuple;
 * it doesn't do any interpretation of polymorphic types.
 */
int
get_func_arg_info(HeapTuple procTup, Oid **p_argtypes, char ***p_argnames, char **p_argmodes)
{














































































}

/*
 * get_func_trftypes
 *
 * Returns the number of transformed types used by the function.
 * If there are any, a palloc'd array of the type OIDs is returned
 * into *p_trftypes.
 */
int
get_func_trftypes(HeapTuple procTup, Oid **p_trftypes)
{





























}

/*
 * get_func_input_arg_names
 *
 * Extract the names of input arguments only, given a function's
 * proargnames and proargmodes entries in Datum form.
 *
 * Returns the number of input arguments, which is the length of the
 * palloc'd array returned to *arg_names.  Entries for unnamed args
 * are set to NULL.  You don't get anything if proargnames is NULL.
 */
int
get_func_input_arg_names(Datum proargnames, Datum proargmodes, char ***arg_names)
{






































































}

/*
 * get_func_result_name
 *
 * If the function has exactly one output parameter, and that parameter
 * is named, return the name (as a palloc'd string).  Else return NULL.
 *
 * This is used to determine the default output column name for functions
 * returning scalar types.
 */
char *
get_func_result_name(Oid functionId)
{



















































































}

/*
 * build_function_result_tupdesc_t
 *
 * Given a pg_proc row for a function, return a tuple descriptor for the
 * result rowtype, or NULL if the function does not have OUT parameters.
 *
 * Note that this does not handle resolution of polymorphic types;
 * that is deliberate.
 */
TupleDesc
build_function_result_tupdesc_t(HeapTuple procTuple)
{






























}

/*
 * build_function_result_tupdesc_d
 *
 * Build a RECORD function's tupledesc from the pg_proc proallargtypes,
 * proargmodes, and proargnames arrays.  This is split out for the
 * convenience of ProcedureCreate, which needs to be able to compute the
 * tupledesc before actually creating the function.
 *
 * For functions (but not for procedures), returns NULL if there are not at
 * least two OUT or INOUT arguments.
 */
TupleDesc
build_function_result_tupdesc_d(char prokind, Datum proallargtypes, Datum proargmodes, Datum proargnames)
{




































































































}

/*
 * RelationNameGetTupleDesc
 *
 * Given a (possibly qualified) relation name, build a TupleDesc.
 *
 * Note: while this works as advertised, it's seldom the best way to
 * build a tupdesc for a function's result type.  It's kept around
 * only for backwards compatibility with existing user-written code.
 */
TupleDesc
RelationNameGetTupleDesc(const char *relname)
{













}

/*
 * TypeGetTupleDesc
 *
 * Given a type Oid, build a TupleDesc.  (In most cases you should be
 * using get_call_result_type or one of its siblings instead of this
 * routine, so that you can handle OUT parameters, RECORD result type,
 * and polymorphic results.)
 *
 * If the type is composite, *and* a colaliases List is provided, *and*
 * the List is of natts length, use the aliases instead of the relation
 * attnames.  (NB: this usage is deprecated since it may result in
 * creation of unnecessary transient record types.)
 *
 * If the type is a base type, a single item alias List is required.
 */
TupleDesc
TypeGetTupleDesc(Oid typeoid, List *colaliases)
{














































































}

/*
 * extract_variadic_args
 *
 * Extract a set of argument values, types and NULL markers for a given
 * input function which makes use of a VARIADIC input whose argument list
 * depends on the caller context. When doing a VARIADIC call, the caller
 * has provided one argument made of an array of values, so deconstruct the
 * array data before using it for the next processing. If no VARIADIC call
 * is used, just fill in the status data based on all the arguments given
 * by the caller.
 *
 * This function returns the number of arguments generated, or -1 in the
 * case of "VARIADIC NULL".
 */
int
extract_variadic_args(FunctionCallInfo fcinfo, int variadic_start, bool convert_unknown, Datum **args, Oid **types, bool **nulls)
{


























































































}