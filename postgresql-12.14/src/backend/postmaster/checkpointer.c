                                                                            
   
                  
   
                                                                            
                                                                               
                                                                           
                                                                             
                                                                           
                                                                    
               
   
                                                                        
                                                                              
                                                                              
                                                                         
                                                                          
                                                                            
                                                                             
   
                                                                               
                                                                             
                                                                            
                                                                       
                                                                     
                                
   
   
                                                                         
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "replication/syncrep.h"
#include "storage/bufmgr.h"
#include "storage/condition_variable.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

             
                                                                          
   
                                                                            
                                            
                                                                           
                                                                
                                                                   
                         
                                                                     
                                               
   
                                  
                                                                 
                                                 
                                         
                                                                       
                                                                         
                                                                               
                                        
                                                                         
                                                                     
                                                                   
                  
                                                                   
                                                                     
   
                                                                       
                                                                       
                                                                          
   
                                                                             
                                                                          
                                                                       
                                                                          
                                                            
   
                                                                        
                                 
   
                                                                            
                                                              
             
   
typedef struct
{
  SyncRequestType type;                   
  FileTag ftag;                              
} CheckpointerRequest;

typedef struct
{
  pid_t checkpointer_pid;                             

  slock_t ckpt_lck;                                     

  int ckpt_started;                                      
  int ckpt_done;                                       
  int ckpt_failed;                                      

  int ckpt_flags;                                             

  ConditionVariable start_cv;                                          
  ConditionVariable done_cv;                                        

  uint32 num_backend_writes;                                        
  uint32 num_backend_fsync;                                       

  int num_requests;                            
  int max_requests;                           
  CheckpointerRequest requests[FLEXIBLE_ARRAY_MEMBER];
} CheckpointerShmemStruct;

static CheckpointerShmemStruct *CheckpointerShmem;

                                                                     
#define WRITES_PER_ABSORB 1000

   
                  
   
int CheckPointTimeout = 300;
int CheckPointWarning = 30;
double CheckPointCompletionTarget = 0.5;

   
                                                                       
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;

   
                 
   
static bool ckpt_active = false;

                                                      
static pg_time_t ckpt_start_time;
static XLogRecPtr ckpt_start_recptr;
static double ckpt_cached_elapsed;

static pg_time_t last_checkpoint_time;
static pg_time_t last_xlog_switch_time;

                                      

static void
CheckArchiveTimeout(void);
static bool
IsCheckpointOnSchedule(double progress);
static bool
ImmediateCheckpointRequested(void);
static bool
CompactCheckpointerRequestQueue(void);
static void
UpdateSharedMemoryConfig(void);

                     

static void chkpt_quickdie(SIGNAL_ARGS);
static void ChkptSigHupHandler(SIGNAL_ARGS);
static void ReqCheckpointHandler(SIGNAL_ARGS);
static void chkpt_sigusr1_handler(SIGNAL_ARGS);
static void ReqShutdownHandler(SIGNAL_ARGS);

   
                                             
   
                                                                            
                                                             
   
