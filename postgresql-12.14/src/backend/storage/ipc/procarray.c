                                                                            
   
               
                                  
   
   
                                                                            
                                                                             
                                                                               
   
                                                                           
                                                                             
                                                   
   
                                                                        
                                                                            
                                                                              
                                     
   
                                                                             
                                                                               
                                                                         
                                                                       
                                                                           
                                                                            
                                                                             
                                                                           
                                
   
                                                                           
                                                                             
                                                                         
                                                                             
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <signal.h>

#include "access/clog.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

#define UINT32_ACCESS_ONCE(var) ((uint32)(*((volatile uint32 *)&(var))))

                            
typedef struct ProcArrayStruct
{
  int numProcs;                                    
  int maxProcs;                                    

     
                                  
     
  int maxKnownAssignedXids;                                     
  int numKnownAssignedXids;                                        
  int tailKnownAssignedXids;                                          
  int headKnownAssignedXids;                                         
  slock_t known_assigned_xids_lck;                                  

     
                                                                          
                                                                           
                                                                      
                                                                            
                      
     
  TransactionId lastOverflowedXid;

                                           
  TransactionId replication_slot_xmin;
                                                   
  TransactionId replication_slot_catalog_xmin;

                                                                
  int pgprocnos[FLEXIBLE_ARRAY_MEMBER];
} ProcArrayStruct;

static ProcArrayStruct *procArray;

static PGPROC *allProcs;
static PGXACT *allPgXact;

   
                                                 
   
typedef enum KAXCompressReason
{
  KAX_NO_SPACE,                                                    
  KAX_PRUNE,                                               
  KAX_TRANSACTION_END,                                              
  KAX_STARTUP_PROCESS_IDLE                                        
} KAXCompressReason;

   
                                                                             
   
static TransactionId cachedXidIsNotInProgress = InvalidTransactionId;

   
                                                              
   
static TransactionId *KnownAssignedXids;
static bool *KnownAssignedXidsValid;
static TransactionId latestObservedXid = InvalidTransactionId;

   
                                                                             
                                                                     
                      
   
static TransactionId standbySnapshotPendingXmin;

#ifdef XIDCACHE_DEBUG

                                       
static long xc_by_recent_xmin = 0;
static long xc_by_known_xact = 0;
static long xc_by_my_xact = 0;
static long xc_by_latest_xid = 0;
static long xc_by_main_xid = 0;
static long xc_by_child_xid = 0;
static long xc_by_known_assigned = 0;
static long xc_no_overflow = 0;
static long xc_slow_answer = 0;

#define xc_by_recent_xmin_inc() (xc_by_recent_xmin++)
#define xc_by_known_xact_inc() (xc_by_known_xact++)
#define xc_by_my_xact_inc() (xc_by_my_xact++)
#define xc_by_latest_xid_inc() (xc_by_latest_xid++)
#define xc_by_main_xid_inc() (xc_by_main_xid++)
#define xc_by_child_xid_inc() (xc_by_child_xid++)
#define xc_by_known_assigned_inc() (xc_by_known_assigned++)
#define xc_no_overflow_inc() (xc_no_overflow++)
#define xc_slow_answer_inc() (xc_slow_answer++)

static void
DisplayXidCache(void);
#else                      

#define xc_by_recent_xmin_inc() ((void)0)
#define xc_by_known_xact_inc() ((void)0)
#define xc_by_my_xact_inc() ((void)0)
#define xc_by_latest_xid_inc() ((void)0)
#define xc_by_main_xid_inc() ((void)0)
#define xc_by_child_xid_inc() ((void)0)
#define xc_by_known_assigned_inc() ((void)0)
#define xc_no_overflow_inc() ((void)0)
#define xc_slow_answer_inc() ((void)0)
#endif                     

static VirtualTransactionId *
GetVirtualXIDsDelayingChkptGuts(int *nvxids, int type);
static bool
HaveVirtualXIDsDelayingChkptGuts(VirtualTransactionId *vxids, int nvxids, int type);

                                                                 
static void
KnownAssignedXidsCompress(KAXCompressReason reason, bool haveLock);
static void
KnownAssignedXidsAdd(TransactionId from_xid, TransactionId to_xid, bool exclusive_lock);
static bool
KnownAssignedXidsSearch(TransactionId xid, bool remove);
static bool
KnownAssignedXidExists(TransactionId xid);
static void
KnownAssignedXidsRemove(TransactionId xid);
static void
KnownAssignedXidsRemoveTree(TransactionId xid, int nsubxids, TransactionId *subxids);
static void
KnownAssignedXidsRemovePreceding(TransactionId xid);
static int
KnownAssignedXidsGet(TransactionId *xarray, TransactionId xmax);
static int
KnownAssignedXidsGetAndSetXmin(TransactionId *xarray, TransactionId *xmin, TransactionId xmax);
static TransactionId
KnownAssignedXidsGetOldestXmin(void);
static void
KnownAssignedXidsDisplay(int trace_level);
static void
KnownAssignedXidsReset(void);
static inline void
ProcArrayEndTransactionInternal(PGPROC *proc, PGXACT *pgxact, TransactionId latestXid);
static void
ProcArrayGroupClearXid(PGPROC *proc, TransactionId latestXid);

   
                                                               
   
Size
ProcArrayShmemSize(void)
{
  Size size;

                                              
#define PROCARRAY_MAXPROCS (MaxBackends + max_prepared_xacts)

  size = offsetof(ProcArrayStruct, pgprocnos);
  size = add_size(size, mul_size(sizeof(int), PROCARRAY_MAXPROCS));

     
                                                                   
                                                                            
                                                                
                                                                             
                                                                           
                                                                            
                                                     
     
                                                                           
                                                                        
                                    
     
#define TOTAL_MAX_CACHED_SUBXIDS ((PGPROC_MAX_CACHED_SUBXIDS + 1) * PROCARRAY_MAXPROCS)

  if (EnableHotStandby)
  {
    size = add_size(size, mul_size(sizeof(TransactionId), TOTAL_MAX_CACHED_SUBXIDS));
    size = add_size(size, mul_size(sizeof(bool), TOTAL_MAX_CACHED_SUBXIDS));
  }

  return size;
}

   
                                                                 
   
void
CreateSharedProcArray(void)
{
  bool found;

                                                          
  procArray = (ProcArrayStruct *)ShmemInitStruct("Proc Array", add_size(offsetof(ProcArrayStruct, pgprocnos), mul_size(sizeof(int), PROCARRAY_MAXPROCS)), &found);

  if (!found)
  {
       
                                     
       
    procArray->numProcs = 0;
    procArray->maxProcs = PROCARRAY_MAXPROCS;
    procArray->maxKnownAssignedXids = TOTAL_MAX_CACHED_SUBXIDS;
    procArray->numKnownAssignedXids = 0;
    procArray->tailKnownAssignedXids = 0;
    procArray->headKnownAssignedXids = 0;
    SpinLockInit(&procArray->known_assigned_xids_lck);
    procArray->lastOverflowedXid = InvalidTransactionId;
    procArray->replication_slot_xmin = InvalidTransactionId;
    procArray->replication_slot_catalog_xmin = InvalidTransactionId;
  }

  allProcs = ProcGlobal->allProcs;
  allPgXact = ProcGlobal->allPgXact;

                                                                       
  if (EnableHotStandby)
  {
    KnownAssignedXids = (TransactionId *)ShmemInitStruct("KnownAssignedXids", mul_size(sizeof(TransactionId), TOTAL_MAX_CACHED_SUBXIDS), &found);
    KnownAssignedXidsValid = (bool *)ShmemInitStruct("KnownAssignedXidsValid", mul_size(sizeof(bool), TOTAL_MAX_CACHED_SUBXIDS), &found);
  }

                                                           
  LWLockRegisterTranche(LWTRANCHE_PROC, "proc");
}

   
                                                 
   
void
ProcArrayAdd(PGPROC *proc)
{
  ProcArrayStruct *arrayP = procArray;
  int index;

  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  if (arrayP->numProcs >= arrayP->maxProcs)
  {
       
                                                                       
                                                                        
                 
       
    LWLockRelease(ProcArrayLock);
    ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("sorry, too many clients already")));
  }

     
                                                                      
                                                                             
                                                                            
                                    
     
                                                                           
                                                                     
     
  for (index = 0; index < arrayP->numProcs; index++)
  {
       
                                                                         
                           
       
    if ((arrayP->pgprocnos[index] == -1) || (arrayP->pgprocnos[index] > proc->pgprocno))
    {
      break;
    }
  }

  memmove(&arrayP->pgprocnos[index + 1], &arrayP->pgprocnos[index], (arrayP->numProcs - index) * sizeof(int));
  arrayP->pgprocnos[index] = proc->pgprocno;
  arrayP->numProcs++;

  LWLockRelease(ProcArrayLock);
}

   
                                                      
   
                                                                            
                                                                           
                                                                           
                                                                             
                                                                            
                                      
   
