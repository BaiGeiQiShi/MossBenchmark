/*-------------------------------------------------------------------------
 *
 * walsender.c
 *
 * The WAL sender process (walsender) is new as of Postgres 9.0. It takes
 * care of sending XLOG from the primary server to a single recipient.
 * (Note that there can be more than one walsender process concurrently.)
 * It is started by the postmaster when the walreceiver of a standby server
 * connects to the primary server and requests XLOG streaming replication.
 *
 * A walsender is similar to a regular backend, ie. there is a one-to-one
 * relationship between a connection and a walsender process, but instead
 * of processing SQL queries, it understands a small set of special
 * replication-mode commands. The START_REPLICATION command begins streaming
 * WAL to the client. While streaming, the walsender keeps reading XLOG
 * records from the disk and sends them to the standby server over the
 * COPY protocol, until either side ends the replication by exiting COPY
 * mode (or until the connection is closed).
 *
 * Normal termination is by SIGTERM, which instructs the walsender to
 * close the connection and exit(0) at the next convenient moment. Emergency
 * termination is by SIGQUIT; like any backend, the walsender will simply
 * abort and exit on SIGQUIT. A close of the connection and a FATAL error
 * are treated as not a crash but approximately normal termination;
 * the walsender will exit quickly without sending any more XLOG records.
 *
 * If the server is shut down, checkpointer sends us
 * PROCSIG_WALSND_INIT_STOPPING after all regular backends have exited.  If
 * the backend is idle or runs an SQL query this causes the backend to
 * shutdown, if logical replication is in progress all existing WAL records
 * are processed followed by a shutdown.  Otherwise this causes the walsender
 * to switch to the "stopping" state. In this state, the walsender will reject
 * any further replication commands. The checkpointer begins the shutdown
 * checkpoint once all walsenders are confirmed as stopping. When the shutdown
 * checkpoint finishes, the postmaster sends us SIGUSR2. This instructs
 * walsender to send any outstanding WAL, including the shutdown checkpoint
 * record, wait for it to be replicated to the standby, and then exit.
 *
 *
 * Portions Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/walsender.c
 *
 *-------------------------------------------------------------------------
 */
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

/*
 * Maximum data payload in a WAL data message.  Must be >= XLOG_BLCKSZ.
 *
 * We don't have a good idea of what a good value would be; there's some
 * overhead per message in both walsender and walreceiver, but on the other
 * hand sending large batches makes walsender less responsive to signals
 * because signals are checked only between messages.  128kB (with
 * default 8k blocks) seems like a reasonable guess for now.
 */


/* Array of WalSnds in shared memory */
WalSndCtlData *WalSndCtl = NULL;

/* My slot in the shared memory array */
WalSnd *MyWalSnd = NULL;

/* Global state */
bool am_walsender = false;           /* Am I a walsender process? */
bool am_cascading_walsender = false; /* Am I cascading WAL to another
                                      * standby? */
bool am_db_walsender = false;        /* Connected to a database? */

/* User-settable parameters for walsender */
int max_wal_senders = 0;            /* the maximum number of concurrent
                                     * walsenders */
int wal_sender_timeout = 60 * 1000; /* maximum time to send one WAL
                                     * data message */
bool log_replication_commands = false;

/*
 * State for WalSndWakeupRequest
 */
bool wake_wal_senders = false;

/*
 * These variables are used similarly to openLogFile/SegNo/Off,
 * but for walsender to read the XLOG.
 */
static int sendFile = -1;
static XLogSegNo sendSegNo = 0;
static uint32 sendOff = 0;

/* Timeline ID of the currently open file */
static TimeLineID curFileTimeLine = 0;

/*
 * These variables keep track of the state of the timeline we're currently
 * sending. sendTimeLine identifies the timeline. If sendTimeLineIsHistoric,
 * the timeline is not the latest timeline on this server, and the server's
 * history forked off from that timeline at sendTimeLineValidUpto.
 */
static TimeLineID sendTimeLine = 0;
static TimeLineID sendTimeLineNextTLI = 0;
static bool sendTimeLineIsHistoric = false;
static XLogRecPtr sendTimeLineValidUpto = InvalidXLogRecPtr;

