                                                                            
   
          
                                               
   
                                                                         
                                                                        
   
   
                  
                              
   
                                                                            
   

#include "postgres.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "commands/user.h"
#include "common/ip.h"
#include "common/md5.h"
#include "common/scram-common.h"
#include "libpq/auth.h"
#include "libpq/crypt.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/scram.h"
#include "miscadmin.h"
#include "port/pg_bswap.h"
#include "replication/walsender.h"
#include "storage/ipc.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"

                                                                   
                                   
                                                                   
   
static void
sendAuthRequest(Port *port, AuthRequest areq, const char *extradata, int extralen);
static void
auth_failed(Port *port, int status, char *logdetail);
static char *
recv_password_packet(Port *port);

                                                                   
                                                                            
                                                                   
   
static int
CheckPasswordAuth(Port *port, char **logdetail);
static int
CheckPWChallengeAuth(Port *port, char **logdetail);

static int
CheckMD5Auth(Port *port, char *shadow_pass, char **logdetail);
static int
CheckSCRAMAuth(Port *port, char *shadow_pass, char **logdetail);

                                                                   
                        
                                                                   
   
                                                  
#define IDENT_USERNAME_MAX 512

                                                                   
#define IDENT_PORT 113

static int
ident_inet(hbaPort *port);

#ifdef HAVE_UNIX_SOCKETS
static int
auth_peer(hbaPort *port);
#endif

                                                                   
                      
                                                                   
   
#ifdef USE_PAM
#ifdef HAVE_PAM_PAM_APPL_H
#include <pam/pam_appl.h>
#endif
#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif

#define PGSQL_PAM_SERVICE "postgresql"                                 

static int
CheckPAMAuth(Port *port, const char *user, const char *password);
static int
pam_passwd_conv_proc(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr);

static struct pam_conv pam_passw_conv = {&pam_passwd_conv_proc, NULL};

static const char *pam_passwd = NULL;                               
                                                      
static Port *pam_port_cludge;                                                     
                                                                
static bool pam_no_password;                                               
#endif                                             

                                                                   
                      
                                                                   
   
#ifdef USE_BSD_AUTH
#include <bsd_auth.h>

static int
CheckBSDAuth(Port *port, char *user);
#endif                   

                                                                   
                       
                                                                   
   
#ifdef USE_LDAP
#ifndef WIN32
                                                                          
#define LDAP_DEPRECATED 1
#include <ldap.h>
#else
#include <winldap.h>

                                          
typedef ULONG (*__ldap_start_tls_sA)(IN PLDAP ExternalHandle, OUT PULONG ServerReturnValue, OUT LDAPMessage **result, IN PLDAPControlA *ServerControls, IN PLDAPControlA *ClientControls);
#endif

static int
CheckLDAPAuth(Port *port);

                                                       
#ifndef LDAP_OPT_DIAGNOSTIC_MESSAGE
#define LDAP_OPT_DIAGNOSTIC_MESSAGE LDAP_OPT_ERROR_STRING
#endif

#endif               

                                                                   
                       
                                                                   
   
#ifdef USE_SSL
static int
CheckCertAuth(Port *port);
#endif

                                                                   
                            
                                                                   
   
char *pg_krb_server_keyfile;
bool pg_krb_caseins_users;

                                                                   
                         
                                                                   
   
#ifdef ENABLE_GSS
#include "libpq/be-gssapi-common.h"

static int
pg_GSS_checkauth(Port *port);
static int
pg_GSS_recvauth(Port *port);
#endif                 

                                                                   
                       
                                                                   
   
#ifdef ENABLE_SSPI
typedef SECURITY_STATUS(WINAPI *QUERY_SECURITY_CONTEXT_TOKEN_FN)(PCtxtHandle, void **);
static int
pg_SSPI_recvauth(Port *port);
static int
pg_SSPI_make_upn(char *accountname, size_t accountnamesize, char *domainname, size_t domainnamesize, bool update_accountname);
#endif

                                                                   
                         
                                                                   
   
static int
CheckRADIUSAuth(Port *port);
static int
PerformRadiusTransaction(const char *server, const char *secret, const char *portstr, const char *identifier, const char *user_name, const char *passwd);

   
                                                                
   
                                                                            
                                                                            
                                                                              
                                                                            
                                                                             
                                                                        
                                                                         
                                                                         
                                                                      
   
#define PG_MAX_AUTH_TOKEN_LENGTH 65535

   
                                           
   
                                                                              
                           
   
#define PG_MAX_SASL_MESSAGE_LENGTH 1024

                                                                   
                                   
                                                                   
   

   
                                                                            
                                                                              
                                                                            
   
ClientAuthentication_hook_type ClientAuthentication_hook = NULL;

   
                                                                      
   
                                                                      
                                                                             
                                                               
                                                                       
                                                                          
           
                                                                        
                                                                    
                                                                        
   
static void
auth_failed(Port *port, int status, char *logdetail)
{
  const char *errstr;
  char *cdetail;
  int errcode_return = ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION;

     
                                                                         
                                                                           
                                                                       
                                                                          
                                                                            
                                                                          
                                                                            
              
     
  if (status == STATUS_EOF)
  {
    proc_exit(0);
  }

  switch (port->hba->auth_method)
  {
  case uaReject:
  case uaImplicitReject:
    errstr = gettext_noop("authentication failed for user \"%s\": host rejected");
    break;
  case uaTrust:
    errstr = gettext_noop("\"trust\" authentication failed for user \"%s\"");
    break;
  case uaIdent:
    errstr = gettext_noop("Ident authentication failed for user \"%s\"");
    break;
  case uaPeer:
    errstr = gettext_noop("Peer authentication failed for user \"%s\"");
    break;
  case uaPassword:
  case uaMD5:
  case uaSCRAM:
    errstr = gettext_noop("password authentication failed for user \"%s\"");
                                                             
    errcode_return = ERRCODE_INVALID_PASSWORD;
    break;
  case uaGSS:
    errstr = gettext_noop("GSSAPI authentication failed for user \"%s\"");
    break;
  case uaSSPI:
    errstr = gettext_noop("SSPI authentication failed for user \"%s\"");
    break;
  case uaPAM:
    errstr = gettext_noop("PAM authentication failed for user \"%s\"");
    break;
  case uaBSD:
    errstr = gettext_noop("BSD authentication failed for user \"%s\"");
    break;
  case uaLDAP:
    errstr = gettext_noop("LDAP authentication failed for user \"%s\"");
    break;
  case uaCert:
    errstr = gettext_noop("certificate authentication failed for user \"%s\"");
    break;
  case uaRADIUS:
    errstr = gettext_noop("RADIUS authentication failed for user \"%s\"");
    break;
  default:
    errstr = gettext_noop("authentication failed for user \"%s\": invalid authentication method");
    break;
  }

  cdetail = psprintf(_("Connection matched pg_hba.conf line %d: \"%s\""), port->hba->linenumber, port->hba->rawline);
  if (logdetail)
  {
    logdetail = psprintf("%s\n%s", logdetail, cdetail);
  }
  else
  {
    logdetail = cdetail;
  }

  ereport(FATAL, (errcode(errcode_return), errmsg(errstr, port->user_name), logdetail ? errdetail_log("%s", logdetail) : 0));

                      
}

   
                                                                  
                                                                   
   
void
ClientAuthentication(Port *port)
{
  int status = STATUS_ERROR;
  char *logdetail = NULL;

     
                                                                     
                                                                          
                                                                           
                                                           
     
  hba_getauthmethod(port);

  CHECK_FOR_INTERRUPTS();

     
                                                                            
                                                                       
                                                                         
     
  if (port->hba->clientcert != clientCertOff)
  {
                                                             
    if (!secure_loaded_verify_locations())
    {
      ereport(FATAL, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("client certificates can only be checked if a root certificate store is available")));
    }

       
                                                                      
                                                                         
                                                                     
                                       
       
    if (!port->peer_cert_valid)
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("connection requires a valid client certificate")));
    }
  }

     
                                                       
     
  switch (port->hba->auth_method)
  {
  case uaReject:

       
                                                                       
                                                                
                                                                    
                                                                       
                                                                    
                                                                    
                                                                  
                                                 
       
    {
      char hostinfo[NI_MAXHOST];
      const char *encryption_state;

      pg_getnameinfo_all(&port->raddr.addr, port->raddr.salen, hostinfo, sizeof(hostinfo), NULL, 0, NI_NUMERICHOST);

      encryption_state =
#ifdef ENABLE_GSS
          (port->gss && port->gss->enc) ? _("GSS encryption") :
#endif
#ifdef USE_SSL
          port->ssl_in_use ? _("SSL on")
                           :
#endif
                           _("SSL off");

      if (am_walsender)
      {
        ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION),
                                                                               
                           errmsg("pg_hba.conf rejects replication connection for host \"%s\", user \"%s\", %s", hostinfo, port->user_name, encryption_state)));
      }
      else
      {
        ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION),
                                                                               
                           errmsg("pg_hba.conf rejects connection for host \"%s\", user \"%s\", database \"%s\", %s", hostinfo, port->user_name, port->database_name, encryption_state)));
      }
      break;
    }

  case uaImplicitReject:

       
                                                            
       
                                                                    
                                                                  
                                                                     
                          
       
    {
      char hostinfo[NI_MAXHOST];
      const char *encryption_state;

      pg_getnameinfo_all(&port->raddr.addr, port->raddr.salen, hostinfo, sizeof(hostinfo), NULL, 0, NI_NUMERICHOST);

      encryption_state =
#ifdef ENABLE_GSS
          (port->gss && port->gss->enc) ? _("GSS encryption") :
#endif
#ifdef USE_SSL
          port->ssl_in_use ? _("SSL on")
                           :
#endif
                           _("SSL off");

#define HOSTNAME_LOOKUP_DETAIL(port) (port->remote_hostname ? (port->remote_hostname_resolv == +1 ? errdetail_log("Client IP address resolved to \"%s\", forward lookup matches.", port->remote_hostname) : port->remote_hostname_resolv == 0 ? errdetail_log("Client IP address resolved to \"%s\", forward lookup not checked.", port->remote_hostname) : port->remote_hostname_resolv == -1 ? errdetail_log("Client IP address resolved to \"%s\", forward lookup does not match.", port->remote_hostname) : port->remote_hostname_resolv == -2 ? errdetail_log("Could not translate client host name \"%s\" to IP address: %s.", port->remote_hostname, gai_strerror(port->remote_hostname_errcode)) : 0) : (port->remote_hostname_resolv == -2 ? errdetail_log("Could not resolve client IP address to a host name: %s.", gai_strerror(port->remote_hostname_errcode)) : 0))

      if (am_walsender)
      {
        ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION),
                                                                               
                           errmsg("no pg_hba.conf entry for replication connection from host \"%s\", user \"%s\", %s", hostinfo, port->user_name, encryption_state), HOSTNAME_LOOKUP_DETAIL(port)));
      }
      else
      {
        ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION),
                                                                               
                           errmsg("no pg_hba.conf entry for host \"%s\", user \"%s\", database \"%s\", %s", hostinfo, port->user_name, port->database_name, encryption_state), HOSTNAME_LOOKUP_DETAIL(port)));
      }
      break;
    }

  case uaGSS:
