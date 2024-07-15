                                                                            
   
              
                                        
   
                                                                         
                                                                        
   
                  
                                          
   
         
                                                                    
                                                             
                                                      
   
                                                                     
                                                               
                                                                   
                                                                      
                                                                 
                                                     
   
                                                                           
                                                                        
                                                                    
   
                                                                     
                                                                       
                                                                      
                                                                      
                                                                     
                                                                    
                                                             
                                                                 
   
                                           
   
                                                                           
                                                       
                                
                                                                          
                               
                                                                            
              
                                                                            
            
   
                                                                        
                                                                            
   
                                                               
   
                                                                         
                                                               
                                                                        
                                       
                                                                             
                                                     
                                                                        
                                                                      
                                                               
                                                                         
                                                                      
                               
                                                                          
                                                                         
                                                           
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "access/commit_ts.h"
#include "access/htup_details.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "access/xlogreader.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "replication/origin.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/md.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"

   
                                                               
   
#define TWOPHASE_DIR "pg_twophase"

                                                  
int max_prepared_xacts = 0;

   
                                                                          
                                     
   
                                             
   
                                                                              
                                                                              
                                        
   
                                                                            
                                                
   
                                                                             
                                                                             
                                                                          
                                              
   
                                                                              
                                                                              
                 
   
                                                                           
                                                                          
                                           
   
                                                                      
              
   

typedef struct GlobalTransactionData
{
  GlobalTransaction next;                                
  int pgprocno;                                                
  BackendId dummyBackendId;                                         
  TimestampTz prepared_at;                           

     
                                                                         
                                                                            
                                                                        
                                                                            
                
     
  XLogRecPtr prepare_start_lsn;                                          
  XLogRecPtr prepare_end_lsn;                                          
  TransactionId xid;                              

  Oid owner;                                                        
  BackendId locking_backend;                                            
  bool valid;                                                           
  bool ondisk;                                                          
  bool inredo;                                                          
  char gid[GIDSIZE];                                                    
} GlobalTransactionData;

   
                                                                      
                         
   
typedef struct TwoPhaseStateData
{
                                                                 
  GlobalTransaction freeGXacts;

                                          
  int numPrepXacts;

                                                        
  GlobalTransaction prepXacts[FLEXIBLE_ARRAY_MEMBER];
} TwoPhaseStateData;

static TwoPhaseStateData *TwoPhaseState;

   
                                                                           
                                                                        
                                                                             
                                   
   
static GlobalTransaction MyLockedGxact = NULL;

static bool twophaseExitRegistered = false;

static void
RecordTransactionCommitPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, int ninvalmsgs, SharedInvalidationMessage *invalmsgs, bool initfileinval, const char *gid);
static void
RecordTransactionAbortPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, const char *gid);
static void
ProcessRecords(char *bufptr, TransactionId xid, const TwoPhaseCallback callbacks[]);
static void
RemoveGXact(GlobalTransaction gxact);

static void
XlogReadTwoPhaseData(XLogRecPtr lsn, char **buf, int *len);
static char *
ProcessTwoPhaseBuffer(TransactionId xid, XLogRecPtr prepare_start_lsn, bool fromdisk, bool setParent, bool setNextXid);
static void
MarkAsPreparingGuts(GlobalTransaction gxact, TransactionId xid, const char *gid, TimestampTz prepared_at, Oid owner, Oid databaseid);
static void
RemoveTwoPhaseFile(TransactionId xid, bool giveWarning);
static void
RecreateTwoPhaseFile(TransactionId xid, void *content, int len);

   
                                   
   
Size
TwoPhaseShmemSize(void)
{
  Size size;

                                                                         
  size = offsetof(TwoPhaseStateData, prepXacts);
  size = add_size(size, mul_size(max_prepared_xacts, sizeof(GlobalTransaction)));
  size = MAXALIGN(size);
  size = add_size(size, mul_size(max_prepared_xacts, sizeof(GlobalTransactionData)));

  return size;
}

void
TwoPhaseShmemInit(void)
{
  bool found;

  TwoPhaseState = ShmemInitStruct("Prepared Transaction Table", TwoPhaseShmemSize(), &found);
  if (!IsUnderPostmaster)
  {
    GlobalTransaction gxacts;
    int i;

    Assert(!found);
    TwoPhaseState->freeGXacts = NULL;
    TwoPhaseState->numPrepXacts = 0;

       
                                                                        
       
    gxacts = (GlobalTransaction)((char *)TwoPhaseState + MAXALIGN(offsetof(TwoPhaseStateData, prepXacts) + sizeof(GlobalTransaction) * max_prepared_xacts));
    for (i = 0; i < max_prepared_xacts; i++)
    {
                                   
      gxacts[i].next = TwoPhaseState->freeGXacts;
      TwoPhaseState->freeGXacts = &gxacts[i];

                                                                 
      gxacts[i].pgprocno = PreparedXactProcs[i].pgprocno;

         
                                                                      
                                                                   
                                                                         
                                                                       
                                                                       
                                                                    
                                                                
                                                                        
                                                               
                    
         
      gxacts[i].dummyBackendId = MaxBackends + 1 + i;
    }
  }
  else
  {
    Assert(found);
  }
}

   
                                                                      
   
static void
AtProcExit_Twophase(int code, Datum arg)
{
                           
  AtAbort_Twophase();
}

   
                                                                       
   
void
AtAbort_Twophase(void)
{
  if (MyLockedGxact == NULL)
  {
    return;
  }

     
                                                                             
                                                                       
                                                                          
                                                                   
                                                                             
                                                                            
                                                                             
                               
     
                                                                            
                                           
     
                                                                         
                                                                          
                                                                        
                                                                             
                                                                           
                                                        
     
  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  if (!MyLockedGxact->valid)
  {
    RemoveGXact(MyLockedGxact);
  }
  else
  {
    MyLockedGxact->locking_backend = InvalidBackendId;
  }
  LWLockRelease(TwoPhaseStateLock);

  MyLockedGxact = NULL;
}

   
                                                                            
                 
   
void
PostPrepare_Twophase(void)
{
  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  MyLockedGxact->locking_backend = InvalidBackendId;
  LWLockRelease(TwoPhaseStateLock);

  MyLockedGxact = NULL;
}

   
                   
                                               
   