void
CheckpointerMain(void)
{
  sigjmp_buf local_sigjmp_buf;
  MemoryContext checkpointer_context;

  CheckpointerShmem->checkpointer_pid = MyProcPid;

     
                                                                    
     
                                                                          
                                                                         
                                                                          
                                                   
     
  pqsignal(SIGHUP, ChkptSigHupHandler);                                     
  pqsignal(SIGINT, ReqCheckpointHandler);                         
  pqsignal(SIGTERM, SIG_IGN);                                 
  pqsignal(SIGQUIT, chkpt_quickdie);                           
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, chkpt_sigusr1_handler);
  pqsignal(SIGUSR2, ReqShutdownHandler);                       

     
                                                                     
     
  pqsignal(SIGCHLD, SIG_DFL);

                                                
  sigdelset(&BlockSig, SIGQUIT);

     
                                                                             
     
  last_checkpoint_time = last_xlog_switch_time = (pg_time_t)time(NULL);

     
                                                                             
                                                                           
                                                            
                                                                      
     
  checkpointer_context = AllocSetContextCreate(TopMemoryContext, "Checkpointer", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(checkpointer_context);

     
                                                              
     
                                                              
     
  if (sigsetjmp(local_sigjmp_buf, 1) != 0)
  {
                                                                
    error_context_stack = NULL;

                                              
    HOLD_INTERRUPTS();

                                            
    EmitErrorReport();

       
                                                            
                                                                       
                                                                        
              
       
    LWLockReleaseAll();
    ConditionVariableCancelSleep();
    pgstat_report_wait_end();
    AbortBufferIO();
    UnlockBuffers();
    ReleaseAuxProcessResources(false);
    AtEOXact_Buffers(false);
    AtEOXact_SMgr();
    AtEOXact_Files(false);
    AtEOXact_HashTables(false);

                                                               
    if (ckpt_active)
    {
      SpinLockAcquire(&CheckpointerShmem->ckpt_lck);
      CheckpointerShmem->ckpt_failed++;
      CheckpointerShmem->ckpt_done = CheckpointerShmem->ckpt_started;
      SpinLockRelease(&CheckpointerShmem->ckpt_lck);

      ConditionVariableBroadcast(&CheckpointerShmem->done_cv);

      ckpt_active = false;
    }

       
                                                                         
                  
       
    MemoryContextSwitchTo(checkpointer_context);
    FlushErrorState();

                                                        
    MemoryContextResetAndDeleteChildren(checkpointer_context);

                                           
    RESUME_INTERRUPTS();

       
                                                                         
                                                                         
                       
       
    pg_usleep(1000000L);

       
                                                                          
                                                                       
                                                                
       
    smgrcloseall();
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

     
                                                                       
     
  PG_SETMASK(&UnBlockSig);

     
                                                                             
                                                                          
     
  UpdateSharedMemoryConfig();

     
                                                                         
               
     
  ProcGlobal->checkpointerLatch = &MyProc->procLatch;

     
                  
     
  for (;;)
  {
    bool do_checkpoint = false;
    int flags = 0;
    pg_time_t now;
    int elapsed_secs;
    int cur_timeout;

                                           
    ResetLatch(MyLatch);

       
                                                          
       
    AbsorbSyncRequests();

    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);

         
                                                                        
                                                                         
                                                       
         
                                                                        
                                                                      
                                                                     
                                                                
                                                      
         
      UpdateSharedMemoryConfig();
    }
    if (shutdown_requested)
    {
         
                                                                     
                                                   
         
      ExitOnAnyError = true;
                                   
      ShutdownXLOG(0, 0);
                                                     
      proc_exit(0);           
    }

       
                                                                         
                                                                           
                          
       
    if (((volatile CheckpointerShmemStruct *)CheckpointerShmem)->ckpt_flags)
    {
      do_checkpoint = true;
      BgWriterStats.m_requested_checkpoints++;
    }

       
                                                                           
                                                                     
                                                                          
                                                      
       
    now = (pg_time_t)time(NULL);
    elapsed_secs = now - last_checkpoint_time;
    if (elapsed_secs >= CheckPointTimeout)
    {
      if (!do_checkpoint)
      {
        BgWriterStats.m_timed_checkpoints++;
      }
      do_checkpoint = true;
      flags |= CHECKPOINT_CAUSE_TIME;
    }

       
                                     
       
    if (do_checkpoint)
    {
      bool ckpt_performed = false;
      bool do_restartpoint;

         
                                                                         
                                                                     
                           
         
      do_restartpoint = RecoveryInProgress();

         
                                                                         
                                                                        
                                                             
         
      SpinLockAcquire(&CheckpointerShmem->ckpt_lck);
      flags |= CheckpointerShmem->ckpt_flags;
      CheckpointerShmem->ckpt_flags = 0;
      CheckpointerShmem->ckpt_started++;
      SpinLockRelease(&CheckpointerShmem->ckpt_lck);

      ConditionVariableBroadcast(&CheckpointerShmem->start_cv);

         
                                                                    
                                                  
         
      if (flags & CHECKPOINT_END_OF_RECOVERY)
      {
        do_restartpoint = false;
      }

         
                                                                      
                                                                        
                                                                        
                                                             
                                                
         
      if (!do_restartpoint && (flags & CHECKPOINT_CAUSE_XLOG) && elapsed_secs < CheckPointWarning)
      {
        ereport(LOG, (errmsg_plural("checkpoints are occurring too frequently (%d second apart)", "checkpoints are occurring too frequently (%d seconds apart)", elapsed_secs, elapsed_secs), errhint("Consider increasing the configuration parameter \"max_wal_size\".")));
      }

         
                                                               
                     
         
      ckpt_active = true;
      if (do_restartpoint)
      {
        ckpt_start_recptr = GetXLogReplayRecPtr(NULL);
      }
      else
      {
        ckpt_start_recptr = GetInsertRecPtr();
      }
      ckpt_start_time = now;
      ckpt_cached_elapsed = 0;

         
                            
         
      if (!do_restartpoint)
      {
        CreateCheckPoint(flags);
        ckpt_performed = true;
      }
      else
      {
        ckpt_performed = CreateRestartPoint(flags);
      }

         
                                                                    
                                                                        
         
      smgrcloseall();

         
                                                                 
         
      SpinLockAcquire(&CheckpointerShmem->ckpt_lck);
      CheckpointerShmem->ckpt_done = CheckpointerShmem->ckpt_started;
      SpinLockRelease(&CheckpointerShmem->ckpt_lck);

      ConditionVariableBroadcast(&CheckpointerShmem->done_cv);

      if (ckpt_performed)
      {
           
                                                                    
                                                              
                                                        
           
        last_checkpoint_time = now;
      }
      else
      {
           
                                                                     
                                                                     
                                                                      
                                                 
           
        last_checkpoint_time = now - CheckPointTimeout + 15;
      }

      ckpt_active = false;
    }

                                                                       
    CheckArchiveTimeout();

       
                                                                         
                                                                         
                                                                        
                                                                         
                             
       
    pgstat_send_bgwriter();

       
                                                                          
                         
       
    now = (pg_time_t)time(NULL);
    elapsed_secs = now - last_checkpoint_time;
    if (elapsed_secs >= CheckPointTimeout)
    {
      continue;                          
    }
    cur_timeout = CheckPointTimeout - elapsed_secs;
    if (XLogArchiveTimeout > 0 && !RecoveryInProgress())
    {
      elapsed_secs = now - last_xlog_switch_time;
      if (elapsed_secs >= XLogArchiveTimeout)
      {
        continue;                          
      }
      cur_timeout = Min(cur_timeout, XLogArchiveTimeout - elapsed_secs);
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, cur_timeout * 1000L                    , WAIT_EVENT_CHECKPOINTER_MAIN);
  }
}

   
                                                                          
   
                                                                         
                                                                               
                                                                               
                                                                          
                                                                   
                                                                         
                                                                        
            
   
