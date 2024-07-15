                                                                            
   
              
                                                   
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/nbtree.h"
#include "access/parallel.h"
#include "access/session.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_enum.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "commands/async.h"
#include "executor/execParallel.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqmq.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "storage/ipc.h"
#include "storage/predicate.h"
#include "storage/sinval.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/combocid.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/typcache.h"

   
                                                                           
                                                                            
                                                                              
                                                                             
                                                                              
   
#define PARALLEL_ERROR_QUEUE_SIZE 16384

                                            
#define PARALLEL_MAGIC 0x50477c7c

   
                                                                            
                                                                            
           
   
#define PARALLEL_KEY_FIXED UINT64CONST(0xFFFFFFFFFFFF0001)
#define PARALLEL_KEY_ERROR_QUEUE UINT64CONST(0xFFFFFFFFFFFF0002)
#define PARALLEL_KEY_LIBRARY UINT64CONST(0xFFFFFFFFFFFF0003)
#define PARALLEL_KEY_GUC UINT64CONST(0xFFFFFFFFFFFF0004)
#define PARALLEL_KEY_COMBO_CID UINT64CONST(0xFFFFFFFFFFFF0005)
#define PARALLEL_KEY_TRANSACTION_SNAPSHOT UINT64CONST(0xFFFFFFFFFFFF0006)
#define PARALLEL_KEY_ACTIVE_SNAPSHOT UINT64CONST(0xFFFFFFFFFFFF0007)
#define PARALLEL_KEY_TRANSACTION_STATE UINT64CONST(0xFFFFFFFFFFFF0008)
#define PARALLEL_KEY_ENTRYPOINT UINT64CONST(0xFFFFFFFFFFFF0009)
#define PARALLEL_KEY_SESSION_DSM UINT64CONST(0xFFFFFFFFFFFF000A)
#define PARALLEL_KEY_REINDEX_STATE UINT64CONST(0xFFFFFFFFFFFF000B)
#define PARALLEL_KEY_RELMAPPER_STATE UINT64CONST(0xFFFFFFFFFFFF000C)
#define PARALLEL_KEY_ENUMBLACKLIST UINT64CONST(0xFFFFFFFFFFFF000D)

                                
typedef struct FixedParallelState
{
                                                   
  Oid database_id;
  Oid authenticated_user_id;
  Oid current_user_id;
  Oid outer_user_id;
  Oid temp_namespace_id;
  Oid temp_toast_namespace_id;
  int sec_context;
  bool is_superuser;
  PGPROC *parallel_master_pgproc;
  pid_t parallel_master_pid;
  BackendId parallel_master_backend_id;
  TimestampTz xact_ts;
  TimestampTz stmt_ts;
  SerializableXactHandle serializable_xact_handle;

                                        
  slock_t mutex;

                                             
  XLogRecPtr last_xlog_end;
} FixedParallelState;

   
                                                                              
                                                                               
                                                                              
                                                       
   
int ParallelWorkerNumber = -1;

                                                                   
volatile bool ParallelMessagePending = false;

                                            
bool InitializingParallelWorker = false;

                                          
static FixedParallelState *MyFixedParallelState;

                                       
static dlist_head pcxt_list = DLIST_STATIC_INIT(pcxt_list);

                                                         
static pid_t ParallelMasterPid;

   
                                                                    
                                                               
   
static const struct
{
  const char *fn_name;
  parallel_worker_main_type fn_addr;
} InternalParallelWorkers[] =

    {{"ParallelQueryMain", ParallelQueryMain}, {"_bt_parallel_build_main", _bt_parallel_build_main}};

                        
