/*-------------------------------------------------------------------------
 *
 * aclchk.c
 *	  Routines to check access control permissions.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/aclchk.c
 *
 * NOTES
 *	  See acl.h.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_am.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_init_privs.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_transform.h"
#include "commands/dbcommands.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/proclang.h"
#include "commands/tablespace.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/aclchk_internal.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/*
 * Internal format used by ALTER DEFAULT PRIVILEGES.
 */
typedef struct
{
  Oid roleid; /* owning role */
  Oid nspid;  /* namespace, or InvalidOid if none */
  /* remaining fields are same as in InternalGrant: */
  bool is_grant;
  ObjectType objtype;
  bool all_privs;
  AclMode privileges;
  List *grantees;
  bool grant_option;
  DropBehavior behavior;
} InternalDefaultACL;

/*
 * When performing a binary-upgrade, pg_dump will call a function to set
 * this variable to let us know that we need to populate the pg_init_privs
 * table for the GRANT/REVOKE commands while this variable is set to true.
 */
bool binary_upgrade_record_init_privs = false;

static void
ExecGrantStmt_oids(InternalGrant *istmt);
static void
ExecGrant_Relation(InternalGrant *grantStmt);
static void
ExecGrant_Database(InternalGrant *grantStmt);
static void
ExecGrant_Fdw(InternalGrant *grantStmt);
static void
ExecGrant_ForeignServer(InternalGrant *grantStmt);
static void
ExecGrant_Function(InternalGrant *grantStmt);
static void
ExecGrant_Language(InternalGrant *grantStmt);
static void
ExecGrant_Largeobject(InternalGrant *grantStmt);
static void
ExecGrant_Namespace(InternalGrant *grantStmt);
static void
ExecGrant_Tablespace(InternalGrant *grantStmt);
static void
ExecGrant_Type(InternalGrant *grantStmt);

static void
SetDefaultACLsInSchemas(InternalDefaultACL *iacls, List *nspnames);
static void
SetDefaultACL(InternalDefaultACL *iacls);

static List *
objectNamesToOids(ObjectType objtype, List *objnames);
static List *
objectsInSchemaToOids(ObjectType objtype, List *nspnames);
static List *
getRelationsInNamespace(Oid namespaceId, char relkind);
static void
expand_col_privileges(List *colnames, Oid table_oid, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges);
static void
expand_all_col_privileges(Oid table_oid, Form_pg_class classForm, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges);
static AclMode
string_to_privilege(const char *privname);
static const char *
privilege_to_string(AclMode privilege);
static AclMode
restrict_and_check_grant(bool is_grant, AclMode avail_goptions, bool all_privs, AclMode privileges, Oid objectId, Oid grantorId, ObjectType objtype, const char *objname, AttrNumber att_number, const char *colname);
static AclMode
pg_aclmask(ObjectType objtype, Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mask, AclMaskHow how);
static void
recordExtensionInitPriv(Oid objoid, Oid classoid, int objsubid, Acl *new_acl);
static void
recordExtensionInitPrivWorker(Oid objoid, Oid classoid, int objsubid, Acl *new_acl);

#ifdef ACLDEBUG
static void
dumpacl(Acl *acl)
{
  int i;
  AclItem *aip;

  elog(DEBUG2, "acl size = %d, # acls = %d", ACL_SIZE(acl), ACL_NUM(acl));
  aip = ACL_DAT(acl);
  for (i = 0; i < ACL_NUM(acl); ++i)
  {
    elog(DEBUG2, "	acl[%d]: %s", i, DatumGetCString(DirectFunctionCall1(aclitemout, PointerGetDatum(aip + i))));
  }
}
#endif /* ACLDEBUG */

/*
 * If is_grant is true, adds the given privileges for the list of
 * grantees to the existing old_acl.  If is_grant is false, the
 * privileges for the given grantees are removed from old_acl.
 *
 * NB: the original old_acl is pfree'd.
 */
static Acl *
merge_acl_with_grant(Acl *old_acl, bool is_grant, bool grant_option, DropBehavior behavior, List *grantees, AclMode privileges, Oid grantorId, Oid ownerId)
{




















































}

/*
 * Restrict the privileges to what we can actually grant, and emit
 * the standards-mandated warning and error messages.
 */
static AclMode
restrict_and_check_grant(bool is_grant, AclMode avail_goptions, bool all_privs, AclMode privileges, Oid objectId, Oid grantorId, ObjectType objtype, const char *objname, AttrNumber att_number, const char *colname)
{



































































































































}

/*
 * Called to execute the utility commands GRANT and REVOKE
 */
