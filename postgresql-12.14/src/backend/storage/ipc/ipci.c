                                                                            
   
          
                                                               
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/subtrans.h"
#include "access/twophase.h"
#include "commands/async.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/bgworker_internals.h"
#include "postmaster/bgwriter.h"
#include "postmaster/postmaster.h"
#include "replication/logicallauncher.h"
#include "replication/slot.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "replication/origin.h"
#include "storage/bufmgr.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "utils/snapmgr.h"

          
int shared_memory_type = DEFAULT_SHARED_MEMORY_TYPE;

shmem_startup_hook_type shmem_startup_hook = NULL;

static Size total_addin_request = 0;
static bool addin_request_allowed = true;

   
                          
                                                           
                       
   
                                                                          
                                                                     
                                                                       
                                                                   
                                                               
   
void
RequestAddinShmemSpace(Size size)
{
  if (IsUnderPostmaster || !addin_request_allowed)
  {
    return;               
  }
  total_addin_request = add_size(total_addin_request, size);
}

   
                                   
                                                          
   
                                                                
                                                                    
                                                                     
                                                                      
                                                                             
                                                                            
                                                                          
                                                                           
                                                                           
                                                         
   
void
CreateSharedMemoryAndSemaphores(int port)
{
  PGShmemHeader *shim = NULL;

  if (!IsUnderPostmaster)
  {
    PGShmemHeader *seghdr;
    Size size;
    int numSemas;

                                                 
    numSemas = ProcGlobalSemas();
    numSemas += SpinlockSemas();

       
                                                                 
                                                                         
                                                         
       
                                                                         
                                                                        
                                                                 
       
    size = 100000;
    size = add_size(size, PGSemaphoreShmemSize(numSemas));
    size = add_size(size, SpinlockSemaSize());
    size = add_size(size, hash_estimate_size(SHMEM_INDEX_SIZE, sizeof(ShmemIndexEnt)));
    size = add_size(size, BufferShmemSize());
    size = add_size(size, LockShmemSize());
    size = add_size(size, PredicateLockShmemSize());
    size = add_size(size, ProcGlobalShmemSize());
    size = add_size(size, XLOGShmemSize());
    size = add_size(size, CLOGShmemSize());
    size = add_size(size, CommitTsShmemSize());
    size = add_size(size, SUBTRANSShmemSize());
    size = add_size(size, TwoPhaseShmemSize());
    size = add_size(size, BackgroundWorkerShmemSize());
    size = add_size(size, MultiXactShmemSize());
    size = add_size(size, LWLockShmemSize());
    size = add_size(size, ProcArrayShmemSize());
    size = add_size(size, BackendStatusShmemSize());
    size = add_size(size, SInvalShmemSize());
    size = add_size(size, PMSignalShmemSize());
    size = add_size(size, ProcSignalShmemSize());
    size = add_size(size, CheckpointerShmemSize());
    size = add_size(size, AutoVacuumShmemSize());
    size = add_size(size, ReplicationSlotsShmemSize());
    size = add_size(size, ReplicationOriginShmemSize());
    size = add_size(size, WalSndShmemSize());
    size = add_size(size, WalRcvShmemSize());
    size = add_size(size, ApplyLauncherShmemSize());
    size = add_size(size, SnapMgrShmemSize());
    size = add_size(size, BTreeShmemSize());
    size = add_size(size, SyncScanShmemSize());
    size = add_size(size, AsyncShmemSize());
#ifdef EXEC_BACKEND
    size = add_size(size, ShmemBackendArraySize());
#endif

                                                      
    addin_request_allowed = false;
    size = add_size(size, total_addin_request);

                                                                         
    size = add_size(size, 8192 - (size % 8192));

    elog(DEBUG3, "invoking IpcMemoryCreate(size=%zu)", size);

       
                                
       
    seghdr = PGSharedMemoryCreate(size, port, &shim);

    InitShmemAccess(seghdr);

       
                         
       
    PGReserveSemaphores(numSemas, port);

       
                                                                    
                                                               
       
#ifndef HAVE_SPINLOCKS
    SpinlockSemaInit();
#endif
  }
  else
  {
       
                                                                     
                                                        
       
#ifndef EXEC_BACKEND
    elog(PANIC, "should be attached to shared memory already");
#endif
  }

     
                                               
     
  if (!IsUnderPostmaster)
  {
    InitShmemAllocation();
  }

     
                                                                       
                                
     
  CreateLWLocks();

     
                                    
     
  InitShmemIndex();

     
                                    
     
  XLOGShmemInit();
  CLOGShmemInit();
  CommitTsShmemInit();
  SUBTRANSShmemInit();
  MultiXactShmemInit();
  InitBufferPool();

     
                         
     
  InitLocks();

     
                                   
     
  InitPredicateLocks();

     
                          
     
  if (!IsUnderPostmaster)
  {
    InitProcGlobal();
  }
  CreateSharedProcArray();
  CreateSharedBackendStatus();
  TwoPhaseShmemInit();
  BackgroundWorkerShmemInit();

     
                                   
     
  CreateSharedInvalidationState();

     
                                              
     
  PMSignalShmemInit();
  ProcSignalShmemInit();
  CheckpointerShmemInit();
  AutoVacuumShmemInit();
  ReplicationSlotsShmemInit();
  ReplicationOriginShmemInit();
  WalSndShmemInit();
  WalRcvShmemInit();
  ApplyLauncherShmemInit();

     
                                                             
     
  SnapMgrInit();
  BTreeShmemInit();
  SyncScanShmemInit();
  AsyncShmemInit();

#ifdef EXEC_BACKEND

     
                                          
     
  if (!IsUnderPostmaster)
  {
    ShmemBackendArrayAllocation();
  }
#endif

                                                    
  if (!IsUnderPostmaster)
  {
    dsm_postmaster_startup(shim);
  }

     
                                                                          
     
  if (shmem_startup_hook)
  {
    shmem_startup_hook();
  }
}
