                                                                            
   
         
                                                       
   
                                                                        
                                                                      
                                                           
   
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include "miscadmin.h"
#ifdef PROFILE_PID_DIR
#include "postmaster/autovacuum.h"
#endif
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "tcop/tcopprot.h"

   
                                                                       
                                                                       
                                                                         
   
bool proc_exit_inprogress = false;

   
                                         
   
bool shmem_exit_inprogress = false;

   
                                                                         
                                  
   
static bool atexit_callback_setup = false;

                     
static void
proc_exit_prepare(int code);

                                                                    
                              
   
                                                                 
                                                                   
                                                                     
                                                                       
   
                                                                        
                                                                      
                                
                                                                    
   

#define MAX_ON_EXITS 20

struct ONEXIT
{
  pg_on_exit_callback function;
  Datum arg;
};

static struct ONEXIT on_proc_exit_list[MAX_ON_EXITS];
static struct ONEXIT on_shmem_exit_list[MAX_ON_EXITS];
static struct ONEXIT before_shmem_exit_list[MAX_ON_EXITS];

static int on_proc_exit_index, on_shmem_exit_index, before_shmem_exit_index;

                                                                    
              
   
                                                     
                                                    
   
                                                     
                
   
                                                              
                                                              
                                                                  
                                                            
                                                                    
   
void
proc_exit(int code)
{
                                                   
  proc_exit_prepare(code);

#ifdef PROFILE_PID_DIR
  {
       
                                                                       
                                                                          
                                                                         
                                                                    
                                                                           
                                                                           
                                                               
                                       
       
                                                                     
                                                                        
                                                                          
                                                                          
       
                                                                          
                                                                         
                                                                          
                                                                        
                                                                           
                   
       
    char gprofDirName[32];

    if (IsAutoVacuumWorkerProcess())
    {
      snprintf(gprofDirName, 32, "gprof/avworker");
    }
    else
    {
      snprintf(gprofDirName, 32, "gprof/%d", (int)getpid());
    }

       
                                                                         
                          
       
    mkdir("gprof", S_IRWXU | S_IRWXG | S_IRWXO);
    mkdir(gprofDirName, S_IRWXU | S_IRWXG | S_IRWXO);
    chdir(gprofDirName);
  }
#endif

  elog(DEBUG3, "exit(%d)", code);

  exit(code);
}

   
                                                                       
                                                                         
                                                
   
static void
proc_exit_prepare(int code)
{
     
                                                                          
                                                                  
     
  proc_exit_inprogress = true;

     
                                                                        
                                                                        
                                                              
     
  InterruptPending = false;
  ProcDiePending = false;
  QueryCancelPending = false;
  InterruptHoldoffCount = 1;
  CritSectionCount = 0;

     
                                                                         
                                                                             
                                                                           
                                                                        
                                                                           
                                                                      
                               
     
  error_context_stack = NULL;
                                                                           
  debug_query_string = NULL;

                                        
  shmem_exit(code);

  elog(DEBUG3, "proc_exit(%d): %d callbacks to make", code, on_proc_exit_index);

     
                                        
     
                                                                     
                                                                      
                                                              
                                                                          
               
     
  while (--on_proc_exit_index >= 0)
  {
    on_proc_exit_list[on_proc_exit_index].function(code, on_proc_exit_list[on_proc_exit_index].arg);
  }

  on_proc_exit_index = 0;
}

                      
                                                                      
                                                                     
                                                                      
                                                                  
                                   
                      
   
void
shmem_exit(int code)
{
  shmem_exit_inprogress = true;

     
                                       
     
                                                                            
                                                                        
                                                                             
                                                
     
  elog(DEBUG3, "shmem_exit(%d): %d before_shmem_exit callbacks to make", code, before_shmem_exit_index);
  while (--before_shmem_exit_index >= 0)
  {
    before_shmem_exit_list[before_shmem_exit_index].function(code, before_shmem_exit_list[before_shmem_exit_index].arg);
  }
  before_shmem_exit_index = 0;

     
                                           
     
                                                                            
                                                                 
                                                                          
                                                                     
                                                                            
                                                                     
     
                                                                             
                                                                            
                                                                     
                                                                           
                                                                      
     
  dsm_backend_shutdown();

     
                                   
     
                                                                          
                                                                           
                                                                           
                                                                     
     
  elog(DEBUG3, "shmem_exit(%d): %d on_shmem_exit callbacks to make", code, on_shmem_exit_index);
  while (--on_shmem_exit_index >= 0)
  {
    on_shmem_exit_list[on_shmem_exit_index].function(code, on_shmem_exit_list[on_shmem_exit_index].arg);
  }
  on_shmem_exit_index = 0;

  shmem_exit_inprogress = false;
}

                                                                    
                    
   
                                                                     
   
                                                                   
                                                                    
                                                      
                                                                    
   
static void
atexit_callback(void)
{
                                                   
                                                        
  proc_exit_prepare(-1);
}

                                                                    
                 
   
                                                          
                                                    
                                                                    
   
void
on_proc_exit(pg_on_exit_callback function, Datum arg)
{
  if (on_proc_exit_index >= MAX_ON_EXITS)
  {
    ereport(FATAL, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg_internal("out of on_proc_exit slots")));
  }

  on_proc_exit_list[on_proc_exit_index].function = function;
  on_proc_exit_list[on_proc_exit_index].arg = arg;

  ++on_proc_exit_index;

  if (!atexit_callback_setup)
  {
    atexit(atexit_callback);
    atexit_callback_setup = true;
  }
}

                                                                    
                      
   
                                                           
                                                          
                          
                                                                    
   
void
before_shmem_exit(pg_on_exit_callback function, Datum arg)
{
  if (before_shmem_exit_index >= MAX_ON_EXITS)
  {
    ereport(FATAL, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg_internal("out of before_shmem_exit slots")));
  }

  before_shmem_exit_list[before_shmem_exit_index].function = function;
  before_shmem_exit_list[before_shmem_exit_index].arg = arg;

  ++before_shmem_exit_index;

  if (!atexit_callback_setup)
  {
    atexit(atexit_callback);
    atexit_callback_setup = true;
  }
}

                                                                    
                  
   
                                                             
                                                             
                                                 
                                                                    
   
void
on_shmem_exit(pg_on_exit_callback function, Datum arg)
{
  if (on_shmem_exit_index >= MAX_ON_EXITS)
  {
    ereport(FATAL, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg_internal("out of on_shmem_exit slots")));
  }

  on_shmem_exit_list[on_shmem_exit_index].function = function;
  on_shmem_exit_list[on_shmem_exit_index].arg = arg;

  ++on_shmem_exit_index;

  if (!atexit_callback_setup)
  {
    atexit(atexit_callback);
    atexit_callback_setup = true;
  }
}

                                                                    
                             
   
                                                                    
                                                            
                                                             
                   
                                                                    
   
void
cancel_before_shmem_exit(pg_on_exit_callback function, Datum arg)
{
  if (before_shmem_exit_index > 0 && before_shmem_exit_list[before_shmem_exit_index - 1].function == function && before_shmem_exit_list[before_shmem_exit_index - 1].arg == arg)
  {
    --before_shmem_exit_index;
  }
}

                                                                    
                  
   
                                                                
                                                                      
                                                                        
                                      
                                                                    
   
void
on_exit_reset(void)
{
  before_shmem_exit_index = 0;
  on_shmem_exit_index = 0;
  on_proc_exit_index = 0;
  reset_on_dsm_detach();
}
