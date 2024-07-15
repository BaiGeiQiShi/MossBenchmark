                                                                            
   
                 
                                                            
   
                                                                           
                                                                         
                                                                            
                           
   
                                                                              
                                                                           
                                                                
   
   
                                                                
   
                  
                            
   
                                                                            
   

                                                                          
#include "c.h"

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "getaddrinfo.h"
#include "libpq/pqcomm.h"                                         
#include "port/pg_bswap.h"

#ifdef WIN32
   
                                                                               
                                                                                
                                                                
   
typedef int(__stdcall *getaddrinfo_ptr_t)(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res);

typedef void(__stdcall *freeaddrinfo_ptr_t)(struct addrinfo *ai);

typedef int(__stdcall *getnameinfo_ptr_t)(const struct sockaddr *sa, int salen, char *host, int hostlen, char *serv, int servlen, int flags);

                                                                            
static getaddrinfo_ptr_t getaddrinfo_ptr = NULL;
static freeaddrinfo_ptr_t freeaddrinfo_ptr = NULL;
static getnameinfo_ptr_t getnameinfo_ptr = NULL;

static bool
haveNativeWindowsIPv6routines(void)
{
  void *hLibrary = NULL;
  static bool alreadyLookedForIpv6routines = false;

  if (alreadyLookedForIpv6routines)
  {
    return (getaddrinfo_ptr != NULL);
  }

     
                                                                             
                                                                       
     

  hLibrary = LoadLibraryA("ws2_32");

  if (hLibrary == NULL || GetProcAddress(hLibrary, "getaddrinfo") == NULL)
  {
       
                                                               
                    
       
    if (hLibrary != NULL)
    {
      FreeLibrary(hLibrary);
    }

       
                                                                           
                                              
       

    hLibrary = LoadLibraryA("wship6");
  }

                                                                  
  if (hLibrary != NULL)
  {
                                                                  

    getaddrinfo_ptr = (getaddrinfo_ptr_t)GetProcAddress(hLibrary, "getaddrinfo");
    freeaddrinfo_ptr = (freeaddrinfo_ptr_t)GetProcAddress(hLibrary, "freeaddrinfo");
    getnameinfo_ptr = (getnameinfo_ptr_t)GetProcAddress(hLibrary, "getnameinfo");

       
                                                                     
                       
       
    if (getaddrinfo_ptr == NULL || freeaddrinfo_ptr == NULL || getnameinfo_ptr == NULL)
    {
      FreeLibrary(hLibrary);
      hLibrary = NULL;
      getaddrinfo_ptr = NULL;
      freeaddrinfo_ptr = NULL;
      getnameinfo_ptr = NULL;
    }
  }

  alreadyLookedForIpv6routines = true;
  return (getaddrinfo_ptr != NULL);
}
#endif

   
                                      
   
                                                                 
                       
                                     
                                               
   
int
getaddrinfo(const char *node, const char *service, const struct addrinfo *hintp, struct addrinfo **res)
{
  struct addrinfo *ai;
  struct sockaddr_in sin, *psin;
  struct addrinfo hints;

#ifdef WIN32

     
                                                                         
                                                   
     
  if (haveNativeWindowsIPv6routines())
  {
    return (*getaddrinfo_ptr)(node, service, hintp, res);
  }
#endif

  if (hintp == NULL)
  {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
  }
  else
  {
    memcpy(&hints, hintp, sizeof(hints));
  }

  if (hints.ai_family != AF_INET && hints.ai_family != AF_UNSPEC)
  {
    return EAI_FAMILY;
  }

  if (hints.ai_socktype == 0)
  {
    hints.ai_socktype = SOCK_STREAM;
  }

  if (!node && !service)
  {
    return EAI_NONAME;
  }

  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;

  if (node)
  {
    if (node[0] == '\0')
    {
      sin.sin_addr.s_addr = pg_hton32(INADDR_ANY);
    }
    else if (hints.ai_flags & AI_NUMERICHOST)
    {
      if (!inet_aton(node, &sin.sin_addr))
      {
        return EAI_NONAME;
      }
    }
    else
    {
      struct hostent *hp;

#ifdef FRONTEND
      struct hostent hpstr;
      char buf[BUFSIZ];
      int herrno = 0;

      pqGethostbyname(node, &hpstr, buf, sizeof(buf), &hp, &herrno);
#else
      hp = gethostbyname(node);
#endif
      if (hp == NULL)
      {
        switch (h_errno)
        {
        case HOST_NOT_FOUND:
        case NO_DATA:
          return EAI_NONAME;
        case TRY_AGAIN:
          return EAI_AGAIN;
        case NO_RECOVERY:
        default:
          return EAI_FAIL;
        }
      }
      if (hp->h_addrtype != AF_INET)
      {
        return EAI_FAIL;
      }

      memcpy(&(sin.sin_addr), hp->h_addr, hp->h_length);
    }
  }
  else
  {
    if (hints.ai_flags & AI_PASSIVE)
    {
      sin.sin_addr.s_addr = pg_hton32(INADDR_ANY);
    }
    else
    {
      sin.sin_addr.s_addr = pg_hton32(INADDR_LOOPBACK);
    }
  }

  if (service)
  {
    sin.sin_port = pg_hton16((unsigned short)atoi(service));
  }

#ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
  sin.sin_len = sizeof(sin);
#endif

  ai = malloc(sizeof(*ai));
  if (!ai)
  {
    return EAI_MEMORY;
  }

  psin = malloc(sizeof(*psin));
  if (!psin)
  {
    free(ai);
    return EAI_MEMORY;
  }

  memcpy(psin, &sin, sizeof(*psin));

  ai->ai_flags = 0;
  ai->ai_family = AF_INET;
  ai->ai_socktype = hints.ai_socktype;
  ai->ai_protocol = hints.ai_protocol;
  ai->ai_addrlen = sizeof(*psin);
  ai->ai_addr = (struct sockaddr *)psin;
  ai->ai_canonname = NULL;
  ai->ai_next = NULL;

  *res = ai;

  return 0;
}

