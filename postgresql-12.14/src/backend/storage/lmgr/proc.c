                                                                            
   
          
                                                                 
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
   
                  
                               
                                                                  
                                                              
   
                                                                               
                                                                               
                                                 
   
                  
   
                                                                           
   
                                                            
                                
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "replication/slot.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "storage/condition_variable.h"
#include "storage/standby.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/spin.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"

                   
int DeadlockTimeout = 1000;
int StatementTimeout = 0;
int LockTimeout = 0;
int IdleInTransactionSessionTimeout = 0;
bool log_lock_waits = false;

                                                                 
PGPROC *MyProc = NULL;
PGXACT *MyPgXact = NULL;

   
                                                                      
                                                                         
                                                                           
                                                                             
                                     
   
NON_EXEC_STATIC slock_t *ProcStructLock = NULL;

                                          
PROC_HDR *ProcGlobal = NULL;
NON_EXEC_STATIC PGPROC *AuxiliaryProcs = NULL;
PGPROC *PreparedXactProcs = NULL;

                                                                           
static LOCALLOCK *lockAwaited = NULL;

static DeadLockState deadlock_state = DS_NOT_YET_CHECKED;

                                  
static volatile sig_atomic_t got_deadlock_timeout;

static void
RemoveProcFromArray(int code, Datum arg);
static void
ProcKill(int code, Datum arg);
static void
AuxiliaryProcKill(int code, Datum arg);
static void
CheckDeadLock(void);

   
                                                        
   
Size
ProcGlobalShmemSize(void)
{
  Size size = 0;

                  
  size = add_size(size, sizeof(PROC_HDR));
                                                          
  size = add_size(size, mul_size(MaxBackends, sizeof(PGPROC)));
                      
  size = add_size(size, mul_size(NUM_AUXILIARY_PROCS, sizeof(PGPROC)));
                      
  size = add_size(size, mul_size(max_prepared_xacts, sizeof(PGPROC)));
                      
  size = add_size(size, sizeof(slock_t));

  size = add_size(size, mul_size(MaxBackends, sizeof(PGXACT)));
  size = add_size(size, mul_size(NUM_AUXILIARY_PROCS, sizeof(PGXACT)));
  size = add_size(size, mul_size(max_prepared_xacts, sizeof(PGXACT)));

  return size;
}

   
                                                         
   
int
ProcGlobalSemas(void)
{
     
                                                                          
                        
     
  return MaxBackends + NUM_AUXILIARY_PROCS;
}

   
                    
                                                                         
                      
   
                                                                           
                                                                       
                                                                          
                                                                    
                                                                        
                                                                           
                                                                         
                                                                           
                                                               
                                                                       
                                        
   
                                                                       
                                                                      
                                  
   
                                                                       
                                                                         
                                                                     
   
void
InitProcGlobal(void)
{
  PGPROC *procs;
  PGXACT *pgxacts;
  int i, j;
  bool found;
  uint32 TotalProcs = MaxBackends + NUM_AUXILIARY_PROCS + max_prepared_xacts;

                                              
  ProcGlobal = (PROC_HDR *)ShmemInitStruct("Proc Header", sizeof(PROC_HDR), &found);
  Assert(!found);

     
                                     
     
  ProcGlobal->spins_per_delay = DEFAULT_SPINS_PER_DELAY;
  ProcGlobal->freeProcs = NULL;
  ProcGlobal->autovacFreeProcs = NULL;
  ProcGlobal->bgworkerFreeProcs = NULL;
  ProcGlobal->walsenderFreeProcs = NULL;
  ProcGlobal->startupProc = NULL;
  ProcGlobal->startupProcPid = 0;
  ProcGlobal->startupBufferPinWaitBufId = -1;
  ProcGlobal->walwriterLatch = NULL;
  ProcGlobal->checkpointerLatch = NULL;
  pg_atomic_init_u32(&ProcGlobal->procArrayGroupFirst, INVALID_PGPROCNO);
  pg_atomic_init_u32(&ProcGlobal->clogGroupFirst, INVALID_PGPROCNO);

     
                                                                            
                                                                          
                                                                        
                                                                         
                                                                      
                     
     
  procs = (PGPROC *)ShmemAlloc(TotalProcs * sizeof(PGPROC));
  MemSet(procs, 0, TotalProcs * sizeof(PGPROC));
  ProcGlobal->allProcs = procs;
                                                                             
  ProcGlobal->allProcCount = MaxBackends + NUM_AUXILIARY_PROCS;

     
                                                                            
                                                                          
                                                                           
                                                                
                                                                            
                
     
  pgxacts = (PGXACT *)ShmemAlloc(TotalProcs * sizeof(PGXACT));
  MemSet(pgxacts, 0, TotalProcs * sizeof(PGXACT));
  ProcGlobal->allPgXact = pgxacts;

  for (i = 0; i < TotalProcs; i++)
  {
                                                                    

       
                                                                          
                                                                        
                           
       
    if (i < MaxBackends + NUM_AUXILIARY_PROCS)
    {
      procs[i].sem = PGSemaphoreCreate();
      InitSharedLatch(&(procs[i].procLatch));
      LWLockInitialize(&(procs[i].backendLock), LWTRANCHE_PROC);
    }
    procs[i].pgprocno = i;

       
                                                                           
                                                                          
                                                                          
                                                                        
                                                                         
                                         
       
    if (i < MaxConnections)
    {
                                                            
      procs[i].links.next = (SHM_QUEUE *)ProcGlobal->freeProcs;
      ProcGlobal->freeProcs = &procs[i];
      procs[i].procgloballist = &ProcGlobal->freeProcs;
    }
    else if (i < MaxConnections + autovacuum_max_workers + 1)
    {
                                                                       
      procs[i].links.next = (SHM_QUEUE *)ProcGlobal->autovacFreeProcs;
      ProcGlobal->autovacFreeProcs = &procs[i];
      procs[i].procgloballist = &ProcGlobal->autovacFreeProcs;
    }
    else if (i < MaxConnections + autovacuum_max_workers + 1 + max_worker_processes)
    {
                                                              
      procs[i].links.next = (SHM_QUEUE *)ProcGlobal->bgworkerFreeProcs;
      ProcGlobal->bgworkerFreeProcs = &procs[i];
      procs[i].procgloballist = &ProcGlobal->bgworkerFreeProcs;
    }
    else if (i < MaxBackends)
    {
                                                                
      procs[i].links.next = (SHM_QUEUE *)ProcGlobal->walsenderFreeProcs;
      ProcGlobal->walsenderFreeProcs = &procs[i];
      procs[i].procgloballist = &ProcGlobal->walsenderFreeProcs;
    }

                                                        
    for (j = 0; j < NUM_LOCK_PARTITIONS; j++)
    {
      SHMQueueInit(&(procs[i].myProcLocks[j]));
    }

                                           
    dlist_init(&procs[i].lockGroupMembers);

       
                                                                       
                                                              
       
    pg_atomic_init_u32(&(procs[i].procArrayGroupNext), INVALID_PGPROCNO);
    pg_atomic_init_u32(&(procs[i].clogGroupNext), INVALID_PGPROCNO);
  }

     
                                                                             
                                          
     
  AuxiliaryProcs = &procs[MaxBackends];
  PreparedXactProcs = &procs[MaxBackends + NUM_AUXILIARY_PROCS];

                                           
  ProcStructLock = (slock_t *)ShmemAlloc(sizeof(slock_t));
  SpinLockInit(ProcStructLock);
}

   
                                                                           
   
void
InitProcess(void)
{
  PGPROC *volatile *procgloballist;

     
                                                                          
                                                                    
     
  if (ProcGlobal == NULL)
  {
    elog(PANIC, "proc header uninitialized");
  }

  if (MyProc != NULL)
  {
    elog(ERROR, "you already exist");
  }

                                                   
  if (IsAnyAutoVacuumProcess())
  {
    procgloballist = &ProcGlobal->autovacFreeProcs;
  }
  else if (IsBackgroundWorker)
  {
    procgloballist = &ProcGlobal->bgworkerFreeProcs;
  }
  else if (am_walsender)
  {
    procgloballist = &ProcGlobal->walsenderFreeProcs;
  }
  else
  {
    procgloballist = &ProcGlobal->freeProcs;
  }

     
                                                                       
                                                                             
     
                                                                           
                                                   
     
  SpinLockAcquire(ProcStructLock);

  set_spins_per_delay(ProcGlobal->spins_per_delay);

  MyProc = *procgloballist;

  if (MyProc != NULL)
  {
    *procgloballist = (PGPROC *)MyProc->links.next;
    SpinLockRelease(ProcStructLock);
  }
  else
  {
       
                                                                         
                                                                           
                                                                          
                               
       
    SpinLockRelease(ProcStructLock);
    if (am_walsender)
    {
      ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("number of requested standby connections exceeds max_wal_senders (currently %d)", max_wal_senders)));
    }
    ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("sorry, too many clients already")));
  }
  MyPgXact = &ProcGlobal->allPgXact[MyProc->pgprocno];

     
                                                                            
                                                        
     
  Assert(MyProc->procgloballist == procgloballist);

     
                                                                       
                                                                            
                                                                          
                                
     
  if (IsUnderPostmaster && !IsAutoVacuumLauncherProcess())
  {
    MarkPostmasterChildActive();
  }

     
                                                                  
                                    
     
  SHMQueueElemInit(&(MyProc->links));
  MyProc->waitStatus = STATUS_OK;
  MyProc->lxid = InvalidLocalTransactionId;
  MyProc->fpVXIDLock = false;
  MyProc->fpLocalTransactionId = InvalidLocalTransactionId;
  MyPgXact->xid = InvalidTransactionId;
  MyPgXact->xmin = InvalidTransactionId;
  MyProc->pid = MyProcPid;
                                                                
  MyProc->backendId = InvalidBackendId;
  MyProc->databaseId = InvalidOid;
  MyProc->roleId = InvalidOid;
  MyProc->tempNamespaceId = InvalidOid;
  MyProc->isBackgroundWorker = IsBackgroundWorker;
  MyPgXact->delayChkpt = 0;
  MyPgXact->vacuumFlags = 0;
                                                                       
  if (IsAutoVacuumWorkerProcess())
  {
    MyPgXact->vacuumFlags |= PROC_IS_AUTOVACUUM;
  }
  MyProc->lwWaiting = false;
  MyProc->lwWaitMode = 0;
  MyProc->waitLock = NULL;
  MyProc->waitProcLock = NULL;
