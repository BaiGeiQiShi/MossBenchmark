/*-------------------------------------------------------------------------
 *
 * walreceiverfuncs.c
 *
 * This file contains functions used by the startup process to communicate
 * with the walreceiver process. Functions implementing walreceiver itself
 * are in walreceiver.c.
 *
 * Portions Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/walreceiverfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "access/xlog_internal.h"
#include "postmaster/startup.h"
#include "replication/walreceiver.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"
#include "utils/timestamp.h"

WalRcvData *WalRcv = NULL;

/*
 * How long to wait for walreceiver to start up after requesting
 * postmaster to launch it. In seconds.
 */


/* Report shared memory space needed by WalRcvShmemInit */
Size
WalRcvShmemSize(void)
{
  Size size = 0;

  size = add_size(size, sizeof(WalRcvData));

  return size;
}

/* Allocate and initialize walreceiver-related shared memory */
void
WalRcvShmemInit(void)
{
  bool found;

  WalRcv = (WalRcvData *)ShmemInitStruct("Wal Receiver Ctl", WalRcvShmemSize(),
                                         &found);

  if (!found) {
    /* First time through, so initialize */
    MemSet(WalRcv, 0, WalRcvShmemSize());
    WalRcv->walRcvState = WALRCV_STOPPED;
    SpinLockInit(&WalRcv->mutex);
    WalRcv->latch = NULL;
  }
}

/* Is walreceiver running (or starting up)? */
bool
WalRcvRunning(void)
{
  WalRcvData *walrcv = WalRcv;
  WalRcvState state;
  pg_time_t startTime;

  SpinLockAcquire(&walrcv->mutex);

  state = walrcv->walRcvState;
  startTime = walrcv->startTime;

  SpinLockRelease(&walrcv->mutex);

  /*
   * If it has taken too long for walreceiver to start up, give up. Setting
   * the state to STOPPED ensures that if walreceiver later does start up
   * after all, it will see that it's not supposed to be running and die
   * without doing anything.
   */
  if (state == WALRCV_STARTING) {













  if (state != WALRCV_STOPPED) {

  } else {
    return false;
  }
}

/*
 * Is walreceiver running and streaming (or at least attempting to connect,
 * or starting up)?
 */
bool
WalRcvStreaming(void)
{





































}

/*
 * Stop walreceiver (if running) and wait for it to die.
 * Executed by the Startup process.
 */
void
ShutdownWalRcv(void)
{
  WalRcvData *walrcv = WalRcv;
  pid_t walrcvpid = 0;

  /*
   * Request walreceiver to stop. Walreceiver will switch to WALRCV_STOPPED
   * mode once it's finished, and will also request postmaster to not
   * restart itself.
   */
  SpinLockAcquire(&walrcv->mutex);
  switch (walrcv->walRcvState) {
  case WALRCV_STOPPED:
    break;
  case WALRCV_STARTING:



  case WALRCV_STREAMING:







  }
  SpinLockRelease(&walrcv->mutex);

  /*
   * Signal walreceiver process if it was still running.
   */
  if (walrcvpid != 0) {



  /*
   * Wait for walreceiver to acknowledge its death by setting state to
   * WALRCV_STOPPED.
   */
  while (WalRcvRunning()) {








}

/*
 * Request postmaster to start walreceiver.
 *
 * recptr indicates the position where streaming should begin, conninfo
 * is a libpq connection string to use, and slotname is, optionally, the name
 * of a replication slot to acquire.
 */
void
RequestXLogStreaming(TimeLineID tli, XLogRecPtr recptr, const char *conninfo,
                     const char *slotname)
{






























































}

/*
 * Returns the last+1 byte position that walreceiver has written.
 *
 * Optionally, returns the previous chunk start, that is the first byte
 * written in the most recent walreceiver flush cycle.  Callers not
 * interested in that value may pass NULL for latestChunkStart. Same for
 * receiveTLI.
 */
XLogRecPtr
GetWalRcvWriteRecPtr(XLogRecPtr *latestChunkStart, TimeLineID *receiveTLI)
{














}

/*
 * Returns the replication apply delay in ms or -1
 * if the apply delay info is not available
 */
int
GetReplicationApplyDelay(void)
{























}

/*
 * Returns the network latency in ms, note that this includes any
 * difference in clock settings between the servers, as well as timezone.
 */
int
GetReplicationTransferLatency(void)
{










}
