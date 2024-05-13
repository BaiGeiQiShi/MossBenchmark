/*-------------------------------------------------------------------------
 *
 * foreign.c
 *		  support for foreign-data wrappers, servers and user mappings.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  src/backend/foreign/foreign.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/*
 * GetForeignDataWrapper -	look up the foreign-data wrapper by OID.
 */
ForeignDataWrapper *
GetForeignDataWrapper(Oid fdwid)
{

}

/*
 * GetForeignDataWrapperExtended -	look up the foreign-data wrapper
 * by OID. If flags uses FDW_MISSING_OK, return NULL if the object cannot
 * be found instead of raising an error.
 */
ForeignDataWrapper *
GetForeignDataWrapperExtended(Oid fdwid, bits16 flags)
{








































}

/*
 * GetForeignDataWrapperByName - look up the foreign-data wrapper
 * definition by name.
 */
ForeignDataWrapper *
GetForeignDataWrapperByName(const char *fdwname, bool missing_ok)
{








}

/*
 * GetForeignServer - look up the foreign server definition.
 */
ForeignServer *
GetForeignServer(Oid serverid)
{

}

/*
 * GetForeignServerExtended - look up the foreign server definition. If
 * flags uses FSV_MISSING_OK, return NULL if the object cannot be found
 * instead of raising an error.
 */
ForeignServer *
GetForeignServerExtended(Oid serverid, bits16 flags)
{















































}

/*
 * GetForeignServerByName - look up the foreign server definition by name.
 */
ForeignServer *
GetForeignServerByName(const char *srvname, bool missing_ok)
{








}

/*
 * GetUserMapping - look up the user mapping.
 *
 * If no mapping is found for the supplied user, we also look for
 * PUBLIC mappings (userid == InvalidOid).
 */
UserMapping *
GetUserMapping(Oid userid, Oid serverid)
{





































}

/*
 * GetForeignTable - look up the foreign table definition by relation oid.
 */
ForeignTable *
GetForeignTable(Oid relid)
{































}

/*
 * GetForeignColumnOptions - Get attfdwoptions of given relation/attnum
 * as list of DefElem.
 */
List *
GetForeignColumnOptions(Oid relid, AttrNumber attnum)
{























}

/*
 * GetFdwRoutine - call the specified foreign-data wrapper handler routine
 * to get its FdwRoutine struct.
 */
FdwRoutine *
GetFdwRoutine(Oid fdwhandler)
{












}

/*
 * GetForeignServerIdByRelId - look up the foreign server
 * for the given foreign table, and return its OID.
 */
Oid
GetForeignServerIdByRelId(Oid relid)
{














}

/*
 * GetFdwRoutineByServerId - look up the handler of the foreign-data wrapper
 * for the given foreign server, and retrieve its FdwRoutine struct.
 */
FdwRoutine *
GetFdwRoutineByServerId(Oid serverid)
{



































}

/*
 * GetFdwRoutineByRelId - look up the handler of the foreign-data wrapper
 * for the given foreign table, and retrieve its FdwRoutine struct.
 */
FdwRoutine *
GetFdwRoutineByRelId(Oid relid)
{







}

/*
 * GetFdwRoutineForRelation - look up the handler of the foreign-data wrapper
 * for the given foreign table, and retrieve its FdwRoutine struct.
 *
 * This function is preferred over GetFdwRoutineByRelId because it caches
 * the data in the relcache entry, saving a number of catalog lookups.
 *
 * If makecopy is true then the returned data is freshly palloc'd in the
 * caller's memory context.  Otherwise, it's a pointer to the relcache data,
 * which will be lost in any relcache reset --- so don't rely on it long.
 */
FdwRoutine *
GetFdwRoutineForRelation(Relation relation, bool makecopy)
{



























}

/*
 * IsImportableForeignTable - filter table names for IMPORT FOREIGN SCHEMA
 *
 * Returns true if given table name should be imported according to the
 * statement's import filter options.
 */
