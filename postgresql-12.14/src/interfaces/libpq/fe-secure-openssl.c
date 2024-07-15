                                                                            
   
                       
                     
   
   
                                                                         
                                                                        
   
   
                  
                                              
   
         
   
                                                         
                                                                   
                                           
   
                                                                            
   

#include "postgres_fe.h"

#include <signal.h>
#include <fcntl.h>
#include <ctype.h>

#include "libpq-fe.h"
#include "fe-auth.h"
#include "fe-secure-common.h"
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

#include <openssl/ssl.h>
#include <openssl/conf.h>
#ifdef USE_SSL_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/x509v3.h>

static int
verify_cb(int ok, X509_STORE_CTX *ctx);
static int
openssl_verify_peer_name_matches_certificate_name(PGconn *conn, ASN1_STRING *name, char **store_name);
static void
destroy_ssl_system(void);
static int
initialize_SSL(PGconn *conn);
static PostgresPollingStatusType
open_client_SSL(PGconn *);
static char *
SSLerrmessage(unsigned long ecode);
static void
SSLerrfree(char *buf);

static int
my_sock_read(BIO *h, char *buf, int size);
static int
my_sock_write(BIO *h, const char *buf, int size);
static BIO_METHOD *
my_BIO_s_socket(void);
static int
my_SSL_set_fd(PGconn *conn, int fd);

static bool pq_init_ssl_lib = true;
static bool pq_init_crypto_lib = true;

static bool ssl_lib_initialized = false;

#ifdef ENABLE_THREAD_SAFETY
static long ssl_open_connections = 0;

#ifndef WIN32
static pthread_mutex_t ssl_config_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
static pthread_mutex_t ssl_config_mutex = NULL;
static long win32_ssl_create_mutex = 0;
#endif
#endif                           

                                                                  
                                                   
                                                                  

void
pgtls_init_library(bool do_ssl, int do_crypto)
{
#ifdef ENABLE_THREAD_SAFETY

     
                                                                           
                              
     
  if (ssl_open_connections != 0)
  {
    return;
  }
#endif

  pq_init_ssl_lib = do_ssl;
  pq_init_crypto_lib = do_crypto;
}

PostgresPollingStatusType
pgtls_open_client(PGconn *conn)
{
                           
  if (conn->ssl == NULL)
  {
       
                                                                
                                                       
       
    if (initialize_SSL(conn) != 0)
    {
                                                                      
      pgtls_close(conn);
      return PGRES_POLLING_FAILED;
    }
  }

                                              
  return open_client_SSL(conn);
}

ssize_t
pgtls_read(PGconn *conn, void *ptr, size_t len)
{
  ssize_t n;
  int result_errno = 0;
  char sebuf[PG_STRERROR_R_BUFLEN];
  int err;
  unsigned long ecode;

rloop:

     
                                                                        
                                                                        
                                                                            
                                                                         
                                                                            
                                                                          
                                                        
     
  SOCK_ERRNO_SET(0);
  ERR_clear_error();
  n = SSL_read(conn->ssl, ptr, len);
  err = SSL_get_error(conn->ssl, n);

     
                                                                       
                                                                           
                                                                       
                                                                          
                                                                
                                    
     
  ecode = (err != SSL_ERROR_NONE || n < 0) ? ERR_get_error() : 0;
  switch (err)
  {
  case SSL_ERROR_NONE:
    if (n < 0)
    {
                                                                 
      printfPQExpBuffer(&conn->errorMessage, "SSL_read failed but did not provide error information\n");
                                           
      result_errno = ECONNRESET;
    }
    break;
  case SSL_ERROR_WANT_READ:
    n = 0;
    break;
  case SSL_ERROR_WANT_WRITE:

       
                                                                   
                                                             
                                                                  
                                                  
       
    goto rloop;
  case SSL_ERROR_SYSCALL:
    if (n < 0)
    {
      result_errno = SOCK_ERRNO;
      if (result_errno == EPIPE || result_errno == ECONNRESET)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server closed the connection unexpectedly\n"
                                                             "\tThis probably means the server terminated abnormally\n"
                                                             "\tbefore or while processing the request.\n"));
      }
      else
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL SYSCALL error: %s\n"), SOCK_STRERROR(result_errno, sebuf, sizeof(sebuf)));
      }
    }
    else
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL SYSCALL error: EOF detected\n"));
                                           
      result_errno = ECONNRESET;
      n = -1;
    }
    break;
  case SSL_ERROR_SSL:
  {
    char *errm = SSLerrmessage(ecode);

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL error: %s\n"), errm);
    SSLerrfree(errm);
                                         
    result_errno = ECONNRESET;
    n = -1;
    break;
  }
  case SSL_ERROR_ZERO_RETURN:

       
                                                                       
                                                                   
                     
       
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL connection has been closed unexpectedly\n"));
    result_errno = ECONNRESET;
    n = -1;
    break;
  default:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unrecognized SSL error code: %d\n"), err);
                                         
    result_errno = ECONNRESET;
    n = -1;
    break;
  }

                                                     
  SOCK_ERRNO_SET(result_errno);

  return n;
}

