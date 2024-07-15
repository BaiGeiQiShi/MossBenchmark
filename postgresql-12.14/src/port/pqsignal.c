                                                                            
   
              
                                                                       
                     
   
                                                                         
                                                                        
   
   
                  
                         
   
                                                                   
                                                                       
                                                                       
                                                      
   
                                                                            
                                                                           
                                                                      
                                                                      
                                               
   
                                                                            
   

#include "c.h"

#include <signal.h>

#if !defined(WIN32) || defined(FRONTEND)

   
                                                                
   
                                 
   
pqsigfunc
pqsignal(int signo, pqsigfunc func)
{
#ifndef WIN32
  struct sigaction act, oact;

  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;
#ifdef SA_NOCLDSTOP
  if (signo == SIGCHLD)
  {
    act.sa_flags |= SA_NOCLDSTOP;
  }
#endif
  if (sigaction(signo, &act, &oact) < 0)
  {
    return SIG_ERR;
  }
  return oact.sa_handler;
#else            
  return signal(signo, func);
#endif
}

#endif                                           