void
ProcArrayRemove(PGPROC *proc, TransactionId latestXid)
{
  ProcArrayStruct *arrayP = procArray;
  int index;

#ifdef XIDCACHE_DEBUG
                                                                 
  if (proc->pid != 0)
  {
    DisplayXidCache();
  }
#endif

  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  if (TransactionIdIsValid(latestXid))
  {
    Assert(TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

                                                                  
    if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, latestXid))
    {
      ShmemVariableCache->latestCompletedXid = latestXid;
    }
  }
  else
  {
                                                               
    Assert(!TransactionIdIsValid(allPgXact[proc->pgprocno].xid));
  }

  for (index = 0; index < arrayP->numProcs; index++)
  {
    if (arrayP->pgprocnos[index] == proc->pgprocno)
    {
                                                         
      memmove(&arrayP->pgprocnos[index], &arrayP->pgprocnos[index + 1], (arrayP->numProcs - index - 1) * sizeof(int));
      arrayP->pgprocnos[arrayP->numProcs - 1] = -1;                    
      arrayP->numProcs--;
      LWLockRelease(ProcArrayLock);
      return;
    }
  }

            
  LWLockRelease(ProcArrayLock);

  elog(LOG, "failed to find proc %p in ProcArray", proc);
}

   
                                                                      
   
                                                                             
                                                             
   
                                                                               
                                                                    
                                                                            
                                                                           
                                                                   
                
   
void
ProcArrayEndTransaction(PGPROC *proc, TransactionId latestXid)
{
  PGXACT *pgxact = &allPgXact[proc->pgprocno];

  if (TransactionIdIsValid(latestXid))
  {
       
                                                                        
                                                                           
                                                     
                                          
       
    Assert(TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

       
                                                                         
                                                                        
                   
       
    if (LWLockConditionalAcquire(ProcArrayLock, LW_EXCLUSIVE))
    {
      ProcArrayEndTransactionInternal(proc, pgxact, latestXid);
      LWLockRelease(ProcArrayLock);
    }
    else
    {
      ProcArrayGroupClearXid(proc, latestXid);
    }
  }
  else
  {
       
                                                                       
                                                                       
                                               
       
    Assert(!TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

    proc->lxid = InvalidLocalTransactionId;
    pgxact->xmin = InvalidTransactionId;
                                        
    pgxact->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;

                                            
    pgxact->delayChkpt = false;
    proc->delayChkptEnd = false;

    proc->recoveryConflictPending = false;

    Assert(pgxact->nxids == 0);
    Assert(pgxact->overflowed == false);
  }
}

   
                                                  
   
                                                          
   
static inline void
ProcArrayEndTransactionInternal(PGPROC *proc, PGXACT *pgxact, TransactionId latestXid)
{
  pgxact->xid = InvalidTransactionId;
  proc->lxid = InvalidLocalTransactionId;
  pgxact->xmin = InvalidTransactionId;
                                      
  pgxact->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;

                                          
  pgxact->delayChkpt = false;
  proc->delayChkptEnd = false;

  proc->recoveryConflictPending = false;

                                                                     
  pgxact->nxids = 0;
  pgxact->overflowed = false;

                                                                     
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, latestXid))
  {
    ShmemVariableCache->latestCompletedXid = latestXid;
  }
}

   
                                                
   
                                                                         
                                                                          
                                                                      
                                                                               
                                                                           
                                                                          
                                                                        
                        
   
static void
ProcArrayGroupClearXid(PGPROC *proc, TransactionId latestXid)
{
  PROC_HDR *procglobal = ProcGlobal;
  uint32 nextidx;
  uint32 wakeidx;

                                                  
  Assert(TransactionIdIsValid(allPgXact[proc->pgprocno].xid));

                                                                         
  proc->procArrayGroupMember = true;
  proc->procArrayGroupMemberXid = latestXid;
  while (true)
  {
    nextidx = pg_atomic_read_u32(&procglobal->procArrayGroupFirst);
    pg_atomic_write_u32(&proc->procArrayGroupNext, nextidx);

    if (pg_atomic_compare_exchange_u32(&procglobal->procArrayGroupFirst, &nextidx, (uint32)proc->pgprocno))
    {
      break;
    }
  }

     
                                                                      
                                                                             
                                                                   
                       
     
  if (nextidx != INVALID_PGPROCNO)
  {
    int extraWaits = 0;

                                                
    pgstat_report_wait_start(WAIT_EVENT_PROCARRAY_GROUP_UPDATE);
    for (;;)
    {
                                  
      PGSemaphoreLock(proc->sem);
      if (!proc->procArrayGroupMember)
      {
        break;
      }
      extraWaits++;
    }
    pgstat_report_wait_end();

    Assert(pg_atomic_read_u32(&proc->procArrayGroupNext) == INVALID_PGPROCNO);

                                                      
    while (extraWaits-- > 0)
    {
      PGSemaphoreUnlock(proc->sem);
    }
    return;
  }

                                                                   
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

     
                                                                          
                                                                           
                                                                 
     
  nextidx = pg_atomic_exchange_u32(&procglobal->procArrayGroupFirst, INVALID_PGPROCNO);

                                                                            
  wakeidx = nextidx;

                                         
  while (nextidx != INVALID_PGPROCNO)
  {
    PGPROC *nextproc = &allProcs[nextidx];
    PGXACT *pgxact = &allPgXact[nextidx];

    ProcArrayEndTransactionInternal(nextproc, pgxact, nextproc->procArrayGroupMemberXid);

                                    
    nextidx = pg_atomic_read_u32(&nextproc->procArrayGroupNext);
  }

                                     
  LWLockRelease(ProcArrayLock);

     
                                                                          
                                                                     
                                                                           
                                                                            
                       
     
  while (wakeidx != INVALID_PGPROCNO)
  {
    PGPROC *nextproc = &allProcs[wakeidx];

    wakeidx = pg_atomic_read_u32(&nextproc->procArrayGroupNext);
    pg_atomic_write_u32(&nextproc->procArrayGroupNext, INVALID_PGPROCNO);

                                                                           
    pg_write_barrier();

    nextproc->procArrayGroupMember = false;

    if (nextproc != MyProc)
    {
      PGSemaphoreUnlock(nextproc->sem);
    }
  }
}

   
                                                             
   
                                                                            
                                                                            
                                                                            
                                                   
   
void
ProcArrayClearTransaction(PGPROC *proc)
{
  PGXACT *pgxact = &allPgXact[proc->pgprocno];

     
                                                                          
                                                                            
                                                                      
                
     
  pgxact->xid = InvalidTransactionId;
  proc->lxid = InvalidLocalTransactionId;
  pgxact->xmin = InvalidTransactionId;
  proc->recoveryConflictPending = false;

                                   
  pgxact->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;
  pgxact->delayChkpt = false;

                                              
  pgxact->nxids = 0;
  pgxact->overflowed = false;
}

   
                                                                     
   
                                                                              
                                                                               
                      
   
void
ProcArrayInitRecovery(TransactionId initializedUptoXID)
{
  Assert(standbyState == STANDBY_INITIALIZED);
  Assert(TransactionIdIsNormal(initializedUptoXID));

     
                                                                          
                                                        
                                                                      
                                   
     
  latestObservedXid = initializedUptoXID;
  TransactionIdRetreat(latestObservedXid);
}

   
                                                                
   
                                                              
                                                                         
                                                         
   
                                                                           
                                                                            
                                                                           
                                                                             
             
   
                                          
   
void
ProcArrayApplyRecoveryInfo(RunningTransactions running)
{
  TransactionId *xids;
  int nxids;
  int i;

  Assert(standbyState >= STANDBY_INITIALIZED);
  Assert(TransactionIdIsValid(running->nextXid));
  Assert(TransactionIdIsValid(running->oldestRunningXid));
  Assert(TransactionIdIsNormal(running->latestCompletedXid));

     
                                        
     
  ExpireOldKnownAssignedTransactionIds(running->oldestRunningXid);

     
                                 
     
  StandbyReleaseOldLocks(running->oldestRunningXid);

     
                                                             
     
  if (standbyState == STANDBY_SNAPSHOT_READY)
  {
    return;
  }

     
                                                                            
                                                                            
                                                                             
                                                                            
                                                                        
                                                                          
                                                                             
                                                                      
                        
     
  if (standbyState == STANDBY_SNAPSHOT_PENDING)
  {
       
                                                                         
                                                    
       
    if (!running->subxid_overflow || running->xcnt == 0)
    {
         
                                                                      
                                                                
         
      KnownAssignedXidsReset();
      standbyState = STANDBY_INITIALIZED;
    }
    else
    {
      if (TransactionIdPrecedes(standbySnapshotPendingXmin, running->oldestRunningXid))
      {
        standbyState = STANDBY_SNAPSHOT_READY;
        elog(trace_recovery(DEBUG1), "recovery snapshots are now enabled");
      }
      else
      {
        elog(trace_recovery(DEBUG1),
            "recovery snapshot waiting for non-overflowed snapshot or "
            "until oldest active xid on standby is at least %u (now %u)",
            standbySnapshotPendingXmin, running->oldestRunningXid);
      }
      return;
    }
  }

  Assert(standbyState == STANDBY_INITIALIZED);

     
                                                                        
     
                                                                            
                
     

     
                                                       
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

     
                                                                            
                      
     
                                                                           
                                                                          
                                                                          
                                                                            
                                                                        
                                                             
     

     
                                                                       
               
     
  xids = palloc(sizeof(TransactionId) * (running->xcnt + running->subxcnt));

     
                                                                      
     
  nxids = 0;
  for (i = 0; i < running->xcnt + running->subxcnt; i++)
  {
    TransactionId xid = running->xids[i];

       
                                                                           
                                                                      
                                                                       
             
       
    if (TransactionIdDidCommit(xid) || TransactionIdDidAbort(xid))
    {
      continue;
    }

    xids[nxids++] = xid;
  }

  if (nxids > 0)
  {
    if (procArray->numKnownAssignedXids != 0)
    {
      LWLockRelease(ProcArrayLock);
      elog(ERROR, "KnownAssignedXids is not empty");
    }

       
                                                          
                          
       
                                                                          
                                                                           
                                                                            
                                        
       
    qsort(xids, nxids, sizeof(TransactionId), xidLogicalComparator);

       
                                                                          
                                                                
                                     
       
    for (i = 0; i < nxids; i++)
    {
      if (i > 0 && TransactionIdEquals(xids[i - 1], xids[i]))
      {
        elog(DEBUG1, "found duplicated transaction %u for KnownAssignedXids insertion", xids[i]);
        continue;
      }
      KnownAssignedXidsAdd(xids[i], xids[i], true);
    }

    KnownAssignedXidsDisplay(trace_recovery(DEBUG3));
  }

  pfree(xids);

     
                                                                       
                                                                       
                                                                     
                                               
     
                                                                            
                                                                             
                                                              
     
  Assert(TransactionIdIsNormal(latestObservedXid));
  TransactionIdAdvance(latestObservedXid);
  while (TransactionIdPrecedes(latestObservedXid, running->nextXid))
  {
    ExtendSUBTRANS(latestObservedXid);
    TransactionIdAdvance(latestObservedXid);
  }
  TransactionIdRetreat(latestObservedXid);                             

                
                                                                          
                                                         
     
                                                               
                                                                
               
     
                                                                             
                                                                           
                                                                           
                                                                          
                
     
  if (running->subxid_overflow)
  {
    standbyState = STANDBY_SNAPSHOT_PENDING;

    standbySnapshotPendingXmin = latestObservedXid;
    procArray->lastOverflowedXid = latestObservedXid;
  }
  else
  {
    standbyState = STANDBY_SNAPSHOT_READY;

    standbySnapshotPendingXmin = InvalidTransactionId;
  }

     
                                                                          
                                                                             
                                                                             
     
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, running->latestCompletedXid))
  {
    ShmemVariableCache->latestCompletedXid = running->latestCompletedXid;
  }

  Assert(TransactionIdIsNormal(ShmemVariableCache->latestCompletedXid));

  LWLockRelease(ProcArrayLock);

                                                                        
  AdvanceNextFullTransactionIdPastXid(latestObservedXid);

  Assert(FullTransactionIdIsValid(ShmemVariableCache->nextFullXid));

  KnownAssignedXidsDisplay(trace_recovery(DEBUG3));
  if (standbyState == STANDBY_SNAPSHOT_READY)
  {
    elog(trace_recovery(DEBUG1), "recovery snapshots are now enabled");
  }
  else
  {
    elog(trace_recovery(DEBUG1),
        "recovery snapshot waiting for non-overflowed snapshot or "
        "until oldest active xid on standby is at least %u (now %u)",
        standbySnapshotPendingXmin, running->oldestRunningXid);
  }
}

   
                               
                                               
   