GlobalTransaction
MarkAsPreparing(TransactionId xid, const char *gid, TimestampTz prepared_at, Oid owner, Oid databaseid)
{
  GlobalTransaction gxact;
  int i;

  if (strlen(gid) >= GIDSIZE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("transaction identifier \"%s\" is too long", gid)));
  }

                                               
  if (max_prepared_xacts == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("prepared transactions are disabled"), errhint("Set max_prepared_transactions to a nonzero value.")));
  }

                                             
  if (!twophaseExitRegistered)
  {
    before_shmem_exit(AtProcExit_Twophase, 0);
    twophaseExitRegistered = true;
  }

  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);

                                 
  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    gxact = TwoPhaseState->prepXacts[i];
    if (strcmp(gxact->gid, gid) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("transaction identifier \"%s\" is already in use", gid)));
    }
  }

                                          
  if (TwoPhaseState->freeGXacts == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("maximum number of prepared transactions reached"), errhint("Increase max_prepared_transactions (currently %d).", max_prepared_xacts)));
  }
  gxact = TwoPhaseState->freeGXacts;
  TwoPhaseState->freeGXacts = gxact->next;

  MarkAsPreparingGuts(gxact, xid, gid, prepared_at, owner, databaseid);

  gxact->ondisk = false;

                                           
  Assert(TwoPhaseState->numPrepXacts < max_prepared_xacts);
  TwoPhaseState->prepXacts[TwoPhaseState->numPrepXacts++] = gxact;

  LWLockRelease(TwoPhaseStateLock);

  return gxact;
}

   
                       
   
                                                               
                                                                          
                                                       
   
                                                                     
   
static void
MarkAsPreparingGuts(GlobalTransaction gxact, TransactionId xid, const char *gid, TimestampTz prepared_at, Oid owner, Oid databaseid)
{
  PGPROC *proc;
  PGXACT *pgxact;
  int i;

  Assert(LWLockHeldByMeInMode(TwoPhaseStateLock, LW_EXCLUSIVE));

  Assert(gxact != NULL);
  proc = &ProcGlobal->allProcs[gxact->pgprocno];
  pgxact = &ProcGlobal->allPgXact[gxact->pgprocno];

                                   
  MemSet(proc, 0, sizeof(PGPROC));
  proc->pgprocno = gxact->pgprocno;
  SHMQueueElemInit(&(proc->links));
  proc->waitStatus = STATUS_OK;
  if (LocalTransactionIdIsValid(MyProc->lxid))
  {
                                                              
    proc->lxid = MyProc->lxid;
    proc->backendId = MyBackendId;
  }
  else
  {
    Assert(AmStartupProcess() || !IsPostmasterEnvironment);
                                                                   
    proc->lxid = xid;
    proc->backendId = InvalidBackendId;
  }
  pgxact->xid = xid;
  pgxact->xmin = InvalidTransactionId;
  pgxact->delayChkpt = false;
  pgxact->vacuumFlags = 0;
  proc->delayChkptEnd = false;
  proc->pid = 0;
  proc->databaseId = databaseid;
  proc->roleId = owner;
  proc->tempNamespaceId = InvalidOid;
  proc->isBackgroundWorker = false;
  proc->lwWaiting = false;
  proc->lwWaitMode = 0;
  proc->waitLock = NULL;
  proc->waitProcLock = NULL;
  for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
  {
    SHMQueueInit(&(proc->myProcLocks[i]));
  }
                                                                
  pgxact->overflowed = false;
  pgxact->nxids = 0;

  gxact->prepared_at = prepared_at;
  gxact->xid = xid;
  gxact->owner = owner;
  gxact->locking_backend = MyBackendId;
  gxact->valid = false;
  gxact->inredo = false;
  strcpy(gxact->gid, gid);

     
                                                                             
                                           
     
  MyLockedGxact = gxact;
}

   
                        
   
                                                                         
                                                                        
           
   
static void
GXactLoadSubxactData(GlobalTransaction gxact, int nsubxacts, TransactionId *children)
{
  PGPROC *proc = &ProcGlobal->allProcs[gxact->pgprocno];
  PGXACT *pgxact = &ProcGlobal->allPgXact[gxact->pgprocno];

                                                             
  if (nsubxacts > PGPROC_MAX_CACHED_SUBXIDS)
  {
    pgxact->overflowed = true;
    nsubxacts = PGPROC_MAX_CACHED_SUBXIDS;
  }
  if (nsubxacts > 0)
  {
    memcpy(proc->subxids.xids, children, nsubxacts * sizeof(TransactionId));
    pgxact->nxids = nsubxacts;
  }
}

   
                  
                                                                           
   
                                                                       
   
static void
MarkAsPrepared(GlobalTransaction gxact, bool lock_held)
{
                                                                    
  if (!lock_held)
  {
    LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  }
  Assert(!gxact->valid);
  gxact->valid = true;
  if (!lock_held)
  {
    LWLockRelease(TwoPhaseStateLock);
  }

     
                                                                             
                               
     
  ProcArrayAdd(&ProcGlobal->allProcs[gxact->pgprocno]);
}

   
             
                                                                            
   
static GlobalTransaction
LockGXact(const char *gid, Oid user)
{
  int i;

                                             
  if (!twophaseExitRegistered)
  {
    before_shmem_exit(AtProcExit_Twophase, 0);
    twophaseExitRegistered = true;
  }

  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);

  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];
    PGPROC *proc = &ProcGlobal->allProcs[gxact->pgprocno];

                                   
    if (!gxact->valid)
    {
      continue;
    }
    if (strcmp(gxact->gid, gid) != 0)
    {
      continue;
    }

                                                       
    if (gxact->locking_backend != InvalidBackendId)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("prepared transaction with identifier \"%s\" is busy", gid)));
    }

    if (user != gxact->owner && !superuser_arg(user))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to finish prepared transaction"), errhint("Must be superuser or the user that prepared the transaction.")));
    }

       
                                                                    
                                                                           
                                                                     
                                               
       
    if (MyDatabaseId != proc->databaseId)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("prepared transaction belongs to another database"), errhint("Connect to the database where the transaction was prepared to finish it.")));
    }

                              
    gxact->locking_backend = MyBackendId;
    MyLockedGxact = gxact;

    LWLockRelease(TwoPhaseStateLock);

    return gxact;
  }

  LWLockRelease(TwoPhaseStateLock);

  ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("prepared transaction with identifier \"%s\" does not exist", gid)));

                  
  return NULL;
}

   
               
                                                                  
   
                                                            
   
static void
RemoveGXact(GlobalTransaction gxact)
{
  int i;

  Assert(LWLockHeldByMeInMode(TwoPhaseStateLock, LW_EXCLUSIVE));

  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    if (gxact == TwoPhaseState->prepXacts[i])
    {
                                        
      TwoPhaseState->numPrepXacts--;
      TwoPhaseState->prepXacts[i] = TwoPhaseState->prepXacts[TwoPhaseState->numPrepXacts];

                                           
      gxact->next = TwoPhaseState->freeGXacts;
      TwoPhaseState->freeGXacts = gxact;

      return;
    }
  }

  elog(ERROR, "failed to find %p in GlobalTransaction array", gxact);
}

   
                                                                    
                              
   
                                                                       
                                                                           
   
                                                                            
                                                                    
   
                                   
   
