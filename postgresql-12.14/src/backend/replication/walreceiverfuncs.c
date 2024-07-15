                                                                            
   
                      
   
                                                                           
                                                                           
                         
   
                                                                         
   
   
                  
                                                
   
                                                                            
   
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

   
                                                                 
                                        
   
#define WALRCV_STARTUP_TIMEOUT 10

                                                          
Size
WalRcvShmemSize(void)
{
  Size size = 0;

  size = add_size(size, sizeof(WalRcvData));

  return size;
}

                                                               
void
WalRcvShmemInit(void)
{
  bool found;

  WalRcv = (WalRcvData *)ShmemInitStruct("Wal Receiver Ctl", WalRcvShmemSize(), &found);

  if (!found)
  {
                                           
    MemSet(WalRcv, 0, WalRcvShmemSize());
    WalRcv->walRcvState = WALRCV_STOPPED;
    SpinLockInit(&WalRcv->mutex);
    WalRcv->latch = NULL;
  }
}

                                              
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

     
                                                                            
                                                                          
                                                                         
                             
     
  if (state == WALRCV_STARTING)
  {
    pg_time_t now = (pg_time_t)time(NULL);

    if ((now - startTime) > WALRCV_STARTUP_TIMEOUT)
    {
      SpinLockAcquire(&walrcv->mutex);

      if (walrcv->walRcvState == WALRCV_STARTING)
      {
        state = walrcv->walRcvState = WALRCV_STOPPED;
      }

      SpinLockRelease(&walrcv->mutex);
    }
  }

  if (state != WALRCV_STOPPED)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                                            
                    
   
