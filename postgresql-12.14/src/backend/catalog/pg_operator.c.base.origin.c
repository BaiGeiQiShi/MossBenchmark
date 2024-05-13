/*-------------------------------------------------------------------------
 *
 * pg_operator.c
 *	  routines to support manipulation of the pg_operator relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_operator.c
 *
 * NOTES
 *	  these routines moved here from commands/define.c and somewhat cleaned
 *up.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
OperatorGet(const char *operatorName, Oid operatorNamespace, Oid leftObjectId, Oid rightObjectId, bool *defined);

static Oid
OperatorLookup(List *operatorName, Oid leftObjectId, Oid rightObjectId, bool *defined);

static Oid
OperatorShellMake(const char *operatorName, Oid operatorNamespace, Oid leftTypeId, Oid rightTypeId);

static Oid
get_other_operator(List *otherOp, Oid otherLeftTypeId, Oid otherRightTypeId, const char *operatorName, Oid operatorNamespace, Oid leftTypeId, Oid rightTypeId, bool isCommutator);

/*
 * Check whether a proposed operator name is legal
 *
 * This had better match the behavior of parser/scan.l!
 *
 * We need this because the parser is not smart enough to check that
 * the arguments of CREATE OPERATOR's COMMUTATOR, NEGATOR, etc clauses
 * are operator names rather than some other lexical entity.
 */
static bool
validOperatorName(const char *name)
{




















































}

/*
 * OperatorGet
 *
 *		finds an operator given an exact specification (name, namespace,
 *		left and right type IDs).
 *
 *		*defined is set true if defined (not a shell)
 */
static Oid
OperatorGet(const char *operatorName, Oid operatorNamespace, Oid leftObjectId, Oid rightObjectId, bool *defined)
{



















}

/*
 * OperatorLookup
 *
 *		looks up an operator given a possibly-qualified name and
 *		left and right type IDs.
 *
 *		*defined is set true if defined (not a shell)
 */
static Oid
OperatorLookup(List *operatorName, Oid leftObjectId, Oid rightObjectId, bool *defined)
{














}

/*
 * OperatorShellMake
 *		Make a "shell" entry for a not-yet-existing operator.
 */
static Oid
OperatorShellMake(const char *operatorName, Oid operatorNamespace, Oid leftTypeId, Oid rightTypeId)
{



















































































}

/*
 * OperatorCreate
 *
 * "X" indicates an optional argument (i.e. one that can be NULL or 0)
 *		operatorName			name for new operator
 *		operatorNamespace		namespace for new operator
 *		leftTypeId				X left type ID
 *		rightTypeId				X right type ID
 *		procedureId				procedure ID for
 *operator commutatorName			X commutator operator
 *		negatorName				X negator operator
 *		restrictionId			X restriction selectivity
 *procedure ID joinId					X join selectivity
 *procedure ID canMerge				merge join can be used with this
 *operator canHash					hash join can be used
 *with this operator
 *
 * The caller should have validated properties and permissions for the
 * objects passed as OID references.  We must handle the commutator and
 * negator operator references specially, however, since those need not
 * exist beforehand.
 *
 * This routine gets complicated because it allows the user to
 * specify operators that do not exist.  For example, if operator
 * "op" is being defined, the negator operator "negop" and the
 * commutator "commop" can also be defined without specifying
 * any information other than their names.  Since in order to
 * add "op" to the PG_OPERATOR catalog, all the Oid's for these
 * operators must be placed in the fields of "op", a forward
 * declaration is done on the commutator and negator operators.
 * This is called creating a shell, and its main effect is to
 * create a tuple in the PG_OPERATOR catalog with minimal
 * information about the operator (just its name and types).
 * Forward declaration is used only for this purpose, it is
 * not available to the user as it is for type definition.
 */
ObjectAddress
OperatorCreate(const char *operatorName, Oid operatorNamespace, Oid leftTypeId, Oid rightTypeId, Oid procedureId, List *commutatorName, List *negatorName, Oid restrictionId, Oid joinId, bool canMerge, bool canHash)
{































































































































































































































}

/*
 * Try to lookup another operator (commutator, etc)
 *
 * If not found, check to see if it is exactly the operator we are trying
 * to define; if so, return InvalidOid.  (Note that this case is only
 * sensible for a commutator, so we error out otherwise.)  If it is not
 * the same operator, create a shell operator.
 */
static Oid
get_other_operator(List *otherOp, Oid otherLeftTypeId, Oid otherRightTypeId, const char *operatorName, Oid operatorNamespace, Oid leftTypeId, Oid rightTypeId, bool isCommutator)
{







































}

/*
 * OperatorUpd
 *
 *	For a given operator, look up its negator and commutator operators.
 *	When isDelete is false, update their negator and commutator fields to
 *	point back to the given operator; when isDelete is true, update those
 *	fields to no longer point back to the given operator.
 *
 *	The !isDelete case solves a problem for users who need to insert two new
 *	operators that are the negator or commutator of each other, while the
 *	isDelete case is needed so as not to leave dangling OID links behind
 *	after dropping an operator.
 */
void
OperatorUpd(Oid baseId, Oid commId, Oid negId, bool isDelete)
{






















































































































}

/*
 * Create dependencies for an operator (either a freshly inserted
 * complete operator, a new shell operator, a just-updated shell,
 * or an operator that's being modified by ALTER OPERATOR).
 *
 * NB: the OidIsValid tests in this routine are necessary, in case
 * the given operator is a shell.
 */
ObjectAddress
makeOperatorDependencies(HeapTuple tuple, bool isUpdate)
{
































































































}