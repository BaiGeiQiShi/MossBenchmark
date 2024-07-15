                                                                            
   
              
                                                              
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include "libpq/pqsignal.h"

                      
sigset_t UnBlockSig, BlockSig, StartupBlockSig;

   
                                                         
   
                                                                       
                                                                          
                                            
   
                                                                        
                                                                          
   
                                                                         
                                    
   
void
pqinitmask(void)
{
  sigemptyset(&UnBlockSig);

                                               
  sigfillset(&BlockSig);
  sigfillset(&StartupBlockSig);

     
                                                                             
                                                                           
                                 
     
#ifdef SIGTRAP
  sigdelset(&BlockSig, SIGTRAP);
  sigdelset(&StartupBlockSig, SIGTRAP);
#endif
#ifdef SIGABRT
  sigdelset(&BlockSig, SIGABRT);
  sigdelset(&StartupBlockSig, SIGABRT);
#endif
#ifdef SIGILL
  sigdelset(&BlockSig, SIGILL);
  sigdelset(&StartupBlockSig, SIGILL);
#endif
#ifdef SIGFPE
  sigdelset(&BlockSig, SIGFPE);
  sigdelset(&StartupBlockSig, SIGFPE);
#endif
#ifdef SIGSEGV
  sigdelset(&BlockSig, SIGSEGV);
  sigdelset(&StartupBlockSig, SIGSEGV);
#endif
#ifdef SIGBUS
  sigdelset(&BlockSig, SIGBUS);
  sigdelset(&StartupBlockSig, SIGBUS);
#endif
#ifdef SIGSYS
  sigdelset(&BlockSig, SIGSYS);
  sigdelset(&StartupBlockSig, SIGSYS);
#endif
#ifdef SIGCONT
  sigdelset(&BlockSig, SIGCONT);
  sigdelset(&StartupBlockSig, SIGCONT);
#endif

                               
#ifdef SIGQUIT
  sigdelset(&StartupBlockSig, SIGQUIT);
#endif
#ifdef SIGTERM
  sigdelset(&StartupBlockSig, SIGTERM);
#endif
#ifdef SIGALRM
  sigdelset(&StartupBlockSig, SIGALRM);
#endif
}

   
                                                         
   
                                 
   
                                                                          
                                                                             
                                                                            
                                                                       
                                     
   
                                                   
   
                                                             
                                                                            
                                                                        
                                                        
   
pqsigfunc
pqsignal_pm(int signo, pqsigfunc func)
{
#ifndef WIN32
  struct sigaction act, oact;

  act.sa_handler = func;
  if (func == SIG_IGN || func == SIG_DFL)
  {
                                                    
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
  }
  else
  {
    act.sa_mask = BlockSig;
    act.sa_flags = 0;
  }
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
  return pqsignal(signo, func);
#endif
}
