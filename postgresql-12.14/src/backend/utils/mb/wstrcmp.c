   
                                  
   
    
                            
                                                                      
   
                                                                 
                
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   
                                               
#include "postgres_fe.h"

#include "mb/pg_wchar.h"

int
pg_char_and_wchar_strcmp(const char *s1, const pg_wchar *s2)
{
  while ((pg_wchar)*s1 == *s2++)
  {
    if (*s1++ == 0)
    {
      return 0;
    }
  }
  return *(const unsigned char *)s1 - *(const pg_wchar *)(s2 - 1);
}
