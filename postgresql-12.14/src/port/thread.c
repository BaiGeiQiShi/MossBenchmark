                                                                            
   
            
   
                                                                   
                                                                               
   
                                                                         
   
                     
   
                                                                            
   

#include "c.h"

#include <pwd.h>

   
                                                                      
                                                                    
                                                                  
                                                                  
                                                                      
                                   
   
                                                                   
                                                                        
                                                                          
                                                                       
                                                    
   
                                                                        
                                                                        
                                                                       
                                                                   
                                                                        
                                                                         
                                                                      
                                    
   
                                                        
   
                                        
                        
                                                  
   
                                                                               
   
                                                                        
                      
   

   
                                                                         
                                                             
   
                                      
                                              
                                                
                                                               
                                                               
   
#ifndef WIN32
int
pqGetpwuid(uid_t uid, struct passwd *resultbuf, char *buffer, size_t buflen, struct passwd **result)
{
#if defined(FRONTEND) && defined(ENABLE_THREAD_SAFETY) && defined(HAVE_GETPWUID_R)
  return getpwuid_r(uid, resultbuf, buffer, buflen, result);
#else
                                                      
  errno = 0;
  *result = getpwuid(uid);
                                                  
  return (*result == NULL) ? errno : 0;
#endif
}
#endif

   
                                                                
                                                                          
                                                                            
   
#ifndef HAVE_GETADDRINFO
int
pqGethostbyname(const char *name, struct hostent *resultbuf, char *buffer, size_t buflen, struct hostent **result, int *herrno)
{
#if defined(FRONTEND) && defined(ENABLE_THREAD_SAFETY) && defined(HAVE_GETHOSTBYNAME_R)

     
                                                                             
                
     
  *result = gethostbyname_r(name, resultbuf, buffer, buflen, herrno);
  return (*result == NULL) ? -1 : 0;
#else

                                                      
  *result = gethostbyname(name);

  if (*result != NULL)
  {
    *herrno = h_errno;
  }

  if (*result != NULL)
  {
    return 0;
  }
  else
  {
    return -1;
  }
#endif
}

#endif
