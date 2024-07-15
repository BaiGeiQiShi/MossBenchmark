                                                                            
   
             
                                                    
   
                                                                         
                                                                        
   
                  
                                    
   
                                                                            
   

   
                      
                                  
                                                     
                                                                    
                                       
   

#include "postgres_fe.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>                                 
#include <sys/socket.h>
#ifdef HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif
#ifndef MAXHOSTNAMELEN
#include <netdb.h>                                 
#endif
#include <pwd.h>
#endif

#include "common/md5.h"
#include "common/scram-common.h"
#include "libpq-fe.h"
#include "fe-auth.h"

#ifdef ENABLE_GSS
   
                                 
   

#include "fe-gssapi-common.h"

   
                                                          
   
static int
pg_GSS_continue(PGconn *conn, int payloadlen)
{
  OM_uint32 maj_stat, min_stat, lmin_s;
  gss_buffer_desc ginbuf;
  gss_buffer_desc goutbuf;

     
                                                                          
                                    
     
  if (conn->gctx != GSS_C_NO_CONTEXT)
  {
    ginbuf.length = payloadlen;
    ginbuf.value = malloc(payloadlen);
    if (!ginbuf.value)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory allocating GSSAPI buffer (%d)\n"), payloadlen);
      return STATUS_ERROR;
    }
    if (pqGetnchar(ginbuf.value, payloadlen, conn))
    {
         
                                                                         
                                                       
         
      free(ginbuf.value);
      return STATUS_ERROR;
    }
  }
  else
  {
    ginbuf.length = 0;
    ginbuf.value = NULL;
  }

  maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL, &conn->gctx, conn->gtarg_nam, GSS_C_NO_OID, GSS_C_MUTUAL_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS, (ginbuf.value == NULL) ? GSS_C_NO_BUFFER : &ginbuf, NULL, &goutbuf, NULL, NULL);

  if (ginbuf.value)
  {
    free(ginbuf.value);
  }

  if (goutbuf.length != 0)
  {
       
                                                                           
                                                                       
               
       
    if (pqPacketSend(conn, 'p', goutbuf.value, goutbuf.length) != STATUS_OK)
    {
      gss_release_buffer(&lmin_s, &goutbuf);
      return STATUS_ERROR;
    }
  }
  gss_release_buffer(&lmin_s, &goutbuf);

  if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
  {
    pg_GSS_error(libpq_gettext("GSSAPI continuation error"), conn, maj_stat, min_stat);
    gss_release_name(&lmin_s, &conn->gtarg_nam);
    if (conn->gctx)
    {
      gss_delete_sec_context(&lmin_s, &conn->gctx, GSS_C_NO_BUFFER);
    }
    return STATUS_ERROR;
  }

  if (maj_stat == GSS_S_COMPLETE)
  {
    gss_release_name(&lmin_s, &conn->gtarg_nam);
  }

  return STATUS_OK;
}

   
                                         
   
static int
pg_GSS_startup(PGconn *conn, int payloadlen)
{
  int ret;
  char *host = conn->connhost[conn->whichhost].host;

  if (!(host && host[0] != '\0'))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("host name must be specified\n"));
    return STATUS_ERROR;
  }

  if (conn->gctx)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("duplicate GSS authentication request\n"));
    return STATUS_ERROR;
  }

  ret = pg_GSS_load_servicename(conn);
  if (ret != STATUS_OK)
  {
    return ret;
  }

     
                                                                         
              
     
  conn->gctx = GSS_C_NO_CONTEXT;

  return pg_GSS_continue(conn, payloadlen);
}
#endif                 

#ifdef ENABLE_SSPI
   
                                             
   

