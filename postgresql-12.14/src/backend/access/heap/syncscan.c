                                                                            
   
              
                                       
   
                                                                          
                                                                            
                                                                              
                                                                             
              
   
                                                                              
                                                                              
                                                                             
                                                                             
                                                                               
                                                                          
                                                                             
                                     
   
                                                                             
                                                                             
                                                                            
                                                                         
                                                                            
                                                
   
                                                                             
                                                                              
                                                                               
                                                                            
                                                                  
   
                      
                                                                  
                                                      
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "miscadmin.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/rel.h"

                   
#ifdef TRACE_SYNCSCAN
bool trace_syncscan = false;
#endif

   
                         
   
                                                    
   
                                                                   
                                                                              
                                                                   
   
#define SYNC_SCAN_NELEM 20

   
                                                                           
   
                                                                           
                                                                            
                                                                           
                                                                              
                                                                          
                          
   
#define SYNC_SCAN_REPORT_INTERVAL (128 * 1024 / BLCKSZ)

   
                                                                             
                                                                                
                             
   
typedef struct ss_scan_location_t
{
  RelFileNode relfilenode;                             
  BlockNumber location;                                                
} ss_scan_location_t;

typedef struct ss_lru_item_t
{
  struct ss_lru_item_t *prev;
  struct ss_lru_item_t *next;
  ss_scan_location_t location;
} ss_lru_item_t;

typedef struct ss_scan_locations_t
{
  ss_lru_item_t *head;
  ss_lru_item_t *tail;
  ss_lru_item_t items[FLEXIBLE_ARRAY_MEMBER];                            
} ss_scan_locations_t;

#define SizeOfScanLocations(N) (offsetof(ss_scan_locations_t, items) + (N) * sizeof(ss_lru_item_t))

                                        
static ss_scan_locations_t *scan_locations;

                                       
static BlockNumber
ss_search(RelFileNode relfilenode, BlockNumber location, bool set);

   
                                                                     
   
Size
SyncScanShmemSize(void)
{
  return SizeOfScanLocations(SYNC_SCAN_NELEM);
}

   
                                                                
   
void
SyncScanShmemInit(void)
{
  int i;
  bool found;

  scan_locations = (ss_scan_locations_t *)ShmemInitStruct("Sync Scan Locations List", SizeOfScanLocations(SYNC_SCAN_NELEM), &found);

  if (!IsUnderPostmaster)
  {
                                       
    Assert(!found);

    scan_locations->head = &scan_locations->items[0];
    scan_locations->tail = &scan_locations->items[SYNC_SCAN_NELEM - 1];

    for (i = 0; i < SYNC_SCAN_NELEM; i++)
    {
      ss_lru_item_t *item = &scan_locations->items[i];

         
                                                                         
                                                                  
                                     
         
      item->location.relfilenode.spcNode = InvalidOid;
      item->location.relfilenode.dbNode = InvalidOid;
      item->location.relfilenode.relNode = InvalidOid;
      item->location.location = InvalidBlockNumber;

      item->prev = (i > 0) ? (&scan_locations->items[i - 1]) : NULL;
      item->next = (i < SYNC_SCAN_NELEM - 1) ? (&scan_locations->items[i + 1]) : NULL;
    }
  }
  else
  {
    Assert(found);
  }
}

   
                                                                           
                       
   
                                                                           
                                                                            
                                                                
   
                                                                
   
                                                                         
                   
   
static BlockNumber
ss_search(RelFileNode relfilenode, BlockNumber location, bool set)
{
  ss_lru_item_t *item;

  item = scan_locations->head;
  for (;;)
  {
    bool match;

    match = RelFileNodeEquals(item->location.relfilenode, relfilenode);

    if (match || item->next == NULL)
    {
         
                                                                         
                        
         
      if (!match)
      {
        item->location.relfilenode = relfilenode;
        item->location.location = location;
      }
      else if (set)
      {
        item->location.location = location;
      }

                                                       
      if (item != scan_locations->head)
      {
                    
        if (item == scan_locations->tail)
        {
          scan_locations->tail = item->prev;
        }
        item->prev->next = item->next;
        if (item->next)
        {
          item->next->prev = item->prev;
        }

                  
        item->prev = NULL;
        item->next = scan_locations->head;
        scan_locations->head->prev = item;
        scan_locations->head = item;
      }

      return item->location.location;
    }

    item = item->next;
  }

                   
}

   
                                                                  
   
                                                                  
                                                 
   
                                                                       
                                                                           
                                                             
   
BlockNumber
ss_get_location(Relation rel, BlockNumber relnblocks)
{
  BlockNumber startloc;

  LWLockAcquire(SyncScanLock, LW_EXCLUSIVE);
  startloc = ss_search(rel->rd_node, 0, false);
  LWLockRelease(SyncScanLock);

     
                                                                            
     
                                                                            
                         
     
  if (startloc >= relnblocks)
  {
    startloc = 0;
  }

#ifdef TRACE_SYNCSCAN
  if (trace_syncscan)
  {
    elog(LOG, "SYNC_SCAN: start \"%s\" (size %u) at %u", RelationGetRelationName(rel), relnblocks, startloc);
  }
#endif

  return startloc;
}

   
                                                           
   
                                                               
                                                                      
                     
   
void
ss_report_location(Relation rel, BlockNumber location)
{
#ifdef TRACE_SYNCSCAN
  if (trace_syncscan)
  {
    if ((location % 1024) == 0)
    {
      elog(LOG, "SYNC_SCAN: scanning \"%s\" at %u", RelationGetRelationName(rel), location);
    }
  }
#endif

     
                                                                             
                                                                           
                                                                         
                                                                            
                                                                       
                         
     
  if ((location % SYNC_SCAN_REPORT_INTERVAL) == 0)
  {
    if (LWLockConditionalAcquire(SyncScanLock, LW_EXCLUSIVE))
    {
      (void)ss_search(rel->rd_node, location, true);
      LWLockRelease(SyncScanLock);
    }
#ifdef TRACE_SYNCSCAN
    else if (trace_syncscan)
    {
      elog(LOG, "SYNC_SCAN: missed update for \"%s\" at %u", RelationGetRelationName(rel), location);
    }
#endif
  }
}