#ifdef USE_ASSERT_CHECKING
  {
    int i;

                                                      
    for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
    {
      Assert(SHMQueueEmpty(&(MyProc->myProcLocks[i])));
    }
  }
#endif
  MyProc->recoveryConflictPending = false;

                                      
  MyProc->waitLSN = 0;
  MyProc->syncRepState = SYNC_REP_NOT_WAITING;
  SHMQueueElemInit(&(MyProc->syncRepLinks));

                                                 
  MyProc->procArrayGroupMember = false;
  MyProc->procArrayGroupMemberXid = InvalidTransactionId;
  Assert(pg_atomic_read_u32(&MyProc->procArrayGroupNext) == INVALID_PGPROCNO);

                                                                      
  Assert(MyProc->lockGroupLeader == NULL);
  Assert(dlist_is_empty(&MyProc->lockGroupMembers));

                                          
  MyProc->wait_event_info = 0;

                                                              
  MyProc->clogGroupMember = false;
  MyProc->clogGroupMemberXid = InvalidTransactionId;
  MyProc->clogGroupMemberXidStatus = TRANSACTION_STATUS_IN_PROGRESS;
  MyProc->clogGroupMemberPage = -1;
  MyProc->clogGroupMemberLsn = InvalidXLogRecPtr;
  Assert(pg_atomic_read_u32(&MyProc->clogGroupNext) == INVALID_PGPROCNO);

     
                                                                           
                                                                       
                                                     
     
  OwnLatch(&MyProc->procLatch);
  SwitchToSharedLatch();

     
                                                                           
                                                                        
                                                                     
     
  PGSemaphoreReset(MyProc->sem);

     
                                          
     
  on_shmem_exit(ProcKill, 0);

     
                                                                             
                                                               
     
  InitLWLockAccess();
  InitDeadLockChecking();
}

   
                                                                     
   
                                                                            
                                                                           
                                                                
   
void
InitProcessPhase2(void)
{
  Assert(MyProc != NULL);

     
                                                          
     
  ProcArrayAdd(MyProc);

     
                                               
     
  on_shmem_exit(RemoveProcFromArray, 0);
}

   
                                                                         
   
                                                                             
                                                                             
                                                                       
                   
   
                                                                             
                                                                            
                                                                          
                                                                         
                       
   
                                                                      
                                                                             
                                                                        
                                                                  
   
void
InitAuxiliaryProcess(void)
{
  PGPROC *auxproc;
  int proctype;

     
                                                                          
                                                                    
     
  if (ProcGlobal == NULL || AuxiliaryProcs == NULL)
  {
    elog(PANIC, "proc header uninitialized");
  }

  if (MyProc != NULL)
  {
    elog(ERROR, "you already exist");
  }

     
                                                                      
                             
     
                                                                           
                                                   
     
  SpinLockAcquire(ProcStructLock);

  set_spins_per_delay(ProcGlobal->spins_per_delay);

     
                                                                  
     
  for (proctype = 0; proctype < NUM_AUXILIARY_PROCS; proctype++)
  {
    auxproc = &AuxiliaryProcs[proctype];
    if (auxproc->pid == 0)
    {
      break;
    }
  }
  if (proctype >= NUM_AUXILIARY_PROCS)
  {
    SpinLockRelease(ProcStructLock);
    elog(FATAL, "all AuxiliaryProcs are in use");
  }

                                           
                                                          
  ((volatile PGPROC *)auxproc)->pid = MyProcPid;

  MyProc = auxproc;
  MyPgXact = &ProcGlobal->allPgXact[auxproc->pgprocno];

  SpinLockRelease(ProcStructLock);

     
                                                                  
                                    
     
  SHMQueueElemInit(&(MyProc->links));
  MyProc->waitStatus = STATUS_OK;
  MyProc->lxid = InvalidLocalTransactionId;
  MyProc->fpVXIDLock = false;
  MyProc->fpLocalTransactionId = InvalidLocalTransactionId;
  MyPgXact->xid = InvalidTransactionId;
  MyPgXact->xmin = InvalidTransactionId;
  MyProc->backendId = InvalidBackendId;
  MyProc->databaseId = InvalidOid;
  MyProc->roleId = InvalidOid;
  MyProc->tempNamespaceId = InvalidOid;
  MyProc->isBackgroundWorker = IsBackgroundWorker;
  MyPgXact->delayChkpt = 0;
  MyPgXact->vacuumFlags = 0;
  MyProc->lwWaiting = false;
  MyProc->lwWaitMode = 0;
  MyProc->waitLock = NULL;
  MyProc->waitProcLock = NULL;
#ifdef USE_ASSERT_CHECKING
  {
    int i;

                                                      
    for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
    {
      Assert(SHMQueueEmpty(&(MyProc->myProcLocks[i])));
    }
  }
#endif

     
                                                                           
                                                                       
                                                     
     
  OwnLatch(&MyProc->procLatch);
  SwitchToSharedLatch();

                                                                      
  Assert(MyProc->lockGroupLeader == NULL);
  Assert(dlist_is_empty(&MyProc->lockGroupMembers));

     
                                                                           
                                                                        
                                                                     
     
  PGSemaphoreReset(MyProc->sem);

     
                                          
     
  on_shmem_exit(AuxiliaryProcKill, Int32GetDatum(proctype));
}

   
                                                                            
                                                                  
   
