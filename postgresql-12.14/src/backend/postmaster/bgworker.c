                                                                       
              
                                                         
   
                                                                         
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>

#include "libpq/pqsignal.h"
#include "access/parallel.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "postmaster/bgworker_internals.h"
#include "postmaster/postmaster.h"
#include "replication/logicallauncher.h"
#include "replication/logicalworker.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/shmem.h"
#include "tcop/tcopprot.h"
#include "utils/ascii.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"

   
                                                                              
   
slist_head BackgroundWorkerList = SLIST_STATIC_INIT(BackgroundWorkerList);

   
                                                                         
                                                                              
                                                                           
                                                                             
                                                                           
                                         
   
                                                                             
                                                                       
                                                                              
                                                                              
                                                                     
                                                                                
                                                                           
                                                                               
                         
   
                                                                            
                                                                       
                                                                             
                               
   
                                                                            
                                                                               
                                                                          
                                                                          
                                                                           
                                                                         
                                                                      
               
   
typedef struct BackgroundWorkerSlot
{
  bool in_use;
  bool terminate;
  pid_t pid;                                                     
  uint64 generation;                                        
  BackgroundWorker worker;
} BackgroundWorkerSlot;

   
                                                                        
                                                                        
                                                                                
                                                                            
                                                                             
                                                                             
                                                                            
                                                                         
                                                                          
   
typedef struct BackgroundWorkerArray
{
  int total_slots;
  uint32 parallel_register_count;
  uint32 parallel_terminate_count;
  BackgroundWorkerSlot slot[FLEXIBLE_ARRAY_MEMBER];
} BackgroundWorkerArray;

struct BackgroundWorkerHandle
{
  int slot;
  uint64 generation;
};

static BackgroundWorkerArray *BackgroundWorkerData;

   
                                                                      
                                                                 
   
static const struct
{
  const char *fn_name;
  bgworker_main_type fn_addr;
} InternalBGWorkers[] =

    {{"ParallelWorkerMain", ParallelWorkerMain}, {"ApplyLauncherMain", ApplyLauncherMain}, {"ApplyWorkerMain", ApplyWorkerMain}};

                        
static bgworker_main_type
LookupBackgroundWorkerFunction(const char *libraryname, const char *funcname);

   
                                   
   
Size
BackgroundWorkerShmemSize(void)
{
  Size size;

                                           
  size = offsetof(BackgroundWorkerArray, slot);
  size = add_size(size, mul_size(max_worker_processes, sizeof(BackgroundWorkerSlot)));

  return size;
}

   
                             
   
void
BackgroundWorkerShmemInit(void)
{
  bool found;

  BackgroundWorkerData = ShmemInitStruct("Background Worker Data", BackgroundWorkerShmemSize(), &found);
  if (!IsUnderPostmaster)
  {
    slist_iter siter;
    int slotno = 0;

    BackgroundWorkerData->total_slots = max_worker_processes;
    BackgroundWorkerData->parallel_register_count = 0;
    BackgroundWorkerData->parallel_terminate_count = 0;

       
                                                                           
                                                                   
                                                                          
                         
       
    slist_foreach(siter, &BackgroundWorkerList)
    {
      BackgroundWorkerSlot *slot = &BackgroundWorkerData->slot[slotno];
      RegisteredBgWorker *rw;

      rw = slist_container(RegisteredBgWorker, rw_lnode, siter.cur);
      Assert(slotno < max_worker_processes);
      slot->in_use = true;
      slot->terminate = false;
      slot->pid = InvalidPid;
      slot->generation = 0;
      rw->rw_shmem_slot = slotno;
      rw->rw_worker.bgw_notify_pid = 0;                                  
      memcpy(&slot->worker, &rw->rw_worker, sizeof(BackgroundWorker));
      ++slotno;
    }

       
                                               
       
    while (slotno < max_worker_processes)
    {
      BackgroundWorkerSlot *slot = &BackgroundWorkerData->slot[slotno];

      slot->in_use = false;
      ++slotno;
    }
  }
  else
  {
    Assert(found);
  }
}

   
                                                                              
                                                   
   