static void
pg_SSPI_error(PGconn *conn, const char *mprefix, SECURITY_STATUS r)
{
  char sysmsg[256];

  if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, r, 0, sysmsg, sizeof(sysmsg), NULL) == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, "%s: SSPI error %x\n", mprefix, (unsigned int)r);
  }
  else
  {
    printfPQExpBuffer(&conn->errorMessage, "%s: %s (%x)\n", mprefix, sysmsg, (unsigned int)r);
  }
}

   
                                                           
   
static int
pg_SSPI_continue(PGconn *conn, int payloadlen)
{
  SECURITY_STATUS r;
  CtxtHandle newContext;
  ULONG contextAttr;
  SecBufferDesc inbuf;
  SecBufferDesc outbuf;
  SecBuffer OutBuffers[1];
  SecBuffer InBuffers[1];
  char *inputbuf = NULL;

  if (conn->sspictx != NULL)
  {
       
                                                                        
                                           
       
    inputbuf = malloc(payloadlen);
    if (!inputbuf)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory allocating SSPI buffer (%d)\n"), payloadlen);
      return STATUS_ERROR;
    }
    if (pqGetnchar(inputbuf, payloadlen, conn))
    {
         
                                                                         
                                                       
         
      free(inputbuf);
      return STATUS_ERROR;
    }

    inbuf.ulVersion = SECBUFFER_VERSION;
    inbuf.cBuffers = 1;
    inbuf.pBuffers = InBuffers;
    InBuffers[0].pvBuffer = inputbuf;
    InBuffers[0].cbBuffer = payloadlen;
    InBuffers[0].BufferType = SECBUFFER_TOKEN;
  }

  OutBuffers[0].pvBuffer = NULL;
  OutBuffers[0].BufferType = SECBUFFER_TOKEN;
  OutBuffers[0].cbBuffer = 0;
  outbuf.cBuffers = 1;
  outbuf.pBuffers = OutBuffers;
  outbuf.ulVersion = SECBUFFER_VERSION;

  r = InitializeSecurityContext(conn->sspicred, conn->sspictx, conn->sspitarget, ISC_REQ_ALLOCATE_MEMORY, 0, SECURITY_NETWORK_DREP, (conn->sspictx == NULL) ? NULL : &inbuf, 0, &newContext, &outbuf, &contextAttr, NULL);

                                       
  if (inputbuf)
  {
    free(inputbuf);
  }

  if (r != SEC_E_OK && r != SEC_I_CONTINUE_NEEDED)
  {
    pg_SSPI_error(conn, libpq_gettext("SSPI continuation error"), r);

    return STATUS_ERROR;
  }

  if (conn->sspictx == NULL)
  {
                                                         
    conn->sspictx = malloc(sizeof(CtxtHandle));
    if (conn->sspictx == NULL)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
      return STATUS_ERROR;
    }
    memcpy(conn->sspictx, &newContext, sizeof(CtxtHandle));
  }

     
                                                                        
                                                  
     
  if (outbuf.cBuffers > 0)
  {
    if (outbuf.cBuffers != 1)
    {
         
                                                             
                                                                   
                                       
         
      printfPQExpBuffer(&conn->errorMessage, "SSPI returned invalid number of output buffers\n");
      return STATUS_ERROR;
    }

       
                                                                        
                                                                         
                
       
    if (outbuf.pBuffers[0].cbBuffer > 0)
    {
      if (pqPacketSend(conn, 'p', outbuf.pBuffers[0].pvBuffer, outbuf.pBuffers[0].cbBuffer))
      {
        FreeContextBuffer(outbuf.pBuffers[0].pvBuffer);
        return STATUS_ERROR;
      }
    }
    FreeContextBuffer(outbuf.pBuffers[0].pvBuffer);
  }

                                                      
  return STATUS_OK;
}

   
                                           
                                                                       
                                                                          
                                                                           
   
