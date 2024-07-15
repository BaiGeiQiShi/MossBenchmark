                                                                            
   
         
                                                       
   
                                                                         
   
   
                  
                    
   
                                                                          
                                                                             
                                                                           
                                                
   
                                       
   

    
                            
                                                                      
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

#include "c.h"

   
                     
   
int
fls(int mask)
{
  int bit;

  if (mask == 0)
  {
    return (0);
  }
  for (bit = 1; mask != 1; bit++)
  {
    mask = (unsigned int)mask >> 1;
  }
  return (bit);
}
