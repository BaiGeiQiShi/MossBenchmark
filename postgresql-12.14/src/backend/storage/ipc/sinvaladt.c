                                                                            
   
               
                                                      
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "miscadmin.h"
#include "storage/backendid.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/shmem.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "access/transam.h"

   
                                                                         
                                                                          
                                                                              
                                                                           
                                                                              
                                                                                
                                                                         
            
   
                                                                      
                                                                            
                                                                      
                                                               
   
                                                                              
                                                                        
                                                                     
                                                                         
                                                                              
                                                                          
                                                                            
                                                                          
                                                                          
                                                                
   
                                                                              
                                                                         
                                                                              
                                                                        
                                                                         
                                                                           
                                                                           
                                                                            
                                                                           
                                                                         
                                                                         
                                                                            
                                                                             
                                                                            
                                                                      
   
                                                                       
                                                                             
                                                                          
                                                                           
                                                                          
                              
   
                                                                               
                                                                          
                                                                           
                                                                       
                                                                          
                                                                            
                                                                        
                                                                        
                                                                           
                                                                     
                                                                       
                                                                          
                                                                             
                                                                         
                                                                           
                                                                   
   
                                                                             
                                                                           
                                                                            
                                                                           
                                                                         
                                                                         
                                                                     
   

   
                            
   
                                                                      
                                   
   
                                                                             
                                                           
   
                                                                          
                                            
   
                                                                        
                                                             
   
                                                                            
                                                          
   
                                                                         
                                                                          
                                                                         
                  
   

#define MAXNUMMESSAGES 4096
#define MSGNUMWRAPAROUND (MAXNUMMESSAGES * 262144)
#define CLEANUP_MIN (MAXNUMMESSAGES / 2)
#define CLEANUP_QUANTUM (MAXNUMMESSAGES / 16)
#define SIG_THRESHOLD (MAXNUMMESSAGES / 2)
#define WRITE_QUANTUM 64

                                                        
typedef struct ProcState
{
                                                             
  pid_t procPid;                                    
  PGPROC *proc;                         
                                                                        
  int nextMsgNum;                                    
  bool resetState;                                        
  bool signaled;                                              
  bool hasMessages;                                  

     
                                                                            
                                                                             
                                                                          
                     
     
  bool sendOnly;                                         

     
                                                                         
                                                                          
                                                                           
                                               
     
  LocalTransactionId nextLXID;
} ProcState;

                                              
typedef struct SISeg
{
     
                               
     
  int minMsgNum;                                      
  int maxMsgNum;                                             
  int nextThreshold;                                           
  int lastBackend;                                                 
  int maxBackends;                                

  slock_t msgnumLock;                                    

     
                                                   
     
  SharedInvalidationMessage buffer[MAXNUMMESSAGES];

     
                                                                    
     
  ProcState procState[FLEXIBLE_ARRAY_MEMBER];
} SISeg;

static SISeg *shmInvalBuffer;                                         

static LocalTransactionId nextLocalTransactionId;

static void
CleanupInvalidationState(int status, Datum arg);

   
                                                         
   
Size
SInvalShmemSize(void)
{
  Size size;

  size = offsetof(SISeg, procState);
  size = add_size(size, mul_size(sizeof(ProcState), MaxBackends));

  return size;
}

   
                                 
                                                
   
