                                                                            
   
               
                                                                    
   
   
                                                                         
                                                                        
   
                  
                                        
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "access/tableam.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "storage/indexfsm.h"
#include "storage/predicate.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct
{
  GinState ginstate;
  double indtuples;
  GinStatsData buildStats;
  MemoryContext tmpCtx;
  MemoryContext funcCtx;
  BuildAccumulator accum;
} GinBuildState;

   
                                                           
                                                           
                                                         
                                                         
                                                       
   
static IndexTuple
addItemPointersToLeafTuple(GinState *ginstate, IndexTuple old, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats, Buffer buffer)
{
  OffsetNumber attnum;
  Datum key;
  GinNullCategory category;
  IndexTuple res;
  ItemPointerData *newItems, *oldItems;
  int oldNPosting, newNPosting;
  GinPostingList *compressedList;

  Assert(!GinIsPostingTree(old));

  attnum = gintuple_get_attrnum(ginstate, old);
  key = gintuple_get_key(ginstate, old, &category);

                                           
  oldItems = ginReadTuple(ginstate, attnum, old, &oldNPosting);

  newItems = ginMergeItemPointers(items, nitem, oldItems, oldNPosting, &newNPosting);

                                                                            
  res = NULL;
  compressedList = ginCompressPostingList(newItems, newNPosting, GinMaxItemSize, NULL);
  pfree(newItems);
  if (compressedList)
  {
    res = GinFormTuple(ginstate, attnum, key, category, (char *)compressedList, SizeOfGinPostingList(compressedList), newNPosting, false);
    pfree(compressedList);
  }
  if (!res)
  {
                                                                
    BlockNumber postingRoot;

       
                                                                        
                                                                       
                                               
       
    postingRoot = createPostingTree(ginstate->index, oldItems, oldNPosting, buildStats, buffer);

                                                               
    ginInsertItemPointers(ginstate->index, postingRoot, items, nitem, buildStats);

                                                        
    res = GinFormTuple(ginstate, attnum, key, category, NULL, 0, 0, true);
    GinSetPostingTree(res, postingRoot);
  }
  pfree(oldItems);

  return res;
}

   
                                                                        
                                                       
                                                       
   
                                                                      
                                              
   
static IndexTuple
buildFreshLeafTuple(GinState *ginstate, OffsetNumber attnum, Datum key, GinNullCategory category, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats, Buffer buffer)
{
  IndexTuple res = NULL;
  GinPostingList *compressedList;

                                                            
  compressedList = ginCompressPostingList(items, nitem, GinMaxItemSize, NULL);
  if (compressedList)
  {
    res = GinFormTuple(ginstate, attnum, key, category, (char *)compressedList, SizeOfGinPostingList(compressedList), nitem, false);
    pfree(compressedList);
  }
  if (!res)
  {
                                                           
    BlockNumber postingRoot;

       
                                                                        
                                           
       
    res = GinFormTuple(ginstate, attnum, key, category, NULL, 0, 0, true);

       
                                                    
       
    postingRoot = createPostingTree(ginstate->index, items, nitem, buildStats, buffer);

                                                    
    GinSetPostingTree(res, postingRoot);
  }

  return res;
}

   
                                                                     
                                                                             
   
                                                                  
                                                
   
void
ginEntryInsert(GinState *ginstate, OffsetNumber attnum, Datum key, GinNullCategory category, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats)
{
  GinBtreeData btree;
  GinBtreeEntryInsertData insertdata;
  GinBtreeStack *stack;
  IndexTuple itup;
  Page page;

  insertdata.isDelete = false;

                                                          
  if (buildStats)
  {
    buildStats->nEntries++;
  }

  ginPrepareEntryScan(&btree, attnum, key, category, ginstate);
  btree.isBuild = (buildStats != NULL);

  stack = ginFindLeafPage(&btree, false, false, NULL);
  page = BufferGetPage(stack->buffer);

  if (btree.findItem(&btree, stack))
  {
                                  
    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, stack->off));

    if (GinIsPostingTree(itup))
    {
                                                
      BlockNumber rootPostingTree = GinGetPostingTree(itup);

                             
      LockBuffer(stack->buffer, GIN_UNLOCK);
      freeGinBtreeStack(stack);

                                    
      ginInsertItemPointers(ginstate->index, rootPostingTree, items, nitem, buildStats);
      return;
    }

    CheckForSerializableConflictIn(ginstate->index, NULL, stack->buffer);
                                       
    itup = addItemPointersToLeafTuple(ginstate, itup, items, nitem, buildStats, stack->buffer);

    insertdata.isDelete = true;
  }
  else
  {
    CheckForSerializableConflictIn(ginstate->index, NULL, stack->buffer);
                                                 
    itup = buildFreshLeafTuple(ginstate, attnum, key, category, items, nitem, buildStats, stack->buffer);
  }

                                             
  insertdata.entry = itup;
  ginInsertValue(&btree, stack, &insertdata, buildStats);
  pfree(itup);
}

   
                                                                          
                             
   
                                                             
   
