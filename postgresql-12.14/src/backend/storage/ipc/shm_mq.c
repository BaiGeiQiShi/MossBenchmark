                                                                            
   
            
                                                              
   
                                                                         
                                                                            
                                                                        
                                                                       
   
                                                                         
                                                                        
   
                                    
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/procsignal.h"
#include "storage/shm_mq.h"
#include "storage/spin.h"
#include "utils/memutils.h"

   
                                                                        
   
                                  
   
                                                                          
                                                                     
                                                                               
                                                                              
                            
   
                                                                                
                                                                               
                                                                               
                                                   
   
                                                                            
                                                                         
                                                                        
                                                                            
                                                                                
                                     
   
                                                                          
                                           
   
                                                                       
                                                               
                                                                            
                                                                         
                                                                           
                                                                       
                                                                              
                                                                          
                                                                         
                                                                             
                                                                             
                                                                             
                                                                           
                                                               
                                                                      
   
struct shm_mq
{
  slock_t mq_mutex;
  PGPROC *mq_receiver;
  PGPROC *mq_sender;
  pg_atomic_uint64 mq_bytes_read;
  pg_atomic_uint64 mq_bytes_written;
  Size mq_ring_size;
  bool mq_detached;
  uint8 mq_ring_offset;
  char mq_ring[FLEXIBLE_ARRAY_MEMBER];
};

   
                                                                     
   
                                                                          
                                                                              
                                                                         
                                                                  
   
                                                                              
                                                                            
                                                                            
                                                                              
                                                                           
                                                                              
                                                                             
                                                                          
                                                                               
                                                                          
                      
   
                                                                                
                                                                           
                                                                               
                                                                             
                                                                             
                                                                       
   
                                                                       
                                                                            
                                                                           
                                                                          
                                                                
                                                                            
                                                                             
                                                                         
                                                                           
                                                  
   
                                                                             
                                                                          
                       
   
                                                                          
                                                                           
                                                                            
                             
   
struct shm_mq_handle
{
  shm_mq *mqh_queue;
  dsm_segment *mqh_segment;
  BackgroundWorkerHandle *mqh_handle;
  char *mqh_buffer;
  Size mqh_buflen;
  Size mqh_consume_pending;
  Size mqh_partial_bytes;
  Size mqh_expected_bytes;
  bool mqh_length_word_complete;
  bool mqh_counterparty_attached;
  MemoryContext mqh_context;
};

static void
shm_mq_detach_internal(shm_mq *mq);
static shm_mq_result
shm_mq_send_bytes(shm_mq_handle *mqh, Size nbytes, const void *data, bool nowait, Size *bytes_written);
static shm_mq_result
shm_mq_receive_bytes(shm_mq_handle *mqh, Size bytes_needed, bool nowait, Size *nbytesp, void **datap);
static bool
shm_mq_counterparty_gone(shm_mq *mq, BackgroundWorkerHandle *handle);
static bool
shm_mq_wait_internal(shm_mq *mq, PGPROC **ptr, BackgroundWorkerHandle *handle);
static void
shm_mq_inc_bytes_read(shm_mq *mq, Size n);
static void
shm_mq_inc_bytes_written(shm_mq *mq, Size n);
static void
shm_mq_detach_callback(dsm_segment *seg, Datum arg);

                                                                             
const Size shm_mq_minimum_size = MAXALIGN(offsetof(shm_mq, mq_ring)) + MAXIMUM_ALIGNOF;

#define MQH_INITIAL_BUFSIZE 8192

   
                                          
   
shm_mq *
shm_mq_create(void *address, Size size)
{
  shm_mq *mq = address;
  Size data_offset = MAXALIGN(offsetof(shm_mq, mq_ring));

                                                                 
  size = MAXALIGN_DOWN(size);

                                                          
  Assert(size > data_offset);

                                
  SpinLockInit(&mq->mq_mutex);
  mq->mq_receiver = NULL;
  mq->mq_sender = NULL;
  pg_atomic_init_u64(&mq->mq_bytes_read, 0);
  pg_atomic_init_u64(&mq->mq_bytes_written, 0);
  mq->mq_ring_size = size - data_offset;
  mq->mq_detached = false;
  mq->mq_ring_offset = data_offset - offsetof(shm_mq, mq_ring);

  return mq;
}

   
                                                                           
          
   
