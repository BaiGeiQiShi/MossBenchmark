                                                                            
   
              
                                    
   
   
                                                                         
                                                                        
   
   
                  
                           
   
                                                                            
   
#include "c.h"

#include "common/kwlookup.h"

   
                                                        
   
                                                                              
   
                                                                          
                                                                      
                                                
   
                                                                          
                                                                              
                                                                              
                                                                        
                                                                              
                                                   
   
int
ScanKeywordLookup(const char *str, const ScanKeywordList *keywords)
{
  size_t len;
  int h;
  const char *kw;

     
                                                                           
                                                  
     
  len = strlen(str);
  if (len > keywords->max_kw_len)
  {
    return -1;
  }

     
                                                                       
                                                                        
                                                  
     
  h = keywords->hash(str, len);

                                               
  if (h < 0 || h >= keywords->num_keywords)
  {
    return -1;
  }

     
                                                                           
                                                                     
                                                                          
                    
     
  kw = GetScanKeyword(h, keywords);
  while (*str != '\0')
  {
    char ch = *str++;

    if (ch >= 'A' && ch <= 'Z')
    {
      ch += 'a' - 'A';
    }
    if (ch != *kw++)
    {
      return -1;
    }
  }
  if (*kw != '\0')
  {
    return -1;
  }

                
  return h;
}
