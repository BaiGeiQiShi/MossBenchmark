                                                                            
   
             
                                                       
   
                                                                         
                                                                        
   
                  
                        
   
                                                                            
   

#include "c.h"

#include <fcntl.h>

   
                                  
                                              
   
bool
pg_set_noblock(pgsocket sock)
{
#if !defined(WIN32)
  int flags;

  flags = fcntl(sock, F_GETFL);
  if (flags < 0)
  {
    return false;
  }
  if (fcntl(sock, F_SETFL, (flags | O_NONBLOCK)) == -1)
  {
    return false;
  }
  return true;
#else
  unsigned long ioctlsocket_ret = 1;

                                                                     
  return (ioctlsocket(sock, FIONBIO, &ioctlsocket_ret) == 0);
#endif
}

   
                                  
                                              
   
bool
pg_set_block(pgsocket sock)
{
#if !defined(WIN32)
  int flags;

  flags = fcntl(sock, F_GETFL);
  if (flags < 0)
  {
    return false;
  }
  if (fcntl(sock, F_SETFL, (flags & ~O_NONBLOCK)) == -1)
  {
    return false;
  }
  return true;
#else
  unsigned long ioctlsocket_ret = 0;

                                                                     
  return (ioctlsocket(sock, FIONBIO, &ioctlsocket_ret) == 0);
#endif
}
