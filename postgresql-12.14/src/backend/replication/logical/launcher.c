                                                                            
              
                                                             
   
                                                                
   
                  
                                                
   
         
                                                                        
                                                                    
                                                         
   
                                                                            
   

#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"

#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"

#include "libpq/pqsignal.h"

#include "postmaster/bgworker.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"

#include "replication/logicallauncher.h"
#include "replication/logicalworker.h"
#include "replication/slot.h"
#include "replication/walreceiver.h"
#include "replication/worker_internal.h"

#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"

#include "tcop/tcopprot.h"

#include "utils/memutils.h"
#include "utils/pg_lsn.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"
#include "utils/snapmgr.h"

                                          
#define DEFAULT_NAPTIME_PER_CYCLE 180000L

int max_logical_replication_workers = 4;
int max_sync_workers_per_subscription = 2;

LogicalRepWorker *MyLogicalRepWorker = NULL;

typedef struct LogicalRepCtxStruct
{
                           
  pid_t launcher_pid;

                           
  LogicalRepWorker workers[FLEXIBLE_ARRAY_MEMBER];
} LogicalRepCtxStruct;

LogicalRepCtxStruct *LogicalRepCtx;

typedef struct LogicalRepWorkerId
{
  Oid subid;
  Oid relid;
} LogicalRepWorkerId;

typedef struct StopWorkersData
{
  int nestDepth;                                                  
  List *workers;                                                  
  struct StopWorkersData *parent;                                  
                                                             
} StopWorkersData;

   
                                                                              
                                          
   
static StopWorkersData *on_commit_stop_workers = NULL;

static void
ApplyLauncherWakeup(void);
static void
logicalrep_launcher_onexit(int code, Datum arg);
static void
logicalrep_worker_onexit(int code, Datum arg);
static void
logicalrep_worker_detach(void);
static void
logicalrep_worker_cleanup(LogicalRepWorker *worker);

                                  
static volatile sig_atomic_t got_SIGHUP = false;

static bool on_commit_launcher_wakeup = false;

Datum pg_stat_get_subscription(PG_FUNCTION_ARGS);

   
                                   
   
                                                                              
                      
   
static List *
get_subscription_list(void)
{
  List *res = NIL;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tup;
  MemoryContext resultcxt;

                                                                    
  resultcxt = CurrentMemoryContext;

     
                                                                           
                                                                          
                                                                            
                                                                         
                                                                     
     
  StartTransactionCommand();
  (void)GetTransactionSnapshot();

  rel = table_open(SubscriptionRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);

  while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
  {
    Form_pg_subscription subform = (Form_pg_subscription)GETSTRUCT(tup);
    Subscription *sub;
    MemoryContext oldcxt;

       
                                                             
                                                                           
                                                                        
                                                       
       
    oldcxt = MemoryContextSwitchTo(resultcxt);

    sub = (Subscription *)palloc0(sizeof(Subscription));
    sub->oid = subform->oid;
    sub->dbid = subform->subdbid;
    sub->owner = subform->subowner;
    sub->enabled = subform->subenabled;
    sub->name = pstrdup(NameStr(subform->subname));
                                                        

    res = lappend(res, sub);
    MemoryContextSwitchTo(oldcxt);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  CommitTransactionCommand();

  return res;
}

   
                                                                             
   
                                                                            
                    
   
static void
WaitForReplicationWorkerAttach(LogicalRepWorker *worker, uint16 generation, BackgroundWorkerHandle *handle)
{
  BgwHandleStatus status;
  int rc;

  for (;;)
  {
    pid_t pid;

    CHECK_FOR_INTERRUPTS();

    LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

                                                                    
    if (!worker->in_use || worker->proc)
    {
      LWLockRelease(LogicalRepWorkerLock);
      return;
    }

    LWLockRelease(LogicalRepWorkerLock);

                                                                           
    status = GetBackgroundWorkerPid(handle, &pid);

    if (status == BGWH_STOPPED)
    {
      LWLockAcquire(LogicalRepWorkerLock, LW_EXCLUSIVE);
                                                                 
      if (generation == worker->generation)
      {
        logicalrep_worker_cleanup(worker);
      }
      LWLockRelease(LogicalRepWorkerLock);
      return;
    }

       
                                                                         
                                                                           
       
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 10L, WAIT_EVENT_BGWORKER_STARTUP);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }
  }

  return;
}

   
                                                                   
                              
   