bool
pgtls_read_pending(PGconn *conn)
{
  return SSL_pending(conn->ssl) > 0;
}

ssize_t
pgtls_write(PGconn *conn, const void *ptr, size_t len)
{
  ssize_t n;
  int result_errno = 0;
  char sebuf[PG_STRERROR_R_BUFLEN];
  int err;
  unsigned long ecode;

  SOCK_ERRNO_SET(0);
  ERR_clear_error();
  n = SSL_write(conn->ssl, ptr, len);
  err = SSL_get_error(conn->ssl, n);
  ecode = (err != SSL_ERROR_NONE || n < 0) ? ERR_get_error() : 0;
  switch (err)
  {
  case SSL_ERROR_NONE:
    if (n < 0)
    {
                                                                 
      printfPQExpBuffer(&conn->errorMessage, "SSL_write failed but did not provide error information\n");
                                           
      result_errno = ECONNRESET;
    }
    break;
  case SSL_ERROR_WANT_READ:

       
                                                                     
                                                                   
       
    n = 0;
    break;
  case SSL_ERROR_WANT_WRITE:
    n = 0;
    break;
  case SSL_ERROR_SYSCALL:
    if (n < 0)
    {
      result_errno = SOCK_ERRNO;
      if (result_errno == EPIPE || result_errno == ECONNRESET)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server closed the connection unexpectedly\n"
                                                             "\tThis probably means the server terminated abnormally\n"
                                                             "\tbefore or while processing the request.\n"));
      }
      else
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL SYSCALL error: %s\n"), SOCK_STRERROR(result_errno, sebuf, sizeof(sebuf)));
      }
    }
    else
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL SYSCALL error: EOF detected\n"));
                                           
      result_errno = ECONNRESET;
      n = -1;
    }
    break;
  case SSL_ERROR_SSL:
  {
    char *errm = SSLerrmessage(ecode);

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL error: %s\n"), errm);
    SSLerrfree(errm);
                                         
    result_errno = ECONNRESET;
    n = -1;
    break;
  }
  case SSL_ERROR_ZERO_RETURN:

       
                                                                       
                                                                   
                     
       
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL connection has been closed unexpectedly\n"));
    result_errno = ECONNRESET;
    n = -1;
    break;
  default:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unrecognized SSL error code: %d\n"), err);
                                         
    result_errno = ECONNRESET;
    n = -1;
    break;
  }

                                                     
  SOCK_ERRNO_SET(result_errno);

  return n;
}

