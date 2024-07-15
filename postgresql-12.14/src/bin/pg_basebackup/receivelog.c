                                                                            
   
                                                        
                              
   
                                                 
   
                                                                         
   
                  
                                         
                                                                            
   

#include "postgres_fe.h"

#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

                    
#include "receivelog.h"
#include "streamutil.h"

#include "libpq-fe.h"
#include "access/xlog_internal.h"
#include "common/file_utils.h"
#include "common/logging.h"

                                                 
static Walfile *walfile = NULL;
static char current_walfile_name[MAXPGPATH] = "";
static bool reportFlushPosition = false;
static XLogRecPtr lastFlushPosition = InvalidXLogRecPtr;

static bool still_sending = true;                                       

static PGresult *
HandleCopyStream(PGconn *conn, StreamCtl *stream, XLogRecPtr *stoppos);
static int
CopyStreamPoll(PGconn *conn, long timeout_ms, pgsocket stop_socket);
static int
CopyStreamReceive(PGconn *conn, long timeout, pgsocket stop_socket, char **buffer);
static bool
ProcessKeepaliveMsg(PGconn *conn, StreamCtl *stream, char *copybuf, int len, XLogRecPtr blockpos, TimestampTz *last_status);
static bool
ProcessXLogDataMsg(PGconn *conn, StreamCtl *stream, char *copybuf, int len, XLogRecPtr *blockpos);
static PGresult *
HandleEndOfCopyStream(PGconn *conn, StreamCtl *stream, char *copybuf, XLogRecPtr blockpos, XLogRecPtr *stoppos);
static bool
CheckCopyStreamStop(PGconn *conn, StreamCtl *stream, XLogRecPtr blockpos, XLogRecPtr *stoppos);
static long
CalculateCopyStreamSleeptime(TimestampTz now, int standby_message_timeout, TimestampTz last_status);

static bool
ReadEndOfStreamingResult(PGresult *res, XLogRecPtr *startpos, uint32 *timeline);

