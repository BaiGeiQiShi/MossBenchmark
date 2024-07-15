                                                                            
   
         
              
   
                
                                    
   
                                                                
                                                                     
                                                                    
                                                                        
                                                                              
   
                                                                       
                                
   
                                                                          
             
   
   
                                                                         
                                                                        
   
                  
                                    
   
                                                                            
   

#include "postgres_fe.h"

#include <signal.h>
#include <time.h>

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "libpq-fe.h"
#include "libpq-int.h"
#include "mb/pg_wchar.h"
#include "port/pg_bswap.h"
#include "pg_config_paths.h"

static int
pqPutMsgBytes(const void *buf, size_t len, PGconn *conn);
static int
pqSendSome(PGconn *conn, int len);
static int
pqSocketCheck(PGconn *conn, int forRead, int forWrite, time_t end_time);
static int
pqSocketPoll(int sock, int forRead, int forWrite, time_t end_time);

   
                                                 
   
int
PQlibVersion(void)
{
  return PG_VERSION_NUM;
}

   
                                               
   
                                                                 
                                                                
   
static void
fputnbytes(FILE *f, const char *str, size_t n)
{
  while (n-- > 0)
  {
    fputc(*str++, f);
  }
}

   
                                               
   
                                                         
                                                                      
                                                                   
   
int
pqGetc(char *result, PGconn *conn)
{
  if (conn->inCursor >= conn->inEnd)
  {
    return EOF;
  }

  *result = conn->inBuffer[conn->inCursor++];

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "From backend> %c\n", *result);
  }

  return 0;
}

   
                                               
   
int
pqPutc(char c, PGconn *conn)
{
  if (pqPutMsgBytes(&c, 1, conn))
  {
    return EOF;
  }

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "To backend> %c\n", c);
  }

  return 0;
}

   
                    
                                                     
                                              
                                                             
                                                     
   
static int
pqGets_internal(PQExpBuffer buf, PGconn *conn, bool resetbuffer)
{
                                                       
  char *inBuffer = conn->inBuffer;
  int inCursor = conn->inCursor;
  int inEnd = conn->inEnd;
  int slen;

  while (inCursor < inEnd && inBuffer[inCursor])
  {
    inCursor++;
  }

  if (inCursor >= inEnd)
  {
    return EOF;
  }

  slen = inCursor - conn->inCursor;

  if (resetbuffer)
  {
    resetPQExpBuffer(buf);
  }

  appendBinaryPQExpBuffer(buf, inBuffer + conn->inCursor, slen);

  conn->inCursor = ++inCursor;

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "From backend> \"%s\"\n", buf->data);
  }

  return 0;
}

int
pqGets(PQExpBuffer buf, PGconn *conn)
{
  return pqGets_internal(buf, conn, true);
}

int
pqGets_append(PQExpBuffer buf, PGconn *conn)
{
  return pqGets_internal(buf, conn, false);
}

   
                                                                 
   
int
pqPuts(const char *s, PGconn *conn)
{
  if (pqPutMsgBytes(s, strlen(s) + 1, conn))
  {
    return EOF;
  }

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "To backend> \"%s\"\n", s);
  }

  return 0;
}

   
               
                                                                      
   
int
pqGetnchar(char *s, size_t len, PGconn *conn)
{
  if (len > (size_t)(conn->inEnd - conn->inCursor))
  {
    return EOF;
  }

  memcpy(s, conn->inBuffer + conn->inCursor, len);
                           

  conn->inCursor += len;

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "From backend (%lu)> ", (unsigned long)len);
    fputnbytes(conn->Pfdebug, s, len);
    fprintf(conn->Pfdebug, "\n");
  }

  return 0;
}

   
                
                                        
   
                                                                     
                                                                          
                                                                         
   
int
pqSkipnchar(size_t len, PGconn *conn)
{
  if (len > (size_t)(conn->inEnd - conn->inCursor))
  {
    return EOF;
  }

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "From backend (%lu)> ", (unsigned long)len);
    fputnbytes(conn->Pfdebug, conn->inBuffer + conn->inCursor, len);
    fprintf(conn->Pfdebug, "\n");
  }

  conn->inCursor += len;

  return 0;
}

   
               
                                                  
   