static int
pg_SSPI_startup(PGconn *conn, int use_negotiate, int payloadlen)
{
  SECURITY_STATUS r;
  TimeStamp expire;
  char *host = conn->connhost[conn->whichhost].host;

  if (conn->sspictx)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("duplicate SSPI authentication request\n"));
    return STATUS_ERROR;
  }

     
                                 
     
  conn->sspicred = malloc(sizeof(CredHandle));
  if (conn->sspicred == NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return STATUS_ERROR;
  }

  r = AcquireCredentialsHandle(NULL, use_negotiate ? "negotiate" : "kerberos", SECPKG_CRED_OUTBOUND, NULL, NULL, NULL, NULL, conn->sspicred, &expire);
  if (r != SEC_E_OK)
  {
    pg_SSPI_error(conn, libpq_gettext("could not acquire SSPI credentials"), r);
    free(conn->sspicred);
    conn->sspicred = NULL;
    return STATUS_ERROR;
  }

     
                                                                             
                                                                             
                                        
     
  if (!(host && host[0] != '\0'))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("host name must be specified\n"));
    return STATUS_ERROR;
  }
  conn->sspitarget = malloc(strlen(conn->krbsrvname) + strlen(host) + 2);
  if (!conn->sspitarget)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return STATUS_ERROR;
  }
  sprintf(conn->sspitarget, "%s/%s", conn->krbsrvname, host);

     
                                                                       
                                                              
     
  conn->usesspi = 1;

  return pg_SSPI_continue(conn, payloadlen);
}
#endif                  

   
                                            
   
static int
pg_SASL_init(PGconn *conn, int payloadlen)
{
  char *initialresponse = NULL;
  int initialresponselen;
  bool done;
  bool success;
  const char *selected_mechanism;
  PQExpBufferData mechanism_buf;
  char *password;

  initPQExpBuffer(&mechanism_buf);

  if (conn->sasl_state)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("duplicate SASL authentication request\n"));
    goto error;
  }

     
                                                             
                                                                       
                                                                      
                                                                        
     
  selected_mechanism = NULL;
  for (;;)
  {
    if (pqGets(&mechanism_buf, conn))
    {
      printfPQExpBuffer(&conn->errorMessage, "fe_sendauth: invalid authentication request from server: invalid list of authentication mechanisms\n");
      goto error;
    }
    if (PQExpBufferDataBroken(mechanism_buf))
    {
      goto oom_error;
    }

                                               
    if (mechanism_buf.data[0] == '\0')
    {
      break;
    }

       
                                                                           
                                                                        
                                                                           
                                                                       
                         
       
    if (strcmp(mechanism_buf.data, SCRAM_SHA_256_PLUS_NAME) == 0)
    {
      if (conn->ssl_in_use)
      {
           
                                                                    
                                                                     
                           
           
#ifdef HAVE_PGTLS_GET_PEER_CERTIFICATE_HASH
        selected_mechanism = SCRAM_SHA_256_PLUS_NAME;
#endif
      }
      else
      {
           
                                                                     
                                                                  
                                                                
                                                                      
                                                                     
                                                                  
                                                          
           
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server offered SCRAM-SHA-256-PLUS authentication over a non-SSL connection\n"));
        goto error;
      }
    }
    else if (strcmp(mechanism_buf.data, SCRAM_SHA_256_NAME) == 0 && !selected_mechanism)
    {
      selected_mechanism = SCRAM_SHA_256_NAME;
    }
  }

  if (!selected_mechanism)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("none of the server's SASL authentication mechanisms are supported\n"));
    goto error;
  }

     
                                                                   
                                       
     

     
                                                                        
                                                                          
                                                                         
     
  conn->password_needed = true;
  password = conn->connhost[conn->whichhost].password;
  if (password == NULL)
  {
    password = conn->pgpass;
  }
  if (password == NULL || password[0] == '\0')
  {
    printfPQExpBuffer(&conn->errorMessage, PQnoPasswordSupplied);
    goto error;
  }

     
                                                                             
                                  
     
                                                        
     
  conn->sasl_state = pg_fe_scram_init(conn, password, selected_mechanism);
  if (!conn->sasl_state)
  {
    goto oom_error;
  }

                                                                  
  pg_fe_scram_exchange(conn->sasl_state, NULL, -1, &initialresponse, &initialresponselen, &done, &success);

  if (done && !success)
  {
    goto error;
  }

     
                                                       
     
  if (pqPutMsgStart('p', true, conn))
  {
    goto error;
  }
  if (pqPuts(selected_mechanism, conn))
  {
    goto error;
  }
  if (initialresponse)
  {
    if (pqPutInt(initialresponselen, 4, conn))
    {
      goto error;
    }
    if (pqPutnchar(initialresponse, initialresponselen, conn))
    {
      goto error;
    }
  }
  if (pqPutMsgEnd(conn))
  {
    goto error;
  }
  if (pqFlush(conn))
  {
    goto error;
  }

  termPQExpBuffer(&mechanism_buf);
  if (initialresponse)
  {
    free(initialresponse);
  }

  return STATUS_OK;

