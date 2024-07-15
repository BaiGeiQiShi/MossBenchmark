                                                                            
   
              
                                                   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
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

   
                                            
   
                                                                         
                                                                          
                                                                         
   
List *
EventCacheLookup(EventTriggerEvent event)
{
  EventTriggerCacheEntry *entry;

  if (EventTriggerCacheState != ETCS_VALID)
  {
    BuildEventTriggerCache();
  }
  entry = hash_search(EventTriggerCache, &event, HASH_FIND, NULL);
  return entry != NULL ? entry->triggerlist : NIL;
}

   
                                    
   
static void
BuildEventTriggerCache(void)
{
  HASHCTL ctl;
  HTAB *cache;
  MemoryContext oldcontext;
  Relation rel;
  Relation irel;
  SysScanDesc scan;

  if (EventTriggerCacheContext != NULL)
  {
       
                                                                         
                                                                    
                                                                         
       
    MemoryContextResetAndDeleteChildren(EventTriggerCacheContext);
  }
  else
  {
       
                                                                           
                                                                     
                                           
       
    if (CacheMemoryContext == NULL)
    {
      CreateCacheMemoryContext();
    }
    EventTriggerCacheContext = AllocSetContextCreate(CacheMemoryContext, "EventTriggerCache", ALLOCSET_DEFAULT_SIZES);
    CacheRegisterSyscacheCallback(EVENTTRIGGEROID, InvalidateEventCacheCallback, (Datum)0);
  }

                                         
  oldcontext = MemoryContextSwitchTo(EventTriggerCacheContext);

                                                                           
  EventTriggerCacheState = ETCS_REBUILD_STARTED;

                              
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(EventTriggerEvent);
  ctl.entrysize = sizeof(EventTriggerCacheEntry);
  ctl.hcxt = EventTriggerCacheContext;
  cache = hash_create("Event Trigger Cache", 32, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

     
                                                     
     
  rel = relation_open(EventTriggerRelationId, AccessShareLock);
  irel = index_open(EventTriggerNameIndexId, AccessShareLock);
  scan = systable_beginscan_ordered(rel, irel, NULL, 0, NULL);

     
                                                                             
                                     
     
  for (;;)
  {
    HeapTuple tup;
    Form_pg_event_trigger form;
    char *evtevent;
    EventTriggerEvent event;
    EventTriggerCacheItem *item;
    Datum evttags;
    bool evttags_isnull;
    EventTriggerCacheEntry *entry;
    bool found;

                         
    tup = systable_getnext_ordered(scan, ForwardScanDirection);
    if (!HeapTupleIsValid(tup))
    {
      break;
    }

                                   
    form = (Form_pg_event_trigger)GETSTRUCT(tup);
    if (form->evtenabled == TRIGGER_DISABLED)
    {
      continue;
    }

                            
    evtevent = NameStr(form->evtevent);
    if (strcmp(evtevent, "ddl_command_start") == 0)
    {
      event = EVT_DDLCommandStart;
    }
    else if (strcmp(evtevent, "ddl_command_end") == 0)
    {
      event = EVT_DDLCommandEnd;
    }
    else if (strcmp(evtevent, "sql_drop") == 0)
    {
      event = EVT_SQLDrop;
    }
    else if (strcmp(evtevent, "table_rewrite") == 0)
    {
      event = EVT_TableRewrite;
    }
    else
    {
      continue;
    }

                                  
    item = palloc0(sizeof(EventTriggerCacheItem));
    item->fnoid = form->evtfoid;
    item->enabled = form->evtenabled;

                                     
    evttags = heap_getattr(tup, Anum_pg_event_trigger_evttags, RelationGetDescr(rel), &evttags_isnull);
    if (!evttags_isnull)
    {
      item->ntags = DecodeTextArrayToCString(evttags, &item->tag);
      qsort(item->tag, item->ntags, sizeof(char *), pg_qsort_strcmp);
    }

                             
    entry = hash_search(cache, &event, HASH_ENTER, &found);
    if (found)
    {
      entry->triggerlist = lappend(entry->triggerlist, item);
    }
    else
    {
      entry->triggerlist = list_make1(item);
    }
  }

                                        
  systable_endscan_ordered(scan);
  index_close(irel, AccessShareLock);
  relation_close(rel, AccessShareLock);

                                        
  MemoryContextSwitchTo(oldcontext);

                          
  EventTriggerCache = cache;

     
                                                                         
                                                                            
                                                                       
                                                                           
     
  if (EventTriggerCacheState == ETCS_REBUILD_STARTED)
  {
    EventTriggerCacheState = ETCS_VALID;
  }
}

   
                                           
   
                                                                              
                                                                           
               
   
static int
DecodeTextArrayToCString(Datum array, char ***cstringp)
{
  ArrayType *arr = DatumGetArrayTypeP(array);
  Datum *elems;
  char **cstring;
  int i;
  int nelems;

  if (ARR_NDIM(arr) != 1 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != TEXTOID)
  {
    elog(ERROR, "expected 1-D text array");
  }
  deconstruct_array(arr, TEXTOID, -1, false, 'i', &elems, NULL, &nelems);

  cstring = palloc(nelems * sizeof(char *));
  for (i = 0; i < nelems; ++i)
  {
    cstring[i] = TextDatumGetCString(elems[i]);
  }

  pfree(elems);
  *cstringp = cstring;
  return nelems;
}

   
                                                             
   
                                                                           
                                                                             
                 
   
static void
InvalidateEventCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{
     
                                                                             
                                                                          
                                                      
     
  if (EventTriggerCacheState == ETCS_VALID)
  {
    MemoryContextResetAndDeleteChildren(EventTriggerCacheContext);
    EventTriggerCache = NULL;
  }

                               
  EventTriggerCacheState = ETCS_NEEDS_REBUILD;
}