void
shm_mq_set_receiver(shm_mq *mq, PGPROC *proc)
{
  PGPROC *sender;

  SpinLockAcquire(&mq->mq_mutex);
  Assert(mq->mq_receiver == NULL);
  mq->mq_receiver = proc;
  sender = mq->mq_sender;
  SpinLockRelease(&mq->mq_mutex);

  if (sender != NULL)
  {
    SetLatch(&sender->procLatch);
  }
}

   
                                                                             
   
void
shm_mq_set_sender(shm_mq *mq, PGPROC *proc)
{
  PGPROC *receiver;

  SpinLockAcquire(&mq->mq_mutex);
  Assert(mq->mq_sender == NULL);
  mq->mq_sender = proc;
  receiver = mq->mq_receiver;
  SpinLockRelease(&mq->mq_mutex);

  if (receiver != NULL)
  {
    SetLatch(&receiver->procLatch);
  }
}

   
                                
   
PGPROC *
shm_mq_get_receiver(shm_mq *mq)
{
  PGPROC *receiver;

  SpinLockAcquire(&mq->mq_mutex);
  receiver = mq->mq_receiver;
  SpinLockRelease(&mq->mq_mutex);

  return receiver;
}

   
                              
   
PGPROC *
shm_mq_get_sender(shm_mq *mq)
{
  PGPROC *sender;

  SpinLockAcquire(&mq->mq_mutex);
  sender = mq->mq_sender;
  SpinLockRelease(&mq->mq_mutex);

  return sender;
}

   
                                                                        
   
                                                                           
                                                                            
                                                                          
                                                                           
   
                                                                              
                                      
   
                                                                       
                                                                           
                                                                          
                     
   
                                                                   
                                                                    
                                                                         
                                      
   
shm_mq_handle *
shm_mq_attach(shm_mq *mq, dsm_segment *seg, BackgroundWorkerHandle *handle)
{
  shm_mq_handle *mqh = palloc(sizeof(shm_mq_handle));

  Assert(mq->mq_receiver == MyProc || mq->mq_sender == MyProc);
  mqh->mqh_queue = mq;
  mqh->mqh_segment = seg;
  mqh->mqh_handle = handle;
  mqh->mqh_buffer = NULL;
  mqh->mqh_buflen = 0;
  mqh->mqh_consume_pending = 0;
  mqh->mqh_partial_bytes = 0;
  mqh->mqh_expected_bytes = 0;
  mqh->mqh_length_word_complete = false;
  mqh->mqh_counterparty_attached = false;
  mqh->mqh_context = CurrentMemoryContext;

  if (seg != NULL)
  {
    on_dsm_detach(seg, shm_mq_detach_callback, PointerGetDatum(mq));
  }

  return mqh;
}

   
                                                                             
                                 
   
