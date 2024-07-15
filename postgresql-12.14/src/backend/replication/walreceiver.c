                                                                            
   
                 
   
                                                                        
                                                                       
                                                                    
   
                                                                          
                                                                            
                                                                   
                                                                     
                                                                        
                                                            
                                                                         
                                                       
   
                                                                             
                                                                           
                                                                 
                                                                             
                                                                          
                                                                               
              
   
                                                                        
                                                                           
                                                                           
                                                                         
                     
   
                                                                         
                                                                  
                                                       
   
                                                                         
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/htup_details.h"
#include "access/timeline.h"
#include "access/transam.h"
#include "access/xlog_internal.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_type.h"
#include "common/ip.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/pg_lsn.h"
#include "utils/ps_status.h"
#include "utils/resowner.h"
#include "utils/timestamp.h"

                   
int wal_receiver_status_interval;
int wal_receiver_timeout;
bool hot_standby_feedback;

                                 
static WalReceiverConn *wrconn = NULL;
WalReceiverFunctionsType *WalReceiverFunctions = NULL;

#define NAPTIME_PER_CYCLE 100                                            

   
                                                                
                                                                        
                                           
   
static int recvFile = -1;
static TimeLineID recvFileTLI = 0;
static XLogSegNo recvSegNo = 0;
static uint32 recvOff = 0;

   
                                                                           
              
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGTERM = false;

   
                                                                     
                    
   
static struct
{
  XLogRecPtr Write;                                               
  XLogRecPtr Flush;                                           
} LogstreamResult;

