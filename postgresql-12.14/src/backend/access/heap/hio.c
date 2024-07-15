                                                                            
   
         
                                                    
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include "access/heapam.h"
#include "access/hio.h"
#include "access/htup_details.h"
#include "access/visibilitymap.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"

   
                                                        
   
                                                                       
   
                                                                
   
void
RelationPutHeapTuple(Relation relation, Buffer buffer, HeapTuple tuple, bool token)
{
  Page pageHeader;
  OffsetNumber offnum;

     
                                                                         
                
     
  Assert(!token || HeapTupleHeaderIsSpeculative(tuple->t_data));

                                 
  pageHeader = BufferGetPage(buffer);

  offnum = PageAddItem(pageHeader, (Item)tuple->t_data, tuple->t_len, InvalidOffsetNumber, false, true);

  if (offnum == InvalidOffsetNumber)
  {
    elog(PANIC, "failed to add tuple to page");
  }

                                                                       
  ItemPointerSet(&(tuple->t_self), BufferGetBlockNumber(buffer), offnum);

     
                                                                            
                                                                         
                         
     
  if (!token)
  {
    ItemId itemId = PageGetItemId(pageHeader, offnum);
    HeapTupleHeader item = (HeapTupleHeader)PageGetItem(pageHeader, itemId);

    item->t_ctid = tuple->t_self;
  }
}

   
                                                                               
   
static Buffer
ReadBufferBI(Relation relation, BlockNumber targetBlock, ReadBufferMode mode, BulkInsertState bistate)
{
  Buffer buffer;

                                                   
  if (!bistate)
  {
    return ReadBufferExtended(relation, MAIN_FORKNUM, targetBlock, mode, NULL);
  }

                                                                         
  if (bistate->current_buf != InvalidBuffer)
  {
    if (BufferGetBlockNumber(bistate->current_buf) == targetBlock)
    {
         
                                                                 
                                                         
         
      Assert(mode != RBM_ZERO_AND_LOCK && mode != RBM_ZERO_AND_CLEANUP_LOCK);

      IncrBufferRefCount(bistate->current_buf);
      return bistate->current_buf;
    }
                                      
    ReleaseBuffer(bistate->current_buf);
    bistate->current_buf = InvalidBuffer;
  }

                                                
  buffer = ReadBufferExtended(relation, MAIN_FORKNUM, targetBlock, mode, bistate->strategy);

                                                            
  IncrBufferRefCount(buffer);
  bistate->current_buf = buffer;

  return buffer;
}

   
                                                                             
                                                       
   
                                                                          
                                                                          
                        
   
static void
GetVisibilityMapPins(Relation relation, Buffer buffer1, Buffer buffer2, BlockNumber block1, BlockNumber block2, Buffer *vmbuffer1, Buffer *vmbuffer2)
{
  bool need_to_pin_buffer1;
  bool need_to_pin_buffer2;

  Assert(BufferIsValid(buffer1));
  Assert(buffer2 == InvalidBuffer || block1 <= block2);

  while (1)
  {
                                                       
    need_to_pin_buffer1 = PageIsAllVisible(BufferGetPage(buffer1)) && !visibilitymap_pin_ok(block1, *vmbuffer1);
    need_to_pin_buffer2 = buffer2 != InvalidBuffer && PageIsAllVisible(BufferGetPage(buffer2)) && !visibilitymap_pin_ok(block2, *vmbuffer2);
    if (!need_to_pin_buffer1 && !need_to_pin_buffer2)
    {
      return;
    }

                                                           
    LockBuffer(buffer1, BUFFER_LOCK_UNLOCK);
    if (buffer2 != InvalidBuffer && buffer2 != buffer1)
    {
      LockBuffer(buffer2, BUFFER_LOCK_UNLOCK);
    }

                   
    if (need_to_pin_buffer1)
    {
      visibilitymap_pin(relation, block1, vmbuffer1);
    }
    if (need_to_pin_buffer2)
    {
      visibilitymap_pin(relation, block2, vmbuffer2);
    }

                         
    LockBuffer(buffer1, BUFFER_LOCK_EXCLUSIVE);
    if (buffer2 != InvalidBuffer && buffer2 != buffer1)
    {
      LockBuffer(buffer2, BUFFER_LOCK_EXCLUSIVE);
    }

       
                                                                         
                                                                          
                                                                       
                                                                     
       
    if (buffer2 == InvalidBuffer || buffer1 == buffer2 || (need_to_pin_buffer1 && need_to_pin_buffer2))
    {
      break;
    }
  }
}

   
                                                                          
                                                                          
                                                                            
                                          
   
