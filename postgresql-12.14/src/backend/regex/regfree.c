   
                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                               
   
   
                                                                       
                                                                    
                                                                  
                                                                    
                                                    
   

#include "regex/regguts.h"

   
                                                                             
   
                                                   
   
void
pg_regfree(regex_t *re)
{
  if (re == NULL)
  {
    return;
  }
  (*((struct fns *)re->re_fns)->free)(re);
}
