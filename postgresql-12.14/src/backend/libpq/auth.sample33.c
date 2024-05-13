/*-------------------------------------------------------------------------
 *
 * auth.c
 *	  Routines to handle network authentication
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/libpq/auth.c
 *
 *-------------------------------------------------------------------------
 */

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

/*----------------------------------------------------------------
 * Global authentication functions
 *----------------------------------------------------------------
 */
static void
sendAuthRequest(Port *port, AuthRequest areq, const char *extradata, int extralen);
static void
auth_failed(Port *port, int status, char *logdetail);
static char *
recv_password_packet(Port *port);

/*----------------------------------------------------------------
 * Password-based authentication methods (password, md5, and scram-sha-256)
 *----------------------------------------------------------------
 */
static int
CheckPasswordAuth(Port *port, char **logdetail);
static int
CheckPWChallengeAuth(Port *port, char **logdetail);

static int
CheckMD5Auth(Port *port, char *shadow_pass, char **logdetail);
static int
CheckSCRAMAuth(Port *port, char *shadow_pass, char **logdetail);

/*----------------------------------------------------------------
 * Ident authentication
 *----------------------------------------------------------------
 */
/* Max size of username ident server can return */
#define IDENT_USERNAME_MAX 512

/* Standard TCP port number for Ident service.  Assigned by IANA */
#define IDENT_PORT 113

static int
ident_inet(hbaPort *port);

#ifdef HAVE_UNIX_SOCKETS
static int
auth_peer(hbaPort *port);
#endif

/*----------------------------------------------------------------
 * PAM authentication
 *----------------------------------------------------------------
 */
#ifdef USE_PAM
#ifdef HAVE_PAM_PAM_APPL_H
#include <pam/pam_appl.h>
#endif
#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif

#define PGSQL_PAM_SERVICE "postgresql" /* Service name passed to PAM */

static int
CheckPAMAuth(Port *port, const char *user, const char *password);
static int
pam_passwd_conv_proc(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr);

static struct pam_conv pam_passw_conv = {&pam_passwd_conv_proc, NULL};

static const char *pam_passwd = NULL; /* Workaround for Solaris 2.6
                                       * brokenness */
static Port *pam_port_cludge;         /* Workaround for passing "Port *port" into
                                       * pam_passwd_conv_proc */
static bool pam_no_password;          /* For detecting no-password-given */
#endif                                /* USE_PAM */

/*----------------------------------------------------------------
 * BSD authentication
 *----------------------------------------------------------------
 */
#ifdef USE_BSD_AUTH
#include <bsd_auth.h>

static int
CheckBSDAuth(Port *port, char *user);
#endif /* USE_BSD_AUTH */

/*----------------------------------------------------------------
 * LDAP authentication
 *----------------------------------------------------------------
 */
#ifdef USE_LDAP
#ifndef WIN32
/* We use a deprecated function to keep the codepath the same as win32. */
#define LDAP_DEPRECATED 1
#include <ldap.h>
#else
#include <winldap.h>

/* Correct header from the Platform SDK */
typedef ULONG (*__ldap_start_tls_sA)(IN PLDAP ExternalHandle, OUT PULONG ServerReturnValue, OUT LDAPMessage **result, IN PLDAPControlA *ServerControls, IN PLDAPControlA *ClientControls);
#endif

static int
CheckLDAPAuth(Port *port);

/* LDAP_OPT_DIAGNOSTIC_MESSAGE is the newer spelling */
#ifndef LDAP_OPT_DIAGNOSTIC_MESSAGE
#define LDAP_OPT_DIAGNOSTIC_MESSAGE LDAP_OPT_ERROR_STRING
#endif

#endif /* USE_LDAP */

/*----------------------------------------------------------------
 * Cert authentication
 *----------------------------------------------------------------
 */
#ifdef USE_SSL
static int
CheckCertAuth(Port *port);
#endif

/*----------------------------------------------------------------
 * Kerberos and GSSAPI GUCs
 *----------------------------------------------------------------
 */
char *pg_krb_server_keyfile;
bool pg_krb_caseins_users;

/*----------------------------------------------------------------
 * GSSAPI Authentication
 *----------------------------------------------------------------
 */
#ifdef ENABLE_GSS
#include "libpq/be-gssapi-common.h"

static int
pg_GSS_checkauth(Port *port);
static int
pg_GSS_recvauth(Port *port);
#endif /* ENABLE_GSS */

/*----------------------------------------------------------------
 * SSPI Authentication
 *----------------------------------------------------------------
 */
#ifdef ENABLE_SSPI
typedef SECURITY_STATUS(WINAPI *QUERY_SECURITY_CONTEXT_TOKEN_FN)(PCtxtHandle, void **);
static int
pg_SSPI_recvauth(Port *port);
static int
pg_SSPI_make_upn(char *accountname, size_t accountnamesize, char *domainname, size_t domainnamesize, bool update_accountname);
#endif

/*----------------------------------------------------------------
 * RADIUS Authentication
 *----------------------------------------------------------------
 */
static int
CheckRADIUSAuth(Port *port);
static int
PerformRadiusTransaction(const char *server, const char *secret, const char *portstr, const char *identifier, const char *user_name, const char *passwd);

