                                                                            
   
                      
                              
   
                                                                         
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>

#include "libpq/auth.h"
#include "libpq/be-gssapi-common.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/memutils.h"

   
                                                          
   
                                                                  
                                                                   
                                                                    
                                                                  
                                                
   
                                                                    
                                                              
                                                                    
                                                                     
                                                                
                           
   
                                                                     
                                                                       
                                                                    
                                                                       
   
                                                                       
                                   
   
#define PQ_GSS_SEND_BUFFER_SIZE 16384
#define PQ_GSS_RECV_BUFFER_SIZE 16384

   
                                                                     
                                                                    
                                                                   
   
static char *PqGSSSendBuffer;                                        
static int PqGSSSendLength;                                                 
static int PqGSSSendNext;                                       
                                                   
static int PqGSSSendConsumed;                                               
                                                                       

static char *PqGSSRecvBuffer;                               
static int PqGSSRecvLength;                                                 

static char *PqGSSResultBuffer;                                           
static int PqGSSResultLength;                                                   
static int PqGSSResultNext;                                       
                                                       

static uint32 PqGSSMaxPktSize;                                            
                                                                   

   
                                                                                 
   
                                                                             
                                       
   
                                                                             
                                                                              
                                                                            
             
   
                                                                               
                                                                             
                                                                        
                                                                              
   
ssize_t
be_gssapi_write(Port *port, void *ptr, size_t len)
{
  OM_uint32 major, minor;
  gss_buffer_desc input, output;
  size_t bytes_sent = 0;
  size_t bytes_to_encrypt;
  size_t bytes_encrypted;
  gss_ctx_id_t gctx = port->gss->ctx;

     
                                                                             
                                                                     
                                                                           
                                                                     
                                                                             
                                                                            
                                                                      
     
  if (len < PqGSSSendConsumed)
  {
    elog(COMMERROR, "GSSAPI caller failed to retransmit all data needing to be retried");
    errno = ECONNRESET;
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

      ret = secure_raw_write(port, PqGSSSendBuffer + PqGSSSendNext, amount);
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
      pg_GSS_error(_("GSSAPI wrap error"), major, minor);
      errno = ECONNRESET;
      return -1;
    }
    if (conf_state == 0)
    {
      ereport(COMMERROR, (errmsg("outgoing GSSAPI message would not use confidentiality")));
      errno = ECONNRESET;
      return -1;
    }
    if (output.length > PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32))
    {
      ereport(COMMERROR, (errmsg("server tried to send oversize GSSAPI packet (%zu > %zu)", (size_t)output.length, PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32))));
      errno = ECONNRESET;
      return -1;
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

  return bytes_sent;
}

   
                                                                             
   
                                                                             
                                       
   
                                                                    
                                                                           
                                   
   
                                                                           
                                                         
   
ssize_t
be_gssapi_read(Port *port, void *ptr, size_t len)
{
  OM_uint32 major, minor;
  gss_buffer_desc input, output;
  ssize_t ret;
  size_t bytes_returned = 0;
  gss_ctx_id_t gctx = port->gss->ctx;

     
                                                                 
                                                                           
                                                                       
                           
     
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
      ret = secure_raw_read(port, PqGSSRecvBuffer + PqGSSRecvLength, sizeof(uint32) - PqGSSRecvLength);

                                                                      
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
      ereport(COMMERROR, (errmsg("oversize GSSAPI packet sent by the client (%zu > %zu)", (size_t)input.length, PQ_GSS_RECV_BUFFER_SIZE - sizeof(uint32))));
      errno = ECONNRESET;
      return -1;
    }

       
                                                                      
                                                               
       
    ret = secure_raw_read(port, PqGSSRecvBuffer + PqGSSRecvLength, input.length - (PqGSSRecvLength - sizeof(uint32)));
                                                                    
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
      pg_GSS_error(_("GSSAPI unwrap error"), major, minor);
      errno = ECONNRESET;
      return -1;
    }
    if (conf_state == 0)
    {
      ereport(COMMERROR, (errmsg("incoming GSSAPI message did not use confidentiality")));
      errno = ECONNRESET;
      return -1;
    }

    memcpy(PqGSSResultBuffer, output.value, output.length);
    PqGSSResultLength = output.length;

                                                   
    PqGSSRecvLength = 0;

                                                    
    gss_release_buffer(&minor, &output);
  }

  return bytes_returned;
}

   
                                                                  
                                        
   
                                          
   
                                                                        
   
static ssize_t
read_or_wait(Port *port, ssize_t len)
{
  ssize_t ret;

     
                                                                           
                
     
  while (PqGSSRecvLength < len)
  {
    ret = secure_raw_read(port, PqGSSRecvBuffer + PqGSSRecvLength, len - PqGSSRecvLength);

       
                                                  
                                               
       
    if (ret < 0 && !(errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR))
    {
      return -1;
    }

       
                                                                           
                                   
       
                                                                    
                       
       
    if (ret <= 0)
    {
      WaitLatchOrSocket(MyLatch, WL_SOCKET_READABLE | WL_EXIT_ON_PM_DEATH, port->sock, 0, WAIT_EVENT_GSS_OPEN_SERVER);

         
                                                                        
                                                                         
                                           
         
                                                                    
                                                          
         
                                                       
         
      if (ret == 0)
      {
        ret = secure_raw_read(port, PqGSSRecvBuffer + PqGSSRecvLength, len - PqGSSRecvLength);
        if (ret == 0)
        {
          return -1;
        }
      }
      if (ret < 0)
      {
        continue;
      }
    }

    PqGSSRecvLength += ret;
  }

  return len;
}

   
                                                                 
                                                                     
                                                                        
                                                                     
               
   
                                                                       
                                                                       
                                                                   
            
   
