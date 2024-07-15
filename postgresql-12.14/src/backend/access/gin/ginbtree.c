                                                                            
   
              
                                                                            
   
   
                                                                         
                                                                        
   
                  
                                       
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "storage/predicate.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static void
ginFindParents(GinBtree btree, GinBtreeStack *stack);
static bool
ginPlaceToPage(GinBtree btree, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, Buffer childbuf, GinStatsData *buildStats);
static void
ginFinishSplit(GinBtree btree, GinBtreeStack *stack, bool freestack, GinStatsData *buildStats);

   
                                            
   
int
ginTraverseLock(Buffer buffer, bool searchMode)
{
  Page page;
  int access = GIN_SHARE;

  LockBuffer(buffer, GIN_SHARE);
  page = BufferGetPage(buffer);
  if (GinPageIsLeaf(page))
  {
    if (searchMode == false)
    {
                                     
      LockBuffer(buffer, GIN_UNLOCK);
      LockBuffer(buffer, GIN_EXCLUSIVE);

                                                      
      if (!GinPageIsLeaf(page))
      {
                                               
        LockBuffer(buffer, GIN_UNLOCK);
        LockBuffer(buffer, GIN_SHARE);
      }
      else
      {
        access = GIN_EXCLUSIVE;
      }
    }
  }

  return access;
}

   
                                                                            
                                                                        
                                                                          
                       
   
                                                                            
                                                                               
                                               
   
                                                                          
             
   
GinBtreeStack *
ginFindLeafPage(GinBtree btree, bool searchMode, bool rootConflictCheck, Snapshot snapshot)
{
  GinBtreeStack *stack;

  stack = (GinBtreeStack *)palloc(sizeof(GinBtreeStack));
  stack->blkno = btree->rootBlkno;
  stack->buffer = ReadBuffer(btree->index, btree->rootBlkno);
  stack->parent = NULL;
  stack->predictNumber = 1;

  if (rootConflictCheck)
  {
    CheckForSerializableConflictIn(btree->index, NULL, stack->buffer);
  }

  for (;;)
  {
    Page page;
    BlockNumber child;
    int access;

    stack->off = InvalidOffsetNumber;

    page = BufferGetPage(stack->buffer);
    TestForOldSnapshot(snapshot, btree->index, page);

    access = ginTraverseLock(stack->buffer, searchMode);

       
                                                                          
                             
       
    if (!searchMode && GinPageIsIncompleteSplit(page))
    {
      ginFinishSplit(btree, stack, false, NULL);
    }

       
                                                                       
                                                          
       
    while (btree->fullScan == false && stack->blkno != btree->rootBlkno && btree->isMoveRight(btree, page))
    {
      BlockNumber rightlink = GinPageGetOpaque(page)->rightlink;

      if (rightlink == InvalidBlockNumber)
      {
                            
        break;
      }

      stack->buffer = ginStepRight(stack->buffer, btree->index, access);
      stack->blkno = rightlink;
      page = BufferGetPage(stack->buffer);
      TestForOldSnapshot(snapshot, btree->index, page);

      if (!searchMode && GinPageIsIncompleteSplit(page))
      {
        ginFinishSplit(btree, stack, false, NULL);
      }
    }

    if (GinPageIsLeaf(page))                                   
    {
      return stack;
    }

                                                       
    child = btree->findChildPage(btree, stack);

    LockBuffer(stack->buffer, GIN_UNLOCK);
    Assert(child != InvalidBlockNumber);
    Assert(stack->blkno != child);

    if (searchMode)
    {
                                                     
      stack->blkno = child;
      stack->buffer = ReleaseAndReadBuffer(stack->buffer, btree->index, stack->blkno);
    }
    else
    {
      GinBtreeStack *ptr = (GinBtreeStack *)palloc(sizeof(GinBtreeStack));

      ptr->parent = stack;
      stack = ptr;
      stack->blkno = child;
      stack->buffer = ReadBuffer(btree->index, stack->blkno);
      stack->predictNumber = 1;
    }
  }
}

   
                                 
   
                                                                             
                                                                    
                   
   
Buffer
ginStepRight(Buffer buffer, Relation index, int lockmode)
{
  Buffer nextbuffer;
  Page page = BufferGetPage(buffer);
  bool isLeaf = GinPageIsLeaf(page);
  bool isData = GinPageIsData(page);
  BlockNumber blkno = GinPageGetOpaque(page)->rightlink;

  nextbuffer = ReadBuffer(index, blkno);
  LockBuffer(nextbuffer, lockmode);
  UnlockReleaseBuffer(buffer);

                                                                    
  page = BufferGetPage(nextbuffer);
  if (isLeaf != GinPageIsLeaf(page) || isData != GinPageIsData(page))
  {
    elog(ERROR, "right sibling of GIN page is of different type");
  }

  return nextbuffer;
}

