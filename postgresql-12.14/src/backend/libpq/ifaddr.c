                                                                            
   
            
                                                                  
   
                                                                         
                                                                        
   
   
                  
                                
   
                                                                    
                                                               
                        
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <sys/file.h>

#include "libpq/ifaddr.h"
#include "port/pg_bswap.h"

static int
range_sockaddr_AF_INET(const struct sockaddr_in *addr, const struct sockaddr_in *netaddr, const struct sockaddr_in *netmask);

#ifdef HAVE_IPV6
static int
range_sockaddr_AF_INET6(const struct sockaddr_in6 *addr, const struct sockaddr_in6 *netaddr, const struct sockaddr_in6 *netmask);
#endif

   
                                                                                
   
                                                                        
                                                                        
   
int
pg_range_sockaddr(const struct sockaddr_storage *addr, const struct sockaddr_storage *netaddr, const struct sockaddr_storage *netmask)
{
  if (addr->ss_family == AF_INET)
  {
    return range_sockaddr_AF_INET((const struct sockaddr_in *)addr, (const struct sockaddr_in *)netaddr, (const struct sockaddr_in *)netmask);
  }
#ifdef HAVE_IPV6
  else if (addr->ss_family == AF_INET6)
  {
    return range_sockaddr_AF_INET6((const struct sockaddr_in6 *)addr, (const struct sockaddr_in6 *)netaddr, (const struct sockaddr_in6 *)netmask);
  }
#endif
  else
  {
    return 0;
  }
}

static int
range_sockaddr_AF_INET(const struct sockaddr_in *addr, const struct sockaddr_in *netaddr, const struct sockaddr_in *netmask)
{
  if (((addr->sin_addr.s_addr ^ netaddr->sin_addr.s_addr) & netmask->sin_addr.s_addr) == 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

#ifdef HAVE_IPV6

static int
range_sockaddr_AF_INET6(const struct sockaddr_in6 *addr, const struct sockaddr_in6 *netaddr, const struct sockaddr_in6 *netmask)
{
  int i;

  for (i = 0; i < 16; i++)
  {
    if (((addr->sin6_addr.s6_addr[i] ^ netaddr->sin6_addr.s6_addr[i]) & netmask->sin6_addr.s6_addr[i]) != 0)
    {
      return 0;
    }
  }

  return 1;
}
#endif                

   
                                                                         
                                             
   
                                                             
   
                                                                          
   
                                         
   
int
pg_sockaddr_cidr_mask(struct sockaddr_storage *mask, char *numbits, int family)
{
  long bits;
  char *endptr;

  if (numbits == NULL)
  {
    bits = (family == AF_INET) ? 32 : 128;
  }
  else
  {
    bits = strtol(numbits, &endptr, 10);
    if (*numbits == '\0' || *endptr != '\0')
    {
      return -1;
    }
  }

  switch (family)
  {
  case AF_INET:
  {
    struct sockaddr_in mask4;
    long maskl;

    if (bits < 0 || bits > 32)
    {
      return -1;
    }
    memset(&mask4, 0, sizeof(mask4));
                                                
    if (bits > 0)
    {
      maskl = (0xffffffffUL << (32 - (int)bits)) & 0xffffffffUL;
    }
    else
    {
      maskl = 0;
    }
    mask4.sin_addr.s_addr = pg_hton32(maskl);
    memcpy(mask, &mask4, sizeof(mask4));
    break;
  }

#ifdef HAVE_IPV6
  case AF_INET6:
  {
    struct sockaddr_in6 mask6;
    int i;

    if (bits < 0 || bits > 128)
    {
      return -1;
    }
    memset(&mask6, 0, sizeof(mask6));
    for (i = 0; i < 16; i++)
    {
      if (bits <= 0)
      {
        mask6.sin6_addr.s6_addr[i] = 0;
      }
      else if (bits >= 8)
      {
        mask6.sin6_addr.s6_addr[i] = 0xff;
      }
      else
      {
        mask6.sin6_addr.s6_addr[i] = (0xff << (8 - (int)bits)) & 0xff;
      }
      bits -= 8;
    }
    memcpy(mask, &mask6, sizeof(mask6));
    break;
  }
#endif
  default:
    return -1;
  }

  mask->ss_family = family;
  return 0;
}

   
                                                                      
                              
   
static void
run_ifaddr_callback(PgIfAddrCallback callback, void *cb_data, struct sockaddr *addr, struct sockaddr *mask)
{
  struct sockaddr_storage fullmask;

  if (!addr)
  {
    return;
  }

                                    
  if (mask)
  {
    if (mask->sa_family != addr->sa_family)
    {
      mask = NULL;
    }
    else if (mask->sa_family == AF_INET)
    {
      if (((struct sockaddr_in *)mask)->sin_addr.s_addr == INADDR_ANY)
      {
        mask = NULL;
      }
    }
#ifdef HAVE_IPV6
    else if (mask->sa_family == AF_INET6)
    {
      if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)mask)->sin6_addr))
      {
        mask = NULL;
      }
    }
