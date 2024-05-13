/*-------------------------------------------------------------------------
 *
 * parse_oper.c
 *		handle operator things for parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_oper.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/*
 * The lookup key for the operator lookaside hash table.  Unused bits must be
 * zeroes to ensure hashing works consistently --- in particular, oprname
 * must be zero-padded and any unused entries in search_path must be zero.
 *
 * search_path contains the actual search_path with which the entry was
 * derived (minus temp namespace if any), or else the single specified
 * schema OID if we are looking up an explicitly-qualified operator name.
 *
 * search_path has to be fixed-length since the hashtable code insists on
 * fixed-size keys.  If your search path is longer than that, we just punt
 * and don't cache anything.
 */

/* If your search_path is longer than this, sucks to be you ... */
#define MAX_CACHED_PATH_LEN 16

typedef struct OprCacheKey
{
  char oprname[NAMEDATALEN];
  Oid left_arg;  /* Left input OID, or 0 if prefix op */
  Oid right_arg; /* Right input OID, or 0 if postfix op */
  Oid search_path[MAX_CACHED_PATH_LEN];
} OprCacheKey;

typedef struct OprCacheEntry
{
  /* the hash lookup key MUST BE FIRST */
  OprCacheKey key;

  Oid opr_oid; /* OID of the resolved operator */
} OprCacheEntry;

static Oid
binary_oper_exact(List *opname, Oid arg1, Oid arg2);
static FuncDetailCode
oper_select_candidate(int nargs, Oid *input_typeids, FuncCandidateList candidates, Oid *operOid);
static const char *
op_signature_string(List *op, char oprkind, Oid arg1, Oid arg2);
static void
op_error(ParseState *pstate, List *op, char oprkind, Oid arg1, Oid arg2, FuncDetailCode fdresult, int location);
static bool
make_oper_cache_key(ParseState *pstate, OprCacheKey *key, List *opname, Oid ltypeId, Oid rtypeId, int location);
static Oid
find_oper_cache_entry(OprCacheKey *key);
static void
make_oper_cache_entry(OprCacheKey *key, Oid opr_oid);
static void
InvalidateOprCacheCallBack(Datum arg, int cacheid, uint32 hashvalue);

/*
 * LookupOperName
 *		Given a possibly-qualified operator name and exact input
 *datatypes, look up the operator.
 *
 * Pass oprleft = InvalidOid for a prefix op, oprright = InvalidOid for
 * a postfix op.
 *
 * If the operator name is not schema-qualified, it is sought in the current
 * namespace search path.
 *
 * If the operator is not found, we return InvalidOid if noError is true,
 * else raise an error.  pstate and location are used only to report the
 * error position; pass NULL/-1 if not available.
 */
Oid
LookupOperName(ParseState *pstate, List *opername, Oid oprleft, Oid oprright, bool noError, int location)
{






























}

/*
 * LookupOperWithArgs
 *		Like LookupOperName, but the argument types are specified by
 *		a ObjectWithArg node.
 */
Oid
LookupOperWithArgs(ObjectWithArgs *oper, bool noError)
{


























}

/*
 * get_sort_group_operators - get default sorting/grouping operators for type
 *
 * We fetch the "<", "=", and ">" operators all at once to reduce lookup
 * overhead (knowing that most callers will be interested in at least two).
 * However, a given datatype might have only an "=" operator, if it is
 * hashable but not sortable.  (Other combinations of present and missing
 * operators shouldn't happen, unless the system catalogs are messed up.)
 *
 * If an operator is missing and the corresponding needXX flag is true,
 * throw a standard error message, else return InvalidOid.
 *
 * In addition to the operator OIDs themselves, this function can identify
 * whether the "=" operator is hashable.
 *
 * Callers can pass NULL pointers for any results they don't care to get.
 *
 * Note: the results are guaranteed to be exact or binary-compatible matches,
 * since most callers are not prepared to cope with adding any run-time type
 * coercion steps.
 */
