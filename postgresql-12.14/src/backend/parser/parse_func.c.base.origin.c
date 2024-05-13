/*-------------------------------------------------------------------------
 *
 * parse_func.c
 *		handle function calls in parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_func.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/* Possible error codes from LookupFuncNameInternal */
typedef enum
{
  FUNCLOOKUP_NOSUCHFUNC,
  FUNCLOOKUP_AMBIGUOUS
} FuncLookupError;

static void
unify_hypothetical_args(ParseState *pstate, List *fargs, int numAggregatedArgs, Oid *actual_arg_types, Oid *declared_arg_types);
static Oid
FuncNameAsType(List *funcname);
static Node *
ParseComplexProjection(ParseState *pstate, const char *funcname, Node *first_arg, int location);
static Oid
LookupFuncNameInternal(List *funcname, int nargs, const Oid *argtypes, bool missing_ok, FuncLookupError *lookupError);

/*
 *	Parse a function call
 *
 *	For historical reasons, Postgres tries to treat the notations tab.col
 *	and col(tab) as equivalent: if a single-argument function call has an
 *	argument of complex type and the (unqualified) function name matches
 *	any attribute of the type, we can interpret it as a column projection.
 *	Conversely a function of a single complex-type argument can be written
 *	like a column reference, allowing functions to act like computed
 *columns.
 *
 *	If both interpretations are possible, we prefer the one matching the
 *	syntactic form, but otherwise the form does not matter.
 *
 *	Hence, both cases come through here.  If fn is null, we're dealing with
 *	column syntax not function syntax.  In the function-syntax case,
 *	the FuncCall struct is needed to carry various decoration that applies
 *	to aggregate and window functions.
 *
 *	Also, when fn is null, we return NULL on failure rather than
 *	reporting a no-such-function error.
 *
 *	The argument expressions (in fargs) must have been transformed
 *	already.  However, nothing in *fn has been transformed.
 *
 *	last_srf should be a copy of pstate->p_last_srf from just before we
 *	started transforming fargs.  If the caller knows that fargs couldn't
 *	contain any SRF calls, last_srf can just be pstate->p_last_srf.
 *
 *	proc_call is true if we are considering a CALL statement, so that the
 *	name must resolve to a procedure name, not anything else.
 */
Node *
ParseFuncOrColumn(ParseState *pstate, List *funcname, List *fargs, Node *last_srf, FuncCall *fn, bool proc_call, int location)
{




































































































































































































































































































































































































































































































































































































































































































}

/* func_match_argtypes()
 *
 * Given a list of candidate functions (having the right name and number
 * of arguments) and an array of input datatype OIDs, produce a shortlist of
 * those candidates that actually accept the input datatypes (either exactly
 * or by coercion), and return the number of such candidates.
 *
 * Note that can_coerce_type will assume that UNKNOWN inputs are coercible to
 * anything, so candidates will not be eliminated on that basis.
 *
 * NB: okay to modify input list structure, as long as we find at least
 * one match.  If no match at all, the list must remain unmodified.
 */
int
func_match_argtypes(int nargs, Oid *input_typeids, FuncCandidateList raw_candidates, FuncCandidateList *candidates) /* return value */
{


















} /* func_match_argtypes() */

