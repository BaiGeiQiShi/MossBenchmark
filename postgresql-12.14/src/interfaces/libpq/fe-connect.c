                                                                            
   
                
                                                                 
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#include "postgres_fe.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include "libpq-fe.h"
#include "libpq-int.h"
#include "fe-auth.h"
#include "pg_config_paths.h"

#ifdef WIN32
#include "win32.h"
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0500
#ifdef near
#undef near
#endif
#define near
#include <shlobj.h>
#ifdef _MSC_VER                                    
#include <mstcpip.h>
#endif
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#endif

#ifdef ENABLE_THREAD_SAFETY
#ifdef WIN32
#include "pthread-win32.h"
#else
#include <pthread.h>
#endif
#endif

#ifdef USE_LDAP
#ifdef WIN32
#include <winldap.h>
#else
                                                                    
#define LDAP_DEPRECATED 1
#include <ldap.h>
typedef struct timeval LDAP_TIMEVAL;
#endif
static int
ldapServiceLookup(const char *purl, PQconninfoOption *options, PQExpBuffer errorMessage);
#endif

#include "common/ip.h"
#include "common/link-canary.h"
#include "common/scram-common.h"
#include "mb/pg_wchar.h"
#include "port/pg_bswap.h"

#ifndef WIN32
#define PGPASSFILE ".pgpass"
#else
#define PGPASSFILE "pgpass.conf"
#endif

   
                                                             
                                                                        
                                                                      
                                         
   
#define ERRCODE_APPNAME_UNKNOWN "42704"

                                                    
#define ERRCODE_INVALID_PASSWORD "28P01"
              
#define ERRCODE_CANNOT_CONNECT_NOW "57P03"

   
                                                                              
                                                                            
   
#if defined(TCP_KEEPIDLE)
                                                               
#define PG_TCP_KEEPALIVE_IDLE TCP_KEEPIDLE
#define PG_TCP_KEEPALIVE_IDLE_STR "TCP_KEEPIDLE"
#elif defined(TCP_KEEPALIVE_THRESHOLD)
                                                                         
#define PG_TCP_KEEPALIVE_IDLE TCP_KEEPALIVE_THRESHOLD
#define PG_TCP_KEEPALIVE_IDLE_STR "TCP_KEEPALIVE_THRESHOLD"
#elif defined(TCP_KEEPALIVE) && defined(__darwin__)
                                                       
                                                                       
#define PG_TCP_KEEPALIVE_IDLE TCP_KEEPALIVE
#define PG_TCP_KEEPALIVE_IDLE_STR "TCP_KEEPALIVE"
#endif

   
                                                                       
                            
   
#define DefaultHost "localhost"
#define DefaultTty ""
#define DefaultOption ""
#define DefaultAuthtype ""
#define DefaultTargetSessionAttrs "any"
#ifdef USE_SSL
#define DefaultSSLMode "prefer"
#else
#define DefaultSSLMode "disable"
#endif
#ifdef ENABLE_GSS
#include "fe-gssapi-common.h"
#define DefaultGSSMode "prefer"
#else
#define DefaultGSSMode "disable"
#endif

              
                                                                       
   
                                                                
                                                                  
                                        
   
                                                                             
                                                                          
                           
   
                                                                      
                                                                        
                                            
                           
                                     
                                              
   
                                                                            
                                                                  
                                                                          
                                                                          
                                        
   
                                                                        
                                                             
              
   
typedef struct _internalPQconninfoOption
{
  char *keyword;                                   
  char *envvar;                                           
  char *compiled;                                         
  char *val;                                             
  char *label;                                           
  char *dispchar;                                             
                                                            
                                                              
                                                               
                                  
  int dispsize;                                            
         
                                                           
                                                            
                   
         
     
  off_t connofs;                                                 
} internalPQconninfoOption;

static const internalPQconninfoOption PQconninfoOptions[] = {
       
                                                                             
                                                                               
                            
       
    {"authtype", "PGAUTHTYPE", DefaultAuthtype, NULL, "Database-Authtype", "D", 20, -1},

    {"service", "PGSERVICE", NULL, NULL, "Database-Service", "", 20, -1},

    {"user", "PGUSER", NULL, NULL, "Database-User", "", 20, offsetof(struct pg_conn, pguser)},

    {"password", "PGPASSWORD", NULL, NULL, "Database-Password", "*", 20, offsetof(struct pg_conn, pgpass)},

    {"passfile", "PGPASSFILE", NULL, NULL, "Database-Password-File", "", 64, offsetof(struct pg_conn, pgpassfile)},

    {"connect_timeout", "PGCONNECT_TIMEOUT", NULL, NULL, "Connect-timeout", "", 10,                              
        offsetof(struct pg_conn, connect_timeout)},

    {"dbname", "PGDATABASE", NULL, NULL, "Database-Name", "", 20, offsetof(struct pg_conn, dbName)},

    {"host", "PGHOST", NULL, NULL, "Database-Host", "", 40, offsetof(struct pg_conn, pghost)},

    {"hostaddr", "PGHOSTADDR", NULL, NULL, "Database-Host-IP-Address", "", 45, offsetof(struct pg_conn, pghostaddr)},

    {"port", "PGPORT", DEF_PGPORT_STR, NULL, "Database-Port", "", 6, offsetof(struct pg_conn, pgport)},

    {"client_encoding", "PGCLIENTENCODING", NULL, NULL, "Client-Encoding", "", 10, offsetof(struct pg_conn, client_encoding_initial)},

       
                                                                         
                      
       
    {"tty", "PGTTY", DefaultTty, NULL, "Backend-Debug-TTY", "D", 40, offsetof(struct pg_conn, pgtty)},

    {"options", "PGOPTIONS", DefaultOption, NULL, "Backend-Options", "", 40, offsetof(struct pg_conn, pgoptions)},

    {"application_name", "PGAPPNAME", NULL, NULL, "Application-Name", "", 64, offsetof(struct pg_conn, appname)},

    {"fallback_application_name", NULL, NULL, NULL, "Fallback-Application-Name", "", 64, offsetof(struct pg_conn, fbappname)},

    {"keepalives", NULL, NULL, NULL, "TCP-Keepalives", "", 1,                                
        offsetof(struct pg_conn, keepalives)},

    {"keepalives_idle", NULL, NULL, NULL, "TCP-Keepalives-Idle", "", 10,                              
        offsetof(struct pg_conn, keepalives_idle)},

    {"keepalives_interval", NULL, NULL, NULL, "TCP-Keepalives-Interval", "", 10,                              
        offsetof(struct pg_conn, keepalives_interval)},

    {"keepalives_count", NULL, NULL, NULL, "TCP-Keepalives-Count", "", 10,                              
        offsetof(struct pg_conn, keepalives_count)},

    {"tcp_user_timeout", NULL, NULL, NULL, "TCP-User-Timeout", "", 10,                              
        offsetof(struct pg_conn, pgtcp_user_timeout)},

       
                                                                           
                                                                      
                                                                               
                                                         
       
    {"sslmode", "PGSSLMODE", DefaultSSLMode, NULL, "SSL-Mode", "", 12,                                  
        offsetof(struct pg_conn, sslmode)},

    {"sslcompression", "PGSSLCOMPRESSION", "0", NULL, "SSL-Compression", "", 1, offsetof(struct pg_conn, sslcompression)},

    {"sslcert", "PGSSLCERT", NULL, NULL, "SSL-Client-Cert", "", 64, offsetof(struct pg_conn, sslcert)},

    {"sslkey", "PGSSLKEY", NULL, NULL, "SSL-Client-Key", "", 64, offsetof(struct pg_conn, sslkey)},

    {"sslrootcert", "PGSSLROOTCERT", NULL, NULL, "SSL-Root-Certificate", "", 64, offsetof(struct pg_conn, sslrootcert)},

    {"sslcrl", "PGSSLCRL", NULL, NULL, "SSL-Revocation-List", "", 64, offsetof(struct pg_conn, sslcrl)},

    {"requirepeer", "PGREQUIREPEER", NULL, NULL, "Require-Peer", "", 10, offsetof(struct pg_conn, requirepeer)},

       
                                                                               
                
       
    {"gssencmode", "PGGSSENCMODE", DefaultGSSMode, NULL, "GSSENC-Mode", "", 8,                             
        offsetof(struct pg_conn, gssencmode)},

                                                                                
    {"krbsrvname", "PGKRBSRVNAME", PG_KRB_SRVNAM, NULL, "Kerberos-service-name", "", 20, offsetof(struct pg_conn, krbsrvname)},

    {"gsslib", "PGGSSLIB", NULL, NULL, "GSS-library", "", 7,                            
        offsetof(struct pg_conn, gsslib)},

    {"replication", NULL, NULL, NULL, "Replication", "D", 5, offsetof(struct pg_conn, replication)},

    {"target_session_attrs", "PGTARGETSESSIONATTRS", DefaultTargetSessionAttrs, NULL, "Target-Session-Attrs", "", 11,                                
        offsetof(struct pg_conn, target_session_attrs)},

                                            
    {NULL, NULL, NULL, NULL, NULL, NULL, 0}};

static const PQEnvironmentOption EnvironmentOptions[] = {
                                        
    {"PGDATESTYLE", "datestyle"}, {"PGTZ", "timezone"},
                                               
    {"PGGEQO", "geqo"}, {NULL, NULL}};

                                                                             
static const char uri_designator[] = "postgresql://";
static const char short_uri_designator[] = "postgres://";

static bool
connectOptions1(PGconn *conn, const char *conninfo);
static bool
connectOptions2(PGconn *conn);
static int
connectDBStart(PGconn *conn);
static int
connectDBComplete(PGconn *conn);
static PGPing
internal_ping(PGconn *conn);
static PGconn *
makeEmptyPGconn(void);
static bool
fillPGconn(PGconn *conn, PQconninfoOption *connOptions);
static void
freePGconn(PGconn *conn);
static void
closePGconn(PGconn *conn);
static void
release_conn_addrinfo(PGconn *conn);
static void
sendTerminateConn(PGconn *conn);
static PQconninfoOption *
conninfo_init(PQExpBuffer errorMessage);
static PQconninfoOption *
parse_connection_string(const char *conninfo, PQExpBuffer errorMessage, bool use_defaults);
static int
uri_prefix_length(const char *connstr);
static bool
recognized_connection_string(const char *connstr);
static PQconninfoOption *
conninfo_parse(const char *conninfo, PQExpBuffer errorMessage, bool use_defaults);
static PQconninfoOption *
conninfo_array_parse(const char *const *keywords, const char *const *values, PQExpBuffer errorMessage, bool use_defaults, int expand_dbname);
static bool
conninfo_add_defaults(PQconninfoOption *options, PQExpBuffer errorMessage);
static PQconninfoOption *
conninfo_uri_parse(const char *uri, PQExpBuffer errorMessage, bool use_defaults);
static bool
conninfo_uri_parse_options(PQconninfoOption *options, const char *uri, PQExpBuffer errorMessage);
static bool
conninfo_uri_parse_params(char *params, PQconninfoOption *connOptions, PQExpBuffer errorMessage);
static char *
conninfo_uri_decode(const char *str, PQExpBuffer errorMessage);
static bool
get_hexdigit(char digit, int *value);
static const char *
conninfo_getval(PQconninfoOption *connOptions, const char *keyword);
static PQconninfoOption *
conninfo_storeval(PQconninfoOption *connOptions, const char *keyword, const char *value, PQExpBuffer errorMessage, bool ignoreMissing, bool uri_decode);
static PQconninfoOption *
conninfo_find(PQconninfoOption *connOptions, const char *keyword);
static void
defaultNoticeReceiver(void *arg, const PGresult *res);
static void
defaultNoticeProcessor(void *arg, const char *message);
static int
parseServiceInfo(PQconninfoOption *options, PQExpBuffer errorMessage);
static int
parseServiceFile(const char *serviceFile, const char *service, PQconninfoOption *options, PQExpBuffer errorMessage, bool *group_found);
static char *
pwdfMatchesString(char *buf, const char *token);
static char *
passwordFromFile(const char *hostname, const char *port, const char *dbname, const char *username, const char *pgpassfile);
static void
pgpassfileWarning(PGconn *conn);
static void
default_threadlock(int acquire);

                                                          
pgthreadlock_t pg_g_threadlock = default_threadlock;

   
                     
   
                                                                     
                                                                    
                                                                          
                    
   
                                                                           
                                                                         
                                                                      
   
void
pqDropConnection(PGconn *conn, bool flushInput)
{
                          
  pqsecure_close(conn);

                               
  if (conn->sock != PGINVALID_SOCKET)
  {
    closesocket(conn->sock);
  }
  conn->sock = PGINVALID_SOCKET;

                                          
  if (flushInput)
  {
    conn->inStart = conn->inCursor = conn->inEnd = 0;
  }

                                      
  conn->outCount = 0;

                                            
#ifdef ENABLE_GSS
  {
    OM_uint32 min_s;

    if (conn->gcred != GSS_C_NO_CREDENTIAL)
    {
      gss_release_cred(&min_s, &conn->gcred);
      conn->gcred = GSS_C_NO_CREDENTIAL;
    }
    if (conn->gctx)
    {
      gss_delete_sec_context(&min_s, &conn->gctx, GSS_C_NO_BUFFER);
    }
    if (conn->gtarg_nam)
    {
      gss_release_name(&min_s, &conn->gtarg_nam);
    }
    if (conn->gss_SendBuffer)
    {
      free(conn->gss_SendBuffer);
      conn->gss_SendBuffer = NULL;
    }
    if (conn->gss_RecvBuffer)
    {
      free(conn->gss_RecvBuffer);
      conn->gss_RecvBuffer = NULL;
    }
    if (conn->gss_ResultBuffer)
    {
      free(conn->gss_ResultBuffer);
      conn->gss_ResultBuffer = NULL;
    }
    conn->gssenc = false;
  }
#endif
#ifdef ENABLE_SSPI
  if (conn->sspitarget)
  {
    free(conn->sspitarget);
    conn->sspitarget = NULL;
  }
  if (conn->sspicred)
  {
    FreeCredentialsHandle(conn->sspicred);
    free(conn->sspicred);
    conn->sspicred = NULL;
  }
  if (conn->sspictx)
  {
    DeleteSecurityContext(conn->sspictx);
    free(conn->sspictx);
    conn->sspictx = NULL;
  }
  conn->usesspi = 0;
#endif
  if (conn->sasl_state)
  {
       
                                                                         
                                                
       
    pg_fe_scram_free(conn->sasl_state);
    conn->sasl_state = NULL;
  }
}

   
                     
   
                                                                             
                                                                       
                                                                           
               
   
                                                                        
                                                                      
                                                                           
                                                                   
   
static void
pqDropServerData(PGconn *conn)
{
  PGnotify *notify;
  pgParameterStatus *pstatus;

                               
  notify = conn->notifyHead;
  while (notify != NULL)
  {
    PGnotify *prev = notify;

    notify = notify->next;
    free(prev);
  }
  conn->notifyHead = conn->notifyTail = NULL;

                                                                        
  pstatus = conn->pstatus;
  while (pstatus != NULL)
  {
    pgParameterStatus *prev = pstatus;

    pstatus = pstatus->next;
    free(prev);
  }
  conn->pstatus = NULL;
  conn->client_encoding = PG_SQL_ASCII;
  conn->std_strings = false;
  conn->sversion = 0;

                                     
  if (conn->lobjfuncs)
  {
    free(conn->lobjfuncs);
  }
  conn->lobjfuncs = NULL;

                                                 
  conn->last_sqlstate[0] = '\0';
  conn->auth_req_received = false;
  conn->password_needed = false;
  conn->write_failed = false;
  if (conn->write_err_msg)
  {
    free(conn->write_err_msg);
  }
  conn->write_err_msg = NULL;
  conn->be_pid = 0;
  conn->be_key = 0;
}

   
                             
   
                                                                          
                                                                            
                                                                       
                                                                              
                   
   
                                                                           
                                                                             
                                                                              
                                   
   
                                                                          
                                                                           
                                                         
   
                                                                      
                                         
   

   
                      
   
                                                                         
                                               
   
                                    
   
                                                          
   
                                  
   
                                                        
   
                                                                             
                                  
                                                                     
                                                                       
   
                                                                             
                   
   
PGconn *
PQconnectdbParams(const char *const *keywords, const char *const *values, int expand_dbname)
{
  PGconn *conn = PQconnectStartParams(keywords, values, expand_dbname);

  if (conn && conn->status != CONNECTION_BAD)
  {
    (void)connectDBComplete(conn);
  }

  return conn;
}

   
                 
   
                                                                            
   
PGPing
PQpingParams(const char *const *keywords, const char *const *values, int expand_dbname)
{
  PGconn *conn = PQconnectStartParams(keywords, values, expand_dbname);
  PGPing ret;

  ret = internal_ping(conn);
  PQfinish(conn);

  return ret;
}

   
                
   
                                                                         
                                             
   
                                                                
   
                     
   
                                                                        
                                                                        
                                                                             
                                    
   
                                                                             
                                  
                                                                     
                                                                       
   
                                                                             
                   
   
PGconn *
PQconnectdb(const char *conninfo)
{
  PGconn *conn = PQconnectStart(conninfo);

  if (conn && conn->status != CONNECTION_BAD)
  {
    (void)connectDBComplete(conn);
  }

  return conn;
}

   
           
   
                                                                      
   
PGPing
PQping(const char *conninfo)
{
  PGconn *conn = PQconnectStart(conninfo);
  PGPing ret;

  ret = internal_ping(conn);
  PQfinish(conn);

  return ret;
}

   
                         
   
                                                                              
                                                        
   
                                                                              
   
                                                                             
                                                                          
                                                                    
                                                                           
                                                                               
                                                                             
                                                                              
                      
   
                                    
   
PGconn *
PQconnectStartParams(const char *const *keywords, const char *const *values, int expand_dbname)
{
  PGconn *conn;
  PQconninfoOption *connOptions;

     
                                            
     
  conn = makeEmptyPGconn();
  if (conn == NULL)
  {
    return NULL;
  }

     
                               
     
  connOptions = conninfo_array_parse(keywords, values, &conn->errorMessage, true, expand_dbname);
  if (connOptions == NULL)
  {
    conn->status = CONNECTION_BAD;
                                     
    return conn;
  }

     
                                            
     
  if (!fillPGconn(conn, connOptions))
  {
    PQconninfoFree(connOptions);
    return conn;
  }

     
                                               
     
  PQconninfoFree(connOptions);

     
                             
     
  if (!connectOptions2(conn))
  {
    return conn;
  }

     
                             
     
  if (!connectDBStart(conn))
  {
                                                            
    conn->status = CONNECTION_BAD;
  }

  return conn;
}

   
                   
   
                                                                              
                                                        
   
                                                                        
   
                                                                             
                                                                          
                                                                    
                                                                           
                                                                               
                                                                             
                                                                              
                      
   
                                    
   
PGconn *
PQconnectStart(const char *conninfo)
{
  PGconn *conn;

     
                                            
     
  conn = makeEmptyPGconn();
  if (conn == NULL)
  {
    return NULL;
  }

     
                               
     
  if (!connectOptions1(conn, conninfo))
  {
    return conn;
  }

     
                             
     
  if (!connectOptions2(conn))
  {
    return conn;
  }

     
                             
     
  if (!connectDBStart(conn))
  {
                                                            
    conn->status = CONNECTION_BAD;
  }

  return conn;
}

   
                                          
   
                                                              
                       
   
                                                                              
   