error:
  termPQExpBuffer(&mechanism_buf);
  if (initialresponse)
  {
    free(initialresponse);
  }
  return STATUS_ERROR;

oom_error:
  termPQExpBuffer(&mechanism_buf);
  if (initialresponse)
  {
    free(initialresponse);
  }
  printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
  return STATUS_ERROR;
}

   
                                                                        
                                                                          
                 
   
static int
pg_SASL_continue(PGconn *conn, int payloadlen, bool final)
{
  char *output;
  int outputlen;
  bool done;
  bool success;
  int res;
  char *challenge;

                                                                            
  challenge = malloc(payloadlen + 1);
  if (!challenge)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory allocating SASL buffer (%d)\n"), payloadlen);
    return STATUS_ERROR;
  }

  if (pqGetnchar(challenge, payloadlen, conn))
  {
    free(challenge);
    return STATUS_ERROR;
  }
                                                                         
  challenge[payloadlen] = '\0';

  pg_fe_scram_exchange(conn->sasl_state, challenge, payloadlen, &output, &outputlen, &done, &success);
  free(challenge);                                   

  if (final && !done)
  {
    if (outputlen != 0)
    {
      free(output);
    }

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("AuthenticationSASLFinal received from server, but SASL authentication was not completed\n"));
    return STATUS_ERROR;
  }
  if (outputlen != 0)
  {
       
                                             
       
    res = pqPacketSend(conn, 'p', output, outputlen);
    free(output);

    if (res != STATUS_OK)
    {
      return STATUS_ERROR;
    }
  }

  if (done && !success)
  {
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

   
                                            
   
                                                                             
                                                                             
                                                                           
                                                                        
                                                  
   
static int
pg_local_sendauth(PGconn *conn)
{
#ifdef HAVE_STRUCT_CMSGCRED
  char buf;
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  union
  {
    struct cmsghdr hdr;
    unsigned char buf[CMSG_SPACE(sizeof(struct cmsgcred))];
  } cmsgbuf;

     
                                                                          
                                                            
     
  buf = '\0';
  iov.iov_base = &buf;
  iov.iov_len = 1;

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

                                                                 
  memset(&cmsgbuf, 0, sizeof(cmsgbuf));
  msg.msg_control = &cmsgbuf.buf;
  msg.msg_controllen = sizeof(cmsgbuf.buf);
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len = CMSG_LEN(sizeof(struct cmsgcred));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_CREDS;

  if (sendmsg(conn->sock, &msg, 0) == -1)
  {
    char sebuf[PG_STRERROR_R_BUFLEN];

    printfPQExpBuffer(&conn->errorMessage, "pg_local_sendauth: sendmsg: %s\n", strerror_r(errno, sebuf, sizeof(sebuf)));
    return STATUS_ERROR;
  }
  return STATUS_OK;
#else
  printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SCM_CRED authentication method not supported\n"));
  return STATUS_ERROR;
#endif
}

static int
pg_password_sendauth(PGconn *conn, const char *password, AuthRequest areq)
{
  int ret;
  char *crypt_pwd = NULL;
  const char *pwd_to_send;
  char md5Salt[4];

                                                                 
  if (areq == AUTH_REQ_MD5)
  {
    if (pqGetnchar(md5Salt, 4, conn))
    {
      return STATUS_ERROR;                       
    }
  }

                                       

  switch (areq)
  {
  case AUTH_REQ_MD5:
  {
    char *crypt_pwd2;

                                                  
    crypt_pwd = malloc(2 * (MD5_PASSWD_LEN + 1));
    if (!crypt_pwd)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
      return STATUS_ERROR;
    }

    crypt_pwd2 = crypt_pwd + MD5_PASSWD_LEN + 1;
    if (!pg_md5_encrypt(password, conn->pguser, strlen(conn->pguser), crypt_pwd2))
    {
      free(crypt_pwd);
      return STATUS_ERROR;
    }
    if (!pg_md5_encrypt(crypt_pwd2 + strlen("md5"), md5Salt, 4, crypt_pwd))
    {
      free(crypt_pwd);
      return STATUS_ERROR;
    }

    pwd_to_send = crypt_pwd;
    break;
  }
  case AUTH_REQ_PASSWORD:
    pwd_to_send = password;
    break;
  default:
    return STATUS_ERROR;
  }
                                                    
  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    ret = pqPacketSend(conn, 'p', pwd_to_send, strlen(pwd_to_send) + 1);
  }
  else
  {
    ret = pqPacketSend(conn, 0, pwd_to_send, strlen(pwd_to_send) + 1);
  }
  if (crypt_pwd)
  {
    free(crypt_pwd);
  }
  return ret;
}

   
                  
                                                                  
   
                                                                       
                                                                          
                                                                        
                                                                         
                                                                         
                
   
int
pg_fe_sendauth(AuthRequest areq, int payloadlen, PGconn *conn)
{
  switch (areq)
  {
  case AUTH_REQ_OK:
    break;

  case AUTH_REQ_KRB4:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("Kerberos 4 authentication not supported\n"));
    return STATUS_ERROR;

  case AUTH_REQ_KRB5:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("Kerberos 5 authentication not supported\n"));
    return STATUS_ERROR;

