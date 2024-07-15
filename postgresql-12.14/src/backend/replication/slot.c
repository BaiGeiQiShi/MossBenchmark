                                                                            
   
          
                                   
   
   
                                                                
   
   
                  
                                    
   
         
   
                                                                      
                                                                           
                                                                            
                                                                             
                                                                               
                                                                             
                                                                     
   
                                                                               
                                                                           
                                                                            
                                                                            
               
   
                                                                             
                                                                           
                                                                              
                                                                          
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "access/transam.h"
#include "access/xlog_internal.h"
#include "common/string.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/slot.h"
#include "storage/fd.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"

   
                                            
   
typedef struct ReplicationSlotOnDisk
{
                                                                 

                                    
  uint32 magic;
  pg_crc32c checksum;

                                
  uint32 version;
  uint32 length;

     
                                                                            
                
     

  ReplicationSlotPersistentData slotdata;
} ReplicationSlotOnDisk;

                                      
#define ReplicationSlotOnDiskConstantSize offsetof(ReplicationSlotOnDisk, slotdata)
                                                              
#define SnapBuildOnDiskNotChecksummedSize offsetof(ReplicationSlotOnDisk, version)
                                              
#define SnapBuildOnDiskChecksummedSize sizeof(ReplicationSlotOnDisk) - SnapBuildOnDiskNotChecksummedSize
                                                     
#define ReplicationSlotOnDiskV2Size sizeof(ReplicationSlotOnDisk) - ReplicationSlotOnDiskConstantSize

#define SLOT_MAGIC 0x1051CA1                        
#define SLOT_VERSION 2                                  

                                                   
ReplicationSlotCtlData *ReplicationSlotCtl = NULL;

                                                              
ReplicationSlot *MyReplicationSlot = NULL;

          
int max_replication_slots = 0;                                      
                                          

static void
ReplicationSlotDropAcquired(void);
static void
ReplicationSlotDropPtr(ReplicationSlot *slot);

                                    
static void
RestoreSlotFromDisk(const char *name);
static void
CreateSlotOnDisk(ReplicationSlot *slot);
static void
SaveSlotToPath(ReplicationSlot *slot, const char *path, int elevel);

   
                                                                  
   
Size
ReplicationSlotsShmemSize(void)
{
  Size size = 0;

  if (max_replication_slots == 0)
  {
    return size;
  }

  size = offsetof(ReplicationSlotCtlData, replication_slots);
  size = add_size(size, mul_size(max_replication_slots, sizeof(ReplicationSlot)));

  return size;
}

   
                                                                
   
void
ReplicationSlotsShmemInit(void)
{
  bool found;

  if (max_replication_slots == 0)
  {
    return;
  }

  ReplicationSlotCtl = (ReplicationSlotCtlData *)ShmemInitStruct("ReplicationSlot Ctl", ReplicationSlotsShmemSize(), &found);

  LWLockRegisterTranche(LWTRANCHE_REPLICATION_SLOT_IO_IN_PROGRESS, "replication_slot_io");

  if (!found)
  {
    int i;

                                           
    MemSet(ReplicationSlotCtl, 0, ReplicationSlotsShmemSize());

    for (i = 0; i < max_replication_slots; i++)
    {
      ReplicationSlot *slot = &ReplicationSlotCtl->replication_slots[i];

                                                         
      SpinLockInit(&slot->mutex);
      LWLockInitialize(&slot->io_in_progress_lock, LWTRANCHE_REPLICATION_SLOT_IO_IN_PROGRESS);
      ConditionVariableInit(&slot->active_cv);
    }
  }
}

   
                                                                            
   
                                                                               
                                                                  
   
                                                                         
   
