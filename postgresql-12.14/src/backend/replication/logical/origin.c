                                                                            
   
            
                                                    
   
                                                                
   
                  
                                              
   
         
   
                                     
                                                            
                                                                            
                                  
   
                                                                           
                                                                           
                                                                         
                                                                               
                                                                               
                                                                             
                                                  
   
                                                            
                                                                      
                                                                              
                                                                   
                                                                               
                                                                             
                                                                              
                                                                       
                                                                           
                                                                             
                                                                            
                                                                               
                                                                     
   
                                                                          
                                                                          
                                                                            
                    
   
                                                
   
                                                                 
                                                                        
                                                                        
   
                                                                                
                                                                           
                                                                       
                                                                            
                                  
   
                                                                              
                                                                        
                                                                            
                                                                       
                                                                            
                                                           
                                      
   
                                                                               
   

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "funcapi.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"

#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "nodes/execnodes.h"

#include "replication/origin.h"
#include "replication/logical.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/condition_variable.h"
#include "storage/copydir.h"

#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_lsn.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"

   
                                            
   
typedef struct ReplicationState
{
     
                                           
     
  RepOriginId roident;

     
                                                         
     
  XLogRecPtr remote_lsn;

     
                                                                             
                                                                          
           
     
  XLogRecPtr local_lsn;

     
                                                        
     
  int acquired_by;

     
                                                                   
     
  ConditionVariable origin_cv;

     
                                               
     
  LWLock lock;
} ReplicationState;

   
                                        
   
typedef struct ReplicationStateOnDisk
{
  RepOriginId roident;
  XLogRecPtr remote_lsn;
} ReplicationStateOnDisk;

typedef struct ReplicationStateCtl
{
                                             
  int tranche_id;
                                             
  ReplicationState states[FLEXIBLE_ARRAY_MEMBER];
} ReplicationStateCtl;

                        
RepOriginId replorigin_session_origin = InvalidRepOriginId;                       
XLogRecPtr replorigin_session_origin_lsn = InvalidXLogRecPtr;
TimestampTz replorigin_session_origin_timestamp = 0;

   
                                                                         
                          
   
                                                                   
                          
   
static ReplicationState *replication_states;

   
                                                                          
   
static ReplicationStateCtl *replication_states_ctl;

   
                                                                            
                                                                             
                                     
   
static ReplicationState *session_replication_state = NULL;

                              
#define REPLICATION_STATE_MAGIC ((uint32)0x1257DADE)