/*
 * Maximum accepted size of GSS and SSPI authentication tokens.
 *
 * Kerberos tickets are usually quite small, but the TGTs issued by Windows
 * domain controllers include an authorization field known as the Privilege
 * Attribute Certificate (PAC), which contains the user's Windows permissions
 * (group memberships etc.). The PAC is copied into all tickets obtained on
 * the basis of this TGT (even those issued by Unix realms which the Windows
 * realm trusts), and can be several kB in size. The maximum token size
 * accepted by Windows systems is determined by the MaxAuthToken Windows
 * registry setting. Microsoft recommends that it is not set higher than
 * 65535 bytes, so that seems like a reasonable limit for us as well.
 */
#define PG_MAX_AUTH_TOKEN_LENGTH 65535

/*
 * Maximum accepted size of SASL messages.
 *
 * The messages that the server or libpq generate are much smaller than this,
 * but have some headroom.
 */
#define PG_MAX_SASL_MESSAGE_LENGTH 1024

/*----------------------------------------------------------------
 * Global authentication functions
 *----------------------------------------------------------------
 */

/*
 * This hook allows plugins to get control following client authentication,
 * but before the user has been informed about the results.  It could be used
 * to record login events, insert a delay after failed authentication, etc.
 */
ClientAuthentication_hook_type ClientAuthentication_hook = NULL;

/*
 * Tell the user the authentication failed, but not (much about) why.
 *
 * There is a tradeoff here between security concerns and making life
 * unnecessarily difficult for legitimate users.  We would not, for example,
 * want to report the password we were expecting to receive...
 * But it seems useful to report the username and authorization method
 * in use, and these are items that must be presumed known to an attacker
 * anyway.
 * Note that many sorts of failure report additional information in the
 * postmaster log, which we hope is only readable by good guys.  In
 * particular, if logdetail isn't NULL, we send that string to the log.
 */
static void
auth_failed(Port *port, int status, char *logdetail)
{
















































































}

/*
 * Client authentication starts here.  If there is an error, this
 * function does not return and the backend process is terminated.
 */