void
freeGinBtreeStack(GinBtreeStack *stack)
{
  while (stack)
  {
    GinBtreeStack *tmp = stack->parent;

    if (stack->buffer != InvalidBuffer)
    {
      ReleaseBuffer(stack->buffer);
    }

    pfree(stack);
    stack = tmp;
  }
}

   
                                                                             
                                                                        
                                         
   
static void
ginFindParents(GinBtree btree, GinBtreeStack *stack)
{
  Page page;
  Buffer buffer;
  BlockNumber blkno, leftmostBlkno;
  OffsetNumber offset;
  GinBtreeStack *root;
  GinBtreeStack *ptr;

     
                                                                        
           
     
                                                                         
                                                                  
     
  root = stack->parent;
  while (root->parent)
  {
    ReleaseBuffer(root->buffer);
    root = root->parent;
  }

  Assert(root->blkno == btree->rootBlkno);
  Assert(BufferGetBlockNumber(root->buffer) == btree->rootBlkno);
  root->off = InvalidOffsetNumber;

  blkno = root->blkno;
  buffer = root->buffer;
  offset = InvalidOffsetNumber;

  ptr = (GinBtreeStack *)palloc(sizeof(GinBtreeStack));

  for (;;)
  {
    LockBuffer(buffer, GIN_EXCLUSIVE);
    page = BufferGetPage(buffer);
    if (GinPageIsLeaf(page))
    {
      elog(ERROR, "Lost path");
    }

    if (GinPageIsIncompleteSplit(page))
    {
      Assert(blkno != btree->rootBlkno);
      ptr->blkno = blkno;
      ptr->buffer = buffer;

         
                                                                      
                                                         
         
      ptr->parent = root;
      ptr->off = InvalidOffsetNumber;

      ginFinishSplit(btree, ptr, false, NULL);
    }

    leftmostBlkno = btree->getLeftMostChild(btree, page);

    while ((offset = btree->findChildPtr(btree, page, stack->blkno, InvalidOffsetNumber)) == InvalidOffsetNumber)
    {
      blkno = GinPageGetOpaque(page)->rightlink;
      if (blkno == InvalidBlockNumber)
      {
        UnlockReleaseBuffer(buffer);
        break;
      }
      buffer = ginStepRight(buffer, btree->index, GIN_EXCLUSIVE);
      page = BufferGetPage(buffer);

                                                  
      if (GinPageIsIncompleteSplit(page))
      {
        Assert(blkno != btree->rootBlkno);
        ptr->blkno = blkno;
        ptr->buffer = buffer;
        ptr->parent = root;
        ptr->off = InvalidOffsetNumber;

        ginFinishSplit(btree, ptr, false, NULL);
      }
    }

    if (blkno != InvalidBlockNumber)
    {
      ptr->blkno = blkno;
      ptr->buffer = buffer;
      ptr->parent = root;                                              
                                       
      ptr->off = offset;
      stack->parent = ptr;
      return;
    }

                                    
    blkno = leftmostBlkno;
    buffer = ReadBuffer(btree->index, blkno);
  }
}

   
                                
   
                                                                                
                                                                            
                                                       
   
                                                                          
                                                                            
                                                                            
                                                          
   
                                                         
                                    
   
static bool
ginPlaceToPage(GinBtree btree, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, Buffer childbuf, GinStatsData *buildStats)
{
  Page page = BufferGetPage(stack->buffer);
  bool result;
  GinPlaceToPageRC rc;
  uint16 xlflags = 0;
  Page childpage = NULL;
  Page newlpage = NULL, newrpage = NULL;
  void *ptp_workspace = NULL;
  MemoryContext tmpCxt;
  MemoryContext oldCxt;

     
                                                                             
                                                                           
                                                                            
                        
     
  tmpCxt = AllocSetContextCreate(CurrentMemoryContext, "ginPlaceToPage temporary context", ALLOCSET_DEFAULT_SIZES);
  oldCxt = MemoryContextSwitchTo(tmpCxt);

  if (GinPageIsData(page))
  {
    xlflags |= GIN_INSERT_ISDATA;
  }
  if (GinPageIsLeaf(page))
  {
    xlflags |= GIN_INSERT_ISLEAF;
    Assert(!BufferIsValid(childbuf));
    Assert(updateblkno == InvalidBlockNumber);
  }
  else
  {
    Assert(BufferIsValid(childbuf));
    Assert(updateblkno != InvalidBlockNumber);
    childpage = BufferGetPage(childbuf);
  }

     
                                                                            
                                                                      
                                                                            
                                                 
     
  rc = btree->beginPlaceToPage(btree, stack->buffer, stack, insertdata, updateblkno, &ptp_workspace, &newlpage, &newrpage);

  if (rc == GPTP_NO_WORK)
  {
                       
    result = true;
  }
  else if (rc == GPTP_INSERT)
  {
                                            
    START_CRIT_SECTION();

    if (RelationNeedsWAL(btree->index) && !btree->isBuild)
    {
      XLogBeginInsert();
      XLogRegisterBuffer(0, stack->buffer, REGBUF_STANDARD);
      if (BufferIsValid(childbuf))
      {
        XLogRegisterBuffer(1, childbuf, REGBUF_STANDARD);
      }
    }

                                                                  
    btree->execPlaceToPage(btree, stack->buffer, stack, insertdata, updateblkno, ptp_workspace);

    MarkBufferDirty(stack->buffer);

                                                                        
    if (BufferIsValid(childbuf))
    {
      GinPageGetOpaque(childpage)->flags &= ~GIN_INCOMPLETE_SPLIT;
      MarkBufferDirty(childbuf);
    }

    if (RelationNeedsWAL(btree->index) && !btree->isBuild)
    {
      XLogRecPtr recptr;
      ginxlogInsert xlrec;
      BlockIdData childblknos[2];

      xlrec.flags = xlflags;

      XLogRegisterData((char *)&xlrec, sizeof(ginxlogInsert));

         
                                                                   
                   
         
      if (BufferIsValid(childbuf))
      {
        BlockIdSet(&childblknos[0], BufferGetBlockNumber(childbuf));
        BlockIdSet(&childblknos[1], GinPageGetOpaque(childpage)->rightlink);
        XLogRegisterData((char *)childblknos, sizeof(BlockIdData) * 2);
      }

      recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_INSERT);
      PageSetLSN(page, recptr);
      if (BufferIsValid(childbuf))
      {
        PageSetLSN(childpage, recptr);
      }
    }

    END_CRIT_SECTION();

                                
    result = true;
  }
  else if (rc == GPTP_SPLIT)
  {
       
                                                                           
                                                                          
                                                        
       
    Buffer rbuffer;
    BlockNumber savedRightLink;
    ginxlogSplit data;
    Buffer lbuffer = InvalidBuffer;
    Page newrootpg = NULL;

                                                       
    rbuffer = GinNewBuffer(btree->index);

                                                
    if (buildStats)
    {
      if (btree->isData)
      {
        buildStats->nDataPages++;
      }
      else
      {
        buildStats->nEntryPages++;
      }
    }

    savedRightLink = GinPageGetOpaque(page)->rightlink;

                                     
    data.node = btree->index->rd_node;
    data.flags = xlflags;
    if (BufferIsValid(childbuf))
    {
      data.leftChildBlkno = BufferGetBlockNumber(childbuf);
      data.rightChildBlkno = GinPageGetOpaque(childpage)->rightlink;
    }
    else
    {
      data.leftChildBlkno = data.rightChildBlkno = InvalidBlockNumber;
    }

    if (stack->parent == NULL)
    {
         
                                                                      
                                                             
         
      lbuffer = GinNewBuffer(btree->index);

                                                       
      if (buildStats)
      {
        if (btree->isData)
        {
          buildStats->nDataPages++;
        }
        else
        {
          buildStats->nEntryPages++;
        }
      }

      data.rrlink = InvalidBlockNumber;
      data.flags |= GIN_SPLIT_ROOT;

      GinPageGetOpaque(newrpage)->rightlink = InvalidBlockNumber;
      GinPageGetOpaque(newlpage)->rightlink = BufferGetBlockNumber(rbuffer);

         
                                                                        
                                                                    
                                                                        
                                
         
      newrootpg = PageGetTempPage(newrpage);
      GinInitPage(newrootpg, GinPageGetOpaque(newlpage)->flags & ~(GIN_LEAF | GIN_COMPRESSED), BLCKSZ);

      btree->fillRoot(btree, newrootpg, BufferGetBlockNumber(lbuffer), newlpage, BufferGetBlockNumber(rbuffer), newrpage);

      if (GinPageIsLeaf(BufferGetPage(stack->buffer)))
      {

        PredicateLockPageSplit(btree->index, BufferGetBlockNumber(stack->buffer), BufferGetBlockNumber(lbuffer));

        PredicateLockPageSplit(btree->index, BufferGetBlockNumber(stack->buffer), BufferGetBlockNumber(rbuffer));
      }
    }
    else
    {
                                     
      data.rrlink = savedRightLink;

      GinPageGetOpaque(newrpage)->rightlink = savedRightLink;
      GinPageGetOpaque(newlpage)->flags |= GIN_INCOMPLETE_SPLIT;
      GinPageGetOpaque(newlpage)->rightlink = BufferGetBlockNumber(rbuffer);

      if (GinPageIsLeaf(BufferGetPage(stack->buffer)))
      {

        PredicateLockPageSplit(btree->index, BufferGetBlockNumber(stack->buffer), BufferGetBlockNumber(rbuffer));
      }
    }

       
                                                                         
                                                                
                                                                          
       
                                                                         
                                     
       

    START_CRIT_SECTION();

    MarkBufferDirty(rbuffer);
    MarkBufferDirty(stack->buffer);

       
                                                           
       
    if (stack->parent == NULL)
    {
                                                     
      MarkBufferDirty(lbuffer);
      memcpy(page, newrootpg, BLCKSZ);
      memcpy(BufferGetPage(lbuffer), newlpage, BLCKSZ);
      memcpy(BufferGetPage(rbuffer), newrpage, BLCKSZ);
    }
    else
    {
                                                  
      memcpy(page, newlpage, BLCKSZ);
      memcpy(BufferGetPage(rbuffer), newrpage, BLCKSZ);
    }

                                                                   
    if (BufferIsValid(childbuf))
    {
      GinPageGetOpaque(childpage)->flags &= ~GIN_INCOMPLETE_SPLIT;
      MarkBufferDirty(childbuf);
    }

                          
    if (RelationNeedsWAL(btree->index) && !btree->isBuild)
    {
      XLogRecPtr recptr;

      XLogBeginInsert();

         
                                                                      
                                                                       
                               
         
      if (stack->parent == NULL)
      {
        XLogRegisterBuffer(0, lbuffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);
        XLogRegisterBuffer(1, rbuffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);
        XLogRegisterBuffer(2, stack->buffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);
      }
      else
      {
        XLogRegisterBuffer(0, stack->buffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);
        XLogRegisterBuffer(1, rbuffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);
      }
      if (BufferIsValid(childbuf))
      {
        XLogRegisterBuffer(3, childbuf, REGBUF_STANDARD);
      }

      XLogRegisterData((char *)&data, sizeof(ginxlogSplit));

      recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_SPLIT);

      PageSetLSN(page, recptr);
      PageSetLSN(BufferGetPage(rbuffer), recptr);
      if (stack->parent == NULL)
      {
        PageSetLSN(BufferGetPage(lbuffer), recptr);
      }
      if (BufferIsValid(childbuf))
      {
        PageSetLSN(childpage, recptr);
      }
    }
    END_CRIT_SECTION();

       
                                                                    
                                                                    
       
    UnlockReleaseBuffer(rbuffer);
    if (stack->parent == NULL)
    {
      UnlockReleaseBuffer(lbuffer);
    }

       
                                                                    
                                                                         
                   
       
    result = (stack->parent == NULL);
  }
  else
  {
    elog(ERROR, "invalid return code from GIN placeToPage method: %d", rc);
    result = false;                          
  }

                             
  MemoryContextSwitchTo(oldCxt);
  MemoryContextDelete(tmpCxt);

  return result;
}

   
                                                                        
   
                                                  
   
                                                                         
                                                                            
                                                                             
                               
   