bool
ReplicationSlotValidateName(const char *name, int elevel)
{
  const char *cp;

  if (strlen(name) == 0)
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_NAME), errmsg("replication slot name \"%s\" is too short", name)));
    return false;
  }

  if (strlen(name) >= NAMEDATALEN)
  {
    ereport(elevel, (errcode(ERRCODE_NAME_TOO_LONG), errmsg("replication slot name \"%s\" is too long", name)));
    return false;
  }

  for (cp = name; *cp; cp++)
  {
    if (!((*cp >= 'a' && *cp <= 'z') || (*cp >= '0' && *cp <= '9') || (*cp == '_')))
    {
      ereport(elevel, (errcode(ERRCODE_INVALID_NAME), errmsg("replication slot name \"%s\" contains invalid character", name), errhint("Replication slot names may only contain lower case letters, numbers, and the underscore character.")));
      return false;
    }
  }
  return true;
}

   
                                                                      
   
                          
                                                                         
                                                   
   
void
ReplicationSlotCreate(const char *name, bool db_specific, ReplicationSlotPersistency persistency)
{
  ReplicationSlot *slot = NULL;
  int i;

  Assert(MyReplicationSlot == NULL);

  ReplicationSlotValidateName(name, ERROR);

     
                                                                           
                                                                          
                                                                             
                                                                         
                                                      
     
  LWLockAcquire(ReplicationSlotAllocationLock, LW_EXCLUSIVE);

     
                                                                             
                                                                             
                                                                   
     
  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];

    if (s->in_use && strcmp(name, NameStr(s->data.name)) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("replication slot \"%s\" already exists", name)));
    }
    if (!s->in_use && slot == NULL)
    {
      slot = s;
    }
  }
  LWLockRelease(ReplicationSlotControlLock);

                                                   
  if (slot == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("all replication slots are in use"), errhint("Free one or increase max_replication_slots.")));
  }

     
                                                                            
                                                                          
                                                                           
                                                          
     
  Assert(!slot->in_use);
  Assert(slot->active_pid == 0);

                                        
  memset(&slot->data, 0, sizeof(ReplicationSlotPersistentData));
  StrNCpy(NameStr(slot->data.name), name, NAMEDATALEN);
  slot->data.database = db_specific ? MyDatabaseId : InvalidOid;
  slot->data.persistency = persistency;

                                                   
  slot->just_dirtied = false;
  slot->dirty = false;
  slot->effective_xmin = InvalidTransactionId;
  slot->effective_catalog_xmin = InvalidTransactionId;
  slot->candidate_catalog_xmin = InvalidTransactionId;
  slot->candidate_xmin_lsn = InvalidXLogRecPtr;
  slot->candidate_restart_valid = InvalidXLogRecPtr;
  slot->candidate_restart_lsn = InvalidXLogRecPtr;

     
                                                                             
                                                                
     
  CreateSlotOnDisk(slot);

     
                                                                          
                                                                         
                                                                  
                                                   
     
  LWLockAcquire(ReplicationSlotControlLock, LW_EXCLUSIVE);

  slot->in_use = true;

                                                                    
  SpinLockAcquire(&slot->mutex);
  Assert(slot->active_pid == 0);
  slot->active_pid = MyProcPid;
  SpinLockRelease(&slot->mutex);
  MyReplicationSlot = slot;

  LWLockRelease(ReplicationSlotControlLock);

     
                                                                          
                                               
     
  LWLockRelease(ReplicationSlotAllocationLock);

                                                   
  ConditionVariableBroadcast(&slot->active_cv);
}

   
                                                                       
   
void
ReplicationSlotAcquire(const char *name, bool nowait)
{
  ReplicationSlot *slot;
  int active_pid;
  int i;

retry:
  Assert(MyReplicationSlot == NULL);

     
                                                                         
                                                                             
                                  
     
  active_pid = 0;
  slot = NULL;
  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];

    if (s->in_use && strcmp(name, NameStr(s->data.name)) == 0)
    {
         
                                                                         
                                                                  
         
      if (IsUnderPostmaster)
      {
           
                                                                       
                                                                       
                          
           
        ConditionVariablePrepareToSleep(&s->active_cv);

        SpinLockAcquire(&s->mutex);

        active_pid = s->active_pid;
        if (active_pid == 0)
        {
          active_pid = s->active_pid = MyProcPid;
        }

        SpinLockRelease(&s->mutex);
      }
      else
      {
        active_pid = MyProcPid;
      }
      slot = s;

      break;
    }
  }
  LWLockRelease(ReplicationSlotControlLock);

                                               
  if (slot == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("replication slot \"%s\" does not exist", name)));
  }

     
                                                                         
                                                                        
     
  if (active_pid != MyProcPid)
  {
    if (nowait)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("replication slot \"%s\" is active for PID %d", name, active_pid)));
    }

                                                           
    ConditionVariableSleep(&slot->active_cv, WAIT_EVENT_REPLICATION_SLOT_DROP);
    ConditionVariableCancelSleep();
    goto retry;
  }
  else
  {
    ConditionVariableCancelSleep();                                
  }

                                                   
  ConditionVariableBroadcast(&slot->active_cv);

                                                   
  MyReplicationSlot = slot;
}

   
                                                                    
   
                                                          
                                                   
   
