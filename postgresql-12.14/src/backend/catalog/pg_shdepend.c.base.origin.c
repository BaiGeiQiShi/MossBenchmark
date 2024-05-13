/*-------------------------------------------------------------------------
 *
 * pg_shdepend.c
 *	  routines to support manipulation of the pg_shdepend relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_shdepend.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_shdepend.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "commands/alter.h"
#include "commands/dbcommands.h"
#include "commands/collationcmds.h"
#include "commands/conversioncmds.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/policy.h"
#include "commands/proclang.h"
#include "commands/publicationcmds.h"
#include "commands/schemacmds.h"
#include "commands/subscriptioncmds.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/typecmds.h"
#include "storage/lmgr.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

typedef enum
{
  LOCAL_OBJECT,
  SHARED_OBJECT,
  REMOTE_OBJECT
} SharedDependencyObjectType;

typedef struct
{
  ObjectAddress object;
  char deptype;
  SharedDependencyObjectType objtype;
} ShDependObjectInfo;

static void
getOidListDiff(Oid *list1, int *nlist1, Oid *list2, int *nlist2);
static Oid
classIdGetDbId(Oid classId);
static void
shdepChangeDep(Relation sdepRel, Oid classid, Oid objid, int32 objsubid, Oid refclassid, Oid refobjid, SharedDependencyType deptype);
static void
shdepAddDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, Oid refclassId, Oid refobjId, SharedDependencyType deptype);
static void
shdepDropDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, bool drop_subobjects, Oid refclassId, Oid refobjId, SharedDependencyType deptype);
static void
storeObjectDescription(StringInfo descs, SharedDependencyObjectType type, ObjectAddress *object, SharedDependencyType deptype, int count);
static bool
isSharedObjectPinned(Oid classId, Oid objectId, Relation sdepRel);

/*
 * recordSharedDependencyOn
 *
 * Record a dependency between 2 objects via their respective ObjectAddresses.
 * The first argument is the dependent object, the second the one it
 * references (which must be a shared object).
 *
 * This locks the referenced object and makes sure it still exists.
 * Then it creates an entry in pg_shdepend.  The lock is kept until
 * the end of the transaction.
 *
 * Dependencies on pinned objects are not recorded.
 */
void
recordSharedDependencyOn(ObjectAddress *depender, ObjectAddress *referenced, SharedDependencyType deptype)
{


























}

/*
 * recordDependencyOnOwner
 *
 * A convenient wrapper of recordSharedDependencyOn -- register the specified
 * user as owner of the given object.
 *
 * Note: it's the caller's responsibility to ensure that there isn't an owner
 * entry for the object already.
 */
void
recordDependencyOnOwner(Oid classId, Oid objectId, Oid owner)
{











}

/*
 * shdepChangeDep
 *
 * Update shared dependency records to account for an updated referenced
 * object.  This is an internal workhorse for operations such as changing
 * an object's owner.
 *
 * There must be no more than one existing entry for the given dependent
 * object and dependency type!	So in practice this can only be used for
 * updating SHARED_DEPENDENCY_OWNER and SHARED_DEPENDENCY_TABLESPACE
 * entries, which should have that property.
 *
 * If there is no previous entry, we assume it was referencing a PINned
 * object, so we create a new entry.  If the new referenced object is
 * PINned, we don't create an entry (and drop the old one, if any).
 * (For tablespaces, we don't record dependencies in certain cases, so
 * there are other possible reasons for entries to be missing.)
 *
 * sdepRel must be the pg_shdepend relation, already opened and suitably
 * locked.
 */
static void
shdepChangeDep(Relation sdepRel, Oid classid, Oid objid, int32 objsubid, Oid refclassid, Oid refobjid, SharedDependencyType deptype)
{























































































}

/*
 * changeDependencyOnOwner
 *
 * Update the shared dependencies to account for the new owner.
 *
 * Note: we don't need an objsubid argument because only whole objects
 * have owners.
 */
void
changeDependencyOnOwner(Oid classId, Oid objectId, Oid newOwnerId)
{





























}

