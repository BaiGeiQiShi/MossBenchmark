                                                                            
   
                 
                                                
   
                                                                         
                                                                        
   
   
                  
                                                   
   
                                                                             
   

#include "postgres.h"

#include "access/tsmapi.h"

   
                                                                      
   
                                                                        
   
TsmRoutine *
GetTsmRoutine(Oid tsmhandler)
{
  Datum datum;
  TsmRoutine *routine;

  datum = OidFunctionCall1(tsmhandler, PointerGetDatum(NULL));
  routine = (TsmRoutine *)DatumGetPointer(datum);

  if (routine == NULL || !IsA(routine, TsmRoutine))
  {
    elog(ERROR, "tablesample handler function %u did not return a TsmRoutine struct", tsmhandler);
  }

  return routine;
}