void
ClientAuthentication(Port *port)
{
  int status = STATUS_ERROR;
  char *logdetail = NULL;

  /*
   * Get the authentication method to use for this frontend/database
   * combination.  Note: we do not parse the file at this point; this has
   * already been done elsewhere.  hba.c dropped an error message into the
   * server logfile if parsing the hba config file failed.
   */
  hba_getauthmethod(port);

  CHECK_FOR_INTERRUPTS();

  /*
   * This is the first point where we have access to the hba record for the
   * current connection, so perform any verifications based on the hba
   * options field that should be done *before* the authentication here.
   */
  if (port->hba->clientcert != clientCertOff)
  {
    /* If we haven't loaded a root certificate store, fail */





    /*
     * If we loaded a root certificate store, and if a certificate is
     * present on the client, then it has been verified against our root
     * certificate store, and the connection would have been aborted
     * already if it didn't verify ok.
     */




  }

  /*
   * Now proceed to do the actual authentication check
   */
  switch (port->hba->auth_method)
  {
  case uaReject:;;

    /*
     * An explicit "reject" entry in pg_hba.conf.  This report exposes
     * the fact that there's an explicit reject entry, which is
     * perhaps not so desirable from a security standpoint; but the
     * message for an implicit reject could confuse the DBA a lot when
     * the true situation is a match to an explicit reject.  And we
     * don't want to change the message for an implicit reject.  As
     * noted below, the additional information shown here doesn't
     * expose anything not known to an attacker.
     */
    {
      char hostinfo[NI_MAXHOST];
      const char *encryption_state;






















    }

  case uaImplicitReject:;;

    /*
     * No matching entry, so tell the user we fell through.
     *
     * NOTE: the extra info reported here is not a security breach,
     * because all that info is known at the frontend and must be
     * assumed known to bad guys.  We're merely helping out the less
     * clueful good guys.
     */
    {
      char hostinfo[NI_MAXHOST];
      const char *encryption_state;













#define HOSTNAME_LOOKUP_DETAIL(port)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  (port->remote_hostname ? (port->remote_hostname_resolv == +1      ? errdetail_log("Client IP address resolved to \"%s\", forward "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                                                         "lookup matches.",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
                                                                          port->remote_hostname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                               : port->remote_hostname_resolv == 0  ? errdetail_log("Client IP address resolved to \"%s\", forward "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                                                     "lookup not checked.",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                          port->remote_hostname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                               : port->remote_hostname_resolv == -1 ? errdetail_log("Client IP address resolved to \"%s\", forward "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                                                    "lookup does not match.",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
                                                                          port->remote_hostname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                               : port->remote_hostname_resolv == -2 ? errdetail_log("Could not translate client host name \"%s\" "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
                                                                                    "to IP address: %s.",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
                                                                          port->remote_hostname, gai_strerror(port->remote_hostname_errcode))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
                                                                    : 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
                         : (port->remote_hostname_resolv == -2 ? errdetail_log("Could not resolve client IP address to a host name: %s.", gai_strerror(port->remote_hostname_errcode)) : 0))










    }

  case uaGSS:;;
#ifdef ENABLE_GSS
    /* We might or might not have the gss workspace already */
    if (port->gss == NULL)
    {
      port->gss = (pg_gssinfo *)MemoryContextAllocZero(TopMemoryContext, sizeof(pg_gssinfo));
    }
    port->gss->auth = true;

    /*
     * If GSS state was set up while enabling encryption, we can just
     * check the client's principal.  Otherwise, ask for it.
     */
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

#endif


  case uaSSPI:;;
#ifdef ENABLE_SSPI
    if (port->gss == NULL)
    {
      port->gss = (pg_gssinfo *)MemoryContextAllocZero(TopMemoryContext, sizeof(pg_gssinfo));
    }
    sendAuthRequest(port, AUTH_REQ_SSPI, NULL, 0);
    status = pg_SSPI_recvauth(port);
#else

#endif


  case uaPeer:;;
#ifdef HAVE_UNIX_SOCKETS

#else
    Assert(false);
#endif
    break;

  case uaIdent:;;



  case uaMD5:;;
  case uaSCRAM:;;



  case uaPassword:;;



  case uaPAM:;;
#ifdef USE_PAM
    status = CheckPAMAuth(port, port->user_name, "");
#else

#endif /* USE_PAM */


  case uaBSD:;;
#ifdef USE_BSD_AUTH
    status = CheckBSDAuth(port, port->user_name);
#else

#endif /* USE_BSD_AUTH */


  case uaLDAP:;;
#ifdef USE_LDAP
    status = CheckLDAPAuth(port);
#else

#endif

  case uaRADIUS:;;


  case uaCert:;;
    /* uaCert will be treated as if clientcert=verify-full (uaTrust) */
  case uaTrust:;;
    status = STATUS_OK;
    break;
  }

  if ((status == STATUS_OK && port->hba->clientcert == clientCertFull) || port->hba->auth_method == uaCert)
  {
    /*
     * Make sure we only check the certificate if we use the cert method
     * or verify-full option.
     */
#ifdef USE_SSL
    status = CheckCertAuth(port);
#else

#endif
  }

  if (ClientAuthentication_hook)
  {

  }

  if (status == STATUS_OK)
  {
    sendAuthRequest(port, AUTH_REQ_OK, NULL, 0);
  }
  else
  {

  }
}

/*
 * Send an authentication request packet to the frontend.
 */
static void
sendAuthRequest(Port *port, AuthRequest areq, const char *extradata, int extralen)
{
  StringInfoData buf;

  CHECK_FOR_INTERRUPTS();

  pq_beginmessage(&buf, 'R');
  pq_sendint32(&buf, (int32)areq);
  if (extralen > 0)
  {

  }

  pq_endmessage(&buf);

  /*
   * Flush message so client will see it, except for AUTH_REQ_OK and
   * AUTH_REQ_SASL_FIN, which need not be sent until we are ready for
   * queries.
   */
  if (areq != AUTH_REQ_OK && areq != AUTH_REQ_SASL_FIN)
  {

  }

  CHECK_FOR_INTERRUPTS();
}

/*
 * Collect password response packet from frontend.
 *
 * Returns NULL if couldn't get password, else palloc'd string.
 */
static char *
recv_password_packet(Port *port)
{














































































}

/*----------------------------------------------------------------
 * Password-based authentication mechanisms
 *----------------------------------------------------------------
 */

/*
 * Plaintext password authentication.
 */
static int
CheckPasswordAuth(Port *port, char **logdetail)
{





























}

/*
 * MD5 and SCRAM authentication.
 */
static int
CheckPWChallengeAuth(Port *port, char **logdetail)
{




























































}

static int
CheckMD5Auth(Port *port, char *shadow_pass, char **logdetail)
{




































}

static int
CheckSCRAMAuth(Port *port, char *shadow_pass, char **logdetail)
{































































































































































}

/*----------------------------------------------------------------
 * GSSAPI authentication system
 *----------------------------------------------------------------
 */
#ifdef ENABLE_GSS
static int
pg_GSS_recvauth(Port *port)
{
  OM_uint32 maj_stat, min_stat, lmin_s, gflags;
  int mtype;
  StringInfoData buf;
  gss_buffer_desc gbuf;

  /*
   * GSS auth is not supported for protocol versions before 3, because it
   * relies on the overall message length word to determine the GSS payload
   * size in AuthenticationGSSContinue and PasswordMessage messages. (This
   * is, in fact, a design error in our GSS support, because protocol
   * messages are supposed to be parsable without relying on the length
   * word; but it's not worth changing it now.)
   */
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("GSSAPI is not supported in protocol version 2")));
  }

  /*
   * Use the configured keytab, if there is one.  Unfortunately, Heimdal
   * doesn't support the cred store extensions, so use the env var.
   */
  if (pg_krb_server_keyfile != NULL && pg_krb_server_keyfile[0] != '\0')
  {
    if (setenv("KRB5_KTNAME", pg_krb_server_keyfile, 1) != 0)
    {
      /* The only likely failure cause is OOM, so use that errcode */
      ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("could not set environment: %m")));
    }
  }

  /*
   * We accept any service principal that's present in our keytab. This
   * increases interoperability between kerberos implementations that see
   * for example case sensitivity differently, while not really opening up
   * any vector of attack.
   */
  port->gss->cred = GSS_C_NO_CREDENTIAL;

  /*
   * Initialize sequence with an empty context
   */
  port->gss->ctx = GSS_C_NO_CONTEXT;

  /*
   * Loop through GSSAPI message exchange. This exchange can consist of
   * multiple messages sent in both directions. First message is always from
   * the client. All messages from client to server are password packets
   * (type 'p').
   */
  do
  {
    pq_startmsgread();

    CHECK_FOR_INTERRUPTS();

    mtype = pq_getbyte();
    if (mtype != 'p')
    {
      /* Only log error if client didn't disconnect. */
      if (mtype != EOF)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("expected GSS response, got message type %d", mtype)));
      }
      return STATUS_ERROR;
    }

    /* Get the actual GSS token */
    initStringInfo(&buf);
    if (pq_getmessage(&buf, PG_MAX_AUTH_TOKEN_LENGTH))
    {
      /* EOF - pq_getmessage already logged error */
      pfree(buf.data);
      return STATUS_ERROR;
    }

    /* Map to GSSAPI style buffer */
    gbuf.length = buf.len;
    gbuf.value = buf.data;

    elog(DEBUG4, "Processing received GSS token of length %u", (unsigned int)gbuf.length);

    maj_stat = gss_accept_sec_context(&min_stat, &port->gss->ctx, port->gss->cred, &gbuf, GSS_C_NO_CHANNEL_BINDINGS, &port->gss->name, NULL, &port->gss->outbuf, &gflags, NULL, NULL);

    /* gbuf no longer used */
    pfree(buf.data);

    elog(DEBUG5, "gss_accept_sec_context major: %d, minor: %d, outlen: %u, outflags: %x", maj_stat, min_stat, (unsigned int)port->gss->outbuf.length, gflags);

    CHECK_FOR_INTERRUPTS();

    if (port->gss->outbuf.length != 0)
    {
      /*
       * Negotiation generated data to be sent to the client.
       */
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
    /*
     * Release service principal credentials
     */
    gss_release_cred(&min_stat, &port->gss->cred);
  }
  return pg_GSS_checkauth(port);
}