static bool
mark_file_as_archived(StreamCtl *stream, const char *fname)
{
  Walfile *f;
  static char tmppath[MAXPGPATH];

  snprintf(tmppath, sizeof(tmppath), "archive_status/%s.done", fname);

  f = stream->walmethod->open_for_write(tmppath, NULL, 0);
  if (f == NULL)
  {
    pg_log_error("could not create archive status file \"%s\": %s", tmppath, stream->walmethod->getlasterror());
    return false;
  }

  if (stream->walmethod->close(f, CLOSE_NORMAL) != 0)
  {
    pg_log_error("could not close archive status file \"%s\": %s", tmppath, stream->walmethod->getlasterror());
    return false;
  }

  return true;
}

   
                                                   
   
                                                                              
                                                                              
                                                                 
   
                                                
   
static bool
open_walfile(StreamCtl *stream, XLogRecPtr startpoint)
{
  Walfile *f;
  char *fn;
  ssize_t size;
  XLogSegNo segno;

  XLByteToSeg(startpoint, segno, WalSegSz);
  XLogFileName(current_walfile_name, stream->timeline, segno, WalSegSz);

                                                                  
  fn = stream->walmethod->get_file_name(current_walfile_name, stream->partial_suffix);

     
                                                                             
                                                                           
                                                                             
                                                                          
           
     
                                                                            
                                  
     
  if (stream->walmethod->compression() == 0 && stream->walmethod->existsfile(fn))
  {
    size = stream->walmethod->get_file_size(fn);
    if (size < 0)
    {
      pg_log_error("could not get size of write-ahead log file \"%s\": %s", fn, stream->walmethod->getlasterror());
      pg_free(fn);
      return false;
    }
    if (size == WalSegSz)
    {
                                                
      f = stream->walmethod->open_for_write(current_walfile_name, stream->partial_suffix, 0);
      if (f == NULL)
      {
        pg_log_error("could not open existing write-ahead log file \"%s\": %s", fn, stream->walmethod->getlasterror());
        pg_free(fn);
        return false;
      }

                                                  
      if (stream->walmethod->sync(f) != 0)
      {
        pg_log_error("could not fsync existing write-ahead log file \"%s\": %s", fn, stream->walmethod->getlasterror());
        pg_free(fn);
        stream->walmethod->close(f, CLOSE_UNLINK);
        return false;
      }

      walfile = f;
      pg_free(fn);
      return true;
    }
    if (size != 0)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      pg_log_error(ngettext("write-ahead log file \"%s\" has %d byte, should be 0 or %d", "write-ahead log file \"%s\" has %d bytes, should be 0 or %d", size), fn, (int)size, WalSegSz);
      pg_free(fn);
      return false;
    }
                                                              
  }

                                      

  f = stream->walmethod->open_for_write(current_walfile_name, stream->partial_suffix, WalSegSz);
  if (f == NULL)
  {
    pg_log_error("could not open write-ahead log file \"%s\": %s", fn, stream->walmethod->getlasterror());
    pg_free(fn);
    return false;
  }

  pg_free(fn);
  walfile = f;
  return true;
}

   
                                                                      
                                                                            
                                              
   
static bool
close_walfile(StreamCtl *stream, XLogRecPtr pos)
{
  off_t currpos;
  int r;

  if (walfile == NULL)
  {
    return true;
  }

  currpos = stream->walmethod->get_current_pos(walfile);
  if (currpos == -1)
  {
    pg_log_error("could not determine seek position in file \"%s\": %s", current_walfile_name, stream->walmethod->getlasterror());
    stream->walmethod->close(walfile, CLOSE_UNLINK);
    walfile = NULL;

    return false;
  }

  if (stream->partial_suffix)
  {
    if (currpos == WalSegSz)
    {
      r = stream->walmethod->close(walfile, CLOSE_NORMAL);
    }
    else
    {
      pg_log_info("not renaming \"%s%s\", segment is not complete", current_walfile_name, stream->partial_suffix);
      r = stream->walmethod->close(walfile, CLOSE_NO_RENAME);
    }
  }
  else
  {
    r = stream->walmethod->close(walfile, CLOSE_NORMAL);
  }

  walfile = NULL;

  if (r != 0)
  {
    pg_log_error("could not close file \"%s\": %s", current_walfile_name, stream->walmethod->getlasterror());
    return false;
  }

     
                                                                            
                                                                             
                                                                 
                                                      
     
  if (currpos == WalSegSz && stream->mark_done)
  {
                                        
    if (!mark_file_as_archived(stream, current_walfile_name))
    {
      return false;
    }
  }

  lastFlushPosition = pos;
  return true;
}

   
                                            
   
static bool
existsTimeLineHistoryFile(StreamCtl *stream)
{
  char histfname[MAXFNAMELEN];

     
                                                                          
                                       
     
  if (stream->timeline == 1)
  {
    return true;
  }

  TLHistoryFileName(histfname, stream->timeline);

  return stream->walmethod->existsfile(histfname);
}

static bool
writeTimeLineHistoryFile(StreamCtl *stream, char *filename, char *content)
{
  int size = strlen(content);
  char histfname[MAXFNAMELEN];
  Walfile *f;

     
                                                                          
                         
     
  TLHistoryFileName(histfname, stream->timeline);
  if (strcmp(histfname, filename) != 0)
  {
    pg_log_error("server reported unexpected history file name for timeline %u: %s", stream->timeline, filename);
    return false;
  }

  f = stream->walmethod->open_for_write(histfname, ".tmp", 0);
  if (f == NULL)
  {
    pg_log_error("could not create timeline history file \"%s\": %s", histfname, stream->walmethod->getlasterror());
    return false;
  }

  if ((int)stream->walmethod->write(f, content, size) != size)
  {
    pg_log_error("could not write timeline history file \"%s\": %s", histfname, stream->walmethod->getlasterror());

       
                                                                    
       
    stream->walmethod->close(f, CLOSE_UNLINK);

    return false;
  }

  if (stream->walmethod->close(f, CLOSE_NORMAL) != 0)
  {
    pg_log_error("could not close file \"%s\": %s", histfname, stream->walmethod->getlasterror());
    return false;
  }

                                                                   
  if (stream->mark_done)
  {
                                        
    if (!mark_file_as_archived(stream, histfname))
    {
      return false;
    }
  }

  return true;
}

   
                                                   
   
static bool
sendFeedback(PGconn *conn, XLogRecPtr blockpos, TimestampTz now, bool replyRequested)
{
  char replybuf[1 + 8 + 8 + 8 + 8 + 1];
  int len = 0;

  replybuf[len] = 'r';
  len += 1;
  fe_sendint64(blockpos, &replybuf[len]);            
  len += 8;
  if (reportFlushPosition)
  {
    fe_sendint64(lastFlushPosition, &replybuf[len]);            
  }
  else
  {
    fe_sendint64(InvalidXLogRecPtr, &replybuf[len]);            
  }
  len += 8;
  fe_sendint64(InvalidXLogRecPtr, &replybuf[len]);            
  len += 8;
  fe_sendint64(now, &replybuf[len]);               
  len += 8;
  replybuf[len] = replyRequested ? 1 : 0;                     
  len += 1;

  if (PQputCopyData(conn, replybuf, len) <= 0 || PQflush(conn))
  {
    pg_log_error("could not send feedback packet: %s", PQerrorMessage(conn));
    return false;
  }

  return true;
}

   
                                                                    
                        
   
                                                                              
   
