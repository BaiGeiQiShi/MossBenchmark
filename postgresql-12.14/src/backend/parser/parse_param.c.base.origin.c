/*-------------------------------------------------------------------------
 *
 * parse_param.c
 *	  handle parameters in parser
 *
 * This code covers two cases that are used within the core backend:
 *		* a fixed list of parameters with known types
 *		* an expandable list of parameters whose types can optionally
 *		  be determined from context
 * In both cases, only explicit $n references (ParamRef nodes) are supported.
 *
 * Note that other approaches to parameters are possible using the parser
 * hooks defined in ParseState.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_param.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_param.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

typedef struct FixedParamState
{
  Oid *paramTypes; /* array of parameter type OIDs */
  int numParams;   /* number of array entries */
} FixedParamState;

/*
 * In the varparams case, the caller-supplied OID array (if any) can be
 * re-palloc'd larger at need.  A zero array entry means that parameter number
 * hasn't been seen, while UNKNOWNOID means the parameter has been used but
 * its type is not yet known.
 */
typedef struct VarParamState
{
  Oid **paramTypes; /* array of parameter type OIDs */
  int *numParams;   /* number of array entries */
} VarParamState;

static Node *
fixed_paramref_hook(ParseState *pstate, ParamRef *pref);
static Node *
variable_paramref_hook(ParseState *pstate, ParamRef *pref);
static Node *
variable_coerce_param_hook(ParseState *pstate, Param *param, Oid targetTypeId, int32 targetTypeMod, int location);
static bool
check_parameter_resolution_walker(Node *node, ParseState *pstate);
static bool
query_contains_extern_params_walker(Node *node, void *context);

/*
 * Set up to process a query containing references to fixed parameters.
 */
void
parse_fixed_parameters(ParseState *pstate, Oid *paramTypes, int numParams)
{







}

/*
 * Set up to process a query containing references to variable parameters.
 */
void
parse_variable_parameters(ParseState *pstate, Oid **paramTypes, int *numParams)
{







}

/*
 * Transform a ParamRef using fixed parameter types.
 */
static Node *
fixed_paramref_hook(ParseState *pstate, ParamRef *pref)
{



















}

/*
 * Transform a ParamRef using variable parameter types.
 *
 * The only difference here is we must enlarge the parameter type array
 * as needed.
 */
static Node *
variable_paramref_hook(ParseState *pstate, ParamRef *pref)
{












































}

/*
 * Coerce a Param to a query-requested datatype, in the varparams case.
 */
static Node *
variable_coerce_param_hook(ParseState *pstate, Param *param, Oid targetTypeId, int32 targetTypeMod, int location)
{




























































}

/*
 * Check for consistent assignment of variable parameters after completion
 * of parsing with parse_variable_parameters.
 *
 * Note: this code intentionally does not check that all parameter positions
 * were used, nor that all got non-UNKNOWN types assigned.  Caller of parser
 * should enforce that if it's important.
 */
void
check_variable_parameters(ParseState *pstate, Query *query)
{







}

/*
 * Traverse a fully-analyzed tree to verify that parameter symbols
 * match their types.  We need this because some Params might still
 * be UNKNOWN, if there wasn't anything to force their coercion,
 * and yet other instances seen later might have gotten coerced.
 */
static bool
check_parameter_resolution_walker(Node *node, ParseState *pstate)
{
































}

/*
 * Check to see if a fully-parsed query tree contains any PARAM_EXTERN Params.
 */
bool
query_contains_extern_params(Query *query)
{

}

static bool
query_contains_extern_params_walker(Node *node, void *context)
{




















}