void
ReplicationSlotRelease(void)
{
  ReplicationSlot *slot = MyReplicationSlot;

  Assert(slot != NULL && slot->active_pid != 0);

  if (slot->data.persistency == RS_EPHEMERAL)
  {
       
                                                                         
                                                                         
             
       
    ReplicationSlotDropAcquired();
  }

     
                                                                          
                                                                    
                                                                        
               
     
  if (!TransactionIdIsValid(slot->data.xmin) && TransactionIdIsValid(slot->effective_xmin))
  {
    SpinLockAcquire(&slot->mutex);
    slot->effective_xmin = InvalidTransactionId;
    SpinLockRelease(&slot->mutex);
    ReplicationSlotsComputeRequiredXmin(false);
  }

  if (slot->data.persistency == RS_PERSISTENT)
  {
       
                                                                  
                                                                     
       
    SpinLockAcquire(&slot->mutex);
    slot->active_pid = 0;
    SpinLockRelease(&slot->mutex);
    ConditionVariableBroadcast(&slot->active_cv);
  }

  MyReplicationSlot = NULL;

                                                            
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  MyPgXact->vacuumFlags &= ~PROC_IN_LOGICAL_DECODING;
  LWLockRelease(ProcArrayLock);
}

   
                                                           
   
void
ReplicationSlotCleanup(void)
{
  int i;

  Assert(MyReplicationSlot == NULL);

restart:
  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];

    if (!s->in_use)
    {
      continue;
    }

    SpinLockAcquire(&s->mutex);
    if (s->active_pid == MyProcPid)
    {
      Assert(s->data.persistency == RS_TEMPORARY);
      SpinLockRelease(&s->mutex);
      LWLockRelease(ReplicationSlotControlLock);                     

      ReplicationSlotDropPtr(s);

      ConditionVariableBroadcast(&s->active_cv);
      goto restart;
    }
    else
    {
      SpinLockRelease(&s->mutex);
    }
  }

  LWLockRelease(ReplicationSlotControlLock);
}

   
                                                                       
   
void
ReplicationSlotDrop(const char *name, bool nowait)
{
  Assert(MyReplicationSlot == NULL);

  ReplicationSlotAcquire(name, nowait);

  ReplicationSlotDropAcquired();
}

   
                                                             
   
static void
ReplicationSlotDropAcquired(void)
{
  ReplicationSlot *slot = MyReplicationSlot;

  Assert(MyReplicationSlot != NULL);

                                   
  MyReplicationSlot = NULL;

  ReplicationSlotDropPtr(slot);
}

   
                                                                             
                          
   