void
ProcArrayApplyXidAssignment(TransactionId topxid, int nsubxids, TransactionId *subxids)
{
  TransactionId max_xid;
  int i;

  Assert(standbyState >= STANDBY_INITIALIZED);

  max_xid = TransactionIdLatest(topxid, nsubxids, subxids);

     
                                               
     
                                                                     
                                                                            
                                                                             
                                                  
     
  RecordKnownAssignedTransactionIds(max_xid);

     
                                                                           
                                                                        
                                                                     
                                                                             
                                                                           
                                                                
                                                                           
                                                                         
                                            
     
  for (i = 0; i < nsubxids; i++)
  {
    SubTransSetParent(subxids[i], topxid);
  }

                                                                     
  if (standbyState == STANDBY_INITIALIZED)
  {
    return;
  }

     
                                             
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

     
                                               
     
  KnownAssignedXidsRemoveTree(InvalidTransactionId, nsubxids, subxids);

     
                                                                         
     
  if (TransactionIdPrecedes(procArray->lastOverflowedXid, max_xid))
  {
    procArray->lastOverflowedXid = max_xid;
  }

  LWLockRelease(ProcArrayLock);
}

   
                                                                             
   
                                                                          
                                                                   
   
                                                                             
                                                     
   
                                                                           
                                     
   
                                                                            
                                        
   
                                                                              
                                                                             
                                                                         
                                                                      
                
   
                                                                              
                                                                        
                                                                             
                                                  
   
bool
TransactionIdIsInProgress(TransactionId xid)
{
  static TransactionId *xids = NULL;
  int nxids = 0;
  ProcArrayStruct *arrayP = procArray;
  TransactionId topxid;
  int i, j;

     
                                                                             
                                                                            
                                                                     
               
     
  if (TransactionIdPrecedes(xid, RecentXmin))
  {
    xc_by_recent_xmin_inc();
    return false;
  }

     
                                                                          
                                                                          
                    
     
  if (TransactionIdEquals(cachedXidIsNotInProgress, xid))
  {
    xc_by_known_xact_inc();
    return false;
  }

     
                                                                           
                                  
     
  if (TransactionIdIsCurrentTransactionId(xid))
  {
    xc_by_my_xact_inc();
    return true;
  }

     
                                                                       
                                                                    
     
  if (xids == NULL)
  {
       
                                                                         
                                                                           
                                                           
       
    int maxxids = RecoveryInProgress() ? TOTAL_MAX_CACHED_SUBXIDS : arrayP->maxProcs;

    xids = (TransactionId *)malloc(maxxids * sizeof(TransactionId));
    if (xids == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

     
                                                                        
                                                          
     
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, xid))
  {
    LWLockRelease(ProcArrayLock);
    xc_by_latest_xid_inc();
    return true;
  }

                                                    
  for (i = 0; i < arrayP->numProcs; i++)
  {
    int pgprocno = arrayP->pgprocnos[i];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId pxid;
    int pxids;

                                                    
    if (proc == MyProc)
    {
      continue;
    }

                                                       
    pxid = UINT32_ACCESS_ONCE(pgxact->xid);

    if (!TransactionIdIsValid(pxid))
    {
      continue;
    }

       
                                  
       
    if (TransactionIdEquals(pxid, xid))
    {
      LWLockRelease(ProcArrayLock);
      xc_by_main_xid_inc();
      return true;
    }

       
                                                                           
                                                     
       
    if (TransactionIdPrecedes(xid, pxid))
    {
      continue;
    }

       
                                                  
       
    pxids = pgxact->nxids;
    pg_read_barrier();                                                  
    for (j = pxids - 1; j >= 0; j--)
    {
                                                         
      TransactionId cxid = UINT32_ACCESS_ONCE(proc->subxids.xids[j]);

      if (TransactionIdEquals(cxid, xid))
      {
        LWLockRelease(ProcArrayLock);
        xc_by_child_xid_inc();
        return true;
      }
    }

       
                                                                         
                                                                       
                                                                           
                                                                       
                     
       
    if (pgxact->overflowed)
    {
      xids[nxids++] = pxid;
    }
  }

     
                                                                            
                                             
     
  if (RecoveryInProgress())
  {
                                                                         
    Assert(nxids == 0);

    if (KnownAssignedXidExists(xid))
    {
      LWLockRelease(ProcArrayLock);
      xc_by_known_assigned_inc();
      return true;
    }

       
                                                                         
                                                                       
                                                                           
                                                                          
                               
       
    if (TransactionIdPrecedesOrEquals(xid, procArray->lastOverflowedXid))
    {
      nxids = KnownAssignedXidsGet(xids, xid);
    }
  }

  LWLockRelease(ProcArrayLock);

     
                                                                       
                                                  
     
  if (nxids == 0)
  {
    xc_no_overflow_inc();
    cachedXidIsNotInProgress = xid;
    return false;
  }

     
                                        
     
                                                                            
                                                                
                                                                             
                                                                             
     
  xc_slow_answer_inc();

  if (TransactionIdDidAbort(xid))
  {
    cachedXidIsNotInProgress = xid;
    return false;
  }

     
                                                                           
                                                                          
                          
     
  topxid = SubTransGetTopmostTransaction(xid);
  Assert(TransactionIdIsValid(topxid));
  if (!TransactionIdEquals(topxid, xid))
  {
    for (i = 0; i < nxids; i++)
    {
      if (TransactionIdEquals(xids[i], topxid))
      {
        return true;
      }
    }
  }

  cachedXidIsNotInProgress = xid;
  return false;
}

   
                                                                           
   
                                                                           
                                                                           
                                                                         
                     
   
bool
TransactionIdIsActive(TransactionId xid)
{
  bool result = false;
  ProcArrayStruct *arrayP = procArray;
  int i;

     
                                                                             
                                
     
  if (TransactionIdPrecedes(xid, RecentXmin))
  {
    return false;
  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (i = 0; i < arrayP->numProcs; i++)
  {
    int pgprocno = arrayP->pgprocnos[i];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId pxid;

                                                       
    pxid = UINT32_ACCESS_ONCE(pgxact->xid);

    if (!TransactionIdIsValid(pxid))
    {
      continue;
    }

    if (proc->pid == 0)
    {
      continue;                                   
    }

    if (TransactionIdEquals(pxid, xid))
    {
      result = true;
      break;
    }
  }

  LWLockRelease(ProcArrayLock);

  return result;
}

   
                                                                
                                                 
   
                                                                               
                                                          
   
                                                                            
                                                                          
                                                            
   
                                                                    
                                                                       
                                
   
                                                                              
                                                                               
                                                                            
                                                                               
                                                                             
                                                                          
   
                                                                           
                                                                            
              
   
                                                                              
                                                                          
                                                                               
                                                   
   
                                                                           
                                                                              
                                                                             
                                                                              
                                                                      
                                                                          
                                                                              
                                                                              
                                                                             
                                                                          
                                                                             
                                                                            
                                                                           
                                                                          
                                                                          
                                                                          
                                                                          
                                                                          
                                                                             
                                                                       
                                                                  
                                                                            
   
TransactionId
GetOldestXmin(Relation rel, int flags)
{
  ProcArrayStruct *arrayP = procArray;
  TransactionId result;
  int index;
  bool allDbs;

  TransactionId replication_slot_xmin = InvalidTransactionId;
  TransactionId replication_slot_catalog_xmin = InvalidTransactionId;

     
                                                                      
                                                                       
                 
     
  allDbs = rel == NULL || rel->rd_rel->relisshared;

                                                            
  Assert(allDbs || !RecoveryInProgress());

  LWLockAcquire(ProcArrayLock, LW_SHARED);

     
                                                                           
                                                                             
                                                                        
                
     
  result = ShmemVariableCache->latestCompletedXid;
  Assert(TransactionIdIsNormal(result));
  TransactionIdAdvance(result);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (pgxact->vacuumFlags & (flags & PROCARRAY_PROC_FLAGS_MASK))
    {
      continue;
    }

    if (allDbs || proc->databaseId == MyDatabaseId || proc->databaseId == 0)                               
    {
                                                         
      TransactionId xid = UINT32_ACCESS_ONCE(pgxact->xid);

                                                            
      if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, result))
      {
        result = xid;
      }

         
                                                       
         
                                                                     
                                                                     
                                                          
         
      xid = UINT32_ACCESS_ONCE(pgxact->xmin);
      if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, result))
      {
        result = xid;
      }
    }
  }

     
                                                                 
                                                                        
           
     
  replication_slot_xmin = procArray->replication_slot_xmin;
  replication_slot_catalog_xmin = procArray->replication_slot_catalog_xmin;

  if (RecoveryInProgress())
  {
       
                                                                          
                                
       
    TransactionId kaxmin = KnownAssignedXidsGetOldestXmin();

    LWLockRelease(ProcArrayLock);

    if (TransactionIdIsNormal(kaxmin) && TransactionIdPrecedes(kaxmin, result))
    {
      result = kaxmin;
    }
  }
  else
  {
       
                                                                     
       
    LWLockRelease(ProcArrayLock);

       
                                                                       
                                                        
       
                                                                        
                                                                         
                                                                          
                                                                        
                                                                   
                                                                           
                                                                           
                                                                 
                                                    
       
    result -= vacuum_defer_cleanup_age;
    if (!TransactionIdIsNormal(result))
    {
      result = FirstNormalTransactionId;
    }
  }

     
                                                                        
     
  if (!(flags & PROCARRAY_SLOTS_XMIN) && TransactionIdIsValid(replication_slot_xmin) && NormalTransactionIdPrecedes(replication_slot_xmin, result))
  {
    result = replication_slot_xmin;
  }

     
                                                                            
                                                                       
                                                                           
                                                                         
     
  if (!(flags & PROCARRAY_SLOTS_XMIN) && (rel == NULL || RelationIsAccessibleInLogicalDecoding(rel)) && TransactionIdIsValid(replication_slot_catalog_xmin) && NormalTransactionIdPrecedes(replication_slot_catalog_xmin, result))
  {
    result = replication_slot_catalog_xmin;
  }

  return result;
}

   
                                                                 
   
                                                
   
int
GetMaxSnapshotXidCount(void)
{
  return procArray->maxProcs;
}

   
                                                                        
   
                                                
   
int
GetMaxSnapshotSubxidCount(void)
{
  return TOTAL_MAX_CACHED_SUBXIDS;
}

   
                                                                      
   
                                                                       
                                                                        
                                                            
                                                 
                                                       
                                                                   
                                     
                                                                      
                                                             
   
                                                                            
                                                                          
                                                                         
                                                                               
                                                                            
                                                                              
                            
   
                                                          
                                                                   
                                                               
                                                                      
                                                     
                                                                         
                                                                       
                                  
                                                  
                                                                 
                         
   
                                                                             
                                                        
   
