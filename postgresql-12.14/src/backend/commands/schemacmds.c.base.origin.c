/*-------------------------------------------------------------------------
 *
 * schemacmds.c
 *	  schema creation/manipulation commands
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/schemacmds.c
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
#include "catalog/pg_authid.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "commands/dbcommands.h"
#include "commands/event_trigger.h"
#include "commands/schemacmds.h"
#include "miscadmin.h"
#include "parser/parse_utilcmd.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
AlterSchemaOwner_internal(HeapTuple tup, Relation rel, Oid newOwnerId);

/*
 * CREATE SCHEMA
 *
 * Note: caller should pass in location information for the whole
 * CREATE SCHEMA statement, which in turn we pass down as the location
 * of the component commands.  This comports with our general plan of
 * reporting location/len for the whole command even when executing
 * a subquery.
 */
Oid
CreateSchemaCommand(CreateSchemaStmt *stmt, const char *queryString, int stmt_location, int stmt_len)
{





































































































































































}

/*
 * Guts of schema deletion.
 */
void
RemoveSchemaById(Oid schemaOid)
{
















}

/*
 * Rename schema
 */
ObjectAddress
RenameSchema(const char *oldname, const char *newname)
{






















































}

void
AlterSchemaOwner_oid(Oid oid, Oid newOwnerId)
{
















}

/*
 * Change schema owner
 */
ObjectAddress
AlterSchemaOwner(const char *name, Oid newOwnerId)
{


























}

static void
AlterSchemaOwner_internal(HeapTuple tup, Relation rel, Oid newOwnerId)
{











































































}