int
pqPutnchar(const char *s, size_t len, PGconn *conn)
{
  if (pqPutMsgBytes(s, len, conn))
  {
    return EOF;
  }

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "To backend> ");
    fputnbytes(conn->Pfdebug, s, len);
    fprintf(conn->Pfdebug, "\n");
  }

  return 0;
}

   
            
                                                                  
                       
   
int
pqGetInt(int *result, size_t bytes, PGconn *conn)
{
  uint16 tmp2;
  uint32 tmp4;

  switch (bytes)
  {
  case 2:
    if (conn->inCursor + 2 > conn->inEnd)
    {
      return EOF;
    }
    memcpy(&tmp2, conn->inBuffer + conn->inCursor, 2);
    conn->inCursor += 2;
    *result = (int)pg_ntoh16(tmp2);
    break;
  case 4:
    if (conn->inCursor + 4 > conn->inEnd)
    {
      return EOF;
    }
    memcpy(&tmp4, conn->inBuffer + conn->inCursor, 4);
    conn->inCursor += 4;
    *result = (int)pg_ntoh32(tmp4);
    break;
  default:
    pqInternalNotice(&conn->noticeHooks, "integer of size %lu not supported by pqGetInt", (unsigned long)bytes);
    return EOF;
  }

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "From backend (#%lu)> %d\n", (unsigned long)bytes, *result);
  }

  return 0;
}

   
            
                                                                     
                          
   
int
pqPutInt(int value, size_t bytes, PGconn *conn)
{
  uint16 tmp2;
  uint32 tmp4;

  switch (bytes)
  {
  case 2:
    tmp2 = pg_hton16((uint16)value);
    if (pqPutMsgBytes((const char *)&tmp2, 2, conn))
    {
      return EOF;
    }
    break;
  case 4:
    tmp4 = pg_hton32((uint32)value);
    if (pqPutMsgBytes((const char *)&tmp4, 4, conn))
    {
      return EOF;
    }
    break;
  default:
    pqInternalNotice(&conn->noticeHooks, "integer of size %lu not supported by pqPutInt", (unsigned long)bytes);
    return EOF;
  }

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "To backend (%lu#)> %d\n", (unsigned long)bytes, value);
  }

  return 0;
}

   
                                                                           
                                                
   
                                                         
   
int
pqCheckOutBufferSpace(size_t bytes_needed, PGconn *conn)
{
  int newsize = conn->outBufSize;
  char *newbuf;

                                          
  if (bytes_needed <= (size_t)newsize)
  {
    return 0;
  }

     
                                                                             
                                                                           
                                                     
     
                                                                
     
  do
  {
    newsize *= 2;
  } while (newsize > 0 && bytes_needed > (size_t)newsize);

  if (newsize > 0 && bytes_needed <= (size_t)newsize)
  {
    newbuf = realloc(conn->outBuffer, newsize);
    if (newbuf)
    {
                             
      conn->outBuffer = newbuf;
      conn->outBufSize = newsize;
      return 0;
    }
  }

  newsize = conn->outBufSize;
  do
  {
    newsize += 8192;
  } while (newsize > 0 && bytes_needed > (size_t)newsize);

  if (newsize > 0 && bytes_needed <= (size_t)newsize)
  {
    newbuf = realloc(conn->outBuffer, newsize);
    if (newbuf)
    {
                             
      conn->outBuffer = newbuf;
      conn->outBufSize = newsize;
      return 0;
    }
  }

                                              
  printfPQExpBuffer(&conn->errorMessage, "cannot allocate memory for output buffer\n");
  return EOF;
}

   
                                                                          
                                                
   
                                                         
   
