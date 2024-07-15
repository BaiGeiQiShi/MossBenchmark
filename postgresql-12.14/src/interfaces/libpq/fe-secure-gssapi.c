                                                                            
   
                      
                                                          
   
                                                                         
   
                  
                                            
   
                                                                            
   

#include "postgres_fe.h"

#include "libpq-fe.h"
#include "libpq-int.h"
#include "fe-gssapi-common.h"
#include "port/pg_bswap.h"

   
                                                                    
                            
   
#define GSS_REQUIRED_FLAGS GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG

   
                                                          
   
                                                                  
                                                                   
                                                                    
                                                                  
                                                
   
                                                                    
                                                              
                                                                    
                                                                     
                                                                
                           
   
                                                                     
                                                                       
                                                                    
                                                                       
   
                                                                       
                                   
   
#define PQ_GSS_SEND_BUFFER_SIZE 16384
#define PQ_GSS_RECV_BUFFER_SIZE 16384

   
                                                                         
                                                                        
                 
   
#define PqGSSSendBuffer (conn->gss_SendBuffer)
#define PqGSSSendLength (conn->gss_SendLength)
#define PqGSSSendNext (conn->gss_SendNext)
#define PqGSSSendConsumed (conn->gss_SendConsumed)
#define PqGSSRecvBuffer (conn->gss_RecvBuffer)
#define PqGSSRecvLength (conn->gss_RecvLength)
#define PqGSSResultBuffer (conn->gss_ResultBuffer)
#define PqGSSResultLength (conn->gss_ResultLength)
#define PqGSSResultNext (conn->gss_ResultNext)
#define PqGSSMaxPktSize (conn->gss_MaxPktSize)

   
                                                                                 
   
                                                                             
                                       
   
                                                                             
                                                                             
                                                                              
                                                                          
                             
   
ssize_t
pg_GSS_write(PGconn *conn, const void *ptr, size_t len)
{
  OM_uint32 major, minor;
  gss_buffer_desc input, output = GSS_C_EMPTY_BUFFER;
  ssize_t ret = -1;
  size_t bytes_sent = 0;
  size_t bytes_to_encrypt;
  size_t bytes_encrypted;
  gss_ctx_id_t gctx = conn->gctx;

     
                                                                             
                                                                     
                                                                           
                                                                     
                                                                             
                                                                            
                                                                      
     
  if (len < PqGSSSendConsumed)
  {
    printfPQExpBuffer(&conn->errorMessage, "GSSAPI caller failed to retransmit all data needing to be retried\n");
    errno = EINVAL;
    return -1;
  }

                                                           
  bytes_to_encrypt = len - PqGSSSendConsumed;
  bytes_encrypted = PqGSSSendConsumed;

     
                                                                            
                                                                             
                                                                             
                            
     
  while (bytes_to_encrypt || PqGSSSendLength)
  {
    int conf_state = 0;
    uint32 netlen;

       
                                                                          
                                                                         
                                                                          
               
       
    if (PqGSSSendLength)
    {
      ssize_t ret;
      ssize_t amount = PqGSSSendLength - PqGSSSendNext;

      ret = pqsecure_raw_write(conn, PqGSSSendBuffer + PqGSSSendNext, amount);
      if (ret <= 0)
      {
           
                                                                       
                                                                       
                                                                     
                                                         
           
        if (bytes_sent)
        {
          return bytes_sent;
        }
        return ret;
      }

         
                                                                         
                                          
         
      if (ret != amount)
      {
        PqGSSSendNext += ret;
        continue;
      }

                                                                     
      bytes_sent += PqGSSSendConsumed;

                                                                 
      PqGSSSendLength = PqGSSSendNext = PqGSSSendConsumed = 0;
    }

       
                                                                          
       
    if (!bytes_to_encrypt)
    {
      break;
    }

       
                                                                         
                                                                         
                             
       
    if (bytes_to_encrypt > PqGSSMaxPktSize)
    {
      input.length = PqGSSMaxPktSize;
    }
    else
    {
      input.length = bytes_to_encrypt;
    }

    input.value = (char *)ptr + bytes_encrypted;

    output.value = NULL;
    output.length = 0;

       
                                                                           
                                                             
       
    major = gss_wrap(&minor, gctx, 1, GSS_C_QOP_DEFAULT, &input, &conf_state, &output);
    if (major != GSS_S_COMPLETE)
    {
      pg_GSS_error(libpq_gettext("GSSAPI wrap error"), conn, major, minor);
      errno = EIO;                                
      goto cleanup;
    }

    if (conf_state == 0)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("outgoing GSSAPI message would not use confidentiality\n"));
      errno = EIO;                                
      goto cleanup;
    }

    if (output.length > PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32))
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("client tried to send oversize GSSAPI packet (%zu > %zu)\n"), (size_t)output.length, PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32));
      errno = EIO;                                
      goto cleanup;
    }

    bytes_encrypted += input.length;
    bytes_to_encrypt -= input.length;
    PqGSSSendConsumed += input.length;

                                                       
    netlen = htonl(output.length);
    memcpy(PqGSSSendBuffer + PqGSSSendLength, &netlen, sizeof(uint32));
    PqGSSSendLength += sizeof(uint32);

    memcpy(PqGSSSendBuffer + PqGSSSendLength, output.value, output.length);
    PqGSSSendLength += output.length;

                                                    
    gss_release_buffer(&minor, &output);
  }

                                                         
  Assert(bytes_sent == len);
  Assert(bytes_sent == bytes_encrypted);

  ret = bytes_sent;