static void
ReplicationSlotDropPtr(ReplicationSlot *slot)
{
  char path[MAXPGPATH];
  char tmppath[MAXPGPATH];

     
                                                                            
                                                                           
                                       
     
  LWLockAcquire(ReplicationSlotAllocationLock, LW_EXCLUSIVE);

                           
  sprintf(path, "pg_replslot/%s", NameStr(slot->data.name));
  sprintf(tmppath, "pg_replslot/%s.tmp", NameStr(slot->data.name));

     
                                                                          
                                                                           
                                                                            
                                                                          
                                                                          
     
  if (rename(path, tmppath) == 0)
  {
       
                                                                          
                                                                           
                                                                          
                                                   
                                                                     
                
       
    START_CRIT_SECTION();
    fsync_fname(tmppath, true);
    fsync_fname("pg_replslot", true);
    END_CRIT_SECTION();
  }
  else
  {
    bool fail_softly = slot->data.persistency != RS_PERSISTENT;

    SpinLockAcquire(&slot->mutex);
    slot->active_pid = 0;
    SpinLockRelease(&slot->mutex);

                                             
    ConditionVariableBroadcast(&slot->active_cv);

    ereport(fail_softly ? WARNING : ERROR, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", path, tmppath)));
  }

     
                                                                          
                                                                           
                                                                            
                                                                        
                         
     
                                            
     
  LWLockAcquire(ReplicationSlotControlLock, LW_EXCLUSIVE);
  slot->active_pid = 0;
  slot->in_use = false;
  LWLockRelease(ReplicationSlotControlLock);
  ConditionVariableBroadcast(&slot->active_cv);

     
                                                                          
             
     
  ReplicationSlotsComputeRequiredXmin(false);
  ReplicationSlotsComputeRequiredLSN();

     
                                                                          
                                                                         
                                                                       
     
  if (!rmtree(tmppath, true))
  {
    ereport(WARNING, (errmsg("could not remove directory \"%s\"", tmppath)));
  }

     
                                                                             
                                                                       
     
  LWLockRelease(ReplicationSlotAllocationLock);
}

   
                                                                              
                                                        
   
void
ReplicationSlotSave(void)
{
  char path[MAXPGPATH];

  Assert(MyReplicationSlot != NULL);

  sprintf(path, "pg_replslot/%s", NameStr(MyReplicationSlot->data.name));
  SaveSlotToPath(MyReplicationSlot, path, ERROR);
}

   
                                                                          
                        
   
                                                                         
                                                                   
   
void
ReplicationSlotMarkDirty(void)
{
  ReplicationSlot *slot = MyReplicationSlot;

  Assert(MyReplicationSlot != NULL);

  SpinLockAcquire(&slot->mutex);
  MyReplicationSlot->just_dirtied = true;
  MyReplicationSlot->dirty = true;
  SpinLockRelease(&slot->mutex);
}

   
                                                                         
                                                          
   
void
ReplicationSlotPersist(void)
{
  ReplicationSlot *slot = MyReplicationSlot;

  Assert(slot != NULL);
  Assert(slot->data.persistency != RS_PERSISTENT);

  SpinLockAcquire(&slot->mutex);
  slot->data.persistency = RS_PERSISTENT;
  SpinLockRelease(&slot->mutex);

  ReplicationSlotMarkDirty();
  ReplicationSlotSave();
}

   
                                                                           
   
                                                                      
                
   
void
ReplicationSlotsComputeRequiredXmin(bool already_locked)
{
  int i;
  TransactionId agg_xmin = InvalidTransactionId;
  TransactionId agg_catalog_xmin = InvalidTransactionId;

  Assert(ReplicationSlotCtl != NULL);

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];
    TransactionId effective_xmin;
    TransactionId effective_catalog_xmin;

    if (!s->in_use)
    {
      continue;
    }

    SpinLockAcquire(&s->mutex);
    effective_xmin = s->effective_xmin;
    effective_catalog_xmin = s->effective_catalog_xmin;
    SpinLockRelease(&s->mutex);

                             
    if (TransactionIdIsValid(effective_xmin) && (!TransactionIdIsValid(agg_xmin) || TransactionIdPrecedes(effective_xmin, agg_xmin)))
    {
      agg_xmin = effective_xmin;
    }

                                
    if (TransactionIdIsValid(effective_catalog_xmin) && (!TransactionIdIsValid(agg_catalog_xmin) || TransactionIdPrecedes(effective_catalog_xmin, agg_catalog_xmin)))
    {
      agg_catalog_xmin = effective_catalog_xmin;
    }
  }

  LWLockRelease(ReplicationSlotControlLock);

  ProcArraySetReplicationSlotXmin(agg_xmin, agg_catalog_xmin, already_locked);
}

   
                                                                           
   
void
ReplicationSlotsComputeRequiredLSN(void)
{
  int i;
  XLogRecPtr min_required = InvalidXLogRecPtr;

  Assert(ReplicationSlotCtl != NULL);

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];
    XLogRecPtr restart_lsn;

    if (!s->in_use)
    {
      continue;
    }

    SpinLockAcquire(&s->mutex);
    restart_lsn = s->data.restart_lsn;
    SpinLockRelease(&s->mutex);

    if (restart_lsn != InvalidXLogRecPtr && (min_required == InvalidXLogRecPtr || restart_lsn < min_required))
    {
      min_required = restart_lsn;
    }
  }
  LWLockRelease(ReplicationSlotControlLock);

  XLogSetReplicationSlotMinimumLSN(min_required);
}

   
                                                                     
   
                                                                           
                
   
                                                                              
                                       
   
                                                                              
                                                                        
   
