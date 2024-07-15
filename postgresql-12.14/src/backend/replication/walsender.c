                                                                            
   
               
   
                                                                          
                                                                       
                                                                          
                                                                            
                                                                           
   
                                                                          
                                                                          
                                                                    
                                                                             
                                                                        
                                                                       
                                                                         
                                             
   
                                                                      
                                                                             
                                                                          
                                                                          
                                                                    
                                                                          
   
                                                     
                                                                            
                                                                       
                                                                            
                                                                              
                                                                               
                                                                          
                                                                               
                                                                        
                                                                            
                                                                       
   
   
                                                                         
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/printtup.h"
#include "access/timeline.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/xlogutils.h"

#include "catalog/pg_authid.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "funcapi.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/replnodes.h"
#include "pgstat.h"
#include "replication/basebackup.h"
#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/logicalfuncs.h"
#include "replication/slot.h"
#include "replication/snapbuild.h"
#include "replication/syncrep.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/condition_variable.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "tcop/dest.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/pg_lsn.h"
#include "utils/portal.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"

   
                                                                        
   
                                                                         
                                                                            
                                                                         
                                                                   
                                                             
   
#define MAX_SEND_SIZE (XLOG_BLCKSZ * 16)

                                       
WalSndCtlData *WalSndCtl = NULL;

                                        
WalSnd *MyWalSnd = NULL;

                  
bool am_walsender = false;                                          
bool am_cascading_walsender = false;                                  
                                                   
bool am_db_walsender = false;                                      

                                            
int max_wal_senders = 0;                                                
                                                    
int wal_sender_timeout = 60 * 1000;                                 
                                                      
bool log_replication_commands = false;

   
                                 
   
bool wake_wal_senders = false;

   
                                                                
                                       
   
static int sendFile = -1;
static XLogSegNo sendSegNo = 0;
static uint32 sendOff = 0;

                                            
static TimeLineID curFileTimeLine = 0;

   
                                                                           
                                                                             
                                                                            
                                                                   
   
static TimeLineID sendTimeLine = 0;
static TimeLineID sendTimeLineNextTLI = 0;
static bool sendTimeLineIsHistoric = false;
static XLogRecPtr sendTimeLineValidUpto = InvalidXLogRecPtr;

   
                                                                
                                                                          
   
static XLogRecPtr sentPtr = InvalidXLogRecPtr;

                                                                               
static StringInfoData output_message;
static StringInfoData reply_message;
static StringInfoData tmpbuf;

                                              
static TimestampTz last_processing = 0;

   
                                                                     
                                                                      
   
static TimestampTz last_reply_timestamp = 0;

                                                                          
static bool waiting_for_ping_response = false;

   
                                                                         
                                                                              
                                                                              
                                                                           
   
static bool streamingDoneSending;
static bool streamingDoneReceiving;

                       
static bool WalSndCaughtUp = false;

                                                                 
static volatile sig_atomic_t got_SIGUSR2 = false;
static volatile sig_atomic_t got_STOPPING = false;

   
                                                    
                                                                               
                                                                               
                                                 
   
static volatile sig_atomic_t replication_active = false;

static LogicalDecodingContext *logical_decoding_ctx = NULL;
static XLogRecPtr logical_startptr = InvalidXLogRecPtr;

                                                                       
typedef struct
{
  XLogRecPtr lsn;
  TimestampTz time;
} WalTimeSample;

                                             
#define LAG_TRACKER_BUFFER_SIZE 8192

                                               
typedef struct
{
  XLogRecPtr last_lsn;
  WalTimeSample buffer[LAG_TRACKER_BUFFER_SIZE];
  int write_head;
  int read_heads[NUM_SYNC_REP_WAIT_MODE];
  WalTimeSample last_read[NUM_SYNC_REP_WAIT_MODE];
} LagTracker;

static LagTracker *lag_tracker;

                     
static void WalSndLastCycleHandler(SIGNAL_ARGS);

                                      
typedef void (*WalSndSendDataCallback)(void);
static void
WalSndLoop(WalSndSendDataCallback send_data);
static void
InitWalSenderSlot(void);
static void
WalSndKill(int code, Datum arg);
static void
WalSndShutdown(void) pg_attribute_noreturn();
static void
XLogSendPhysical(void);
static void
XLogSendLogical(void);
static void
WalSndDone(WalSndSendDataCallback send_data);
static XLogRecPtr
GetStandbyFlushRecPtr(void);
static void
IdentifySystem(void);
static void
CreateReplicationSlot(CreateReplicationSlotCmd *cmd);
static void
DropReplicationSlot(DropReplicationSlotCmd *cmd);
static void
StartReplication(StartReplicationCmd *cmd);
static void
StartLogicalReplication(StartReplicationCmd *cmd);
static void
ProcessStandbyMessage(void);
static void
ProcessStandbyReplyMessage(void);
static void
ProcessStandbyHSFeedbackMessage(void);
static void
ProcessRepliesIfAny(void);
static void
ProcessPendingWrites(void);
static void
WalSndKeepalive(bool requestReply);
static void
WalSndKeepaliveIfNecessary(void);
static void
WalSndCheckTimeOut(void);
static long
WalSndComputeSleeptime(TimestampTz now);
static void
WalSndPrepareWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write);
static void
WalSndWriteData(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write);
static void
WalSndUpdateProgress(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid);
static XLogRecPtr
WalSndWaitForWal(XLogRecPtr loc);
static void
LagTrackerWrite(XLogRecPtr lsn, TimestampTz local_flush_time);
static TimeOffset
LagTrackerRead(int head, XLogRecPtr lsn, TimestampTz now);
static bool
TransactionIdInRecentPast(TransactionId xid, uint32 epoch);

static void
XLogRead(char *buf, XLogRecPtr startptr, Size count);

                                                                        
void
InitWalSender(void)
{
  am_cascading_walsender = RecoveryInProgress();

                                                              
  InitWalSenderSlot();

     
                                                                           
                                                                  
     

     
                                                                            
                                                                           
                                                                             
                                                                             
                                                                             
     
  MarkPostmasterChildWalSender();
  SendPostmasterSignal(PMSIGNAL_ADVANCE_STATE_MACHINE);

                                                           
  lag_tracker = MemoryContextAllocZero(TopMemoryContext, sizeof(LagTracker));
}

   
                            
   
                                                                         
                                                                          
                                                                         
   
void
WalSndErrorCleanup(void)
{
  LWLockReleaseAll();
  ConditionVariableCancelSleep();
  pgstat_report_wait_end();

  if (sendFile >= 0)
  {
    close(sendFile);
    sendFile = -1;
  }

  if (MyReplicationSlot != NULL)
  {
    ReplicationSlotRelease();
  }

  ReplicationSlotCleanup();

  replication_active = false;

  if (got_STOPPING || got_SIGUSR2)
  {
    proc_exit(0);
  }

                                    
  WalSndSetState(WALSNDSTATE_STARTUP);
}

   
                                                            
   
static void
WalSndShutdown(void)
{
     
                                                                            
                                   
     
  if (whereToSendOutput == DestRemote)
  {
    whereToSendOutput = DestNone;
  }

  proc_exit(0);
  abort();                              
}

   
                                       
   
static void
IdentifySystem(void)
{
  char sysid[32];
  char xloc[MAXFNAMELEN];
  XLogRecPtr logptr;
  char *dbname = NULL;
  DestReceiver *dest;
  TupOutputState *tstate;
  TupleDesc tupdesc;
  Datum values[4];
  bool nulls[4];

     
                                                                             
                                                                       
                                                                   
     

  snprintf(sysid, sizeof(sysid), UINT64_FORMAT, GetSystemIdentifier());

  am_cascading_walsender = RecoveryInProgress();
  if (am_cascading_walsender)
  {
                                          
    logptr = GetStandbyFlushRecPtr();
  }
  else
  {
    logptr = GetFlushRecPtr();
  }

  snprintf(xloc, sizeof(xloc), "%X/%X", (uint32)(logptr >> 32), (uint32)logptr);

  if (MyDatabaseId != InvalidOid)
  {
    MemoryContext cur = CurrentMemoryContext;

                                                  
    StartTransactionCommand();
                                             
    MemoryContextSwitchTo(cur);
    dbname = get_database_name(MyDatabaseId);
    CommitTransactionCommand();
                                                               
    MemoryContextSwitchTo(cur);
  }

  dest = CreateDestReceiver(DestRemoteSimple);
  MemSet(nulls, false, sizeof(nulls));

                                                         
  tupdesc = CreateTemplateTupleDesc(4);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)1, "systemid", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)2, "timeline", INT4OID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)3, "xlogpos", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)4, "dbname", TEXTOID, -1, 0);

                                        
  tstate = begin_tup_output_tupdesc(dest, tupdesc, &TTSOpsVirtual);

                                   
  values[0] = CStringGetTextDatum(sysid);

                          
  values[1] = Int32GetDatum(ThisTimeLineID);

                              
  values[2] = CStringGetTextDatum(xloc);

                                                
  if (dbname)
  {
    values[3] = CStringGetTextDatum(dbname);
  }
  else
  {
    nulls[3] = true;
  }

                       
  do_tup_output(tstate, values, nulls);

  end_tup_output(tstate);
}

   
                                    
   
static void
SendTimeLineHistory(TimeLineHistoryCmd *cmd)
{
  StringInfoData buf;
  char histfname[MAXFNAMELEN];
  char path[MAXPGPATH];
  int fd;
  off_t histfilelen;
  off_t bytesleft;
  Size len;

     
                                                                             
                                                        
     

  TLHistoryFileName(histfname, cmd->timeline);
  TLHistoryFilePath(path, cmd->timeline);

                                     
  pq_beginmessage(&buf, 'T');
  pq_sendint16(&buf, 2);               

                   
  pq_sendstring(&buf, "filename");               
  pq_sendint32(&buf, 0);                          
  pq_sendint16(&buf, 0);                       
  pq_sendint32(&buf, TEXTOID);                   
  pq_sendint16(&buf, -1);                      
  pq_sendint32(&buf, 0);                       
  pq_sendint16(&buf, 0);                            

                    
  pq_sendstring(&buf, "content");               
  pq_sendint32(&buf, 0);                         
  pq_sendint16(&buf, 0);                      
     
                                                                     
                       
     
  pq_sendint32(&buf, BYTEAOID);               
  pq_sendint16(&buf, -1);                   
  pq_sendint32(&buf, 0);                    
  pq_sendint16(&buf, 0);                         
  pq_endmessage(&buf);

                              
  pq_beginmessage(&buf, 'D');
  pq_sendint16(&buf, 2);                   
  len = strlen(histfname);
  pq_sendint32(&buf, len);               
  pq_sendbytes(&buf, histfname, len);

  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

                                                   
  histfilelen = lseek(fd, 0, SEEK_END);
  if (histfilelen < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek to end of file \"%s\": %m", path)));
  }
  if (lseek(fd, 0, SEEK_SET) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek to beginning of file \"%s\": %m", path)));
  }

  pq_sendint32(&buf, histfilelen);               

  bytesleft = histfilelen;
  while (bytesleft > 0)
  {
    PGAlignedBlock rbuf;
    int nread;

    pgstat_report_wait_start(WAIT_EVENT_WALSENDER_TIMELINE_HISTORY_READ);
    nread = read(fd, rbuf.data, sizeof(rbuf));
    pgstat_report_wait_end();
    if (nread < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else if (nread == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, nread, (Size)bytesleft)));
    }

    pq_sendbytes(&buf, rbuf.data, nread);
    bytesleft -= nread;
  }

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }

  pq_endmessage(&buf);
}

   
                                     
   
                                                                              
                     
   