void
PublishStartupProcessInformation(void)
{
  SpinLockAcquire(ProcStructLock);

  ProcGlobal->startupProc = MyProc;
  ProcGlobal->startupProcPid = MyProcPid;

  SpinLockRelease(ProcStructLock);
}

   
                                                                           
                                                                       
                                                                           
                                                                       
                                    
   
void
SetStartupBufferPinWaitBufId(int bufid)
{
                                                          
  volatile PROC_HDR *procglobal = ProcGlobal;

  procglobal->startupBufferPinWaitBufId = bufid;
}

   
                                                                               
   
int
GetStartupBufferPinWaitBufId(void)
{
                                                          
  volatile PROC_HDR *procglobal = ProcGlobal;

  return procglobal->startupBufferPinWaitBufId;
}

   
                                                           
   
                                                                            
   
bool
HaveNFreeProcs(int n)
{
  PGPROC *proc;

  SpinLockAcquire(ProcStructLock);

  proc = ProcGlobal->freeProcs;

  while (n > 0 && proc != NULL)
  {
    proc = (PGPROC *)proc->links.next;
    n--;
  }

  SpinLockRelease(ProcStructLock);

  return (n <= 0);
}

   
                                                    
   
bool
IsWaitingForLock(void)
{
  if (lockAwaited == NULL)
  {
    return false;
  }

  return true;
}

   
                                                                             
                                                                
   
                                                               
                                                                            
                                                  
   