static void
HandleParallelMessage(ParallelContext *pcxt, int i, StringInfo msg);
static void
WaitForParallelWorkersToExit(ParallelContext *pcxt);
static parallel_worker_main_type
LookupParallelWorkerFunction(const char *libraryname, const char *funcname);
static void
ParallelWorkerShutdown(int code, Datum arg);

   
                                                                         
                                                                       
                                                        
   
ParallelContext *
CreateParallelContext(const char *library_name, const char *function_name, int nworkers)
{
  MemoryContext oldcontext;
  ParallelContext *pcxt;

                                                                          
  Assert(IsInParallelMode());

                                                 
  Assert(nworkers >= 0);

                                                            
  oldcontext = MemoryContextSwitchTo(TopTransactionContext);

                                         
  pcxt = palloc0(sizeof(ParallelContext));
  pcxt->subid = GetCurrentSubTransactionId();
  pcxt->nworkers = nworkers;
  pcxt->library_name = pstrdup(library_name);
  pcxt->function_name = pstrdup(function_name);
  pcxt->error_context_stack = error_context_stack;
  shm_toc_initialize_estimator(&pcxt->estimator);
  dlist_push_head(&pcxt_list, &pcxt->node);

                                        
  MemoryContextSwitchTo(oldcontext);

  return pcxt;
}

   
                                                                          
                                                                       
                             
   
void
InitializeParallelDSM(ParallelContext *pcxt)
{
  MemoryContext oldcontext;
  Size library_len = 0;
  Size guc_len = 0;
  Size combocidlen = 0;
  Size tsnaplen = 0;
  Size asnaplen = 0;
  Size tstatelen = 0;
  Size reindexlen = 0;
  Size relmapperlen = 0;
  Size enumblacklistlen = 0;
  Size segsize = 0;
  int i;
  FixedParallelState *fps;
  dsm_handle session_dsm_handle = DSM_HANDLE_INVALID;
  Snapshot transaction_snapshot = GetTransactionSnapshot();
  Snapshot active_snapshot = GetActiveSnapshot();

                                                                 
  oldcontext = MemoryContextSwitchTo(TopTransactionContext);

                                                           
  shm_toc_estimate_chunk(&pcxt->estimator, sizeof(FixedParallelState));
  shm_toc_estimate_keys(&pcxt->estimator, 1);

     
                                                                             
                                                                     
     
  if (pcxt->nworkers > 0)
  {
                                                               
    session_dsm_handle = GetSessionDsmHandle();

       
                                                                           
                                                                     
                                                                      
               
       
    if (session_dsm_handle == DSM_HANDLE_INVALID)
    {
      pcxt->nworkers = 0;
    }
  }

  if (pcxt->nworkers > 0)
  {
                                                            
    library_len = EstimateLibraryStateSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, library_len);
    guc_len = EstimateGUCStateSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, guc_len);
    combocidlen = EstimateComboCIDStateSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, combocidlen);
    if (IsolationUsesXactSnapshot())
    {
      tsnaplen = EstimateSnapshotSpace(transaction_snapshot);
      shm_toc_estimate_chunk(&pcxt->estimator, tsnaplen);
    }
    asnaplen = EstimateSnapshotSpace(active_snapshot);
    shm_toc_estimate_chunk(&pcxt->estimator, asnaplen);
    tstatelen = EstimateTransactionStateSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, tstatelen);
    shm_toc_estimate_chunk(&pcxt->estimator, sizeof(dsm_handle));
    reindexlen = EstimateReindexStateSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, reindexlen);
    relmapperlen = EstimateRelationMapSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, relmapperlen);
    enumblacklistlen = EstimateEnumBlacklistSpace();
    shm_toc_estimate_chunk(&pcxt->estimator, enumblacklistlen);
                                                                     
    shm_toc_estimate_keys(&pcxt->estimator, 10);

                                               
    StaticAssertStmt(BUFFERALIGN(PARALLEL_ERROR_QUEUE_SIZE) == PARALLEL_ERROR_QUEUE_SIZE, "parallel error queue size not buffer-aligned");
    shm_toc_estimate_chunk(&pcxt->estimator, mul_size(PARALLEL_ERROR_QUEUE_SIZE, pcxt->nworkers));
    shm_toc_estimate_keys(&pcxt->estimator, 1);

                                                               
    shm_toc_estimate_chunk(&pcxt->estimator, strlen(pcxt->library_name) + strlen(pcxt->function_name) + 2);
    shm_toc_estimate_keys(&pcxt->estimator, 1);
  }

     
                                                                            
                                                                             
                                                               
     
                                                                          
                                                                             
                                                                            
                                                                    
                                        
     
  segsize = shm_toc_estimate(&pcxt->estimator);
  if (pcxt->nworkers > 0)
  {
    pcxt->seg = dsm_create(segsize, DSM_CREATE_NULL_IF_MAXSEGMENTS);
  }
  if (pcxt->seg != NULL)
  {
    pcxt->toc = shm_toc_create(PARALLEL_MAGIC, dsm_segment_address(pcxt->seg), segsize);
  }
  else
  {
    pcxt->nworkers = 0;
    pcxt->private_memory = MemoryContextAlloc(TopMemoryContext, segsize);
    pcxt->toc = shm_toc_create(PARALLEL_MAGIC, pcxt->private_memory, segsize);
  }

                                                     
  fps = (FixedParallelState *)shm_toc_allocate(pcxt->toc, sizeof(FixedParallelState));
  fps->database_id = MyDatabaseId;
  fps->authenticated_user_id = GetAuthenticatedUserId();
  fps->outer_user_id = GetCurrentRoleId();
  fps->is_superuser = session_auth_is_superuser;
  GetUserIdAndSecContext(&fps->current_user_id, &fps->sec_context);
  GetTempNamespaceState(&fps->temp_namespace_id, &fps->temp_toast_namespace_id);
  fps->parallel_master_pgproc = MyProc;
  fps->parallel_master_pid = MyProcPid;
  fps->parallel_master_backend_id = MyBackendId;
  fps->xact_ts = GetCurrentTransactionStartTimestamp();
  fps->stmt_ts = GetCurrentStatementStartTimestamp();
  fps->serializable_xact_handle = ShareSerializableXact();
  SpinLockInit(&fps->mutex);
  fps->last_xlog_end = 0;
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_FIXED, fps);

                                                                            
  if (pcxt->nworkers > 0)
  {
    char *libraryspace;
    char *gucspace;
    char *combocidspace;
    char *tsnapspace;
    char *asnapspace;
    char *tstatespace;
    char *reindexspace;
    char *relmapperspace;
    char *error_queue_space;
    char *session_dsm_handle_space;
    char *entrypointstate;
    char *enumblacklistspace;
    Size lnamelen;

                                                    
    libraryspace = shm_toc_allocate(pcxt->toc, library_len);
    SerializeLibraryState(library_len, libraryspace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_LIBRARY, libraryspace);

                                 
    gucspace = shm_toc_allocate(pcxt->toc, guc_len);
    SerializeGUCState(guc_len, gucspace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_GUC, gucspace);

                                    
    combocidspace = shm_toc_allocate(pcxt->toc, combocidlen);
    SerializeComboCIDState(combocidlen, combocidspace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_COMBO_CID, combocidspace);

       
                                                             
                                                    
       
    if (IsolationUsesXactSnapshot())
    {
      tsnapspace = shm_toc_allocate(pcxt->toc, tsnaplen);
      SerializeSnapshot(transaction_snapshot, tsnapspace);
      shm_toc_insert(pcxt->toc, PARALLEL_KEY_TRANSACTION_SNAPSHOT, tsnapspace);
    }

                                        
    asnapspace = shm_toc_allocate(pcxt->toc, asnaplen);
    SerializeSnapshot(active_snapshot, asnapspace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_ACTIVE_SNAPSHOT, asnapspace);

                                                     
    session_dsm_handle_space = shm_toc_allocate(pcxt->toc, sizeof(dsm_handle));
    *(dsm_handle *)session_dsm_handle_space = session_dsm_handle;
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_SESSION_DSM, session_dsm_handle_space);

                                      
    tstatespace = shm_toc_allocate(pcxt->toc, tstatelen);
    SerializeTransactionState(tstatelen, tstatespace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_TRANSACTION_STATE, tstatespace);

                                  
    reindexspace = shm_toc_allocate(pcxt->toc, reindexlen);
    SerializeReindexState(reindexlen, reindexspace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_REINDEX_STATE, reindexspace);

                                    
    relmapperspace = shm_toc_allocate(pcxt->toc, relmapperlen);
    SerializeRelationMap(relmapperlen, relmapperspace);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_RELMAPPER_STATE, relmapperspace);

                                         
    enumblacklistspace = shm_toc_allocate(pcxt->toc, enumblacklistlen);
    SerializeEnumBlacklist(enumblacklistspace, enumblacklistlen);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_ENUMBLACKLIST, enumblacklistspace);

                                                
    pcxt->worker = palloc0(sizeof(ParallelWorkerInfo) * pcxt->nworkers);

       
                                                        
       
                                                                        
                                                                         
                                                                     
       
    error_queue_space = shm_toc_allocate(pcxt->toc, mul_size(PARALLEL_ERROR_QUEUE_SIZE, pcxt->nworkers));
    for (i = 0; i < pcxt->nworkers; ++i)
    {
      char *start;
      shm_mq *mq;

      start = error_queue_space + i * PARALLEL_ERROR_QUEUE_SIZE;
      mq = shm_mq_create(start, PARALLEL_ERROR_QUEUE_SIZE);
      shm_mq_set_receiver(mq, MyProc);
      pcxt->worker[i].error_mqh = shm_mq_attach(mq, pcxt->seg, NULL);
    }
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_ERROR_QUEUE, error_queue_space);

       
                                                                       
                                                                           
                                                                         
                                                                         
                             
       
    lnamelen = strlen(pcxt->library_name);
    entrypointstate = shm_toc_allocate(pcxt->toc, lnamelen + strlen(pcxt->function_name) + 2);
    strcpy(entrypointstate, pcxt->library_name);
    strcpy(entrypointstate + lnamelen + 1, pcxt->function_name);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_ENTRYPOINT, entrypointstate);
  }

                                        
  MemoryContextSwitchTo(oldcontext);
}

   
                                                                              
                                              
   
