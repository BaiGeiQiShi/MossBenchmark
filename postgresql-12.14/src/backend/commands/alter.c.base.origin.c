/*-------------------------------------------------------------------------
 *
 * alter.c
 *	  Drivers for generic alter commands
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/alter.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "commands/alter.h"
#include "commands/collationcmds.h"
#include "commands/conversioncmds.h"
#include "commands/dbcommands.h"
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
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "commands/user.h"
#include "parser/parse_func.h"
#include "miscadmin.h"
#include "rewrite/rewriteDefine.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
AlterObjectNamespace_internal(Relation rel, Oid objid, Oid nspOid);

/*
 * Raise an error to the effect that an object of the given name is already
 * present in the given namespace.
 */
static void
report_name_conflict(Oid classId, const char *name)
{




























}

static void
report_namespace_conflict(Oid classId, const char *name, Oid nspOid)
{




































}

/*
 * AlterObjectRename_internal
 *
 * Generic function to rename the given object, for simple cases (won't
 * work for tables, nor other cases where we need to do more than change
 * the name column of a single catalog entry).
 *
 * rel: catalog relation containing object (RowExclusiveLock'd by caller)
 * objectId: OID of object to be renamed
 * new_name: CString representation of new name
 */
static void
AlterObjectRename_internal(Relation rel, Oid objectId, const char *new_name)
{


























































































































































}

/*
 * Executes an ALTER OBJECT / RENAME TO statement.  Based on the object
 * type, the function appropriate to that type is executed.
 *
 * Return value is the address of the renamed object.
 */
ObjectAddress
ExecRenameStmt(RenameStmt *stmt)
{

















































































}

/*
 * Executes an ALTER OBJECT / DEPENDS ON [EXTENSION] statement.
 *
 * Return value is the address of the altered object.  refAddress is an output
 * argument which, if not null, receives the address of the object that the
 * altered object now depends on.
 */
ObjectAddress
ExecAlterObjectDependsStmt(AlterObjectDependsStmt *stmt, ObjectAddress *refAddress)
{









































}

/*
 * Executes an ALTER OBJECT / SET SCHEMA statement.  Based on the object
 * type, the function appropriate to that type is executed.
 *
 * Return value is that of the altered object.
 *
 * oldSchemaAddr is an output argument which, if not NULL, is set to the object
 * address of the original schema.
 */
ObjectAddress
ExecAlterObjectSchemaStmt(AlterObjectSchemaStmt *stmt, ObjectAddress *oldSchemaAddr)
{

































































}

/*
 * Change an object's namespace given its classOid and object Oid.
 *
 * Objects that don't have a namespace should be ignored.
 *
 * This function is currently used only by ALTER EXTENSION SET SCHEMA,
 * so it only needs to cover object types that can be members of an
 * extension, and it doesn't have to deal with certain special cases
 * such as not wanting to process array types --- those should never
 * be direct members of an extension anyway.  Nonetheless, we insist
 * on listing all OCLASS types in the switch.
 *
 * Returns the OID of the object's previous namespace, or InvalidOid if
 * object doesn't have a schema.
 */
Oid
AlterObjectNamespace_oid(Oid classId, Oid objid, Oid nspOid, ObjectAddresses *objsMoved)
{



















































































}

/*
 * Generic function to change the namespace of a given object, for simple
 * cases (won't work for tables, nor other cases where we need to do more
 * than change the namespace column of a single catalog entry).
 *
 * rel: catalog relation containing object (RowExclusiveLock'd by caller)
 * objid: OID of object to change the namespace of
 * nspOid: OID of new namespace
 *
 * Returns the OID of the object's previous namespace.
 */
static Oid
AlterObjectNamespace_internal(Relation rel, Oid objid, Oid nspOid)
{






























































































































}

/*
 * Executes an ALTER OBJECT / OWNER TO statement.  Based on the object
 * type, the function appropriate to that type is executed.
 */
ObjectAddress
ExecAlterOwnerStmt(AlterOwnerStmt *stmt)
{















































































}

/*
 * Generic function to change the ownership of a given object, for simple
 * cases (won't work for tables, nor other cases where we need to do more than
 * change the ownership column of a single catalog entry).
 *
 * rel: catalog relation containing object (RowExclusiveLock'd by caller)
 * objectId: OID of object to change the ownership of
 * new_ownerId: OID of new object owner
 */
void
AlterObjectOwner_internal(Relation rel, Oid objectId, Oid new_ownerId)
{























































































































}