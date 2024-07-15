                                                                            
   
              
                                                                   
   
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "port/atomics.h"
#include "storage/buf_internals.h"
#include "storage/bufmgr.h"
#include "storage/proc.h"

#define INT_ACCESS_ONCE(var) ((int)(*((volatile int *)&(var))))

   
                                            
   
typedef struct
{
                                           
  slock_t buffer_strategy_lock;

     
                                                                            
                                                                            
                                                                
     
  pg_atomic_uint32 nextVictimBuffer;

  int firstFreeBuffer;                                     
  int lastFreeBuffer;                                      

     
                                                                            
                             
     

     
                                                                       
                                              
     
  uint32 completePasses;                                                    
  pg_atomic_uint32 numBufferAllocs;                                         

     
                                                                      
                             
     
  int bgwprocno;
} BufferStrategyControl;

                              
static BufferStrategyControl *StrategyControl = NULL;

   
                                                                               
                                                                               
                             
   
typedef struct BufferAccessStrategyData
{
                             
  BufferAccessStrategyType btype;
                                             
  int ring_size;

     
                                                                        
                                    
     
  int current;

     
                                                                           
                   
     
  bool current_was_in_ring;

     
                                                                          
                                                                        
                                                                       
             
     
  Buffer buffers[FLEXIBLE_ARRAY_MEMBER];
} BufferAccessStrategyData;

                                       
static BufferDesc *
GetBufferFromRing(BufferAccessStrategy strategy, uint32 *buf_state);
static void
AddBufferToRing(BufferAccessStrategy strategy, BufferDesc *buf);

   
                                                           
   
                                                                               
                                        
   
static inline uint32
ClockSweepTick(void)
{
  uint32 victim;

     
                                                                          
                                                                         
                     
     
  victim = pg_atomic_fetch_add_u32(&StrategyControl->nextVictimBuffer, 1);

  if (victim >= NBuffers)
  {
    uint32 originalVictim = victim;

                                                          
    victim = victim % NBuffers;

       
                                                             
                                                                       
                                                                        
                                                                
       
    if (victim == 0)
    {
      uint32 expected;
      uint32 wrapped;
      bool success = false;

      expected = originalVictim + 1;

      while (!success)
      {
           
                                                                      
                                                             
                                                                       
                                                                  
                                                                      
                                                                 
           
        SpinLockAcquire(&StrategyControl->buffer_strategy_lock);

        wrapped = expected % NBuffers;

        success = pg_atomic_compare_exchange_u32(&StrategyControl->nextVictimBuffer, &expected, wrapped);
        if (success)
        {
          StrategyControl->completePasses++;
        }
        SpinLockRelease(&StrategyControl->buffer_strategy_lock);
      }
    }
  }
  return victim;
}

   
                                                                            
                       
   
                                                                                
                                                                             
                         
   