static void
StartReplication(StartReplicationCmd *cmd)
{
  StringInfoData buf;
  XLogRecPtr FlushPtr;

  if (ThisTimeLineID == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("IDENTIFY_SYSTEM has not been run before START_REPLICATION")));
  }

     
                                                                         
                                                              
     
                                                                         
                                                                       
                                     
     

  if (cmd->slotname)
  {
    ReplicationSlotAcquire(cmd->slotname, true);
    if (SlotIsLogical(MyReplicationSlot))
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), (errmsg("cannot use a logical replication slot for physical replication"))));
    }
  }

     
                                                                        
                                                                            
                             
     
  if (am_cascading_walsender)
  {
                                          
    FlushPtr = GetStandbyFlushRecPtr();
  }
  else
  {
    FlushPtr = GetFlushRecPtr();
  }

  if (cmd->timeline != 0)
  {
    XLogRecPtr switchpoint;

    sendTimeLine = cmd->timeline;
    if (sendTimeLine == ThisTimeLineID)
    {
      sendTimeLineIsHistoric = false;
      sendTimeLineValidUpto = InvalidXLogRecPtr;
    }
    else
    {
      List *timeLineHistory;

      sendTimeLineIsHistoric = true;

         
                                                                      
                                                       
         
      timeLineHistory = readTimeLineHistory(ThisTimeLineID);
      switchpoint = tliSwitchPoint(cmd->timeline, timeLineHistory, &sendTimeLineNextTLI);
      list_free_deep(timeLineHistory);

         
                                                                 
                                                                  
         
                                                                      
                                                                    
                                                                   
                                                                     
                                                                    
                                                                        
                                                                       
                                                                     
                                                           
         
                                                                       
                                                                      
                      
         
      if (!XLogRecPtrIsInvalid(switchpoint) && switchpoint < cmd->startpoint)
      {
        ereport(ERROR, (errmsg("requested starting point %X/%X on timeline %u is not in this server's history", (uint32)(cmd->startpoint >> 32), (uint32)(cmd->startpoint), cmd->timeline), errdetail("This server's history forked from timeline %u at %X/%X.", cmd->timeline, (uint32)(switchpoint >> 32), (uint32)(switchpoint))));
      }
      sendTimeLineValidUpto = switchpoint;
    }
  }
  else
  {
    sendTimeLine = ThisTimeLineID;
    sendTimeLineValidUpto = InvalidXLogRecPtr;
    sendTimeLineIsHistoric = false;
  }

  streamingDoneSending = streamingDoneReceiving = false;

                                                                 
  if (!sendTimeLineIsHistoric || cmd->startpoint < sendTimeLineValidUpto)
  {
       
                                                                      
                                                               
                                                                           
                                                                        
                                                                        
                                                                       
                   
       
    WalSndSetState(WALSNDSTATE_CATCHUP);

                                                              
    pq_beginmessage(&buf, 'W');
    pq_sendbyte(&buf, 0);
    pq_sendint16(&buf, 0);
    pq_endmessage(&buf);
    pq_flush();

       
                                                                       
                                                       
       
    if (FlushPtr < cmd->startpoint)
    {
      ereport(ERROR, (errmsg("requested starting point %X/%X is ahead of the WAL flush position of this server %X/%X", (uint32)(cmd->startpoint >> 32), (uint32)(cmd->startpoint), (uint32)(FlushPtr >> 32), (uint32)(FlushPtr))));
    }

                                                  
    sentPtr = cmd->startpoint;

                                              
    SpinLockAcquire(&MyWalSnd->mutex);
    MyWalSnd->sentPtr = sentPtr;
    SpinLockRelease(&MyWalSnd->mutex);

    SyncRepInitConfig();

                                
    replication_active = true;

    WalSndLoop(XLogSendPhysical);

    replication_active = false;
    if (got_STOPPING)
    {
      proc_exit(0);
    }
    WalSndSetState(WALSNDSTATE_STARTUP);

    Assert(streamingDoneSending && streamingDoneReceiving);
  }

  if (cmd->slotname)
  {
    ReplicationSlotRelease();
  }

     
                                                                            
               
     
  if (sendTimeLineIsHistoric)
  {
    char startpos_str[8 + 1 + 8 + 1];
    DestReceiver *dest;
    TupOutputState *tstate;
    TupleDesc tupdesc;
    Datum values[2];
    bool nulls[2];

    snprintf(startpos_str, sizeof(startpos_str), "%X/%X", (uint32)(sendTimeLineValidUpto >> 32), (uint32)sendTimeLineValidUpto);

    dest = CreateDestReceiver(DestRemoteSimple);
    MemSet(nulls, false, sizeof(nulls));

       
                                                                       
                                                                          
                                                           
       
    tupdesc = CreateTemplateTupleDesc(2);
    TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)1, "next_tli", INT8OID, -1, 0);
    TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)2, "next_tli_startpos", TEXTOID, -1, 0);

                                         
    tstate = begin_tup_output_tupdesc(dest, tupdesc, &TTSOpsVirtual);

    values[0] = Int64GetDatum((int64)sendTimeLineNextTLI);
    values[1] = CStringGetTextDatum(startpos_str);

                         
    do_tup_output(tstate, values, nulls);

    end_tup_output(tstate);
  }

                                    
  pq_puttextmessage('C', "START_STREAMING");
}

   
                                                                             
   
                                                                            
                                                                               
                                  
   
static int
logical_read_xlog_page(XLogReaderState *state, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *cur_page, TimeLineID *pageTLI)
{
  XLogRecPtr flushptr;
  int count;

  XLogReadDetermineTimeline(state, targetPagePtr, reqLen);
  sendTimeLineIsHistoric = (state->currTLI != ThisTimeLineID);
  sendTimeLine = state->currTLI;
  sendTimeLineValidUpto = state->currTLIValidUntil;
  sendTimeLineNextTLI = state->nextTLI;

                                              
  flushptr = WalSndWaitForWal(targetPagePtr + reqLen);

                                                       
  if (flushptr < targetPagePtr + reqLen)
  {
    return -1;
  }

  if (targetPagePtr + XLOG_BLCKSZ <= flushptr)
  {
    count = XLOG_BLCKSZ;                                    
  }
  else
  {
    count = flushptr - targetPagePtr;                                 
  }

                                                      
  XLogRead(cur_page, targetPagePtr, XLOG_BLCKSZ);

  return count;
}

   
                                                           
   
static void
parseCreateReplSlotOptions(CreateReplicationSlotCmd *cmd, bool *reserve_wal, CRSSnapshotAction *snapshot_action)
{
  ListCell *lc;
  bool snapshot_action_given = false;
  bool reserve_wal_given = false;

                     
  foreach (lc, cmd->options)
  {
    DefElem *defel = (DefElem *)lfirst(lc);

    if (strcmp(defel->defname, "export_snapshot") == 0)
    {
      if (snapshot_action_given || cmd->kind != REPLICATION_KIND_LOGICAL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      snapshot_action_given = true;
      *snapshot_action = defGetBoolean(defel) ? CRS_EXPORT_SNAPSHOT : CRS_NOEXPORT_SNAPSHOT;
    }
    else if (strcmp(defel->defname, "use_snapshot") == 0)
    {
      if (snapshot_action_given || cmd->kind != REPLICATION_KIND_LOGICAL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      snapshot_action_given = true;
      *snapshot_action = CRS_USE_SNAPSHOT;
    }
    else if (strcmp(defel->defname, "reserve_wal") == 0)
    {
      if (reserve_wal_given || cmd->kind != REPLICATION_KIND_PHYSICAL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      reserve_wal_given = true;
      *reserve_wal = true;
    }
    else
    {
      elog(ERROR, "unrecognized option: %s", defel->defname);
    }
  }
}

   
                                  
   
static void
CreateReplicationSlot(CreateReplicationSlotCmd *cmd)
{
  const char *snapshot_name = NULL;
  char xloc[MAXFNAMELEN];
  char *slot_name;
  bool reserve_wal = false;
  CRSSnapshotAction snapshot_action = CRS_EXPORT_SNAPSHOT;
  DestReceiver *dest;
  TupOutputState *tstate;
  TupleDesc tupdesc;
  Datum values[4];
  bool nulls[4];

  Assert(!MyReplicationSlot);

  parseCreateReplSlotOptions(cmd, &reserve_wal, &snapshot_action);

                                    
  sendTimeLineIsHistoric = false;
  sendTimeLine = ThisTimeLineID;

  if (cmd->kind == REPLICATION_KIND_PHYSICAL)
  {
    ReplicationSlotCreate(cmd->slotname, false, cmd->temporary ? RS_TEMPORARY : RS_PERSISTENT);
  }
  else
  {
    CheckLogicalDecodingRequirements();

       
                                                                         
                                                                    
                                                                          
                                                                          
                                          
       
    ReplicationSlotCreate(cmd->slotname, true, cmd->temporary ? RS_TEMPORARY : RS_EPHEMERAL);
  }

  if (cmd->kind == REPLICATION_KIND_LOGICAL)
  {
    LogicalDecodingContext *ctx;
    bool need_full_snapshot = false;

       
                                                                     
                                                               
       
    if (snapshot_action == CRS_EXPORT_SNAPSHOT)
    {
      if (IsTransactionBlock())
      {
        ereport(ERROR,
                                                                        
            (errmsg("%s must not be called inside a transaction", "CREATE_REPLICATION_SLOT ... EXPORT_SNAPSHOT")));
      }

      need_full_snapshot = true;
    }
    else if (snapshot_action == CRS_USE_SNAPSHOT)
    {
      if (!IsTransactionBlock())
      {
        ereport(ERROR,
                                                                        
            (errmsg("%s must be called inside a transaction", "CREATE_REPLICATION_SLOT ... USE_SNAPSHOT")));
      }

      if (XactIsoLevel != XACT_REPEATABLE_READ)
      {
        ereport(ERROR,
                                                                        
            (errmsg("%s must be called in REPEATABLE READ isolation mode transaction", "CREATE_REPLICATION_SLOT ... USE_SNAPSHOT")));
      }

      if (FirstSnapshotSet)
      {
        ereport(ERROR,
                                                                        
            (errmsg("%s must be called before any query", "CREATE_REPLICATION_SLOT ... USE_SNAPSHOT")));
      }

      if (IsSubTransaction())
      {
        ereport(ERROR,
                                                                        
            (errmsg("%s must not be called in a subtransaction", "CREATE_REPLICATION_SLOT ... USE_SNAPSHOT")));
      }

      need_full_snapshot = true;
    }

    ctx = CreateInitDecodingContext(cmd->plugin, NIL, need_full_snapshot, InvalidXLogRecPtr, logical_read_xlog_page, WalSndPrepareWrite, WalSndWriteData, WalSndUpdateProgress);

       
                                                                   
                                                                   
                                                                    
                                                                        
             
       
    last_reply_timestamp = 0;

                                                    
    DecodingContextFindStartpoint(ctx);

       
                                                                
       
                                                                      
                                 
       
    if (snapshot_action == CRS_EXPORT_SNAPSHOT)
    {
      snapshot_name = SnapBuildExportSnapshot(ctx->snapshot_builder);
    }
    else if (snapshot_action == CRS_USE_SNAPSHOT)
    {
      Snapshot snap;

      snap = SnapBuildInitialSnapshot(ctx->snapshot_builder);
      RestoreTransactionSnapshot(snap, MyProc);
    }

                                                 
    FreeDecodingContext(ctx);

    if (!cmd->temporary)
    {
      ReplicationSlotPersist();
    }
  }
  else if (cmd->kind == REPLICATION_KIND_PHYSICAL && reserve_wal)
  {
    ReplicationSlotReserveWal();

    ReplicationSlotMarkDirty();

                                                          
    if (!cmd->temporary)
    {
      ReplicationSlotSave();
    }
  }

  snprintf(xloc, sizeof(xloc), "%X/%X", (uint32)(MyReplicationSlot->data.confirmed_flush >> 32), (uint32)MyReplicationSlot->data.confirmed_flush);

  dest = CreateDestReceiver(DestRemoteSimple);
  MemSet(nulls, false, sizeof(nulls));

               
                                                        
                                  
                                                       
                                             
                                   
               
     
  tupdesc = CreateTemplateTupleDesc(4);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)1, "slot_name", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)2, "consistent_point", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)3, "snapshot_name", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)4, "output_plugin", TEXTOID, -1, 0);

                                        
  tstate = begin_tup_output_tupdesc(dest, tupdesc, &TTSOpsVirtual);

                 
  slot_name = NameStr(MyReplicationSlot->data.name);
  values[0] = CStringGetTextDatum(slot_name);

                               
  values[1] = CStringGetTextDatum(xloc);

                                      
  if (snapshot_name != NULL)
  {
    values[2] = CStringGetTextDatum(snapshot_name);
  }
  else
  {
    nulls[2] = true;
  }

                               
  if (cmd->plugin != NULL)
  {
    values[3] = CStringGetTextDatum(cmd->plugin);
  }
  else
  {
    nulls[3] = true;
  }

                       
  do_tup_output(tstate, values, nulls);
  end_tup_output(tstate);

  ReplicationSlotRelease();
}

   
                                                           
   
static void
DropReplicationSlot(DropReplicationSlotCmd *cmd)
{
  ReplicationSlotDrop(cmd->slotname, !cmd->wait);
  EndCommand("DROP_REPLICATION_SLOT", DestRemote);
}

   
                                                                            
                
   