int
pqCheckInBufferSpace(size_t bytes_needed, PGconn *conn)
{
  int newsize = conn->inBufSize;
  char *newbuf;

                                          
  if (bytes_needed <= (size_t)newsize)
  {
    return 0;
  }

     
                                                                        
                                                                        
                                                                         
                                                                          
                                                                           
     
  bytes_needed -= conn->inStart;

  if (conn->inStart < conn->inEnd)
  {
    if (conn->inStart > 0)
    {
      memmove(conn->inBuffer, conn->inBuffer + conn->inStart, conn->inEnd - conn->inStart);
      conn->inEnd -= conn->inStart;
      conn->inCursor -= conn->inStart;
      conn->inStart = 0;
    }
  }
  else
  {
                                             
    conn->inStart = conn->inCursor = conn->inEnd = 0;
  }

                                            
  if (bytes_needed <= (size_t)newsize)
  {
    return 0;
  }

     
                                                                             
                                                                           
                                                     
     
                                                                
     
  do
  {
    newsize *= 2;
  } while (newsize > 0 && bytes_needed > (size_t)newsize);

  if (newsize > 0 && bytes_needed <= (size_t)newsize)
  {
    newbuf = realloc(conn->inBuffer, newsize);
    if (newbuf)
    {
                             
      conn->inBuffer = newbuf;
      conn->inBufSize = newsize;
      return 0;
    }
  }

  newsize = conn->inBufSize;
  do
  {
    newsize += 8192;
  } while (newsize > 0 && bytes_needed > (size_t)newsize);

  if (newsize > 0 && bytes_needed <= (size_t)newsize)
  {
    newbuf = realloc(conn->inBuffer, newsize);
    if (newbuf)
    {
                             
      conn->inBuffer = newbuf;
      conn->inBufSize = newsize;
      return 0;
    }
  }

                                              
  printfPQExpBuffer(&conn->errorMessage, "cannot allocate memory for input buffer\n");
  return EOF;
}

   
                                                                
   
                                                                           
                                             
   
                                                                         
                                
   
                                      
   
                                                                      
                                                             
                                                                              
                                                                            
                                                                       
   
                                                                           
                                                                         
                                                                          
                                                                        
                                                            
   
int
pqPutMsgStart(char msg_type, bool force_len, PGconn *conn)
{
  int lenPos;
  int endPos;

                                        
  if (msg_type)
  {
    endPos = conn->outCount + 1;
  }
  else
  {
    endPos = conn->outCount;
  }

                                 
  if (force_len || PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    lenPos = endPos;
                                       
    endPos += 4;
  }
  else
  {
    lenPos = -1;
  }

                                                  
  if (pqCheckOutBufferSpace(endPos, conn))
  {
    return EOF;
  }
                                               
  if (msg_type)
  {
    conn->outBuffer[conn->outCount] = msg_type;
  }
                                   
  conn->outMsgStart = lenPos;
  conn->outMsgEnd = endPos;
                                                                

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "To backend> Msg %c\n", msg_type ? msg_type : ' ');
  }

  return 0;
}

   
                                                               
   
                                      
   
static int
pqPutMsgBytes(const void *buf, size_t len, PGconn *conn)
{
                                      
  if (pqCheckOutBufferSpace(conn->outMsgEnd + len, conn))
  {
    return EOF;
  }
                           
  memcpy(conn->outBuffer + conn->outMsgEnd, buf, len);
  conn->outMsgEnd += len;
                                                 
  return 0;
}

   
                                                                   
   
                                      
   
                                                                          
                                                                         
                                                                           
                                                                
   
int
pqPutMsgEnd(PGconn *conn)
{
  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "To backend> Msg complete, length %u\n", conn->outMsgEnd - conn->outCount);
  }

                                     
  if (conn->outMsgStart >= 0)
  {
    uint32 msgLen = conn->outMsgEnd - conn->outMsgStart;

    msgLen = pg_hton32(msgLen);
    memcpy(conn->outBuffer + conn->outMsgStart, &msgLen, 4);
  }

                                     
  conn->outCount = conn->outMsgEnd;

  if (conn->outCount >= 8192)
  {
    int toSend = conn->outCount - (conn->outCount % 8192);

    if (pqSendSome(conn, toSend) < 0)
    {
      return EOF;
    }
                                                                   
  }

  return 0;
}

              
                                                   
                           
                                                  
                                                             
                                                            
                           
                                                                              
                                  
              
   
int
pqReadData(PGconn *conn)
{
  int someread = 0;
  int nread;

  if (conn->sock == PGINVALID_SOCKET)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("connection not open\n"));
    return -1;
  }

                                                        
  if (conn->inStart < conn->inEnd)
  {
    if (conn->inStart > 0)
    {
      memmove(conn->inBuffer, conn->inBuffer + conn->inStart, conn->inEnd - conn->inStart);
      conn->inEnd -= conn->inStart;
      conn->inCursor -= conn->inStart;
      conn->inStart = 0;
    }
  }
  else
  {
                                             
    conn->inStart = conn->inCursor = conn->inEnd = 0;
  }

     
                                                                             
                                                                             
                                                                          
                                                                          
                                                                            
                        
     
  if (conn->inBufSize - conn->inEnd < 8192)
  {
    if (pqCheckInBufferSpace(conn->inEnd + (size_t)8192, conn))
    {
         
                                                                        
         
      if (conn->inBufSize - conn->inEnd < 100)
      {
        return -1;                               
      }
    }
  }

                                 