#ifdef ENABLE_GSS
                                                              
    if (port->gss == NULL)
    {
      port->gss = (pg_gssinfo *)MemoryContextAllocZero(TopMemoryContext, sizeof(pg_gssinfo));
    }
    port->gss->auth = true;

       
                                                                      
                                                             
       
    if (port->gss->enc)
    {
      status = pg_GSS_checkauth(port);
    }
    else
    {
      sendAuthRequest(port, AUTH_REQ_GSS, NULL, 0);
      status = pg_GSS_recvauth(port);
    }
#else
    Assert(false);
#endif
    break;

  case uaSSPI:
#ifdef ENABLE_SSPI
    if (port->gss == NULL)
    {
      port->gss = (pg_gssinfo *)MemoryContextAllocZero(TopMemoryContext, sizeof(pg_gssinfo));
    }
    sendAuthRequest(port, AUTH_REQ_SSPI, NULL, 0);
    status = pg_SSPI_recvauth(port);
#else
    Assert(false);
#endif
    break;

  case uaPeer:
#ifdef HAVE_UNIX_SOCKETS
    status = auth_peer(port);
#else
    Assert(false);
#endif
    break;

  case uaIdent:
    status = ident_inet(port);
    break;

  case uaMD5:
  case uaSCRAM:
    status = CheckPWChallengeAuth(port, &logdetail);
    break;

  case uaPassword:
    status = CheckPasswordAuth(port, &logdetail);
    break;

  case uaPAM:
#ifdef USE_PAM
    status = CheckPAMAuth(port, port->user_name, "");
#else
    Assert(false);
#endif              
    break;

  case uaBSD:
#ifdef USE_BSD_AUTH
    status = CheckBSDAuth(port, port->user_name);
#else
    Assert(false);
#endif                   
    break;

  case uaLDAP:
#ifdef USE_LDAP
    status = CheckLDAPAuth(port);
#else
    Assert(false);
#endif
    break;
  case uaRADIUS:
    status = CheckRADIUSAuth(port);
    break;
  case uaCert:
                                                                       
  case uaTrust:
    status = STATUS_OK;
    break;
  }

  if ((status == STATUS_OK && port->hba->clientcert == clientCertFull) || port->hba->auth_method == uaCert)
  {
       
                                                                         
                              
       
#ifdef USE_SSL
    status = CheckCertAuth(port);
#else
    Assert(false);
#endif
  }

  if (ClientAuthentication_hook)
  {
    (*ClientAuthentication_hook)(port, status);
  }

  if (status == STATUS_OK)
  {
    sendAuthRequest(port, AUTH_REQ_OK, NULL, 0);
  }
  else
  {
    auth_failed(port, status, logdetail);
  }
}

   
                                                          
   
static void
sendAuthRequest(Port *port, AuthRequest areq, const char *extradata, int extralen)
{
  StringInfoData buf;

  CHECK_FOR_INTERRUPTS();

  pq_beginmessage(&buf, 'R');
  pq_sendint32(&buf, (int32)areq);
  if (extralen > 0)
  {
    pq_sendbytes(&buf, extradata, extralen);
  }

  pq_endmessage(&buf);

     
                                                                     
                                                                      
              
     
  if (areq != AUTH_REQ_OK && areq != AUTH_REQ_SASL_FIN)
  {
    pq_flush();
  }

  CHECK_FOR_INTERRUPTS();
}

   
                                                   
   
                                                                
   
static char *
recv_password_packet(Port *port)
{
  StringInfoData buf;

  pq_startmsgread();
  if (PG_PROTOCOL_MAJOR(port->proto) >= 3)
  {
                                 
    int mtype;

    mtype = pq_getbyte();
    if (mtype != 'p')
    {
         
                                                                     
                                                                         
                                                                      
              
         
      if (mtype != EOF)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("expected password response, got message type %d", mtype)));
      }
      return NULL;                              
    }
  }
  else
  {
                                                                      
    if (pq_peekbyte() == EOF)
    {
      return NULL;          
    }
  }

  initStringInfo(&buf);
  if (pq_getmessage(&buf, 1000))                       
  {
                                                               
    pfree(buf.data);
    return NULL;
  }

     
                                                                            
                                                                   
                                                        
     
  if (strlen(buf.data) + 1 != buf.len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid password packet size")));
  }

     
                                                                            
                                                                          
                                                       
     
                                                                         
                                                                           
                                                                             
                                                                         
                                                                        
                                                                        
                                        
     
  if (buf.len == 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PASSWORD), errmsg("empty password returned by client")));
  }

                                                   
  elog(DEBUG5, "received password packet");

     
                                                                   
                                                                          
                                             
     
  return buf.data;
}

                                                                   
                                            
                                                                   
   

   
                                      
   
static int
CheckPasswordAuth(Port *port, char **logdetail)
{
  char *passwd;
  int result;
  char *shadow_pass;

  sendAuthRequest(port, AUTH_REQ_PASSWORD, NULL, 0);

  passwd = recv_password_packet(port);
  if (passwd == NULL)
  {
    return STATUS_EOF;                                    
  }

  shadow_pass = get_role_password(port->user_name, logdetail);
  if (shadow_pass)
  {
    result = plain_crypt_verify(port->user_name, shadow_pass, passwd, logdetail);
  }
  else
  {
    result = STATUS_ERROR;
  }

  if (shadow_pass)
  {
    pfree(shadow_pass);
  }
  pfree(passwd);

  return result;
}

   
                                 
   
static int
CheckPWChallengeAuth(Port *port, char **logdetail)
{
  int auth_result;
  char *shadow_pass;
  PasswordType pwtype;

  Assert(port->hba->auth_method == uaSCRAM || port->hba->auth_method == uaMD5);

                                          
  shadow_pass = get_role_password(port->user_name, logdetail);

     
                                                                        
                                                                           
                                                                            
                                                                             
                                                                       
                                                                             
                                                            
     
  if (!shadow_pass)
  {
    pwtype = Password_encryption;
  }
  else
  {
    pwtype = get_password_type(shadow_pass);
  }

     
                                                                            
                                                                           
                                                                             
                                                      
     
                                                                          
                                                      
     
  if (port->hba->auth_method == uaMD5 && pwtype == PASSWORD_TYPE_MD5)
  {
    auth_result = CheckMD5Auth(port, shadow_pass, logdetail);
  }
  else
  {
    auth_result = CheckSCRAMAuth(port, shadow_pass, logdetail);
  }

  if (shadow_pass)
  {
    pfree(shadow_pass);
  }

     
                                                                      
                               
     
  if (!shadow_pass)
  {
    Assert(auth_result != STATUS_OK);
    return STATUS_ERROR;
  }
  return auth_result;
}

static int
CheckMD5Auth(Port *port, char *shadow_pass, char **logdetail)
{
  char md5Salt[4];                    
  char *passwd;
  int result;

  if (Db_user_namespace)
  {
    ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("MD5 authentication is not supported when \"db_user_namespace\" is enabled")));
  }

                                                          
  if (!pg_strong_random(md5Salt, 4))
  {
    ereport(LOG, (errmsg("could not generate random MD5 salt")));
    return STATUS_ERROR;
  }

  sendAuthRequest(port, AUTH_REQ_MD5, md5Salt, 4);

  passwd = recv_password_packet(port);
  if (passwd == NULL)
  {
    return STATUS_EOF;                                    
  }

  if (shadow_pass)
  {
    result = md5_crypt_verify(port->user_name, shadow_pass, passwd, md5Salt, 4, logdetail);
  }
  else
  {
    result = STATUS_ERROR;
  }

  pfree(passwd);

  return result;
}