void
ExecuteGrantStmt(GrantStmt *stmt)
{



















































































































































































}

/*
 * ExecGrantStmt_oids
 *
 * Internal entry point for granting and revoking privileges.
 */
static void
ExecGrantStmt_oids(InternalGrant *istmt)
{


















































}

/*
 * objectNamesToOids
 *
 * Turn a list of object names of a given type into an Oid list.
 *
 * XXX: This function doesn't take any sort of locks on the objects whose
 * names it looks up.  In the face of concurrent DDL, we might easily latch
 * onto an old version of an object, causing the GRANT or REVOKE statement
 * to fail.
 */
static List *
objectNamesToOids(ObjectType objtype, List *objnames)
{







































































































































}

/*
 * objectsInSchemaToOids
 *
 * Find all objects of a given type in specified schemas, and make a list
 * of their Oids.  We check USAGE privilege on the schemas, but there is
 * no privilege checking on the individual objects here.
 */
static List *
objectsInSchemaToOids(ObjectType objtype, List *nspnames)
{









































































}

/*
 * getRelationsInNamespace
 *
 * Return Oid list of relations in given namespace filtered by relation kind
 */
static List *
getRelationsInNamespace(Oid namespaceId, char relkind)
{























}

/*
 * ALTER DEFAULT PRIVILEGES statement
 */
void
ExecAlterDefaultPrivilegesStmt(ParseState *pstate, AlterDefaultPrivilegesStmt *stmt)
{





























































































































































































}

/*
 * Process ALTER DEFAULT PRIVILEGES for a list of target schemas
 *
 * All fields of *iacls except nspid were filled already
 */
static void
SetDefaultACLsInSchemas(InternalDefaultACL *iacls, List *nspnames)
{

































}

/*
 * Create or update a pg_default_acl entry
 */
static void
SetDefaultACL(InternalDefaultACL *iacls)
{
























































































































































































































































}

/*
 * RemoveRoleFromObjectACL
 *
 * Used by shdepDropOwned to remove mentions of a role in ACLs
 */
void
RemoveRoleFromObjectACL(Oid roleid, Oid classid, Oid objid)
{




















































































































}

/*
 * Remove a pg_default_acl entry
 */
void
RemoveDefaultACLById(Oid defaclOid)
{






















}

/*
 * expand_col_privileges
 *
 * OR the specified privilege(s) into per-column array entries for each
 * specified attribute.  The per-column array is indexed starting at
 * FirstLowInvalidHeapAttributeNumber, up to relation's last attribute.
 */
static void
expand_col_privileges(List *colnames, Oid table_oid, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges)
{



















}

/*
 * expand_all_col_privileges
 *
 * OR the specified privilege(s) into per-column array entries for each valid
 * attribute of a relation.  The per-column array is indexed starting at
 * FirstLowInvalidHeapAttributeNumber, up to relation's last attribute.
 */
static void
expand_all_col_privileges(Oid table_oid, Form_pg_class classForm, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges)
{





































}

/*
 *	This processes attributes, but expects to be called from
 *	ExecGrant_Relation, not directly from ExecGrantStmt.
 */
static void
ExecGrant_Attribute(InternalGrant *istmt, Oid relOid, const char *relname, AttrNumber attnum, Oid ownerId, AclMode col_privileges, Relation attRelation, const Acl *old_rel_acl)
{























































































































}

/*
 *	This processes both sequences and non-sequences.
 */
static void
ExecGrant_Relation(InternalGrant *istmt)
{




































































































































































































































































































}

static void
ExecGrant_Database(InternalGrant *istmt)
{








































































































}

static void
ExecGrant_Fdw(InternalGrant *istmt)
{











































































































}

static void
ExecGrant_ForeignServer(InternalGrant *istmt)
{











































































































}

static void
ExecGrant_Function(InternalGrant *istmt)
{











































































































}

static void
ExecGrant_Language(InternalGrant *istmt)
{
















































































































}

static void
ExecGrant_Largeobject(InternalGrant *istmt)
{




















































































































}

static void
ExecGrant_Namespace(InternalGrant *istmt)
{











































































































}

static void
ExecGrant_Tablespace(InternalGrant *istmt)
{








































































































}

static void
ExecGrant_Type(InternalGrant *istmt)
{






















































































































}

static AclMode
string_to_privilege(const char *privname)
{


























































}

static const char *
privilege_to_string(AclMode privilege)
{






























}

/*
 * Standardized reporting of aclcheck permissions failures.
 *
 * Note: we do not double-quote the %s's below, because many callers
 * supply strings that might be already quoted.
 */