LogicalRepWorker *
logicalrep_worker_find(Oid subid, Oid relid, bool only_running)
{
  int i;
  LogicalRepWorker *res = NULL;

  Assert(LWLockHeldByMe(LogicalRepWorkerLock));

                                                               
  for (i = 0; i < max_logical_replication_workers; i++)
  {
    LogicalRepWorker *w = &LogicalRepCtx->workers[i];

    if (w->in_use && w->subid == subid && w->relid == relid && (!only_running || w->proc))
    {
      res = w;
      break;
    }
  }

  return res;
}

   
                                                                            
                                       
   
List *
logicalrep_workers_find(Oid subid, bool only_running)
{
  int i;
  List *res = NIL;

  Assert(LWLockHeldByMe(LogicalRepWorkerLock));

                                                               
  for (i = 0; i < max_logical_replication_workers; i++)
  {
    LogicalRepWorker *w = &LogicalRepCtx->workers[i];

    if (w->in_use && w->subid == subid && (!only_running || w->proc))
    {
      res = lappend(res, w);
    }
  }

  return res;
}

   
                                                   
   
void
logicalrep_worker_launch(Oid dbid, Oid subid, const char *subname, Oid userid, Oid relid)
{
  BackgroundWorker bgw;
  BackgroundWorkerHandle *bgw_handle;
  uint16 generation;
  int i;
  int slot = 0;
  LogicalRepWorker *worker = NULL;
  int nsyncworkers;
  TimestampTz now;

  ereport(DEBUG1, (errmsg("starting logical replication worker for subscription \"%s\"", subname)));

                                                                       
  if (max_replication_slots == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("cannot start logical replication workers when max_replication_slots = 0")));
  }

     
                                                                            
                              
     
  LWLockAcquire(LogicalRepWorkerLock, LW_EXCLUSIVE);

retry:
                                
  for (i = 0; i < max_logical_replication_workers; i++)
  {
    LogicalRepWorker *w = &LogicalRepCtx->workers[i];

    if (!w->in_use)
    {
      worker = w;
      slot = i;
      break;
    }
  }

  nsyncworkers = logicalrep_sync_worker_count(subid);

  now = GetCurrentTimestamp();

     
                                                                       
                                                                            
                                                                           
     
  if (worker == NULL || nsyncworkers >= max_sync_workers_per_subscription)
  {
    bool did_cleanup = false;

    for (i = 0; i < max_logical_replication_workers; i++)
    {
      LogicalRepWorker *w = &LogicalRepCtx->workers[i];

         
                                                                        
                            
         
      if (w->in_use && !w->proc && TimestampDifferenceExceeds(w->launch_time, now, wal_receiver_timeout))
      {
        elog(WARNING, "logical replication worker for subscription %u took too long to start; canceled", w->subid);

        logicalrep_worker_cleanup(w);
        did_cleanup = true;
      }
    }

    if (did_cleanup)
    {
      goto retry;
    }
  }

     
                                                                              
                                                                             
                                                           
     
  if (OidIsValid(relid) && nsyncworkers >= max_sync_workers_per_subscription)
  {
    LWLockRelease(LogicalRepWorkerLock);
    return;
  }

     
                                                                          
                     
     
  if (worker == NULL)
  {
    LWLockRelease(LogicalRepWorkerLock);
    ereport(WARNING, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("out of logical replication worker slots"), errhint("You might need to increase max_logical_replication_workers.")));
    return;
  }

                                
  worker->launch_time = now;
  worker->in_use = true;
  worker->generation++;
  worker->proc = NULL;
  worker->dbid = dbid;
  worker->userid = userid;
  worker->subid = subid;
  worker->relid = relid;
  worker->relstate = SUBREL_STATE_UNKNOWN;
  worker->relstate_lsn = InvalidXLogRecPtr;
  worker->last_lsn = InvalidXLogRecPtr;
  TIMESTAMP_NOBEGIN(worker->last_send_time);
  TIMESTAMP_NOBEGIN(worker->last_recv_time);
  worker->reply_lsn = InvalidXLogRecPtr;
  TIMESTAMP_NOBEGIN(worker->reply_time);

                                                                             
  generation = worker->generation;

  LWLockRelease(LogicalRepWorkerLock);

                                        
  memset(&bgw, 0, sizeof(bgw));
  bgw.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
  snprintf(bgw.bgw_library_name, BGW_MAXLEN, "postgres");
  snprintf(bgw.bgw_function_name, BGW_MAXLEN, "ApplyWorkerMain");
  if (OidIsValid(relid))
  {
    snprintf(bgw.bgw_name, BGW_MAXLEN, "logical replication worker for subscription %u sync %u", subid, relid);
  }
  else
  {
    snprintf(bgw.bgw_name, BGW_MAXLEN, "logical replication worker for subscription %u", subid);
  }
  snprintf(bgw.bgw_type, BGW_MAXLEN, "logical replication worker");

  bgw.bgw_restart_time = BGW_NEVER_RESTART;
  bgw.bgw_notify_pid = MyProcPid;
  bgw.bgw_main_arg = Int32GetDatum(slot);

  if (!RegisterDynamicBackgroundWorker(&bgw, &bgw_handle))
  {
                                                              
    LWLockAcquire(LogicalRepWorkerLock, LW_EXCLUSIVE);
    Assert(generation == worker->generation);
    logicalrep_worker_cleanup(worker);
    LWLockRelease(LogicalRepWorkerLock);

    ereport(WARNING, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("out of background worker slots"), errhint("You might need to increase max_worker_processes.")));
    return;
  }

                                   
  WaitForReplicationWorkerAttach(worker, generation, bgw_handle);
}

   
                                                                               
                              
   
