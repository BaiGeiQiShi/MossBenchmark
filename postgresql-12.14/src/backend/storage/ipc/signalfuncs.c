                                                                            
   
                 
                                       
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <signal.h>

#include "catalog/pg_authid.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/builtins.h"

   
                                     
   
                                                                         
                                                                            
                                                                        
   
                                                                            
                                                
   
                                                                             
                                                                          
               
   
#define SIGNAL_BACKEND_SUCCESS 0
#define SIGNAL_BACKEND_ERROR 1
#define SIGNAL_BACKEND_NOPERMISSION 2
#define SIGNAL_BACKEND_NOSUPERUSER 3
static int
pg_signal_backend(int pid, int sig)
{
  PGPROC *proc = BackendPidGetProc(pid);

     
                                                                            
                                                                         
                                                                         
                                                                            
                                                                             
                                                     
     
  if (proc == NULL)
  {
       
                                                                         
                                                            
       
    ereport(WARNING, (errmsg("PID %d is not a PostgreSQL server process", pid)));
    return SIGNAL_BACKEND_ERROR;
  }

                                                                 
  if (superuser_arg(proc->roleId) && !superuser())
  {
    return SIGNAL_BACKEND_NOSUPERUSER;
  }

                                                               
  if (!has_privs_of_role(GetUserId(), proc->roleId) && !has_privs_of_role(GetUserId(), DEFAULT_ROLE_SIGNAL_BACKENDID))
  {
    return SIGNAL_BACKEND_NOPERMISSION;
  }

     
                                                                            
                                                                            
                                                                         
                                                                             
                                                                            
                                  
     

                                                                     
#ifdef HAVE_SETSID
  if (kill(-pid, sig))
#else
  if (kill(pid, sig))
#endif
  {
                                              
    ereport(WARNING, (errmsg("could not send signal to process %d: %m", pid)));
    return SIGNAL_BACKEND_ERROR;
  }
  return SIGNAL_BACKEND_SUCCESS;
}

   
                                                                               
                                             
   
                                                                   
   
Datum
pg_cancel_backend(PG_FUNCTION_ARGS)
{
  int r = pg_signal_backend(PG_GETARG_INT32(0), SIGINT);

  if (r == SIGNAL_BACKEND_NOSUPERUSER)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be a superuser to cancel superuser query"))));
  }

  if (r == SIGNAL_BACKEND_NOPERMISSION)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be a member of the role whose query is being canceled or member of pg_signal_backend"))));
  }

  PG_RETURN_BOOL(r == SIGNAL_BACKEND_SUCCESS);
}

   
                                                                               
                                                  
   
                                                                   
   
Datum
pg_terminate_backend(PG_FUNCTION_ARGS)
{
  int r = pg_signal_backend(PG_GETARG_INT32(0), SIGTERM);

  if (r == SIGNAL_BACKEND_NOSUPERUSER)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be a superuser to terminate superuser process"))));
  }

  if (r == SIGNAL_BACKEND_NOPERMISSION)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be a member of the role whose process is being terminated or member of pg_signal_backend"))));
  }

  PG_RETURN_BOOL(r == SIGNAL_BACKEND_SUCCESS);
}

   
                                               
   
                                                                       
                 
   
Datum
pg_reload_conf(PG_FUNCTION_ARGS)
{
  if (kill(PostmasterPid, SIGHUP))
  {
    ereport(WARNING, (errmsg("failed to send signal to postmaster: %m")));
    PG_RETURN_BOOL(false);
  }

  PG_RETURN_BOOL(true);
}

   
                   
   
                                                   
   
Datum
pg_rotate_logfile(PG_FUNCTION_ARGS)
{
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser to rotate log files with adminpack 1.0"),
                                                                                                            
                                                                 errhint("Consider using %s, which is part of core, instead.", "pg_logfile_rotate()"))));
  }

  if (!Logging_collector)
  {
    ereport(WARNING, (errmsg("rotation not possible because log collection not active")));
    PG_RETURN_BOOL(false);
  }

  SendPostmasterSignal(PMSIGNAL_ROTATE_LOGFILE);
  PG_RETURN_BOOL(true);
}

   
                   
   
                                                                       
                 
   
Datum
pg_rotate_logfile_v2(PG_FUNCTION_ARGS)
{
  if (!Logging_collector)
  {
    ereport(WARNING, (errmsg("rotation not possible because log collection not active")));
    PG_RETURN_BOOL(false);
  }

  SendPostmasterSignal(PMSIGNAL_ROTATE_LOGFILE);
  PG_RETURN_BOOL(true);
}
