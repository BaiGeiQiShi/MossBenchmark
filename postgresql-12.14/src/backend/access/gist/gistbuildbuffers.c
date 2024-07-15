                                                                            
   
                      
                                                                          
   
   
                                                                         
                                                                        
   
                  
                                                
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/gist_private.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/buffile.h"
#include "storage/bufmgr.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static GISTNodeBufferPage *
gistAllocateNewPageBuffer(GISTBuildBuffers *gfbb);
static void
gistAddLoadedBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer);
static void
gistLoadNodeBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer);
static void
gistUnloadNodeBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer);
static void
gistPlaceItupToPage(GISTNodeBufferPage *pageBuffer, IndexTuple item);
static void
gistGetItupFromPage(GISTNodeBufferPage *pageBuffer, IndexTuple *item);
static long
gistBuffersGetFreeBlock(GISTBuildBuffers *gfbb);
static void
gistBuffersReleaseBlock(GISTBuildBuffers *gfbb, long blocknum);

static void
ReadTempFileBlock(BufFile *file, long blknum, void *ptr);
static void
WriteTempFileBlock(BufFile *file, long blknum, void *ptr);

   
                                  
   
GISTBuildBuffers *
gistInitBuildBuffers(int pagesPerBuffer, int levelStep, int maxLevel)
{
  GISTBuildBuffers *gfbb;
  HASHCTL hashCtl;

  gfbb = palloc(sizeof(GISTBuildBuffers));
  gfbb->pagesPerBuffer = pagesPerBuffer;
  gfbb->levelStep = levelStep;

     
                                                                          
             
     
  gfbb->pfile = BufFileCreateTemp(false);
  gfbb->nFileBlocks = 0;

                                        
  gfbb->nFreeBlocks = 0;
  gfbb->freeBlocksLen = 32;
  gfbb->freeBlocks = (long *)palloc(gfbb->freeBlocksLen * sizeof(long));

     
                                                                           
                                                             
     
  gfbb->context = CurrentMemoryContext;

     
                                                                      
              
     
  memset(&hashCtl, 0, sizeof(hashCtl));
  hashCtl.keysize = sizeof(BlockNumber);
  hashCtl.entrysize = sizeof(GISTNodeBuffer);
  hashCtl.hcxt = CurrentMemoryContext;
  gfbb->nodeBuffersTab = hash_create("gistbuildbuffers", 1024, &hashCtl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  gfbb->bufferEmptyingQueue = NIL;

     
                                                                           
                                                      
     
  gfbb->buffersOnLevelsLen = 1;
  gfbb->buffersOnLevels = (List **)palloc(sizeof(List *) * gfbb->buffersOnLevelsLen);
  gfbb->buffersOnLevels[0] = NIL;

     
                                                                         
                       
     
  gfbb->loadedBuffersLen = 32;
  gfbb->loadedBuffers = (GISTNodeBuffer **)palloc(gfbb->loadedBuffersLen * sizeof(GISTNodeBuffer *));
  gfbb->loadedBuffersCount = 0;

  gfbb->rootlevel = maxLevel;

  return gfbb;
}

   
                                                                      
                      
   
GISTNodeBuffer *
gistGetNodeBuffer(GISTBuildBuffers *gfbb, GISTSTATE *giststate, BlockNumber nodeBlocknum, int level)
{
  GISTNodeBuffer *nodeBuffer;
  bool found;

                                      
  nodeBuffer = (GISTNodeBuffer *)hash_search(gfbb->nodeBuffersTab, (const void *)&nodeBlocknum, HASH_ENTER, &found);
  if (!found)
  {
       
                                                                     
       
    MemoryContext oldcxt = MemoryContextSwitchTo(gfbb->context);

                                                                            
    nodeBuffer->blocksCount = 0;
    nodeBuffer->pageBlocknum = InvalidBlockNumber;
    nodeBuffer->pageBuffer = NULL;
    nodeBuffer->queuedForEmptying = false;
    nodeBuffer->isTemp = false;
    nodeBuffer->level = level;

       
                                                                     
                                        
       
    if (level >= gfbb->buffersOnLevelsLen)
    {
      int i;

      gfbb->buffersOnLevels = (List **)repalloc(gfbb->buffersOnLevels, (level + 1) * sizeof(List *));

                                           
      for (i = gfbb->buffersOnLevelsLen; i <= level; i++)
      {
        gfbb->buffersOnLevels[i] = NIL;
      }
      gfbb->buffersOnLevelsLen = level + 1;
    }

       
                                                                         
                                                                        
                                                                        
                                                                           
                                                                         
                                                                      
                                                                   
                                                                        
                             
       
    gfbb->buffersOnLevels[level] = lcons(nodeBuffer, gfbb->buffersOnLevels[level]);

    MemoryContextSwitchTo(oldcxt);
  }

  return nodeBuffer;
}

   
                                      
   
static GISTNodeBufferPage *
gistAllocateNewPageBuffer(GISTBuildBuffers *gfbb)
{
  GISTNodeBufferPage *pageBuffer;

  pageBuffer = (GISTNodeBufferPage *)MemoryContextAllocZero(gfbb->context, BLCKSZ);
  pageBuffer->prev = InvalidBlockNumber;

                           
  PAGE_FREE_SPACE(pageBuffer) = BLCKSZ - BUFFER_PAGE_DATA_OFFSET;
  return pageBuffer;
}

   
                                                  
   
static void
gistAddLoadedBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer)
{
                                                 
  if (nodeBuffer->isTemp)
  {
    return;
  }

                                   
  if (gfbb->loadedBuffersCount >= gfbb->loadedBuffersLen)
  {
    gfbb->loadedBuffersLen *= 2;
    gfbb->loadedBuffers = (GISTNodeBuffer **)repalloc(gfbb->loadedBuffers, gfbb->loadedBuffersLen * sizeof(GISTNodeBuffer *));
  }

  gfbb->loadedBuffers[gfbb->loadedBuffersCount] = nodeBuffer;
  gfbb->loadedBuffersCount++;
}

   
                                                   
   
static void
gistLoadNodeBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer)
{
                                                
  if (!nodeBuffer->pageBuffer && nodeBuffer->blocksCount > 0)
  {
                                  
    nodeBuffer->pageBuffer = gistAllocateNewPageBuffer(gfbb);

                                        
    ReadTempFileBlock(gfbb->pfile, nodeBuffer->pageBlocknum, nodeBuffer->pageBuffer);

                                 
    gistBuffersReleaseBlock(gfbb, nodeBuffer->pageBlocknum);

                                    
    gistAddLoadedBuffer(gfbb, nodeBuffer);
    nodeBuffer->pageBlocknum = InvalidBlockNumber;
  }
}

   
                                               
   
static void
gistUnloadNodeBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer)
{
                                           
  if (nodeBuffer->pageBuffer)
  {
    BlockNumber blkno;

                             
    blkno = gistBuffersGetFreeBlock(gfbb);

                                           
    WriteTempFileBlock(gfbb->pfile, blkno, nodeBuffer->pageBuffer);

                                  
    pfree(nodeBuffer->pageBuffer);
    nodeBuffer->pageBuffer = NULL;

                           
    nodeBuffer->pageBlocknum = blkno;
  }
}

   
                                                     
   
void
gistUnloadNodeBuffers(GISTBuildBuffers *gfbb)
{
  int i;

                                                                 
  for (i = 0; i < gfbb->loadedBuffersCount; i++)
  {
    gistUnloadNodeBuffer(gfbb, gfbb->loadedBuffers[i]);
  }

                                                           
  gfbb->loadedBuffersCount = 0;
}

   
                                   
   
static void
gistPlaceItupToPage(GISTNodeBufferPage *pageBuffer, IndexTuple itup)
{
  Size itupsz = IndexTupleSize(itup);
  char *ptr;

                                        
  Assert(PAGE_FREE_SPACE(pageBuffer) >= MAXALIGN(itupsz));

                                                                        
  PAGE_FREE_SPACE(pageBuffer) -= MAXALIGN(itupsz);

                                                                    
  ptr = (char *)pageBuffer + BUFFER_PAGE_DATA_OFFSET + PAGE_FREE_SPACE(pageBuffer);

                                   
  memcpy(ptr, itup, itupsz);
}

   
                                                           
   
static void
gistGetItupFromPage(GISTNodeBufferPage *pageBuffer, IndexTuple *itup)
{
  IndexTuple ptr;
  Size itupsz;

  Assert(!PAGE_IS_EMPTY(pageBuffer));                              

                                       
  ptr = (IndexTuple)((char *)pageBuffer + BUFFER_PAGE_DATA_OFFSET + PAGE_FREE_SPACE(pageBuffer));
  itupsz = IndexTupleSize(ptr);

                                
  *itup = (IndexTuple)palloc(itupsz);
  memcpy(*itup, ptr, itupsz);

                                                
  PAGE_FREE_SPACE(pageBuffer) += MAXALIGN(itupsz);
}

   
                                       
   
void
gistPushItupToNodeBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer, IndexTuple itup)
{
     
                                                                          
                                      
     
  MemoryContext oldcxt = MemoryContextSwitchTo(gfbb->context);

     
                                                              
     
  if (nodeBuffer->blocksCount == 0)
  {
    nodeBuffer->pageBuffer = gistAllocateNewPageBuffer(gfbb);
    nodeBuffer->blocksCount = 1;
    gistAddLoadedBuffer(gfbb, nodeBuffer);
  }

                                                                    
  if (!nodeBuffer->pageBuffer)
  {
    gistLoadNodeBuffer(gfbb, nodeBuffer);
  }

     
                                                                    
     
  if (PAGE_NO_SPACE(nodeBuffer->pageBuffer, itup))
  {
       
                                                                 
       
    BlockNumber blkno;

                                       
    blkno = gistBuffersGetFreeBlock(gfbb);
    WriteTempFileBlock(gfbb->pfile, blkno, nodeBuffer->pageBuffer);

       
                                                                         
                                                                  
       
    PAGE_FREE_SPACE(nodeBuffer->pageBuffer) = BLCKSZ - MAXALIGN(offsetof(GISTNodeBufferPage, tupledata));
    nodeBuffer->pageBuffer->prev = blkno;

                                        
    nodeBuffer->blocksCount++;
  }

  gistPlaceItupToPage(nodeBuffer->pageBuffer, itup);

     
                                                                  
     
  if (BUFFER_HALF_FILLED(nodeBuffer, gfbb) && !nodeBuffer->queuedForEmptying)
  {
    gfbb->bufferEmptyingQueue = lcons(nodeBuffer, gfbb->bufferEmptyingQueue);
    nodeBuffer->queuedForEmptying = true;
  }

                              
  MemoryContextSwitchTo(oldcxt);
}

   
                                                                               
                            
   