static int
GetPreparedTransactionList(GlobalTransaction *gxacts)
{
  GlobalTransaction array;
  int num;
  int i;

  LWLockAcquire(TwoPhaseStateLock, LW_SHARED);

  if (TwoPhaseState->numPrepXacts == 0)
  {
    LWLockRelease(TwoPhaseStateLock);

    *gxacts = NULL;
    return 0;
  }

  num = TwoPhaseState->numPrepXacts;
  array = (GlobalTransaction)palloc(sizeof(GlobalTransactionData) * num);
  *gxacts = array;
  for (i = 0; i < num; i++)
  {
    memcpy(array + i, TwoPhaseState->prepXacts[i], sizeof(GlobalTransactionData));
  }

  LWLockRelease(TwoPhaseStateLock);

  return num;
}

                                         
typedef struct
{
  GlobalTransaction array;
  int ngxacts;
  int currIdx;
} Working_State;

   
                    
                                                          
   
                                                        
                                            
   
Datum
pg_prepared_xact(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  Working_State *status;

  if (SRF_IS_FIRSTCALL())
  {
    TupleDesc tupdesc;
    MemoryContext oldcontext;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                         
                                                                          
    tupdesc = CreateTemplateTupleDesc(5);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "transaction", XIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "gid", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "prepared", TIMESTAMPTZOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)4, "ownerid", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)5, "dbid", OIDOID, -1, 0);

    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

       
                                                                           
                            
       
    status = (Working_State *)palloc(sizeof(Working_State));
    funcctx->user_fctx = (void *)status;

    status->ngxacts = GetPreparedTransactionList(&status->array);
    status->currIdx = 0;

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  status = (Working_State *)funcctx->user_fctx;

  while (status->array != NULL && status->currIdx < status->ngxacts)
  {
    GlobalTransaction gxact = &status->array[status->currIdx++];
    PGPROC *proc = &ProcGlobal->allProcs[gxact->pgprocno];
    PGXACT *pgxact = &ProcGlobal->allPgXact[gxact->pgprocno];
    Datum values[5];
    bool nulls[5];
    HeapTuple tuple;
    Datum result;

    if (!gxact->valid)
    {
      continue;
    }

       
                                         
       
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, 0, sizeof(nulls));

    values[0] = TransactionIdGetDatum(pgxact->xid);
    values[1] = CStringGetTextDatum(gxact->gid);
    values[2] = TimestampTzGetDatum(gxact->prepared_at);
    values[3] = ObjectIdGetDatum(gxact->owner);
    values[4] = ObjectIdGetDatum(proc->databaseId);

    tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    result = HeapTupleGetDatum(tuple);
    SRF_RETURN_NEXT(funcctx, result);
  }

  SRF_RETURN_DONE(funcctx);
}

   
                    
                                                                
                     
   
                                                                            
                              
   
static GlobalTransaction
TwoPhaseGetGXact(TransactionId xid, bool lock_held)
{
  GlobalTransaction result = NULL;
  int i;

  static TransactionId cached_xid = InvalidTransactionId;
  static GlobalTransaction cached_gxact = NULL;

  Assert(!lock_held || LWLockHeldByMe(TwoPhaseStateLock));

     
                                                                            
                                                                         
     
  if (xid == cached_xid)
  {
    return cached_gxact;
  }

  if (!lock_held)
  {
    LWLockAcquire(TwoPhaseStateLock, LW_SHARED);
  }

  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];
    PGXACT *pgxact = &ProcGlobal->allPgXact[gxact->pgprocno];

    if (pgxact->xid == xid)
    {
      result = gxact;
      break;
    }
  }

  if (!lock_held)
  {
    LWLockRelease(TwoPhaseStateLock);
  }

  if (result == NULL)                        
  {
    elog(ERROR, "failed to find GlobalTransaction for xid %u", xid);
  }

  cached_xid = xid;
  cached_gxact = result;

  return result;
}

   
                              
                                                         
   
                                                                            
                                                                       
                                                                            
   
TransactionId
TwoPhaseGetXidByVirtualXID(VirtualTransactionId vxid, bool *have_more)
{
  int i;
  TransactionId result = InvalidTransactionId;

  Assert(VirtualTransactionIdIsValid(vxid));
  LWLockAcquire(TwoPhaseStateLock, LW_SHARED);

  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];
    PGPROC *proc;
    VirtualTransactionId proc_vxid;

    if (!gxact->valid)
    {
      continue;
    }
    proc = &ProcGlobal->allProcs[gxact->pgprocno];
    GET_VXID_FROM_PGPROC(proc_vxid, *proc);
    if (VirtualTransactionIdEquals(vxid, proc_vxid))
    {
                                                                     
      Assert(!gxact->inredo);

      if (result != InvalidTransactionId)
      {
        *have_more = true;
        break;
      }
      result = gxact->xid;
    }
  }

  LWLockRelease(TwoPhaseStateLock);

  return result;
}

   
                             
                                                                       
   
                                                                       
                                                                             
                                                                          
                                                                          
   
BackendId
TwoPhaseGetDummyBackendId(TransactionId xid, bool lock_held)
{
  GlobalTransaction gxact = TwoPhaseGetGXact(xid, lock_held);

  return gxact->dummyBackendId;
}

   
                        
                                                                           
   
                                                                            
                              
   
PGPROC *
TwoPhaseGetDummyProc(TransactionId xid, bool lock_held)
{
  GlobalTransaction gxact = TwoPhaseGetGXact(xid, lock_held);

  return &ProcGlobal->allProcs[gxact->pgprocno];
}

                                                                          
                                    
                                                                          

#define TwoPhaseFilePath(path, xid) snprintf(path, MAXPGPATH, TWOPHASE_DIR "/%08X", xid)

   
                          
   
                         
                                        
                                                    
                                                   
                                                                        
                           
          
                                                                      
                         
   
                                                         
   

   
                               
   
#define TWOPHASE_MAGIC 0x57F94534                        

typedef struct TwoPhaseFileHeader
{
  uint32 magic;                                        
  uint32 total_len;                                     
  TransactionId xid;                                          
  Oid database;                                                
  TimestampTz prepared_at;                               
  Oid owner;                                                      
  int32 nsubxacts;                                                    
  int32 ncommitrels;                                                 
  int32 nabortrels;                                                 
  int32 ninvalmsgs;                                                        
  bool initfileinval;                                                           
  uint16 gidlen;                                                                
  XLogRecPtr origin_lsn;                                               
  TimestampTz origin_timestamp;                                     
} TwoPhaseFileHeader;

   
                                          
   
                                                                             
                                                                 
   
typedef struct TwoPhaseRecordOnDisk
{
  uint32 len;                                   
  TwoPhaseRmgrId rmid;                                       
  uint16 info;                                        
} TwoPhaseRecordOnDisk;

   
                                                                           
                                                                              
             
   
typedef struct StateFileChunk
{
  char *data;
  uint32 len;
  struct StateFileChunk *next;
} StateFileChunk;

static struct xllist
{
  StateFileChunk *head;                                    
  StateFileChunk *tail;                          
  uint32 num_chunks;
  uint32 bytes_free;                                    
  uint32 total_len;                                 
} records;

   
                                                     
   
                                                                  
                                              
   
                                                                      
   
