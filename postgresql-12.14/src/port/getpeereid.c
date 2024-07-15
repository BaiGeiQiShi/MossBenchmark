                                                                            
   
                
                                                      
   
                                                                         
   
   
                  
                           
   
                                                                            
   

#include "c.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_UCRED_H
#include <ucred.h>
#endif
#ifdef HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif

   
                                                      
   
int
getpeereid(int sock, uid_t *uid, gid_t *gid)
{
#if defined(SO_PEERCRED)
                                          
  struct ucred peercred;
  ACCEPT_TYPE_ARG3 so_len = sizeof(peercred);

  if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &peercred, &so_len) != 0 || so_len != sizeof(peercred))
  {
    return -1;
  }
  *uid = peercred.uid;
  *gid = peercred.gid;
  return 0;
#elif defined(LOCAL_PEERCRED)
                                                                  
  struct xucred peercred;
  ACCEPT_TYPE_ARG3 so_len = sizeof(peercred);

  if (getsockopt(sock, 0, LOCAL_PEERCRED, &peercred, &so_len) != 0 || so_len != sizeof(peercred) || peercred.cr_version != XUCRED_VERSION)
  {
    return -1;
  }
  *uid = peercred.cr_uid;
  *gid = peercred.cr_gid;
  return 0;
#elif defined(HAVE_GETPEERUCRED)
                                   
  ucred_t *ucred;

  ucred = NULL;                                  
  if (getpeerucred(sock, &ucred) == -1)
  {
    return -1;
  }

  *uid = ucred_geteuid(ucred);
  *gid = ucred_getegid(ucred);
  ucred_free(ucred);

  if (*uid == (uid_t)(-1) || *gid == (gid_t)(-1))
  {
    return -1;
  }
  return 0;
#else
                                                    
  errno = ENOSYS;
  return -1;
#endif
}