/*
 * Check whether the GSSAPI-authenticated user is allowed to connect as the
 * claimed username.
 */
static int
pg_GSS_checkauth(Port *port)
{
  int ret;
  OM_uint32 maj_stat, min_stat, lmin_s;
  gss_buffer_desc gbuf;
  char *princ;

  /*
   * Get the name of the user that authenticated, and compare it to the pg
   * username that was specified for the connection.
   */
  maj_stat = gss_display_name(&min_stat, port->gss->name, &gbuf, NULL);
  if (maj_stat != GSS_S_COMPLETE)
  {
    pg_GSS_error(_("retrieving GSS user name failed"), maj_stat, min_stat);
    return STATUS_ERROR;
  }

  /*
   * gbuf.value might not be null-terminated, so turn it into a regular
   * null-terminated string.
   */
  princ = palloc(gbuf.length + 1);
  memcpy(princ, gbuf.value, gbuf.length);
  princ[gbuf.length] = '\0';
  gss_release_buffer(&lmin_s, &gbuf);

  /*
   * Copy the original name of the authenticated principal into our backend
   * memory for display later.
   */
  port->gss->princ = MemoryContextStrdup(TopMemoryContext, princ);

  /*
   * Split the username at the realm separator
   */
  if (strchr(princ, '@'))
  {
    char *cp = strchr(princ, '@');

    /*
     * If we are not going to include the realm in the username that is
     * passed to the ident map, destructively modify it here to remove the
     * realm. Then advance past the separator to check the realm.
     */
    if (!port->hba->include_realm)
    {
      *cp = '\0';
    }
    cp++;

    if (port->hba->krb_realm != NULL && strlen(port->hba->krb_realm))
    {
      /*
       * Match the realm part of the name first
       */
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
        /* GSS realm does not match */
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
#endif /* ENABLE_GSS */

/*----------------------------------------------------------------
 * SSPI authentication system
 *----------------------------------------------------------------
 */
#ifdef ENABLE_SSPI

/*
 * Generate an error for SSPI authentication.  The caller should apply
 * _() to errmsg to make it translatable.
 */
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

  /*
   * SSPI auth is not supported for protocol versions before 3, because it
   * relies on the overall message length word to determine the SSPI payload
   * size in AuthenticationGSSContinue and PasswordMessage messages. (This
   * is, in fact, a design error in our SSPI support, because protocol
   * messages are supposed to be parsable without relying on the length
   * word; but it's not worth changing it now.)
   */
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("SSPI is not supported in protocol version 2")));
  }

  /*
   * Acquire a handle to the server credentials.
   */
  r = AcquireCredentialsHandle(NULL, "negotiate", SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &sspicred, &expiry);
  if (r != SEC_E_OK)
  {
    pg_SSPI_error(ERROR, _("could not acquire SSPI credentials"), r);
  }

  /*
   * Loop through SSPI message exchange. This exchange can consist of
   * multiple messages sent in both directions. First message is always from
   * the client. All messages from client to server are password packets
   * (type 'p').
   */
  do
  {
    pq_startmsgread();
    mtype = pq_getbyte();
    if (mtype != 'p')
    {
      /* Only log error if client didn't disconnect. */
      if (mtype != EOF)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("expected SSPI response, got message type %d", mtype)));
      }
      return STATUS_ERROR;
    }

    /* Get the actual SSPI token */
    initStringInfo(&buf);
    if (pq_getmessage(&buf, PG_MAX_AUTH_TOKEN_LENGTH))
    {
      /* EOF - pq_getmessage already logged error */
      pfree(buf.data);
      return STATUS_ERROR;
    }

    /* Map to SSPI style buffer */
    inbuf.ulVersion = SECBUFFER_VERSION;
    inbuf.cBuffers = 1;
    inbuf.pBuffers = InBuffers;
    InBuffers[0].pvBuffer = buf.data;
    InBuffers[0].cbBuffer = buf.len;
    InBuffers[0].BufferType = SECBUFFER_TOKEN;

    /* Prepare output buffer */
    OutBuffers[0].pvBuffer = NULL;
    OutBuffers[0].BufferType = SECBUFFER_TOKEN;
    OutBuffers[0].cbBuffer = 0;
    outbuf.cBuffers = 1;
    outbuf.pBuffers = OutBuffers;
    outbuf.ulVersion = SECBUFFER_VERSION;

    elog(DEBUG4, "Processing received SSPI token of length %u", (unsigned int)buf.len);

    r = AcceptSecurityContext(&sspicred, sspictx, &inbuf, ASC_REQ_ALLOCATE_MEMORY, SECURITY_NETWORK_DREP, &newctx, &outbuf, &contextattr, NULL);

    /* input buffer no longer used */
    pfree(buf.data);

    if (outbuf.cBuffers > 0 && outbuf.pBuffers[0].cbBuffer > 0)
    {
      /*
       * Negotiation generated data to be sent to the client.
       */
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

    /*
     * Overwrite the current context with the one we just received. If
     * sspictx is NULL it was the first loop and we need to allocate a
     * buffer for it. On subsequent runs, we can just overwrite the buffer
     * contents since the size does not change.
     */
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

  /*
   * Release service principal credentials
   */
  FreeCredentialsHandle(&sspicred);

  /*
   * SEC_E_OK indicates that authentication is now complete.
   *
   * Get the name of the user that authenticated, and compare it to the pg
   * username that was specified for the connection.
   *
   * MingW is missing the export for QuerySecurityContextToken in the
   * secur32 library, so we have to load it dynamically.
   */

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

  /*
   * No longer need the security context, everything from here on uses the
   * token instead.
   */
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
      /* Error already reported from pg_SSPI_make_upn */
      return status;
    }
  }

  /*
   * Compare realm/domain if requested. In SSPI, always compare case
   * insensitive.
   */
  if (port->hba->krb_realm && strlen(port->hba->krb_realm))
  {
    if (pg_strcasecmp(port->hba->krb_realm, domainname) != 0)
    {
      elog(DEBUG2, "SSPI domain (%s) and configured domain (%s) don't match", domainname, port->hba->krb_realm);

      return STATUS_ERROR;
    }
  }

  /*
   * We have the username (without domain/realm) in accountname, compare to
   * the supplied value. In SSPI, always compare case insensitive.
   *
   * If set to include realm, append it in <username>@<realm> format.
   */
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