#if defined(ENABLE_GSS) || defined(ENABLE_SSPI)
  case AUTH_REQ_GSS:
#if !defined(ENABLE_SSPI)
                                                      
  case AUTH_REQ_SSPI:
#endif
  {
    int r;

    pglock_thread();

       
                                                                  
                                                               
                                                                  
                                                                  
                                                           
                                       
       
#if defined(ENABLE_GSS) && defined(ENABLE_SSPI)
    if (conn->gsslib && (pg_strcasecmp(conn->gsslib, "gssapi") == 0))
    {
      r = pg_GSS_startup(conn, payloadlen);
    }
    else
    {
      r = pg_SSPI_startup(conn, 0, payloadlen);
    }
#elif defined(ENABLE_GSS) && !defined(ENABLE_SSPI)
    r = pg_GSS_startup(conn, payloadlen);
#elif !defined(ENABLE_GSS) && defined(ENABLE_SSPI)
    r = pg_SSPI_startup(conn, 0, payloadlen);
#endif
    if (r != STATUS_OK)
    {
                                            
      pgunlock_thread();
      return STATUS_ERROR;
    }
    pgunlock_thread();
  }
  break;

  case AUTH_REQ_GSS_CONT:
  {
    int r;

    pglock_thread();
#if defined(ENABLE_GSS) && defined(ENABLE_SSPI)
    if (conn->usesspi)
    {
      r = pg_SSPI_continue(conn, payloadlen);
    }
    else
    {
      r = pg_GSS_continue(conn, payloadlen);
    }
#elif defined(ENABLE_GSS) && !defined(ENABLE_SSPI)
    r = pg_GSS_continue(conn, payloadlen);
#elif !defined(ENABLE_GSS) && defined(ENABLE_SSPI)
    r = pg_SSPI_continue(conn, payloadlen);
#endif
    if (r != STATUS_OK)
    {
                                            
      pgunlock_thread();
      return STATUS_ERROR;
    }
    pgunlock_thread();
  }
  break;
