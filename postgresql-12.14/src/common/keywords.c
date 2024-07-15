                                                                            
   
              
                                       
   
   
                                                                         
                                                                        
   
   
                  
                           
   
                                                                            
   
#include "c.h"

#include "common/keywords.h"

                                                  

#include "kwlist_d.h"

                                         

#define PG_KEYWORD(kwname, value, category) category,

const uint8 ScanKeywordCategories[SCANKEYWORDS_NUM_KEYWORDS] = {
#include "parser/kwlist.h"
};

#undef PG_KEYWORD