void
shm_mq_set_handle(shm_mq_handle *mqh, BackgroundWorkerHandle *handle)
{
  Assert(mqh->mqh_handle == NULL);
  mqh->mqh_handle = handle;
}

   
                                                
   
shm_mq_result
shm_mq_send(shm_mq_handle *mqh, Size nbytes, const void *data, bool nowait)
{
  shm_mq_iovec iov;

  iov.data = data;
  iov.len = nbytes;

  return shm_mq_sendv(mqh, &iov, 1, nowait);
}

   
                                                                       
              
   
                                                                             
                                                                                
                                               
   
                                                                            
                                                                          
                                                                        
                                                                            
                                                                               
                                                  
   
shm_mq_result
shm_mq_sendv(shm_mq_handle *mqh, shm_mq_iovec *iov, int iovcnt, bool nowait)
{
  shm_mq_result res;
  shm_mq *mq = mqh->mqh_queue;
  PGPROC *receiver;
  Size nbytes = 0;
  Size bytes_written;
  int i;
  int which_iov = 0;
  Size offset;

  Assert(mq->mq_sender == MyProc);

                                    
  for (i = 0; i < iovcnt; ++i)
  {
    nbytes += iov[i].len;
  }

                                                           
  if (nbytes > MaxAllocSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("cannot send a message of size %zu via shared memory queue", nbytes)));
  }

                                                                         
  while (!mqh->mqh_length_word_complete)
  {
    Assert(mqh->mqh_partial_bytes < sizeof(Size));
    res = shm_mq_send_bytes(mqh, sizeof(Size) - mqh->mqh_partial_bytes, ((char *)&nbytes) + mqh->mqh_partial_bytes, nowait, &bytes_written);

    if (res == SHM_MQ_DETACHED)
    {
                                                                     
      mqh->mqh_partial_bytes = 0;
      mqh->mqh_length_word_complete = false;
      return res;
    }
    mqh->mqh_partial_bytes += bytes_written;

    if (mqh->mqh_partial_bytes >= sizeof(Size))
    {
      Assert(mqh->mqh_partial_bytes == sizeof(Size));

      mqh->mqh_partial_bytes = 0;
      mqh->mqh_length_word_complete = true;
    }

    if (res != SHM_MQ_SUCCESS)
    {
      return res;
    }

                                                                           
    Assert(mqh->mqh_length_word_complete || sizeof(Size) > MAXIMUM_ALIGNOF);
  }

                                                    
  Assert(mqh->mqh_partial_bytes <= nbytes);
  offset = mqh->mqh_partial_bytes;
  do
  {
    Size chunksize;

                                                      
    if (offset >= iov[which_iov].len)
    {
      offset -= iov[which_iov].len;
      ++which_iov;
      if (which_iov >= iovcnt)
      {
        break;
      }
      continue;
    }

       
                                                                       
                                                                           
                                                                     
                                                                       
                                                                        
                                                                     
                   
       
    if (which_iov + 1 < iovcnt && offset + MAXIMUM_ALIGNOF > iov[which_iov].len)
    {
      char tmpbuf[MAXIMUM_ALIGNOF];
      int j = 0;

      for (;;)
      {
        if (offset < iov[which_iov].len)
        {
          tmpbuf[j] = iov[which_iov].data[offset];
          j++;
          offset++;
          if (j == MAXIMUM_ALIGNOF)
          {
            break;
          }
        }
        else
        {
          offset -= iov[which_iov].len;
          which_iov++;
          if (which_iov >= iovcnt)
          {
            break;
          }
        }
      }

      res = shm_mq_send_bytes(mqh, j, tmpbuf, nowait, &bytes_written);

      if (res == SHM_MQ_DETACHED)
      {
                                                                       
        mqh->mqh_partial_bytes = 0;
        mqh->mqh_length_word_complete = false;
        return res;
      }

      mqh->mqh_partial_bytes += bytes_written;
      if (res != SHM_MQ_SUCCESS)
      {
        return res;
      }
      continue;
    }

       
                                                                        
                                                                   
                                     
       
    chunksize = iov[which_iov].len - offset;
    if (which_iov + 1 < iovcnt)
    {
      chunksize = MAXALIGN_DOWN(chunksize);
    }
    res = shm_mq_send_bytes(mqh, chunksize, &iov[which_iov].data[offset], nowait, &bytes_written);

    if (res == SHM_MQ_DETACHED)
    {
                                                                     
      mqh->mqh_length_word_complete = false;
      mqh->mqh_partial_bytes = 0;
      return res;
    }

    mqh->mqh_partial_bytes += bytes_written;
    offset += bytes_written;
    if (res != SHM_MQ_SUCCESS)
    {
      return res;
    }
  } while (mqh->mqh_partial_bytes < nbytes);

                               
  mqh->mqh_partial_bytes = 0;
  mqh->mqh_length_word_complete = false;

                                                    
  if (mq->mq_detached)
  {
    return SHM_MQ_DETACHED;
  }

     
                                                                            
                                                                          
                             
     
  if (mqh->mqh_counterparty_attached)
  {
    receiver = mq->mq_receiver;
  }
  else
  {
    SpinLockAcquire(&mq->mq_mutex);
    receiver = mq->mq_receiver;
    SpinLockRelease(&mq->mq_mutex);
    if (receiver == NULL)
    {
      return SHM_MQ_SUCCESS;
    }
    mqh->mqh_counterparty_attached = true;
  }

                                                              
  SetLatch(&receiver->procLatch);
  return SHM_MQ_SUCCESS;
}

   
                                                  
   
                                                                          
                                                                    
                                                                              
                                                                            
                                                                            
                                                                           
                                                                             
   
                                                                             
                                                                          
                                                                            
                                                                   
                                           
   
                                                                            
                                                                         
                                                                         
                                                        
   
