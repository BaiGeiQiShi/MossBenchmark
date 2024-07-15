                                                                            
   
          
                                           
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#include "miscadmin.h"
#include "pgstat.h"
#include "access/xlogutils.h"
#include "access/xlog.h"
#include "commands/tablespace.h"
#include "portability/instr_time.h"
#include "postmaster/bgwriter.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/md.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/inval.h"

static MemoryContext pendingOpsCxt;                                         

   
                                                                          
                                                                               
                                                                             
                                                                             
                                                                          
                                                   
   
                                                                          
                                                                             
                                                                             
   
                                                                         
                                                                         
              
   
                                                                          
                              
   
typedef uint16 CycleCtr;                                         

typedef struct
{
  FileTag tag;                                         
  CycleCtr cycle_ctr;                                       
  bool canceled;                                                      
} PendingFsyncEntry;

typedef struct
{
  FileTag tag;                                         
  CycleCtr cycle_ctr;                                                 
} PendingUnlinkEntry;

static HTAB *pendingOps = NULL;
static List *pendingUnlinks = NIL;
static MemoryContext pendingOpsCxt;                             

static CycleCtr sync_cycle_ctr = 0;
static CycleCtr checkpoint_cycle_ctr = 0;

                                              
#define FSYNCS_PER_ABSORB 10
#define UNLINKS_PER_ABSORB 10

   
                                                            
   
typedef struct SyncOps
{
  int (*sync_syncfiletag)(const FileTag *ftag, char *path);
  int (*sync_unlinkfiletag)(const FileTag *ftag, char *path);
  bool (*sync_filetagmatches)(const FileTag *ftag, const FileTag *candidate);
} SyncOps;

static const SyncOps syncsw[] = {
                       
    {.sync_syncfiletag = mdsyncfiletag, .sync_unlinkfiletag = mdunlinkfiletag, .sync_filetagmatches = mdfiletagmatches}};

   
                                                          
   