retry3:
  nread = pqsecure_read(conn, conn->inBuffer + conn->inEnd, conn->inBufSize - conn->inEnd);
  if (nread < 0)
  {
    if (SOCK_ERRNO == EINTR)
    {
      goto retry3;
    }
                                                            
#ifdef EAGAIN
    if (SOCK_ERRNO == EAGAIN)
    {
      return someread;
    }
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    if (SOCK_ERRNO == EWOULDBLOCK)
    {
      return someread;
    }
#endif
                                                                    
#ifdef ECONNRESET
    if (SOCK_ERRNO == ECONNRESET)
    {
      goto definitelyFailed;
    }
#endif
                                                    
    return -1;
  }
  if (nread > 0)
  {
    conn->inEnd += nread;

       
                                                                           
                                                                        
                                                                        
                                                                          
                                                                      
                                                                           
       
                                                                     
                                                                        
                                                        
       
    if (conn->inEnd > 32768 && (conn->inBufSize - conn->inEnd) >= 8192)
    {
      someread = 1;
      goto retry3;
    }
    return 1;
  }

  if (someread)
  {
    return 1;                                             
  }

     
                                                                           
                                                                          
                                                                         
                                                                        
                                                                        
                                                                          
                                            
     
                                                                          
                                                                             
                                                                            
                                                                           
                                                        
     

#ifdef USE_SSL
  if (conn->ssl_in_use)
  {
    return 0;
  }
#endif

  switch (pqReadReady(conn))
  {
  case 0:
                                      
    return 0;
  case 1:
                        
    break;
  default:
                                                                      
    goto definitelyEOF;
  }

     
                                                                     
              
     
retry4:
  nread = pqsecure_read(conn, conn->inBuffer + conn->inEnd, conn->inBufSize - conn->inEnd);
  if (nread < 0)
  {
    if (SOCK_ERRNO == EINTR)
    {
      goto retry4;
    }
                                                            
#ifdef EAGAIN
    if (SOCK_ERRNO == EAGAIN)
    {
      return 0;
    }
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    if (SOCK_ERRNO == EWOULDBLOCK)
    {
      return 0;
    }
#endif
                                                                    
#ifdef ECONNRESET
    if (SOCK_ERRNO == ECONNRESET)
    {
      goto definitelyFailed;
    }
#endif
                                                    
    return -1;
  }
  if (nread > 0)
  {
    conn->inEnd += nread;
    return 1;
  }

     
                                                                          
                                                  
     
definitelyEOF:
  printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server closed the connection unexpectedly\n"
                                                       "\tThis probably means the server terminated abnormally\n"
                                                       "\tbefore or while processing the request.\n"));

                                                                         
definitelyFailed:
                                                                  
  pqDropConnection(conn, false);
  conn->status = CONNECTION_BAD;                                    
  return -1;
}

   
                                                       
   
                                                                        
             
   
                                                                            
                                                                      
   
                                                                         
                                                                       
                                       
   
                                                                          
                                                                           
                                                                              
                                                                          
                                                                              
                                                                        
   