static void
StartLogicalReplication(StartReplicationCmd *cmd)
{
  StringInfoData buf;

                                                           
  CheckLogicalDecodingRequirements();

  Assert(!MyReplicationSlot);

  ReplicationSlotAcquire(cmd->slotname, true);

     
                                                                        
                                                                        
                                                                       
     
  if (am_cascading_walsender && !RecoveryInProgress())
  {
    ereport(LOG, (errmsg("terminating walsender process after promotion")));
    got_STOPPING = true;
  }

     
                                                                           
               
     
                                                                           
                         
     
  logical_decoding_ctx = CreateDecodingContext(cmd->startpoint, cmd->options, false, logical_read_xlog_page, WalSndPrepareWrite, WalSndWriteData, WalSndUpdateProgress);

  WalSndSetState(WALSNDSTATE_CATCHUP);

                                                            
  pq_beginmessage(&buf, 'W');
  pq_sendbyte(&buf, 0);
  pq_sendint16(&buf, 0);
  pq_endmessage(&buf);
  pq_flush();

                                                       
  logical_startptr = MyReplicationSlot->data.restart_lsn;

     
                                                                           
                      
     
  sentPtr = MyReplicationSlot->data.confirmed_flush;

                                                             
  SpinLockAcquire(&MyWalSnd->mutex);
  MyWalSnd->sentPtr = MyReplicationSlot->data.restart_lsn;
  SpinLockRelease(&MyWalSnd->mutex);

  replication_active = true;

  SyncRepInitConfig();

                              
  WalSndLoop(XLogSendLogical);

  FreeDecodingContext(logical_decoding_ctx);
  ReplicationSlotRelease();

  replication_active = false;
  if (got_STOPPING)
  {
    proc_exit(0);
  }
  WalSndSetState(WALSNDSTATE_STARTUP);

                                               
  EndCommand("COPY 0", DestRemote);
}

   
                                                    
   
                                      
   
                                                                                    
                  
   
static void
WalSndPrepareWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write)
{
                                                                          
  if (!last_write)
  {
    lsn = InvalidXLogRecPtr;
  }

  resetStringInfo(ctx->out);

  pq_sendbyte(ctx->out, 'w');
  pq_sendint64(ctx->out, lsn);                
  pq_sendint64(ctx->out, lsn);             

     
                                                                             
                         
     
  pq_sendint64(ctx->out, 0);               
}

   
                                            
   
                                                                            
                                                                           
                                        
   
static void
WalSndWriteData(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write)
{
  TimestampTz now;

     
                                                                            
                                                                             
                                                         
     
  resetStringInfo(&tmpbuf);
  now = GetCurrentTimestamp();
  pq_sendint64(&tmpbuf, now);
  memcpy(&ctx->out->data[1 + sizeof(int64) + sizeof(int64)], tmpbuf.data, sizeof(int64));

                                                            
  pq_putmessage_noblock('d', ctx->out->data, ctx->out->len);

  CHECK_FOR_INTERRUPTS();

                                                 
  if (pq_flush_if_writable() != 0)
  {
    WalSndShutdown();
  }

                                                                          
  if (now < TimestampTzPlusMilliseconds(last_reply_timestamp, wal_sender_timeout / 2) && !pq_is_send_pending())
  {
    return;
  }

                                                      
  ProcessPendingWrites();
}

   
                                                                             
                                        
   
