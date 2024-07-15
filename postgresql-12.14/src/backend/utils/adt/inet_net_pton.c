   
                                                                   
                                                            
   
                                                                         
                                                                          
                                                                     
   
                                                                     
                                                                    
                                                                     
                                                                          
                                                                         
                                                                        
                                                                     
   
                                           
   

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "Id: inet_net_pton.c,v 1.4.2.3 2004/03/17 00:40:11 marka Exp $";
#endif

#include "postgres.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>

#include "utils/builtins.h"                          /* needed on some
														 * platforms */
#include "utils/inet.h"

static int
inet_net_pton_ipv4(const char *src, u_char *dst);
static int
inet_cidr_pton_ipv4(const char *src, u_char *dst, size_t size);
static int
inet_net_pton_ipv6(const char *src, u_char *dst);
static int
inet_cidr_pton_ipv6(const char *src, u_char *dst, size_t size);

   
       
                                     
                                                               
                                                               
                                           
           
                                                                      
                                                                      
                                      
           
                               
   
            
                                                                    
                                           
   
   
int
inet_net_pton(int af, const char *src, void *dst, size_t size)
{
  switch (af)
  {
  case PGSQL_AF_INET:
    return size == -1 ? inet_net_pton_ipv4(src, dst) : inet_cidr_pton_ipv4(src, dst, size);
  case PGSQL_AF_INET6:
    return size == -1 ? inet_net_pton_ipv6(src, dst) : inet_cidr_pton_ipv6(src, dst, size);
  default:
    errno = EAFNOSUPPORT;
    return -1;
  }
}

   
              
                                       
                                                                    
                                                               
                                           
           
                                                                      
                                                                      
                                      
         
                                                              
                                   
           
                               
   