static void
replorigin_check_prerequisites(bool check_slots, bool recoveryOK)
{
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("only superusers can query or manipulate replication origins")));
  }

  if (check_slots && max_replication_slots == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot query or manipulate replication origin when max_replication_slots = 0")));
  }

  if (!recoveryOK && RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION), errmsg("cannot manipulate replication origins during recovery")));
  }
}

                                                                               
                                                              
                                                                               
   

   
                                                                 
   
                                                                          
   
RepOriginId
replorigin_by_name(char *roname, bool missing_ok)
{
  Form_pg_replication_origin ident;
  Oid roident = InvalidOid;
  HeapTuple tuple;
  Datum roname_d;

  roname_d = CStringGetTextDatum(roname);

  tuple = SearchSysCache1(REPLORIGNAME, roname_d);
  if (HeapTupleIsValid(tuple))
  {
    ident = (Form_pg_replication_origin)GETSTRUCT(tuple);
    roident = ident->roident;
    ReleaseSysCache(tuple);
  }
  else if (!missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("replication origin \"%s\" does not exist", roname)));
  }

  return roident;
}

   
                                
   
                                        
   
RepOriginId
replorigin_create(char *roname)
{
  Oid roident;
  HeapTuple tuple = NULL;
  Relation rel;
  Datum roname_d;
  SnapshotData SnapshotDirty;
  SysScanDesc scan;
  ScanKeyData key;

  roname_d = CStringGetTextDatum(roname);

  Assert(IsTransactionState());

     
                                                                           
                                                               
                                                                            
                                                                          
                                                                           
             
     
                                                                         
                                                                            
                                                                     
                                                                             
                                                                            
                     
     
  InitDirtySnapshot(SnapshotDirty);

  rel = table_open(ReplicationOriginRelationId, ExclusiveLock);

  for (roident = InvalidOid + 1; roident < PG_UINT16_MAX; roident++)
  {
    bool nulls[Natts_pg_replication_origin];
    Datum values[Natts_pg_replication_origin];
    bool collides;

    CHECK_FOR_INTERRUPTS();

    ScanKeyInit(&key, Anum_pg_replication_origin_roident, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roident));

    scan = systable_beginscan(rel, ReplicationOriginIdentIndex, true              , &SnapshotDirty, 1, &key);

    collides = HeapTupleIsValid(systable_getnext(scan));

    systable_endscan(scan);

    if (!collides)
    {
         
                                                                       
                                                        
         
      memset(&nulls, 0, sizeof(nulls));

      values[Anum_pg_replication_origin_roident - 1] = ObjectIdGetDatum(roident);
      values[Anum_pg_replication_origin_roname - 1] = roname_d;

      tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);
      CatalogTupleInsert(rel, tuple);
      CommandCounterIncrement();
      break;
    }
  }

                               
  table_close(rel, ExclusiveLock);

  if (tuple == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("could not find free replication origin OID")));
  }

  heap_freetuple(tuple);
  return roident;
}

   
                            
   
                                        
   
void
replorigin_drop(RepOriginId roident, bool nowait)
{
  HeapTuple tuple;
  Relation rel;
  int i;

  Assert(IsTransactionState());

     
                                                                     
                                                     
     
  rel = table_open(ReplicationOriginRelationId, ExclusiveLock);

     
                                                                         
     
restart:
  tuple = NULL;
  LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationState *state = &replication_states[i];

    if (state->roident == roident)
    {
                                       
      if (state->acquired_by != 0)
      {
        ConditionVariable *cv;

        if (nowait)
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("could not drop replication origin with OID %d, in use by PID %d", state->roident, state->acquired_by)));
        }

           
                                                                      
                                                       
                                                                     
                                                                     
                                                
           
        cv = &state->origin_cv;

        LWLockRelease(ReplicationOriginLock);

        ConditionVariableSleep(cv, WAIT_EVENT_REPLICATION_ORIGIN_DROP);
        goto restart;
      }

                                      
      {
        xl_replorigin_drop xlrec;

        xlrec.node_id = roident;
        XLogBeginInsert();
        XLogRegisterData((char *)(&xlrec), sizeof(xlrec));
        XLogInsert(RM_REPLORIGIN_ID, XLOG_REPLORIGIN_DROP);
      }

                                         
      state->roident = InvalidRepOriginId;
      state->remote_lsn = InvalidXLogRecPtr;
      state->local_lsn = InvalidXLogRecPtr;
      break;
    }
  }
  LWLockRelease(ReplicationOriginLock);
  ConditionVariableCancelSleep();

     
                                           
     
  tuple = SearchSysCache1(REPLORIGIDENT, ObjectIdGetDatum(roident));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for replication origin with oid %u", roident);
  }

  CatalogTupleDelete(rel, &tuple->t_self);
  ReleaseSysCache(tuple);

  CommandCounterIncrement();

                              
  table_close(rel, ExclusiveLock);
}

   
                                                               
   
                                                         
   
                                                         
   
bool
replorigin_by_oid(RepOriginId roident, bool missing_ok, char **roname)
{
  HeapTuple tuple;
  Form_pg_replication_origin ric;

  Assert(OidIsValid((Oid)roident));
  Assert(roident != InvalidRepOriginId);
  Assert(roident != DoNotReplicateId);

  tuple = SearchSysCache1(REPLORIGIDENT, ObjectIdGetDatum((Oid)roident));

  if (HeapTupleIsValid(tuple))
  {
    ric = (Form_pg_replication_origin)GETSTRUCT(tuple);
    *roname = text_to_cstring(&ric->roname);
    ReleaseSysCache(tuple);

    return true;
  }
  else
  {
    *roname = NULL;

    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("replication origin with OID %u does not exist", roident)));
    }

    return false;
  }
}

                                                                               
                                                
                                                                               
   

Size
ReplicationOriginShmemSize(void)
{
  Size size = 0;

     
                                                                            
                                                                             
                                                                   
     
  if (max_replication_slots == 0)
  {
    return size;
  }

  size = add_size(size, offsetof(ReplicationStateCtl, states));

  size = add_size(size, mul_size(max_replication_slots, sizeof(ReplicationState)));
  return size;
}