void
get_sort_group_operators(Oid argtype, bool needLT, bool needEQ, bool needGT, Oid *ltOpr, Oid *eqOpr, Oid *gtOpr, bool *isHashable)
{























































}

/* given operator tuple, return the operator OID */
Oid
oprid(Operator op)
{

}

/* given operator tuple, return the underlying function's OID */
Oid
oprfuncid(Operator op)
{



}

/* binary_oper_exact()
 * Check for an "exact" match to the specified operand types.
 *
 * If one operand is an unknown literal, assume it should be taken to be
 * the same type as the other operand for this purpose.  Also, consider
 * the possibility that the other operand is a domain type that needs to
 * be reduced to its base type to find an "exact" match.
 */
static Oid
binary_oper_exact(List *opname, Oid arg1, Oid arg2)
{





































}

/* oper_select_candidate()
 *		Given the input argtype array and one or more candidates
 *		for the operator, attempt to resolve the conflict.
 *
 * Returns FUNCDETAIL_NOTFOUND, FUNCDETAIL_MULTIPLE, or FUNCDETAIL_NORMAL.
 * In the success case the Oid of the best candidate is stored in *operOid.
 *
 * Note that the caller has already determined that there is no candidate
 * exactly matching the input argtype(s).  Incompatible candidates are not yet
 * pruned away, however.
 */
static FuncDetailCode
oper_select_candidate(int nargs, Oid *input_typeids, FuncCandidateList candidates, Oid *operOid) /* output argument */
{


































}

/* oper() -- search for a binary operator
 * Given operator name, types of arg1 and arg2, return oper struct.
 *
 * IMPORTANT: the returned operator (if any) is only promised to be
 * coercion-compatible with the input datatypes.  Do not use this if
 * you need an exact- or binary-compatible match; see compatible_oper.
 *
 * If no matching operator found, return NULL if noError is true,
 * raise an error if it is false.  pstate and location are used only to report
 * the error position; pass NULL/-1 if not available.
 *
 * NOTE: on success, the returned object is a syscache entry.  The caller
 * must ReleaseSysCache() the entry when done with it.
 */
Operator
oper(ParseState *pstate, List *opname, Oid ltypeId, Oid rtypeId, bool noError, int location)
{















































































}

/* compatible_oper()
 *	given an opname and input datatypes, find a compatible binary operator
 *
 *	This is tighter than oper() because it will not return an operator that
 *	requires coercion of the input datatypes (but binary-compatible
 *operators are accepted).  Otherwise, the semantics are the same.
 */
Operator
compatible_oper(ParseState *pstate, List *op, Oid arg1, Oid arg2, bool noError, int location)
{


























}

/* compatible_oper_opid() -- get OID of a binary operator
 *
 * This is a convenience routine that extracts only the operator OID
 * from the result of compatible_oper().  InvalidOid is returned if the
 * lookup fails and noError is true.
 */
Oid
compatible_oper_opid(List *op, Oid arg1, Oid arg2, bool noError)
{











}

/* right_oper() -- search for a unary right operator (postfix operator)
 * Given operator name and type of arg, return oper struct.
 *
 * IMPORTANT: the returned operator (if any) is only promised to be
 * coercion-compatible with the input datatype.  Do not use this if
 * you need an exact- or binary-compatible match.
 *
 * If no matching operator found, return NULL if noError is true,
 * raise an error if it is false.  pstate and location are used only to report
 * the error position; pass NULL/-1 if not available.
 *
 * NOTE: on success, the returned object is a syscache entry.  The caller
 * must ReleaseSysCache() the entry when done with it.
 */
Operator
right_oper(ParseState *pstate, List *op, Oid arg, bool noError, int location)
{



































































}

/* left_oper() -- search for a unary left operator (prefix operator)
 * Given operator name and type of arg, return oper struct.
 *
 * IMPORTANT: the returned operator (if any) is only promised to be
 * coercion-compatible with the input datatype.  Do not use this if
 * you need an exact- or binary-compatible match.
 *
 * If no matching operator found, return NULL if noError is true,
 * raise an error if it is false.  pstate and location are used only to report
 * the error position; pass NULL/-1 if not available.
 *
 * NOTE: on success, the returned object is a syscache entry.  The caller
 * must ReleaseSysCache() the entry when done with it.
 */
