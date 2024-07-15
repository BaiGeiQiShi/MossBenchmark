                                                                            
   
              
                                            
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "storage/bufmgr.h"
#include "storage/buf_internals.h"

BufferDescPadded *BufferDescriptors;
char *BufferBlocks;
LWLockMinimallyPadded *BufferIOLWLockArray = NULL;
WritebackContext BackendWritebackContext;
CkptSortItem *CkptBufferIds;

   
                    
                                                            
   
   
                  
                                                      
                                                         
                                                    
                                                   
                         
   
                       
                                                          
                                             
   
   
                            
   
                                                              
                                                          
                                                          
                                                           
                                                      
   
                                                                        
                                                                        
                                                                         
                                                                           
                                                                          
   

   
                                 
   
                                                                          
                                            
   
void
InitBufferPool(void)
{
  bool foundBufs, foundDescs, foundIOLocks, foundBufCkpt;

                                                  
  BufferDescriptors = (BufferDescPadded *)ShmemInitStruct("Buffer Descriptors", NBuffers * sizeof(BufferDescPadded), &foundDescs);

  BufferBlocks = (char *)ShmemInitStruct("Buffer Blocks", NBuffers * (Size)BLCKSZ, &foundBufs);

                                           
  BufferIOLWLockArray = (LWLockMinimallyPadded *)ShmemInitStruct("Buffer IO Locks", NBuffers * (Size)sizeof(LWLockMinimallyPadded), &foundIOLocks);

  LWLockRegisterTranche(LWTRANCHE_BUFFER_IO_IN_PROGRESS, "buffer_io");
  LWLockRegisterTranche(LWTRANCHE_BUFFER_CONTENT, "buffer_content");

     
                                                                        
                                                                       
                                                                            
                                                                        
              
     
  CkptBufferIds = (CkptSortItem *)ShmemInitStruct("Checkpoint BufferIds", NBuffers * sizeof(CkptSortItem), &foundBufCkpt);

  if (foundDescs || foundBufs || foundIOLocks || foundBufCkpt)
  {
                                                   
    Assert(foundDescs && foundBufs && foundIOLocks && foundBufCkpt);
                                                            
  }
  else
  {
    int i;

       
                                          
       
    for (i = 0; i < NBuffers; i++)
    {
      BufferDesc *buf = GetBufferDescriptor(i);

      CLEAR_BUFFERTAG(buf->tag);

      pg_atomic_init_u32(&buf->state, 0);
      buf->wait_backend_pid = 0;

      buf->buf_id = i;

         
                                                                       
                                                        
         
      buf->freeNext = i + 1;

      LWLockInitialize(BufferDescriptorGetContentLock(buf), LWTRANCHE_BUFFER_CONTENT);

      LWLockInitialize(BufferDescriptorGetIOLock(buf), LWTRANCHE_BUFFER_IO_IN_PROGRESS);
    }

                                           
    GetBufferDescriptor(NBuffers - 1)->freeNext = FREENEXT_END_OF_LIST;
  }

                                                 
  StrategyInitialize(!foundDescs);

                                                 
  WritebackContextInit(&BackendWritebackContext, &backend_flush_after);
}

   
                   
   
                                                                   
                                                     
   
Size
BufferShmemSize(void)
{
  Size size = 0;

                                  
  size = add_size(size, mul_size(NBuffers, sizeof(BufferDescPadded)));
                                            
  size = add_size(size, PG_CACHE_LINE_SIZE);

                          
  size = add_size(size, mul_size(NBuffers, BLCKSZ));

                                              
  size = add_size(size, StrategyShmemSize());

     
                                                                           
                                                                          
                                                                           
                                                                         
                                                                           
                                                                       
              
     
  size = add_size(size, mul_size(NBuffers, sizeof(LWLockMinimallyPadded)));
                                   
  size = add_size(size, PG_CACHE_LINE_SIZE);

                                                 
  size = add_size(size, mul_size(NBuffers, sizeof(CkptSortItem)));

  return size;
}
