   
                                                                   
                                                            
   
                                                                         
                                                                          
                                                                     
   
                                                                     
                                                                    
                                                                     
                                                                          
                                                                         
                                                                        
                                                                     
   
                              
   

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "Id: inet_net_ntop.c,v 1.1.2.2 2004/03/09 09:17:27 marka Exp $";
#endif

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef FRONTEND
#include "utils/inet.h"
#else
   
                                                                           
                                                                           
                                                   
   
#define PGSQL_AF_INET (AF_INET + 0)
#define PGSQL_AF_INET6 (AF_INET + 1)
#endif

#define NS_IN6ADDRSZ 16
#define NS_INT16SZ 2

#ifdef SPRINTF_CHAR
#define SPRINTF(x) strlen(sprintf      x)
#else
#define SPRINTF(x) ((size_t)sprintf x)
#endif

static char *
inet_net_ntop_ipv4(const u_char *src, int bits, char *dst, size_t size);
static char *
inet_net_ntop_ipv6(const u_char *src, int bits, char *dst, size_t size);

   
          
                                           
                                                                     
                                             
           
                                                               
         
                                                                        
                                                                      
                        
           
                                  
   
char *
inet_net_ntop(int af, const void *src, int bits, char *dst, size_t size)
{
     
                                                                            
                                                                          
                                                                           
                                                                             
                                                 
     
  switch (af)
  {
  case PGSQL_AF_INET:
    return (inet_net_ntop_ipv4(src, bits, dst, size));
  case PGSQL_AF_INET6:
#if defined(AF_INET6) && AF_INET6 != PGSQL_AF_INET6
  case AF_INET6:
#endif
    return (inet_net_ntop_ipv6(src, bits, dst, size));
  default:
    errno = EAFNOSUPPORT;
    return (NULL);
  }
}

   
                 
                                            
                                                                     
                                             
           
                                                               
         
                                                              
                                   
           
                                  
   
static char *
inet_net_ntop_ipv4(const u_char *src, int bits, char *dst, size_t size)
{
  char *odst = dst;
  char *t;
  int len = 4;
  int b;

  if (bits < 0 || bits > 32)
  {
    errno = EINVAL;
    return (NULL);
  }

                                                                 
  for (b = len; b > 0; b--)
  {
    if (size <= sizeof ".255")
    {
      goto emsgsize;
    }
    t = dst;
    if (dst != odst)
    {
      *dst++ = '.';
    }
    dst += SPRINTF((dst, "%u", *src++));
    size -= (size_t)(dst - t);
  }

                                      
  if (bits != 32)
  {
    if (size <= sizeof "/32")
    {
      goto emsgsize;
    }
    dst += SPRINTF((dst, "/%u", bits));
  }

  return (odst);

emsgsize:
  errno = EMSGSIZE;
  return (NULL);
}

static int
decoct(const u_char *src, int bytes, char *dst, size_t size)
{
  char *odst = dst;
  char *t;
  int b;

  for (b = 1; b <= bytes; b++)
  {
    if (size <= sizeof "255.")
    {
      return (0);
    }
    t = dst;
    dst += SPRINTF((dst, "%u", *src++));
    if (b != bytes)
    {
      *dst++ = '.';
      *dst = '\0';
    }
    size -= (size_t)(dst - t);
  }
  return (dst - odst);
}

static char *
inet_net_ntop_ipv6(const u_char *src, int bits, char *dst, size_t size)
{
     
                                                                           
                                                                          
                                                                           
                                                                      
                                                   
     
  char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255/128"];
  char *tp;
  struct
  {
    int base, len;
  } best, cur;
  u_int words[NS_IN6ADDRSZ / NS_INT16SZ];
  int i;

  if ((bits < -1) || (bits > 128))
  {
    errno = EINVAL;
    return (NULL);
  }

     
                                                                             
                                                             
     
  memset(words, '\0', sizeof words);
  for (i = 0; i < NS_IN6ADDRSZ; i++)
  {
    words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
  }
  best.base = -1;
  cur.base = -1;
  best.len = 0;
  cur.len = 0;
  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
  {
    if (words[i] == 0)
    {
      if (cur.base == -1)
      {
        cur.base = i, cur.len = 1;
      }
      else
      {
        cur.len++;
      }
    }
    else
    {
      if (cur.base != -1)
      {
        if (best.base == -1 || cur.len > best.len)
        {
          best = cur;
        }
        cur.base = -1;
      }
    }
  }
  if (cur.base != -1)
  {
    if (best.base == -1 || cur.len > best.len)
    {
      best = cur;
    }
  }
  if (best.base != -1 && best.len < 2)
  {
    best.base = -1;
  }

     
                        
     
  tp = tmp;
  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
  {
                                               
    if (best.base != -1 && i >= best.base && i < (best.base + best.len))
    {
      if (i == best.base)
      {
        *tp++ = ':';
      }
      continue;
    }
                                                                   
    if (i != 0)
    {
      *tp++ = ':';
    }
                                               
    if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 7 && words[7] != 0x0001) || (best.len == 5 && words[5] == 0xffff)))
    {
      int n;

      n = decoct(src + 12, 4, tp, sizeof tmp - (tp - tmp));
      if (n == 0)
      {
        errno = EMSGSIZE;
        return (NULL);
      }
      tp += strlen(tp);
      break;
    }
    tp += SPRINTF((tp, "%x", words[i]));
  }

                                        
  if (best.base != -1 && (best.base + best.len) == (NS_IN6ADDRSZ / NS_INT16SZ))
  {
    *tp++ = ':';
  }
  *tp = '\0';

  if (bits != -1 && bits != 128)
  {
    tp += SPRINTF((tp, "/%u", bits));
  }

     
                                               
     
  if ((size_t)(tp - tmp) > size)
  {
    errno = EMSGSIZE;
    return (NULL);
  }
  strcpy(dst, tmp);
  return (dst);
}
