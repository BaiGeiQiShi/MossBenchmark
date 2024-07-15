                                                                            
   
            
                                           
   
                                                                         
                                                                        
   
   
                  
                       
   
                                                                            
   

#include "c.h"

   
                                                                         
   
                                                                      
                                                                            
                                                                              
                                       
   
                                                                          
                                                        
   
                                                                          
            
   
char *
escape_single_quotes_ascii(const char *src)
{
  int len = strlen(src), i, j;
  char *result = malloc(len * 2 + 1);

  if (!result)
  {
    return NULL;
  }

  for (i = 0, j = 0; i < len; i++)
  {
    if (SQL_STR_DOUBLE(src[i], true))
    {
      result[j++] = src[i];
    }
    result[j++] = src[i];
  }
  result[j] = '\0';
  return result;
}
