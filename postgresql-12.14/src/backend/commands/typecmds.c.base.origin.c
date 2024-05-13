/*-------------------------------------------------------------------------
 *
 * typecmds.c
 *	  Routines for SQL commands that manipulate types (and domains).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/typecmds.c
 *
 * DESCRIPTION
 *	  The "DefineFoo" routines take the parse tree and pick out the
 *	  appropriate arguments/flags, passing the results to the
 *	  corresponding "FooDefine" routines (in src/catalog) that do
 *	  the actual catalog-munging.  These routines also verify permission
 *	  of the user to execute the command.
 *
 * NOTES
 *	  These things must be defined and committed in the following order:
 *		"create function":
 *				input/output, recv/send functions
 *		"create type":
 *				type
 *		"create operator":
 *				operators
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/heap.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_am.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_range.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/typecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/* result structure for get_rels_with_domain() */
typedef struct
{
  Relation rel; /* opened and locked relation */
  int natts;    /* number of attributes of interest */
  int *atts;    /* attribute numbers */
  /* atts[] is of allocated length RelationGetNumberOfAttributes(rel) */
} RelToCheck;

/* Potentially set by pg_upgrade_support functions */
Oid binary_upgrade_next_array_pg_type_oid = InvalidOid;

static void
makeRangeConstructors(const char *name, Oid namespace, Oid rangeOid, Oid subtype);
static Oid
findTypeInputFunction(List *procname, Oid typeOid);
static Oid
findTypeOutputFunction(List *procname, Oid typeOid);
static Oid
findTypeReceiveFunction(List *procname, Oid typeOid);
static Oid
findTypeSendFunction(List *procname, Oid typeOid);
static Oid
findTypeTypmodinFunction(List *procname);
static Oid
findTypeTypmodoutFunction(List *procname);
static Oid
findTypeAnalyzeFunction(List *procname, Oid typeOid);
static Oid
findRangeSubOpclass(List *opcname, Oid subtype);
static Oid
findRangeCanonicalFunction(List *procname, Oid typeOid);
static Oid
findRangeSubtypeDiffFunction(List *procname, Oid subtype);
static void
validateDomainConstraint(Oid domainoid, char *ccbin);
static List *
get_rels_with_domain(Oid domainOid, LOCKMODE lockmode);
static void
checkEnumOwner(HeapTuple tup);
static char *
domainAddConstraint(Oid domainOid, Oid domainNamespace, Oid baseTypeOid, int typMod, Constraint *constr, const char *domainName, ObjectAddress *constrAddr);
static Node *
replace_domain_constraint_value(ParseState *pstate, ColumnRef *cref);

/*
 * DefineType
 *		Registers a new base type.
 */
ObjectAddress
DefineType(ParseState *pstate, List *names, List *parameters)
{

























































































































































































































































































































































































































































































































































































































































}

/*
 * Guts of type deletion.
 */
void
RemoveTypeById(Oid typeOid)
{




































}

/*
 * DefineDomain
 *		Registers a new domain.
 */
ObjectAddress
DefineDomain(CreateDomainStmt *stmt)
{














































































































































































































































































































































































































}

/*
 * DefineEnum
 *		Registers a new enum.
 */
ObjectAddress
DefineEnum(CreateEnumStmt *stmt)
{














































































































}

/*
 * AlterEnum
 *		Adds a new label to an existing enum.
 */
ObjectAddress
AlterEnum(AlterEnumStmt *stmt)
{




































}

/*
 * checkEnumOwner
 *
 * Check that the type is actually an enum and that the current user
 * has permission to do ALTER TYPE on it.  Throw an error if not.
 */
static void
checkEnumOwner(HeapTuple tup)
{













}

/*
 * DefineRange
 *		Registers a new range type.
 */
ObjectAddress
DefineRange(CreateRangeStmt *stmt)
{



































































































































































































































































}

