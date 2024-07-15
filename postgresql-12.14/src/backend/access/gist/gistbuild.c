                                                                            
   
               
                                                      
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/genam.h"
#include "access/gist_private.h"
#include "access/gistxlog.h"
#include "access/tableam.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "utils/memutils.h"
#include "utils/rel.h"

                                                                              
#define BUFFERING_MODE_SWITCH_CHECK_STEP 256

   
                                                                             
                                                                            
                                                                      
                   
   
#define BUFFERING_MODE_TUPLE_SIZE_STATS_TARGET 4096

typedef enum
{
  GIST_BUFFERING_DISABLED,                                              
                                       
  GIST_BUFFERING_AUTO,                                                  
                                                                          
                                    
  GIST_BUFFERING_STATS,                                                
                                                                      
                                     
  GIST_BUFFERING_ACTIVE                                 
} GistBufferingMode;

                                                  
typedef struct
{
  Relation indexrel;
  Relation heaprel;
  GISTSTATE *giststate;

  int64 indtuples;                                   
  int64 indtuplesSize;                                       

  Size freespace;                                             

     
                                                                          
                                                                         
                                                       
     
  GISTBuildBuffers *gfbb;
  HTAB *parentMap;

  GistBufferingMode bufferingMode;
} GISTBuildState;

                                      
static void
gistInitBuffering(GISTBuildState *buildstate);
static int
calculatePagesPerBuffer(GISTBuildState *buildstate, int levelStep);
static void
gistBuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state);
static void
gistBufferingBuildInsert(GISTBuildState *buildstate, IndexTuple itup);
static bool
gistProcessItup(GISTBuildState *buildstate, IndexTuple itup, BlockNumber startblkno, int startlevel);
static BlockNumber
gistbufferinginserttuples(GISTBuildState *buildstate, Buffer buffer, int level, IndexTuple *itup, int ntup, OffsetNumber oldoffnum, BlockNumber parentblk, OffsetNumber downlinkoffnum);
static Buffer
gistBufferingFindCorrectParent(GISTBuildState *buildstate, BlockNumber childblkno, int level, BlockNumber *parentblk, OffsetNumber *downlinkoffnum);
static void
gistProcessEmptyingQueue(GISTBuildState *buildstate);
static void
gistEmptyAllBuffers(GISTBuildState *buildstate);
static int
gistGetMaxLevel(Relation index);