bool
gistPopItupFromNodeBuffer(GISTBuildBuffers *gfbb, GISTNodeBuffer *nodeBuffer, IndexTuple *itup)
{
     
                                                
     
  if (nodeBuffer->blocksCount <= 0)
  {
    return false;
  }

                                               
  if (!nodeBuffer->pageBuffer)
  {
    gistLoadNodeBuffer(gfbb, nodeBuffer);
  }

     
                                               
     
  gistGetItupFromPage(nodeBuffer->pageBuffer, itup);

     
                                                                             
                                
     
  if (PAGE_IS_EMPTY(nodeBuffer->pageBuffer))
  {
    BlockNumber prevblkno;

       
                                                                        
       
    nodeBuffer->blocksCount--;

       
                                                  
       
    prevblkno = nodeBuffer->pageBuffer->prev;
    if (prevblkno != InvalidBlockNumber)
    {
                                               
      Assert(nodeBuffer->blocksCount > 0);
      ReadTempFileBlock(gfbb->pfile, prevblkno, nodeBuffer->pageBuffer);

         
                                                                     
                                  
         
      gistBuffersReleaseBlock(gfbb, prevblkno);
    }
    else
    {
                                       
      Assert(nodeBuffer->blocksCount == 0);
      pfree(nodeBuffer->pageBuffer);
      nodeBuffer->pageBuffer = NULL;
    }
  }
  return true;
}

   
                                                   
   
static long
gistBuffersGetFreeBlock(GISTBuildBuffers *gfbb)
{
     
                                                                            
                                                                           
                                                 
     
  if (gfbb->nFreeBlocks > 0)
  {
    return gfbb->freeBlocks[--gfbb->nFreeBlocks];
  }
  else
  {
    return gfbb->nFileBlocks++;
  }
}

   
                                    
   
static void
gistBuffersReleaseBlock(GISTBuildBuffers *gfbb, long blocknum)
{
  int ndx;

                                         
  if (gfbb->nFreeBlocks >= gfbb->freeBlocksLen)
  {
    gfbb->freeBlocksLen *= 2;
    gfbb->freeBlocks = (long *)repalloc(gfbb->freeBlocks, gfbb->freeBlocksLen * sizeof(long));
  }

                             
  ndx = gfbb->nFreeBlocks++;
  gfbb->freeBlocks[ndx] = blocknum;
}

   
                                        
   
void
gistFreeBuildBuffers(GISTBuildBuffers *gfbb)
{
                           
  BufFileClose(gfbb->pfile);

                                                                
}

   
                                                                              
                                         
   