/*
 * How far have we sent WAL already? This is also advertised in
 * MyWalSnd->sentPtr.  (Actually, this is the next WAL location to send.)
 */
static XLogRecPtr sentPtr = InvalidXLogRecPtr;

/* Buffers for constructing outgoing messages and processing reply messages. */
static StringInfoData output_message;
static StringInfoData reply_message;
static StringInfoData tmpbuf;

/* Timestamp of last ProcessRepliesIfAny(). */
static TimestampTz last_processing = 0;

/*
 * Timestamp of last ProcessRepliesIfAny() that saw a reply from the
 * standby. Set to 0 if wal_sender_timeout doesn't need to be active.
 */
static TimestampTz last_reply_timestamp = 0;

/* Have we sent a heartbeat message asking for reply, since last reply? */
static bool waiting_for_ping_response = false;

/*
 * While streaming WAL in Copy mode, streamingDoneSending is set to true
 * after we have sent CopyDone. We should not send any more CopyData messages
 * after that. streamingDoneReceiving is set to true when we receive CopyDone
 * from the other end. When both become true, it's time to exit Copy mode.
 */
static bool streamingDoneSending;
static bool streamingDoneReceiving;

/* Are we there yet? */
static bool WalSndCaughtUp = false;

/* Flags set by signal handlers for later service in main loop */
static volatile sig_atomic_t got_SIGUSR2 = false;
static volatile sig_atomic_t got_STOPPING = false;

/*
 * This is set while we are streaming. When not set
 * PROCSIG_WALSND_INIT_STOPPING signal will be handled like SIGTERM. When set,
 * the main loop is responsible for checking got_STOPPING and terminating when
 * it's set (after streaming any remaining WAL).
 */
static volatile sig_atomic_t replication_active = false;

static LogicalDecodingContext *logical_decoding_ctx = NULL;
static XLogRecPtr logical_startptr = InvalidXLogRecPtr;

/* A sample associating a WAL location with the time it was written. */
typedef struct {
  XLogRecPtr lsn;
  TimestampTz time;
} WalTimeSample;

/* The size of our buffer of time samples. */


/* A mechanism for tracking replication lag. */
typedef struct {
  XLogRecPtr last_lsn;
  WalTimeSample buffer[LAG_TRACKER_BUFFER_SIZE];
  int write_head;
  int read_heads[NUM_SYNC_REP_WAIT_MODE];
  WalTimeSample last_read[NUM_SYNC_REP_WAIT_MODE];
} LagTracker;

static LagTracker *lag_tracker;

/* Signal handlers */
static void WalSndLastCycleHandler(SIGNAL_ARGS);

/* Prototypes for private functions */
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
WalSndPrepareWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn,
                   TransactionId xid, bool last_write);
static void
WalSndWriteData(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid,
                bool last_write);
static void
WalSndUpdateProgress(LogicalDecodingContext *ctx, XLogRecPtr lsn,
                     TransactionId xid);
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

/* Initialize walsender process before entering the main command loop */
void
InitWalSender(void)
{






















}

/*
 * Clean up after an error.
 *
 * WAL sender processes don't use transactions like regular backends do.
 * This function does any cleanup required after an error in a WAL sender
 * process, similar to what transaction abort does in a regular backend.
 */
void
WalSndErrorCleanup(void)
{























}

/*
 * Handle a client's connection abort in an orderly manner.
 */
static void
WalSndShutdown(void)
{










}

/*
 * Handle the IDENTIFY_SYSTEM command.
 */
static void
IdentifySystem(void)
{










































































}

/*
 * Handle TIMELINE_HISTORY command.
 */
static void
SendTimeLineHistory(TimeLineHistoryCmd *cmd)
{

































































































}

/*
 * Handle START_REPLICATION command.
 *
 * At the moment, this never returns, but an ereport(ERROR) will take us back
 * to the main loop.
 */
static void
StartReplication(StartReplicationCmd *cmd)
{












































































































































































































}

/*
 * read_page callback for logical decoding contexts, as a walsender process.
 *
 * Inside the walsender we can do better than logical_read_local_xlog_page,
 * which has to do a plain sleep/busy loop, because the walsender's latch gets
 * set every time WAL is flushed.
 */
