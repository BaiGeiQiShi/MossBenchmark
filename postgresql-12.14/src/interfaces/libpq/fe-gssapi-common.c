                                                                            
   
                      
                                                 
   
                                                                         
                                                                        
   
                  
                                                
                                                                            
   

#include "postgres_fe.h"

#include "fe-gssapi-common.h"

#include "libpq-int.h"
#include "pqexpbuffer.h"

   
                                                            
                                             
   
static void
pg_GSS_error_int(PQExpBuffer str, OM_uint32 stat, int type)
{
  OM_uint32 lmin_s;
  gss_buffer_desc lmsg;
  OM_uint32 msg_ctx = 0;

  do
  {
    if (gss_display_status(&lmin_s, stat, type, GSS_C_NO_OID, &msg_ctx, &lmsg) != GSS_S_COMPLETE)
    {
      break;
    }
    appendPQExpBufferChar(str, ' ');
    appendBinaryPQExpBuffer(str, lmsg.value, lmsg.length);
    gss_release_buffer(&lmin_s, &lmsg);
  } while (msg_ctx);
}

   
                                                                      
   
void
pg_GSS_error(const char *mprefix, PGconn *conn, OM_uint32 maj_stat, OM_uint32 min_stat)
{
  resetPQExpBuffer(&conn->errorMessage);
  appendPQExpBuffer(&conn->errorMessage, "%s:", mprefix);
  pg_GSS_error_int(&conn->errorMessage, maj_stat, GSS_C_GSS_CODE);
  appendPQExpBufferChar(&conn->errorMessage, ':');
  pg_GSS_error_int(&conn->errorMessage, min_stat, GSS_C_MECH_CODE);
  appendPQExpBufferChar(&conn->errorMessage, '\n');
}

   
                                                                      
   
bool
pg_GSS_have_cred_cache(gss_cred_id_t *cred_out)
{
  OM_uint32 major, minor;
  gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;

  major = gss_acquire_cred(&minor, GSS_C_NO_NAME, 0, GSS_C_NO_OID_SET, GSS_C_INITIATE, &cred, NULL, NULL);
  if (major != GSS_S_COMPLETE)
  {
    *cred_out = NULL;
    return false;
  }
  *cred_out = cred;
  return true;
}

   
                                             
   
int
pg_GSS_load_servicename(PGconn *conn)
{
  OM_uint32 maj_stat, min_stat;
  int maxlen;
  gss_buffer_desc temp_gbuf;
  char *host;

  if (conn->gtarg_nam != NULL)
  {
                                            
    return STATUS_OK;
  }

  host = PQhost(conn);
  if (!(host && host[0] != '\0'))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("host name must be specified\n"));
    return STATUS_ERROR;
  }

     
                                                                           
                        
     
  maxlen = strlen(conn->krbsrvname) + strlen(host) + 2;
  temp_gbuf.value = (char *)malloc(maxlen);
  if (!temp_gbuf.value)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return STATUS_ERROR;
  }
  snprintf(temp_gbuf.value, maxlen, "%s@%s", conn->krbsrvname, host);
  temp_gbuf.length = strlen(temp_gbuf.value);

  maj_stat = gss_import_name(&min_stat, &temp_gbuf, GSS_C_NT_HOSTBASED_SERVICE, &conn->gtarg_nam);
  free(temp_gbuf.value);

  if (maj_stat != GSS_S_COMPLETE)
  {
    pg_GSS_error(libpq_gettext("GSSAPI name import error"), conn, maj_stat, min_stat);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}