#endif
  }

                                                           
  if (!mask)
  {
    pg_sockaddr_cidr_mask(&fullmask, NULL, addr->sa_family);
    mask = (struct sockaddr *)&fullmask;
  }

  (*callback)(addr, mask, cb_data);
}

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

   
                                                                            
                                                          
   
                                                                             
   
int
pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data)
{
  INTERFACE_INFO *ptr, *ii = NULL;
  unsigned long length, i;
  unsigned long n_ii = 0;
  SOCKET sock;
  int error;

  sock = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
  if (sock == INVALID_SOCKET)
  {
    return -1;
  }

  while (n_ii < 1024)
  {
    n_ii += 64;
    ptr = realloc(ii, sizeof(INTERFACE_INFO) * n_ii);
    if (!ptr)
    {
      free(ii);
      closesocket(sock);
      errno = ENOMEM;
      return -1;
    }

    ii = ptr;
    if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, 0, 0, ii, n_ii * sizeof(INTERFACE_INFO), &length, 0, 0) == SOCKET_ERROR)
    {
      error = WSAGetLastError();
      if (error == WSAEFAULT || error == WSAENOBUFS)
      {
        continue;                                     
      }
      closesocket(sock);
      free(ii);
      return -1;
    }

    break;
  }

  for (i = 0; i < length / sizeof(INTERFACE_INFO); ++i)
  {
    run_ifaddr_callback(callback, cb_data, (struct sockaddr *)&ii[i].iiAddress, (struct sockaddr *)&ii[i].iiNetmask);
  }

  closesocket(sock);
  free(ii);
  return 0;
}
#elif HAVE_GETIFADDRS                

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

   
                                                                            
                                                          
   
                                                                       
                                
   
int
pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data)
{
  struct ifaddrs *ifa, *l;

  if (getifaddrs(&ifa) < 0)
  {
    return -1;
  }

  for (l = ifa; l; l = l->ifa_next)
  {
    run_ifaddr_callback(callback, cb_data, l->ifa_addr, l->ifa_netmask);
  }

  freeifaddrs(ifa);
  return 0;
}
#else                                 

#include <sys/ioctl.h>

#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

   
                                                         
                                                           
   
                                                        
                                                       
                                                            
                                                           
                                      
   

#if defined(SIOCGLIFCONF) && !defined(__hpux)

   
                                                                            
                                                          
   
                                          
   
int
pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data)
{
  struct lifconf lifc;
  struct lifreq *lifr, lmask;
  struct sockaddr *addr, *mask;
  char *ptr, *buffer = NULL;
  size_t n_buffer = 1024;
  pgsocket sock, fd;

#ifdef HAVE_IPV6
  pgsocket sock6;
#endif
  int i, total;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == PGINVALID_SOCKET)
  {
    return -1;
  }

  while (n_buffer < 1024 * 100)
  {
    n_buffer += 1024;
    ptr = realloc(buffer, n_buffer);
    if (!ptr)
    {
      free(buffer);
      close(sock);
      errno = ENOMEM;
      return -1;
    }

    memset(&lifc, 0, sizeof(lifc));
    lifc.lifc_family = AF_UNSPEC;
    lifc.lifc_buf = buffer = ptr;
    lifc.lifc_len = n_buffer;

    if (ioctl(sock, SIOCGLIFCONF, &lifc) < 0)
    {
      if (errno == EINVAL)
      {
        continue;
      }
      free(buffer);
      close(sock);
      return -1;
    }

       
                                                                   
                                                                           
                                           
       
    if (lifc.lifc_len < n_buffer - 1024)
    {
      break;
    }
  }

#ifdef HAVE_IPV6
                                                                    
  sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock6 == PGINVALID_SOCKET)
  {
    free(buffer);
    close(sock);
    return -1;
  }