static RegisteredBgWorker *
FindRegisteredWorkerBySlotNumber(int slotno)
{
  slist_iter siter;

  slist_foreach(siter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;

    rw = slist_container(RegisteredBgWorker, rw_lnode, siter.cur);
    if (rw->rw_shmem_slot == slotno)
    {
      return rw;
    }
  }

  return NULL;
}

   
                                                           
                                                                 
   
                                                                              
                                                                           
                            
   
void
BackgroundWorkerStateChange(bool allow_new_workers)
{
  int slotno;

     
                                                                        
                                                                        
                                                            
                                                                            
              
     
  if (max_worker_processes != BackgroundWorkerData->total_slots)
  {
    elog(LOG, "inconsistent background worker state (max_worker_processes=%d, total_slots=%d", max_worker_processes, BackgroundWorkerData->total_slots);
    return;
  }

     
                                                                            
                   
     
  for (slotno = 0; slotno < max_worker_processes; ++slotno)
  {
    BackgroundWorkerSlot *slot = &BackgroundWorkerData->slot[slotno];
    RegisteredBgWorker *rw;

    if (!slot->in_use)
    {
      continue;
    }

       
                                                                      
                 
       
    pg_read_barrier();

                                                        
    rw = FindRegisteredWorkerBySlotNumber(slotno);
    if (rw != NULL)
    {
         
                                                                       
                                                                   
         
      if (slot->terminate && !rw->rw_terminate)
      {
        rw->rw_terminate = true;
        if (rw->rw_pid != 0)
        {
          kill(rw->rw_pid, SIGTERM);
        }
        else
        {
                                                                    
          ReportBackgroundWorkerPID(rw);
        }
      }
      continue;
    }

       
                                                                       
                                                                      
                                                                           
                                                                    
       
    if (!allow_new_workers)
    {
      slot->terminate = true;
    }

       
                                                                           
                                                                           
                                                                         
                                                                           
                     
       
    if (slot->terminate)
    {
      int notify_pid;

         
                                                                     
                                                                   
                                              
         
      notify_pid = slot->worker.bgw_notify_pid;
      if ((slot->worker.bgw_flags & BGWORKER_CLASS_PARALLEL) != 0)
      {
        BackgroundWorkerData->parallel_terminate_count++;
      }
      slot->pid = 0;

      pg_memory_barrier();
      slot->in_use = false;

      if (notify_pid != 0)
      {
        kill(notify_pid, SIGUSR1);
      }

      continue;
    }

       
                                                                    
       
    rw = malloc(sizeof(RegisteredBgWorker));
    if (rw == NULL)
    {
      ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
      return;
    }

       
                                                                           
                                                     
       
    ascii_safe_strlcpy(rw->rw_worker.bgw_name, slot->worker.bgw_name, BGW_MAXLEN);
    ascii_safe_strlcpy(rw->rw_worker.bgw_type, slot->worker.bgw_type, BGW_MAXLEN);
    ascii_safe_strlcpy(rw->rw_worker.bgw_library_name, slot->worker.bgw_library_name, BGW_MAXLEN);
    ascii_safe_strlcpy(rw->rw_worker.bgw_function_name, slot->worker.bgw_function_name, BGW_MAXLEN);

       
                                       
       
                                                                           
                                                                   
                                                                        
                                  
       
    rw->rw_worker.bgw_flags = slot->worker.bgw_flags;
    rw->rw_worker.bgw_start_time = slot->worker.bgw_start_time;
    rw->rw_worker.bgw_restart_time = slot->worker.bgw_restart_time;
    rw->rw_worker.bgw_main_arg = slot->worker.bgw_main_arg;
    memcpy(rw->rw_worker.bgw_extra, slot->worker.bgw_extra, BGW_EXTRALEN);

       
                                                                        
                                                                          
                                                                         
                                                                       
                                                                           
                                                                        
                         
       
    rw->rw_worker.bgw_notify_pid = slot->worker.bgw_notify_pid;
    if (!PostmasterMarkPIDForWorkerNotify(rw->rw_worker.bgw_notify_pid))
    {
      elog(DEBUG1, "worker notification PID %lu is not valid", (long)rw->rw_worker.bgw_notify_pid);
      rw->rw_worker.bgw_notify_pid = 0;
    }

                                            
    rw->rw_backend = NULL;
    rw->rw_pid = 0;
    rw->rw_child_slot = 0;
    rw->rw_crashed_at = 0;
    rw->rw_shmem_slot = slotno;
    rw->rw_terminate = false;

                 
    ereport(DEBUG1, (errmsg("registering background worker \"%s\"", rw->rw_worker.bgw_name)));

    slist_push_head(&BackgroundWorkerList, &rw->rw_lnode);
  }
}

   
                                                             
   
                                                                       
                                                                    
                                                                           
   
                                                                       
   
                                                         
   
void
ForgetBackgroundWorker(slist_mutable_iter *cur)
{
  RegisteredBgWorker *rw;
  BackgroundWorkerSlot *slot;

  rw = slist_container(RegisteredBgWorker, rw_lnode, cur->cur);

  Assert(rw->rw_shmem_slot < max_worker_processes);
  slot = &BackgroundWorkerData->slot[rw->rw_shmem_slot];
  Assert(slot->in_use);

     
                                                                   
                                                                    
     
  if ((rw->rw_worker.bgw_flags & BGWORKER_CLASS_PARALLEL) != 0)
  {
    BackgroundWorkerData->parallel_terminate_count++;
  }

  pg_memory_barrier();
  slot->in_use = false;

  ereport(DEBUG1, (errmsg("unregistering background worker \"%s\"", rw->rw_worker.bgw_name)));

  slist_delete_current(cur);
  free(rw);
}

   
                                                                          
   
                                                            
   
void
ReportBackgroundWorkerPID(RegisteredBgWorker *rw)
{
  BackgroundWorkerSlot *slot;

  Assert(rw->rw_shmem_slot < max_worker_processes);
  slot = &BackgroundWorkerData->slot[rw->rw_shmem_slot];
  slot->pid = rw->rw_pid;

  if (rw->rw_worker.bgw_notify_pid != 0)
  {
    kill(rw->rw_worker.bgw_notify_pid, SIGUSR1);
  }
}

   
                                                                    
                                                    
   
                                                            
   
