                                                                            
   
             
                                              
   
                                                             
                                                                     
                                      
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   
#include "postgres.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/standby.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"

                                  
int vacuum_defer_cleanup_age;
int max_standby_archive_delay = 30 * 1000;
int max_standby_streaming_delay = 30 * 1000;

static HTAB *RecoveryLockLists;

                                   
static volatile sig_atomic_t got_standby_deadlock_timeout = false;
static volatile sig_atomic_t got_standby_delay_timeout = false;
static volatile sig_atomic_t got_standby_lock_timeout = false;

static void
ResolveRecoveryConflictWithVirtualXIDs(VirtualTransactionId *waitlist, ProcSignalReason reason, bool report_waiting);
static void
SendRecoveryConflictWithBufferPin(ProcSignalReason reason);
static XLogRecPtr
LogCurrentRunningXacts(RunningTransactions CurrRunningXacts);
static void
LogAccessExclusiveLocks(int nlocks, xl_standby_lock *locks);

   
                                                             
   
typedef struct RecoveryLockListsEntry
{
  TransactionId xid;
  List *locks;
} RecoveryLockListsEntry;

   
                                      
                                                              
   
                                                                       
                                                                        
                                                                   
                                                                          
                                                                       
                                                                    
                                                      
   
void
InitRecoveryTransactionEnvironment(void)
{
  VirtualTransactionId vxid;
  HASHCTL hash_ctl;

     
                                                                           
                  
     
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(TransactionId);
  hash_ctl.entrysize = sizeof(RecoveryLockListsEntry);
  RecoveryLockLists = hash_create("RecoveryLockLists", 64, &hash_ctl, HASH_ELEM | HASH_BLOBS);

     
                                                                          
                                                                             
                                                                            
         
     
  SharedInvalBackendInit(true);

     
                                                        
     
                                                       
                                                                             
                                       
     
                                                                         
                                                                          
                                                                         
                                                                           
     
  vxid.backendId = MyBackendId;
  vxid.localTransactionId = GetNextLocalTransactionId();
  VirtualXactLockTableInsert(vxid);

  standbyState = STANDBY_INITIALIZED;
}

   
                                          
                                   
   
                                                                          
                                       
   
                                                                          
                                                                   
                                                                             
                                                                         
                    
   
void
ShutdownRecoveryTransactionEnvironment(void)
{
     
                                                                      
                                                                           
                                                                          
                                  
     
  if (RecoveryLockLists == NULL)
  {
    return;
  }

                                                              
  ExpireAllKnownAssignedTransactionIds();

                                                               
  StandbyReleaseAllLocks();

                                        
  hash_destroy(RecoveryLockLists);
  RecoveryLockLists = NULL;

                                      
  VirtualXactLockTableCleanup();
}

   
                                                         
                                                 
                                                         
   

   
                                                                             
                                                                             
                    
   
static TimestampTz
GetStandbyLimitTime(void)
{
  TimestampTz rtime;
  bool fromStream;

     
                                                                            
                                                      
     
  GetXLogReceiptTime(&rtime, &fromStream);
  if (fromStream)
  {
    if (max_standby_streaming_delay < 0)
    {
      return 0;                   
    }
    return TimestampTzPlusMilliseconds(rtime, max_standby_streaming_delay);
  }
  else
  {
    if (max_standby_archive_delay < 0)
    {
      return 0;                   
    }
    return TimestampTzPlusMilliseconds(rtime, max_standby_archive_delay);
  }
}

#define STANDBY_INITIAL_WAIT_US 1000
static int standbyWait_us = STANDBY_INITIAL_WAIT_US;

   
                                                                  
                                                                        
                                                                    
   
static bool
WaitExceedsMaxStandbyDelay(void)
{
  TimestampTz ltime;

  CHECK_FOR_INTERRUPTS();

                                   
  ltime = GetStandbyLimitTime();
  if (ltime && GetCurrentTimestamp() >= ltime)
  {
    return true;
  }

     
                                                            
     
  pg_usleep(standbyWait_us);

     
                                                                            
                                                      
     
  standbyWait_us *= 2;
  if (standbyWait_us > 1000000)
  {
    standbyWait_us = 1000000;
  }

  return false;
}

   
                                                                          
                                                                       
                                                                          
                                                
   
                                                                                
                                                                            
                                                        
   
