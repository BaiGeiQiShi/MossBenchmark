                                                                            
   
                
                                                                      
   
                                            
   
                                                                            
   
#include "postgres_fe.h"

#include "preproc_extern.h"
#include "preproc.h"

                                                
#include "c_kwlist_d.h"

                                
#define PG_KEYWORD(kwname, value) value,

static const uint16 ScanCKeywordTokens[] = {
#include "c_kwlist.h"
};

#undef PG_KEYWORD

   
                                                         
   
                                                              
   
                                                                        
                                                                
   
int
ScanCKeywordLookup(const char *str)
{
  size_t len;
  int h;
  const char *kw;

     
                                                                           
                                   
     
  len = strlen(str);
  if (len > ScanCKeywords.max_kw_len)
  {
    return -1;
  }

     
                                                                         
                                                  
     
  h = ScanCKeywords_hash_func(str, len);

                                               
  if (h < 0 || h >= ScanCKeywords.num_keywords)
  {
    return -1;
  }

  kw = GetScanKeyword(h, &ScanCKeywords);

  if (strcmp(kw, str) == 0)
  {
    return ScanCKeywordTokens[h];
  }

  return -1;
}