static bool
fillPGconn(PGconn *conn, PQconninfoOption *connOptions)
{
  const internalPQconninfoOption *option;

  for (option = PQconninfoOptions; option->keyword; option++)
  {
    if (option->connofs >= 0)
    {
      const char *tmp = conninfo_getval(connOptions, option->keyword);

      if (tmp)
      {
        char **connmember = (char **)((char *)conn + option->connofs);

        if (*connmember)
        {
          free(*connmember);
        }
        *connmember = strdup(tmp);
        if (*connmember == NULL)
        {
          printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
          return false;
        }
      }
    }
  }

  return true;
}

   
                    
   
                                                                         
                                                                     
                                                                      
                                                
   
                                                                           
                            
   
static bool
connectOptions1(PGconn *conn, const char *conninfo)
{
  PQconninfoOption *connOptions;

     
                               
     
  connOptions = parse_connection_string(conninfo, &conn->errorMessage, true);
  if (connOptions == NULL)
  {
    conn->status = CONNECTION_BAD;
                                     
    return false;
  }

     
                                            
     
  if (!fillPGconn(conn, connOptions))
  {
    conn->status = CONNECTION_BAD;
    PQconninfoFree(connOptions);
    return false;
  }

     
                                               
     
  PQconninfoFree(connOptions);

  return true;
}

   
                                                                  
   
static int
count_comma_separated_elems(const char *input)
{
  int n;

  n = 1;
  for (; *input != '\0'; input++)
  {
    if (*input == ',')
    {
      n++;
    }
  }

  return n;
}

   
                                        
   
                                                                             
                                                                           
                                                               
   
                                   
   
static char *
parse_comma_separated_list(char **startptr, bool *more)
{
  char *p;
  char *s = *startptr;
  char *e;
  int len;

     
                                                                         
                           
     
  e = s;
  while (*e != '\0' && *e != ',')
  {
    ++e;
  }
  *more = (*e == ',');

  len = e - s;
  p = (char *)malloc(sizeof(char) * (len + 1));
  if (p)
  {
    memcpy(p, s, len);
    p[len] = '\0';
  }
  *startptr = e + 1;

  return p;
}

   
                    
   
                                                                              
   
                                                                           
                            
   
static bool
connectOptions2(PGconn *conn)
{
  int i;

     
                                                                            
                                                                             
                                                             
     
  conn->whichhost = 0;
  if (conn->pghostaddr && conn->pghostaddr[0] != '\0')
  {
    conn->nconnhost = count_comma_separated_elems(conn->pghostaddr);
  }
  else if (conn->pghost && conn->pghost[0] != '\0')
  {
    conn->nconnhost = count_comma_separated_elems(conn->pghost);
  }
  else
  {
    conn->nconnhost = 1;
  }
  conn->connhost = (pg_conn_host *)calloc(conn->nconnhost, sizeof(pg_conn_host));
  if (conn->connhost == NULL)
  {
    goto oom_error;
  }

     
                                                                            
                                                                            
     
  if (conn->pghostaddr != NULL && conn->pghostaddr[0] != '\0')
  {
    char *s = conn->pghostaddr;
    bool more = true;

    for (i = 0; i < conn->nconnhost && more; i++)
    {
      conn->connhost[i].hostaddr = parse_comma_separated_list(&s, &more);
      if (conn->connhost[i].hostaddr == NULL)
      {
        goto oom_error;
      }
    }

       
                                                                       
                                                                           
                   
       
    Assert(!more);
    Assert(i == conn->nconnhost);
  }

  if (conn->pghost != NULL && conn->pghost[0] != '\0')
  {
    char *s = conn->pghost;
    bool more = true;

    for (i = 0; i < conn->nconnhost && more; i++)
    {
      conn->connhost[i].host = parse_comma_separated_list(&s, &more);
      if (conn->connhost[i].host == NULL)
      {
        goto oom_error;
      }
    }

                                               
    if (more || i != conn->nconnhost)
    {
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not match %d host names to %d hostaddr values\n"), count_comma_separated_elems(conn->pghost), conn->nconnhost);
      return false;
    }
  }

     
                                                                             
                                               
     
  for (i = 0; i < conn->nconnhost; i++)
  {
    pg_conn_host *ch = &conn->connhost[i];

    if (ch->hostaddr != NULL && ch->hostaddr[0] != '\0')
    {
      ch->type = CHT_HOST_ADDRESS;
    }
    else if (ch->host != NULL && ch->host[0] != '\0')
    {
      ch->type = CHT_HOST_NAME;
#ifdef HAVE_UNIX_SOCKETS
      if (is_absolute_path(ch->host))
      {
        ch->type = CHT_UNIX_SOCKET;
      }
#endif
    }
    else
    {
      if (ch->host)
      {
        free(ch->host);
      }
#ifdef HAVE_UNIX_SOCKETS
      ch->host = strdup(DEFAULT_PGSOCKET_DIR);
      ch->type = CHT_UNIX_SOCKET;
#else
      ch->host = strdup(DefaultHost);
      ch->type = CHT_HOST_NAME;
#endif
      if (ch->host == NULL)
      {
        goto oom_error;
      }
    }
  }

     
                                                                     
     
                                                                             
                                                                          
                             
     
  if (conn->pgport != NULL && conn->pgport[0] != '\0')
  {
    char *s = conn->pgport;
    bool more = true;

    for (i = 0; i < conn->nconnhost && more; i++)
    {
      conn->connhost[i].port = parse_comma_separated_list(&s, &more);
      if (conn->connhost[i].port == NULL)
      {
        goto oom_error;
      }
    }

       
                                                                         
                                                                
       
    if (i == 1 && !more)
    {
      for (i = 1; i < conn->nconnhost; i++)
      {
        conn->connhost[i].port = strdup(conn->connhost[0].port);
        if (conn->connhost[i].port == NULL)
        {
          goto oom_error;
        }
      }
    }
    else if (more || i != conn->nconnhost)
    {
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not match %d port numbers to %d hosts\n"), count_comma_separated_elems(conn->pgport), conn->nconnhost);
      return false;
    }
  }

     
                                                                         
                                                                           
                                                                         
     
  if (conn->pguser == NULL || conn->pguser[0] == '\0')
  {
    if (conn->pguser)
    {
      free(conn->pguser);
    }
    conn->pguser = pg_fe_getauthname(&conn->errorMessage);
    if (!conn->pguser)
    {
      conn->status = CONNECTION_BAD;
      return false;
    }
  }

     
                                                                   
     
  if (conn->dbName == NULL || conn->dbName[0] == '\0')
  {
    if (conn->dbName)
    {
      free(conn->dbName);
    }
    conn->dbName = strdup(conn->pguser);
    if (!conn->dbName)
    {
      goto oom_error;
    }
  }

     
                                                                          
                                                                 
     
  if (conn->pgpass == NULL || conn->pgpass[0] == '\0')
  {
                                                             
    if (conn->pgpassfile == NULL || conn->pgpassfile[0] == '\0')
    {
      char homedir[MAXPGPATH];

      if (pqGetHomeDirectory(homedir, sizeof(homedir)))
      {
        if (conn->pgpassfile)
        {
          free(conn->pgpassfile);
        }
        conn->pgpassfile = malloc(MAXPGPATH);
        if (!conn->pgpassfile)
        {
          goto oom_error;
        }
        snprintf(conn->pgpassfile, MAXPGPATH, "%s/%s", homedir, PGPASSFILE);
      }
    }

    if (conn->pgpassfile != NULL && conn->pgpassfile[0] != '\0')
    {
      for (i = 0; i < conn->nconnhost; i++)
      {
           
                                                                       
                                                                   
                                                             
           
        const char *pwhost = conn->connhost[i].host;

        if (pwhost == NULL || pwhost[0] == '\0')
        {
          pwhost = conn->connhost[i].hostaddr;
        }

        conn->connhost[i].password = passwordFromFile(pwhost, conn->connhost[i].port, conn->dbName, conn->pguser, conn->pgpassfile);
      }
    }
  }

     
                             
     
  if (conn->sslmode)
  {
    if (strcmp(conn->sslmode, "disable") != 0 && strcmp(conn->sslmode, "allow") != 0 && strcmp(conn->sslmode, "prefer") != 0 && strcmp(conn->sslmode, "require") != 0 && strcmp(conn->sslmode, "verify-ca") != 0 && strcmp(conn->sslmode, "verify-full") != 0)
    {
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("invalid sslmode value: \"%s\"\n"), conn->sslmode);
      return false;
    }

#ifndef USE_SSL
    switch (conn->sslmode[0])
    {
    case 'a':              
    case 'p':               

         
                                                                   
                                        
         
      break;

    case 'r':                
    case 'v':                                   
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("sslmode value \"%s\" invalid when SSL support is not compiled in\n"), conn->sslmode);
      return false;
    }
#endif
  }
  else
  {
    conn->sslmode = strdup(DefaultSSLMode);
    if (!conn->sslmode)
    {
      goto oom_error;
    }
  }

     
                                
     
  if (conn->gssencmode)
  {
    if (strcmp(conn->gssencmode, "disable") != 0 && strcmp(conn->gssencmode, "prefer") != 0 && strcmp(conn->gssencmode, "require") != 0)
    {
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("invalid gssencmode value: \"%s\"\n"), conn->gssencmode);
      return false;
    }
#ifndef ENABLE_GSS
    if (strcmp(conn->gssencmode, "require") == 0)
    {
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("gssencmode value \"%s\" invalid when GSSAPI support is not compiled in\n"), conn->gssencmode);
      return false;
    }
#endif
  }
  else
  {
    conn->gssencmode = strdup(DefaultGSSMode);
    if (!conn->gssencmode)
    {
      goto oom_error;
    }
  }

     
                                                            
     
  if (conn->client_encoding_initial && strcmp(conn->client_encoding_initial, "auto") == 0)
  {
    free(conn->client_encoding_initial);
    conn->client_encoding_initial = strdup(pg_encoding_to_char(pg_get_encoding_from_locale(NULL, true)));
    if (!conn->client_encoding_initial)
    {
      goto oom_error;
    }
  }

     
                                           
     
  if (conn->target_session_attrs)
  {
    if (strcmp(conn->target_session_attrs, "any") != 0 && strcmp(conn->target_session_attrs, "read-write") != 0)
    {
      conn->status = CONNECTION_BAD;
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("invalid target_session_attrs value: \"%s\"\n"), conn->target_session_attrs);
      return false;
    }
  }

     
                                                                             
                                                                          
                                                  
     
  conn->options_valid = true;

  return true;

oom_error:
  conn->status = CONNECTION_BAD;
  printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
  return false;
}

   
                   
   
                                                                          
                                                                              
                                                                    
   
                                                                          
                                     
   
                                                                           
                                                                              
                                                                         
                                                                            
                                                  
   
PQconninfoOption *
PQconndefaults(void)
{
  PQExpBufferData errorBuf;
  PQconninfoOption *connOptions;

                                                                           
  initPQExpBuffer(&errorBuf);
  if (PQExpBufferDataBroken(errorBuf))
  {
    return NULL;                                
  }

  connOptions = conninfo_init(&errorBuf);
  if (connOptions != NULL)
  {
                                             
    if (!conninfo_add_defaults(connOptions, NULL))
    {
      PQconninfoFree(connOptions);
      connOptions = NULL;
    }
  }

  termPQExpBuffer(&errorBuf);
  return connOptions;
}

                    
                 
   
                                                                         
                                   
   
                                                                    
   
                                                                     
                                                      
                    
   
PGconn *
PQsetdbLogin(const char *pghost, const char *pgport, const char *pgoptions, const char *pgtty, const char *dbName, const char *login, const char *pwd)
{
  PGconn *conn;

     
                                            
     
  conn = makeEmptyPGconn();
  if (conn == NULL)
  {
    return NULL;
  }

     
                                                                           
                                                      
     
  if (dbName && recognized_connection_string(dbName))
  {
    if (!connectOptions1(conn, dbName))
    {
      return conn;
    }
  }
  else
  {
       
                                                                         
                                                              
       
    if (!connectOptions1(conn, ""))
    {
      return conn;
    }

                                                   
    if (dbName && dbName[0] != '\0')
    {
      if (conn->dbName)
      {
        free(conn->dbName);
      }
      conn->dbName = strdup(dbName);
      if (!conn->dbName)
      {
        goto oom_error;
      }
    }
  }

     
                                                                           
                                                               
     
  if (pghost && pghost[0] != '\0')
  {
    if (conn->pghost)
    {
      free(conn->pghost);
    }
    conn->pghost = strdup(pghost);
    if (!conn->pghost)
    {
      goto oom_error;
    }
  }

  if (pgport && pgport[0] != '\0')
  {
    if (conn->pgport)
    {
      free(conn->pgport);
    }
    conn->pgport = strdup(pgport);
    if (!conn->pgport)
    {
      goto oom_error;
    }
  }

  if (pgoptions && pgoptions[0] != '\0')
  {
    if (conn->pgoptions)
    {
      free(conn->pgoptions);
    }
    conn->pgoptions = strdup(pgoptions);
    if (!conn->pgoptions)
    {
      goto oom_error;
    }
  }

  if (pgtty && pgtty[0] != '\0')
  {
    if (conn->pgtty)
    {
      free(conn->pgtty);
    }
    conn->pgtty = strdup(pgtty);
    if (!conn->pgtty)
    {
      goto oom_error;
    }
  }

  if (login && login[0] != '\0')
  {
    if (conn->pguser)
    {
      free(conn->pguser);
    }
    conn->pguser = strdup(login);
    if (!conn->pguser)
    {
      goto oom_error;
    }
  }

  if (pwd && pwd[0] != '\0')
  {
    if (conn->pgpass)
    {
      free(conn->pgpass);
    }
    conn->pgpass = strdup(pwd);
    if (!conn->pgpass)
    {
      goto oom_error;
    }
  }

     
                             
     
  if (!connectOptions2(conn))
  {
    return conn;
  }

     
                             
     
  if (connectDBStart(conn))
  {
    (void)connectDBComplete(conn);
  }

  return conn;

oom_error:
  conn->status = CONNECTION_BAD;
  printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
  return conn;
}

              
                    
                                       
                                      
              
   