static void
ResolveRecoveryConflictWithVirtualXIDs(VirtualTransactionId *waitlist, ProcSignalReason reason, bool report_waiting)
{
  TimestampTz waitStart = 0;
  char *new_status;

                                                                        
  if (!VirtualTransactionIdIsValid(*waitlist))
  {
    return;
  }

  if (report_waiting)
  {
    waitStart = GetCurrentTimestamp();
  }
  new_status = NULL;                                        

  while (VirtualTransactionIdIsValid(*waitlist))
  {
                                                        
    standbyWait_us = STANDBY_INITIAL_WAIT_US;

                                            
    while (!VirtualXactLock(*waitlist, false))
    {
         
                                                                      
                                        
         
      if (update_process_title && new_status == NULL && report_waiting && TimestampDifferenceExceeds(waitStart, GetCurrentTimestamp(), 500))
      {
        const char *old_status;
        int len;

        old_status = get_ps_display(&len);
        new_status = (char *)palloc(len + 8 + 1);
        memcpy(new_status, old_status, len);
        strcpy(new_status + len, " waiting");
        set_ps_display(new_status, false);
        new_status[len] = '\0';                              
      }

                                  
      if (WaitExceedsMaxStandbyDelay())
      {
        pid_t pid;

           
                                                         
           
        Assert(VirtualTransactionIdIsValid(*waitlist));
        pid = CancelVirtualTransaction(*waitlist, reason);

           
                                                                     
                                                                  
           
        if (pid != 0)
        {
          pg_usleep(5000L);
        }
      }
    }

                                                                    
    waitlist++;
  }

                                         
  if (new_status)
  {
    set_ps_display(new_status, false);
    pfree(new_status);
  }
}

void
ResolveRecoveryConflictWithSnapshot(TransactionId latestRemovedXid, RelFileNode node)
{
  VirtualTransactionId *backends;

     
                                                                             
     
                                                                        
                                                                       
                                                                             
                                                                    
                                                                             
                                                                           
                                            
     
  if (!TransactionIdIsValid(latestRemovedXid))
  {
    return;
  }

  backends = GetConflictingVirtualXIDs(latestRemovedXid, node.dbNode);

  ResolveRecoveryConflictWithVirtualXIDs(backends, PROCSIG_RECOVERY_CONFLICT_SNAPSHOT, true);
}

void
ResolveRecoveryConflictWithTablespace(Oid tsid)
{
  VirtualTransactionId *temp_file_users;

     
                                                                    
                                                               
                                                                           
            
     
                                                                           
                                                                         
                                                    
     
                                                                   
                                                                           
                                                                        
           
     
                                                                            
     
  temp_file_users = GetConflictingVirtualXIDs(InvalidTransactionId, InvalidOid);
  ResolveRecoveryConflictWithVirtualXIDs(temp_file_users, PROCSIG_RECOVERY_CONFLICT_TABLESPACE, true);
}

