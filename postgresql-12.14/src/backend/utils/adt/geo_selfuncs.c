                                                                            
   
                  
                                                                    
                                         
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                   
                                  
   
                                                                            
   
#include "postgres.h"

#include "utils/builtins.h"
#include "utils/geo_decls.h"

   
                                                                             
                                                                          
                                                     
   
                                                                         
                                                                        
                                                                           
                    
   
                                                                            
                                                                           
                                                                         
                                                                               
                                                                           
                                                
   

   
                                                                     
   

Datum
areasel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(0.005);
}

Datum
areajoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(0.005);
}

   
               
   
                                                                       
                
   

Datum
positionsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(0.1);
}

Datum
positionjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(0.1);
}

   
                                                                            
   
                                                                     
                               
   

Datum
contsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(0.001);
}

Datum
contjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(0.001);
}