void
ReplicationOriginShmemInit(void)
{
  bool found;

  if (max_replication_slots == 0)
  {
    return;
  }

  replication_states_ctl = (ReplicationStateCtl *)ShmemInitStruct("ReplicationOriginState", ReplicationOriginShmemSize(), &found);
  replication_states = replication_states_ctl->states;

  if (!found)
  {
    int i;

    MemSet(replication_states_ctl, 0, ReplicationOriginShmemSize());

    replication_states_ctl->tranche_id = LWTRANCHE_REPLICATION_ORIGIN;

    for (i = 0; i < max_replication_slots; i++)
    {
      LWLockInitialize(&replication_states[i].lock, replication_states_ctl->tranche_id);
      ConditionVariableInit(&replication_states[i].origin_cv);
    }
  }

  LWLockRegisterTranche(replication_states_ctl->tranche_id, "replication_origin");
}

                                                                               
                                                                              
                                                                               
                                                                               
                                                                 
   
                                                 
                                                                        
                                                                            
                                                                        
   
                                                           
                                                                   
                                                            
                                                                               
   
void
CheckPointReplicationOrigin(void)
{
  const char *tmppath = "pg_logical/replorigin_checkpoint.tmp";
  const char *path = "pg_logical/replorigin_checkpoint";
  int tmpfd;
  int i;
  uint32 magic = REPLICATION_STATE_MAGIC;
  pg_crc32c crc;

  if (max_replication_slots == 0)
  {
    return;
  }

  INIT_CRC32C(crc);

                                               
  if (unlink(tmppath) < 0 && errno != ENOENT)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", tmppath)));
  }

     
                                                                            
                     
     
  tmpfd = OpenTransientFile(tmppath, O_CREAT | O_EXCL | O_WRONLY | PG_BINARY);
  if (tmpfd < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmppath)));
  }

                   
  errno = 0;
  if ((write(tmpfd, &magic, sizeof(magic))) != sizeof(magic))
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
  }
  COMP_CRC32C(crc, &magic, sizeof(magic));

                                          
  LWLockAcquire(ReplicationOriginLock, LW_SHARED);

                         
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationStateOnDisk disk_state;
    ReplicationState *curstate = &replication_states[i];
    XLogRecPtr local_lsn;

    if (curstate->roident == InvalidRepOriginId)
    {
      continue;
    }

                                                    
    memset(&disk_state, 0, sizeof(disk_state));

    LWLockAcquire(&curstate->lock, LW_SHARED);

    disk_state.roident = curstate->roident;

    disk_state.remote_lsn = curstate->remote_lsn;
    local_lsn = curstate->local_lsn;

    LWLockRelease(&curstate->lock);

                                                                
    XLogFlush(local_lsn);

    errno = 0;
    if ((write(tmpfd, &disk_state, sizeof(disk_state))) != sizeof(disk_state))
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
    }

    COMP_CRC32C(crc, &disk_state, sizeof(disk_state));
  }

  LWLockRelease(ReplicationOriginLock);

                         
  FIN_CRC32C(crc);
  errno = 0;
  if ((write(tmpfd, &crc, sizeof(crc))) != sizeof(crc))
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
  }

  if (CloseTransientFile(tmpfd))
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
  }

                                                                 
  durable_rename(tmppath, path, PANIC);
}

   
                                                                           
                                
   
                                                                             
                                                                                
                                                                   
   