#ifdef HAVE_X509_GET_SIGNATURE_NID
char *
pgtls_get_peer_certificate_hash(PGconn *conn, size_t *len)
{
  X509 *peer_cert;
  const EVP_MD *algo_type;
  unsigned char hash[EVP_MAX_MD_SIZE];                       
  unsigned int hash_size;
  int algo_nid;
  char *cert_hash;

  *len = 0;

  if (!conn->peer)
  {
    return NULL;
  }

  peer_cert = conn->peer;

     
                                                                          
                                      
     
  if (!OBJ_find_sigid_algs(X509_get_signature_nid(peer_cert), &algo_nid, NULL))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not determine server certificate signature algorithm\n"));
    return NULL;
  }

     
                                                                          
                                                             
                                                                           
                                                                
     
  switch (algo_nid)
  {
  case NID_md5:
  case NID_sha1:
    algo_type = EVP_sha256();
    break;
  default:
    algo_type = EVP_get_digestbynid(algo_nid);
    if (algo_type == NULL)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not find digest for NID %s\n"), OBJ_nid2sn(algo_nid));
      return NULL;
    }
    break;
  }

  if (!X509_digest(peer_cert, algo_type, hash, &hash_size))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not generate peer certificate hash\n"));
    return NULL;
  }

                   
  cert_hash = malloc(hash_size);
  if (cert_hash == NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return NULL;
  }
  memcpy(cert_hash, hash, hash_size);
  *len = hash_size;

  return cert_hash;
}
#endif                                  

                                                                  
                                    
                                                                  

   
                                     
   
                                                               
                                                                 
                                                      
   
                                                                   
                                                                
                                         
   
static int
verify_cb(int ok, X509_STORE_CTX *ctx)
{
  return ok;
}

   
                                   
                                                                              
                          
   
static int
openssl_verify_peer_name_matches_certificate_name(PGconn *conn, ASN1_STRING *name_entry, char **store_name)
{
  int len;
  const unsigned char *namedata;

                            
  if (name_entry == NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL certificate's name entry is missing\n"));
    return -1;
  }

     
                                                            
     
#ifdef HAVE_ASN1_STRING_GET0_DATA
  namedata = ASN1_STRING_get0_data(name_entry);
#else
  namedata = ASN1_STRING_data(name_entry);
#endif
  len = ASN1_STRING_length(name_entry);

                                                                     
  return pq_verify_peer_name_matches_certificate_name(conn, (const char *)namedata, len, store_name);
}

   
                                                                            
   
                                                                               
   
int
pgtls_verify_peer_name_matches_certificate_guts(PGconn *conn, int *names_examined, char **first_name)
{
  STACK_OF(GENERAL_NAME) * peer_san;
  int i;
  int rc = 0;

     
                                                                           
                                                             
     
  peer_san = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(conn->peer, NID_subject_alt_name, NULL, NULL);

  if (peer_san)
  {
    int san_len = sk_GENERAL_NAME_num(peer_san);

    for (i = 0; i < san_len; i++)
    {
      const GENERAL_NAME *name = sk_GENERAL_NAME_value(peer_san, i);

      if (name->type == GEN_DNS)
      {
        char *alt_name;

        (*names_examined)++;
        rc = openssl_verify_peer_name_matches_certificate_name(conn, name->d.dNSName, &alt_name);

        if (alt_name)
        {
          if (!*first_name)
          {
            *first_name = alt_name;
          }
          else
          {
            free(alt_name);
          }
        }
      }
      if (rc != 0)
      {
        break;
      }
    }
    sk_GENERAL_NAME_pop_free(peer_san, GENERAL_NAME_free);
  }

     
                                                                        
                  
     
                                                                         
                                                  
     
  if (*names_examined == 0)
  {
    X509_NAME *subject_name;

    subject_name = X509_get_subject_name(conn->peer);
    if (subject_name != NULL)
    {
      int cn_index;

      cn_index = X509_NAME_get_index_by_NID(subject_name, NID_commonName, -1);
      if (cn_index >= 0)
      {
        (*names_examined)++;
        rc = openssl_verify_peer_name_matches_certificate_name(conn, X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subject_name, cn_index)), first_name);
      }
    }
  }

  return rc;
}

#if defined(ENABLE_THREAD_SAFETY) && defined(HAVE_CRYPTO_LOCK)
   
                                                                    
                                                              
                                                                   
                                                                  
                                      
   

static unsigned long
pq_threadidcallback(void)
{
     
                                                                             
                                                                             
                              
     
  return (unsigned long)pthread_self();
}

static pthread_mutex_t *pq_lockarray;

static void
pq_lockingcallback(int mode, int n, const char *file, int line)
{
  if (mode & CRYPTO_LOCK)
  {
    if (pthread_mutex_lock(&pq_lockarray[n]))
    {
      PGTHREAD_ERROR("failed to lock mutex");
    }
  }
  else
  {
    if (pthread_mutex_unlock(&pq_lockarray[n]))
    {
      PGTHREAD_ERROR("failed to unlock mutex");
    }
  }
}
#endif                                               

   
                           
   
                                                                             
                         
   
                                                                           
                                                                            
                
   