static void
gistInitParentMap(GISTBuildState *buildstate);
static void
gistMemorizeParent(GISTBuildState *buildstate, BlockNumber child, BlockNumber parent);
static void
gistMemorizeAllDownlinks(GISTBuildState *buildstate, Buffer parent);
static BlockNumber
gistGetParent(GISTBuildState *buildstate, BlockNumber child);

   
                                                                               
                                                                            
                                                         
   
IndexBuildResult *
gistbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
  IndexBuildResult *result;
  double reltuples;
  GISTBuildState buildstate;
  Buffer buffer;
  Page page;
  MemoryContext oldcxt = CurrentMemoryContext;
  int fillfactor;

  buildstate.indexrel = index;
  buildstate.heaprel = heap;
  if (index->rd_options)
  {
                                                    
    GiSTOptions *options = (GiSTOptions *)index->rd_options;
    char *bufferingMode = (char *)options + options->bufferingModeOffset;

    if (strcmp(bufferingMode, "on") == 0)
    {
      buildstate.bufferingMode = GIST_BUFFERING_STATS;
    }
    else if (strcmp(bufferingMode, "off") == 0)
    {
      buildstate.bufferingMode = GIST_BUFFERING_DISABLED;
    }
    else
    {
      buildstate.bufferingMode = GIST_BUFFERING_AUTO;
    }

    fillfactor = options->fillfactor;
  }
  else
  {
       
                                                                           
                        
       
    buildstate.bufferingMode = GIST_BUFFERING_AUTO;
    fillfactor = GIST_DEFAULT_FILLFACTOR;
  }
                                                               
  buildstate.freespace = BLCKSZ * (100 - fillfactor) / 100;

     
                                                                           
                                               
     
  if (RelationGetNumberOfBlocks(index) != 0)
  {
    elog(ERROR, "index \"%s\" already contains data", RelationGetRelationName(index));
  }

                            
  buildstate.giststate = initGISTstate(index);

     
                                                                         
                                                                    
                                                                          
     
  buildstate.giststate->tempCxt = createTempGistContext();

                                
  buffer = gistNewBuffer(index);
  Assert(BufferGetBlockNumber(buffer) == GIST_ROOT_BLKNO);
  page = BufferGetPage(buffer);

  START_CRIT_SECTION();

  GISTInitBuffer(buffer, F_LEAF);

  MarkBufferDirty(buffer);
  PageSetLSN(page, GistBuildLSN);

  UnlockReleaseBuffer(buffer);

  END_CRIT_SECTION();

                       
  buildstate.indtuples = 0;
  buildstate.indtuplesSize = 0;

     
                       
     
  reltuples = table_index_build_scan(heap, index, indexInfo, true, true, gistBuildCallback, (void *)&buildstate, NULL);

     
                                                                           
              
     
  if (buildstate.bufferingMode == GIST_BUFFERING_ACTIVE)
  {
    elog(DEBUG1, "all tuples processed, emptying buffers");
    gistEmptyAllBuffers(&buildstate);
    gistFreeBuildBuffers(buildstate.gfbb);
  }

                                         
  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(buildstate.giststate->tempCxt);

  freeGISTstate(buildstate.giststate);

     
                                                                             
                                               
     
  if (RelationNeedsWAL(index))
  {
    log_newpage_range(index, MAIN_FORKNUM, 0, RelationGetNumberOfBlocks(index), true);
  }

     
                       
     
  result = (IndexBuildResult *)palloc(sizeof(IndexBuildResult));

  result->heap_tuples = reltuples;
  result->index_tuples = (double)buildstate.indtuples;

  return result;
}

   
                                                                           
                      
   
void
gistValidateBufferingOption(const char *value)
{
  if (value == NULL || (strcmp(value, "on") != 0 && strcmp(value, "off") != 0 && strcmp(value, "auto") != 0))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for \"buffering\" option"), errdetail("Valid values are \"on\", \"off\", and \"auto\".")));
  }
}

   
                                        
   
                                                                         
                                                                         
                                                                               
                          
   