static void
CheckArchiveTimeout(void)
{
  pg_time_t now;
  pg_time_t last_time;
  XLogRecPtr last_switch_lsn;

  if (XLogArchiveTimeout <= 0 || RecoveryInProgress())
  {
    return;
  }

  now = (pg_time_t)time(NULL);

                                                                   
  if ((int)(now - last_xlog_switch_time) < XLogArchiveTimeout)
  {
    return;
  }

     
                                                                             
                                            
     
  last_time = GetLastSegSwitchData(&last_switch_lsn);

  last_xlog_switch_time = Max(last_xlog_switch_time, last_time);

                                     
  if ((int)(now - last_xlog_switch_time) >= XLogArchiveTimeout)
  {
       
                                                                          
                                                                     
                            
       
    if (GetLastImportantRecPtr() > last_switch_lsn)
    {
      XLogRecPtr switchpoint;

                                                                     
      switchpoint = RequestXLogSwitch(true);

         
                                                                       
                                  
         
      if (XLogSegmentOffset(switchpoint, wal_segment_size) != 0)
      {
        elog(DEBUG1, "write-ahead log switch forced (archive_timeout=%d)", XLogArchiveTimeout);
      }
    }

       
                                                                       
                       
       
    last_xlog_switch_time = now;
  }
}

   
                                                                           
                                                                              
                                    
   
static bool
ImmediateCheckpointRequested(void)
{
  volatile CheckpointerShmemStruct *cps = CheckpointerShmem;

     
                                                                           
                                   
     
  if (cps->ckpt_flags & CHECKPOINT_IMMEDIATE)
  {
    return true;
  }
  return false;
}

   
                                                      
   
                                                                            
                                                                     
                                 
   
                                                                            
                                                                           
   
                                                                         
                                                                
   
void
CheckpointWriteDelay(int flags, double progress)
{
  static int absorb_counter = WRITES_PER_ABSORB;

                                                                              
  if (!AmCheckpointerProcess())
  {
    return;
  }

     
                                                                            
                                                                   
     
  if (!(flags & CHECKPOINT_IMMEDIATE) && !shutdown_requested && !ImmediateCheckpointRequested() && IsCheckpointOnSchedule(progress))
  {
    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
                                                   
      UpdateSharedMemoryConfig();
    }

    AbsorbSyncRequests();
    absorb_counter = WRITES_PER_ABSORB;

    CheckArchiveTimeout();

       
                                                                  
       
    pgstat_send_bgwriter();

       
                                                                           
                                                                      
                                                                       
              
       
    pg_usleep(100000L);
  }
  else if (--absorb_counter <= 0)
  {
       
                                                                        
                                                                       
                            
       
    AbsorbSyncRequests();
    absorb_counter = WRITES_PER_ABSORB;
  }
}

   
                                                                          
                                
   
                                                                              
                                                                               
                                   
   