XLogRecPtr
ReplicationSlotsComputeLogicalRestartLSN(void)
{
  XLogRecPtr result = InvalidXLogRecPtr;
  int i;

  if (max_replication_slots <= 0)
  {
    return InvalidXLogRecPtr;
  }

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s;
    XLogRecPtr restart_lsn;

    s = &ReplicationSlotCtl->replication_slots[i];

                                                            
    if (!s->in_use)
    {
      continue;
    }

                                                
    if (!SlotIsLogical(s))
    {
      continue;
    }

                                                                 
    SpinLockAcquire(&s->mutex);
    restart_lsn = s->data.restart_lsn;
    SpinLockRelease(&s->mutex);

    if (result == InvalidXLogRecPtr || restart_lsn < result)
    {
      result = restart_lsn;
    }
  }

  LWLockRelease(ReplicationSlotControlLock);

  return result;
}

   
                                                                               
                        
   
                                                                              
                                                                            
                     
   
bool
ReplicationSlotsCountDBSlots(Oid dboid, int *nslots, int *nactive)
{
  int i;

  *nslots = *nactive = 0;

  if (max_replication_slots <= 0)
  {
    return false;
  }

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s;

    s = &ReplicationSlotCtl->replication_slots[i];

                                                            
    if (!s->in_use)
    {
      continue;
    }

                                                        
    if (!SlotIsLogical(s))
    {
      continue;
    }

                                
    if (s->data.database != dboid)
    {
      continue;
    }

                                        
    SpinLockAcquire(&s->mutex);
    (*nslots)++;
    if (s->active_pid != 0)
    {
      (*nactive)++;
    }
    SpinLockRelease(&s->mutex);
  }
  LWLockRelease(ReplicationSlotControlLock);

  if (*nslots > 0)
  {
    return true;
  }
  return false;
}

   
                                                                             
                                                                        
                                                                               
                                  
   
                                                                                
                                                                              
                                               
   
                                                                      
                                                             
   
void
ReplicationSlotsDropDBSlots(Oid dboid)
{
  int i;

  if (max_replication_slots <= 0)
  {
    return;
  }

restart:
  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s;
    char *slotname;
    int active_pid;

    s = &ReplicationSlotCtl->replication_slots[i];

                                                            
    if (!s->in_use)
    {
      continue;
    }

                                                        
    if (!SlotIsLogical(s))
    {
      continue;
    }

                                
    if (s->data.database != dboid)
    {
      continue;
    }

                                                                     
    SpinLockAcquire(&s->mutex);
                                                               
    slotname = NameStr(s->data.name);
    active_pid = s->active_pid;
    if (active_pid == 0)
    {
      MyReplicationSlot = s;
      s->active_pid = MyProcPid;
    }
    SpinLockRelease(&s->mutex);

       
                                                                      
                                                                  
                                                                        
       
                                                                   
       
    if (active_pid)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("replication slot \"%s\" is active for PID %d", slotname, active_pid)));
    }

       
                                                                       
                                                                      
                                                  
                                    
       
                                                                          
                                                
       
    LWLockRelease(ReplicationSlotControlLock);
    ReplicationSlotDropAcquired();
    goto restart;
  }
  LWLockRelease(ReplicationSlotControlLock);
}

   
                                                                       
          
   
void
CheckSlotRequirements(void)
{
     
                                                                          
                           
     

  if (max_replication_slots == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), (errmsg("replication slots can only be used if max_replication_slots > 0"))));
  }

  if (wal_level < WAL_LEVEL_REPLICA)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("replication slots can only be used if wal_level >= replica")));
  }
}

   
                                              
   
                                                                              
                                  
   
