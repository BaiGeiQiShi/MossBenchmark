/*-------------------------------------------------------------------------
 *
 * attoptcache.c
 *	  Attribute options cache management.
 *
 * Attribute options are cached separately from the fixed-size portion of
 * pg_attribute entries, which are handled by the relcache.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/attoptcache.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/reloptions.h"
#include "utils/attoptcache.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/syscache.h"

/* Hash table for information about each attribute's options */
static HTAB *AttoptCacheHash = NULL;

/* attrelid and attnum form the lookup key, and must appear first */
typedef struct
{
  Oid attrelid;
  int attnum;
} AttoptCacheKey;

typedef struct
{
  AttoptCacheKey key;  /* lookup key - must be first */
  AttributeOpts *opts; /* options, or NULL if none */
} AttoptCacheEntry;

/*
 * InvalidateAttoptCacheCallback
 *		Flush all cache entries when pg_attribute is updated.
 *
 * When pg_attribute is updated, we must flush the cache entry at least
 * for that attribute.  Currently, we just flush them all.  Since attribute
 * options are not currently used in performance-critical paths (such as
 * query execution), this seems OK.
 */
static void
InvalidateAttoptCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{















}

/*
 * InitializeAttoptCache
 *		Initialize the attribute options cache.
 */
static void
InitializeAttoptCache(void)
{
















}

/*
 * get_attribute_options
 *		Fetch attribute options for a specified table OID.
 */
AttributeOpts *
get_attribute_options(Oid attrelid, int attnum)
{



































































}