static void
save_state_data(const void *data, uint32 len)
{
  uint32 padlen = MAXALIGN(len);

  if (padlen > records.bytes_free)
  {
    records.tail->next = palloc0(sizeof(StateFileChunk));
    records.tail = records.tail->next;
    records.tail->len = 0;
    records.tail->next = NULL;
    records.num_chunks++;

    records.bytes_free = Max(padlen, 512);
    records.tail->data = palloc(records.bytes_free);
  }

  memcpy(((char *)records.tail->data) + records.tail->len, data, len);
  records.tail->len += padlen;
  records.bytes_free -= padlen;
  records.total_len += padlen;
}

   
                                 
   
                                                                      
   
void
StartPrepare(GlobalTransaction gxact)
{
  PGPROC *proc = &ProcGlobal->allProcs[gxact->pgprocno];
  PGXACT *pgxact = &ProcGlobal->allPgXact[gxact->pgprocno];
  TransactionId xid = pgxact->xid;
  TwoPhaseFileHeader hdr;
  TransactionId *children;
  RelFileNode *commitrels;
  RelFileNode *abortrels;
  SharedInvalidationMessage *invalmsgs;

                              
  records.head = palloc0(sizeof(StateFileChunk));
  records.head->len = 0;
  records.head->next = NULL;

  records.bytes_free = Max(sizeof(TwoPhaseFileHeader), 512);
  records.head->data = palloc(records.bytes_free);

  records.tail = records.head;
  records.num_chunks = 1;

  records.total_len = 0;

                     
  hdr.magic = TWOPHASE_MAGIC;
  hdr.total_len = 0;                                   
  hdr.xid = xid;
  hdr.database = proc->databaseId;
  hdr.prepared_at = gxact->prepared_at;
  hdr.owner = gxact->owner;
  hdr.nsubxacts = xactGetCommittedChildren(&children);
  hdr.ncommitrels = smgrGetPendingDeletes(true, &commitrels);
  hdr.nabortrels = smgrGetPendingDeletes(false, &abortrels);
  hdr.ninvalmsgs = xactGetCommittedInvalidationMessages(&invalmsgs, &hdr.initfileinval);
  hdr.gidlen = strlen(gxact->gid) + 1;                   

  save_state_data(&hdr, sizeof(TwoPhaseFileHeader));
  save_state_data(gxact->gid, hdr.gidlen);

     
                                                                       
                            
     
  if (hdr.nsubxacts > 0)
  {
    save_state_data(children, hdr.nsubxacts * sizeof(TransactionId));
                                                                      
    GXactLoadSubxactData(gxact, hdr.nsubxacts, children);
  }
  if (hdr.ncommitrels > 0)
  {
    save_state_data(commitrels, hdr.ncommitrels * sizeof(RelFileNode));
    pfree(commitrels);
  }
  if (hdr.nabortrels > 0)
  {
    save_state_data(abortrels, hdr.nabortrels * sizeof(RelFileNode));
    pfree(abortrels);
  }
  if (hdr.ninvalmsgs > 0)
  {
    save_state_data(invalmsgs, hdr.ninvalmsgs * sizeof(SharedInvalidationMessage));
    pfree(invalmsgs);
  }
}

   
                                                      
   
void
EndPrepare(GlobalTransaction gxact)
{
  TwoPhaseFileHeader *hdr;
  StateFileChunk *record;
  bool replorigin;

                                                       
  RegisterTwoPhaseRecord(TWOPHASE_RM_END_ID, 0, NULL, 0);

                                                               
  hdr = (TwoPhaseFileHeader *)records.head->data;
  Assert(hdr->magic == TWOPHASE_MAGIC);
  hdr->total_len = records.total_len + sizeof(pg_crc32c);

  replorigin = (replorigin_session_origin != InvalidRepOriginId && replorigin_session_origin != DoNotReplicateId);

  if (replorigin)
  {
    hdr->origin_lsn = replorigin_session_origin_lsn;
    hdr->origin_timestamp = replorigin_session_origin_timestamp;
  }
  else
  {
    hdr->origin_lsn = InvalidXLogRecPtr;
    hdr->origin_timestamp = 0;
  }

     
                                                                           
                                                                        
                                                                  
     
  if (hdr->total_len > MaxAllocSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("two-phase state file maximum length exceeded")));
  }

     
                                                                        
                                                       
     
                                                                          
                                                                         
                                                                           
                                                                             
                                            
     
                                                                         
                         
     
  XLogEnsureRecordSpace(0, records.num_chunks);

  START_CRIT_SECTION();

  Assert(!MyPgXact->delayChkpt);
  MyPgXact->delayChkpt = true;

  XLogBeginInsert();
  for (record = records.head; record != NULL; record = record->next)
  {
    XLogRegisterData(record->data, record->len);
  }

  XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

  gxact->prepare_end_lsn = XLogInsert(RM_XACT_ID, XLOG_XACT_PREPARE);

  if (replorigin)
  {
                                                       
    replorigin_session_advance(replorigin_session_origin_lsn, gxact->prepare_end_lsn);
  }

  XLogFlush(gxact->prepare_end_lsn);

                                                                     

                                                                  
  gxact->prepare_start_lsn = ProcLastRecPtr;

     
                                                                      
                                                                         
                                                                  
     
                                                                          
                                                                            
                                                                      
                                                                          
                                                                            
                                      
     
  MarkAsPrepared(gxact, false);

     
                                                                        
                                                                      
                             
     
  MyPgXact->delayChkpt = false;

     
                                                                           
                                                                           
                                                                          
     
  MyLockedGxact = gxact;

  END_CRIT_SECTION();

     
                                                    
     
                                                                           
                                                                   
     
  SyncRepWaitForLSN(gxact->prepare_end_lsn, false);

  records.tail = records.head = NULL;
  records.num_chunks = 0;
}

   
                                                      
   
void
RegisterTwoPhaseRecord(TwoPhaseRmgrId rmid, uint16 info, const void *data, uint32 len)
{
  TwoPhaseRecordOnDisk record;

  record.rmid = rmid;
  record.info = info;
  record.len = len;
  save_state_data(&record, sizeof(TwoPhaseRecordOnDisk));
  if (len > 0)
  {
    save_state_data(data, len);
  }
}

   
                                             
   
                                                                          
                                                                           
                                                                        
                                                                              
   
