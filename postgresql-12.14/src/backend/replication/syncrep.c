                                                                            
   
             
   
                                                        
   
                                                                     
                                             
   
                                                                      
                                                                       
                                                                       
   
                                                                  
                                                                          
                                                                     
                                                                        
                                                                       
                                                             
                                      
   
                                                                          
                                                                        
                                                                      
                                                                  
                               
   
                                                                       
                                                                  
                                                                   
   
                                                                 
                                                                        
                                                                          
                                                                   
                                                                         
                                                                        
                                                                        
   
                                                                       
                                                                  
                                                                           
                                                                            
                                                                    
                                                                          
                                                                    
                                                                         
                                      
   
                                                                   
                                                                  
                                                                       
                                                                         
                                               
   
                                                                       
                                                                      
                                                  
   
                                                                 
                                                                 
                                                         
                                                                   
                                        
   
                                                                         
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "access/xact.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/ps_status.h"

                                           
char *SyncRepStandbyNames;

#define SyncStandbysDefined() (SyncRepStandbyNames != NULL && SyncRepStandbyNames[0] != '\0')

static bool announce_next_takeover = true;

SyncRepConfigData *SyncRepConfig = NULL;
static int SyncRepWaitMode = SYNC_REP_NO_WAIT;

static void
SyncRepQueueInsert(int mode);
static void
SyncRepCancelWait(void);
static int
SyncRepWakeQueue(bool all, int mode);

static bool
SyncRepGetSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr, XLogRecPtr *applyPtr, bool *am_sync);
static void
SyncRepGetOldestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr, XLogRecPtr *applyPtr, SyncRepStandbyData *sync_standbys, int num_standbys);
static void
SyncRepGetNthLatestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr, XLogRecPtr *applyPtr, SyncRepStandbyData *sync_standbys, int num_standbys, uint8 nth);
static int
SyncRepGetStandbyPriority(void);
static List *
SyncRepGetSyncStandbysPriority(bool *am_sync);
static List *
SyncRepGetSyncStandbysQuorum(bool *am_sync);
static int
standby_priority_comparator(const void *a, const void *b);
static int
cmp_lsn(const void *a, const void *b);

#ifdef USE_ASSERT_CHECKING
static bool
SyncRepQueueIsOrderedByLSN(int mode);
#endif

   
                                                               
                                                              
                                                               
   

   
                                                           
   
                                                                   
                                                                 
                                                                    
                                                                      
                                                               
   
                                                                              
                                                                             
                                                                     
                                                                     
   
void
SyncRepWaitForLSN(XLogRecPtr lsn, bool commit)
{
  char *new_status = NULL;
  const char *old_status;
  int mode;

                                                                          
  if (commit)
  {
    mode = SyncRepWaitMode;
  }
  else
  {
    mode = Min(SyncRepWaitMode, SYNC_REP_WAIT_FLUSH);
  }

     
                                                           
     
  if (!SyncRepRequested())
  {
    return;
  }

  Assert(SHMQueueIsDetached(&(MyProc->syncRepLinks)));
  Assert(WalSndCtl != NULL);

  LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);
  Assert(MyProc->syncRepState == SYNC_REP_NOT_WAITING);

     
                                                                           
                                                 
     
                                                                       
                                                                           
                             
     
  if (!WalSndCtl->sync_standbys_defined || lsn <= WalSndCtl->lsn[mode])
  {
    LWLockRelease(SyncRepLock);
    return;
  }

     
                                                                     
                             
     
  MyProc->waitLSN = lsn;
  MyProc->syncRepState = SYNC_REP_WAITING;
  SyncRepQueueInsert(mode);
  Assert(SyncRepQueueIsOrderedByLSN(mode));
  LWLockRelease(SyncRepLock);

                                                      
  if (update_process_title)
  {
    int len;

    old_status = get_ps_display(&len);
    new_status = (char *)palloc(len + 32 + 1);
    memcpy(new_status, old_status, len);
    sprintf(new_status + len, " waiting for %X/%X", (uint32)(lsn >> 32), (uint32)lsn);
    set_ps_display(new_status, false);
    new_status[len] = '\0';                                  
  }

     
                                             
     
                                                                    
                           
     
  for (;;)
  {
    int rc;

                                                    
    ResetLatch(MyLatch);

       
                                                                  
                                                                      
                                                                           
                                                                          
                     
       
    if (MyProc->syncRepState == SYNC_REP_WAIT_COMPLETE)
    {
      break;
    }

       
                                                                        
                                                                          
                                                                         
                                                                       
                                                                        
                                                                          
                                                                          
                                                                           
                                                                          
                                 
       
    if (ProcDiePending)
    {
      ereport(WARNING, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("canceling the wait for synchronous replication and terminating connection due to administrator command"), errdetail("The transaction has already committed locally, but might not have been replicated to the standby.")));
      whereToSendOutput = DestNone;
      SyncRepCancelWait();
      break;
    }

       
                                                                        
                                                                      
                                                                       
                         
       
    if (QueryCancelPending)
    {
      QueryCancelPending = false;
      ereport(WARNING, (errmsg("canceling wait for synchronous replication due to user request"), errdetail("The transaction has already committed locally, but might not have been replicated to the standby.")));
      SyncRepCancelWait();
      break;
    }

       
                                                                         
                                      
       
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH, -1, WAIT_EVENT_SYNC_REP);

       
                                                                           
                                                                         
       
    if (rc & WL_POSTMASTER_DEATH)
    {
      ProcDiePending = true;
      whereToSendOutput = DestNone;
      SyncRepCancelWait();
      break;
    }
  }

     
                                                                           
                                                                           
                                                                            
                                                                             
                                                                  
                                              
     
  pg_read_barrier();
  Assert(SHMQueueIsDetached(&(MyProc->syncRepLinks)));
  MyProc->syncRepState = SYNC_REP_NOT_WAITING;
  MyProc->waitLSN = 0;

  if (new_status)
  {
                          
    set_ps_display(new_status, false);
    pfree(new_status);
  }
}

   
                                                                                
   
                                                                            
                                                                         
   