static int
CheckSCRAMAuth(Port *port, char *shadow_pass, char **logdetail)
{
  StringInfoData sasl_mechs;
  int mtype;
  StringInfoData buf;
  void *scram_opaq = NULL;
  char *output = NULL;
  int outputlen = 0;
  const char *input;
  int inputlen;
  int result;
  bool initial;

     
                                                                           
                                                                             
                                                                           
                                                                      
                                                                        
                                
     
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("SASL authentication is not supported in protocol version 2")));
  }

     
                                                                            
                                                   
     
  initStringInfo(&sasl_mechs);

  pg_be_scram_get_mechanisms(port, &sasl_mechs);
                                                       
  appendStringInfoChar(&sasl_mechs, '\0');

  sendAuthRequest(port, AUTH_REQ_SASL, sasl_mechs.data, sasl_mechs.len);
  pfree(sasl_mechs.data);

     
                                                                       
                                                                         
                                                                       
                         
     
  initial = true;
  do
  {
    pq_startmsgread();
    mtype = pq_getbyte();
    if (mtype != 'p')
    {
                                                       
      if (mtype != EOF)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("expected SASL response, got message type %d", mtype)));
      }
      else
      {
        return STATUS_EOF;
      }
    }

                                     
    initStringInfo(&buf);
    if (pq_getmessage(&buf, PG_MAX_SASL_MESSAGE_LENGTH))
    {
                                                    
      pfree(buf.data);
      return STATUS_ERROR;
    }

    elog(DEBUG4, "Processing received SASL response of length %d", buf.len);

       
                                                                           
                                                                           
                                                                    
                                                            
       
    if (initial)
    {
      const char *selected_mech;

      selected_mech = pq_getmsgrawstring(&buf);

         
                                                              
         
                                                                         
                                                               
                                                                     
                                                                     
                      
         
                                                                     
                                                                     
         
      scram_opaq = pg_be_scram_init(port, selected_mech, shadow_pass);

      inputlen = pq_getmsgint(&buf, 4);
      if (inputlen == -1)
      {
        input = NULL;
      }
      else
      {
        input = pq_getmsgbytes(&buf, inputlen);
      }

      initial = false;
    }
    else
    {
      inputlen = buf.len;
      input = pq_getmsgbytes(&buf, buf.len);
    }
    pq_getmsgend(&buf);

       
                                                                  
                 
       
    Assert(input == NULL || input[inputlen] == '\0');

       
                                                                     
                                                                          
       
    result = pg_be_scram_exchange(scram_opaq, input, inputlen, &output, &outputlen, logdetail);

                                     
    pfree(buf.data);

    if (output)
    {
         
                                                              
         
      elog(DEBUG4, "sending SASL challenge of length %u", outputlen);

      if (result == SASL_EXCHANGE_SUCCESS)
      {
        sendAuthRequest(port, AUTH_REQ_SASL_FIN, output, outputlen);
      }
      else
      {
        sendAuthRequest(port, AUTH_REQ_SASL_CONT, output, outputlen);
      }

      pfree(output);
    }
  } while (result == SASL_EXCHANGE_CONTINUE);

                                    
  if (result != SASL_EXCHANGE_SUCCESS)
  {
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

                                                                   
                                
                                                                   
   
#ifdef ENABLE_GSS
static int
pg_GSS_recvauth(Port *port)
{
  OM_uint32 maj_stat, min_stat, lmin_s, gflags;
  int mtype;
  StringInfoData buf;
  gss_buffer_desc gbuf;

     
                                                                          
                                                                            
                                                                           
                                                                      
                                                                        
                                                
     
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("GSSAPI is not supported in protocol version 2")));
  }

     
                                                                         
                                                                    
     
  if (pg_krb_server_keyfile != NULL && pg_krb_server_keyfile[0] != '\0')
  {
    if (setenv("KRB5_KTNAME", pg_krb_server_keyfile, 1) != 0)
    {
                                                                     
      ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("could not set environment: %m")));
    }
  }

     
                                                                        
                                                                          
                                                                           
                           
     
  port->gss->cred = GSS_C_NO_CREDENTIAL;

     
                                               
     
  port->gss->ctx = GSS_C_NO_CONTEXT;

     
                                                                        
                                                                             
                                                                         
                 
     
  do
  {
    pq_startmsgread();

    CHECK_FOR_INTERRUPTS();

    mtype = pq_getbyte();
    if (mtype != 'p')
    {
                                                       
      if (mtype != EOF)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("expected GSS response, got message type %d", mtype)));
      }
      return STATUS_ERROR;
    }

                                  
    initStringInfo(&buf);
    if (pq_getmessage(&buf, PG_MAX_AUTH_TOKEN_LENGTH))
    {
                                                    
      pfree(buf.data);
      return STATUS_ERROR;
    }

                                    
    gbuf.length = buf.len;
    gbuf.value = buf.data;

    elog(DEBUG4, "Processing received GSS token of length %u", (unsigned int)gbuf.length);

    maj_stat = gss_accept_sec_context(&min_stat, &port->gss->ctx, port->gss->cred, &gbuf, GSS_C_NO_CHANNEL_BINDINGS, &port->gss->name, NULL, &port->gss->outbuf, &gflags, NULL, NULL);

                             
    pfree(buf.data);

    elog(DEBUG5,
        "gss_accept_sec_context major: %d, "
        "minor: %d, outlen: %u, outflags: %x",
        maj_stat, min_stat, (unsigned int)port->gss->outbuf.length, gflags);

    CHECK_FOR_INTERRUPTS();

    if (port->gss->outbuf.length != 0)
    {
         
                                                              
         
      elog(DEBUG4, "sending GSS response token of length %u", (unsigned int)port->gss->outbuf.length);

      sendAuthRequest(port, AUTH_REQ_GSS_CONT, port->gss->outbuf.value, port->gss->outbuf.length);

      gss_release_buffer(&lmin_s, &port->gss->outbuf);
    }

    if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
    {
      gss_delete_sec_context(&lmin_s, &port->gss->ctx, GSS_C_NO_BUFFER);
      pg_GSS_error(_("accepting GSS security context failed"), maj_stat, min_stat);
      return STATUS_ERROR;
    }

    if (maj_stat == GSS_S_CONTINUE_NEEDED)
    {
      elog(DEBUG4, "GSS continue needed");
    }

  } while (maj_stat == GSS_S_CONTINUE_NEEDED);

  if (port->gss->cred != GSS_C_NO_CREDENTIAL)
  {
       
                                             
       
    gss_release_cred(&min_stat, &port->gss->cred);
  }
  return pg_GSS_checkauth(port);
}

   
                                                                            
                     
   
static int
pg_GSS_checkauth(Port *port)
{
  int ret;
  OM_uint32 maj_stat, min_stat, lmin_s;
  gss_buffer_desc gbuf;
  char *princ;

     
                                                                           
                                                     
     
  maj_stat = gss_display_name(&min_stat, port->gss->name, &gbuf, NULL);
  if (maj_stat != GSS_S_COMPLETE)
  {
    pg_GSS_error(_("retrieving GSS user name failed"), maj_stat, min_stat);
    return STATUS_ERROR;
  }

     
                                                                        
                             
     
  princ = palloc(gbuf.length + 1);
  memcpy(princ, gbuf.value, gbuf.length);
  princ[gbuf.length] = '\0';
  gss_release_buffer(&lmin_s, &gbuf);

     
                                                                            
                               
     
  port->gss->princ = MemoryContextStrdup(TopMemoryContext, princ);

     
                                               
     
  if (strchr(princ, '@'))
  {
    char *cp = strchr(princ, '@');

       
                                                                        
                                                                           
                                                                  
       
    if (!port->hba->include_realm)
    {
      *cp = '\0';
    }
    cp++;

    if (port->hba->krb_realm != NULL && strlen(port->hba->krb_realm))
    {
         
                                                
         
      if (pg_krb_caseins_users)
      {
        ret = pg_strcasecmp(port->hba->krb_realm, cp);
      }
      else
      {
        ret = strcmp(port->hba->krb_realm, cp);
      }

      if (ret)
      {
                                      
        elog(DEBUG2, "GSSAPI realm (%s) and configured realm (%s) don't match", cp, port->hba->krb_realm);
        pfree(princ);
        return STATUS_ERROR;
      }
    }
  }
  else if (port->hba->krb_realm && strlen(port->hba->krb_realm))
  {
    elog(DEBUG2, "GSSAPI did not return realm but realm matching was requested");
    pfree(princ);
    return STATUS_ERROR;
  }

  ret = check_usermap(port->hba->usermap, port->user_name, princ, pg_krb_caseins_users);

  pfree(princ);

  return ret;
}
#endif                 

                                                                   
                              
                                                                   
   
#ifdef ENABLE_SSPI

   
                                                                       
                                          
   
static void
pg_SSPI_error(int severity, const char *errmsg, SECURITY_STATUS r)
{
  char sysmsg[256];

  if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, r, 0, sysmsg, sizeof(sysmsg), NULL) == 0)
  {
    ereport(severity, (errmsg_internal("%s", errmsg), errdetail_internal("SSPI error %x", (unsigned int)r)));
  }
  else
  {
    ereport(severity, (errmsg_internal("%s", errmsg), errdetail_internal("%s (%x)", sysmsg, (unsigned int)r)));
  }
}