void
ReportBackgroundWorkerExit(slist_mutable_iter *cur)
{
  RegisteredBgWorker *rw;
  BackgroundWorkerSlot *slot;
  int notify_pid;

  rw = slist_container(RegisteredBgWorker, rw_lnode, cur->cur);

  Assert(rw->rw_shmem_slot < max_worker_processes);
  slot = &BackgroundWorkerData->slot[rw->rw_shmem_slot];
  slot->pid = rw->rw_pid;
  notify_pid = rw->rw_worker.bgw_notify_pid;

     
                                                                           
                                                                        
                                                                           
                                                                          
                                                       
     
  if (rw->rw_terminate || rw->rw_worker.bgw_restart_time == BGW_NEVER_RESTART)
  {
    ForgetBackgroundWorker(cur);
  }

  if (notify_pid != 0)
  {
    kill(notify_pid, SIGUSR1);
  }
}

   
                                                                           
   
                                                            
   
void
BackgroundWorkerStopNotifications(pid_t pid)
{
  slist_iter siter;

  slist_foreach(siter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;

    rw = slist_container(RegisteredBgWorker, rw_lnode, siter.cur);
    if (rw->rw_worker.bgw_notify_pid == pid)
    {
      rw->rw_worker.bgw_notify_pid = 0;
    }
  }
}

   
                                                                           
   
                                                                         
                                                                            
                                                                          
                                                                           
                                                                          
                                      
   
                                                            
   
