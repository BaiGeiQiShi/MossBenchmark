/*-------------------------------------------------------------------------
 *
 * pg_constraint.c
 *	  routines to support manipulation of the pg_constraint relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_constraint.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tupconvert.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/*
 * CreateConstraintEntry
 *	Create a constraint table entry.
 *
 * Subsidiary records (such as triggers or indexes to implement the
 * constraint) are *not* created here.  But we do make dependency links
 * from the constraint to the things it depends on.
 *
 * The new constraint's OID is returned.
 */
Oid
CreateConstraintEntry(const char *constraintName, Oid constraintNamespace, char constraintType, bool isDeferrable, bool isDeferred, bool isValidated, Oid parentConstrId, Oid relId, const int16 *constraintKey, int constraintNKeys, int constraintNTotalKeys, Oid domainId, Oid indexRelId, Oid foreignRelId, const int16 *foreignKey, const Oid *pfEqOp, const Oid *ppEqOp, const Oid *ffEqOp, int foreignNKeys, char foreignUpdateType, char foreignDeleteType, char foreignMatchType, const Oid *exclOp, Node *conExpr, const char *conBin, bool conIsLocal, int conInhCount, bool conNoInherit, bool is_internal)
{





































































































































































































































































































































}

/*
 * Test whether given name is currently used as a constraint name
 * for the given object (relation or domain).
 *
 * This is used to decide whether to accept a user-specified constraint name.
 * It is deliberately not the same test as ChooseConstraintName uses to decide
 * whether an auto-generated name is OK: here, we will allow it unless there
 * is an identical constraint name in use *on the same object*.
 *
 * NB: Caller should hold exclusive lock on the given object, else
 * this test can be fooled by concurrent additions.
 */
bool
ConstraintNameIsUsed(ConstraintCategory conCat, Oid objId, const char *conname)
{




















}

/*
 * Does any constraint of the given name exist in the given namespace?
 *
 * This is used for code that wants to match ChooseConstraintName's rule
 * that we should avoid autogenerating duplicate constraint names within a
 * namespace.
 */
bool
ConstraintNameExists(const char *conname, Oid namespaceid)
{



















}

/*
 * Select a nonconflicting name for a new constraint.
 *
 * The objective here is to choose a name that is unique within the
 * specified namespace.  Postgres does not require this, but the SQL
 * spec does, and some apps depend on it.  Therefore we avoid choosing
 * default names that so conflict.
 *
 * name1, name2, and label are used the same way as for makeObjectName(),
 * except that the label can't be NULL; digits will be appended to the label
 * if needed to create a name that is unique within the specified namespace.
 *
 * 'others' can be a list of string names already chosen within the current
 * command (but not yet reflected into the catalogs); we will not choose
 * a duplicate of one of these either.
 *
 * Note: it is theoretically possible to get a collision anyway, if someone
 * else chooses the same name concurrently.  This is fairly unlikely to be
 * a problem in practice, especially if one is holding an exclusive lock on
 * the relation identified by name1.
 *
 * Returns a palloc'd string.
 */
char *
ChooseConstraintName(const char *name1, const char *name2, const char *label, Oid namespaceid, List *others)
{























































}

/*
 * Delete a single constraint record.
 */
void
RemoveConstraintById(Oid conId)
{

















































































}

/*
 * RenameConstraintById
 *		Rename a constraint.
 *
 * Note: this isn't intended to be a user-exposed function; it doesn't check
 * permissions etc.  Currently this is only invoked when renaming an index
 * that is associated with a constraint, but it's made a little more general
 * than that with the expectation of someday having ALTER TABLE RENAME
 * CONSTRAINT.
 */
void
RenameConstraintById(Oid conId, const char *newname)
{


































}

/*
 * AlterConstraintNamespaces
 *		Find any constraints belonging to the specified object,
 *		and move them to the specified new namespace.
 *
 * isType indicates whether the owning object is a type or a relation.
 */