int
pgtls_init(PGconn *conn)
{
#ifdef ENABLE_THREAD_SAFETY
#ifdef WIN32
                                                                   
  if (ssl_config_mutex == NULL)
  {
    while (InterlockedExchange(&win32_ssl_create_mutex, 1) == 1)
                                             ;
    if (ssl_config_mutex == NULL)
    {
      if (pthread_mutex_init(&ssl_config_mutex, NULL))
      {
        return -1;
      }
    }
    InterlockedExchange(&win32_ssl_create_mutex, 0);
  }
#endif
  if (pthread_mutex_lock(&ssl_config_mutex))
  {
    return -1;
  }

#ifdef HAVE_CRYPTO_LOCK
  if (pq_init_crypto_lib)
  {
       
                                                                  
                                                          
       
    if (pq_lockarray == NULL)
    {
      int i;

      pq_lockarray = malloc(sizeof(pthread_mutex_t) * CRYPTO_num_locks());
      if (!pq_lockarray)
      {
        pthread_mutex_unlock(&ssl_config_mutex);
        return -1;
      }
      for (i = 0; i < CRYPTO_num_locks(); i++)
      {
        if (pthread_mutex_init(&pq_lockarray[i], NULL))
        {
          free(pq_lockarray);
          pq_lockarray = NULL;
          pthread_mutex_unlock(&ssl_config_mutex);
          return -1;
        }
      }
    }

    if (ssl_open_connections++ == 0)
    {
         
                                                                      
                                                                      
         
      if (CRYPTO_get_id_callback() == NULL)
      {
        CRYPTO_set_id_callback(pq_threadidcallback);
      }
      if (CRYPTO_get_locking_callback() == NULL)
      {
        CRYPTO_set_locking_callback(pq_lockingcallback);
      }
    }
  }
#endif                       
#endif                           

  if (!ssl_lib_initialized)
  {
    if (pq_init_ssl_lib)
    {
#ifdef HAVE_OPENSSL_INIT_SSL
      OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL);
#else
      OPENSSL_config(NULL);
      SSL_library_init();
      SSL_load_error_strings();
#endif
    }
    ssl_lib_initialized = true;
  }

#ifdef ENABLE_THREAD_SAFETY
  pthread_mutex_unlock(&ssl_config_mutex);
#endif
  return 0;
}

   
                                                                    
                                                                          
                                                                     
                                                            
                                                                      
                   
   
                                                                     
                                                                      
                               
   
static void
destroy_ssl_system(void)
{
#if defined(ENABLE_THREAD_SAFETY) && defined(HAVE_CRYPTO_LOCK)
                                        
  if (pthread_mutex_lock(&ssl_config_mutex))
  {
    return;
  }

  if (pq_init_crypto_lib && ssl_open_connections > 0)
  {
    --ssl_open_connections;
  }

  if (pq_init_crypto_lib && ssl_open_connections == 0)
  {
       
                                                                      
                                                  
       
    if (CRYPTO_get_locking_callback() == pq_lockingcallback)
    {
      CRYPTO_set_locking_callback(NULL);
    }
    if (CRYPTO_get_id_callback() == pq_threadidcallback)
    {
      CRYPTO_set_id_callback(NULL);
    }

       
                                                                          
                                                                    
       
                                                                         
                
       
  }

  pthread_mutex_unlock(&ssl_config_mutex);
#endif
}

   
                                                                      
                                      
   
                                                                          
   
