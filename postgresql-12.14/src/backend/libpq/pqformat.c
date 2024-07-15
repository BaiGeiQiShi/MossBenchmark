                                                                            
   
              
                                                                  
   
                                                                               
                                                                               
                                                                             
                                                                              
                                                                            
                                                                           
          
   
                                                                      
                                                                             
                   
   
                                                                              
                                                                          
                                                                       
                  
   
   
                                                                         
                                                                        
   
                                
   
                                                                            
   
   
                      
                                
                                                   
                                                            
                                                                 
                                                                     
                                                           
                                                           
                                                          
                                                                                      
                                                          
                                                                           
                                                                                     
                                                               
                                                                           
                                                                           
                                           
   
                                                                              
                                                   
                                                              
   
                                
                                                                               
                                                                         
   
                                
                                                         
                                                              
                                                                   
                                                         
                                                         
                                                        
                                                          
                                                                
                                                                          
                                                                           
                                                 
   

#include "postgres.h"

#include <sys/param.h>

#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "port/pg_bswap.h"

                                    
                                                        
                                    
   
void
pq_beginmessage(StringInfo buf, char msgtype)
{
  initStringInfo(buf);

     
                                                                         
                                                                          
                                                                           
     
  buf->cursor = msgtype;
}

                                    
 
                                                                           
   
                                                                         
                   
                                    
   
void
pq_beginmessage_reuse(StringInfo buf, char msgtype)
{
  resetStringInfo(buf);

     
                                                                         
                                                                          
                                                                           
     
  buf->cursor = msgtype;
}

                                    
                                                          
                                    
   
void
pq_sendbytes(StringInfo buf, const char *data, int datalen)
{
                                                                       
  appendBinaryStringInfo(buf, data, datalen);
}

                                    
                                                                                      
   
                                                                         
                                                                         
                                                                            
                                                                         
                                 
                                    
   
void
pq_sendcountedtext(StringInfo buf, const char *str, int slen, bool countincludesself)
{
  int extra = countincludesself ? 4 : 0;
  char *p;

  p = pg_server_to_client(str, slen);
  if (p != str)                                       
  {
    slen = strlen(p);
    pq_sendint32(buf, slen + extra);
    appendBinaryStringInfoNT(buf, p, slen);
    pfree(p);
  }
  else
  {
    pq_sendint32(buf, slen + extra);
    appendBinaryStringInfoNT(buf, str, slen);
  }
}

                                    
                                                          
   
                                                                         
                                                                        
                                                                      
                                                                         
                       
                                    
   
void
pq_sendtext(StringInfo buf, const char *str, int slen)
{
  char *p;

  p = pg_server_to_client(str, slen);
  if (p != str)                                       
  {
    slen = strlen(p);
    appendBinaryStringInfo(buf, p, slen);
    pfree(p);
  }
  else
  {
    appendBinaryStringInfo(buf, str, slen);
  }
}

                                    
                                                                           
   
                                                                      
                         
                                    
   
void
pq_sendstring(StringInfo buf, const char *str)
{
  int slen = strlen(str);
  char *p;

  p = pg_server_to_client(str, slen);
  if (p != str)                                       
  {
    slen = strlen(p);
    appendBinaryStringInfoNT(buf, p, slen + 1);
    pfree(p);
  }
  else
  {
    appendBinaryStringInfoNT(buf, str, slen + 1);
  }
}

                                    
                                                                                     
   
                                                                          
                                                                          
                                                                          
                                                                            
                                                                          
                                                                             
                                                                         
   
                                                                      
                         
                                    
   
void
pq_send_ascii_string(StringInfo buf, const char *str)
{
  while (*str)
  {
    char ch = *str++;

    if (IS_HIGHBIT_SET(ch))
    {
      ch = '?';
    }
    appendStringInfoCharMacro(buf, ch);
  }
  appendStringInfoChar(buf, '\0');
}

                                    
                                                           
   
                                                                             
                                                                        
   
                                                                          
                                                                         
                                        
                                    
   
void
pq_sendfloat4(StringInfo buf, float4 f)
{
  union
  {
    float4 f;
    uint32 i;
  } swap;

  swap.f = f;
  pq_sendint32(buf, swap.i);
}

                                    
                                                           
   
                                                                             
                                                                        
   
                                                                          
                                                                         
                                        
                                    
   
void
pq_sendfloat8(StringInfo buf, float8 f)
{
  union
  {
    float8 f;
    int64 i;
  } swap;

  swap.f = f;
  pq_sendint64(buf, swap.i);
}

                                    
                                                               
   
                                                                         
                                                       
                                    
   
void
pq_endmessage(StringInfo buf)
{
                                         
  (void)pq_putmessage(buf->cursor, buf->data, buf->len);
                                                                         
  pfree(buf->data);
  buf->data = NULL;
}

                                    
                                                                     
   
                                                                     
                          
                                  
   

void
pq_endmessage_reuse(StringInfo buf)
{
                                         
  (void)pq_putmessage(buf->cursor, buf->data, buf->len);
}

                                    
                                                                  
                                    
   