/*
 * Replaces the domainname with the Kerberos realm name,
 * and optionally the accountname with the Kerberos user name.
 */
static int
pg_SSPI_make_upn(char *accountname, size_t accountnamesize, char *domainname, size_t domainnamesize, bool update_accountname)
{
  char *samname;
  char *upname = NULL;
  char *p = NULL;
  ULONG upnamesize = 0;
  size_t upnamerealmsize;
  BOOLEAN res;

  /*
   * Build SAM name (DOMAIN\user), then translate to UPN
   * (user@kerberos.realm). The realm name is returned in lower case, but
   * that is fine because in SSPI auth, string comparisons are always
   * case-insensitive.
   */

  samname = psprintf("%s\\%s", domainname, accountname);
  res = TranslateName(samname, NameSamCompatible, NameUserPrincipal, NULL, &upnamesize);

  if ((!res && GetLastError() != ERROR_INSUFFICIENT_BUFFER) || upnamesize == 0)
  {
    pfree(samname);
    ereport(LOG, (errcode(ERRCODE_INVALID_ROLE_SPECIFICATION), errmsg("could not translate name")));
    return STATUS_ERROR;
  }

  /* upnamesize includes the terminating NUL. */
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

  /* Length of realm name after the '@', including the NUL. */
  upnamerealmsize = upnamesize - (p - upname + 1);

  /* Replace domainname with realm name. */
  if (upnamerealmsize > domainnamesize)
  {
    pfree(upname);
    ereport(LOG, (errcode(ERRCODE_INVALID_ROLE_SPECIFICATION), errmsg("realm name too long")));
    return STATUS_ERROR;
  }

  /* Length is now safe. */
  strcpy(domainname, p + 1);

  /* Replace account name as well (in case UPN != SAM)? */
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
#endif /* ENABLE_SSPI */

/*----------------------------------------------------------------
 * Ident authentication system
 *----------------------------------------------------------------
 */

/*
 *	Parse the string "*ident_response" as a response from a query to an
 *Ident server.  If it's a normal response indicating a user name, return true
 *	and store the user name at *ident_user. If it's anything else,
 *	return false.
 */
static bool
interpret_ident_response(const char *ident_response, char *ident_user)
{





























































































}

/*
 *	Talk to the ident server on host "remote_ip_addr" and find out who
 *	owns the tcp connection from his port "remote_port" to port
 *	"local_port_addr" on host "local_ip_addr".  Return the user name the
 *	ident server gives as "*ident_user".
 *
 *	IP addresses and port numbers are in network byte order.
 *
 *	But iff we're unable to get the information from ident, return false.
 *
 *	XXX: Using WaitLatchOrSocket() and doing a CHECK_FOR_INTERRUPTS() if the
 *	latch was set would improve the responsiveness to
 *timeouts/cancellations.
 */
static int
ident_inet(hbaPort *port)
{















































































































































}

/*
 *	Ask kernel about the credentials of the connecting process,
 *	determine the symbolic name of the corresponding user, and check
 *	if valid per the usermap.
 *
 *	Iff authorized, return STATUS_OK, otherwise return STATUS_ERROR.
 */
#ifdef HAVE_UNIX_SOCKETS

static int
auth_peer(hbaPort *port)
{
































}
#endif /* HAVE_UNIX_SOCKETS */

/*----------------------------------------------------------------
 * PAM authentication system
 *----------------------------------------------------------------
 */
#ifdef USE_PAM

/*
 * PAM conversation function
 */

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
    /*
     * Workaround for Solaris 2.6 where the PAM library is broken and does
     * not pass appdata_ptr to the conversation routine
     */
    passwd = pam_passwd;
  }

  *resp = NULL; /* in case of error exit */

  if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
  {
    return PAM_CONV_ERR;
  }

  /*
   * Explicitly not using palloc here - PAM will free this memory in
   * pam_end()
   */
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
        /*
         * Password wasn't passed to PAM the first time around -
         * let's go ask the client to send a password, which we
         * then stuff into PAM.
         */
        sendAuthRequest(pam_port_cludge, AUTH_REQ_PASSWORD, NULL, 0);
        passwd = recv_password_packet(pam_port_cludge);
        if (passwd == NULL)
        {
          /*
           * Client didn't want to send password.  We
           * intentionally do not log anything about this,
           * either here or at higher levels.
           */
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
      /* FALL THROUGH */
    case PAM_TEXT_INFO:
      /* we don't bother to log TEXT_INFO messages */
      if ((reply[i].resp = strdup("")) == NULL)
      {
        goto fail;
      }
      reply[i].resp_retcode = PAM_SUCCESS;
      break;
    default:;
      elog(LOG, "unsupported PAM conversation %d/\"%s\"", msg[i]->msg_style, msg[i]->msg ? msg[i]->msg : "(none)");
      goto fail;
    }
  }

  *resp = reply;
  return PAM_SUCCESS;