static int
pg_SSPI_recvauth(Port *port)
{
  int mtype;
  StringInfoData buf;
  SECURITY_STATUS r;
  CredHandle sspicred;
  CtxtHandle *sspictx = NULL, newctx;
  TimeStamp expiry;
  ULONG contextattr;
  SecBufferDesc inbuf;
  SecBufferDesc outbuf;
  SecBuffer OutBuffers[1];
  SecBuffer InBuffers[1];
  HANDLE token;
  TOKEN_USER *tokenuser;
  DWORD retlen;
  char accountname[MAXPGPATH];
  char domainname[MAXPGPATH];
  DWORD accountnamesize = sizeof(accountname);
  DWORD domainnamesize = sizeof(domainname);
  SID_NAME_USE accountnameuse;
  HMODULE secur32;

  QUERY_SECURITY_CONTEXT_TOKEN_FN _QuerySecurityContextToken;

     
                                                                           
                                                                             
                                                                           
                                                                       
                                                                        
                                                
     
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("SSPI is not supported in protocol version 2")));
  }

     
                                                 
     
  r = AcquireCredentialsHandle(NULL, "negotiate", SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &sspicred, &expiry);
  if (r != SEC_E_OK)
  {
    pg_SSPI_error(ERROR, _("could not acquire SSPI credentials"), r);
  }

     
                                                                      
                                                                             
                                                                         
                 
     
  do
  {
    pq_startmsgread();
    mtype = pq_getbyte();
    if (mtype != 'p')
    {
                                                       
      if (mtype != EOF)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("expected SSPI response, got message type %d", mtype)));
      }
      return STATUS_ERROR;
    }

                                   
    initStringInfo(&buf);
    if (pq_getmessage(&buf, PG_MAX_AUTH_TOKEN_LENGTH))
    {
                                                    
      pfree(buf.data);
      return STATUS_ERROR;
    }

                                  
    inbuf.ulVersion = SECBUFFER_VERSION;
    inbuf.cBuffers = 1;
    inbuf.pBuffers = InBuffers;
    InBuffers[0].pvBuffer = buf.data;
    InBuffers[0].cbBuffer = buf.len;
    InBuffers[0].BufferType = SECBUFFER_TOKEN;

                               
    OutBuffers[0].pvBuffer = NULL;
    OutBuffers[0].BufferType = SECBUFFER_TOKEN;
    OutBuffers[0].cbBuffer = 0;
    outbuf.cBuffers = 1;
    outbuf.pBuffers = OutBuffers;
    outbuf.ulVersion = SECBUFFER_VERSION;

    elog(DEBUG4, "Processing received SSPI token of length %u", (unsigned int)buf.len);

    r = AcceptSecurityContext(&sspicred, sspictx, &inbuf, ASC_REQ_ALLOCATE_MEMORY, SECURITY_NETWORK_DREP, &newctx, &outbuf, &contextattr, NULL);

                                     
    pfree(buf.data);

    if (outbuf.cBuffers > 0 && outbuf.pBuffers[0].cbBuffer > 0)
    {
         
                                                              
         
      elog(DEBUG4, "sending SSPI response token of length %u", (unsigned int)outbuf.pBuffers[0].cbBuffer);

      port->gss->outbuf.length = outbuf.pBuffers[0].cbBuffer;
      port->gss->outbuf.value = outbuf.pBuffers[0].pvBuffer;

      sendAuthRequest(port, AUTH_REQ_GSS_CONT, port->gss->outbuf.value, port->gss->outbuf.length);

      FreeContextBuffer(outbuf.pBuffers[0].pvBuffer);
    }

    if (r != SEC_E_OK && r != SEC_I_CONTINUE_NEEDED)
    {
      if (sspictx != NULL)
      {
        DeleteSecurityContext(sspictx);
        free(sspictx);
      }
      FreeCredentialsHandle(&sspicred);
      pg_SSPI_error(ERROR, _("could not accept SSPI security context"), r);
    }

       
                                                                       
                                                                       
                                                                           
                                                
       
    if (sspictx == NULL)
    {
      sspictx = malloc(sizeof(CtxtHandle));
      if (sspictx == NULL)
      {
        ereport(ERROR, (errmsg("out of memory")));
      }
    }

    memcpy(sspictx, &newctx, sizeof(CtxtHandle));

    if (r == SEC_I_CONTINUE_NEEDED)
    {
      elog(DEBUG4, "SSPI continue needed");
    }

  } while (r == SEC_I_CONTINUE_NEEDED);

     
                                           
     
  FreeCredentialsHandle(&sspicred);

     
                                                             
     
                                                                           
                                                     
     
                                                                      
                                                         
     

  secur32 = LoadLibrary("SECUR32.DLL");
  if (secur32 == NULL)
  {
    ereport(ERROR, (errmsg_internal("could not load secur32.dll: error code %lu", GetLastError())));
  }

  _QuerySecurityContextToken = (QUERY_SECURITY_CONTEXT_TOKEN_FN)GetProcAddress(secur32, "QuerySecurityContextToken");
  if (_QuerySecurityContextToken == NULL)
  {
    FreeLibrary(secur32);
    ereport(ERROR, (errmsg_internal("could not locate QuerySecurityContextToken in secur32.dll: error code %lu", GetLastError())));
  }

  r = (_QuerySecurityContextToken)(sspictx, &token);
  if (r != SEC_E_OK)
  {
    FreeLibrary(secur32);
    pg_SSPI_error(ERROR, _("could not get token from SSPI security context"), r);
  }

  FreeLibrary(secur32);

     
                                                                           
                    
     
  DeleteSecurityContext(sspictx);
  free(sspictx);

  if (!GetTokenInformation(token, TokenUser, NULL, 0, &retlen) && GetLastError() != 122)
  {
    ereport(ERROR, (errmsg_internal("could not get token information buffer size: error code %lu", GetLastError())));
  }

  tokenuser = malloc(retlen);
  if (tokenuser == NULL)
  {
    ereport(ERROR, (errmsg("out of memory")));
  }

  if (!GetTokenInformation(token, TokenUser, tokenuser, retlen, &retlen))
  {
    ereport(ERROR, (errmsg_internal("could not get token information: error code %lu", GetLastError())));
  }

  CloseHandle(token);

  if (!LookupAccountSid(NULL, tokenuser->User.Sid, accountname, &accountnamesize, domainname, &domainnamesize, &accountnameuse))
  {
    ereport(ERROR, (errmsg_internal("could not look up account SID: error code %lu", GetLastError())));
  }

  free(tokenuser);

  if (!port->hba->compat_realm)
  {
    int status = pg_SSPI_make_upn(accountname, sizeof(accountname), domainname, sizeof(domainname), port->hba->upn_username);

    if (status != STATUS_OK)
    {
                                                        
      return status;
    }
  }

     
                                                                     
                  
     
  if (port->hba->krb_realm && strlen(port->hba->krb_realm))
  {
    if (pg_strcasecmp(port->hba->krb_realm, domainname) != 0)
    {
      elog(DEBUG2, "SSPI domain (%s) and configured domain (%s) don't match", domainname, port->hba->krb_realm);

      return STATUS_ERROR;
    }
  }

     
                                                                            
                                                                   
     
                                                                      
     
  if (port->hba->include_realm)
  {
    char *namebuf;
    int retval;

    namebuf = psprintf("%s@%s", accountname, domainname);
    retval = check_usermap(port->hba->usermap, port->user_name, namebuf, true);
    pfree(namebuf);
    return retval;
  }
  else
  {
    return check_usermap(port->hba->usermap, port->user_name, accountname, true);
  }
}

   
                                                         
                                                               
   
static int
pg_SSPI_make_upn(char *accountname, size_t accountnamesize, char *domainname, size_t domainnamesize, bool update_accountname)
{
  char *samname;
  char *upname = NULL;
  char *p = NULL;
  ULONG upnamesize = 0;
  size_t upnamerealmsize;
  BOOLEAN res;

     
                                                         
                                                                          
                                                                      
                       
     

  samname = psprintf("%s\\%s", domainname, accountname);
  res = TranslateName(samname, NameSamCompatible, NameUserPrincipal, NULL, &upnamesize);

  if ((!res && GetLastError() != ERROR_INSUFFICIENT_BUFFER) || upnamesize == 0)
  {
    pfree(samname);
    ereport(LOG, (errcode(ERRCODE_INVALID_ROLE_SPECIFICATION), errmsg("could not translate name")));
    return STATUS_ERROR;
  }

                                                
  upname = palloc(upnamesize);

  res = TranslateName(samname, NameSamCompatible, NameUserPrincipal, upname, &upnamesize);

  pfree(samname);
  if (res)
  {
    p = strchr(upname, '@');
  }

  if (!res || p == NULL)
  {
    pfree(upname);
    ereport(LOG, (errcode(ERRCODE_INVALID_ROLE_SPECIFICATION), errmsg("could not translate name")));
    return STATUS_ERROR;
  }

                                                              
  upnamerealmsize = upnamesize - (p - upname + 1);

                                           
  if (upnamerealmsize > domainnamesize)
  {
    pfree(upname);
    ereport(LOG, (errcode(ERRCODE_INVALID_ROLE_SPECIFICATION), errmsg("realm name too long")));
    return STATUS_ERROR;
  }

                           
  strcpy(domainname, p + 1);

                                                          
  if (update_accountname)
  {
    if ((p - upname + 1) > accountnamesize)
    {
      pfree(upname);
      ereport(LOG, (errcode(ERRCODE_INVALID_ROLE_SPECIFICATION), errmsg("translated account name too long")));
      return STATUS_ERROR;
    }

    *p = 0;
    strcpy(accountname, upname);
  }

  pfree(upname);
  return STATUS_OK;
}
#endif                  

                                                                   
                               
                                                                   
   

   
                                                                             
                                                                          
                                                                  
                 
   