static StringInfoData reply_message;
static StringInfoData incoming_message;

                                      
static void
WalRcvFetchTimeLineHistoryFiles(TimeLineID first, TimeLineID last);
static void
WalRcvWaitForStartPosition(XLogRecPtr *startpoint, TimeLineID *startpointTLI);
static void
WalRcvDie(int code, Datum arg);
static void
XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len);
static void
XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr);
static void
XLogWalRcvFlush(bool dying);
static void
XLogWalRcvClose(XLogRecPtr recptr);
static void
XLogWalRcvSendReply(bool force, bool requestReply);
static void
XLogWalRcvSendHSFeedback(bool immed);
static void
ProcessWalSndrMessage(XLogRecPtr walEnd, TimestampTz sendTime);

                     
static void WalRcvSigHupHandler(SIGNAL_ARGS);
static void WalRcvSigUsr1Handler(SIGNAL_ARGS);
static void WalRcvShutdownHandler(SIGNAL_ARGS);
static void WalRcvQuickDieHandler(SIGNAL_ARGS);

   
                                                                     
                                                                      
   
                                                                             
                                                                            
                                                                               
                                                                               
                                                                           
                                                                               
                                                                          
                                     
   
void
ProcessWalRcvInterrupts(void)
{
     
                                                                            
                                                                           
                                    
     
  CHECK_FOR_INTERRUPTS();

  if (got_SIGTERM)
  {
    ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating walreceiver process due to administrator command")));
  }
}

                                              
void
WalReceiverMain(void)
{
  char conninfo[MAXCONNINFO];
  char *tmp_conninfo;
  char slotname[NAMEDATALEN];
  XLogRecPtr startpoint;
  TimeLineID startpointTLI;
  TimeLineID primaryTLI;
  bool first_stream;
  WalRcvData *walrcv = WalRcv;
  TimestampTz last_recv_timestamp;
  TimestampTz now;
  bool ping_sent;
  char *err;
  char *sender_host = NULL;
  int sender_port = 0;

     
                                                                           
                                                               
     
  Assert(walrcv != NULL);

  now = GetCurrentTimestamp();

     
                                                   
     
                                                                          
                                                                            
                                                     
     
  SpinLockAcquire(&walrcv->mutex);
  Assert(walrcv->pid == 0);
  switch (walrcv->walRcvState)
  {
  case WALRCV_STOPPING:
                                                                  
    walrcv->walRcvState = WALRCV_STOPPED;
                      

  case WALRCV_STOPPED:
    SpinLockRelease(&walrcv->mutex);
    proc_exit(1);
    break;

  case WALRCV_STARTING:
                        
    break;

  case WALRCV_WAITING:
  case WALRCV_STREAMING:
  case WALRCV_RESTARTING:
  default:
                          
    SpinLockRelease(&walrcv->mutex);
    elog(PANIC, "walreceiver still running according to shared memory state");
  }
                                                                 
  walrcv->pid = MyProcPid;
  walrcv->walRcvState = WALRCV_STREAMING;

                                                     
  walrcv->ready_to_display = false;
  strlcpy(conninfo, (char *)walrcv->conninfo, MAXCONNINFO);
  strlcpy(slotname, (char *)walrcv->slotname, NAMEDATALEN);
  startpoint = walrcv->receiveStart;
  startpointTLI = walrcv->receiveStartTLI;

                                    
  walrcv->lastMsgSendTime = walrcv->lastMsgReceiptTime = walrcv->latestWalEndTime = now;

                                                      
  walrcv->latch = &MyProc->procLatch;

  SpinLockRelease(&walrcv->mutex);

                                               
  on_shmem_exit(WalRcvDie, 0);

                                                                      
  pqsignal(SIGHUP, WalRcvSigHupHandler);                                   
  pqsignal(SIGINT, SIG_IGN);
  pqsignal(SIGTERM, WalRcvShutdownHandler);                       
  pqsignal(SIGQUIT, WalRcvQuickDieHandler);                      
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, WalRcvSigUsr1Handler);
  pqsignal(SIGUSR2, SIG_IGN);

                                                                       
  pqsignal(SIGCHLD, SIG_DFL);

                                                
  sigdelset(&BlockSig, SIGQUIT);

                                         
  load_file("libpqwalreceiver", false);
  if (WalReceiverFunctions == NULL)
  {
    elog(ERROR, "libpqwalreceiver didn't initialize correctly");
  }

                                                                         
  PG_SETMASK(&UnBlockSig);

                                                                  
  wrconn = walrcv_connect(conninfo, false, cluster_name[0] ? cluster_name : "walreceiver", &err);
  if (!wrconn)
  {
    ereport(ERROR, (errmsg("could not connect to the primary server: %s", err)));
  }

     
                                                                      
                                                                          
                                       
     
  tmp_conninfo = walrcv_get_conninfo(wrconn);
  walrcv_get_senderinfo(wrconn, &sender_host, &sender_port);
  SpinLockAcquire(&walrcv->mutex);
  memset(walrcv->conninfo, 0, MAXCONNINFO);
  if (tmp_conninfo)
  {
    strlcpy((char *)walrcv->conninfo, tmp_conninfo, MAXCONNINFO);
  }

  memset(walrcv->sender_host, 0, NI_MAXHOST);
  if (sender_host)
  {
    strlcpy((char *)walrcv->sender_host, sender_host, NI_MAXHOST);
  }

  walrcv->sender_port = sender_port;
  walrcv->ready_to_display = true;
  SpinLockRelease(&walrcv->mutex);

  if (tmp_conninfo)
  {
    pfree(tmp_conninfo);
  }

  if (sender_host)
  {
    pfree(sender_host);
  }

  first_stream = true;
  for (;;)
  {
    char *primary_sysid;
    char standby_sysid[32];
    WalRcvStreamOptions options;

       
                                                              
                                            
       
    primary_sysid = walrcv_identify_system(wrconn, &primaryTLI);

    snprintf(standby_sysid, sizeof(standby_sysid), UINT64_FORMAT, GetSystemIdentifier());
    if (strcmp(primary_sysid, standby_sysid) != 0)
    {
      ereport(ERROR, (errmsg("database system identifier differs between the primary and standby"), errdetail("The primary's identifier is %s, the standby's identifier is %s.", primary_sysid, standby_sysid)));
    }

       
                                                                       
                      
       
    if (primaryTLI < startpointTLI)
    {
      ereport(ERROR, (errmsg("highest timeline %u of the primary is behind recovery timeline %u", primaryTLI, startpointTLI)));
    }

       
                                                                         
                                                                     
                                                                          
                                                                         
                                                                        
                                                                         
                                                                        
            
       
    WalRcvFetchTimeLineHistoryFiles(startpointTLI, primaryTLI);

       
                        
       
                                                                        
                                                                         
                                                                          
                                                                      
                                                                       
                                                                       
                                                                          
                            
       
    options.logical = false;
    options.startpoint = startpoint;
    options.slotname = slotname[0] != '\0' ? slotname : NULL;
    options.proto.physical.startpointTLI = startpointTLI;
    ThisTimeLineID = startpointTLI;
    if (walrcv_startstreaming(wrconn, &options))
    {
      if (first_stream)
      {
        ereport(LOG, (errmsg("started streaming WAL from primary at %X/%X on timeline %u", (uint32)(startpoint >> 32), (uint32)startpoint, startpointTLI)));
      }
      else
      {
        ereport(LOG, (errmsg("restarted WAL streaming at %X/%X on timeline %u", (uint32)(startpoint >> 32), (uint32)startpoint, startpointTLI)));
      }
      first_stream = false;

                                                                          
      LogstreamResult.Write = LogstreamResult.Flush = GetXLogReplayRecPtr(NULL);
      initStringInfo(&reply_message);
      initStringInfo(&incoming_message);

                                              
      last_recv_timestamp = GetCurrentTimestamp();
      ping_sent = false;

                                                
      for (;;)
      {
        char *buf;
        int len;
        bool endofwal = false;
        pgsocket wait_fd = PGINVALID_SOCKET;
        int rc;

           
                                                                      
                                                    
           
        if (!RecoveryInProgress())
        {
          ereport(FATAL, (errmsg("cannot continue WAL streaming, recovery has already ended")));
        }

                                                               
        ProcessWalRcvInterrupts();

        if (got_SIGHUP)
        {
          got_SIGHUP = false;
          ProcessConfigFile(PGC_SIGHUP);
          XLogWalRcvSendHSFeedback(true);
        }

                                                 
        len = walrcv_receive(wrconn, &buf, &wait_fd);
        if (len != 0)
        {
             
                                                                   
                                        
             
          for (;;)
          {
            if (len > 0)
            {
                 
                                                              
                         
                 
              last_recv_timestamp = GetCurrentTimestamp();
              ping_sent = false;
              XLogWalRcvProcessMsg(buf[0], &buf[1], len - 1);
            }
            else if (len == 0)
            {
              break;
            }
            else if (len < 0)
            {
              ereport(LOG, (errmsg("replication terminated by primary server"), errdetail("End of WAL reached on timeline %u at %X/%X.", startpointTLI, (uint32)(LogstreamResult.Write >> 32), (uint32)LogstreamResult.Write)));
              endofwal = true;
              break;
            }
            len = walrcv_receive(wrconn, &buf, &wait_fd);
          }

                                                               
          XLogWalRcvSendReply(false, false);

             
                                                                   
                                                                   
                   
             
          XLogWalRcvFlush(false);
        }

                                                          
        if (endofwal)
        {
          break;
        }

           
                                                                   
                                                                     
                                                                  
                                                                    
                                                                  
                                                                    
                                                                    
                                                                       
                                       
           
        Assert(wait_fd != PGINVALID_SOCKET);
        rc = WaitLatchOrSocket(walrcv->latch, WL_EXIT_ON_PM_DEATH | WL_SOCKET_READABLE | WL_TIMEOUT | WL_LATCH_SET, wait_fd, NAPTIME_PER_CYCLE, WAIT_EVENT_WAL_RECEIVER_MAIN);
        if (rc & WL_LATCH_SET)
        {
          ResetLatch(walrcv->latch);
          ProcessWalRcvInterrupts();

          if (walrcv->force_reply)
          {
               
                                                               
                                                                  
                                                                   
                                                        
               
            walrcv->force_reply = false;
            pg_memory_barrier();
            XLogWalRcvSendReply(true, false);
          }
        }
        if (rc & WL_TIMEOUT)
        {
             
                                                                 
                                                    
                                                                 
                                                                
                                                                    
                                                                   
                  
             
          bool requestReply = false;

             
                                                               
                                           
             
          if (wal_receiver_timeout > 0)
          {
            TimestampTz now = GetCurrentTimestamp();
            TimestampTz timeout;

            timeout = TimestampTzPlusMilliseconds(last_recv_timestamp, wal_receiver_timeout);

            if (now >= timeout)
            {
              ereport(ERROR, (errmsg("terminating walreceiver due to timeout")));
            }

               
                                                           
                                                              
               
            if (!ping_sent)
            {
              timeout = TimestampTzPlusMilliseconds(last_recv_timestamp, (wal_receiver_timeout / 2));
              if (now >= timeout)
              {
                requestReply = true;
                ping_sent = true;
              }
            }
          }

          XLogWalRcvSendReply(requestReply, requestReply);
          XLogWalRcvSendHSFeedback(false);
        }
      }

         
                                                                       
                        
         
      walrcv_endstreaming(wrconn, &primaryTLI);

         
                                                                     
                                                                        
                   
         
      WalRcvFetchTimeLineHistoryFiles(startpointTLI, primaryTLI);
    }
    else
    {
      ereport(LOG, (errmsg("primary server contains no more WAL on requested timeline %u", startpointTLI)));
    }

       
                                                                    
                                                                   
       
    if (recvFile >= 0)
    {
      char xlogfname[MAXFNAMELEN];

      XLogWalRcvFlush(false);
      if (close(recvFile) != 0)
      {
        ereport(PANIC, (errcode_for_file_access(), errmsg("could not close log segment %s: %m", XLogFileNameP(recvFileTLI, recvSegNo))));
      }

         
                                                                         
                               
         
      XLogFileName(xlogfname, recvFileTLI, recvSegNo, wal_segment_size);
      if (XLogArchiveMode != ARCHIVE_MODE_ALWAYS)
      {
        XLogArchiveForceDone(xlogfname);
      }
      else
      {
        XLogArchiveNotify(xlogfname);
      }
    }
    recvFile = -1;

    elog(DEBUG1, "walreceiver ended streaming and awaits new instructions");
    WalRcvWaitForStartPosition(&startpoint, &startpointTLI);
  }
                   
}

   
                                                                     
   
static void
WalRcvWaitForStartPosition(XLogRecPtr *startpoint, TimeLineID *startpointTLI)
{
  WalRcvData *walrcv = WalRcv;
  int state;

  SpinLockAcquire(&walrcv->mutex);
  state = walrcv->walRcvState;
  if (state != WALRCV_STREAMING)
  {
    SpinLockRelease(&walrcv->mutex);
    if (state == WALRCV_STOPPING)
    {
      proc_exit(0);
    }
    else
    {
      elog(FATAL, "unexpected walreceiver state");
    }
  }
  walrcv->walRcvState = WALRCV_WAITING;
  walrcv->receiveStart = InvalidXLogRecPtr;
  walrcv->receiveStartTLI = 0;
  SpinLockRelease(&walrcv->mutex);

  if (update_process_title)
  {
    set_ps_display("idle", false);
  }

     
                                                                          
                                   
     
  WakeupRecovery();
  for (;;)
  {
    ResetLatch(walrcv->latch);

    ProcessWalRcvInterrupts();

    SpinLockAcquire(&walrcv->mutex);
    Assert(walrcv->walRcvState == WALRCV_RESTARTING || walrcv->walRcvState == WALRCV_WAITING || walrcv->walRcvState == WALRCV_STOPPING);
    if (walrcv->walRcvState == WALRCV_RESTARTING)
    {
                                                      
      *startpoint = walrcv->receiveStart;
      *startpointTLI = walrcv->receiveStartTLI;
      walrcv->walRcvState = WALRCV_STREAMING;
      SpinLockRelease(&walrcv->mutex);
      break;
    }
    if (walrcv->walRcvState == WALRCV_STOPPING)
    {
         
                                                                       
                                                      
         
      SpinLockRelease(&walrcv->mutex);
      exit(1);
    }
    SpinLockRelease(&walrcv->mutex);

    (void)WaitLatch(walrcv->latch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, WAIT_EVENT_WAL_RECEIVER_WAIT_START);
  }

  if (update_process_title)
  {
    char activitymsg[50];

    snprintf(activitymsg, sizeof(activitymsg), "restarting at %X/%X", (uint32)(*startpoint >> 32), (uint32)*startpoint);
    set_ps_display(activitymsg, false);
  }
}

   
                                                                       
                                
   
