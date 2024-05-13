/*-------------------------------------------------------------------------
 *
 * spccache.c
 *	  Tablespace cache management.
 *
 * We cache the parsed version of spcoptions for each tablespace to avoid
 * needing to reparse on every lookup.  Right now, there doesn't appear to
 * be a measurable performance gain from doing this, but that might change
 * in the future as we add more options.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/spccache.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/reloptions.h"
#include "catalog/pg_tablespace.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "storage/bufmgr.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/spccache.h"
#include "utils/syscache.h"

/* Hash table for information about each tablespace */
static HTAB *TableSpaceCacheHash = NULL;

typedef struct
{
  Oid oid;              /* lookup key - must be first */
  TableSpaceOpts *opts; /* options, or NULL if none */
} TableSpaceCacheEntry;

/*
 * InvalidateTableSpaceCacheCallback
 *		Flush all cache entries when pg_tablespace is updated.
 *
 * When pg_tablespace is updated, we must flush the cache entry at least
 * for that tablespace.  Currently, we just flush them all.  This is quick
 * and easy and doesn't cost much, since there shouldn't be terribly many
 * tablespaces, nor do we expect them to be frequently modified.
 */
static void
InvalidateTableSpaceCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{















}

/*
 * InitializeTableSpaceCache
 *		Initialize the tablespace cache.
 */
static void
InitializeTableSpaceCache(void)
{
















}

/*
 * get_tablespace
 *		Fetch TableSpaceCacheEntry structure for a specified table OID.
 *
 * Pointers returned by this function should not be stored, since a cache
 * flush will invalidate them.
 */
static TableSpaceCacheEntry *
get_tablespace(Oid spcid)
{































































}

/*
 * get_tablespace_page_costs
 *		Return random and/or sequential page costs for a given
 *tablespace.
 *
 *		This value is not locked by the transaction, so this value may
 *		be changed while a SELECT that has used these values for
 *planning is still executing.
 */
void
get_tablespace_page_costs(Oid spcid, double *spc_random_page_cost, double *spc_seq_page_cost)
{



























}

/*
 * get_tablespace_io_concurrency
 *
 *		This value is not locked by the transaction, so this value may
 *		be changed while a SELECT that has used these values for
 *planning is still executing.
 */
int
get_tablespace_io_concurrency(Oid spcid)
{










}