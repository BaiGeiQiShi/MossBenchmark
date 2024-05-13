/*-------------------------------------------------------------------------
 *
 * pg_depend.c
 *	  routines to support manipulation of the pg_depend relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_depend.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_extension.h"
#include "commands/extension.h"
#include "miscadmin.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

static bool
isObjectPinned(const ObjectAddress *object, Relation rel);

/*
 * Record a dependency between 2 objects via their respective objectAddress.
 * The first argument is the dependent object, the second the one it
 * references.
 *
 * This simply creates an entry in pg_depend, without any other processing.
 */
void
recordDependencyOn(const ObjectAddress *depender, const ObjectAddress *referenced, DependencyType behavior)
{

}

/*
 * Record multiple dependencies (of the same kind) for a single dependent
 * object.  This has a little less overhead than recording each separately.
 */
void
recordMultipleDependencies(const ObjectAddress *depender, const ObjectAddress *referenced, int nreferenced, DependencyType behavior)
{







































































}

/*
 * If we are executing a CREATE EXTENSION operation, mark the given object
 * as being a member of the extension, or check that it already is one.
 * Otherwise, do nothing.
 *
 * This must be called during creation of any user-definable object type
 * that could be a member of an extension.
 *
 * isReplace must be true if the object already existed, and false if it is
 * newly created.  In the former case we insist that it already be a member
 * of the current extension.  In the latter case we can skip checking whether
 * it is already a member of any extension.
 *
 * Note: isReplace = true is typically used when updating a object in
 * CREATE OR REPLACE and similar commands.  We used to allow the target
 * object to not already be an extension member, instead silently absorbing
 * it into the current extension.  However, this was both error-prone
 * (extensions might accidentally overwrite free-standing objects) and
 * a security hazard (since the object would retain its previous ownership).
 */
void
recordDependencyOnCurrentExtension(const ObjectAddress *object, bool isReplace)
{








































}

/*
 * If we are executing a CREATE EXTENSION operation, check that the given
 * object is a member of the extension, and throw an error if it isn't.
 * Otherwise, do nothing.
 *
 * This must be called whenever a CREATE IF NOT EXISTS operation (for an
 * object type that can be an extension member) has found that an object of
 * the desired name already exists.  It is insecure for an extension to use
 * IF NOT EXISTS except when the conflicting object is already an extension
 * member; otherwise a hostile user could substitute an object with arbitrary
 * properties.
 */
void
checkMembershipInCurrentExtension(const ObjectAddress *object)
{























}

/*
 * deleteDependencyRecordsFor -- delete all records with given depender
 * classId/objectId.  Returns the number of records deleted.
 *
 * This is used when redefining an existing object.  Links leading to the
 * object do not change, and links leading from it will be recreated
 * (possibly with some differences from before).
 *
 * If skipExtensionDeps is true, we do not delete any dependencies that
 * show that the given object is a member of an extension.  This avoids
 * needing a lot of extra logic to fetch and recreate that dependency.
 */
long
deleteDependencyRecordsFor(Oid classId, Oid objectId, bool skipExtensionDeps)
{





























}

/*
 * deleteDependencyRecordsForClass -- delete all records with given depender
 * classId/objectId, dependee classId, and deptype.
 * Returns the number of records deleted.
 *
 * This is a variant of deleteDependencyRecordsFor, useful when revoking
 * an object property that is expressed by a dependency record (such as
 * extension membership).
 */
long
deleteDependencyRecordsForClass(Oid classId, Oid objectId, Oid refclassId, char deptype)
{





























}

/*
 * Adjust dependency record(s) to point to a different object of the same type
 *
 * classId/objectId specify the referencing object.
 * refClassId/oldRefObjectId specify the old referenced object.
 * newRefObjectId is the new referenced object (must be of class refClassId).
 *
 * Note the lack of objsubid parameters.  If there are subobject references
 * they will all be readjusted.  Also, there is an expectation that we are
 * dealing with NORMAL dependencies: if we have to replace an (implicit)
 * dependency on a pinned object with an explicit dependency on an unpinned
 * one, the new one will be NORMAL.
 *
 * Returns the number of records updated -- zero indicates a problem.
 */