static void
ProcessPendingWrites(void)
{
  for (;;)
  {
    int wakeEvents;
    long sleeptime;

                                         
    ProcessRepliesIfAny();

                                    
    WalSndCheckTimeOut();

                                             
    WalSndKeepaliveIfNecessary();

    if (!pq_is_send_pending())
    {
      break;
    }

    sleeptime = WalSndComputeSleeptime(GetCurrentTimestamp());

    wakeEvents = WL_LATCH_SET | WL_EXIT_ON_PM_DEATH | WL_SOCKET_WRITEABLE | WL_SOCKET_READABLE | WL_TIMEOUT;

                                                      
    (void)WaitLatchOrSocket(MyLatch, wakeEvents, MyProcPort->sock, sleeptime, WAIT_EVENT_WAL_SENDER_WRITE_DATA);

                                           
    ResetLatch(MyLatch);

    CHECK_FOR_INTERRUPTS();

                                                           
    if (ConfigReloadPending)
    {
      ConfigReloadPending = false;
      ProcessConfigFile(PGC_SIGHUP);
      SyncRepInitConfig();
    }

                                                   
    if (pq_flush_if_writable() != 0)
    {
      WalSndShutdown();
    }
  }

                                                        
  SetLatch(MyLatch);
}

   
                                                      
   
                                                                         
   
static void
WalSndUpdateProgress(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid)
{
  static TimestampTz sendTime = 0;
  TimestampTz now = GetCurrentTimestamp();
  bool end_xact = ctx->end_xact;

     
                                                                             
                                                               
     
                                                                         
                                                                    
                      
     
#define WALSND_LOGICAL_LAG_TRACK_INTERVAL_MS 1000
  if (end_xact && TimestampDifferenceExceeds(sendTime, now, WALSND_LOGICAL_LAG_TRACK_INTERVAL_MS))
  {
    LagTrackerWrite(lsn, now);
    sendTime = now;
  }

     
                                                                            
                                                                           
                                                                          
                                                                           
                  
     
  if (!end_xact && now >= TimestampTzPlusMilliseconds(last_reply_timestamp, wal_sender_timeout / 2))
  {
    ProcessPendingWrites();
  }
}

   
                                                                              
   
                                                                      
                                                                      
                                                      
   
static XLogRecPtr
WalSndWaitForWal(XLogRecPtr loc)
{
  int wakeEvents;
  static XLogRecPtr RecentFlushPtr = InvalidXLogRecPtr;

     
                                                                          
                                                                          
                 
     
  if (RecentFlushPtr != InvalidXLogRecPtr && loc <= RecentFlushPtr)
  {
    return RecentFlushPtr;
  }

                                        
  if (!RecoveryInProgress())
  {
    RecentFlushPtr = GetFlushRecPtr();
  }
  else
  {
    RecentFlushPtr = GetXLogReplayRecPtr(NULL);
  }

  for (;;)
  {
    long sleeptime;

                                           
    ResetLatch(MyLatch);

    CHECK_FOR_INTERRUPTS();

                                                           
    if (ConfigReloadPending)
    {
      ConfigReloadPending = false;
      ProcessConfigFile(PGC_SIGHUP);
      SyncRepInitConfig();
    }

                                         
    ProcessRepliesIfAny();

       
                                                                      
                                                                      
                                                         
       
    if (got_STOPPING)
    {
      XLogBackgroundFlush();
    }

                                                            
    if (!RecoveryInProgress())
    {
      RecentFlushPtr = GetFlushRecPtr();
    }
    else
    {
      RecentFlushPtr = GetXLogReplayRecPtr(NULL);
    }

       
                                                           
       
                                                                  
                                                                         
             
       
    if (got_STOPPING)
    {
      break;
    }

       
                                                                    
                                                                          
                                                                          
                                                                     
                                                                           
                                                   
       
    if (MyWalSnd->flush < sentPtr && MyWalSnd->write < sentPtr && !waiting_for_ping_response)
    {
      WalSndKeepalive(false);
    }

                                  
    if (loc <= RecentFlushPtr)
    {
      break;
    }

                                                                          
    WalSndCaughtUp = true;

       
                                                      
       
    if (pq_flush_if_writable() != 0)
    {
      WalSndShutdown();
    }

       
                                                                   
                                                                    
                                                         
       
    if (streamingDoneReceiving && streamingDoneSending && !pq_is_send_pending())
    {
      break;
    }

                                    
    WalSndCheckTimeOut();

                                             
    WalSndKeepaliveIfNecessary();

       
                                                                        
                                                                  
                                                                        
                                                                           
                                         
       
    sleeptime = WalSndComputeSleeptime(GetCurrentTimestamp());

    wakeEvents = WL_LATCH_SET | WL_EXIT_ON_PM_DEATH | WL_SOCKET_READABLE | WL_TIMEOUT;

    if (pq_is_send_pending())
    {
      wakeEvents |= WL_SOCKET_WRITEABLE;
    }

    (void)WaitLatchOrSocket(MyLatch, wakeEvents, MyProcPort->sock, sleeptime, WAIT_EVENT_WAL_SENDER_WAIT_WAL);
  }

                                                        
  SetLatch(MyLatch);
  return RecentFlushPtr;
}

   
                                            
   
                                                                             
           
   
bool
exec_replication_command(const char *cmd_string)
{
  int parse_rc;
  Node *cmd_node;
  MemoryContext cmd_context;
  MemoryContext old_context;

     
                                                                            
                                                                           
     
  if (got_STOPPING)
  {
    WalSndSetState(WALSNDSTATE_STOPPING);
  }

     
                                                                           
                                                                         
                                              
     
  if (MyWalSnd->state == WALSNDSTATE_STOPPING)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot execute new commands while WAL sender is in stopping mode")));
  }

     
                                                                           
                                                                  
     
  SnapBuildClearExportedSnapshot();

  CHECK_FOR_INTERRUPTS();

     
                                               
     
  cmd_context = AllocSetContextCreate(CurrentMemoryContext, "Replication command context", ALLOCSET_DEFAULT_SIZES);
  old_context = MemoryContextSwitchTo(cmd_context);

  replication_scanner_init(cmd_string);

     
                                
     
  if (!replication_scanner_is_replication_command())
  {
                                     
    replication_scanner_finish();

    MemoryContextSwitchTo(old_context);
    MemoryContextDelete(cmd_context);

                                                              
    if (MyDatabaseId == InvalidOid)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot execute SQL commands in WAL sender for physical replication")));
    }

                                                               
    return false;
  }

     
                                                  
     
  parse_rc = replication_yyparse();
  if (parse_rc != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg_internal("replication command parser returned %d", parse_rc)));
  }
  replication_scanner_finish();

  cmd_node = replication_parse_result;

     
                                                                          
                                                         
     
  debug_query_string = cmd_string;

  pgstat_report_activity(STATE_RUNNING, cmd_string);

     
                                                                          
                                                                        
                    
     
  ereport(log_replication_commands ? LOG : DEBUG1, (errmsg("received replication command: %s", cmd_string)));

     
                                                                  
     
  if (IsAbortedTransactionBlockState())
  {
    ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION), errmsg("current transaction is aborted, "
                                                                       "commands ignored until end of transaction block")));
  }

  CHECK_FOR_INTERRUPTS();

     
                                                                       
                                                                           
     
  initStringInfo(&output_message);
  initStringInfo(&reply_message);
  initStringInfo(&tmpbuf);

  switch (cmd_node->type)
  {
  case T_IdentifySystemCmd:
    IdentifySystem();
    break;

  case T_BaseBackupCmd:
    PreventInTransactionBlock(true, "BASE_BACKUP");
    SendBaseBackup((BaseBackupCmd *)cmd_node);
    break;

  case T_CreateReplicationSlotCmd:
    CreateReplicationSlot((CreateReplicationSlotCmd *)cmd_node);
    break;

  case T_DropReplicationSlotCmd:
    DropReplicationSlot((DropReplicationSlotCmd *)cmd_node);
    break;

  case T_StartReplicationCmd:
  {
    StartReplicationCmd *cmd = (StartReplicationCmd *)cmd_node;

    PreventInTransactionBlock(true, "START_REPLICATION");

    if (cmd->kind == REPLICATION_KIND_PHYSICAL)
    {
      StartReplication(cmd);
    }
    else
    {
      StartLogicalReplication(cmd);
    }
    break;
  }

  case T_TimeLineHistoryCmd:
    PreventInTransactionBlock(true, "TIMELINE_HISTORY");
    SendTimeLineHistory((TimeLineHistoryCmd *)cmd_node);
    break;

  case T_VariableShowStmt:
  {
    DestReceiver *dest = CreateDestReceiver(DestRemoteSimple);
    VariableShowStmt *n = (VariableShowStmt *)cmd_node;

                                                         
    StartTransactionCommand();
    GetPGVariable(n->name, dest);
    CommitTransactionCommand();
  }
  break;

  default:
    elog(ERROR, "unrecognized replication command node tag: %u", cmd_node->type);
  }

            
  MemoryContextSwitchTo(old_context);
  MemoryContextDelete(cmd_context);

                                    
  EndCommand("SELECT", DestRemote);

                                                      
  pgstat_report_activity(STATE_IDLE, NULL);
  debug_query_string = NULL;

  return true;
}

   
                                                                            
                                  
   
