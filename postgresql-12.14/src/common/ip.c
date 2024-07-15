                                                                            
   
        
                                
   
                                                                         
                                                                        
   
   
                  
                     
   
                                                                    
                                                               
                        
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <arpa/inet.h>
#include <sys/file.h>

#include "common/ip.h"

#ifdef HAVE_UNIX_SOCKETS
static int
getaddrinfo_unix(const char *path, const struct addrinfo *hintsp, struct addrinfo **result);

static int
getnameinfo_unix(const struct sockaddr_un *sa, int salen, char *node, int nodelen, char *service, int servicelen, int flags);
#endif

   
                                                                         
   
int
pg_getaddrinfo_all(const char *hostname, const char *servname, const struct addrinfo *hintp, struct addrinfo **result)
{
  int rc;

                                                                 
  *result = NULL;

#ifdef HAVE_UNIX_SOCKETS
  if (hintp->ai_family == AF_UNIX)
  {
    return getaddrinfo_unix(servname, hintp, result);
  }
#endif

                                                  
  rc = getaddrinfo((!hostname || hostname[0] == '\0') ? NULL : hostname, servname, hintp, result);

  return rc;
}

   
                                                                          
   
                                                                           
                                                                             
                                                                               
                                                                          
                                                         
   
void
pg_freeaddrinfo_all(int hint_ai_family, struct addrinfo *ai)
{
#ifdef HAVE_UNIX_SOCKETS
  if (hint_ai_family == AF_UNIX)
  {
                                                                       
    while (ai != NULL)
    {
      struct addrinfo *p = ai;

      ai = ai->ai_next;
      free(p->ai_addr);
      free(p);
    }
  }
  else
#endif                        
  {
                                           
    if (ai != NULL)
    {
      freeaddrinfo(ai);
    }
  }
}

   
                                                                      
   
                                                                              
                                                                          
                                                                            
                                                                  
   
int
pg_getnameinfo_all(const struct sockaddr_storage *addr, int salen, char *node, int nodelen, char *service, int servicelen, int flags)
{
  int rc;

#ifdef HAVE_UNIX_SOCKETS
  if (addr && addr->ss_family == AF_UNIX)
  {
    rc = getnameinfo_unix((const struct sockaddr_un *)addr, salen, node, nodelen, service, servicelen, flags);
  }
  else
#endif
    rc = getnameinfo((const struct sockaddr *)addr, salen, node, nodelen, service, servicelen, flags);

  if (rc != 0)
  {
    if (node)
    {
      strlcpy(node, "???", nodelen);
    }
    if (service)
    {
      strlcpy(service, "???", servicelen);
    }
  }

  return rc;
}

#if defined(HAVE_UNIX_SOCKETS)

           
                                                                     
   
                                                                
                       
                                     
           
   
static int
getaddrinfo_unix(const char *path, const struct addrinfo *hintsp, struct addrinfo **result)
{
  struct addrinfo hints;
  struct addrinfo *aip;
  struct sockaddr_un *unp;

  *result = NULL;

  MemSet(&hints, 0, sizeof(hints));

  if (strlen(path) >= sizeof(unp->sun_path))
  {
    return EAI_FAIL;
  }

  if (hintsp == NULL)
  {
    hints.ai_family = AF_UNIX;
    hints.ai_socktype = SOCK_STREAM;
  }
  else
  {
    memcpy(&hints, hintsp, sizeof(hints));
  }

  if (hints.ai_socktype == 0)
  {
    hints.ai_socktype = SOCK_STREAM;
  }

  if (hints.ai_family != AF_UNIX)
  {
                                    
    return EAI_FAIL;
  }

  aip = calloc(1, sizeof(struct addrinfo));
  if (aip == NULL)
  {
    return EAI_MEMORY;
  }

  unp = calloc(1, sizeof(struct sockaddr_un));
  if (unp == NULL)
  {
    free(aip);
    return EAI_MEMORY;
  }

  aip->ai_family = AF_UNIX;
  aip->ai_socktype = hints.ai_socktype;
  aip->ai_protocol = hints.ai_protocol;
  aip->ai_next = NULL;
  aip->ai_canonname = NULL;
  *result = aip;

  unp->sun_family = AF_UNIX;
  aip->ai_addr = (struct sockaddr *)unp;
  aip->ai_addrlen = sizeof(struct sockaddr_un);

  strcpy(unp->sun_path, path);

#ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
  unp->sun_len = sizeof(struct sockaddr_un);
#endif

  return 0;
}

   
                                     
   
static int
getnameinfo_unix(const struct sockaddr_un *sa, int salen, char *node, int nodelen, char *service, int servicelen, int flags)
{
  int ret;

                          
  if (sa == NULL || sa->sun_family != AF_UNIX || (node == NULL && service == NULL))
  {
    return EAI_FAIL;
  }

  if (node)
  {
    ret = snprintf(node, nodelen, "%s", "[local]");
    if (ret < 0 || ret >= nodelen)
    {
      return EAI_MEMORY;
    }
  }

  if (service)
  {
    ret = snprintf(service, servicelen, "%s", sa->sun_path);
    if (ret < 0 || ret >= servicelen)
    {
      return EAI_MEMORY;
    }
  }

  return 0;
}
#endif                        