static int
initialize_SSL(PGconn *conn)
{
  SSL_CTX *SSL_context;
  struct stat buf;
  char homedir[MAXPGPATH];
  char fnbuf[MAXPGPATH];
  char sebuf[PG_STRERROR_R_BUFLEN];
  bool have_homedir;
  bool have_cert;
  bool have_rootcert;
  EVP_PKEY *pkey = NULL;

     
                                                                         
                                                                        
                           
     
  if (!(conn->sslcert && strlen(conn->sslcert) > 0) || !(conn->sslkey && strlen(conn->sslkey) > 0) || !(conn->sslrootcert && strlen(conn->sslrootcert) > 0) || !(conn->sslcrl && strlen(conn->sslcrl) > 0))
  {
    have_homedir = pqGetHomeDirectory(homedir, sizeof(homedir));
  }
  else                    
  {
    have_homedir = false;
  }

     
                                  
     
                                                                           
                                                                       
                                                                             
     
  SSL_context = SSL_CTX_new(SSLv23_method());
  if (!SSL_context)
  {
    char *err = SSLerrmessage(ERR_get_error());

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not create SSL context: %s\n"), err);
    SSLerrfree(err);
    return -1;
  }

                                     
  SSL_CTX_set_options(SSL_context, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

     
                                                                           
                                                     
     
  SSL_CTX_set_mode(SSL_context, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

     
                                                                         
                                                                       
                                                           
     
  if (conn->sslrootcert && strlen(conn->sslrootcert) > 0)
  {
    strlcpy(fnbuf, conn->sslrootcert, sizeof(fnbuf));
  }
  else if (have_homedir)
  {
    snprintf(fnbuf, sizeof(fnbuf), "%s/%s", homedir, ROOT_CERT_FILE);
  }
  else
  {
    fnbuf[0] = '\0';
  }

  if (fnbuf[0] != '\0' && stat(fnbuf, &buf) == 0)
  {
    X509_STORE *cvstore;

    if (SSL_CTX_load_verify_locations(SSL_context, fnbuf, NULL) != 1)
    {
      char *err = SSLerrmessage(ERR_get_error());

      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not read root certificate file \"%s\": %s\n"), fnbuf, err);
      SSLerrfree(err);
      SSL_CTX_free(SSL_context);
      return -1;
    }

    if ((cvstore = SSL_CTX_get_cert_store(SSL_context)) != NULL)
    {
      if (conn->sslcrl && strlen(conn->sslcrl) > 0)
      {
        strlcpy(fnbuf, conn->sslcrl, sizeof(fnbuf));
      }
      else if (have_homedir)
      {
        snprintf(fnbuf, sizeof(fnbuf), "%s/%s", homedir, ROOT_CRL_FILE);
      }
      else
      {
        fnbuf[0] = '\0';
      }

                                                                 
      if (fnbuf[0] != '\0' && X509_STORE_load_locations(cvstore, fnbuf, NULL) == 1)
      {
                                                                 
#ifdef X509_V_FLAG_CRL_CHECK
        X509_STORE_set_flags(cvstore, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
#else
        char *err = SSLerrmessage(ERR_get_error());

        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL library does not support CRL certificates (file \"%s\")\n"), fnbuf);
        SSLerrfree(err);
        SSL_CTX_free(SSL_context);
        return -1;
#endif
      }
                                                                 
      ERR_clear_error();
    }
    have_rootcert = true;
  }
  else
  {
       
                                                                     
                                                                        
                                                        
       
    if (conn->sslmode[0] == 'v')                                   
    {
         
                                                                 
                                                                        
                                                                        
         
      if (fnbuf[0] == '\0')
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not get home directory to locate root certificate file\n"
                                                             "Either provide the file or change sslmode to disable server certificate verification.\n"));
      }
      else
      {
        printfPQExpBuffer(&conn->errorMessage,
            libpq_gettext("root certificate file \"%s\" does not exist\n"
                          "Either provide the file or change sslmode to disable server certificate verification.\n"),
            fnbuf);
      }
      SSL_CTX_free(SSL_context);
      return -1;
    }
    have_rootcert = false;
  }

                                        
  if (conn->sslcert && strlen(conn->sslcert) > 0)
  {
    strlcpy(fnbuf, conn->sslcert, sizeof(fnbuf));
  }
  else if (have_homedir)
  {
    snprintf(fnbuf, sizeof(fnbuf), "%s/%s", homedir, USER_CERT_FILE);
  }
  else
  {
    fnbuf[0] = '\0';
  }

  if (fnbuf[0] == '\0')
  {
                                                          
    have_cert = false;
  }
  else if (stat(fnbuf, &buf) != 0)
  {
       
                                                                        
                                                                   
                                          
       
    if (errno != ENOENT && errno != ENOTDIR)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not open certificate file \"%s\": %s\n"), fnbuf, strerror_r(errno, sebuf, sizeof(sebuf)));
      SSL_CTX_free(SSL_context);
      return -1;
    }
    have_cert = false;
  }
  else
  {
       
                                                                       
                                                                          
                                                         
       
    if (SSL_CTX_use_certificate_chain_file(SSL_context, fnbuf) != 1)
    {
      char *err = SSLerrmessage(ERR_get_error());

      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not read certificate file \"%s\": %s\n"), fnbuf, err);
      SSLerrfree(err);
      SSL_CTX_free(SSL_context);
      return -1;
    }

                                                      
    have_cert = true;
  }

     
                                                                    
                                                                            
                                                                            
                                                                           
                                
     
  if (!(conn->ssl = SSL_new(SSL_context)) || !SSL_set_app_data(conn->ssl, conn) || !my_SSL_set_fd(conn, conn->sock))
  {
    char *err = SSLerrmessage(ERR_get_error());

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not establish SSL connection: %s\n"), err);
    SSLerrfree(err);
    SSL_CTX_free(SSL_context);
    return -1;
  }
  conn->ssl_in_use = true;

     
                                                                           
                                                                             
                              
     
  SSL_CTX_free(SSL_context);
  SSL_context = NULL;

     
                                                                        
                                                                         
                                                                             
                                                                          
     
  if (have_cert && conn->sslkey && strlen(conn->sslkey) > 0)
  {
#ifdef USE_SSL_ENGINE
    if (strchr(conn->sslkey, ':')
#ifdef WIN32
        && conn->sslkey[1] != ':'
#endif
    )
    {
                                                                   
      char *engine_str = strdup(conn->sslkey);
      char *engine_colon;

      if (engine_str == NULL)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
        return -1;
      }

                                                                       
      engine_colon = strchr(engine_str, ':');

      *engine_colon = '\0';                                     
      engine_colon++;                                          

      conn->engine = ENGINE_by_id(engine_str);
      if (conn->engine == NULL)
      {
        char *err = SSLerrmessage(ERR_get_error());

        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not load SSL engine \"%s\": %s\n"), engine_str, err);
        SSLerrfree(err);
        free(engine_str);
        return -1;
      }

      if (ENGINE_init(conn->engine) == 0)
      {
        char *err = SSLerrmessage(ERR_get_error());

        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not initialize SSL engine \"%s\": %s\n"), engine_str, err);
        SSLerrfree(err);
        ENGINE_free(conn->engine);
        conn->engine = NULL;
        free(engine_str);
        return -1;
      }

      pkey = ENGINE_load_private_key(conn->engine, engine_colon, NULL, NULL);
      if (pkey == NULL)
      {
        char *err = SSLerrmessage(ERR_get_error());

        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not read private SSL key \"%s\" from engine \"%s\": %s\n"), engine_colon, engine_str, err);
        SSLerrfree(err);
        ENGINE_finish(conn->engine);
        ENGINE_free(conn->engine);
        conn->engine = NULL;
        free(engine_str);
        return -1;
      }
      if (SSL_use_PrivateKey(conn->ssl, pkey) != 1)
      {
        char *err = SSLerrmessage(ERR_get_error());

        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not load private SSL key \"%s\" from engine \"%s\": %s\n"), engine_colon, engine_str, err);
        SSLerrfree(err);
        ENGINE_finish(conn->engine);
        ENGINE_free(conn->engine);
        conn->engine = NULL;
        free(engine_str);
        return -1;
      }

      free(engine_str);

      fnbuf[0] = '\0';                                            
                                 
    }
    else