void
ReinitializeParallelDSM(ParallelContext *pcxt)
{
  FixedParallelState *fps;

                                         
  if (pcxt->nworkers_launched > 0)
  {
    WaitForParallelWorkersToFinish(pcxt);
    WaitForParallelWorkersToExit(pcxt);
    pcxt->nworkers_launched = 0;
    if (pcxt->known_attached_workers)
    {
      pfree(pcxt->known_attached_workers);
      pcxt->known_attached_workers = NULL;
      pcxt->nknown_attached_workers = 0;
    }
  }

                                                                  
  fps = shm_toc_lookup(pcxt->toc, PARALLEL_KEY_FIXED, false);
  fps->last_xlog_end = 0;

                                              
  if (pcxt->nworkers > 0)
  {
    char *error_queue_space;
    int i;

    error_queue_space = shm_toc_lookup(pcxt->toc, PARALLEL_KEY_ERROR_QUEUE, false);
    for (i = 0; i < pcxt->nworkers; ++i)
    {
      char *start;
      shm_mq *mq;

      start = error_queue_space + i * PARALLEL_ERROR_QUEUE_SIZE;
      mq = shm_mq_create(start, PARALLEL_ERROR_QUEUE_SIZE);
      shm_mq_set_receiver(mq, MyProc);
      pcxt->worker[i].error_mqh = shm_mq_attach(mq, pcxt->seg, NULL);
    }
  }
}

   
                            
   
void
LaunchParallelWorkers(ParallelContext *pcxt)
{
  MemoryContext oldcontext;
  BackgroundWorker worker;
  int i;
  bool any_registrations_failed = false;

                                        
  if (pcxt->nworkers == 0)
  {
    return;
  }

                                          
  BecomeLockGroupLeader();

                                                              
  Assert(pcxt->seg != NULL);

                                                            
  oldcontext = MemoryContextSwitchTo(TopTransactionContext);

                           
  memset(&worker, 0, sizeof(worker));
  snprintf(worker.bgw_name, BGW_MAXLEN, "parallel worker for PID %d", MyProcPid);
  snprintf(worker.bgw_type, BGW_MAXLEN, "parallel worker");
  worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION | BGWORKER_CLASS_PARALLEL;
  worker.bgw_start_time = BgWorkerStart_ConsistentState;
  worker.bgw_restart_time = BGW_NEVER_RESTART;
  sprintf(worker.bgw_library_name, "postgres");
  sprintf(worker.bgw_function_name, "ParallelWorkerMain");
  worker.bgw_main_arg = UInt32GetDatum(dsm_segment_handle(pcxt->seg));
  worker.bgw_notify_pid = MyProcPid;

     
                    
     
                                                                           
                                                                          
                                                                             
                                                                          
     
  for (i = 0; i < pcxt->nworkers; ++i)
  {
    memcpy(worker.bgw_extra, &i, sizeof(int));
    if (!any_registrations_failed && RegisterDynamicBackgroundWorker(&worker, &pcxt->worker[i].bgwhandle))
    {
      shm_mq_set_handle(pcxt->worker[i].error_mqh, pcxt->worker[i].bgwhandle);
      pcxt->nworkers_launched++;
    }
    else
    {
         
                                                                         
                                                            
                                                                        
                                                                        
                                                                        
                                                                      
                              
         
      any_registrations_failed = true;
      pcxt->worker[i].bgwhandle = NULL;
      shm_mq_detach(pcxt->worker[i].error_mqh);
      pcxt->worker[i].error_mqh = NULL;
    }
  }

     
                                                                             
                             
     
  if (pcxt->nworkers_launched > 0)
  {
    pcxt->known_attached_workers = palloc0(sizeof(bool) * pcxt->nworkers_launched);
    pcxt->nknown_attached_workers = 0;
  }

                                        
  MemoryContextSwitchTo(oldcontext);
}

   
                                                                               
                                
   
                                                                           
                                                                           
                                                                                
                                                                          
                                                                            
                                                                              
                                                                       
   
                                                                               
                                                                                
                                                                             
                                                                            
                                                                                
                                                    
                                                                            
                                                 
   
                                                                            
                                                                            
                                                                            
                                                                         
                                                                         
                                                                            
                                                                          
                                                                           
                                                                              
                              
   
