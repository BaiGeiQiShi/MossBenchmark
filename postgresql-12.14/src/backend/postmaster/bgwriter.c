                                                                            
   
              
   
                                                                            
                                                                          
                                                                             
                                                                            
                                                                              
                                                                            
                         
   
                                                                  
   
                                                                               
                                                                             
                                                                   
                                                                              
                                                                            
                                     
   
                                                                           
                                                                             
                                                                  
   
   
                                                                         
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "storage/bufmgr.h"
#include "storage/buf_internals.h"
#include "storage/condition_variable.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "storage/standby.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner.h"
#include "utils/timestamp.h"

   
                  
   
int BgWriterDelay = 200;

   
                                                                     
                                            
   
#define HIBERNATE_FACTOR 50

   
                                                                          
                 
   
#define LOG_SNAPSHOT_INTERVAL_MS 15000

   
                                                                              
                                                                              
                  
   
static TimestampTz last_snapshot_ts;
static XLogRecPtr last_snapshot_lsn = InvalidXLogRecPtr;

   
                                                                       
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;

                     

static void bg_quickdie(SIGNAL_ARGS);
static void BgSigHupHandler(SIGNAL_ARGS);
static void ReqShutdownHandler(SIGNAL_ARGS);
static void bgwriter_sigusr1_handler(SIGNAL_ARGS);

   
                                         
   
                                                                            
                                                             
   
void
BackgroundWriterMain(void)
{
  sigjmp_buf local_sigjmp_buf;
  MemoryContext bgwriter_context;
  bool prev_hibernate;
  WritebackContext wb_context;

     
                                                                     
     
                                                                          
                                                
     
  pqsignal(SIGHUP, BgSigHupHandler);                                   
  pqsignal(SIGINT, SIG_IGN);
  pqsignal(SIGTERM, ReqShutdownHandler);               
  pqsignal(SIGQUIT, bg_quickdie);                             
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, bgwriter_sigusr1_handler);
  pqsignal(SIGUSR2, SIG_IGN);

     
                                                                     
     
  pqsignal(SIGCHLD, SIG_DFL);

                                                
  sigdelset(&BlockSig, SIGQUIT);

     
                                                                 
                               
     
  last_snapshot_ts = GetCurrentTimestamp();

     
                                                                             
                                                                           
                                                            
                                                                      
     
  bgwriter_context = AllocSetContextCreate(TopMemoryContext, "Background Writer", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(bgwriter_context);

  WritebackContextInit(&wb_context, &bgwriter_flush_after);

     
                                                              
     
                                                              
     
  if (sigsetjmp(local_sigjmp_buf, 1) != 0)
  {
                                                                
    error_context_stack = NULL;

                                              
    HOLD_INTERRUPTS();

                                            
    EmitErrorReport();

       
                                                            
                                                                       
                                                                           
       
    LWLockReleaseAll();
    ConditionVariableCancelSleep();
    AbortBufferIO();
    UnlockBuffers();
    ReleaseAuxProcessResources(false);
    AtEOXact_Buffers(false);
    AtEOXact_SMgr();
    AtEOXact_Files(false);
    AtEOXact_HashTables(false);

       
                                                                         
                  
       
    MemoryContextSwitchTo(bgwriter_context);
    FlushErrorState();

                                                        
    MemoryContextResetAndDeleteChildren(bgwriter_context);

                                                                 
    WritebackContextInit(&wb_context, &bgwriter_flush_after);

                                           
    RESUME_INTERRUPTS();

       
                                                                         
                                                                         
                       
       
    pg_usleep(1000000L);

       
                                                                          
                                                                       
                                                                
       
    smgrcloseall();

                                                                            
    pgstat_report_wait_end();
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

     
                                                                       
     
  PG_SETMASK(&UnBlockSig);

     
                                              
     
  prev_hibernate = false;

     
                  
     
  for (;;)
  {
    bool can_hibernate;
    int rc;

                                           
    ResetLatch(MyLatch);

    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
    }
    if (shutdown_requested)
    {
         
                                                                     
                                                   
         
      ExitOnAnyError = true;
                                                 
      proc_exit(0);           
    }

       
                                             
       
    can_hibernate = BgBufferSync(&wb_context);

       
                                                           
       
    pgstat_send_bgwriter();

    if (FirstCallSinceLastCheckpoint())
    {
         
                                                                    
                                                                        
         
      smgrcloseall();
    }

       
                                                                        
                                                                  
                                                                  
                                                                       
                                                             
       
                                                              
                                                                           
                                                                          
                                                                      
                                                                         
                                                                       
                                                                         
                                     
       
                                                                         
                                                                       
                                                                          
                                       
       
    if (XLogStandbyInfoActive() && !RecoveryInProgress())
    {
      TimestampTz timeout = 0;
      TimestampTz now = GetCurrentTimestamp();

      timeout = TimestampTzPlusMilliseconds(last_snapshot_ts, LOG_SNAPSHOT_INTERVAL_MS);

         
                                                                         
                                                                         
                                                                     
                                                                       
                                
         
      if (now >= timeout && last_snapshot_lsn <= GetLastImportantRecPtr())
      {
        last_snapshot_lsn = LogStandbySnapshot();
        last_snapshot_ts = now;
      }
    }

       
                                                                 
       
                                                                         
                                                                           
                                                                         
                                                                         
                                                                          
                         
       
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, BgWriterDelay         , WAIT_EVENT_BGWRITER_MAIN);

       
                                                                           
                                                                       
                                                                          
                                                                         
                                                                           
                                                                          
                                                                         
                                                        
       
                                                                          
                                                                        
                                                                    
                                                                           
                                                                           
                                                                   
                                                                   
       
    if (rc == WL_TIMEOUT && can_hibernate && prev_hibernate)
    {
                                                          
      StrategyNotifyBgWriter(MyProc->pgprocno);
                     
      (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, BgWriterDelay * HIBERNATE_FACTOR, WAIT_EVENT_BGWRITER_HIBERNATE);
                                                               
      StrategyNotifyBgWriter(-1);
    }

    prev_hibernate = can_hibernate;
  }
}

                                    
                            
                                    
   

   
                                                                  
   
                                     
                                                 
   
static void
bg_quickdie(SIGNAL_ARGS)
{
     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

                                                                     
static void
BgSigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  SetLatch(MyLatch);

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

                                     
static void
bgwriter_sigusr1_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  latch_sigusr1_handler();

  errno = save_errno;
}