bool
CheckServerVersionForStreaming(PGconn *conn)
{
  int minServerMajor, maxServerMajor;
  int serverMajor;

     
                                                                            
                                                                          
                                                                           
           
     
  minServerMajor = 903;
  maxServerMajor = PG_VERSION_NUM / 100;
  serverMajor = PQserverVersion(conn) / 100;
  if (serverMajor < minServerMajor)
  {
    const char *serverver = PQparameterStatus(conn, "server_version");

    pg_log_error("incompatible server version %s; client does not support streaming from server versions older than %s", serverver ? serverver : "'unknown'", "9.3");
    return false;
  }
  else if (serverMajor > maxServerMajor)
  {
    const char *serverver = PQparameterStatus(conn, "server_version");

    pg_log_error("incompatible server version %s; client does not support streaming from server versions newer than %s", serverver ? serverver : "'unknown'", PG_VERSION);
    return false;
  }
  return true;
}

   
                                                            
   
                                                                     
   
                                                                
                                                          
                                                 
   
                                                          
                                                                           
          
   
                                                           
                                                                   
                                                  
                                                                
                 
   
                                                                            
                                                                         
                                                                          
                                                                         
                                                                       
   
                                                                
                                                                     
                                    
                                                                
                    
   
                                                                         
                                                                           
                                                                          
                                                  
   
                                                                             
                                               
   
                                                            
   
bool
ReceiveXlogStream(PGconn *conn, StreamCtl *stream)
{
  char query[128];
  char slotcmd[128];
  PGresult *res;
  XLogRecPtr stoppos;

     
                                                                             
                                    
     
  if (!CheckServerVersionForStreaming(conn))
  {
    return false;
  }

     
                                                                           
                                                                   
                                                                            
                                   
     
                                                                      
                                                        
                                                                            
                                                               
     
  if (stream->replication_slot != NULL)
  {
    reportFlushPosition = true;
    sprintf(slotcmd, "SLOT \"%s\" ", stream->replication_slot);
  }
  else
  {
    if (stream->synchronous)
    {
      reportFlushPosition = true;
    }
    else
    {
      reportFlushPosition = false;
    }
    slotcmd[0] = 0;
  }

  if (stream->sysidentifier != NULL)
  {
                                                   
    res = PQexec(conn, "IDENTIFY_SYSTEM");
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      pg_log_error("could not send replication command \"%s\": %s", "IDENTIFY_SYSTEM", PQerrorMessage(conn));
      PQclear(res);
      return false;
    }
    if (PQntuples(res) != 1 || PQnfields(res) < 3)
    {
      pg_log_error("could not identify system: got %d rows and %d fields, expected %d rows and %d or more fields", PQntuples(res), PQnfields(res), 1, 3);
      PQclear(res);
      return false;
    }
    if (strcmp(stream->sysidentifier, PQgetvalue(res, 0, 0)) != 0)
    {
      pg_log_error("system identifier does not match between base backup and streaming connection");
      PQclear(res);
      return false;
    }
    if (stream->timeline > atoi(PQgetvalue(res, 0, 1)))
    {
      pg_log_error("starting timeline %u is not present in the server", stream->timeline);
      PQclear(res);
      return false;
    }
    PQclear(res);
  }

     
                                                                    
                                      
     
  lastFlushPosition = stream->startpos;

  while (1)
  {
       
                                                                           
                                                                      
                                                                  
                                                                     
       
    if (!existsTimeLineHistoryFile(stream))
    {
      snprintf(query, sizeof(query), "TIMELINE_HISTORY %u", stream->timeline);
      res = PQexec(conn, query);
      if (PQresultStatus(res) != PGRES_TUPLES_OK)
      {
                                                          
        pg_log_error("could not send replication command \"%s\": %s", "TIMELINE_HISTORY", PQresultErrorMessage(res));
        PQclear(res);
        return false;
      }

         
                                                                     
                                               
         
      if (PQnfields(res) != 2 || PQntuples(res) != 1)
      {
        pg_log_warning("unexpected response to TIMELINE_HISTORY command: got %d rows and %d fields, expected %d rows and %d fields", PQntuples(res), PQnfields(res), 1, 2);
      }

                                          
      writeTimeLineHistoryFile(stream, PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1));

      PQclear(res);
    }

       
                                                                           
                                       
       
    if (stream->stream_stop(stream->startpos, stream->timeline, false))
    {
      return true;
    }

                                                               
    snprintf(query, sizeof(query), "START_REPLICATION %s%X/%X TIMELINE %u", slotcmd, (uint32)(stream->startpos >> 32), (uint32)stream->startpos, stream->timeline);
    res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COPY_BOTH)
    {
      pg_log_error("could not send replication command \"%s\": %s", "START_REPLICATION", PQresultErrorMessage(res));
      PQclear(res);
      return false;
    }
    PQclear(res);

                        
    res = HandleCopyStream(conn, stream, &stoppos);
    if (res == NULL)
    {
      goto error;
    }

       
                           
       
                                                                          
                                                              
                                                                     
                                                                      
                                                                      
                                       
       
    if (PQresultStatus(res) == PGRES_TUPLES_OK)
    {
         
                                                                   
                                                                        
                                                                       
                                                                        
                                                                     
                                                                        
                       
         
      uint32 newtimeline;
      bool parsed;

      parsed = ReadEndOfStreamingResult(res, &stream->startpos, &newtimeline);
      PQclear(res);
      if (!parsed)
      {
        goto error;
      }

                                                      
      if (newtimeline <= stream->timeline)
      {
        pg_log_error("server reported unexpected next timeline %u, following timeline %u", newtimeline, stream->timeline);
        goto error;
      }
      if (stream->startpos > stoppos)
      {
        pg_log_error("server stopped streaming timeline %u at %X/%X, but reported next timeline %u to begin at %X/%X", stream->timeline, (uint32)(stoppos >> 32), (uint32)stoppos, newtimeline, (uint32)(stream->startpos >> 32), (uint32)stream->startpos);
        goto error;
      }

                                                                   
      res = PQgetResult(conn);
      if (PQresultStatus(res) != PGRES_COMMAND_OK)
      {
        pg_log_error("unexpected termination of replication stream: %s", PQresultErrorMessage(res));
        PQclear(res);
        goto error;
      }
      PQclear(res);

         
                                                                    
                                                        
         
      stream->timeline = newtimeline;
      stream->startpos = stream->startpos - XLogSegmentOffset(stream->startpos, WalSegSz);
      continue;
    }
    else if (PQresultStatus(res) == PGRES_COMMAND_OK)
    {
      PQclear(res);

         
                                                                      
         
                                                                    
                   
         
      if (stream->stream_stop(stoppos, stream->timeline, false))
      {
        return true;
      }
      else
      {
        pg_log_error("replication stream was terminated before stop point");
        goto error;
      }
    }
    else
    {
                                     
      pg_log_error("unexpected termination of replication stream: %s", PQresultErrorMessage(res));
      PQclear(res);
      goto error;
    }
  }

