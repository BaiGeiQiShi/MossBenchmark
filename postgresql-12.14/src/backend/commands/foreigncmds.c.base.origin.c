/*-------------------------------------------------------------------------
 *
 * foreigncmds.c
 *	  foreign-data wrapper/server creation/manipulation commands
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/foreigncmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "commands/defrem.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  char *tablename;
  char *cmd;
} import_error_callback_arg;

/* Internal functions */
static void
import_error_callback(void *arg);

/*
 * Convert a DefElem list to the text array format that is used in
 * pg_foreign_data_wrapper, pg_foreign_server, pg_user_mapping, and
 * pg_foreign_table.
 *
 * Returns the array in the form of a Datum, or PointerGetDatum(NULL)
 * if the list is empty.
 *
 * Note: The array is usually stored to database without further
 * processing, hence any validation should be done before this
 * conversion.
 */
static Datum
optionListToArray(List *options)
{

























}

/*
 * Transform a list of DefElem into text array format.  This is substantially
 * the same thing as optionListToArray(), except we recognize SET/ADD/DROP
 * actions for modifying an existing list of options, which is passed in
 * Datum form as oldOptions.  Also, if fdwvalidator isn't InvalidOid
 * it specifies a validator function to call on the result.
 *
 * Returns the array in the form of a Datum, or PointerGetDatum(NULL)
 * if the list is empty.
 *
 * This is used by CREATE/ALTER of FOREIGN DATA WRAPPER/SERVER/USER MAPPING/
 * FOREIGN TABLE.
 */
Datum
transformGenericOptions(Oid catalogId, Datum oldOptions, List *options, Oid fdwvalidator)
{





















































































}

/*
 * Internal workhorse for changing a data wrapper's owner.
 *
 * Allow this only for superusers; also the new owner must be a
 * superuser.
 */
static void
AlterForeignDataWrapperOwner_internal(Relation rel, HeapTuple tup, Oid newOwnerId)
{
















































}

/*
 * Change foreign-data wrapper owner -- by name
 *
 * Note restrictions in the "_internal" function, above.
 */
ObjectAddress
AlterForeignDataWrapperOwner(const char *name, Oid newOwnerId)
{



























}

/*
 * Change foreign-data wrapper owner -- by OID
 *
 * Note restrictions in the "_internal" function, above.
 */
void
AlterForeignDataWrapperOwner_oid(Oid fwdId, Oid newOwnerId)
{

















}

/*
 * Internal workhorse for changing a foreign server's owner
 */
static void
AlterForeignServerOwner_internal(Relation rel, HeapTuple tup, Oid newOwnerId)
{































































}

/*
 * Change foreign server owner -- by name
 */
ObjectAddress
AlterForeignServerOwner(const char *name, Oid newOwnerId)
{



























}

/*
 * Change foreign server owner -- by OID
 */
void
AlterForeignServerOwner_oid(Oid srvId, Oid newOwnerId)
{

















}

/*
 * Convert a handler function name passed from the parser to an Oid.
 */
static Oid
lookup_fdw_handler_func(DefElem *handler)
{


















}

/*
 * Convert a validator function name passed from the parser to an Oid.
 */
static Oid
lookup_fdw_validator_func(DefElem *validator)
{













}

/*
 * Process function options of CREATE/ALTER FDW
 */
static void
parse_func_options(List *func_options, bool *handler_given, Oid *fdwhandler, bool *validator_given, Oid *fdwvalidator)
{



































}

/*
 * Create a foreign-data wrapper
 */
ObjectAddress
CreateForeignDataWrapper(CreateFdwStmt *stmt)
{





































































































}

/*
 * Alter foreign-data wrapper
 */
ObjectAddress
AlterForeignDataWrapper(AlterFdwStmt *stmt)
{

















































































































































}

/*
 * Drop foreign-data wrapper by OID
 */
void
RemoveForeignDataWrapperById(Oid fdwId)
{

















}

/*
 * Create a foreign server
 */
ObjectAddress
CreateForeignServer(CreateForeignServerStmt *stmt)
{


































































































































}

/*
 * Alter foreign server
 */
ObjectAddress
AlterForeignServer(AlterForeignServerStmt *stmt)
{




























































































}

/*
 * Drop foreign server by OID
 */
void
RemoveForeignServerById(Oid srvId)
{

















}

/*
 * Common routine to check permission for user-mapping-related DDL
 * commands.  We allow server owners to operate on any mapping, and
 * users to operate on their own mapping.
 */
static void
user_mapping_ddl_aclcheck(Oid umuserid, Oid serverid, const char *servername)
{



















}

/*
 * Create user mapping
 */
ObjectAddress
CreateUserMapping(CreateUserMappingStmt *stmt)
{

















































































































}

/*
 * Alter user mapping
 */
ObjectAddress
AlterUserMapping(AlterUserMappingStmt *stmt)
{
























































































}

/*
 * Drop user mapping
 */
Oid
RemoveUserMapping(DropUserMappingStmt *stmt)
{































































}

/*
 * Drop user mapping by OID.  This is called to clean up dependencies.
 */
void
RemoveUserMappingById(Oid umId)
{

















}

/*
 * Create a foreign table
 * call after DefineRelation().
 */
void
CreateForeignTable(CreateForeignTableStmt *stmt, Oid relid)
{











































































}

/*
 * Import a foreign schema
 */
void
ImportForeignSchema(ImportForeignSchemaStmt *stmt)
{












































































































}

/*
 * error context callback to let us supply the failing SQL statement's text
 */
static void
import_error_callback(void *arg)
{
















}