#else                                                   
                                     
  case AUTH_REQ_GSS:
  case AUTH_REQ_GSS_CONT:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("GSSAPI authentication not supported\n"));
    return STATUS_ERROR;
#endif                                                  

#ifdef ENABLE_SSPI
  case AUTH_REQ_SSPI:

       
                                                                  
                                                                    
                                        
       
    pglock_thread();
    if (pg_SSPI_startup(conn, 1, payloadlen) != STATUS_OK)
    {
                                            
      pgunlock_thread();
      return STATUS_ERROR;
    }
    pgunlock_thread();
    break;
#else

       
                                                                
                                                                     
                                                                    
                  
       
#if !defined(ENABLE_GSS)
  case AUTH_REQ_SSPI:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSPI authentication not supported\n"));
    return STATUS_ERROR;
#endif                          
#endif                  

  case AUTH_REQ_CRYPT:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("Crypt authentication not supported\n"));
    return STATUS_ERROR;

  case AUTH_REQ_MD5:
  case AUTH_REQ_PASSWORD:
  {
    char *password;

    conn->password_needed = true;
    password = conn->connhost[conn->whichhost].password;
    if (password == NULL)
    {
      password = conn->pgpass;
    }
    if (password == NULL || password[0] == '\0')
    {
      printfPQExpBuffer(&conn->errorMessage, PQnoPasswordSupplied);
      return STATUS_ERROR;
    }
    if (pg_password_sendauth(conn, password, areq) != STATUS_OK)
    {
      printfPQExpBuffer(&conn->errorMessage, "fe_sendauth: error sending password authentication\n");
      return STATUS_ERROR;
    }
    break;
  }

  case AUTH_REQ_SASL:

       
                                                                  
                                 
       
    if (pg_SASL_init(conn, payloadlen) != STATUS_OK)
    {
                                                      
      return STATUS_ERROR;
    }
    break;

  case AUTH_REQ_SASL_CONT:
  case AUTH_REQ_SASL_FIN:
    if (conn->sasl_state == NULL)
    {
      printfPQExpBuffer(&conn->errorMessage, "fe_sendauth: invalid authentication request from server: AUTH_REQ_SASL_CONT without AUTH_REQ_SASL\n");
      return STATUS_ERROR;
    }
    if (pg_SASL_continue(conn, payloadlen, (areq == AUTH_REQ_SASL_FIN)) != STATUS_OK)
    {
                                             
      if (conn->errorMessage.len == 0)
      {
        printfPQExpBuffer(&conn->errorMessage, "fe_sendauth: error in SASL authentication\n");
      }
      return STATUS_ERROR;
    }
    break;

  case AUTH_REQ_SCM_CREDS:
    if (pg_local_sendauth(conn) != STATUS_OK)
    {
      return STATUS_ERROR;
    }
    break;

  default:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("authentication method %u not supported\n"), areq);
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

   
                     
   
                                                                         
                                                                        
                                                                         
   
char *
pg_fe_getauthname(PQExpBuffer errorMessage)
{
  char *result = NULL;
  const char *name = NULL;

#ifdef WIN32
                                                                      
  char username[256 + 1];
  DWORD namesize = sizeof(username);
#else
  uid_t user_id = geteuid();
  char pwdbuf[BUFSIZ];
  struct passwd pwdstr;
  struct passwd *pw = NULL;
  int pwerr;
#endif

     
                                                                        
                                                                
                                                                         
                                                                           
                                                              
     
  pglock_thread();

#ifdef WIN32
  if (GetUserName(username, &namesize))
  {
    name = username;
  }
  else if (errorMessage)
  {
    printfPQExpBuffer(errorMessage, libpq_gettext("user name lookup failure: error code %lu\n"), GetLastError());
  }
#else
  pwerr = pqGetpwuid(user_id, &pwdstr, pwdbuf, sizeof(pwdbuf), &pw);
  if (pw != NULL)
  {
    name = pw->pw_name;
  }
  else if (errorMessage)
  {
    if (pwerr != 0)
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("could not look up local user ID %d: %s\n"), (int)user_id, strerror_r(pwerr, pwdbuf, sizeof(pwdbuf)));
    }
    else
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("local user with ID %d does not exist\n"), (int)user_id);
    }
  }