error:
  if (walfile != NULL && stream->walmethod->close(walfile, CLOSE_NO_RENAME) != 0)
  {
    pg_log_error("could not close file \"%s\": %s", current_walfile_name, stream->walmethod->getlasterror());
  }
  walfile = NULL;
  return false;
}

   
                                                                              
                                                                          
   
static bool
ReadEndOfStreamingResult(PGresult *res, XLogRecPtr *startpos, uint32 *timeline)
{
  uint32 startpos_xlogid, startpos_xrecoff;

               
                                                              
     
                                  
                                    
                       
     
                                                                         
                                                                          
                                
               
     
  if (PQnfields(res) < 2 || PQntuples(res) != 1)
  {
    pg_log_error("unexpected result set after end-of-timeline: got %d rows and %d fields, expected %d rows and %d fields", PQntuples(res), PQnfields(res), 1, 2);
    return false;
  }

  *timeline = atoi(PQgetvalue(res, 0, 0));
  if (sscanf(PQgetvalue(res, 0, 1), "%X/%X", &startpos_xlogid, &startpos_xrecoff) != 2)
  {
    pg_log_error("could not parse next timeline's starting point \"%s\"", PQgetvalue(res, 0, 1));
    return false;
  }
  *startpos = ((uint64)startpos_xlogid << 32) | startpos_xrecoff;

  return true;
}

   
                                                                     
                                                            
   
                                                                          
                                                                          
                                             
   