static void
ProcessRepliesIfAny(void)
{
  unsigned char firstchar;
  int r;
  bool received = false;

  last_processing = GetCurrentTimestamp();

     
                                                                         
                                                                           
                               
     
  while (!streamingDoneReceiving)
  {
    pq_startmsgread();
    r = pq_getbyte_if_available(&firstchar);
    if (r < 0)
    {
                                   
      ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("unexpected EOF on standby connection")));
      proc_exit(0);
    }
    if (r == 0)
    {
                                              
      pq_endmsgread();
      break;
    }

                                   
    resetStringInfo(&reply_message);
    if (pq_getmessage(&reply_message, 0))
    {
      ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("unexpected EOF on standby connection")));
      proc_exit(0);
    }

                                                                           
    switch (firstchar)
    {
         
                                                                 
         
    case 'd':
      ProcessStandbyMessage();
      received = true;
      break;

         
                                                                   
                                                               
         
    case 'c':
      if (!streamingDoneSending)
      {
        pq_putmessage_noblock('c', NULL, 0);
        streamingDoneSending = true;
      }

      streamingDoneReceiving = true;
      received = true;
      break;

         
                                                                
         
    case 'X':
      proc_exit(0);

    default:
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid standby message type \"%c\"", firstchar)));
    }
  }

     
                                                                         
     
  if (received)
  {
    last_reply_timestamp = last_processing;
    waiting_for_ping_response = false;
  }
}

   
                                                          
   
static void
ProcessStandbyMessage(void)
{
  char msgtype;

     
                                             
     
  msgtype = pq_getmsgbyte(&reply_message);

  switch (msgtype)
  {
  case 'r':
    ProcessStandbyReplyMessage();
    break;

  case 'h':
    ProcessStandbyHSFeedbackMessage();
    break;

  default:
    ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("unexpected message type \"%c\"", msgtype)));
    proc_exit(0);
  }
}

   
                                                                    
   
static void
PhysicalConfirmReceivedLocation(XLogRecPtr lsn)
{
  bool changed = false;
  ReplicationSlot *slot = MyReplicationSlot;

  Assert(lsn != InvalidXLogRecPtr);
  SpinLockAcquire(&slot->mutex);
  if (slot->data.restart_lsn != lsn)
  {
    changed = true;
    slot->data.restart_lsn = lsn;
  }
  SpinLockRelease(&slot->mutex);

  if (changed)
  {
    ReplicationSlotMarkDirty();
    ReplicationSlotsComputeRequiredLSN();
  }

     
                                                                           
                                                                          
                                                                             
                                     
     
}

   
                                                                           
   
static void
ProcessStandbyReplyMessage(void)
{
  XLogRecPtr writePtr, flushPtr, applyPtr;
  bool replyRequested;
  TimeOffset writeLag, flushLag, applyLag;
  bool clearLagTimes;
  TimestampTz now;
  TimestampTz replyTime;

  static bool fullyAppliedLastTime = false;

                                                    
  writePtr = pq_getmsgint64(&reply_message);
  flushPtr = pq_getmsgint64(&reply_message);
  applyPtr = pq_getmsgint64(&reply_message);
  replyTime = pq_getmsgint64(&reply_message);
  replyRequested = pq_getmsgbyte(&reply_message);

  if (log_min_messages <= DEBUG2)
  {
    char *replyTimeStr;

                                                                 
    replyTimeStr = pstrdup(timestamptz_to_str(replyTime));

    elog(DEBUG2, "write %X/%X flush %X/%X apply %X/%X%s reply_time %s", (uint32)(writePtr >> 32), (uint32)writePtr, (uint32)(flushPtr >> 32), (uint32)flushPtr, (uint32)(applyPtr >> 32), (uint32)applyPtr, replyRequested ? " (reply requested)" : "", replyTimeStr);

    pfree(replyTimeStr);
  }

                                                                     
  now = GetCurrentTimestamp();
  writeLag = LagTrackerRead(SYNC_REP_WAIT_WRITE, writePtr, now);
  flushLag = LagTrackerRead(SYNC_REP_WAIT_FLUSH, flushPtr, now);
  applyLag = LagTrackerRead(SYNC_REP_WAIT_APPLY, applyPtr, now);

     
                                                                      
                                                                          
                                                                           
                                                                   
                                                                            
                                     
     
  clearLagTimes = false;
  if (applyPtr == sentPtr)
  {
    if (fullyAppliedLastTime)
    {
      clearLagTimes = true;
    }
    fullyAppliedLastTime = true;
  }
  else
  {
    fullyAppliedLastTime = false;
  }

                                                  
  if (replyRequested)
  {
    WalSndKeepalive(false);
  }

     
                                                                             
              
     
  {
    WalSnd *walsnd = MyWalSnd;

    SpinLockAcquire(&walsnd->mutex);
    walsnd->write = writePtr;
    walsnd->flush = flushPtr;
    walsnd->apply = applyPtr;
    if (writeLag != -1 || clearLagTimes)
    {
      walsnd->writeLag = writeLag;
    }
    if (flushLag != -1 || clearLagTimes)
    {
      walsnd->flushLag = flushLag;
    }
    if (applyLag != -1 || clearLagTimes)
    {
      walsnd->applyLag = applyLag;
    }
    walsnd->replyTime = replyTime;
    SpinLockRelease(&walsnd->mutex);
  }

  if (!am_cascading_walsender)
  {
    SyncRepReleaseWaiters();
  }

     
                                                                       
     
  if (MyReplicationSlot && flushPtr != InvalidXLogRecPtr)
  {
    if (SlotIsLogical(MyReplicationSlot))
    {
      LogicalConfirmReceivedLocation(flushPtr);
    }
    else
    {
      PhysicalConfirmReceivedLocation(flushPtr);
    }
  }
}

                                                         
static void
PhysicalReplicationSlotNewXmin(TransactionId feedbackXmin, TransactionId feedbackCatalogXmin)
{
  bool changed = false;
  ReplicationSlot *slot = MyReplicationSlot;

  SpinLockAcquire(&slot->mutex);
  MyPgXact->xmin = InvalidTransactionId;

     
                                                                           
                                                                        
                                                          
     
  if (!TransactionIdIsNormal(slot->data.xmin) || !TransactionIdIsNormal(feedbackXmin) || TransactionIdPrecedes(slot->data.xmin, feedbackXmin))
  {
    changed = true;
    slot->data.xmin = feedbackXmin;
    slot->effective_xmin = feedbackXmin;
  }
  if (!TransactionIdIsNormal(slot->data.catalog_xmin) || !TransactionIdIsNormal(feedbackCatalogXmin) || TransactionIdPrecedes(slot->data.catalog_xmin, feedbackCatalogXmin))
  {
    changed = true;
    slot->data.catalog_xmin = feedbackCatalogXmin;
    slot->effective_catalog_xmin = feedbackCatalogXmin;
  }
  SpinLockRelease(&slot->mutex);

  if (changed)
  {
    ReplicationSlotMarkDirty();
    ReplicationSlotsComputeRequiredXmin(false);
  }
}

   
                                                                           
                                                        
   
                                                                     
                                           
   
                                                                    
           
   
static bool
TransactionIdInRecentPast(TransactionId xid, uint32 epoch)
{
  FullTransactionId nextFullXid;
  TransactionId nextXid;
  uint32 nextEpoch;

  nextFullXid = ReadNextFullTransactionId();
  nextXid = XidFromFullTransactionId(nextFullXid);
  nextEpoch = EpochFromFullTransactionId(nextFullXid);

  if (xid <= nextXid)
  {
    if (epoch != nextEpoch)
    {
      return false;
    }
  }
  else
  {
    if (epoch + 1 != nextEpoch)
    {
      return false;
    }
  }

  if (!TransactionIdPrecedesOrEquals(xid, nextXid))
  {
    return false;                                        
  }

  return true;
}

   
                        
   
static void
ProcessStandbyHSFeedbackMessage(void)
{
  TransactionId feedbackXmin;
  uint32 feedbackEpoch;
  TransactionId feedbackCatalogXmin;
  uint32 feedbackCatalogEpoch;
  TimestampTz replyTime;

     
                                                                         
                                                                            
                      
     
  replyTime = pq_getmsgint64(&reply_message);
  feedbackXmin = pq_getmsgint(&reply_message, 4);
  feedbackEpoch = pq_getmsgint(&reply_message, 4);
  feedbackCatalogXmin = pq_getmsgint(&reply_message, 4);
  feedbackCatalogEpoch = pq_getmsgint(&reply_message, 4);

  if (log_min_messages <= DEBUG2)
  {
    char *replyTimeStr;

                                                                 
    replyTimeStr = pstrdup(timestamptz_to_str(replyTime));

    elog(DEBUG2, "hot standby feedback xmin %u epoch %u, catalog_xmin %u epoch %u reply_time %s", feedbackXmin, feedbackEpoch, feedbackCatalogXmin, feedbackCatalogEpoch, replyTimeStr);

    pfree(replyTimeStr);
  }

     
                                                                             
              
     
  {
    WalSnd *walsnd = MyWalSnd;

    SpinLockAcquire(&walsnd->mutex);
    walsnd->replyTime = replyTime;
    SpinLockRelease(&walsnd->mutex);
  }

     
                                                                         
                                                                       
     
  if (!TransactionIdIsNormal(feedbackXmin) && !TransactionIdIsNormal(feedbackCatalogXmin))
  {
    MyPgXact->xmin = InvalidTransactionId;
    if (MyReplicationSlot != NULL)
    {
      PhysicalReplicationSlotNewXmin(feedbackXmin, feedbackCatalogXmin);
    }
    return;
  }

     
                                                                             
                                                                          
     
  if (TransactionIdIsNormal(feedbackXmin) && !TransactionIdInRecentPast(feedbackXmin, feedbackEpoch))
  {
    return;
  }

  if (TransactionIdIsNormal(feedbackCatalogXmin) && !TransactionIdInRecentPast(feedbackCatalogXmin, feedbackCatalogEpoch))
  {
    return;
  }

     
                                                                             
                                                                           
                                                                         
                                              
     
                                                                         
                                                                        
                                                                          
                                                                            
                                                                             
                                                                         
                                                                           
                                                                          
                                                           
     
                                                                            
                                                                         
                                                                          
                                                                       
                                                                             
     
                                                                     
                                                                       
                                                                             
                                         
     
                                                                           
                                                                
     
  if (MyReplicationSlot != NULL)                                     
  {
    PhysicalReplicationSlotNewXmin(feedbackXmin, feedbackCatalogXmin);
  }
  else
  {
    if (TransactionIdIsNormal(feedbackCatalogXmin) && TransactionIdPrecedes(feedbackCatalogXmin, feedbackXmin))
    {
      MyPgXact->xmin = feedbackCatalogXmin;
    }
    else
    {
      MyPgXact->xmin = feedbackXmin;
    }
  }
}

   
                                                     
   
                                                                       
                                                                         
            
   
static long
WalSndComputeSleeptime(TimestampTz now)
{
  long sleeptime = 10000;           

  if (wal_sender_timeout > 0 && last_reply_timestamp > 0)
  {
    TimestampTz wakeup_time;

       
                                                                    
                
       
    wakeup_time = TimestampTzPlusMilliseconds(last_reply_timestamp, wal_sender_timeout);

       
                                                                     
                                                                           
                                              
       
    if (!waiting_for_ping_response)
    {
      wakeup_time = TimestampTzPlusMilliseconds(last_reply_timestamp, wal_sender_timeout / 2);
    }

                                             
    sleeptime = TimestampDifferenceMilliseconds(now, wakeup_time);
  }

  return sleeptime;
}

   
                                                                
                                                                         
                                                                          
                                                                           
                                                                               
                                                                             
                                                                               
                                                                           
                                                                     
                                             
   