static int
connectNoDelay(PGconn *conn)
{
#ifdef TCP_NODELAY
  int on = 1;

  if (setsockopt(conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0)
  {
    char sebuf[PG_STRERROR_R_BUFLEN];

    appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not set socket to TCP no delay mode: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
    return 0;
  }
#endif

  return 1;
}

              
                                                                               
                                             
              
   
static void
getHostaddr(PGconn *conn, char *host_addr, int host_addr_len)
{
  struct sockaddr_storage *addr = &conn->raddr.addr;

  if (addr->ss_family == AF_INET)
  {
    if (inet_net_ntop(AF_INET, &((struct sockaddr_in *)addr)->sin_addr.s_addr, 32, host_addr, host_addr_len) == NULL)
    {
      host_addr[0] = '\0';
    }
  }
#ifdef HAVE_IPV6
  else if (addr->ss_family == AF_INET6)
  {
    if (inet_net_ntop(AF_INET6, &((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, 128, host_addr, host_addr_len) == NULL)
    {
      host_addr[0] = '\0';
    }
  }
#endif
  else
  {
    host_addr[0] = '\0';
  }
}

              
                           
                                                          
              
   
static void
connectFailureMessage(PGconn *conn, int errorno)
{
  char sebuf[PG_STRERROR_R_BUFLEN];

#ifdef HAVE_UNIX_SOCKETS
  if (IS_AF_UNIX(conn->raddr.addr.ss_family))
  {
    char service[NI_MAXHOST];

    pg_getnameinfo_all(&conn->raddr.addr, conn->raddr.salen, NULL, 0, service, sizeof(service), NI_NUMERICSERV);
    appendPQExpBuffer(&conn->errorMessage,
        libpq_gettext("could not connect to server: %s\n"
                      "\tIs the server running locally and accepting\n"
                      "\tconnections on Unix domain socket \"%s\"?\n"),
        SOCK_STRERROR(errorno, sebuf, sizeof(sebuf)), service);
  }
  else
#endif                        
  {
    char host_addr[NI_MAXHOST];
    const char *displayed_host;
    const char *displayed_port;

       
                                                                         
                                                                
       
    getHostaddr(conn, host_addr, NI_MAXHOST);

                                                             
    if (conn->connhost[conn->whichhost].type == CHT_HOST_ADDRESS)
    {
      displayed_host = conn->connhost[conn->whichhost].hostaddr;
    }
    else
    {
      displayed_host = conn->connhost[conn->whichhost].host;
    }
    displayed_port = conn->connhost[conn->whichhost].port;
    if (displayed_port == NULL || displayed_port[0] == '\0')
    {
      displayed_port = DEF_PGPORT_STR;
    }

       
                                                                      
                                                                    
                             
       
    if (conn->connhost[conn->whichhost].type != CHT_HOST_ADDRESS && strlen(host_addr) > 0 && strcmp(displayed_host, host_addr) != 0)
    {
      appendPQExpBuffer(&conn->errorMessage,
          libpq_gettext("could not connect to server: %s\n"
                        "\tIs the server running on host \"%s\" (%s) and accepting\n"
                        "\tTCP/IP connections on port %s?\n"),
          SOCK_STRERROR(errorno, sebuf, sizeof(sebuf)), displayed_host, host_addr, displayed_port);
    }
    else
    {
      appendPQExpBuffer(&conn->errorMessage,
          libpq_gettext("could not connect to server: %s\n"
                        "\tIs the server running on host \"%s\" and accepting\n"
                        "\tTCP/IP connections on port %s?\n"),
          SOCK_STRERROR(errorno, sebuf, sizeof(sebuf)), displayed_host, displayed_port);
    }
  }
}

   
                                                                   
                                                                   
            
   
static int
useKeepalives(PGconn *conn)
{
  char *ep;
  int val;

  if (conn->keepalives == NULL)
  {
    return 1;
  }
  val = strtol(conn->keepalives, &ep, 10);
  if (*ep)
  {
    return -1;
  }
  return val != 0 ? 1 : 0;
}

   
                                                                              
                                                                           
                                                                          
   
static bool
parse_int_param(const char *value, int *result, PGconn *conn, const char *context)
{
  char *end;
  long numval;

  Assert(value != NULL);

  *result = 0;

                                           
  errno = 0;
  numval = strtol(value, &end, 10);

     
                                                                            
                                                      
     
  if (value == end || errno != 0 || numval != (int)numval)
  {
    goto error;
  }

     
                                                                             
                                     
     
  while (*end != '\0' && isspace((unsigned char)*end))
  {
    end++;
  }

  if (*end != '\0')
  {
    goto error;
  }

  *result = numval;
  return true;

error:
  appendPQExpBuffer(&conn->errorMessage, libpq_gettext("invalid integer value \"%s\" for connection option \"%s\"\n"), value, context);
  return false;
}

#ifndef WIN32
   
                                 
   
static int
setKeepalivesIdle(PGconn *conn)
{
  int idle;

  if (conn->keepalives_idle == NULL)
  {
    return 1;
  }

  if (!parse_int_param(conn->keepalives_idle, &idle, conn, "keepalives_idle"))
  {
    return 0;
  }
  if (idle < 0)
  {
    idle = 0;
  }

#ifdef PG_TCP_KEEPALIVE_IDLE
  if (setsockopt(conn->sock, IPPROTO_TCP, PG_TCP_KEEPALIVE_IDLE, (char *)&idle, sizeof(idle)) < 0)
  {
    char sebuf[PG_STRERROR_R_BUFLEN];

    appendPQExpBuffer(&conn->errorMessage, libpq_gettext("setsockopt(%s) failed: %s\n"), PG_TCP_KEEPALIVE_IDLE_STR, SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
    return 0;
  }
#endif

  return 1;
}

   
                               
   
static int
setKeepalivesInterval(PGconn *conn)
{
  int interval;

  if (conn->keepalives_interval == NULL)
  {
    return 1;
  }

  if (!parse_int_param(conn->keepalives_interval, &interval, conn, "keepalives_interval"))
  {
    return 0;
  }
  if (interval < 0)
  {
    interval = 0;
  }

#ifdef TCP_KEEPINTVL
  if (setsockopt(conn->sock, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&interval, sizeof(interval)) < 0)
  {
    char sebuf[PG_STRERROR_R_BUFLEN];

    appendPQExpBuffer(&conn->errorMessage, libpq_gettext("setsockopt(%s) failed: %s\n"), "TCP_KEEPINTVL", SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
    return 0;
  }
#endif

  return 1;
}

   
                                                                          
          
   
static int
setKeepalivesCount(PGconn *conn)
{
  int count;

  if (conn->keepalives_count == NULL)
  {
    return 1;
  }

  if (!parse_int_param(conn->keepalives_count, &count, conn, "keepalives_count"))
  {
    return 0;
  }
  if (count < 0)
  {
    count = 0;
  }

#ifdef TCP_KEEPCNT
  if (setsockopt(conn->sock, IPPROTO_TCP, TCP_KEEPCNT, (char *)&count, sizeof(count)) < 0)
  {
    char sebuf[PG_STRERROR_R_BUFLEN];

    appendPQExpBuffer(&conn->errorMessage, libpq_gettext("setsockopt(%s) failed: %s\n"), "TCP_KEEPCNT", SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
    return 0;
  }
#endif

  return 1;
}
#else            
#ifdef SIO_KEEPALIVE_VALS
   
                                                            
                                           
   
static int
setKeepalivesWin32(PGconn *conn)
{
  struct tcp_keepalive ka;
  DWORD retsize;
  int idle = 0;
  int interval = 0;

  if (conn->keepalives_idle && !parse_int_param(conn->keepalives_idle, &idle, conn, "keepalives_idle"))
  {
    return 0;
  }
  if (idle <= 0)
  {
    idle = 2 * 60 * 60;                        
  }

  if (conn->keepalives_interval && !parse_int_param(conn->keepalives_interval, &interval, conn, "keepalives_interval"))
  {
    return 0;
  }
  if (interval <= 0)
  {
    interval = 1;                         
  }

  ka.onoff = 1;
  ka.keepalivetime = idle * 1000;
  ka.keepaliveinterval = interval * 1000;

  if (WSAIoctl(conn->sock, SIO_KEEPALIVE_VALS, (LPVOID)&ka, sizeof(ka), NULL, 0, &retsize, NULL, NULL) != 0)
  {
    appendPQExpBuffer(&conn->errorMessage, libpq_gettext("WSAIoctl(SIO_KEEPALIVE_VALS) failed: %ui\n"), WSAGetLastError());
    return 0;
  }
  return 1;
}
#endif                         
#endif            

   
                             
   
static int
setTCPUserTimeout(PGconn *conn)
{
  int timeout;

  if (conn->pgtcp_user_timeout == NULL)
  {
    return 1;
  }

  if (!parse_int_param(conn->pgtcp_user_timeout, &timeout, conn, "tcp_user_timeout"))
  {
    return 0;
  }

  if (timeout < 0)
  {
    timeout = 0;
  }

#ifdef TCP_USER_TIMEOUT
  if (setsockopt(conn->sock, IPPROTO_TCP, TCP_USER_TIMEOUT, (char *)&timeout, sizeof(timeout)) < 0)
  {
    char sebuf[256];

    appendPQExpBuffer(&conn->errorMessage, libpq_gettext("setsockopt(%s) failed: %s\n"), "TCP_USER_TIMEOUT", SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
    return 0;
  }
#endif

  return 1;
}

              
                    
                                                             
   
                                      
              
   
static int
connectDBStart(PGconn *conn)
{
  if (!conn)
  {
    return 0;
  }

  if (!conn->options_valid)
  {
    goto connect_errReturn;
  }

     
                                                                      
                                                                            
                                                                       
                     
     
  if (!pg_link_canary_is_frontend())
  {
    printfPQExpBuffer(&conn->errorMessage, "libpq is incorrectly linked to backend functions\n");
    goto connect_errReturn;
  }

                                    
  conn->inStart = conn->inCursor = conn->inEnd = 0;
  conn->outCount = 0;

     
                                                                            
                                                                         
                                                                             
                                 
     
  resetPQExpBuffer(&conn->errorMessage);

     
                                                                             
                                                                     
                                 
     
  conn->whichhost = -1;
  conn->try_next_addr = false;
  conn->try_next_host = true;
  conn->status = CONNECTION_NEEDED;

     
                                                                            
                                                                     
                                                                       
                                                                            
                                                       
     
  if (PQconnectPoll(conn) == PGRES_POLLING_WRITING)
  {
    return 1;
  }

connect_errReturn:

     
                                                                      
                                                                            
                                                                 
     
  pqDropConnection(conn, true);
  conn->status = CONNECTION_BAD;
  return 0;
}

   
                      
   
                                    
   
                                       
   
static int
connectDBComplete(PGconn *conn)
{
  PostgresPollingStatusType flag = PGRES_POLLING_WRITING;
  time_t finish_time = ((time_t)-1);
  int timeout = 0;
  int last_whichhost = -2;                                         
  struct addrinfo *last_addr_cur = NULL;

  if (conn == NULL || conn->status == CONNECTION_BAD)
  {
    return 0;
  }

     
                                                         
     
  if (conn->connect_timeout != NULL)
  {
    if (!parse_int_param(conn->connect_timeout, &timeout, conn, "connect_timeout"))
    {
                                                                    
      conn->status = CONNECTION_BAD;
      return 0;
    }

    if (timeout > 0)
    {
         
                                                                       
                                                                       
                      
         
      if (timeout < 2)
      {
        timeout = 2;
      }
    }
    else                       
    {
      timeout = 0;
    }
  }

  for (;;)
  {
    int ret = 0;

       
                                                                     
                                                                        
                                                             
       
    if (flag != PGRES_POLLING_OK && timeout > 0 && (conn->whichhost != last_whichhost || conn->addr_cur != last_addr_cur))
    {
      finish_time = time(NULL) + timeout;
      last_whichhost = conn->whichhost;
      last_addr_cur = conn->addr_cur;
    }

       
                                                                    
                                                                        
       
    switch (flag)
    {
    case PGRES_POLLING_OK:

         
                                                                 
                    
         
      resetPQExpBuffer(&conn->errorMessage);
      return 1;               

    case PGRES_POLLING_READING:
      ret = pqWaitTimed(1, 0, conn, finish_time);
      if (ret == -1)
      {
                                                                  
        conn->status = CONNECTION_BAD;
        return 0;
      }
      break;

    case PGRES_POLLING_WRITING:
      ret = pqWaitTimed(0, 1, conn, finish_time);
      if (ret == -1)
      {
                                                                  
        conn->status = CONNECTION_BAD;
        return 0;
      }
      break;

    default:
                                                             
      conn->status = CONNECTION_BAD;
      return 0;
    }

    if (ret == 1)                              
    {
         
                                                              
         
      conn->try_next_addr = true;
      conn->status = CONNECTION_NEEDED;
    }

       
                                             
       
    flag = PQconnectPoll(conn);
  }
}

   
                                                                            
                                                                  
   
static bool
saveErrorMessage(PGconn *conn, PQExpBuffer savedMessage)
{
  initPQExpBuffer(savedMessage);
  appendPQExpBufferStr(savedMessage, conn->errorMessage.data);
  if (PQExpBufferBroken(savedMessage))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return false;
  }
                                             
  resetPQExpBuffer(&conn->errorMessage);
  return true;
}

   
                                                                             
                                                                             
                                                                           
   
static void
restoreErrorMessage(PGconn *conn, PQExpBuffer savedMessage)
{
  appendPQExpBufferStr(savedMessage, conn->errorMessage.data);
  resetPQExpBuffer(&conn->errorMessage);
  appendPQExpBufferStr(&conn->errorMessage, savedMessage->data);
                                                   
  if (PQExpBufferBroken(savedMessage) || PQExpBufferBroken(&conn->errorMessage))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
  }
  termPQExpBuffer(savedMessage);
}

                    
                  
   
                                    
   
                                        
                                                                      
                 
   
                                                     
   
                                                                            
                                                                               
                                  
   
                                                                              
                    
                                                                       
                                                                  
                                                                    
                                                       
                                                                         
                                                                        
                              
   
                    
   
PostgresPollingStatusType
PQconnectPoll(PGconn *conn)
{
  bool reset_connection_state_machine = false;
  bool need_new_connection = false;
  PGresult *res;
  char sebuf[PG_STRERROR_R_BUFLEN];
  int optval;
  PQExpBufferData savedMessage;

  if (conn == NULL)
  {
    return PGRES_POLLING_FAILED;
  }

                        
  switch (conn->status)
  {
       
                                                                       
                      
       
  case CONNECTION_BAD:
    return PGRES_POLLING_FAILED;
  case CONNECTION_OK:
    return PGRES_POLLING_OK;

                                  
  case CONNECTION_AWAITING_RESPONSE:
  case CONNECTION_AUTH_OK:
  {
                           
    int n = pqReadData(conn);

    if (n < 0)
    {
      goto error_return;
    }
    if (n == 0)
    {
      return PGRES_POLLING_READING;
    }

    break;
  }

                                                       
  case CONNECTION_STARTED:
  case CONNECTION_MADE:
    break;

                                                             
  case CONNECTION_SETENV:
    break;

                                                 
  case CONNECTION_SSL_STARTUP:
  case CONNECTION_NEEDED:
  case CONNECTION_CHECK_WRITABLE:
  case CONNECTION_CONSUME:
  case CONNECTION_GSS_STARTUP:
    break;

  default:
    appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("invalid connection state, "
                                                            "probably indicative of memory corruption\n"));
    goto error_return;
  }

keep_going:                                             
                                     

                                                                           
  if (conn->try_next_addr)
  {
    if (conn->addr_cur && conn->addr_cur->ai_next)
    {
      conn->addr_cur = conn->addr_cur->ai_next;
      reset_connection_state_machine = true;
    }
    else
    {
      conn->try_next_host = true;
    }
    conn->try_next_addr = false;
  }

                                                 
  if (conn->try_next_host)
  {
    pg_conn_host *ch;
    struct addrinfo hint;
    int thisport;
    int ret;
    char portstr[MAXPGPATH];

    if (conn->whichhost + 1 >= conn->nconnhost)
    {
         
                                                                       
                                               
         
      goto error_return;
    }
    conn->whichhost++;

                                                 
    release_conn_addrinfo(conn);

       
                                                                      
                                                                         
                                                            
       
    ch = &conn->connhost[conn->whichhost];

                                   
    MemSet(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM;
    conn->addrlist_family = hint.ai_family = AF_UNSPEC;

                                                        
    if (ch->port == NULL || ch->port[0] == '\0')
    {
      thisport = DEF_PGPORT;
    }
    else
    {
      if (!parse_int_param(ch->port, &thisport, conn, "port"))
      {
        goto error_return;
      }

      if (thisport < 1 || thisport > 65535)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("invalid port number: \"%s\"\n"), ch->port);
        goto keep_going;
      }
    }
    snprintf(portstr, sizeof(portstr), "%d", thisport);

                                                         
    switch (ch->type)
    {
    case CHT_HOST_NAME:
      ret = pg_getaddrinfo_all(ch->host, portstr, &hint, &conn->addrlist);
      if (ret || !conn->addrlist)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not translate host name \"%s\" to address: %s\n"), ch->host, gai_strerror(ret));
        goto keep_going;
      }
      break;

    case CHT_HOST_ADDRESS:
      hint.ai_flags = AI_NUMERICHOST;
      ret = pg_getaddrinfo_all(ch->hostaddr, portstr, &hint, &conn->addrlist);
      if (ret || !conn->addrlist)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not parse network address \"%s\": %s\n"), ch->hostaddr, gai_strerror(ret));
        goto keep_going;
      }
      break;

    case CHT_UNIX_SOCKET:
#ifdef HAVE_UNIX_SOCKETS
      conn->addrlist_family = hint.ai_family = AF_UNIX;
      UNIXSOCK_PATH(portstr, thisport, ch->host);
      if (strlen(portstr) >= UNIXSOCK_PATH_BUFLEN)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("Unix-domain socket path \"%s\" is too long (maximum %d bytes)\n"), portstr, (int)(UNIXSOCK_PATH_BUFLEN - 1));
        goto keep_going;
      }

         
                                                                     
                                            
         
      ret = pg_getaddrinfo_all(NULL, portstr, &hint, &conn->addrlist);
      if (ret || !conn->addrlist)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not translate Unix-domain socket path \"%s\" to address: %s\n"), portstr, gai_strerror(ret));
        goto keep_going;
      }
#else
      Assert(false);