void
pq_begintypsend(StringInfo buf)
{
  initStringInfo(buf);
                                                    
  appendStringInfoCharMacro(buf, '\0');
  appendStringInfoCharMacro(buf, '\0');
  appendStringInfoCharMacro(buf, '\0');
  appendStringInfoCharMacro(buf, '\0');
}

                                    
                                                       
   
                                                                        
                                                                            
                                                                           
                        
                                    
   
bytea *
pq_endtypsend(StringInfo buf)
{
  bytea *result = (bytea *)buf->data;

                                                    
  Assert(buf->len >= VARHDRSZ);
  SET_VARSIZE(result, buf->len);

  return result;
}

                                    
                                                                               
   
                                                                        
                                                                   
                        
                                    
   
void
pq_puttextmessage(char msgtype, const char *str)
{
  int slen = strlen(str);
  char *p;

  p = pg_server_to_client(str, slen);
  if (p != str)                                       
  {
    (void)pq_putmessage(msgtype, p, strlen(p) + 1);
    pfree(p);
    return;
  }
  (void)pq_putmessage(msgtype, str, slen + 1);
}

                                    
                                                                         
                                    
   
void
pq_putemptymessage(char msgtype)
{
  (void)pq_putmessage(msgtype, NULL, 0);
}

                                    
                                                         
                                    
   
int
pq_getmsgbyte(StringInfo msg)
{
  if (msg->cursor >= msg->len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("no data left in message")));
  }
  return (unsigned char)msg->data[msg->cursor++];
}

                                    
                                                              
   
                                    
                                    
   
unsigned int
pq_getmsgint(StringInfo msg, int b)
{
  unsigned int result;
  unsigned char n8;
  uint16 n16;
  uint32 n32;

  switch (b)
  {
  case 1:
    pq_copymsgbytes(msg, (char *)&n8, 1);
    result = n8;
    break;
  case 2:
    pq_copymsgbytes(msg, (char *)&n16, 2);
    result = pg_ntoh16(n16);
    break;
  case 4:
    pq_copymsgbytes(msg, (char *)&n32, 4);
    result = pg_ntoh32(n32);
    break;
  default:
    elog(ERROR, "unsupported integer size %d", b);
    result = 0;                          
    break;
  }
  return result;
}

                                    
                                                                   
   
                                                                             
                                                                        
                                                
                                    
   
int64
pq_getmsgint64(StringInfo msg)
{
  uint64 n64;

  pq_copymsgbytes(msg, (char *)&n64, sizeof(n64));

  return pg_ntoh64(n64);
}

                                    
                                                         
   
                                
                                    
   
float4
pq_getmsgfloat4(StringInfo msg)
{
  union
  {
    float4 f;
    uint32 i;
  } swap;

  swap.i = pq_getmsgint(msg, 4);
  return swap.f;
}

                                    
                                                         
   
                                
                                    
   
float8
pq_getmsgfloat8(StringInfo msg)
{
  union
  {
    float8 f;
    int64 i;
  } swap;

  swap.i = pq_getmsgint64(msg);
  return swap.f;
}

                                    
                                                        
   
                                                                  
                                           
                                    
   
const char *
pq_getmsgbytes(StringInfo msg, int datalen)
{
  const char *result;

  if (datalen < 0 || datalen > (msg->len - msg->cursor))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("insufficient data left in message")));
  }
  result = &msg->data[msg->cursor];
  msg->cursor += datalen;
  return result;
}

                                    
                                                          
   
                                                             
                                    
   
void
pq_copymsgbytes(StringInfo msg, char *buf, int datalen)
{
  if (datalen < 0 || datalen > (msg->len - msg->cursor))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("insufficient data left in message")));
  }
  memcpy(buf, &msg->data[msg->cursor], datalen);
  msg->cursor += datalen;
}

                                    
                                                                
   
                                                           
                                                                           
                                    
   
char *
pq_getmsgtext(StringInfo msg, int rawbytes, int *nbytes)
{
  char *str;
  char *p;

  if (rawbytes < 0 || rawbytes > (msg->len - msg->cursor))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("insufficient data left in message")));
  }
  str = &msg->data[msg->cursor];
  msg->cursor += rawbytes;

  p = pg_client_to_server(str, rawbytes);
  if (p != str)                                       
  {
    *nbytes = strlen(p);
  }
  else
  {
    p = (char *)palloc(rawbytes + 1);
    memcpy(p, str, rawbytes);
    p[rawbytes] = '\0';
    *nbytes = rawbytes;
  }
  return p;
}

                                    
                                                                          
   
                                                                        
                                     
                                    
   
const char *
pq_getmsgstring(StringInfo msg)
{
  char *str;
  int slen;

  str = &msg->data[msg->cursor];

     
                                                                          
                                                                      
              
     
  slen = strlen(str);
  if (msg->cursor + slen >= msg->len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid string in message")));
  }
  msg->cursor += slen + 1;

  return pg_client_to_server(str, slen);
}

                                    
                                                                           
   
                                                        
                                    
   
const char *
pq_getmsgrawstring(StringInfo msg)
{
  char *str;
  int slen;

  str = &msg->data[msg->cursor];

     
                                                                          
                                                                      
              
     
  slen = strlen(str);
  if (msg->cursor + slen >= msg->len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid string in message")));
  }
  msg->cursor += slen + 1;

  return str;
}

                                    
                                                 
                                    
   
void
pq_getmsgend(StringInfo msg)
{
  if (msg->cursor != msg->len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid message format")));
  }
}