void
WaitForParallelWorkersToAttach(ParallelContext *pcxt)
{
  int i;

                                                 
  if (pcxt->nworkers_launched == 0)
  {
    return;
  }

  for (;;)
  {
       
                                                                           
                                                     
       
    CHECK_FOR_INTERRUPTS();

    for (i = 0; i < pcxt->nworkers_launched; ++i)
    {
      BgwHandleStatus status;
      shm_mq *mq;
      int rc;
      pid_t pid;

      if (pcxt->known_attached_workers[i])
      {
        continue;
      }

         
                                                                  
                  
         
      if (pcxt->worker[i].error_mqh == NULL)
      {
        pcxt->known_attached_workers[i] = true;
        ++pcxt->nknown_attached_workers;
        continue;
      }

      status = GetBackgroundWorkerPid(pcxt->worker[i].bgwhandle, &pid);
      if (status == BGWH_STARTED)
      {
                                                         
        mq = shm_mq_get_queue(pcxt->worker[i].error_mqh);
        if (shm_mq_get_sender(mq) != NULL)
        {
                                                   
          pcxt->known_attached_workers[i] = true;
          ++pcxt->nknown_attached_workers;
        }
      }
      else if (status == BGWH_STOPPED)
      {
           
                                                                       
                           
           
        mq = shm_mq_get_queue(pcxt->worker[i].error_mqh);
        if (shm_mq_get_sender(mq) == NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("parallel worker failed to initialize"), errhint("More details may be available in the server log.")));
        }

        pcxt->known_attached_workers[i] = true;
        ++pcxt->nknown_attached_workers;
      }
      else
      {
           
                                                                    
                                                                    
                                                                     
                                                          
           
        rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, -1, WAIT_EVENT_BGWORKER_STARTUP);

        if (rc & WL_LATCH_SET)
        {
          ResetLatch(MyLatch);
        }
      }
    }

                                                               
    if (pcxt->nknown_attached_workers >= pcxt->nworkers_launched)
    {
      Assert(pcxt->nknown_attached_workers == pcxt->nworkers_launched);
      break;
    }
  }
}

   
                                             
   
                                                                             
                                                                            
                                                                               
                                      
   
                                                                        
             
   
void
WaitForParallelWorkersToFinish(ParallelContext *pcxt)
{
  for (;;)
  {
    bool anyone_alive = false;
    int nfinished = 0;
    int i;

       
                                                                           
                                                                          
                                       
       
    CHECK_FOR_INTERRUPTS();

    for (i = 0; i < pcxt->nworkers_launched; ++i)
    {
         
                                                                  
                                                                        
                                                                        
                                               
         
      if (pcxt->worker[i].error_mqh == NULL)
      {
        ++nfinished;
      }
      else if (pcxt->known_attached_workers[i])
      {
        anyone_alive = true;
        break;
      }
    }

    if (!anyone_alive)
    {
                                                                  
      if (nfinished >= pcxt->nworkers_launched)
      {
        Assert(nfinished == pcxt->nworkers_launched);
        break;
      }

         
                                                                      
                                                                    
                                                                
                                
         
      for (i = 0; i < pcxt->nworkers_launched; ++i)
      {
        pid_t pid;
        shm_mq *mq;

           
                                                                     
                                                                  
                                            
           
        if (pcxt->worker[i].error_mqh == NULL || pcxt->worker[i].bgwhandle == NULL || GetBackgroundWorkerPid(pcxt->worker[i].bgwhandle, &pid) != BGWH_STOPPED)
        {
          continue;
        }

           
                                                                  
                                                                    
                                                                       
                                                                   
                                                                 
                    
           
        mq = shm_mq_get_queue(pcxt->worker[i].error_mqh);
        if (shm_mq_get_sender(mq) == NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("parallel worker failed to initialize"), errhint("More details may be available in the server log.")));
        }

           
                                                                      
                                                                      
                                                               
                                                                    
                                                                       
                                                                      
                                                          
           
      }
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, -1, WAIT_EVENT_PARALLEL_FINISH);
    ResetLatch(MyLatch);
  }

  if (pcxt->toc != NULL)
  {
    FixedParallelState *fps;

    fps = shm_toc_lookup(pcxt->toc, PARALLEL_KEY_FIXED, false);
    if (fps->last_xlog_end > XactLastRecEnd)
    {
      XactLastRecEnd = fps->last_xlog_end;
    }
  }
}

   
                                 
   
                                                                          
                                                                          
                                                                        
                                                                          
   
