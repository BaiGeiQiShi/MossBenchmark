/*-------------------------------------------------------------------------
 *
 * evtcache.c
 *	  Special-purpose cache for event trigger data.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/evtcache.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/indexing.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/evtcache.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/hsearch.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

typedef enum
{
  ETCS_NEEDS_REBUILD,
  ETCS_REBUILD_STARTED,
  ETCS_VALID
} EventTriggerCacheStateType;

typedef struct
{
  EventTriggerEvent event;
  List *triggerlist;
} EventTriggerCacheEntry;

static HTAB *EventTriggerCache;
static MemoryContext EventTriggerCacheContext;
static EventTriggerCacheStateType EventTriggerCacheState = ETCS_NEEDS_REBUILD;

static void
BuildEventTriggerCache(void);
static void
InvalidateEventCacheCallback(Datum arg, int cacheid, uint32 hashvalue);
static int
DecodeTextArrayToCString(Datum array, char ***cstringp);

/*
 * Search the event cache by trigger event.
 *
 * Note that the caller had better copy any data it wants to keep around
 * across any operation that might touch a system catalog into some other
 * memory context, since a cache reset could blow the return value away.
 */
List *
EventCacheLookup(EventTriggerEvent event)
{








}

/*
 * Rebuild the event trigger cache.
 */
static void
BuildEventTriggerCache(void)
{






















































































































































}

/*
 * Decode text[] to an array of C strings.
 *
 * We could avoid a bit of overhead here if we were willing to duplicate some
 * of the logic from deconstruct_array, but it doesn't seem worth the code
 * complexity.
 */
static int
DecodeTextArrayToCString(Datum array, char ***cstringp)
{





















}

/*
 * Flush all cache entries when pg_event_trigger is updated.
 *
 * This should be rare enough that we don't need to be very granular about
 * it, so we just blow away everything, which also avoids the possibility of
 * memory leaks.
 */
static void
InvalidateEventCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{













}