void
CreateSharedInvalidationState(void)
{
  int i;
  bool found;

                                       
  shmInvalBuffer = (SISeg *)ShmemInitStruct("shmInvalBuffer", SInvalShmemSize(), &found);
  if (found)
  {
    return;
  }

                                                                           
  shmInvalBuffer->minMsgNum = 0;
  shmInvalBuffer->maxMsgNum = 0;
  shmInvalBuffer->nextThreshold = CLEANUP_MIN;
  shmInvalBuffer->lastBackend = 0;
  shmInvalBuffer->maxBackends = MaxBackends;
  SpinLockInit(&shmInvalBuffer->msgnumLock);

                                                                          

                                                           
  for (i = 0; i < shmInvalBuffer->maxBackends; i++)
  {
    shmInvalBuffer->procState[i].procPid = 0;               
    shmInvalBuffer->procState[i].proc = NULL;
    shmInvalBuffer->procState[i].nextMsgNum = 0;                  
    shmInvalBuffer->procState[i].resetState = false;
    shmInvalBuffer->procState[i].signaled = false;
    shmInvalBuffer->procState[i].hasMessages = false;
    shmInvalBuffer->procState[i].nextLXID = InvalidLocalTransactionId;
  }
}

   
                          
                                                             
   
void
SharedInvalBackendInit(bool sendOnly)
{
  int index;
  ProcState *stateP = NULL;
  SISeg *segP = shmInvalBuffer;

     
                                                                       
                                                                        
                                
     
  LWLockAcquire(SInvalWriteLock, LW_EXCLUSIVE);

                                                    
  for (index = 0; index < segP->lastBackend; index++)
  {
    if (segP->procState[index].procPid == 0)                     
    {
      stateP = &segP->procState[index];
      break;
    }
  }

  if (stateP == NULL)
  {
    if (segP->lastBackend < segP->maxBackends)
    {
      stateP = &segP->procState[segP->lastBackend];
      Assert(stateP->procPid == 0);
      segP->lastBackend++;
    }
    else
    {
         
                                                                         
         
      MyBackendId = InvalidBackendId;
      LWLockRelease(SInvalWriteLock);
      ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("sorry, too many clients already")));
    }
  }

  MyBackendId = (stateP - &segP->procState[0]) + 1;

                                               
  MyProc->backendId = MyBackendId;

                                                         
  nextLocalTransactionId = stateP->nextLXID;

                                                                 
  stateP->procPid = MyProcPid;
  stateP->proc = MyProc;
  stateP->nextMsgNum = segP->maxMsgNum;
  stateP->resetState = false;
  stateP->signaled = false;
  stateP->hasMessages = false;
  stateP->sendOnly = sendOnly;

  LWLockRelease(SInvalWriteLock);

                                                               
  on_shmem_exit(CleanupInvalidationState, PointerGetDatum(segP));

  elog(DEBUG4, "my backend ID is %d", MyBackendId);
}

   
                            
                                                  
   
                                                                        
   
                                   
   
static void
CleanupInvalidationState(int status, Datum arg)
{
  SISeg *segP = (SISeg *)DatumGetPointer(arg);
  ProcState *stateP;
  int i;

  Assert(PointerIsValid(segP));

  LWLockAcquire(SInvalWriteLock, LW_EXCLUSIVE);

  stateP = &segP->procState[MyBackendId - 1];

                                                                          
  stateP->nextLXID = nextLocalTransactionId;

                            
  stateP->procPid = 0;
  stateP->proc = NULL;
  stateP->nextMsgNum = 0;
  stateP->resetState = false;
  stateP->signaled = false;

                                              
  for (i = segP->lastBackend; i > 0; i--)
  {
    if (segP->procState[i - 1].procPid != 0)
    {
      break;
    }
  }
  segP->lastBackend = i;

  LWLockRelease(SInvalWriteLock);
}

   
                    
                                                                  
                                                                     
                                                                 
                                           
   
PGPROC *
BackendIdGetProc(int backendID)
{
  PGPROC *result = NULL;
  SISeg *segP = shmInvalBuffer;

                                                       
  LWLockAcquire(SInvalWriteLock, LW_SHARED);

  if (backendID > 0 && backendID <= segP->lastBackend)
  {
    ProcState *stateP = &segP->procState[backendID - 1];

    result = stateP->proc;
  }

  LWLockRelease(SInvalWriteLock);

  return result;
}

   
                              
                                                                       
                                                                      
                         
   