/*
 * Because there may exist several range types over the same subtype, the
 * range type can't be uniquely determined from the subtype.  So it's
 * impossible to define a polymorphic constructor; we have to generate new
 * constructor functions explicitly for each range type.
 *
 * We actually define 4 functions, with 0 through 3 arguments.  This is just
 * to offer more convenience for the user.
 */
static void
makeRangeConstructors(const char *name, Oid namespace, Oid rangeOid, Oid subtype)
{






















































}

/*
 * Find suitable I/O functions for a type.
 *
 * typeOid is the type's OID (which will already exist, if only as a shell
 * type).
 */

static Oid
findTypeInputFunction(List *procname, Oid typeOid)
{

























































































}

static Oid
findTypeOutputFunction(List *procname, Oid typeOid)
{











































}

static Oid
findTypeReceiveFunction(List *procname, Oid typeOid)
{
































}

static Oid
findTypeSendFunction(List *procname, Oid typeOid)
{

















}

static Oid
findTypeTypmodinFunction(List *procname)
{




















}

static Oid
findTypeTypmodoutFunction(List *procname)
{




















}

static Oid
findTypeAnalyzeFunction(List *procname, Oid typeOid)
{




















}

/*
 * Find suitable support functions and opclasses for a range type.
 */

/*
 * Find named btree opclass for subtype, or default btree opclass if
 * opcname is NIL.
 */
static Oid
findRangeSubOpclass(List *opcname, Oid subtype)
{




























}

static Oid
findRangeCanonicalFunction(List *procname, Oid typeOid)
{



































}

static Oid
findRangeSubtypeDiffFunction(List *procname, Oid subtype)
{




































}

/*
 *	AssignTypeArrayOid
 *
 *	Pre-assign the type's array OID for use in pg_type.typarray
 */
Oid
AssignTypeArrayOid(void)
{






















}

/*-------------------------------------------------------------------
 * DefineCompositeType
 *
 * Create a Composite Type relation.
 * `DefineRelation' does all the work, we just provide the correct
 * arguments!
 *
 * If the relation already exists, then 'DefineRelation' will abort
 * the xact...
 *
 * Return type is the new type's object address.
 *-------------------------------------------------------------------
 */
ObjectAddress
DefineCompositeType(RangeVar *typevar, List *coldeflist)
{









































}

/*
 * AlterDomainDefault
 *
 * Routine implementing ALTER DOMAIN SET/DROP DEFAULT statements.
 *
 * Returns ObjectAddress of the modified domain.
 */
ObjectAddress
AlterDomainDefault(List *names, Node *defaultRaw)
{


























































































































}

/*
 * AlterDomainNotNull
 *
 * Routine implementing ALTER DOMAIN SET/DROP NOT NULL statements.
 *
 * Returns ObjectAddress of the modified domain.
 */
ObjectAddress
AlterDomainNotNull(List *names, bool notNull)
{









































































































}

/*
 * AlterDomainDropConstraint
 *
 * Implements the ALTER DOMAIN DROP CONSTRAINT statement
 *
 * Returns ObjectAddress of the modified domain.
 */
ObjectAddress
AlterDomainDropConstraint(List *names, const char *constrName, DropBehavior behavior, bool missing_ok)
{















































































}

/*
 * AlterDomainAddConstraint
 *
 * Implements the ALTER DOMAIN .. ADD CONSTRAINT statement.
 */
ObjectAddress
AlterDomainAddConstraint(List *names, Node *newConstraint, ObjectAddress *constrAddr)
{

































































































}

/*
 * AlterDomainValidateConstraint
 *
 * Implements the ALTER DOMAIN .. VALIDATE CONSTRAINT statement.
 */
ObjectAddress
AlterDomainValidateConstraint(List *names, const char *constrName)
{






















































































}

static void
validateDomainConstraint(Oid domainoid, char *ccbin)
{













































































}