static void
WalRcvFetchTimeLineHistoryFiles(TimeLineID first, TimeLineID last)
{
  TimeLineID tli;

  for (tli = first; tli <= last; tli++)
  {
                                                
    if (tli != 1 && !existsTimeLineHistory(tli))
    {
      char *fname;
      char *content;
      int len;
      char expectedfname[MAXFNAMELEN];

      ereport(LOG, (errmsg("fetching timeline history file for timeline %u from primary server", tli)));

      walrcv_readtimelinehistoryfile(wrconn, tli, &fname, &content, &len);

         
                                                               
                                                                      
                       
         
      TLHistoryFileName(expectedfname, tli);
      if (strcmp(fname, expectedfname) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg_internal("primary reported unexpected file name for timeline history file of timeline %u", tli)));
      }

         
                                   
         
      writeTimeLineHistoryFile(tli, content, len);

         
                                                               
                                    
         
      if (XLogArchiveMode != ARCHIVE_MODE_ALWAYS)
      {
        XLogArchiveForceDone(fname);
      }
      else
      {
        XLogArchiveNotify(fname);
      }

      pfree(fname);
      pfree(content);
    }
  }
}

   
                                                
   
static void
WalRcvDie(int code, Datum arg)
{
  WalRcvData *walrcv = WalRcv;

                                                                
  XLogWalRcvFlush(true);

                                                
  SpinLockAcquire(&walrcv->mutex);
  Assert(walrcv->walRcvState == WALRCV_STREAMING || walrcv->walRcvState == WALRCV_RESTARTING || walrcv->walRcvState == WALRCV_STARTING || walrcv->walRcvState == WALRCV_WAITING || walrcv->walRcvState == WALRCV_STOPPING);
  Assert(walrcv->pid == MyProcPid);
  walrcv->walRcvState = WALRCV_STOPPED;
  walrcv->pid = 0;
  walrcv->ready_to_display = false;
  walrcv->latch = NULL;
  SpinLockRelease(&walrcv->mutex);

                                            
  if (wrconn != NULL)
  {
    walrcv_disconnect(wrconn);
  }

                                                                      
  WakeupRecovery();
}

                                                                     
static void
WalRcvSigHupHandler(SIGNAL_ARGS)
{
  got_SIGHUP = true;
}

                                      
static void
WalRcvSigUsr1Handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  latch_sigusr1_handler();

  errno = save_errno;
}

                                                   
static void
WalRcvShutdownHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGTERM = true;

  if (WalRcv->latch)
  {
    SetLatch(WalRcv->latch);
  }

  errno = save_errno;
}

   
                                                                            
   
                                                                             
         
   