void
LockErrorCleanup(void)
{
  LWLock *partitionLock;
  DisableTimeoutParams timeouts[2];

  HOLD_INTERRUPTS();

  AbortStrongLockAcquire();

                                                      
  if (lockAwaited == NULL)
  {
    RESUME_INTERRUPTS();
    return;
  }

     
                                                                      
                                                                      
                                                            
                                                                     
                                                                            
             
     
  timeouts[0].id = DEADLOCK_TIMEOUT;
  timeouts[0].keep_indicator = false;
  timeouts[1].id = LOCK_TIMEOUT;
  timeouts[1].keep_indicator = true;
  disable_timeouts(timeouts, 2);

                                                                           
  partitionLock = LockHashPartitionLock(lockAwaited->hashcode);
  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

  if (MyProc->links.next != NULL)
  {
                                                     
    RemoveFromWaitQueue(MyProc, lockAwaited->hashcode);
  }
  else
  {
       
                                                                    
                                                                         
                                                                        
              
       
    if (MyProc->waitStatus == STATUS_OK)
    {
      GrantAwaitedLock();
    }
  }

  lockAwaited = NULL;

  LWLockRelease(partitionLock);

  RESUME_INTERRUPTS();
}

   
                                                                           
                                         
   
                                                                               
                                                                            
   
                                                                   
                                                                
   
                                                                             
                                                                          
                                                                             
                                                                         
                                
   
void
ProcReleaseLocks(bool isCommit)
{
  if (!MyProc)
  {
    return;
  }
                                                                          
  LockErrorCleanup();
                                                                   
  LockReleaseAll(DEFAULT_LOCKMETHOD, !isCommit);
                                                
  LockReleaseAll(USER_LOCKMETHOD, false);
}

   
                                                                           
   
static void
RemoveProcFromArray(int code, Datum arg)
{
  Assert(MyProc != NULL);
  ProcArrayRemove(MyProc, InvalidTransactionId);
}

   
                                                         
                                                    
   
static void
ProcKill(int code, Datum arg)
{
  PGPROC *proc;
  PGPROC *volatile *procgloballist;

  Assert(MyProc != NULL);

                                                 
  SyncRepCleanupAtProcExit();

#ifdef USE_ASSERT_CHECKING
  {
    int i;

                                                      
    for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
    {
      Assert(SHMQueueEmpty(&(MyProc->myProcLocks[i])));
    }
  }
#endif

     
                                                                            
                                                                      
                                          
     
  LWLockReleaseAll();

                                                        
  ConditionVariableCancelSleep();

                                                       
  if (MyReplicationSlot != NULL)
  {
    ReplicationSlotRelease();
  }

                                             
  ReplicationSlotCleanup();

     
                                                                         
                                                                             
                                                                      
                                              
     
  if (MyProc->lockGroupLeader != NULL)
  {
    PGPROC *leader = MyProc->lockGroupLeader;
    LWLock *leader_lwlock = LockHashPartitionLockByProc(leader);

    LWLockAcquire(leader_lwlock, LW_EXCLUSIVE);
    Assert(!dlist_is_empty(&leader->lockGroupMembers));
    dlist_delete(&MyProc->lockGroupLink);
    if (dlist_is_empty(&leader->lockGroupMembers))
    {
      leader->lockGroupLeader = NULL;
      if (leader != MyProc)
      {
        procgloballist = leader->procgloballist;

                                                     
        SpinLockAcquire(ProcStructLock);
        leader->links.next = (SHM_QUEUE *)*procgloballist;
        *procgloballist = leader;
        SpinLockRelease(ProcStructLock);
      }
    }
    else if (leader != MyProc)
    {
      MyProc->lockGroupLeader = NULL;
    }
    LWLockRelease(leader_lwlock);
  }

     
                                                                     
                                                                        
                                                                       
            
     
  SwitchBackToLocalLatch();
  proc = MyProc;
  MyProc = NULL;
  DisownLatch(&proc->procLatch);

  procgloballist = proc->procgloballist;
  SpinLockAcquire(ProcStructLock);

     
                                                                           
                                                                             
                                                          
     
  if (proc->lockGroupLeader == NULL)
  {
                                                                          
    Assert(dlist_is_empty(&proc->lockGroupMembers));

                                                                         
    proc->links.next = (SHM_QUEUE *)*procgloballist;
    *procgloballist = proc;
  }

                                                 
  ProcGlobal->spins_per_delay = update_spins_per_delay(ProcGlobal->spins_per_delay);

  SpinLockRelease(ProcStructLock);

     
                                                                          
                                                                        
                                                       
     
  if (IsUnderPostmaster && !IsAutoVacuumLauncherProcess())
  {
    MarkPostmasterChildInactive();
  }

                                                                         
  if (AutovacuumLauncherPid != 0)
  {
    kill(AutovacuumLauncherPid, SIGUSR2);
  }
}

   
                                                                     
                                                                           
                          
   
static void
AuxiliaryProcKill(int code, Datum arg)
{
  int proctype = DatumGetInt32(arg);
  PGPROC *auxproc PG_USED_FOR_ASSERTS_ONLY;
  PGPROC *proc;

  Assert(proctype >= 0 && proctype < NUM_AUXILIARY_PROCS);

  auxproc = &AuxiliaryProcs[proctype];

  Assert(MyProc == auxproc);

                                                           
  LWLockReleaseAll();

                                                        
  ConditionVariableCancelSleep();

     
                                                                     
                                                                        
                                                                       
            
     
  SwitchBackToLocalLatch();
  proc = MyProc;
  MyProc = NULL;
  DisownLatch(&proc->procLatch);

  SpinLockAcquire(ProcStructLock);

                                            
  proc->pid = 0;

                                                 
  ProcGlobal->spins_per_delay = update_spins_per_delay(ProcGlobal->spins_per_delay);

  SpinLockRelease(ProcStructLock);
}

   
                                                              
                 
   
                              
   