static bool
interpret_ident_response(const char *ident_response, char *ident_user)
{
  const char *cursor = ident_response;                                  

     
                                                                           
     
  if (strlen(ident_response) < 2)
  {
    return false;
  }
  else if (ident_response[strlen(ident_response) - 2] != '\r')
  {
    return false;
  }
  else
  {
    while (*cursor != ':' && *cursor != '\r')
    {
      cursor++;                      
    }

    if (*cursor != ':')
    {
      return false;
    }
    else
    {
                                                                
      char response_type[80];
      int i;                                

      cursor++;                    
      while (pg_isblank(*cursor))
      {
        cursor++;                  
      }
      i = 0;
      while (*cursor != ':' && *cursor != '\r' && !pg_isblank(*cursor) && i < (int)(sizeof(response_type) - 1))
      {
        response_type[i++] = *cursor++;
      }
      response_type[i] = '\0';
      while (pg_isblank(*cursor))
      {
        cursor++;                  
      }
      if (strcmp(response_type, "USERID") != 0)
      {
        return false;
      }
      else
      {
           
                                                                       
                                                                 
           
        if (*cursor != ':')
        {
          return false;
        }
        else
        {
          cursor++;                    
                                                 
          while (*cursor != ':' && *cursor != '\r')
          {
            cursor++;
          }
          if (*cursor != ':')
          {
            return false;
          }
          else
          {
            int i;                             

            cursor++;                    
            while (pg_isblank(*cursor))
            {
              cursor++;                  
            }
                                                           
            i = 0;
            while (*cursor != '\r' && i < IDENT_USERNAME_MAX)
            {
              ident_user[i++] = *cursor++;
            }
            ident_user[i] = '\0';
            return true;
          }
        }
      }
    }
  }
}

   
                                                                      
                                                               
                                                                        
                                        
   
                                                            
   
                                                                         
   
                                                                            
                                                                             
   
static int
ident_inet(hbaPort *port)
{
  const SockAddr remote_addr = port->raddr;
  const SockAddr local_addr = port->laddr;
  char ident_user[IDENT_USERNAME_MAX + 1];
  pgsocket sock_fd = PGINVALID_SOCKET;                                  
  int rc;                                                                              
  bool ident_return;
  char remote_addr_s[NI_MAXHOST];
  char remote_port[NI_MAXSERV];
  char local_addr_s[NI_MAXHOST];
  char local_port[NI_MAXSERV];
  char ident_port[NI_MAXSERV];
  char ident_query[80];
  char ident_response[80 + IDENT_USERNAME_MAX];
  struct addrinfo *ident_serv = NULL, *la = NULL, hints;

     
                                                                            
                                              
     
  pg_getnameinfo_all(&remote_addr.addr, remote_addr.salen, remote_addr_s, sizeof(remote_addr_s), remote_port, sizeof(remote_port), NI_NUMERICHOST | NI_NUMERICSERV);
  pg_getnameinfo_all(&local_addr.addr, local_addr.salen, local_addr_s, sizeof(local_addr_s), local_port, sizeof(local_port), NI_NUMERICHOST | NI_NUMERICSERV);

  snprintf(ident_port, sizeof(ident_port), "%d", IDENT_PORT);
  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = remote_addr.addr.ss_family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_addrlen = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  rc = pg_getaddrinfo_all(remote_addr_s, ident_port, &hints, &ident_serv);
  if (rc || !ident_serv)
  {
                                        
    ident_return = false;
    goto ident_inet_done;
  }

  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = local_addr.addr.ss_family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_addrlen = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  rc = pg_getaddrinfo_all(local_addr_s, NULL, &hints, &la);
  if (rc || !la)
  {
                                        
    ident_return = false;
    goto ident_inet_done;
  }

  sock_fd = socket(ident_serv->ai_family, ident_serv->ai_socktype, ident_serv->ai_protocol);
  if (sock_fd == PGINVALID_SOCKET)
  {
    ereport(LOG, (errcode_for_socket_access(), errmsg("could not create socket for Ident connection: %m")));
    ident_return = false;
    goto ident_inet_done;
  }

     
                                                                          
                                                                           
                                                                      
     
  rc = bind(sock_fd, la->ai_addr, la->ai_addrlen);
  if (rc != 0)
  {
    ereport(LOG, (errcode_for_socket_access(), errmsg("could not bind to local address \"%s\": %m", local_addr_s)));
    ident_return = false;
    goto ident_inet_done;
  }

  rc = connect(sock_fd, ident_serv->ai_addr, ident_serv->ai_addrlen);
  if (rc != 0)
  {
    ereport(LOG, (errcode_for_socket_access(), errmsg("could not connect to Ident server at address \"%s\", port %s: %m", remote_addr_s, ident_port)));
    ident_return = false;
    goto ident_inet_done;
  }

                                             
  snprintf(ident_query, sizeof(ident_query), "%s,%s\r\n", remote_port, local_port);

                                        
  do
  {
    CHECK_FOR_INTERRUPTS();

    rc = send(sock_fd, ident_query, strlen(ident_query), 0);
  } while (rc < 0 && errno == EINTR);

  if (rc < 0)
  {
    ereport(LOG, (errcode_for_socket_access(), errmsg("could not send query to Ident server at address \"%s\", port %s: %m", remote_addr_s, ident_port)));
    ident_return = false;
    goto ident_inet_done;
  }

  do
  {
    CHECK_FOR_INTERRUPTS();

    rc = recv(sock_fd, ident_response, sizeof(ident_response) - 1, 0);
  } while (rc < 0 && errno == EINTR);

  if (rc < 0)
  {
    ereport(LOG, (errcode_for_socket_access(), errmsg("could not receive response from Ident server at address \"%s\", port %s: %m", remote_addr_s, ident_port)));
    ident_return = false;
    goto ident_inet_done;
  }

  ident_response[rc] = '\0';
  ident_return = interpret_ident_response(ident_response, ident_user);
  if (!ident_return)
  {
    ereport(LOG, (errmsg("invalidly formatted response from Ident server: \"%s\"", ident_response)));
  }

ident_inet_done:
  if (sock_fd != PGINVALID_SOCKET)
  {
    closesocket(sock_fd);
  }
  if (ident_serv)
  {
    pg_freeaddrinfo_all(remote_addr.addr.ss_family, ident_serv);
  }
  if (la)
  {
    pg_freeaddrinfo_all(local_addr.addr.ss_family, la);
  }

  if (ident_return)
  {
                                    
    return check_usermap(port->hba->usermap, port->user_name, ident_user, false);
  }
  return STATUS_ERROR;
}

   
                                                               
                                                                    
                             
   
                                                                    
   
#ifdef HAVE_UNIX_SOCKETS

static int
auth_peer(hbaPort *port)
{
  char ident_user[IDENT_USERNAME_MAX + 1];
  uid_t uid;
  gid_t gid;
  struct passwd *pw;

  if (getpeereid(port->sock, &uid, &gid) != 0)
  {
                                                               
    if (errno == ENOSYS)
    {
      ereport(LOG, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("peer authentication is not supported on this platform")));
    }
    else
    {
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not get peer credentials: %m")));
    }
    return STATUS_ERROR;
  }

  errno = 0;                              
  pw = getpwuid(uid);
  if (!pw)
  {
    int save_errno = errno;

    ereport(LOG, (errmsg("could not look up local user ID %ld: %s", (long)uid, save_errno ? strerror(save_errno) : _("user does not exist"))));
    return STATUS_ERROR;
  }

  strlcpy(ident_user, pw->pw_name, IDENT_USERNAME_MAX + 1);

  return check_usermap(port->hba->usermap, port->user_name, ident_user, false);
}
#endif                        

                                                                   
                             
                                                                   
   
#ifdef USE_PAM

   
                             
   

static int
pam_passwd_conv_proc(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
  const char *passwd;
  struct pam_response *reply;
  int i;

  if (appdata_ptr)
  {
    passwd = (char *)appdata_ptr;
  }
  else
  {
       
                                                                           
                                                        
       
    passwd = pam_passwd;
  }

  *resp = NULL;                            

  if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
  {
    return PAM_CONV_ERR;
  }

     
                                                                     
               
     
  if ((reply = calloc(num_msg, sizeof(struct pam_response))) == NULL)
  {
    ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    return PAM_CONV_ERR;
  }

  for (i = 0; i < num_msg; i++)
  {
    switch (msg[i]->msg_style)
    {
    case PAM_PROMPT_ECHO_OFF:
      if (strlen(passwd) == 0)
      {
           
                                                                 
                                                                
                                
           
        sendAuthRequest(pam_port_cludge, AUTH_REQ_PASSWORD, NULL, 0);
        passwd = recv_password_packet(pam_port_cludge);
        if (passwd == NULL)
        {
             
                                                      
                                                           
                                              
             
          pam_no_password = true;
          goto fail;
        }
      }
      if ((reply[i].resp = strdup(passwd)) == NULL)
      {
        goto fail;
      }
      reply[i].resp_retcode = PAM_SUCCESS;
      break;
    case PAM_ERROR_MSG:
      ereport(LOG, (errmsg("error from underlying PAM layer: %s", msg[i]->msg)));
                        
    case PAM_TEXT_INFO:
                                                     
      if ((reply[i].resp = strdup("")) == NULL)
      {
        goto fail;
      }
      reply[i].resp_retcode = PAM_SUCCESS;
      break;
    default:
      elog(LOG, "unsupported PAM conversation %d/\"%s\"", msg[i]->msg_style, msg[i]->msg ? msg[i]->msg : "(none)");
      goto fail;
    }
  }

  *resp = reply;
  return PAM_SUCCESS;

fail:
                                     
  for (i = 0; i < num_msg; i++)
  {
    if (reply[i].resp != NULL)
    {
      free(reply[i].resp);
    }
  }
  free(reply);

  return PAM_CONV_ERR;
}

   
                                     
   
static int
CheckPAMAuth(Port *port, const char *user, const char *password)
{
  int retval;
  pam_handle_t *pamh = NULL;

     
                                                                          
                                                                    
                        
     
  pam_passwd = password;
  pam_port_cludge = port;
  pam_no_password = false;

     
                                                                           
                                                                        
                            
     
  pam_passw_conv.appdata_ptr = unconstify(char *, password);                         
                                                                                

                                                               
  if (port->hba->pamservice && port->hba->pamservice[0] != '\0')
  {
    retval = pam_start(port->hba->pamservice, "pgsql@", &pam_passw_conv, &pamh);
  }
  else
  {
    retval = pam_start(PGSQL_PAM_SERVICE, "pgsql@", &pam_passw_conv, &pamh);
  }

  if (retval != PAM_SUCCESS)
  {
    ereport(LOG, (errmsg("could not create PAM authenticator: %s", pam_strerror(pamh, retval))));
    pam_passwd = NULL;                       
    return STATUS_ERROR;
  }

  retval = pam_set_item(pamh, PAM_USER, user);

  if (retval != PAM_SUCCESS)
  {
    ereport(LOG, (errmsg("pam_set_item(PAM_USER) failed: %s", pam_strerror(pamh, retval))));
    pam_passwd = NULL;                       
    return STATUS_ERROR;
  }

  if (port->hba->conntype != ctLocal)
  {
    char hostinfo[NI_MAXHOST];
    int flags;

    if (port->hba->pam_use_hostname)
    {
      flags = 0;
    }
    else
    {
      flags = NI_NUMERICHOST | NI_NUMERICSERV;
    }

    retval = pg_getnameinfo_all(&port->raddr.addr, port->raddr.salen, hostinfo, sizeof(hostinfo), NULL, 0, flags);
    if (retval != 0)
    {
      ereport(WARNING, (errmsg_internal("pg_getnameinfo_all() failed: %s", gai_strerror(retval))));
      return STATUS_ERROR;
    }

    retval = pam_set_item(pamh, PAM_RHOST, hostinfo);

    if (retval != PAM_SUCCESS)
    {
      ereport(LOG, (errmsg("pam_set_item(PAM_RHOST) failed: %s", pam_strerror(pamh, retval))));
      pam_passwd = NULL;
      return STATUS_ERROR;
    }
  }

  retval = pam_set_item(pamh, PAM_CONV, &pam_passw_conv);

  if (retval != PAM_SUCCESS)
  {
    ereport(LOG, (errmsg("pam_set_item(PAM_CONV) failed: %s", pam_strerror(pamh, retval))));
    pam_passwd = NULL;                       
    return STATUS_ERROR;
  }

  retval = pam_authenticate(pamh, 0);

  if (retval != PAM_SUCCESS)
  {
                                                             
    if (!pam_no_password)
    {
      ereport(LOG, (errmsg("pam_authenticate failed: %s", pam_strerror(pamh, retval))));
    }
    pam_passwd = NULL;                       
    return pam_no_password ? STATUS_EOF : STATUS_ERROR;
  }

  retval = pam_acct_mgmt(pamh, 0);

  if (retval != PAM_SUCCESS)
  {
                                                             
    if (!pam_no_password)
    {
      ereport(LOG, (errmsg("pam_acct_mgmt failed: %s", pam_strerror(pamh, retval))));
    }
    pam_passwd = NULL;                       
    return pam_no_password ? STATUS_EOF : STATUS_ERROR;
  }

  retval = pam_end(pamh, retval);

  if (retval != PAM_SUCCESS)
  {
    ereport(LOG, (errmsg("could not release PAM authenticator: %s", pam_strerror(pamh, retval))));
  }

  pam_passwd = NULL;                       

  return (retval == PAM_SUCCESS ? STATUS_OK : STATUS_ERROR);
}
#endif              

                                                                   
                             
                                                                   
   
