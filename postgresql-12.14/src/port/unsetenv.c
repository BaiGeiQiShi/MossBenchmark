                                                                            
   
              
                                                  
   
                                                                         
                                                                        
   
   
                  
                         
   
                                                                            
   

#include "c.h"

void
unsetenv(const char *name)
{
  char *envstr;

  if (getenv(name) == NULL)
  {
    return;              
  }

     
                                                                            
                                                                          
                                                                          
                                                                          
                                                                             
                                                                            
                                                                            
                                                     
     
                                                                           
                  
     

  envstr = (char *)malloc(strlen(name) + 2);
  if (!envstr)                                      
  {
    return;
  }

                                                                  
  sprintf(envstr, "%s=", name);
  putenv(envstr);

                                                            
  strcpy(envstr, "=");

     
                                                                           
                                          
     
  putenv(envstr);
}