static int
pqSendSome(PGconn *conn, int len)
{
  char *ptr = conn->outBuffer;
  int remaining = conn->outCount;
  int result = 0;

     
                                                                             
                                                                          
                                                                     
                                                                           
                                                                            
                                                                          
                                                   
     
  if (conn->write_failed)
  {
                                                      
    conn->outCount = 0;
                                                             
    if (conn->sock != PGINVALID_SOCKET)
    {
      if (pqReadData(conn) < 0)
      {
        return -1;
      }
    }
    return 0;
  }

  if (conn->sock == PGINVALID_SOCKET)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("connection not open\n"));
    conn->write_failed = true;
                                                                    
                                                  
    conn->write_err_msg = strdup(conn->errorMessage.data);
    resetPQExpBuffer(&conn->errorMessage);
                                                           
    conn->outCount = 0;
    return 0;
  }

                                        
  while (len > 0)
  {
    int sent;

#ifndef WIN32
    sent = pqsecure_write(conn, ptr, len);
#else

       
                                                                    
                                                                      
                                               
       
    sent = pqsecure_write(conn, ptr, Min(len, 65536));
#endif

    if (sent < 0)
    {
                                                               
      switch (SOCK_ERRNO)
      {
#ifdef EAGAIN
      case EAGAIN:
        break;
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
      case EWOULDBLOCK:
        break;
#endif
      case EINTR:
        continue;

      default:
                                                         
        conn->write_failed = true;

           
                                                             
                                                              
           
                                                                   
                                                                
                                                                
                                                                 
                                                   
           
        conn->write_err_msg = strdup(conn->errorMessage.data);
        resetPQExpBuffer(&conn->errorMessage);

                                                               
        conn->outCount = 0;

                                                                 
        if (conn->sock != PGINVALID_SOCKET)
        {
          if (pqReadData(conn) < 0)
          {
            return -1;
          }
        }
        return 0;
      }
    }
    else
    {
      ptr += sent;
      len -= sent;
      remaining -= sent;
    }

    if (len > 0)
    {
         
                                                            
         
                                                                     
                                                                         
                                                                        
                                                                        
                                                                   
                                                                       
                                                                   
                                                                     
                                                                       
                                                       
         
                                                                         
                                                                         
                                                            
                                                                         
                                                                   
                                                                      
                                                                        
                                                                    
                                                                      
         
                                                                     
              
         
      if (pqReadData(conn) < 0)
      {
        result = -1;                                   
        break;
      }

      if (pqIsnonblocking(conn))
      {
        result = 1;
        break;
      }

      if (pqWait(true, true, conn))
      {
        result = -1;
        break;
      }
    }
  }

                                                  
  if (remaining > 0)
  {
    memmove(conn->outBuffer, ptr, remaining);
  }
  conn->outCount = remaining;

  return result;
}

   
                                                       
   
                                                                            
                                                                      
                                                                  
   
int
pqFlush(PGconn *conn)
{
  if (conn->Pfdebug)
  {
    fflush(conn->Pfdebug);
  }

  if (conn->outCount > 0)
  {
    return pqSendSome(conn, conn->outCount);
  }

  return 0;
}

   
                                                                 
   
                                                                              
                     
   
                                                                              
                                                                            
                                                      
   
int
pqWait(int forRead, int forWrite, PGconn *conn)
{
  return pqWaitTimed(forRead, forWrite, conn, (time_t)-1);
}

   
                                                
   
                                                        
   
                                                                                   
   
int
pqWaitTimed(int forRead, int forWrite, PGconn *conn, time_t finish_time)
{
  int result;

  result = pqSocketCheck(conn, forRead, forWrite, finish_time);

  if (result < 0)
  {
    return -1;                                  
  }

  if (result == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("timeout expired\n"));
    return 1;
  }

  return 0;
}

   
                                                              
                                                      
   
int
pqReadReady(PGconn *conn)
{
  return pqSocketCheck(conn, 1, 0, (time_t)0);
}

   
                                                                
                                                      
   
int
pqWriteReady(PGconn *conn)
{
  return pqSocketCheck(conn, 0, 1, (time_t)0);
}

   
                                                                        
                                                                         
                                 
   
                                                                            
                           
   
static int
pqSocketCheck(PGconn *conn, int forRead, int forWrite, time_t end_time)
{
  int result;

  if (!conn)
  {
    return -1;
  }
  if (conn->sock == PGINVALID_SOCKET)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("invalid socket\n"));
    return -1;
  }

#ifdef USE_SSL
                                                  
  if (forRead && conn->ssl_in_use && pgtls_read_pending(conn))
  {
                                  
    return 1;
  }