static char *
ReadTwoPhaseFile(TransactionId xid, bool missing_ok)
{
  char path[MAXPGPATH];
  char *buf;
  TwoPhaseFileHeader *hdr;
  int fd;
  struct stat stat;
  uint32 crc_offset;
  pg_crc32c calc_crc, file_crc;
  int r;

  TwoPhaseFilePath(path, xid);

  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
    if (missing_ok && errno == ENOENT)
    {
      return NULL;
    }

    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

     
                                                                          
                                                                            
                                                                         
                           
     
  if (fstat(fd, &stat))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", path)));
  }

  if (stat.st_size < (MAXALIGN(sizeof(TwoPhaseFileHeader)) + MAXALIGN(sizeof(TwoPhaseRecordOnDisk)) + sizeof(pg_crc32c)) || stat.st_size > MaxAllocSize)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_plural("incorrect size of file \"%s\": %zu byte", "incorrect size of file \"%s\": %zu bytes", (Size)stat.st_size, path, (Size)stat.st_size)));
  }

  crc_offset = stat.st_size - sizeof(pg_crc32c);
  if (crc_offset != MAXALIGN(crc_offset))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("incorrect alignment of CRC offset for file \"%s\"", path)));
  }

     
                            
     
  buf = (char *)palloc(stat.st_size);

  pgstat_report_wait_start(WAIT_EVENT_TWOPHASE_FILE_READ);
  r = read(fd, buf, stat.st_size);
  if (r != stat.st_size)
  {
    if (r < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(ERROR, (errmsg("could not read file \"%s\": read %d of %zu", path, r, (Size)stat.st_size)));
    }
  }

  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }

  hdr = (TwoPhaseFileHeader *)buf;
  if (hdr->magic != TWOPHASE_MAGIC)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("invalid magic number stored in file \"%s\"", path)));
  }

  if (hdr->total_len != stat.st_size)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("invalid size stored in file \"%s\"", path)));
  }

  INIT_CRC32C(calc_crc);
  COMP_CRC32C(calc_crc, buf, crc_offset);
  FIN_CRC32C(calc_crc);

  file_crc = *((pg_crc32c *)(buf + crc_offset));

  if (!EQ_CRC32C(calc_crc, file_crc))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("calculated CRC checksum does not match value stored in file \"%s\"", path)));
  }

  return buf;
}

   
                      
   
void
ParsePrepareRecord(uint8 info, char *xlrec, xl_xact_parsed_prepare *parsed)
{
  TwoPhaseFileHeader *hdr;
  char *bufptr;

  hdr = (TwoPhaseFileHeader *)xlrec;
  bufptr = xlrec + MAXALIGN(sizeof(TwoPhaseFileHeader));

  parsed->origin_lsn = hdr->origin_lsn;
  parsed->origin_timestamp = hdr->origin_timestamp;
  parsed->twophase_xid = hdr->xid;
  parsed->dbId = hdr->database;
  parsed->nsubxacts = hdr->nsubxacts;
  parsed->nrels = hdr->ncommitrels;
  parsed->nabortrels = hdr->nabortrels;
  parsed->nmsgs = hdr->ninvalmsgs;

  strncpy(parsed->twophase_gid, bufptr, hdr->gidlen);
  bufptr += MAXALIGN(hdr->gidlen);

  parsed->subxacts = (TransactionId *)bufptr;
  bufptr += MAXALIGN(hdr->nsubxacts * sizeof(TransactionId));

  parsed->xnodes = (RelFileNode *)bufptr;
  bufptr += MAXALIGN(hdr->ncommitrels * sizeof(RelFileNode));

  parsed->abortnodes = (RelFileNode *)bufptr;
  bufptr += MAXALIGN(hdr->nabortrels * sizeof(RelFileNode));

  parsed->msgs = (SharedInvalidationMessage *)bufptr;
  bufptr += MAXALIGN(hdr->ninvalmsgs * sizeof(SharedInvalidationMessage));
}

   
                                                                          
                                                               
   
                                                                           
                                                                       
                                                                    
                                                                             
                                                                         
                             
   
static void
XlogReadTwoPhaseData(XLogRecPtr lsn, char **buf, int *len)
{
  XLogRecord *record;
  XLogReaderState *xlogreader;
  char *errormsg;
  TimeLineID save_currtli = ThisTimeLineID;

  xlogreader = XLogReaderAllocate(wal_segment_size, &read_local_xlog_page, NULL);
  if (!xlogreader)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed while allocating a WAL reading processor.")));
  }

  record = XLogReadRecord(xlogreader, lsn, &errormsg);

     
                                                                  
                                                                         
                                                                            
     
  ThisTimeLineID = save_currtli;

  if (record == NULL)
  {
    if (errormsg)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read two-phase state from WAL at %X/%X: %s", (uint32)(lsn >> 32), (uint32)lsn, errormsg)));
    }
    else
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read two-phase state from WAL at %X/%X", (uint32)(lsn >> 32), (uint32)lsn)));
    }
  }

  if (XLogRecGetRmid(xlogreader) != RM_XACT_ID || (XLogRecGetInfo(xlogreader) & XLOG_XACT_OPMASK) != XLOG_XACT_PREPARE)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("expected two-phase state data is not present in WAL at %X/%X", (uint32)(lsn >> 32), (uint32)lsn)));
  }

  if (len != NULL)
  {
    *len = XLogRecGetDataLen(xlogreader);
  }

  *buf = palloc(sizeof(char) * XLogRecGetDataLen(xlogreader));
  memcpy(*buf, XLogRecGetData(xlogreader), sizeof(char) * XLogRecGetDataLen(xlogreader));

  XLogReaderFree(xlogreader);
}

   
                                                
   
bool
StandbyTransactionIdIsPrepared(TransactionId xid)
{
  char *buf;
  TwoPhaseFileHeader *hdr;
  bool result;

  Assert(TransactionIdIsValid(xid));

  if (max_prepared_xacts <= 0)
  {
    return false;                    
  }

                              
  buf = ReadTwoPhaseFile(xid, true);
  if (buf == NULL)
  {
    return false;
  }

                         
  hdr = (TwoPhaseFileHeader *)buf;
  result = TransactionIdEquals(hdr->xid, xid);
  pfree(buf);

  return result;
}

   
                                                                           
   
void
FinishPreparedTransaction(const char *gid, bool isCommit)
{
  GlobalTransaction gxact;
  PGPROC *proc;
  PGXACT *pgxact;
  TransactionId xid;
  char *buf;
  char *bufptr;
  TwoPhaseFileHeader *hdr;
  TransactionId latestXid;
  TransactionId *children;
  RelFileNode *commitrels;
  RelFileNode *abortrels;
  RelFileNode *delrels;
  int ndelrels;
  SharedInvalidationMessage *invalmsgs;

     
                                                                             
                                         
     
  gxact = LockGXact(gid, GetUserId());
  proc = &ProcGlobal->allProcs[gxact->pgprocno];
  pgxact = &ProcGlobal->allPgXact[gxact->pgprocno];
  xid = pgxact->xid;

     
                                                                           
                                                                           
                                                                 
     
  if (gxact->ondisk)
  {
    buf = ReadTwoPhaseFile(xid, false);
  }
  else
  {
    XlogReadTwoPhaseData(gxact->prepare_start_lsn, &buf, NULL);
  }

     
                                 
     
  hdr = (TwoPhaseFileHeader *)buf;
  Assert(TransactionIdEquals(hdr->xid, xid));
  bufptr = buf + MAXALIGN(sizeof(TwoPhaseFileHeader));
  bufptr += MAXALIGN(hdr->gidlen);
  children = (TransactionId *)bufptr;
  bufptr += MAXALIGN(hdr->nsubxacts * sizeof(TransactionId));
  commitrels = (RelFileNode *)bufptr;
  bufptr += MAXALIGN(hdr->ncommitrels * sizeof(RelFileNode));
  abortrels = (RelFileNode *)bufptr;
  bufptr += MAXALIGN(hdr->nabortrels * sizeof(RelFileNode));
  invalmsgs = (SharedInvalidationMessage *)bufptr;
  bufptr += MAXALIGN(hdr->ninvalmsgs * sizeof(SharedInvalidationMessage));

                                            
  latestXid = TransactionIdLatest(xid, hdr->nsubxacts, children);

                                                      
  HOLD_INTERRUPTS();

     
                                                                       
                                                                        
                                                                            
                                                                        
                                                                      
                                                            
     
  if (isCommit)
  {
    RecordTransactionCommitPrepared(xid, hdr->nsubxacts, children, hdr->ncommitrels, commitrels, hdr->ninvalmsgs, invalmsgs, hdr->initfileinval, gid);
  }
  else
  {
    RecordTransactionAbortPrepared(xid, hdr->nsubxacts, children, hdr->nabortrels, abortrels, gid);
  }

  ProcArrayRemove(proc, latestXid);

     
                                                                            
                                                                            
                                                                        
                        
     
                                                                        
     
  gxact->valid = false;

     
                                                                       
                                                                         
                                                             
     
                                                                        
     
  if (isCommit)
  {
    delrels = commitrels;
    ndelrels = hdr->ncommitrels;
  }
  else
  {
    delrels = abortrels;
    ndelrels = hdr->nabortrels;
  }

                                                          
  DropRelationFiles(delrels, ndelrels, false);

     
                                         
     
                                                                         
                                                         
     
  if (hdr->initfileinval)
  {
    RelationCacheInitFilePreInvalidate();
  }
  SendSharedInvalidMessages(invalmsgs, hdr->ninvalmsgs);
  if (hdr->initfileinval)
  {
    RelationCacheInitFilePostInvalidate();
  }

     
                                                                             
                                                                           
                                                                             
                              
     
  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);

                                
  if (isCommit)
  {
    ProcessRecords(bufptr, xid, twophase_postcommit_callbacks);
  }
  else
  {
    ProcessRecords(bufptr, xid, twophase_postabort_callbacks);
  }

  PredicateLockTwoPhaseFinish(xid, isCommit);

                                 
  RemoveGXact(gxact);

     
                                                                            
              
     
  LWLockRelease(TwoPhaseStateLock);

                                                       
  AtEOXact_PgStat(isCommit, false);

     
                                                         
     
  if (gxact->ondisk)
  {
    RemoveTwoPhaseFile(xid, true);
  }

  MyLockedGxact = NULL;

  RESUME_INTERRUPTS();

  pfree(buf);
}

   
                                                                                       
   
static void
ProcessRecords(char *bufptr, TransactionId xid, const TwoPhaseCallback callbacks[])
{
  for (;;)
  {
    TwoPhaseRecordOnDisk *record = (TwoPhaseRecordOnDisk *)bufptr;

    Assert(record->rmid <= TWOPHASE_RM_MAX_ID);
    if (record->rmid == TWOPHASE_RM_END_ID)
    {
      break;
    }

    bufptr += MAXALIGN(sizeof(TwoPhaseRecordOnDisk));

    if (callbacks[record->rmid] != NULL)
    {
      callbacks[record->rmid](xid, record->info, (void *)bufptr, record->len);
    }

    bufptr += MAXALIGN(record->len);
  }
}

   
                                              
   
                                                                    
                                               
   