Snapshot
GetSnapshotData(Snapshot snapshot)
{
  ProcArrayStruct *arrayP = procArray;
  TransactionId xmin;
  TransactionId xmax;
  TransactionId globalxmin;
  int index;
  int count = 0;
  int subcount = 0;
  bool suboverflowed = false;
  TransactionId replication_slot_xmin = InvalidTransactionId;
  TransactionId replication_slot_catalog_xmin = InvalidTransactionId;

  Assert(snapshot != NULL);

     
                                                                            
                                                                            
                                                                         
                                                  
     
                                                                           
                                                                           
                                                                        
                                   
     
  if (snapshot->xip == NULL)
  {
       
                                                                          
                                               
       
    snapshot->xip = (TransactionId *)malloc(GetMaxSnapshotXidCount() * sizeof(TransactionId));
    if (snapshot->xip == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
    Assert(snapshot->subxip == NULL);
    snapshot->subxip = (TransactionId *)malloc(GetMaxSnapshotSubxidCount() * sizeof(TransactionId));
    if (snapshot->subxip == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
  }

     
                                                                          
                                  
     
  LWLockAcquire(ProcArrayLock, LW_SHARED);

                                             
  xmax = ShmemVariableCache->latestCompletedXid;
  Assert(TransactionIdIsNormal(xmax));
  TransactionIdAdvance(xmax);

                                             
  globalxmin = xmin = xmax;

  snapshot->takenDuringRecovery = RecoveryInProgress();

  if (!snapshot->takenDuringRecovery)
  {
    int *pgprocnos = arrayP->pgprocnos;
    int numProcs;

       
                                                                         
                                                                          
                
       
    numProcs = arrayP->numProcs;
    for (index = 0; index < numProcs; index++)
    {
      int pgprocno = pgprocnos[index];
      PGXACT *pgxact = &allPgXact[pgprocno];
      TransactionId xid;

         
                                                                      
                                                                
         
      if (pgxact->vacuumFlags & (PROC_IN_LOGICAL_DECODING | PROC_IN_VACUUM))
      {
        continue;
      }

                                                           
      xid = UINT32_ACCESS_ONCE(pgxact->xmin);
      if (TransactionIdIsNormal(xid) && NormalTransactionIdPrecedes(xid, globalxmin))
      {
        globalxmin = xid;
      }

                                                         
      xid = UINT32_ACCESS_ONCE(pgxact->xid);

         
                                                                    
                                                                         
                                                                      
                                                  
         
      if (!TransactionIdIsNormal(xid) || !NormalTransactionIdPrecedes(xid, xmax))
      {
        continue;
      }

         
                                                                        
                                    
         
      if (NormalTransactionIdPrecedes(xid, xmin))
      {
        xmin = xid;
      }
      if (pgxact == MyPgXact)
      {
        continue;
      }

                                
      snapshot->xip[count++] = xid;

         
                                                                
                                                                         
                                                                      
                                                                         
                                                            
         
                                                                         
                                                                     
                                                                        
                                                                         
                
         
                                                               
         
      if (!suboverflowed)
      {
        if (pgxact->overflowed)
        {
          suboverflowed = true;
        }
        else
        {
          int nxids = pgxact->nxids;

          if (nxids > 0)
          {
            PGPROC *proc = &allProcs[pgprocno];

            pg_read_barrier();                                     

            memcpy(snapshot->subxip + subcount, (void *)proc->subxids.xids, nxids * sizeof(TransactionId));
            subcount += nxids;
          }
        }
      }
    }
  }
  else
  {
       
                                                                 
       
                                                             
       
                                                                        
                                                                         
       
                                                                           
                                                                         
                                                                           
                            
       
                                                                          
                                                                           
                                                                          
                                                                          
                                                                   
       
                                                                      
                                                                    
                                          
       
                                                                        
                                                                           
                                                                          
                                                                       
                                         
       
    subcount = KnownAssignedXidsGetAndSetXmin(snapshot->subxip, &xmin, xmax);

    if (TransactionIdPrecedesOrEquals(xmin, procArray->lastOverflowedXid))
    {
      suboverflowed = true;
    }
  }

     
                                                                 
                                                                        
           
     
  replication_slot_xmin = procArray->replication_slot_xmin;
  replication_slot_catalog_xmin = procArray->replication_slot_catalog_xmin;

  if (!TransactionIdIsValid(MyPgXact->xmin))
  {
    MyPgXact->xmin = TransactionXmin = xmin;
  }

  LWLockRelease(ProcArrayLock);

     
                                                                           
                                                                            
                      
     
  if (TransactionIdPrecedes(xmin, globalxmin))
  {
    globalxmin = xmin;
  }

                                   
  RecentGlobalXmin = globalxmin - vacuum_defer_cleanup_age;
  if (!TransactionIdIsNormal(RecentGlobalXmin))
  {
    RecentGlobalXmin = FirstNormalTransactionId;
  }

                                                                         
  if (TransactionIdIsValid(replication_slot_xmin) && NormalTransactionIdPrecedes(replication_slot_xmin, RecentGlobalXmin))
  {
    RecentGlobalXmin = replication_slot_xmin;
  }

                                                                 
  RecentGlobalDataXmin = RecentGlobalXmin;

     
                                                                         
           
     
  if (TransactionIdIsNormal(replication_slot_catalog_xmin) && NormalTransactionIdPrecedes(replication_slot_catalog_xmin, RecentGlobalXmin))
  {
    RecentGlobalXmin = replication_slot_catalog_xmin;
  }

  RecentXmin = xmin;

  snapshot->xmin = xmin;
  snapshot->xmax = xmax;
  snapshot->xcnt = count;
  snapshot->subxcnt = subcount;
  snapshot->suboverflowed = suboverflowed;

  snapshot->curcid = GetCurrentCommandId(false);

     
                                                                            
                                      
     
  snapshot->active_count = 0;
  snapshot->regd_count = 0;
  snapshot->copied = false;

  if (old_snapshot_threshold < 0)
  {
       
                                                                         
                                                    
       
    snapshot->lsn = InvalidXLogRecPtr;
    snapshot->whenTaken = 0;
  }
  else
  {
       
                                                                     
                                                                       
                             
       
    snapshot->lsn = GetXLogInsertRecPtr();
    snapshot->whenTaken = GetSnapshotCurrentTimestamp();
    MaintainOldSnapshotTimeMapping(snapshot->whenTaken, xmin);
  }

  return snapshot;
}

   
                                                                             
   
                                                                   
                                                                         
                                                                          
                                                 
   
                                                                          
   
bool
ProcArrayInstallImportedXmin(TransactionId xmin, VirtualTransactionId *sourcevxid)
{
  bool result = false;
  ProcArrayStruct *arrayP = procArray;
  int index;

  Assert(TransactionIdIsNormal(xmin));
  if (!sourcevxid)
  {
    return false;
  }

                                                                
  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId xid;

                                          
    if (pgxact->vacuumFlags & PROC_IN_VACUUM)
    {
      continue;
    }

                                                                     
    if (proc->backendId != sourcevxid->backendId)
    {
      continue;
    }
    if (proc->lxid != sourcevxid->localTransactionId)
    {
      continue;
    }

       
                                                                           
                                                                          
                                                                  
                                
       
    if (proc->databaseId != MyDatabaseId)
    {
      continue;
    }

       
                                                                   
       
    xid = UINT32_ACCESS_ONCE(pgxact->xmin);
    if (!TransactionIdIsNormal(xid) || !TransactionIdPrecedesOrEquals(xid, xmin))
    {
      continue;
    }

       
                                                                      
                                                                 
                                                                         
                             
       
    MyPgXact->xmin = TransactionXmin = xmin;

    result = true;
    break;
  }

  LWLockRelease(ProcArrayLock);

  return result;
}

   
                                                                             
   
                                                                           
                                                                              
           
   
                                                                          
   
bool
ProcArrayInstallRestoredXmin(TransactionId xmin, PGPROC *proc)
{
  bool result = false;
  TransactionId xid;
  PGXACT *pgxact;

  Assert(TransactionIdIsNormal(xmin));
  Assert(proc != NULL);

                                                                
  LWLockAcquire(ProcArrayLock, LW_SHARED);

  pgxact = &allPgXact[proc->pgprocno];

     
                                                                           
                                                                          
                                                                             
                                                        
     
  xid = UINT32_ACCESS_ONCE(pgxact->xmin);
  if (proc->databaseId == MyDatabaseId && TransactionIdIsNormal(xid) && TransactionIdPrecedesOrEquals(xid, xmin))
  {
    MyPgXact->xmin = TransactionXmin = xmin;
    result = true;
  }

  LWLockRelease(ProcArrayLock);

  return result;
}

   
                                                                                
   
                                                                       
                                                                         
                          
   
                                                                              
                                                                                
                                                                         
                                                                               
                     
   
                                                                          
                                                                  
   
                                                                         
                      
   
                                                                           
                                                                       
                                                                       
                                                                  
   
                                                                         
                                                                          
                     
   
                                                                          
                                                           
   
RunningTransactions
GetRunningTransactionData(void)
{
                        
  static RunningTransactionsData CurrentRunningXactsData;

  ProcArrayStruct *arrayP = procArray;
  RunningTransactions CurrentRunningXacts = &CurrentRunningXactsData;
  TransactionId latestCompletedXid;
  TransactionId oldestRunningXid;
  TransactionId *xids;
  int index;
  int count;
  int subcount;
  bool suboverflowed;

  Assert(!RecoveryInProgress());

     
                                                                            
                                                                            
                                                                         
                                                  
     
                                                                           
                  
     
  if (CurrentRunningXacts->xids == NULL)
  {
       
                  
       
    CurrentRunningXacts->xids = (TransactionId *)malloc(TOTAL_MAX_CACHED_SUBXIDS * sizeof(TransactionId));
    if (CurrentRunningXacts->xids == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
  }

  xids = CurrentRunningXacts->xids;

  count = subcount = 0;
  suboverflowed = false;

     
                                                                      
               
     
  LWLockAcquire(ProcArrayLock, LW_SHARED);
  LWLockAcquire(XidGenLock, LW_SHARED);

  latestCompletedXid = ShmemVariableCache->latestCompletedXid;

  oldestRunningXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);

     
                                             
     
  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId xid;

                                                       
    xid = UINT32_ACCESS_ONCE(pgxact->xid);

       
                                                                           
                                                                      
       
    if (!TransactionIdIsValid(xid))
    {
      continue;
    }

       
                                                                           
                                                                         
                                                    
       
    if (TransactionIdPrecedes(xid, oldestRunningXid))
    {
      oldestRunningXid = xid;
    }

    if (pgxact->overflowed)
    {
      suboverflowed = true;
    }

       
                                                                          
                                                                         
                                                                           
                                                                        
                                                   
       

    xids[count++] = xid;
  }

     
                                                                          
                         
     
  if (!suboverflowed)
  {
    for (index = 0; index < arrayP->numProcs; index++)
    {
      int pgprocno = arrayP->pgprocnos[index];
      PGPROC *proc = &allProcs[pgprocno];
      PGXACT *pgxact = &allPgXact[pgprocno];
      int nxids;

         
                                                                      
                                                 
         
      nxids = pgxact->nxids;
      if (nxids > 0)
      {
                                                                         
        pg_read_barrier();                                     

        memcpy(&xids[count], (void *)proc->subxids.xids, nxids * sizeof(TransactionId));
        count += nxids;
        subcount += nxids;

           
                                                                     
                                                                
                                                     
           
      }
    }
  }

     
                                                                          
                                                                            
                                                                             
                                                                          
                                                                     
                            
     

  CurrentRunningXacts->xcnt = count - subcount;
  CurrentRunningXacts->subxcnt = subcount;
  CurrentRunningXacts->subxid_overflow = suboverflowed;
  CurrentRunningXacts->nextXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  CurrentRunningXacts->oldestRunningXid = oldestRunningXid;
  CurrentRunningXacts->latestCompletedXid = latestCompletedXid;

  Assert(TransactionIdIsValid(CurrentRunningXacts->nextXid));
  Assert(TransactionIdIsValid(CurrentRunningXacts->oldestRunningXid));
  Assert(TransactionIdIsNormal(CurrentRunningXacts->latestCompletedXid));

                                                                           

  return CurrentRunningXacts;
}

   
                                  
   
                                                                           
                                                                      
                                                                          
                                                      
   
                                                                         
                      
   
                                                                         
                                                                          
                     
   
TransactionId
GetOldestActiveTransactionId(void)
{
  ProcArrayStruct *arrayP = procArray;
  TransactionId oldestRunningXid;
  int index;

  Assert(!RecoveryInProgress());

     
                                                              
     
                                                                          
                                                                            
                                                    
     
  LWLockAcquire(XidGenLock, LW_SHARED);
  oldestRunningXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  LWLockRelease(XidGenLock);

     
                                                          
     
  LWLockAcquire(ProcArrayLock, LW_SHARED);
  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGXACT *pgxact = &allPgXact[pgprocno];
    TransactionId xid;

                                                       
    xid = UINT32_ACCESS_ONCE(pgxact->xid);

    if (!TransactionIdIsNormal(xid))
    {
      continue;
    }

    if (TransactionIdPrecedes(xid, oldestRunningXid))
    {
      oldestRunningXid = xid;
    }

       
                                                                     
                                                                    
                                     
       
  }
  LWLockRelease(ProcArrayLock);

  return oldestRunningXid;
}

   
                                                                           
   
                                                                             
                                                                       
                                                                               
                                                                               
                                               
   
                                                                           
                                                           
   
                                                                        
                                                                              
                                                                         
   