fail:
  /* free up whatever we allocated */
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

/*
 * Check authentication against PAM.
 */
static int
CheckPAMAuth(Port *port, const char *user, const char *password)
{
  int retval;
  pam_handle_t *pamh = NULL;

  /*
   * We can't entirely rely on PAM to pass through appdata --- it appears
   * not to work on at least Solaris 2.6.  So use these ugly static
   * variables instead.
   */
  pam_passwd = password;
  pam_port_cludge = port;
  pam_no_password = false;

  /*
   * Set the application data portion of the conversation struct.  This is
   * later used inside the PAM conversation to pass the password to the
   * authentication module.
   */
  pam_passw_conv.appdata_ptr = unconstify(char *, password); /* from password above,
                                                              * not allocated */

  /* Optionally, one can set the service name in pg_hba.conf */
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
    pam_passwd = NULL; /* Unset pam_passwd */
    return STATUS_ERROR;
  }

  retval = pam_set_item(pamh, PAM_USER, user);

  if (retval != PAM_SUCCESS)
  {
    ereport(LOG, (errmsg("pam_set_item(PAM_USER) failed: %s", pam_strerror(pamh, retval))));
    pam_passwd = NULL; /* Unset pam_passwd */
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
    pam_passwd = NULL; /* Unset pam_passwd */
    return STATUS_ERROR;
  }

  retval = pam_authenticate(pamh, 0);

  if (retval != PAM_SUCCESS)
  {
    /* If pam_passwd_conv_proc saw EOF, don't log anything */
    if (!pam_no_password)
    {
      ereport(LOG, (errmsg("pam_authenticate failed: %s", pam_strerror(pamh, retval))));
    }
    pam_passwd = NULL; /* Unset pam_passwd */
    return pam_no_password ? STATUS_EOF : STATUS_ERROR;
  }

  retval = pam_acct_mgmt(pamh, 0);

  if (retval != PAM_SUCCESS)
  {
    /* If pam_passwd_conv_proc saw EOF, don't log anything */
    if (!pam_no_password)
    {
      ereport(LOG, (errmsg("pam_acct_mgmt failed: %s", pam_strerror(pamh, retval))));
    }
    pam_passwd = NULL; /* Unset pam_passwd */
    return pam_no_password ? STATUS_EOF : STATUS_ERROR;
  }

  retval = pam_end(pamh, retval);

  if (retval != PAM_SUCCESS)
  {
    ereport(LOG, (errmsg("could not release PAM authenticator: %s", pam_strerror(pamh, retval))));
  }

  pam_passwd = NULL; /* Unset pam_passwd */

  return (retval == PAM_SUCCESS ? STATUS_OK : STATUS_ERROR);
}
#endif /* USE_PAM */

/*----------------------------------------------------------------
 * BSD authentication system
 *----------------------------------------------------------------
 */