void
StartupReplicationOrigin(void)
{
  const char *path = "pg_logical/replorigin_checkpoint";
  int fd;
  int readBytes;
  uint32 magic = REPLICATION_STATE_MAGIC;
  int last_state = 0;
  pg_crc32c file_crc;
  pg_crc32c crc;

                                                      
#ifdef USE_ASSERT_CHECKING
  static bool already_started = false;

  Assert(!already_started);
  already_started = true;
#endif

  if (max_replication_slots == 0)
  {
    return;
  }

  INIT_CRC32C(crc);

  elog(DEBUG2, "starting up replication origin progress state");

  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);

     
                                                                            
                   
     
  if (fd < 0 && errno == ENOENT)
  {
    return;
  }
  else if (fd < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

                                                                
  readBytes = read(fd, &magic, sizeof(magic));
  if (readBytes != sizeof(magic))
  {
    if (readBytes < 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, sizeof(magic))));
    }
  }
  COMP_CRC32C(crc, &magic, sizeof(magic));

  if (magic != REPLICATION_STATE_MAGIC)
  {
    ereport(PANIC, (errmsg("replication checkpoint has wrong magic %u instead of %u", magic, REPLICATION_STATE_MAGIC)));
  }

                                                             

                                                                      
  while (true)
  {
    ReplicationStateOnDisk disk_state;

    readBytes = read(fd, &disk_state, sizeof(disk_state));

                         
    if (readBytes == sizeof(crc))
    {
                                      
      file_crc = *(pg_crc32c *)&disk_state;
      break;
    }

    if (readBytes < 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }

    if (readBytes != sizeof(disk_state))
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, sizeof(disk_state))));
    }

    COMP_CRC32C(crc, &disk_state, sizeof(disk_state));

    if (last_state == max_replication_slots)
    {
      ereport(PANIC, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("could not find free replication state, increase max_replication_slots")));
    }

                                    
    replication_states[last_state].roident = disk_state.roident;
    replication_states[last_state].remote_lsn = disk_state.remote_lsn;
    last_state++;

    elog(LOG, "recovered replication state of node %u to %X/%X", disk_state.roident, (uint32)(disk_state.remote_lsn >> 32), (uint32)disk_state.remote_lsn);
  }

                          
  FIN_CRC32C(crc);
  if (file_crc != crc)
  {
    ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("replication slot checkpoint has wrong checksum %u, expected %u", crc, file_crc)));
  }

  if (CloseTransientFile(fd))
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }
}

void
replorigin_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  switch (info)
  {
  case XLOG_REPLORIGIN_SET:
  {
    xl_replorigin_set *xlrec = (xl_replorigin_set *)XLogRecGetData(record);

    replorigin_advance(xlrec->node_id, xlrec->remote_lsn, record->EndRecPtr, xlrec->force               , false /* WAL log */);
    break;
  }
  case XLOG_REPLORIGIN_DROP:
  {
    xl_replorigin_drop *xlrec;
    int i;

    xlrec = (xl_replorigin_drop *)XLogRecGetData(record);

    for (i = 0; i < max_replication_slots; i++)
    {
      ReplicationState *state = &replication_states[i];

                          
      if (state->roident == xlrec->node_id)
      {
                         
        state->roident = InvalidRepOriginId;
        state->remote_lsn = InvalidXLogRecPtr;
        state->local_lsn = InvalidXLogRecPtr;
        break;
      }
    }
    break;
  }
  default:
    elog(PANIC, "replorigin_redo: unknown op code %u", info);
  }
}

   
                                                                            
                                                                            
                                                                           
                                                                          
                                                                         
                                                                        
   
                                                                               
                                                                 
   
                                                                        
                               
   
void
replorigin_advance(RepOriginId node, XLogRecPtr remote_commit, XLogRecPtr local_commit, bool go_backward, bool wal_log)
{
  int i;
  ReplicationState *replication_state = NULL;
  ReplicationState *free_state = NULL;

  Assert(node != InvalidRepOriginId);

                                       
  if (node == DoNotReplicateId)
  {
    return;
  }

     
                                                                        
                                                                            
                                                                         
                                                
     

                                                                     
  LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

     
                                                                             
          
     
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationState *curstate = &replication_states[i];

                                               
    if (curstate->roident == InvalidRepOriginId && free_state == NULL)
    {
      free_state = curstate;
      continue;
    }

                      
    if (curstate->roident != node)
    {
      continue;
    }

                        
    replication_state = curstate;

    LWLockAcquire(&replication_state->lock, LW_EXCLUSIVE);

                                                  
    if (replication_state->acquired_by != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("replication origin with OID %d is already active for PID %d", replication_state->roident, replication_state->acquired_by)));
    }

    break;
  }

  if (replication_state == NULL && free_state == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("could not find free replication state slot for replication origin with OID %u", node), errhint("Increase max_replication_slots and try again.")));
  }

  if (replication_state == NULL)
  {
                             
    LWLockAcquire(&free_state->lock, LW_EXCLUSIVE);
    replication_state = free_state;
    Assert(replication_state->remote_lsn == InvalidXLogRecPtr);
    Assert(replication_state->local_lsn == InvalidXLogRecPtr);
    replication_state->roident = node;
  }

  Assert(replication_state->roident != InvalidRepOriginId);

     
                                                                          
                                                                            
                                                                       
     
  if (wal_log)
  {
    xl_replorigin_set xlrec;

    xlrec.remote_lsn = remote_commit;
    xlrec.node_id = node;
    xlrec.force = go_backward;

    XLogBeginInsert();
    XLogRegisterData((char *)(&xlrec), sizeof(xlrec));

    XLogInsert(RM_REPLORIGIN_ID, XLOG_REPLORIGIN_SET);
  }

     
                                                                          
                                                                         
                            
     
  if (go_backward || replication_state->remote_lsn < remote_commit)
  {
    replication_state->remote_lsn = remote_commit;
  }
  if (local_commit != InvalidXLogRecPtr && (go_backward || replication_state->local_lsn < local_commit))
  {
    replication_state->local_lsn = local_commit;
  }
  LWLockRelease(&replication_state->lock);

     
                                                                           
                                   
     
  LWLockRelease(ReplicationOriginLock);
}