static void
WaitForParallelWorkersToExit(ParallelContext *pcxt)
{
  int i;

                                            
  for (i = 0; i < pcxt->nworkers_launched; ++i)
  {
    BgwHandleStatus status;

    if (pcxt->worker == NULL || pcxt->worker[i].bgwhandle == NULL)
    {
      continue;
    }

    status = WaitForBackgroundWorkerShutdown(pcxt->worker[i].bgwhandle);

       
                                                                          
                                                                           
                                                                         
                                                              
       
    if (status == BGWH_POSTMASTER_DIED)
    {
      ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("postmaster exited during a parallel transaction")));
    }

                         
    pfree(pcxt->worker[i].bgwhandle);
    pcxt->worker[i].bgwhandle = NULL;
  }
}

   
                               
   
                                                                              
                                                                            
                                                                            
                                                                            
   
void
DestroyParallelContext(ParallelContext *pcxt)
{
  int i;

     
                                                                        
                                                                        
                                                                          
                                                     
     
  dlist_delete(&pcxt->node);

                                                                
  if (pcxt->worker != NULL)
  {
    for (i = 0; i < pcxt->nworkers_launched; ++i)
    {
      if (pcxt->worker[i].error_mqh != NULL)
      {
        TerminateBackgroundWorker(pcxt->worker[i].bgwhandle);

        shm_mq_detach(pcxt->worker[i].error_mqh);
        pcxt->worker[i].error_mqh = NULL;
      }
    }
  }

     
                                                                         
                                                                             
                   
     
  if (pcxt->seg != NULL)
  {
    dsm_detach(pcxt->seg);
    pcxt->seg = NULL;
  }

     
                                                                           
                                                   
     
  if (pcxt->private_memory != NULL)
  {
    pfree(pcxt->private_memory);
    pcxt->private_memory = NULL;
  }

     
                                                                          
                                                                       
                               
     
  HOLD_INTERRUPTS();
  WaitForParallelWorkersToExit(pcxt);
  RESUME_INTERRUPTS();

                                     
  if (pcxt->worker != NULL)
  {
    pfree(pcxt->worker);
    pcxt->worker = NULL;
  }

                    
  pfree(pcxt->library_name);
  pfree(pcxt->function_name);
  pfree(pcxt);
}

   
                                                     
   
bool
ParallelContextActive(void)
{
  return !dlist_is_empty(&pcxt_list);
}

   
                                                                        
   
                                                                       
                                                                    
                             
   
void
HandleParallelMessageInterrupt(void)
{
  InterruptPending = true;
  ParallelMessagePending = true;
  SetLatch(MyLatch);
}

   
                                                                       
   
void
HandleParallelMessages(void)
{
  dlist_iter iter;
  MemoryContext oldcontext;

  static MemoryContext hpm_context = NULL;

     
                                                                     
                                                                             
                                                                             
                                                                            
                                                               
     
  HOLD_INTERRUPTS();

     
                                                                           
                                                                           
                                                                       
     
  if (hpm_context == NULL)                          
  {
    hpm_context = AllocSetContextCreate(TopMemoryContext, "HandleParallelMessages", ALLOCSET_DEFAULT_SIZES);
  }
  else
  {
    MemoryContextReset(hpm_context);
  }

  oldcontext = MemoryContextSwitchTo(hpm_context);

                                                                            
  ParallelMessagePending = false;

  dlist_foreach(iter, &pcxt_list)
  {
    ParallelContext *pcxt;
    int i;

    pcxt = dlist_container(ParallelContext, node, iter.cur);
    if (pcxt->worker == NULL)
    {
      continue;
    }

    for (i = 0; i < pcxt->nworkers_launched; ++i)
    {
         
                                                                         
                                                                         
                                                                      
                                                                     
         
      while (pcxt->worker[i].error_mqh != NULL)
      {
        shm_mq_result res;
        Size nbytes;
        void *data;

        res = shm_mq_receive(pcxt->worker[i].error_mqh, &nbytes, &data, true);
        if (res == SHM_MQ_WOULD_BLOCK)
        {
          break;
        }
        else if (res == SHM_MQ_SUCCESS)
        {
          StringInfoData msg;

          initStringInfo(&msg);
          appendBinaryStringInfo(&msg, data, nbytes);
          HandleParallelMessage(pcxt, i, &msg);
          pfree(msg.data);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("lost connection to parallel worker")));
        }
      }
    }
  }

  MemoryContextSwitchTo(oldcontext);

                                                      
  MemoryContextReset(hpm_context);

  RESUME_INTERRUPTS();
}

   
                                                                            
   
static void
HandleParallelMessage(ParallelContext *pcxt, int i, StringInfo msg)
{
  char msgtype;

  if (pcxt->known_attached_workers != NULL && !pcxt->known_attached_workers[i])
  {
    pcxt->known_attached_workers[i] = true;
    pcxt->nknown_attached_workers++;
  }

  msgtype = pq_getmsgbyte(msg);

  switch (msgtype)
  {
  case 'K':                     
  {
    int32 pid = pq_getmsgint(msg, 4);

    (void)pq_getmsgint(msg, 4);                         
    (void)pq_getmsgend(msg);
    pcxt->worker[i].pid = pid;
    break;
  }

  case 'E':                    
  case 'N':                     
  {
    ErrorData edata;
    ErrorContextCallback *save_error_context_stack;

                                                
    pq_parse_errornotice(msg, &edata);

                                                                   
    edata.elevel = Min(edata.elevel, ERROR);

       
                                                             
                                                                 
                                                              
                                                                   
                                                              
                                                           
       
    if (force_parallel_mode != FORCE_PARALLEL_REGRESS)
    {
      if (edata.context)
      {
        edata.context = psprintf("%s\n%s", edata.context, _("parallel worker"));
      }
      else
      {
        edata.context = pstrdup(_("parallel worker"));
      }
    }

       
                                                                  
                                                                 
                             
       
    save_error_context_stack = error_context_stack;
    error_context_stack = pcxt->error_context_stack;

                                        
    ThrowErrorData(&edata);

                                                          
    error_context_stack = save_error_context_stack;

    break;
  }

  case 'A':                     
  {
                                   
    int32 pid;
    const char *channel;
    const char *payload;

    pid = pq_getmsgint(msg, 4);
    channel = pq_getmsgrawstring(msg);
    payload = pq_getmsgrawstring(msg);
    pq_endmessage(msg);

    NotifyMyFrontEnd(channel, payload, pid);

    break;
  }

  case 'X':                                       
  {
    shm_mq_detach(pcxt->worker[i].error_mqh);
    pcxt->worker[i].error_mqh = NULL;
    break;
  }

  default:
  {
    elog(ERROR, "unrecognized message type received from parallel worker: %c (message length %d bytes)", msgtype, msg->len);
  }
  }
}

   
                                                        
   
                                                                      
                                                                           
                                                                     
                                                                            
   