static void
WalRcvQuickDieHandler(SIGNAL_ARGS)
{
     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                         
                                                                             
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

   
                                                        
   
static void
XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len)
{
  int hdrlen;
  XLogRecPtr dataStart;
  XLogRecPtr walEnd;
  TimestampTz sendTime;
  bool replyRequested;

  resetStringInfo(&incoming_message);

  switch (type)
  {
  case 'w':                  
  {
                                    
    hdrlen = sizeof(int64) + sizeof(int64) + sizeof(int64);
    if (len < hdrlen)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg_internal("invalid WAL message received from primary")));
    }
    appendBinaryStringInfo(&incoming_message, buf, hdrlen);

                         
    dataStart = pq_getmsgint64(&incoming_message);
    walEnd = pq_getmsgint64(&incoming_message);
    sendTime = pq_getmsgint64(&incoming_message);
    ProcessWalSndrMessage(walEnd, sendTime);

    buf += hdrlen;
    len -= hdrlen;
    XLogWalRcvWrite(buf, len, dataStart);
    break;
  }
  case 'k':                
  {
                                    
    hdrlen = sizeof(int64) + sizeof(int64) + sizeof(char);
    if (len != hdrlen)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg_internal("invalid keepalive message received from primary")));
    }
    appendBinaryStringInfo(&incoming_message, buf, hdrlen);

                         
    walEnd = pq_getmsgint64(&incoming_message);
    sendTime = pq_getmsgint64(&incoming_message);
    replyRequested = pq_getmsgbyte(&incoming_message);

    ProcessWalSndrMessage(walEnd, sendTime);

                                                                
    if (replyRequested)
    {
      XLogWalRcvSendReply(true, false);
    }
    break;
  }
  default:
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg_internal("invalid replication message type %d", type)));
  }
}

   
                            
   
static void
XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr)
{
  int startoff;
  int byteswritten;

  while (nbytes > 0)
  {
    int segbytes;

                                                     
    if (recvFile >= 0 && !XLByteInSeg(recptr, recvSegNo, wal_segment_size))
    {
      XLogWalRcvClose(recptr);
    }

    if (recvFile < 0)
    {
      bool use_existent = true;

                                   
      XLByteToSeg(recptr, recvSegNo, wal_segment_size);
      recvFile = XLogFileInit(recvSegNo, &use_existent, true);
      recvFileTLI = ThisTimeLineID;
      recvOff = 0;
    }

                                                         
    startoff = XLogSegmentOffset(recptr, wal_segment_size);

    if (startoff + nbytes > wal_segment_size)
    {
      segbytes = wal_segment_size - startoff;
    }
    else
    {
      segbytes = nbytes;
    }

                                   
    if (recvOff != startoff)
    {
      if (lseek(recvFile, (off_t)startoff, SEEK_SET) < 0)
      {
        ereport(PANIC, (errcode_for_file_access(), errmsg("could not seek in log segment %s to offset %u: %m", XLogFileNameP(recvFileTLI, recvSegNo), startoff)));
      }
      recvOff = startoff;
    }

                              
    errno = 0;

    byteswritten = write(recvFile, buf, segbytes);
    if (byteswritten <= 0)
    {
                                                           
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not write to log segment %s "
                                                        "at offset %u, length %lu: %m",
                                                     XLogFileNameP(recvFileTLI, recvSegNo), recvOff, (unsigned long)segbytes)));
    }

                                
    recptr += byteswritten;

    recvOff += byteswritten;
    nbytes -= byteswritten;
    buf += byteswritten;

    LogstreamResult.Write = recptr;
  }

     
                                                                             
                                                                           
                                                                         
                                      
     
  if (recvFile >= 0 && !XLByteInSeg(recptr, recvSegNo, wal_segment_size))
  {
    XLogWalRcvClose(recptr);
  }
}

   
                          
   
                                                                               
                                                      
   
