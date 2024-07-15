                                                                            
   
                
                                          
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/parallel.h"
#include "commands/async.h"
#include "miscadmin.h"
#include "replication/walsender.h"
#include "storage/latch.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/sinval.h"
#include "tcop/tcopprot.h"

   
                                                                          
                                                                          
                                                                        
                                                                             
                                                                            
                          
   
                                                                       
                                                                            
                                                                            
                                                                         
                                                                  
   
                                                                          
                                                                      
                                                                         
   
typedef struct
{
  pid_t pss_pid;
  sig_atomic_t pss_signalFlags[NUM_PROCSIGNALS];
} ProcSignalSlot;

   
                                                                    
                                                                       
                                                           
   
#define NumProcSignalSlots (MaxBackends + NUM_AUXPROCTYPES)

static ProcSignalSlot *ProcSignalSlots = NULL;
static volatile ProcSignalSlot *MyProcSignalSlot = NULL;

static bool
CheckProcSignal(ProcSignalReason reason);
static void
CleanupProcSignalState(int status, Datum arg);

   
                       
                                                        
   
Size
ProcSignalShmemSize(void)
{
  return NumProcSignalSlots * sizeof(ProcSignalSlot);
}

   
                       
                                                       
   
void
ProcSignalShmemInit(void)
{
  Size size = ProcSignalShmemSize();
  bool found;

  ProcSignalSlots = (ProcSignalSlot *)ShmemInitStruct("ProcSignalSlots", size, &found);

                                                
  if (!found)
  {
    MemSet(ProcSignalSlots, 0, size);
  }
}

   
                  
                                                         
   
                                                                   
                                             
   
void
ProcSignalInit(int pss_idx)
{
  volatile ProcSignalSlot *slot;

  Assert(pss_idx >= 1 && pss_idx <= NumProcSignalSlots);

  slot = &ProcSignalSlots[pss_idx - 1];

                    
  if (slot->pss_pid != 0)
  {
    elog(LOG, "process %d taking over ProcSignal slot %d, but it's not empty", MyProcPid, pss_idx);
  }

                                             
  MemSet(slot->pss_signalFlags, 0, NUM_PROCSIGNALS * sizeof(sig_atomic_t));

                             
  slot->pss_pid = MyProcPid;

                                                  
  MyProcSignalSlot = slot;

                                                  
  on_shmem_exit(CleanupProcSignalState, Int32GetDatum(pss_idx));
}

   
                          
                                                
   
                                                                        
   
static void
CleanupProcSignalState(int status, Datum arg)
{
  int pss_idx = DatumGetInt32(arg);
  volatile ProcSignalSlot *slot;

  slot = &ProcSignalSlots[pss_idx - 1];
  Assert(slot == MyProcSignalSlot);

     
                                                                         
                                                                        
                                                      
     
  MyProcSignalSlot = NULL;

                    
  if (slot->pss_pid != MyProcPid)
  {
       
                                                                          
                                    
       
    elog(LOG, "process %d releasing ProcSignal slot %d, but it contains %d", MyProcPid, pss_idx, (int)slot->pss_pid);
    return;                                          
  }

  slot->pss_pid = 0;
}

   
                  
                                        
   
                                                                        
   
                                                     
                                                                             
   
                                          
   
int
SendProcSignal(pid_t pid, ProcSignalReason reason, BackendId backendId)
{
  volatile ProcSignalSlot *slot;

  if (backendId != InvalidBackendId)
  {
    slot = &ProcSignalSlots[backendId - 1];

       
                                                                     
                                                                      
                                                                         
                                                                          
                                                                          
                                                                  
       
    if (slot->pss_pid == pid)
    {
                                          
      slot->pss_signalFlags[reason] = true;
                       
      return kill(pid, SIGUSR1);
    }
  }
  else
  {
       
                                                                         
                                                                         
                                                                          
                                                                  
       
    int i;

    for (i = NumProcSignalSlots - 1; i >= 0; i--)
    {
      slot = &ProcSignalSlots[i];

      if (slot->pss_pid == pid)
      {
                                                                   

                                            
        slot->pss_signalFlags[reason] = true;
                         
        return kill(pid, SIGUSR1);
      }
    }
  }

  errno = ESRCH;
  return -1;
}

   
                                                                  
                                                                          
            
   
static bool
CheckProcSignal(ProcSignalReason reason)
{
  volatile ProcSignalSlot *slot = MyProcSignalSlot;

  if (slot != NULL)
  {
                                                                     
    if (slot->pss_signalFlags[reason])
    {
      slot->pss_signalFlags[reason] = false;
      return true;
    }
  }

  return false;
}

   
                                                       
   
void
procsignal_sigusr1_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  if (CheckProcSignal(PROCSIG_CATCHUP_INTERRUPT))
  {
    HandleCatchupInterrupt();
  }

  if (CheckProcSignal(PROCSIG_NOTIFY_INTERRUPT))
  {
    HandleNotifyInterrupt();
  }

  if (CheckProcSignal(PROCSIG_PARALLEL_MESSAGE))
  {
    HandleParallelMessageInterrupt();
  }

  if (CheckProcSignal(PROCSIG_WALSND_INIT_STOPPING))
  {
    HandleWalSndInitStopping();
  }

  if (CheckProcSignal(PROCSIG_RECOVERY_CONFLICT_DATABASE))
  {
    RecoveryConflictInterrupt(PROCSIG_RECOVERY_CONFLICT_DATABASE);
  }

  if (CheckProcSignal(PROCSIG_RECOVERY_CONFLICT_TABLESPACE))
  {
    RecoveryConflictInterrupt(PROCSIG_RECOVERY_CONFLICT_TABLESPACE);
  }

  if (CheckProcSignal(PROCSIG_RECOVERY_CONFLICT_LOCK))
  {
    RecoveryConflictInterrupt(PROCSIG_RECOVERY_CONFLICT_LOCK);
  }

  if (CheckProcSignal(PROCSIG_RECOVERY_CONFLICT_SNAPSHOT))
  {
    RecoveryConflictInterrupt(PROCSIG_RECOVERY_CONFLICT_SNAPSHOT);
  }

  if (CheckProcSignal(PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK))
  {
    RecoveryConflictInterrupt(PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK);
  }

  if (CheckProcSignal(PROCSIG_RECOVERY_CONFLICT_BUFFERPIN))
  {
    RecoveryConflictInterrupt(PROCSIG_RECOVERY_CONFLICT_BUFFERPIN);
  }

  SetLatch(MyLatch);

  latch_sigusr1_handler();

  errno = save_errno;
}