#endif

  total = lifc.lifc_len / sizeof(struct lifreq);
  lifr = lifc.lifc_req;
  for (i = 0; i < total; ++i)
  {
    addr = (struct sockaddr *)&lifr[i].lifr_addr;
    memcpy(&lmask, &lifr[i], sizeof(struct lifreq));
#ifdef HAVE_IPV6
    fd = (addr->sa_family == AF_INET6) ? sock6 : sock;
#else
    fd = sock;
#endif
    if (ioctl(fd, SIOCGLIFNETMASK, &lmask) < 0)
    {
      mask = NULL;
    }
    else
    {
      mask = (struct sockaddr *)&lmask.lifr_addr;
    }
    run_ifaddr_callback(callback, cb_data, addr, mask);
  }

  free(buffer);
  close(sock);
#ifdef HAVE_IPV6
  close(sock6);
#endif
  return 0;
}
#elif defined(SIOCGIFCONF)

   
                                                                       
                                                                      
                                                                   
                                                                       
                                                                 
   

                                                        
#ifndef _SIZEOF_ADDR_IFREQ

                                        
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#define _SIZEOF_ADDR_IFREQ(ifr) ((ifr).ifr_addr.sa_len > sizeof(struct sockaddr) ? (sizeof(struct ifreq) - sizeof(struct sockaddr) + (ifr).ifr_addr.sa_len) : sizeof(struct ifreq))

                                    
#else
#define _SIZEOF_ADDR_IFREQ(ifr) sizeof(struct ifreq)
#endif
#endif                          

   
                                                                            
                                                          
   
                                         
   
int
pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data)
{
  struct ifconf ifc;
  struct ifreq *ifr, *end, addr, mask;
  char *ptr, *buffer = NULL;
  size_t n_buffer = 1024;
  pgsocket sock;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == PGINVALID_SOCKET)
  {
    return -1;
  }

  while (n_buffer < 1024 * 100)
  {
    n_buffer += 1024;
    ptr = realloc(buffer, n_buffer);
    if (!ptr)
    {
      free(buffer);
      close(sock);
      errno = ENOMEM;
      return -1;
    }

    memset(&ifc, 0, sizeof(ifc));
    ifc.ifc_buf = buffer = ptr;
    ifc.ifc_len = n_buffer;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)
    {
      if (errno == EINVAL)
      {
        continue;
      }
      free(buffer);
      close(sock);
      return -1;
    }

       
                                                                   
                                                                           
                                           
       
    if (ifc.ifc_len < n_buffer - 1024)
    {
      break;
    }
  }

  end = (struct ifreq *)(buffer + ifc.ifc_len);
  for (ifr = ifc.ifc_req; ifr < end;)
  {
    memcpy(&addr, ifr, sizeof(addr));
    memcpy(&mask, ifr, sizeof(mask));
    if (ioctl(sock, SIOCGIFADDR, &addr, sizeof(addr)) == 0 && ioctl(sock, SIOCGIFNETMASK, &mask, sizeof(mask)) == 0)
    {
      run_ifaddr_callback(callback, cb_data, &addr.ifr_addr, &mask.ifr_addr);
    }
    ifr = (struct ifreq *)((char *)ifr + _SIZEOF_ADDR_IFREQ(*ifr));
  }

  free(buffer);
  close(sock);
  return 0;
}
#else                             

   
                                                                            
                                                          
   
                                                                   
                                                                      
   
int
pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data)
{
  struct sockaddr_in addr;
  struct sockaddr_storage mask;

#ifdef HAVE_IPV6
  struct sockaddr_in6 addr6;
#endif

                        
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = pg_ntoh32(0x7f000001);
  memset(&mask, 0, sizeof(mask));
  pg_sockaddr_cidr_mask(&mask, "8", AF_INET);
  run_ifaddr_callback(callback, cb_data, (struct sockaddr *)&addr, (struct sockaddr *)&mask);

#ifdef HAVE_IPV6
                    
  memset(&addr6, 0, sizeof(addr6));
  addr6.sin6_family = AF_INET6;
  addr6.sin6_addr.s6_addr[15] = 1;
  memset(&mask, 0, sizeof(mask));
  pg_sockaddr_cidr_mask(&mask, "128", AF_INET6);
  run_ifaddr_callback(callback, cb_data, (struct sockaddr *)&addr6, (struct sockaddr *)&mask);
#endif

  return 0;
}
#endif                            

#endif                       