cleanup:
                                                           
  if (output.value != NULL)
  {
    gss_release_buffer(&minor, &output);
  }
  return ret;
}

   
                                                                             
   
                                                                             
                                       
   
                                                                    
                                                                         
                                                                           
                                                      
   
ssize_t
pg_GSS_read(PGconn *conn, void *ptr, size_t len)
{
  OM_uint32 major, minor;
  gss_buffer_desc input = GSS_C_EMPTY_BUFFER, output = GSS_C_EMPTY_BUFFER;
  ssize_t ret;
  size_t bytes_returned = 0;
  gss_ctx_id_t gctx = conn->gctx;

     
                                                                 
                                                                           
                                                                       
                           
     
  while (bytes_returned < len)
  {
    int conf_state = 0;

                                                                            
    if (PqGSSResultNext < PqGSSResultLength)
    {
      size_t bytes_in_buffer = PqGSSResultLength - PqGSSResultNext;
      size_t bytes_to_copy = Min(bytes_in_buffer, len - bytes_returned);

         
                                                                        
                                                                   
         
      memcpy((char *)ptr + bytes_returned, PqGSSResultBuffer + PqGSSResultNext, bytes_to_copy);
      PqGSSResultNext += bytes_to_copy;
      bytes_returned += bytes_to_copy;

         
                                                                   
                                                                       
                                                                         
                                                                         
                                                                    
                                                                         
                           
         
      break;
    }

                                                          
    PqGSSResultLength = PqGSSResultNext = 0;

       
                                                                       
                                                                        
                                                                         
                                             
       
    Assert(bytes_returned == 0);

       
                                                                       
                                                                           
                                                     
       

                                                  
    if (PqGSSRecvLength < sizeof(uint32))
    {
      ret = pqsecure_raw_read(conn, PqGSSRecvBuffer + PqGSSRecvLength, sizeof(uint32) - PqGSSRecvLength);

                                                                        
      if (ret <= 0)
      {
        return ret;
      }

      PqGSSRecvLength += ret;

                                                                    
      if (PqGSSRecvLength < sizeof(uint32))
      {
        errno = EWOULDBLOCK;
        return -1;
      }
    }

                                                                  
    input.length = ntohl(*(uint32 *)PqGSSRecvBuffer);

    if (input.length > PQ_GSS_RECV_BUFFER_SIZE - sizeof(uint32))
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("oversize GSSAPI packet sent by the server (%zu > %zu)\n"), (size_t)input.length, PQ_GSS_RECV_BUFFER_SIZE - sizeof(uint32));
      errno = EIO;                                
      return -1;
    }

       
                                                                      
                                                               
       
    ret = pqsecure_raw_read(conn, PqGSSRecvBuffer + PqGSSRecvLength, input.length - (PqGSSRecvLength - sizeof(uint32)));
                                                                      
    if (ret <= 0)
    {
      return ret;
    }

    PqGSSRecvLength += ret;

                                                                     
    if (PqGSSRecvLength - sizeof(uint32) < input.length)
    {
      errno = EWOULDBLOCK;
      return -1;
    }

       
                                                                         
                                                                        
                                                                       
                                        
       
    output.value = NULL;
    output.length = 0;
    input.value = PqGSSRecvBuffer + sizeof(uint32);

    major = gss_unwrap(&minor, gctx, &input, &output, &conf_state, NULL);
    if (major != GSS_S_COMPLETE)
    {
      pg_GSS_error(libpq_gettext("GSSAPI unwrap error"), conn, major, minor);
      ret = -1;
      errno = EIO;                                
      goto cleanup;
    }

    if (conf_state == 0)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("incoming GSSAPI message did not use confidentiality\n"));
      ret = -1;
      errno = EIO;                                
      goto cleanup;
    }

    memcpy(PqGSSResultBuffer, output.value, output.length);
    PqGSSResultLength = output.length;

                                                   
    PqGSSRecvLength = 0;

                                                    
    gss_release_buffer(&minor, &output);
  }

  ret = bytes_returned;

