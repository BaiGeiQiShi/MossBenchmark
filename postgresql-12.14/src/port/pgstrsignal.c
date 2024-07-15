                                                                            
   
                 
                                   
   
                                                                           
                                     
   
                                                                       
                          
   
                                                                         
                                                                        
   
                  
                            
   
                                                                            
   

#include "c.h"

   
                
   
                                                             
   
                                                                    
                                                                       
                                                                   
   
                                                                  
                                                             
   
                                                                     
                                                                     
                                                                   
                                             
   
const char *
pg_strsignal(int signum)
{
  const char *result;

     
                                                                          
     
#ifdef HAVE_STRSIGNAL
  result = strsignal(signum);
  if (result == NULL)
  {
    result = "unrecognized signal";
  }
#else

     
                                                                         
                                                                           
                                                                       
     
  result = "(signal names not available on this platform)";
#endif

  return result;
}
