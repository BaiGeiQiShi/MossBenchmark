                                                                            
   
            
                                                
   
                                                                         
                                                                        
   
   
                  
                       
   
                                                                            
   

#include "c.h"

int
setenv(const char *name, const char *value, int overwrite)
{
  char *envstr;

                                   
  if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL || value == NULL)
  {
    errno = EINVAL;
    return -1;
  }

                                                              
  if (overwrite == 0 && getenv(name) != NULL)
  {
    return 0;
  }

     
                                                                            
                                                                         
                                            
     
  envstr = (char *)malloc(strlen(name) + strlen(value) + 2);
  if (!envstr)                                      
  {
    return -1;
  }

  sprintf(envstr, "%s=%s", name, value);

  return putenv(envstr);
}