static void
gistInitBuffering(GISTBuildState *buildstate)
{
  Relation index = buildstate->indexrel;
  int pagesPerBuffer;
  Size pageFreeSpace;
  Size itupAvgSize, itupMinSize;
  double avgIndexTuplesPerPage, maxIndexTuplesPerPage;
  int i;
  int levelStep;

                                                                    
  pageFreeSpace = BLCKSZ - SizeOfPageHeaderData - sizeof(GISTPageOpaqueData) - sizeof(ItemIdData) - buildstate->freespace;

     
                                                                            
                 
     
  itupAvgSize = (double)buildstate->indtuplesSize / (double)buildstate->indtuples;

     
                                                                       
                                                   
     
                                                                           
                                                   
     
  itupMinSize = (Size)MAXALIGN(sizeof(IndexTupleData));
  for (i = 0; i < index->rd_att->natts; i++)
  {
    if (TupleDescAttr(index->rd_att, i)->attlen < 0)
    {
      itupMinSize += VARHDRSZ;
    }
    else
    {
      itupMinSize += TupleDescAttr(index->rd_att, i)->attlen;
    }
  }

                                                                              
  avgIndexTuplesPerPage = pageFreeSpace / itupAvgSize;
  maxIndexTuplesPerPage = pageFreeSpace / itupMinSize;

     
                                                                      
                                   
     
                                                                        
                                                                           
                                                                             
                                                                            
                     
     
                                                                          
                                                                       
                                                                           
                                                                            
                                                                   
                                                                        
                                                           
     
                                                                           
                            
     
                                                             
     
                                                                           
                                                                      
                                                                    
                                                      
     
                                                                            
                                                                             
                                                                         
                                                                        
                                       
     
                                                                   
                                                                           
                                                                           
                                                                        
                                                                   
                                                                           
     
                                                                         
                                                                          
                                                                            
                                                                            
                     
     
  levelStep = 1;
  for (;;)
  {
    double subtreesize;
    double maxlowestlevelpages;

                                                                  
    subtreesize = (1 - pow(avgIndexTuplesPerPage, (double)(levelStep + 1))) / (1 - avgIndexTuplesPerPage);

                                                              
    maxlowestlevelpages = pow(maxIndexTuplesPerPage, (double)levelStep);

                                                             
    if (subtreesize > effective_cache_size / 4)
    {
      break;
    }

                                                                           
    if (maxlowestlevelpages > ((double)maintenance_work_mem * 1024) / BLCKSZ)
    {
      break;
    }

                                                                          
    levelStep++;
  }

     
                                                                          
                                                          
     
  levelStep--;

     
                                                                             
              
     
  if (levelStep <= 0)
  {
    elog(DEBUG1, "failed to switch to buffered GiST build");
    buildstate->bufferingMode = GIST_BUFFERING_DISABLED;
    return;
  }

     
                                                                         
                                                                          
                                                              
     
  pagesPerBuffer = calculatePagesPerBuffer(buildstate, levelStep);

                                                         
  buildstate->gfbb = gistInitBuildBuffers(pagesPerBuffer, levelStep, gistGetMaxLevel(index));

  gistInitParentMap(buildstate);

  buildstate->bufferingMode = GIST_BUFFERING_ACTIVE;

  elog(DEBUG1, "switched to buffered GiST build; level step = %d, pagesPerBuffer = %d", levelStep, pagesPerBuffer);
}

   
                                                                   
   
                                                                      
                                                                              
                            
   
static int
calculatePagesPerBuffer(GISTBuildState *buildstate, int levelStep)
{
  double pagesPerBuffer;
  double avgIndexTuplesPerPage;
  double itupAvgSize;
  Size pageFreeSpace;

                                                                    
  pageFreeSpace = BLCKSZ - SizeOfPageHeaderData - sizeof(GISTPageOpaqueData) - sizeof(ItemIdData) - buildstate->freespace;

     
                                                                            
                 
     
  itupAvgSize = (double)buildstate->indtuplesSize / (double)buildstate->indtuples;

  avgIndexTuplesPerPage = pageFreeSpace / itupAvgSize;

     
                                           
     
  pagesPerBuffer = 2 * pow(avgIndexTuplesPerPage, levelStep);

  return (int)rint(pagesPerBuffer);
}

   
                                                  
   
static void
gistBuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{
  GISTBuildState *buildstate = (GISTBuildState *)state;
  IndexTuple itup;
  MemoryContext oldCtx;

  oldCtx = MemoryContextSwitchTo(buildstate->giststate->tempCxt);

                                                          
  itup = gistFormTuple(buildstate->giststate, index, values, isnull, true);
  itup->t_tid = htup->t_self;

  if (buildstate->bufferingMode == GIST_BUFFERING_ACTIVE)
  {
                                       
    gistBufferingBuildInsert(buildstate, itup);
  }
  else
  {
       
                                                                          
                                              
       
    gistdoinsert(index, itup, buildstate->freespace, buildstate->giststate, buildstate->heaprel, true);
  }

                                          
  buildstate->indtuples += 1;
  buildstate->indtuplesSize += IndexTupleSize(itup);

  MemoryContextSwitchTo(oldCtx);
  MemoryContextReset(buildstate->giststate->tempCxt);

  if (buildstate->bufferingMode == GIST_BUFFERING_ACTIVE && buildstate->indtuples % BUFFERING_MODE_TUPLE_SIZE_STATS_TARGET == 0)
  {
                                           
    buildstate->gfbb->pagesPerBuffer = calculatePagesPerBuffer(buildstate, buildstate->gfbb->levelStep);
  }

     
                                                                             
                                             
     
                                                                      
                                                   
     
  if ((buildstate->bufferingMode == GIST_BUFFERING_AUTO && buildstate->indtuples % BUFFERING_MODE_SWITCH_CHECK_STEP == 0 && effective_cache_size < smgrnblocks(RelationGetSmgr(index), MAIN_FORKNUM)) || (buildstate->bufferingMode == GIST_BUFFERING_STATS && buildstate->indtuples >= BUFFERING_MODE_TUPLE_SIZE_STATS_TARGET))
  {
       
                                                                      
                             
       
    gistInitBuffering(buildstate);
  }
}

   
                                              
   
static void
gistBufferingBuildInsert(GISTBuildState *buildstate, IndexTuple itup)
{
                                    
  gistProcessItup(buildstate, itup, 0, buildstate->gfbb->rootlevel);

                                                                    
  gistProcessEmptyingQueue(buildstate);
}

   
                                                                              
                                                                             
                                                                            
                          
   