#ifdef USE_BSD_AUTH
static int
CheckBSDAuth(Port *port, char *user)
{
  char *passwd;
  int retval;

                                                                     
  sendAuthRequest(port, AUTH_REQ_PASSWORD, NULL, 0);

  passwd = recv_password_packet(port);
  if (passwd == NULL)
  {
    return STATUS_EOF;
  }

     
                                                                          
                                                                     
                                        
     
  retval = auth_userokay(user, NULL, "auth-postgresql", passwd);

  pfree(passwd);

  if (!retval)
  {
    return STATUS_ERROR;
  }

  return STATUS_OK;
}
#endif                   

                                                                   
                              
                                                                   
   
#ifdef USE_LDAP

static int
errdetail_for_ldap(LDAP *ldap);

   
                                                                    
                     
   
static int
InitializeLDAPConnection(Port *port, LDAP **ldap)
{
  const char *scheme;
  int ldapversion = LDAP_VERSION3;
  int r;

  scheme = port->hba->ldapscheme;
  if (scheme == NULL)
  {
    scheme = "ldap";
  }
#ifdef WIN32
  if (strcmp(scheme, "ldaps") == 0)
  {
    *ldap = ldap_sslinit(port->hba->ldapserver, port->hba->ldapport, 1);
  }
  else
  {
    *ldap = ldap_init(port->hba->ldapserver, port->hba->ldapport);
  }
  if (!*ldap)
  {
    ereport(LOG, (errmsg("could not initialize LDAP: error code %d", (int)LdapGetLastError())));

    return STATUS_ERROR;
  }
#else
#ifdef HAVE_LDAP_INITIALIZE

     
                                                                             
                                                                           
                                                                             
                                                                             
                                                         
     
  {
    StringInfoData uris;
    char *hostlist = NULL;
    char *p;
    bool append_port;

                                                                        
    initStringInfo(&uris);

       
                                                                           
                                                                          
                                                   
       
    if (!port->hba->ldapserver || port->hba->ldapserver[0] == '\0')
    {
      char *domain;

                                            
      if (ldap_dn2domain(port->hba->ldapbasedn, &domain))
      {
        ereport(LOG, (errmsg("could not extract domain name from ldapbasedn")));
        return STATUS_ERROR;
      }

                                                                
      if (ldap_domain2hostlist(domain, &hostlist))
      {
        ereport(LOG, (errmsg("LDAP authentication could not find DNS SRV records for \"%s\"", domain), (errhint("Set an LDAP server name explicitly."))));
        ldap_memfree(domain);
        return STATUS_ERROR;
      }
      ldap_memfree(domain);

                                                               
      p = hostlist;
      append_port = false;
    }
    else
    {
                                                                    
      p = port->hba->ldapserver;
      append_port = true;
    }

                                                              
    do
    {
      size_t size;

                                           
      size = strcspn(p, " ");

                                                                
      if (uris.len > 0)
      {
        appendStringInfoChar(&uris, ' ');
      }

                                     
      appendStringInfoString(&uris, scheme);
      appendStringInfoString(&uris, "://");
      appendBinaryStringInfo(&uris, p, size);
      if (append_port)
      {
        appendStringInfo(&uris, ":%d", port->hba->ldapport);
      }

                                                                  
      p += size;
      while (*p == ' ')
      {
        ++p;
      }
    } while (*p);

                                                               
    if (hostlist)
    {
      ldap_memfree(hostlist);
    }

                                                    
    r = ldap_initialize(ldap, uris.data);
    pfree(uris.data);
    if (r != LDAP_SUCCESS)
    {
      ereport(LOG, (errmsg("could not initialize LDAP: %s", ldap_err2string(r))));

      return STATUS_ERROR;
    }
  }
#else
  if (strcmp(scheme, "ldaps") == 0)
  {
    ereport(LOG, (errmsg("ldaps not supported with this LDAP library")));

    return STATUS_ERROR;
  }
  *ldap = ldap_init(port->hba->ldapserver, port->hba->ldapport);
  if (!*ldap)
  {
    ereport(LOG, (errmsg("could not initialize LDAP: %m")));

    return STATUS_ERROR;
  }
#endif
#endif

  if ((r = ldap_set_option(*ldap, LDAP_OPT_PROTOCOL_VERSION, &ldapversion)) != LDAP_SUCCESS)
  {
    ereport(LOG, (errmsg("could not set LDAP protocol version: %s", ldap_err2string(r)), errdetail_for_ldap(*ldap)));
    ldap_unbind(*ldap);
    return STATUS_ERROR;
  }

  if (port->hba->ldaptls)
  {
#ifndef WIN32
    if ((r = ldap_start_tls_s(*ldap, NULL, NULL)) != LDAP_SUCCESS)
#else
    static __ldap_start_tls_sA _ldap_start_tls_sA = NULL;

    if (_ldap_start_tls_sA == NULL)
    {
         
                                                                    
                                                                      
                            
         
      HANDLE ldaphandle;

      ldaphandle = LoadLibrary("WLDAP32.DLL");
      if (ldaphandle == NULL)
      {
           
                                                                
                                     
           
        ereport(LOG, (errmsg("could not load wldap32.dll")));
        ldap_unbind(*ldap);
        return STATUS_ERROR;
      }
      _ldap_start_tls_sA = (__ldap_start_tls_sA)GetProcAddress(ldaphandle, "ldap_start_tls_sA");
      if (_ldap_start_tls_sA == NULL)
      {
        ereport(LOG, (errmsg("could not load function _ldap_start_tls_sA in wldap32.dll"), errdetail("LDAP over SSL is not supported on this platform.")));
        ldap_unbind(*ldap);
        return STATUS_ERROR;
      }

         
                                                                     
                                                                        
                                                                      
         
    }
    if ((r = _ldap_start_tls_sA(*ldap, NULL, NULL, NULL, NULL)) != LDAP_SUCCESS)
#endif
    {
      ereport(LOG, (errmsg("could not start LDAP TLS session: %s", ldap_err2string(r)), errdetail_for_ldap(*ldap)));
      ldap_unbind(*ldap);
      return STATUS_ERROR;
    }
  }

  return STATUS_OK;
}

                                                                       
#define LPH_USERNAME "$username"
#define LPH_USERNAME_LEN (sizeof(LPH_USERNAME) - 1)

                                               
#ifndef LDAP_NO_ATTRS
#define LDAP_NO_ATTRS "1.1"
#endif

                                               
#ifndef LDAPS_PORT
#define LDAPS_PORT 636
#endif

   
                                                                    
                                                                         
   