static void
XLogWalRcvFlush(bool dying)
{
  if (LogstreamResult.Flush < LogstreamResult.Write)
  {
    WalRcvData *walrcv = WalRcv;

    issue_xlog_fsync(recvFile, recvSegNo);

    LogstreamResult.Flush = LogstreamResult.Write;

                                     
    SpinLockAcquire(&walrcv->mutex);
    if (walrcv->receivedUpto < LogstreamResult.Flush)
    {
      walrcv->latestChunkStart = walrcv->receivedUpto;
      walrcv->receivedUpto = LogstreamResult.Flush;
      walrcv->receivedTLI = ThisTimeLineID;
    }
    SpinLockRelease(&walrcv->mutex);

                                                                           
    WakeupRecovery();
    if (AllowCascadeReplication())
    {
      WalSndWakeup();
    }

                                                      
    if (update_process_title)
    {
      char activitymsg[50];

      snprintf(activitymsg, sizeof(activitymsg), "streaming %X/%X", (uint32)(LogstreamResult.Write >> 32), (uint32)LogstreamResult.Write);
      set_ps_display(activitymsg, false);
    }

                                                             
    if (!dying)
    {
      XLogWalRcvSendReply(false, false);
      XLogWalRcvSendHSFeedback(false);
    }
  }
}

   
                              
   
                                                                     
                              
   
                                                                             
   