static bool
IsCheckpointOnSchedule(double progress)
{
  XLogRecPtr recptr;
  struct timeval now;
  double elapsed_xlogs, elapsed_time;

  Assert(ckpt_active);

                                                                 
  progress *= CheckPointCompletionTarget;

     
                                                                      
                                                                        
                                                                   
                                                                             
     
  if (progress < ckpt_cached_elapsed)
  {
    return false;
  }

     
                                                                         
     
                                                                     
                                                                           
                                                                        
                                                                      
                                                                           
                      
     
                                                                          
                                                                             
                                                                            
                                                                             
                                                                           
                                                                            
                                                                         
                                                                            
                                                           
     
  if (RecoveryInProgress())
  {
    recptr = GetXLogReplayRecPtr(NULL);
  }
  else
  {
    recptr = GetInsertRecPtr();
  }
  elapsed_xlogs = (((double)(recptr - ckpt_start_recptr)) / wal_segment_size) / CheckPointSegments;

  if (progress < elapsed_xlogs)
  {
    ckpt_cached_elapsed = elapsed_xlogs;
    return false;
  }

     
                                                                 
     
  gettimeofday(&now, NULL);
  elapsed_time = ((double)((pg_time_t)now.tv_sec - ckpt_start_time) + now.tv_usec / 1000000.0) / CheckPointTimeout;

  if (progress < elapsed_time)
  {
    ckpt_cached_elapsed = elapsed_time;
    return false;
  }

                                        
  return true;
}

                                    
                            
                                    
   

   
                                                                     
   
                                     
                                                 
   
static void
chkpt_quickdie(SIGNAL_ARGS)
{
     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

                                                                     
static void
ChkptSigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                            
static void
ReqCheckpointHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                                                          
                                                                       
     
  SetLatch(MyLatch);

  errno = save_errno;
}

                                     
static void
chkpt_sigusr1_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  latch_sigusr1_handler();

  errno = save_errno;
}

                                                             
static void
ReqShutdownHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  shutdown_requested = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                    
                                
                                    
   

   
                         
                                                                
   
Size
CheckpointerShmemSize(void)
{
  Size size;

     
                                                                             
                                                      
     
  size = offsetof(CheckpointerShmemStruct, requests);
  size = add_size(size, mul_size(NBuffers, sizeof(CheckpointerRequest)));

  return size;
}

   
                         
                                                               
   