shm_mq_result
shm_mq_receive(shm_mq_handle *mqh, Size *nbytesp, void **datap, bool nowait)
{
  shm_mq *mq = mqh->mqh_queue;
  shm_mq_result res;
  Size rb = 0;
  Size nbytes;
  void *rawdata;

  Assert(mq->mq_receiver == MyProc);

                                                            
  if (!mqh->mqh_counterparty_attached)
  {
    if (nowait)
    {
      int counterparty_gone;

         
                                                                    
                                                                         
                                                                    
                                                                        
                                                                      
                                                                        
                                                                 
                                                                      
                                                                    
         
      counterparty_gone = shm_mq_counterparty_gone(mq, mqh->mqh_handle);
      if (shm_mq_get_sender(mq) == NULL)
      {
        if (counterparty_gone)
        {
          return SHM_MQ_DETACHED;
        }
        else
        {
          return SHM_MQ_WOULD_BLOCK;
        }
      }
    }
    else if (!shm_mq_wait_internal(mq, &mq->mq_sender, mqh->mqh_handle) && shm_mq_get_sender(mq) == NULL)
    {
      mq->mq_detached = true;
      return SHM_MQ_DETACHED;
    }
    mqh->mqh_counterparty_attached = true;
  }

     
                                                                        
                                                                          
                                                                       
                                                                           
            
     
  if (mqh->mqh_consume_pending > mq->mq_ring_size / 4)
  {
    shm_mq_inc_bytes_read(mq, mqh->mqh_consume_pending);
    mqh->mqh_consume_pending = 0;
  }

                                                                        
  while (!mqh->mqh_length_word_complete)
  {
                                                 
    Assert(mqh->mqh_partial_bytes < sizeof(Size));
    res = shm_mq_receive_bytes(mqh, sizeof(Size) - mqh->mqh_partial_bytes, nowait, &rb, &rawdata);
    if (res != SHM_MQ_SUCCESS)
    {
      return res;
    }

       
                                                                        
                                                                          
                       
       
    if (mqh->mqh_partial_bytes == 0 && rb >= sizeof(Size))
    {
      Size needed;

      nbytes = *(Size *)rawdata;

                                                               
      needed = MAXALIGN(sizeof(Size)) + MAXALIGN(nbytes);
      if (rb >= needed)
      {
        mqh->mqh_consume_pending += needed;
        *nbytesp = nbytes;
        *datap = ((char *)rawdata) + MAXALIGN(sizeof(Size));
        return SHM_MQ_SUCCESS;
      }

         
                                                                         
                      
         
      mqh->mqh_expected_bytes = nbytes;
      mqh->mqh_length_word_complete = true;
      mqh->mqh_consume_pending += MAXALIGN(sizeof(Size));
      rb -= MAXALIGN(sizeof(Size));
    }
    else
    {
      Size lengthbytes;

                                                                 
      Assert(sizeof(Size) > MAXIMUM_ALIGNOF);

                                                             
      if (mqh->mqh_buffer == NULL)
      {
        mqh->mqh_buffer = MemoryContextAlloc(mqh->mqh_context, MQH_INITIAL_BUFSIZE);
        mqh->mqh_buflen = MQH_INITIAL_BUFSIZE;
      }
      Assert(mqh->mqh_buflen >= sizeof(Size));

                                                             
      if (mqh->mqh_partial_bytes + rb > sizeof(Size))
      {
        lengthbytes = sizeof(Size) - mqh->mqh_partial_bytes;
      }
      else
      {
        lengthbytes = rb;
      }
      memcpy(&mqh->mqh_buffer[mqh->mqh_partial_bytes], rawdata, lengthbytes);
      mqh->mqh_partial_bytes += lengthbytes;
      mqh->mqh_consume_pending += MAXALIGN(lengthbytes);
      rb -= lengthbytes;

                                                                       
      if (mqh->mqh_partial_bytes >= sizeof(Size))
      {
        Assert(mqh->mqh_partial_bytes == sizeof(Size));
        mqh->mqh_expected_bytes = *(Size *)mqh->mqh_buffer;
        mqh->mqh_length_word_complete = true;
        mqh->mqh_partial_bytes = 0;
      }
    }
  }
  nbytes = mqh->mqh_expected_bytes;

     
                                                                            
                                                                         
                                  
     
  if (nbytes > MaxAllocSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("invalid message size %zu in shared memory queue", nbytes)));
  }

  if (mqh->mqh_partial_bytes == 0)
  {
       
                                                                          
                                                                        
                      
       
    res = shm_mq_receive_bytes(mqh, nbytes, nowait, &rb, &rawdata);
    if (res != SHM_MQ_SUCCESS)
    {
      return res;
    }
    if (rb >= nbytes)
    {
      mqh->mqh_length_word_complete = false;
      mqh->mqh_consume_pending += MAXALIGN(nbytes);
      *nbytesp = nbytes;
      *datap = rawdata;
      return SHM_MQ_SUCCESS;
    }

       
                                                                           
                                                                          
                                        
       
    if (mqh->mqh_buflen < nbytes)
    {
      Size newbuflen = Max(mqh->mqh_buflen, MQH_INITIAL_BUFSIZE);

         
                                                                     
                       
         
      while (newbuflen < nbytes)
      {
        newbuflen *= 2;
      }
      newbuflen = Min(newbuflen, MaxAllocSize);

      if (mqh->mqh_buffer != NULL)
      {
        pfree(mqh->mqh_buffer);
        mqh->mqh_buffer = NULL;
        mqh->mqh_buflen = 0;
      }
      mqh->mqh_buffer = MemoryContextAlloc(mqh->mqh_context, newbuflen);
      mqh->mqh_buflen = newbuflen;
    }
  }

                                                   
  for (;;)
  {
    Size still_needed;

                                 
    Assert(mqh->mqh_partial_bytes + rb <= nbytes);
    if (rb > 0)
    {
      memcpy(&mqh->mqh_buffer[mqh->mqh_partial_bytes], rawdata, rb);
      mqh->mqh_partial_bytes += rb;
    }

       
                                                                  
                                                                         
                                                                          
                                                                          
       
    Assert(mqh->mqh_partial_bytes == nbytes || rb == MAXALIGN(rb));
    mqh->mqh_consume_pending += MAXALIGN(rb);

                                                
    if (mqh->mqh_partial_bytes >= nbytes)
    {
      break;
    }

                                  
    still_needed = nbytes - mqh->mqh_partial_bytes;
    res = shm_mq_receive_bytes(mqh, still_needed, nowait, &rb, &rawdata);
    if (res != SHM_MQ_SUCCESS)
    {
      return res;
    }
    if (rb > still_needed)
    {
      rb = still_needed;
    }
  }

                                                                
  *nbytesp = nbytes;
  *datap = mqh->mqh_buffer;
  mqh->mqh_length_word_complete = false;
  mqh->mqh_partial_bytes = 0;
  return SHM_MQ_SUCCESS;
}

   
                                                                          
          
   
                                                                             
                                                                               
                                                                            
                                                                          
   
shm_mq_result
shm_mq_wait_for_attach(shm_mq_handle *mqh)
{
  shm_mq *mq = mqh->mqh_queue;
  PGPROC **victim;

  if (shm_mq_get_receiver(mq) == MyProc)
  {
    victim = &mq->mq_sender;
  }
  else
  {
    Assert(shm_mq_get_sender(mq) == MyProc);
    victim = &mq->mq_receiver;
  }

  if (shm_mq_wait_internal(mq, victim, mqh->mqh_handle))
  {
    return SHM_MQ_SUCCESS;
  }
  else
  {
    return SHM_MQ_DETACHED;
  }
}

   
                                                                      
   
void
shm_mq_detach(shm_mq_handle *mqh)
{
                                                  
  shm_mq_detach_internal(mqh->mqh_queue);

                                              
  if (mqh->mqh_segment)
  {
    cancel_on_dsm_detach(mqh->mqh_segment, shm_mq_detach_callback, PointerGetDatum(mqh->mqh_queue));
  }

                                                    
  if (mqh->mqh_buffer != NULL)
  {
    pfree(mqh->mqh_buffer);
  }
  pfree(mqh);
}

   
                                                                       
   
                                                                 
                                                                          
                                                                      
                                                                        
                                                                         
                                                                           
   
                                                                           
                                                                         
                                                                 
   