Operator
left_oper(ParseState *pstate, List *op, Oid arg, bool noError, int location)
{















































































}

/*
 * op_signature_string
 *		Build a string representing an operator name, including arg
 *type(s). The result is something like "integer + integer".
 *
 * This is typically used in the construction of operator-not-found error
 * messages.
 */
static const char *
op_signature_string(List *op, char oprkind, Oid arg1, Oid arg2)
{

















}

/*
 * op_error - utility routine to complain about an unresolvable operator
 */
static void
op_error(ParseState *pstate, List *op, char oprkind, Oid arg1, Oid arg2, FuncDetailCode fdresult, int location)
{








}

/*
 * make_op()
 *		Operator expression construction.
 *
 * Transform operator expression ensuring type compatibility.
 * This is where some type conversion happens.
 *
 * last_srf should be a copy of pstate->p_last_srf from just before we
 * started transforming the operator's arguments; this is used for nested-SRF
 * detection.  If the caller will throw an error anyway for a set-returning
 * expression, it's okay to cheat and just pass pstate->p_last_srf.
 */
Expr *
make_op(ParseState *pstate, List *opname, Node *ltree, Node *rtree, Node *last_srf, int location)
{




































































































}

/*
 * make_scalar_array_op()
 *		Build expression tree for "scalar op ANY/ALL (array)" construct.
 */
Expr *
make_scalar_array_op(ParseState *pstate, List *opname, bool useOr, Node *ltree, Node *rtree, int location)
{






































































































}

/*
 * Lookaside cache to speed operator lookup.  Possibly this should be in
 * a separate module under utils/cache/ ?
 *
 * The idea here is that the mapping from operator name and given argument
 * types is constant for a given search path (or single specified schema OID)
 * so long as the contents of pg_operator and pg_cast don't change.  And that
 * mapping is pretty expensive to compute, especially for ambiguous operators;
 * this is mainly because there are a *lot* of instances of popular operator
 * names such as "=", and we have to check each one to see which is the
 * best match.  So once we have identified the correct mapping, we save it
 * in a cache that need only be flushed on pg_operator or pg_cast change.
 * (pg_cast must be considered because changes in the set of implicit casts
 * affect the set of applicable operators for any given input datatype.)
 *
 * XXX in principle, ALTER TABLE ... INHERIT could affect the mapping as
 * well, but we disregard that since there's no convenient way to find out
 * about it, and it seems a pretty far-fetched corner-case anyway.
 *
 * Note: at some point it might be worth doing a similar cache for function
 * lookups.  However, the potential gain is a lot less since (a) function
 * names are generally not overloaded as heavily as operator names, and
 * (b) we'd have to flush on pg_proc updates, which are probably a good
 * deal more common than pg_operator updates.
 */

/* The operator cache hashtable */
static HTAB *OprCacheHash = NULL;

/*
 * make_oper_cache_key
 *		Fill the lookup key struct given operator name and arg types.
 *
 * Returns true if successful, false if the search_path overflowed
 * (hence no caching is possible).
 *
 * pstate/location are used only to report the error position; pass NULL/-1
 * if not available.
 */
static bool
make_oper_cache_key(ParseState *pstate, OprCacheKey *key, List *opname, Oid ltypeId, Oid rtypeId, int location)
{

































}

/*
 * find_oper_cache_entry
 *
 * Look for a cache entry matching the given key.  If found, return the
 * contained operator OID, else return InvalidOid.
 */
static Oid
find_oper_cache_entry(OprCacheKey *key)
{

























}

/*
 * make_oper_cache_entry
 *
 * Insert a cache entry for the given key.
 */
static void
make_oper_cache_entry(OprCacheKey *key, Oid opr_oid)
{






}

/*
 * Callback for pg_operator and pg_cast inval events
 */
static void
InvalidateOprCacheCallBack(Datum arg, int cacheid, uint32 hashvalue)
{















}