void
ResolveRecoveryConflictWithDatabase(Oid dbid)
{
     
                                                                          
                                                                          
                                                                             
                                      
     
                                                             
                                                                          
                                                                       
                                
     
  while (CountDBBackends(dbid) > 0)
  {
    CancelDBBackends(dbid, PROCSIG_RECOVERY_CONFLICT_DATABASE, true);

       
                                                                
                                                           
       
    pg_usleep(10000);
  }
}

   
                                                              
                                                                    
   
                                                    
                                                                
   
                                                                          
                              
   
                                                                        
                                                                       
                                                                   
   
                                                                      
                                                                    
                                                                     
                                                
   
void
ResolveRecoveryConflictWithLock(LOCKTAG locktag)
{
  TimestampTz ltime;

  Assert(InHotStandby);

  ltime = GetStandbyLimitTime();

  if (GetCurrentTimestamp() >= ltime && ltime != 0)
  {
       
                                                                     
       
    VirtualTransactionId *backends;

    backends = GetLockConflicts(&locktag, AccessExclusiveLock, NULL);

       
                                                                       
                                                                        
                                                                    
       
    ResolveRecoveryConflictWithVirtualXIDs(backends, PROCSIG_RECOVERY_CONFLICT_LOCK, false);
  }
  else
  {
       
                                                                         
                                                          
       
    EnableTimeoutParams timeouts[2];
    int cnt = 0;

    if (ltime != 0)
    {
      got_standby_lock_timeout = false;
      timeouts[cnt].id = STANDBY_LOCK_TIMEOUT;
      timeouts[cnt].type = TMPARAM_AT;
      timeouts[cnt].fin_time = ltime;
      cnt++;
    }

    got_standby_deadlock_timeout = false;
    timeouts[cnt].id = STANDBY_DEADLOCK_TIMEOUT;
    timeouts[cnt].type = TMPARAM_AFTER;
    timeouts[cnt].delay_ms = DeadlockTimeout;
    cnt++;

    enable_timeouts(timeouts, cnt);
  }

                                                               
  ProcWaitForSignal(PG_WAIT_LOCK | locktag.locktag_type);

     
                                                                         
                                                                          
           
     
  if (got_standby_lock_timeout)
  {
    goto cleanup;
  }

  if (got_standby_deadlock_timeout)
  {
    VirtualTransactionId *backends;

    backends = GetLockConflicts(&locktag, AccessExclusiveLock, NULL);

                                                  
    if (!VirtualTransactionIdIsValid(*backends))
    {
      goto cleanup;
    }

       
                                                                          
                                                   
       
    while (VirtualTransactionIdIsValid(*backends))
    {
      SignalVirtualTransaction(*backends, PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK, false);
      backends++;
    }

       
                                                                           
                                                                         
                                                                         
                                                                         
                                                                  
       
    got_standby_deadlock_timeout = false;
    ProcWaitForSignal(PG_WAIT_LOCK | locktag.locktag_type);
  }

cleanup:

     
                                                                            
                                                                            
                                                                          
                                                  
     
  disable_all_timeouts(false);
  got_standby_lock_timeout = false;
  got_standby_deadlock_timeout = false;
}

   
                                                                              
                                                                 
   
                                                                         
                                                                
   
                                                                          
                              
   
                                                                             
                                                                             
                                                                         
   
                                                                         
                                                                              
                                                                              
                                                                            
                                                                        
                                                                         
                                                                          
                                                           
   
                                                                        
                                                                              
                              
   
void
ResolveRecoveryConflictWithBufferPin(void)
{
  TimestampTz ltime;

  Assert(InHotStandby);

  ltime = GetStandbyLimitTime();

  if (GetCurrentTimestamp() >= ltime && ltime != 0)
  {
       
                                                                     
       
    SendRecoveryConflictWithBufferPin(PROCSIG_RECOVERY_CONFLICT_BUFFERPIN);
  }
  else
  {
       
                                                                       
                                            
       
    EnableTimeoutParams timeouts[2];
    int cnt = 0;

    if (ltime != 0)
    {
      timeouts[cnt].id = STANDBY_TIMEOUT;
      timeouts[cnt].type = TMPARAM_AT;
      timeouts[cnt].fin_time = ltime;
      cnt++;
    }

    got_standby_deadlock_timeout = false;
    timeouts[cnt].id = STANDBY_DEADLOCK_TIMEOUT;
    timeouts[cnt].type = TMPARAM_AFTER;
    timeouts[cnt].delay_ms = DeadlockTimeout;
    cnt++;

    enable_timeouts(timeouts, cnt);
  }

     
                                                                            
                                               
     
  ProcWaitForSignal(PG_WAIT_BUFFER_PIN);

  if (got_standby_delay_timeout)
  {
    SendRecoveryConflictWithBufferPin(PROCSIG_RECOVERY_CONFLICT_BUFFERPIN);
  }
  else if (got_standby_deadlock_timeout)
  {
       
                                                                           
                  
       
                                                                           
                                                                    
                                                                    
                                                                       
                                                                       
                                                                        
                                                                           
                                                        
       
    SendRecoveryConflictWithBufferPin(PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK);
  }

     
                                                                            
                                                                             
                                                                   
                                         
     
  disable_all_timeouts(false);
  got_standby_delay_timeout = false;
  got_standby_deadlock_timeout = false;
}

static void
SendRecoveryConflictWithBufferPin(ProcSignalReason reason)
{
  Assert(reason == PROCSIG_RECOVERY_CONFLICT_BUFFERPIN || reason == PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK);

     
                                                                        
                                                                           
                                                                      
                                                             
     
  CancelDBBackends(InvalidOid, reason, false);
}

   
                                                                       
                                                                           
                           
   
                                                                     
                                                                          
                                                                           
                                                                             
                                                                         
                                                                          
                
   
void
CheckRecoveryConflictDeadlock(void)
{
  Assert(!InRecovery);                                     

  if (!HoldingBufferPinThatDelaysRecovery())
  {
    return;
  }

     
                                                                         
                                                                           
                                                                    
                                                                      
                                                                         
     
  ereport(ERROR, (errcode(ERRCODE_T_R_DEADLOCK_DETECTED), errmsg("canceling statement due to conflict with recovery"), errdetail("User transaction caused buffer deadlock with recovery.")));
}

                                    
                             
                                    
   

   
                                                                          
             
   