static int
logical_read_xlog_page(XLogReaderState *state, XLogRecPtr targetPagePtr,
                       int reqLen, XLogRecPtr targetRecPtr, char *cur_page,
                       TimeLineID *pageTLI)
{



























}

/*
 * Process extra options given to CREATE_REPLICATION_SLOT.
 */
static void
parseCreateReplSlotOptions(CreateReplicationSlotCmd *cmd, bool *reserve_wal,
                           CRSSnapshotAction *snapshot_action)
{





































}

/*
 * Create a new replication slot.
 */
static void
CreateReplicationSlot(CreateReplicationSlotCmd *cmd)
{





























































































































































































}

/*
 * Get rid of a replication slot that is no longer wanted.
 */
static void
DropReplicationSlot(DropReplicationSlotCmd *cmd)
{


}

/*
 * Load previously initiated logical slot and prepare for sending data (via
 * WalSndLoop).
 */
static void
StartLogicalReplication(StartReplicationCmd *cmd)
{







































































}

/*
 * LogicalDecodingContext 'prepare_write' callback.
 *
 * Prepare a write into a StringInfo.
 *
 * Don't do anything lasting in here, it's quite possible that nothing will be
 * done with the data.
 */
static void
WalSndPrepareWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn,
                   TransactionId xid, bool last_write)
{
















}

/*
 * LogicalDecodingContext 'write' callback.
 *
 * Actually write out data previously prepared by WalSndPrepareWrite out to
 * the network. Take as long as needed, but process replies from the other
 * side and check timeouts during that.
 */
static void
WalSndWriteData(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid,
                bool last_write)
{
































}

/*
 * Wait until there is no pending write. Also process replies from the other
 * side and check timeouts during that.
 */
static void
ProcessPendingWrites(void)
{














































}

/*
 * LogicalDecodingContext 'update_progress' callback.
 *
 * Write the current position to the lag tracker (see XLogSendPhysical).
 */
static void
WalSndUpdateProgress(LogicalDecodingContext *ctx, XLogRecPtr lsn,
                     TransactionId xid)
{






























}

/*
 * Wait till WAL < loc is flushed to disk so it can be safely sent to client.
 *
 * Returns end LSN of flushed WAL.  Normally this will be >= loc, but
 * if we detect a shutdown request (either from postmaster or client)
 * we will return early, so caller must always check.
 */
static XLogRecPtr
WalSndWaitForWal(XLogRecPtr loc)
{



































































































































}

/*
 * Execute an incoming replication command.
 *
 * Returns true if the cmd_string was recognized as WalSender command, false
 * if not.
 */
bool
exec_replication_command(const char *cmd_string)
{














































































































































































}

/*
 * Process any incoming messages while streaming. Also checks if the remote
 * end has closed the connection.
 */
static void
ProcessRepliesIfAny(void)
{














































































}

/*
 * Process a status update message received from standby.
 */
static void
ProcessStandbyMessage(void)
{





















}

/*
 * Remember that a walreceiver just confirmed receipt of lsn `lsn`.
 */
static void
PhysicalConfirmReceivedLocation(XLogRecPtr lsn)
{






















}

/*
 * Regular reply from standby advising of WAL locations on standby server.
 */
static void
ProcessStandbyReplyMessage(void)
{

































































































}

/* compute new replication slot xmin horizon if needed */
static void
PhysicalReplicationSlotNewXmin(TransactionId feedbackXmin,
                               TransactionId feedbackCatalogXmin)
{































}

/*
 * Check that the provided xmin/epoch are sane, that is, not in the future
 * and not so far back as to be already wrapped around.
 *
 * Epoch of nextXid should be same as standby, or if the counter has
 * wrapped, then one greater than standby.
 *
 * This check doesn't care about whether clog exists for these xids
 * at all.
 */
static bool
TransactionIdInRecentPast(TransactionId xid, uint32 epoch)
{























}

/*
 * Hot Standby feedback
 */
static void
ProcessStandbyHSFeedbackMessage(void)
{















































































































}

/*
 * Compute how long send/receive loops should sleep.
 *
 * If wal_sender_timeout is enabled we want to wake up in time to send
 * keepalives and to abort the connection if wal_sender_timeout has been
 * reached.
 */
static long
WalSndComputeSleeptime(TimestampTz now)
{



























}