TransactionId
GetOldestSafeDecodingTransactionId(bool catalogOnly)
{
  ProcArrayStruct *arrayP = procArray;
  TransactionId oldestSafeXid;
  int index;
  bool recovery_in_progress = RecoveryInProgress();

  Assert(LWLockHeldByMe(ProcArrayLock));

     
                                                                           
                                                                             
                                           
     
                                                                            
                                     
     
  LWLockAcquire(XidGenLock, LW_SHARED);
  oldestSafeXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);

     
                                                                           
                                                                        
                                                                           
                                                                         
                                                      
     
  if (TransactionIdIsValid(procArray->replication_slot_xmin) && TransactionIdPrecedes(procArray->replication_slot_xmin, oldestSafeXid))
  {
    oldestSafeXid = procArray->replication_slot_xmin;
  }

  if (catalogOnly && TransactionIdIsValid(procArray->replication_slot_catalog_xmin) && TransactionIdPrecedes(procArray->replication_slot_catalog_xmin, oldestSafeXid))
  {
    oldestSafeXid = procArray->replication_slot_catalog_xmin;
  }

     
                                                                          
                                                                     
                                                                    
                                                                        
                         
     
                                                                           
                                                                           
                                                                            
                                                                       
     
  if (!recovery_in_progress)
  {
       
                                                           
       
    for (index = 0; index < arrayP->numProcs; index++)
    {
      int pgprocno = arrayP->pgprocnos[index];
      PGXACT *pgxact = &allPgXact[pgprocno];
      TransactionId xid;

                                                         
      xid = UINT32_ACCESS_ONCE(pgxact->xid);

      if (!TransactionIdIsNormal(xid))
      {
        continue;
      }

      if (TransactionIdPrecedes(xid, oldestSafeXid))
      {
        oldestSafeXid = xid;
      }
    }
  }

  LWLockRelease(XidGenLock);

  return oldestSafeXid;
}

   
                                                                             
                                                                        
                        
   
                                                                             
                                                                               
        
   
                                                                
                                           
   
                                                                        
                                                                          
                                                                       
                                                                       
                                                                          
                                                                         
                                                                   
                                                           
   
static VirtualTransactionId *
GetVirtualXIDsDelayingChkptGuts(int *nvxids, int type)
{
  VirtualTransactionId *vxids;
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

  Assert(type != 0);

                                                     
  vxids = (VirtualTransactionId *)palloc(sizeof(VirtualTransactionId) * arrayP->maxProcs);

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (((type & DELAY_CHKPT_START) && pgxact->delayChkpt) || ((type & DELAY_CHKPT_COMPLETE) && proc->delayChkptEnd))
    {
      VirtualTransactionId vxid;

      GET_VXID_FROM_PGPROC(vxid, *proc);
      if (VirtualTransactionIdIsValid(vxid))
      {
        vxids[count++] = vxid;
      }
    }
  }

  LWLockRelease(ProcArrayLock);

  *nvxids = count;
  return vxids;
}

   
                                                                        
                                       
   
VirtualTransactionId *
GetVirtualXIDsDelayingChkpt(int *nvxids)
{
  return GetVirtualXIDsDelayingChkptGuts(nvxids, DELAY_CHKPT_START);
}

   
                                                                           
                                     
   
VirtualTransactionId *
GetVirtualXIDsDelayingChkptEnd(int *nvxids)
{
  return GetVirtualXIDsDelayingChkptGuts(nvxids, DELAY_CHKPT_COMPLETE);
}

   
                                                                            
   
                                                                              
                                                                  
   
                                                                            
                                                                    
   
static bool
HaveVirtualXIDsDelayingChkptGuts(VirtualTransactionId *vxids, int nvxids, int type)
{
  bool result = false;
  ProcArrayStruct *arrayP = procArray;
  int index;

  Assert(type != 0);

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];
    VirtualTransactionId vxid;

    GET_VXID_FROM_PGPROC(vxid, *proc);

    if ((((type & DELAY_CHKPT_START) && pgxact->delayChkpt) || ((type & DELAY_CHKPT_COMPLETE) && proc->delayChkptEnd)) && VirtualTransactionIdIsValid(vxid))
    {
      int i;

      for (i = 0; i < nvxids; i++)
      {
        if (VirtualTransactionIdEquals(vxid, vxids[i]))
        {
          result = true;
          break;
        }
      }
      if (result)
      {
        break;
      }
    }
  }

  LWLockRelease(ProcArrayLock);

  return result;
}

   
                                                                           
                              
   
bool
HaveVirtualXIDsDelayingChkpt(VirtualTransactionId *vxids, int nvxids)
{
  return HaveVirtualXIDsDelayingChkptGuts(vxids, nvxids, DELAY_CHKPT_START);
}

   
                                                                              
                            
   