static char *
FormatSearchFilter(const char *pattern, const char *user_name)
{
  StringInfoData output;

  initStringInfo(&output);
  while (*pattern != '\0')
  {
    if (strncmp(pattern, LPH_USERNAME, LPH_USERNAME_LEN) == 0)
    {
      appendStringInfoString(&output, user_name);
      pattern += LPH_USERNAME_LEN;
    }
    else
    {
      appendStringInfoChar(&output, *pattern++);
    }
  }

  return output.data;
}

   
                               
   
static int
CheckLDAPAuth(Port *port)
{
  char *passwd;
  LDAP *ldap;
  int r;
  char *fulluser;
  const char *server_name;

#ifdef HAVE_LDAP_INITIALIZE

     
                                                                             
                                                                   
     
  if ((!port->hba->ldapserver || port->hba->ldapserver[0] == '\0') && (!port->hba->ldapbasedn || port->hba->ldapbasedn[0] == '\0'))
  {
    ereport(LOG, (errmsg("LDAP server not specified, and no ldapbasedn")));
    return STATUS_ERROR;
  }
#else
  if (!port->hba->ldapserver || port->hba->ldapserver[0] == '\0')
  {
    ereport(LOG, (errmsg("LDAP server not specified")));
    return STATUS_ERROR;
  }
#endif

     
                                                                           
                                             
     
  server_name = port->hba->ldapserver ? port->hba->ldapserver : "";

  if (port->hba->ldapport == 0)
  {
    if (port->hba->ldapscheme != NULL && strcmp(port->hba->ldapscheme, "ldaps") == 0)
    {
      port->hba->ldapport = LDAPS_PORT;
    }
    else
    {
      port->hba->ldapport = LDAP_PORT;
    }
  }

  sendAuthRequest(port, AUTH_REQ_PASSWORD, NULL, 0);

  passwd = recv_password_packet(port);
  if (passwd == NULL)
  {
    return STATUS_EOF;                                    
  }

  if (InitializeLDAPConnection(port, &ldap) == STATUS_ERROR)
  {
                                    
    pfree(passwd);
    return STATUS_ERROR;
  }

  if (port->hba->ldapbasedn)
  {
       
                                                                       
                            
       
    char *filter;
    LDAPMessage *search_message;
    LDAPMessage *entry;
    char *attributes[] = {LDAP_NO_ATTRS, NULL};
    char *dn;
    char *c;
    int count;

       
                                                                       
                                                                          
                                                                           
                        
       
    for (c = port->user_name; *c; c++)
    {
      if (*c == '*' || *c == '(' || *c == ')' || *c == '\\' || *c == '/')
      {
        ereport(LOG, (errmsg("invalid character in user name for LDAP authentication")));
        ldap_unbind(ldap);
        pfree(passwd);
        return STATUS_ERROR;
      }
    }

       
                                                                    
                                                                           
       
    r = ldap_simple_bind_s(ldap, port->hba->ldapbinddn ? port->hba->ldapbinddn : "", port->hba->ldapbindpasswd ? port->hba->ldapbindpasswd : "");
    if (r != LDAP_SUCCESS)
    {
      ereport(LOG, (errmsg("could not perform initial LDAP bind for ldapbinddn \"%s\" on server \"%s\": %s", port->hba->ldapbinddn ? port->hba->ldapbinddn : "", server_name, ldap_err2string(r)), errdetail_for_ldap(ldap)));
      ldap_unbind(ldap);
      pfree(passwd);
      return STATUS_ERROR;
    }

                                                             
    if (port->hba->ldapsearchfilter)
    {
      filter = FormatSearchFilter(port->hba->ldapsearchfilter, port->user_name);
    }
    else if (port->hba->ldapsearchattribute)
    {
      filter = psprintf("(%s=%s)", port->hba->ldapsearchattribute, port->user_name);
    }
    else
    {
      filter = psprintf("(uid=%s)", port->user_name);
    }

    r = ldap_search_s(ldap, port->hba->ldapbasedn, port->hba->ldapscope, filter, attributes, 0, &search_message);

    if (r != LDAP_SUCCESS)
    {
      ereport(LOG, (errmsg("could not search LDAP for filter \"%s\" on server \"%s\": %s", filter, server_name, ldap_err2string(r)), errdetail_for_ldap(ldap)));
      ldap_unbind(ldap);
      pfree(passwd);
      pfree(filter);
      return STATUS_ERROR;
    }

    count = ldap_count_entries(ldap, search_message);
    if (count != 1)
    {
      if (count == 0)
      {
        ereport(LOG, (errmsg("LDAP user \"%s\" does not exist", port->user_name), errdetail("LDAP search for filter \"%s\" on server \"%s\" returned no entries.", filter, server_name)));
      }
      else
      {
        ereport(LOG, (errmsg("LDAP user \"%s\" is not unique", port->user_name), errdetail_plural("LDAP search for filter \"%s\" on server \"%s\" returned %d entry.", "LDAP search for filter \"%s\" on server \"%s\" returned %d entries.", count, filter, server_name, count)));
      }

      ldap_unbind(ldap);
      pfree(passwd);
      pfree(filter);
      ldap_msgfree(search_message);
      return STATUS_ERROR;
    }

    entry = ldap_first_entry(ldap, search_message);
    dn = ldap_get_dn(ldap, entry);
    if (dn == NULL)
    {
      int error;

      (void)ldap_get_option(ldap, LDAP_OPT_ERROR_NUMBER, &error);
      ereport(LOG, (errmsg("could not get dn for the first entry matching \"%s\" on server \"%s\": %s", filter, server_name, ldap_err2string(error)), errdetail_for_ldap(ldap)));
      ldap_unbind(ldap);
      pfree(passwd);
      pfree(filter);
      ldap_msgfree(search_message);
      return STATUS_ERROR;
    }
    fulluser = pstrdup(dn);

    pfree(filter);
    ldap_memfree(dn);
    ldap_msgfree(search_message);

                                                    
    r = ldap_unbind_s(ldap);
    if (r != LDAP_SUCCESS)
    {
      ereport(LOG, (errmsg("could not unbind after searching for user \"%s\" on server \"%s\"", fulluser, server_name)));
      pfree(passwd);
      pfree(fulluser);
      return STATUS_ERROR;
    }

       
                                                                         
                                     
       
    if (InitializeLDAPConnection(port, &ldap) == STATUS_ERROR)
    {
      pfree(passwd);
      pfree(fulluser);

                                      
      return STATUS_ERROR;
    }
  }
  else
  {
    fulluser = psprintf("%s%s%s", port->hba->ldapprefix ? port->hba->ldapprefix : "", port->user_name, port->hba->ldapsuffix ? port->hba->ldapsuffix : "");
  }

  r = ldap_simple_bind_s(ldap, fulluser, passwd);

  if (r != LDAP_SUCCESS)
  {
    ereport(LOG, (errmsg("LDAP login failed for user \"%s\" on server \"%s\": %s", fulluser, server_name, ldap_err2string(r)), errdetail_for_ldap(ldap)));
    ldap_unbind(ldap);
    pfree(passwd);
    pfree(fulluser);
    return STATUS_ERROR;
  }

  ldap_unbind(ldap);
  pfree(passwd);
  pfree(fulluser);

  return STATUS_OK;
}

   
                                                                      
                                                   
   
static int
errdetail_for_ldap(LDAP *ldap)
{
  char *message;
  int rc;

  rc = ldap_get_option(ldap, LDAP_OPT_DIAGNOSTIC_MESSAGE, &message);
  if (rc == LDAP_SUCCESS && message != NULL)
  {
    errdetail("LDAP diagnostics: %s", message);
    ldap_memfree(message);
  }

  return 0;
}

#endif               

                                                                   
                                         
                                                                   
   
#ifdef USE_SSL
static int
CheckCertAuth(Port *port)
{
  int status_check_usermap = STATUS_ERROR;

  Assert(port->ssl);

                                                                
  if (port->peer_cn == NULL || strlen(port->peer_cn) <= 0)
  {
    ereport(LOG, (errmsg("certificate authentication failed for user \"%s\": client certificate contains no user name", port->user_name)));
    return STATUS_ERROR;
  }

                                                         
  status_check_usermap = check_usermap(port->hba->usermap, port->user_name, port->peer_cn, false);
  if (status_check_usermap != STATUS_OK)
  {
       
                                                                      
                                                                     
                       
       
    if (port->hba->clientcert == clientCertFull && port->hba->auth_method != uaCert)
    {
      ereport(LOG, (errmsg("certificate validation (clientcert=verify-full) failed for user \"%s\": CN mismatch", port->user_name)));
    }
  }
  return status_check_usermap;
}
#endif

                                                                   
                         
                                                                   
   

   
                                                                       
   

#define RADIUS_VECTOR_LENGTH 16
#define RADIUS_HEADER_LENGTH 20
#define RADIUS_MAX_PASSWORD_LENGTH 128

                                                              
#define RADIUS_BUFFER_SIZE 1024

typedef struct
{
  uint8 attribute;
  uint8 length;
  uint8 data[FLEXIBLE_ARRAY_MEMBER];
} radius_attribute;

typedef struct
{
  uint8 code;
  uint8 id;
  uint16 length;
  uint8 vector[RADIUS_VECTOR_LENGTH];
                                                     
  char pad[RADIUS_BUFFER_SIZE - RADIUS_VECTOR_LENGTH];
} radius_packet;

                         
#define RADIUS_ACCESS_REQUEST 1
#define RADIUS_ACCESS_ACCEPT 2
#define RADIUS_ACCESS_REJECT 3

                       
#define RADIUS_USER_NAME 1
#define RADIUS_PASSWORD 2
#define RADIUS_SERVICE_TYPE 6
#define RADIUS_NAS_IDENTIFIER 32

                          
#define RADIUS_AUTHENTICATE_ONLY 8

                                                            
#define RADIUS_TIMEOUT 3

static void
radius_add_attribute(radius_packet *packet, uint8 type, const unsigned char *data, int len)
{
  radius_attribute *attr;

  if (packet->length + len > RADIUS_BUFFER_SIZE)
  {
       
                                                                         
                                                                           
                                                                           
             
       
    elog(WARNING, "Adding attribute code %d with length %d to radius packet would create oversize packet, ignoring", type, len);
    return;
  }

  attr = (radius_attribute *)((unsigned char *)packet + packet->length);
  attr->attribute = type;
  attr->length = len + 2;                                          
  memcpy(attr->data, data, len);
  packet->length += attr->length;
}

static int
CheckRADIUSAuth(Port *port)
{
  char *passwd;
  ListCell *server, *secrets, *radiusports, *identifiers;

                                             
  Assert(offsetof(radius_packet, vector) == 4);

                         
  if (list_length(port->hba->radiusservers) < 1)
  {
    ereport(LOG, (errmsg("RADIUS server not specified")));
    return STATUS_ERROR;
  }

  if (list_length(port->hba->radiussecrets) < 1)
  {
    ereport(LOG, (errmsg("RADIUS secret not specified")));
    return STATUS_ERROR;
  }

                                                                     
  sendAuthRequest(port, AUTH_REQ_PASSWORD, NULL, 0);

  passwd = recv_password_packet(port);
  if (passwd == NULL)
  {
    return STATUS_EOF;                                    
  }

  if (strlen(passwd) > RADIUS_MAX_PASSWORD_LENGTH)
  {
    ereport(LOG, (errmsg("RADIUS authentication does not support passwords longer than %d characters", RADIUS_MAX_PASSWORD_LENGTH)));
    pfree(passwd);
    return STATUS_ERROR;
  }

     
                                             
     
  secrets = list_head(port->hba->radiussecrets);
  radiusports = list_head(port->hba->radiusports);
  identifiers = list_head(port->hba->radiusidentifiers);
  foreach (server, port->hba->radiusservers)
  {
    int ret = PerformRadiusTransaction(lfirst(server), lfirst(secrets), radiusports ? lfirst(radiusports) : NULL, identifiers ? lfirst(identifiers) : NULL, port->user_name, passwd);

             
                            
                                                        
                                                            
             
       
    if (ret == STATUS_OK)
    {
      pfree(passwd);
      return STATUS_OK;
    }
    else if (ret == STATUS_EOF)
    {
      pfree(passwd);
      return STATUS_ERROR;
    }

       
                                                                        
                                                                         
                                                                       
                                                    
       
    if (list_length(port->hba->radiussecrets) > 1)
    {
      secrets = lnext(secrets);
    }
    if (list_length(port->hba->radiusports) > 1)
    {
      radiusports = lnext(radiusports);
    }
    if (list_length(port->hba->radiusidentifiers) > 1)
    {
      identifiers = lnext(identifiers);
    }
  }

                                          
  pfree(passwd);
  return STATUS_ERROR;
}