void
ReplicationSlotReserveWal(void)
{
  ReplicationSlot *slot = MyReplicationSlot;

  Assert(slot != NULL);
  Assert(slot->data.restart_lsn == InvalidXLogRecPtr);

     
                                                                           
                                                                             
                                                                             
                                                                             
                                    
     
  while (true)
  {
    XLogSegNo segno;
    XLogRecPtr restart_lsn;

       
                                                                           
                                                                       
                
       
                                                                           
                                                                         
                                                                        
                                                                         
                        
       
    if (!RecoveryInProgress() && SlotIsLogical(slot))
    {
      XLogRecPtr flushptr;

                                            
      restart_lsn = GetXLogInsertRecPtr();
      SpinLockAcquire(&slot->mutex);
      slot->data.restart_lsn = restart_lsn;
      SpinLockRelease(&slot->mutex);

                                                         
      flushptr = LogStandbySnapshot();

                                              
      XLogFlush(flushptr);
    }
    else
    {
      restart_lsn = GetRedoRecPtr();
      SpinLockAcquire(&slot->mutex);
      slot->data.restart_lsn = restart_lsn;
      SpinLockRelease(&slot->mutex);
    }

                                                 
    ReplicationSlotsComputeRequiredLSN();

       
                                                                       
                                                                    
                                                                           
                                                                           
                        
       
    XLByteToSeg(slot->data.restart_lsn, segno, wal_segment_size);
    if (XLogGetLastRemovedSegno() < segno)
    {
      break;
    }
  }
}

   
                                        
   
                                                                        
             
   
void
CheckPointReplicationSlots(void)
{
  int i;

  elog(DEBUG1, "performing replication slot checkpoint");

     
                                                                           
                                                                           
                                                                       
                                                                             
                                                                       
     
  LWLockAcquire(ReplicationSlotAllocationLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];
    char path[MAXPGPATH];

    if (!s->in_use)
    {
      continue;
    }

                                                                       
    sprintf(path, "pg_replslot/%s", NameStr(s->data.name));
    SaveSlotToPath(s, path, LOG);
  }
  LWLockRelease(ReplicationSlotAllocationLock);
}

   
                                                                            
                                                   
   