/* func_select_candidate()
 *		Given the input argtype array and more than one candidate
 *		for the function, attempt to resolve the conflict.
 *
 * Returns the selected candidate if the conflict can be resolved,
 * otherwise returns NULL.
 *
 * Note that the caller has already determined that there is no candidate
 * exactly matching the input argtypes, and has pruned away any "candidates"
 * that aren't actually coercion-compatible with the input types.
 *
 * This is also used for resolving ambiguous operator references.  Formerly
 * parse_oper.c had its own, essentially duplicate code for the purpose.
 * The following comments (formerly in parse_oper.c) are kept to record some
 * of the history of these heuristics.
 *
 * OLD COMMENTS:
 *
 * This routine is new code, replacing binary_oper_select_candidate()
 * which dates from v4.2/v1.0.x days. It tries very hard to match up
 * operators with types, including allowing type coercions if necessary.
 * The important thing is that the code do as much as possible,
 * while _never_ doing the wrong thing, where "the wrong thing" would
 * be returning an operator when other better choices are available,
 * or returning an operator which is a non-intuitive possibility.
 * - thomas 1998-05-21
 *
 * The comments below came from binary_oper_select_candidate(), and
 * illustrate the issues and choices which are possible:
 * - thomas 1998-05-20
 *
 * current wisdom holds that the default operator should be one in which
 * both operands have the same type (there will only be one such
 * operator)
 *
 * 7.27.93 - I have decided not to do this; it's too hard to justify, and
 * it's easy enough to typecast explicitly - avi
 * [the rest of this routine was commented out since then - ay]
 *
 * 6/23/95 - I don't complete agree with avi. In particular, casting
 * floats is a pain for users. Whatever the rationale behind not doing
 * this is, I need the following special case to work.
 *
 * In the WHERE clause of a query, if a float is specified without
 * quotes, we treat it as float8. I added the float48* operators so
 * that we can operate on float4 and float8. But now we have more than
 * one matching operator if the right arg is unknown (eg. float
 * specified with quotes). This break some stuff in the regression
 * test where there are floats in quotes not properly casted. Below is
 * the solution. In addition to requiring the operator operates on the
 * same type for both operands [as in the code Avi originally
 * commented out], we also require that the operators be equivalent in
 * some sense. (see equivalentOpersAfterPromotion for details.)
 * - ay 6/95
 */
FuncCandidateList
func_select_candidate(int nargs, Oid *input_typeids, FuncCandidateList candidates)
{







































































































































































































































































































































































} /* func_select_candidate() */

/* func_get_detail()
 *
 * Find the named function in the system catalogs.
 *
 * Attempt to find the named function in the system catalogs with
 * arguments exactly as specified, so that the normal case (exact match)
 * is as quick as possible.
 *
 * If an exact match isn't found:
 *	1) check for possible interpretation as a type coercion request
 *	2) apply the ambiguous-function resolution rules
 *
 * Return values *funcid through *true_typeids receive info about the function.
 * If argdefaults isn't NULL, *argdefaults receives a list of any default
 * argument expressions that need to be added to the given arguments.
 *
 * When processing a named- or mixed-notation call (ie, fargnames isn't NIL),
 * the returned true_typeids and argdefaults are ordered according to the
 * call's argument ordering: first any positional arguments, then the named
 * arguments, then defaulted arguments (if needed and allowed by
 * expand_defaults).  Some care is needed if this information is to be compared
 * to the function's pg_proc entry, but in practice the caller can usually
 * just work with the call's argument ordering.
 *
 * We rely primarily on fargnames/nargs/argtypes as the argument description.
 * The actual expression node list is passed in fargs so that we can check
 * for type coercion of a constant.  Some callers pass fargs == NIL indicating
 * they don't need that check made.  Note also that when fargnames isn't NIL,
 * the fargs list must be passed if the caller wants actual argument position
 * information to be returned into the NamedArgExpr nodes.
 */
FuncDetailCode
func_get_detail(List *funcname, List *fargs, List *fargnames, int nargs, Oid *argtypes, bool expand_variadic, bool expand_defaults, Oid *funcid, /* return value */
    Oid *rettype,                                                                                                                                /* return value */
    bool *retset,                                                                                                                                /* return value */
    int *nvargs,                                                                                                                                 /* return value */
    Oid *vatype,                                                                                                                                 /* return value */
    Oid **true_typeids,                                                                                                                          /* return value */
    List **argdefaults)                                                                                                                          /* optional return value */
{





































































































































































































































































































































}

/*
 * unify_hypothetical_args()
 *
 * Ensure that each hypothetical direct argument of a hypothetical-set
 * aggregate has the same type as the corresponding aggregated argument.
 * Modify the expressions in the fargs list, if necessary, and update
 * actual_arg_types[].
 *
 * If the agg declared its args non-ANY (even ANYELEMENT), we need only a
 * sanity check that the declared types match; make_fn_arguments will coerce
 * the actual arguments to match the declared ones.  But if the declaration
 * is ANY, nothing will happen in make_fn_arguments, so we need to fix any
 * mismatch here.  We use the same type resolution logic as UNION etc.
 */
static void
unify_hypothetical_args(ParseState *pstate, List *fargs, int numAggregatedArgs, Oid *actual_arg_types, Oid *declared_arg_types)
{





























































}