PGPROC *
AuxiliaryPidGetProc(int pid)
{
  PGPROC *result = NULL;
  int index;

  if (pid == 0)                                
  {
    return NULL;
  }

  for (index = 0; index < NUM_AUXILIARY_PROCS; index++)
  {
    PGPROC *proc = &AuxiliaryProcs[index];

    if (proc->pid == pid)
    {
      result = proc;
      break;
    }
  }
  return result;
}

   
                                                              
                        
   

   
                                                                   
   
                                   
                                                                 
   
#ifdef NOT_USED
PROC_QUEUE *
ProcQueueAlloc(const char *name)
{
  PROC_QUEUE *queue;
  bool found;

  queue = (PROC_QUEUE *)ShmemInitStruct(name, sizeof(PROC_QUEUE), &found);

  if (!found)
  {
    ProcQueueInit(queue);
  }

  return queue;
}
#endif

   
                                                             
   
void
ProcQueueInit(PROC_QUEUE *queue)
{
  SHMQueueInit(&(queue->links));
  queue->size = 0;
}

   
                                                             
   
                                                                        
                                                            
   
                                                                           
            
   
                                                                              
   
                                                              
                                   
   
                                                                 
   
int
ProcSleep(LOCALLOCK *locallock, LockMethod lockMethodTable)
{
  LOCKMODE lockmode = locallock->tag.mode;
  LOCK *lock = locallock->lock;
  PROCLOCK *proclock = locallock->proclock;
  uint32 hashcode = locallock->hashcode;
  LWLock *partitionLock = LockHashPartitionLock(hashcode);
  PROC_QUEUE *waitQueue = &(lock->waitProcs);
  SHM_QUEUE *waitQueuePos;
  LOCKMASK myHeldLocks = MyProc->heldLocks;
  bool early_deadlock = false;
  bool allow_autovacuum_cancel = true;
  int myWaitStatus;
  PGPROC *leader = MyProc->lockGroupLeader;
  int i;

     
                                                                           
                                         
     
  if (leader != NULL)
  {
    SHM_QUEUE *procLocks = &(lock->procLocks);
    PROCLOCK *otherproclock;

    otherproclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, lockLink));
    while (otherproclock != NULL)
    {
      if (otherproclock->groupLeader == leader)
      {
        myHeldLocks |= otherproclock->holdMask;
      }
      otherproclock = (PROCLOCK *)SHMQueueNext(procLocks, &otherproclock->lockLink, offsetof(PROCLOCK, lockLink));
    }
  }

     
                                                      
     
                                                                          
                                                                           
                                                                             
                                                                             
                                                                        
                                                            
     
                                                                           
                                                                           
                                                                             
                                                                             
                                                                            
            
     
  if (myHeldLocks != 0 && waitQueue->size > 0)
  {
    LOCKMASK aheadRequests = 0;
    SHM_QUEUE *proc_node;

    proc_node = waitQueue->links.next;
    for (i = 0; i < waitQueue->size; i++)
    {
      PGPROC *proc = (PGPROC *)proc_node;

         
                                                                     
                                                            
                        
         
      if (leader != NULL && leader == proc->lockGroupLeader)
      {
        proc_node = proc->links.next;
        continue;
      }
                                
      if (lockMethodTable->conflictTab[proc->waitLockMode] & myHeldLocks)
      {
                                   
        if (lockMethodTable->conflictTab[lockmode] & proc->heldLocks)
        {
             
                                                                  
                                                                
                                                                     
                                                                  
                                                     
             
          RememberSimpleDeadLock(MyProc, lockmode, lock, proc);
          early_deadlock = true;
          break;
        }
                                                                
        if ((lockMethodTable->conflictTab[lockmode] & aheadRequests) == 0 && LockCheckConflicts(lockMethodTable, lockmode, lock, proclock) == STATUS_OK)
        {
                                                             
          GrantLock(lock, proclock, lockmode);
          GrantAwaitedLock();
          return STATUS_OK;
        }
                                                        
        break;
      }
                                           
      aheadRequests |= LOCKBIT_ON(proc->waitLockMode);
      proc_node = proc->links.next;
    }

       
                                                                           
                                                            
       
    waitQueuePos = proc_node;
  }
  else
  {
                                                              
    waitQueuePos = &waitQueue->links;
  }

     
                                                               
     
  SHMQueueInsertBefore(waitQueuePos, &MyProc->links);
  waitQueue->size++;

  lock->waitMask |= LOCKBIT_ON(lockmode);

                                                     
  MyProc->waitLock = lock;
  MyProc->waitProcLock = proclock;
  MyProc->waitLockMode = lockmode;

  MyProc->waitStatus = STATUS_WAITING;

     
                                                                             
                                    
     
  if (early_deadlock)
  {
    RemoveFromWaitQueue(MyProc, hashcode);
    return STATUS_ERROR;
  }

                                           
  lockAwaited = locallock;

     
                                              
     
                                                                           
                                                                           
                                                                   
                                                           
     
  LWLockRelease(partitionLock);

     
                                                                         
                                                                       
                                                                           
                                                        
     
  if (RecoveryInProgress() && !InRecovery)
  {
    CheckRecoveryConflictDeadlock();
  }

                                                                
  deadlock_state = DS_NOT_YET_CHECKED;
  got_deadlock_timeout = false;

     
                                                                             
                                                                 
                                                                          
                   
     
                                                                      
                                                                     
     
                                                                             
                                                              
     
                                                                             
           
     
  if (!InHotStandby)
  {
    if (LockTimeout > 0)
    {
      EnableTimeoutParams timeouts[2];

      timeouts[0].id = DEADLOCK_TIMEOUT;
      timeouts[0].type = TMPARAM_AFTER;
      timeouts[0].delay_ms = DeadlockTimeout;
      timeouts[1].id = LOCK_TIMEOUT;
      timeouts[1].type = TMPARAM_AFTER;
      timeouts[1].delay_ms = LockTimeout;
      enable_timeouts(timeouts, 2);
    }
    else
    {
      enable_timeout_after(DEADLOCK_TIMEOUT, DeadlockTimeout);
    }
  }

     
                                                                         
                                                                            
                                                                      
                                  
     
                                                                          
                                                                          
                                                                           
                                                                      
                                                                            
                                                                            
                                               
     
  do
  {
    if (InHotStandby)
    {
                                                                       
      ResolveRecoveryConflictWithLock(locallock->tag.lock);
    }
    else
    {
      (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, PG_WAIT_LOCK | locallock->tag.lock.locktag_type);
      ResetLatch(MyLatch);
                                                                    
      if (got_deadlock_timeout)
      {
        CheckDeadLock();
        got_deadlock_timeout = false;
      }
      CHECK_FOR_INTERRUPTS();
    }

       
                                                                     
                                                                         
                                                
       
    myWaitStatus = *((volatile int *)&MyProc->waitStatus);

       
                                                                          
                                            
       
    if (deadlock_state == DS_BLOCKED_BY_AUTOVACUUM && allow_autovacuum_cancel)
    {
      PGPROC *autovac = GetBlockingAutoVacuumPgproc();
      PGXACT *autovac_pgxact = &ProcGlobal->allPgXact[autovac->pgprocno];

      LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

         
                                                                        
                     
         
      if ((autovac_pgxact->vacuumFlags & PROC_IS_AUTOVACUUM) && !(autovac_pgxact->vacuumFlags & PROC_VACUUM_FOR_WRAPAROUND))
      {
        int pid = autovac->pid;
        StringInfoData locktagbuf;
        StringInfoData logbuf;                               

        initStringInfo(&locktagbuf);
        initStringInfo(&logbuf);
        DescribeLockTag(&locktagbuf, &lock->tag);
        appendStringInfo(&logbuf, _("Process %d waits for %s on %s."), MyProcPid, GetLockmodeName(lock->tag.locktag_lockmethodid, lockmode), locktagbuf.data);

                                                 
        LWLockRelease(ProcArrayLock);

                                                              
        ereport(DEBUG1, (errmsg("sending cancel to blocking autovacuum PID %d", pid), errdetail_log("%s", logbuf.data)));

        if (kill(pid, SIGINT) < 0)
        {
             
                                                                
                                                                    
                                                                 
                                                                
                                                                    
                                                                     
                                                                     
                                                   
             
          if (errno != ESRCH)
          {
            ereport(WARNING, (errmsg("could not send signal to process %d: %m", pid)));
          }
        }

        pfree(logbuf.data);
        pfree(locktagbuf.data);
      }
      else
      {
        LWLockRelease(ProcArrayLock);
      }

                                                               
      allow_autovacuum_cancel = false;
    }

       
                                                                 
                                                         
       
    if (log_lock_waits && deadlock_state != DS_NOT_YET_CHECKED)
    {
      StringInfoData buf, lock_waiters_sbuf, lock_holders_sbuf;
      const char *modename;
      long secs;
      int usecs;
      long msecs;
      SHM_QUEUE *procLocks;
      PROCLOCK *proclock;
      bool first_holder = true, first_waiter = true;
      int lockHoldersNum = 0;

      initStringInfo(&buf);
      initStringInfo(&lock_waiters_sbuf);
      initStringInfo(&lock_holders_sbuf);

      DescribeLockTag(&buf, &locallock->tag.lock);
      modename = GetLockmodeName(locallock->tag.lock.locktag_lockmethodid, lockmode);
      TimestampDifference(get_timeout_start_time(DEADLOCK_TIMEOUT), GetCurrentTimestamp(), &secs, &usecs);
      msecs = secs * 1000 + usecs / 1000;
      usecs = usecs % 1000;

         
                                                                   
                                                                   
                                                           
         
                                                                       
                    
         

      LWLockAcquire(partitionLock, LW_SHARED);

      procLocks = &(lock->procLocks);
      proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, lockLink));

      while (proclock)
      {
           
                                                                       
                                                         
           
        if (proclock->tag.myProc->waitProcLock == proclock)
        {
          if (first_waiter)
          {
            appendStringInfo(&lock_waiters_sbuf, "%d", proclock->tag.myProc->pid);
            first_waiter = false;
          }
          else
          {
            appendStringInfo(&lock_waiters_sbuf, ", %d", proclock->tag.myProc->pid);
          }
        }
        else
        {
          if (first_holder)
          {
            appendStringInfo(&lock_holders_sbuf, "%d", proclock->tag.myProc->pid);
            first_holder = false;
          }
          else
          {
            appendStringInfo(&lock_holders_sbuf, ", %d", proclock->tag.myProc->pid);
          }

          lockHoldersNum++;
        }

        proclock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->lockLink, offsetof(PROCLOCK, lockLink));
      }

      LWLockRelease(partitionLock);

      if (deadlock_state == DS_SOFT_DEADLOCK)
      {
        ereport(LOG, (errmsg("process %d avoided deadlock for %s on %s by rearranging queue order after %ld.%03d ms", MyProcPid, modename, buf.data, msecs, usecs), (errdetail_log_plural("Process holding the lock: %s. Wait queue: %s.", "Processes holding the lock: %s. Wait queue: %s.", lockHoldersNum, lock_holders_sbuf.data, lock_waiters_sbuf.data))));
      }
      else if (deadlock_state == DS_HARD_DEADLOCK)
      {
           
                                                                       
                                                                     
                                                                  
                                                                   
                              
           
        ereport(LOG, (errmsg("process %d detected deadlock while waiting for %s on %s after %ld.%03d ms", MyProcPid, modename, buf.data, msecs, usecs), (errdetail_log_plural("Process holding the lock: %s. Wait queue: %s.", "Processes holding the lock: %s. Wait queue: %s.", lockHoldersNum, lock_holders_sbuf.data, lock_waiters_sbuf.data))));
      }

      if (myWaitStatus == STATUS_WAITING)
      {
        ereport(LOG, (errmsg("process %d still waiting for %s on %s after %ld.%03d ms", MyProcPid, modename, buf.data, msecs, usecs), (errdetail_log_plural("Process holding the lock: %s. Wait queue: %s.", "Processes holding the lock: %s. Wait queue: %s.", lockHoldersNum, lock_holders_sbuf.data, lock_waiters_sbuf.data))));
      }
      else if (myWaitStatus == STATUS_OK)
      {
        ereport(LOG, (errmsg("process %d acquired %s on %s after %ld.%03d ms", MyProcPid, modename, buf.data, msecs, usecs)));
      }
      else
      {
        Assert(myWaitStatus == STATUS_ERROR);

           
                                                                
                                                                      
                                                                      
                                                               
                                                                     
                                        
           
        if (deadlock_state != DS_HARD_DEADLOCK)
        {
          ereport(LOG, (errmsg("process %d failed to acquire %s on %s after %ld.%03d ms", MyProcPid, modename, buf.data, msecs, usecs), (errdetail_log_plural("Process holding the lock: %s. Wait queue: %s.", "Processes holding the lock: %s. Wait queue: %s.", lockHoldersNum, lock_holders_sbuf.data, lock_waiters_sbuf.data))));
        }
      }

         
                                                                       
                                                           
         
      deadlock_state = DS_NO_DEADLOCK;

      pfree(buf.data);
      pfree(lock_holders_sbuf.data);
      pfree(lock_waiters_sbuf.data);
    }
  } while (myWaitStatus == STATUS_WAITING);

     
                                                                             
                                                                             
                                                                            
                                                       
     
  if (!InHotStandby)
  {
    if (LockTimeout > 0)
    {
      DisableTimeoutParams timeouts[2];

      timeouts[0].id = DEADLOCK_TIMEOUT;
      timeouts[0].keep_indicator = false;
      timeouts[1].id = LOCK_TIMEOUT;
      timeouts[1].keep_indicator = true;
      disable_timeouts(timeouts, 2);
    }
    else
    {
      disable_timeout(DEADLOCK_TIMEOUT, false);
    }
  }

     
                                                                             
                                                                            
                                                          
     
  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

     
                                                        
     
  lockAwaited = NULL;

     
                                                                        
     
  if (MyProc->waitStatus == STATUS_OK)
  {
    GrantAwaitedLock();
  }

     
                                                                       
                                                    
     
  return MyProc->waitStatus;
}

   
                                                         
   
                                                                           
                                                
   
                                                               
   
                                                                           
                                                                           
                                                                         
                                                                  
   
PGPROC *
ProcWakeup(PGPROC *proc, int waitStatus)
{
  PGPROC *retProc;

                                   
  if (proc->links.prev == NULL || proc->links.next == NULL)
  {
    return NULL;
  }
  Assert(proc->waitStatus == STATUS_WAITING);

                                                     
  retProc = (PGPROC *)proc->links.next;

                                      
  SHMQueueDelete(&(proc->links));
  (proc->waitLock->waitProcs.size)--;

                                                              
  proc->waitLock = NULL;
  proc->waitProcLock = NULL;
  proc->waitStatus = waitStatus;

                     
  SetLatch(&proc->procLatch);

  return retProc;
}

   
                                                                    
                                                               
                                                    
   
                                                               
   
void
ProcLockWakeup(LockMethod lockMethodTable, LOCK *lock)
{
  PROC_QUEUE *waitQueue = &(lock->waitProcs);
  int queue_size = waitQueue->size;
  PGPROC *proc;
  LOCKMASK aheadRequests = 0;

  Assert(queue_size >= 0);

  if (queue_size == 0)
  {
    return;
  }

  proc = (PGPROC *)waitQueue->links.next;

  while (queue_size-- > 0)
  {
    LOCKMODE lockmode = proc->waitLockMode;

       
                                                                           
                                                     
       
    if ((lockMethodTable->conflictTab[lockmode] & aheadRequests) == 0 && LockCheckConflicts(lockMethodTable, lockmode, lock, proc->waitProcLock) == STATUS_OK)
    {
                       
      GrantLock(lock, proc->waitProcLock, lockmode);
      proc = ProcWakeup(proc, STATUS_OK);

         
                                                                       
                                                                         
                                    
         
    }
    else
    {
         
                                                                      
         
      aheadRequests |= LOCKBIT_ON(lockmode);
      proc = (PGPROC *)proc->links.next;
    }
  }

  Assert(waitQueue->size >= 0);
}

   
                 
   
                                                                              
                                                                               
                                                                 
                                                                               
                                                           
   
static void
CheckDeadLock(void)
{
  int i;

     
                                                                            
                                                                      
     
                                                                      
                                                                             
                                                                         
                                                                       
                 
     
  for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
  {
    LWLockAcquire(LockHashPartitionLockByIndex(i), LW_EXCLUSIVE);
  }

     
                                                                 
     
                                                                        
                                                                            
                                                 
     
                                                                            
                                                           
     
  if (MyProc->links.prev == NULL || MyProc->links.next == NULL)
  {
    goto check_done;
  }

#ifdef LOCK_DEBUG
  if (Debug_deadlocks)
  {
    DumpAllLocks();
  }
#endif

                                                                           
  deadlock_state = DeadLockCheck(MyProc);

  if (deadlock_state == DS_HARD_DEADLOCK)
  {
       
                                  
       
                                                                        
                                                                     
                                                                        
                                    
       
                                                                       
                                                                      
                
       
    Assert(MyProc->waitLock != NULL);
    RemoveFromWaitQueue(MyProc, LockTagHashCode(&(MyProc->waitLock->tag)));

       
                                                                    
                                                                     
                                                                         
                                                                       
                                                                          
                                                                     
                                                                     
                                                                      
       
  }

     
                                                                          
                                                                             
                                                                          
                                                                     
                                    
     
check_done:
  for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
  {
    LWLockRelease(LockHashPartitionLockByIndex(i));
  }
}

   
                                                               
   
                                                 
   
void
CheckDeadLockAlert(void)
{
  int save_errno = errno;

  got_deadlock_timeout = true;

     
                                                                             
                                                                         
                                                                      
     
                                                                             
                                                                            
     
  SetLatch(MyLatch);
  errno = save_errno;
}

   
                                                               
   
                                                                              
                                                                            
                      
   
void
ProcWaitForSignal(uint32 wait_event_info)
{
  (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, wait_event_info);
  ResetLatch(MyLatch);
  CHECK_FOR_INTERRUPTS();
}

   
                                                                 
   
void
ProcSendSignal(int pid)
{
  PGPROC *proc = NULL;

  if (RecoveryInProgress())
  {
    SpinLockAcquire(ProcStructLock);

       
                                                                         
                                                                           
                                                                     
                                                                          
                                                                          
                                                          
       
    if (pid == ProcGlobal->startupProcPid)
    {
      proc = ProcGlobal->startupProc;
    }

    SpinLockRelease(ProcStructLock);
  }

  if (proc == NULL)
  {
    proc = BackendPidGetProc(pid);
  }

  if (proc != NULL)
  {
    SetLatch(&proc->procLatch);
  }
}

   
                                                                  
   
                                                                            
                                     
   