void
freeaddrinfo(struct addrinfo *res)
{
  if (res)
  {
#ifdef WIN32

       
                                                                           
                                                     
       
    if (haveNativeWindowsIPv6routines())
    {
      (*freeaddrinfo_ptr)(res);
      return;
    }
#endif

    if (res->ai_addr)
    {
      free(res->ai_addr);
    }
    free(res);
  }
}

const char *
gai_strerror(int errcode)
{
#ifdef HAVE_HSTRERROR
  int hcode;

  switch (errcode)
  {
  case EAI_NONAME:
    hcode = HOST_NOT_FOUND;
    break;
  case EAI_AGAIN:
    hcode = TRY_AGAIN;
    break;
  case EAI_FAIL:
  default:
    hcode = NO_RECOVERY;
    break;
  }

  return hstrerror(hcode);
#else                      

  switch (errcode)
  {
  case EAI_NONAME:
    return "Unknown host";
  case EAI_AGAIN:
    return "Host name lookup failure";
                                              
#ifdef EAI_BADFLAGS
  case EAI_BADFLAGS:
    return "Invalid argument";
#endif
#ifdef EAI_FAMILY
  case EAI_FAMILY:
    return "Address family not supported";
#endif
#ifdef EAI_MEMORY
  case EAI_MEMORY:
    return "Not enough memory";
#endif
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME                           
  case EAI_NODATA:
    return "No host data of that type was found";
#endif
#ifdef EAI_SERVICE
  case EAI_SERVICE:
    return "Class type not found";
#endif
#ifdef EAI_SOCKTYPE
  case EAI_SOCKTYPE:
    return "Socket type not supported";
#endif
  default:
    return "Unknown server error";
  }
#endif                     
}

   
                                          
   
                                                                     
                                        
                       
   
int
getnameinfo(const struct sockaddr *sa, int salen, char *node, int nodelen, char *service, int servicelen, int flags)
{
#ifdef WIN32

     
                                                                         
                                                   
     
  if (haveNativeWindowsIPv6routines())
  {
    return (*getnameinfo_ptr)(sa, salen, node, nodelen, service, servicelen, flags);
  }
#endif

                          
  if (sa == NULL || (node == NULL && service == NULL))
  {
    return EAI_FAIL;
  }

#ifdef HAVE_IPV6
  if (sa->sa_family == AF_INET6)
  {
    return EAI_FAMILY;
  }
#endif

                          
  if (flags & NI_NAMEREQD)
  {
    return EAI_AGAIN;
  }

  if (node)
  {
    if (sa->sa_family == AF_INET)
    {
      if (inet_net_ntop(AF_INET, &((struct sockaddr_in *)sa)->sin_addr, sa->sa_family == AF_INET ? 32 : 128, node, nodelen) == NULL)
      {
        return EAI_MEMORY;
      }
    }
    else
    {
      return EAI_MEMORY;
    }
  }

  if (service)
  {
    int ret = -1;

    if (sa->sa_family == AF_INET)
    {
      ret = snprintf(service, servicelen, "%d", pg_ntoh16(((struct sockaddr_in *)sa)->sin_port));
    }
    if (ret < 0 || ret >= servicelen)
    {
      return EAI_MEMORY;
    }
  }

  return 0;
}