static void
RelationAddExtraBlocks(Relation relation, BulkInsertState bistate)
{
  BlockNumber blockNum, firstBlock = InvalidBlockNumber;
  int extraBlocks;
  int lockWaiters;

                                                                          
  lockWaiters = RelationExtensionLockWaiterCount(relation);
  if (lockWaiters <= 0)
  {
    return;
  }

     
                                                                             
                                                                          
                                                                 
                           
     
  extraBlocks = Min(512, lockWaiters * 20);

  do
  {
    Buffer buffer;
    Page page;
    Size freespace;

       
                                                                      
                                                                        
                                                                        
                                        
       
    buffer = ReadBufferBI(relation, P_NEW, RBM_ZERO_AND_LOCK, bistate);
    page = BufferGetPage(buffer);

    if (!PageIsNew(page))
    {
      elog(ERROR, "page %u of relation \"%s\" should be empty but is not", BufferGetBlockNumber(buffer), RelationGetRelationName(relation));
    }

       
                                                                   
                                                                           
                                                                          
                                                                
                                                                
                           
       

                                    
    blockNum = BufferGetBlockNumber(buffer);
    freespace = BufferGetPageSize(buffer) - SizeOfPageHeaderData;

    UnlockReleaseBuffer(buffer);

                                                 
    if (firstBlock == InvalidBlockNumber)
    {
      firstBlock = blockNum;
    }

       
                                                                        
                                                                          
                                                           
       
    RecordPageWithFreeSpace(relation, blockNum, freespace);
  } while (--extraBlocks > 0);

     
                                                                            
                                                                             
                                                                         
                    
     
  FreeSpaceMapVacuumRange(relation, firstBlock, blockNum + 1);
}

   
                             
   
                                                                          
                                 
   
                                                                        
                                                                       
                                                                             
                                                      
   
                                                                        
                                                                         
                                                                         
                                                                       
                                                                           
                                                                          
                 
   
                                                                             
                                                                         
                                                                             
                                                                               
   
                                                                            
                                                                              
                                                                            
                                                                          
                                                                        
                                                                             
                                                                           
   
                                                             
                                                                            
                                                                              
                                                                          
                                         
   
                                                                         
                                                                             
                                                                              
                                                                          
                                                                             
                                                                          
                                                                       
   
                                                                         
                                                                       
                                                                      
                                                                     
                                                          
   
                                                                              
                                                                            
                                                                          
                         
   
                                                                    
                                                          
   
