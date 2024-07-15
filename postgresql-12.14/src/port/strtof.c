                                                                            
   
            
   
                                                                    
   
   
                  
                       
   
                                                                            
   

#include "c.h"

#include <float.h>
#include <math.h>

#ifndef HAVE_STRTOF
   
                                                                             
                                                                              
                                                                             
                          
   

float
strtof(const char *nptr, char **endptr)
{
  int caller_errno = errno;
  double dresult;
  float fresult;

  errno = 0;
  dresult = strtod(nptr, endptr);
  fresult = (float)dresult;

  if (errno == 0)
  {
       
                                                         
       
    if (dresult != 0 && fresult == 0)
    {
      caller_errno = ERANGE;                
    }
    if (!isinf(dresult) && isinf(fresult))
    {
      caller_errno = ERANGE;               
    }
  }
  else
  {
    caller_errno = errno;
  }

  errno = caller_errno;
  return fresult;
}

#elif HAVE_BUGGY_STRTOF
   
                                                                           
                                                                               
                                                                             
                                                                           
                                                                              
                                                                              
                                                                              
                                                                           
   
                                                                              
                                                                               
                                                                           
                                    
   
                                                                              
                                       
   
float
pg_strtof(const char *nptr, char **endptr)
{
  int caller_errno = errno;
  float fresult;

  errno = 0;
  fresult = (strtof)(nptr, endptr);
  if (errno)
  {
                                                        
    return fresult;
  }
  else if ((*endptr == nptr) || isnan(fresult) || ((fresult >= FLT_MIN || fresult <= -FLT_MIN) && !isinf(fresult)))
  {
       
                                                                       
                                                                           
                      
       
    errno = caller_errno;
    return fresult;
  }
  else
  {
       
                                           
       
    double dresult = strtod(nptr, NULL);

    if (errno)
    {
                                           
      return fresult;
    }
    else if ((dresult == 0.0 && fresult == 0.0) || (isinf(dresult) && isinf(fresult) && (fresult == dresult)))
    {
                                                            
      errno = caller_errno;
      return fresult;
    }
    else if ((dresult > 0 && dresult <= FLT_MIN && (float)dresult != 0.0) || (dresult < 0 && dresult >= -FLT_MIN && (float)dresult != 0.0))
    {
                                       
      errno = caller_errno;
      return (float)dresult;
    }
    else
    {
      errno = ERANGE;
      return fresult;
    }
  }
}

#endif