#endif
      break;
    }

                                                             
    conn->addr_cur = conn->addrlist;
    reset_connection_state_machine = true;
    conn->try_next_host = false;
  }

                                       
  if (reset_connection_state_machine)
  {
       
                                                                     
                                                                        
                                                                       
                                                          
       
    conn->pversion = PG_PROTOCOL(3, 0);
    conn->send_appname = true;
#ifdef USE_SSL
                                                   
    conn->allow_ssl_try = (conn->sslmode[0] != 'd');                
    conn->wait_ssl_try = (conn->sslmode[0] == 'a');               
#endif
#ifdef ENABLE_GSS
    conn->try_gss = (conn->gssencmode[0] != 'd');                
#endif

    reset_connection_state_machine = false;
    need_new_connection = true;
  }

                                                                      
  if (need_new_connection)
  {
                                      
    pqDropConnection(conn, true);

                                                  
    pqDropServerData(conn);

                                              
    conn->asyncStatus = PGASYNC_IDLE;
    conn->xactStatus = PQTRANS_IDLE;
    pqClearAsyncResult(conn);

                                                                        
    conn->status = CONNECTION_NEEDED;

    need_new_connection = false;
  }

                                                                
  switch (conn->status)
  {
  case CONNECTION_NEEDED:
  {
       
                                                            
                                                                
                        
       
                                                               
                                                               
       
    {
      struct addrinfo *addr_cur = conn->addr_cur;
      char host_addr[NI_MAXHOST];

         
                                                              
                                             
         
      if (addr_cur == NULL)
      {
        conn->try_next_host = true;
        goto keep_going;
      }

                                                           
      memcpy(&conn->raddr.addr, addr_cur->ai_addr, addr_cur->ai_addrlen);
      conn->raddr.salen = addr_cur->ai_addrlen;

                      
      if (conn->connip != NULL)
      {
        free(conn->connip);
        conn->connip = NULL;
      }

      getHostaddr(conn, host_addr, NI_MAXHOST);
      if (strlen(host_addr) > 0)
      {
        conn->connip = strdup(host_addr);
      }

         
                                                               
                          
         

      conn->sock = socket(addr_cur->ai_family, SOCK_STREAM, 0);
      if (conn->sock == PGINVALID_SOCKET)
      {
           
                                                            
                                                             
                                                               
                                                    
           
        if (addr_cur->ai_next != NULL || conn->whichhost + 1 < conn->nconnhost)
        {
          conn->try_next_addr = true;
          goto keep_going;
        }
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not create socket: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
        goto error_return;
      }

         
                                                              
                                                             
                                            
         
      if (!IS_AF_UNIX(addr_cur->ai_family))
      {
        if (!connectNoDelay(conn))
        {
                                             
          conn->try_next_addr = true;
          goto keep_going;
        }
      }
      if (!pg_set_noblock(conn->sock))
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not set socket to nonblocking mode: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
        conn->try_next_addr = true;
        goto keep_going;
      }

#ifdef F_SETFD
      if (fcntl(conn->sock, F_SETFD, FD_CLOEXEC) == -1)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not set socket to close-on-exec mode: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
        conn->try_next_addr = true;
        goto keep_going;
      }
#endif              

      if (!IS_AF_UNIX(addr_cur->ai_family))
      {
#ifndef WIN32
        int on = 1;
#endif
        int usekeepalives = useKeepalives(conn);
        int err = 0;

        if (usekeepalives < 0)
        {
          appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("keepalives parameter must be an integer\n"));
          err = 1;
        }
        else if (usekeepalives == 0)
        {
                          
        }
#ifndef WIN32
        else if (setsockopt(conn->sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
        {
          appendPQExpBuffer(&conn->errorMessage, libpq_gettext("setsockopt(%s) failed: %s\n"), "SO_KEEPALIVE", SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
          err = 1;
        }
        else if (!setKeepalivesIdle(conn) || !setKeepalivesInterval(conn) || !setKeepalivesCount(conn))
        {
          err = 1;
        }
#else            
#ifdef SIO_KEEPALIVE_VALS
        else if (!setKeepalivesWin32(conn))
        {
          err = 1;
        }
#endif                         
#endif            
        else if (!setTCPUserTimeout(conn))
        {
          err = 1;
        }

        if (err)
        {
          conn->try_next_addr = true;
          goto keep_going;
        }
      }

                   
                                                          
                                      
         
                                          
                                         
                                                            
         
                                                            
                                                            
                                                               
                            
         
                                                         
                                                            
         
                                                              
                                                              
                                                                
                                                                
                         
                   
         
      conn->sigpipe_so = false;
#ifdef MSG_NOSIGNAL
      conn->sigpipe_flag = true;
#else
      conn->sigpipe_flag = false;
#endif                   

#ifdef SO_NOSIGPIPE
      optval = 1;
      if (setsockopt(conn->sock, SOL_SOCKET, SO_NOSIGPIPE, (char *)&optval, sizeof(optval)) == 0)
      {
        conn->sigpipe_so = true;
        conn->sigpipe_flag = false;
      }
#endif                   

         
                                                                 
                                                           
         
      if (connect(conn->sock, addr_cur->ai_addr, addr_cur->ai_addrlen) < 0)
      {
        if (SOCK_ERRNO == EINPROGRESS ||
#ifdef WIN32
            SOCK_ERRNO == EWOULDBLOCK ||
#endif
            SOCK_ERRNO == EINTR)
        {
             
                                                            
                                                            
                                             
             
          conn->status = CONNECTION_STARTED;
          return PGRES_POLLING_WRITING;
        }
                                
      }
      else
      {
           
                                                               
                                                              
                                 
           
        conn->status = CONNECTION_STARTED;
        goto keep_going;
      }

         
                                                          
                                                               
         
      connectFailureMessage(conn, SOCK_ERRNO);
      conn->try_next_addr = true;
      goto keep_going;
    }
  }

  case CONNECTION_STARTED:
  {
    ACCEPT_TYPE_ARG3 optlen = sizeof(optval);

       
                                                                
                                        
       

       
                                                               
                                           
       

    if (getsockopt(conn->sock, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen) == -1)
    {
      appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not get socket error status: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
      goto error_return;
    }
    else if (optval != 0)
    {
         
                                                                 
                                                               
                        
         
      connectFailureMessage(conn, optval);

         
                                                                
                                                 
         
      conn->try_next_addr = true;
      goto keep_going;
    }

                                    
    conn->laddr.salen = sizeof(conn->laddr.addr);
    if (getsockname(conn->sock, (struct sockaddr *)&conn->laddr.addr, &conn->laddr.salen) < 0)
    {
      appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not get client address from socket: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
      goto error_return;
    }

       
                                                             
       
    conn->status = CONNECTION_MADE;
    return PGRES_POLLING_WRITING;
  }

  case CONNECTION_MADE:
  {
    char *startpacket;
    int packetlen;

#ifdef HAVE_UNIX_SOCKETS

       
                                                            
                           
       
    if (conn->requirepeer && conn->requirepeer[0] && IS_AF_UNIX(conn->raddr.addr.ss_family))
    {
      char pwdbuf[BUFSIZ];
      struct passwd pass_buf;
      struct passwd *pass;
      int passerr;
      uid_t uid;
      gid_t gid;

      errno = 0;
      if (getpeereid(conn->sock, &uid, &gid) != 0)
      {
           
                                                            
                
           
        if (errno == ENOSYS)
        {
          appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("requirepeer parameter is not supported on this platform\n"));
        }
        else
        {
          appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not get peer credentials: %s\n"), strerror_r(errno, sebuf, sizeof(sebuf)));
        }
        goto error_return;
      }

      passerr = pqGetpwuid(uid, &pass_buf, pwdbuf, sizeof(pwdbuf), &pass);
      if (pass == NULL)
      {
        if (passerr != 0)
        {
          appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not look up local user ID %d: %s\n"), (int)uid, strerror_r(passerr, sebuf, sizeof(sebuf)));
        }
        else
        {
          appendPQExpBuffer(&conn->errorMessage, libpq_gettext("local user with ID %d does not exist\n"), (int)uid);
        }
        goto error_return;
      }

      if (strcmp(pass->pw_name, conn->requirepeer) != 0)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("requirepeer specifies \"%s\", but actual peer user name is \"%s\"\n"), conn->requirepeer, pass->pw_name);
        goto error_return;
      }
    }
#endif                        

    if (IS_AF_UNIX(conn->raddr.addr.ss_family))
    {
                                                         
#ifdef USE_SSL
      conn->allow_ssl_try = false;
#endif
#ifdef ENABLE_GSS
      conn->try_gss = false;
#endif
    }

#ifdef ENABLE_GSS

       
                                                                   
                                                                   
                                                                   
               
       
    if (conn->try_gss && !conn->gctx)
    {
      conn->try_gss = pg_GSS_have_cred_cache(&conn->gcred);
    }
    if (conn->try_gss && !conn->gctx)
    {
      ProtocolVersion pv = pg_hton32(NEGOTIATE_GSS_CODE);

      if (pqPacketSend(conn, 0, &pv, sizeof(pv)) != STATUS_OK)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not send GSSAPI negotiation packet: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
        goto error_return;
      }

                                 
      conn->status = CONNECTION_GSS_STARTUP;
      return PGRES_POLLING_READING;
    }
    else if (!conn->gctx && conn->gssencmode[0] == 'r')
    {
      appendPQExpBuffer(&conn->errorMessage, libpq_gettext("GSSAPI encryption required but was impossible (possibly no credential cache, no server support, or using a local socket)\n"));
      goto error_return;
    }
#endif

#ifdef USE_SSL

       
                                                                  
                                                             
                        
       
    if (conn->allow_ssl_try && !conn->wait_ssl_try && !conn->ssl_in_use
#ifdef ENABLE_GSS
        && !conn->gssenc
#endif
    )
    {
      ProtocolVersion pv;

         
                                      
         
                                                        
                                                           
                      
         
      pv = pg_hton32(NEGOTIATE_SSL_CODE);
      if (pqPacketSend(conn, 0, &pv, sizeof(pv)) != STATUS_OK)
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not send SSL negotiation packet: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
        goto error_return;
      }
                                 
      conn->status = CONNECTION_SSL_STARTUP;
      return PGRES_POLLING_READING;
    }
#endif              

       
                                 
       
    if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
    {
      startpacket = pqBuildStartupPacket3(conn, &packetlen, EnvironmentOptions);
    }
    else
    {
      startpacket = pqBuildStartupPacket2(conn, &packetlen, EnvironmentOptions);
    }
    if (!startpacket)
    {
         
                                                               
                           
         
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
      goto error_return;
    }

       
                                
       
                                                                
                                                            
       
    if (pqPacketSend(conn, 0, startpacket, packetlen) != STATUS_OK)
    {
      appendPQExpBuffer(&conn->errorMessage, libpq_gettext("could not send startup packet: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
      free(startpacket);
      goto error_return;
    }

    free(startpacket);

    conn->status = CONNECTION_AWAITING_RESPONSE;
    return PGRES_POLLING_READING;
  }

       
                                                                
                             
       
  case CONNECTION_SSL_STARTUP:
  {
#ifdef USE_SSL
    PostgresPollingStatusType pollres;

       
                                                                   
                               
       
    if (!conn->ssl_in_use)
    {
         
                                                          
                                                                
                                                         
         
      char SSLok;
      int rdresult;

      rdresult = pqReadData(conn);
      if (rdresult < 0)
      {
                                               
        goto error_return;
      }
      if (rdresult == 0)
      {
                                            
        return PGRES_POLLING_READING;
      }
      if (pqGetc(&SSLok, conn) < 0)
      {
                                      
        return PGRES_POLLING_READING;
      }
      if (SSLok == 'S')
      {
                                
        conn->inStart = conn->inCursor;
                                                 
        if (pqsecure_initialize(conn) != 0)
        {
          goto error_return;
        }
      }
      else if (SSLok == 'N')
      {
                                
        conn->inStart = conn->inCursor;
                                   
        if (conn->sslmode[0] == 'r' ||                
            conn->sslmode[0] == 'v')                     
                                                          
        {
                                                        
          appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("server does not support SSL, but SSL was required\n"));
          goto error_return;
        }
                                                    
        conn->allow_ssl_try = false;
                                                  
        conn->status = CONNECTION_MADE;
        return PGRES_POLLING_WRITING;
      }
      else if (SSLok == 'E')
      {
           
                                                           
                                                           
                                                              
                                                         
                                                            
                                                               
                                                              
                      
           
        conn->status = CONNECTION_AWAITING_RESPONSE;
        goto keep_going;
      }
      else
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("received invalid response to SSL negotiation: %c\n"), SSLok);
        goto error_return;
      }
    }

       
                                                      
       
    pollres = pqsecure_open_client(conn);
    if (pollres == PGRES_POLLING_OK)
    {
         
                                                                
                                                               
                                                               
                                               
         
      if (conn->inCursor != conn->inEnd)
      {
        appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("received unencrypted data after SSL response\n"));
        goto error_return;
      }

                                                            
      conn->status = CONNECTION_MADE;
      return PGRES_POLLING_WRITING;
    }
    if (pollres == PGRES_POLLING_FAILED)
    {
         
                                                             
               
         
      if (conn->sslmode[0] == 'p'               
          && conn->allow_ssl_try                  
          && !conn->wait_ssl_try)                 
      {
                             
        conn->allow_ssl_try = false;
        need_new_connection = true;
        goto keep_going;
      }
                                    
      goto error_return;
    }
                                                                
    return pollres;
#else                
                        
    goto error_return;
#endif              
  }

  case CONNECTION_GSS_STARTUP:
  {
#ifdef ENABLE_GSS
    PostgresPollingStatusType pollres;

       
                                                               
                          
       
    if (conn->try_gss && !conn->gctx)
    {
      char gss_ok;
      int rdresult = pqReadData(conn);

      if (rdresult < 0)
      {
                                               
        goto error_return;
      }
      else if (rdresult == 0)
      {
                                            
        return PGRES_POLLING_READING;
      }
      if (pqGetc(&gss_ok, conn) < 0)
      {
                                 
        return PGRES_POLLING_READING;
      }

      if (gss_ok == 'E')
      {
           
                                                       
                                                              
                                                             
                                                             
                                             
           
        conn->try_gss = false;
        need_new_connection = true;
        goto keep_going;
      }

                              
      conn->inStart = conn->inCursor;

      if (gss_ok == 'N')
      {
                                                             
        if (conn->gssencmode[0] == 'r')
        {
          appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("server doesn't support GSSAPI encryption, but it was required\n"));
          goto error_return;
        }

        conn->try_gss = false;
                                                  
        conn->status = CONNECTION_MADE;
        return PGRES_POLLING_WRITING;
      }
      else if (gss_ok != 'G')
      {
        appendPQExpBuffer(&conn->errorMessage, libpq_gettext("received invalid response to GSSAPI negotiation: %c\n"), gss_ok);
        goto error_return;
      }
    }

                                              
    pollres = pqsecure_open_gss(conn);
    if (pollres == PGRES_POLLING_OK)
    {
         
                                                                
                                                               
                                                               
                                               
         
      if (conn->inCursor != conn->inEnd)
      {
        appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("received unencrypted data after GSSAPI encryption response\n"));
        goto error_return;
      }

                                      
      conn->status = CONNECTION_MADE;
      return PGRES_POLLING_WRITING;
    }
    else if (pollres == PGRES_POLLING_FAILED && conn->gssencmode[0] == 'p')
    {
         
                                                                
                                                  
         
      conn->try_gss = false;
      need_new_connection = true;
      goto keep_going;
    }
    return pollres;
#else                   
                     
    goto error_return;
#endif                 
  }

       
                                                                    
                                 
       
  case CONNECTION_AWAITING_RESPONSE:
  {
    char beresp;
    int msgLength;
    int avail;
    AuthRequest areq;
    int res;

       
                                                                 
                                                                   
                                            
       
    conn->inCursor = conn->inStart;

                        
    if (pqGetc(&beresp, conn))
    {
                                                   
      return PGRES_POLLING_READING;
    }

       
                                                               
                                                               
                                                  
       
    if (!(beresp == 'R' || beresp == 'E'))
    {
      appendPQExpBuffer(&conn->errorMessage,
          libpq_gettext("expected authentication request from "
                        "server, but received %c\n"),
          beresp);
      goto error_return;
    }

    if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
    {
                                    
      if (pqGetInt(&msgLength, 4, conn))
      {
                                                     
        return PGRES_POLLING_READING;
      }
    }
    else
    {
                                                            
      msgLength = 8;
    }

       
                                                       
                                                                 
                                                             
                                                                
                                                              
                                      
       
    if (beresp == 'R' && (msgLength < 8 || msgLength > 2000))
    {
      appendPQExpBuffer(&conn->errorMessage,
          libpq_gettext("expected authentication request from "
                        "server, but received %c\n"),
          beresp);
      goto error_return;
    }

    if (beresp == 'E' && (msgLength < 8 || msgLength > 30000))
    {
                                              
      conn->inCursor = conn->inStart + 1;                  
      if (pqGets_append(&conn->errorMessage, conn))
      {
                                                     
        return PGRES_POLLING_READING;
      }
                                                       
      conn->inStart = conn->inCursor;

         
                                                               
                                                              
         
      appendPQExpBufferChar(&conn->errorMessage, '\n');

         
                                                             
                                    
         
      if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
      {
        conn->pversion = PG_PROTOCOL(2, 0);
        need_new_connection = true;
        goto keep_going;
      }

      goto error_return;
    }

       
                                                         
       
                                                                
                               
       
    msgLength -= 4;
    avail = conn->inEnd - conn->inCursor;
    if (avail < msgLength)
    {
         
                                                              
                                                        
                        
         
      if (pqCheckInBufferSpace(conn->inCursor + (size_t)msgLength, conn))
      {
        goto error_return;
      }
                                                   
      return PGRES_POLLING_READING;
    }

                        
    if (beresp == 'E')
    {
      if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
      {
        if (pqGetErrorNotice3(conn, true))
        {
                                                       
          return PGRES_POLLING_READING;
        }
      }
      else
      {
        if (pqGets_append(&conn->errorMessage, conn))
        {
                                                       
          return PGRES_POLLING_READING;
        }
      }
                                                       
      conn->inStart = conn->inCursor;

                                                        
      pgpassfileWarning(conn);

#ifdef ENABLE_GSS

         
                                                                 
                     
         
      if (conn->gssenc && conn->gssencmode[0] == 'p')
      {
                             
        conn->try_gss = false;
        need_new_connection = true;
        goto keep_going;
      }
#endif

#ifdef USE_SSL

         
                                                           
                                                               
         
      if (conn->sslmode[0] == 'a'              
          && !conn->ssl_in_use && conn->allow_ssl_try && conn->wait_ssl_try)
      {
                             
        conn->wait_ssl_try = false;
        need_new_connection = true;
        goto keep_going;
      }

         
                                                                
                                 
         
      if (conn->sslmode[0] == 'p'                                  
          && conn->ssl_in_use && conn->allow_ssl_try                 
          && !conn->wait_ssl_try)                                    
      {
                             
        conn->allow_ssl_try = false;
        need_new_connection = true;
        goto keep_going;
      }
#endif

      goto error_return;
    }

                                          
    conn->auth_req_received = true;

                                  
    if (pqGetInt((int *)&areq, 4, conn))
    {
                                                    
      return PGRES_POLLING_READING;
    }
    msgLength -= 4;

       
                                                                   
                                                               
                                                                 
                                                                 
                                     
       
    if (areq == AUTH_REQ_MD5 && PG_PROTOCOL_MAJOR(conn->pversion) < 3)
    {
      msgLength += 4;

      avail = conn->inEnd - conn->inCursor;
      if (avail < 4)
      {
           
                                                             
                                                             
                          
           
        if (pqCheckInBufferSpace(conn->inCursor + (size_t)4, conn))
        {
          goto error_return;
        }
                                                     
        return PGRES_POLLING_READING;
      }
    }

       
                                                                   
                                   
       
                                                                  
                                                         
       
    res = pg_fe_sendauth(areq, msgLength, conn);
    conn->errorMessage.len = strlen(conn->errorMessage.data);

                                                               
    conn->inStart = conn->inCursor;

    if (res != STATUS_OK)
    {
      goto error_return;
    }

       
                                                              
                                                                 
                                                                  
       
    if (pqFlush(conn))
    {
      goto error_return;
    }

    if (areq == AUTH_REQ_OK)
    {
                                                    
      conn->status = CONNECTION_AUTH_OK;

         
                                                             
                                                             
                
         
      conn->asyncStatus = PGASYNC_BUSY;
    }

                                               
    goto keep_going;
  }

  case CONNECTION_AUTH_OK:
  {
       
                                                               
                                                                  
                                                             
                                                                 
                                                               
                                                       
                                                                
                                                         
                                                
       

    if (PQisBusy(conn))
    {
      return PGRES_POLLING_READING;
    }

    res = PQgetResult(conn);

       
                                                            
                
       
    if (res)
    {
      if (res->resultStatus != PGRES_FATAL_ERROR)
      {
        appendPQExpBufferStr(&conn->errorMessage, libpq_gettext("unexpected message from server during startup\n"));
      }
      else if (conn->send_appname && (conn->appname || conn->fbappname))
      {
           
                                                              
                                                               
                                                           
                                                          
                                                            
                                                              
                                                           
                                               
           
        const char *sqlstate;

        sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        if (sqlstate && strcmp(sqlstate, ERRCODE_APPNAME_UNKNOWN) == 0)
        {
          PQclear(res);
          conn->send_appname = false;
          need_new_connection = true;
          goto keep_going;
        }
      }

         
                                                               
                                                                
                                                            
                                                     
         
      if (conn->errorMessage.len <= 0 || conn->errorMessage.data[conn->errorMessage.len - 1] != '\n')
      {
        appendPQExpBufferChar(&conn->errorMessage, '\n');
      }
      PQclear(res);
      goto error_return;
    }

                                                        
    if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
    {
      conn->status = CONNECTION_SETENV;
      conn->setenv_state = SETENV_STATE_CLIENT_ENCODING_SEND;
      conn->next_eo = EnvironmentOptions;
      return PGRES_POLLING_WRITING;
    }

       
                                                                   
       
                                                                  
                                                                   
                                            
       
    if (conn->sversion >= 70400 && conn->target_session_attrs != NULL && strcmp(conn->target_session_attrs, "read-write") == 0)
    {
         
                                                             
                                                            
                                                             
                                                                
                                   
         
      if (!saveErrorMessage(conn, &savedMessage))
      {
        goto error_return;
      }

      conn->status = CONNECTION_OK;
      if (!PQsendQuery(conn, "SHOW transaction_read_only"))
      {
        restoreErrorMessage(conn, &savedMessage);
        goto error_return;
      }
      conn->status = CONNECTION_CHECK_WRITABLE;
      restoreErrorMessage(conn, &savedMessage);
      return PGRES_POLLING_READING;
    }

                                              
    release_conn_addrinfo(conn);

                                   
    conn->status = CONNECTION_OK;
    return PGRES_POLLING_OK;
  }

  case CONNECTION_SETENV:

       
                                                                      
       
                                                                      
                
       
    conn->status = CONNECTION_OK;

    switch (pqSetenvPoll(conn))
    {
    case PGRES_POLLING_OK:              
      break;

    case PGRES_POLLING_READING:                  
      conn->status = CONNECTION_SETENV;
      return PGRES_POLLING_READING;

    case PGRES_POLLING_WRITING:                  
      conn->status = CONNECTION_SETENV;
      return PGRES_POLLING_WRITING;

    default:
      goto error_return;
    }

       
                                                                   
                                                                    
               
       
                                                                     
                                                                    
                                        
       
    if (conn->sversion >= 70400 && conn->target_session_attrs != NULL && strcmp(conn->target_session_attrs, "read-write") == 0)
    {
      if (!saveErrorMessage(conn, &savedMessage))
      {
        goto error_return;
      }

      conn->status = CONNECTION_OK;
      if (!PQsendQuery(conn, "SHOW transaction_read_only"))
      {
        restoreErrorMessage(conn, &savedMessage);
        goto error_return;
      }
      conn->status = CONNECTION_CHECK_WRITABLE;
      restoreErrorMessage(conn, &savedMessage);
      return PGRES_POLLING_READING;
    }

                                              
    release_conn_addrinfo(conn);

                                   
    conn->status = CONNECTION_OK;
    return PGRES_POLLING_OK;

  case CONNECTION_CONSUME:
  {
    conn->status = CONNECTION_OK;
    if (!PQconsumeInput(conn))
    {
      goto error_return;
    }

    if (PQisBusy(conn))
    {
      conn->status = CONNECTION_CONSUME;
      return PGRES_POLLING_READING;
    }

       
                                                        
       
    res = PQgetResult(conn);
    if (res != NULL)
    {
      PQclear(res);
      conn->status = CONNECTION_CONSUME;
      goto keep_going;
    }

                                              
    release_conn_addrinfo(conn);

                                   
    conn->status = CONNECTION_OK;
    return PGRES_POLLING_OK;
  }
  case CONNECTION_CHECK_WRITABLE:
  {
    const char *displayed_host;
    const char *displayed_port;

    if (!saveErrorMessage(conn, &savedMessage))
    {
      goto error_return;
    }

    conn->status = CONNECTION_OK;
    if (!PQconsumeInput(conn))
    {
      restoreErrorMessage(conn, &savedMessage);
      goto error_return;
    }

    if (PQisBusy(conn))
    {
      conn->status = CONNECTION_CHECK_WRITABLE;
      restoreErrorMessage(conn, &savedMessage);
      return PGRES_POLLING_READING;
    }

    res = PQgetResult(conn);
    if (res && (PQresultStatus(res) == PGRES_TUPLES_OK) && PQntuples(res) == 1)
    {
      char *val;

      val = PQgetvalue(res, 0, 0);
      if (strncmp(val, "on", 2) == 0)
      {
                                                 
        PQclear(res);
        restoreErrorMessage(conn, &savedMessage);

                                                        
        if (conn->connhost[conn->whichhost].type == CHT_HOST_ADDRESS)
        {
          displayed_host = conn->connhost[conn->whichhost].hostaddr;
        }
        else
        {
          displayed_host = conn->connhost[conn->whichhost].host;
        }
        displayed_port = conn->connhost[conn->whichhost].port;
        if (displayed_port == NULL || displayed_port[0] == '\0')
        {
          displayed_port = DEF_PGPORT_STR;
        }

        appendPQExpBuffer(&conn->errorMessage,
            libpq_gettext("could not make a writable "
                          "connection to server "
                          "\"%s:%s\"\n"),
            displayed_host, displayed_port);

                                        
        conn->status = CONNECTION_OK;
        sendTerminateConn(conn);

           
                                                               
                                               
           
        conn->try_next_host = true;
        goto keep_going;
      }

                                                 
      PQclear(res);
      termPQExpBuffer(&savedMessage);

         
                                                            
                              
         
      conn->status = CONNECTION_CONSUME;
      goto keep_going;
    }

       
                                                                  
                                  
       
    if (res)
    {
      PQclear(res);
    }
    restoreErrorMessage(conn, &savedMessage);

                                                    
    if (conn->connhost[conn->whichhost].type == CHT_HOST_ADDRESS)
    {
      displayed_host = conn->connhost[conn->whichhost].hostaddr;
    }
    else
    {
      displayed_host = conn->connhost[conn->whichhost].host;
    }
    displayed_port = conn->connhost[conn->whichhost].port;
    if (displayed_port == NULL || displayed_port[0] == '\0')
    {
      displayed_port = DEF_PGPORT_STR;
    }
    appendPQExpBuffer(&conn->errorMessage,
        libpq_gettext("test \"SHOW transaction_read_only\" failed "
                      "on server \"%s:%s\"\n"),
        displayed_host, displayed_port);

                                    
    conn->status = CONNECTION_OK;
    sendTerminateConn(conn);

                          
    conn->try_next_addr = true;
    goto keep_going;
  }

  default:
    appendPQExpBuffer(&conn->errorMessage,
        libpq_gettext("invalid connection state %d, "
                      "probably indicative of memory corruption\n"),
        conn->status);
    goto error_return;
  }

                   