void
StandbyDeadLockHandler(void)
{
  got_standby_deadlock_timeout = true;
}

   
                                                                          
   
void
StandbyTimeoutHandler(void)
{
  got_standby_delay_timeout = true;
}

   
                                                                                   
   
void
StandbyLockTimeoutHandler(void)
{
  got_standby_lock_timeout = true;
}

   
                                                         
                            
                                                         
   
                                                                    
                                                                        
                                                                     
                                                                           
                                                                     
   
                                                                           
                                    
   
                                                                        
                                                                          
                                                               
   
                                                                             
                                                          
   
                                                                  
                   
   

void
StandbyAcquireAccessExclusiveLock(TransactionId xid, Oid dbOid, Oid relOid)
{
  RecoveryLockListsEntry *entry;
  xl_standby_lock *newlock;
  LOCKTAG locktag;
  bool found;

                          
  if (!TransactionIdIsValid(xid) || TransactionIdDidCommit(xid) || TransactionIdDidAbort(xid))
  {
    return;
  }

  elog(trace_recovery(DEBUG4), "adding recovery lock: db %u rel %u", dbOid, relOid);

                                                                  
  Assert(OidIsValid(relOid));

                                                                     
  entry = hash_search(RecoveryLockLists, &xid, HASH_ENTER, &found);
  if (!found)
  {
    entry->xid = xid;
    entry->locks = NIL;
  }

  newlock = palloc(sizeof(xl_standby_lock));
  newlock->xid = xid;
  newlock->dbOid = dbOid;
  newlock->relOid = relOid;
  entry->locks = lappend(entry->locks, newlock);

  SET_LOCKTAG_RELATION(locktag, newlock->dbOid, newlock->relOid);

  (void)LockAcquire(&locktag, AccessExclusiveLock, true, false);
}

static void
StandbyReleaseLockList(List *locks)
{
  while (locks)
  {
    xl_standby_lock *lock = (xl_standby_lock *)linitial(locks);
    LOCKTAG locktag;

    elog(trace_recovery(DEBUG4), "releasing recovery lock: xid %u db %u rel %u", lock->xid, lock->dbOid, lock->relOid);
    SET_LOCKTAG_RELATION(locktag, lock->dbOid, lock->relOid);
    if (!LockRelease(&locktag, AccessExclusiveLock, true))
    {
      elog(LOG, "RecoveryLockLists contains entry for lock no longer recorded by lock manager: xid %u database %u relation %u", lock->xid, lock->dbOid, lock->relOid);
      Assert(false);
    }
    pfree(lock);
    locks = list_delete_first(locks);
  }
}

static void
StandbyReleaseLocks(TransactionId xid)
{
  RecoveryLockListsEntry *entry;

  if (TransactionIdIsValid(xid))
  {
    if ((entry = hash_search(RecoveryLockLists, &xid, HASH_FIND, NULL)))
    {
      StandbyReleaseLockList(entry->locks);
      hash_search(RecoveryLockLists, entry, HASH_REMOVE, NULL);
    }
  }
  else
  {
    StandbyReleaseAllLocks();
  }
}

   
                                                                    
                      
   
                                                                         
                                                                  
   
void
StandbyReleaseLockTree(TransactionId xid, int nsubxids, TransactionId *subxids)
{
  int i;

  StandbyReleaseLocks(xid);

  for (i = 0; i < nsubxids; i++)
  {
    StandbyReleaseLocks(subxids[i]);
  }
}

   
                                                                    
   
void
StandbyReleaseAllLocks(void)
{
  HASH_SEQ_STATUS status;
  RecoveryLockListsEntry *entry;

  elog(trace_recovery(DEBUG2), "release all standby locks");

  hash_seq_init(&status, RecoveryLockLists);
  while ((entry = hash_seq_search(&status)))
  {
    StandbyReleaseLockList(entry->locks);
    hash_search(RecoveryLockLists, entry, HASH_REMOVE, NULL);
  }
}

   
                          
                                                                      
                                                  
   
void
StandbyReleaseOldLocks(TransactionId oldxid)
{
  HASH_SEQ_STATUS status;
  RecoveryLockListsEntry *entry;

  hash_seq_init(&status, RecoveryLockLists);
  while ((entry = hash_seq_search(&status)))
  {
    Assert(TransactionIdIsValid(entry->xid));

                                       
    if (StandbyTransactionIdIsPrepared(entry->xid))
    {
      continue;
    }

                            
    if (!TransactionIdPrecedes(entry->xid, oldxid))
    {
      continue;
    }

                                                
    StandbyReleaseLockList(entry->locks);
    hash_search(RecoveryLockLists, entry, HASH_REMOVE, NULL);
  }
}

   
                                                                        
                                             
   
                                                                      
                                                                        
   