bool
HaveVirtualXIDsDelayingChkptEnd(VirtualTransactionId *vxids, int nvxids)
{
  return HaveVirtualXIDsDelayingChkptGuts(vxids, nvxids, DELAY_CHKPT_COMPLETE);
}

   
                                                             
   
                                                                      
                                                                     
                         
   
PGPROC *
BackendPidGetProc(int pid)
{
  PGPROC *result;

  if (pid == 0)                                
  {
    return NULL;
  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  result = BackendPidGetProcWithLock(pid);

  LWLockRelease(ProcArrayLock);

  return result;
}

   
                                                                     
   
                                                                          
                                                                               
   
PGPROC *
BackendPidGetProcWithLock(int pid)
{
  PGPROC *result = NULL;
  ProcArrayStruct *arrayP = procArray;
  int index;

  if (pid == 0)                                
  {
    return NULL;
  }

  for (index = 0; index < arrayP->numProcs; index++)
  {
    PGPROC *proc = &allProcs[arrayP->pgprocnos[index]];

    if (proc->pid == pid)
    {
      result = proc;
      break;
    }
  }

  return result;
}

   
                                                         
   
                                                                     
                                                               
                                                            
   
                                                                      
                                                    
   
                                                                            
                                                           
   
int
BackendXidGetPid(TransactionId xid)
{
  int result = 0;
  ProcArrayStruct *arrayP = procArray;
  int index;

  if (xid == InvalidTransactionId)                              
  {
    return 0;
  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (pgxact->xid == xid)
    {
      result = proc->pid;
      break;
    }
  }

  LWLockRelease(ProcArrayLock);

  return result;
}

   
                                                    
   
                                                                         
   
bool
IsBackendPid(int pid)
{
  return (BackendPidGetProc(pid) != NULL);
}

   
                                                                        
   
                                                                                
   
                                                                             
                                    
                                                                 
                      
                                                          
                                                                   
                                                         
                                               
   
                                                                        
                                                                       
                                                                           
                                                                            
                                                                       
                                                                            
                                                                           
                                                                             
                                                                              
                                                                          
                                                                 
   
VirtualTransactionId *
GetCurrentVirtualXIDs(TransactionId limitXmin, bool excludeXmin0, bool allDbs, int excludeVacuum, int *nvxids)
{
  VirtualTransactionId *vxids;
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

                                                     
  vxids = (VirtualTransactionId *)palloc(sizeof(VirtualTransactionId) * arrayP->maxProcs);

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

    if (proc == MyProc)
    {
      continue;
    }

    if (excludeVacuum & pgxact->vacuumFlags)
    {
      continue;
    }

    if (allDbs || proc->databaseId == MyDatabaseId)
    {
                                                     
      TransactionId pxmin = UINT32_ACCESS_ONCE(pgxact->xmin);

      if (excludeXmin0 && !TransactionIdIsValid(pxmin))
      {
        continue;
      }

         
                                                                      
                                                                
         
      if (!TransactionIdIsValid(limitXmin) || TransactionIdPrecedesOrEquals(pxmin, limitXmin))
      {
        VirtualTransactionId vxid;

        GET_VXID_FROM_PGPROC(vxid, *proc);
        if (VirtualTransactionIdIsValid(vxid))
        {
          vxids[count++] = vxid;
        }
      }
    }
  }

  LWLockRelease(ProcArrayLock);

  *nvxids = count;
  return vxids;
}

   
                                                                            
   
                                                                               
                                                                             
                                                                               
   
                                                                        
                                                                           
                                     
   
                                                                            
                                                                      
                                                                        
                                                                         
                                                                           
                                                                           
                                                                           
                                                                              
                                                                          
                                                                             
                                                  
                                                                            
                                                                     
   
                                                                   
   
                                                                     
                                                                    
   
VirtualTransactionId *
GetConflictingVirtualXIDs(TransactionId limitXmin, Oid dbOid)
{
  static VirtualTransactionId *vxids;
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

     
                                                                       
                                                                          
                                                      
     
  if (vxids == NULL)
  {
    vxids = (VirtualTransactionId *)malloc(sizeof(VirtualTransactionId) * (arrayP->maxProcs + 1));
    if (vxids == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
  }

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

                                       
    if (proc->pid == 0)
    {
      continue;
    }

    if (!OidIsValid(dbOid) || proc->databaseId == dbOid)
    {
                                                                      
      TransactionId pxmin = UINT32_ACCESS_ONCE(pgxact->xmin);

         
                                                                        
                                                                         
                                                                         
                                                                    
                                                                        
                    
         
      if (!TransactionIdIsValid(limitXmin) || (TransactionIdIsValid(pxmin) && !TransactionIdFollows(pxmin, limitXmin)))
      {
        VirtualTransactionId vxid;

        GET_VXID_FROM_PGPROC(vxid, *proc);
        if (VirtualTransactionIdIsValid(vxid))
        {
          vxids[count++] = vxid;
        }
      }
    }
  }

  LWLockRelease(ProcArrayLock);

                          
  vxids[count].backendId = InvalidBackendId;
  vxids[count].localTransactionId = InvalidLocalTransactionId;

  return vxids;
}

   
                                                                   
   
                                                           
   
pid_t
CancelVirtualTransaction(VirtualTransactionId vxid, ProcSignalReason sigmode)
{
  return SignalVirtualTransaction(vxid, sigmode, true);
}

pid_t
SignalVirtualTransaction(VirtualTransactionId vxid, ProcSignalReason sigmode, bool conflictPending)
{
  ProcArrayStruct *arrayP = procArray;
  int index;
  pid_t pid = 0;

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    VirtualTransactionId procvxid;

    GET_VXID_FROM_PGPROC(procvxid, *proc);

    if (procvxid.backendId == vxid.backendId && procvxid.localTransactionId == vxid.localTransactionId)
    {
      proc->recoveryConflictPending = conflictPending;
      pid = proc->pid;
      if (pid != 0)
      {
           
                                                                   
                                        
           
        (void)SendProcSignal(pid, sigmode, vxid.backendId);
      }
      break;
    }
  }

  LWLockRelease(ProcArrayLock);

  return pid;
}

   
                                                                         
                                                                  
                                                                        
                                                        
   
                                                                            
                                                       
   
bool
MinimumActiveBackends(int min)
{
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

                                                      
  if (min == 0)
  {
    return true;
  }

     
                                                                            
                                                                         
                                                                             
     
  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];
    PGXACT *pgxact = &allPgXact[pgprocno];

       
                                                                        
                                                                       
                             
       
                                                                          
                                                                        
                                                                          
                                                                           
                                                
       
    if (pgprocno == -1)
    {
      continue;                                   
    }
    if (proc == MyProc)
    {
      continue;                          
    }
    if (pgxact->xid == InvalidTransactionId)
    {
      continue;                                      
    }
    if (proc->pid == 0)
    {
      continue;                                  
    }
    if (proc->waitLock != NULL)
    {
      continue;                                        
    }
    count++;
    if (count >= min)
    {
      break;
    }
  }

  return count >= min;
}

   
                                                                        
   
int
CountDBBackends(Oid databaseid)
{
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];

    if (proc->pid == 0)
    {
      continue;                                  
    }
    if (!OidIsValid(databaseid) || proc->databaseId == databaseid)
    {
      count++;
    }
  }

  LWLockRelease(ProcArrayLock);

  return count;
}

   
                                                                           
                     
   
int
CountDBConnections(Oid databaseid)
{
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];

    if (proc->pid == 0)
    {
      continue;                                  
    }
    if (proc->isBackgroundWorker)
    {
      continue;                                      
    }
    if (!OidIsValid(databaseid) || proc->databaseId == databaseid)
    {
      count++;
    }
  }

  LWLockRelease(ProcArrayLock);

  return count;
}

   
                                                                          
   
void
CancelDBBackends(Oid databaseid, ProcSignalReason sigmode, bool conflictPending)
{
  ProcArrayStruct *arrayP = procArray;
  int index;
  pid_t pid = 0;

                                
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];

    if (databaseid == InvalidOid || proc->databaseId == databaseid)
    {
      VirtualTransactionId procvxid;

      GET_VXID_FROM_PGPROC(procvxid, *proc);

      proc->recoveryConflictPending = conflictPending;
      pid = proc->pid;
      if (pid != 0)
      {
           
                                                                   
                                        
           
        (void)SendProcSignal(pid, sigmode, procvxid.backendId);
      }
    }
  }

  LWLockRelease(ProcArrayLock);
}

   
                                                                        
   
int
CountUserBackends(Oid roleid)
{
  ProcArrayStruct *arrayP = procArray;
  int count = 0;
  int index;

  LWLockAcquire(ProcArrayLock, LW_SHARED);

  for (index = 0; index < arrayP->numProcs; index++)
  {
    int pgprocno = arrayP->pgprocnos[index];
    PGPROC *proc = &allProcs[pgprocno];

    if (proc->pid == 0)
    {
      continue;                                  
    }
    if (proc->isBackgroundWorker)
    {
      continue;                                      
    }
    if (proc->roleId == roleid)
    {
      count++;
    }
  }

  LWLockRelease(ProcArrayLock);

  return count;
}

   
                                                                            
   
                                                                              
                                                                          
                                                                       
   
                                                                           
                                                                           
   
                                                                             
                                                                           
                                                      
   
                                                                         
                                                                             
                                                                              
                                                                            
                                                                               
                                                                              
                                                                      
                 
   
bool
CountOtherDBBackends(Oid databaseId, int *nbackends, int *nprepared)
{
  ProcArrayStruct *arrayP = procArray;

#define MAXAUTOVACPIDS 10                                            
  int autovac_pids[MAXAUTOVACPIDS];
  int tries;

                                                                      
  for (tries = 0; tries < 50; tries++)
  {
    int nautovacs = 0;
    bool found = false;
    int index;

    CHECK_FOR_INTERRUPTS();

    *nbackends = *nprepared = 0;

    LWLockAcquire(ProcArrayLock, LW_SHARED);

    for (index = 0; index < arrayP->numProcs; index++)
    {
      int pgprocno = arrayP->pgprocnos[index];
      PGPROC *proc = &allProcs[pgprocno];
      PGXACT *pgxact = &allPgXact[pgprocno];

      if (proc->databaseId != databaseId)
      {
        continue;
      }
      if (proc == MyProc)
      {
        continue;
      }

      found = true;

      if (proc->pid == 0)
      {
        (*nprepared)++;
      }
      else
      {
        (*nbackends)++;
        if ((pgxact->vacuumFlags & PROC_IS_AUTOVACUUM) && nautovacs < MAXAUTOVACPIDS)
        {
          autovac_pids[nautovacs++] = proc->pid;
        }
      }
    }

    LWLockRelease(ProcArrayLock);

    if (!found)
    {
      return false;                                       
    }

       
                                                                       
                                                                        
                                                                           
                                         
       
    for (index = 0; index < nautovacs; index++)
    {
      (void)kill(autovac_pids[index], SIGTERM);                       
    }

                               
    pg_usleep(100 * 1000L);            
  }

  return true;                                 
}

   
                                   
   
                                                                               
                                                                            
                      
   
void
ProcArraySetReplicationSlotXmin(TransactionId xmin, TransactionId catalog_xmin, bool already_locked)
{
  Assert(!already_locked || LWLockHeldByMe(ProcArrayLock));

  if (!already_locked)
  {
    LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  }

  procArray->replication_slot_xmin = xmin;
  procArray->replication_slot_catalog_xmin = catalog_xmin;

  if (!already_locked)
  {
    LWLockRelease(ProcArrayLock);
  }
}

   
                                   
   
                                                                           
                                        
   