#endif                     
    {
                                                             
      strlcpy(fnbuf, conn->sslkey, sizeof(fnbuf));
    }
  }
  else if (have_homedir)
  {
                                                  
    snprintf(fnbuf, sizeof(fnbuf), "%s/%s", homedir, USER_KEY_FILE);
  }
  else
  {
    fnbuf[0] = '\0';
  }

  if (have_cert && fnbuf[0] != '\0')
  {
                                       

    if (stat(fnbuf, &buf) != 0)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("certificate present, but not private key file \"%s\"\n"), fnbuf);
      return -1;
    }

                                         
    if (!S_ISREG(buf.st_mode))
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("private key file \"%s\" is not a regular file\n"), fnbuf);
      return -1;
    }

       
                                                                      
                                                                       
                                                                     
                                                                          
                                                                           
                                                                   
                                                      
       
                                                                        
                                                                         
                                                 
       
                                                         
                                                                         
                                                                         
                                                          
       
                                                                         
                                                                         
                         
       
#if !defined(WIN32) && !defined(__CYGWIN__)
    if (buf.st_uid == 0 ? buf.st_mode & (S_IWGRP | S_IXGRP | S_IRWXO) : buf.st_mode & (S_IRWXG | S_IRWXO))
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("private key file \"%s\" has group or world access; file must have permissions u=rw (0600) or less if owned by the current user, or permissions u=rw,g=r (0640) or less if owned by root\n"), fnbuf);
      return -1;
    }
#endif

    if (SSL_use_PrivateKey_file(conn->ssl, fnbuf, SSL_FILETYPE_PEM) != 1)
    {
      char *err = SSLerrmessage(ERR_get_error());

      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not load private key file \"%s\": %s\n"), fnbuf, err);
      SSLerrfree(err);
      return -1;
    }
  }

                                                
  if (have_cert && SSL_check_private_key(conn->ssl) != 1)
  {
    char *err = SSLerrmessage(ERR_get_error());

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("certificate does not match private key file \"%s\": %s\n"), fnbuf, err);
    SSLerrfree(err);
    return -1;
  }

     
                                                                      
               
     
  if (have_rootcert)
  {
    SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, verify_cb);
  }

     
                                                                          
                
     
#ifdef SSL_OP_NO_COMPRESSION
  if (conn->sslcompression && conn->sslcompression[0] == '0')
  {
    SSL_set_options(conn->ssl, SSL_OP_NO_COMPRESSION);
  }

     
                                                            
                                                                   
                                                                           
                                                                 
     
#ifdef HAVE_SSL_CLEAR_OPTIONS
  else
  {
    SSL_clear_options(conn->ssl, SSL_OP_NO_COMPRESSION);
  }