static void
RemoveTwoPhaseFile(TransactionId xid, bool giveWarning)
{
  char path[MAXPGPATH];

  TwoPhaseFilePath(path, xid);
  if (unlink(path))
  {
    if (errno != ENOENT || giveWarning)
    {
      ereport(WARNING, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
    }
  }
}

   
                                                                 
                        
   
                                            
   
static void
RecreateTwoPhaseFile(TransactionId xid, void *content, int len)
{
  char path[MAXPGPATH];
  pg_crc32c statefile_crc;
  int fd;

                     
  INIT_CRC32C(statefile_crc);
  COMP_CRC32C(statefile_crc, content, len);
  FIN_CRC32C(statefile_crc);

  TwoPhaseFilePath(path, xid);

  fd = OpenTransientFile(path, O_CREAT | O_TRUNC | O_WRONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not recreate file \"%s\": %m", path)));
  }

                             
  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_TWOPHASE_FILE_WRITE);
  if (write(fd, content, len) != len)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", path)));
  }
  if (write(fd, &statefile_crc, sizeof(pg_crc32c)) != sizeof(pg_crc32c))
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", path)));
  }
  pgstat_report_wait_end();

     
                                                                             
                                                                  
     
  pgstat_report_wait_start(WAIT_EVENT_TWOPHASE_FILE_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", path)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }
}

   
                                                                
   
                                                                       
                                                                        
                                                                      
                                                                    
                 
   
                                                                            
                                                                      
                                                                           
                                                                        
                                                                       
                                                                       
   
                                                                         
                                                       
   
void
CheckPointTwoPhase(XLogRecPtr redo_horizon)
{
  int i;
  int serialized_xacts = 0;

  if (max_prepared_xacts <= 0)
  {
    return;                    
  }

  TRACE_POSTGRESQL_TWOPHASE_CHECKPOINT_START();

     
                                                                        
                                                                     
                                                                       
                                                                           
                                                                    
     
                                                                           
                                                                          
                                                                         
                                            
     
                                                                
                                                                             
                                             
     
  LWLockAcquire(TwoPhaseStateLock, LW_SHARED);
  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
       
                                                                         
            
       
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];

    if ((gxact->valid || gxact->inredo) && !gxact->ondisk && gxact->prepare_end_lsn <= redo_horizon)
    {
      char *buf;
      int len;

      XlogReadTwoPhaseData(gxact->prepare_start_lsn, &buf, &len);
      RecreateTwoPhaseFile(gxact->xid, buf, len);
      gxact->ondisk = true;
      gxact->prepare_start_lsn = InvalidXLogRecPtr;
      gxact->prepare_end_lsn = InvalidXLogRecPtr;
      pfree(buf);
      serialized_xacts++;
    }
  }
  LWLockRelease(TwoPhaseStateLock);

     
                                                                        
                                                                         
                                                                            
                                           
     
  fsync_fname(TWOPHASE_DIR, true);

  TRACE_POSTGRESQL_TWOPHASE_CHECKPOINT_DONE();

  if (log_checkpoints && serialized_xacts > 0)
  {
    ereport(LOG, (errmsg_plural("%u two-phase state file was written "
                                "for a long-running prepared transaction",
                     "%u two-phase state files were written "
                     "for long-running prepared transactions",
                     serialized_xacts, serialized_xacts)));
  }
}

   
                       
   
                                                                          
                                                                      
                                                                   
                                                 
   
