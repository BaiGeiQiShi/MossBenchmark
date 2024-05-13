/*-------------------------------------------------------------------------
 *
 * walreceiver.c
 *
 * The WAL receiver process (walreceiver) is new as of Postgres 9.0. It
 * is the process in the standby server that takes charge of receiving
 * XLOG records from a primary server during streaming replication.
 *
 * When the startup process determines that it's time to start streaming,
 * it instructs postmaster to start walreceiver. Walreceiver first connects
 * to the primary server (it will be served by a walsender process
 * in the primary server), and then keeps receiving XLOG records and
 * writing them to the disk as long as the connection is alive. As XLOG
 * records are received and flushed to disk, it updates the
 * WalRcv->receivedUpto variable in shared memory, to inform the startup
 * process of how far it can proceed with XLOG replay.
 *
 * If the primary server ends streaming, but doesn't disconnect, walreceiver
 * goes into "waiting" mode, and waits for the startup process to give new
 * instructions. The startup process will treat that the same as
 * disconnection, and will rescan the archive/pg_wal directory. But when the
 * startup process wants to try streaming replication again, it will just
 * nudge the existing walreceiver process that's waiting, instead of launching
 * a new one.
 *
 * Normal termination is by SIGTERM, which instructs the walreceiver to
 * exit(0). Emergency termination is by SIGQUIT; like any postmaster child
 * process, the walreceiver will simply abort and exit on SIGQUIT. A close
 * of the connection and a FATAL error are treated not as a crash but as
 * normal operation.
 *
 * This file contains the server-facing parts of walreceiver. The libpq-
 * specific parts are in the libpqwalreceiver module. It's loaded
 * dynamically to avoid linking the server with libpq.
 *
 * Portions Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/walreceiver.c
 *
 *-------------------------------------------------------------------------
 */
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

/* GUC variables */
int wal_receiver_status_interval;
int wal_receiver_timeout;
bool hot_standby_feedback;

/* libpqwalreceiver connection */
static WalReceiverConn *wrconn = NULL;
WalReceiverFunctionsType *WalReceiverFunctions = NULL;



/*
 * These variables are used similarly to openLogFile/SegNo/Off,
 * but for walreceiver to write the XLOG. recvFileTLI is the TimeLineID
 * corresponding the filename of recvFile.
 */
static int recvFile = -1;
static TimeLineID recvFileTLI = 0;
static XLogSegNo recvSegNo = 0;
static uint32 recvOff = 0;

/*
 * Flags set by interrupt handlers of walreceiver for later service in the
 * main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGTERM = false;

/*
 * LogstreamResult indicates the byte positions that we have already
 * written/fsynced.
 */
static struct {
  XLogRecPtr Write; /* last byte + 1 written out in the standby */
  XLogRecPtr Flush; /* last byte + 1 flushed in the standby */
} LogstreamResult;

static StringInfoData reply_message;
static StringInfoData incoming_message;

/* Prototypes for private functions */
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

/* Signal handlers */
static void WalRcvSigHupHandler(SIGNAL_ARGS);
static void WalRcvSigUsr1Handler(SIGNAL_ARGS);
static void WalRcvShutdownHandler(SIGNAL_ARGS);
static void WalRcvQuickDieHandler(SIGNAL_ARGS);

/*
 * Process any interrupts the walreceiver process may have received.
 * This should be called any time the process's latch has become set.
 *
 * Currently, only SIGTERM is of interest.  We can't just exit(1) within the
 * SIGTERM signal handler, because the signal might arrive in the middle of
 * some critical operation, like while we're holding a spinlock.  Instead, the
 * signal handler sets a flag variable as well as setting the process's latch.
 * We must check the flag (by calling ProcessWalRcvInterrupts) anytime the
 * latch has become set.  Operations that could block for a long time, such as
 * reading from a remote server, must pay attention to the latch too; see
 * libpqrcv_PQgetResult for example.
 */
void
ProcessWalRcvInterrupts(void)
{














}

/* Main entry point for walreceiver process */
void
WalReceiverMain(void)
{














































































































































































































































































































































































































































}

