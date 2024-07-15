                                                                            
   
              
                                                                     
   
                                                                         
                                                                        
   
   
                  
                         
   
                                                                            
   
#include "c.h"

   
                                                                               
                                 
   
#undef strerror
#undef strerror_r

static char *
gnuish_strerror_r(int errnum, char *buf, size_t buflen);
static char *
get_errno_symbol(int errnum);
#ifdef WIN32
static char *
win32_socket_strerror(int errnum, char *buf, size_t buflen);
#endif

   
                                               
   
char *
pg_strerror(int errnum)
{
  static char errorstr_buf[PG_STRERROR_R_BUFLEN];

  return pg_strerror_r(errnum, errorstr_buf, sizeof(errorstr_buf));
}

   
                                                 
   
char *
pg_strerror_r(int errnum, char *buf, size_t buflen)
{
  char *str;

                                                                    
#ifdef WIN32
                                                
  if (errnum >= 10000 && errnum <= 11999)
  {
    return win32_socket_strerror(errnum, buf, buflen);
  }
#endif

                                                                 
  str = gnuish_strerror_r(errnum, buf, buflen);

     
                                                                           
                                                                         
                                                                            
                                                                             
                                                                     
     
  if (str == NULL || *str == '\0' || *str == '?')
  {
    str = get_errno_symbol(errnum);
  }

  if (str == NULL)
  {
    snprintf(buf, buflen, _("operating system error %d"), errnum);
    str = buf;
  }

  return str;
}

   
                                                                             
                                                                             
                                                               
   
static char *
gnuish_strerror_r(int errnum, char *buf, size_t buflen)
{
#ifdef HAVE_STRERROR_R
#ifdef STRERROR_R_INT
                 
  if (strerror_r(errnum, buf, buflen) == 0)
  {
    return buf;
  }
  return NULL;                                   
#else
               
  return strerror_r(errnum, buf, buflen);
#endif
#else                       
  char *sbuf = strerror(errnum);

  if (sbuf == NULL)                                      
  {
    return NULL;
  }
                                                                     
  strlcpy(buf, sbuf, buflen);
  return buf;
#endif
}

   
                                                       
                                             
   
static char *
get_errno_symbol(int errnum)
{
  switch (errnum)
  {
  case E2BIG:
    return "E2BIG";
  case EACCES:
    return "EACCES";
#ifdef EADDRINUSE
  case EADDRINUSE:
    return "EADDRINUSE";
#endif
#ifdef EADDRNOTAVAIL
  case EADDRNOTAVAIL:
    return "EADDRNOTAVAIL";
#endif
  case EAFNOSUPPORT:
    return "EAFNOSUPPORT";
#ifdef EAGAIN
  case EAGAIN:
    return "EAGAIN";
#endif
#ifdef EALREADY
  case EALREADY:
    return "EALREADY";
#endif
  case EBADF:
    return "EBADF";
#ifdef EBADMSG
  case EBADMSG:
    return "EBADMSG";
#endif
  case EBUSY:
    return "EBUSY";
  case ECHILD:
    return "ECHILD";
#ifdef ECONNABORTED
  case ECONNABORTED:
    return "ECONNABORTED";
#endif
  case ECONNREFUSED:
    return "ECONNREFUSED";
#ifdef ECONNRESET
  case ECONNRESET:
    return "ECONNRESET";
#endif
  case EDEADLK:
    return "EDEADLK";
  case EDOM:
    return "EDOM";
  case EEXIST:
    return "EEXIST";
  case EFAULT:
    return "EFAULT";
  case EFBIG:
    return "EFBIG";
#ifdef EHOSTUNREACH
  case EHOSTUNREACH:
    return "EHOSTUNREACH";
#endif
  case EIDRM:
    return "EIDRM";
  case EINPROGRESS:
    return "EINPROGRESS";
  case EINTR:
    return "EINTR";
  case EINVAL:
    return "EINVAL";
  case EIO:
    return "EIO";
#ifdef EISCONN
  case EISCONN:
    return "EISCONN";
#endif
  case EISDIR:
    return "EISDIR";
#ifdef ELOOP
  case ELOOP:
    return "ELOOP";
#endif
  case EMFILE:
    return "EMFILE";
  case EMLINK:
    return "EMLINK";
  case EMSGSIZE:
    return "EMSGSIZE";
  case ENAMETOOLONG:
    return "ENAMETOOLONG";
  case ENFILE:
    return "ENFILE";
  case ENOBUFS:
    return "ENOBUFS";
  case ENODEV:
    return "ENODEV";
  case ENOENT:
    return "ENOENT";
  case ENOEXEC:
    return "ENOEXEC";
  case ENOMEM:
    return "ENOMEM";
  case ENOSPC:
    return "ENOSPC";
  case ENOSYS:
    return "ENOSYS";
#ifdef ENOTCONN
  case ENOTCONN:
    return "ENOTCONN";
#endif
  case ENOTDIR:
    return "ENOTDIR";
#if defined(ENOTEMPTY) && (ENOTEMPTY != EEXIST)                       
  case ENOTEMPTY:
    return "ENOTEMPTY";
#endif
#ifdef ENOTSOCK
  case ENOTSOCK:
    return "ENOTSOCK";
#endif
#ifdef ENOTSUP
  case ENOTSUP:
    return "ENOTSUP";
#endif
  case ENOTTY:
    return "ENOTTY";
  case ENXIO:
    return "ENXIO";
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (EOPNOTSUPP != ENOTSUP))
  case EOPNOTSUPP:
    return "EOPNOTSUPP";
#endif
#ifdef EOVERFLOW
  case EOVERFLOW:
    return "EOVERFLOW";
#endif
  case EPERM:
    return "EPERM";
  case EPIPE:
    return "EPIPE";
  case EPROTONOSUPPORT:
    return "EPROTONOSUPPORT";
  case ERANGE:
    return "ERANGE";
#ifdef EROFS
  case EROFS:
    return "EROFS";
#endif
  case ESRCH:
    return "ESRCH";
#ifdef ETIMEDOUT
  case ETIMEDOUT:
    return "ETIMEDOUT";
#endif
#ifdef ETXTBSY
  case ETXTBSY:
    return "ETXTBSY";
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
  case EWOULDBLOCK:
    return "EWOULDBLOCK";
#endif
  case EXDEV:
    return "EXDEV";
  }

  return NULL;
}

#ifdef WIN32

   
                                                                               
   
static char *
win32_socket_strerror(int errnum, char *buf, size_t buflen)
{
  static HANDLE handleDLL = INVALID_HANDLE_VALUE;

  if (handleDLL == INVALID_HANDLE_VALUE)
  {
    handleDLL = LoadLibraryEx("netmsg.dll", NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
    if (handleDLL == NULL)
    {
      snprintf(buf, buflen, "winsock error %d (could not load netmsg.dll to translate: error code %lu)", errnum, GetLastError());
      return buf;
    }
  }

  ZeroMemory(buf, buflen);
  if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE, handleDLL, errnum, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), buf, buflen - 1, NULL) == 0)
  {
                          
    snprintf(buf, buflen, "unrecognized winsock error %d", errnum);
  }

  return buf;
}

#endif            