static void
SyncRepQueueInsert(int mode)
{
  PGPROC *proc;

  Assert(mode >= 0 && mode < NUM_SYNC_REP_WAIT_MODE);
  proc = (PGPROC *)SHMQueuePrev(&(WalSndCtl->SyncRepQueue[mode]), &(WalSndCtl->SyncRepQueue[mode]), offsetof(PGPROC, syncRepLinks));

  while (proc)
  {
       
                                                                          
                          
       
    if (proc->waitLSN < MyProc->waitLSN)
    {
      break;
    }

    proc = (PGPROC *)SHMQueuePrev(&(WalSndCtl->SyncRepQueue[mode]), &(proc->syncRepLinks), offsetof(PGPROC, syncRepLinks));
  }

  if (proc)
  {
    SHMQueueInsertAfter(&(proc->syncRepLinks), &(MyProc->syncRepLinks));
  }
  else
  {
    SHMQueueInsertAfter(&(WalSndCtl->SyncRepQueue[mode]), &(MyProc->syncRepLinks));
  }
}

   
                                                                  
   
static void
SyncRepCancelWait(void)
{
  LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);
  if (!SHMQueueIsDetached(&(MyProc->syncRepLinks)))
  {
    SHMQueueDelete(&(MyProc->syncRepLinks));
  }
  MyProc->syncRepState = SYNC_REP_NOT_WAITING;
  LWLockRelease(SyncRepLock);
}

void
SyncRepCleanupAtProcExit(void)
{
     
                                                                          
                             
     
  if (!SHMQueueIsDetached(&(MyProc->syncRepLinks)))
  {
    LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);

                                                     
    if (!SHMQueueIsDetached(&(MyProc->syncRepLinks)))
    {
      SHMQueueDelete(&(MyProc->syncRepLinks));
    }

    LWLockRelease(SyncRepLock);
  }
}

   
                                                               
                                                              
                                                               
   

   
                                                                     
                                                            
   
