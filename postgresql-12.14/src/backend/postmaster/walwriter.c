                                                                            
   
               
   
                                                                             
                                                                            
                                                                           
                                                                         
                                                                          
                                                     
   
                                                                           
                                                                             
                                                                          
                                        
   
                                                                         
                                                                            
                                                                             
                                                                               
                                  
   
                                                                                
                                                                              
                                                                               
                                                                             
                                     
   
                                                                            
                                                                             
                                                                  
   
   
                                                                         
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/xlog.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/walwriter.h"
#include "storage/bufmgr.h"
#include "storage/condition_variable.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

   
                  
   
int WalWriterDelay = 200;
int WalWriterFlushAfter = 128;

   
                                                                         
                                                                         
                                            
   
#define LOOPS_UNTIL_HIBERNATE 50
#define HIBERNATE_FACTOR 25

   
                                                                       
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;

                     
static void wal_quickdie(SIGNAL_ARGS);
static void WalSigHupHandler(SIGNAL_ARGS);
static void WalShutdownHandler(SIGNAL_ARGS);
static void walwriter_sigusr1_handler(SIGNAL_ARGS);

   
                                          
   
                                                                            
                                                             
   
void
WalWriterMain(void)
{
  sigjmp_buf local_sigjmp_buf;
  MemoryContext walwriter_context;
  int left_till_hibernate;
  bool hibernating;

     
                                                                    
     
                                                                   
                                       
     
  pqsignal(SIGHUP, WalSigHupHandler);                                      
  pqsignal(SIGINT, WalShutdownHandler);                        
  pqsignal(SIGTERM, WalShutdownHandler);                       
  pqsignal(SIGQUIT, wal_quickdie);                            
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, walwriter_sigusr1_handler);
  pqsignal(SIGUSR2, SIG_IGN);               

     
                                                                     
     
  pqsignal(SIGCHLD, SIG_DFL);

                                                
  sigdelset(&BlockSig, SIGQUIT);

     
                                                                             
                                                                           
                                                            
                                                                      
     
  walwriter_context = AllocSetContextCreate(TopMemoryContext, "Wal Writer", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(walwriter_context);

     
                                                              
     
                                                    
     
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

       
                                                                         
                  
       
    MemoryContextSwitchTo(walwriter_context);
    FlushErrorState();

                                                        
    MemoryContextResetAndDeleteChildren(walwriter_context);

                                           
    RESUME_INTERRUPTS();

       
                                                                         
                                                                         
                       
       
    pg_usleep(1000000L);

       
                                                                          
                                                                       
                                                                
       
    smgrcloseall();
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

     
                                                                       
     
  PG_SETMASK(&UnBlockSig);

     
                                              
     
  left_till_hibernate = LOOPS_UNTIL_HIBERNATE;
  hibernating = false;
  SetWalWriterSleeping(false);

     
                                                                         
               
     
  ProcGlobal->walwriterLatch = &MyProc->procLatch;

     
                  
     
  for (;;)
  {
    long cur_timeout;

       
                                                                       
                                                                        
                                                                       
                                                                         
                                                                          
                                                                       
                                                              
       
    if (hibernating != (left_till_hibernate <= 1))
    {
      hibernating = (left_till_hibernate <= 1);
      SetWalWriterSleeping(hibernating);
    }

                                           
    ResetLatch(MyLatch);

       
                                                          
       
    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
    }
    if (shutdown_requested)
    {
                                                  
      proc_exit(0);           
    }

       
                                                                           
                                              
       
    if (XLogBackgroundFlush())
    {
      left_till_hibernate = LOOPS_UNTIL_HIBERNATE;
    }
    else if (left_till_hibernate > 0)
    {
      left_till_hibernate--;
    }

       
                                                                         
                                                                      
                                                                       
       
    if (left_till_hibernate > 0)
    {
      cur_timeout = WalWriterDelay;            
    }
    else
    {
      cur_timeout = WalWriterDelay * HIBERNATE_FACTOR;
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, cur_timeout, WAIT_EVENT_WAL_WRITER_MAIN);
  }
}

                                    
                            
                                    
   

   
                                                                   
   
                                     
                                                 
   
static void
wal_quickdie(SIGNAL_ARGS)
{
     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

                                                                     
static void
WalSigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                        
static void
WalShutdownHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  shutdown_requested = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                     
static void
walwriter_sigusr1_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  latch_sigusr1_handler();

  errno = save_errno;
}