void
standby_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                     
  Assert(!XLogRecHasAnyBlockRefs(record));

                                                   
  if (standbyState == STANDBY_DISABLED)
  {
    return;
  }

  if (info == XLOG_STANDBY_LOCK)
  {
    xl_standby_locks *xlrec = (xl_standby_locks *)XLogRecGetData(record);
    int i;

    for (i = 0; i < xlrec->nlocks; i++)
    {
      StandbyAcquireAccessExclusiveLock(xlrec->locks[i].xid, xlrec->locks[i].dbOid, xlrec->locks[i].relOid);
    }
  }
  else if (info == XLOG_RUNNING_XACTS)
  {
    xl_running_xacts *xlrec = (xl_running_xacts *)XLogRecGetData(record);
    RunningTransactionsData running;

    running.xcnt = xlrec->xcnt;
    running.subxcnt = xlrec->subxcnt;
    running.subxid_overflow = xlrec->subxid_overflow;
    running.nextXid = xlrec->nextXid;
    running.latestCompletedXid = xlrec->latestCompletedXid;
    running.oldestRunningXid = xlrec->oldestRunningXid;
    running.xids = xlrec->xids;

    ProcArrayApplyRecoveryInfo(&running);
  }
  else if (info == XLOG_INVALIDATIONS)
  {
    xl_invalidations *xlrec = (xl_invalidations *)XLogRecGetData(record);

    ProcessCommittedInvalidationMessages(xlrec->msgs, xlrec->nmsgs, xlrec->relcacheInitFileInval, xlrec->dbId, xlrec->tsId);
  }
  else
  {
    elog(PANIC, "standby_redo: unknown op code %u", info);
  }
}

   
                                                                              
                                                                
   
                                            
   
                                                                   
                                                                        
                                                                      
                                                                      
                                                                       
                                    
   
                                                                      
                                                                      
                                                                  
                                                                     
                                                                  
                                                                     
   
                                                                      
                                                                          
                                                                          
                                                                     
                                                                      
                                                                         
                                                                          
                                                                          
                                                                           
                                                                          
                                                                      
                                                                          
                                                                        
                                                                      
                                                                    
                     
                                                         
                                                    
                                                   
                                                    
   
                                                                           
                                                                          
                                                                          
                                                                         
                              
   
                                                                           
                                                                        
                       
   
                                                                      
                                                                         
                                                                        
                                                                  
                                                                       
                                        
   
                                                                      
                                                                               
                                                                       
                                                                          
   
   
                                                   
   