void
SyncRepInitConfig(void)
{
  int priority;

     
                                                                          
                                        
     
  priority = SyncRepGetStandbyPriority();
  if (MyWalSnd->sync_standby_priority != priority)
  {
    SpinLockAcquire(&MyWalSnd->mutex);
    MyWalSnd->sync_standby_priority = priority;
    SpinLockRelease(&MyWalSnd->mutex);

    ereport(DEBUG1, (errmsg("standby \"%s\" now has synchronous standby priority %u", application_name, priority)));
  }
}

   
                                                                   
                                                                           
   
                                                                       
                                                    
   
void
SyncRepReleaseWaiters(void)
{
  volatile WalSndCtlData *walsndctl = WalSndCtl;
  XLogRecPtr writePtr;
  XLogRecPtr flushPtr;
  XLogRecPtr applyPtr;
  bool got_recptr;
  bool am_sync;
  int numwrite = 0;
  int numflush = 0;
  int numapply = 0;

     
                                                                       
                                                                         
                                                                             
                                                                        
                                             
     
  if (MyWalSnd->sync_standby_priority == 0 || (MyWalSnd->state != WALSNDSTATE_STREAMING && MyWalSnd->state != WALSNDSTATE_STOPPING) || XLogRecPtrIsInvalid(MyWalSnd->flush))
  {
    announce_next_takeover = true;
    return;
  }

     
                                                                         
                                                  
     
  LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);

     
                                                                          
                                                                            
                                                                            
                                                                             
                                                                      
                    
     
  got_recptr = SyncRepGetSyncRecPtr(&writePtr, &flushPtr, &applyPtr, &am_sync);

     
                                                                         
                                              
     
  if (announce_next_takeover && am_sync)
  {
    announce_next_takeover = false;

    if (SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY)
    {
      ereport(LOG, (errmsg("standby \"%s\" is now a synchronous standby with priority %u", application_name, MyWalSnd->sync_standby_priority)));
    }
    else
    {
      ereport(LOG, (errmsg("standby \"%s\" is now a candidate for quorum synchronous standby", application_name)));
    }
  }

     
                                                                        
                                              
     
  if (!got_recptr || !am_sync)
  {
    LWLockRelease(SyncRepLock);
    announce_next_takeover = !am_sync;
    return;
  }

     
                                                                             
                    
     
  if (walsndctl->lsn[SYNC_REP_WAIT_WRITE] < writePtr)
  {
    walsndctl->lsn[SYNC_REP_WAIT_WRITE] = writePtr;
    numwrite = SyncRepWakeQueue(false, SYNC_REP_WAIT_WRITE);
  }
  if (walsndctl->lsn[SYNC_REP_WAIT_FLUSH] < flushPtr)
  {
    walsndctl->lsn[SYNC_REP_WAIT_FLUSH] = flushPtr;
    numflush = SyncRepWakeQueue(false, SYNC_REP_WAIT_FLUSH);
  }
  if (walsndctl->lsn[SYNC_REP_WAIT_APPLY] < applyPtr)
  {
    walsndctl->lsn[SYNC_REP_WAIT_APPLY] = applyPtr;
    numapply = SyncRepWakeQueue(false, SYNC_REP_WAIT_APPLY);
  }

  LWLockRelease(SyncRepLock);

  elog(DEBUG3, "released %d procs up to write %X/%X, %d procs up to flush %X/%X, %d procs up to apply %X/%X", numwrite, (uint32)(writePtr >> 32), (uint32)writePtr, numflush, (uint32)(flushPtr >> 32), (uint32)flushPtr, numapply, (uint32)(applyPtr >> 32), (uint32)applyPtr);
}

   
                                                                              
   
                                                            
                                                                  
                                                                
   
                                                                         
                                              
   
static bool
SyncRepGetSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr, XLogRecPtr *applyPtr, bool *am_sync)
{
  SyncRepStandbyData *sync_standbys;
  int num_standbys;
  int i;

                                  
  *writePtr = InvalidXLogRecPtr;
  *flushPtr = InvalidXLogRecPtr;
  *applyPtr = InvalidXLogRecPtr;
  *am_sync = false;

                                                          
  if (SyncRepConfig == NULL)
  {
    return false;
  }

                                                                      
  num_standbys = SyncRepGetCandidateStandbys(&sync_standbys);

                                               
  for (i = 0; i < num_standbys; i++)
  {
    if (sync_standbys[i].is_me)
    {
      *am_sync = true;
      break;
    }
  }

     
                                                                           
                                      
     
  if (!(*am_sync) || num_standbys < SyncRepConfig->num_sync)
  {
    pfree(sync_standbys);
    return false;
  }

     
                                                                        
                                                                          
                  
     
                                                                   
                                                                             
                                        
     
                                                                             
                                                                     
                                                        
     
  if (SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY)
  {
    SyncRepGetOldestSyncRecPtr(writePtr, flushPtr, applyPtr, sync_standbys, num_standbys);
  }
  else
  {
    SyncRepGetNthLatestSyncRecPtr(writePtr, flushPtr, applyPtr, sync_standbys, num_standbys, SyncRepConfig->num_sync);
  }

  pfree(sync_standbys);
  return true;
}

   
                                                                              
   
static void
SyncRepGetOldestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr, XLogRecPtr *applyPtr, SyncRepStandbyData *sync_standbys, int num_standbys)
{
  int i;

     
                                                                          
                                                                         
                        
     
  for (i = 0; i < num_standbys; i++)
  {
    XLogRecPtr write = sync_standbys[i].write;
    XLogRecPtr flush = sync_standbys[i].flush;
    XLogRecPtr apply = sync_standbys[i].apply;

    if (XLogRecPtrIsInvalid(*writePtr) || *writePtr > write)
    {
      *writePtr = write;
    }
    if (XLogRecPtrIsInvalid(*flushPtr) || *flushPtr > flush)
    {
      *flushPtr = flush;
    }
    if (XLogRecPtrIsInvalid(*applyPtr) || *applyPtr > apply)
    {
      *applyPtr = apply;
    }
  }
}

   
                                                                        
             
   