bool
IsImportableForeignTable(const char *tablename, ImportForeignSchemaStmt *stmt)
{
































}

/*
 * deflist_to_tuplestore - Helper function to convert DefElem list to
 * tuplestore usable in SRF.
 */
static void
deflist_to_tuplestore(ReturnSetInfo *rsinfo, List *options)
{





















































}

/*
 * Convert options array to name/value table.  Useful for information
 * schema and pg_dump.
 */
Datum
pg_options_to_table(PG_FUNCTION_ARGS)
{





}

/*
 * Describes the valid options for postgresql FDW, server, and user mapping.
 */
struct ConnectionOption
{
  const char *optname;
  Oid optcontext; /* Oid of catalog in which option may appear */
};

/*
 * Copied from fe-connect.c PQconninfoOptions.
 *
 * The list is small - don't bother with bsearch if it stays so.
 */
static const struct ConnectionOption libpq_conninfo_options[] = {{"authtype", ForeignServerRelationId}, {"service", ForeignServerRelationId}, {"user", UserMappingRelationId}, {"password", UserMappingRelationId}, {"connect_timeout", ForeignServerRelationId}, {"dbname", ForeignServerRelationId}, {"host", ForeignServerRelationId}, {"hostaddr", ForeignServerRelationId}, {"port", ForeignServerRelationId}, {"tty", ForeignServerRelationId}, {"options", ForeignServerRelationId}, {"requiressl", ForeignServerRelationId}, {"sslmode", ForeignServerRelationId}, {"gsslib", ForeignServerRelationId}, {NULL, InvalidOid}};

/*
 * Check if the provided option is one of libpq conninfo options.
 * context is the Oid of the catalog the option came from, or 0 if we
 * don't care.
 */
static bool
is_conninfo_option(const char *option, Oid context)
{










}

/*
 * Validate the generic option given to SERVER or USER MAPPING.
 * Raise an ERROR if the option or its value is considered invalid.
 *
 * Valid server options are all libpq conninfo options except
 * user and password -- these may only appear in USER MAPPING options.
 *
 * Caution: this function is deprecated, and is now meant only for testing
 * purposes, because the list of options it knows about doesn't necessarily
 * square with those known to whichever libpq instance you might be using.
 * Inquire of libpq itself, instead.
 */
Datum
postgresql_fdw_validator(PG_FUNCTION_ARGS)
{


































}

/*
 * get_foreign_data_wrapper_oid - given a FDW name, look up the OID
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return InvalidOid.
 */
Oid
get_foreign_data_wrapper_oid(const char *fdwname, bool missing_ok)
{








}

/*
 * get_foreign_server_oid - given a server name, look up the OID
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return InvalidOid.
 */
Oid
get_foreign_server_oid(const char *servername, bool missing_ok)
{








}

/*
 * Get a copy of an existing local path for a given join relation.
 *
 * This function is usually helpful to obtain an alternate local path for EPQ
 * checks.
 *
 * Right now, this function only supports unparameterized foreign joins, so we
 * only search for unparameterized path in the given list of paths. Since we
 * are searching for a path which can be used to construct an alternative local
 * plan for a foreign join, we look for only MergeJoin, HashJoin or NestLoop
 * paths.
 *
 * If the inner or outer subpath of the chosen path is a ForeignScan, we
 * replace it with its outer subpath.  For this reason, and also because the
 * planner might free the original path later, the path returned by this
 * function is a shallow copy of the original.  There's no need to copy
 * the substructure, so we don't.
 *
 * Since the plan created using this path will presumably only be used to
 * execute EPQ checks, efficiency of the path is not a concern. But since the
 * path list in RelOptInfo is anyway sorted by total cost we are likely to
 * choose the most efficient path, which is all for the best.
 */
Path *
GetExistingLocalJoinPath(RelOptInfo *joinrel)
{



























































































}