static PGresult *
HandleCopyStream(PGconn *conn, StreamCtl *stream, XLogRecPtr *stoppos)
{
  char *copybuf = NULL;
  TimestampTz last_status = -1;
  XLogRecPtr blockpos = stream->startpos;

  still_sending = true;

  while (1)
  {
    int r;
    TimestampTz now;
    long sleeptime;

       
                                                                      
       
    if (!CheckCopyStreamStop(conn, stream, blockpos, stoppos))
    {
      goto error;
    }

    now = feGetCurrentTimestamp();

       
                                                                          
                                                    
       
    if (stream->synchronous && lastFlushPosition < blockpos && walfile != NULL)
    {
      if (stream->walmethod->sync(walfile) != 0)
      {
        pg_log_error("could not fsync file \"%s\": %s", current_walfile_name, stream->walmethod->getlasterror());
        goto error;
      }
      lastFlushPosition = blockpos;

         
                                                                        
                      
         
      if (!sendFeedback(conn, blockpos, now, false))
      {
        goto error;
      }
      last_status = now;
    }

       
                                                       
       
    if (still_sending && stream->standby_message_timeout > 0 && feTimestampDifferenceExceeds(last_status, now, stream->standby_message_timeout))
    {
                                  
      if (!sendFeedback(conn, blockpos, now, false))
      {
        goto error;
      }
      last_status = now;
    }

       
                                                          
       
    sleeptime = CalculateCopyStreamSleeptime(now, stream->standby_message_timeout, last_status);

    r = CopyStreamReceive(conn, sleeptime, stream->stop_socket, &copybuf);
    while (r != 0)
    {
      if (r == -1)
      {
        goto error;
      }
      if (r == -2)
      {
        PGresult *res = HandleEndOfCopyStream(conn, stream, copybuf, blockpos, stoppos);

        if (res == NULL)
        {
          goto error;
        }
        else
        {
          return res;
        }
      }

                                   
      if (copybuf[0] == 'k')
      {
        if (!ProcessKeepaliveMsg(conn, stream, copybuf, r, blockpos, &last_status))
        {
          goto error;
        }
      }
      else if (copybuf[0] == 'w')
      {
        if (!ProcessXLogDataMsg(conn, stream, copybuf, r, &blockpos))
        {
          goto error;
        }

           
                                                                   
                  
           
        if (!CheckCopyStreamStop(conn, stream, blockpos, stoppos))
        {
          goto error;
        }
      }
      else
      {
        pg_log_error("unrecognized streaming header: \"%c\"", copybuf[0]);
        goto error;
      }

         
                                                                        
                           
         
      r = CopyStreamReceive(conn, 0, stream->stop_socket, &copybuf);
    }
  }

error:
  if (copybuf != NULL)
  {
    PQfreemem(copybuf);
  }
  return NULL;
}

   
                                              
                                                                      
                                                                 
   
                                                                      
                                                                      
   