#endif
#endif

  return 0;
}

   
                                        
   
static PostgresPollingStatusType
open_client_SSL(PGconn *conn)
{
  int r;

  ERR_clear_error();
  r = SSL_connect(conn->ssl);
  if (r <= 0)
  {
    int err = SSL_get_error(conn->ssl, r);
    unsigned long ecode;

    ecode = ERR_get_error();
    switch (err)
    {
    case SSL_ERROR_WANT_READ:
      return PGRES_POLLING_READING;

    case SSL_ERROR_WANT_WRITE:
      return PGRES_POLLING_WRITING;

    case SSL_ERROR_SYSCALL:
    {
      char sebuf[PG_STRERROR_R_BUFLEN];

      if (r == -1)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL SYSCALL error: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
      }
      else
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL SYSCALL error: EOF detected\n"));
      }
      pgtls_close(conn);
      return PGRES_POLLING_FAILED;
    }
    case SSL_ERROR_SSL:
    {
      char *err = SSLerrmessage(ecode);

      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL error: %s\n"), err);
      SSLerrfree(err);
      pgtls_close(conn);
      return PGRES_POLLING_FAILED;
    }

    default:
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unrecognized SSL error code: %d\n"), err);
      pgtls_close(conn);
      return PGRES_POLLING_FAILED;
    }
  }

     
                                                                         
                                               
     

                              
  conn->peer = SSL_get_peer_certificate(conn->ssl);
  if (conn->peer == NULL)
  {
    char *err;

    err = SSLerrmessage(ERR_get_error());

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("certificate could not be obtained: %s\n"), err);
    SSLerrfree(err);
    pgtls_close(conn);
    return PGRES_POLLING_FAILED;
  }

  if (!pq_verify_peer_name_matches_certificate(conn))
  {
    pgtls_close(conn);
    return PGRES_POLLING_FAILED;
  }

                                 
  return PGRES_POLLING_OK;
}

void
pgtls_close(PGconn *conn)
{
  bool destroy_needed = false;

  if (conn->ssl)
  {
       
                                                                        
                                                                 
                                                           
       
    destroy_needed = true;

    SSL_shutdown(conn->ssl);
    SSL_free(conn->ssl);
    conn->ssl = NULL;
    conn->ssl_in_use = false;
  }

  if (conn->peer)
  {
    X509_free(conn->peer);
    conn->peer = NULL;
  }

#ifdef USE_SSL_ENGINE
  if (conn->engine)
  {
    ENGINE_finish(conn->engine);
    ENGINE_free(conn->engine);
    conn->engine = NULL;
  }
#endif

     
                                                                     
                                                                         
                                                                             
                         
     
                                              
     
  if (destroy_needed)
  {
    destroy_ssl_system();
  }
}

   
                                               
   
                                                                  
   
                                                                  
                                                                 
                             
   
static char ssl_nomem[] = "out of memory allocating error description";

#define SSL_ERR_LEN 128

static char *
SSLerrmessage(unsigned long ecode)
{
  const char *errreason;
  char *errbuf;

  errbuf = malloc(SSL_ERR_LEN);
  if (!errbuf)
  {
    return ssl_nomem;
  }
  if (ecode == 0)
  {
    snprintf(errbuf, SSL_ERR_LEN, libpq_gettext("no SSL error reported"));
    return errbuf;
  }
  errreason = ERR_reason_error_string(ecode);
  if (errreason != NULL)
  {
    strlcpy(errbuf, errreason, SSL_ERR_LEN);
    return errbuf;
  }
  snprintf(errbuf, SSL_ERR_LEN, libpq_gettext("SSL error code %lu"), ecode);
  return errbuf;
}

static void
SSLerrfree(char *buf)
{
  if (buf != ssl_nomem)
  {
    free(buf);
  }
}

                                                                  
                                       
                                                                  

   
                                     
   
void *
PQgetssl(PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }
  return conn->ssl;
}

void *
PQsslStruct(PGconn *conn, const char *struct_name)
{
  if (!conn)
  {
    return NULL;
  }
  if (strcmp(struct_name, "OpenSSL") == 0)
  {
    return conn->ssl;
  }
  return NULL;
}

const char *const *
PQsslAttributeNames(PGconn *conn)
{
  static const char *const result[] = {"library", "key_bits", "cipher", "compression", "protocol", NULL};

  return result;
}