static void
ginHeapTupleBulkInsert(GinBuildState *buildstate, OffsetNumber attnum, Datum value, bool isNull, ItemPointer heapptr)
{
  Datum *entries;
  GinNullCategory *categories;
  int32 nentries;
  MemoryContext oldCtx;

  oldCtx = MemoryContextSwitchTo(buildstate->funcCtx);
  entries = ginExtractEntries(buildstate->accum.ginstate, attnum, value, isNull, &nentries, &categories);
  MemoryContextSwitchTo(oldCtx);

  ginInsertBAEntries(&buildstate->accum, heapptr, attnum, entries, categories, nentries);

  buildstate->indtuples += nentries;

  MemoryContextReset(buildstate->funcCtx);
}

static void
ginBuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{
  GinBuildState *buildstate = (GinBuildState *)state;
  MemoryContext oldCtx;
  int i;

  oldCtx = MemoryContextSwitchTo(buildstate->tmpCtx);

  for (i = 0; i < buildstate->ginstate.origTupdesc->natts; i++)
  {
    ginHeapTupleBulkInsert(buildstate, (OffsetNumber)(i + 1), values[i], isnull[i], &htup->t_self);
  }

                                                                             
  if (buildstate->accum.allocatedMemory >= (Size)maintenance_work_mem * 1024L)
  {
    ItemPointerData *list;
    Datum key;
    GinNullCategory category;
    uint32 nlist;
    OffsetNumber attnum;

    ginBeginBAScan(&buildstate->accum);
    while ((list = ginGetBAEntry(&buildstate->accum, &attnum, &key, &category, &nlist)) != NULL)
    {
                                                                    
      CHECK_FOR_INTERRUPTS();
      ginEntryInsert(&buildstate->ginstate, attnum, key, category, list, nlist, &buildstate->buildStats);
    }

    MemoryContextReset(buildstate->tmpCtx);
    ginInitBA(&buildstate->accum);
  }

  MemoryContextSwitchTo(oldCtx);
}