bool
WalRcvStreaming(void)
{
  WalRcvData *walrcv = WalRcv;
  WalRcvState state;
  pg_time_t startTime;

  SpinLockAcquire(&walrcv->mutex);

  state = walrcv->walRcvState;
  startTime = walrcv->startTime;

  SpinLockRelease(&walrcv->mutex);

     
                                                                            
                                                                          
                                                                         
                             
     
  if (state == WALRCV_STARTING)
  {
    pg_time_t now = (pg_time_t)time(NULL);

    if ((now - startTime) > WALRCV_STARTUP_TIMEOUT)
    {
      SpinLockAcquire(&walrcv->mutex);

      if (walrcv->walRcvState == WALRCV_STARTING)
      {
        state = walrcv->walRcvState = WALRCV_STOPPED;
      }

      SpinLockRelease(&walrcv->mutex);
    }
  }

  if (state == WALRCV_STREAMING || state == WALRCV_STARTING || state == WALRCV_RESTARTING)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                         
                                    
   
void
ShutdownWalRcv(void)
{
  WalRcvData *walrcv = WalRcv;
  pid_t walrcvpid = 0;

     
                                                                            
                                                                      
                     
     
  SpinLockAcquire(&walrcv->mutex);
  switch (walrcv->walRcvState)
  {
  case WALRCV_STOPPED:
    break;
  case WALRCV_STARTING:
    walrcv->walRcvState = WALRCV_STOPPED;
    break;

  case WALRCV_STREAMING:
  case WALRCV_WAITING:
  case WALRCV_RESTARTING:
    walrcv->walRcvState = WALRCV_STOPPING;
                      
  case WALRCV_STOPPING:
    walrcvpid = walrcv->pid;
    break;
  }
  SpinLockRelease(&walrcv->mutex);

     
                                                         
     
  if (walrcvpid != 0)
  {
    kill(walrcvpid, SIGTERM);
  }

     
                                                                       
                     
     
  while (WalRcvRunning())
  {
       
                                                                     
                
       
    HandleStartupProcInterrupts();

    pg_usleep(100000);            
  }
}

   
                                            
   
                                                                        
                                                                              
                                     
   
void
RequestXLogStreaming(TimeLineID tli, XLogRecPtr recptr, const char *conninfo, const char *slotname)
{
  WalRcvData *walrcv = WalRcv;
  bool launch = false;
  pg_time_t now = (pg_time_t)time(NULL);
  Latch *latch;

     
                                                                             
                                                                         
                                                                            
                                  
     
  if (XLogSegmentOffset(recptr, wal_segment_size) != 0)
  {
    recptr -= XLogSegmentOffset(recptr, wal_segment_size);
  }

  SpinLockAcquire(&walrcv->mutex);

                                                    
  Assert(walrcv->walRcvState == WALRCV_STOPPED || walrcv->walRcvState == WALRCV_WAITING);

  if (conninfo != NULL)
  {
    strlcpy((char *)walrcv->conninfo, conninfo, MAXCONNINFO);
  }
  else
  {
    walrcv->conninfo[0] = '\0';
  }

  if (slotname != NULL)
  {
    strlcpy((char *)walrcv->slotname, slotname, NAMEDATALEN);
  }
  else
  {
    walrcv->slotname[0] = '\0';
  }

  if (walrcv->walRcvState == WALRCV_STOPPED)
  {
    launch = true;
    walrcv->walRcvState = WALRCV_STARTING;
  }
  else
  {
    walrcv->walRcvState = WALRCV_RESTARTING;
  }
  walrcv->startTime = now;

     
                                                                     
                                                                         
     
  if (walrcv->receiveStart == 0 || walrcv->receivedTLI != tli)
  {
    walrcv->receivedUpto = recptr;
    walrcv->receivedTLI = tli;
    walrcv->latestChunkStart = recptr;
  }
  walrcv->receiveStart = recptr;
  walrcv->receiveStartTLI = tli;

  latch = walrcv->latch;

  SpinLockRelease(&walrcv->mutex);

  if (launch)
  {
    SendPostmasterSignal(PMSIGNAL_START_WALRECEIVER);
  }
  else if (latch)
  {
    SetLatch(latch);
  }
}

   
                                                                  
   
                                                                        
                                                                    
                                                                         
               
   
XLogRecPtr
GetWalRcvWriteRecPtr(XLogRecPtr *latestChunkStart, TimeLineID *receiveTLI)
{
  WalRcvData *walrcv = WalRcv;
  XLogRecPtr recptr;

  SpinLockAcquire(&walrcv->mutex);
  recptr = walrcv->receivedUpto;
  if (latestChunkStart)
  {
    *latestChunkStart = walrcv->latestChunkStart;
  }
  if (receiveTLI)
  {
    *receiveTLI = walrcv->receivedTLI;
  }
  SpinLockRelease(&walrcv->mutex);

  return recptr;
}

   
                                                   
                                            
   
int
GetReplicationApplyDelay(void)
{
  WalRcvData *walrcv = WalRcv;
  XLogRecPtr receivePtr;
  XLogRecPtr replayPtr;
  TimestampTz chunkReplayStartTime;

  SpinLockAcquire(&walrcv->mutex);
  receivePtr = walrcv->receivedUpto;
  SpinLockRelease(&walrcv->mutex);

  replayPtr = GetXLogReplayRecPtr(NULL);

  if (receivePtr == replayPtr)
  {
    return 0;
  }

  chunkReplayStartTime = GetCurrentChunkReplayStartTime();

  if (chunkReplayStartTime == 0)
  {
    return -1;
  }

  return TimestampDifferenceMilliseconds(chunkReplayStartTime, GetCurrentTimestamp());
}

   
                                                                  
                                                                          
   
int
GetReplicationTransferLatency(void)
{
  WalRcvData *walrcv = WalRcv;
  TimestampTz lastMsgSendTime;
  TimestampTz lastMsgReceiptTime;

  SpinLockAcquire(&walrcv->mutex);
  lastMsgSendTime = walrcv->lastMsgSendTime;
  lastMsgReceiptTime = walrcv->lastMsgReceiptTime;
  SpinLockRelease(&walrcv->mutex);

  return TimestampDifferenceMilliseconds(lastMsgSendTime, lastMsgReceiptTime);
}