void
ForgetUnstartedBackgroundWorkers(void)
{
  slist_mutable_iter iter;

  slist_foreach_modify(iter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;
    BackgroundWorkerSlot *slot;

    rw = slist_container(RegisteredBgWorker, rw_lnode, iter.cur);
    Assert(rw->rw_shmem_slot < max_worker_processes);
    slot = &BackgroundWorkerData->slot[rw->rw_shmem_slot];

                                                                  
    if (slot->pid == InvalidPid && rw->rw_worker.bgw_notify_pid != 0)
    {
                                                  
      int notify_pid = rw->rw_worker.bgw_notify_pid;

      ForgetBackgroundWorker(&iter);
      if (notify_pid != 0)
      {
        kill(notify_pid, SIGUSR1);
      }
    }
  }
}

   
                                        
   
                                                                               
                                                                              
                                                                              
                                                                      
   
                                                            
   
void
ResetBackgroundWorkerCrashTimes(void)
{
  slist_mutable_iter iter;

  slist_foreach_modify(iter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;

    rw = slist_container(RegisteredBgWorker, rw_lnode, iter.cur);

    if (rw->rw_worker.bgw_restart_time == BGW_NEVER_RESTART)
    {
         
                                                                         
                                                                       
                                                                    
                                                                   
                                                                      
         
      ForgetBackgroundWorker(&iter);
    }
    else
    {
         
                                                                    
                                                                         
                                                                    
                                                                      
                                         
         
      Assert((rw->rw_worker.bgw_flags & BGWORKER_CLASS_PARALLEL) == 0);

         
                                                                       
                    
         
      rw->rw_crashed_at = 0;

         
                                                              
         
      rw->rw_worker.bgw_notify_pid = 0;
    }
  }
}

#ifdef EXEC_BACKEND
   
                                                                         
                  
   
BackgroundWorker *
BackgroundWorkerEntry(int slotno)
{
  static BackgroundWorker myEntry;
  BackgroundWorkerSlot *slot;

  Assert(slotno < BackgroundWorkerData->total_slots);
  slot = &BackgroundWorkerData->slot[slotno];
  Assert(slot->in_use);

                                                                     
  memcpy(&myEntry, &slot->worker, sizeof myEntry);
  return &myEntry;
}
#endif

   
                                                                            
                                                                        
                                                          
   
static bool
SanityCheckBackgroundWorker(BackgroundWorker *worker, int elevel)
{
                              
  if (worker->bgw_flags & BGWORKER_BACKEND_DATABASE_CONNECTION)
  {
    if (!(worker->bgw_flags & BGWORKER_SHMEM_ACCESS))
    {
      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("background worker \"%s\": must attach to shared memory in order to request a database connection", worker->bgw_name)));
      return false;
    }

    if (worker->bgw_start_time == BgWorkerStart_PostmasterStart)
    {
      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("background worker \"%s\": cannot request database access if starting at postmaster start", worker->bgw_name)));
      return false;
    }

                           
  }

  if ((worker->bgw_restart_time < 0 && worker->bgw_restart_time != BGW_NEVER_RESTART) || (worker->bgw_restart_time > USECS_PER_DAY / 1000))
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("background worker \"%s\": invalid restart interval", worker->bgw_name)));
    return false;
  }

     
                                                                     
                                                                       
                                                                        
     
  if (worker->bgw_restart_time != BGW_NEVER_RESTART && (worker->bgw_flags & BGWORKER_CLASS_PARALLEL) != 0)
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("background worker \"%s\": parallel workers may not be configured for restart", worker->bgw_name)));
    return false;
  }

     
                                                 
     
  if (strcmp(worker->bgw_type, "") == 0)
  {
    strcpy(worker->bgw_type, worker->bgw_name);
  }

  return true;
}

static void
bgworker_quickdie(SIGNAL_ARGS)
{
     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

   
                                                   
   
static void
bgworker_die(SIGNAL_ARGS)
{
  PG_SETMASK(&BlockSig);

  ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating background worker \"%s\" due to administrator command", MyBgworkerEntry->bgw_type)));
}

   
                                                    
   
                                                                       
                   
   
static void
bgworker_sigusr1_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  latch_sigusr1_handler();

  errno = save_errno;
}

   
                                 
   
                                                                         
               
   