void
AtEOSubXact_Parallel(bool isCommit, SubTransactionId mySubId)
{
  while (!dlist_is_empty(&pcxt_list))
  {
    ParallelContext *pcxt;

    pcxt = dlist_head_element(ParallelContext, node, &pcxt_list);
    if (pcxt->subid != mySubId)
    {
      break;
    }
    if (isCommit)
    {
      elog(WARNING, "leaked parallel context");
    }
    DestroyParallelContext(pcxt);
  }
}

   
                                                     
   
void
AtEOXact_Parallel(bool isCommit)
{
  while (!dlist_is_empty(&pcxt_list))
  {
    ParallelContext *pcxt;

    pcxt = dlist_head_element(ParallelContext, node, &pcxt_list);
    if (isCommit)
    {
      elog(WARNING, "leaked parallel context");
    }
    DestroyParallelContext(pcxt);
  }
}

   
                                         
   
void
ParallelWorkerMain(Datum main_arg)
{
  dsm_segment *seg;
  shm_toc *toc;
  FixedParallelState *fps;
  char *error_queue_space;
  shm_mq *mq;
  shm_mq_handle *mqh;
  char *libraryspace;
  char *entrypointstate;
  char *library_name;
  char *function_name;
  parallel_worker_main_type entrypt;
  char *gucspace;
  char *combocidspace;
  char *tsnapspace;
  char *asnapspace;
  char *tstatespace;
  char *reindexspace;
  char *relmapperspace;
  char *enumblacklistspace;
  StringInfoData msgbuf;
  char *session_dsm_handle_space;
  Snapshot tsnapshot;
  Snapshot asnapshot;

                                                                       
  InitializingParallelWorker = true;

                                  
  pqsignal(SIGTERM, die);
  BackgroundWorkerUnblockSignals();

                                                     
  Assert(ParallelWorkerNumber == -1);
  memcpy(&ParallelWorkerNumber, MyBgworkerEntry->bgw_extra, sizeof(int));

                                                                 
  CurrentMemoryContext = AllocSetContextCreate(TopMemoryContext, "Parallel worker", ALLOCSET_DEFAULT_SIZES);

     
                                                                             
                                 
     
                                                                        
                                                                           
                                                                           
                                                             
     
  seg = dsm_attach(DatumGetUInt32(main_arg));
  if (seg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not map dynamic shared memory segment")));
  }
  toc = shm_toc_attach(PARALLEL_MAGIC, dsm_segment_address(seg));
  if (toc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid magic number in dynamic shared memory segment")));
  }

                                     
  fps = shm_toc_lookup(toc, PARALLEL_KEY_FIXED, false);
  MyFixedParallelState = fps;

                                                
  ParallelMasterPid = fps->parallel_master_pid;
  ParallelMasterBackendId = fps->parallel_master_backend_id;
  on_shmem_exit(ParallelWorkerShutdown, (Datum)0);

     
                                                                            
                                                                             
                                                                     
               
     
  error_queue_space = shm_toc_lookup(toc, PARALLEL_KEY_ERROR_QUEUE, false);
  mq = (shm_mq *)(error_queue_space + ParallelWorkerNumber * PARALLEL_ERROR_QUEUE_SIZE);
  shm_mq_set_sender(mq, MyProc);
  mqh = shm_mq_attach(mq, seg, NULL);
  pq_redirect_to_shm_mq(seg, mqh);
  pq_set_parallel_master(fps->parallel_master_pid, fps->parallel_master_backend_id);

     
                                                                             
                                                                            
                                                                     
                                                                             
                   
     
  pq_beginmessage(&msgbuf, 'K');
  pq_sendint32(&msgbuf, (int32)MyProcPid);
  pq_sendint32(&msgbuf, (int32)MyCancelKey);
  pq_endmessage(&msgbuf);

     
                                                                             
                                                        
     

     
                                                                            
                                                                           
                                                                       
                                                                        
                                                                     
                                                                            
                            
     
  if (!BecomeLockGroupMember(fps->parallel_master_pgproc, fps->parallel_master_pid))
  {
    return;
  }

     
                                                                         
                                                                            
                       
     
  SetParallelStartTimestamps(fps->xact_ts, fps->stmt_ts);

     
                                                                            
                                                                             
                                                      
     
  entrypointstate = shm_toc_lookup(toc, PARALLEL_KEY_ENTRYPOINT, false);
  library_name = entrypointstate;
  function_name = entrypointstate + strlen(library_name) + 1;

  entrypt = LookupParallelWorkerFunction(library_name, function_name);

                                    
  BackgroundWorkerInitializeConnectionByOid(fps->database_id, fps->authenticated_user_id, 0);

     
                                                                          
                             
     
  SetClientEncoding(GetDatabaseEncoding());

     
                                                                         
                                                                           
                
     
  libraryspace = shm_toc_lookup(toc, PARALLEL_KEY_LIBRARY, false);
  StartTransactionCommand();
  RestoreLibraryState(libraryspace);

                                                  
  gucspace = shm_toc_lookup(toc, PARALLEL_KEY_GUC, false);
  RestoreGUCState(gucspace);
  CommitTransactionCommand();

                                                                      
  tstatespace = shm_toc_lookup(toc, PARALLEL_KEY_TRANSACTION_STATE, false);
  StartParallelWorkerTransaction(tstatespace);

                                
  combocidspace = shm_toc_lookup(toc, PARALLEL_KEY_COMBO_CID, false);
  RestoreComboCIDState(combocidspace);

                                                                    
  session_dsm_handle_space = shm_toc_lookup(toc, PARALLEL_KEY_SESSION_DSM, false);
  AttachSession(*(dsm_handle *)session_dsm_handle_space);

     
                                                                            
                                                                            
                                                                     
                                                                          
                                                                           
                                                                       
                                                                       
                                                                           
                                                                          
                                              
     
  asnapspace = shm_toc_lookup(toc, PARALLEL_KEY_ACTIVE_SNAPSHOT, false);
  tsnapspace = shm_toc_lookup(toc, PARALLEL_KEY_TRANSACTION_SNAPSHOT, true);
  asnapshot = RestoreSnapshot(asnapspace);
  tsnapshot = tsnapspace ? RestoreSnapshot(tsnapspace) : asnapshot;
  RestoreTransactionSnapshot(tsnapshot, fps->parallel_master_pgproc);
  PushActiveSnapshot(asnapshot);

     
                                                                          
                    
     
  InvalidateSystemCaches();

     
                                                                      
                                                                            
                   
     
  SetCurrentRoleId(fps->outer_user_id, fps->is_superuser);

                                             
  SetUserIdAndSecContext(fps->current_user_id, fps->sec_context);

                                                                            
  SetTempNamespaceState(fps->temp_namespace_id, fps->temp_toast_namespace_id);

                              
  reindexspace = shm_toc_lookup(toc, PARALLEL_KEY_REINDEX_STATE, false);
  RestoreReindexState(reindexspace);

                                
  relmapperspace = shm_toc_lookup(toc, PARALLEL_KEY_RELMAPPER_STATE, false);
  RestoreRelationMap(relmapperspace);

                               
  enumblacklistspace = shm_toc_lookup(toc, PARALLEL_KEY_ENUMBLACKLIST, false);
  RestoreEnumBlacklist(enumblacklistspace);

                                                                         
  AttachSerializableXact(fps->serializable_xact_handle);

     
                                                                   
                
     
  InitializingParallelWorker = false;
  EnterParallelMode();

     
                                                                
     
  entrypt(seg, toc);

                                                       
  ExitParallelMode();

                                                               
  PopActiveSnapshot();

                                                  
  EndParallelWorkerTransaction();

                                                
  DetachSession();

                       
  pq_putmessage('X', NULL, 0);
}

   
                                                                           
                                                               
   