cleanup:
                                                           
  if (output.value != NULL)
  {
    gss_release_buffer(&minor, &output);
  }
  return ret;
}

   
                                                      
   
                                                                                
                                                                                 
                                       
   
static PostgresPollingStatusType
gss_read(PGconn *conn, void *recv_buffer, size_t length, ssize_t *ret)
{
  *ret = pqsecure_raw_read(conn, recv_buffer, length);
  if (*ret < 0)
  {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    {
      return PGRES_POLLING_READING;
    }
    else
    {
      return PGRES_POLLING_FAILED;
    }
  }

                     
  if (*ret == 0)
  {
    int result = pqReadReady(conn);

    if (result < 0)
    {
      return PGRES_POLLING_FAILED;
    }

    if (!result)
    {
      return PGRES_POLLING_READING;
    }

    *ret = pqsecure_raw_read(conn, recv_buffer, length);
    if (*ret < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      {
        return PGRES_POLLING_READING;
      }
      else
      {
        return PGRES_POLLING_FAILED;
      }
    }
    if (*ret == 0)
    {
      return PGRES_POLLING_FAILED;
    }
  }

  return PGRES_POLLING_OK;
}

   
                                                                        
                                                           
                                                                     
                                                              
   
PostgresPollingStatusType
pqsecure_open_gss(PGconn *conn)
{
  ssize_t ret;
  OM_uint32 major, minor;
  uint32 netlen;
  PostgresPollingStatusType result;
  gss_buffer_desc input = GSS_C_EMPTY_BUFFER, output = GSS_C_EMPTY_BUFFER;

     
                                                                     
                                                                           
                                                                            
                                             
     
  if (PqGSSSendBuffer == NULL)
  {
    PqGSSSendBuffer = malloc(PQ_GSS_SEND_BUFFER_SIZE);
    PqGSSRecvBuffer = malloc(PQ_GSS_RECV_BUFFER_SIZE);
    PqGSSResultBuffer = malloc(PQ_GSS_RECV_BUFFER_SIZE);
    if (!PqGSSSendBuffer || !PqGSSRecvBuffer || !PqGSSResultBuffer)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
      return PGRES_POLLING_FAILED;
    }
    PqGSSSendLength = PqGSSSendNext = PqGSSSendConsumed = 0;
    PqGSSRecvLength = PqGSSResultLength = PqGSSResultNext = 0;
  }

     
                                                                             
     
  if (PqGSSSendLength)
  {
    ssize_t amount = PqGSSSendLength - PqGSSSendNext;

    ret = pqsecure_raw_write(conn, PqGSSSendBuffer + PqGSSSendNext, amount);
    if (ret < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      {
        return PGRES_POLLING_WRITING;
      }
      else
      {
        return PGRES_POLLING_FAILED;
      }
    }

    if (ret < amount)
    {
      PqGSSSendNext += ret;
      return PGRES_POLLING_WRITING;
    }

    PqGSSSendLength = PqGSSSendNext = 0;
  }

     
                                                                            
                                                                           
                                   
     
  if (conn->gctx)
  {
                                                 

                                                      
    if (PqGSSRecvLength < sizeof(uint32))
    {
                                           
      result = gss_read(conn, PqGSSRecvBuffer + PqGSSRecvLength, sizeof(uint32) - PqGSSRecvLength, &ret);
      if (result != PGRES_POLLING_OK)
      {
        return result;
      }

      PqGSSRecvLength += ret;

      if (PqGSSRecvLength < sizeof(uint32))
      {
        return PGRES_POLLING_READING;
      }
    }

       
                                       
       
                                                                           
                                                                     
                                                                        
              
       
    if (PqGSSRecvBuffer[0] == 'E')
    {
         
                                                                       
                                                                         
                                                                         
                             
         
      result = gss_read(conn, PqGSSRecvBuffer + PqGSSRecvLength, PQ_GSS_RECV_BUFFER_SIZE - PqGSSRecvLength - 1, &ret);
      if (result != PGRES_POLLING_OK)
      {
        return result;
      }

      PqGSSRecvLength += ret;

      Assert(PqGSSRecvLength < PQ_GSS_RECV_BUFFER_SIZE);
      PqGSSRecvBuffer[PqGSSRecvLength] = '\0';
      printfPQExpBuffer(&conn->errorMessage, "%s\n", PqGSSRecvBuffer + 1);

      return PGRES_POLLING_FAILED;
    }

       
                                                                         
                                                     
       

                                                         
    input.length = ntohl(*(uint32 *)PqGSSRecvBuffer);
    if (input.length > PQ_GSS_RECV_BUFFER_SIZE - sizeof(uint32))
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("oversize GSSAPI packet sent by the server (%zu > %zu)\n"), (size_t)input.length, PQ_GSS_RECV_BUFFER_SIZE - sizeof(uint32));
      return PGRES_POLLING_FAILED;
    }

       
                                                                      
                                                               
       
    result = gss_read(conn, PqGSSRecvBuffer + PqGSSRecvLength, input.length - (PqGSSRecvLength - sizeof(uint32)), &ret);
    if (result != PGRES_POLLING_OK)
    {
      return result;
    }

    PqGSSRecvLength += ret;

       
                                                                         
                            
       
    if (PqGSSRecvLength - sizeof(uint32) < input.length)
    {
      return PGRES_POLLING_READING;
    }

    input.value = PqGSSRecvBuffer + sizeof(uint32);
  }

                                                    
  ret = pg_GSS_load_servicename(conn);
  if (ret != STATUS_OK)
  {
    return PGRES_POLLING_FAILED;
  }

     
                                                                           
                             
     
  major = gss_init_sec_context(&minor, conn->gcred, &conn->gctx, conn->gtarg_nam, GSS_C_NO_OID, GSS_REQUIRED_FLAGS, 0, 0, &input, NULL, &output, NULL, NULL);

                                                               
  PqGSSRecvLength = 0;

  if (GSS_ERROR(major))
  {
    pg_GSS_error(libpq_gettext("could not initiate GSSAPI security context"), conn, major, minor);
    return PGRES_POLLING_FAILED;
  }

  if (output.length == 0)
  {
       
                                                                         
                                      
       
    conn->gssenc = true;

                  
    gss_release_cred(&minor, &conn->gcred);
    conn->gcred = GSS_C_NO_CREDENTIAL;
    gss_release_buffer(&minor, &output);

       
                                                                         
                                                                
       
    major = gss_wrap_size_limit(&minor, conn->gctx, 1, GSS_C_QOP_DEFAULT, PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32), &PqGSSMaxPktSize);

    if (GSS_ERROR(major))
    {
      pg_GSS_error(libpq_gettext("GSSAPI size check error"), conn, major, minor);
      return PGRES_POLLING_FAILED;
    }

    return PGRES_POLLING_OK;
  }

                                   
  if (output.length > PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32))
  {
    pg_GSS_error(libpq_gettext("GSSAPI context establishment error"), conn, major, minor);
    gss_release_buffer(&minor, &output);
    return PGRES_POLLING_FAILED;
  }

                                   
  netlen = htonl(output.length);

  memcpy(PqGSSSendBuffer, (char *)&netlen, sizeof(uint32));
  PqGSSSendLength += sizeof(uint32);

  memcpy(PqGSSSendBuffer + PqGSSSendLength, output.value, output.length);
  PqGSSSendLength += output.length;

                                                   

                                                  
  gss_release_buffer(&minor, &output);

                                            
  return PGRES_POLLING_WRITING;
}

   
                                 
   

   
                                     
   
void *
PQgetgssctx(PGconn *conn)
{
  if (!conn)
  {
    return NULL;
  }

  return conn->gctx;
}

   
                                               
   
int
PQgssEncInUse(PGconn *conn)
{
  if (!conn || !conn->gctx)
  {
    return 0;
  }

  return conn->gssenc;
}