static void
ginFinishSplit(GinBtree btree, GinBtreeStack *stack, bool freestack, GinStatsData *buildStats)
{
  Page page;
  bool done;
  bool first = true;

     
                                                                            
                                                                           
                                                       
     
  if (!freestack)
  {
    elog(DEBUG1, "finishing incomplete split of block %u in gin index \"%s\"", stack->blkno, RelationGetRelationName(btree->index));
  }

                                                                     
  do
  {
    GinBtreeStack *parent = stack->parent;
    void *insertdata;
    BlockNumber updateblkno;

                               
    LockBuffer(parent->buffer, GIN_EXCLUSIVE);

       
                                                                           
                                           
       
                                                                          
                                                                           
                                                                         
                   
       
    if (GinPageIsIncompleteSplit(BufferGetPage(parent->buffer)))
    {
      ginFinishSplit(btree, parent, false, buildStats);
    }

                                   
    page = BufferGetPage(parent->buffer);
    while ((parent->off = btree->findChildPtr(btree, page, stack->blkno, parent->off)) == InvalidOffsetNumber)
    {
      if (GinPageRightMost(page))
      {
           
                                                                   
                           
           
        LockBuffer(parent->buffer, GIN_UNLOCK);
        ginFindParents(btree, stack);
        parent = stack->parent;
        Assert(parent != NULL);
        break;
      }

      parent->buffer = ginStepRight(parent->buffer, btree->index, GIN_EXCLUSIVE);
      parent->blkno = BufferGetBlockNumber(parent->buffer);
      page = BufferGetPage(parent->buffer);

      if (GinPageIsIncompleteSplit(BufferGetPage(parent->buffer)))
      {
        ginFinishSplit(btree, parent, false, buildStats);
      }
    }

                             
    insertdata = btree->prepareDownlink(btree, stack->buffer);
    updateblkno = GinPageGetOpaque(BufferGetPage(stack->buffer))->rightlink;
    done = ginPlaceToPage(btree, parent, insertdata, updateblkno, stack->buffer, buildStats);
    pfree(insertdata);

       
                                                                         
                                                                        
                                                                        
                                                           
       
    if (!first || freestack)
    {
      LockBuffer(stack->buffer, GIN_UNLOCK);
    }
    if (freestack)
    {
      ReleaseBuffer(stack->buffer);
      pfree(stack);
    }
    stack = parent;

    first = false;
  } while (!done);

                         
  LockBuffer(stack->buffer, GIN_UNLOCK);

  if (freestack)
  {
    freeGinBtreeStack(stack);
  }
}

   
                                              
   
                                                                         
                                                                           
                                                   
   
                                                                              
                              
   
                                                                     
   
void
ginInsertValue(GinBtree btree, GinBtreeStack *stack, void *insertdata, GinStatsData *buildStats)
{
  bool done;

                                                                       
  if (GinPageIsIncompleteSplit(BufferGetPage(stack->buffer)))
  {
    ginFinishSplit(btree, stack, false, buildStats);
  }

  done = ginPlaceToPage(btree, stack, insertdata, InvalidBlockNumber, InvalidBuffer, buildStats);
  if (done)
  {
    LockBuffer(stack->buffer, GIN_UNLOCK);
    freeGinBtreeStack(stack);
  }
  else
  {
    ginFinishSplit(btree, stack, true, buildStats);
  }
}
