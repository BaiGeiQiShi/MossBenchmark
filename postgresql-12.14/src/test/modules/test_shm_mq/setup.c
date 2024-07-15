                                                                             
   
           
                                                                    
                                                                 
             
   
                                                                
   
                  
                                         
   
                                                                             
   

#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/procsignal.h"
#include "storage/shm_toc.h"
#include "utils/memutils.h"

#include "test_shm_mq.h"

typedef struct
{
  int nworkers;
  BackgroundWorkerHandle *handle[FLEXIBLE_ARRAY_MEMBER];
} worker_state;

static void
setup_dynamic_shared_memory(int64 queue_size, int nworkers, dsm_segment **segp, test_shm_mq_header **hdrp, shm_mq **outp, shm_mq **inp);
static worker_state *
setup_background_workers(int nworkers, dsm_segment *seg);
static void
cleanup_background_workers(dsm_segment *seg, Datum arg);
static void
wait_for_workers_to_become_ready(worker_state *wstate, volatile test_shm_mq_header *hdr);
static bool
check_worker_status(worker_state *wstate);

   
                                                                              
                   
   
void
test_shm_mq_setup(int64 queue_size, int32 nworkers, dsm_segment **segp, shm_mq_handle **output, shm_mq_handle **input)
{
  dsm_segment *seg;
  test_shm_mq_header *hdr;
  shm_mq *outq = NULL;                       
  shm_mq *inq = NULL;                        
  worker_state *wstate;

                                               
  setup_dynamic_shared_memory(queue_size, nworkers, &seg, &hdr, &outq, &inq);
  *segp = seg;

                                    
  wstate = setup_background_workers(nworkers, seg);

                          
  *output = shm_mq_attach(outq, seg, wstate->handle[0]);
  *input = shm_mq_attach(inq, seg, wstate->handle[nworkers - 1]);

                                         
  wait_for_workers_to_become_ready(wstate, hdr);

     
                                                                            
                                                                         
                
     
  cancel_on_dsm_detach(seg, cleanup_background_workers, PointerGetDatum(wstate));
  pfree(wstate);
}

   
                                           
   
                                                                             
                                                                           
                                    
   
static void
setup_dynamic_shared_memory(int64 queue_size, int nworkers, dsm_segment **segp, test_shm_mq_header **hdrp, shm_mq **outp, shm_mq **inp)
{
  shm_toc_estimator e;
  int i;
  Size segsize;
  dsm_segment *seg;
  shm_toc *toc;
  test_shm_mq_header *hdr;

                                  
  if (queue_size < 0 || ((uint64)queue_size) < shm_mq_minimum_size)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("queue size must be at least %zu bytes", shm_mq_minimum_size)));
  }
  if (queue_size != ((Size)queue_size))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("queue size overflows size_t")));
  }

     
                                              
     
                                                                           
                                                       
     
                                                                         
                                                                     
     
  shm_toc_initialize_estimator(&e);
  shm_toc_estimate_chunk(&e, sizeof(test_shm_mq_header));
  for (i = 0; i <= nworkers; ++i)
  {
    shm_toc_estimate_chunk(&e, (Size)queue_size);
  }
  shm_toc_estimate_keys(&e, 2 + nworkers);
  segsize = shm_toc_estimate(&e);

                                                                           
  seg = dsm_create(shm_toc_estimate(&e), 0);
  toc = shm_toc_create(PG_TEST_SHM_MQ_MAGIC, dsm_segment_address(seg), segsize);

                                 
  hdr = shm_toc_allocate(toc, sizeof(test_shm_mq_header));
  SpinLockInit(&hdr->mutex);
  hdr->workers_total = nworkers;
  hdr->workers_attached = 0;
  hdr->workers_ready = 0;
  shm_toc_insert(toc, 0, hdr);

                                                      
  for (i = 0; i <= nworkers; ++i)
  {
    shm_mq *mq;

    mq = shm_mq_create(shm_toc_allocate(toc, (Size)queue_size), (Size)queue_size);
    shm_toc_insert(toc, i + 1, mq);

    if (i == 0)
    {
                                                
      shm_mq_set_sender(mq, MyProc);
      *outp = mq;
    }
    if (i == nworkers)
    {
                                                    
      shm_mq_set_receiver(mq, MyProc);
      *inp = mq;
    }
  }

                                 
  *segp = seg;
  *hdrp = hdr;
}

   
                                
   
static worker_state *
setup_background_workers(int nworkers, dsm_segment *seg)
{
  MemoryContext oldcontext;
  BackgroundWorker worker;
  worker_state *wstate;
  int i;

     
                                                                          
                                                                          
                                                                           
                
     
  oldcontext = MemoryContextSwitchTo(CurTransactionContext);

                                   
  wstate = MemoryContextAlloc(TopTransactionContext, offsetof(worker_state, handle) + sizeof(BackgroundWorkerHandle *) * nworkers);
  wstate->nworkers = 0;

     
                                                                        
                                                                          
     
                                                                            
                                                                           
                                                                          
                                                                        
                                                                        
                                                                       
                      
     
                                                                            
                                                                      
                                                                            
                                                                            
                                                          
     
  on_dsm_detach(seg, cleanup_background_workers, PointerGetDatum(wstate));

                           
  memset(&worker, 0, sizeof(worker));
  worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
  worker.bgw_start_time = BgWorkerStart_ConsistentState;
  worker.bgw_restart_time = BGW_NEVER_RESTART;
  sprintf(worker.bgw_library_name, "test_shm_mq");
  sprintf(worker.bgw_function_name, "test_shm_mq_main");
  snprintf(worker.bgw_type, BGW_MAXLEN, "test_shm_mq");
  worker.bgw_main_arg = UInt32GetDatum(dsm_segment_handle(seg));
                                                                
  worker.bgw_notify_pid = MyProcPid;

                             
  for (i = 0; i < nworkers; ++i)
  {
    if (!RegisterDynamicBackgroundWorker(&worker, &wstate->handle[i]))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("could not register background process"), errhint("You may need to increase max_worker_processes.")));
    }
    ++wstate->nworkers;
  }

                 
  MemoryContextSwitchTo(oldcontext);
  return wstate;
}