error_return:

     
                                                                          
                                                                          
                                                                          
                                                                             
                                              
     
  conn->status = CONNECTION_BAD;
  return PGRES_POLLING_FAILED;
}

   
                 
                                                                  
   
                                                                        
   
static PGPing
internal_ping(PGconn *conn)
{
                                                         
  if (!conn || !conn->options_valid)
  {
    return PQPING_NO_ATTEMPT;
  }

                                          
  if (conn->status != CONNECTION_BAD)
  {
    (void)connectDBComplete(conn);
  }

                                     
  if (conn->status != CONNECTION_BAD)
  {
    return PQPING_OK;
  }

     
                                                                            
                                                                            
                                                                             
                                                                         
                                                                            
                                                                           
                                         
     
  if (conn->auth_req_received)
  {
    return PQPING_OK;
  }

     
                                                                        
                                                                         
                                                                             
                                                                          
                                                                       
                                                                       
                                                                            
     
                                                                         
                                                               
                                                                           
                                  
     
  if (strlen(conn->last_sqlstate) != 5)
  {
    return PQPING_NO_RESPONSE;
  }

     
                                                                             
                                                                  
     
  if (strcmp(conn->last_sqlstate, ERRCODE_CANNOT_CONNECT_NOW) == 0)
  {
    return PQPING_REJECT;
  }

     
                                                                        
                                                                            
                                                                            
                          
     
  return PQPING_OK;
}

   
                   
                                                                       
   
static PGconn *
makeEmptyPGconn(void)
{
  PGconn *conn;

#ifdef WIN32

     
                                                                 
     
                                                                         
                                                                         
                                                                  
     
  static bool wsastartup_done = false;

  if (!wsastartup_done)
  {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
    {
      return NULL;
    }
    wsastartup_done = true;
  }

                                
  WSASetLastError(0);
#endif            

  conn = (PGconn *)malloc(sizeof(PGconn));
  if (conn == NULL)
  {
    return conn;
  }

                                      
  MemSet(conn, 0, sizeof(PGconn));

                                    
  conn->noticeHooks.noticeRec = defaultNoticeReceiver;
  conn->noticeHooks.noticeProc = defaultNoticeProcessor;

  conn->status = CONNECTION_BAD;
  conn->asyncStatus = PGASYNC_IDLE;
  conn->xactStatus = PQTRANS_IDLE;
  conn->options_valid = false;
  conn->nonblocking = false;
  conn->setenv_state = SETENV_STATE_IDLE;
  conn->client_encoding = PG_SQL_ASCII;
  conn->std_strings = false;                                     
  conn->verbosity = PQERRORS_DEFAULT;
  conn->show_context = PQSHOW_CONTEXT_ERRORS;
  conn->sock = PGINVALID_SOCKET;

     
                                                                           
                                                                            
                                                                        
                                                                           
                                           
     
                                                                           
                                                                            
                 
     
  conn->inBufSize = 16 * 1024;
  conn->inBuffer = (char *)malloc(conn->inBufSize);
  conn->outBufSize = 16 * 1024;
  conn->outBuffer = (char *)malloc(conn->outBufSize);
  conn->rowBufLen = 32;
  conn->rowBuf = (PGdataValue *)malloc(conn->rowBufLen * sizeof(PGdataValue));
  initPQExpBuffer(&conn->errorMessage);
  initPQExpBuffer(&conn->workBuffer);

  if (conn->inBuffer == NULL || conn->outBuffer == NULL || conn->rowBuf == NULL || PQExpBufferBroken(&conn->errorMessage) || PQExpBufferBroken(&conn->workBuffer))
  {
                                   
    freePGconn(conn);
    conn = NULL;
  }

  return conn;
}

   
              
                                                  
   
                                                                       
                                                                           
                                                                         
                                                                                
   
static void
freePGconn(PGconn *conn)
{
  int i;

                                                     
  for (i = 0; i < conn->nEvents; i++)
  {
    PGEventConnDestroy evt;

    evt.conn = conn;
    (void)conn->events[i].proc(PGEVT_CONNDESTROY, &evt, conn->events[i].passThrough);
    free(conn->events[i].name);
  }

                                        
  if (conn->connhost != NULL)
  {
    for (i = 0; i < conn->nconnhost; ++i)
    {
      if (conn->connhost[i].host != NULL)
      {
        free(conn->connhost[i].host);
      }
      if (conn->connhost[i].hostaddr != NULL)
      {
        free(conn->connhost[i].hostaddr);
      }
      if (conn->connhost[i].port != NULL)
      {
        free(conn->connhost[i].port);
      }
      if (conn->connhost[i].password != NULL)
      {
        free(conn->connhost[i].password);
      }
    }
    free(conn->connhost);
  }

  if (conn->client_encoding_initial)
  {
    free(conn->client_encoding_initial);
  }
  if (conn->events)
  {
    free(conn->events);
  }
  if (conn->pghost)
  {
    free(conn->pghost);
  }
  if (conn->pghostaddr)
  {
    free(conn->pghostaddr);
  }
  if (conn->pgport)
  {
    free(conn->pgport);
  }
  if (conn->pgtty)
  {
    free(conn->pgtty);
  }
  if (conn->connect_timeout)
  {
    free(conn->connect_timeout);
  }
  if (conn->pgtcp_user_timeout)
  {
    free(conn->pgtcp_user_timeout);
  }
  if (conn->pgoptions)
  {
    free(conn->pgoptions);
  }
  if (conn->appname)
  {
    free(conn->appname);
  }
  if (conn->fbappname)
  {
    free(conn->fbappname);
  }
  if (conn->dbName)
  {
    free(conn->dbName);
  }
  if (conn->replication)
  {
    free(conn->replication);
  }
  if (conn->pguser)
  {
    free(conn->pguser);
  }
  if (conn->pgpass)
  {
    free(conn->pgpass);
  }
  if (conn->pgpassfile)
  {
    free(conn->pgpassfile);
  }
  if (conn->keepalives)
  {
    free(conn->keepalives);
  }
  if (conn->keepalives_idle)
  {
    free(conn->keepalives_idle);
  }
  if (conn->keepalives_interval)
  {
    free(conn->keepalives_interval);
  }
  if (conn->keepalives_count)
  {
    free(conn->keepalives_count);
  }
  if (conn->sslmode)
  {
    free(conn->sslmode);
  }
  if (conn->sslcert)
  {
    free(conn->sslcert);
  }
  if (conn->sslkey)
  {
    free(conn->sslkey);
  }
  if (conn->sslrootcert)
  {
    free(conn->sslrootcert);
  }
  if (conn->sslcrl)
  {
    free(conn->sslcrl);
  }
  if (conn->sslcompression)
  {
    free(conn->sslcompression);
  }
  if (conn->requirepeer)
  {
    free(conn->requirepeer);
  }
  if (conn->gssencmode)
  {
    free(conn->gssencmode);
  }
  if (conn->krbsrvname)
  {
    free(conn->krbsrvname);
  }
  if (conn->gsslib)
  {
    free(conn->gsslib);
  }
  if (conn->connip)
  {
    free(conn->connip);
  }
                                                            
  if (conn->last_query)
  {
    free(conn->last_query);
  }
  if (conn->write_err_msg)
  {
    free(conn->write_err_msg);
  }
  if (conn->inBuffer)
  {
    free(conn->inBuffer);
  }
  if (conn->outBuffer)
  {
    free(conn->outBuffer);
  }
  if (conn->rowBuf)
  {
    free(conn->rowBuf);
  }
  if (conn->target_session_attrs)
  {
    free(conn->target_session_attrs);
  }
  termPQExpBuffer(&conn->errorMessage);
  termPQExpBuffer(&conn->workBuffer);

  free(conn);
}

   
                         
                                            
   
static void
release_conn_addrinfo(PGconn *conn)
{
  if (conn->addrlist)
  {
    pg_freeaddrinfo_all(conn->addrlist_family, conn->addrlist);
    conn->addrlist = NULL;
    conn->addr_cur = NULL;                 
  }
}

   
                     
                                           
   
static void
sendTerminateConn(PGconn *conn)
{
     
                                                                        
                               
     
  if (conn->sock != PGINVALID_SOCKET && conn->status == CONNECTION_OK)
  {
       
                                                                     
              
       
    pqPutMsgStart('X', false, conn);
    pqPutMsgEnd(conn);
    (void)pqFlush(conn);
  }
}

   
               
                                                 
   
                                                                            
                                                                            
                                                        
   
static void
closePGconn(PGconn *conn)
{
     
                                                                           
     
  sendTerminateConn(conn);

     
                                                                       
     
                                                                          
                           
     
  conn->nonblocking = false;

     
                                                                         
     
  pqDropConnection(conn, true);
  conn->status = CONNECTION_BAD;                                           
  conn->asyncStatus = PGASYNC_IDLE;
  conn->xactStatus = PQTRANS_IDLE;
  pqClearAsyncResult(conn);                        
  resetPQExpBuffer(&conn->errorMessage);
  release_conn_addrinfo(conn);

                                                 
  pqDropServerData(conn);
}

   
                                                                    
                                                                    
   
void
PQfinish(PGconn *conn)
{
  if (conn)
  {
    closePGconn(conn);
    freePGconn(conn);
  }
}

   
                                                                
                                               
   
void
PQreset(PGconn *conn)
{
  if (conn)
  {
    closePGconn(conn);

    if (connectDBStart(conn) && connectDBComplete(conn))
    {
         
                                                                         
                                                            
         
      int i;

      for (i = 0; i < conn->nEvents; i++)
      {
        PGEventConnReset evt;

        evt.conn = conn;
        if (!conn->events[i].proc(PGEVT_CONNRESET, &evt, conn->events[i].passThrough))
        {
          conn->status = CONNECTION_BAD;
          printfPQExpBuffer(&conn->errorMessage, libpq_gettext("PGEventProc \"%s\" failed during PGEVT_CONNRESET event\n"), conn->events[i].name);
          break;
        }
      }
    }
  }
}

   
                 
                                        
                                                      
                                       
   
int
PQresetStart(PGconn *conn)
{
  if (conn)
  {
    closePGconn(conn);

    return connectDBStart(conn);
  }

  return 0;
}

   
                
                                        
                                                      
   
PostgresPollingStatusType
PQresetPoll(PGconn *conn)
{
  if (conn)
  {
    PostgresPollingStatusType status = PQconnectPoll(conn);

    if (status == PGRES_POLLING_OK)
    {
         
                                                                         
                                                            
         
      int i;

      for (i = 0; i < conn->nEvents; i++)
      {
        PGEventConnReset evt;

        evt.conn = conn;
        if (!conn->events[i].proc(PGEVT_CONNRESET, &evt, conn->events[i].passThrough))
        {
          conn->status = CONNECTION_BAD;
          printfPQExpBuffer(&conn->errorMessage, libpq_gettext("PGEventProc \"%s\" failed during PGEVT_CONNRESET event\n"), conn->events[i].name);
          return PGRES_POLLING_FAILED;
        }
      }
    }

    return status;
  }

  return PGRES_POLLING_FAILED;
}

   
                                                                        
   
                                                                          
                                                                          
                                                                           
                                                                           
                                                       
   
PGcancel *
PQgetCancel(PGconn *conn)
{
  PGcancel *cancel;

  if (!conn)
  {
    return NULL;
  }

  if (conn->sock == PGINVALID_SOCKET)
  {
    return NULL;
  }

  cancel = malloc(sizeof(PGcancel));
  if (cancel == NULL)
  {
    return NULL;
  }

  memcpy(&cancel->raddr, &conn->raddr, sizeof(SockAddr));
  cancel->be_pid = conn->be_pid;
  cancel->be_key = conn->be_key;

  return cancel;
}

                                           
void
PQfreeCancel(PGcancel *cancel)
{
  if (cancel)
  {
    free(cancel);
  }
}

   
                                                                        
                      
   
                                                                   
                                                                           
                                                                              
                                                                          
   
                                                                             
                                                                            
                                                                               
                                                                             
                                                                           
                                                                          
                                                                          
   
                                                                         
                                                             
   