static void
WalSndCheckTimeOut(void)
{
  TimestampTz timeout;

                                                                             
  if (last_reply_timestamp <= 0)
  {
    return;
  }

  timeout = TimestampTzPlusMilliseconds(last_reply_timestamp, wal_sender_timeout);

  if (wal_sender_timeout > 0 && last_processing >= timeout)
  {
       
                                                               
                                                                     
                
       
    ereport(COMMERROR, (errmsg("terminating walsender process due to replication timeout")));

    WalSndShutdown();
  }
}

                                                                             
static void
WalSndLoop(WalSndSendDataCallback send_data)
{
     
                                                                          
                  
     
  last_reply_timestamp = GetCurrentTimestamp();
  waiting_for_ping_response = false;

     
                                                                            
                     
     
  for (;;)
  {
                                           
    ResetLatch(MyLatch);

    CHECK_FOR_INTERRUPTS();

                                                           
    if (ConfigReloadPending)
    {
      ConfigReloadPending = false;
      ProcessConfigFile(PGC_SIGHUP);
      SyncRepInitConfig();
    }

                                         
    ProcessRepliesIfAny();

       
                                                                   
                                                                    
                  
       
    if (streamingDoneReceiving && streamingDoneSending && !pq_is_send_pending())
    {
      break;
    }

       
                                                                           
                                                                       
                                                                          
                  
       
    if (!pq_is_send_pending())
    {
      send_data();
    }
    else
    {
      WalSndCaughtUp = false;
    }

                                                   
    if (pq_flush_if_writable() != 0)
    {
      WalSndShutdown();
    }

                                                     
    if (WalSndCaughtUp && !pq_is_send_pending())
    {
         
                                                                   
                                                                      
                                                                     
                                                                   
                                                                   
                                                                 
         
      if (MyWalSnd->state == WALSNDSTATE_CATCHUP)
      {
        ereport(DEBUG1, (errmsg("\"%s\" has now caught up with upstream server", application_name)));
        WalSndSetState(WALSNDSTATE_STREAMING);
      }

         
                                                                      
                                                                        
                                                                       
                                                                       
                            
         
      if (got_SIGUSR2)
      {
        WalSndDone(send_data);
      }
    }

                                        
    WalSndCheckTimeOut();

                                             
    WalSndKeepaliveIfNecessary();

       
                                                                    
                                                                   
                                                                     
                                                                          
                                                                         
                     
       
    if ((WalSndCaughtUp && !streamingDoneSending) || pq_is_send_pending())
    {
      long sleeptime;
      int wakeEvents;

      wakeEvents = WL_LATCH_SET | WL_EXIT_ON_PM_DEATH | WL_TIMEOUT;

      if (!streamingDoneReceiving)
      {
        wakeEvents |= WL_SOCKET_READABLE;
      }

         
                                                                       
                                                                    
         
      sleeptime = WalSndComputeSleeptime(GetCurrentTimestamp());

      if (pq_is_send_pending())
      {
        wakeEvents |= WL_SOCKET_WRITEABLE;
      }

                                                        
      (void)WaitLatchOrSocket(MyLatch, wakeEvents, MyProcPort->sock, sleeptime, WAIT_EVENT_WAL_SENDER_MAIN);
    }
  }
  return;
}

                                                                          
static void
InitWalSenderSlot(void)
{
  int i;

     
                                                                      
                                                  
     
  Assert(WalSndCtl != NULL);
  Assert(MyWalSnd == NULL);

     
                                                                          
                                                            
     
  for (i = 0; i < max_wal_senders; i++)
  {
    WalSnd *walsnd = &WalSndCtl->walsnds[i];

    SpinLockAcquire(&walsnd->mutex);

    if (walsnd->pid != 0)
    {
      SpinLockRelease(&walsnd->mutex);
      continue;
    }
    else
    {
         
                                               
         
      walsnd->pid = MyProcPid;
      walsnd->state = WALSNDSTATE_STARTUP;
      walsnd->sentPtr = InvalidXLogRecPtr;
      walsnd->needreload = false;
      walsnd->write = InvalidXLogRecPtr;
      walsnd->flush = InvalidXLogRecPtr;
      walsnd->apply = InvalidXLogRecPtr;
      walsnd->writeLag = -1;
      walsnd->flushLag = -1;
      walsnd->applyLag = -1;
      walsnd->sync_standby_priority = 0;
      walsnd->latch = &MyProc->procLatch;
      walsnd->replyTime = 0;
      SpinLockRelease(&walsnd->mutex);
                                       
      MyWalSnd = (WalSnd *)walsnd;

      break;
    }
  }

  Assert(MyWalSnd != NULL);

                                             
  on_shmem_exit(WalSndKill, 0);
}

                                                                         
static void
WalSndKill(int code, Datum arg)
{
  WalSnd *walsnd = MyWalSnd;

  Assert(walsnd != NULL);

  MyWalSnd = NULL;

  SpinLockAcquire(&walsnd->mutex);
                                                                        
  walsnd->latch = NULL;
                                                     
  walsnd->pid = 0;
  SpinLockRelease(&walsnd->mutex);
}

   
                                                                           
   
                                                                       
                              
   
                                                                       
                                                                        
                                                                        
                  
   
static void
XLogRead(char *buf, XLogRecPtr startptr, Size count)
{
  char *p;
  XLogRecPtr recptr;
  Size nbytes;
  XLogSegNo segno;

retry:
  p = buf;
  recptr = startptr;
  nbytes = count;

  while (nbytes > 0)
  {
    uint32 startoff;
    int segbytes;
    int readbytes;

    startoff = XLogSegmentOffset(recptr, wal_segment_size);

    if (sendFile < 0 || !XLByteInSeg(recptr, sendSegNo, wal_segment_size))
    {
      char path[MAXPGPATH];

                                             
      if (sendFile >= 0)
      {
        close(sendFile);
      }

      XLByteToSeg(recptr, sendSegNo, wal_segment_size);

                
                                                                        
                                                                         
                              
         
                                                                        
                                                                       
                                                                      
         
             
                                  
                                  
                                  
                                  
             
         
                                                                
                                                                
                                                                       
                                                                  
                                                                         
                                                                       
                                                                        
                                                                    
                
         
      curFileTimeLine = sendTimeLine;
      if (sendTimeLineIsHistoric)
      {
        XLogSegNo endSegNo;

        XLByteToSeg(sendTimeLineValidUpto, endSegNo, wal_segment_size);
        if (sendSegNo == endSegNo)
        {
          curFileTimeLine = sendTimeLineNextTLI;
        }
      }

      XLogFilePath(path, curFileTimeLine, sendSegNo, wal_segment_size);

      sendFile = BasicOpenFile(path, O_RDONLY | PG_BINARY);
      if (sendFile < 0)
      {
           
                                                                     
                                                                 
                                
           
        if (errno == ENOENT)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("requested WAL segment %s has already been removed", XLogFileNameP(curFileTimeLine, sendSegNo))));
        }
        else
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
        }
      }
      sendOff = 0;
    }

                                   
    if (sendOff != startoff)
    {
      if (lseek(sendFile, (off_t)startoff, SEEK_SET) < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in log segment %s to offset %u: %m", XLogFileNameP(curFileTimeLine, sendSegNo), startoff)));
      }
      sendOff = startoff;
    }

                                                 
    if (nbytes > (wal_segment_size - startoff))
    {
      segbytes = wal_segment_size - startoff;
    }
    else
    {
      segbytes = nbytes;
    }

    pgstat_report_wait_start(WAIT_EVENT_WAL_READ);
    readbytes = read(sendFile, p, segbytes);
    pgstat_report_wait_end();
    if (readbytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from log segment %s, offset %u, length %zu: %m", XLogFileNameP(curFileTimeLine, sendSegNo), sendOff, (Size)segbytes)));
    }
    else if (readbytes == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read from log segment %s, offset %u: read %d of %zu", XLogFileNameP(curFileTimeLine, sendSegNo), sendOff, readbytes, (Size)segbytes)));
    }

                               
    recptr += readbytes;

    sendOff += readbytes;
    nbytes -= readbytes;
    p += readbytes;
  }

     
                                                                             
                                                                             
                                                                       
                                                                       
                                                         
     
  XLByteToSeg(startptr, segno, wal_segment_size);
  CheckXLogRemoved(segno, ThisTimeLineID);

     
                                                                             
                                                                        
                                                                         
                                                      
     
  if (am_cascading_walsender)
  {
    WalSnd *walsnd = MyWalSnd;
    bool reload;

    SpinLockAcquire(&walsnd->mutex);
    reload = walsnd->needreload;
    walsnd->needreload = false;
    SpinLockRelease(&walsnd->mutex);

    if (reload && sendFile >= 0)
    {
      close(sendFile);
      sendFile = -1;

      goto retry;
    }
  }
}

   
                                                        
   
                                                                      
                                                                     
           
   
                                                                       
                                             
   
