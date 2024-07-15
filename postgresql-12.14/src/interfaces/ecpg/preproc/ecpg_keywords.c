                                                                            
   
                   
                                                                      
   
                  
                                                 
   
                                                                            
   

#include "postgres_fe.h"

#include <ctype.h>

#include "preproc_extern.h"
#include "preproc.h"

                                                   
#include "ecpg_kwlist_d.h"

                                   
#define PG_KEYWORD(kwname, value) value,

static const uint16 ECPGScanKeywordTokens[] = {
#include "ecpg_kwlist.h"
};

#undef PG_KEYWORD

   
                                                            
   
                                                              
   
                                                                             
   
int
ScanECPGKeywordLookup(const char *text)
{
  int kwnum;

                                                       
  kwnum = ScanKeywordLookup(text, &ScanKeywords);
  if (kwnum >= 0)
  {
    return SQLScanKeywordTokens[kwnum];
  }

                                   
  kwnum = ScanKeywordLookup(text, &ScanECPGKeywords);
  if (kwnum >= 0)
  {
    return ECPGScanKeywordTokens[kwnum];
  }

  return -1;
}