/*
 * get_rels_with_domain
 *
 * Fetch all relations / attributes which are using the domain
 *
 * The result is a list of RelToCheck structs, one for each distinct
 * relation, each containing one or more attribute numbers that are of
 * the domain type.  We have opened each rel and acquired the specified lock
 * type on it.
 *
 * We support nested domains by including attributes that are of derived
 * domain types.  Current callers do not need to distinguish between attributes
 * that are of exactly the given domain and those that are of derived domains.
 *
 * XXX this is completely broken because there is no way to lock the domain
 * to prevent columns from being added or dropped while our command runs.
 * We can partially protect against column drops by locking relations as we
 * come across them, but there is still a race condition (the window between
 * seeing a pg_depend entry and acquiring lock on the relation it references).
 * Also, holding locks on all these relations simultaneously creates a non-
 * trivial risk of deadlock.  We can minimize but not eliminate the deadlock
 * risk by using the weakest suitable lock (ShareLock for most callers).
 *
 * XXX the API for this is not sufficient to support checking domain values
 * that are inside container types, such as composite types, arrays, or
 * ranges.  Currently we just error out if a container type containing the
 * target domain is stored anywhere.
 *
 * Generally used for retrieving a list of tests when adding
 * new constraints to a domain.
 */
static List *
get_rels_with_domain(Oid domainOid, LOCKMODE lockmode)
{























































































































































}

/*
 * checkDomainOwner
 *
 * Check that the type is actually a domain and that the current user
 * has permission to do ALTER DOMAIN on it.  Throw an error if not.
 */
void
checkDomainOwner(HeapTuple tup)
{













}

/*
 * domainAddConstraint - code shared between CREATE and ALTER DOMAIN
 */
static char *
domainAddConstraint(Oid domainOid, Oid domainNamespace, Oid baseTypeOid, int typMod, Constraint *constr, const char *domainName, ObjectAddress *constrAddr)
{







































































































}

/* Parser pre_columnref_hook for domain CHECK constraint parsing */
static Node *
replace_domain_constraint_value(ParseState *pstate, ColumnRef *cref)
{























}

/*
 * Execute ALTER TYPE RENAME
 */
ObjectAddress
RenameType(RenameStmt *stmt)
{





































































}

/*
 * Change the owner of a type.
 */
ObjectAddress
AlterTypeOwner(List *names, Oid newOwnerId, ObjectType objecttype)
{





















































































}

/*
 * AlterTypeOwner_oid - change type owner unconditionally
 *
 * This function recurses to handle a pg_class entry, if necessary.  It
 * invokes any necessary access object hooks.  If hasDependEntry is true, this
 * function modifies the pg_shdepend entry appropriately (this should be
 * passed as false only for table rowtypes and array types).
 *
 * This is used by ALTER TABLE/TYPE OWNER commands, as well as by REASSIGN
 * OWNED BY.  It assumes the caller has done all needed check.
 */
void
AlterTypeOwner_oid(Oid typeOid, Oid newOwnerId, bool hasDependEntry)
{





































}

/*
 * AlterTypeOwnerInternal - bare-bones type owner change.
 *
 * This routine simply modifies the owner of a pg_type entry, and recurses
 * to handle a possible array type.
 */
void
AlterTypeOwnerInternal(Oid typeOid, Oid newOwnerId)
{














































}

/*
 * Execute ALTER TYPE SET SCHEMA
 */
ObjectAddress
AlterTypeNamespace(List *names, const char *newschema, ObjectType objecttype, Oid *oldschema)
{
































}

Oid
AlterTypeNamespace_oid(Oid typeOid, Oid nspOid, ObjectAddresses *objsMoved)
{

















}

/*
 * Move specified type to new namespace.
 *
 * Caller must have already checked privileges.
 *
 * The function automatically recurses to process the type's array type,
 * if any.  isImplicitArray should be true only when doing this internal
 * recursion (outside callers must never try to move an array type directly).
 *
 * If errorOnTableType is true, the function errors out if the type is
 * a table type.  ALTER TABLE has to be used to move a table to a new
 * namespace.
 *
 * Returns the type's old namespace OID.
 */
Oid
AlterTypeNamespaceInternal(Oid typeOid, Oid nspOid, bool isImplicitArray, bool errorOnTableType, ObjectAddresses *objsMoved)
{


























































































































}