static void
XLogSendPhysical(void)
{
  XLogRecPtr SendRqstPtr;
  XLogRecPtr startptr;
  XLogRecPtr endptr;
  Size nbytes;

                                                                 
  if (got_STOPPING)
  {
    WalSndSetState(WALSNDSTATE_STOPPING);
  }

  if (streamingDoneSending)
  {
    WalSndCaughtUp = true;
    return;
  }

                                                      
  if (sendTimeLineIsHistoric)
  {
       
                                                                         
                                                                     
                                                                     
       
    SendRqstPtr = sendTimeLineValidUpto;
  }
  else if (am_cascading_walsender)
  {
       
                                                   
       
                                                                          
                                                                 
                                                                        
                         
       
                                                                   
                                                                           
                                                                         
                                                                        
                                                                   
                                                                         
                                                                          
                                                               
       
    bool becameHistoric = false;

    SendRqstPtr = GetStandbyFlushRecPtr();

    if (!RecoveryInProgress())
    {
         
                                                             
                                                     
         
      am_cascading_walsender = false;
      becameHistoric = true;
    }
    else
    {
         
                                                                      
                                                                       
                                                            
         
      if (sendTimeLine != ThisTimeLineID)
      {
        becameHistoric = true;
      }
    }

    if (becameHistoric)
    {
         
                                                                    
                                                                        
                                                          
         
      List *history;

      history = readTimeLineHistory(ThisTimeLineID);
      sendTimeLineValidUpto = tliSwitchPoint(sendTimeLine, history, &sendTimeLineNextTLI);

      Assert(sendTimeLine < sendTimeLineNextTLI);
      list_free_deep(history);

      sendTimeLineIsHistoric = true;

      SendRqstPtr = sendTimeLineValidUpto;
    }
  }
  else
  {
       
                                                   
       
                                                                    
                                                                           
                                                                        
                                                                        
                                                                         
                                                                  
       
    SendRqstPtr = GetFlushRecPtr();
  }

     
                                                                             
                                                                     
     
                                                                             
                                                                           
                                                                    
                                                                             
                                                                          
                                                                             
                                                                         
                                                                             
                   
     
                                                                            
                                                                         
          
     
                                                                            
                                                                          
                                                                            
                                                                       
                                                       
     
  LagTrackerWrite(SendRqstPtr, GetCurrentTimestamp());

     
                                                                         
                                                  
     
                                                                       
                                                                         
                                                                    
                                                                          
                                                                           
                                                                             
                                                                             
                                                                      
                      
     
  if (sendTimeLineIsHistoric && sendTimeLineValidUpto <= sentPtr)
  {
                                 
    if (sendFile >= 0)
    {
      close(sendFile);
    }
    sendFile = -1;

                       
    pq_putmessage_noblock('c', NULL, 0);
    streamingDoneSending = true;

    WalSndCaughtUp = true;

    elog(DEBUG1, "walsender reached end of timeline at %X/%X (sent up to %X/%X)", (uint32)(sendTimeLineValidUpto >> 32), (uint32)sendTimeLineValidUpto, (uint32)(sentPtr >> 32), (uint32)sentPtr);
    return;
  }

                                  
  Assert(sentPtr <= SendRqstPtr);
  if (SendRqstPtr <= sentPtr)
  {
    WalSndCaughtUp = true;
    return;
  }

     
                                                                         
                                                                  
                                                                      
     
                                                                             
                                                                            
                                                                          
                                                                       
                                                             
     
  startptr = sentPtr;
  endptr = startptr;
  endptr += MAX_SEND_SIZE;

                                               
  if (SendRqstPtr <= endptr)
  {
    endptr = SendRqstPtr;
    if (sendTimeLineIsHistoric)
    {
      WalSndCaughtUp = false;
    }
    else
    {
      WalSndCaughtUp = true;
    }
  }
  else
  {
                                      
    endptr -= (endptr % XLOG_BLCKSZ);
    WalSndCaughtUp = false;
  }

  nbytes = endptr - startptr;
  Assert(nbytes <= MAX_SEND_SIZE);

     
                                    
     
  resetStringInfo(&output_message);
  pq_sendbyte(&output_message, 'w');

  pq_sendint64(&output_message, startptr);                   
  pq_sendint64(&output_message, SendRqstPtr);             
  pq_sendint64(&output_message, 0);                                         

     
                                                                        
            
     
  enlargeStringInfo(&output_message, nbytes);
  XLogRead(&output_message.data[output_message.len], startptr, nbytes);
  output_message.len += nbytes;
  output_message.data[output_message.len] = '\0';

     
                                                                            
     
  resetStringInfo(&tmpbuf);
  pq_sendint64(&tmpbuf, GetCurrentTimestamp());
  memcpy(&output_message.data[1 + sizeof(int64) + sizeof(int64)], tmpbuf.data, sizeof(int64));

  pq_putmessage_noblock('d', output_message.data, output_message.len);

  sentPtr = endptr;

                                   
  {
    WalSnd *walsnd = MyWalSnd;

    SpinLockAcquire(&walsnd->mutex);
    walsnd->sentPtr = sentPtr;
    SpinLockRelease(&walsnd->mutex);
  }

                                                       
  if (update_process_title)
  {
    char activitymsg[50];

    snprintf(activitymsg, sizeof(activitymsg), "streaming %X/%X", (uint32)(sentPtr >> 32), (uint32)sentPtr);
    set_ps_display(activitymsg, false);
  }

  return;
}

   
                                      
   
static void
XLogSendLogical(void)
{
  XLogRecord *record;
  char *errm;

     
                                                                             
                                                                            
                                                                           
               
     
  static XLogRecPtr flushPtr = InvalidXLogRecPtr;

     
                                                                         
                                                                         
                                                                       
                                                  
     
  WalSndCaughtUp = false;

  record = XLogReadRecord(logical_decoding_ctx->reader, logical_startptr, &errm);
  logical_startptr = InvalidXLogRecPtr;

                               
  if (errm != NULL)
  {
    elog(ERROR, "%s", errm);
  }

  if (record != NULL)
  {
       
                                                                          
                                                                     
                                   
       
    LogicalDecodingProcessRecord(logical_decoding_ctx, logical_decoding_ctx->reader);

    sentPtr = logical_decoding_ctx->reader->EndRecPtr;
  }

     
                                                                             
                                                              
     
  if (flushPtr == InvalidXLogRecPtr)
  {
    flushPtr = GetFlushRecPtr();
  }
  else if (logical_decoding_ctx->reader->EndRecPtr >= flushPtr)
  {
    flushPtr = GetFlushRecPtr();
  }

                                                                       
  if (logical_decoding_ctx->reader->EndRecPtr >= flushPtr)
  {
    WalSndCaughtUp = true;
  }

     
                                                                           
                                                                          
                       
     
  if (WalSndCaughtUp && got_STOPPING)
  {
    got_SIGUSR2 = true;
  }

                                   
  {
    WalSnd *walsnd = MyWalSnd;

    SpinLockAcquire(&walsnd->mutex);
    walsnd->sentPtr = sentPtr;
    SpinLockRelease(&walsnd->mutex);
  }
}

   
                                        
   
                                                                             
                    
   
                                                                        
                                               
   
static void
WalSndDone(WalSndSendDataCallback send_data)
{
  XLogRecPtr replicatedPtr;

                                                       
  send_data();

     
                                                                           
                                                                             
                                                                            
     
  replicatedPtr = XLogRecPtrIsInvalid(MyWalSnd->flush) ? MyWalSnd->write : MyWalSnd->flush;

  if (WalSndCaughtUp && sentPtr == replicatedPtr && !pq_is_send_pending())
  {
                                                        
    EndCommand("COPY 0", DestRemote);
    pq_flush();

    proc_exit(0);
  }
  if (!waiting_for_ping_response)
  {
    WalSndKeepalive(true);
  }
}

   
                                                                             
                                                                            
                                              
   
                                                                      
                        
   
static XLogRecPtr
GetStandbyFlushRecPtr(void)
{
  XLogRecPtr replayPtr;
  TimeLineID replayTLI;
  XLogRecPtr receivePtr;
  TimeLineID receiveTLI;
  XLogRecPtr result;

     
                                                                           
                                                                           
                                                 
     

  receivePtr = GetWalRcvWriteRecPtr(NULL, &receiveTLI);
  replayPtr = GetXLogReplayRecPtr(&replayTLI);

  ThisTimeLineID = replayTLI;

  result = replayPtr;
  if (receiveTLI == ThisTimeLineID && receivePtr > replayPtr)
  {
    result = receivePtr;
  }

  return result;
}

   
                                                            
   
void
WalSndRqstFileReload(void)
{
  int i;

  for (i = 0; i < max_wal_senders; i++)
  {
    WalSnd *walsnd = &WalSndCtl->walsnds[i];

    SpinLockAcquire(&walsnd->mutex);
    if (walsnd->pid == 0)
    {
      SpinLockRelease(&walsnd->mutex);
      continue;
    }
    walsnd->needreload = true;
    SpinLockRelease(&walsnd->mutex);
  }
}

   
                                               
   
void
HandleWalSndInitStopping(void)
{
  Assert(am_walsender);

     
                                                                   
                                                                          
                                                                        
                                        
     
  if (!replication_active)
  {
    kill(MyProcPid, SIGTERM);
  }
  else
  {
    got_STOPPING = true;
  }
}

   
                                                                          
                                                                       
               
   
static void
WalSndLastCycleHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGUSR2 = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                            
void
WalSndSignals(void)
{
                              
  pqsignal(SIGHUP, PostgresSigHupHandler);                             
                                                      
  pqsignal(SIGINT, StatementCancelHandler);                   
  pqsignal(SIGTERM, die);                                         
  pqsignal(SIGQUIT, quickdie);                                   
  InitializeTimeouts();                                                      
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, procsignal_sigusr1_handler);
  pqsignal(SIGUSR2, WalSndLastCycleHandler);                             
                                                           

                                                                       
  pqsignal(SIGCHLD, SIG_DFL);
}

                                                          
Size
WalSndShmemSize(void)
{
  Size size = 0;

  size = offsetof(WalSndCtlData, walsnds);
  size = add_size(size, mul_size(max_wal_senders, sizeof(WalSnd)));

  return size;
}

                                                             
void
WalSndShmemInit(void)
{
  bool found;
  int i;

  WalSndCtl = (WalSndCtlData *)ShmemInitStruct("Wal Sender Ctl", WalSndShmemSize(), &found);

  if (!found)
  {
                                           
    MemSet(WalSndCtl, 0, WalSndShmemSize());

    for (i = 0; i < NUM_SYNC_REP_WAIT_MODE; i++)
    {
      SHMQueueInit(&(WalSndCtl->SyncRepQueue[i]));
    }

    for (i = 0; i < max_wal_senders; i++)
    {
      WalSnd *walsnd = &WalSndCtl->walsnds[i];

      SpinLockInit(&walsnd->mutex);
    }
  }
}

   
                          
   
                                                                             
              
   
void
WalSndWakeup(void)
{
  int i;

  for (i = 0; i < max_wal_senders; i++)
  {
    Latch *latch;
    WalSnd *walsnd = &WalSndCtl->walsnds[i];

       
                                                                        
                                                         
       
    SpinLockAcquire(&walsnd->mutex);
    latch = walsnd->latch;
    SpinLockRelease(&walsnd->mutex);

    if (latch != NULL)
    {
      SetLatch(latch);
    }
  }
}

   
                                                    
   
                                                                               
                                                  
   
void
WalSndInitStopping(void)
{
  int i;

  for (i = 0; i < max_wal_senders; i++)
  {
    WalSnd *walsnd = &WalSndCtl->walsnds[i];
    pid_t pid;

    SpinLockAcquire(&walsnd->mutex);
    pid = walsnd->pid;
    SpinLockRelease(&walsnd->mutex);

    if (pid == 0)
    {
      continue;
    }

    SendProcSignal(pid, PROCSIG_WALSND_INIT_STOPPING, InvalidBackendId);
  }
}

   
                                                                               
                                                                           
                        
   
void
WalSndWaitStopping(void)
{
  for (;;)
  {
    int i;
    bool all_stopped = true;

    for (i = 0; i < max_wal_senders; i++)
    {
      WalSnd *walsnd = &WalSndCtl->walsnds[i];

      SpinLockAcquire(&walsnd->mutex);

      if (walsnd->pid == 0)
      {
        SpinLockRelease(&walsnd->mutex);
        continue;
      }

      if (walsnd->state != WALSNDSTATE_STOPPING)
      {
        all_stopped = false;
        SpinLockRelease(&walsnd->mutex);
        break;
      }
      SpinLockRelease(&walsnd->mutex);
    }

                                                                   
    if (all_stopped)
    {
      return;
    }

    pg_usleep(10000L);                       
  }
}

                                                                
void
WalSndSetState(WalSndState state)
{
  WalSnd *walsnd = MyWalSnd;

  Assert(am_walsender);

  if (walsnd->state == state)
  {
    return;
  }

  SpinLockAcquire(&walsnd->mutex);
  walsnd->state = state;
  SpinLockRelease(&walsnd->mutex);
}

   
                                                                 
                                                    
   