void
BecomeLockGroupLeader(void)
{
  LWLock *leader_lwlock;

                                                           
  if (MyProc->lockGroupLeader == MyProc)
  {
    return;
  }

                                        
  Assert(MyProc->lockGroupLeader == NULL);

                                                              
  leader_lwlock = LockHashPartitionLockByProc(MyProc);
  LWLockAcquire(leader_lwlock, LW_EXCLUSIVE);
  MyProc->lockGroupLeader = MyProc;
  dlist_push_head(&MyProc->lockGroupMembers, &MyProc->lockGroupLink);
  LWLockRelease(leader_lwlock);
}

   
                                                                  
   
                                                                             
                                                                          
                                                                         
                                                                         
                                                                         
                            
   
bool
BecomeLockGroupMember(PGPROC *leader, int pid)
{
  LWLock *leader_lwlock;
  bool ok = false;

                                                 
  Assert(MyProc != leader);

                                            
  Assert(MyProc->lockGroupLeader == NULL);

                          
  Assert(pid != 0);

     
                                                                             
                                                                             
                                                                       
                                                                        
                                                                             
     
  leader_lwlock = LockHashPartitionLockByProc(leader);
  LWLockAcquire(leader_lwlock, LW_EXCLUSIVE);

                                             
  if (leader->pid == pid && leader->lockGroupLeader == leader)
  {
                            
    ok = true;
    MyProc->lockGroupLeader = leader;
    dlist_push_tail(&leader->lockGroupMembers, &MyProc->lockGroupLink);
  }
  LWLockRelease(leader_lwlock);

  return ok;
}