static void
cleanup_background_workers(dsm_segment *seg, Datum arg)
{
  worker_state *wstate = (worker_state *)DatumGetPointer(arg);

  while (wstate->nworkers > 0)
  {
    --wstate->nworkers;
    TerminateBackgroundWorker(wstate->handle[wstate->nworkers]);
  }
}

static void
wait_for_workers_to_become_ready(worker_state *wstate, volatile test_shm_mq_header *hdr)
{
  bool result = false;

  for (;;)
  {
    int workers_ready;

                                                          
    SpinLockAcquire(&hdr->mutex);
    workers_ready = hdr->workers_ready;
    SpinLockRelease(&hdr->mutex);
    if (workers_ready >= wstate->nworkers)
    {
      result = true;
      break;
    }

                                                                       
    if (!check_worker_status(wstate))
    {
      result = false;
      break;
    }

                               
    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, PG_WAIT_EXTENSION);

                                           
    ResetLatch(MyLatch);

                                                               
    CHECK_FOR_INTERRUPTS();
  }

  if (!result)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("one or more background workers failed to start")));
  }
}

static bool
check_worker_status(worker_state *wstate)
{
  int n;

                                                                     
  for (n = 0; n < wstate->nworkers; ++n)
  {
    BgwHandleStatus status;
    pid_t pid;

    status = GetBackgroundWorkerPid(wstate->handle[n], &pid);
    if (status == BGWH_STOPPED || status == BGWH_POSTMASTER_DIED)
    {
      return false;
    }
  }

                                        
  return true;
}