void
ParallelWorkerReportLastRecEnd(XLogRecPtr last_xlog_end)
{
  FixedParallelState *fps = MyFixedParallelState;

  Assert(fps != NULL);
  SpinLockAcquire(&fps->mutex);
  if (fps->last_xlog_end < last_xlog_end)
  {
    fps->last_xlog_end = last_xlog_end;
  }
  SpinLockRelease(&fps->mutex);
}

   
                                                                          
                                                                           
                                                                              
             
   
static void
ParallelWorkerShutdown(int code, Datum arg)
{
  SendProcSignal(ParallelMasterPid, PROCSIG_PARALLEL_MESSAGE, ParallelMasterBackendId);
}

   
                                                                       
   
                                                                            
                                                                          
                                                                       
   
                                                                         
                                                                       
                                                                        
                                                                        
                                                                             
                                                    
   
                                                                                
                                                                         
                                                                         
   
static parallel_worker_main_type
LookupParallelWorkerFunction(const char *libraryname, const char *funcname)
{
     
                                                                      
                                    
     
  if (strcmp(libraryname, "postgres") == 0)
  {
    int i;

    for (i = 0; i < lengthof(InternalParallelWorkers); i++)
    {
      if (strcmp(InternalParallelWorkers[i].fn_name, funcname) == 0)
      {
        return InternalParallelWorkers[i].fn_addr;
      }
    }

                                                      
    elog(ERROR, "internal function \"%s\" not found", funcname);
  }

                                             
  return (parallel_worker_main_type)load_external_function(libraryname, funcname, true, NULL);
}
