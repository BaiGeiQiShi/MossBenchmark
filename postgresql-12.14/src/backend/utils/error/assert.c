                                                                            
   
            
                  
   
                                                                         
                                                                        
   
   
                  
                                      
   
        
                                             
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

   
                                                             
   
void
ExceptionalCondition(const char *conditionName, const char *errorType, const char *fileName, int lineNumber)
{
  if (!PointerIsValid(conditionName) || !PointerIsValid(fileName) || !PointerIsValid(errorType))
  {
    write_stderr("TRAP: ExceptionalCondition: bad arguments\n");
  }
  else
  {
    write_stderr("TRAP: %s(\"%s\", File: \"%s\", Line: %d)\n", errorType, conditionName, fileName, lineNumber);
  }

                                                                        
  fflush(stderr);

#ifdef SLEEP_ON_ASSERT

     
                                                                            
                                     
     
  sleep(1000000);
#endif

  abort();
}