void
CheckpointerShmemInit(void)
{
  Size size = CheckpointerShmemSize();
  bool found;

  CheckpointerShmem = (CheckpointerShmemStruct *)ShmemInitStruct("Checkpointer Data", size, &found);

  if (!found)
  {
       
                                                                       
                                                                           
                                                                    
       
    MemSet(CheckpointerShmem, 0, size);
    SpinLockInit(&CheckpointerShmem->ckpt_lck);
    CheckpointerShmem->max_requests = NBuffers;
    ConditionVariableInit(&CheckpointerShmem->start_cv);
    ConditionVariableInit(&CheckpointerShmem->done_cv);
  }
}

   
                     
                                                        
   
                                           
                                                                
                                                                      
                                                     
                                                     
                                                                              
                                                             
                                 
                                                                     
                                                    
                                                                       
                                                                         
   
void
RequestCheckpoint(int flags)
{
  int ntries;
  int old_failed, old_started;

     
                                                       
     
  if (!IsPostmasterEnvironment)
  {
       
                                                                           
                                                                       
       
    CreateCheckPoint(flags | CHECKPOINT_IMMEDIATE);

       
                                                                        
                                                                
       
    smgrcloseall();

    return;
  }

     
                                                                            
                                                                           
                                     
     
                                                                            
                                                                       
                               
     
  SpinLockAcquire(&CheckpointerShmem->ckpt_lck);

  old_failed = CheckpointerShmem->ckpt_failed;
  old_started = CheckpointerShmem->ckpt_started;
  CheckpointerShmem->ckpt_flags |= (flags | CHECKPOINT_REQUESTED);

  SpinLockRelease(&CheckpointerShmem->ckpt_lck);

     
                                                                             
                                                                            
                                                                           
                                                                   
                                                                            
                                                                          
                                                                           
                                                                       
     
#define MAX_SIGNAL_TRIES 600                        
  for (ntries = 0;; ntries++)
  {
    if (CheckpointerShmem->checkpointer_pid == 0)
    {
      if (ntries >= MAX_SIGNAL_TRIES || !(flags & CHECKPOINT_WAIT))
      {
        elog((flags & CHECKPOINT_WAIT) ? ERROR : LOG, "could not signal for checkpoint: checkpointer is not running");
        break;
      }
    }
    else if (kill(CheckpointerShmem->checkpointer_pid, SIGINT) != 0)
    {
      if (ntries >= MAX_SIGNAL_TRIES || !(flags & CHECKPOINT_WAIT))
      {
        elog((flags & CHECKPOINT_WAIT) ? ERROR : LOG, "could not signal for checkpoint: %m");
        break;
      }
    }
    else
    {
      break;                               
    }

    CHECK_FOR_INTERRUPTS();
    pg_usleep(100000L);                               
  }

     
                                                                           
                                
     
  if (flags & CHECKPOINT_WAIT)
  {
    int new_started, new_failed;

                                             
    ConditionVariablePrepareToSleep(&CheckpointerShmem->start_cv);
    for (;;)
    {
      SpinLockAcquire(&CheckpointerShmem->ckpt_lck);
      new_started = CheckpointerShmem->ckpt_started;
      SpinLockRelease(&CheckpointerShmem->ckpt_lck);

      if (new_started != old_started)
      {
        break;
      }

      ConditionVariableSleep(&CheckpointerShmem->start_cv, WAIT_EVENT_CHECKPOINT_START);
    }
    ConditionVariableCancelSleep();

       
                                                                       
       
    ConditionVariablePrepareToSleep(&CheckpointerShmem->done_cv);
    for (;;)
    {
      int new_done;

      SpinLockAcquire(&CheckpointerShmem->ckpt_lck);
      new_done = CheckpointerShmem->ckpt_done;
      new_failed = CheckpointerShmem->ckpt_failed;
      SpinLockRelease(&CheckpointerShmem->ckpt_lck);

      if (new_done - new_started >= 0)
      {
        break;
      }

      ConditionVariableSleep(&CheckpointerShmem->done_cv, WAIT_EVENT_CHECKPOINT_DONE);
    }
    ConditionVariableCancelSleep();

    if (new_failed != old_failed)
    {
      ereport(ERROR, (errmsg("checkpoint request failed"), errhint("Consult recent messages in the server log for details.")));
    }
  }
}

   
                      
                                                                    
   
                                                                   
                                                                               
                                                                           
                                                                          
                                                              
   
                                                                          
                                                                              
                                                                           
                                                                           
                                                                           
                                                                          
                                                                          
                                                                          
                                            
   
bool
ForwardSyncRequest(const FileTag *ftag, SyncRequestType type)
{
  CheckpointerRequest *request;
  bool too_full;

  if (!IsUnderPostmaster)
  {
    return false;                                       
  }

  if (AmCheckpointerProcess())
  {
    elog(ERROR, "ForwardSyncRequest must not be called in checkpointer");
  }

  LWLockAcquire(CheckpointerCommLock, LW_EXCLUSIVE);

                                                                       
  if (!AmBackgroundWriterProcess())
  {
    CheckpointerShmem->num_backend_writes++;
  }

     
                                                                         
                                                                             
                                                              
     
  if (CheckpointerShmem->checkpointer_pid == 0 || (CheckpointerShmem->num_requests >= CheckpointerShmem->max_requests && !CompactCheckpointerRequestQueue()))
  {
       
                                                                      
             
       
    if (!AmBackgroundWriterProcess())
    {
      CheckpointerShmem->num_backend_fsync++;
    }
    LWLockRelease(CheckpointerCommLock);
    return false;
  }

                          
  request = &CheckpointerShmem->requests[CheckpointerShmem->num_requests++];
  request->ftag = *ftag;
  request->type = type;

                                                                           
  too_full = (CheckpointerShmem->num_requests >= CheckpointerShmem->max_requests / 2);

  LWLockRelease(CheckpointerCommLock);

                                                  
  if (too_full && ProcGlobal->checkpointerLatch)
  {
    SetLatch(ProcGlobal->checkpointerLatch);
  }

  return true;
}

   
                                   
                                                                      
                                                
   
                                                                            
                                                                         
                                                                          
                                                                         
                                                                            
                                                           
   
                                                                      
                                                                         
                                                        
   