void
aclcheck_error(AclResult aclerr, ObjectType objtype, const char *objectname)
{
















































































































































































































































































}

void
aclcheck_error_col(AclResult aclerr, ObjectType objtype, const char *objectname, const char *colname)
{
















}

/*
 * Special common handling for types: use element type instead of array type,
 * and format nicely
 */
void
aclcheck_error_type(AclResult aclerr, Oid typeOid)
{



}

/*
 * Relay for the various pg_*_mask routines depending on object kind
 */
static AclMode
pg_aclmask(ObjectType objtype, Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mask, AclMaskHow how)
{






































}

/* ****************************************************************
 * Exported routines for examining a user's privileges for various objects
 *
 * See aclmask() for a description of the common API for these functions.
 *
 * Note: we give lookup failure the full ereport treatment because the
 * has_xxx_privilege() family of functions allow users to pass any random
 * OID to these functions.
 * ****************************************************************
 */

/*
 * Exported routine for examining a user's privileges for a column
 *
 * Note: this considers only privileges granted specifically on the column.
 * It is caller's responsibility to take relation-level privileges into account
 * as appropriate.  (For the same reason, we have no special case for
 * superuser-ness here.)
 */
AclMode
pg_attribute_aclmask(Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mask, AclMaskHow how)
{









































































}

/*
 * Exported routine for examining a user's privileges for a table
 */
AclMode
pg_class_aclmask(Oid table_oid, Oid roleid, AclMode mask, AclMaskHow how)
{



















































































}

/*
 * Exported routine for examining a user's privileges for a database
 */
AclMode
pg_database_aclmask(Oid db_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
















































}

/*
 * Exported routine for examining a user's privileges for a function
 */
AclMode
pg_proc_aclmask(Oid proc_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
















































}

/*
 * Exported routine for examining a user's privileges for a language
 */
AclMode
pg_language_aclmask(Oid lang_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
















































}

/*
 * Exported routine for examining a user's privileges for a largeobject
 *
 * When a large object is opened for reading, it is opened relative to the
 * caller's snapshot, but when it is opened for writing, a current
 * MVCC snapshot will be used.  See doc/src/sgml/lobj.sgml.  This function
 * takes a snapshot argument so that the permissions check can be made
 * relative to the same snapshot that will be used to read the underlying
 * data.  The caller will actually pass NULL for an instantaneous MVCC
 * snapshot, since all we do with the snapshot argument is pass it through
 * to systable_beginscan().
 */
AclMode
pg_largeobject_aclmask_snapshot(Oid lobj_oid, Oid roleid, AclMode mask, AclMaskHow how, Snapshot snapshot)
{




























































}

/*
 * Exported routine for examining a user's privileges for a namespace
 */
AclMode
pg_namespace_aclmask(Oid nsp_oid, Oid roleid, AclMode mask, AclMaskHow how)
{















































































}

/*
 * Exported routine for examining a user's privileges for a tablespace
 */
AclMode
pg_tablespace_aclmask(Oid spc_oid, Oid roleid, AclMode mask, AclMaskHow how)
{

















































}

/*
 * Exported routine for examining a user's privileges for a foreign
 * data wrapper
 */
AclMode
pg_foreign_data_wrapper_aclmask(Oid fdw_oid, Oid roleid, AclMode mask, AclMaskHow how)
{






















































}

/*
 * Exported routine for examining a user's privileges for a foreign
 * server.
 */
AclMode
pg_foreign_server_aclmask(Oid srv_oid, Oid roleid, AclMode mask, AclMaskHow how)
{






















































}

/*
 * Exported routine for examining a user's privileges for a type.
 */
AclMode
pg_type_aclmask(Oid type_oid, Oid roleid, AclMode mask, AclMaskHow how)
{









































































}

/*
 * Exported routine for checking a user's access privileges to a column
 *
 * Returns ACLCHECK_OK if the user has any of the privileges identified by
 * 'mode'; otherwise returns a suitable error code (in practice, always
 * ACLCHECK_NO_PRIV).
 *
 * As with pg_attribute_aclmask, only privileges granted directly on the
 * column are considered here.
 */
