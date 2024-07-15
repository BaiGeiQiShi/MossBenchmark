                                                                            
   
            
                                              
   
   
                                                                  
                                                                    
                                                                 
                                                              
                                      
   
                                   
   
                                                                        
                                      
   
                   
                                   
                                                                            
                          
                                                                        
                 
                                                                    
                       
   
                                                                           
                                                                         
                                                                            
                     
   
                                                                         
   
                     
   
                                                                            
   

#if defined(WIN32) && !defined(__CYGWIN__)

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <fcntl.h>

#undef system
#undef popen

int
pgwin32_system(const char *command)
{
  size_t cmdlen = strlen(command);
  char *buf;
  int save_errno;
  int res;

     
                                                                          
                    
     
  buf = malloc(cmdlen + 2 + 1);
  if (buf == NULL)
  {
    errno = ENOMEM;
    return -1;
  }
  buf[0] = '"';
  memcpy(&buf[1], command, cmdlen);
  buf[cmdlen + 1] = '"';
  buf[cmdlen + 2] = '\0';

  res = system(buf);

  save_errno = errno;
  free(buf);
  errno = save_errno;

  return res;
}

FILE *
pgwin32_popen(const char *command, const char *type)
{
  size_t cmdlen = strlen(command);
  char *buf;
  int save_errno;
  FILE *res;

     
                                                                          
                    
     
  buf = malloc(cmdlen + 2 + 1);
  if (buf == NULL)
  {
    errno = ENOMEM;
    return NULL;
  }
  buf[0] = '"';
  memcpy(&buf[1], command, cmdlen);
  buf[cmdlen + 1] = '"';
  buf[cmdlen + 2] = '\0';

  res = _popen(buf, type);

  save_errno = errno;
  free(buf);
  errno = save_errno;

  return res;
}

#endif
