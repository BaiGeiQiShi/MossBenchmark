                                                                            
   
               
                                                                         
                                                                 
                                                    
   
   
                                                                         
                                                                        
   
   
                  
                                      
   
         
   
                                                         
                                                                   
                                           
   
                                                                            
   

#include "postgres_fe.h"

#include <signal.h>
#include <fcntl.h>
#include <ctype.h>

#include "libpq-fe.h"
#include "fe-auth.h"
#include "libpq-int.h"

#ifdef WIN32
#include "win32.h"
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <arpa/inet.h>
#endif

#include <sys/stat.h>

#ifdef ENABLE_THREAD_SAFETY
#ifdef WIN32
#include "pthread-win32.h"
#else
#include <pthread.h>
#endif
#endif

   
                                                                                
                                                               
   

#ifndef WIN32

#define SIGPIPE_MASKED(conn) ((conn)->sigpipe_so || (conn)->sigpipe_flag)

#ifdef ENABLE_THREAD_SAFETY

struct sigpipe_info
{
  sigset_t oldsigmask;
  bool sigpipe_pending;
  bool got_epipe;
};

#define DECLARE_SIGPIPE_INFO(spinfo) struct sigpipe_info spinfo

#define DISABLE_SIGPIPE(conn, spinfo, failaction)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (spinfo).got_epipe = false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    if (!SIGPIPE_MASKED(conn))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      if (pq_block_sigpipe(&(spinfo).oldsigmask, &(spinfo).sigpipe_pending) < 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
        failaction;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#define REMEMBER_EPIPE(spinfo, cond)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (cond)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      (spinfo).got_epipe = true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

#define RESTORE_SIGPIPE(conn, spinfo)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!SIGPIPE_MASKED(conn))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      pq_reset_sigpipe(&(spinfo).oldsigmask, (spinfo).sigpipe_pending, (spinfo).got_epipe);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  } while (0)
#else                            

#define DECLARE_SIGPIPE_INFO(spinfo) pqsigfunc spinfo = NULL

#define DISABLE_SIGPIPE(conn, spinfo, failaction)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!SIGPIPE_MASKED(conn))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      spinfo = pqsignal(SIGPIPE, SIG_IGN);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)

#define REMEMBER_EPIPE(spinfo, cond)

#define RESTORE_SIGPIPE(conn, spinfo)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!SIGPIPE_MASKED(conn))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      pqsignal(SIGPIPE, spinfo);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)
#endif                           
#else             

#define DECLARE_SIGPIPE_INFO(spinfo)
#define DISABLE_SIGPIPE(conn, spinfo, failaction)
#define REMEMBER_EPIPE(spinfo, cond)
#define RESTORE_SIGPIPE(conn, spinfo)
#endif            

                                                                  
                                                   
                                                                  

int
PQsslInUse(PGconn *conn)
{
  if (!conn)
  {
    return 0;
  }
  return conn->ssl_in_use;
}

   
                                                                  
                        
   
void
PQinitSSL(int do_init)
{
#ifdef USE_SSL
  pgtls_init_library(do_init, do_init);
#endif
}

   
                                                                  
                                         
   
void
PQinitOpenSSL(int do_ssl, int do_crypto)
{
#ifdef USE_SSL
  pgtls_init_library(do_ssl, do_crypto);
#endif
}

   
                                 
   
int
pqsecure_initialize(PGconn *conn)
{
  int r = 0;

#ifdef USE_SSL
  r = pgtls_init(conn);
#endif

  return r;
}

   
                                                   
   
PostgresPollingStatusType
pqsecure_open_client(PGconn *conn)
{
#ifdef USE_SSL
  return pgtls_open_client(conn);
#else
                          
  return PGRES_POLLING_FAILED;
#endif
}

   
                         
   
void
pqsecure_close(PGconn *conn)
{
#ifdef USE_SSL
  if (conn->ssl_in_use)
  {
    pgtls_close(conn);
  }
#endif
}

   
                                       
   
                                                                           
                                                                           
                                                       
   
ssize_t
pqsecure_read(PGconn *conn, void *ptr, size_t len)
{
  ssize_t n;

#ifdef USE_SSL
  if (conn->ssl_in_use)
  {
    n = pgtls_read(conn, ptr, len);
  }
  else
#endif
#ifdef ENABLE_GSS
      if (conn->gssenc)
  {
    n = pg_GSS_read(conn, ptr, len);
  }
  else
#endif
  {
    n = pqsecure_raw_read(conn, ptr, len);
  }

  return n;
}

