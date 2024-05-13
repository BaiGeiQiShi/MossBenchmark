/*-------------------------------------------------------------------------
 *
 * pg_largeobject.c
 *	  routines to support manipulation of the pg_largeobject relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_largeobject.c
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
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

/*
 * Create a large object having the given LO identifier.
 *
 * We create a new large object by inserting an entry into
 * pg_largeobject_metadata without any data pages, so that the object
 * will appear to exist with size 0.
 */
Oid
LargeObjectCreate(Oid loid)
{




































}

/*
 * Drop a large object having the given LO identifier.  Both the data pages
 * and metadata must be dropped.
 */
void
LargeObjectDrop(Oid loid)
{











































}

/*
 * LargeObjectExists
 *
 * We don't use the system cache for large object metadata, for fear of
 * using too much local memory.
 *
 * This function always scans the system catalog using an up-to-date snapshot,
 * so it should not be used when a large object is opened in read-only mode
 * (because large objects opened in read only mode are supposed to be viewed
 * relative to the caller's snapshot, whereas in read-write mode they are
 * relative to a current snapshot).
 */
bool
LargeObjectExists(Oid loid)
{























}