void
logicalrep_worker_stop(Oid subid, Oid relid)
{
  LogicalRepWorker *worker;
  uint16 generation;

  LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

  worker = logicalrep_worker_find(subid, relid, false);

                                 
  if (!worker)
  {
    LWLockRelease(LogicalRepWorkerLock);
    return;
  }

     
                                                                             
                            
     
  generation = worker->generation;

     
                                                                         
                                                                   
     
  while (worker->in_use && !worker->proc)
  {
    int rc;

    LWLockRelease(LogicalRepWorkerLock);

                                                              
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 10L, WAIT_EVENT_BGWORKER_STARTUP);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }

                                
    LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

       
                                                                         
                                                                       
                                                                      
       
    if (!worker->in_use || worker->generation != generation)
    {
      LWLockRelease(LogicalRepWorkerLock);
      return;
    }

                                                      
    if (worker->proc)
    {
      break;
    }
  }

                                    
  kill(worker->proc->pid, SIGTERM);

                                   
  for (;;)
  {
    int rc;

                     
    if (!worker->proc || worker->generation != generation)
    {
      break;
    }

    LWLockRelease(LogicalRepWorkerLock);

                                                              
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 10L, WAIT_EVENT_BGWORKER_SHUTDOWN);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }

    LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);
  }

  LWLockRelease(LogicalRepWorkerLock);
}

   
                                                                 
   
void
logicalrep_worker_stop_at_commit(Oid subid, Oid relid)
{
  int nestDepth = GetCurrentTransactionNestLevel();
  LogicalRepWorkerId *wid;
  MemoryContext oldctx;

                                                                          
  oldctx = MemoryContextSwitchTo(TopTransactionContext);

                                                                  
  Assert(on_commit_stop_workers == NULL || nestDepth >= on_commit_stop_workers->nestDepth);

     
                                                                           
                
     
  if (on_commit_stop_workers == NULL || nestDepth > on_commit_stop_workers->nestDepth)
  {
    StopWorkersData *newdata = palloc(sizeof(StopWorkersData));

    newdata->nestDepth = nestDepth;
    newdata->workers = NIL;
    newdata->parent = on_commit_stop_workers;
    on_commit_stop_workers = newdata;
  }

     
                                                                  
                     
     
  wid = palloc(sizeof(LogicalRepWorkerId));
  wid->subid = subid;
  wid->relid = relid;
  on_commit_stop_workers->workers = lappend(on_commit_stop_workers->workers, wid);

  MemoryContextSwitchTo(oldctx);
}

   
                                                                               
   
void
logicalrep_worker_wakeup(Oid subid, Oid relid)
{
  LogicalRepWorker *worker;

  LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

  worker = logicalrep_worker_find(subid, relid, true);

  if (worker)
  {
    logicalrep_worker_wakeup_ptr(worker);
  }

  LWLockRelease(LogicalRepWorkerLock);
}

   
                                                                   
   
                                                                   
   
void
logicalrep_worker_wakeup_ptr(LogicalRepWorker *worker)
{
  Assert(LWLockHeldByMe(LogicalRepWorkerLock));

  SetLatch(&worker->proc->procLatch);
}

   
                     
   
void
logicalrep_worker_attach(int slot)
{
                                
  LWLockAcquire(LogicalRepWorkerLock, LW_EXCLUSIVE);

  Assert(slot >= 0 && slot < max_logical_replication_workers);
  MyLogicalRepWorker = &LogicalRepCtx->workers[slot];

  if (!MyLogicalRepWorker->in_use)
  {
    LWLockRelease(LogicalRepWorkerLock);
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication worker slot %d is empty, cannot attach", slot)));
  }

  if (MyLogicalRepWorker->proc)
  {
    LWLockRelease(LogicalRepWorkerLock);
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication worker slot %d is already used by "
                                                                              "another worker, cannot attach",
                                                                           slot)));
  }

  MyLogicalRepWorker->proc = MyProc;
  before_shmem_exit(logicalrep_worker_onexit, (Datum)0);

  LWLockRelease(LogicalRepWorkerLock);
}

   
                                                  
   
static void
logicalrep_worker_detach(void)
{
                                
  LWLockAcquire(LogicalRepWorkerLock, LW_EXCLUSIVE);

  logicalrep_worker_cleanup(MyLogicalRepWorker);

  LWLockRelease(LogicalRepWorkerLock);
}

   
                         
   
static void
logicalrep_worker_cleanup(LogicalRepWorker *worker)
{
  Assert(LWLockHeldByMeInMode(LogicalRepWorkerLock, LW_EXCLUSIVE));

  worker->in_use = false;
  worker->proc = NULL;
  worker->dbid = InvalidOid;
  worker->userid = InvalidOid;
  worker->subid = InvalidOid;
  worker->relid = InvalidOid;
}

   
                                                      
   
                                                
   
static void
logicalrep_launcher_onexit(int code, Datum arg)
{
  LogicalRepCtx->launcher_pid = 0;
}

   
                     
   
                                              
   
static void
logicalrep_worker_onexit(int code, Datum arg)
{
                                                   
  if (LogRepWorkerWalRcvConn)
  {
    walrcv_disconnect(LogRepWorkerWalRcvConn);
  }

  logicalrep_worker_detach();

  ApplyLauncherWakeup();
}

                                                                      
static void
logicalrep_launcher_sighup(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;

                                                   
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                                                                         
                       
   
int
logicalrep_sync_worker_count(Oid subid)
{
  int i;
  int res = 0;

  Assert(LWLockHeldByMe(LogicalRepWorkerLock));

                                                               
  for (i = 0; i < max_logical_replication_workers; i++)
  {
    LogicalRepWorker *w = &LogicalRepCtx->workers[i];

    if (w->subid == subid && OidIsValid(w->relid))
    {
      res++;
    }
  }

  return res;
}

   
                          
                                                                
   
Size
ApplyLauncherShmemSize(void)
{
  Size size;

     
                                                              
     
  size = sizeof(LogicalRepCtxStruct);
  size = MAXALIGN(size);
  size = add_size(size, mul_size(max_logical_replication_workers, sizeof(LogicalRepWorker)));
  return size;
}

   
                         
                                                                           
   
void
ApplyLauncherRegister(void)
{
  BackgroundWorker bgw;

  if (max_logical_replication_workers == 0)
  {
    return;
  }

  memset(&bgw, 0, sizeof(bgw));
  bgw.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
  snprintf(bgw.bgw_library_name, BGW_MAXLEN, "postgres");
  snprintf(bgw.bgw_function_name, BGW_MAXLEN, "ApplyLauncherMain");
  snprintf(bgw.bgw_name, BGW_MAXLEN, "logical replication launcher");
  snprintf(bgw.bgw_type, BGW_MAXLEN, "logical replication launcher");
  bgw.bgw_restart_time = 5;
  bgw.bgw_notify_pid = 0;
  bgw.bgw_main_arg = (Datum)0;

  RegisterBackgroundWorker(&bgw);
}

   
                          
                                                               
   
void
ApplyLauncherShmemInit(void)
{
  bool found;

  LogicalRepCtx = (LogicalRepCtxStruct *)ShmemInitStruct("Logical Replication Launcher Data", ApplyLauncherShmemSize(), &found);

  if (!found)
  {
    int slot;

    memset(LogicalRepCtx, 0, ApplyLauncherShmemSize());

                                                                
    for (slot = 0; slot < max_logical_replication_workers; slot++)
    {
      LogicalRepWorker *worker = &LogicalRepCtx->workers[slot];

      memset(worker, 0, sizeof(LogicalRepWorker));
      SpinLockInit(&worker->relmutex);
    }
  }
}

   
                                                                         
            
   
bool
XactManipulatesLogicalReplicationWorkers(void)
{
  return (on_commit_stop_workers != NULL);
}

   
                                               
   
void
AtEOXact_ApplyLauncher(bool isCommit)
{

  Assert(on_commit_stop_workers == NULL || (on_commit_stop_workers->nestDepth == 1 && on_commit_stop_workers->parent == NULL));

  if (isCommit)
  {
    ListCell *lc;

    if (on_commit_stop_workers != NULL)
    {
      List *workers = on_commit_stop_workers->workers;

      foreach (lc, workers)
      {
        LogicalRepWorkerId *wid = lfirst(lc);

        logicalrep_worker_stop(wid->subid, wid->relid);
      }
    }

    if (on_commit_launcher_wakeup)
    {
      ApplyLauncherWakeup();
    }
  }

     
                                                                   
                                                                    
     
  on_commit_stop_workers = NULL;
  on_commit_launcher_wakeup = false;
}

   
                                                                     
                                 
                                                                 
                      
   
void
AtEOSubXact_ApplyLauncher(bool isCommit, int nestDepth)
{
  StopWorkersData *parent;

                                                                
  if (on_commit_stop_workers == NULL || on_commit_stop_workers->nestDepth < nestDepth)
  {
    return;
  }

  Assert(on_commit_stop_workers->nestDepth == nestDepth);

  parent = on_commit_stop_workers->parent;

  if (isCommit)
  {
       
                                                             
                                                                         
                                                                        
                             
       
    if (!parent || parent->nestDepth < nestDepth - 1)
    {
      on_commit_stop_workers->nestDepth--;
      return;
    }

    parent->workers = list_concat(parent->workers, on_commit_stop_workers->workers);
  }
  else
  {
       
                                                                           
                                                         
       
    list_free_deep(on_commit_stop_workers->workers);
  }

     
                                                                            
                                                        
     
  pfree(on_commit_stop_workers);
  on_commit_stop_workers = parent;
}

   
                                                                
   
                                                                         
                                                                           
                                                   
   
void
ApplyLauncherWakeupAtCommit(void)
{
  if (!on_commit_launcher_wakeup)
  {
    on_commit_launcher_wakeup = true;
  }
}

static void
ApplyLauncherWakeup(void)
{
  if (LogicalRepCtx->launcher_pid != 0)
  {
    kill(LogicalRepCtx->launcher_pid, SIGUSR1);
  }
}

   
                                             
   
void
ApplyLauncherMain(Datum main_arg)
{
  TimestampTz last_start_time = 0;

  ereport(DEBUG1, (errmsg("logical replication launcher started")));

  before_shmem_exit(logicalrep_launcher_onexit, (Datum)0);

  Assert(LogicalRepCtx->launcher_pid == 0);
  LogicalRepCtx->launcher_pid = MyProcPid;

                                  
  pqsignal(SIGHUP, logicalrep_launcher_sighup);
  pqsignal(SIGTERM, die);
  BackgroundWorkerUnblockSignals();

     
                                                                  
                       
     
  BackgroundWorkerInitializeConnection(NULL, NULL, 0);

                       
  for (;;)
  {
    int rc;
    List *sublist;
    ListCell *lc;
    MemoryContext subctx;
    MemoryContext oldctx;
    TimestampTz now;
    long wait_time = DEFAULT_NAPTIME_PER_CYCLE;

    CHECK_FOR_INTERRUPTS();

    now = GetCurrentTimestamp();

                                                                     
    if (TimestampDifferenceExceeds(last_start_time, now, wal_retrieve_retry_interval))
    {
                                                                        
      subctx = AllocSetContextCreate(TopMemoryContext, "Logical Replication Launcher sublist", ALLOCSET_DEFAULT_SIZES);
      oldctx = MemoryContextSwitchTo(subctx);

                                                      
      sublist = get_subscription_list();

                                                                
      foreach (lc, sublist)
      {
        Subscription *sub = (Subscription *)lfirst(lc);
        LogicalRepWorker *w;

        if (!sub->enabled)
        {
          continue;
        }

        LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);
        w = logicalrep_worker_find(sub->oid, InvalidOid, false);
        LWLockRelease(LogicalRepWorkerLock);

        if (w == NULL)
        {
          last_start_time = now;
          wait_time = wal_retrieve_retry_interval;

          logicalrep_worker_launch(sub->dbid, sub->oid, sub->name, sub->owner, InvalidOid);
        }
      }

                                                   
      MemoryContextSwitchTo(oldctx);
                                       
      MemoryContextDelete(subctx);
    }
    else
    {
         
                                                                 
                                                                         
                                                                  
                                            
         
      wait_time = wal_retrieve_retry_interval;
    }

                             
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, wait_time, WAIT_EVENT_LOGICAL_LAUNCHER_MAIN);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }

    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
    }
  }

                     
}

   
                                                        
   
bool
IsLogicalLauncher(void)
{
  return LogicalRepCtx->launcher_pid == MyProcPid;
}

   
                                       
   
Datum
pg_stat_get_subscription(PG_FUNCTION_ARGS)
{
#define PG_STAT_GET_SUBSCRIPTION_COLS 8
  Oid subid = PG_ARGISNULL(0) ? InvalidOid : PG_GETARG_OID(0);
  int i;
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;

                                                                 
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

                                                        
  LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

  for (i = 0; i < max_logical_replication_workers; i++)
  {
                      
    Datum values[PG_STAT_GET_SUBSCRIPTION_COLS];
    bool nulls[PG_STAT_GET_SUBSCRIPTION_COLS];
    int worker_pid;
    LogicalRepWorker worker;

    memcpy(&worker, &LogicalRepCtx->workers[i], sizeof(LogicalRepWorker));
    if (!worker.proc || !IsBackendPid(worker.proc->pid))
    {
      continue;
    }

    if (OidIsValid(subid) && worker.subid != subid)
    {
      continue;
    }

    worker_pid = worker.proc->pid;

    MemSet(values, 0, sizeof(values));
    MemSet(nulls, 0, sizeof(nulls));

    values[0] = ObjectIdGetDatum(worker.subid);
    if (OidIsValid(worker.relid))
    {
      values[1] = ObjectIdGetDatum(worker.relid);
    }
    else
    {
      nulls[1] = true;
    }
    values[2] = Int32GetDatum(worker_pid);
    if (XLogRecPtrIsInvalid(worker.last_lsn))
    {
      nulls[3] = true;
    }
    else
    {
      values[3] = LSNGetDatum(worker.last_lsn);
    }
    if (worker.last_send_time == 0)
    {
      nulls[4] = true;
    }
    else
    {
      values[4] = TimestampTzGetDatum(worker.last_send_time);
    }
    if (worker.last_recv_time == 0)
    {
      nulls[5] = true;
    }
    else
    {
      values[5] = TimestampTzGetDatum(worker.last_recv_time);
    }
    if (XLogRecPtrIsInvalid(worker.reply_lsn))
    {
      nulls[6] = true;
    }
    else
    {
      values[6] = LSNGetDatum(worker.reply_lsn);
    }
    if (worker.reply_time == 0)
    {
      nulls[7] = true;
    }
    else
    {
      values[7] = TimestampTzGetDatum(worker.reply_time);
    }

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);

       
                                                                     
              
       
    if (OidIsValid(subid))
    {
      break;
    }
  }

  LWLockRelease(LogicalRepWorkerLock);

                                          
  tuplestore_donestoring(tupstore);

  return (Datum)0;
}
