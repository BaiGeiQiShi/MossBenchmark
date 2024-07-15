                                                                            
   
              
                                                                              
                                                                             
   
                                                                         
                                                                        
   
   
                  
                         
   
                                                                            
   

#include "c.h"

int
pgwin32_putenv(const char *envval)
{
  char *envcpy;
  char *cp;
  typedef int(_cdecl * PUTENVPROC)(const char *);
  static const char *const modulenames[] = {"msvcrt",                                
      "msvcrtd", "msvcr70",                                                   
      "msvcr70d", "msvcr71",                                                  
      "msvcr71d", "msvcr80",                                                  
      "msvcr80d", "msvcr90",                                                  
      "msvcr90d", "msvcr100",                                                 
      "msvcr100d", "msvcr110",                                                
      "msvcr110d", "msvcr120",                                                
      "msvcr120d", "ucrtbase",                                                          
      "ucrtbased", NULL};
  int i;

     
                                                                     
                                                                           
                                                                             
                                                                 
     
                                                    
     
  envcpy = strdup(envval);
  if (!envcpy)
  {
    return -1;
  }
  cp = strchr(envcpy, '=');
  if (cp == NULL)
  {
    free(envcpy);
    return -1;
  }
  *cp = '\0';
  cp++;
  if (strlen(cp))
  {
       
                                                                         
                                                                    
                                  
       
    if (!SetEnvironmentVariable(envcpy, cp))
    {
      free(envcpy);
      return -1;
    }
  }
  free(envcpy);

     
                                                                        
                                                                          
                                                                         
                                                                            
                                                 
     
  for (i = 0; modulenames[i]; i++)
  {
    HMODULE hmodule = NULL;
    BOOL res = GetModuleHandleEx(0, modulenames[i], &hmodule);

    if (res != 0 && hmodule != NULL)
    {
      PUTENVPROC putenvFunc;

      putenvFunc = (PUTENVPROC)GetProcAddress(hmodule, "_putenv");
      if (putenvFunc)
      {
        putenvFunc(envval);
      }
      FreeLibrary(hmodule);
    }
  }

     
                                                                       
                                                                           
                                                                            
     
  return _putenv(envval);
}

void
pgwin32_unsetenv(const char *name)
{
  char *envbuf;

  envbuf = (char *)malloc(strlen(name) + 2);
  if (!envbuf)
  {
    return;
  }

  sprintf(envbuf, "%s=", name);
  pgwin32_putenv(envbuf);
  free(envbuf);
}
