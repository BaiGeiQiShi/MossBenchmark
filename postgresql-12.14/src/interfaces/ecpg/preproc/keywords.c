                                                                            
   
              
                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres_fe.h"

   
                                                                      
                                                                   
                                                                    
                                                                         
                                                                            
                                                                    
                                                                    
                                              
   

#include "preproc_extern.h"
#include "preproc.h"

#define PG_KEYWORD(kwname, value, category) value,

const uint16 SQLScanKeywordTokens[] = {
#include "parser/kwlist.h"
};

#undef PG_KEYWORD