#endif

                                             
  do
  {
    result = pqSocketPoll(conn->sock, forRead, forWrite, end_time);
  } while (result < 0 && SOCK_ERRNO == EINTR);

  if (result < 0)
  {
    char sebuf[PG_STRERROR_R_BUFLEN];

    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("select() failed: %s\n"), SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
  }

  return result;
}

   
                                                                         
                                                                         
                                                                  
                                                                
   
                                                                              
                                                      
   
static int
pqSocketPoll(int sock, int forRead, int forWrite, time_t end_time)
{
                                                        
#ifdef HAVE_POLL
  struct pollfd input_fd;
  int timeout_ms;

  if (!forRead && !forWrite)
  {
    return 0;
  }

  input_fd.fd = sock;
  input_fd.events = POLLERR;
  input_fd.revents = 0;

  if (forRead)
  {
    input_fd.events |= POLLIN;
  }
  if (forWrite)
  {
    input_fd.events |= POLLOUT;
  }

                                            
  if (end_time == ((time_t)-1))
  {
    timeout_ms = -1;
  }
  else
  {
    time_t now = time(NULL);

    if (end_time > now)
    {
      timeout_ms = (end_time - now) * 1000;
    }
    else
    {
      timeout_ms = 0;
    }
  }

  return poll(&input_fd, 1, timeout_ms);
#else                  

  fd_set input_mask;
  fd_set output_mask;
  fd_set except_mask;
  struct timeval timeout;
  struct timeval *ptr_timeout;

  if (!forRead && !forWrite)
  {
    return 0;
  }

  FD_ZERO(&input_mask);
  FD_ZERO(&output_mask);
  FD_ZERO(&except_mask);
  if (forRead)
  {
    FD_SET(sock, &input_mask);
  }

  if (forWrite)
  {
    FD_SET(sock, &output_mask);
  }
  FD_SET(sock, &except_mask);

                                            
  if (end_time == ((time_t)-1))
  {
    ptr_timeout = NULL;
  }
  else
  {
    time_t now = time(NULL);

    if (end_time > now)
    {
      timeout.tv_sec = end_time - now;
    }
    else
    {
      timeout.tv_sec = 0;
    }
    timeout.tv_usec = 0;
    ptr_timeout = &timeout;
  }

  return select(sock + 1, &input_mask, &output_mask, &except_mask, ptr_timeout);
#endif                
}

   
                                                                      
                                                
   

   
                                                                      
                       
   
int
PQmblen(const char *s, int encoding)
{
  return pg_encoding_mblen(encoding, s);
}

   
                                                                         
                       
   
int
PQdsplen(const char *s, int encoding)
{
  return pg_encoding_dsplen(encoding, s);
}

   
                                                               
   
int
PQenv2encoding(void)
{
  char *str;
  int encoding = PG_SQL_ASCII;

  str = getenv("PGCLIENTENCODING");
  if (str && *str != '\0')
  {
    encoding = pg_char_to_encoding(str);
    if (encoding < 0)
    {
      encoding = PG_SQL_ASCII;
    }
  }
  return encoding;
}

#ifdef ENABLE_NLS

static void
libpq_binddomain()
{
     
                                                                             
                                                                            
                                                                    
                                                                            
                                                      
     
  static volatile bool already_bound = false;

  if (!already_bound)
  {
                                                  
#ifdef WIN32
    int save_errno = GetLastError();
#else
    int save_errno = errno;
#endif
    const char *ldir;

                                                                         
    ldir = getenv("PGLOCALEDIR");
    if (!ldir)
    {
      ldir = LOCALEDIR;
    }
    bindtextdomain(PG_TEXTDOMAIN("libpq"), ldir);
    already_bound = true;
#ifdef WIN32
    SetLastError(save_errno);
#else
    errno = save_errno;
#endif
  }
}

char *
libpq_gettext(const char *msgid)
{
  libpq_binddomain();
  return dgettext(PG_TEXTDOMAIN("libpq"), msgid);
}

char *
libpq_ngettext(const char *msgid, const char *msgid_plural, unsigned long n)
{
  libpq_binddomain();
  return dngettext(PG_TEXTDOMAIN("libpq"), msgid, msgid_plural, n);
}

#endif                 