void
InitSync(void)
{
     
                                                                            
                                                                             
                                        
     
  if (!IsUnderPostmaster || AmStartupProcess() || AmCheckpointerProcess())
  {
    HASHCTL hash_ctl;

       
                                                                           
                                                                      
                                                                          
                                                                        
                                                                      
                                                                           
                 
       
    pendingOpsCxt = AllocSetContextCreate(TopMemoryContext, "Pending ops context", ALLOCSET_DEFAULT_SIZES);
    MemoryContextAllowInCriticalSection(pendingOpsCxt, true);

    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(FileTag);
    hash_ctl.entrysize = sizeof(PendingFsyncEntry);
    hash_ctl.hcxt = pendingOpsCxt;
    pendingOps = hash_create("Pending Ops Table", 100L, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
    pendingUnlinks = NIL;
  }
}

   
                                                 
   
                                                                      
                                                                         
                                                                    
                                
   
                                                                         
                                                                       
                                                                         
                                     
   
                                                                      
                                          
   
void
SyncPreCheckpoint(void)
{
     
                                                                             
                                                                        
                                                                            
                                                                       
                                                                             
                                
     
  AbsorbSyncRequests();

     
                                                                             
                                                                 
     
  checkpoint_cycle_ctr++;
}

   
                                                   
   
                                                              
   
void
SyncPostCheckpoint(void)
{
  int absorb_counter;

  absorb_counter = UNLINKS_PER_ABSORB;
  while (pendingUnlinks != NIL)
  {
    PendingUnlinkEntry *entry = (PendingUnlinkEntry *)linitial(pendingUnlinks);
    char path[MAXPGPATH];

       
                                                                         
                                       
       
                                                                          
                                                                        
                                                                         
                                     
       
    if (entry->cycle_ctr == checkpoint_cycle_ctr)
    {
      break;
    }

                         
    if (syncsw[entry->tag.handler].sync_unlinkfiletag(&entry->tag, path) < 0)
    {
         
                                                                       
                                                                       
                                                                         
                                                                       
                                                        
         
      if (errno != ENOENT)
      {
        ereport(WARNING, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
      }
    }

                                   
    pendingUnlinks = list_delete_first(pendingUnlinks);
    pfree(entry);

       
                                                                        
                                                                         
                                                                         
                                                   
       
    if (--absorb_counter <= 0)
    {
      AbsorbSyncRequests();
      absorb_counter = UNLINKS_PER_ABSORB;
    }
  }
}

   
 
                                                           
   
void
ProcessSyncRequests(void)
{
  static bool sync_in_progress = false;

  HASH_SEQ_STATUS hstat;
  PendingFsyncEntry *entry;
  int absorb_counter;

                                
  int processed = 0;
  instr_time sync_start, sync_end, sync_diff;
  uint64 elapsed;
  uint64 longest = 0;
  uint64 total_elapsed = 0;

     
                                                                         
                                                        
     
  if (!pendingOps)
  {
    elog(ERROR, "cannot sync without a pendingOps table");
  }

     
                                                                          
                                                                           
                                                                           
                                                                             
                                                                           
                                                                          
                                                                        
     
  AbsorbSyncRequests();

     
                                                                             
                                                                             
                                                                        
                                                                        
                                                                          
                     
     
                                                                             
                                                                        
                                                                       
                                                                         
                                                                      
                                                                        
                                                                             
                                                                             
                                                                        
                                                        
     
                                                                        
                                                                            
                                                                             
                                                                  
     
  if (sync_in_progress)
  {
                                                                
    hash_seq_init(&hstat, pendingOps);
    while ((entry = (PendingFsyncEntry *)hash_seq_search(&hstat)) != NULL)
    {
      entry->cycle_ctr = sync_cycle_ctr;
    }
  }

                                                                         
  sync_cycle_ctr++;

                                                                        
  sync_in_progress = true;

                                                            
  absorb_counter = FSYNCS_PER_ABSORB;
  hash_seq_init(&hstat, pendingOps);
  while ((entry = (PendingFsyncEntry *)hash_seq_search(&hstat)) != NULL)
  {
    int failures;

       
                                                                       
                                                                          
             
       
    if (entry->cycle_ctr == sync_cycle_ctr)
    {
      continue;
    }

                                          
    Assert((CycleCtr)(entry->cycle_ctr + 1) == sync_cycle_ctr);

       
                                                                        
                                                                           
                                  
       
    if (enableFsync)
    {
         
                                                                         
                                                                      
                                                                    
                                                                   
                              
         
      if (--absorb_counter <= 0)
      {
        AbsorbSyncRequests();
        absorb_counter = FSYNCS_PER_ABSORB;
      }

         
                                                                       
                                                                         
                                                                        
                                                                     
                                                                       
                                                                   
                                                                      
                                                                        
                                     
         
      for (failures = 0; !entry->canceled; failures++)
      {
        char path[MAXPGPATH];

        INSTR_TIME_SET_CURRENT(sync_start);
        if (syncsw[entry->tag.handler].sync_syncfiletag(&entry->tag, path) == 0)
        {
                                                            
          INSTR_TIME_SET_CURRENT(sync_end);
          sync_diff = sync_end;
          INSTR_TIME_SUBTRACT(sync_diff, sync_start);
          elapsed = INSTR_TIME_GET_MICROSEC(sync_diff);
          if (elapsed > longest)
          {
            longest = elapsed;
          }
          total_elapsed += elapsed;
          processed++;

          if (log_checkpoints)
          {
            elog(DEBUG1, "checkpoint sync: number=%d file=%s time=%.3f msec", processed, path, (double)elapsed / 1000);
          }

          break;                        
        }

           
                                                                
                                                                     
                                                                    
                 
           
        if (!FILE_POSSIBLY_DELETED(errno) || failures > 0)
        {
          ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", path)));
        }
        else
        {
          ereport(DEBUG1, (errcode_for_file_access(), errmsg("could not fsync file \"%s\" but retrying: %m", path)));
        }

           
                                                                 
                                           
           
        AbsorbSyncRequests();
        absorb_counter = FSYNCS_PER_ABSORB;                       
      }                     
    }

                                                
    if (hash_search(pendingOps, &entry->tag, HASH_REMOVE, NULL) == NULL)
    {
      elog(ERROR, "pendingOps corrupted");
    }
  }                                      

                                                                    
  CheckpointStats.ckpt_sync_rels = processed;
  CheckpointStats.ckpt_longest_sync = longest;
  CheckpointStats.ckpt_agg_sync_time = total_elapsed;

                                                         
  sync_in_progress = false;
}

   
                                                                            
   
                                                                   
                                                                         
                                                                         
   
                                                                            
   
void
RememberSyncRequest(const FileTag *ftag, SyncRequestType type)
{
  Assert(pendingOps);

  if (type == SYNC_FORGET_REQUEST)
  {
    PendingFsyncEntry *entry;

                                           
    entry = (PendingFsyncEntry *)hash_search(pendingOps, (void *)ftag, HASH_FIND, NULL);
    if (entry != NULL)
    {
      entry->canceled = true;
    }
  }
  else if (type == SYNC_FILTER_REQUEST)
  {
    HASH_SEQ_STATUS hstat;
    PendingFsyncEntry *entry;
    ListCell *cell, *prev, *next;

                                        
    hash_seq_init(&hstat, pendingOps);
    while ((entry = (PendingFsyncEntry *)hash_seq_search(&hstat)) != NULL)
    {
      if (entry->tag.handler == ftag->handler && syncsw[ftag->handler].sync_filetagmatches(ftag, &entry->tag))
      {
        entry->canceled = true;
      }
    }

                                         
    prev = NULL;
    for (cell = list_head(pendingUnlinks); cell; cell = next)
    {
      PendingUnlinkEntry *entry = (PendingUnlinkEntry *)lfirst(cell);

      next = lnext(cell);
      if (entry->tag.handler == ftag->handler && syncsw[ftag->handler].sync_filetagmatches(ftag, &entry->tag))
      {
        pendingUnlinks = list_delete_cell(pendingUnlinks, cell, prev);
        pfree(entry);
      }
      else
      {
        prev = cell;
      }
    }
  }
  else if (type == SYNC_UNLINK_REQUEST)
  {
                                                   
    MemoryContext oldcxt = MemoryContextSwitchTo(pendingOpsCxt);
    PendingUnlinkEntry *entry;

    entry = palloc(sizeof(PendingUnlinkEntry));
    entry->tag = *ftag;
    entry->cycle_ctr = checkpoint_cycle_ctr;

    pendingUnlinks = lappend(pendingUnlinks, entry);

    MemoryContextSwitchTo(oldcxt);
  }
  else
  {
                                                            
    MemoryContext oldcxt = MemoryContextSwitchTo(pendingOpsCxt);
    PendingFsyncEntry *entry;
    bool found;

    Assert(type == SYNC_REQUEST);

    entry = (PendingFsyncEntry *)hash_search(pendingOps, (void *)ftag, HASH_ENTER, &found);
                                     
    if (!found)
    {
      entry->cycle_ctr = sync_cycle_ctr;
      entry->canceled = false;
    }

       
                                                                        
                                                                      
                                           
       

    MemoryContextSwitchTo(oldcxt);
  }
}

   
                                                                         
   
                                                                          
                                                                        
   
bool
RegisterSyncRequest(const FileTag *ftag, SyncRequestType type, bool retryOnError)
{
  bool ret;

  if (pendingOps != NULL)
  {
                                                                     
    RememberSyncRequest(ftag, type);
    return true;
  }

  for (;;)
  {
       
                                                                           
                                                                       
                                     
       
                                                                          
                                                                
                                                                         
                                                                          
                   
       
    ret = ForwardSyncRequest(ftag, type);

       
                                                                           
                                                
       
    if (ret || (!ret && !retryOnError))
    {
      break;
    }

    WaitLatch(NULL, WL_EXIT_ON_PM_DEATH | WL_TIMEOUT, 10, WAIT_EVENT_REGISTER_SYNC_REQUEST);
  }

  return ret;
}

   
                                                                               
                                                                       
                                                                      
                                                          
   
void
EnableSyncRequestForwarding(void)
{
                                                                         
  if (pendingOps)
  {
    ProcessSyncRequests();
    hash_destroy(pendingOps);
  }
  pendingOps = NULL;

     
                                                                            
                                        
     
  Assert(pendingUnlinks == NIL);
}
