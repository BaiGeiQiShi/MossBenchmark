/*
 * pg_db_role_setting.c
 *		Routines to support manipulation of the pg_db_role_setting
 *relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/backend/catalog/pg_db_role_setting.c
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_db_role_setting.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

void
AlterSetting(Oid databaseid, Oid roleid, VariableSetStmt *setstmt)
{



































































































































}

/*
 * Drop some settings from the catalog.  These can be for a particular
 * database, or for a particular role.  (It is of course possible to do both
 * too, but it doesn't make sense for current uses.)
 */
void
DropSetting(Oid databaseid, Oid roleid)
{



























}

/*
 * Scan pg_db_role_setting looking for applicable settings, and load them on
 * the current process.
 *
 * relsetting is pg_db_role_setting, already opened and locked.
 *
 * Note: we only consider setting for the exact databaseid/roleid combination.
 * This probably needs to be called more than once, with InvalidOid passed as
 * databaseid/roleid.
 */
void
ApplySetting(Snapshot snapshot, Oid databaseid, Oid roleid, Relation relsetting, GucSource source)
{




























}