/*
 * make_fn_arguments()
 *
 * Given the actual argument expressions for a function, and the desired
 * input types for the function, add any necessary typecasting to the
 * expression tree.  Caller should already have verified that casting is
 * allowed.
 *
 * Caution: given argument list is modified in-place.
 *
 * As with coerce_type, pstate may be NULL if no special unknown-Param
 * processing is wanted.
 */
void
make_fn_arguments(ParseState *pstate, List *fargs, Oid *actual_arg_types, Oid *declared_arg_types)
{





























}

/*
 * FuncNameAsType -
 *	  convenience routine to see if a function name matches a type name
 *
 * Returns the OID of the matching type, or InvalidOid if none.  We ignore
 * shell types and complex types.
 */
static Oid
FuncNameAsType(List *funcname)
{
























}

/*
 * ParseComplexProjection -
 *	  handles function calls with a single argument that is of complex type.
 *	  If the function call is actually a column projection, return a
 *suitably transformed expression tree.  If not, return NULL.
 */
static Node *
ParseComplexProjection(ParseState *pstate, const char *funcname, Node *first_arg, int location)
{






























































}

/*
 * funcname_signature_string
 *		Build a string representing a function name, including arg
 *types. The result is something like "foo(integer)".
 *
 * If argnames isn't NIL, it is a list of C strings representing the actual
 * arg names for the last N arguments.  This must be considered part of the
 * function signature too, when dealing with named-notation function calls.
 *
 * This is typically used in the construction of function-not-found error
 * messages.
 */
const char *
funcname_signature_string(const char *funcname, int nargs, List *argnames, const Oid *argtypes)
{





























}

/*
 * func_signature_string
 *		As above, but function name is passed as a qualified name list.
 */
const char *
func_signature_string(List *funcname, int nargs, List *argnames, const Oid *argtypes)
{

}

/*
 * LookupFuncNameInternal
 *		Workhorse for LookupFuncName/LookupFuncWithArgs
 *
 * In an error situation, e.g. can't find the function, then we return
 * InvalidOid and set *lookupError to indicate what went wrong.
 *
 * Possible errors:
 *	FUNCLOOKUP_NOSUCHFUNC: we can't find a function of this name.
 *	FUNCLOOKUP_AMBIGUOUS: nargs == -1 and more than one function matches.
 */
static Oid
LookupFuncNameInternal(List *funcname, int nargs, const Oid *argtypes, bool missing_ok, FuncLookupError *lookupError)
{














































}

/*
 * LookupFuncName
 *
 * Given a possibly-qualified function name and optionally a set of argument
 * types, look up the function.  Pass nargs == -1 to indicate that the number
 * and types of the arguments are unspecified (this is NOT the same as
 * specifying that there are no arguments).
 *
 * If the function name is not schema-qualified, it is sought in the current
 * namespace search path.
 *
 * If the function is not found, we return InvalidOid if missing_ok is true,
 * else raise an error.
 *
 * If nargs == -1 and multiple functions are found matching this function name
 * we will raise an ambiguous-function error, regardless of what missing_ok is
 * set to.
 */
Oid
LookupFuncName(List *funcname, int nargs, const Oid *argtypes, bool missing_ok)
{




































}

/*
 * LookupFuncWithArgs
 *
 * Like LookupFuncName, but the argument types are specified by an
 * ObjectWithArgs node.  Also, this function can check whether the result is a
 * function, procedure, or aggregate, based on the objtype argument.  Pass
 * OBJECT_ROUTINE to accept any of them.
 *
 * For historical reasons, we also accept aggregates when looking for a
 * function.
 *
 * When missing_ok is true we don't generate any error for missing objects and
 * return InvalidOid.  Other types of errors can still be raised, regardless
 * of the value of missing_ok.
 */
Oid
LookupFuncWithArgs(ObjectType objtype, ObjectWithArgs *func, bool missing_ok)
{





































































































































































}

/*
 * check_srf_call_placement
 *		Verify that a set-returning function is called in a valid place,
 *		and throw a nice error if not.
 *
 * A side-effect is to set pstate->p_hasTargetSRFs true if appropriate.
 *
 * last_srf should be a copy of pstate->p_last_srf from just before we
 * started transforming the function's arguments.  This allows detection
 * of whether the SRF's arguments contain any SRFs.
 */
void
check_srf_call_placement(ParseState *pstate, Node *last_srf, int location)
{

























































































































































}