static void
shm_mq_detach_internal(shm_mq *mq)
{
  PGPROC *victim;

  SpinLockAcquire(&mq->mq_mutex);
  if (mq->mq_sender == MyProc)
  {
    victim = mq->mq_receiver;
  }
  else
  {
    Assert(mq->mq_receiver == MyProc);
    victim = mq->mq_sender;
  }
  mq->mq_detached = true;
  SpinLockRelease(&mq->mq_mutex);

  if (victim != NULL)
  {
    SetLatch(&victim->procLatch);
  }
}

   
                               
   
shm_mq *
shm_mq_get_queue(shm_mq_handle *mqh)
{
  return mqh->mqh_queue;
}

   
                                            
   
static shm_mq_result
shm_mq_send_bytes(shm_mq_handle *mqh, Size nbytes, const void *data, bool nowait, Size *bytes_written)
{
  shm_mq *mq = mqh->mqh_queue;
  Size sent = 0;
  uint64 used;
  Size ringsize = mq->mq_ring_size;
  Size available;

  while (sent < nbytes)
  {
    uint64 rb;
    uint64 wb;

                                                                 
    rb = pg_atomic_read_u64(&mq->mq_bytes_read);
    wb = pg_atomic_read_u64(&mq->mq_bytes_written);
    Assert(wb >= rb);
    used = wb - rb;
    Assert(used <= ringsize);
    available = Min(ringsize - used, nbytes - sent);

       
                                                                          
                                                             
                                                                 
                                                                     
                                                                       
                                                                        
                                                            
       
    pg_compiler_barrier();
    if (mq->mq_detached)
    {
      *bytes_written = sent;
      return SHM_MQ_DETACHED;
    }

    if (available == 0 && !mqh->mqh_counterparty_attached)
    {
         
                                                                     
                                                    
         
      if (nowait)
      {
        if (shm_mq_counterparty_gone(mq, mqh->mqh_handle))
        {
          *bytes_written = sent;
          return SHM_MQ_DETACHED;
        }
        if (shm_mq_get_receiver(mq) == NULL)
        {
          *bytes_written = sent;
          return SHM_MQ_WOULD_BLOCK;
        }
      }
      else if (!shm_mq_wait_internal(mq, &mq->mq_receiver, mqh->mqh_handle))
      {
        mq->mq_detached = true;
        *bytes_written = sent;
        return SHM_MQ_DETACHED;
      }
      mqh->mqh_counterparty_attached = true;

         
                                                                     
                                                           
         
    }
    else if (available == 0)
    {
         
                                                                         
                                                                        
                                                                   
         
      Assert(mqh->mqh_counterparty_attached);
      SetLatch(&mq->mq_receiver->procLatch);

                                                            
      if (nowait)
      {
        *bytes_written = sent;
        return SHM_MQ_WOULD_BLOCK;
      }

         
                                                                         
                                                                     
                                                                       
                                                                      
                                                       
         
      (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, WAIT_EVENT_MQ_SEND);

                                             
      ResetLatch(MyLatch);

                                                                 
      CHECK_FOR_INTERRUPTS();
    }
    else
    {
      Size offset;
      Size sendnow;

      offset = wb % (uint64)ringsize;
      sendnow = Min(available, ringsize - offset);

         
                                                                       
                                                                     
                                                                   
                                                                  
                                                                    
         
      pg_memory_barrier();
      memcpy(&mq->mq_ring[mq->mq_ring_offset + offset], (char *)data + sent, sendnow);
      sent += sendnow;

         
                                                                      
                                                                        
                                                                         
                                                    
         
      Assert(sent == nbytes || sendnow == MAXALIGN(sendnow));
      shm_mq_inc_bytes_written(mq, MAXALIGN(sendnow));

         
                                                                         
                                                                       
                  
         
    }
  }

  *bytes_written = sent;
  return SHM_MQ_SUCCESS;
}

   
                                                                        
                                                                            
                                                                         
                                                                            
                                                                           
                                                                           
                      
   
static shm_mq_result
shm_mq_receive_bytes(shm_mq_handle *mqh, Size bytes_needed, bool nowait, Size *nbytesp, void **datap)
{
  shm_mq *mq = mqh->mqh_queue;
  Size ringsize = mq->mq_ring_size;
  uint64 used;
  uint64 written;

  for (;;)
  {
    Size offset;
    uint64 read;

                                                                        
    written = pg_atomic_read_u64(&mq->mq_bytes_written);

       
                                                                        
                 
       
    read = pg_atomic_read_u64(&mq->mq_bytes_read) + mqh->mqh_consume_pending;
    used = written - read;
    Assert(used <= ringsize);
    offset = read % (uint64)ringsize;

                                                                   
    if (used >= bytes_needed || offset + used >= ringsize)
    {
      *nbytesp = Min(used, ringsize - offset);
      *datap = &mq->mq_ring[mq->mq_ring_offset + offset];

         
                                                                     
                                                                     
                                   
         
      pg_read_barrier();
      return SHM_MQ_SUCCESS;
    }

       
                                                               
       
                                                                           
                                                                           
                                                                          
                 
       
    if (mq->mq_detached)
    {
         
                                                              
                                                                
                                                                       
                                                 
         
      pg_read_barrier();
      if (written != pg_atomic_read_u64(&mq->mq_bytes_written))
      {
        continue;
      }

      return SHM_MQ_DETACHED;
    }

       
                                                                          
                                                              
       
    if (mqh->mqh_consume_pending > 0)
    {
      shm_mq_inc_bytes_read(mq, mqh->mqh_consume_pending);
      mqh->mqh_consume_pending = 0;
    }

                                                          
    if (nowait)
    {
      return SHM_MQ_WOULD_BLOCK;
    }

       
                                                                       
                                                                           
                                                                       
                                                                       
                                        
       
    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, WAIT_EVENT_MQ_RECEIVE);

                                           
    ResetLatch(MyLatch);

                                                               
    CHECK_FOR_INTERRUPTS();
  }
}

   
                                                                                 
   
static bool
shm_mq_counterparty_gone(shm_mq *mq, BackgroundWorkerHandle *handle)
{
  pid_t pid;

                                                                        
  if (mq->mq_detached)
  {
    return true;
  }

                                                 
  if (handle != NULL)
  {
    BgwHandleStatus status;

                                            
    status = GetBackgroundWorkerPid(handle, &pid);
    if (status != BGWH_STARTED && status != BGWH_NOT_YET_STARTED)
    {
                                                       
      mq->mq_detached = true;
      return true;
    }
  }

                                              
  return false;
}

   
                                                                               
                                                                       
                                                                            
                                                                              
                                                                           
                                               
   
                                                                         
                                                        
   
static bool
shm_mq_wait_internal(shm_mq *mq, PGPROC **ptr, BackgroundWorkerHandle *handle)
{
  bool result = false;

  for (;;)
  {
    BgwHandleStatus status;
    pid_t pid;

                                                                 
    SpinLockAcquire(&mq->mq_mutex);
    result = (*ptr != NULL);
    SpinLockRelease(&mq->mq_mutex);

                                                        
    if (mq->mq_detached)
    {
      result = false;
      break;
    }
    if (result)
    {
      break;
    }

    if (handle != NULL)
    {
                                              
      status = GetBackgroundWorkerPid(handle, &pid);
      if (status != BGWH_STARTED && status != BGWH_NOT_YET_STARTED)
      {
        result = false;
        break;
      }
    }

                               
    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, WAIT_EVENT_MQ_INTERNAL);

                                           
    ResetLatch(MyLatch);

                                                               
    CHECK_FOR_INTERRUPTS();
  }

  return result;
}

   
                                       
   
static void
shm_mq_inc_bytes_read(shm_mq *mq, Size n)
{
  PGPROC *sender;

     
                                                                         
                                                         
                                                                       
                                                                           
            
     
  pg_read_barrier();

     
                                                                         
                                                                      
     
  pg_atomic_write_u64(&mq->mq_bytes_read, pg_atomic_read_u64(&mq->mq_bytes_read) + n);

     
                                                                          
                                                                             
     
  sender = mq->mq_sender;
  Assert(sender != NULL);
  SetLatch(&sender->procLatch);
}

   
                                          
   
static void
shm_mq_inc_bytes_written(shm_mq *mq, Size n)
{
     
                                                                        
                                                                    
                           
     
  pg_write_barrier();

     
                                                                         
                                                                         
                         
     
  pg_atomic_write_u64(&mq->mq_bytes_written, pg_atomic_read_u64(&mq->mq_bytes_written) + n);
}

                               
static void
shm_mq_detach_callback(dsm_segment *seg, Datum arg)
{
  shm_mq *mq = (shm_mq *)DatumGetPointer(arg);

  shm_mq_detach_internal(mq);
}