Buffer
RelationGetBufferForTuple(Relation relation, Size len, Buffer otherBuffer, int options, BulkInsertState bistate, Buffer *vmbuffer, Buffer *vmbuffer_other)
{
  bool use_fsm = !(options & HEAP_INSERT_SKIP_FSM);
  Buffer buffer = InvalidBuffer;
  Page page;
  Size pageFreeSpace = 0, saveFreeSpace = 0;
  BlockNumber targetBlock, otherBlock;
  bool needLock;

  len = MAXALIGN(len);                      

                                                               
  Assert(otherBuffer == InvalidBuffer || !bistate);

     
                                                              
     
  if (len > MaxHeapTupleSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("row is too big: size %zu, maximum size %zu", len, MaxHeapTupleSize)));
  }

                                                                
  saveFreeSpace = RelationGetTargetPageFreeSpace(relation, HEAP_DEFAULT_FILLFACTOR);

  if (otherBuffer != InvalidBuffer)
  {
    otherBlock = BufferGetBlockNumber(otherBuffer);
  }
  else
  {
    otherBlock = InvalidBlockNumber;                                  
  }

     
                                                                             
                                                                      
                                                                        
                                                                          
                                                                             
                                                                           
                                                                             
                                                                        
     
                                                                             
                                  
     
  if (len + saveFreeSpace > MaxHeapTupleSize)
  {
                                            
    targetBlock = InvalidBlockNumber;
    use_fsm = false;
  }
  else if (bistate && bistate->current_buf != InvalidBuffer)
  {
    targetBlock = BufferGetBlockNumber(bistate->current_buf);
  }
  else
  {
    targetBlock = RelationGetTargetBlock(relation);
  }

  if (targetBlock == InvalidBlockNumber && use_fsm)
  {
       
                                                                    
               
       
    targetBlock = GetPageWithFreeSpace(relation, len + saveFreeSpace);

       
                                                                        
                                                                           
                                                      
       
    if (targetBlock == InvalidBlockNumber)
    {
      BlockNumber nblocks = RelationGetNumberOfBlocks(relation);

      if (nblocks > 0)
      {
        targetBlock = nblocks - 1;
      }
    }
  }