const char *
PQsslAttribute(PGconn *conn, const char *attribute_name)
{
  if (!conn)
  {
    return NULL;
  }
  if (conn->ssl == NULL)
  {
    return NULL;
  }

  if (strcmp(attribute_name, "library") == 0)
  {
    return "OpenSSL";
  }

  if (strcmp(attribute_name, "key_bits") == 0)
  {
    static char sslbits_str[12];
    int sslbits;

    SSL_get_cipher_bits(conn->ssl, &sslbits);
    snprintf(sslbits_str, sizeof(sslbits_str), "%d", sslbits);
    return sslbits_str;
  }

  if (strcmp(attribute_name, "cipher") == 0)
  {
    return SSL_get_cipher(conn->ssl);
  }

  if (strcmp(attribute_name, "compression") == 0)
  {
    return SSL_get_current_compression(conn->ssl) ? "on" : "off";
  }

  if (strcmp(attribute_name, "protocol") == 0)
  {
    return SSL_get_version(conn->ssl);
  }

  return NULL;                        
}

   
                                                                     
                                                                        
                                                                              
   
                                                                               
                                                                        
                                                                              
                                                       
   

#ifndef HAVE_BIO_GET_DATA
#define BIO_get_data(bio) (bio->ptr)
#define BIO_set_data(bio, data) (bio->ptr = data)
#endif

static BIO_METHOD *my_bio_methods;

static int
my_sock_read(BIO *h, char *buf, int size)
{
  int res;

  res = pqsecure_raw_read((PGconn *)BIO_get_data(h), buf, size);
  BIO_clear_retry_flags(h);
  if (res < 0)
  {
                                                      
    switch (SOCK_ERRNO)
    {
#ifdef EAGAIN
    case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK:
#endif
    case EINTR:
      BIO_set_retry_read(h);
      break;

    default:
      break;
    }
  }

  return res;
}

static int
my_sock_write(BIO *h, const char *buf, int size)
{
  int res;

  res = pqsecure_raw_write((PGconn *)BIO_get_data(h), buf, size);
  BIO_clear_retry_flags(h);
  if (res < 0)
  {
                                                      
    switch (SOCK_ERRNO)
    {
#ifdef EAGAIN
    case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK:
#endif
    case EINTR:
      BIO_set_retry_write(h);
      break;

    default:
      break;
    }
  }

  return res;
}

static BIO_METHOD *
my_BIO_s_socket(void)
{
  if (!my_bio_methods)
  {
    BIO_METHOD *biom = (BIO_METHOD *)BIO_s_socket();
#ifdef HAVE_BIO_METH_NEW
    int my_bio_index;

    my_bio_index = BIO_get_new_index();
    if (my_bio_index == -1)
    {
      return NULL;
    }
    my_bio_index |= (BIO_TYPE_DESCRIPTOR | BIO_TYPE_SOURCE_SINK);
    my_bio_methods = BIO_meth_new(my_bio_index, "libpq socket");
    if (!my_bio_methods)
    {
      return NULL;
    }

       
                                                                         
                                       
       
    if (!BIO_meth_set_write(my_bio_methods, my_sock_write) || !BIO_meth_set_read(my_bio_methods, my_sock_read) || !BIO_meth_set_gets(my_bio_methods, BIO_meth_get_gets(biom)) || !BIO_meth_set_puts(my_bio_methods, BIO_meth_get_puts(biom)) || !BIO_meth_set_ctrl(my_bio_methods, BIO_meth_get_ctrl(biom)) || !BIO_meth_set_create(my_bio_methods, BIO_meth_get_create(biom)) || !BIO_meth_set_destroy(my_bio_methods, BIO_meth_get_destroy(biom)) || !BIO_meth_set_callback_ctrl(my_bio_methods, BIO_meth_get_callback_ctrl(biom)))
    {
      BIO_meth_free(my_bio_methods);
      my_bio_methods = NULL;
      return NULL;
    }
#else
    my_bio_methods = malloc(sizeof(BIO_METHOD));
    if (!my_bio_methods)
    {
      return NULL;
    }
    memcpy(my_bio_methods, biom, sizeof(BIO_METHOD));
    my_bio_methods->bread = my_sock_read;
    my_bio_methods->bwrite = my_sock_write;
#endif
  }
  return my_bio_methods;
}

                                                                            
static int
my_SSL_set_fd(PGconn *conn, int fd)
{
  int ret = 0;
  BIO *bio;
  BIO_METHOD *bio_method;

  bio_method = my_BIO_s_socket();
  if (bio_method == NULL)
  {
    SSLerr(SSL_F_SSL_SET_FD, ERR_R_BUF_LIB);
    goto err;
  }
  bio = BIO_new(bio_method);
  if (bio == NULL)
  {
    SSLerr(SSL_F_SSL_SET_FD, ERR_R_BUF_LIB);
    goto err;
  }
  BIO_set_data(bio, conn);

  SSL_set_bio(conn->ssl, bio, bio);
  BIO_set_fd(bio, fd, BIO_NOCLOSE);
  ret = 1;
err:
  return ret;
}