bool
have_free_buffer()
{
  if (StrategyControl->firstFreeBuffer >= 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                     
   
                                                                   
                                                                      
                                                               
   
                                                                            
   
                                                                       
                                                                 
   
BufferDesc *
StrategyGetBuffer(BufferAccessStrategy strategy, uint32 *buf_state)
{
  BufferDesc *buf;
  int bgwprocno;
  int trycounter;
  uint32 local_buf_state;                                         

     
                                                                        
                                                              
     
  if (strategy != NULL)
  {
    buf = GetBufferFromRing(strategy, buf_state);
    if (buf != NULL)
    {
      return buf;
    }
  }

     
                                                                             
                                                                           
                                                                          
                                                                             
                                                 
     
                                                                          
                                                                    
                                                                           
                             
     
  bgwprocno = INT_ACCESS_ONCE(StrategyControl->bgwprocno);
  if (bgwprocno != -1)
  {
                                                         
    StrategyControl->bgwprocno = -1;

       
                                                                     
                                                                        
                                                                  
       
    SetLatch(&ProcGlobal->allProcs[bgwprocno].procLatch);
  }

     
                                                                           
                                                                      
                                                         
     
  pg_atomic_fetch_add_u32(&StrategyControl->numBufferAllocs, 1);

     
                                                                             
                                                                      
                                                                      
                                                                             
                                                                            
                                                                       
     
                                                                         
                                                                          
                    
     
                                                                         
                                                                             
                                                   
     
  if (StrategyControl->firstFreeBuffer >= 0)
  {
    while (true)
    {
                                                                    
      SpinLockAcquire(&StrategyControl->buffer_strategy_lock);

      if (StrategyControl->firstFreeBuffer < 0)
      {
        SpinLockRelease(&StrategyControl->buffer_strategy_lock);
        break;
      }

      buf = GetBufferDescriptor(StrategyControl->firstFreeBuffer);
      Assert(buf->freeNext != FREENEXT_NOT_IN_LIST);

                                                       
      StrategyControl->firstFreeBuffer = buf->freeNext;
      buf->freeNext = FREENEXT_NOT_IN_LIST;

         
                                                                        
                                   
         
      SpinLockRelease(&StrategyControl->buffer_strategy_lock);

         
                                                                         
                                                                        
                                                                       
                                                                         
                                                
         
      local_buf_state = LockBufHdr(buf);
      if (BUF_STATE_GET_REFCOUNT(local_buf_state) == 0 && BUF_STATE_GET_USAGECOUNT(local_buf_state) == 0)
      {
        if (strategy != NULL)
        {
          AddBufferToRing(strategy, buf);
        }
        *buf_state = local_buf_state;
        return buf;
      }
      UnlockBufHdr(buf, local_buf_state);
    }
  }

                                                                   
  trycounter = NBuffers;
  for (;;)
  {
    buf = GetBufferDescriptor(ClockSweepTick());

       
                                                                           
                                                                        
       
    local_buf_state = LockBufHdr(buf);

    if (BUF_STATE_GET_REFCOUNT(local_buf_state) == 0)
    {
      if (BUF_STATE_GET_USAGECOUNT(local_buf_state) != 0)
      {
        local_buf_state -= BUF_USAGECOUNT_ONE;

        trycounter = NBuffers;
      }
      else
      {
                                   
        if (strategy != NULL)
        {
          AddBufferToRing(strategy, buf);
        }
        *buf_state = local_buf_state;
        return buf;
      }
    }
    else if (--trycounter == 0)
    {
         
                                                                         
                                                                         
                                                                       
                                                                  
                        
         
      UnlockBufHdr(buf, local_buf_state);
      elog(ERROR, "no unpinned buffers available");
    }
    UnlockBufHdr(buf, local_buf_state);
  }
}

   
                                                    
   
void
StrategyFreeBuffer(BufferDesc *buf)
{
  SpinLockAcquire(&StrategyControl->buffer_strategy_lock);

     
                                                                           
                                                      
     
  if (buf->freeNext == FREENEXT_NOT_IN_LIST)
  {
    buf->freeNext = StrategyControl->firstFreeBuffer;
    if (buf->freeNext < 0)
    {
      StrategyControl->lastFreeBuffer = buf->buf_id;
    }
    StrategyControl->firstFreeBuffer = buf->buf_id;
  }

  SpinLockRelease(&StrategyControl->buffer_strategy_lock);
}

   
                                                               
   
                                                                    
                                                                            
   
                                                                         
                                                                             
                                                                           
               
   
int
StrategySyncStart(uint32 *complete_passes, uint32 *num_buf_alloc)
{
  uint32 nextVictimBuffer;
  int result;

  SpinLockAcquire(&StrategyControl->buffer_strategy_lock);
  nextVictimBuffer = pg_atomic_read_u32(&StrategyControl->nextVictimBuffer);
  result = nextVictimBuffer % NBuffers;

  if (complete_passes)
  {
    *complete_passes = StrategyControl->completePasses;

       
                                                                       
                                                                   
       
    *complete_passes += nextVictimBuffer / NBuffers;
  }

  if (num_buf_alloc)
  {
    *num_buf_alloc = pg_atomic_exchange_u32(&StrategyControl->numBufferAllocs, 0);
  }
  SpinLockRelease(&StrategyControl->buffer_strategy_lock);
  return result;
}

   
                                                                        
   
                                                                        
                                                                        
                                                                            
                                                               
   
void
StrategyNotifyBgWriter(int bgwprocno)
{
     
                                                                           
                                                                        
                                                                      
     
  SpinLockAcquire(&StrategyControl->buffer_strategy_lock);
  StrategyControl->bgwprocno = bgwprocno;
  SpinLockRelease(&StrategyControl->buffer_strategy_lock);
}

   
                     
   
                                                                               
   
                                                                           
                            
   
Size
StrategyShmemSize(void)
{
  Size size = 0;

                                                                       
  size = add_size(size, BufTableShmemSize(NBuffers + NUM_BUFFER_PARTITIONS));

                                                             
  size = add_size(size, MAXALIGN(sizeof(BufferStrategyControl)));

  return size;
}

   
                                                                 
              
   
                                                                     
                                                              
   
void
StrategyInitialize(bool init)
{
  bool found;

     
                                                    
     
                                                                             
                                                                            
                                                                            
                                                                      
                                                                           
                                               
     
  InitBufTable(NBuffers + NUM_BUFFER_PARTITIONS);

     
                                                     
     
  StrategyControl = (BufferStrategyControl *)ShmemInitStruct("Buffer Strategy Status", sizeof(BufferStrategyControl), &found);

  if (!found)
  {
       
                                             
       
    Assert(init);

    SpinLockInit(&StrategyControl->buffer_strategy_lock);

       
                                                                       
                                                            
       
    StrategyControl->firstFreeBuffer = 0;
    StrategyControl->lastFreeBuffer = NBuffers - 1;

                                            
    pg_atomic_init_u32(&StrategyControl->nextVictimBuffer, 0);

                          
    StrategyControl->completePasses = 0;
    pg_atomic_init_u32(&StrategyControl->numBufferAllocs, 0);

                                 
    StrategyControl->bgwprocno = -1;
  }
  else
  {
    Assert(!init);
  }
}

                                                                    
                                             
                                                                    
   

   
                                                             
   
                                                          
   
BufferAccessStrategy
GetAccessStrategy(BufferAccessStrategyType btype)
{
  BufferAccessStrategy strategy;
  int ring_size;

     
                                                                 
     
                                                                  
                                                          
     
  switch (btype)
  {
  case BAS_NORMAL:
                                                                      
    return NULL;

  case BAS_BULKREAD:
    ring_size = 256 * 1024 / BLCKSZ;
    break;
  case BAS_BULKWRITE:
    ring_size = 16 * 1024 * 1024 / BLCKSZ;
    break;
  case BAS_VACUUM:
    ring_size = 256 * 1024 / BLCKSZ;
    break;

  default:
    elog(ERROR, "unrecognized buffer access strategy: %d", (int)btype);
    return NULL;                          
  }

                                                                
  ring_size = Min(NBuffers / 8, ring_size);

                                                                 
  strategy = (BufferAccessStrategy)palloc0(offsetof(BufferAccessStrategyData, buffers) + ring_size * sizeof(Buffer));

                                            
  strategy->btype = btype;
  strategy->ring_size = ring_size;

  return strategy;
}

   
                                                               
   
                                                                           
                                                                            
   
void
FreeAccessStrategy(BufferAccessStrategy strategy)
{
                                                     
  if (strategy != NULL)
  {
    pfree(strategy);
  }
}

   
                                                                       
                   
   
                                                        
   
static BufferDesc *
GetBufferFromRing(BufferAccessStrategy strategy, uint32 *buf_state)
{
  BufferDesc *buf;
  Buffer bufnum;
  uint32 local_buf_state;                                         

                                 
  if (++strategy->current >= strategy->ring_size)
  {
    strategy->current = 0;
  }

     
                                                                           
                                                                         
                                                          
     
  bufnum = strategy->buffers[strategy->current];
  if (bufnum == InvalidBuffer)
  {
    strategy->current_was_in_ring = false;
    return NULL;
  }

     
                                                                       
     
                                                                         
                                                                         
                                                                           
                                                                             
                          
     
  buf = GetBufferDescriptor(bufnum - 1);
  local_buf_state = LockBufHdr(buf);
  if (BUF_STATE_GET_REFCOUNT(local_buf_state) == 0 && BUF_STATE_GET_USAGECOUNT(local_buf_state) <= 1)
  {
    strategy->current_was_in_ring = true;
    *buf_state = local_buf_state;
    return buf;
  }
  UnlockBufHdr(buf, local_buf_state);

     
                                                                     
                                                                          
     
  strategy->current_was_in_ring = false;
  return NULL;
}

   
                                                      
   
                                                                          
                                                                   
   
static void
AddBufferToRing(BufferAccessStrategy strategy, BufferDesc *buf)
{
  strategy->buffers[strategy->current] = BufferDescriptorGetBuffer(buf);
}

   
                                                             
   
                                                                              
                                                                            
                                                                              
                                          
   
                                                                         
                                                 
   
bool
StrategyRejectBuffer(BufferAccessStrategy strategy, BufferDesc *buf)
{
                                        
  if (strategy->btype != BAS_BULKREAD)
  {
    return false;
  }

                                                                      
  if (!strategy->current_was_in_ring || strategy->buffers[strategy->current] != BufferDescriptorGetBuffer(buf))
  {
    return false;
  }

     
                                                                          
                                         
     
  strategy->buffers[strategy->current] = InvalidBuffer;

  return true;
}
