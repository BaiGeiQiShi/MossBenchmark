/*-------------------------------------------------------------------------
 *
 * toasting.c
 *	  This file contains routines to support creation of toast tables
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/catalog/toasting.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "catalog/toasting.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "storage/lock.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/* Potentially set by pg_upgrade_support functions */
Oid binary_upgrade_next_toast_pg_type_oid = InvalidOid;

static void
CheckAndCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast);
static bool
create_toast_table(Relation rel, Oid toastOid, Oid toastIndexOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast);
static bool
needs_toast_table(Relation rel);

/*
 * CreateToastTable variants
 *		If the table needs a toast table, and doesn't already have one,
 *		then create a toast table for it.
 *
 * reloptions for the toast table can be passed, too.  Pass (Datum) 0
 * for default reloptions.
 *
 * We expect the caller to have verified that the relation is a table and have
 * already done any necessary permission checks.  Callers expect this function
 * to end with CommandCounterIncrement if it makes any changes.
 */
void
AlterTableCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode)
{

}

void
NewHeapCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, Oid OIDOldToast)
{

}

void
NewRelationCreateToastTable(Oid relOid, Datum reloptions)
{

}

static void
CheckAndCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast)
{








}

/*
 * Create a toast table during bootstrap
 *
 * Here we need to prespecify the OIDs of the toast table and its index
 */
void
BootstrapToastTable(char *relName, Oid toastOid, Oid toastIndexOid)
{
















}

/*
 * create_toast_table --- internal workhorse
 *
 * rel is already opened and locked
 * toastOid and toastIndexOid are normally InvalidOid, but during
 * bootstrap they can be nonzero to specify hand-assigned OIDs
 */
static bool
create_toast_table(Relation rel, Oid toastOid, Oid toastIndexOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast)
{





































































































































































































































}

/*
 * Check to see whether the table needs a TOAST table.
 */
static bool
needs_toast_table(Relation rel)
{






























}