ssize_t
secure_open_gssapi(Port *port)
{
  bool complete_next = false;
  OM_uint32 major, minor;

     
                                                          
     
  port->gss = (pg_gssinfo *)MemoryContextAllocZero(TopMemoryContext, sizeof(pg_gssinfo));

     
                                                                         
                                                                            
                                                                  
                                                                          
                          
     
  PqGSSSendBuffer = malloc(PQ_GSS_SEND_BUFFER_SIZE);
  PqGSSRecvBuffer = malloc(PQ_GSS_RECV_BUFFER_SIZE);
  PqGSSResultBuffer = malloc(PQ_GSS_RECV_BUFFER_SIZE);
  if (!PqGSSSendBuffer || !PqGSSRecvBuffer || !PqGSSResultBuffer)
  {
    ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }
  PqGSSSendLength = PqGSSSendNext = PqGSSSendConsumed = 0;
  PqGSSRecvLength = PqGSSResultLength = PqGSSResultNext = 0;

     
                                                                         
                                                                    
     
  if (pg_krb_server_keyfile != NULL && pg_krb_server_keyfile[0] != '\0')
  {
    if (setenv("KRB5_KTNAME", pg_krb_server_keyfile, 1) != 0)
    {
                                                                     
      ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("could not set environment: %m")));
    }
  }

  while (true)
  {
    ssize_t ret;
    gss_buffer_desc input, output = GSS_C_EMPTY_BUFFER;

       
                                                                      
                                                                         
       
    ret = read_or_wait(port, sizeof(uint32));
    if (ret < 0)
    {
      return ret;
    }

       
                                                              
       
    input.length = ntohl(*(uint32 *)PqGSSRecvBuffer);

                                                
    PqGSSRecvLength = 0;

       
                                                                    
                                                                 
       
                                                                      
       
    if (input.length > PQ_GSS_RECV_BUFFER_SIZE)
    {
      ereport(COMMERROR, (errmsg("oversize GSSAPI packet sent by the client (%zu > %d)", (size_t)input.length, PQ_GSS_RECV_BUFFER_SIZE)));
      return -1;
    }

       
                                                                        
                    
       
    ret = read_or_wait(port, input.length);
    if (ret < 0)
    {
      return ret;
    }

    input.value = PqGSSRecvBuffer;

                                                           
    major = gss_accept_sec_context(&minor, &port->gss->ctx, GSS_C_NO_CREDENTIAL, &input, GSS_C_NO_CHANNEL_BINDINGS, &port->gss->name, NULL, &output, NULL, NULL, NULL);
    if (GSS_ERROR(major))
    {
      pg_GSS_error(_("could not accept GSSAPI security context"), major, minor);
      gss_release_buffer(&minor, &output);
      return -1;
    }
    else if (!(major & GSS_S_CONTINUE_NEEDED))
    {
         
                                                                        
                                                    
         
      complete_next = true;
    }

                                                             
    PqGSSRecvLength = 0;

       
                                                                         
           
       
    if (output.length > 0)
    {
      uint32 netlen = htonl(output.length);

      if (output.length > PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32))
      {
        ereport(COMMERROR, (errmsg("server tried to send oversize GSSAPI packet (%zu > %zu)", (size_t)output.length, PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32))));
        gss_release_buffer(&minor, &output);
        return -1;
      }

      memcpy(PqGSSSendBuffer, (char *)&netlen, sizeof(uint32));
      PqGSSSendLength += sizeof(uint32);

      memcpy(PqGSSSendBuffer + PqGSSSendLength, output.value, output.length);
      PqGSSSendLength += output.length;

                                                       

      while (PqGSSSendNext < PqGSSSendLength)
      {
        ret = secure_raw_write(port, PqGSSSendBuffer + PqGSSSendNext, PqGSSSendLength - PqGSSSendNext);

           
                                                      
                                                   
           
        if (ret < 0 && !(errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR))
        {
          gss_release_buffer(&minor, &output);
          return -1;
        }

                                                     
        if (ret <= 0)
        {
          WaitLatchOrSocket(MyLatch, WL_SOCKET_WRITEABLE | WL_EXIT_ON_PM_DEATH, port->sock, 0, WAIT_EVENT_GSS_OPEN_SERVER);
          continue;
        }

        PqGSSSendNext += ret;
      }

                                                     
      PqGSSSendLength = PqGSSSendNext = 0;

      gss_release_buffer(&minor, &output);
    }

       
                                                                        
                                                       
       
    if (complete_next)
    {
      break;
    }
  }

     
                                                                       
                                                                 
     
  major = gss_wrap_size_limit(&minor, port->gss->ctx, 1, GSS_C_QOP_DEFAULT, PQ_GSS_SEND_BUFFER_SIZE - sizeof(uint32), &PqGSSMaxPktSize);

  if (GSS_ERROR(major))
  {
    pg_GSS_error(_("GSSAPI size check error"), major, minor);
    return -1;
  }

  port->gss->enc = true;

  return 0;
}

   
                                                                
   
bool
be_gssapi_get_auth(Port *port)
{
  if (!port || !port->gss)
  {
    return false;
  }

  return port->gss->auth;
}

   
                                                                             
   
bool
be_gssapi_get_enc(Port *port)
{
  if (!port || !port->gss)
  {
    return false;
  }

  return port->gss->enc;
}

   
                                                                          
                                                       
   
const char *
be_gssapi_get_princ(Port *port)
{
  if (!port || !port->gss)
  {
    return NULL;
  }

  return port->gss->princ;
}
