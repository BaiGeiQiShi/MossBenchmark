                                                                            
   
             
                               
   
   
                                                                         
   
                      
   
                                                                            
   
#include "c.h"

#include <unistd.h>
#include <sys/time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

   
                                                                      
                                                                
   
#if defined(FRONTEND) || !defined(WIN32)

   
                                                             
   
                                                                        
                                                                        
                                                                         
   
                                                                            
   
                                                                            
                                                                           
                                                                               
                                                                           
                                                                              
                                                                                
                                                                           
                                                                             
                                         
   
void
pg_usleep(long microsec)
{
  if (microsec > 0)
  {
#ifndef WIN32
    struct timeval delay;

    delay.tv_sec = microsec / 1000000L;
    delay.tv_usec = microsec % 1000000L;
    (void)select(0, NULL, NULL, NULL, &delay);
#else
    SleepEx((microsec < 500 ? 1 : (microsec + 500) / 1000), FALSE);
#endif
  }
}

#endif                                           