static bool
gistProcessItup(GISTBuildState *buildstate, IndexTuple itup, BlockNumber startblkno, int startlevel)
{
  GISTSTATE *giststate = buildstate->giststate;
  GISTBuildBuffers *gfbb = buildstate->gfbb;
  Relation indexrel = buildstate->indexrel;
  BlockNumber childblkno;
  Buffer buffer;
  bool result = false;
  BlockNumber blkno;
  int level;
  OffsetNumber downlinkoffnum = InvalidOffsetNumber;
  BlockNumber parentblkno = InvalidBlockNumber;

  CHECK_FOR_INTERRUPTS();

     
                                                                          
                                                                           
                   
     
  blkno = startblkno;
  level = startlevel;
  for (;;)
  {
    ItemId iid;
    IndexTuple idxtuple, newtup;
    Page page;
    OffsetNumber childoffnum;

                                               
    if (LEVEL_HAS_BUFFERS(level, gfbb) && level != startlevel)
    {
      break;
    }

                                      
    if (level == 0)
    {
      break;
    }

       
                                                                    
                        
       

    buffer = ReadBuffer(indexrel, blkno);
    LockBuffer(buffer, GIST_EXCLUSIVE);

    page = (Page)BufferGetPage(buffer);
    childoffnum = gistchoose(indexrel, page, itup, giststate);
    iid = PageGetItemId(page, childoffnum);
    idxtuple = (IndexTuple)PageGetItem(page, iid);
    childblkno = ItemPointerGetBlockNumber(&(idxtuple->t_tid));

    if (level > 1)
    {
      gistMemorizeParent(buildstate, childblkno, blkno);
    }

       
                                                                           
                                                            
       
    newtup = gistgetadjusted(indexrel, idxtuple, itup, giststate);
    if (newtup)
    {
      blkno = gistbufferinginserttuples(buildstate, buffer, level, &newtup, 1, childoffnum, InvalidBlockNumber, InvalidOffsetNumber);
                                                           
    }
    else
    {
      UnlockReleaseBuffer(buffer);
    }

                              
    parentblkno = blkno;
    blkno = childblkno;
    downlinkoffnum = childoffnum;
    Assert(level > 0);
    level--;
  }

  if (LEVEL_HAS_BUFFERS(level, gfbb))
  {
       
                                                                      
                                                                         
       
    GISTNodeBuffer *childNodeBuffer;

                                             
    childNodeBuffer = gistGetNodeBuffer(gfbb, giststate, blkno, level);

                               
    gistPushItupToNodeBuffer(gfbb, childNodeBuffer, itup);

    if (BUFFER_OVERFLOWED(childNodeBuffer, gfbb))
    {
      result = true;
    }
  }
  else
  {
       
                                                        
       
    Assert(level == 0);
    buffer = ReadBuffer(indexrel, blkno);
    LockBuffer(buffer, GIST_EXCLUSIVE);
    gistbufferinginserttuples(buildstate, buffer, level, &itup, 1, InvalidOffsetNumber, parentblkno, downlinkoffnum);
                                                         
  }

  return result;
}

   
                                  
   
                                                                            
   
                                                                               
                                                                               
                                   
   
                                                                             
                 
   