XLogRecPtr
replorigin_get_progress(RepOriginId node, bool flush)
{
  int i;
  XLogRecPtr local_lsn = InvalidXLogRecPtr;
  XLogRecPtr remote_lsn = InvalidXLogRecPtr;

                                                     
  LWLockAcquire(ReplicationOriginLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationState *state;

    state = &replication_states[i];

    if (state->roident == node)
    {
      LWLockAcquire(&state->lock, LW_SHARED);

      remote_lsn = state->remote_lsn;
      local_lsn = state->local_lsn;

      LWLockRelease(&state->lock);

      break;
    }
  }

  LWLockRelease(ReplicationOriginLock);

  if (flush && local_lsn != InvalidXLogRecPtr)
  {
    XLogFlush(local_lsn);
  }

  return remote_lsn;
}

   
                                                                               
         
   
static void
ReplicationOriginExitCleanup(int code, Datum arg)
{
  ConditionVariable *cv = NULL;

  LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

  if (session_replication_state != NULL && session_replication_state->acquired_by == MyProcPid)
  {
    cv = &session_replication_state->origin_cv;

    session_replication_state->acquired_by = 0;
    session_replication_state = NULL;
  }

  LWLockRelease(ReplicationOriginLock);

  if (cv)
  {
    ConditionVariableBroadcast(cv);
  }
}

   
                                                                        
                                                                          
                                                  
                                 
   
                                                                               
                                                                            
                                    
   
void
replorigin_session_setup(RepOriginId node)
{
  static bool registered_cleanup;
  int i;
  int free_slot = -1;

  if (!registered_cleanup)
  {
    on_shmem_exit(ReplicationOriginExitCleanup, 0);
    registered_cleanup = true;
  }

  Assert(max_replication_slots > 0);

  if (session_replication_state != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot setup replication origin when one is already setup")));
  }

                                                                     
  LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

     
                                                                             
          
     
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationState *curstate = &replication_states[i];

                                               
    if (curstate->roident == InvalidRepOriginId && free_slot == -1)
    {
      free_slot = i;
      continue;
    }

                      
    if (curstate->roident != node)
    {
      continue;
    }

    else if (curstate->acquired_by != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("replication origin with OID %d is already active for PID %d", curstate->roident, curstate->acquired_by)));
    }

                        
    session_replication_state = curstate;
  }

  if (session_replication_state == NULL && free_slot == -1)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("could not find free replication state slot for replication origin with OID %u", node), errhint("Increase max_replication_slots and try again.")));
  }
  else if (session_replication_state == NULL)
  {
                             
    session_replication_state = &replication_states[free_slot];
    Assert(session_replication_state->remote_lsn == InvalidXLogRecPtr);
    Assert(session_replication_state->local_lsn == InvalidXLogRecPtr);
    session_replication_state->roident = node;
  }

  Assert(session_replication_state->roident != InvalidRepOriginId);

  session_replication_state->acquired_by = MyProcPid;

  LWLockRelease(ReplicationOriginLock);

                                      
  ConditionVariableBroadcast(&session_replication_state->origin_cv);
}

   
                                                        
   
                                                                
                               
   
void
replorigin_session_reset(void)
{
  ConditionVariable *cv;

  Assert(max_replication_slots != 0);

  if (session_replication_state == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("no replication origin is configured")));
  }

  LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

  session_replication_state->acquired_by = 0;
  cv = &session_replication_state->origin_cv;
  session_replication_state = NULL;

  LWLockRelease(ReplicationOriginLock);

  ConditionVariableBroadcast(cv);
}

   
                                                                     
                      
   
                                                               
   