long
changeDependencyFor(Oid classId, Oid objectId, Oid refClassId, Oid oldRefObjectId, Oid newRefObjectId)
{



























































































}

/*
 * Adjust all dependency records to come from a different object of the same
 * type
 *
 * classId/oldObjectId specify the old referencing object.
 * newObjectId is the new referencing object (must be of class classId).
 *
 * Returns the number of records updated.
 */
long
changeDependenciesOf(Oid classId, Oid oldObjectId, Oid newObjectId)
{



































}

/*
 * Adjust all dependency records to point to a different object of the same type
 *
 * refClassId/oldRefObjectId specify the old referenced object.
 * newRefObjectId is the new referenced object (must be of class refClassId).
 *
 * Returns the number of records updated.
 */
long
changeDependenciesOn(Oid refClassId, Oid oldRefObjectId, Oid newRefObjectId)
{




































































}

/*
 * isObjectPinned()
 *
 * Test if an object is required for basic database functionality.
 * Caller must already have opened pg_depend.
 *
 * The passed subId, if any, is ignored; we assume that only whole objects
 * are pinned (and that this implies pinning their components).
 */
static bool
isObjectPinned(const ObjectAddress *object, Relation rel)
{































}

/*
 * Various special-purpose lookups and manipulations of pg_depend.
 */

/*
 * Find the extension containing the specified object, if any
 *
 * Returns the OID of the extension, or InvalidOid if the object does not
 * belong to any extension.
 *
 * Extension membership is marked by an EXTENSION dependency from the object
 * to the extension.  Note that the result will be indeterminate if pg_depend
 * contains links from this object to more than one extension ... but that
 * should never happen.
 */
Oid
getExtensionOfObject(Oid classId, Oid objectId)
{





























}

/*
 * Return (possibly NIL) list of extensions that the given object depends on
 * in DEPENDENCY_AUTO_EXTENSION mode.
 */
List *
getAutoExtensionsOfObject(Oid classId, Oid objectId)
{




























}

/*
 * Detect whether a sequence is marked as "owned" by a column
 *
 * An ownership marker is an AUTO or INTERNAL dependency from the sequence to
 * the column.  If we find one, store the identity of the owning column into
 * *tableId and *colId and return true; else return false.
 *
 * Note: if there's more than one such pg_depend entry then you get
 * a random one of them returned into the out parameters.  This should
 * not happen, though.
 */
bool
sequenceIsOwned(Oid seqId, char deptype, Oid *tableId, int32 *colId)
{































}

/*
 * Collect a list of OIDs of all sequences owned by the specified relation,
 * and column if specified.
 */
List *
getOwnedSequences(Oid relid, AttrNumber attnum)
{





































}

/*
 * Get owned sequence, error if not exactly one.
 */
Oid
getOwnedSequence(Oid relid, AttrNumber attnum)
{












}

/*
 * get_constraint_index
 *		Given the OID of a unique, primary-key, or exclusion constraint,
 *		return the OID of the underlying index.
 *
 * Return InvalidOid if the index couldn't be found; this suggests the
 * given OID is bogus, but we leave it to caller to decide what to do.
 */
Oid
get_constraint_index(Oid constraintId)
{













































}

/*
 * get_index_constraint
 *		Given the OID of an index, return the OID of the owning unique,
 *		primary-key, or exclusion constraint, or InvalidOid if there
 *		is no owning constraint.
 */
Oid
get_index_constraint(Oid indexId)
{


































}

/*
 * get_index_ref_constraints
 *		Given the OID of an index, return the OID of all foreign key
 *		constraints which reference the index.
 */
List *
get_index_ref_constraints(Oid indexId)
{

































}