static BlockNumber
gistbufferinginserttuples(GISTBuildState *buildstate, Buffer buffer, int level, IndexTuple *itup, int ntup, OffsetNumber oldoffnum, BlockNumber parentblk, OffsetNumber downlinkoffnum)
{
  GISTBuildBuffers *gfbb = buildstate->gfbb;
  List *splitinfo;
  bool is_split;
  BlockNumber placed_to_blk = InvalidBlockNumber;

  is_split = gistplacetopage(buildstate->indexrel, buildstate->freespace, buildstate->giststate, buffer, itup, ntup, oldoffnum, &placed_to_blk, InvalidBuffer, &splitinfo, false, buildstate->heaprel, true);

     
                                                                             
                                                                            
                                                                            
             
     
  if (is_split && BufferGetBlockNumber(buffer) == GIST_ROOT_BLKNO)
  {
    Page page = BufferGetPage(buffer);
    OffsetNumber off;
    OffsetNumber maxoff;

    Assert(level == gfbb->rootlevel);
    gfbb->rootlevel++;

    elog(DEBUG2, "splitting GiST root page, now %d levels deep", gfbb->rootlevel);

       
                                                                          
                                                                           
                      
       
    if (gfbb->rootlevel > 1)
    {
      maxoff = PageGetMaxOffsetNumber(page);
      for (off = FirstOffsetNumber; off <= maxoff; off++)
      {
        ItemId iid = PageGetItemId(page, off);
        IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);
        BlockNumber childblkno = ItemPointerGetBlockNumber(&(idxtuple->t_tid));
        Buffer childbuf = ReadBuffer(buildstate->indexrel, childblkno);

        LockBuffer(childbuf, GIST_SHARE);
        gistMemorizeAllDownlinks(buildstate, childbuf);
        UnlockReleaseBuffer(childbuf);

           
                                                                      
                       
           
        gistMemorizeParent(buildstate, childblkno, GIST_ROOT_BLKNO);
      }
    }
  }

  if (splitinfo)
  {
       
                                                                  
                                                                           
                                                                          
                       
       
    IndexTuple *downlinks;
    int ndownlinks, i;
    Buffer parentBuffer;
    ListCell *lc;

                                                               
    parentBuffer = gistBufferingFindCorrectParent(buildstate, BufferGetBlockNumber(buffer), level, &parentblk, &downlinkoffnum);

       
                                                                       
                                                                         
                                                                          
                                                                        
                                                         
       
    gistRelocateBuildBuffersOnSplit(gfbb, buildstate->giststate, buildstate->indexrel, level, buffer, splitinfo);

                                                    
    ndownlinks = list_length(splitinfo);
    downlinks = (IndexTuple *)palloc(sizeof(IndexTuple) * ndownlinks);
    i = 0;
    foreach (lc, splitinfo)
    {
      GISTPageSplitInfo *splitinfo = lfirst(lc);

         
                                                                       
                                                                        
                                                                     
                                                                        
                               
         
      if (level > 0)
      {
        gistMemorizeParent(buildstate, BufferGetBlockNumber(splitinfo->buf), BufferGetBlockNumber(parentBuffer));
      }

         
                                                                         
                                                                    
                                                                    
                
         
      if (level > 1)
      {
        gistMemorizeAllDownlinks(buildstate, splitinfo->buf);
      }

         
                                                                      
                                                                     
         
      UnlockReleaseBuffer(splitinfo->buf);
      downlinks[i++] = splitinfo->downlink;
    }

                                  
    gistbufferinginserttuples(buildstate, parentBuffer, level + 1, downlinks, ndownlinks, downlinkoffnum, InvalidBlockNumber, InvalidOffsetNumber);

    list_free_deep(splitinfo);                                 
  }
  else
  {
    UnlockReleaseBuffer(buffer);
  }

  return placed_to_blk;
}

   
                                               
   
                                                                            
                                                                          
                                                                          
                                                                         
                                                                     
                                                                        
             
   
                                                                              
                                                                              
          
   
                                                                           
                                                                           
                            
   
static Buffer
gistBufferingFindCorrectParent(GISTBuildState *buildstate, BlockNumber childblkno, int level, BlockNumber *parentblkno, OffsetNumber *downlinkoffnum)
{
  BlockNumber parent;
  Buffer buffer;
  Page page;
  OffsetNumber maxoff;
  OffsetNumber off;

  if (level > 0)
  {
    parent = gistGetParent(buildstate, childblkno);
  }
  else
  {
       
                                                                      
               
       
    if (*parentblkno == InvalidBlockNumber)
    {
      elog(ERROR, "no parent buffer provided of child %d", childblkno);
    }
    parent = *parentblkno;
  }

  buffer = ReadBuffer(buildstate->indexrel, parent);
  page = BufferGetPage(buffer);
  LockBuffer(buffer, GIST_EXCLUSIVE);
  gistcheckpage(buildstate->indexrel, buffer);
  maxoff = PageGetMaxOffsetNumber(page);

                                 
  if (parent == *parentblkno && *parentblkno != InvalidBlockNumber && *downlinkoffnum != InvalidOffsetNumber && *downlinkoffnum <= maxoff)
  {
    ItemId iid = PageGetItemId(page, *downlinkoffnum);
    IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);

    if (ItemPointerGetBlockNumber(&(idxtuple->t_tid)) == childblkno)
    {
                       
      return buffer;
    }
  }

     
                                                                          
                                                                          
                                                                            
                                                                           
                   
     
  for (off = FirstOffsetNumber; off <= maxoff; off = OffsetNumberNext(off))
  {
    ItemId iid = PageGetItemId(page, off);
    IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);

    if (ItemPointerGetBlockNumber(&(idxtuple->t_tid)) == childblkno)
    {
                           
      *downlinkoffnum = off;
      return buffer;
    }
  }

  elog(ERROR, "failed to re-find parent for block %u", childblkno);
  return InvalidBuffer;                          
}

   
                                                                             
                                                                          
                                                                 
   
static void
gistProcessEmptyingQueue(GISTBuildState *buildstate)
{
  GISTBuildBuffers *gfbb = buildstate->gfbb;

                                                                 
  while (gfbb->bufferEmptyingQueue != NIL)
  {
    GISTNodeBuffer *emptyingNodeBuffer;

                                              
    emptyingNodeBuffer = (GISTNodeBuffer *)linitial(gfbb->bufferEmptyingQueue);
    gfbb->bufferEmptyingQueue = list_delete_first(gfbb->bufferEmptyingQueue);
    emptyingNodeBuffer->queuedForEmptying = false;

       
                                                                         
                                                          
       
    gistUnloadNodeBuffers(gfbb);

       
                                                                      
                                                                      
                                                          
       
                                                                   
                                                                        
                                                                       
                                                                          
                                                                        
                                                        
       
    while (true)
    {
      IndexTuple itup;

                                                
      if (!gistPopItupFromNodeBuffer(gfbb, emptyingNodeBuffer, &itup))
      {
        break;
      }

         
                                                                 
         
                                                                        
                                                                      
                                                                      
                                                                         
                                                                      
                                             
         
      if (gistProcessItup(buildstate, itup, emptyingNodeBuffer->nodeBlocknum, emptyingNodeBuffer->level))
      {
           
                                                                      
                                                        
           
        break;
      }

                                                                       
      MemoryContextReset(buildstate->giststate->tempCxt);
    }
  }
}

   
                                                                          
                                                           
   
                                                                            
                                   
   
static void
gistEmptyAllBuffers(GISTBuildState *buildstate)
{
  GISTBuildBuffers *gfbb = buildstate->gfbb;
  MemoryContext oldCtx;
  int i;

  oldCtx = MemoryContextSwitchTo(buildstate->giststate->tempCxt);

     
                                                    
     
  for (i = gfbb->buffersOnLevelsLen - 1; i >= 0; i--)
  {
       
                                                                         
                                                                           
                                                                           
                                                                         
                                
       
    while (gfbb->buffersOnLevels[i] != NIL)
    {
      GISTNodeBuffer *nodeBuffer;

      nodeBuffer = (GISTNodeBuffer *)linitial(gfbb->buffersOnLevels[i]);

      if (nodeBuffer->blocksCount != 0)
      {
           
                                                                       
                      
           
        if (!nodeBuffer->queuedForEmptying)
        {
          MemoryContextSwitchTo(gfbb->context);
          nodeBuffer->queuedForEmptying = true;
          gfbb->bufferEmptyingQueue = lcons(nodeBuffer, gfbb->bufferEmptyingQueue);
          MemoryContextSwitchTo(buildstate->giststate->tempCxt);
        }
        gistProcessEmptyingQueue(buildstate);
      }
      else
      {
        gfbb->buffersOnLevels[i] = list_delete_first(gfbb->buffersOnLevels[i]);
      }
    }
    elog(DEBUG2, "emptied all buffers at level %d", i);
  }
  MemoryContextSwitchTo(oldCtx);
}

   
                                    
   
static int
gistGetMaxLevel(Relation index)
{
  int maxLevel;
  BlockNumber blkno;

     
                                                                           
            
     
  maxLevel = 0;
  blkno = GIST_ROOT_BLKNO;
  while (true)
  {
    Buffer buffer;
    Page page;
    IndexTuple itup;

    buffer = ReadBuffer(index, blkno);

       
                                                                           
                  
       
    LockBuffer(buffer, GIST_SHARE);
    page = (Page)BufferGetPage(buffer);

    if (GistPageIsLeaf(page))
    {
                                             
      UnlockReleaseBuffer(buffer);
      break;
    }

       
                                                                      
                                                                    
                                                  
       
    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, FirstOffsetNumber));
    blkno = ItemPointerGetBlockNumber(&(itup->t_tid));
    UnlockReleaseBuffer(buffer);

       
                                                                         
                          
       
    maxLevel++;
  }
  return maxLevel;
}

   
                                         
   
                                                                              
                                                                             
                                                                          
                                                                             
                                                                           
                                                            
   
                                                                              
                                                                             
                                                                              
                                                                           
                       
   
                                                                           
                                                                       
                                                                             
                        
   
                                                                               
                                                                            
                                                                       
                                                                        
                                                                                
                                                         
   