/*
 * Check whether there have been responses by the client within
 * wal_sender_timeout and shutdown if not.  Using last_processing as the
 * reference point avoids counting server-side stalls against the client.
 * However, a long server-side stall can make WalSndKeepaliveIfNecessary()
 * postdate last_processing by more than wal_sender_timeout.  If that happens,
 * the client must reply almost immediately to avoid a timeout.  This rarely
 * affects the default configuration, under which clients spontaneously send a
 * message every standby_message_timeout = wal_sender_timeout/6 = 10s.  We
 * could eliminate that problem by recognizing timeout expiration at
 * wal_sender_timeout/2 after the keepalive.
 */
static void
WalSndCheckTimeOut(void)
{






















}

/* Main loop of walsender process that streams the WAL over Copy messages. */
static void
WalSndLoop(WalSndSendDataCallback send_data)
{


























































































































}

/* Initialize a per-walsender data structure for this walsender process */
static void
InitWalSenderSlot(void)
{


















































}

/* Destroy the per-walsender data structure for this walsender process */
static void
WalSndKill(int code, Datum arg)
{












}

/*
 * Read 'count' bytes from WAL into 'buf', starting at location 'startptr'
 *
 * XXX probably this should be improved to suck data directly from the
 * WAL buffers when possible.
 *
 * Will open, and keep open, one WAL segment stored in the global file
 * descriptor sendFile. This means if XLogRead is used once, there will
 * always be one descriptor left open until the process ends, but never
 * more than one.
 */
static void
XLogRead(char *buf, XLogRecPtr startptr, Size count)
{




































































































































































}

/*
 * Send out the WAL in its normal physical/stored form.
 *
 * Read up to MAX_SEND_SIZE bytes of WAL that's been flushed to disk,
 * but not yet sent to the client, and buffer it in the libpq output
 * buffer.
 *
 * If there is no unsent WAL remaining, WalSndCaughtUp is set to true,
 * otherwise WalSndCaughtUp is set to false.
 */
static void
XLogSendPhysical(void)
{





















































































































































































































































}

/*
 * Stream out logically decoded data.
 */
static void
XLogSendLogical(void)
{








































































}

/*
 * Shutdown if the sender is caught up.
 *
 * NB: This should only be called when the shutdown signal has been received
 * from postmaster.
 *
 * Note that if we determine that there's still more data to send, this
 * function will return control to the caller.
 */
static void
WalSndDone(WalSndSendDataCallback send_data)
{























}

/*
 * Returns the latest point in WAL that has been safely flushed to disk, and
 * can be sent to the standby. This should only be called when in recovery,
 * ie. we're streaming to a cascaded standby.
 *
 * As a side-effect, ThisTimeLineID is updated to the TLI of the last
 * replayed WAL record.
 */
static XLogRecPtr
GetStandbyFlushRecPtr(void)
{























}

/*
 * Request walsenders to reload the currently-open WAL file
 */
void
WalSndRqstFileReload(void)
{













}

/*
 * Handle PROCSIG_WALSND_INIT_STOPPING signal.
 */
void
HandleWalSndInitStopping(void)
{













}

/*
 * SIGUSR2: set flag to do a last cycle and shut down afterwards. The WAL
 * sender should already have been switched to WALSNDSTATE_STOPPING at
 * this point.
 */
static void
WalSndLastCycleHandler(SIGNAL_ARGS)
{






}

/* Set up signal handlers */
void
WalSndSignals(void)
{














}

/* Report shared-memory space needed by WalSndShmemInit */
Size
WalSndShmemSize(void)
{
  Size size = 0;

  size = offsetof(WalSndCtlData, walsnds);
  size = add_size(size, mul_size(max_wal_senders, sizeof(WalSnd)));

  return size;
}

/* Allocate and initialize walsender-related shared memory */
void
WalSndShmemInit(void)
{
  bool found;
  int i;

  WalSndCtl = (WalSndCtlData *)ShmemInitStruct("Wal Sender Ctl",
                                               WalSndShmemSize(), &found);

  if (!found) {
    /* First time through, so initialize */
    MemSet(WalSndCtl, 0, WalSndShmemSize());

    for (i = 0; i < NUM_SYNC_REP_WAIT_MODE; i++) {
      SHMQueueInit(&(WalSndCtl->SyncRepQueue[i]));
    }

    for (i = 0; i < max_wal_senders; i++) {
      WalSnd *walsnd = &WalSndCtl->walsnds[i];

      SpinLockInit(&walsnd->mutex);
    }
  }
}