#ifdef USE_BSD_AUTH
static int
CheckBSDAuth(Port *port, char *user)
{
  char *passwd;
  int retval;

  /* Send regular password request to client, and get the response */
  sendAuthRequest(port, AUTH_REQ_PASSWORD, NULL, 0);

  passwd = recv_password_packet(port);
  if (passwd == NULL)
  {
    return STATUS_EOF;
  }

  /*
   * Ask the BSD auth system to verify password.  Note that auth_userokay
   * will overwrite the password string with zeroes, but it's just a
   * temporary string so we don't care.
   */
  retval = auth_userokay(user, NULL, "auth-postgresql", passwd);

  pfree(passwd);

  if (!retval)
  {
    return STATUS_ERROR;
  }

  return STATUS_OK;
}
#endif /* USE_BSD_AUTH */

/*----------------------------------------------------------------
 * LDAP authentication system
 *----------------------------------------------------------------
 */
#ifdef USE_LDAP

static int
errdetail_for_ldap(LDAP *ldap);

/*
 * Initialize a connection to the LDAP server, including setting up
 * TLS if requested.
 */
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

  /*
   * OpenLDAP provides a non-standard extension ldap_initialize() that takes
   * a list of URIs, allowing us to request "ldaps" instead of "ldap".  It
   * also provides ldap_domain2hostlist() to find LDAP servers automatically
   * using DNS SRV.  They were introduced in the same version, so for now we
   * don't have an extra configure check for the latter.
   */
  {
    StringInfoData uris;
    char *hostlist = NULL;
    char *p;
    bool append_port;

    /* We'll build a space-separated scheme://hostname:port list here */
    initStringInfo(&uris);

    /*
     * If pg_hba.conf provided no hostnames, we can ask OpenLDAP to try to
     * find some by extracting a domain name from the base DN and looking
     * up DSN SRV records for _ldap._tcp.<domain>.
     */
    if (!port->hba->ldapserver || port->hba->ldapserver[0] == '\0')
    {
      char *domain;

      /* ou=blah,dc=foo,dc=bar -> foo.bar */
      if (ldap_dn2domain(port->hba->ldapbasedn, &domain))
      {
        ereport(LOG, (errmsg("could not extract domain name from ldapbasedn")));
        return STATUS_ERROR;
      }

      /* Look up a list of LDAP server hosts and port numbers */
      if (ldap_domain2hostlist(domain, &hostlist))
      {
        ereport(LOG, (errmsg("LDAP authentication could not find DNS SRV records for \"%s\"", domain), (errhint("Set an LDAP server name explicitly."))));
        ldap_memfree(domain);
        return STATUS_ERROR;
      }
      ldap_memfree(domain);

      /* We have a space-separated list of host:port entries */
      p = hostlist;
      append_port = false;
    }
    else
    {
      /* We have a space-separated list of hosts from pg_hba.conf */
      p = port->hba->ldapserver;
      append_port = true;
    }

    /* Convert the list of host[:port] entries to full URIs */
    do
    {
      size_t size;

      /* Find the span of the next entry */
      size = strcspn(p, " ");

      /* Append a space separator if this isn't the first URI */
      if (uris.len > 0)
      {
        appendStringInfoChar(&uris, ' ');
      }

      /* Append scheme://host:port */
      appendStringInfoString(&uris, scheme);
      appendStringInfoString(&uris, "://");
      appendBinaryStringInfo(&uris, p, size);
      if (append_port)
      {
        appendStringInfo(&uris, ":%d", port->hba->ldapport);
      }

      /* Step over this entry and any number of trailing spaces */
      p += size;
      while (*p == ' ')
      {
        ++p;
      }
    } while (*p);

    /* Free memory from OpenLDAP if we looked up SRV records */
    if (hostlist)
    {
      ldap_memfree(hostlist);
    }

    /* Finally, try to connect using the URI list */
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
      /*
       * Need to load this function dynamically because it does not
       * exist on Windows 2000, and causes a load error for the whole
       * exe if referenced.
       */
      HANDLE ldaphandle;

      ldaphandle = LoadLibrary("WLDAP32.DLL");
      if (ldaphandle == NULL)
      {
        /*
         * should never happen since we import other files from
         * wldap32, but check anyway
         */
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

      /*
       * Leak LDAP handle on purpose, because we need the library to
       * stay open. This is ok because it will only ever be leaked once
       * per process and is automatically cleaned up on process exit.
       */
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

/* Placeholders recognized by FormatSearchFilter.  For now just one. */
#define LPH_USERNAME "$username"
#define LPH_USERNAME_LEN (sizeof(LPH_USERNAME) - 1)

/* Not all LDAP implementations define this. */
#ifndef LDAP_NO_ATTRS
#define LDAP_NO_ATTRS "1.1"
#endif

/* Not all LDAP implementations define this. */
#ifndef LDAPS_PORT
#define LDAPS_PORT 636
#endif

/*
 * Return a newly allocated C string copied from "pattern" with all
 * occurrences of the placeholder "$username" replaced with "user_name".
 */
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

/*
 * Perform LDAP authentication
 */
static int
CheckLDAPAuth(Port *port)
{
  char *passwd;
  LDAP *ldap;
  int r;
  char *fulluser;
  const char *server_name;

#ifdef HAVE_LDAP_INITIALIZE

  /*
   * For OpenLDAP, allow empty hostname if we have a basedn.  We'll look for
   * servers with DNS SRV records via OpenLDAP library facilities.
   */
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

  /*
   * If we're using SRV records, we don't have a server name so we'll just
   * show an empty string in error messages.
   */
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
    return STATUS_EOF; /* client wouldn't send password */
  }

  if (InitializeLDAPConnection(port, &ldap) == STATUS_ERROR)
  {
    /* Error message already sent */
    pfree(passwd);
    return STATUS_ERROR;
  }

  if (port->hba->ldapbasedn)
  {
    /*
     * First perform an LDAP search to find the DN for the user we are
     * trying to log in as.
     */
    char *filter;
    LDAPMessage *search_message;
    LDAPMessage *entry;
    char *attributes[] = {LDAP_NO_ATTRS, NULL};
    char *dn;
    char *c;
    int count;

    /*
     * Disallow any characters that we would otherwise need to escape,
     * since they aren't really reasonable in a username anyway. Allowing
     * them would make it possible to inject any kind of custom filters in
     * the LDAP filter.
     */
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

    /*
     * Bind with a pre-defined username/password (if available) for
     * searching. If none is specified, this turns into an anonymous bind.
     */
    r = ldap_simple_bind_s(ldap, port->hba->ldapbinddn ? port->hba->ldapbinddn : "", port->hba->ldapbindpasswd ? port->hba->ldapbindpasswd : "");
    if (r != LDAP_SUCCESS)
    {
      ereport(LOG, (errmsg("could not perform initial LDAP bind for ldapbinddn \"%s\" on server \"%s\": %s", port->hba->ldapbinddn ? port->hba->ldapbinddn : "", server_name, ldap_err2string(r)), errdetail_for_ldap(ldap)));
      ldap_unbind(ldap);
      pfree(passwd);
      return STATUS_ERROR;
    }

    /* Build a custom filter or a single attribute filter? */
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

    /* Unbind and disconnect from the LDAP server */
    r = ldap_unbind_s(ldap);
    if (r != LDAP_SUCCESS)
    {
      ereport(LOG, (errmsg("could not unbind after searching for user \"%s\" on server \"%s\"", fulluser, server_name)));
      pfree(passwd);
      pfree(fulluser);
      return STATUS_ERROR;
    }

    /*
     * Need to re-initialize the LDAP connection, so that we can bind to
     * it with a different username.
     */
    if (InitializeLDAPConnection(port, &ldap) == STATUS_ERROR)
    {
      pfree(passwd);
      pfree(fulluser);

      /* Error message already sent */
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

/*
 * Add a detail error message text to the current error if one can be
 * constructed from the LDAP 'diagnostic message'.
 */
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

#endif /* USE_LDAP */

/*----------------------------------------------------------------
 * SSL client certificate authentication
 *----------------------------------------------------------------
 */
#ifdef USE_SSL
static int
CheckCertAuth(Port *port)
{
  int status_check_usermap = STATUS_ERROR;

  Assert(port->ssl);

  /* Make sure we have received a username in the certificate */
  if (port->peer_cn == NULL || strlen(port->peer_cn) <= 0)
  {
    ereport(LOG, (errmsg("certificate authentication failed for user \"%s\": client certificate contains no user name", port->user_name)));
    return STATUS_ERROR;
  }

  /* Just pass the certificate cn to the usermap check */
  status_check_usermap = check_usermap(port->hba->usermap, port->user_name, port->peer_cn, false);
  if (status_check_usermap != STATUS_OK)
  {
    /*
     * If clientcert=verify-full was specified and the authentication
     * method is other than uaCert, log the reason for rejecting the
     * authentication.
     */
    if (port->hba->clientcert == clientCertFull && port->hba->auth_method != uaCert)
    {
      ereport(LOG, (errmsg("certificate validation (clientcert=verify-full) failed for user \"%s\": CN mismatch", port->user_name)));
    }
  }
  return status_check_usermap;
}
#endif

/*----------------------------------------------------------------
 * RADIUS authentication
 *----------------------------------------------------------------
 */

/*
 * RADIUS authentication is described in RFC2865 (and several others).
 */

#define RADIUS_VECTOR_LENGTH 16
#define RADIUS_HEADER_LENGTH 20
#define RADIUS_MAX_PASSWORD_LENGTH 128

/* Maximum size of a RADIUS packet we will create or accept */
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
  /* this is a bit longer than strictly necessary: */
  char pad[RADIUS_BUFFER_SIZE - RADIUS_VECTOR_LENGTH];
} radius_packet;

/* RADIUS packet types */
#define RADIUS_ACCESS_REQUEST 1
#define RADIUS_ACCESS_ACCEPT 2
#define RADIUS_ACCESS_REJECT 3

/* RADIUS attributes */
#define RADIUS_USER_NAME 1
#define RADIUS_PASSWORD 2
#define RADIUS_SERVICE_TYPE 6
#define RADIUS_NAS_IDENTIFIER 32

/* RADIUS service types */
#define RADIUS_AUTHENTICATE_ONLY 8

/* Seconds to wait - XXX: should be in a config variable! */
#define RADIUS_TIMEOUT 3

static void
radius_add_attribute(radius_packet *packet, uint8 type, const unsigned char *data, int len)
{



















}

static int
CheckRADIUSAuth(Port *port)
{





















































































}

static int
PerformRadiusTransaction(const char *server, const char *secret, const char *portstr, const char *identifier, const char *user_name, const char *passwd)
{





























































































































































































































































































































}