static void
XLogWalRcvClose(XLogRecPtr recptr)
{
  char xlogfname[MAXFNAMELEN];

  Assert(recvFile >= 0 && !XLByteInSeg(recptr, recvSegNo, wal_segment_size));

     
                                                                           
                                                          
     
  XLogWalRcvFlush(false);

  XLogFileName(xlogfname, recvFileTLI, recvSegNo, wal_segment_size);

     
                                                                             
                                                                          
                                     
     
  if (close(recvFile) != 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close log segment %s: %m", xlogfname)));
  }

     
                                                                           
                     
     
  if (XLogArchiveMode != ARCHIVE_MODE_ALWAYS)
  {
    XLogArchiveForceDone(xlogfname);
  }
  else
  {
    XLogArchiveNotify(xlogfname);
  }

  recvFile = -1;
}

   
                                                                               
                              
   
                                                                      
                                                                          
                                                                         
                           
   
                                                                            
                                                                          
                         
   
static void
XLogWalRcvSendReply(bool force, bool requestReply)
{
  static XLogRecPtr writePtr = 0;
  static XLogRecPtr flushPtr = 0;
  XLogRecPtr applyPtr;
  static TimestampTz sendTime = 0;
  TimestampTz now;

     
                                                                           
                                           
     
  if (!force && wal_receiver_status_interval <= 0)
  {
    return;
  }

                              
  now = GetCurrentTimestamp();

     
                                                                         
                                                                          
                                                                          
                                                                       
                                                                         
                                                                          
                  
     
  if (!force && writePtr == LogstreamResult.Write && flushPtr == LogstreamResult.Flush && !TimestampDifferenceExceeds(sendTime, now, wal_receiver_status_interval * 1000))
  {
    return;
  }
  sendTime = now;

                               
  writePtr = LogstreamResult.Write;
  flushPtr = LogstreamResult.Flush;
  applyPtr = GetXLogReplayRecPtr(NULL);

  resetStringInfo(&reply_message);
  pq_sendbyte(&reply_message, 'r');
  pq_sendint64(&reply_message, writePtr);
  pq_sendint64(&reply_message, flushPtr);
  pq_sendint64(&reply_message, applyPtr);
  pq_sendint64(&reply_message, GetCurrentTimestamp());
  pq_sendbyte(&reply_message, requestReply ? 1 : 0);

               
  elog(DEBUG2, "sending write %X/%X flush %X/%X apply %X/%X%s", (uint32)(writePtr >> 32), (uint32)writePtr, (uint32)(flushPtr >> 32), (uint32)flushPtr, (uint32)(applyPtr >> 32), (uint32)applyPtr, requestReply ? " (reply requested)" : "");

  walrcv_send(wrconn, reply_message.data, reply_message.len);
}

   
                                                                        
                                    
   
                                                                        
                                                                       
                                                                      
                                                                      
                                                                     
   