static int
internal_cancel(SockAddr *raddr, int be_pid, int be_key, char *errbuf, int errbufsize)
{
  int save_errno = SOCK_ERRNO;
  pgsocket tmpsock = PGINVALID_SOCKET;
  int maxlen;
  struct
  {
    uint32 packetlen;
    CancelRequestPacket cp;
  } crp;

     
                                                                            
                        
     
  if ((tmpsock = socket(raddr->addr.ss_family, SOCK_STREAM, 0)) == PGINVALID_SOCKET)
  {
    strlcpy(errbuf, "PQcancel() -- socket() failed: ", errbufsize);
    goto cancel_errReturn;
  }
retry3:
  if (connect(tmpsock, (struct sockaddr *)&raddr->addr, raddr->salen) < 0)
  {
    if (SOCK_ERRNO == EINTR)
    {
                                                          
      goto retry3;
    }
    strlcpy(errbuf, "PQcancel() -- connect() failed: ", errbufsize);
    goto cancel_errReturn;
  }

     
                                                             
     

                                                  

  crp.packetlen = pg_hton32((uint32)sizeof(crp));
  crp.cp.cancelRequestCode = (MsgType)pg_hton32(CANCEL_REQUEST_CODE);
  crp.cp.backendPID = pg_hton32(be_pid);
  crp.cp.cancelAuthCode = pg_hton32(be_key);

retry4:
  if (send(tmpsock, (char *)&crp, sizeof(crp), 0) != (int)sizeof(crp))
  {
    if (SOCK_ERRNO == EINTR)
    {
                                                          
      goto retry4;
    }
    strlcpy(errbuf, "PQcancel() -- send() failed: ", errbufsize);
    goto cancel_errReturn;
  }

     
                                                                           
                                                                             
                                                                           
                                                                           
                                                                          
     
retry5:
  if (recv(tmpsock, (char *)&crp, 1, 0) < 0)
  {
    if (SOCK_ERRNO == EINTR)
    {
                                                          
      goto retry5;
    }
                                          
  }

                
  closesocket(tmpsock);
  SOCK_ERRNO_SET(save_errno);
  return true;

cancel_errReturn:

     
                                                                             
                                            
     
  maxlen = errbufsize - strlen(errbuf) - 2;
  if (maxlen >= 0)
  {
       
                                                                          
                                                                          
                     
       
    int val = SOCK_ERRNO;
    char buf[32];
    char *bufp;

    bufp = buf + sizeof(buf) - 1;
    *bufp = '\0';
    do
    {
      *(--bufp) = (val % 10) + '0';
      val /= 10;
    } while (val > 0);
    bufp -= 6;
    memcpy(bufp, "error ", 6);
    strncat(errbuf, bufp, maxlen);
    strcat(errbuf, "\n");
  }
  if (tmpsock != PGINVALID_SOCKET)
  {
    closesocket(tmpsock);
  }
  SOCK_ERRNO_SET(save_errno);
  return false;
}

   
                                  
   
                                                                  
   
                                                                            
                                                                          
                   
   
int
PQcancel(PGcancel *cancel, char *errbuf, int errbufsize)
{
  if (!cancel)
  {
    strlcpy(errbuf, "PQcancel() -- no cancel object supplied", errbufsize);
    return false;
  }

  return internal_cancel(&cancel->raddr, cancel->be_pid, cancel->be_key, errbuf, errbufsize);
}

   
                                                                              
   
                                                                  
   
                                                                            
                                                                          
                          
   
                                                                   
                                                                             
   
int
PQrequestCancel(PGconn *conn)
{
  int r;

                                        
  if (!conn)
  {
    return false;
  }

  if (conn->sock == PGINVALID_SOCKET)
  {
    strlcpy(conn->errorMessage.data, "PQrequestCancel() -- connection is not open\n", conn->errorMessage.maxlen);
    conn->errorMessage.len = strlen(conn->errorMessage.data);

    return false;
  }

  r = internal_cancel(&conn->raddr, conn->be_pid, conn->be_key, conn->errorMessage.data, conn->errorMessage.maxlen);

  if (!r)
  {
    conn->errorMessage.len = strlen(conn->errorMessage.data);
  }

  return r;
}

   
                                                                      
   
                                                                         
                                              
   
                                                                           
                                                                         
   
                                                                  
                            
   
                                                                         
                             
   
int
pqPacketSend(PGconn *conn, char pack_type, const void *buf, size_t buf_len)
{
                          
  if (pqPutMsgStart(pack_type, true, conn))
  {
    return STATUS_ERROR;
  }

                              
  if (pqPutnchar(buf, buf_len, conn))
  {
    return STATUS_ERROR;
  }

                           
  if (pqPutMsgEnd(conn))
  {
    return STATUS_ERROR;
  }

                                        
  if (pqFlush(conn))
  {
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

#ifdef USE_LDAP

#define LDAP_URL "ldap://"
#define LDAP_DEF_PORT 389
#define PGLDAP_TIMEOUT 2

#define ld_is_sp_tab(x) ((x) == ' ' || (x) == '\t')
#define ld_is_nl_cr(x) ((x) == '\r' || (x) == '\n')

   
                      
   
                                                                       
                                                                          
                                      
   
                                                                
                                                          
   
           
                                   
                                                                   
                                  
                                                   
                                
   
                                                                                
   
static int
ldapServiceLookup(const char *purl, PQconninfoOption *options, PQExpBuffer errorMessage)
{
  int port = LDAP_DEF_PORT, scope, rc, size, state, oldstate, i;
#ifndef WIN32
  int msgid;
#endif
  bool found_keyword;
  char *url, *hostname, *portstr, *endptr, *dn, *scopestr, *filter, *result, *p, *p1 = NULL, *optname = NULL, *optval = NULL;
  char *attrs[2] = {NULL, NULL};
  LDAP *ld = NULL;
  LDAPMessage *res, *entry;
  struct berval **values;
  LDAP_TIMEVAL time = {PGLDAP_TIMEOUT, 0};

  if ((url = strdup(purl)) == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    return 3;
  }

     
                                                                           
                                                                      
                
     

  if (pg_strncasecmp(url, LDAP_URL, strlen(LDAP_URL)) != 0)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": scheme must be ldap://\n"), purl);
    free(url);
    return 3;
  }

                
  hostname = url + strlen(LDAP_URL);
  if (*hostname == '/')                   
  {
    hostname = DefaultHost;                  
  }

                                
  p = strchr(url + strlen(LDAP_URL), '/');
  if (p == NULL || *(p + 1) == '\0' || *(p + 1) == '?')
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": missing distinguished name\n"), purl);
    free(url);
    return 3;
  }
  *p = '\0';                         
  dn = p + 1;

                 
  if ((p = strchr(dn, '?')) == NULL || *(p + 1) == '\0' || *(p + 1) == '?')
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": must have exactly one attribute\n"), purl);
    free(url);
    return 3;
  }
  *p = '\0';
  attrs[0] = p + 1;

             
  if ((p = strchr(attrs[0], '?')) == NULL || *(p + 1) == '\0' || *(p + 1) == '?')
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": must have search scope (base/one/sub)\n"), purl);
    free(url);
    return 3;
  }
  *p = '\0';
  scopestr = p + 1;

              
  if ((p = strchr(scopestr, '?')) == NULL || *(p + 1) == '\0' || *(p + 1) == '?')
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": no filter\n"), purl);
    free(url);
    return 3;
  }
  *p = '\0';
  filter = p + 1;
  if ((p = strchr(filter, '?')) != NULL)
  {
    *p = '\0';
  }

                    
  if ((p1 = strchr(hostname, ':')) != NULL)
  {
    long lport;

    *p1 = '\0';
    portstr = p1 + 1;
    errno = 0;
    lport = strtol(portstr, &endptr, 10);
    if (*portstr == '\0' || *endptr != '\0' || errno || lport < 0 || lport > 65535)
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": invalid port number\n"), purl);
      free(url);
      return 3;
    }
    port = (int)lport;
  }

                                
  if (strchr(attrs[0], ',') != NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": must have exactly one attribute\n"), purl);
    free(url);
    return 3;
  }

                 
  if (pg_strcasecmp(scopestr, "base") == 0)
  {
    scope = LDAP_SCOPE_BASE;
  }
  else if (pg_strcasecmp(scopestr, "one") == 0)
  {
    scope = LDAP_SCOPE_ONELEVEL;
  }
  else if (pg_strcasecmp(scopestr, "sub") == 0)
  {
    scope = LDAP_SCOPE_SUBTREE;
  }
  else
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid LDAP URL \"%s\": must have search scope (base/one/sub)\n"), purl);
    free(url);
    return 3;
  }

                                 
  if ((ld = ldap_init(hostname, port)) == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("could not create LDAP structure\n"));
    free(url);
    return 3;
  }

     
                                         
     
                                                                           
                                                                          
                                                                             
                                                                             
                                
     
                                                                            
                           
     
#ifdef WIN32
                                                                        
  if (ldap_connect(ld, &time) != LDAP_SUCCESS)
  {
                                          
    free(url);
    ldap_unbind(ld);
    return 2;
  }
#else              
                                                            
  if (ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &time) != LDAP_SUCCESS)
  {
    free(url);
    ldap_unbind(ld);
    return 3;
  }

                      
  if ((msgid = ldap_simple_bind(ld, NULL, NULL)) == -1)
  {
                                  
    free(url);
    ldap_unbind(ld);
    return 2;
  }

                                                    
  res = NULL;
  if ((rc = ldap_result(ld, msgid, LDAP_MSG_ALL, &time, &res)) == -1 || res == NULL)
  {
                          
    if (res != NULL)
    {
      ldap_msgfree(res);
    }
    free(url);
    ldap_unbind(ld);
    return 2;
  }
  ldap_msgfree(res);

                     
  time.tv_sec = -1;
  if (ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &time) != LDAP_SUCCESS)
  {
    free(url);
    ldap_unbind(ld);
    return 3;
  }
#endif            

              
  res = NULL;
  if ((rc = ldap_search_st(ld, dn, scope, filter, attrs, 0, &time, &res)) != LDAP_SUCCESS)
  {
    if (res != NULL)
    {
      ldap_msgfree(res);
    }
    printfPQExpBuffer(errorMessage, libpq_gettext("lookup on LDAP server failed: %s\n"), ldap_err2string(rc));
    ldap_unbind(ld);
    free(url);
    return 1;
  }

                                                    
  if ((rc = ldap_count_entries(ld, res)) != 1)
  {
    printfPQExpBuffer(errorMessage, rc ? libpq_gettext("more than one entry found on LDAP lookup\n") : libpq_gettext("no entry found on LDAP lookup\n"));
    ldap_msgfree(res);
    ldap_unbind(ld);
    free(url);
    return 1;
  }

                 
  if ((entry = ldap_first_entry(ld, res)) == NULL)
  {
                             
    printfPQExpBuffer(errorMessage, libpq_gettext("no entry found on LDAP lookup\n"));
    ldap_msgfree(res);
    ldap_unbind(ld);
    free(url);
    return 1;
  }

                  
  if ((values = ldap_get_values_len(ld, entry, attrs[0])) == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("attribute has no values on LDAP lookup\n"));
    ldap_msgfree(res);
    ldap_unbind(ld);
    free(url);
    return 1;
  }

  ldap_msgfree(res);
  free(url);

  if (values[0] == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("attribute has no values on LDAP lookup\n"));
    ldap_value_free_len(values);
    ldap_unbind(ld);
    return 1;
  }

                                                                        
  size = 1;                            
  for (i = 0; values[i] != NULL; i++)
  {
    size += values[i]->bv_len + 1;
  }
  if ((result = malloc(size)) == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    ldap_value_free_len(values);
    ldap_unbind(ld);
    return 3;
  }
  p = result;
  for (i = 0; values[i] != NULL; i++)
  {
    memcpy(p, values[i]->bv_val, values[i]->bv_len);
    p += values[i]->bv_len;
    *(p++) = '\n';
  }
  *p = '\0';

  ldap_value_free_len(values);
  ldap_unbind(ld);

                           
  oldstate = state = 0;
  for (p = result; *p != '\0'; ++p)
  {
    switch (state)
    {
    case 0:                      
      if (!ld_is_sp_tab(*p) && !ld_is_nl_cr(*p))
      {
        optname = p;
        state = 1;
      }
      break;
    case 1:                     
      if (ld_is_sp_tab(*p))
      {
        *p = '\0';
        state = 2;
      }
      else if (ld_is_nl_cr(*p))
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("missing \"=\" after \"%s\" in connection info string\n"), optname);
        free(result);
        return 3;
      }
      else if (*p == '=')
      {
        *p = '\0';
        state = 3;
      }
      break;
    case 2:                        
      if (*p == '=')
      {
        state = 3;
      }
      else if (!ld_is_sp_tab(*p))
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("missing \"=\" after \"%s\" in connection info string\n"), optname);
        free(result);
        return 3;
      }
      break;
    case 3:                          
      if (*p == '\'')
      {
        optval = p + 1;
        p1 = p + 1;
        state = 5;
      }
      else if (ld_is_nl_cr(*p))
      {
        optval = optname + strlen(optname);            
        state = 0;
      }
      else if (!ld_is_sp_tab(*p))
      {
        optval = p;
        state = 4;
      }
      break;
    case 4:                               
      if (ld_is_sp_tab(*p) || ld_is_nl_cr(*p))
      {
        *p = '\0';
        state = 0;
      }
      break;
    case 5:                             
      if (*p == '\'')
      {
        *p1 = '\0';
        state = 0;
      }
      else if (*p == '\\')
      {
        state = 6;
      }
      else
      {
        *(p1++) = *p;
      }
      break;
    case 6:                                          
      *(p1++) = *p;
      state = 5;
      break;
    }

    if (state == 0 && oldstate != 0)
    {
      found_keyword = false;
      for (i = 0; options[i].keyword; i++)
      {
        if (strcmp(options[i].keyword, optname) == 0)
        {
          if (options[i].val == NULL)
          {
            options[i].val = strdup(optval);
            if (!options[i].val)
            {
              printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
              free(result);
              return 3;
            }
          }
          found_keyword = true;
          break;
        }
      }
      if (!found_keyword)
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("invalid connection option \"%s\"\n"), optname);
        free(result);
        return 1;
      }
      optname = NULL;
      optval = NULL;
    }
    oldstate = state;
  }

  free(result);

  if (state == 5 || state == 6)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("unterminated quoted string in connection info string\n"));
    return 3;
  }

  return 0;
}

#endif               

#define MAXBUFSIZE 256

   
                                                                             
                                             
   
                                                                          
                                                                          
                                                                          
                                                                            
                                
   
static int
parseServiceInfo(PQconninfoOption *options, PQExpBuffer errorMessage)
{
  const char *service = conninfo_getval(options, "service");
  char serviceFile[MAXPGPATH];
  char *env;
  bool group_found = false;
  int status;
  struct stat stat_buf;

     
                                                                            
                                                                            
                               
     
  if (service == NULL)
  {
    service = getenv("PGSERVICE");
  }

                                               
  if (service == NULL)
  {
    return 0;
  }

     
                                                                          
              
     
  if ((env = getenv("PGSERVICEFILE")) != NULL)
  {
    strlcpy(serviceFile, env, sizeof(serviceFile));
  }
  else
  {
    char homedir[MAXPGPATH];

    if (!pqGetHomeDirectory(homedir, sizeof(homedir)))
    {
      goto next_file;
    }
    snprintf(serviceFile, MAXPGPATH, "%s/%s", homedir, ".pg_service.conf");
    if (stat(serviceFile, &stat_buf) != 0)
    {
      goto next_file;
    }
  }

  status = parseServiceFile(serviceFile, service, options, errorMessage, &group_found);
  if (group_found || status != 0)
  {
    return status;
  }

next_file:

     
                                                                      
                                        
     
  snprintf(serviceFile, MAXPGPATH, "%s/pg_service.conf", getenv("PGSYSCONFDIR") ? getenv("PGSYSCONFDIR") : SYSCONFDIR);
  if (stat(serviceFile, &stat_buf) != 0)
  {
    goto last_file;
  }

  status = parseServiceFile(serviceFile, service, options, errorMessage, &group_found);
  if (status != 0)
  {
    return status;
  }

last_file:
  if (!group_found)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("definition of service \"%s\" not found\n"), service);
    return 3;
  }

  return 0;
}

static int
parseServiceFile(const char *serviceFile, const char *service, PQconninfoOption *options, PQExpBuffer errorMessage, bool *group_found)
{
  int linenr = 0, i;
  FILE *f;
  char buf[MAXBUFSIZE], *line;

  f = fopen(serviceFile, "r");
  if (f == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("service file \"%s\" not found\n"), serviceFile);
    return 1;
  }

  while ((line = fgets(buf, sizeof(buf), f)) != NULL)
  {
    int len;

    linenr++;

    if (strlen(line) >= sizeof(buf) - 1)
    {
      fclose(f);
      printfPQExpBuffer(errorMessage, libpq_gettext("line %d too long in service file \"%s\"\n"), linenr, serviceFile);
      return 2;
    }

                                                                         
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
      line[--len] = '\0';
    }

                               
    while (*line && isspace((unsigned char)line[0]))
    {
      line++;
    }

                                         
    if (line[0] == '\0' || line[0] == '#')
    {
      continue;
    }

                                   
    if (line[0] == '[')
    {
      if (*group_found)
      {
                                     
        fclose(f);
        return 0;
      }

      if (strncmp(line + 1, service, strlen(service)) == 0 && line[strlen(service) + 1] == ']')
      {
        *group_found = true;
      }
      else
      {
        *group_found = false;
      }
    }
    else
    {
      if (*group_found)
      {
           
                                                                     
           
        char *key, *val;
        bool found_keyword;

#ifdef USE_LDAP
        if (strncmp(line, "ldap", 4) == 0)
        {
          int rc = ldapServiceLookup(line, options, errorMessage);

                                                     
          switch (rc)
          {
          case 0:
            fclose(f);
            return 0;
          case 1:
          case 3:
            fclose(f);
            return 3;
          case 2:
            continue;
          }
        }
#endif

        key = line;
        val = strchr(line, '=');
        if (val == NULL)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("syntax error in service file \"%s\", line %d\n"), serviceFile, linenr);
          fclose(f);
          return 3;
        }
        *val++ = '\0';

        if (strcmp(key, "service") == 0)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("nested service specifications not supported in service file \"%s\", line %d\n"), serviceFile, linenr);
          fclose(f);
          return 3;
        }

           
                                                                 
                             
           
        found_keyword = false;
        for (i = 0; options[i].keyword; i++)
        {
          if (strcmp(options[i].keyword, key) == 0)
          {
            if (options[i].val == NULL)
            {
              options[i].val = strdup(val);
            }
            if (!options[i].val)
            {
              printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
              fclose(f);
              return 3;
            }
            found_keyword = true;
            break;
          }
        }

        if (!found_keyword)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("syntax error in service file \"%s\", line %d\n"), serviceFile, linenr);
          fclose(f);
          return 3;
        }
      }
    }
  }

  fclose(f);

  return 0;
}

   
                    
   
                                                             
                                                                     
                                                                      
                                    
   
                                                                          
                                                                              
                                              
   
                                                                
                                                        
   
PQconninfoOption *
PQconninfoParse(const char *conninfo, char **errmsg)
{
  PQExpBufferData errorBuf;
  PQconninfoOption *connOptions;

  if (errmsg)
  {
    *errmsg = NULL;              
  }
  initPQExpBuffer(&errorBuf);
  if (PQExpBufferDataBroken(errorBuf))
  {
    return NULL;                                
  }
  connOptions = parse_connection_string(conninfo, &errorBuf, false);
  if (connOptions == NULL && errmsg)
  {
    *errmsg = errorBuf.data;
  }
  else
  {
    termPQExpBuffer(&errorBuf);
  }
  return connOptions;
}

   
                                                                 
   
static PQconninfoOption *
conninfo_init(PQExpBuffer errorMessage)
{
  PQconninfoOption *options;
  PQconninfoOption *opt_dest;
  const internalPQconninfoOption *cur_opt;

     
                                                                          
                                
     
  options = (PQconninfoOption *)malloc(sizeof(PQconninfoOption) * sizeof(PQconninfoOptions) / sizeof(PQconninfoOptions[0]));
  if (options == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    return NULL;
  }
  opt_dest = options;

  for (cur_opt = PQconninfoOptions; cur_opt->keyword; cur_opt++)
  {
                                                                        
    memcpy(opt_dest, cur_opt, sizeof(PQconninfoOption));
    opt_dest++;
  }
  MemSet(opt_dest, 0, sizeof(PQconninfoOption));

  return options;
}

   
                            
   
                                                                        
                                                                             
   
                                                                               
                                
   
static PQconninfoOption *
parse_connection_string(const char *connstr, PQExpBuffer errorMessage, bool use_defaults)
{
                                                            
  if (uri_prefix_length(connstr) != 0)
  {
    return conninfo_uri_parse(connstr, errorMessage, use_defaults);
  }

                                  
  return conninfo_parse(connstr, errorMessage, use_defaults);
}

   
                                                                          
                
   
                                                                                
   
                                            
   