void
restoreTwoPhaseData(void)
{
  DIR *cldir;
  struct dirent *clde;

  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  cldir = AllocateDir(TWOPHASE_DIR);
  while ((clde = ReadDir(cldir, TWOPHASE_DIR)) != NULL)
  {
    if (strlen(clde->d_name) == 8 && strspn(clde->d_name, "0123456789ABCDEF") == 8)
    {
      TransactionId xid;
      char *buf;

      xid = (TransactionId)strtoul(clde->d_name, NULL, 16);

      buf = ProcessTwoPhaseBuffer(xid, InvalidXLogRecPtr, true, false, false);
      if (buf == NULL)
      {
        continue;
      }

      PrepareRedoAdd(buf, InvalidXLogRecPtr, InvalidXLogRecPtr, InvalidRepOriginId);
    }
  }
  LWLockRelease(TwoPhaseStateLock);
  FreeDir(cldir);
}

   
                               
   
                                                                           
                                                                         
                                                                                
                                                                   
   
                                                                                
                                                                        
                                                                        
                                                                           
                                 
   
                                                                             
                                                                          
                                                                        
                 
   
                                                                          
                                                                        
                               
   
                                                                            
                                                                               
                                                               
   
                                                                          
                                                                           
                            
   
TransactionId
PrescanPreparedTransactions(TransactionId **xids_p, int *nxids_p)
{
  FullTransactionId nextFullXid = ShmemVariableCache->nextFullXid;
  TransactionId origNextXid = XidFromFullTransactionId(nextFullXid);
  TransactionId result = origNextXid;
  TransactionId *xids = NULL;
  int nxids = 0;
  int allocsize = 0;
  int i;

  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    TransactionId xid;
    char *buf;
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];

    Assert(gxact->inredo);

    xid = gxact->xid;

    buf = ProcessTwoPhaseBuffer(xid, gxact->prepare_start_lsn, gxact->ondisk, false, true);

    if (buf == NULL)
    {
      continue;
    }

       
                                                                  
                               
       
    if (TransactionIdPrecedes(xid, result))
    {
      result = xid;
    }

    if (xids_p)
    {
      if (nxids == allocsize)
      {
        if (nxids == 0)
        {
          allocsize = 10;
          xids = palloc(allocsize * sizeof(TransactionId));
        }
        else
        {
          allocsize = allocsize * 2;
          xids = repalloc(xids, allocsize * sizeof(TransactionId));
        }
      }
      xids[nxids++] = xid;
    }

    pfree(buf);
  }
  LWLockRelease(TwoPhaseStateLock);

  if (xids_p)
  {
    *xids_p = xids;
    *nxids_p = nxids;
  }

  return result;
}

   
                                      
   
                                                                              
                                                                                
           
   
                                                        
                                                
   
                                                                     
                                                                                
                                   
   
void
StandbyRecoverPreparedTransactions(void)
{
  int i;

  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    TransactionId xid;
    char *buf;
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];

    Assert(gxact->inredo);

    xid = gxact->xid;

    buf = ProcessTwoPhaseBuffer(xid, gxact->prepare_start_lsn, gxact->ondisk, false, false);
    if (buf != NULL)
    {
      pfree(buf);
    }
  }
  LWLockRelease(TwoPhaseStateLock);
}

   
                               
   
                                                                            
                                                     
   
                                                                             
        
   
                                                                             
                                                                             
                                                                      
                                                                            
                                                                     
             
   
void
RecoverPreparedTransactions(void)
{
  int i;

  LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    TransactionId xid;
    char *buf;
    GlobalTransaction gxact = TwoPhaseState->prepXacts[i];
    char *bufptr;
    TwoPhaseFileHeader *hdr;
    TransactionId *subxids;
    const char *gid;

    xid = gxact->xid;

       
                                                                         
                                                                      
                                                                      
                                                                    
                                                                   
                                                                          
                                         
       
    buf = ProcessTwoPhaseBuffer(xid, gxact->prepare_start_lsn, gxact->ondisk, true, false);
    if (buf == NULL)
    {
      continue;
    }

    ereport(LOG, (errmsg("recovering prepared transaction %u from shared memory", xid)));

    hdr = (TwoPhaseFileHeader *)buf;
    Assert(TransactionIdEquals(hdr->xid, xid));
    bufptr = buf + MAXALIGN(sizeof(TwoPhaseFileHeader));
    gid = (const char *)bufptr;
    bufptr += MAXALIGN(hdr->gidlen);
    subxids = (TransactionId *)bufptr;
    bufptr += MAXALIGN(hdr->nsubxacts * sizeof(TransactionId));
    bufptr += MAXALIGN(hdr->ncommitrels * sizeof(RelFileNode));
    bufptr += MAXALIGN(hdr->nabortrels * sizeof(RelFileNode));
    bufptr += MAXALIGN(hdr->ninvalmsgs * sizeof(SharedInvalidationMessage));

       
                                                                      
                                                           
       
    MarkAsPreparingGuts(gxact, xid, gid, hdr->prepared_at, hdr->owner, hdr->database);

                                                                    
    gxact->inredo = false;

    GXactLoadSubxactData(gxact, hdr->nsubxacts, subxids);
    MarkAsPrepared(gxact, true);

    LWLockRelease(TwoPhaseStateLock);

       
                                                                    
       
    ProcessRecords(bufptr, xid, twophase_recover_callbacks);

       
                                                                       
                                                                 
                                         
       
    if (InHotStandby)
    {
      StandbyReleaseLockTree(xid, hdr->nsubxacts, subxids);
    }

       
                                                                         
                                                                   
       
    PostPrepare_Twophase();

    pfree(buf);

    LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
  }

  LWLockRelease(TwoPhaseStateLock);
}

   
                         
   
                                                                        
                                                                         
   
                                                                
   
                                                                            
                  
   
static char *
ProcessTwoPhaseBuffer(TransactionId xid, XLogRecPtr prepare_start_lsn, bool fromdisk, bool setParent, bool setNextXid)
{
  FullTransactionId nextFullXid = ShmemVariableCache->nextFullXid;
  TransactionId origNextXid = XidFromFullTransactionId(nextFullXid);
  TransactionId *subxids;
  char *buf;
  TwoPhaseFileHeader *hdr;
  int i;

  Assert(LWLockHeldByMeInMode(TwoPhaseStateLock, LW_EXCLUSIVE));

  if (!fromdisk)
  {
    Assert(prepare_start_lsn != InvalidXLogRecPtr);
  }

                          
  if (TransactionIdDidCommit(xid) || TransactionIdDidAbort(xid))
  {
    if (fromdisk)
    {
      ereport(WARNING, (errmsg("removing stale two-phase state file for transaction %u", xid)));
      RemoveTwoPhaseFile(xid, true);
    }
    else
    {
      ereport(WARNING, (errmsg("removing stale two-phase state from memory for transaction %u", xid)));
      PrepareRedoRemove(xid, true);
    }
    return NULL;
  }

                             
  if (TransactionIdFollowsOrEquals(xid, origNextXid))
  {
    if (fromdisk)
    {
      ereport(WARNING, (errmsg("removing future two-phase state file for transaction %u", xid)));
      RemoveTwoPhaseFile(xid, true);
    }
    else
    {
      ereport(WARNING, (errmsg("removing future two-phase state from memory for transaction %u", xid)));
      PrepareRedoRemove(xid, true);
    }
    return NULL;
  }

  if (fromdisk)
  {
                                
    buf = ReadTwoPhaseFile(xid, false);
  }
  else
  {
                        
    XlogReadTwoPhaseData(prepare_start_lsn, &buf, NULL);
  }

                          
  hdr = (TwoPhaseFileHeader *)buf;
  if (!TransactionIdEquals(hdr->xid, xid))
  {
    if (fromdisk)
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted two-phase state file for transaction %u", xid)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted two-phase state in memory for transaction %u", xid)));
    }
  }

     
                                                                          
                                               
     
  subxids = (TransactionId *)(buf + MAXALIGN(sizeof(TwoPhaseFileHeader)) + MAXALIGN(hdr->gidlen));
  for (i = 0; i < hdr->nsubxacts; i++)
  {
    TransactionId subxid = subxids[i];

    Assert(TransactionIdFollows(subxid, xid));

                                      
    if (setNextXid)
    {
      AdvanceNextFullTransactionIdPastXid(subxid);
    }

    if (setParent)
    {
      SubTransSetParent(subxid, xid);
    }
  }

  return buf;
}

   
                                   
   
                                                                             
                                                                             
                   
   
                                                                       
                                                              
   