void
StartBackgroundWorker(void)
{
  sigjmp_buf local_sigjmp_buf;
  BackgroundWorker *worker = MyBgworkerEntry;
  bgworker_main_type entrypt;

  if (worker == NULL)
  {
    elog(FATAL, "unable to find bgworker entry");
  }

  IsBackgroundWorker = true;

                              
  init_ps_display(worker->bgw_name, "", "", "");

     
                                                                          
                                                                    
                                                                            
                                                                        
                                   
     
  if ((worker->bgw_flags & BGWORKER_SHMEM_ACCESS) == 0)
  {
    dsm_detach_all();
    PGSharedMemoryDetach();
  }

  SetProcessingMode(InitProcessing);

                           
  if (PostAuthDelay > 0)
  {
    pg_usleep(PostAuthDelay * 1000000L);
  }

     
                             
     
  if (worker->bgw_flags & BGWORKER_BACKEND_DATABASE_CONNECTION)
  {
       
                                                             
       
    pqsignal(SIGINT, StatementCancelHandler);
    pqsignal(SIGUSR1, procsignal_sigusr1_handler);
    pqsignal(SIGFPE, FloatExceptionHandler);

                                             
  }
  else
  {
    pqsignal(SIGINT, SIG_IGN);
    pqsignal(SIGUSR1, bgworker_sigusr1_handler);
    pqsignal(SIGFPE, SIG_IGN);
  }
  pqsignal(SIGTERM, bgworker_die);
  pqsignal(SIGHUP, SIG_IGN);

  pqsignal(SIGQUIT, bgworker_quickdie);
  InitializeTimeouts();                                  

  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR2, SIG_IGN);
  pqsignal(SIGCHLD, SIG_DFL);

     
                                                              
     
                                                              
     
  if (sigsetjmp(local_sigjmp_buf, 1) != 0)
  {
                                                                
    error_context_stack = NULL;

                                              
    HOLD_INTERRUPTS();

       
                                                                          
                                                                         
                                                                    
       
    BackgroundWorkerUnblockSignals();

                                                                    
    EmitErrorReport();

       
                                                                        
                                                                        
                                                               
       

                     
    proc_exit(1);
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

     
                                                                             
                                              
     
  if (worker->bgw_flags & BGWORKER_SHMEM_ACCESS)
  {
       
                                                                    
                                                                        
                                                                    
               
       
    BaseInit();

       
                                                                          
                                                                           
                                                                          
                                                   
       
#ifndef EXEC_BACKEND
    InitProcess();
#endif
  }

     
                                                                         
     
  entrypt = LookupBackgroundWorkerFunction(worker->bgw_library_name, worker->bgw_function_name);

     
                                                                            
                                                                            
                                                  
                                             
     

     
                                             
     
  entrypt(worker->bgw_main_arg);

                                         
  proc_exit(0);
}

   
                                            
   
                                                                       
                                                                           
                                     
   
void
RegisterBackgroundWorker(BackgroundWorker *worker)
{
  RegisteredBgWorker *rw;
  static int numworkers = 0;

  if (!IsUnderPostmaster)
  {
    ereport(DEBUG1, (errmsg("registering background worker \"%s\"", worker->bgw_name)));
  }

  if (!process_shared_preload_libraries_in_progress && strcmp(worker->bgw_library_name, "postgres") != 0)
  {
    if (!IsUnderPostmaster)
    {
      ereport(LOG, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("background worker \"%s\": must be registered in shared_preload_libraries", worker->bgw_name)));
    }
    return;
  }

  if (!SanityCheckBackgroundWorker(worker, LOG))
  {
    return;
  }

  if (worker->bgw_notify_pid != 0)
  {
    ereport(LOG, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("background worker \"%s\": only dynamic background workers can request notification", worker->bgw_name)));
    return;
  }

     
                                                                             
                                                                             
                                                                         
                                          
     
  if (++numworkers > max_worker_processes)
  {
    ereport(LOG, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("too many background workers"), errdetail_plural("Up to %d background worker can be registered with the current settings.", "Up to %d background workers can be registered with the current settings.", max_worker_processes, max_worker_processes), errhint("Consider increasing the configuration parameter \"max_worker_processes\".")));
    return;
  }

     
                                                                  
     
  rw = malloc(sizeof(RegisteredBgWorker));
  if (rw == NULL)
  {
    ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    return;
  }

  rw->rw_worker = *worker;
  rw->rw_backend = NULL;
  rw->rw_pid = 0;
  rw->rw_child_slot = 0;
  rw->rw_crashed_at = 0;
  rw->rw_terminate = false;

  slist_push_head(&BackgroundWorkerList, &rw->rw_lnode);
}

   
                                                            
   
                                                                              
                                                            
   
                                                                           
                                                                       
                                                
   