void
BackendIdGetTransactionIds(int backendID, TransactionId *xid, TransactionId *xmin)
{
  SISeg *segP = shmInvalBuffer;

  *xid = InvalidTransactionId;
  *xmin = InvalidTransactionId;

                                                       
  LWLockAcquire(SInvalWriteLock, LW_SHARED);

  if (backendID > 0 && backendID <= segP->lastBackend)
  {
    ProcState *stateP = &segP->procState[backendID - 1];
    PGPROC *proc = stateP->proc;

    if (proc != NULL)
    {
      PGXACT *xact = &ProcGlobal->allPgXact[proc->pgprocno];

      *xid = xact->xid;
      *xmin = xact->xmin;
    }
  }

  LWLockRelease(SInvalWriteLock);
}

   
                       
                                                   
   
void
SIInsertDataEntries(const SharedInvalidationMessage *data, int n)
{
  SISeg *segP = shmInvalBuffer;

     
                                                                            
                                                                             
                                                                            
                                                                            
                                                                             
                                                                      
                                    
     
  while (n > 0)
  {
    int nthistime = Min(n, WRITE_QUANTUM);
    int numMsgs;
    int max;
    int i;

    n -= nthistime;

    LWLockAcquire(SInvalWriteLock, LW_EXCLUSIVE);

       
                                                                       
                                                                        
                                                                   
                                                                         
                                         
       
    for (;;)
    {
      numMsgs = segP->maxMsgNum - segP->minMsgNum;
      if (numMsgs + nthistime > MAXNUMMESSAGES || numMsgs >= segP->nextThreshold)
      {
        SICleanupQueue(true, nthistime);
      }
      else
      {
        break;
      }
    }

       
                                                                 
       
    max = segP->maxMsgNum;
    while (nthistime-- > 0)
    {
      segP->buffer[max % MAXNUMMESSAGES] = *data++;
      max++;
    }

                                                          
    SpinLockAcquire(&segP->msgnumLock);
    segP->maxMsgNum = max;
    SpinLockRelease(&segP->msgnumLock);

       
                                                                           
                                                                     
                                                                        
                                                                           
                     
       
    for (i = 0; i < segP->lastBackend; i++)
    {
      ProcState *stateP = &segP->procState[i];

      stateP->hasMessages = true;
    }

    LWLockRelease(SInvalWriteLock);
  }
}

   
                    
                                                                 
   
                           
                               
                                                           
                                   
   
                                                                          
                                                                            
                                                               
   
                                                                         
                                                                               
                                                                              
                                                                              
                                                                         
                                                                            
                                                                              
   
                                                                          
                                                                          
            
   
                                                                              
                                                      
   
int
SIGetDataEntries(SharedInvalidationMessage *data, int datasize)
{
  SISeg *segP;
  ProcState *stateP;
  int max;
  int n;

  segP = shmInvalBuffer;
  stateP = &segP->procState[MyBackendId - 1];

     
                                                                             
                                                                          
                                                                           
                                                                             
                                                                     
                                                                             
                                                                 
                                                                          
                                                                 
     
  if (!stateP->hasMessages)
  {
    return 0;
  }

  LWLockAcquire(SInvalReadLock, LW_SHARED);

     
                                                                          
                                                                    
                                                                          
                                             
     
                                                                       
                                                          
     
  stateP->hasMessages = false;

                                                       
  SpinLockAcquire(&segP->msgnumLock);
  max = segP->maxMsgNum;
  SpinLockRelease(&segP->msgnumLock);

  if (stateP->resetState)
  {
       
                                                                      
                                                                    
                           
       
    stateP->nextMsgNum = max;
    stateP->resetState = false;
    stateP->signaled = false;
    LWLockRelease(SInvalReadLock);
    return -1;
  }

     
                                                                          
                                         
     
                                                                         
                                                                            
                     
     
  n = 0;
  while (n < datasize && stateP->nextMsgNum < max)
  {
    data[n++] = segP->buffer[stateP->nextMsgNum % MAXNUMMESSAGES];
    stateP->nextMsgNum++;
  }

     
                                                                        
                                                       
     
                                                                            
                                              
     
  if (stateP->nextMsgNum >= max)
  {
    stateP->signaled = false;
  }
  else
  {
    stateP->hasMessages = true;
  }

  LWLockRelease(SInvalReadLock);
  return n;
}

   
                  
                                                                   
   
                                                                    
                                                                
   
                                                                     
                                                                           
                                                                          
                                                                             
   
                                                                             
                                                                         
                                                                       
   