static void
XLogWalRcvSendHSFeedback(bool immed)
{
  TimestampTz now;
  FullTransactionId nextFullXid;
  TransactionId nextXid;
  uint32 xmin_epoch, catalog_xmin_epoch;
  TransactionId xmin, catalog_xmin;
  static TimestampTz sendTime = 0;

                                                                      
  static bool master_has_standby_xmin = true;

     
                                                                           
                                           
     
  if ((wal_receiver_status_interval <= 0 || !hot_standby_feedback) && !master_has_standby_xmin)
  {
    return;
  }

                              
  now = GetCurrentTimestamp();

  if (!immed)
  {
       
                                                                    
       
    if (!TimestampDifferenceExceeds(sendTime, now, wal_receiver_status_interval * 1000))
    {
      return;
    }
    sendTime = now;
  }

     
                                                                         
                                                                         
            
     
                                                                           
                                                                         
                                                                          
                   
     
  if (!HotStandbyActive())
  {
    return;
  }

     
                                                                        
                                       
     
  if (hot_standby_feedback)
  {
    TransactionId slot_xmin;

       
                                                                          
                                                                        
                                                                     
                                  
       
    xmin = GetOldestXmin(NULL, PROCARRAY_FLAGS_DEFAULT | PROCARRAY_SLOTS_XMIN);

    ProcArrayGetReplicationSlotXmin(&slot_xmin, &catalog_xmin);

    if (TransactionIdIsValid(slot_xmin) && TransactionIdPrecedes(slot_xmin, xmin))
    {
      xmin = slot_xmin;
    }
  }
  else
  {
    xmin = InvalidTransactionId;
    catalog_xmin = InvalidTransactionId;
  }

     
                                                                           
                         
     
  nextFullXid = ReadNextFullTransactionId();
  nextXid = XidFromFullTransactionId(nextFullXid);
  xmin_epoch = EpochFromFullTransactionId(nextFullXid);
  catalog_xmin_epoch = xmin_epoch;
  if (nextXid < xmin)
  {
    xmin_epoch--;
  }
  if (nextXid < catalog_xmin)
  {
    catalog_xmin_epoch--;
  }

  elog(DEBUG2, "sending hot standby feedback xmin %u epoch %u catalog_xmin %u catalog_xmin_epoch %u", xmin, xmin_epoch, catalog_xmin, catalog_xmin_epoch);

                                          
  resetStringInfo(&reply_message);
  pq_sendbyte(&reply_message, 'h');
  pq_sendint64(&reply_message, GetCurrentTimestamp());
  pq_sendint32(&reply_message, xmin);
  pq_sendint32(&reply_message, xmin_epoch);
  pq_sendint32(&reply_message, catalog_xmin);
  pq_sendint32(&reply_message, catalog_xmin_epoch);
  walrcv_send(wrconn, reply_message.data, reply_message.len);
  if (TransactionIdIsValid(xmin) || TransactionIdIsValid(catalog_xmin))
  {
    master_has_standby_xmin = true;
  }
  else
  {
    master_has_standby_xmin = false;
  }
}

   
                                                                      
   
                                                                          
                                 
   
static void
ProcessWalSndrMessage(XLogRecPtr walEnd, TimestampTz sendTime)
{
  WalRcvData *walrcv = WalRcv;

  TimestampTz lastMsgReceiptTime = GetCurrentTimestamp();

                                   
  SpinLockAcquire(&walrcv->mutex);
  if (walrcv->latestWalEnd < walEnd)
  {
    walrcv->latestWalEndTime = sendTime;
  }
  walrcv->latestWalEnd = walEnd;
  walrcv->lastMsgSendTime = sendTime;
  walrcv->lastMsgReceiptTime = lastMsgReceiptTime;
  SpinLockRelease(&walrcv->mutex);

  if (log_min_messages <= DEBUG2)
  {
    char *sendtime;
    char *receipttime;
    int applyDelay;

                                                                 
    sendtime = pstrdup(timestamptz_to_str(sendTime));
    receipttime = pstrdup(timestamptz_to_str(lastMsgReceiptTime));
    applyDelay = GetReplicationApplyDelay();

                                      
    if (applyDelay == -1)
    {
      elog(DEBUG2, "sendtime %s receipttime %s replication apply delay (N/A) transfer latency %d ms", sendtime, receipttime, GetReplicationTransferLatency());
    }
    else
    {
      elog(DEBUG2, "sendtime %s receipttime %s replication apply delay %d ms transfer latency %d ms", sendtime, receipttime, applyDelay, GetReplicationTransferLatency());
    }

    pfree(sendtime);
    pfree(receipttime);
  }
}

   
                                      
   
                                                                           
                                                                           
                                                                         
                                      
   
void
WalRcvForceReply(void)
{
  Latch *latch;

  WalRcv->force_reply = true;
                                                                       
  SpinLockAcquire(&WalRcv->mutex);
  latch = WalRcv->latch;
  SpinLockRelease(&WalRcv->mutex);
  if (latch)
  {
    SetLatch(latch);
  }
}

   
                                                                 
                                                                  
   