#endif

  if (name)
  {
    result = strdup(name);
    if (result == NULL && errorMessage)
    {
      printfPQExpBuffer(errorMessage, libpq_gettext("out of memory\n"));
    }
  }

  pgunlock_thread();

  return result;
}

   
                                                                        
   
                                                                     
                                                                    
                                                          
                                  
   
char *
PQencryptPassword(const char *passwd, const char *user)
{
  char *crypt_pwd;

  crypt_pwd = malloc(MD5_PASSWD_LEN + 1);
  if (!crypt_pwd)
  {
    return NULL;
  }

  if (!pg_md5_encrypt(passwd, user, strlen(user), crypt_pwd))
  {
    free(crypt_pwd);
    return NULL;
  }

  return crypt_pwd;
}

   
                                                                   
   
                                                                        
                                                                       
                                                                        
                                                                        
                                                                        
                                                                        
                      
   
                                                                      
                                                                        
                                                                        
                                                                        
                                                          
                                                                         
                             
   
                                                                        
                                                                       
                                                                      
                 
   
char *
PQencryptPasswordConn(PGconn *conn, const char *passwd, const char *user, const char *algorithm)
{
#define MAX_ALGORITHM_NAME_LEN 50
  char algobuf[MAX_ALGORITHM_NAME_LEN + 1];
  char *crypt_pwd = NULL;

  if (!conn)
  {
    return NULL;
  }

                                                  
  if (algorithm == NULL)
  {
    PGresult *res;
    char *val;

    res = PQexec(conn, "show password_encryption");
    if (res == NULL)
    {
                                                             
      return NULL;
    }
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
                                                             
      PQclear(res);
      return NULL;
    }
    if (PQntuples(res) != 1 || PQnfields(res) != 1)
    {
      PQclear(res);
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unexpected shape of result set returned for SHOW\n"));
      return NULL;
    }
    val = PQgetvalue(res, 0, 0);

    if (strlen(val) > MAX_ALGORITHM_NAME_LEN)
    {
      PQclear(res);
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("password_encryption value too long\n"));
      return NULL;
    }
    strcpy(algobuf, val);
    PQclear(res);

    algorithm = algobuf;
  }

     
                                                              
                                                                           
                                                          
     
  if (strcmp(algorithm, "on") == 0 || strcmp(algorithm, "off") == 0)
  {
    algorithm = "md5";
  }

     
                                           
     
  if (strcmp(algorithm, "scram-sha-256") == 0)
  {
    crypt_pwd = pg_fe_scram_build_verifier(passwd);
  }
  else if (strcmp(algorithm, "md5") == 0)
  {
    crypt_pwd = malloc(MD5_PASSWD_LEN + 1);
    if (crypt_pwd)
    {
      if (!pg_md5_encrypt(passwd, user, strlen(user), crypt_pwd))
      {
        free(crypt_pwd);
        crypt_pwd = NULL;
      }
    }
  }
  else
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unrecognized password encryption algorithm \"%s\"\n"), algorithm);
    return NULL;
  }

  if (!crypt_pwd)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
  }

  return crypt_pwd;
}