static int
PerformRadiusTransaction(const char *server, const char *secret, const char *portstr, const char *identifier, const char *user_name, const char *passwd)
{
  radius_packet radius_send_pack;
  radius_packet radius_recv_pack;
  radius_packet *packet = &radius_send_pack;
  radius_packet *receivepacket = &radius_recv_pack;
  char *radius_buffer = (char *)&radius_send_pack;
  char *receive_buffer = (char *)&radius_recv_pack;
  int32 service = pg_hton32(RADIUS_AUTHENTICATE_ONLY);
  uint8 *cryptvector;
  int encryptedpasswordlen;
  uint8 encryptedpassword[RADIUS_MAX_PASSWORD_LENGTH];
  uint8 *md5trailer;
  int packetlength;
  pgsocket sock;

#ifdef HAVE_IPV6
  struct sockaddr_in6 localaddr;
  struct sockaddr_in6 remoteaddr;
#else
  struct sockaddr_in localaddr;
  struct sockaddr_in remoteaddr;
#endif
  struct addrinfo hint;
  struct addrinfo *serveraddrs;
  int port;
  ACCEPT_TYPE_ARG3 addrsize;
  fd_set fdset;
  struct timeval endtime;
  int i, j, r;

                             
  if (portstr == NULL)
  {
    portstr = "1812";
  }
  if (identifier == NULL)
  {
    identifier = "postgresql";
  }

  MemSet(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_DGRAM;
  hint.ai_family = AF_UNSPEC;
  port = atoi(portstr);

  r = pg_getaddrinfo_all(server, portstr, &hint, &serveraddrs);
  if (r || !serveraddrs)
  {
    ereport(LOG, (errmsg("could not translate RADIUS server name \"%s\" to address: %s", server, gai_strerror(r))));
    if (serveraddrs)
    {
      pg_freeaddrinfo_all(hint.ai_family, serveraddrs);
    }
    return STATUS_ERROR;
  }
                                                         

                               
  packet->code = RADIUS_ACCESS_REQUEST;
  packet->length = RADIUS_HEADER_LENGTH;
  if (!pg_strong_random(packet->vector, RADIUS_VECTOR_LENGTH))
  {
    ereport(LOG, (errmsg("could not generate random encryption vector")));
    pg_freeaddrinfo_all(hint.ai_family, serveraddrs);
    return STATUS_ERROR;
  }
  packet->id = packet->vector[0];
  radius_add_attribute(packet, RADIUS_SERVICE_TYPE, (const unsigned char *)&service, sizeof(service));
  radius_add_attribute(packet, RADIUS_USER_NAME, (const unsigned char *)user_name, strlen(user_name));
  radius_add_attribute(packet, RADIUS_NAS_IDENTIFIER, (const unsigned char *)identifier, strlen(identifier));

     
                                                                   
                                                                           
                                                                           
                    
     
  encryptedpasswordlen = ((strlen(passwd) + RADIUS_VECTOR_LENGTH - 1) / RADIUS_VECTOR_LENGTH) * RADIUS_VECTOR_LENGTH;
  cryptvector = palloc(strlen(secret) + RADIUS_VECTOR_LENGTH);
  memcpy(cryptvector, secret, strlen(secret));

                                                                        
  md5trailer = packet->vector;
  for (i = 0; i < encryptedpasswordlen; i += RADIUS_VECTOR_LENGTH)
  {
    memcpy(cryptvector + strlen(secret), md5trailer, RADIUS_VECTOR_LENGTH);

       
                                                                       
                          
       
    md5trailer = encryptedpassword + i;

    if (!pg_md5_binary(cryptvector, strlen(secret) + RADIUS_VECTOR_LENGTH, encryptedpassword + i))
    {
      ereport(LOG, (errmsg("could not perform MD5 encryption of password")));
      pfree(cryptvector);
      pg_freeaddrinfo_all(hint.ai_family, serveraddrs);
      return STATUS_ERROR;
    }

    for (j = i; j < i + RADIUS_VECTOR_LENGTH; j++)
    {
      if (j < strlen(passwd))
      {
        encryptedpassword[j] = passwd[j] ^ encryptedpassword[j];
      }
      else
      {
        encryptedpassword[j] = '\0' ^ encryptedpassword[j];
      }
    }
  }
  pfree(cryptvector);

  radius_add_attribute(packet, RADIUS_PASSWORD, encryptedpassword, encryptedpasswordlen);

                                                       
  packetlength = packet->length;
  packet->length = pg_hton16(packet->length);

  sock = socket(serveraddrs[0].ai_family, SOCK_DGRAM, 0);
  if (sock == PGINVALID_SOCKET)
  {
    ereport(LOG, (errmsg("could not create RADIUS socket: %m")));
    pg_freeaddrinfo_all(hint.ai_family, serveraddrs);
    return STATUS_ERROR;
  }

  memset(&localaddr, 0, sizeof(localaddr));
#ifdef HAVE_IPV6
  localaddr.sin6_family = serveraddrs[0].ai_family;
  localaddr.sin6_addr = in6addr_any;
  if (localaddr.sin6_family == AF_INET6)
  {
    addrsize = sizeof(struct sockaddr_in6);
  }
  else
  {
    addrsize = sizeof(struct sockaddr_in);
  }
#else
  localaddr.sin_family = serveraddrs[0].ai_family;
  localaddr.sin_addr.s_addr = INADDR_ANY;
  addrsize = sizeof(struct sockaddr_in);
#endif

  if (bind(sock, (struct sockaddr *)&localaddr, addrsize))
  {
    ereport(LOG, (errmsg("could not bind local RADIUS socket: %m")));
    closesocket(sock);
    pg_freeaddrinfo_all(hint.ai_family, serveraddrs);
    return STATUS_ERROR;
  }

  if (sendto(sock, radius_buffer, packetlength, 0, serveraddrs[0].ai_addr, serveraddrs[0].ai_addrlen) < 0)
  {
    ereport(LOG, (errmsg("could not send RADIUS packet: %m")));
    closesocket(sock);
    pg_freeaddrinfo_all(hint.ai_family, serveraddrs);
    return STATUS_ERROR;
  }

                                             
  pg_freeaddrinfo_all(hint.ai_family, serveraddrs);

     
                                                                            
                                                                            
                                                                           
          
     
                                                                          
                                                           
                             
     
  gettimeofday(&endtime, NULL);
  endtime.tv_sec += RADIUS_TIMEOUT;

  while (true)
  {
    struct timeval timeout;
    struct timeval now;
    int64 timeoutval;

    gettimeofday(&now, NULL);
    timeoutval = (endtime.tv_sec * 1000000 + endtime.tv_usec) - (now.tv_sec * 1000000 + now.tv_usec);
    if (timeoutval <= 0)
    {
      ereport(LOG, (errmsg("timeout waiting for RADIUS response from %s", server)));
      closesocket(sock);
      return STATUS_ERROR;
    }
    timeout.tv_sec = timeoutval / 1000000;
    timeout.tv_usec = timeoutval % 1000000;

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    r = select(sock + 1, &fdset, NULL, NULL, &timeout);
    if (r < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

                                            
      ereport(LOG, (errmsg("could not check status on RADIUS socket: %m")));
      closesocket(sock);
      return STATUS_ERROR;
    }
    if (r == 0)
    {
      ereport(LOG, (errmsg("timeout waiting for RADIUS response from %s", server)));
      closesocket(sock);
      return STATUS_ERROR;
    }

       
                                                                     
       
                                                                         
                                                                        
                                                                         
                                                                      
                                                                        
                           
       

    addrsize = sizeof(remoteaddr);
    packetlength = recvfrom(sock, receive_buffer, RADIUS_BUFFER_SIZE, 0, (struct sockaddr *)&remoteaddr, &addrsize);
    if (packetlength < 0)
    {
      ereport(LOG, (errmsg("could not read RADIUS response: %m")));
      closesocket(sock);
      return STATUS_ERROR;
    }

#ifdef HAVE_IPV6
    if (remoteaddr.sin6_port != pg_hton16(port))
#else
    if (remoteaddr.sin_port != pg_hton16(port))
#endif
    {
#ifdef HAVE_IPV6
      ereport(LOG, (errmsg("RADIUS response from %s was sent from incorrect port: %d", server, pg_ntoh16(remoteaddr.sin6_port))));
#else
      ereport(LOG, (errmsg("RADIUS response from %s was sent from incorrect port: %d", server, pg_ntoh16(remoteaddr.sin_port))));
#endif
      continue;
    }

    if (packetlength < RADIUS_HEADER_LENGTH)
    {
      ereport(LOG, (errmsg("RADIUS response from %s too short: %d", server, packetlength)));
      continue;
    }

    if (packetlength != pg_ntoh16(receivepacket->length))
    {
      ereport(LOG, (errmsg("RADIUS response from %s has corrupt length: %d (actual length %d)", server, pg_ntoh16(receivepacket->length), packetlength)));
      continue;
    }

    if (packet->id != receivepacket->id)
    {
      ereport(LOG, (errmsg("RADIUS response from %s is to a different request: %d (should be %d)", server, receivepacket->id, packet->id)));
      continue;
    }

       
                                                                 
                                                                  
       
    cryptvector = palloc(packetlength + strlen(secret));

    memcpy(cryptvector, receivepacket, 4);                                             
    memcpy(cryptvector + 4, packet->vector, RADIUS_VECTOR_LENGTH);            
                                                                                          
                                                                                        
    if (packetlength > RADIUS_HEADER_LENGTH)                                          
                                                                                          
    {
      memcpy(cryptvector + RADIUS_HEADER_LENGTH, receive_buffer + RADIUS_HEADER_LENGTH, packetlength - RADIUS_HEADER_LENGTH);
    }
    memcpy(cryptvector + packetlength, secret, strlen(secret));

    if (!pg_md5_binary(cryptvector, packetlength + strlen(secret), encryptedpassword))
    {
      ereport(LOG, (errmsg("could not perform MD5 encryption of received packet")));
      pfree(cryptvector);
      continue;
    }
    pfree(cryptvector);

    if (memcmp(receivepacket->vector, encryptedpassword, RADIUS_VECTOR_LENGTH) != 0)
    {
      ereport(LOG, (errmsg("RADIUS response from %s has incorrect MD5 signature", server)));
      continue;
    }

    if (receivepacket->code == RADIUS_ACCESS_ACCEPT)
    {
      closesocket(sock);
      return STATUS_OK;
    }
    else if (receivepacket->code == RADIUS_ACCESS_REJECT)
    {
      closesocket(sock);
      return STATUS_EOF;
    }
    else
    {
      ereport(LOG, (errmsg("RADIUS response from %s has invalid code (%d) for user \"%s\"", server, receivepacket->code, user_name)));
      continue;
    }
  }                   
}