/*
 * Wake up all walsenders
 *
 * This will be called inside critical sections, so throwing an error is not
 * advisable.
 */
void
WalSndWakeup(void)
{
  int i;

  for (i = 0; i < max_wal_senders; i++) {
    Latch *latch;
    WalSnd *walsnd = &WalSndCtl->walsnds[i];

    /*
     * Get latch pointer with spinlock held, for the unlikely case that
     * pointer reads aren't atomic (as they're 8 bytes).
     */
    SpinLockAcquire(&walsnd->mutex);
    latch = walsnd->latch;
    SpinLockRelease(&walsnd->mutex);

    if (latch != NULL) {


  }
}

/*
 * Signal all walsenders to move to stopping state.
 *
 * This will trigger walsenders to move to a state where no further WAL can be
 * generated. See this file's header for details.
 */
void
WalSndInitStopping(void)
{
  int i;

  for (i = 0; i < max_wal_senders; i++) {
    WalSnd *walsnd = &WalSndCtl->walsnds[i];
    pid_t pid;

    SpinLockAcquire(&walsnd->mutex);
    pid = walsnd->pid;
    SpinLockRelease(&walsnd->mutex);

    if (pid == 0) {
      continue;
    }



}

/*
 * Wait that all the WAL senders have quit or reached the stopping state. This
 * is used by the checkpointer to control when the shutdown checkpoint can
 * safely be performed.
 */
void
WalSndWaitStopping(void)
{
  for (;;) {
    int i;
    bool all_stopped = true;

    for (i = 0; i < max_wal_senders; i++) {
      WalSnd *walsnd = &WalSndCtl->walsnds[i];

      SpinLockAcquire(&walsnd->mutex);

      if (walsnd->pid == 0) {
        SpinLockRelease(&walsnd->mutex);
        continue;
      }









    /* safe to leave if confirmation is done for all WAL senders */
    if (all_stopped) {
      return;
    }



}

/* Set state for current walsender (only called in walsender) */
void
WalSndSetState(WalSndState state)
{











}

/*
 * Return a string constant representing the state. This is used
 * in system views, and should *not* be translated.
 */
static const char *
WalSndGetStateString(WalSndState state)
{













}

static Interval *
offset_to_interval(TimeOffset offset)
{







}

/*
 * Returns activity of walsenders, including pids and xlog locations sent to
 * standby servers.
 */
Datum
pg_stat_get_wal_senders(PG_FUNCTION_ARGS)
{




























































































































































































}

/*
 * Send a keepalive message to standby.
 *
 * If requestReply is set, the message requests the other party to send
 * a message back to us, for heartbeat purposes.  We also set a flag to
 * let nearby code that we're waiting for that response, to avoid
 * repeated requests.
 */
static void
WalSndKeepalive(bool requestReply)
{
















}

/*
 * Send keepalive message if too much time has elapsed.
 */
static void
WalSndKeepaliveIfNecessary(void)
{





























}

/*
 * Record the end of the WAL and the time it was flushed locally, so that
 * LagTrackerRead can compute the elapsed time (lag) when this WAL location is
 * eventually reported to have been written, flushed and applied by the
 * standby in a reply message.
 */
static void
LagTrackerWrite(XLogRecPtr lsn, TimestampTz local_flush_time)
{

















































}

/*
 * Find out how much time has elapsed between the moment WAL location 'lsn'
 * (or the highest known earlier LSN) was flushed locally and the time 'now'.
 * We have a separate read head for each of the reported LSN locations we
 * receive in replies from standby; 'head' controls which read head is
 * used.  Whenever a read head crosses an LSN which was written into the
 * lag buffer with LagTrackerWrite, we can use the associated timestamp to
 * find out the time this LSN (or an earlier one) was flushed locally, and
 * therefore compute the lag.
 *
 * Return -1 if no new sample data is available, and otherwise the elapsed
 * time in microseconds.
 */
static TimeOffset
LagTrackerRead(int head, XLogRecPtr lsn, TimestampTz now)
{


















































































}
