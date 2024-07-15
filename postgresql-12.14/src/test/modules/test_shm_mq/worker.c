                                                                             
   
            
                                                                       
                                                                     
                                                              
                                                                  
                                                                
                                                                    
                       
   
                                                                
   
                  
                                          
   
                                                                             
   

#include "postgres.h"

#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/procarray.h"
#include "storage/shm_mq.h"
#include "storage/shm_toc.h"

#include "test_shm_mq.h"

static void handle_sigterm(SIGNAL_ARGS);
static void
attach_to_queues(dsm_segment *seg, shm_toc *toc, int myworkernumber, shm_mq_handle **inqhp, shm_mq_handle **outqhp);
static void
copy_messages(shm_mq_handle *inqh, shm_mq_handle *outqh);

   
                                 
   
                                                                          
                                                                        
                                                                       
                                                             
                                                                             
                                               
   
void
test_shm_mq_main(Datum main_arg)
{
  dsm_segment *seg;
  shm_toc *toc;
  shm_mq_handle *inqh;
  shm_mq_handle *outqh;
  volatile test_shm_mq_header *hdr;
  int myworkernumber;
  PGPROC *registrant;

     
                                
     
                                                                            
                                                                          
                                                              
     
  pqsignal(SIGTERM, handle_sigterm);
  BackgroundWorkerUnblockSignals();

     
                                                   
     
                                                                          
                                                                            
                                                                           
                                                                         
                              
     
                                                                        
                                                                           
                                                                           
                                                             
     
  seg = dsm_attach(DatumGetInt32(main_arg));
  if (seg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("unable to map dynamic shared memory segment")));
  }
  toc = shm_toc_attach(PG_TEST_SHM_MQ_MAGIC, dsm_segment_address(seg));
  if (toc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("bad magic number in dynamic shared memory segment")));
  }

     
                              
     
                                                                          
                                                                         
                                                                           
                                                                       
     
  hdr = shm_toc_lookup(toc, 0, false);
  SpinLockAcquire(&hdr->mutex);
  myworkernumber = ++hdr->workers_attached;
  SpinLockRelease(&hdr->mutex);
  if (myworkernumber > hdr->workers_total)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("too many message queue testing workers already")));
  }

     
                                               
     
  attach_to_queues(seg, toc, myworkernumber, &inqh, &outqh);

     
                                                                            
                                
     
                                                                             
                                                                          
                                                                             
                                                                            
     
  SpinLockAcquire(&hdr->mutex);
  ++hdr->workers_ready;
  SpinLockRelease(&hdr->mutex);
  registrant = BackendPidGetProc(MyBgworkerEntry->bgw_notify_pid);
  if (registrant == NULL)
  {
    elog(DEBUG1, "registrant backend has exited prematurely");
    proc_exit(1);
  }
  SetLatch(&registrant->procLatch);

                    
  copy_messages(inqh, outqh);

     
                                                                            
                                                                     
     
  dsm_detach(seg);
  proc_exit(1);
}

   
                                           
   
                                                                          
                                                                               
                                                                             
                                                                              
                                      
   
static void
attach_to_queues(dsm_segment *seg, shm_toc *toc, int myworkernumber, shm_mq_handle **inqhp, shm_mq_handle **outqhp)
{
  shm_mq *inq;
  shm_mq *outq;

  inq = shm_toc_lookup(toc, myworkernumber, false);
  shm_mq_set_receiver(inq, MyProc);
  *inqhp = shm_mq_attach(inq, seg, NULL);
  outq = shm_toc_lookup(toc, myworkernumber + 1, false);
  shm_mq_set_sender(outq, MyProc);
  *outqhp = shm_mq_attach(outq, seg, NULL);
}

   
                                                                         
   
                                                                              
                                                                                
                                
   
static void
copy_messages(shm_mq_handle *inqh, shm_mq_handle *outqh)
{
  Size len;
  void *data;
  shm_mq_result res;

  for (;;)
  {
                                                   
    CHECK_FOR_INTERRUPTS();

                            
    res = shm_mq_receive(inqh, &len, &data, false);
    if (res != SHM_MQ_SUCCESS)
    {
      break;
    }

                           
    res = shm_mq_send(outqh, len, data, false);
    if (res != SHM_MQ_SUCCESS)
    {
      break;
    }
  }
}

   
                                                                              
                                                                             
          
   
static void
handle_sigterm(SIGNAL_ARGS)
{
  int save_errno = errno;

  SetLatch(MyLatch);

  if (!proc_exit_inprogress)
  {
    InterruptPending = true;
    ProcDiePending = true;
  }

  errno = save_errno;
}