static void
RecordTransactionCommitPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, int ninvalmsgs, SharedInvalidationMessage *invalmsgs, bool initfileinval, const char *gid)
{
  XLogRecPtr recptr;
  TimestampTz committs = GetCurrentTimestamp();
  bool replorigin;

     
                                                                            
                                  
     
  replorigin = (replorigin_session_origin != InvalidRepOriginId && replorigin_session_origin != DoNotReplicateId);

  START_CRIT_SECTION();

                                            
  Assert(!MyPgXact->delayChkpt);
  MyPgXact->delayChkpt = true;

     
                                                                   
                                                                            
                  
     
  recptr = XactLogCommitRecord(committs, nchildren, children, nrels, rels, ninvalmsgs, invalmsgs, initfileinval, false, MyXactFlags | XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK, xid, gid);

  if (replorigin)
  {
                                                       
    replorigin_session_advance(replorigin_session_origin_lsn, XactLastRecEnd);
  }

     
                                                                           
                                                                            
                                                       
     
                                                                          
                                      
     
  if (!replorigin || replorigin_session_origin_timestamp == 0)
  {
    replorigin_session_origin_timestamp = committs;
  }

  TransactionTreeSetCommitTsData(xid, nchildren, children, replorigin_session_origin_timestamp, replorigin_session_origin, false);

     
                                                                            
                                                                            
                      
     

                          
  XLogFlush(recptr);

                                                 
  TransactionIdCommitTree(xid, nchildren, children);

                                  
  MyPgXact->delayChkpt = false;

  END_CRIT_SECTION();

     
                                                    
     
                                                                            
                                                  
     
  SyncRepWaitForLSN(recptr, true);
}

   
                                  
   
                                                         
   
                                                                       
                                                             
   
static void
RecordTransactionAbortPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, const char *gid)
{
  XLogRecPtr recptr;

     
                                                         
                                         
     
  if (TransactionIdDidCommit(xid))
  {
    elog(PANIC, "cannot abort transaction %u, it was already committed", xid);
  }

  START_CRIT_SECTION();

     
                                                                  
                                                                            
                  
     
  recptr = XactLogAbortRecord(GetCurrentTimestamp(), nchildren, children, nrels, rels, MyXactFlags | XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK, xid, gid);

                                                                    
  XLogFlush(recptr);

     
                                                                             
                                                 
     
  TransactionIdAbortTree(xid, nchildren, children);

  END_CRIT_SECTION();

     
                                                    
     
                                                                            
                                                  
     
  SyncRepWaitForLSN(recptr, false);
}

   
                  
   
                                                                           
                                                                      
                                                                      
                                                 
   
void
PrepareRedoAdd(char *buf, XLogRecPtr start_lsn, XLogRecPtr end_lsn, RepOriginId origin_id)
{
  TwoPhaseFileHeader *hdr = (TwoPhaseFileHeader *)buf;
  char *bufptr;
  const char *gid;
  GlobalTransaction gxact;

  Assert(LWLockHeldByMeInMode(TwoPhaseStateLock, LW_EXCLUSIVE));
  Assert(RecoveryInProgress());

  bufptr = buf + MAXALIGN(sizeof(TwoPhaseFileHeader));
  gid = (const char *)bufptr;

     
                                                                      
     
                                                                    
     
                                                                            
                                                                           
                                                                           
                                         
     

                                          
  if (TwoPhaseState->freeGXacts == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("maximum number of prepared transactions reached"), errhint("Increase max_prepared_transactions (currently %d).", max_prepared_xacts)));
  }
  gxact = TwoPhaseState->freeGXacts;
  TwoPhaseState->freeGXacts = gxact->next;

  gxact->prepared_at = hdr->prepared_at;
  gxact->prepare_start_lsn = start_lsn;
  gxact->prepare_end_lsn = end_lsn;
  gxact->xid = hdr->xid;
  gxact->owner = hdr->owner;
  gxact->locking_backend = InvalidBackendId;
  gxact->valid = false;
  gxact->ondisk = XLogRecPtrIsInvalid(start_lsn);
  gxact->inredo = true;                         
  strcpy(gxact->gid, gid);

                                           
  Assert(TwoPhaseState->numPrepXacts < max_prepared_xacts);
  TwoPhaseState->prepXacts[TwoPhaseState->numPrepXacts++] = gxact;

  if (origin_id != InvalidRepOriginId)
  {
                                
    replorigin_advance(origin_id, hdr->origin_lsn, end_lsn, false               , false /* WAL */);
  }

  elog(DEBUG2, "added 2PC data in shared memory for transaction %u", gxact->xid);
}

   
                     
   
                                                                        
                                                                               
   
                                                                               
               
   
void
PrepareRedoRemove(TransactionId xid, bool giveWarning)
{
  GlobalTransaction gxact = NULL;
  int i;
  bool found = false;

  Assert(LWLockHeldByMeInMode(TwoPhaseStateLock, LW_EXCLUSIVE));
  Assert(RecoveryInProgress());

  for (i = 0; i < TwoPhaseState->numPrepXacts; i++)
  {
    gxact = TwoPhaseState->prepXacts[i];

    if (gxact->xid == xid)
    {
      Assert(gxact->inredo);
      found = true;
      break;
    }
  }

     
                                                                         
     
  if (!found)
  {
    return;
  }

     
                                                         
     
  elog(DEBUG2, "removing 2PC data for transaction %u", xid);
  if (gxact->ondisk)
  {
    RemoveTwoPhaseFile(xid, giveWarning);
  }
  RemoveGXact(gxact);

  return;
}