typedef struct
{
  BlockNumber childblkno;               
  BlockNumber parentblkno;
} ParentMapEntry;

static void
gistInitParentMap(GISTBuildState *buildstate)
{
  HASHCTL hashCtl;

  hashCtl.keysize = sizeof(BlockNumber);
  hashCtl.entrysize = sizeof(ParentMapEntry);
  hashCtl.hcxt = CurrentMemoryContext;
  buildstate->parentMap = hash_create("gistbuild parent map", 1024, &hashCtl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

static void
gistMemorizeParent(GISTBuildState *buildstate, BlockNumber child, BlockNumber parent)
{
  ParentMapEntry *entry;
  bool found;

  entry = (ParentMapEntry *)hash_search(buildstate->parentMap, (const void *)&child, HASH_ENTER, &found);
  entry->parentblkno = parent;
}

   
                                                            
   
static void
gistMemorizeAllDownlinks(GISTBuildState *buildstate, Buffer parentbuf)
{
  OffsetNumber maxoff;
  OffsetNumber off;
  BlockNumber parentblkno = BufferGetBlockNumber(parentbuf);
  Page page = BufferGetPage(parentbuf);

  Assert(!GistPageIsLeaf(page));

  maxoff = PageGetMaxOffsetNumber(page);
  for (off = FirstOffsetNumber; off <= maxoff; off++)
  {
    ItemId iid = PageGetItemId(page, off);
    IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);
    BlockNumber childblkno = ItemPointerGetBlockNumber(&(idxtuple->t_tid));

    gistMemorizeParent(buildstate, childblkno, parentblkno);
  }
}

static BlockNumber
gistGetParent(GISTBuildState *buildstate, BlockNumber child)
{
  ParentMapEntry *entry;
  bool found;

                                      
  entry = (ParentMapEntry *)hash_search(buildstate->parentMap, (const void *)&child, HASH_FIND, &found);
  if (!found)
  {
    elog(ERROR, "could not find parent of block %d in lookup table", child);
  }

  return entry->parentblkno;
}