/*
 * recordDependencyOnTablespace
 *
 * A convenient wrapper of recordSharedDependencyOn -- register the specified
 * tablespace as default for the given object.
 *
 * Note: it's the caller's responsibility to ensure that there isn't a
 * tablespace entry for the object already.
 */
void
recordDependencyOnTablespace(Oid classId, Oid objectId, Oid tablespace)
{






}

/*
 * changeDependencyOnTablespace
 *
 * Update the shared dependencies to account for the new tablespace.
 *
 * Note: we don't need an objsubid argument because only whole objects
 * have tablespaces.
 */
void
changeDependencyOnTablespace(Oid classId, Oid objectId, Oid newTablespaceId)
{














}

/*
 * getOidListDiff
 *		Helper for updateAclDependencies.
 *
 * Takes two Oid arrays and removes elements that are common to both arrays,
 * leaving just those that are in one input but not the other.
 * We assume both arrays have been sorted and de-duped.
 */
static void
getOidListDiff(Oid *list1, int *nlist1, Oid *list2, int *nlist2)
{





































}

/*
 * updateAclDependencies
 *		Update the pg_shdepend info for an object's ACL during
 *GRANT/REVOKE.
 *
 * classId, objectId, objsubId: identify the object whose ACL this is
 * ownerId: role owning the object
 * noldmembers, oldmembers: array of roleids appearing in old ACL
 * nnewmembers, newmembers: array of roleids appearing in new ACL
 *
 * We calculate the differences between the new and old lists of roles,
 * and then insert or delete from pg_shdepend as appropriate.
 *
 * Note that we can't just insert all referenced roles blindly during GRANT,
 * because we would end up with duplicate registered dependencies.  We could
 * check for existence of the tuples before inserting, but that seems to be
 * more expensive than what we are doing here.  Likewise we can't just delete
 * blindly during REVOKE, because the user may still have other privileges.
 * It is also possible that REVOKE actually adds dependencies, due to
 * instantiation of a formerly implicit default ACL (although at present,
 * all such dependencies should be for the owning role, which we ignore here).
 *
 * NOTE: Both input arrays must be sorted and de-duped.  (Typically they
 * are extracted from an ACL array by aclmembers(), which takes care of
 * both requirements.)	The arrays are pfreed before return.
 */
void
updateAclDependencies(Oid classId, Oid objectId, int32 objsubId, Oid ownerId, int noldmembers, Oid *oldmembers, int nnewmembers, Oid *newmembers)
{







































































}

/*
 * A struct to keep track of dependencies found in other databases.
 */
typedef struct
{
  Oid dbOid;
  int count;
} remoteDep;

/*
 * qsort comparator for ShDependObjectInfo items
 */
static int
shared_dependency_comparator(const void *a, const void *b)
{


























































}

/*
 * checkSharedDependencies
 *
 * Check whether there are shared dependency entries for a given shared
 * object; return true if so.
 *
 * In addition, return a string containing a newline-separated list of object
 * descriptions that depend on the shared object, or NULL if none is found.
 * We actually return two such strings; the "detail" result is suitable for
 * returning to the client as an errdetail() string, and is limited in size.
 * The "detail_log" string is potentially much longer, and should be emitted
 * to the server log only.
 *
 * We can find three different kinds of dependencies: dependencies on objects
 * of the current database; dependencies on shared objects; and dependencies
 * on objects local to other databases.  We can (and do) provide descriptions
 * of the two former kinds of objects, but we can't do that for "remote"
 * objects, so we just provide a count of them.
 *
 * If we find a SHARED_DEPENDENCY_PIN entry, we can error out early.
 */
bool
checkSharedDependencies(Oid classId, Oid objectId, char **detail_msg, char **detail_log_msg)
{




















































































































































































}

/*
 * copyTemplateDependencies
 *
 * Routine to create the initial shared dependencies of a new database.
 * We simply copy the dependencies from the template database.
 */
void
copyTemplateDependencies(Oid templateDbId, Oid newDbId)
{

















































}

/*
 * dropDatabaseDependencies
 *
 * Delete pg_shdepend entries corresponding to a database that's being
 * dropped.
 */
