/*-------------------------------------------------------------------------
 *
 * parse_node.c
 *	  various routines that make nodes for querytrees
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_node.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "utils/builtins.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/varbit.h"

static void
pcb_error_callback(void *arg);

/*
 * make_parsestate
 *		Allocate and initialize a new ParseState.
 *
 * Caller should eventually release the ParseState via free_parsestate().
 */
ParseState *
make_parsestate(ParseState *parentParseState)
{
























}

/*
 * free_parsestate
 *		Release a ParseState and any subsidiary resources.
 */
void
free_parsestate(ParseState *pstate)
{
















}

/*
 * parser_errposition
 *		Report a parse-analysis-time cursor position, if possible.
 *
 * This is expected to be used within an ereport() call.  The return value
 * is a dummy (always 0, in fact).
 *
 * The locations stored in raw parsetrees are byte offsets into the source
 * string.  We have to convert them to 1-based character indexes for reporting
 * to clients.  (We do things this way to avoid unnecessary overhead in the
 * normal non-error case: computing character indexes would be much more
 * expensive than storing token offsets.)
 */
int
parser_errposition(ParseState *pstate, int location)
{
















}

/*
 * setup_parser_errposition_callback
 *		Arrange for non-parser errors to report an error position
 *
 * Sometimes the parser calls functions that aren't part of the parser
 * subsystem and can't reasonably be passed a ParseState; yet we would
 * like any errors thrown in those functions to be tagged with a parse
 * error location.  Use this function to set up an error context stack
 * entry that will accomplish that.  Usage pattern:
 *
 *		declare a local variable "ParseCallbackState pcbstate"
 *		...
 *		setup_parser_errposition_callback(&pcbstate, pstate, location);
 *		call function that might throw error;
 *		cancel_parser_errposition_callback(&pcbstate);
 */
void
setup_parser_errposition_callback(ParseCallbackState *pcbstate, ParseState *pstate, int location)
{







}

/*
 * Cancel a previously-set-up errposition callback.
 */
void
cancel_parser_errposition_callback(ParseCallbackState *pcbstate)
{


}

/*
 * Error context callback for inserting parser error location.
 *
 * Note that this will be called for *any* error occurring while the
 * callback is installed.  We avoid inserting an irrelevant error location
 * if the error is a query cancel --- are there any other important cases?
 */
static void
pcb_error_callback(void *arg)
{






}

/*
 * make_var
 *		Build a Var node for an attribute identified by RTE and attrno
 */
Var *
make_var(ParseState *pstate, RangeTblEntry *rte, int attrno, int location)
{











}

/*
 * transformContainerType()
 *		Identify the types involved in a subscripting operation for
 *container
 *
 *
 * On entry, containerType/containerTypmod identify the type of the input value
 * to be subscripted (which could be a domain type).  These are modified if
 * necessary to identify the actual container type and typmod, and the
 * container's element type is returned.  An error is thrown if the input isn't
 * an array type.
 */
Oid
transformContainerType(Oid *containerType, int32 *containerTypmod)
{

















































}

/*
 * transformContainerSubscripts()
 *		Transform container (array, etc) subscripting.  This is used for
 *both container fetch and container assignment.
 *
 * In a container fetch, we are given a source container value and we produce
 * an expression that represents the result of extracting a single container
 * element or a container slice.
 *
 * In a container assignment, we are given a destination container value plus a
 * source value that is to be assigned to a single element or a slice of that
 * container. We produce an expression that represents the new container value
 * with the source data inserted into the right part of the container.
 *
 * For both cases, if the source container is of a domain-over-array type,
 * the result is of the base array type or its element type; essentially,
 * we must fold a domain to its base type before applying subscripting.
 * (Note that int2vector and oidvector are treated as domains here.)
 *
 * pstate			Parse state
 * containerBase	Already-transformed expression for the container as a
 *whole containerType	OID of container's datatype (should match type of
 *					containerBase, or be the base type of
 *containerBase's domain type) elementType		OID of container's
 *element type (fetch with transformContainerType, or pass InvalidOid to do it
 *here) containerTypMod	typmod for the container (which is also typmod for the
 *					elements)
 * indirection		Untransformed list of subscripts (must not be NIL)
 * assignFrom		NULL for container fetch, else transformed expression
 *for source.
 */
SubscriptingRef *
transformContainerSubscripts(ParseState *pstate, Node *containerBase, Oid containerType, Oid elementType, int32 containerTypMod, List *indirection, Node *assignFrom)
{
































































































































}

/*
 * make_const
 *
 *	Convert a Value node (as returned by the grammar) to a Const node
 *	of the "natural" type for the constant.  Note that this routine is
 *	only used when there is no explicit cast for the constant, so we
 *	have to guess what type is wanted.
 *
 *	For string literals we produce a constant of type UNKNOWN ---- whose
 *	representation is the same as cstring, but it indicates to later type
 *	resolution that we're not sure yet what type it should be considered.
 *	Explicit "NULL" constants are also typed as UNKNOWN.
 *
 *	For integers and floats we produce int4, int8, or numeric depending
 *	on the value of the number.  XXX We should produce int2 as well,
 *	but additional cleanup is needed before we can do that; there are
 *	too many examples that fail if we try.
 */
Const *
make_const(ParseState *pstate, Value *value, int location)
{


































































































}