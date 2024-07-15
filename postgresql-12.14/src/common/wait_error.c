                                                                            
   
                
                                                                     
   
   
                                                                         
                                                                        
   
   
                  
                             
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <signal.h>
#include <sys/wait.h>

   
                                                                        
                                                                    
                                                                        
   
char *
wait_result_to_str(int exitstatus)
{
  char str[512];

  if (WIFEXITED(exitstatus))
  {
       
                                                                        
                                         
       
    switch (WEXITSTATUS(exitstatus))
    {
    case 126:
      snprintf(str, sizeof(str), _("command not executable"));
      break;

    case 127:
      snprintf(str, sizeof(str), _("command not found"));
      break;

    default:
      snprintf(str, sizeof(str), _("child process exited with exit code %d"), WEXITSTATUS(exitstatus));
    }
  }
  else if (WIFSIGNALED(exitstatus))
  {
#if defined(WIN32)
    snprintf(str, sizeof(str), _("child process was terminated by exception 0x%X"), WTERMSIG(exitstatus));
#else
    snprintf(str, sizeof(str), _("child process was terminated by signal %d: %s"), WTERMSIG(exitstatus), pg_strsignal(WTERMSIG(exitstatus)));
#endif
  }
  else
  {
    snprintf(str, sizeof(str), _("child process exited with unrecognized status %d"), exitstatus);
  }

  return pstrdup(str);
}

   
                                                                    
                                     
   
                                                                  
                                                                   
                                                                   
                                                                   
                                                               
   
                                                                      
                                               
   
bool
wait_result_is_signal(int exit_status, int signum)
{
  if (WIFSIGNALED(exit_status) && WTERMSIG(exit_status) == signum)
  {
    return true;
  }
  if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 128 + signum)
  {
    return true;
  }
  return false;
}

   
                                                                    
                                                                  
                                                                       
   
                                                                    
                                                          
                                                      
   
bool
wait_result_is_any_signal(int exit_status, bool include_command_not_found)
{
  if (WIFSIGNALED(exit_status))
  {
    return true;
  }
  if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) > (include_command_not_found ? 125 : 128))
  {
    return true;
  }
  return false;
}