IndexBuildResult *
ginbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
  IndexBuildResult *result;
  double reltuples;
  GinBuildState buildstate;
  Buffer RootBuffer, MetaBuffer;
  ItemPointerData *list;
  Datum key;
  GinNullCategory category;
  uint32 nlist;
  MemoryContext oldCtx;
  OffsetNumber attnum;

  if (RelationGetNumberOfBlocks(index) != 0)
  {
    elog(ERROR, "index \"%s\" already contains data", RelationGetRelationName(index));
  }

  initGinState(&buildstate.ginstate, index);
  buildstate.indtuples = 0;
  memset(&buildstate.buildStats, 0, sizeof(GinStatsData));

                                
  MetaBuffer = GinNewBuffer(index);

                                
  RootBuffer = GinNewBuffer(index);

  START_CRIT_SECTION();
  GinInitMetabuffer(MetaBuffer);
  MarkBufferDirty(MetaBuffer);
  GinInitBuffer(RootBuffer, GIN_LEAF);
  MarkBufferDirty(RootBuffer);

  UnlockReleaseBuffer(MetaBuffer);
  UnlockReleaseBuffer(RootBuffer);
  END_CRIT_SECTION();

                                          
  buildstate.buildStats.nEntryPages++;

     
                                                                         
                             
     
  buildstate.tmpCtx = AllocSetContextCreate(CurrentMemoryContext, "Gin build temporary context", ALLOCSET_DEFAULT_SIZES);

     
                                                                
                                                            
     
  buildstate.funcCtx = AllocSetContextCreate(CurrentMemoryContext, "Gin build temporary context for user-defined function", ALLOCSET_DEFAULT_SIZES);

  buildstate.accum.ginstate = &buildstate.ginstate;
  ginInitBA(&buildstate.accum);

     
                                                                           
                                             
     
  reltuples = table_index_build_scan(heap, index, indexInfo, false, true, ginBuildCallback, (void *)&buildstate, NULL);

                                           
  oldCtx = MemoryContextSwitchTo(buildstate.tmpCtx);
  ginBeginBAScan(&buildstate.accum);
  while ((list = ginGetBAEntry(&buildstate.accum, &attnum, &key, &category, &nlist)) != NULL)
  {
                                                                  
    CHECK_FOR_INTERRUPTS();
    ginEntryInsert(&buildstate.ginstate, attnum, key, category, list, nlist, &buildstate.buildStats);
  }
  MemoryContextSwitchTo(oldCtx);

  MemoryContextDelete(buildstate.funcCtx);
  MemoryContextDelete(buildstate.tmpCtx);

     
                           
     
  buildstate.buildStats.nTotalPages = RelationGetNumberOfBlocks(index);
  ginUpdateStats(index, &buildstate.buildStats, true);

     
                                                                             
                                               
     
  if (RelationNeedsWAL(index))
  {
    log_newpage_range(index, MAIN_FORKNUM, 0, RelationGetNumberOfBlocks(index), true);
  }

     
                       
     
  result = (IndexBuildResult *)palloc(sizeof(IndexBuildResult));

  result->heap_tuples = reltuples;
  result->index_tuples = buildstate.indtuples;

  return result;
}

   
                                                                          
   
void
ginbuildempty(Relation index)
{
  Buffer RootBuffer, MetaBuffer;

                                         
  MetaBuffer = ReadBufferExtended(index, INIT_FORKNUM, P_NEW, RBM_NORMAL, NULL);
  LockBuffer(MetaBuffer, BUFFER_LOCK_EXCLUSIVE);
  RootBuffer = ReadBufferExtended(index, INIT_FORKNUM, P_NEW, RBM_NORMAL, NULL);
  LockBuffer(RootBuffer, BUFFER_LOCK_EXCLUSIVE);

                                                       
  START_CRIT_SECTION();
  GinInitMetabuffer(MetaBuffer);
  MarkBufferDirty(MetaBuffer);
  log_newpage_buffer(MetaBuffer, true);
  GinInitBuffer(RootBuffer, GIN_LEAF);
  MarkBufferDirty(RootBuffer);
  log_newpage_buffer(RootBuffer, false);
  END_CRIT_SECTION();

                                       
  UnlockReleaseBuffer(MetaBuffer);
  UnlockReleaseBuffer(RootBuffer);
}

   
                                                                    
                               
   
static void
ginHeapTupleInsert(GinState *ginstate, OffsetNumber attnum, Datum value, bool isNull, ItemPointer item)
{
  Datum *entries;
  GinNullCategory *categories;
  int32 i, nentries;

  entries = ginExtractEntries(ginstate, attnum, value, isNull, &nentries, &categories);

  for (i = 0; i < nentries; i++)
  {
    ginEntryInsert(ginstate, attnum, entries[i], categories[i], item, 1, NULL);
  }
}

bool
gininsert(Relation index, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  GinState *ginstate = (GinState *)indexInfo->ii_AmCache;
  MemoryContext oldCtx;
  MemoryContext insertCtx;
  int i;

                                                                 
  if (ginstate == NULL)
  {
    oldCtx = MemoryContextSwitchTo(indexInfo->ii_Context);
    ginstate = (GinState *)palloc(sizeof(GinState));
    initGinState(ginstate, index);
    indexInfo->ii_AmCache = (void *)ginstate;
    MemoryContextSwitchTo(oldCtx);
  }

  insertCtx = AllocSetContextCreate(CurrentMemoryContext, "Gin insert temporary context", ALLOCSET_DEFAULT_SIZES);

  oldCtx = MemoryContextSwitchTo(insertCtx);

  if (GinGetUseFastUpdate(index))
  {
    GinTupleCollector collector;

    memset(&collector, 0, sizeof(GinTupleCollector));

    for (i = 0; i < ginstate->origTupdesc->natts; i++)
    {
      ginHeapTupleFastCollect(ginstate, &collector, (OffsetNumber)(i + 1), values[i], isnull[i], ht_ctid);
    }

    ginHeapTupleFastInsert(ginstate, &collector);
  }
  else
  {
    for (i = 0; i < ginstate->origTupdesc->natts; i++)
    {
      ginHeapTupleInsert(ginstate, (OffsetNumber)(i + 1), values[i], isnull[i], ht_ctid);
    }
  }

  MemoryContextSwitchTo(oldCtx);
  MemoryContextDelete(insertCtx);

  return false;
}