static int
uri_prefix_length(const char *connstr)
{
  if (strncmp(connstr, uri_designator, sizeof(uri_designator) - 1) == 0)
  {
    return sizeof(uri_designator) - 1;
  }

  if (strncmp(connstr, short_uri_designator, sizeof(short_uri_designator) - 1) == 0)
  {
    return sizeof(short_uri_designator) - 1;
  }

  return 0;
}

   
                                                                         
                         
   
                                                                            
                                                                          
   
                                           
   
static bool
recognized_connection_string(const char *connstr)
{
  return uri_prefix_length(connstr) != 0 || strchr(connstr, '=') != NULL;
}

   
                                          
   
                                                  
   
static PQconninfoOption *
conninfo_parse(const char *conninfo, PQExpBuffer errorMessage, bool use_defaults)
{
  char *pname;
  char *pval;
  char *buf;
  char *cp;
  char *cp2;
  PQconninfoOption *options;

                                                
  options = conninfo_init(errorMessage);
  if (options == NULL)
  {
    return NULL;
  }

                                                  
  if ((buf = strdup(conninfo)) == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    PQconninfoFree(options);
    return NULL;
  }
  cp = buf;

  while (*cp)
  {
                                               
    if (isspace((unsigned char)*cp))
    {
      cp++;
      continue;
    }

                                
    pname = cp;
    while (*cp)
    {
      if (*cp == '=')
      {
        break;
      }
      if (isspace((unsigned char)*cp))
      {
        *cp++ = '\0';
        while (*cp)
        {
          if (!isspace((unsigned char)*cp))
          {
            break;
          }
          cp++;
        }
        break;
      }
      cp++;
    }

                                             
    if (*cp != '=')
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("missing \"=\" after \"%s\" in connection info string\n"), pname);
      PQconninfoFree(options);
      free(buf);
      return NULL;
    }
    *cp++ = '\0';

                                   
    while (*cp)
    {
      if (!isspace((unsigned char)*cp))
      {
        break;
      }
      cp++;
    }

                                 
    pval = cp;

    if (*cp != '\'')
    {
      cp2 = pval;
      while (*cp)
      {
        if (isspace((unsigned char)*cp))
        {
          *cp++ = '\0';
          break;
        }
        if (*cp == '\\')
        {
          cp++;
          if (*cp != '\0')
          {
            *cp2++ = *cp++;
          }
        }
        else
        {
          *cp2++ = *cp++;
        }
      }
      *cp2 = '\0';
    }
    else
    {
      cp2 = pval;
      cp++;
      for (;;)
      {
        if (*cp == '\0')
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("unterminated quoted string in connection info string\n"));
          PQconninfoFree(options);
          free(buf);
          return NULL;
        }
        if (*cp == '\\')
        {
          cp++;
          if (*cp != '\0')
          {
            *cp2++ = *cp++;
          }
          continue;
        }
        if (*cp == '\'')
        {
          *cp2 = '\0';
          cp++;
          break;
        }
        *cp2++ = *cp++;
      }
    }

       
                                                                  
       
    if (!conninfo_storeval(options, pname, pval, errorMessage, false, false))
    {
      PQconninfoFree(options);
      free(buf);
      return NULL;
    }
  }

                                             
  free(buf);

     
                                               
     
  if (use_defaults)
  {
    if (!conninfo_add_defaults(options, errorMessage))
    {
      PQconninfoFree(options);
      return NULL;
    }
  }

  return options;
}

   
                                 
   
                                                                 
                                                               
                         
                                                                           
                                                              
   
                                                                               
                                                               
                                                                           
                                                                            
                                                                               
                                                                            
                                                                               
                                                                               
                                                                         
   
static PQconninfoOption *
conninfo_array_parse(const char *const *keywords, const char *const *values, PQExpBuffer errorMessage, bool use_defaults, int expand_dbname)
{
  PQconninfoOption *options;
  PQconninfoOption *dbname_options = NULL;
  PQconninfoOption *option;
  int i = 0;

     
                                                                           
                                              
     
  while (expand_dbname && keywords[i])
  {
    const char *pname = keywords[i];
    const char *pvalue = values[i];

                                    
    if (strcmp(pname, "dbname") == 0 && pvalue)
    {
         
                                                                   
                                                                     
                                                        
         
      if (recognized_connection_string(pvalue))
      {
        dbname_options = parse_connection_string(pvalue, errorMessage, false);
        if (dbname_options == NULL)
        {
          return NULL;
        }
      }
      break;
    }
    ++i;
  }

                                                
  options = conninfo_init(errorMessage);
  if (options == NULL)
  {
    PQconninfoFree(dbname_options);
    return NULL;
  }

                                        
  i = 0;
  while (keywords[i])
  {
    const char *pname = keywords[i];
    const char *pvalue = values[i];

    if (pvalue != NULL && pvalue[0] != '\0')
    {
                                       
      for (option = options; option->keyword != NULL; option++)
      {
        if (strcmp(option->keyword, pname) == 0)
        {
          break;
        }
      }

                                               
      if (option->keyword == NULL)
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("invalid connection option \"%s\"\n"), pname);
        PQconninfoFree(options);
        PQconninfoFree(dbname_options);
        return NULL;
      }

         
                                                                       
                                                                         
                                     
         
      if (strcmp(pname, "dbname") == 0 && dbname_options)
      {
        PQconninfoOption *str_option;

        for (str_option = dbname_options; str_option->keyword != NULL; str_option++)
        {
          if (str_option->val != NULL)
          {
            int k;

            for (k = 0; options[k].keyword; k++)
            {
              if (strcmp(options[k].keyword, str_option->keyword) == 0)
              {
                if (options[k].val)
                {
                  free(options[k].val);
                }
                options[k].val = strdup(str_option->val);
                if (!options[k].val)
                {
                  printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
                  PQconninfoFree(options);
                  PQconninfoFree(dbname_options);
                  return NULL;
                }
                break;
              }
            }
          }
        }

           
                                                                       
                                                   
           
        PQconninfoFree(dbname_options);
        dbname_options = NULL;
      }
      else
      {
           
                                                         
           
        if (option->val)
        {
          free(option->val);
        }
        option->val = strdup(pvalue);
        if (!option->val)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
          PQconninfoFree(options);
          PQconninfoFree(dbname_options);
          return NULL;
        }
      }
    }
    ++i;
  }
  PQconninfoFree(dbname_options);

     
                                               
     
  if (use_defaults)
  {
    if (!conninfo_add_defaults(options, errorMessage))
    {
      PQconninfoFree(options);
      return NULL;
    }
  }

  return options;
}

   
                                                                        
                  
   
                                                                          
   
                                                                           
                                                                           
                                                                          
         
   
static bool
conninfo_add_defaults(PQconninfoOption *options, PQExpBuffer errorMessage)
{
  PQconninfoOption *option;
  char *tmp;

     
                                                                          
                                                                            
                                                       
     
  if (parseServiceInfo(options, errorMessage) != 0 && errorMessage)
  {
    return false;
  }

     
                                                                             
                             
     
  for (option = options; option->keyword != NULL; option++)
  {
    if (option->val != NULL)
    {
      continue;                                       
    }

       
                                                    
       
    if (option->envvar != NULL)
    {
      if ((tmp = getenv(option->envvar)) != NULL)
      {
        option->val = strdup(tmp);
        if (!option->val)
        {
          if (errorMessage)
          {
            printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
          }
          return false;
        }
        continue;
      }
    }

       
                                                                        
                                                                         
                                                                          
                                                                      
       
    if (strcmp(option->keyword, "sslmode") == 0)
    {
      const char *requiresslenv = getenv("PGREQUIRESSL");

      if (requiresslenv != NULL && requiresslenv[0] == '1')
      {
        option->val = strdup("require");
        if (!option->val)
        {
          if (errorMessage)
          {
            printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
          }
          return false;
        }
        continue;
      }
    }

       
                                                                         
                           
       
    if (option->compiled != NULL)
    {
      option->val = strdup(option->compiled);
      if (!option->val)
      {
        if (errorMessage)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
        }
        return false;
      }
      continue;
    }

       
                                                                           
                                                                           
                                                                           
                                                                         
                                                              
       
    if (strcmp(option->keyword, "user") == 0)
    {
      option->val = pg_fe_getauthname(NULL);
      continue;
    }
  }

  return true;
}

   
                                          
   
                                      
   
static PQconninfoOption *
conninfo_uri_parse(const char *uri, PQExpBuffer errorMessage, bool use_defaults)
{
  PQconninfoOption *options;

                                                
  options = conninfo_init(errorMessage);
  if (options == NULL)
  {
    return NULL;
  }

  if (!conninfo_uri_parse_options(options, uri, errorMessage))
  {
    PQconninfoFree(options);
    return NULL;
  }

     
                                               
     
  if (use_defaults)
  {
    if (!conninfo_add_defaults(options, errorMessage))
    {
      PQconninfoFree(options);
      return NULL;
    }
  }

  return options;
}

   
                              
                       
   
                                                                             
                         
                                                                        
   
                                                                              
          
   
                                                                               
   
                                                                                
                                                                        
                                                      
   
                                                                                     
   
                                                          
   
static bool
conninfo_uri_parse_options(PQconninfoOption *options, const char *uri, PQExpBuffer errorMessage)
{
  int prefix_len;
  char *p;
  char *buf = NULL;
  char *start;
  char prevchar = '\0';
  char *user = NULL;
  char *host = NULL;
  bool retval = false;
  PQExpBufferData hostbuf;
  PQExpBufferData portbuf;

  initPQExpBuffer(&hostbuf);
  initPQExpBuffer(&portbuf);
  if (PQExpBufferDataBroken(hostbuf) || PQExpBufferDataBroken(portbuf))
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    goto cleanup;
  }

                                               
  buf = strdup(uri);
  if (buf == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    goto cleanup;
  }
  start = buf;

                           
  prefix_len = uri_prefix_length(uri);
  if (prefix_len == 0)
  {
                             
    printfPQExpBuffer(errorMessage, libpq_gettext("invalid URI propagated to internal parser routine: \"%s\"\n"), uri);
    goto cleanup;
  }
  start += prefix_len;
  p = start;

                                                           
  while (*p && *p != '@' && *p != '/')
  {
    ++p;
  }
  if (*p == '@')
  {
       
                                                                        
                                            
       
    user = start;

    p = user;
    while (*p != ':' && *p != '@')
    {
      ++p;
    }

                                                        
    prevchar = *p;
    *p = '\0';

    if (*user && !conninfo_storeval(options, "user", user, errorMessage, false, true))
    {
      goto cleanup;
    }

    if (prevchar == ':')
    {
      const char *password = p + 1;

      while (*p != '@')
      {
        ++p;
      }
      *p = '\0';

      if (*password && !conninfo_storeval(options, "password", password, errorMessage, false, true))
      {
        goto cleanup;
      }
    }

                                                                
    ++p;
  }
  else
  {
       
                                                                      
       
    p = start;
  }

     
                                                                             
                                                                  
                                                                            
                                                                     
                                                                           
                                               
     
  for (;;)
  {
       
                              
       
    if (*p == '[')
    {
      host = ++p;
      while (*p && *p != ']')
      {
        ++p;
      }
      if (!*p)
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("end of string reached when looking for matching \"]\" in IPv6 host address in URI: \"%s\"\n"), uri);
        goto cleanup;
      }
      if (p == host)
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("IPv6 host address may not be empty in URI: \"%s\"\n"), uri);
        goto cleanup;
      }

                                           
      *(p++) = '\0';

         
                                                                         
                                     
         
      if (*p && *p != ':' && *p != '/' && *p != '?' && *p != ',')
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("unexpected character \"%c\" at position %d in URI (expected \":\" or \"/\"): \"%s\"\n"), *p, (int)(p - buf + 1), uri);
        goto cleanup;
      }
    }
    else
    {
                                                         
      host = p;

         
                                                                  
                                                                     
         
      while (*p && *p != ':' && *p != '/' && *p != '?' && *p != ',')
      {
        ++p;
      }
    }

                                                        
    prevchar = *p;
    *p = '\0';

    appendPQExpBufferStr(&hostbuf, host);

    if (prevchar == ':')
    {
      const char *port = ++p;                                   

      while (*p && *p != '/' && *p != '?' && *p != ',')
      {
        ++p;
      }

      prevchar = *p;
      *p = '\0';

      appendPQExpBufferStr(&portbuf, port);
    }

    if (prevchar != ',')
    {
      break;
    }
    ++p;                                   
    appendPQExpBufferChar(&hostbuf, ',');
    appendPQExpBufferChar(&portbuf, ',');
  }

                                            
  if (PQExpBufferDataBroken(hostbuf) || PQExpBufferDataBroken(portbuf))
  {
    goto cleanup;
  }
  if (hostbuf.data[0] && !conninfo_storeval(options, "host", hostbuf.data, errorMessage, false, true))
  {
    goto cleanup;
  }
  if (portbuf.data[0] && !conninfo_storeval(options, "port", portbuf.data, errorMessage, false, true))
  {
    goto cleanup;
  }

  if (prevchar && prevchar != '?')
  {
    const char *dbname = ++p;                                   

                                   
    while (*p && *p != '?')
    {
      ++p;
    }

    prevchar = *p;
    *p = '\0';

       
                                                                         
                                                                           
                  
       
    if (*dbname && !conninfo_storeval(options, "dbname", dbname, errorMessage, false, true))
    {
      goto cleanup;
    }
  }

  if (prevchar)
  {
    ++p;                              

    if (!conninfo_uri_parse_params(p, options, errorMessage))
    {
      goto cleanup;
    }
  }

                              
  retval = true;

cleanup:
  termPQExpBuffer(&hostbuf);
  termPQExpBuffer(&portbuf);
  if (buf)
  {
    free(buf);
  }
  return retval;
}

   
                                            
   
                                                                       
                                                                               
   
                                           
   
static bool
conninfo_uri_parse_params(char *params, PQconninfoOption *connOptions, PQExpBuffer errorMessage)
{
  while (*params)
  {
    char *keyword = params;
    char *value = NULL;
    char *p = params;
    bool malloced = false;

       
                                                                          
                               
       
    for (;;)
    {
      if (*p == '=')
      {
                                    
        if (value != NULL)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("extra key/value separator \"=\" in URI query parameter: \"%s\"\n"), keyword);
          return false;
        }
                                               
        *p++ = '\0';
        value = p;
      }
      else if (*p == '&' || *p == '\0')
      {
           
                                                                 
                                                            
           
        if (*p != '\0')
        {
          *p++ = '\0';
        }
                                   
        if (value == NULL)
        {
          printfPQExpBuffer(errorMessage, libpq_gettext("missing key/value separator \"=\" in URI query parameter: \"%s\"\n"), keyword);
          return false;
        }
                                                     
        break;
      }
      else
      {
        ++p;                                    
      }
    }

    keyword = conninfo_uri_decode(keyword, errorMessage);
    if (keyword == NULL)
    {
                                                            
      return false;
    }
    value = conninfo_uri_decode(value, errorMessage);
    if (value == NULL)
    {
                                                            
      free(keyword);
      return false;
    }
    malloced = true;

       
                                                                 
       
    if (strcmp(keyword, "ssl") == 0 && strcmp(value, "true") == 0)
    {
      free(keyword);
      free(value);
      malloced = false;

      keyword = "sslmode";
      value = "require";
    }

       
                                                                  
                                                                
                    
       
    if (!conninfo_storeval(connOptions, keyword, value, errorMessage, true, false))
    {
                                                                        
      if (errorMessage->len == 0)
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("invalid URI query parameter: \"%s\"\n"), keyword);
      }
                     
      if (malloced)
      {
        free(keyword);
        free(value);
      }
      return false;
    }

    if (malloced)
    {
      free(keyword);
      free(value);
    }

                                                
    params = p;
  }

  return true;
}

   
                                  
   
                                                       
                                                                       
   
                                                                      
                                                                             
                                                                               
                                                                                
                        
   
static char *
conninfo_uri_decode(const char *str, PQExpBuffer errorMessage)
{
  char *buf;
  char *p;
  const char *q = str;

  buf = malloc(strlen(str) + 1);
  if (buf == NULL)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    return NULL;
  }
  p = buf;

  for (;;)
  {
    if (*q != '%')
    {
                                             
      if (!(*(p++) = *(q++)))
      {
        break;
      }
    }
    else
    {
      int hi;
      int lo;
      int c;

      ++q;                                   

         
                                                          
                                                                       
         
      if (!(get_hexdigit(*q++, &hi) && get_hexdigit(*q++, &lo)))
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("invalid percent-encoded token: \"%s\"\n"), str);
        free(buf);
        return NULL;
      }

      c = (hi << 4) | lo;
      if (c == 0)
      {
        printfPQExpBuffer(errorMessage, libpq_gettext("forbidden value %%00 in percent-encoded value: \"%s\"\n"), str);
        free(buf);
        return NULL;
      }
      *(p++) = c;
    }
  }

  return buf;
}

   
                                                             
   
                                                                               
                                     
   
                                                                           
   
