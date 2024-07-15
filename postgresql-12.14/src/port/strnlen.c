                                                                            
   
             
                                          
   
   
                                                                         
                                                                        
   
                  
                        
   
                                                                            
   

#include "c.h"

   
                                                                          
   
                                                                             
                                                                            
                       
   
size_t
strnlen(const char *str, size_t maxlen)
{
  const char *p = str;

  while (maxlen-- > 0 && *p)
  {
    p++;
  }
  return p - str;
}