ssize_t
pqsecure_raw_read(PGconn *conn, void *ptr, size_t len)
{
  ssize_t n;
  int result_errno = 0;
  char sebuf[PG_STRERROR_R_BUFLEN];

  n = recv(conn->sock, ptr, len, 0);

  if (n < 0)
  {
    result_errno = SOCK_ERRNO;

                                          
    switch (result_errno)
    {
#ifdef EAGAIN
    case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK:
#endif
    case EINTR:
                                                         
      break;

#ifdef ECONNRESET
    case ECONNRESET:
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server closed the connection unexpectedly\n"
                                                           "\tThis probably means the server terminated abnormally\n"
                                                           "\tbefore or while processing the request.\n"));
      break;
#endif

    default:
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not receive data from server: %s\n"), SOCK_STRERROR(result_errno, sebuf, sizeof(sebuf)));
      break;
    }
  }

                                                     
  SOCK_ERRNO_SET(result_errno);

  return n;
}

   
                                      
   
                                                                           
                                                                           
                                                       
   
ssize_t
pqsecure_write(PGconn *conn, const void *ptr, size_t len)
{
  ssize_t n;

#ifdef USE_SSL
  if (conn->ssl_in_use)
  {
    n = pgtls_write(conn, ptr, len);
  }
  else
#endif
#ifdef ENABLE_GSS
      if (conn->gssenc)
  {
    n = pg_GSS_write(conn, ptr, len);
  }
  else
#endif
  {
    n = pqsecure_raw_write(conn, ptr, len);
  }

  return n;
}

ssize_t
pqsecure_raw_write(PGconn *conn, const void *ptr, size_t len)
{
  ssize_t n;
  int flags = 0;
  int result_errno = 0;
  char sebuf[PG_STRERROR_R_BUFLEN];

  DECLARE_SIGPIPE_INFO(spinfo);

#ifdef MSG_NOSIGNAL
  if (conn->sigpipe_flag)
  {
    flags |= MSG_NOSIGNAL;
  }

retry_masked:
#endif                   

  DISABLE_SIGPIPE(conn, spinfo, return -1);

  n = send(conn->sock, ptr, len, flags);

  if (n < 0)
  {
    result_errno = SOCK_ERRNO;

       
                                                                           
                                                                         
                                    
       
#ifdef MSG_NOSIGNAL
    if (flags != 0 && result_errno == EINVAL)
    {
      conn->sigpipe_flag = false;
      flags = 0;
      goto retry_masked;
    }
#endif                   

                                          
    switch (result_errno)
    {
#ifdef EAGAIN
    case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK:
#endif
    case EINTR:
                                                         
      break;

    case EPIPE:
                              
      REMEMBER_EPIPE(spinfo, true);

#ifdef ECONNRESET
                     

    case ECONNRESET:
#endif
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server closed the connection unexpectedly\n"
                                                           "\tThis probably means the server terminated abnormally\n"
                                                           "\tbefore or while processing the request.\n"));
      break;

    default:
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not send data to server: %s\n"), SOCK_STRERROR(result_errno, sebuf, sizeof(sebuf)));
      break;
    }
  }

  RESTORE_SIGPIPE(conn, spinfo);

                                                     
  SOCK_ERRNO_SET(result_errno);

  return n;
}

                                                                          
#ifndef USE_SSL

void *
PQgetssl(PGconn *conn)
{
  return NULL;
}

void *
PQsslStruct(PGconn *conn, const char *struct_name)
{
  return NULL;
}

const char *
PQsslAttribute(PGconn *conn, const char *attribute_name)
{
  return NULL;
}

const char *const *
PQsslAttributeNames(PGconn *conn)
{
  static const char *const result[] = {NULL};

  return result;
}
#endif              

                                                                                   
#ifndef ENABLE_GSS

void *
PQgetgssctx(PGconn *conn)
{
  return NULL;
}

int
PQgssEncInUse(PGconn *conn)
{
  return 0;
}

#endif                 

#if defined(ENABLE_THREAD_SAFETY) && !defined(WIN32)

   
                                                                             
                    
   
int
pq_block_sigpipe(sigset_t *osigset, bool *sigpipe_pending)
{
  sigset_t sigpipe_sigset;
  sigset_t sigset;

  sigemptyset(&sigpipe_sigset);
  sigaddset(&sigpipe_sigset, SIGPIPE);

                                                            
  SOCK_ERRNO_SET(pthread_sigmask(SIG_BLOCK, &sigpipe_sigset, osigset));
  if (SOCK_ERRNO)
  {
    return -1;
  }

                                                                   
  if (sigismember(osigset, SIGPIPE))
  {
                                     
    if (sigpending(&sigset) != 0)
    {
      return -1;
    }

    if (sigismember(&sigset, SIGPIPE))
    {
      *sigpipe_pending = true;
    }
    else
    {
      *sigpipe_pending = false;
    }
  }
  else
  {
    *sigpipe_pending = false;
  }

  return 0;
}

   
                                                          
   
                                                                           
                                                                        
                                                                           
                                                                            
                              
   
                                                                    
                                                                          
                                                                             
                                      
   
                                                                        
                                                                          
                                                           
   
void
pq_reset_sigpipe(sigset_t *osigset, bool sigpipe_pending, bool got_epipe)
{
  int save_errno = SOCK_ERRNO;
  int signo;
  sigset_t sigset;

                                              
  if (got_epipe && !sigpipe_pending)
  {
    if (sigpending(&sigset) == 0 && sigismember(&sigset, SIGPIPE))
    {
      sigset_t sigpipe_sigset;

      sigemptyset(&sigpipe_sigset);
      sigaddset(&sigpipe_sigset, SIGPIPE);

      sigwait(&sigpipe_sigset, &signo);
    }
  }

                                
  pthread_sigmask(SIG_SETMASK, osigset, NULL);

  SOCK_ERRNO_SET(save_errno);
}

#endif                                     