static int
inet_cidr_pton_ipv4(const char *src, u_char *dst, size_t size)
{
  static const char xdigits[] = "0123456789abcdef";
  static const char digits[] = "0123456789";
  int n, ch, tmp = 0, dirty, bits;
  const u_char *odst = dst;

  ch = *src++;
  if (ch == '0' && (src[0] == 'x' || src[0] == 'X') && isxdigit((unsigned char)src[1]))
  {
                                         
    if (size <= 0U)
    {
      goto emsgsize;
    }
    dirty = 0;
    src++;                   
    while ((ch = *src++) != '\0' && isxdigit((unsigned char)ch))
    {
      if (isupper((unsigned char)ch))
      {
        ch = tolower((unsigned char)ch);
      }
      n = strchr(xdigits, ch) - xdigits;
      assert(n >= 0 && n <= 15);
      if (dirty == 0)
      {
        tmp = n;
      }
      else
      {
        tmp = (tmp << 4) | n;
      }
      if (++dirty == 2)
      {
        if (size-- <= 0U)
        {
          goto emsgsize;
        }
        *dst++ = (u_char)tmp;
        dirty = 0;
      }
    }
    if (dirty)
    {                           
      if (size-- <= 0U)
      {
        goto emsgsize;
      }
      *dst++ = (u_char)(tmp << 4);
    }
  }
  else if (isdigit((unsigned char)ch))
  {
                                           
    for (;;)
    {
      tmp = 0;
      do
      {
        n = strchr(digits, ch) - digits;
        assert(n >= 0 && n <= 9);
        tmp *= 10;
        tmp += n;
        if (tmp > 255)
        {
          goto enoent;
        }
      } while ((ch = *src++) != '\0' && isdigit((unsigned char)ch));
      if (size-- <= 0U)
      {
        goto emsgsize;
      }
      *dst++ = (u_char)tmp;
      if (ch == '\0' || ch == '/')
      {
        break;
      }
      if (ch != '.')
      {
        goto enoent;
      }
      ch = *src++;
      if (!isdigit((unsigned char)ch))
      {
        goto enoent;
      }
    }
  }
  else
  {
    goto enoent;
  }

  bits = -1;
  if (ch == '/' && isdigit((unsigned char)src[0]) && dst > odst)
  {
                                                       
    ch = *src++;                       
    bits = 0;
    do
    {
      n = strchr(digits, ch) - digits;
      assert(n >= 0 && n <= 9);
      bits *= 10;
      bits += n;
    } while ((ch = *src++) != '\0' && isdigit((unsigned char)ch));
    if (ch != '\0')
    {
      goto enoent;
    }
    if (bits > 32)
    {
      goto emsgsize;
    }
  }

                                                             
  if (ch != '\0')
  {
    goto enoent;
  }

                                                                       
  if (dst == odst)
  {
    goto enoent;
  }
                                                              
  if (bits == -1)
  {
    if (*odst >= 240)              
    {
      bits = 32;
    }
    else if (*odst >= 224)              
    {
      bits = 8;
    }
    else if (*odst >= 192)              
    {
      bits = 24;
    }
    else if (*odst >= 128)              
    {
      bits = 16;
    }
    else
    {
                   
      bits = 8;
    }
                                                                   
    if (bits < ((dst - odst) * 8))
    {
      bits = (dst - odst) * 8;
    }

       
                                                                       
                         
       
    if (bits == 8 && *odst == 224)
    {
      bits = 4;
    }
  }
                                                
  while (bits > ((dst - odst) * 8))
  {
    if (size-- <= 0U)
    {
      goto emsgsize;
    }
    *dst++ = '\0';
  }
  return bits;

enoent:
  errno = ENOENT;
  return -1;

emsgsize:
  errno = EMSGSIZE;
  return -1;
}

   
       
                                      
                                                                
                                                                    
                                                                     
                                                                     
           
                                                                     
                                        
         
                                                                        
                                                                       
                        
           
                                  
   
static int
inet_net_pton_ipv4(const char *src, u_char *dst)
{
  static const char digits[] = "0123456789";
  const u_char *odst = dst;
  int n, ch, tmp, bits;
  size_t size = 4;

                         
  while (ch = *src++, isdigit((unsigned char)ch))
  {
    tmp = 0;
    do
    {
      n = strchr(digits, ch) - digits;
      assert(n >= 0 && n <= 9);
      tmp *= 10;
      tmp += n;
      if (tmp > 255)
      {
        goto enoent;
      }
    } while ((ch = *src++) != '\0' && isdigit((unsigned char)ch));
    if (size-- == 0)
    {
      goto emsgsize;
    }
    *dst++ = (u_char)tmp;
    if (ch == '\0' || ch == '/')
    {
      break;
    }
    if (ch != '.')
    {
      goto enoent;
    }
  }

                                     
  bits = -1;
  if (ch == '/' && isdigit((unsigned char)src[0]) && dst > odst)
  {
                                                       
    ch = *src++;                       
    bits = 0;
    do
    {
      n = strchr(digits, ch) - digits;
      assert(n >= 0 && n <= 9);
      bits *= 10;
      bits += n;
    } while ((ch = *src++) != '\0' && isdigit((unsigned char)ch));
    if (ch != '\0')
    {
      goto enoent;
    }
    if (bits > 32)
    {
      goto emsgsize;
    }
  }

                                                             
  if (ch != '\0')
  {
    goto enoent;
  }

                                                                        
  if (bits == -1)
  {
    if (dst - odst == 4)
    {
      bits = 32;
    }
    else
    {
      goto enoent;
    }
  }

                                                                       
  if (dst == odst)
  {
    goto enoent;
  }

                                                             
  if ((bits / 8) > (dst - odst))
  {
    goto enoent;
  }

                                      
  while (size-- > 0)
  {
    *dst++ = 0;
  }

  return bits;

enoent:
  errno = ENOENT;
  return -1;

emsgsize:
  errno = EMSGSIZE;
  return -1;
}

static int
getbits(const char *src, int *bitsp)
{
  static const char digits[] = "0123456789";
  int n;
  int val;
  char ch;

  val = 0;
  n = 0;
  while ((ch = *src++) != '\0')
  {
    const char *pch;

    pch = strchr(digits, ch);
    if (pch != NULL)
    {
      if (n++ != 0 && val == 0)                       
      {
        return 0;
      }
      val *= 10;
      val += (pch - digits);
      if (val > 128)            
      {
        return 0;
      }
      continue;
    }
    return 0;
  }
  if (n == 0)
  {
    return 0;
  }
  *bitsp = val;
  return 1;
}

