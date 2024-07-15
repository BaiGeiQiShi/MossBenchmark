                                                                            
                 
                                                                        
   
                                                                
   
                  
                              
   
                                                                            
   
#include "c.h"

#include "common/link-canary.h"

   
                                                                          
                                                                         
                                                                          
                                                                          
                                                                        
                                                                         
                                                                        
                                                                          
                                                                         
                                                                  
   
bool
pg_link_canary_is_frontend(void)
{
#ifdef FRONTEND
  return true;
#else
  return false;
#endif
}