void
SICleanupQueue(bool callerHasWriteLock, int minFree)
{
  SISeg *segP = shmInvalBuffer;
  int min, minsig, lowbound, numMsgs, i;
  ProcState *needSig = NULL;

                                        
  if (!callerHasWriteLock)
  {
    LWLockAcquire(SInvalWriteLock, LW_EXCLUSIVE);
  }
  LWLockAcquire(SInvalReadLock, LW_EXCLUSIVE);

     
                                                                             
                                                                        
                                                                           
                                                                            
                                                           
     
  min = segP->maxMsgNum;
  minsig = min - SIG_THRESHOLD;
  lowbound = min - MAXNUMMESSAGES + minFree;

  for (i = 0; i < segP->lastBackend; i++)
  {
    ProcState *stateP = &segP->procState[i];
    int n = stateP->nextMsgNum;

                                                      
    if (stateP->procPid == 0 || stateP->resetState || stateP->sendOnly)
    {
      continue;
    }

       
                                                                           
                                                                 
       
    if (n < lowbound)
    {
      stateP->resetState = true;
                                         
      continue;
    }

                                             
    if (n < min)
    {
      min = n;
    }

                                                                 
    if (n < minsig && !stateP->signaled)
    {
      minsig = n;
      needSig = stateP;
    }
  }
  segP->minMsgNum = min;

     
                                                                            
                                                                             
                                                         
     
  if (min >= MSGNUMWRAPAROUND)
  {
    segP->minMsgNum -= MSGNUMWRAPAROUND;
    segP->maxMsgNum -= MSGNUMWRAPAROUND;
    for (i = 0; i < segP->lastBackend; i++)
    {
                                                          
      segP->procState[i].nextMsgNum -= MSGNUMWRAPAROUND;
    }
  }

     
                                                                     
                                                           
     
  numMsgs = segP->maxMsgNum - segP->minMsgNum;
  if (numMsgs < CLEANUP_MIN)
  {
    segP->nextThreshold = CLEANUP_MIN;
  }
  else
  {
    segP->nextThreshold = (numMsgs / CLEANUP_QUANTUM + 1) * CLEANUP_QUANTUM;
  }

     
                                                                 
                                                                           
                   
     
  if (needSig)
  {
    pid_t his_pid = needSig->procPid;
    BackendId his_backendId = (needSig - &segP->procState[0]) + 1;

    needSig->signaled = true;
    LWLockRelease(SInvalReadLock);
    LWLockRelease(SInvalWriteLock);
    elog(DEBUG4, "sending sinval catchup signal to PID %d", (int)his_pid);
    SendProcSignal(his_pid, PROCSIG_CATCHUP_INTERRUPT, his_backendId);
    if (callerHasWriteLock)
    {
      LWLockAcquire(SInvalWriteLock, LW_EXCLUSIVE);
    }
  }
  else
  {
    LWLockRelease(SInvalReadLock);
    if (!callerHasWriteLock)
    {
      LWLockRelease(SInvalWriteLock);
    }
  }
}

   
                                                                   
   
                                                                        
                                                                          
                                                                     
                                                                         
                                                                        
                                                                      
                                                                           
                                                                             
                                                    
   
LocalTransactionId
GetNextLocalTransactionId(void)
{
  LocalTransactionId result;

                                                                       
  do
  {
    result = nextLocalTransactionId++;
  } while (!LocalTransactionIdIsValid(result));

  return result;
}
