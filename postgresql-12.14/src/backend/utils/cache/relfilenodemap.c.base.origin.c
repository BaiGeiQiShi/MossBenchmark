/*-------------------------------------------------------------------------
 *
 * relfilenodemap.c
 *	  relfilenode to oid mapping cache.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/relfilenodemap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_class.h"
#include "catalog/pg_tablespace.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/relfilenodemap.h"
#include "utils/relmapper.h"

/* Hash table for information about each relfilenode <-> oid pair */
static HTAB *RelfilenodeMapHash = NULL;

/* built first time through in InitializeRelfilenodeMap */
static ScanKeyData relfilenode_skey[2];

typedef struct
{
  Oid reltablespace;
  Oid relfilenode;
} RelfilenodeMapKey;

typedef struct
{
  RelfilenodeMapKey key; /* lookup key - must be first */
  Oid relid;             /* pg_class.oid */
} RelfilenodeMapEntry;

/*
 * RelfilenodeMapInvalidateCallback
 *		Flush mapping entries when pg_class is updated in a relevant
 *fashion.
 */
static void
RelfilenodeMapInvalidateCallback(Datum arg, Oid relid)
{
























}

/*
 * RelfilenodeMapInvalidateCallback
 *		Initialize cache, either on first use or after a reset.
 */
static void
InitializeRelfilenodeMap(void)
{






































}

/*
 * Map a relation's (tablespace, filenode) to a relation's oid and cache the
 * result.
 *
 * Returns InvalidOid if no relation matching the criteria could be found.
 */
Oid
RelidByRelfilenode(Oid reltablespace, Oid relfilenode)
{













































































































}