static void
SyncRepGetNthLatestSyncRecPtr(XLogRecPtr *writePtr, XLogRecPtr *flushPtr, XLogRecPtr *applyPtr, SyncRepStandbyData *sync_standbys, int num_standbys, uint8 nth)
{
  XLogRecPtr *write_array;
  XLogRecPtr *flush_array;
  XLogRecPtr *apply_array;
  int i;

                                                            
  Assert(nth > 0 && nth <= num_standbys);

  write_array = (XLogRecPtr *)palloc(sizeof(XLogRecPtr) * num_standbys);
  flush_array = (XLogRecPtr *)palloc(sizeof(XLogRecPtr) * num_standbys);
  apply_array = (XLogRecPtr *)palloc(sizeof(XLogRecPtr) * num_standbys);

  for (i = 0; i < num_standbys; i++)
  {
    write_array[i] = sync_standbys[i].write;
    flush_array[i] = sync_standbys[i].flush;
    apply_array[i] = sync_standbys[i].apply;
  }

                                           
  qsort(write_array, num_standbys, sizeof(XLogRecPtr), cmp_lsn);
  qsort(flush_array, num_standbys, sizeof(XLogRecPtr), cmp_lsn);
  qsort(apply_array, num_standbys, sizeof(XLogRecPtr), cmp_lsn);

                                                    
  *writePtr = write_array[nth - 1];
  *flushPtr = flush_array[nth - 1];
  *applyPtr = apply_array[nth - 1];

  pfree(write_array);
  pfree(flush_array);
  pfree(apply_array);
}

   
                                                           
   
static int
cmp_lsn(const void *a, const void *b)
{
  XLogRecPtr lsn1 = *((const XLogRecPtr *)a);
  XLogRecPtr lsn2 = *((const XLogRecPtr *)b);

  if (lsn1 > lsn2)
  {
    return -1;
  }
  else if (lsn1 == lsn2)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

   
                                                                         
   
                                                                          
                                                                         
                                                                   
   
int
SyncRepGetCandidateStandbys(SyncRepStandbyData **standbys)
{
  int i;
  int n;

                           
  *standbys = (SyncRepStandbyData *)palloc(max_wal_senders * sizeof(SyncRepStandbyData));

                                                       
  if (SyncRepConfig == NULL)
  {
    return 0;
  }

                                           
  n = 0;
  for (i = 0; i < max_wal_senders; i++)
  {
    volatile WalSnd *walsnd;                                         
                                                
    SyncRepStandbyData *stby;
    WalSndState state;                                         

    walsnd = &WalSndCtl->walsnds[i];
    stby = *standbys + n;

    SpinLockAcquire(&walsnd->mutex);
    stby->pid = walsnd->pid;
    state = walsnd->state;
    stby->write = walsnd->write;
    stby->flush = walsnd->flush;
    stby->apply = walsnd->apply;
    stby->sync_standby_priority = walsnd->sync_standby_priority;
    SpinLockRelease(&walsnd->mutex);

                        
    if (stby->pid == 0)
    {
      continue;
    }

                                       
    if (state != WALSNDSTATE_STREAMING && state != WALSNDSTATE_STOPPING)
    {
      continue;
    }

                             
    if (stby->sync_standby_priority == 0)
    {
      continue;
    }

                                          
    if (XLogRecPtrIsInvalid(stby->flush))
    {
      continue;
    }

                              
    stby->walsnd_index = i;
    stby->is_me = (walsnd == MyWalSnd);
    n++;
  }

     
                                                                            
                                                                            
               
     
  if (SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY && n > SyncRepConfig->num_sync)
  {
                              
    qsort(*standbys, n, sizeof(SyncRepStandbyData), standby_priority_comparator);
                                                      
    n = SyncRepConfig->num_sync;
  }

  return n;
}

   
                                                                   
   
static int
standby_priority_comparator(const void *a, const void *b)
{
  const SyncRepStandbyData *sa = (const SyncRepStandbyData *)a;
  const SyncRepStandbyData *sb = (const SyncRepStandbyData *)b;

                                                
  if (sa->sync_standby_priority != sb->sync_standby_priority)
  {
    return sa->sync_standby_priority - sb->sync_standby_priority;
  }

     
                                                                             
                                                                         
                                                                       
     
  return sa->walsnd_index - sb->walsnd_index;
}

   
                                                                             
   
                                     
   
                                                                         
                                              
   
                                                                           
                                                                          
                                          
   
List *
SyncRepGetSyncStandbys(bool *am_sync)
{
                          
  if (am_sync != NULL)
  {
    *am_sync = false;
  }

                                                       
  if (SyncRepConfig == NULL)
  {
    return NIL;
  }

  return (SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY) ? SyncRepGetSyncStandbysPriority(am_sync) : SyncRepGetSyncStandbysQuorum(am_sync);
}

   
                                                                   
                                           
   
                                                                          
                                    
   
                                                                         
                                              
   
static List *
SyncRepGetSyncStandbysQuorum(bool *am_sync)
{
  List *result = NIL;
  int i;
  volatile WalSnd *walsnd;                                         
                                              

  Assert(SyncRepConfig->syncrep_method == SYNC_REP_QUORUM);

  for (i = 0; i < max_wal_senders; i++)
  {
    XLogRecPtr flush;
    WalSndState state;
    int pid;

    walsnd = &WalSndCtl->walsnds[i];

    SpinLockAcquire(&walsnd->mutex);
    pid = walsnd->pid;
    flush = walsnd->flush;
    state = walsnd->state;
    SpinLockRelease(&walsnd->mutex);

                        
    if (pid == 0)
    {
      continue;
    }

                                       
    if (state != WALSNDSTATE_STREAMING && state != WALSNDSTATE_STOPPING)
    {
      continue;
    }

                             
    if (walsnd->sync_standby_priority == 0)
    {
      continue;
    }

                                          
    if (XLogRecPtrIsInvalid(flush))
    {
      continue;
    }

       
                                                                         
                                
       
    result = lappend_int(result, i);
    if (am_sync != NULL && walsnd == MyWalSnd)
    {
      *am_sync = true;
    }
  }

  return result;
}

   
                                                                      
                                           
   
                                                          
                                                   
   
                                                                          
                                      
   
                                                                         
                                              
   
static List *
SyncRepGetSyncStandbysPriority(bool *am_sync)
{
  List *result = NIL;
  List *pending = NIL;
  int lowest_priority;
  int next_highest_priority;
  int this_priority;
  int priority;
  int i;
  bool am_in_pending = false;
  volatile WalSnd *walsnd;                                         
                                              

  Assert(SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY);

  lowest_priority = SyncRepConfig->nmembers;
  next_highest_priority = lowest_priority + 1;

     
                                                                           
                                                                           
                                                                          
     
  for (i = 0; i < max_wal_senders; i++)
  {
    XLogRecPtr flush;
    WalSndState state;
    int pid;

    walsnd = &WalSndCtl->walsnds[i];

    SpinLockAcquire(&walsnd->mutex);
    pid = walsnd->pid;
    flush = walsnd->flush;
    state = walsnd->state;
    SpinLockRelease(&walsnd->mutex);

                        
    if (pid == 0)
    {
      continue;
    }

                                       
    if (state != WALSNDSTATE_STREAMING && state != WALSNDSTATE_STOPPING)
    {
      continue;
    }

                             
    this_priority = walsnd->sync_standby_priority;
    if (this_priority == 0)
    {
      continue;
    }

                                          
    if (XLogRecPtrIsInvalid(flush))
    {
      continue;
    }

       
                                                                        
                                                                     
                                                                 
       
    if (this_priority == 1)
    {
      result = lappend_int(result, i);
      if (am_sync != NULL && walsnd == MyWalSnd)
      {
        *am_sync = true;
      }
      if (list_length(result) == SyncRepConfig->num_sync)
      {
        list_free(pending);
        return result;                                       
      }
    }
    else
    {
      pending = lappend_int(pending, i);
      if (am_sync != NULL && walsnd == MyWalSnd)
      {
        am_in_pending = true;
      }

         
                                                                      
                                                                     
                                                                   
                                                                  
                                                      
         
      if (this_priority < next_highest_priority)
      {
        next_highest_priority = this_priority;
      }
    }
  }

     
                                                                      
                                                                       
     
  if (list_length(result) + list_length(pending) <= SyncRepConfig->num_sync)
  {
    bool needfree = (result != NIL && pending != NIL);

       
                                                                     
                                                            
       
    if (am_sync != NULL && !(*am_sync))
    {
      *am_sync = am_in_pending;
    }

    result = list_concat(result, pending);
    if (needfree)
    {
      pfree(pending);
    }
    return result;
  }

     
                                                   
     
  priority = next_highest_priority;
  while (priority <= lowest_priority)
  {
    ListCell *cell;
    ListCell *prev = NULL;
    ListCell *next;

    next_highest_priority = lowest_priority + 1;

    for (cell = list_head(pending); cell != NULL; cell = next)
    {
      i = lfirst_int(cell);
      walsnd = &WalSndCtl->walsnds[i];

      next = lnext(cell);

      this_priority = walsnd->sync_standby_priority;
      if (this_priority == priority)
      {
        result = lappend_int(result, i);
        if (am_sync != NULL && walsnd == MyWalSnd)
        {
          *am_sync = true;
        }

           
                                                                     
                                                                       
                                          
           
        if (list_length(result) == SyncRepConfig->num_sync)
        {
          list_free(pending);
          return result;                                       
        }

           
                                                                   
                                                            
           
        pending = list_delete_cell(pending, cell, prev);

        continue;
      }

      if (this_priority < next_highest_priority)
      {
        next_highest_priority = this_priority;
      }

      prev = cell;
    }

    priority = next_highest_priority;
  }

     
                                                                            
                                                                             
                                                                       
                                                                           
                                                                        
                                                               
     
  list_free(pending);
  return result;
}

   
                                                                      
                                                                       
                                        
   
                                                                          
                                                                    
   
static int
SyncRepGetStandbyPriority(void)
{
  const char *standby_name;
  int priority;
  bool found = false;

     
                                                                             
                                              
     
  if (am_cascading_walsender)
  {
    return 0;
  }

  if (!SyncStandbysDefined() || SyncRepConfig == NULL)
  {
    return 0;
  }

  standby_name = SyncRepConfig->member_names;
  for (priority = 1; priority <= SyncRepConfig->nmembers; priority++)
  {
    if (pg_strcasecmp(standby_name, application_name) == 0 || strcmp(standby_name, "*") == 0)
    {
      found = true;
      break;
    }
    standby_name += strlen(standby_name) + 1;
  }

  if (!found)
  {
    return 0;
  }

     
                                                                             
                         
     
  return (SyncRepConfig->syncrep_method == SYNC_REP_PRIORITY) ? priority : 1;
}

   
                                                                           
                                                                     
                                                                   
                        
   
                          
   
static int
SyncRepWakeQueue(bool all, int mode)
{
  volatile WalSndCtlData *walsndctl = WalSndCtl;
  PGPROC *proc = NULL;
  PGPROC *thisproc = NULL;
  int numprocs = 0;

  Assert(mode >= 0 && mode < NUM_SYNC_REP_WAIT_MODE);
  Assert(SyncRepQueueIsOrderedByLSN(mode));

  proc = (PGPROC *)SHMQueueNext(&(WalSndCtl->SyncRepQueue[mode]), &(WalSndCtl->SyncRepQueue[mode]), offsetof(PGPROC, syncRepLinks));

  while (proc)
  {
       
                                          
       
    if (!all && walsndctl->lsn[mode] < proc->waitLSN)
    {
      return numprocs;
    }

       
                                                                    
                                                       
       
    thisproc = proc;
    proc = (PGPROC *)SHMQueueNext(&(WalSndCtl->SyncRepQueue[mode]), &(proc->syncRepLinks), offsetof(PGPROC, syncRepLinks));

       
                                   
       
    SHMQueueDelete(&(thisproc->syncRepLinks));

       
                                                                           
                                                                      
                            
       
    pg_write_barrier();

       
                                                                        
                           
       
    thisproc->syncRepState = SYNC_REP_WAIT_COMPLETE;

       
                                                                
       
    SetLatch(&(thisproc->procLatch));

    numprocs++;
  }

  return numprocs;
}

   
                                                              
                                                                                
                                                                                
                                                                            
                                    
   
void
SyncRepUpdateSyncStandbysDefined(void)
{
  bool sync_standbys_defined = SyncStandbysDefined();

  if (sync_standbys_defined != WalSndCtl->sync_standbys_defined)
  {
    LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);

       
                                                                         
                                                                      
                                                                
       
    if (!sync_standbys_defined)
    {
      int i;

      for (i = 0; i < NUM_SYNC_REP_WAIT_MODE; i++)
      {
        SyncRepWakeQueue(true, i);
      }
    }

       
                                                                      
                                                                 
                                                                       
                                                                        
                                                           
       
    WalSndCtl->sync_standbys_defined = sync_standbys_defined;

    LWLockRelease(SyncRepLock);
  }
}