bool
RegisterDynamicBackgroundWorker(BackgroundWorker *worker, BackgroundWorkerHandle **handle)
{
  int slotno;
  bool success = false;
  bool parallel;
  uint64 generation = 0;

     
                                                                          
                                                                          
                                                                          
                                                                
                                                                   
                
     
  if (!IsUnderPostmaster)
  {
    return false;
  }

  if (!SanityCheckBackgroundWorker(worker, ERROR))
  {
    return false;
  }

  parallel = (worker->bgw_flags & BGWORKER_CLASS_PARALLEL) != 0;

  LWLockAcquire(BackgroundWorkerLock, LW_EXCLUSIVE);

     
                                                                            
                                                                       
                                                                             
                                                                       
                                                                         
                                                                         
                      
     
  if (parallel && (BackgroundWorkerData->parallel_register_count - BackgroundWorkerData->parallel_terminate_count) >= max_parallel_workers)
  {
    Assert(BackgroundWorkerData->parallel_register_count - BackgroundWorkerData->parallel_terminate_count <= MAX_PARALLEL_WORKER_LIMIT);
    LWLockRelease(BackgroundWorkerLock);
    return false;
  }

     
                                                        
     
  for (slotno = 0; slotno < BackgroundWorkerData->total_slots; ++slotno)
  {
    BackgroundWorkerSlot *slot = &BackgroundWorkerData->slot[slotno];

    if (!slot->in_use)
    {
      memcpy(&slot->worker, worker, sizeof(BackgroundWorker));
      slot->pid = InvalidPid;                                
      slot->generation++;
      slot->terminate = false;
      generation = slot->generation;
      if (parallel)
      {
        BackgroundWorkerData->parallel_register_count++;
      }

         
                                                                       
                                
         
      pg_write_barrier();

      slot->in_use = true;
      success = true;
      break;
    }
  }

  LWLockRelease(BackgroundWorkerLock);

                                                                     
  if (success)
  {
    SendPostmasterSignal(PMSIGNAL_BACKGROUND_WORKER_CHANGE);
  }

     
                                                                           
     
  if (success && handle)
  {
    *handle = palloc(sizeof(BackgroundWorkerHandle));
    (*handle)->slot = slotno;
    (*handle)->generation = generation;
  }

  return success;
}

   
                                                              
   
                                                                       
                                                                          
                                                                               
                                                                          
   
                                                                           
                                                                         
                                                                          
                                                                          
                                                                         
                                                                             
                                                                         
             
   
BgwHandleStatus
GetBackgroundWorkerPid(BackgroundWorkerHandle *handle, pid_t *pidp)
{
  BackgroundWorkerSlot *slot;
  pid_t pid;

  Assert(handle->slot < max_worker_processes);
  slot = &BackgroundWorkerData->slot[handle->slot];

     
                                                                          
                                                                        
                                                                        
                                      
     
  LWLockAcquire(BackgroundWorkerLock, LW_SHARED);

     
                                                                           
                                                                           
                                                                        
                                                                          
                                                  
     
                                                                             
                                                              
     
  if (handle->generation != slot->generation || !slot->in_use)
  {
    pid = 0;
  }
  else
  {
    pid = slot->pid;
  }

                 
  LWLockRelease(BackgroundWorkerLock);

  if (pid == 0)
  {
    return BGWH_STOPPED;
  }
  else if (pid == InvalidPid)
  {
    return BGWH_NOT_YET_STARTED;
  }
  *pidp = pid;
  return BGWH_STARTED;
}

   
                                             
   
                                                                            
                                                                             
                                                                         
                                                                          
               
   
                                                                      
                                                                        
   
