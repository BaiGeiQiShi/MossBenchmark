/*-------------------------------------------------------------------------
 *
 * pg_collation.c
 *	  routines to support manipulation of the pg_collation relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_collation.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_namespace.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_locale.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/*
 * CollationCreate
 *
 * Add a new tuple to pg_collation.
 *
 * if_not_exists: if true, don't fail on duplicate name, just print a notice
 * and return InvalidOid.
 * quiet: if true, don't fail on duplicate name, just silently return
 * InvalidOid (overrides if_not_exists).
 */
Oid
CollationCreate(const char *collname, Oid collnamespace, Oid collowner, char collprovider, bool collisdeterministic, int32 collencoding, const char *collcollate, const char *collctype, const char *collversion, bool if_not_exists, bool quiet)
{






















































































































































}

/*
 * RemoveCollationById
 *
 * Remove a tuple from pg_collation by Oid. This function is solely
 * called inside catalog/dependency.c
 */
void
RemoveCollationById(Oid collationOid)
{

























}