static const char *
WalRcvGetStateString(WalRcvState state)
{
  switch (state)
  {
  case WALRCV_STOPPED:
    return "stopped";
  case WALRCV_STARTING:
    return "starting";
  case WALRCV_STREAMING:
    return "streaming";
  case WALRCV_WAITING:
    return "waiting";
  case WALRCV_RESTARTING:
    return "restarting";
  case WALRCV_STOPPING:
    return "stopping";
  }
  return "UNKNOWN";
}

   
                                                                             
                                                   
   
Datum
pg_stat_get_wal_receiver(PG_FUNCTION_ARGS)
{
  TupleDesc tupdesc;
  Datum *values;
  bool *nulls;
  int pid;
  bool ready_to_display;
  WalRcvState state;
  XLogRecPtr receive_start_lsn;
  TimeLineID receive_start_tli;
  XLogRecPtr received_lsn;
  TimeLineID received_tli;
  TimestampTz last_send_time;
  TimestampTz last_receipt_time;
  XLogRecPtr latest_end_lsn;
  TimestampTz latest_end_time;
  char sender_host[NI_MAXHOST];
  int sender_port = 0;
  char slotname[NAMEDATALEN];
  char conninfo[MAXCONNINFO];

                                               
  SpinLockAcquire(&WalRcv->mutex);
  pid = (int)WalRcv->pid;
  ready_to_display = WalRcv->ready_to_display;
  state = WalRcv->walRcvState;
  receive_start_lsn = WalRcv->receiveStart;
  receive_start_tli = WalRcv->receiveStartTLI;
  received_lsn = WalRcv->receivedUpto;
  received_tli = WalRcv->receivedTLI;
  last_send_time = WalRcv->lastMsgSendTime;
  last_receipt_time = WalRcv->lastMsgReceiptTime;
  latest_end_lsn = WalRcv->latestWalEnd;
  latest_end_time = WalRcv->latestWalEndTime;
  strlcpy(slotname, (char *)WalRcv->slotname, sizeof(slotname));
  strlcpy(sender_host, (char *)WalRcv->sender_host, sizeof(sender_host));
  sender_port = WalRcv->sender_port;
  strlcpy(conninfo, (char *)WalRcv->conninfo, sizeof(conninfo));
  SpinLockRelease(&WalRcv->mutex);

     
                                                                       
            
     
  if (pid == 0 || !ready_to_display)
  {
    PG_RETURN_NULL();
  }

                             
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  values = palloc0(sizeof(Datum) * tupdesc->natts);
  nulls = palloc0(sizeof(bool) * tupdesc->natts);

                    
  values[0] = Int32GetDatum(pid);

  if (!is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_STATS))
  {
       
                                                                         
                                                                      
                                 
       
    MemSet(&nulls[1], true, sizeof(bool) * (tupdesc->natts - 1));
  }
  else
  {
    values[1] = CStringGetTextDatum(WalRcvGetStateString(state));

    if (XLogRecPtrIsInvalid(receive_start_lsn))
    {
      nulls[2] = true;
    }
    else
    {
      values[2] = LSNGetDatum(receive_start_lsn);
    }
    values[3] = Int32GetDatum(receive_start_tli);
    if (XLogRecPtrIsInvalid(received_lsn))
    {
      nulls[4] = true;
    }
    else
    {
      values[4] = LSNGetDatum(received_lsn);
    }
    values[5] = Int32GetDatum(received_tli);
    if (last_send_time == 0)
    {
      nulls[6] = true;
    }
    else
    {
      values[6] = TimestampTzGetDatum(last_send_time);
    }
    if (last_receipt_time == 0)
    {
      nulls[7] = true;
    }
    else
    {
      values[7] = TimestampTzGetDatum(last_receipt_time);
    }
    if (XLogRecPtrIsInvalid(latest_end_lsn))
    {
      nulls[8] = true;
    }
    else
    {
      values[8] = LSNGetDatum(latest_end_lsn);
    }
    if (latest_end_time == 0)
    {
      nulls[9] = true;
    }
    else
    {
      values[9] = TimestampTzGetDatum(latest_end_time);
    }
    if (*slotname == '\0')
    {
      nulls[10] = true;
    }
    else
    {
      values[10] = CStringGetTextDatum(slotname);
    }
    if (*sender_host == '\0')
    {
      nulls[11] = true;
    }
    else
    {
      values[11] = CStringGetTextDatum(sender_host);
    }
    if (sender_port == 0)
    {
      nulls[12] = true;
    }
    else
    {
      values[12] = Int32GetDatum(sender_port);
    }
    if (*conninfo == '\0')
    {
      nulls[13] = true;
    }
    else
    {
      values[13] = CStringGetTextDatum(conninfo);
    }
  }

                                   
  PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}