BgwHandleStatus
WaitForBackgroundWorkerStartup(BackgroundWorkerHandle *handle, pid_t *pidp)
{
  BgwHandleStatus status;
  int rc;

  for (;;)
  {
    pid_t pid;

    CHECK_FOR_INTERRUPTS();

    status = GetBackgroundWorkerPid(handle, &pid);
    if (status == BGWH_STARTED)
    {
      *pidp = pid;
    }
    if (status != BGWH_NOT_YET_STARTED)
    {
      break;
    }

    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH, 0, WAIT_EVENT_BGWORKER_STARTUP);

    if (rc & WL_POSTMASTER_DEATH)
    {
      status = BGWH_POSTMASTER_DIED;
      break;
    }

    ResetLatch(MyLatch);
  }

  return status;
}

   
                                         
   
                                                                           
                                                                               
                                                                        
                                              
   
                                                                      
                                                                        
   
BgwHandleStatus
WaitForBackgroundWorkerShutdown(BackgroundWorkerHandle *handle)
{
  BgwHandleStatus status;
  int rc;

  for (;;)
  {
    pid_t pid;

    CHECK_FOR_INTERRUPTS();

    status = GetBackgroundWorkerPid(handle, &pid);
    if (status == BGWH_STOPPED)
    {
      break;
    }

    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH, 0, WAIT_EVENT_BGWORKER_SHUTDOWN);

    if (rc & WL_POSTMASTER_DEATH)
    {
      status = BGWH_POSTMASTER_DIED;
      break;
    }

    ResetLatch(MyLatch);
  }

  return status;
}

   
                                                             
   
                                                                          
                                                                          
                 
   
void
TerminateBackgroundWorker(BackgroundWorkerHandle *handle)
{
  BackgroundWorkerSlot *slot;
  bool signal_postmaster = false;

  Assert(handle->slot < max_worker_processes);
  slot = &BackgroundWorkerData->slot[handle->slot];

                                                                         
  LWLockAcquire(BackgroundWorkerLock, LW_EXCLUSIVE);
  if (handle->generation == slot->generation)
  {
    slot->terminate = true;
    signal_postmaster = true;
  }
  LWLockRelease(BackgroundWorkerLock);

                                                                     
  if (signal_postmaster)
  {
    SendPostmasterSignal(PMSIGNAL_BACKGROUND_WORKER_CHANGE);
  }
}

   
                                                                
   
                                                                            
                                                                    
                                                                       
   
                                                                         
                                                                       
                                                                        
                                                                        
                                                                             
                                                    
   
                                                                          
                                                                         
                                                                         
   
static bgworker_main_type
LookupBackgroundWorkerFunction(const char *libraryname, const char *funcname)
{
     
                                                                      
                              
     
  if (strcmp(libraryname, "postgres") == 0)
  {
    int i;

    for (i = 0; i < lengthof(InternalBGWorkers); i++)
    {
      if (strcmp(InternalBGWorkers[i].fn_name, funcname) == 0)
      {
        return InternalBGWorkers[i].fn_addr;
      }
    }

                                                      
    elog(ERROR, "internal function \"%s\" not found", funcname);
  }

                                             
  return (bgworker_main_type)load_external_function(libraryname, funcname, true, NULL);
}

   
                                                                            
                                  
   
                                                                              
                                                                              
                                                                       
   
const char *
GetBackgroundWorkerTypeByPid(pid_t pid)
{
  int slotno;
  bool found = false;
  static char result[BGW_MAXLEN];

  LWLockAcquire(BackgroundWorkerLock, LW_SHARED);

  for (slotno = 0; slotno < BackgroundWorkerData->total_slots; slotno++)
  {
    BackgroundWorkerSlot *slot = &BackgroundWorkerData->slot[slotno];

    if (slot->pid > 0 && slot->pid == pid)
    {
      strcpy(result, slot->worker.bgw_type);
      found = true;
      break;
    }
  }

  LWLockRelease(BackgroundWorkerLock);

  if (!found)
  {
    return NULL;
  }

  return result;
}
