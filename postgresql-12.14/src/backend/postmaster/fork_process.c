   
                  
                                                                
                                                                      
                               
   
                                                                
   
                  
                                           
   
#include "postgres.h"
#include "postmaster/fork_process.h"

#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef USE_OPENSSL
#include <openssl/rand.h>
#endif

#ifndef WIN32
   
                                                                       
                                                                     
                                
   
pid_t
fork_process(void)
{
  pid_t result;
  const char *oomfilename;

#ifdef LINUX_PROFILE
  struct itimerval prof_itimer;
#endif

     
                                                                             
                                                                            
                                                                          
                                                                            
                                                              
     
  fflush(stdout);
  fflush(stderr);

#ifdef LINUX_PROFILE

     
                                                                           
                                                                          
                                                                           
                                                                 
     
  getitimer(ITIMER_PROF, &prof_itimer);
#endif

  result = fork();
  if (result == 0)
  {
                                  
#ifdef LINUX_PROFILE
    setitimer(ITIMER_PROF, &prof_itimer, NULL);
#endif

       
                                                                       
                                                                         
                                                                       
                                                                          
                                                                     
                                                                          
                                                                           
                                                                
                                                                           
                                                                      
                                                                       
                                                                      
                                                                          
                                         
       
    oomfilename = getenv("PG_OOM_ADJUST_FILE");

    if (oomfilename != NULL)
    {
         
                                                                         
                                                                   
         
      int fd = open(oomfilename, O_WRONLY, 0);

                                
      if (fd >= 0)
      {
        const char *oomvalue = getenv("PG_OOM_ADJUST_VALUE");
        int rc;

        if (oomvalue == NULL)                              
        {
          oomvalue = "0";
        }

        rc = write(fd, oomvalue, strlen(oomvalue));
        (void)rc;
        close(fd);
      }
    }

       
                                                                  
       
#ifdef USE_OPENSSL
    RAND_cleanup();
#endif
  }

  return result;
}

#endif              