static bool
CompactCheckpointerRequestQueue(void)
{
  struct CheckpointerSlotMapping
  {
    CheckpointerRequest request;
    int slot;
  };

  int n, preserve_count;
  int num_skipped = 0;
  HASHCTL ctl;
  HTAB *htab;
  bool *skip_slot;

                                                        
  Assert(LWLockHeldByMe(CheckpointerCommLock));

                                  
  skip_slot = palloc0(sizeof(bool) * CheckpointerShmem->num_requests);

                                       
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(CheckpointerRequest);
  ctl.entrysize = sizeof(struct CheckpointerSlotMapping);
  ctl.hcxt = CurrentMemoryContext;

  htab = hash_create("CompactCheckpointerRequestQueue", CheckpointerShmem->num_requests, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

     
                                                                           
                                                                         
                                                                        
                                                                             
                                                                  
                                                                         
                                                                            
                                                                             
                                                                          
                                                                     
     
  for (n = 0; n < CheckpointerShmem->num_requests; n++)
  {
    CheckpointerRequest *request;
    struct CheckpointerSlotMapping *slotmap;
    bool found;

       
                                                                    
                                                                          
                                                            
                                                                     
                             
       
    request = &CheckpointerShmem->requests[n];
    slotmap = hash_search(htab, request, HASH_ENTER, &found);
    if (found)
    {
                                                                   
      skip_slot[slotmap->slot] = true;
      num_skipped++;
    }
                                                                          
    slotmap->slot = n;
  }

                                 
  hash_destroy(htab);

                                            
  if (!num_skipped)
  {
    pfree(skip_slot);
    return false;
  }

                                              
  preserve_count = 0;
  for (n = 0; n < CheckpointerShmem->num_requests; n++)
  {
    if (skip_slot[n])
    {
      continue;
    }
    CheckpointerShmem->requests[preserve_count++] = CheckpointerShmem->requests[n];
  }
  ereport(DEBUG1, (errmsg("compacted fsync request queue from %d entries to %d entries", CheckpointerShmem->num_requests, preserve_count)));
  CheckpointerShmem->num_requests = preserve_count;

                
  pfree(skip_slot);
  return true;
}

   
                      
                                                                   
   
                                                                       
                                                                        
                                                                 
                                                               
   
void
AbsorbSyncRequests(void)
{
  CheckpointerRequest *requests = NULL;
  CheckpointerRequest *request;
  int n;

  if (!AmCheckpointerProcess())
  {
    return;
  }

  LWLockAcquire(CheckpointerCommLock, LW_EXCLUSIVE);

                                                          
  BgWriterStats.m_buf_written_backend += CheckpointerShmem->num_backend_writes;
  BgWriterStats.m_buf_fsync_backend += CheckpointerShmem->num_backend_fsync;

  CheckpointerShmem->num_backend_writes = 0;
  CheckpointerShmem->num_backend_fsync = 0;

     
                                                                             
                                                                  
     
                                                                            
                                                                           
                                                                             
                                                                           
                                                                          
     
  n = CheckpointerShmem->num_requests;
  if (n > 0)
  {
    requests = (CheckpointerRequest *)palloc(n * sizeof(CheckpointerRequest));
    memcpy(requests, CheckpointerShmem->requests, n * sizeof(CheckpointerRequest));
  }

  START_CRIT_SECTION();

  CheckpointerShmem->num_requests = 0;

  LWLockRelease(CheckpointerCommLock);

  for (request = requests; n > 0; request++, n--)
  {
    RememberSyncRequest(&request->ftag, request->type);
  }

  END_CRIT_SECTION();

  if (requests)
  {
    pfree(requests);
  }
}

   
                                                                      
   
static void
UpdateSharedMemoryConfig(void)
{
                                              
  SyncRepUpdateSyncStandbysDefined();

     
                                                                            
                                                 
     
  UpdateFullPageWrites();

  elog(DEBUG2, "checkpointer updated shared memory configuration values");
}

   
                                                                        
                                                                              
   
bool
FirstCallSinceLastCheckpoint(void)
{
  static int ckpt_done = 0;
  int new_done;
  bool FirstCall = false;

  SpinLockAcquire(&CheckpointerShmem->ckpt_lck);
  new_done = CheckpointerShmem->ckpt_done;
  SpinLockRelease(&CheckpointerShmem->ckpt_lck);

  if (new_done != ckpt_done)
  {
    FirstCall = true;
  }

  ckpt_done = new_done;

  return FirstCall;
}