/*
 * Wait for startup process to set receiveStart and receiveStartTLI.
 */
static void
WalRcvWaitForStartPosition(XLogRecPtr *startpoint, TimeLineID *startpointTLI)
{

































































}

/*
 * Fetch any missing timeline history files between 'first' and 'last'
 * (inclusive) from the server.
 */
static void
WalRcvFetchTimeLineHistoryFiles(TimeLineID first, TimeLineID last)
{

















































}

/*
 * Mark us as STOPPED in shared memory at exit.
 */
static void
WalRcvDie(int code, Datum arg)
{


























}

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
WalRcvSigHupHandler(SIGNAL_ARGS)
{

}

/* SIGUSR1: used by latch mechanism */
static void
WalRcvSigUsr1Handler(SIGNAL_ARGS)
{





}

/* SIGTERM: set flag for ProcessWalRcvInterrupts */
static void
WalRcvShutdownHandler(SIGNAL_ARGS)
{









}

/*
 * WalRcvQuickDieHandler() occurs when signalled SIGQUIT by the postmaster.
 *
 * Some backend has bought the farm, so we need to stop what we're doing and
 * exit.
 */
static void
WalRcvQuickDieHandler(SIGNAL_ARGS)
{















}

/*
 * Accept the message from XLOG stream, and process it.
 */
static void
XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len)
{




























































}

/*
 * Write XLOG data to disk.
 */
static void
XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr)
{












































































}

/*
 * Flush the log to disk.
 *
 * If we're in the midst of dying, it's unwise to do anything that might throw
 * an error, so we skip sending a reply in that case.
 */
static void
XLogWalRcvFlush(bool dying)
{






































}

/*
 * Close the current segment.
 *
 * Flush the segment to disk before closing it. Otherwise we have to
 * reopen and fsync it later.
 *
 * Create an archive notification file since the segment is known completed.
 */
static void
XLogWalRcvClose(XLogRecPtr recptr)
{

































}

/*
 * Send reply message to primary, indicating our current WAL locations, oldest
 * xmin and the current time.
 *
 * If 'force' is not set, the message is only sent if enough time has
 * passed since last status update to reach wal_receiver_status_interval.
 * If wal_receiver_status_interval is disabled altogether and 'force' is
 * false, this is a no-op.
 *
 * If 'requestReply' is true, requests the server to reply immediately upon
 * receiving this message. This is used for heartbearts, when approaching
 * wal_receiver_timeout.
 */
static void
XLogWalRcvSendReply(bool force, bool requestReply)
{






















































}

/*
 * Send hot standby feedback message to primary, plus the current time,
 * in case they don't have a watch.
 *
 * If the user disables feedback, send one final message to tell sender
 * to forget about the xmin on this standby. We also send this message
 * on first connect because a previous connection might have set xmin
 * on a replication slot. (If we're not using a slot it's harmless to
 * send a feedback message explicitly setting InvalidTransactionId).
 */
static void
XLogWalRcvSendHSFeedback(bool immed)
{











































































































}

/*
 * Update shared memory status upon receiving a message from primary.
 *
 * 'walEnd' and 'sendTime' are the end-of-WAL and timestamp of the latest
 * message, reported by primary.
 */
static void
ProcessWalSndrMessage(XLogRecPtr walEnd, TimestampTz sendTime)
{








































}

/*
 * Wake up the walreceiver main loop.
 *
 * This is called by the startup process whenever interesting xlog records
 * are applied, so that walreceiver can check if it needs to send an apply
 * notification back to the master which may be waiting in a COMMIT with
 * synchronous_commit = remote_apply.
 */
void
WalRcvForceReply(void)
{










}

/*
 * Return a string constant representing the state. This is used
 * in system functions and views, and should *not* be translated.
 */
static const char *
WalRcvGetStateString(WalRcvState state)
{















}

/*
 * Returns activity of WAL receiver, including pid, state and xlog locations
 * received from the WAL sender of another server.
 */
Datum
pg_stat_get_wal_receiver(PG_FUNCTION_ARGS)
{



























































































































}
