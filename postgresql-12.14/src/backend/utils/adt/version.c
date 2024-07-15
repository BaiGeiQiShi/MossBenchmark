                                                                            
   
             
                                          
   
                                                                
   
                  
   
                                   
   
                                                                            
   

#include "postgres.h"

#include "utils/builtins.h"

Datum
pgsql_version(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(cstring_to_text(PG_VERSION_STR));
}