static const char *
WalSndGetStateString(WalSndState state)
{
  switch (state)
  {
  case WALSNDSTATE_STARTUP:
    return "startup";
  case WALSNDSTATE_BACKUP:
    return "backup";
  case WALSNDSTATE_CATCHUP:
    return "catchup";
  case WALSNDSTATE_STREAMING:
    return "streaming";
  case WALSNDSTATE_STOPPING:
    return "stopping";
  }
  return "UNKNOWN";
}

static Interval *
offset_to_interval(TimeOffset offset)
{
  Interval *result = palloc(sizeof(Interval));

  result->month = 0;
  result->day = 0;
  result->time = offset;

  return result;
}

   
                                                                             
                    
   
Datum
pg_stat_get_wal_senders(PG_FUNCTION_ARGS)
{
#define PG_STAT_GET_WAL_SENDERS_COLS 12
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  SyncRepStandbyData *sync_standbys;
  int num_standbys;
  int i;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not "
                                                                   "allowed in this context")));
  }

                                                    
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

     
                                                                          
                                                            
     
  num_standbys = SyncRepGetCandidateStandbys(&sync_standbys);

  for (i = 0; i < max_wal_senders; i++)
  {
    WalSnd *walsnd = &WalSndCtl->walsnds[i];
    XLogRecPtr sentPtr;
    XLogRecPtr write;
    XLogRecPtr flush;
    XLogRecPtr apply;
    TimeOffset writeLag;
    TimeOffset flushLag;
    TimeOffset applyLag;
    int priority;
    int pid;
    WalSndState state;
    TimestampTz replyTime;
    bool is_sync_standby;
    Datum values[PG_STAT_GET_WAL_SENDERS_COLS];
    bool nulls[PG_STAT_GET_WAL_SENDERS_COLS];
    int j;

                                         
    SpinLockAcquire(&walsnd->mutex);
    if (walsnd->pid == 0)
    {
      SpinLockRelease(&walsnd->mutex);
      continue;
    }
    pid = walsnd->pid;
    sentPtr = walsnd->sentPtr;
    state = walsnd->state;
    write = walsnd->write;
    flush = walsnd->flush;
    apply = walsnd->apply;
    writeLag = walsnd->writeLag;
    flushLag = walsnd->flushLag;
    applyLag = walsnd->applyLag;
    priority = walsnd->sync_standby_priority;
    replyTime = walsnd->replyTime;
    SpinLockRelease(&walsnd->mutex);

       
                                                                       
                                                                      
                                
       
    is_sync_standby = false;
    for (j = 0; j < num_standbys; j++)
    {
      if (sync_standbys[j].walsnd_index == i && sync_standbys[j].pid == pid)
      {
        is_sync_standby = true;
        break;
      }
    }

    memset(nulls, 0, sizeof(nulls));
    values[0] = Int32GetDatum(pid);

    if (!is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_STATS))
    {
         
                                                                  
                                                                    
                                    
         
      MemSet(&nulls[1], true, PG_STAT_GET_WAL_SENDERS_COLS - 1);
    }
    else
    {
      values[1] = CStringGetTextDatum(WalSndGetStateString(state));

      if (XLogRecPtrIsInvalid(sentPtr))
      {
        nulls[2] = true;
      }
      values[2] = LSNGetDatum(sentPtr);

      if (XLogRecPtrIsInvalid(write))
      {
        nulls[3] = true;
      }
      values[3] = LSNGetDatum(write);

      if (XLogRecPtrIsInvalid(flush))
      {
        nulls[4] = true;
      }
      values[4] = LSNGetDatum(flush);

      if (XLogRecPtrIsInvalid(apply))
      {
        nulls[5] = true;
      }
      values[5] = LSNGetDatum(apply);

         
                                                                    
                                                               
                               
         
      priority = XLogRecPtrIsInvalid(flush) ? 0 : priority;

      if (writeLag < 0)
      {
        nulls[6] = true;
      }
      else
      {
        values[6] = IntervalPGetDatum(offset_to_interval(writeLag));
      }

      if (flushLag < 0)
      {
        nulls[7] = true;
      }
      else
      {
        values[7] = IntervalPGetDatum(offset_to_interval(flushLag));
      }

      if (applyLag < 0)
      {
        nulls[8] = true;
      }
      else
      {
        values[8] = IntervalPGetDatum(offset_to_interval(applyLag));
      }

      values[9] = Int32GetDatum(priority);

         
                                                                         
                        
         
                                                                    
                                                                  
                                                                         
                                                                      
                                                                         
                                                   
         
      if (priority == 0)
      {
        values[10] = CStringGetTextDatum("async");
      }
      else if (is_sync_standby)
      {
        values[10] = SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY ? CStringGetTextDatum("sync") : CStringGetTextDatum("quorum");
      }
      else
      {
        values[10] = CStringGetTextDatum("potential");
      }

      if (replyTime == 0)
      {
        nulls[11] = true;
      }
      else
      {
        values[11] = TimestampTzGetDatum(replyTime);
      }
    }

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

                                          
  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

   
                                        
   
                                                                        
                                                                        
                                                                  
                      
   
static void
WalSndKeepalive(bool requestReply)
{
  elog(DEBUG2, "sending replication keepalive");

                                
  resetStringInfo(&output_message);
  pq_sendbyte(&output_message, 'k');
  pq_sendint64(&output_message, sentPtr);
  pq_sendint64(&output_message, GetCurrentTimestamp());
  pq_sendbyte(&output_message, requestReply ? 1 : 0);

                                           
  pq_putmessage_noblock('d', output_message.data, output_message.len);

                      
  if (requestReply)
  {
    waiting_for_ping_response = true;
  }
}

   
                                                        
   
static void
WalSndKeepaliveIfNecessary(void)
{
  TimestampTz ping_time;

     
                                                                        
                                                      
     
  if (wal_sender_timeout <= 0 || last_reply_timestamp <= 0)
  {
    return;
  }

  if (waiting_for_ping_response)
  {
    return;
  }

     
                                                                          
                                                                           
                         
     
  ping_time = TimestampTzPlusMilliseconds(last_reply_timestamp, wal_sender_timeout / 2);
  if (last_processing >= ping_time)
  {
    WalSndKeepalive(true);

                                                   
    if (pq_flush_if_writable() != 0)
    {
      WalSndShutdown();
    }
  }
}

   
                                                                          
                                                                               
                                                                        
                               
   
static void
LagTrackerWrite(XLogRecPtr lsn, TimestampTz local_flush_time)
{
  bool buffer_full;
  int new_write_head;
  int i;

  if (!am_walsender)
  {
    return;
  }

     
                                                                            
                                                                
     
  if (lag_tracker->last_lsn == lsn)
  {
    return;
  }
  lag_tracker->last_lsn = lsn;

     
                                                                             
                                                                      
                                                                            
               
     
  new_write_head = (lag_tracker->write_head + 1) % LAG_TRACKER_BUFFER_SIZE;
  buffer_full = false;
  for (i = 0; i < NUM_SYNC_REP_WAIT_MODE; ++i)
  {
    if (new_write_head == lag_tracker->read_heads[i])
    {
      buffer_full = true;
    }
  }

     
                                                                             
                                                                        
                                                                         
     
  if (buffer_full)
  {
    new_write_head = lag_tracker->write_head;
    if (lag_tracker->write_head > 0)
    {
      lag_tracker->write_head--;
    }
    else
    {
      lag_tracker->write_head = LAG_TRACKER_BUFFER_SIZE - 1;
    }
  }

                                                          
  lag_tracker->buffer[lag_tracker->write_head].lsn = lsn;
  lag_tracker->buffer[lag_tracker->write_head].time = local_flush_time;
  lag_tracker->write_head = new_write_head;
}

   
                                                                            
                                                                              
                                                                          
                                                                       
                                                                         
                                                                           
                                                                           
                              
   
                                                                           
                         
   
static TimeOffset
LagTrackerRead(int head, XLogRecPtr lsn, TimestampTz now)
{
  TimestampTz time = 0;

                                                                
  while (lag_tracker->read_heads[head] != lag_tracker->write_head && lag_tracker->buffer[lag_tracker->read_heads[head]].lsn <= lsn)
  {
    time = lag_tracker->buffer[lag_tracker->read_heads[head]].time;
    lag_tracker->last_read[head] = lag_tracker->buffer[lag_tracker->read_heads[head]];
    lag_tracker->read_heads[head] = (lag_tracker->read_heads[head] + 1) % LAG_TRACKER_BUFFER_SIZE;
  }

     
                                                                       
                                                                           
                                                                       
                                                                            
                  
     
  if (lag_tracker->read_heads[head] == lag_tracker->write_head)
  {
    lag_tracker->last_read[head].time = 0;
  }

  if (time > now)
  {
                                                                  
    return -1;
  }
  else if (time == 0)
  {
       
                                                                    
                                                                           
                                                                        
                                                                 
                                                                   
                                                                         
                                       
       
    if (lag_tracker->read_heads[head] == lag_tracker->write_head)
    {
                                                                 
      return -1;
    }
    else if (lag_tracker->last_read[head].time != 0)
    {
                                                                     
      double fraction;
      WalTimeSample prev = lag_tracker->last_read[head];
      WalTimeSample next = lag_tracker->buffer[lag_tracker->read_heads[head]];

      if (lsn < prev.lsn)
      {
           
                                                                   
                                                                   
                  
           
        return -1;
      }

      Assert(prev.lsn < next.lsn);

      if (prev.time > next.time)
      {
                                                                      
        return -1;
      }

                                                                     
      fraction = (double)(lsn - prev.lsn) / (double)(next.lsn - prev.lsn);

                                                      
      time = (TimestampTz)((double)prev.time + (next.time - prev.time) * fraction);
    }
    else
    {
         
                                                                      
                                                                   
                                                                   
                                                                        
                                                                      
         
      time = lag_tracker->buffer[lag_tracker->read_heads[head]].time;
    }
  }

                                                                       
  Assert(time != 0);
  return now - time;
}