static bool
get_hexdigit(char digit, int *value)
{
  if ('0' <= digit && digit <= '9')
  {
    *value = digit - '0';
  }
  else if ('A' <= digit && digit <= 'F')
  {
    *value = digit - 'A' + 10;
  }
  else if ('a' <= digit && digit <= 'f')
  {
    *value = digit - 'a' + 10;
  }
  else
  {
    return false;
  }

  return true;
}

   
                                                                               
   
                                                                         
                                    
   
static const char *
conninfo_getval(PQconninfoOption *connOptions, const char *keyword)
{
  PQconninfoOption *option;

  option = conninfo_find(connOptions, keyword);

  return option ? option->val : NULL;
}

   
                                                                     
                      
   
                                                                           
                                  
   
                                                                           
                                                                            
                                                                         
   
                                                                       
                                                                            
                                                               
   
static PQconninfoOption *
conninfo_storeval(PQconninfoOption *connOptions, const char *keyword, const char *value, PQExpBuffer errorMessage, bool ignoreMissing, bool uri_decode)
{
  PQconninfoOption *option;
  char *value_copy;

     
                                                                  
                                                                         
                                         
     
  if (strcmp(keyword, "requiressl") == 0)
  {
    keyword = "sslmode";
    if (value[0] == '1')
    {
      value = "require";
    }
    else
    {
      value = "prefer";
    }
  }

  option = conninfo_find(connOptions, keyword);
  if (option == NULL)
  {
    if (!ignoreMissing)
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("invalid connection option \"%s\"\n"), keyword);
    }
    return NULL;
  }

  if (uri_decode)
  {
    value_copy = conninfo_uri_decode(value, errorMessage);
    if (value_copy == NULL)
    {
                                                            
      return NULL;
    }
  }
  else
  {
    value_copy = strdup(value);
    if (value_copy == NULL)
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
      return NULL;
    }
  }

  if (option->val)
  {
    free(option->val);
  }
  option->val = value_copy;

  return option;
}

   
                                                                      
                      
   
                                                                          
              
                                    
   
static PQconninfoOption *
conninfo_find(PQconninfoOption *connOptions, const char *keyword)
{
  PQconninfoOption *option;

  for (option = connOptions; option->keyword != NULL; option++)
  {
    if (strcmp(option->keyword, keyword) == 0)
    {
      return option;
    }
  }

  return NULL;
}

   
                                                         
   
PQconninfoOption *
PQconninfo(PGconn *conn)
{
  PQExpBufferData errorBuf;
  PQconninfoOption *connOptions;

  if (conn == NULL)
  {
    return NULL;
  }

                                                                           
  initPQExpBuffer(&errorBuf);
  if (PQExpBufferDataBroken(errorBuf))
  {
    return NULL;                                
  }

  connOptions = conninfo_init(&errorBuf);

  if (connOptions != NULL)
  {
    const internalPQconninfoOption *option;

    for (option = PQconninfoOptions; option->keyword; option++)
    {
      char **connmember;

      if (option->connofs < 0)
      {
        continue;
      }

      connmember = (char **)((char *)conn + option->connofs);

      if (*connmember)
      {
        conninfo_storeval(connOptions, option->keyword, *connmember, &errorBuf, true, false);
      }
    }
  }

  termPQExpBuffer(&errorBuf);

  return connOptions;
}

void
PQconninfoFree(PQconninfoOption *connOptions)
{
  PQconninfoOption *option;

  if (connOptions == NULL)
  {
    return;
  }

  for (option = connOptions; option->keyword != NULL; option++)
  {
    if (option->val != NULL)
    {
      free(option->val);
    }
  }
  free(connOptions);
}

                                                         
char *
PQdb(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }
  return conn->dbName;
}

char *
PQuser(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }
  return conn->pguser;
}

char *
PQpass(const PGconn *conn)
{
  char *password = NULL;

  if (!conn)
  {
    return NULL;
  }
  if (conn->connhost != NULL)
  {
    password = conn->connhost[conn->whichhost].password;
  }
  if (password == NULL)
  {
    password = conn->pgpass;
  }
                                                                         
  if (password == NULL)
  {
    password = "";
  }
  return password;
}

char *
PQhost(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }

  if (conn->connhost != NULL)
  {
       
                                                                           
             
       
    if (conn->connhost[conn->whichhost].host != NULL && conn->connhost[conn->whichhost].host[0] != '\0')
    {
      return conn->connhost[conn->whichhost].host;
    }
    else if (conn->connhost[conn->whichhost].hostaddr != NULL && conn->connhost[conn->whichhost].hostaddr[0] != '\0')
    {
      return conn->connhost[conn->whichhost].hostaddr;
    }
  }

  return "";
}

char *
PQhostaddr(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }

                                    
  if (conn->connhost != NULL && conn->connip != NULL)
  {
    return conn->connip;
  }

  return "";
}

char *
PQport(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }

  if (conn->connhost != NULL)
  {
    return conn->connhost[conn->whichhost].port;
  }

  return "";
}

char *
PQtty(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }
  return conn->pgtty;
}

char *
PQoptions(const PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }
  return conn->pgoptions;
}

ConnStatusType
PQstatus(const PGconn *conn)
{
  if (!conn)
  {
    return CONNECTION_BAD;
  }
  return conn->status;
}

PGTransactionStatusType
PQtransactionStatus(const PGconn *conn)
{
  if (!conn || conn->status != CONNECTION_OK)
  {
    return PQTRANS_UNKNOWN;
  }
  if (conn->asyncStatus != PGASYNC_IDLE)
  {
    return PQTRANS_ACTIVE;
  }
  return conn->xactStatus;
}

const char *
PQparameterStatus(const PGconn *conn, const char *paramName)
{
  const pgParameterStatus *pstatus;

  if (!conn || !paramName)
  {
    return NULL;
  }
  for (pstatus = conn->pstatus; pstatus != NULL; pstatus = pstatus->next)
  {
    if (strcmp(pstatus->name, paramName) == 0)
    {
      return pstatus->value;
    }
  }
  return NULL;
}

int
PQprotocolVersion(const PGconn *conn)
{
  if (!conn)
  {
    return 0;
  }
  if (conn->status == CONNECTION_BAD)
  {
    return 0;
  }
  return PG_PROTOCOL_MAJOR(conn->pversion);
}

int
PQserverVersion(const PGconn *conn)
{
  if (!conn)
  {
    return 0;
  }
  if (conn->status == CONNECTION_BAD)
  {
    return 0;
  }
  return conn->sversion;
}

char *
PQerrorMessage(const PGconn *conn)
{
  if (!conn)
  {
    return libpq_gettext("connection pointer is NULL\n");
  }

  return conn->errorMessage.data;
}

   
                                                                       
                                                                            
                                                                         
                                                                            
                                                   
                                                                                      
                                                                                                  
   
int
PQsocket(const PGconn *conn)
{
  if (!conn)
  {
    return -1;
  }
  return (conn->sock != PGINVALID_SOCKET) ? conn->sock : -1;
}

int
PQbackendPID(const PGconn *conn)
{
  if (!conn || conn->status != CONNECTION_OK)
  {
    return 0;
  }
  return conn->be_pid;
}

int
PQconnectionNeedsPassword(const PGconn *conn)
{
  char *password;

  if (!conn)
  {
    return false;
  }
  password = PQpass(conn);
  if (conn->password_needed && (password == NULL || password[0] == '\0'))
  {
    return true;
  }
  else
  {
    return false;
  }
}

int
PQconnectionUsedPassword(const PGconn *conn)
{
  if (!conn)
  {
    return false;
  }
  if (conn->password_needed)
  {
    return true;
  }
  else
  {
    return false;
  }
}

int
PQclientEncoding(const PGconn *conn)
{
  if (!conn || conn->status != CONNECTION_OK)
  {
    return -1;
  }
  return conn->client_encoding;
}

int
PQsetClientEncoding(PGconn *conn, const char *encoding)
{
  char qbuf[128];
  static const char query[] = "set client_encoding to '%s'";
  PGresult *res;
  int status;

  if (!conn || conn->status != CONNECTION_OK)
  {
    return -1;
  }

  if (!encoding)
  {
    return -1;
  }

                                                    
  if (strcmp(encoding, "auto") == 0)
  {
    encoding = pg_encoding_to_char(pg_get_encoding_from_locale(NULL, true));
  }

                                   
  if (sizeof(qbuf) < (sizeof(query) + strlen(encoding)))
  {
    return -1;
  }

                            
  sprintf(qbuf, query, encoding);
  res = PQexec(conn, qbuf);

  if (res == NULL)
  {
    return -1;
  }
  if (res->resultStatus != PGRES_COMMAND_OK)
  {
    status = -1;
  }
  else
  {
       
                                                                          
                                                                       
                                                                        
                  
       
    if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
    {
      pqSaveParameterStatus(conn, "client_encoding", encoding);
    }
    status = 0;                       
  }
  PQclear(res);
  return status;
}

PGVerbosity
PQsetErrorVerbosity(PGconn *conn, PGVerbosity verbosity)
{
  PGVerbosity old;

  if (!conn)
  {
    return PQERRORS_DEFAULT;
  }
  old = conn->verbosity;
  conn->verbosity = verbosity;
  return old;
}

PGContextVisibility
PQsetErrorContextVisibility(PGconn *conn, PGContextVisibility show_context)
{
  PGContextVisibility old;

  if (!conn)
  {
    return PQSHOW_CONTEXT_ERRORS;
  }
  old = conn->show_context;
  conn->show_context = show_context;
  return old;
}

void
PQtrace(PGconn *conn, FILE *debug_port)
{
  if (conn == NULL)
  {
    return;
  }
  PQuntrace(conn);
  conn->Pfdebug = debug_port;
}

void
PQuntrace(PGconn *conn)
{
  if (conn == NULL)
  {
    return;
  }
  if (conn->Pfdebug)
  {
    fflush(conn->Pfdebug);
    conn->Pfdebug = NULL;
  }
}

PQnoticeReceiver
PQsetNoticeReceiver(PGconn *conn, PQnoticeReceiver proc, void *arg)
{
  PQnoticeReceiver old;

  if (conn == NULL)
  {
    return NULL;
  }

  old = conn->noticeHooks.noticeRec;
  if (proc)
  {
    conn->noticeHooks.noticeRec = proc;
    conn->noticeHooks.noticeRecArg = arg;
  }
  return old;
}

PQnoticeProcessor
PQsetNoticeProcessor(PGconn *conn, PQnoticeProcessor proc, void *arg)
{
  PQnoticeProcessor old;

  if (conn == NULL)
  {
    return NULL;
  }

  old = conn->noticeHooks.noticeProc;
  if (proc)
  {
    conn->noticeHooks.noticeProc = proc;
    conn->noticeHooks.noticeProcArg = arg;
  }
  return old;
}

   
                                                                          
                                                                      
                                                                          
                         
   
static void
defaultNoticeReceiver(void *arg, const PGresult *res)
{
  (void)arg;               
  if (res->noticeHooks.noticeProc != NULL)
  {
    res->noticeHooks.noticeProc(res->noticeHooks.noticeProcArg, PQresultErrorMessage(res));
  }
}

   
                                                        
                                                              
                                                              
                                                               
   
static void
defaultNoticeProcessor(void *arg, const char *message)
{
  (void)arg;               
                                                                          
  fprintf(stderr, "%s", message);
}

   
                                                              
                       
   
static char *
pwdfMatchesString(char *buf, const char *token)
{
  char *tbuf;
  const char *ttok;
  bool bslash = false;

  if (buf == NULL || token == NULL)
  {
    return NULL;
  }
  tbuf = buf;
  ttok = token;
  if (tbuf[0] == '*' && tbuf[1] == ':')
  {
    return tbuf + 2;
  }
  while (*tbuf != 0)
  {
    if (*tbuf == '\\' && !bslash)
    {
      tbuf++;
      bslash = true;
    }
    if (*tbuf == ':' && *ttok == 0 && !bslash)
    {
      return tbuf + 1;
    }
    bslash = false;
    if (*ttok == 0)
    {
      return NULL;
    }
    if (*tbuf == *ttok)
    {
      tbuf++;
      ttok++;
    }
    else
    {
      return NULL;
    }
  }
  return NULL;
}

                                                                      
static char *
passwordFromFile(const char *hostname, const char *port, const char *dbname, const char *username, const char *pgpassfile)
{
  FILE *fp;
  struct stat stat_buf;
  PQExpBufferData buf;

  if (dbname == NULL || dbname[0] == '\0')
  {
    return NULL;
  }

  if (username == NULL || username[0] == '\0')
  {
    return NULL;
  }

                                                                        
  if (hostname == NULL || hostname[0] == '\0')
  {
    hostname = DefaultHost;
  }
  else if (is_absolute_path(hostname))
  {

       
                                                                       
                                                              
       
    if (strcmp(hostname, DEFAULT_PGSOCKET_DIR) == 0)
    {
      hostname = DefaultHost;
    }
  }

  if (port == NULL || port[0] == '\0')
  {
    port = DEF_PGPORT_STR;
  }

                                                     
  if (stat(pgpassfile, &stat_buf) != 0)
  {
    return NULL;
  }

#ifndef WIN32
  if (!S_ISREG(stat_buf.st_mode))
  {
    fprintf(stderr, libpq_gettext("WARNING: password file \"%s\" is not a plain file\n"), pgpassfile);
    return NULL;
  }

                                                                   
  if (stat_buf.st_mode & (S_IRWXG | S_IRWXO))
  {
    fprintf(stderr, libpq_gettext("WARNING: password file \"%s\" has group or world access; permissions should be u=rw (0600) or less\n"), pgpassfile);
    return NULL;
  }
#else

     
                                                                         
           
     
#endif

  fp = fopen(pgpassfile, "r");
  if (fp == NULL)
  {
    return NULL;
  }

                                                                          
  initPQExpBuffer(&buf);

  while (!feof(fp) && !ferror(fp))
  {
                                                                     
    if (!enlargePQExpBuffer(&buf, 128))
    {
      break;
    }

                                                              
    if (fgets(buf.data + buf.len, buf.maxlen - buf.len, fp) == NULL)
    {
      break;
    }
    buf.len += strlen(buf.data + buf.len);

                                                                     
    if (!(buf.len > 0 && buf.data[buf.len - 1] == '\n') && !feof(fp))
    {
      continue;
    }

                         
    if (buf.data[0] != '#')
    {
      char *t = buf.data;
      int len = buf.len;

                                   
      if (len > 0 && t[len - 1] == '\n')
      {
        t[--len] = '\0';
                                                
        if (len > 0 && t[len - 1] == '\r')
        {
          t[--len] = '\0';
        }
      }

      if (len > 0 && (t = pwdfMatchesString(t, hostname)) != NULL && (t = pwdfMatchesString(t, port)) != NULL && (t = pwdfMatchesString(t, dbname)) != NULL && (t = pwdfMatchesString(t, username)) != NULL)
      {
                            
        char *ret, *p1, *p2;

        ret = strdup(t);

        fclose(fp);
        termPQExpBuffer(&buf);

        if (!ret)
        {
                                                                   
          return NULL;
        }

                                 
        for (p1 = p2 = ret; *p1 != ':' && *p1 != '\0'; ++p1, ++p2)
        {
          if (*p1 == '\\' && p1[1] != '\0')
          {
            ++p1;
          }
          *p2 = *p1;
        }
        *p2 = '\0';

        return ret;
      }
    }

                                                          
    buf.len = 0;
  }

  fclose(fp);
  termPQExpBuffer(&buf);
  return NULL;
}

   
                                                                   
                                               
   
static void
pgpassfileWarning(PGconn *conn)
{
                                                                 
                                      
  if (conn->password_needed && conn->connhost[conn->whichhost].password != NULL && conn->result)
  {
    const char *sqlstate = PQresultErrorField(conn->result, PG_DIAG_SQLSTATE);

    if (sqlstate && strcmp(sqlstate, ERRCODE_INVALID_PASSWORD) == 0)
    {
      appendPQExpBuffer(&conn->errorMessage, libpq_gettext("password retrieved from file \"%s\"\n"), conn->pgpassfile);
    }
  }
}

   
                                                        
   
                                                                         
                                                               
   
                                                                          
                                                                            
               
   
                                                                           
   
                                                                               
                                                                          
                                                                              
                                                                            
                           
   
bool
pqGetHomeDirectory(char *buf, int bufsize)
{
#ifndef WIN32
  char pwdbuf[BUFSIZ];
  struct passwd pwdstr;
  struct passwd *pwd = NULL;

  (void)pqGetpwuid(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd);
  if (pwd == NULL)
  {
    return false;
  }
  strlcpy(buf, pwd->pw_dir, bufsize);
  return true;
#else
  char tmppath[MAX_PATH];

  ZeroMemory(tmppath, sizeof(tmppath));
  if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, tmppath) != S_OK)
  {
    return false;
  }
  snprintf(buf, bufsize, "%s/postgresql", tmppath);
  return true;
#endif
}

   
                                                                           
                             
   

static void
default_threadlock(int acquire)
{
#ifdef ENABLE_THREAD_SAFETY
#ifndef WIN32
  static pthread_mutex_t singlethread_lock = PTHREAD_MUTEX_INITIALIZER;
#else
  static pthread_mutex_t singlethread_lock = NULL;
  static long mutex_initlock = 0;

  if (singlethread_lock == NULL)
  {
    while (InterlockedExchange(&mutex_initlock, 1) == 1)
                                             ;
    if (singlethread_lock == NULL)
    {
      if (pthread_mutex_init(&singlethread_lock, NULL))
      {
        PGTHREAD_ERROR("failed to initialize mutex");
      }
    }
    InterlockedExchange(&mutex_initlock, 0);
  }
#endif
  if (acquire)
  {
    if (pthread_mutex_lock(&singlethread_lock))
    {
      PGTHREAD_ERROR("failed to lock mutex");
    }
  }
  else
  {
    if (pthread_mutex_unlock(&singlethread_lock))
    {
      PGTHREAD_ERROR("failed to unlock mutex");
    }
  }
#endif
}

pgthreadlock_t
PQregisterThreadLock(pgthreadlock_t newhandler)
{
  pgthreadlock_t prev = pg_g_threadlock;

  if (newhandler)
  {
    pg_g_threadlock = newhandler;
  }
  else
  {
    pg_g_threadlock = default_threadlock;
  }

  return prev;
}