typedef struct
{
  GISTENTRY entry[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  GISTPageSplitInfo *splitinfo;
  GISTNodeBuffer *nodeBuffer;
} RelocationBufferInfo;

   
                                                                         
                                                                            
                                                        
   
void
gistRelocateBuildBuffersOnSplit(GISTBuildBuffers *gfbb, GISTSTATE *giststate, Relation r, int level, Buffer buffer, List *splitinfo)
{
  RelocationBufferInfo *relocationBuffersInfos;
  bool found;
  GISTNodeBuffer *nodeBuffer;
  BlockNumber blocknum;
  IndexTuple itup;
  int splitPagesCount = 0, i;
  GISTENTRY entry[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  GISTNodeBuffer oldBuf;
  ListCell *lc;

                                                                         
  if (!LEVEL_HAS_BUFFERS(level, gfbb))
  {
    return;
  }

     
                                               
     
  blocknum = BufferGetBlockNumber(buffer);
  nodeBuffer = hash_search(gfbb->nodeBuffersTab, &blocknum, HASH_FIND, &found);
  if (!found)
  {
                                                           
    return;
  }

     
                                                                          
                                                                        
                                                                         
                                                                      
                                                                           
                                                                        
     
  Assert(blocknum != GIST_ROOT_BLKNO);
  memcpy(&oldBuf, nodeBuffer, sizeof(GISTNodeBuffer));
  oldBuf.isTemp = true;

                                                                    
  nodeBuffer->blocksCount = 0;
  nodeBuffer->pageBuffer = NULL;
  nodeBuffer->pageBlocknum = InvalidBlockNumber;

     
                                                               
     
  splitPagesCount = list_length(splitinfo);
  relocationBuffersInfos = (RelocationBufferInfo *)palloc(sizeof(RelocationBufferInfo) * splitPagesCount);

     
                                                                            
               
     
  i = 0;
  foreach (lc, splitinfo)
  {
    GISTPageSplitInfo *si = (GISTPageSplitInfo *)lfirst(lc);
    GISTNodeBuffer *newNodeBuffer;

                                                            
    gistDeCompressAtt(giststate, r, si->downlink, NULL, (OffsetNumber)0, relocationBuffersInfos[i].entry, relocationBuffersInfos[i].isnull);

       
                                                                           
                                                                         
                                                                          
                                                                         
              
       
    newNodeBuffer = gistGetNodeBuffer(gfbb, giststate, BufferGetBlockNumber(si->buf), level);

    relocationBuffersInfos[i].nodeBuffer = newNodeBuffer;
    relocationBuffersInfos[i].splitinfo = si;

    i++;
  }

     
                                                                          
                                                                             
                                                                            
                                                                          
                       
     
                                                               
     
  while (gistPopItupFromNodeBuffer(gfbb, &oldBuf, &itup))
  {
    float best_penalty[INDEX_MAX_KEYS];
    int i, which;
    IndexTuple newtup;
    RelocationBufferInfo *targetBufferInfo;

    gistDeCompressAtt(giststate, r, itup, NULL, (OffsetNumber)0, entry, isnull);

                                                        
    which = 0;

       
                                                                          
                                                                         
                                                
       
    best_penalty[0] = -1;

       
                                                                           
           
       
    for (i = 0; i < splitPagesCount; i++)
    {
      RelocationBufferInfo *splitPageInfo = &relocationBuffersInfos[i];
      bool zero_penalty;
      int j;

      zero_penalty = true;

                                       
      for (j = 0; j < IndexRelationGetNumberOfKeyAttributes(r); j++)
      {
        float usize;

                                              
        usize = gistpenalty(giststate, j, &splitPageInfo->entry[j], splitPageInfo->isnull[j], &entry[j], isnull[j]);
        if (usize > 0)
        {
          zero_penalty = false;
        }

        if (best_penalty[j] < 0 || usize < best_penalty[j])
        {
             
                                                                   
                                                                    
                                                               
                                                                  
                                                                    
                                                              
                                         
             
          which = i;
          best_penalty[j] = usize;

          if (j < IndexRelationGetNumberOfKeyAttributes(r) - 1)
          {
            best_penalty[j + 1] = -1;
          }
        }
        else if (best_penalty[j] == usize)
        {
             
                                                                    
                                                                    
                                                
             
        }
        else
        {
             
                                                                     
                                                                    
                                          
             
          zero_penalty = false;                               
          break;
        }
      }

         
                                                                         
                                                                         
                    
         
      if (zero_penalty)
      {
        break;
      }
    }

                                                            
    targetBufferInfo = &relocationBuffersInfos[which];

                                           
    gistPushItupToNodeBuffer(gfbb, targetBufferInfo->nodeBuffer, itup);

                                                       
    newtup = gistgetadjusted(r, targetBufferInfo->splitinfo->downlink, itup, giststate);
    if (newtup)
    {
      gistDeCompressAtt(giststate, r, newtup, NULL, (OffsetNumber)0, targetBufferInfo->entry, targetBufferInfo->isnull);

      targetBufferInfo->splitinfo->downlink = newtup;
    }
  }

  pfree(relocationBuffersInfos);
}

   
                                                                         
                                                                         
                             
   

static void
ReadTempFileBlock(BufFile *file, long blknum, void *ptr)
{
  size_t nread;

  if (BufFileSeekBlock(file, blknum) != 0)
  {
    elog(ERROR, "could not seek to block %ld in temporary file", blknum);
  }
  nread = BufFileRead(file, ptr, BLCKSZ);
  if (nread != BLCKSZ)
  {
    elog(ERROR, "could not read temporary file: read only %zu of %zu bytes", nread, (size_t)BLCKSZ);
  }
}

static void
WriteTempFileBlock(BufFile *file, long blknum, void *ptr)
{
  if (BufFileSeekBlock(file, blknum) != 0)
  {
    elog(ERROR, "could not seek to block %ld in temporary file", blknum);
  }
  BufFileWrite(file, ptr, BLCKSZ);
}