void
dropDatabaseDependencies(Oid databaseId)
{



























}

/*
 * deleteSharedDependencyRecordsFor
 *
 * Delete all pg_shdepend entries corresponding to an object that's being
 * dropped or modified.  The object is assumed to be either a shared object
 * or local to the current database (the classId tells us which).
 *
 * If objectSubId is zero, we are deleting a whole object, so get rid of
 * pg_shdepend entries for subobjects as well.
 */
void
deleteSharedDependencyRecordsFor(Oid classId, Oid objectId, int32 objectSubId)
{







}

/*
 * shdepAddDependency
 *		Internal workhorse for inserting into pg_shdepend
 *
 * sdepRel must be the pg_shdepend relation, already opened and suitably
 * locked.
 */
static void
shdepAddDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, Oid refclassId, Oid refobjId, SharedDependencyType deptype)
{































}

/*
 * shdepDropDependency
 *		Internal workhorse for deleting entries from pg_shdepend.
 *
 * We drop entries having the following properties:
 *	dependent object is the one identified by classId/objectId/objsubId
 *	if refclassId isn't InvalidOid, it must match the entry's refclassid
 *	if refobjId isn't InvalidOid, it must match the entry's refobjid
 *	if deptype isn't SHARED_DEPENDENCY_INVALID, it must match entry's
 *deptype
 *
 * If drop_subobjects is true, we ignore objsubId and consider all entries
 * matching classId/objectId.
 *
 * sdepRel must be the pg_shdepend relation, already opened and suitably
 * locked.
 */
static void
shdepDropDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, bool drop_subobjects, Oid refclassId, Oid refobjId, SharedDependencyType deptype)
{












































}

/*
 * classIdGetDbId
 *
 * Get the database Id that should be used in pg_shdepend, given the OID
 * of the catalog containing the object.  For shared objects, it's 0
 * (InvalidOid); for all other objects, it's the current database Id.
 */
static Oid
classIdGetDbId(Oid classId)
{












}

/*
 * shdepLockAndCheckObject
 *
 * Lock the object that we are about to record a dependency on.
 * After it's locked, verify that it hasn't been dropped while we
 * weren't looking.  If the object has been dropped, this function
 * does not return!
 */
void
shdepLockAndCheckObject(Oid classId, Oid objectId)
{









































}

/*
 * storeObjectDescription
 *		Append the description of a dependent object to "descs"
 *
 * While searching for dependencies of a shared object, we stash the
 * descriptions of dependent objects we find in a single string, which we
 * later pass to ereport() in the DETAIL field when somebody attempts to
 * drop a referenced shared object.
 *
 * When type is LOCAL_OBJECT or SHARED_OBJECT, we expect object to be the
 * dependent object, deptype is the dependency type, and count is not used.
 * When type is REMOTE_OBJECT, we expect object to be the database object,
 * and count to be nonzero; deptype is not used in this case.
 */
static void
storeObjectDescription(StringInfo descs, SharedDependencyObjectType type, ObjectAddress *object, SharedDependencyType deptype, int count)
{




















































}

/*
 * isSharedObjectPinned
 *		Return whether a given shared object has a SHARED_DEPENDENCY_PIN
 *entry.
 *
 * sdepRel must be the pg_shdepend relation, already opened and suitably
 * locked.
 */
static bool
isSharedObjectPinned(Oid classId, Oid objectId, Relation sdepRel)
{






























}

/*
 * shdepDropOwned
 *
 * Drop the objects owned by any one of the given RoleIds.  If a role has
 * access to an object, the grant will be removed as well (but the object
 * will not, of course).
 *
 * We can revoke grants immediately while doing the scan, but drops are
 * saved up and done all at once with performMultipleDeletions.  This
 * is necessary so that we don't get failures from trying to delete
 * interdependent objects in the wrong order.
 */
void
shdepDropOwned(List *roleids, DropBehavior behavior)
{































































































































}

/*
 * shdepReassignOwned
 *
 * Change the owner of objects owned by any of the roles in roleids to
 * newrole.  Grants are not touched.
 */
void
shdepReassignOwned(List *roleids, Oid newrole)
{

















































































































































































}