#ifdef USE_ASSERT_CHECKING
static bool
SyncRepQueueIsOrderedByLSN(int mode)
{
  PGPROC *proc = NULL;
  XLogRecPtr lastLSN;

  Assert(mode >= 0 && mode < NUM_SYNC_REP_WAIT_MODE);

  lastLSN = 0;

  proc = (PGPROC *)SHMQueueNext(&(WalSndCtl->SyncRepQueue[mode]), &(WalSndCtl->SyncRepQueue[mode]), offsetof(PGPROC, syncRepLinks));

  while (proc)
  {
       
                                                                       
                          
       
    if (proc->waitLSN <= lastLSN)
    {
      return false;
    }

    lastLSN = proc->waitLSN;

    proc = (PGPROC *)SHMQueueNext(&(WalSndCtl->SyncRepQueue[mode]), &(proc->syncRepLinks), offsetof(PGPROC, syncRepLinks));
  }

  return true;
}
#endif

   
                                                               
                                                             
                                                               
   

bool
check_synchronous_standby_names(char **newval, void **extra, GucSource source)
{
  if (*newval != NULL && (*newval)[0] != '\0')
  {
    int parse_rc;
    SyncRepConfigData *pconf;

                                                               
    syncrep_parse_result = NULL;
    syncrep_parse_error_msg = NULL;

                                                    
    syncrep_scanner_init(*newval);
    parse_rc = syncrep_yyparse();
    syncrep_scanner_finish();

    if (parse_rc != 0 || syncrep_parse_result == NULL)
    {
      GUC_check_errcode(ERRCODE_SYNTAX_ERROR);
      if (syncrep_parse_error_msg)
      {
        GUC_check_errdetail("%s", syncrep_parse_error_msg);
      }
      else
      {
        GUC_check_errdetail("synchronous_standby_names parser failed");
      }
      return false;
    }

    if (syncrep_parse_result->num_sync <= 0)
    {
      GUC_check_errmsg("number of synchronous standbys (%d) must be greater than zero", syncrep_parse_result->num_sync);
      return false;
    }

                                                        
    pconf = (SyncRepConfigData *)malloc(syncrep_parse_result->config_size);
    if (pconf == NULL)
    {
      return false;
    }
    memcpy(pconf, syncrep_parse_result, syncrep_parse_result->config_size);

    *extra = (void *)pconf;

       
                                                                          
                                                                    
                                                                          
                                                                           
                          
       
  }
  else
  {
    *extra = NULL;
  }

  return true;
}

void
assign_synchronous_standby_names(const char *newval, void *extra)
{
  SyncRepConfig = (SyncRepConfigData *)extra;
}

void
assign_synchronous_commit(int newval, void *extra)
{
  switch (newval)
  {
  case SYNCHRONOUS_COMMIT_REMOTE_WRITE:
    SyncRepWaitMode = SYNC_REP_WAIT_WRITE;
    break;
  case SYNCHRONOUS_COMMIT_REMOTE_FLUSH:
    SyncRepWaitMode = SYNC_REP_WAIT_FLUSH;
    break;
  case SYNCHRONOUS_COMMIT_REMOTE_APPLY:
    SyncRepWaitMode = SYNC_REP_WAIT_APPLY;
    break;
  default:
    SyncRepWaitMode = SYNC_REP_NO_WAIT;
    break;
  }
}