XLogRecPtr
LogStandbySnapshot(void)
{
  XLogRecPtr recptr;
  RunningTransactions running;
  xl_standby_lock *locks;
  int nlocks;

  Assert(XLogStandbyInfoActive());

     
                                                                       
     
  locks = GetRunningTransactionLocks(&nlocks);
  if (nlocks > 0)
  {
    LogAccessExclusiveLocks(nlocks, locks);
  }
  pfree(locks);

     
                                                                          
                                                                      
     
  running = GetRunningTransactionData();

     
                                                                             
                                                                      
                                                                           
                                                                        
                                                                         
                                                                            
                                                                        
                                                                            
                                                                          
                         
     
  if (wal_level < WAL_LEVEL_LOGICAL)
  {
    LWLockRelease(ProcArrayLock);
  }

  recptr = LogCurrentRunningXacts(running);

                                             
  if (wal_level >= WAL_LEVEL_LOGICAL)
  {
    LWLockRelease(ProcArrayLock);
  }

                                                                           
  LWLockRelease(XidGenLock);

  return recptr;
}

   
                                                                 
   
                                                                            
                                                                     
                                                                              
                                                                               
                                                                    
   
static XLogRecPtr
LogCurrentRunningXacts(RunningTransactions CurrRunningXacts)
{
  xl_running_xacts xlrec;
  XLogRecPtr recptr;

  xlrec.xcnt = CurrRunningXacts->xcnt;
  xlrec.subxcnt = CurrRunningXacts->subxcnt;
  xlrec.subxid_overflow = CurrRunningXacts->subxid_overflow;
  xlrec.nextXid = CurrRunningXacts->nextXid;
  xlrec.oldestRunningXid = CurrRunningXacts->oldestRunningXid;
  xlrec.latestCompletedXid = CurrRunningXacts->latestCompletedXid;

              
  XLogBeginInsert();
  XLogSetRecordFlags(XLOG_MARK_UNIMPORTANT);
  XLogRegisterData((char *)(&xlrec), MinSizeOfXactRunningXacts);

                               
  if (xlrec.xcnt > 0)
  {
    XLogRegisterData((char *)CurrRunningXacts->xids, (xlrec.xcnt + xlrec.subxcnt) * sizeof(TransactionId));
  }

  recptr = XLogInsert(RM_STANDBY_ID, XLOG_RUNNING_XACTS);

  if (CurrRunningXacts->subxid_overflow)
  {
    elog(trace_recovery(DEBUG2), "snapshot of %u running transactions overflowed (lsn %X/%X oldest xid %u latest complete %u next xid %u)", CurrRunningXacts->xcnt, (uint32)(recptr >> 32), (uint32)recptr, CurrRunningXacts->oldestRunningXid, CurrRunningXacts->latestCompletedXid, CurrRunningXacts->nextXid);
  }
  else
  {
    elog(trace_recovery(DEBUG2), "snapshot of %u+%u running transaction ids (lsn %X/%X oldest xid %u latest complete %u next xid %u)", CurrRunningXacts->xcnt, CurrRunningXacts->subxcnt, (uint32)(recptr >> 32), (uint32)recptr, CurrRunningXacts->oldestRunningXid, CurrRunningXacts->latestCompletedXid, CurrRunningXacts->nextXid);
  }

     
                                                                           
                                                                            
                                                             
                                                                          
                                                            
                                                                         
                 
     
  XLogSetAsyncXactLSN(recptr);

  return recptr;
}

   
                                                                           
                                                        
   
static void
LogAccessExclusiveLocks(int nlocks, xl_standby_lock *locks)
{
  xl_standby_locks xlrec;

  xlrec.nlocks = nlocks;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, offsetof(xl_standby_locks, locks));
  XLogRegisterData((char *)locks, nlocks * sizeof(xl_standby_lock));
  XLogSetRecordFlags(XLOG_MARK_UNIMPORTANT);

  (void)XLogInsert(RM_STANDBY_ID, XLOG_STANDBY_LOCK);
}

   
                                                                           
   
void
LogAccessExclusiveLock(Oid dbOid, Oid relOid)
{
  xl_standby_lock xlrec;

  xlrec.xid = GetCurrentTransactionId();

  xlrec.dbOid = dbOid;
  xlrec.relOid = relOid;

  LogAccessExclusiveLocks(1, &xlrec);
  MyXactFlags |= XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK;
}

   
                                                                       
   
void
LogAccessExclusiveLockPrepare(void)
{
     
                                                                            
                                                                         
                                                              
                                                                   
                                                                           
                                                                         
                                                                            
                                                          
                                                                      
                                                               
     
  (void)GetCurrentTransactionId();
}

   
                                                                               
                                           
   
void
LogStandbyInvalidations(int nmsgs, SharedInvalidationMessage *msgs, bool relcacheInitFileInval)
{
  xl_invalidations xlrec;

                      
  memset(&xlrec, 0, sizeof(xlrec));
  xlrec.dbId = MyDatabaseId;
  xlrec.tsId = MyDatabaseTableSpace;
  xlrec.relcacheInitFileInval = relcacheInitFileInval;
  xlrec.nmsgs = nmsgs;

                         
  XLogBeginInsert();
  XLogRegisterData((char *)(&xlrec), MinSizeOfInvalidations);
  XLogRegisterData((char *)msgs, nmsgs * sizeof(SharedInvalidationMessage));
  XLogInsert(RM_STANDBY_ID, XLOG_INVALIDATIONS);
}
