                                                                            
   
             
   
                                                                        
                                                                         
                                                                         
                                                                      
            
   
   
                                                                         
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/xlog.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/startup.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "storage/standby.h"
#include "utils/guc.h"
#include "utils/timeout.h"

   
                                                                       
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;
static volatile sig_atomic_t promote_triggered = false;

   
                                                                             
                                     
   
static volatile sig_atomic_t in_restore_command = false;

                     
static void startupproc_quickdie(SIGNAL_ARGS);
static void StartupProcSigUsr1Handler(SIGNAL_ARGS);
static void StartupProcTriggerHandler(SIGNAL_ARGS);
static void StartupProcSigHupHandler(SIGNAL_ARGS);

               
static void
StartupProcExit(int code, Datum arg);

                                    
                            
                                    
   

   
                                                                           
   
                                     
                                                 
   
static void
startupproc_quickdie(SIGNAL_ARGS)
{
     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

                                                   
static void
StartupProcSigUsr1Handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  latch_sigusr1_handler();

  errno = save_errno;
}

                                          
static void
StartupProcTriggerHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  promote_triggered = true;
  WakeupRecovery();

  errno = save_errno;
}

                                                                     
static void
StartupProcSigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  WakeupRecovery();

  errno = save_errno;
}

                                              
static void
StartupProcShutdownHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  if (in_restore_command)
  {
    proc_exit(1);
  }
  else
  {
    shutdown_requested = true;
  }
  WakeupRecovery();

  errno = save_errno;
}

                                                          
void
HandleStartupProcInterrupts(void)
{
     
                                                        
     
  if (got_SIGHUP)
  {
    got_SIGHUP = false;
    ProcessConfigFile(PGC_SIGHUP);
  }

     
                                                                    
     
  if (shutdown_requested)
  {
    proc_exit(1);
  }

     
                                                                     
                                                              
     
  if (IsUnderPostmaster && !PostmasterIsAlive())
  {
    exit(1);
  }
}

                                    
                            
                                    
   
static void
StartupProcExit(int code, Datum arg)
{
                                         
  if (standbyState != STANDBY_DISABLED)
  {
    ShutdownRecoveryTransactionEnvironment();
  }
}

                                      
                                    
                                      
   
void
StartupProcessMain(void)
{
                                                   
  on_shmem_exit(StartupProcExit, 0);

     
                                                                     
     
  pqsignal(SIGHUP, StartupProcSigHupHandler);                            
  pqsignal(SIGINT, SIG_IGN);                                              
  pqsignal(SIGTERM, StartupProcShutdownHandler);                       
  pqsignal(SIGQUIT, startupproc_quickdie);                            
  InitializeTimeouts();                                                           
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, StartupProcSigUsr1Handler);
  pqsignal(SIGUSR2, StartupProcTriggerHandler);

     
                                                                     
     
  pqsignal(SIGCHLD, SIG_DFL);

     
                                               
     
  RegisterTimeout(STANDBY_DEADLOCK_TIMEOUT, StandbyDeadLockHandler);
  RegisterTimeout(STANDBY_TIMEOUT, StandbyTimeoutHandler);
  RegisterTimeout(STANDBY_LOCK_TIMEOUT, StandbyLockTimeoutHandler);

     
                                                                       
     
  PG_SETMASK(&UnBlockSig);

     
                          
     
  StartupXLOG();

     
                                                                            
                   
     
  proc_exit(0);
}

void
PreRestoreCommand(void)
{
     
                                                                           
                                                                           
                                                                          
                                                 
     
  in_restore_command = true;
  if (shutdown_requested)
  {
    proc_exit(1);
  }
}

void
PostRestoreCommand(void)
{
  in_restore_command = false;
}

bool
IsPromoteTriggered(void)
{
  return promote_triggered;
}

void
ResetPromoteTriggered(void)
{
  promote_triggered = false;
}