void
StartupReplicationSlots(void)
{
  DIR *replication_dir;
  struct dirent *replication_de;

  elog(DEBUG1, "starting up replication slots");

                                                               
  replication_dir = AllocateDir("pg_replslot");
  while ((replication_de = ReadDir(replication_dir, "pg_replslot")) != NULL)
  {
    struct stat statbuf;
    char path[MAXPGPATH + 12];

    if (strcmp(replication_de->d_name, ".") == 0 || strcmp(replication_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(path, sizeof(path), "pg_replslot/%s", replication_de->d_name);

                                                                      
    if (lstat(path, &statbuf) == 0 && !S_ISDIR(statbuf.st_mode))
    {
      continue;
    }

                                                                      
    if (pg_str_endswith(replication_de->d_name, ".tmp"))
    {
      if (!rmtree(path, true))
      {
        ereport(WARNING, (errmsg("could not remove directory \"%s\"", path)));
        continue;
      }
      fsync_fname("pg_replslot", true);
      continue;
    }

                                                      
    RestoreSlotFromDisk(replication_de->d_name);
  }
  FreeDir(replication_dir);

                                             
  if (max_replication_slots <= 0)
  {
    return;
  }

                                                                         
  ReplicationSlotsComputeRequiredXmin(false);
  ReplicationSlotsComputeRequiredLSN();
}

        
                                                      
   
                                                                               
                                                         
        
   
static void
CreateSlotOnDisk(ReplicationSlot *slot)
{
  char tmppath[MAXPGPATH];
  char path[MAXPGPATH];
  struct stat st;

     
                                                                           
                                                                             
                                                                    
     

  sprintf(path, "pg_replslot/%s", NameStr(slot->data.name));
  sprintf(tmppath, "pg_replslot/%s.tmp", NameStr(slot->data.name));

     
                                                                             
                                                                           
                                                                           
                                                                     
              
     
  if (stat(tmppath, &st) == 0 && S_ISDIR(st.st_mode))
  {
    rmtree(tmppath, true);
  }

                                                      
  if (MakePGDirectory(tmppath) < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create directory \"%s\": %m", tmppath)));
  }
  fsync_fname(tmppath, true);

                                    
  slot->dirty = true;                                          
  SaveSlotToPath(slot, tmppath, ERROR);

                                        
  if (rename(tmppath, path) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", tmppath, path)));
  }

     
                                                                             
                                                                       
                                                          
     
  START_CRIT_SECTION();

  fsync_fname(path, true);
  fsync_fname("pg_replslot", true);

  END_CRIT_SECTION();
}

   
                                                                        
   
static void
SaveSlotToPath(ReplicationSlot *slot, const char *dir, int elevel)
{
  char tmppath[MAXPGPATH];
  char path[MAXPGPATH];
  int fd;
  ReplicationSlotOnDisk cp;
  bool was_dirty;

                                                          
  SpinLockAcquire(&slot->mutex);
  was_dirty = slot->dirty;
  slot->just_dirtied = false;
  SpinLockRelease(&slot->mutex);

                                                         
  if (!was_dirty)
  {
    return;
  }

  LWLockAcquire(&slot->io_in_progress_lock, LW_EXCLUSIVE);

                           
  memset(&cp, 0, sizeof(ReplicationSlotOnDisk));

  sprintf(tmppath, "%s/state.tmp", dir);
  sprintf(path, "%s/state", dir);

  fd = OpenTransientFile(tmppath, O_CREAT | O_EXCL | O_WRONLY | PG_BINARY);
  if (fd < 0)
  {
       
                                                                         
                                                                       
                                                                          
                                                
       
    int save_errno = errno;

    LWLockRelease(&slot->io_in_progress_lock);
    errno = save_errno;
    ereport(elevel, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmppath)));
    return;
  }

  cp.magic = SLOT_MAGIC;
  INIT_CRC32C(cp.checksum);
  cp.version = SLOT_VERSION;
  cp.length = ReplicationSlotOnDiskV2Size;

  SpinLockAcquire(&slot->mutex);

  memcpy(&cp.slotdata, &slot->data, sizeof(ReplicationSlotPersistentData));

  SpinLockRelease(&slot->mutex);

  COMP_CRC32C(cp.checksum, (char *)(&cp) + SnapBuildOnDiskNotChecksummedSize, SnapBuildOnDiskChecksummedSize);
  FIN_CRC32C(cp.checksum);

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_REPLICATION_SLOT_WRITE);
  if ((write(fd, &cp, sizeof(cp))) != sizeof(cp))
  {
    int save_errno = errno;

    pgstat_report_wait_end();
    CloseTransientFile(fd);
    LWLockRelease(&slot->io_in_progress_lock);

                                                                    
    errno = save_errno ? save_errno : ENOSPC;
    ereport(elevel, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
    return;
  }
  pgstat_report_wait_end();

                                
  pgstat_report_wait_start(WAIT_EVENT_REPLICATION_SLOT_SYNC);
  if (pg_fsync(fd) != 0)
  {
    int save_errno = errno;

    pgstat_report_wait_end();
    CloseTransientFile(fd);
    LWLockRelease(&slot->io_in_progress_lock);
    errno = save_errno;
    ereport(elevel, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", tmppath)));
    return;
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    int save_errno = errno;

    LWLockRelease(&slot->io_in_progress_lock);
    errno = save_errno;
    ereport(elevel, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
    return;
  }

                                                          
  if (rename(tmppath, path) != 0)
  {
    int save_errno = errno;

    LWLockRelease(&slot->io_in_progress_lock);
    errno = save_errno;
    ereport(elevel, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", tmppath, path)));
    return;
  }

     
                                                                             
     
  START_CRIT_SECTION();

  fsync_fname(path, false);
  fsync_fname(dir, true);
  fsync_fname("pg_replslot", true);

  END_CRIT_SECTION();

     
                                                                        
              
     
  SpinLockAcquire(&slot->mutex);
  if (!slot->just_dirtied)
  {
    slot->dirty = false;
  }
  SpinLockRelease(&slot->mutex);

  LWLockRelease(&slot->io_in_progress_lock);
}

   
                                             
   
static void
RestoreSlotFromDisk(const char *name)
{
  ReplicationSlotOnDisk cp;
  int i;
  char slotdir[MAXPGPATH + 12];
  char path[MAXPGPATH + 22];
  int fd;
  bool restored = false;
  int readBytes;
  pg_crc32c checksum;

                                                              

                                     
  sprintf(slotdir, "pg_replslot/%s", name);
  sprintf(path, "%s/state.tmp", slotdir);
  if (unlink(path) < 0 && errno != ENOENT)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
  }

  sprintf(path, "%s/state", slotdir);

  elog(DEBUG1, "restoring replication slot from \"%s\"", path);

                                                                 
  fd = OpenTransientFile(path, O_RDWR | PG_BINARY);

     
                                                                            
                                                   
     
  if (fd < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

     
                                                                         
                                                                         
     
  pgstat_report_wait_start(WAIT_EVENT_REPLICATION_SLOT_RESTORE_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", path)));
  }
  pgstat_report_wait_end();

                                      
  START_CRIT_SECTION();
  fsync_fname(slotdir, true);
  END_CRIT_SECTION();

                                                                          
  pgstat_report_wait_start(WAIT_EVENT_REPLICATION_SLOT_READ);
  readBytes = read(fd, &cp, ReplicationSlotOnDiskConstantSize);
  pgstat_report_wait_end();
  if (readBytes != ReplicationSlotOnDiskConstantSize)
  {
    if (readBytes < 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, (Size)ReplicationSlotOnDiskConstantSize)));
    }
  }

                    
  if (cp.magic != SLOT_MAGIC)
  {
    ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("replication slot file \"%s\" has wrong magic number: %u instead of %u", path, cp.magic, SLOT_MAGIC)));
  }

                      
  if (cp.version != SLOT_VERSION)
  {
    ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("replication slot file \"%s\" has unsupported version %u", path, cp.version)));
  }

                                
  if (cp.length != ReplicationSlotOnDiskV2Size)
  {
    ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("replication slot file \"%s\" has corrupted length %u", path, cp.length)));
  }

                                                       
  pgstat_report_wait_start(WAIT_EVENT_REPLICATION_SLOT_READ);
  readBytes = read(fd, (char *)&cp + ReplicationSlotOnDiskConstantSize, cp.length);
  pgstat_report_wait_end();
  if (readBytes != cp.length)
  {
    if (readBytes < 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, (Size)cp.length)));
    }
  }

  if (CloseTransientFile(fd))
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }

                          
  INIT_CRC32C(checksum);
  COMP_CRC32C(checksum, (char *)&cp + SnapBuildOnDiskNotChecksummedSize, SnapBuildOnDiskChecksummedSize);
  FIN_CRC32C(checksum);

  if (!EQ_CRC32C(checksum, cp.checksum))
  {
    ereport(PANIC, (errmsg("checksum mismatch for replication slot file \"%s\": is %u, should be %u", path, checksum, cp.checksum)));
  }

     
                                                                           
         
     
  if (cp.slotdata.persistency != RS_PERSISTENT)
  {
    if (!rmtree(slotdir, true))
    {
      ereport(WARNING, (errmsg("could not remove directory \"%s\"", slotdir)));
    }
    fsync_fname("pg_replslot", true);
    return;
  }

     
                                                                         
                                                                          
                                               
     
                                                                        
                                                                         
               
     
                                                               
                                                                     
     
  if (cp.slotdata.database != InvalidOid && wal_level < WAL_LEVEL_LOGICAL)
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication slot \"%s\" exists, but wal_level < logical", NameStr(cp.slotdata.name)), errhint("Change wal_level to be logical or higher.")));
  }
  else if (wal_level < WAL_LEVEL_REPLICA)
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("physical replication slot \"%s\" exists, but wal_level < replica", NameStr(cp.slotdata.name)), errhint("Change wal_level to be replica or higher.")));
  }

                                                      
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *slot;

    slot = &ReplicationSlotCtl->replication_slots[i];

    if (slot->in_use)
    {
      continue;
    }

                                                   
    memcpy(&slot->data, &cp.slotdata, sizeof(ReplicationSlotPersistentData));

                                    
    slot->effective_xmin = cp.slotdata.xmin;
    slot->effective_catalog_xmin = cp.slotdata.catalog_xmin;

    slot->candidate_catalog_xmin = InvalidTransactionId;
    slot->candidate_xmin_lsn = InvalidXLogRecPtr;
    slot->candidate_restart_lsn = InvalidXLogRecPtr;
    slot->candidate_restart_valid = InvalidXLogRecPtr;

    slot->in_use = true;
    slot->active_pid = 0;

    restored = true;
    break;
  }

  if (!restored)
  {
    ereport(FATAL, (errmsg("too many replication slots active before shutdown"), errhint("Increase max_replication_slots and try again.")));
  }
}