static int
CopyStreamPoll(PGconn *conn, long timeout_ms, pgsocket stop_socket)
{
  int ret;
  fd_set input_mask;
  int connsocket;
  int maxfd;
  struct timeval timeout;
  struct timeval *timeoutptr;

  connsocket = PQsocket(conn);
  if (connsocket < 0)
  {
    pg_log_error("invalid socket: %s", PQerrorMessage(conn));
    return -1;
  }

  FD_ZERO(&input_mask);
  FD_SET(connsocket, &input_mask);
  maxfd = connsocket;
  if (stop_socket != PGINVALID_SOCKET)
  {
    FD_SET(stop_socket, &input_mask);
    maxfd = Max(maxfd, stop_socket);
  }

  if (timeout_ms < 0)
  {
    timeoutptr = NULL;
  }
  else
  {
    timeout.tv_sec = timeout_ms / 1000L;
    timeout.tv_usec = (timeout_ms % 1000L) * 1000L;
    timeoutptr = &timeout;
  }

  ret = select(maxfd + 1, &input_mask, NULL, NULL, timeoutptr);

  if (ret < 0)
  {
    if (errno == EINTR)
    {
      return 0;                                    
    }
    pg_log_error("select() failed: %m");
    return -1;
  }
  if (ret > 0 && FD_ISSET(connsocket, &input_mask))
  {
    return 1;                                     
  }

  return 0;                                          
}

   
                                                                     
                            
   
                                                                           
                                                                            
                                          
   
                                                                     
                                               
                                                 
   