loop:
  while (targetBlock != InvalidBlockNumber)
  {
       
                                                                      
                                                                           
                                                
       
                                                                      
                                                                           
                                                                           
                                                                         
                                                                         
                                                                     
                                                                  
       
    if (otherBuffer == InvalidBuffer)
    {
                     
      buffer = ReadBufferBI(relation, targetBlock, RBM_NORMAL, bistate);
      if (PageIsAllVisible(BufferGetPage(buffer)))
      {
        visibilitymap_pin(relation, targetBlock, vmbuffer);
      }
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    }
    else if (otherBlock == targetBlock)
    {
                          
      buffer = otherBuffer;
      if (PageIsAllVisible(BufferGetPage(buffer)))
      {
        visibilitymap_pin(relation, targetBlock, vmbuffer);
      }
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    }
    else if (otherBlock < targetBlock)
    {
                                   
      buffer = ReadBuffer(relation, targetBlock);
      if (PageIsAllVisible(BufferGetPage(buffer)))
      {
        visibilitymap_pin(relation, targetBlock, vmbuffer);
      }
      LockBuffer(otherBuffer, BUFFER_LOCK_EXCLUSIVE);
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    }
    else
    {
                                    
      buffer = ReadBuffer(relation, targetBlock);
      if (PageIsAllVisible(BufferGetPage(buffer)))
      {
        visibilitymap_pin(relation, targetBlock, vmbuffer);
      }
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
      LockBuffer(otherBuffer, BUFFER_LOCK_EXCLUSIVE);
    }

       
                                                                         
                                                                       
                                                                          
                                                                      
                                                                       
                                                                          
                                                                       
              
       
                                                                         
                                                                           
                                                                        
                                               
       
                                                                        
                                                                      
                                                                       
                                                                      
             
       
    if (otherBuffer == InvalidBuffer || targetBlock <= otherBlock)
    {
      GetVisibilityMapPins(relation, buffer, otherBuffer, targetBlock, otherBlock, vmbuffer, vmbuffer_other);
    }
    else
    {
      GetVisibilityMapPins(relation, otherBuffer, buffer, otherBlock, targetBlock, vmbuffer_other, vmbuffer);
    }

       
                                                                         
                   
       
    page = BufferGetPage(buffer);

       
                                                                         
                                                                          
                                                                          
                 
       
    if (PageIsNew(page))
    {
      PageInit(page, BufferGetPageSize(buffer), 0);
      MarkBufferDirty(buffer);
    }

    pageFreeSpace = PageGetHeapFreeSpace(page);
    if (len + saveFreeSpace <= pageFreeSpace)
    {
                                                      
      RelationSetTargetBlock(relation, targetBlock);
      return buffer;
    }

       
                                                                       
                                                                         
                                                                           
                   
       
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
    if (otherBuffer == InvalidBuffer)
    {
      ReleaseBuffer(buffer);
    }
    else if (otherBlock != targetBlock)
    {
      LockBuffer(otherBuffer, BUFFER_LOCK_UNLOCK);
      ReleaseBuffer(buffer);
    }

                                                             
    if (!use_fsm)
    {
      break;
    }

       
                                                                         
               
       
    targetBlock = RecordAndGetPageWithFreeSpace(relation, targetBlock, pageFreeSpace, len + saveFreeSpace);
  }

     
                                  
     
                                                                             
                                                                           
                                                                            
                              
     
  needLock = !RELATION_IS_LOCAL(relation);

     
                                                                           
                                                                            
                                                                          
                                                                
     
  if (needLock)
  {
    if (!use_fsm)
    {
      LockRelationForExtension(relation, ExclusiveLock);
    }
    else if (!ConditionalLockRelationForExtension(relation, ExclusiveLock))
    {
                                                           
      LockRelationForExtension(relation, ExclusiveLock);

         
                                                                       
                                      
         
      targetBlock = GetPageWithFreeSpace(relation, len + saveFreeSpace);

         
                                                                    
                                                               
         
      if (targetBlock != InvalidBlockNumber)
      {
        UnlockRelationForExtension(relation, ExclusiveLock);
        goto loop;
      }

                                
      RelationAddExtraBlocks(relation, bistate);
    }
  }

     
                                                                            
                                                 
     
                                                                             
                                                                             
                                                                          
                                                        
     
  buffer = ReadBufferBI(relation, P_NEW, RBM_ZERO_AND_LOCK, bistate);

     
                                                                            
                                                                         
                                  
     
  page = BufferGetPage(buffer);

  if (!PageIsNew(page))
  {
    elog(ERROR, "page %u of relation \"%s\" should be empty but is not", BufferGetBlockNumber(buffer), RelationGetRelationName(relation));
  }

  PageInit(page, BufferGetPageSize(buffer), 0);
  MarkBufferDirty(buffer);

     
                                                                             
                             
     
  if (needLock)
  {
    UnlockRelationForExtension(relation, ExclusiveLock);
  }

     
                                                                         
                                                                             
                                                                            
                                                                             
                                                                      
                                                                          
                                               
     
                                                                    
                                                                       
                                                              
     
  if (otherBuffer != InvalidBuffer)
  {
    Assert(otherBuffer != buffer);
    targetBlock = BufferGetBlockNumber(buffer);
    Assert(targetBlock > otherBlock);

    if (unlikely(!ConditionalLockBuffer(otherBuffer)))
    {
      LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
      LockBuffer(otherBuffer, BUFFER_LOCK_EXCLUSIVE);
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    }

       
                                                                     
                                                                      
                                                                         
                                                                        
                                    
       
    GetVisibilityMapPins(relation, otherBuffer, buffer, otherBlock, targetBlock, vmbuffer_other, vmbuffer);

       
                                                                  
                                                                         
                                                                          
                            
       
    if (len > PageGetHeapFreeSpace(page))
    {
      LockBuffer(otherBuffer, BUFFER_LOCK_UNLOCK);
      UnlockReleaseBuffer(buffer);

      goto loop;
    }
  }
  else if (len > PageGetHeapFreeSpace(page))
  {
                                                          
    elog(PANIC, "tuple is too big: size %zu", len);
  }

     
                                                                
     
                                                                           
                                                                       
                                                                       
                                                                         
                                                                      
     
  RelationSetTargetBlock(relation, BufferGetBlockNumber(buffer));

  return buffer;
}