void
ProcArrayGetReplicationSlotXmin(TransactionId *xmin, TransactionId *catalog_xmin)
{
  LWLockAcquire(ProcArrayLock, LW_SHARED);

  if (xmin != NULL)
  {
    *xmin = procArray->replication_slot_xmin;
  }

  if (catalog_xmin != NULL)
  {
    *catalog_xmin = procArray->replication_slot_catalog_xmin;
  }

  LWLockRelease(ProcArrayLock);
}

#define XidCacheRemove(i)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    MyProc->subxids.xids[i] = MyProc->subxids.xids[MyPgXact->nxids - 1];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    pg_write_barrier();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    MyPgXact->nxids--;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

   
                             
   
                                                                   
                                                                        
                                                                          
                                                     
   
void
XidCacheRemoveRunningXids(TransactionId xid, int nxids, const TransactionId *xids, TransactionId latestXid)
{
  int i, j;

  Assert(TransactionIdIsValid(xid));

     
                                                                            
                                                                            
                                                                            
                                                                        
                   
     
                                                                             
                                                                          
                                                                             
                                        
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

     
                                                                            
                                                                        
                                           
     
  for (i = nxids - 1; i >= 0; i--)
  {
    TransactionId anxid = xids[i];

    for (j = MyPgXact->nxids - 1; j >= 0; j--)
    {
      if (TransactionIdEquals(MyProc->subxids.xids[j], anxid))
      {
        XidCacheRemove(j);
        break;
      }
    }

       
                                                                
                                                                     
                                                                         
                                                                       
                      
       
    if (j < 0 && !MyPgXact->overflowed)
    {
      elog(WARNING, "did not find subXID %u in MyProc", anxid);
    }
  }

  for (j = MyPgXact->nxids - 1; j >= 0; j--)
  {
    if (TransactionIdEquals(MyProc->subxids.xids[j], xid))
    {
      XidCacheRemove(j);
      break;
    }
  }
                                                                           
  if (j < 0 && !MyPgXact->overflowed)
  {
    elog(WARNING, "did not find subXID %u in MyProc", xid);
  }

                                                                     
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, latestXid))
  {
    ShmemVariableCache->latestCompletedXid = latestXid;
  }

  LWLockRelease(ProcArrayLock);
}

#ifdef XIDCACHE_DEBUG

   
                                                
   
static void
DisplayXidCache(void)
{
  fprintf(stderr, "XidCache: xmin: %ld, known: %ld, myxact: %ld, latest: %ld, mainxid: %ld, childxid: %ld, knownassigned: %ld, nooflo: %ld, slow: %ld\n", xc_by_recent_xmin, xc_by_known_xact, xc_by_my_xact, xc_by_latest_xid, xc_by_main_xid, xc_by_child_xid, xc_by_known_assigned, xc_no_overflow, xc_slow_answer);
}
#endif                     

                                                  
                                         
                                                  
   

   
                                                                              
                                                                          
                                                                           
                                      
   
                                                                              
                                                                              
                                                                             
                                                                            
                                                                         
                                          
   
                                                                            
                                                                         
                                                                          
                                                                            
                                                                       
                                                                             
                                                                  
   
                                                                         
                                                                          
                                                                          
                                                                        
                                                                           
                                                                           
                                                                             
                                                                        
                                                                              
                                                                               
                                                                    
   
                                                                               
                                                                            
                                                                              
                                                                              
                                                                               
                                                                               
                                           
   
                                                                              
                                                                             
                                                                         
                                                                             
                                                             
                                                                     
                                
   

   
                                     
                                                                        
                     
   
                                                                            
                                                                          
                                                                       
   
                                                                                
   
void
RecordKnownAssignedTransactionIds(TransactionId xid)
{
  Assert(standbyState >= STANDBY_INITIALIZED);
  Assert(TransactionIdIsValid(xid));
  Assert(TransactionIdIsValid(latestObservedXid));

  elog(trace_recovery(DEBUG4), "record known xact %u latestObservedXid %u", xid, latestObservedXid);

     
                                                                             
                                                                         
                                       
     
  if (TransactionIdFollows(xid, latestObservedXid))
  {
    TransactionId next_expected_xid;

       
                                                                         
                                                                         
                                                           
       
                                                                    
                                                                     
                     
       
    next_expected_xid = latestObservedXid;
    while (TransactionIdPrecedes(next_expected_xid, xid))
    {
      TransactionIdAdvance(next_expected_xid);
      ExtendSUBTRANS(next_expected_xid);
    }
    Assert(next_expected_xid == xid);

       
                                                                        
                                                          
       
    if (standbyState <= STANDBY_INITIALIZED)
    {
      latestObservedXid = xid;
      return;
    }

       
                                                                      
       
    next_expected_xid = latestObservedXid;
    TransactionIdAdvance(next_expected_xid);
    KnownAssignedXidsAdd(next_expected_xid, xid, false);

       
                                            
       
    latestObservedXid = xid;

                                                                         
    AdvanceNextFullTransactionIdPastXid(latestObservedXid);
    next_expected_xid = latestObservedXid;
    TransactionIdAdvance(next_expected_xid);
  }
}

   
                                         
                                                  
   
                                                                                    
   
void
ExpireTreeKnownAssignedTransactionIds(TransactionId xid, int nsubxids, TransactionId *subxids, TransactionId max_xid)
{
  Assert(standbyState >= STANDBY_INITIALIZED);

     
                                             
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  KnownAssignedXidsRemoveTree(xid, nsubxids, subxids);

                                                                 
  if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, max_xid))
  {
    ShmemVariableCache->latestCompletedXid = max_xid;
  }

  LWLockRelease(ProcArrayLock);
}

   
                                        
                                                                         
   
void
ExpireAllKnownAssignedTransactionIds(void)
{
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  KnownAssignedXidsRemovePreceding(InvalidTransactionId);

     
                                                                             
                                                                       
                                                
     
  procArray->lastOverflowedXid = InvalidTransactionId;
  LWLockRelease(ProcArrayLock);
}

   
                                        
                                                                 
                                         
   
void
ExpireOldKnownAssignedTransactionIds(TransactionId xid)
{
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

     
                                                                        
                                                                             
                                                                       
                    
     
  if (TransactionIdPrecedes(procArray->lastOverflowedXid, xid))
  {
    procArray->lastOverflowedXid = InvalidTransactionId;
  }
  KnownAssignedXidsRemovePreceding(xid);
  LWLockRelease(ProcArrayLock);
}

   
                                              
                                                                   
                         
   
void
KnownAssignedTransactionIdsIdleMaintenance(void)
{
  KnownAssignedXidsCompress(KAX_STARTUP_PROCESS_IDLE, false);
}

   
                                                            
   
                                                                  
   
                                                                      
                                                              
                                                    
                                                                
                                                                   
   
                                                                           
                                                                              
                
   
                                                                             
                                                                           
                                                                         
                                                                            
                                                                         
                                                                            
                                                            
   
                                                                            
                                                                            
   
                                                                           
                                                                           
                                                                            
                                                                         
                                                                           
                                                                          
                                                                           
                                        
   
                                                                                
                                                                               
                                                                          
                                                                          
                                                                               
                                                                            
                                                                               
                                                       
   
                                                                           
                                                                            
                                                                             
                                                                             
                                                                           
                                                                          
                                                                        
                                                                               
                                                                              
                                                                            
                                                                         
                                                                          
                                                                         
                                                                           
                                                                            
                                                                             
                                               
   
                         
   
                                                                        
                                               
   
                                                                           
                 
                                                               
                                                            
                                                        
                                                             
   
                                                                           
                                                                      
                                                                       
                                                                               
                                                                            
                                                                        
                                                                              
                                                                          
                                                                        
                                                                   
   

   
                                                                              
                             
   
                                                                       
                                                                     
   
                                                                 
                                                                  
   