AclResult
pg_attribute_aclcheck(Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to any/all columns
 *
 * If 'how' is ACLMASK_ANY, then returns ACLCHECK_OK if user has any of the
 * privileges identified by 'mode' on any non-dropped column in the relation;
 * otherwise returns a suitable error code (in practice, always
 * ACLCHECK_NO_PRIV).
 *
 * If 'how' is ACLMASK_ALL, then returns ACLCHECK_OK if user has any of the
 * privileges identified by 'mode' on each non-dropped column in the relation
 * (and there must be at least one such column); otherwise returns a suitable
 * error code (in practice, always ACLCHECK_NO_PRIV).
 *
 * As with pg_attribute_aclmask, only privileges granted directly on the
 * column(s) are considered here.
 *
 * Note: system columns are not considered here; there are cases where that
 * might be appropriate but there are also cases where it wouldn't.
 */
AclResult
pg_attribute_aclcheck_all(Oid table_oid, Oid roleid, AclMode mode, AclMaskHow how)
{

















































































}

/*
 * Exported routine for checking a user's access privileges to a table
 *
 * Returns ACLCHECK_OK if the user has any of the privileges identified by
 * 'mode'; otherwise returns a suitable error code (in practice, always
 * ACLCHECK_NO_PRIV).
 */
AclResult
pg_class_aclcheck(Oid table_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a database
 */
AclResult
pg_database_aclcheck(Oid db_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a function
 */
AclResult
pg_proc_aclcheck(Oid proc_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a language
 */
AclResult
pg_language_aclcheck(Oid lang_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a largeobject
 */
AclResult
pg_largeobject_aclcheck_snapshot(Oid lobj_oid, Oid roleid, AclMode mode, Snapshot snapshot)
{








}

/*
 * Exported routine for checking a user's access privileges to a namespace
 */
AclResult
pg_namespace_aclcheck(Oid nsp_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a tablespace
 */
AclResult
pg_tablespace_aclcheck(Oid spc_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a foreign
 * data wrapper
 */
AclResult
pg_foreign_data_wrapper_aclcheck(Oid fdw_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a foreign
 * server
 */
AclResult
pg_foreign_server_aclcheck(Oid srv_oid, Oid roleid, AclMode mode)
{








}

/*
 * Exported routine for checking a user's access privileges to a type
 */
AclResult
pg_type_aclcheck(Oid type_oid, Oid roleid, AclMode mode)
{








}

/*
 * Ownership check for a relation (specified by OID).
 */
bool
pg_class_ownercheck(Oid class_oid, Oid roleid)
{




















}

/*
 * Ownership check for a type (specified by OID).
 */
bool
pg_type_ownercheck(Oid type_oid, Oid roleid)
{




















}

/*
 * Ownership check for an operator (specified by OID).
 */
bool
pg_oper_ownercheck(Oid oper_oid, Oid roleid)
{




















}

/*
 * Ownership check for a function (specified by OID).
 */
bool
pg_proc_ownercheck(Oid proc_oid, Oid roleid)
{




















}

/*
 * Ownership check for a procedural language (specified by OID)
 */
bool
pg_language_ownercheck(Oid lan_oid, Oid roleid)
{




















}

/*
 * Ownership check for a largeobject (specified by OID)
 *
 * This is only used for operations like ALTER LARGE OBJECT that are always
 * relative to an up-to-date snapshot.
 */
bool
pg_largeobject_ownercheck(Oid lobj_oid, Oid roleid)
{































}

/*
 * Ownership check for a namespace (specified by OID).
 */
bool
pg_namespace_ownercheck(Oid nsp_oid, Oid roleid)
{




















}

/*
 * Ownership check for a tablespace (specified by OID).
 */
bool
pg_tablespace_ownercheck(Oid spc_oid, Oid roleid)
{





















}

/*
 * Ownership check for an operator class (specified by OID).
 */
bool
pg_opclass_ownercheck(Oid opc_oid, Oid roleid)
{




















}

/*
 * Ownership check for an operator family (specified by OID).
 */
bool
pg_opfamily_ownercheck(Oid opf_oid, Oid roleid)
{




















}

/*
 * Ownership check for a text search dictionary (specified by OID).
 */
bool
pg_ts_dict_ownercheck(Oid dict_oid, Oid roleid)
{




















}

/*
 * Ownership check for a text search configuration (specified by OID).
 */
bool
pg_ts_config_ownercheck(Oid cfg_oid, Oid roleid)
{




















}

/*
 * Ownership check for a foreign-data wrapper (specified by OID).
 */
bool
pg_foreign_data_wrapper_ownercheck(Oid srv_oid, Oid roleid)
{




















}

/*
 * Ownership check for a foreign server (specified by OID).
 */
bool
pg_foreign_server_ownercheck(Oid srv_oid, Oid roleid)
{




















}

/*
 * Ownership check for an event trigger (specified by OID).
 */
bool
pg_event_trigger_ownercheck(Oid et_oid, Oid roleid)
{




















}

/*
 * Ownership check for a database (specified by OID).
 */
bool
pg_database_ownercheck(Oid db_oid, Oid roleid)
{




















}

/*
 * Ownership check for a collation (specified by OID).
 */
bool
pg_collation_ownercheck(Oid coll_oid, Oid roleid)
{




















}

/*
 * Ownership check for a conversion (specified by OID).
 */
bool
pg_conversion_ownercheck(Oid conv_oid, Oid roleid)
{




















}

/*
 * Ownership check for an extension (specified by OID).
 */
bool
pg_extension_ownercheck(Oid ext_oid, Oid roleid)
{































}

/*
 * Ownership check for a publication (specified by OID).
 */
bool
pg_publication_ownercheck(Oid pub_oid, Oid roleid)
{




















}

/*
 * Ownership check for a subscription (specified by OID).
 */
bool
pg_subscription_ownercheck(Oid sub_oid, Oid roleid)
{




















}

/*
 * Ownership check for a statistics object (specified by OID).
 */
bool
pg_statistics_object_ownercheck(Oid stat_oid, Oid roleid)
{




















}

/*
 * Check whether specified role has CREATEROLE privilege (or is a superuser)
 *
 * Note: roles do not have owners per se; instead we use this test in
 * places where an ownership-like permissions test is needed for a role.
 * Be sure to apply it to the role trying to do the operation, not the
 * role being operated on!	Also note that this generally should not be
 * considered enough privilege if the target role is a superuser.
 * (We don't handle that consideration here because we want to give a
 * separate error message for such cases, so the caller has to deal with it.)
 */
bool
has_createrole_privilege(Oid roleid)
{
















}

bool
has_bypassrls_privilege(Oid roleid)
{
















}

/*
 * Fetch pg_default_acl entry for given role, namespace and object type
 * (object type must be given in pg_default_acl's encoding).
 * Returns NULL if no such entry.
 */
static Acl *
get_default_acl_internal(Oid roleId, Oid nsp_oid, char objtype)
{



















}

/*
 * Get default permissions for newly created object within given schema
 *
 * Returns NULL if built-in system defaults should be used.
 *
 * If the result is not NULL, caller must call recordDependencyOnNewAcl
 * once the OID of the new object is known.
 */
Acl *
get_user_default_acl(ObjectType objtype, Oid ownerId, Oid nsp_oid)
{












































































}

/*
 * Record dependencies on roles mentioned in a new object's ACL.
 */
void
recordDependencyOnNewAcl(Oid classId, Oid objectId, int32 objsubId, Oid ownerId, Acl *acl)
{














}

/*
 * Record initial privileges for the top-level object passed in.
 *
 * For the object passed in, this will record its ACL (if any) and the ACLs of
 * any sub-objects (eg: columns) into pg_init_privs.
 *
 * Any new kinds of objects which have ACLs associated with them and can be
 * added to an extension should be added to the if-else tree below.
 */
void
recordExtObjInitPriv(Oid objoid, Oid classoid)
{















































































































































































































































































}

/*
 * For the object passed in, remove its ACL and the ACLs of any object subIds
 * from pg_init_privs (via recordExtensionInitPrivWorker()).
 */
void
removeExtObjInitPriv(Oid objoid, Oid classoid)
{































































}

/*
 * Record initial ACL for an extension object
 *
 * Can be called at any time, we check if 'creating_extension' is set and, if
 * not, exit immediately.
 *
 * Pass in the object OID, the OID of the class (the OID of the table which
 * the object is defined in) and the 'sub' id of the object (objsubid), if
 * any.  If there is no 'sub' id (they are currently only used for columns of
 * tables) then pass in '0'.  Finally, pass in the complete ACL to store.
 *
 * If an ACL already exists for this object/sub-object then we will replace
 * it with what is passed in.
 *
 * Passing in NULL for 'new_acl' will result in the entry for the object being
 * removed, if one is found.
 */
static void
recordExtensionInitPriv(Oid objoid, Oid classoid, int objsubid, Acl *new_acl)
{














}

/*
 * Record initial ACL for an extension object, worker.
 *
 * This will perform a wholesale replacement of the entire ACL for the object
 * passed in, therefore be sure to pass in the complete new ACL to use.
 *
 * Generally speaking, do *not* use this function directly but instead use
 * recordExtensionInitPriv(), which checks if 'creating_extension' is set.
 * This function does *not* check if 'creating_extension' is set as it is also
 * used when an object is added to or removed from an extension via ALTER
 * EXTENSION ... ADD/DROP.
 */
static void
recordExtensionInitPrivWorker(Oid objoid, Oid classoid, int objsubid, Acl *new_acl)
{

















































































}