void
replorigin_session_advance(XLogRecPtr remote_commit, XLogRecPtr local_commit)
{
  Assert(session_replication_state != NULL);
  Assert(session_replication_state->roident != InvalidRepOriginId);

  LWLockAcquire(&session_replication_state->lock, LW_EXCLUSIVE);
  if (session_replication_state->local_lsn < local_commit)
  {
    session_replication_state->local_lsn = local_commit;
  }
  if (session_replication_state->remote_lsn < remote_commit)
  {
    session_replication_state->remote_lsn = remote_commit;
  }
  LWLockRelease(&session_replication_state->lock);
}

   
                                                                          
                                                     
   
XLogRecPtr
replorigin_session_get_progress(bool flush)
{
  XLogRecPtr remote_lsn;
  XLogRecPtr local_lsn;

  Assert(session_replication_state != NULL);

  LWLockAcquire(&session_replication_state->lock, LW_SHARED);
  remote_lsn = session_replication_state->remote_lsn;
  local_lsn = session_replication_state->local_lsn;
  LWLockRelease(&session_replication_state->lock);

  if (flush && local_lsn != InvalidXLogRecPtr)
  {
    XLogFlush(local_lsn);
  }

  return remote_lsn;
}

                                                                               
                                                      
   
                                                                               
                                                                               
   

   
                                                                             
        
   
Datum
pg_replication_origin_create(PG_FUNCTION_ARGS)
{
  char *name;
  RepOriginId roident;

  replorigin_check_prerequisites(false, false);

  name = text_to_cstring((text *)DatumGetPointer(PG_GETARG_DATUM(0)));

                                                                  
  if (IsReservedName(name))
  {
    ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("replication origin name \"%s\" is reserved", name), errdetail("Origin names starting with \"pg_\" are reserved.")));
  }

     
                                                                     
                                                            
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strncmp(name, "regress_", 8) != 0)
  {
    elog(WARNING, "replication origins created by regression test cases should have names starting with \"regress_\"");
  }
#endif

  roident = replorigin_create(name);

  pfree(name);

  PG_RETURN_OID(roident);
}

   
                            
   
Datum
pg_replication_origin_drop(PG_FUNCTION_ARGS)
{
  char *name;
  RepOriginId roident;

  replorigin_check_prerequisites(false, false);

  name = text_to_cstring((text *)DatumGetPointer(PG_GETARG_DATUM(0)));

  roident = replorigin_by_name(name, false);
  Assert(OidIsValid(roident));

  replorigin_drop(roident, true);

  pfree(name);

  PG_RETURN_VOID();
}

   
                                       
   
Datum
pg_replication_origin_oid(PG_FUNCTION_ARGS)
{
  char *name;
  RepOriginId roident;

  replorigin_check_prerequisites(false, false);

  name = text_to_cstring((text *)DatumGetPointer(PG_GETARG_DATUM(0)));
  roident = replorigin_by_name(name, true);

  pfree(name);

  if (OidIsValid(roident))
  {
    PG_RETURN_OID(roident);
  }
  PG_RETURN_NULL();
}

   
                                                
   
Datum
pg_replication_origin_session_setup(PG_FUNCTION_ARGS)
{
  char *name;
  RepOriginId origin;

  replorigin_check_prerequisites(true, false);

  name = text_to_cstring((text *)DatumGetPointer(PG_GETARG_DATUM(0)));
  origin = replorigin_by_name(name, false);
  replorigin_session_setup(origin);

  replorigin_session_origin = origin;

  pfree(name);

  PG_RETURN_VOID();
}

   
                                                 
   
Datum
pg_replication_origin_session_reset(PG_FUNCTION_ARGS)
{
  replorigin_check_prerequisites(true, false);

  replorigin_session_reset();

  replorigin_session_origin = InvalidRepOriginId;
  replorigin_session_origin_lsn = InvalidXLogRecPtr;
  replorigin_session_origin_timestamp = 0;

  PG_RETURN_VOID();
}

   
                                                         
   
Datum
pg_replication_origin_session_is_setup(PG_FUNCTION_ARGS)
{
  replorigin_check_prerequisites(false, false);

  PG_RETURN_BOOL(replorigin_session_origin != InvalidRepOriginId);
}

   
                                                                            
   
                                                                               
                                                                                
                                                            
   
Datum
pg_replication_origin_session_progress(PG_FUNCTION_ARGS)
{
  XLogRecPtr remote_lsn = InvalidXLogRecPtr;
  bool flush = PG_GETARG_BOOL(0);

  replorigin_check_prerequisites(true, false);

  if (session_replication_state == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("no replication origin is configured")));
  }

  remote_lsn = replorigin_session_get_progress(flush);

  if (remote_lsn == InvalidXLogRecPtr)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_LSN(remote_lsn);
}