static void
KnownAssignedXidsCompress(KAXCompressReason reason, bool haveLock)
{
  ProcArrayStruct *pArray = procArray;
  int head, tail, nelements;
  int compress_index;
  int i;

                                           
  static unsigned int transactionEndsCounter;
  static TimestampTz lastCompressTs;

                        
#define KAX_COMPRESS_FREQUENCY 128                           
#define KAX_COMPRESS_IDLE_INTERVAL 1000            

     
                                                                        
                                          
     
  head = pArray->headKnownAssignedXids;
  tail = pArray->tailKnownAssignedXids;
  nelements = head - tail;

     
                                                                    
                                                                        
                                                                            
                                                               
     
  if (nelements == pArray->numKnownAssignedXids)
  {
       
                                                                     
                                                                           
                                         
       
    if (reason != KAX_NO_SPACE)
    {
      return;
    }
  }
  else if (reason == KAX_TRANSACTION_END)
  {
       
                                                                        
                                 
       
    if ((transactionEndsCounter++) % KAX_COMPRESS_FREQUENCY != 0)
    {
      return;
    }

       
                                                                        
                                           
       
    if (nelements < 2 * pArray->numKnownAssignedXids)
    {
      return;
    }
  }
  else if (reason == KAX_STARTUP_PROCESS_IDLE)
  {
       
                                                                       
                                                                        
                     
       
    if (lastCompressTs != 0)
    {
      TimestampTz compress_after;

      compress_after = TimestampTzPlusMilliseconds(lastCompressTs, KAX_COMPRESS_IDLE_INTERVAL);
      if (GetCurrentTimestamp() < compress_after)
      {
        return;
      }
    }
  }

                                                              
  if (!haveLock)
  {
    LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  }

     
                                                                          
                                      
     
  compress_index = 0;
  for (i = tail; i < head; i++)
  {
    if (KnownAssignedXidsValid[i])
    {
      KnownAssignedXids[compress_index] = KnownAssignedXids[i];
      KnownAssignedXidsValid[compress_index] = true;
      compress_index++;
    }
  }
  Assert(compress_index == pArray->numKnownAssignedXids);

  pArray->tailKnownAssignedXids = 0;
  pArray->headKnownAssignedXids = compress_index;

  if (!haveLock)
  {
    LWLockRelease(ProcArrayLock);
  }

                                                                         
  lastCompressTs = GetCurrentTimestamp();
}

   
                                                             
   
                                                                    
   
                                                                        
                                                                           
                                                                         
                                                                              
                                       
   
static void
KnownAssignedXidsAdd(TransactionId from_xid, TransactionId to_xid, bool exclusive_lock)
{
  ProcArrayStruct *pArray = procArray;
  TransactionId next_xid;
  int head, tail;
  int nxids;
  int i;

  Assert(TransactionIdPrecedesOrEquals(from_xid, to_xid));

     
                                                                            
                                                                             
          
     
  if (to_xid >= from_xid)
  {
    nxids = to_xid - from_xid + 1;
  }
  else
  {
    nxids = 1;
    next_xid = from_xid;
    while (TransactionIdPrecedes(next_xid, to_xid))
    {
      nxids++;
      TransactionIdAdvance(next_xid);
    }
  }

     
                                                                        
                                          
     
  head = pArray->headKnownAssignedXids;
  tail = pArray->tailKnownAssignedXids;

  Assert(head >= 0 && head <= pArray->maxKnownAssignedXids);
  Assert(tail >= 0 && tail < pArray->maxKnownAssignedXids);

     
                                                                             
                                                                          
                                    
     
  if (head > tail && TransactionIdFollowsOrEquals(KnownAssignedXids[head - 1], from_xid))
  {
    KnownAssignedXidsDisplay(LOG);
    elog(ERROR, "out-of-order XID insertion in KnownAssignedXids");
  }

     
                                                                           
     
  if (head + nxids > pArray->maxKnownAssignedXids)
  {
    KnownAssignedXidsCompress(KAX_NO_SPACE, exclusive_lock);

    head = pArray->headKnownAssignedXids;
                                                        

       
                                                      
       
    if (head + nxids > pArray->maxKnownAssignedXids)
    {
      elog(ERROR, "too many KnownAssignedXids");
    }
  }

                                                                  
  next_xid = from_xid;
  for (i = 0; i < nxids; i++)
  {
    KnownAssignedXids[head] = next_xid;
    KnownAssignedXidsValid[head] = true;
    TransactionIdAdvance(next_xid);
    head++;
  }

                                               
  pArray->numKnownAssignedXids += nxids;

     
                                                                     
                                                                        
                                                                          
                                  
     
                                                                             
               
     
  if (exclusive_lock)
  {
    pArray->headKnownAssignedXids = head;
  }
  else
  {
    SpinLockAcquire(&pArray->known_assigned_xids_lck);
    pArray->headKnownAssignedXids = head;
    SpinLockRelease(&pArray->known_assigned_xids_lck);
  }
}

   
                           
   
                                                                            
                                               
   
                                                               
                                                  
   
static bool
KnownAssignedXidsSearch(TransactionId xid, bool remove)
{
  ProcArrayStruct *pArray = procArray;
  int first, last;
  int head;
  int tail;
  int result_index = -1;

  if (remove)
  {
                                                                    
    tail = pArray->tailKnownAssignedXids;
    head = pArray->headKnownAssignedXids;
  }
  else
  {
                                                                  
    SpinLockAcquire(&pArray->known_assigned_xids_lck);
    tail = pArray->tailKnownAssignedXids;
    head = pArray->headKnownAssignedXids;
    SpinLockRelease(&pArray->known_assigned_xids_lck);
  }

     
                                                                            
                                                                      
     
  first = tail;
  last = head - 1;
  while (first <= last)
  {
    int mid_index;
    TransactionId mid_xid;

    mid_index = (first + last) / 2;
    mid_xid = KnownAssignedXids[mid_index];

    if (xid == mid_xid)
    {
      result_index = mid_index;
      break;
    }
    else if (TransactionIdPrecedes(xid, mid_xid))
    {
      last = mid_index - 1;
    }
    else
    {
      first = mid_index + 1;
    }
  }

  if (result_index < 0)
  {
    return false;                   
  }

  if (!KnownAssignedXidsValid[result_index])
  {
    return false;                            
  }

  if (remove)
  {
    KnownAssignedXidsValid[result_index] = false;

    pArray->numKnownAssignedXids--;
    Assert(pArray->numKnownAssignedXids >= 0);

       
                                                                         
                                                               
       
    if (result_index == tail)
    {
      tail++;
      while (tail < head && !KnownAssignedXidsValid[tail])
      {
        tail++;
      }
      if (tail >= head)
      {
                                                           
        pArray->headKnownAssignedXids = 0;
        pArray->tailKnownAssignedXids = 0;
      }
      else
      {
        pArray->tailKnownAssignedXids = tail;
      }
    }
  }

  return true;
}

   
                                                        
   
                                                               
   
static bool
KnownAssignedXidExists(TransactionId xid)
{
  Assert(TransactionIdIsValid(xid));

  return KnownAssignedXidsSearch(xid, false);
}

   
                                                      
   
                                                     
   
static void
KnownAssignedXidsRemove(TransactionId xid)
{
  Assert(TransactionIdIsValid(xid));

  elog(trace_recovery(DEBUG4), "remove KnownAssignedXid %u", xid);

     
                                                                      
                                                                    
                                                                             
                                                              
     
                                                                           
                                                                           
                                        
     
  (void)KnownAssignedXidsSearch(xid, true);
}

   
                               
                                                                       
   
                                                     
   
static void
KnownAssignedXidsRemoveTree(TransactionId xid, int nsubxids, TransactionId *subxids)
{
  int i;

  if (TransactionIdIsValid(xid))
  {
    KnownAssignedXidsRemove(xid);
  }

  for (i = 0; i < nsubxids; i++)
  {
    KnownAssignedXidsRemove(subxids[i]);
  }

                                            
  KnownAssignedXidsCompress(KAX_TRANSACTION_END, true);
}

   
                                                                             
                               
   
                                                     
   
static void
KnownAssignedXidsRemovePreceding(TransactionId removeXid)
{
  ProcArrayStruct *pArray = procArray;
  int count = 0;
  int head, tail, i;

  if (!TransactionIdIsValid(removeXid))
  {
    elog(trace_recovery(DEBUG4), "removing all KnownAssignedXids");
    pArray->numKnownAssignedXids = 0;
    pArray->headKnownAssignedXids = pArray->tailKnownAssignedXids = 0;
    return;
  }

  elog(trace_recovery(DEBUG4), "prune KnownAssignedXids to %u", removeXid);

     
                                                                           
                                                         
     
  tail = pArray->tailKnownAssignedXids;
  head = pArray->headKnownAssignedXids;

  for (i = tail; i < head; i++)
  {
    if (KnownAssignedXidsValid[i])
    {
      TransactionId knownXid = KnownAssignedXids[i];

      if (TransactionIdFollowsOrEquals(knownXid, removeXid))
      {
        break;
      }

      if (!StandbyTransactionIdIsPrepared(knownXid))
      {
        KnownAssignedXidsValid[i] = false;
        count++;
      }
    }
  }

  pArray->numKnownAssignedXids -= count;
  Assert(pArray->numKnownAssignedXids >= 0);

     
                                                                     
     
  for (i = tail; i < head; i++)
  {
    if (KnownAssignedXidsValid[i])
    {
      break;
    }
  }
  if (i >= head)
  {
                                                       
    pArray->headKnownAssignedXids = 0;
    pArray->tailKnownAssignedXids = 0;
  }
  else
  {
    pArray->tailKnownAssignedXids = i;
  }

                                            
  KnownAssignedXidsCompress(KAX_PRUNE, true);
}

   
                                                                              
                                   
   
                                                                           
                               
   
                                                             
   
static int
KnownAssignedXidsGet(TransactionId *xarray, TransactionId xmax)
{
  TransactionId xtmp = InvalidTransactionId;

  return KnownAssignedXidsGetAndSetXmin(xarray, &xtmp, xmax);
}

   
                                                                  
                                                                      
   
                                                             
   
static int
KnownAssignedXidsGetAndSetXmin(TransactionId *xarray, TransactionId *xmin, TransactionId xmax)
{
  int count = 0;
  int head, tail;
  int i;

     
                                                                          
                                                                             
                                                                            
                                                                           
             
     
                                                                    
     
  SpinLockAcquire(&procArray->known_assigned_xids_lck);
  tail = procArray->tailKnownAssignedXids;
  head = procArray->headKnownAssignedXids;
  SpinLockRelease(&procArray->known_assigned_xids_lck);

  for (i = tail; i < head; i++)
  {
                                    
    if (KnownAssignedXidsValid[i])
    {
      TransactionId knownXid = KnownAssignedXids[i];

         
                                                                       
                                    
         
      if (count == 0 && TransactionIdPrecedes(knownXid, *xmin))
      {
        *xmin = knownXid;
      }

         
                                                                       
                   
         
      if (TransactionIdIsValid(xmax) && TransactionIdFollowsOrEquals(knownXid, xmax))
      {
        break;
      }

                                          
      xarray[count++] = knownXid;
    }
  }

  return count;
}

   
                                                                          
                     
   
static TransactionId
KnownAssignedXidsGetOldestXmin(void)
{
  int head, tail;
  int i;

     
                                                              
     
  SpinLockAcquire(&procArray->known_assigned_xids_lck);
  tail = procArray->tailKnownAssignedXids;
  head = procArray->headKnownAssignedXids;
  SpinLockRelease(&procArray->known_assigned_xids_lck);

  for (i = tail; i < head; i++)
  {
                                    
    if (KnownAssignedXidsValid[i])
    {
      return KnownAssignedXids[i];
    }
  }

  return InvalidTransactionId;
}

   
                                                    
   
                                                                       
                    
   
                                                                           
                                                                           
                                                                         
   
static void
KnownAssignedXidsDisplay(int trace_level)
{
  ProcArrayStruct *pArray = procArray;
  StringInfoData buf;
  int head, tail, i;
  int nxids = 0;

  tail = pArray->tailKnownAssignedXids;
  head = pArray->headKnownAssignedXids;

  initStringInfo(&buf);

  for (i = tail; i < head; i++)
  {
    if (KnownAssignedXidsValid[i])
    {
      nxids++;
      appendStringInfo(&buf, "[%d]=%u ", i, KnownAssignedXids[i]);
    }
  }

  elog(trace_level, "%d KnownAssignedXids (num=%d tail=%d head=%d) %s", nxids, pArray->numKnownAssignedXids, pArray->tailKnownAssignedXids, pArray->headKnownAssignedXids, buf.data);

  pfree(buf.data);
}

   
                          
                                         
   
static void
KnownAssignedXidsReset(void)
{
  ProcArrayStruct *pArray = procArray;

  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  pArray->numKnownAssignedXids = 0;
  pArray->tailKnownAssignedXids = 0;
  pArray->headKnownAssignedXids = 0;

  LWLockRelease(ProcArrayLock);
}