static int
CopyStreamReceive(PGconn *conn, long timeout, pgsocket stop_socket, char **buffer)
{
  char *copybuf = NULL;
  int rawlen;

  if (*buffer != NULL)
  {
    PQfreemem(*buffer);
  }
  *buffer = NULL;

                                         
  rawlen = PQgetCopyData(conn, &copybuf, 1);
  if (rawlen == 0)
  {
    int ret;

       
                                                                        
                                                                         
                                                
       
    ret = CopyStreamPoll(conn, timeout, stop_socket);
    if (ret <= 0)
    {
      return ret;
    }

                                                  
    if (PQconsumeInput(conn) == 0)
    {
      pg_log_error("could not receive data from WAL stream: %s", PQerrorMessage(conn));
      return -1;
    }

                                                       
    rawlen = PQgetCopyData(conn, &copybuf, 1);
    if (rawlen == 0)
    {
      return 0;
    }
  }
  if (rawlen == -1)                                
  {
    return -2;
  }
  if (rawlen == -2)
  {
    pg_log_error("could not read COPY data: %s", PQerrorMessage(conn));
    return -1;
  }

                                          
  *buffer = copybuf;
  return rawlen;
}

   
                                  
   
static bool
ProcessKeepaliveMsg(PGconn *conn, StreamCtl *stream, char *copybuf, int len, XLogRecPtr blockpos, TimestampTz *last_status)
{
  int pos;
  bool replyRequested;
  TimestampTz now;

     
                                                                            
                                                                 
     
  pos = 1;                        
  pos += 8;                  
  pos += 8;                    

  if (len < pos + 1)
  {
    pg_log_error("streaming header too small: %d", len);
    return false;
  }
  replyRequested = copybuf[pos];

                                                             
  if (replyRequested && still_sending)
  {
    if (reportFlushPosition && lastFlushPosition < blockpos && walfile != NULL)
    {
         
                                                                   
                                                                         
                                                                      
                                                                     
                                 
         
      if (stream->walmethod->sync(walfile) != 0)
      {
        pg_log_error("could not fsync file \"%s\": %s", current_walfile_name, stream->walmethod->getlasterror());
        return false;
      }
      lastFlushPosition = blockpos;
    }

    now = feGetCurrentTimestamp();
    if (!sendFeedback(conn, blockpos, now, false))
    {
      return false;
    }
    *last_status = now;
  }

  return true;
}

   
                             
   
static bool
ProcessXLogDataMsg(PGconn *conn, StreamCtl *stream, char *copybuf, int len, XLogRecPtr *blockpos)
{
  int xlogoff;
  int bytes_left;
  int bytes_written;
  int hdr_len;

     
                                                                           
                                   
     
  if (!(still_sending))
  {
    return true;
  }

     
                                                                       
                                                                           
                            
     
  hdr_len = 1;                   
  hdr_len += 8;                
  hdr_len += 8;             
  hdr_len += 8;               
  if (len < hdr_len)
  {
    pg_log_error("streaming header too small: %d", len);
    return false;
  }
  *blockpos = fe_recvint64(&copybuf[1]);

                                           
  xlogoff = XLogSegmentOffset(*blockpos, WalSegSz);

     
                                                                           
             
     
  if (walfile == NULL)
  {
                          
    if (xlogoff != 0)
    {
      pg_log_error("received write-ahead log record for offset %u with no file open", xlogoff);
      return false;
    }
  }
  else
  {
                                       
    if (stream->walmethod->get_current_pos(walfile) != xlogoff)
    {
      pg_log_error("got WAL data offset %08x, expected %08x", xlogoff, (int)stream->walmethod->get_current_pos(walfile));
      return false;
    }
  }

  bytes_left = len - hdr_len;
  bytes_written = 0;

  while (bytes_left)
  {
    int bytes_to_write;

       
                                                                    
                     
       
    if (xlogoff + bytes_left > WalSegSz)
    {
      bytes_to_write = WalSegSz - xlogoff;
    }
    else
    {
      bytes_to_write = bytes_left;
    }

    if (walfile == NULL)
    {
      if (!open_walfile(stream, *blockpos))
      {
                                          
        return false;
      }
    }

    if (stream->walmethod->write(walfile, copybuf + hdr_len + bytes_written, bytes_to_write) != bytes_to_write)
    {
      pg_log_error("could not write %u bytes to WAL file \"%s\": %s", bytes_to_write, current_walfile_name, stream->walmethod->getlasterror());
      return false;
    }

                                                    
    bytes_written += bytes_to_write;
    bytes_left -= bytes_to_write;
    *blockpos += bytes_to_write;
    xlogoff += bytes_to_write;

                                                
    if (XLogSegmentOffset(*blockpos, WalSegSz) == 0)
    {
      if (!close_walfile(stream, *blockpos))
      {
                                                      
        return false;
      }

      xlogoff = 0;

      if (still_sending && stream->stream_stop(*blockpos, stream->timeline, true))
      {
        if (PQputCopyEnd(conn, NULL) <= 0 || PQflush(conn))
        {
          pg_log_error("could not send copy-end packet: %s", PQerrorMessage(conn));
          return false;
        }
        still_sending = false;
        return true;                                              
      }
    }
  }
                                                            

  return true;
}

   
                                  
   
static PGresult *
HandleEndOfCopyStream(PGconn *conn, StreamCtl *stream, char *copybuf, XLogRecPtr blockpos, XLogRecPtr *stoppos)
{
  PGresult *res = PQgetResult(conn);

     
                                                                         
                                                                           
                             
     
  if (still_sending)
  {
    if (!close_walfile(stream, blockpos))
    {
                                                    
      PQclear(res);
      return NULL;
    }
    if (PQresultStatus(res) == PGRES_COPY_IN)
    {
      if (PQputCopyEnd(conn, NULL) <= 0 || PQflush(conn))
      {
        pg_log_error("could not send copy-end packet: %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
      }
      res = PQgetResult(conn);
    }
    still_sending = false;
  }
  if (copybuf != NULL)
  {
    PQfreemem(copybuf);
  }
  *stoppos = blockpos;
  return res;
}

   
                                                                  
   
static bool
CheckCopyStreamStop(PGconn *conn, StreamCtl *stream, XLogRecPtr blockpos, XLogRecPtr *stoppos)
{
  if (still_sending && stream->stream_stop(blockpos, stream->timeline, false))
  {
    if (!close_walfile(stream, blockpos))
    {
                                                               
      return false;
    }
    if (PQputCopyEnd(conn, NULL) <= 0 || PQflush(conn))
    {
      pg_log_error("could not send copy-end packet: %s", PQerrorMessage(conn));
      return false;
    }
    still_sending = false;
  }

  return true;
}

   
                                                      
   
static long
CalculateCopyStreamSleeptime(TimestampTz now, int standby_message_timeout, TimestampTz last_status)
{
  TimestampTz status_targettime = 0;
  long sleeptime;

  if (standby_message_timeout && still_sending)
  {
    status_targettime = last_status + (standby_message_timeout - 1) * ((int64)1000);
  }

  if (status_targettime > 0)
  {
    long secs;
    int usecs;

    feTimestampDifference(now, status_targettime, &secs, &usecs);
                                     
    if (secs <= 0)
    {
      secs = 1;
      usecs = 0;
    }

    sleeptime = secs * 1000 + usecs / 1000;
  }
  else
  {
    sleeptime = -1;
  }

  return sleeptime;
}