Datum
pg_replication_origin_xact_setup(PG_FUNCTION_ARGS)
{
  XLogRecPtr location = PG_GETARG_LSN(0);

  replorigin_check_prerequisites(true, false);

  if (session_replication_state == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("no replication origin is configured")));
  }

  replorigin_session_origin_lsn = location;
  replorigin_session_origin_timestamp = PG_GETARG_TIMESTAMPTZ(1);

  PG_RETURN_VOID();
}

Datum
pg_replication_origin_xact_reset(PG_FUNCTION_ARGS)
{
  replorigin_check_prerequisites(true, false);

  replorigin_session_origin_lsn = InvalidXLogRecPtr;
  replorigin_session_origin_timestamp = 0;

  PG_RETURN_VOID();
}

Datum
pg_replication_origin_advance(PG_FUNCTION_ARGS)
{
  text *name = PG_GETARG_TEXT_PP(0);
  XLogRecPtr remote_commit = PG_GETARG_LSN(1);
  RepOriginId node;

  replorigin_check_prerequisites(true, false);

                                                             
  LockRelationOid(ReplicationOriginRelationId, RowExclusiveLock);

  node = replorigin_by_name(text_to_cstring(name), false);

     
                                                                           
                                                                            
                                                               
     
  replorigin_advance(node, remote_commit, InvalidXLogRecPtr, true                  , true /* WAL log */);

  UnlockRelationOid(ReplicationOriginRelationId, RowExclusiveLock);

  PG_RETURN_VOID();
}

   
                                                                         
   
                                                                               
                                                                                
                                                            
   
Datum
pg_replication_origin_progress(PG_FUNCTION_ARGS)
{
  char *name;
  bool flush;
  RepOriginId roident;
  XLogRecPtr remote_lsn = InvalidXLogRecPtr;

  replorigin_check_prerequisites(true, true);

  name = text_to_cstring((text *)DatumGetPointer(PG_GETARG_DATUM(0)));
  flush = PG_GETARG_BOOL(1);

  roident = replorigin_by_name(name, false);
  Assert(OidIsValid(roident));

  remote_lsn = replorigin_get_progress(roident, flush);

  if (remote_lsn == InvalidXLogRecPtr)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_LSN(remote_lsn);
}

Datum
pg_show_replication_origin_status(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  int i;
#define REPLICATION_ORIGIN_PROGRESS_COLS 4

                                                       
  replorigin_check_prerequisites(false, true);

  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  if (tupdesc->natts != REPLICATION_ORIGIN_PROGRESS_COLS)
  {
    elog(ERROR, "wrong function definition");
  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

                                                     
  LWLockAcquire(ReplicationOriginLock, LW_SHARED);

     
                                                                          
                                                                           
                                       
     
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationState *state;
    Datum values[REPLICATION_ORIGIN_PROGRESS_COLS];
    bool nulls[REPLICATION_ORIGIN_PROGRESS_COLS];
    char *roname;

    state = &replication_states[i];

                                         
    if (state->roident == InvalidRepOriginId)
    {
      continue;
    }

    memset(values, 0, sizeof(values));
    memset(nulls, 1, sizeof(nulls));

    values[0] = ObjectIdGetDatum(state->roident);
    nulls[0] = false;

       
                                                                      
                                              
       
    if (replorigin_by_oid(state->roident, true, &roname))
    {
      values[1] = CStringGetTextDatum(roname);
      nulls[1] = false;
    }

    LWLockAcquire(&state->lock, LW_SHARED);

    values[2] = LSNGetDatum(state->remote_lsn);
    nulls[2] = false;

    values[3] = LSNGetDatum(state->local_lsn);
    nulls[3] = false;

    LWLockRelease(&state->lock);

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  tuplestore_donestoring(tupstore);

  LWLockRelease(ReplicationOriginLock);

#undef REPLICATION_ORIGIN_PROGRESS_COLS

  return (Datum)0;
}