static int
getv4(const char *src, u_char *dst, int *bitsp)
{
  static const char digits[] = "0123456789";
  u_char *odst = dst;
  int n;
  u_int val;
  char ch;

  val = 0;
  n = 0;
  while ((ch = *src++) != '\0')
  {
    const char *pch;

    pch = strchr(digits, ch);
    if (pch != NULL)
    {
      if (n++ != 0 && val == 0)                       
      {
        return 0;
      }
      val *= 10;
      val += (pch - digits);
      if (val > 255)            
      {
        return 0;
      }
      continue;
    }
    if (ch == '.' || ch == '/')
    {
      if (dst - odst > 3)                       
      {
        return 0;
      }
      *dst++ = val;
      if (ch == '/')
      {
        return getbits(src, bitsp);
      }
      val = 0;
      n = 0;
      continue;
    }
    return 0;
  }
  if (n == 0)
  {
    return 0;
  }
  if (dst - odst > 3)                       
  {
    return 0;
  }
  *dst++ = val;
  return 1;
}

static int
inet_net_pton_ipv6(const char *src, u_char *dst)
{
  return inet_cidr_pton_ipv6(src, dst, 16);
}

#define NS_IN6ADDRSZ 16
#define NS_INT16SZ 2
#define NS_INADDRSZ 4

static int
inet_cidr_pton_ipv6(const char *src, u_char *dst, size_t size)
{
  static const char xdigits_l[] = "0123456789abcdef", xdigits_u[] = "0123456789ABCDEF";
  u_char tmp[NS_IN6ADDRSZ], *tp, *endp, *colonp;
  const char *xdigits, *curtok;
  int ch, saw_xdigit;
  u_int val;
  int digits;
  int bits;

  if (size < NS_IN6ADDRSZ)
  {
    goto emsgsize;
  }

  memset((tp = tmp), '\0', NS_IN6ADDRSZ);
  endp = tp + NS_IN6ADDRSZ;
  colonp = NULL;
                                                  
  if (*src == ':')
  {
    if (*++src != ':')
    {
      goto enoent;
    }
  }
  curtok = src;
  saw_xdigit = 0;
  val = 0;
  digits = 0;
  bits = -1;
  while ((ch = *src++) != '\0')
  {
    const char *pch;

    if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
    {
      pch = strchr((xdigits = xdigits_u), ch);
    }
    if (pch != NULL)
    {
      val <<= 4;
      val |= (pch - xdigits);
      if (++digits > 4)
      {
        goto enoent;
      }
      saw_xdigit = 1;
      continue;
    }
    if (ch == ':')
    {
      curtok = src;
      if (!saw_xdigit)
      {
        if (colonp)
        {
          goto enoent;
        }
        colonp = tp;
        continue;
      }
      else if (*src == '\0')
      {
        goto enoent;
      }
      if (tp + NS_INT16SZ > endp)
      {
        goto enoent;
      }
      *tp++ = (u_char)(val >> 8) & 0xff;
      *tp++ = (u_char)val & 0xff;
      saw_xdigit = 0;
      digits = 0;
      val = 0;
      continue;
    }
    if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) && getv4(curtok, tp, &bits) > 0)
    {
      tp += NS_INADDRSZ;
      saw_xdigit = 0;
      break;                                     
    }
    if (ch == '/' && getbits(src, &bits) > 0)
    {
      break;
    }
    goto enoent;
  }
  if (saw_xdigit)
  {
    if (tp + NS_INT16SZ > endp)
    {
      goto enoent;
    }
    *tp++ = (u_char)(val >> 8) & 0xff;
    *tp++ = (u_char)val & 0xff;
  }
  if (bits == -1)
  {
    bits = 128;
  }

  endp = tmp + 16;

  if (colonp != NULL)
  {
       
                                                                     
                                            
       
    const int n = tp - colonp;
    int i;

    if (tp == endp)
    {
      goto enoent;
    }
    for (i = 1; i <= n; i++)
    {
      endp[-i] = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp)
  {
    goto enoent;
  }

     
                          
     
  memcpy(dst, tmp, NS_IN6ADDRSZ);

  return bits;

enoent:
  errno = ENOENT;
  return -1;

emsgsize:
  errno = EMSGSIZE;
  return -1;
}