void
AlterConstraintNamespaces(Oid ownerId, Oid oldNspId, Oid newNspId, bool isType, ObjectAddresses *objsMoved)
{



















































}

/*
 * ConstraintSetParentConstraint
 *		Set a partition's constraint as child of its parent constraint,
 *		or remove the linkage if parentConstrId is InvalidOid.
 *
 * This updates the constraint's pg_constraint row to show it as inherited, and
 * adds PARTITION dependencies to prevent the constraint from being deleted
 * on its own.  Alternatively, reverse that.
 */
void
ConstraintSetParentConstraint(Oid childConstrId, Oid parentConstrId, Oid childTableId)
{






















































}

/*
 * get_relation_constraint_oid
 *		Find a constraint on the specified relation with the specified
 *name. Returns constraint's OID.
 */
Oid
get_relation_constraint_oid(Oid relid, const char *conname, bool missing_ok)
{































}

/*
 * get_relation_constraint_attnos
 *		Find a constraint on the specified relation with the specified
 *name and return the constrained columns.
 *
 * Returns a Bitmapset of the column attnos of the constrained columns, with
 * attnos being offset by FirstLowInvalidHeapAttributeNumber so that system
 * columns can be represented.
 *
 * *constraintOid is set to the OID of the constraint, or InvalidOid on
 * failure.
 */
Bitmapset *
get_relation_constraint_attnos(Oid relid, const char *conname, bool missing_ok, Oid *constraintOid)
{





























































}

/*
 * Return the OID of the constraint enforced by the given index in the
 * given relation; or InvalidOid if no such index is catalogued.
 *
 * Much like get_constraint_index, this function is concerned only with the
 * one constraint that "owns" the given index.  Therefore, constraints of
 * types other than unique, primary-key, and exclusion are ignored.
 */
Oid
get_relation_idx_constraint_oid(Oid relationId, Oid indexId)
{
































}

/*
 * get_domain_constraint_oid
 *		Find a constraint on the specified domain with the specified
 *name. Returns constraint's OID.
 */
Oid
get_domain_constraint_oid(Oid typid, const char *conname, bool missing_ok)
{































}

/*
 * get_primary_key_attnos
 *		Identify the columns in a relation's primary key, if any.
 *
 * Returns a Bitmapset of the column attnos of the primary key's columns,
 * with attnos being offset by FirstLowInvalidHeapAttributeNumber so that
 * system columns can be represented.
 *
 * If there is no primary key, return NULL.  We also return NULL if the pkey
 * constraint is deferrable and deferrableOk is false.
 *
 * *constraintOid is set to the OID of the pkey constraint, or InvalidOid
 * on failure.
 */
Bitmapset *
get_primary_key_attnos(Oid relid, bool deferrableOk, Oid *constraintOid)
{








































































}

/*
 * Extract data from the pg_constraint tuple of a foreign-key constraint.
 *
 * All arguments save the first are output arguments; the last three of them
 * can be passed as NULL if caller doesn't need them.
 */
void
DeconstructFkConstraintRow(HeapTuple tuple, int *numfks, AttrNumber *conkey, AttrNumber *confkey, Oid *pf_eq_oprs, Oid *pp_eq_oprs, Oid *ff_eq_oprs)
{













































































































}

/*
 * Determine whether a relation can be proven functionally dependent on
 * a set of grouping columns.  If so, return true and add the pg_constraint
 * OIDs of the constraints needed for the proof to the *constraintDeps list.
 *
 * grouping_columns is a list of grouping expressions, in which columns of
 * the rel of interest are Vars with the indicated varno/varlevelsup.
 *
 * Currently we only check to see if the rel has a primary key that is a
 * subset of the grouping_columns.  We could also use plain unique constraints
 * if all their columns are known not null, but there's a problem: we need
 * to be able to represent the not-null-ness as part of the constraints added
 * to *constraintDeps.  FIXME whenever not-null constraints get represented
 * in pg_constraint.
 */
bool
check_functional_grouping(Oid relid, Index varno, Index varlevelsup, List *grouping_columns, List **constraintDeps)
{
































}