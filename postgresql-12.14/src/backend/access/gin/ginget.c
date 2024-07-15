                                                                            
   
            
                                   
   
   
                                                                         
                                                                        
   
                  
                                     
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "storage/predicate.h"
#include "utils/datum.h"
#include "utils/memutils.h"
#include "utils/rel.h"

                   
int GinFuzzySearchLimit = 0;

typedef struct pendingPosition
{
  Buffer pendingBuffer;
  OffsetNumber firstOffset;
  OffsetNumber lastOffset;
  ItemPointerData item;
  bool *hasMatchKey;
} pendingPosition;

   
                                                                
   
static bool
moveRightIfItNeeded(GinBtreeData *btree, GinBtreeStack *stack, Snapshot snapshot)
{
  Page page = BufferGetPage(stack->buffer);

  if (stack->off > PageGetMaxOffsetNumber(page))
  {
       
                                                               
       
    if (GinPageRightMost(page))
    {
      return false;                    
    }

    stack->buffer = ginStepRight(stack->buffer, btree->index, GIN_SHARE);
    stack->blkno = BufferGetBlockNumber(stack->buffer);
    stack->off = FirstOffsetNumber;
    PredicateLockPage(btree->index, stack->blkno, snapshot);
  }

  return true;
}

   
                                                                       
                             
   
static void
scanPostingTree(Relation index, GinScanEntry scanEntry, BlockNumber rootPostingTree, Snapshot snapshot)
{
  GinBtreeData btree;
  GinBtreeStack *stack;
  Buffer buffer;
  Page page;

                                         
  stack = ginScanBeginPostingTree(&btree, index, rootPostingTree, snapshot);
  buffer = stack->buffer;

  IncrBufferRefCount(buffer);                                         

  freeGinBtreeStack(stack);

     
                                                          
     
  for (;;)
  {
    page = BufferGetPage(buffer);
    if ((GinPageGetOpaque(page)->flags & GIN_DELETED) == 0)
    {
      int n = GinDataLeafPageGetItemsToTbm(page, scanEntry->matchBitmap);

      scanEntry->predictNumberResult += n;
    }

    if (GinPageRightMost(page))
    {
      break;                    
    }

    buffer = ginStepRight(buffer, index, GIN_SHARE);
  }

  UnlockReleaseBuffer(buffer);
}

   
                                                                      
                                                                       
   
                                                               
                                       
                                                                      
                                                                          
                                                                             
                                                            
   
                                                                              
   
static bool
collectMatchBitmap(GinBtreeData *btree, GinBtreeStack *stack, GinScanEntry scanEntry, Snapshot snapshot)
{
  OffsetNumber attnum;
  Form_pg_attribute attr;

                                      
  scanEntry->matchBitmap = tbm_create(work_mem * 1024L, NULL);

                                                
  if (scanEntry->isPartialMatch && scanEntry->queryCategory != GIN_CAT_NORM_KEY)
  {
    return true;
  }

                                                                      
  attnum = scanEntry->attnum;
  attr = TupleDescAttr(btree->ginstate->origTupdesc, attnum - 1);

     
                                                                       
                           
     
  PredicateLockPage(btree->index, stack->buffer, snapshot);

  for (;;)
  {
    Page page;
    IndexTuple itup;
    Datum idatum;
    GinNullCategory icategory;

       
                                                                           
       
    if (moveRightIfItNeeded(btree, stack, snapshot) == false)
    {
      return true;
    }

    page = BufferGetPage(stack->buffer);
    TestForOldSnapshot(snapshot, btree->index, page);
    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, stack->off));

       
                                                        
       
    if (gintuple_get_attrnum(btree->ginstate, itup) != attnum)
    {
      return true;
    }

                                       
    idatum = gintuple_get_key(btree->ginstate, itup, &icategory);

       
                                                  
       
    if (scanEntry->isPartialMatch)
    {
      int32 cmp;

         
                                                            
                                                          
         
      if (icategory != GIN_CAT_NORM_KEY)
      {
        return true;
      }

                   
                                 
                                
                                                   
                                                     
                   
         
      cmp = DatumGetInt32(FunctionCall4Coll(&btree->ginstate->comparePartialFn[attnum - 1], btree->ginstate->supportCollation[attnum - 1], scanEntry->queryKey, idatum, UInt16GetDatum(scanEntry->strategy), PointerGetDatum(scanEntry->extra_data)));

      if (cmp > 0)
      {
        return true;
      }
      else if (cmp < 0)
      {
        stack->off++;
        continue;
      }
    }
    else if (scanEntry->searchMode == GIN_SEARCH_MODE_ALL)
    {
         
                                                                     
                                                                      
                                                                         
                                         
         
      if (icategory == GIN_CAT_NULL_ITEM)
      {
        return true;
      }
    }

       
                                                            
       
    if (GinIsPostingTree(itup))
    {
      BlockNumber rootPostingTree = GinGetPostingTree(itup);

         
                                                                        
                                                    
         
                                                                        
                                
         
      if (icategory == GIN_CAT_NORM_KEY)
      {
        idatum = datumCopy(idatum, attr->attbyval, attr->attlen);
      }

      LockBuffer(stack->buffer, GIN_UNLOCK);

         
                                                                        
                                                                    
                                                  
         
      PredicateLockPage(btree->index, rootPostingTree, snapshot);

                                                             
      scanPostingTree(btree->index, scanEntry, rootPostingTree, snapshot);

         
                                                                       
                                                                  
         
      LockBuffer(stack->buffer, GIN_SHARE);
      page = BufferGetPage(stack->buffer);
      if (!GinPageIsLeaf(page))
      {
           
                                                                  
                                                                      
                                                          
           
        return false;
      }

                                            
      for (;;)
      {
        if (moveRightIfItNeeded(btree, stack, snapshot) == false)
        {
          ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("failed to re-find tuple within index \"%s\"", RelationGetRelationName(btree->index))));
        }

        page = BufferGetPage(stack->buffer);
        itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, stack->off));

        if (gintuple_get_attrnum(btree->ginstate, itup) == attnum)
        {
          Datum newDatum;
          GinNullCategory newCategory;

          newDatum = gintuple_get_key(btree->ginstate, itup, &newCategory);

          if (ginCompareEntries(btree->ginstate, attnum, newDatum, newCategory, idatum, icategory) == 0)
          {
            break;             
          }
        }

        stack->off++;
      }

      if (icategory == GIN_CAT_NORM_KEY && !attr->attbyval)
      {
        pfree(DatumGetPointer(idatum));
      }
    }
    else
    {
      ItemPointer ipd;
      int nipd;

      ipd = ginReadTuple(btree->ginstate, scanEntry->attnum, itup, &nipd);
      tbm_add_tuples(scanEntry->matchBitmap, ipd, nipd, false);
      scanEntry->predictNumberResult += GinGetNPosting(itup);
      pfree(ipd);
    }

       
                                            
       
    stack->off++;
  }
}

   
                                                                                         
   
static void
startScanEntry(GinState *ginstate, GinScanEntry entry, Snapshot snapshot)
{
  GinBtreeData btreeEntry;
  GinBtreeStack *stackEntry;
  Page page;
  bool needUnlock;

restartScanEntry:
  entry->buffer = InvalidBuffer;
  ItemPointerSetMin(&entry->curItem);
  entry->offset = InvalidOffsetNumber;
  if (entry->list)
  {
    pfree(entry->list);
  }
  entry->list = NULL;
  entry->nlist = 0;
  entry->matchBitmap = NULL;
  entry->matchResult = NULL;
  entry->reduceResult = false;
  entry->predictNumberResult = 0;

     
                                                                        
                            
     
  ginPrepareEntryScan(&btreeEntry, entry->attnum, entry->queryKey, entry->queryCategory, ginstate);
  stackEntry = ginFindLeafPage(&btreeEntry, true, false, snapshot);
  page = BufferGetPage(stackEntry->buffer);

                                                                 
  needUnlock = true;

  entry->isFinished = true;

  if (entry->isPartialMatch || entry->queryCategory == GIN_CAT_EMPTY_QUERY)
  {
       
                                                                       
                                                                      
                                                                   
                                                                          
                           
       
    btreeEntry.findItem(&btreeEntry, stackEntry);
    if (collectMatchBitmap(&btreeEntry, stackEntry, entry, snapshot) == false)
    {
         
                                                                     
                                                                    
                              
         
      if (entry->matchBitmap)
      {
        if (entry->matchIterator)
        {
          tbm_end_iterate(entry->matchIterator);
        }
        entry->matchIterator = NULL;
        tbm_free(entry->matchBitmap);
        entry->matchBitmap = NULL;
      }
      LockBuffer(stackEntry->buffer, GIN_UNLOCK);
      freeGinBtreeStack(stackEntry);
      goto restartScanEntry;
    }

    if (entry->matchBitmap && !tbm_is_empty(entry->matchBitmap))
    {
      entry->matchIterator = tbm_begin_iterate(entry->matchBitmap);
      entry->isFinished = false;
    }
  }
  else if (btreeEntry.findItem(&btreeEntry, stackEntry))
  {
    IndexTuple itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, stackEntry->off));

    if (GinIsPostingTree(itup))
    {
      BlockNumber rootPostingTree = GinGetPostingTree(itup);
      GinBtreeStack *stack;
      Page page;
      ItemPointerData minItem;

         
                                                                         
                                                                         
                                    
         
      PredicateLockPage(ginstate->index, rootPostingTree, snapshot);

         
                                                                     
                                                                         
                                                                    
                                                                       
                               
         
      LockBuffer(stackEntry->buffer, GIN_UNLOCK);
      needUnlock = false;

      stack = ginScanBeginPostingTree(&entry->btree, ginstate->index, rootPostingTree, snapshot);
      entry->buffer = stack->buffer;

         
                                                                      
                                                                        
                                                                         
         
      IncrBufferRefCount(entry->buffer);

      page = BufferGetPage(entry->buffer);

         
                                          
         
      ItemPointerSetMin(&minItem);
      entry->list = GinDataLeafPageGetItems(page, &entry->nlist, minItem);

      entry->predictNumberResult = stack->predictNumber * entry->nlist;

      LockBuffer(entry->buffer, GIN_UNLOCK);
      freeGinBtreeStack(stack);
      entry->isFinished = false;
    }
    else
    {
         
                                                                     
                                                                      
                                                                        
                                                                      
                                                                       
                                      
         
      PredicateLockPage(ginstate->index, BufferGetBlockNumber(stackEntry->buffer), snapshot);
      if (GinGetNPosting(itup) > 0)
      {
        entry->list = ginReadTuple(ginstate, entry->attnum, itup, &entry->nlist);
        entry->predictNumberResult = entry->nlist;

        entry->isFinished = false;
      }
    }
  }
  else
  {
       
                                                                        
                                                          
       
    PredicateLockPage(ginstate->index, BufferGetBlockNumber(stackEntry->buffer), snapshot);
  }

  if (needUnlock)
  {
    LockBuffer(stackEntry->buffer, GIN_UNLOCK);
  }
  freeGinBtreeStack(stackEntry);
}

   
                                                                             
                               
   
static int
entryIndexByFrequencyCmp(const void *a1, const void *a2, void *arg)
{
  const GinScanKey key = (const GinScanKey)arg;
  int i1 = *(const int *)a1;
  int i2 = *(const int *)a2;
  uint32 n1 = key->scanEntry[i1]->predictNumberResult;
  uint32 n2 = key->scanEntry[i2]->predictNumberResult;

  if (n1 < n2)
  {
    return -1;
  }
  else if (n1 == n2)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

static void
startScanKey(GinState *ginstate, GinScanOpaque so, GinScanKey key)
{
  MemoryContext oldCtx = CurrentMemoryContext;
  int i;
  int j;
  int *entryIndexes;

  ItemPointerSetMin(&key->curItem);
  key->curItemMatches = false;
  key->recheckCurItem = false;
  key->isFinished = false;

     
                                                                         
                                                                            
                                                                         
                                                                           
                                                                         
                                                                     
                                                                          
     
                                                                        
                                                                             
                                                                           
                                                                       
                                                                             
                                                         
                                                                          
                                                                          
                                                                            
                                    
     
  if (key->nentries > 1)
  {
    MemoryContextSwitchTo(so->tempCtx);

    entryIndexes = (int *)palloc(sizeof(int) * key->nentries);
    for (i = 0; i < key->nentries; i++)
    {
      entryIndexes[i] = i;
    }
    qsort_arg(entryIndexes, key->nentries, sizeof(int), entryIndexByFrequencyCmp, key);

    for (i = 0; i < key->nentries - 1; i++)
    {
                                                                 
      for (j = 0; j <= i; j++)
      {
        key->entryRes[entryIndexes[j]] = GIN_FALSE;
      }
      for (j = i + 1; j < key->nentries; j++)
      {
        key->entryRes[entryIndexes[j]] = GIN_MAYBE;
      }

      if (key->triConsistentFn(key) == GIN_FALSE)
      {
        break;
      }
    }
                                           

    MemoryContextSwitchTo(so->keyCtx);

    key->nrequired = i + 1;
    key->nadditional = key->nentries - key->nrequired;
    key->requiredEntries = palloc(key->nrequired * sizeof(GinScanEntry));
    key->additionalEntries = palloc(key->nadditional * sizeof(GinScanEntry));

    j = 0;
    for (i = 0; i < key->nrequired; i++)
    {
      key->requiredEntries[i] = key->scanEntry[entryIndexes[j++]];
    }
    for (i = 0; i < key->nadditional; i++)
    {
      key->additionalEntries[i] = key->scanEntry[entryIndexes[j++]];
    }

                                                                     
    MemoryContextReset(so->tempCtx);
  }
  else
  {
    MemoryContextSwitchTo(so->keyCtx);

    key->nrequired = 1;
    key->nadditional = 0;
    key->requiredEntries = palloc(1 * sizeof(GinScanEntry));
    key->requiredEntries[0] = key->scanEntry[0];
  }
  MemoryContextSwitchTo(oldCtx);
}

static void
startScan(IndexScanDesc scan)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;
  GinState *ginstate = &so->ginstate;
  uint32 i;

  for (i = 0; i < so->totalentries; i++)
  {
    startScanEntry(ginstate, so->entries[i], scan->xs_snapshot);
  }

  if (GinFuzzySearchLimit > 0)
  {
       
                                                                           
                                                                    
                                                                     
                                    
       
    bool reduce = true;

    for (i = 0; i < so->totalentries; i++)
    {
      if (so->entries[i]->predictNumberResult <= so->totalentries * GinFuzzySearchLimit)
      {
        reduce = false;
        break;
      }
    }
    if (reduce)
    {
      for (i = 0; i < so->totalentries; i++)
      {
        so->entries[i]->predictNumberResult /= so->totalentries;
        so->entries[i]->reduceResult = true;
      }
    }
  }

     
                                                                      
                                 
     
  for (i = 0; i < so->nkeys; i++)
  {
    startScanKey(ginstate, so, so->keys + i);
  }
}

   
                                                             
   
                                                                               
                                                       
   
static void
entryLoadMoreItems(GinState *ginstate, GinScanEntry entry, ItemPointerData advancePast, Snapshot snapshot)
{
  Page page;
  int i;
  bool stepright;

  if (!BufferIsValid(entry->buffer))
  {
    entry->isFinished = true;
    return;
  }

     
                                                                          
                                                                   
                                                                           
                                                                       
     
  if (ginCompareItemPointers(&entry->curItem, &advancePast) == 0)
  {
    stepright = true;
    LockBuffer(entry->buffer, GIN_SHARE);
  }
  else
  {
    GinBtreeStack *stack;

    ReleaseBuffer(entry->buffer);

       
                                                           
       
    if (ItemPointerIsLossyPage(&advancePast))
    {
      ItemPointerSet(&entry->btree.itemptr, GinItemPointerGetBlockNumber(&advancePast) + 1, FirstOffsetNumber);
    }
    else
    {
      ItemPointerSet(&entry->btree.itemptr, GinItemPointerGetBlockNumber(&advancePast), OffsetNumberNext(GinItemPointerGetOffsetNumber(&advancePast)));
    }
    entry->btree.fullScan = false;
    stack = ginFindLeafPage(&entry->btree, true, false, snapshot);

                                                   
    entry->buffer = stack->buffer;
    IncrBufferRefCount(entry->buffer);
    freeGinBtreeStack(stack);
    stepright = false;
  }

  elog(DEBUG2, "entryLoadMoreItems, %u/%u, skip: %d", GinItemPointerGetBlockNumber(&advancePast), GinItemPointerGetOffsetNumber(&advancePast), !stepright);

  page = BufferGetPage(entry->buffer);
  for (;;)
  {
    entry->offset = InvalidOffsetNumber;
    if (entry->list)
    {
      pfree(entry->list);
      entry->list = NULL;
      entry->nlist = 0;
    }

    if (stepright)
    {
         
                                                                     
                                            
         
      if (GinPageRightMost(page))
      {
        UnlockReleaseBuffer(entry->buffer);
        entry->buffer = InvalidBuffer;
        entry->isFinished = true;
        return;
      }

         
                                                                    
                                                     
         
      entry->buffer = ginStepRight(entry->buffer, ginstate->index, GIN_SHARE);
      page = BufferGetPage(entry->buffer);
    }
    stepright = true;

    if (GinPageGetOpaque(page)->flags & GIN_DELETED)
    {
      continue;                                            
    }

       
                                                                   
                                                                          
                                                                        
                                                                          
             
       
    if (!GinPageRightMost(page) && ginCompareItemPointers(&advancePast, GinDataPageGetRightBound(page)) >= 0)
    {
         
                                                                        
                                
         
      continue;
    }

    entry->list = GinDataLeafPageGetItems(page, &entry->nlist, advancePast);

    for (i = 0; i < entry->nlist; i++)
    {
      if (ginCompareItemPointers(&advancePast, &entry->list[i]) < 0)
      {
        entry->offset = i;

        if (GinPageRightMost(page))
        {
                                                              
          UnlockReleaseBuffer(entry->buffer);
          entry->buffer = InvalidBuffer;
        }
        else
        {
          LockBuffer(entry->buffer, GIN_UNLOCK);
        }
        return;
      }
    }
  }
}

#define gin_rand() (((double)random()) / ((double)MAX_RANDOM_VALUE))
#define dropItem(e) (gin_rand() > ((double)GinFuzzySearchLimit) / ((double)((e)->predictNumberResult)))

   
                                                                              
                                                                            
   
                                                  
   
                                                                          
                                                                          
                                                                       
                                                                               
                                                                             
                                                                            
   
static void
entryGetItem(GinState *ginstate, GinScanEntry entry, ItemPointerData advancePast, Snapshot snapshot)
{
  Assert(!entry->isFinished);

  Assert(!ItemPointerIsValid(&entry->curItem) || ginCompareItemPointers(&entry->curItem, &advancePast) <= 0);

  if (entry->matchBitmap)
  {
                         
    BlockNumber advancePastBlk = GinItemPointerGetBlockNumber(&advancePast);
    OffsetNumber advancePastOff = GinItemPointerGetOffsetNumber(&advancePast);

    for (;;)
    {
         
                                                                        
                        
         
      while (entry->matchResult == NULL || (entry->matchResult->ntuples >= 0 && entry->offset >= entry->matchResult->ntuples) || entry->matchResult->blockno < advancePastBlk || (ItemPointerIsLossyPage(&advancePast) && entry->matchResult->blockno == advancePastBlk))
      {
        entry->matchResult = tbm_iterate(entry->matchIterator);

        if (entry->matchResult == NULL)
        {
          ItemPointerSetInvalid(&entry->curItem);
          tbm_end_iterate(entry->matchIterator);
          entry->matchIterator = NULL;
          entry->isFinished = true;
          break;
        }

           
                                                                       
                                                                       
                                                                    
                                  
           
        entry->offset = 0;
      }
      if (entry->isFinished)
      {
        break;
      }

         
                                                                     
                                                           
         
      if (entry->matchResult->ntuples < 0)
      {
        ItemPointerSetLossyPage(&entry->curItem, entry->matchResult->blockno);

           
                                                               
                                                                      
                                                    
           
        break;
      }

         
                                                                     
                      
         
      if (entry->matchResult->blockno == advancePastBlk)
      {
           
                                                                  
                                                               
                                                                     
           
        if (entry->matchResult->offsets[entry->matchResult->ntuples - 1] <= advancePastOff)
        {
          entry->offset = entry->matchResult->ntuples;
          continue;
        }

                                                                 
        while (entry->matchResult->offsets[entry->offset] <= advancePastOff)
        {
          entry->offset++;
        }
      }

      ItemPointerSet(&entry->curItem, entry->matchResult->blockno, entry->matchResult->offsets[entry->offset]);
      entry->offset++;

                                                    
      if (!entry->reduceResult || !dropItem(entry))
      {
        break;
      }
    }
  }
  else if (!BufferIsValid(entry->buffer))
  {
       
                                                                         
             
       
    for (;;)
    {
      if (entry->offset >= entry->nlist)
      {
        ItemPointerSetInvalid(&entry->curItem);
        entry->isFinished = true;
        break;
      }

      entry->curItem = entry->list[entry->offset++];

                                                        
      if (ginCompareItemPointers(&entry->curItem, &advancePast) <= 0)
      {
        continue;
      }

                                                    
      if (!entry->reduceResult || !dropItem(entry))
      {
        break;
      }
    }
  }
  else
  {
                        
    for (;;)
    {
                                                                 
      while (entry->offset >= entry->nlist)
      {
        entryLoadMoreItems(ginstate, entry, advancePast, snapshot);

        if (entry->isFinished)
        {
          ItemPointerSetInvalid(&entry->curItem);
          return;
        }
      }

      entry->curItem = entry->list[entry->offset++];

                                                        
      if (ginCompareItemPointers(&entry->curItem, &advancePast) <= 0)
      {
        continue;
      }

                                                    
      if (!entry->reduceResult || !dropItem(entry))
      {
        break;
      }

         
                                                                       
                                        
         
      advancePast = entry->curItem;
    }
  }
}

   
                                                                               
                                                                             
                   
   
                                                                            
                                                                              
                                                                             
                                                                             
                                          
   
                                                                     
   
                                                      
   
                                                                          
                                                                        
                                                                       
                                                                               
                          
   
static void
keyGetItem(GinState *ginstate, MemoryContext tempCtx, GinScanKey key, ItemPointerData advancePast, Snapshot snapshot)
{
  ItemPointerData minItem;
  ItemPointerData curPageLossy;
  uint32 i;
  bool haveLossyEntry;
  GinScanEntry entry;
  GinTernaryValue res;
  MemoryContext oldCtx;
  bool allFinished;

  Assert(!key->isFinished);

     
                                                                            
                                                                    
                                                             
     
  if (ginCompareItemPointers(&key->curItem, &advancePast) > 0)
  {
    return;
  }

     
                                                                         
     
                                                                             
                                                                           
                                                                    
                              
     
  ItemPointerSetMax(&minItem);
  allFinished = true;
  for (i = 0; i < key->nrequired; i++)
  {
    entry = key->requiredEntries[i];

    if (entry->isFinished)
    {
      continue;
    }

       
                                         
       
                                                                
                                                                        
                                
       
    if (ginCompareItemPointers(&entry->curItem, &advancePast) <= 0)
    {
      entryGetItem(ginstate, entry, advancePast, snapshot);
      if (entry->isFinished)
      {
        continue;
      }
    }

    allFinished = false;
    if (ginCompareItemPointers(&entry->curItem, &minItem) < 0)
    {
      minItem = entry->curItem;
    }
  }

  if (allFinished)
  {
                                  
    key->isFinished = true;
    return;
  }

     
                                                          
     
                                                                         
                                                                         
                                                                        
                                                         
     
  if (ItemPointerIsLossyPage(&minItem))
  {
    if (GinItemPointerGetBlockNumber(&advancePast) < GinItemPointerGetBlockNumber(&minItem))
    {
      ItemPointerSet(&advancePast, GinItemPointerGetBlockNumber(&minItem), InvalidOffsetNumber);
    }
  }
  else
  {
    Assert(GinItemPointerGetOffsetNumber(&minItem) > 0);
    ItemPointerSet(&advancePast, GinItemPointerGetBlockNumber(&minItem), OffsetNumberPrev(GinItemPointerGetOffsetNumber(&minItem)));
  }

     
                                                                         
                                                                             
                                                                          
                                                                           
                                                                             
                                                                     
     
  for (i = 0; i < key->nadditional; i++)
  {
    entry = key->additionalEntries[i];

    if (entry->isFinished)
    {
      continue;
    }

    if (ginCompareItemPointers(&entry->curItem, &advancePast) <= 0)
    {
      entryGetItem(ginstate, entry, advancePast, snapshot);
      if (entry->isFinished)
      {
        continue;
      }
    }

       
                                                                           
                                                                       
                                                                      
       
    if (ginCompareItemPointers(&entry->curItem, &minItem) < 0)
    {
      Assert(ItemPointerIsLossyPage(&minItem));
      minItem = entry->curItem;
    }
  }

     
                                                                             
                                    
     
                                                                        
                                                                             
                                                                         
                                                                          
                                                           
     
                                                                         
                                                                             
                                                                             
                                                                       
                                                                     
                                                                            
                                                                          
                                            
     
                                                                            
                                                                            
                                                                       
                                                                   
                                                                         
                   
     
                                                                           
                                                                             
                                                
     
  key->curItem = minItem;
  ItemPointerSetLossyPage(&curPageLossy, GinItemPointerGetBlockNumber(&key->curItem));
  haveLossyEntry = false;
  for (i = 0; i < key->nentries; i++)
  {
    entry = key->scanEntry[i];
    if (entry->isFinished == false && ginCompareItemPointers(&entry->curItem, &curPageLossy) == 0)
    {
      if (i < key->nuserentries)
      {
        key->entryRes[i] = GIN_MAYBE;
      }
      else
      {
        key->entryRes[i] = GIN_TRUE;
      }
      haveLossyEntry = true;
    }
    else
    {
      key->entryRes[i] = GIN_FALSE;
    }
  }

                                                        
  oldCtx = MemoryContextSwitchTo(tempCtx);

  if (haveLossyEntry)
  {
                                                               
    res = key->triConsistentFn(key);

    if (res == GIN_TRUE || res == GIN_MAYBE)
    {
                                
      MemoryContextSwitchTo(oldCtx);
      MemoryContextReset(tempCtx);

                                                   
      key->curItem = curPageLossy;
      key->curItemMatches = true;
      key->recheckCurItem = true;
      return;
    }
  }

     
                                                                           
                                                                            
                                                                          
                                                                        
                                                                             
                    
     
                                                          
     
  for (i = 0; i < key->nentries; i++)
  {
    entry = key->scanEntry[i];
    if (entry->isFinished)
    {
      key->entryRes[i] = GIN_FALSE;
    }
#if 0

		   
                                                                         
                            
     
		else if (ginCompareItemPointers(&entry->curItem, &advancePast) <= 0)
			key->entryRes[i] = GIN_MAYBE;
#endif
    else if (ginCompareItemPointers(&entry->curItem, &curPageLossy) == 0)
    {
      key->entryRes[i] = GIN_MAYBE;
    }
    else if (ginCompareItemPointers(&entry->curItem, &minItem) == 0)
    {
      key->entryRes[i] = GIN_TRUE;
    }
    else
    {
      key->entryRes[i] = GIN_FALSE;
    }
  }

  res = key->triConsistentFn(key);

  switch (res)
  {
  case GIN_TRUE:
    key->curItemMatches = true;
                                            
    break;

  case GIN_FALSE:
    key->curItemMatches = false;
    break;

  case GIN_MAYBE:
    key->curItemMatches = true;
    key->recheckCurItem = true;
    break;

  default:

       
                                                                  
                                                                 
       
    key->curItemMatches = true;
    key->recheckCurItem = true;
    break;
  }

     
                                                                             
                                                                            
                                                                            
                                                                       
     

                                         
  MemoryContextSwitchTo(oldCtx);
  MemoryContextReset(tempCtx);
}

   
                                                             
                                   
                                           
   
                                                                       
                                                                       
                                                                         
   
static bool
scanGetItem(IndexScanDesc scan, ItemPointerData advancePast, ItemPointerData *item, bool *recheck)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;
  uint32 i;
  bool match;

               
                                                                            
                                                                            
                                                                          
                    
     
                                                                         
                                                                       
               
     
                         
                          
                  
                       
                          
     
                                                                      
                                                                      
                                                             
               
     
  do
  {
    ItemPointerSetMin(item);
    match = true;
    for (i = 0; i < so->nkeys && match; i++)
    {
      GinScanKey key = so->keys + i;

                                                                   
      keyGetItem(&so->ginstate, so->tempCtx, key, advancePast, scan->xs_snapshot);

      if (key->isFinished)
      {
        return false;
      }

         
                                                                       
                                                                      
         
      if (!key->curItemMatches)
      {
        advancePast = key->curItem;
        match = false;
        break;
      }

         
                                                                      
                                                  
         
                                                                        
                                                                       
         
      if (ItemPointerIsLossyPage(&key->curItem))
      {
        if (GinItemPointerGetBlockNumber(&advancePast) < GinItemPointerGetBlockNumber(&key->curItem))
        {
          ItemPointerSet(&advancePast, GinItemPointerGetBlockNumber(&key->curItem), InvalidOffsetNumber);
        }
      }
      else
      {
        Assert(GinItemPointerGetOffsetNumber(&key->curItem) > 0);
        ItemPointerSet(&advancePast, GinItemPointerGetBlockNumber(&key->curItem), OffsetNumberPrev(GinItemPointerGetOffsetNumber(&key->curItem)));
      }

         
                                                                         
                                                           
         
                                                                       
                                                                      
                                                                     
                                                                       
                   
         
      if (i == 0)
      {
        *item = key->curItem;
      }
      else
      {
        if (ItemPointerIsLossyPage(&key->curItem) || ItemPointerIsLossyPage(item))
        {
          Assert(GinItemPointerGetBlockNumber(&key->curItem) >= GinItemPointerGetBlockNumber(item));
          match = (GinItemPointerGetBlockNumber(&key->curItem) == GinItemPointerGetBlockNumber(item));
        }
        else
        {
          Assert(ginCompareItemPointers(&key->curItem, item) >= 0);
          match = (ginCompareItemPointers(&key->curItem, item) == 0);
        }
      }
    }
  } while (!match);

  Assert(!ItemPointerIsMin(item));

     
                                                                         
                                                                            
                
     
                                                                          
     
  *recheck = false;
  for (i = 0; i < so->nkeys; i++)
  {
    GinScanKey key = so->keys + i;

    if (key->recheckCurItem)
    {
      *recheck = true;
      break;
    }
  }

  return true;
}

   
                                           
   

   
                                                                     
                                                                       
                                                                         
                                                                            
                                                                
   
                                                                          
                                                                            
   
static bool
scanGetCandidate(IndexScanDesc scan, pendingPosition *pos)
{
  OffsetNumber maxoff;
  Page page;
  IndexTuple itup;

  ItemPointerSetInvalid(&pos->item);
  for (;;)
  {
    page = BufferGetPage(pos->pendingBuffer);
    TestForOldSnapshot(scan->xs_snapshot, scan->indexRelation, page);

    maxoff = PageGetMaxOffsetNumber(page);
    if (pos->firstOffset > maxoff)
    {
      BlockNumber blkno = GinPageGetOpaque(page)->rightlink;

      if (blkno == InvalidBlockNumber)
      {
        UnlockReleaseBuffer(pos->pendingBuffer);
        pos->pendingBuffer = InvalidBuffer;

        return false;
      }
      else
      {
           
                                                                       
                                                                    
                                                                     
                       
           
        Buffer tmpbuf = ReadBuffer(scan->indexRelation, blkno);

        LockBuffer(tmpbuf, GIN_SHARE);
        UnlockReleaseBuffer(pos->pendingBuffer);

        pos->pendingBuffer = tmpbuf;
        pos->firstOffset = FirstOffsetNumber;
      }
    }
    else
    {
      itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, pos->firstOffset));
      pos->item = itup->t_tid;
      if (GinPageHasFullRow(page))
      {
           
                                            
           
        for (pos->lastOffset = pos->firstOffset + 1; pos->lastOffset <= maxoff; pos->lastOffset++)
        {
          itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, pos->lastOffset));
          if (!ItemPointerEquals(&pos->item, &itup->t_tid))
          {
            break;
          }
        }
      }
      else
      {
           
                                                      
           
        pos->lastOffset = maxoff + 1;
      }

         
                                                                        
                                                                         
                                 
         
      break;
    }
  }

  return true;
}

   
                                                                         
                                        
                                
                                                              
                       
   
                                                                            
                                              
   
static bool
matchPartialInPendingList(GinState *ginstate, Page page, OffsetNumber off, OffsetNumber maxoff, GinScanEntry entry, Datum *datum, GinNullCategory *category, bool *datumExtracted)
{
  IndexTuple itup;
  int32 cmp;

                                               
  if (entry->queryCategory != GIN_CAT_NORM_KEY)
  {
    return false;
  }

  while (off < maxoff)
  {
    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, off));

    if (gintuple_get_attrnum(ginstate, itup) != entry->attnum)
    {
      return false;
    }

    if (datumExtracted[off - 1] == false)
    {
      datum[off - 1] = gintuple_get_key(ginstate, itup, &category[off - 1]);
      datumExtracted[off - 1] = true;
    }

                                                         
    if (category[off - 1] != GIN_CAT_NORM_KEY)
    {
      return false;
    }

                 
                            
                              
                                                                        
                                                   
                 
       
    cmp = DatumGetInt32(FunctionCall4Coll(&ginstate->comparePartialFn[entry->attnum - 1], ginstate->supportCollation[entry->attnum - 1], entry->queryKey, datum[off - 1], UInt16GetDatum(entry->strategy), PointerGetDatum(entry->extra_data)));
    if (cmp == 0)
    {
      return true;
    }
    else if (cmp > 0)
    {
      return false;
    }

    off++;
  }

  return false;
}

   
                                                        
                                                     
   
                                                                  
                                                                         
                                                                               
                                                                      
                    
   
                                                                   
   
static bool
collectMatchesForHeapRow(IndexScanDesc scan, pendingPosition *pos)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;
  OffsetNumber attrnum;
  Page page;
  IndexTuple itup;
  int i, j;

     
                                              
     
  for (i = 0; i < so->nkeys; i++)
  {
    GinScanKey key = so->keys + i;

    memset(key->entryRes, GIN_FALSE, key->nentries);
  }
  memset(pos->hasMatchKey, false, so->nkeys);

     
                                                                             
                                           
     
  for (;;)
  {
    Datum datum[BLCKSZ / sizeof(IndexTupleData)];
    GinNullCategory category[BLCKSZ / sizeof(IndexTupleData)];
    bool datumExtracted[BLCKSZ / sizeof(IndexTupleData)];

    Assert(pos->lastOffset > pos->firstOffset);
    memset(datumExtracted + pos->firstOffset - 1, 0, sizeof(bool) * (pos->lastOffset - pos->firstOffset));

    page = BufferGetPage(pos->pendingBuffer);
    TestForOldSnapshot(scan->xs_snapshot, scan->indexRelation, page);

    for (i = 0; i < so->nkeys; i++)
    {
      GinScanKey key = so->keys + i;

      for (j = 0; j < key->nentries; j++)
      {
        GinScanEntry entry = key->scanEntry[j];
        OffsetNumber StopLow = pos->firstOffset, StopHigh = pos->lastOffset, StopMiddle;

                                                                  
        if (key->entryRes[j])
        {
          continue;
        }

           
                                                           
                                                                      
                                                                    
                                  
           
        while (StopLow < StopHigh)
        {
          int res;

          StopMiddle = StopLow + ((StopHigh - StopLow) >> 1);

          itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, StopMiddle));

          attrnum = gintuple_get_attrnum(&so->ginstate, itup);

          if (key->attnum < attrnum)
          {
            StopHigh = StopMiddle;
            continue;
          }
          if (key->attnum > attrnum)
          {
            StopLow = StopMiddle + 1;
            continue;
          }

          if (datumExtracted[StopMiddle - 1] == false)
          {
            datum[StopMiddle - 1] = gintuple_get_key(&so->ginstate, itup, &category[StopMiddle - 1]);
            datumExtracted[StopMiddle - 1] = true;
          }

          if (entry->queryCategory == GIN_CAT_EMPTY_QUERY)
          {
                                                          
            if (entry->searchMode == GIN_SEARCH_MODE_ALL)
            {
                                                   
              if (category[StopMiddle - 1] == GIN_CAT_NULL_ITEM)
              {
                res = -1;
              }
              else
              {
                res = 0;
              }
            }
            else
            {
                                    
              res = 0;
            }
          }
          else
          {
            res = ginCompareEntries(&so->ginstate, entry->attnum, entry->queryKey, entry->queryCategory, datum[StopMiddle - 1], category[StopMiddle - 1]);
          }

          if (res == 0)
          {
               
                                                                   
                                  
               
                                                                 
                                                 
               
                                                         
               
            if (entry->isPartialMatch)
            {
              key->entryRes[j] = matchPartialInPendingList(&so->ginstate, page, StopMiddle, pos->lastOffset, entry, datum, category, datumExtracted);
            }
            else
            {
              key->entryRes[j] = true;
            }

                                         
            break;
          }
          else if (res < 0)
          {
            StopHigh = StopMiddle;
          }
          else
          {
            StopLow = StopMiddle + 1;
          }
        }

        if (StopLow >= StopHigh && entry->isPartialMatch)
        {
             
                                                                   
                                                                    
                                                                     
                                                             
                                                                     
                                                         
             
          key->entryRes[j] = matchPartialInPendingList(&so->ginstate, page, StopHigh, pos->lastOffset, entry, datum, category, datumExtracted);
        }

        pos->hasMatchKey[i] |= key->entryRes[j];
      }
    }

                                                     
    pos->firstOffset = pos->lastOffset;

    if (GinPageHasFullRow(page))
    {
         
                                                                        
                                       
         
      break;
    }
    else
    {
         
                                                                      
                                            
         
      ItemPointerData item = pos->item;

      if (scanGetCandidate(scan, pos) == false || !ItemPointerEquals(&pos->item, &item))
      {
        elog(ERROR, "could not find additional pending pages for same heap tuple");
      }
    }
  }

     
                                                                         
     
  for (i = 0; i < so->nkeys; i++)
  {
    if (pos->hasMatchKey[i] == false)
    {
      return false;
    }
  }

  return true;
}

   
                                                           
   
static void
scanPendingInsert(IndexScanDesc scan, TIDBitmap *tbm, int64 *ntids)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;
  MemoryContext oldCtx;
  bool recheck, match;
  int i;
  pendingPosition pos;
  Buffer metabuffer = ReadBuffer(scan->indexRelation, GIN_METAPAGE_BLKNO);
  Page page;
  BlockNumber blkno;

  *ntids = 0;

     
                                                                             
                 
     
  PredicateLockPage(scan->indexRelation, GIN_METAPAGE_BLKNO, scan->xs_snapshot);

  LockBuffer(metabuffer, GIN_SHARE);
  page = BufferGetPage(metabuffer);
  TestForOldSnapshot(scan->xs_snapshot, scan->indexRelation, page);
  blkno = GinPageGetMeta(page)->head;

     
                                                                            
                                           
     
  if (blkno == InvalidBlockNumber)
  {
                                                      
    UnlockReleaseBuffer(metabuffer);
    return;
  }

  pos.pendingBuffer = ReadBuffer(scan->indexRelation, blkno);
  LockBuffer(pos.pendingBuffer, GIN_SHARE);
  pos.firstOffset = FirstOffsetNumber;
  UnlockReleaseBuffer(metabuffer);
  pos.hasMatchKey = palloc(sizeof(bool) * so->nkeys);

     
                                                                        
                             
     
  while (scanGetCandidate(scan, &pos))
  {
       
                                                         
       
                                                                      
                                                                       
                    
       
    if (!collectMatchesForHeapRow(scan, &pos))
    {
      continue;
    }

       
                                                                      
                             
       
    oldCtx = MemoryContextSwitchTo(so->tempCtx);
    recheck = false;
    match = true;

    for (i = 0; i < so->nkeys; i++)
    {
      GinScanKey key = so->keys + i;

      if (!key->boolConsistentFn(key))
      {
        match = false;
        break;
      }
      recheck |= key->recheckCurItem;
    }

    MemoryContextSwitchTo(oldCtx);
    MemoryContextReset(so->tempCtx);

    if (match)
    {
      tbm_add_tuples(tbm, &pos.item, 1, recheck);
      (*ntids)++;
    }
  }

  pfree(pos.hasMatchKey);
}

#define GinIsVoidRes(s) (((GinScanOpaque)scan->opaque)->isVoidRes)

int64
gingetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;
  int64 ntids;
  ItemPointerData iptr;
  bool recheck;

     
                                                              
     
  ginFreeScanKeys(so);                                                
                                 
  ginNewScanKey(scan);

  if (GinIsVoidRes(scan))
  {
    return 0;
  }

  ntids = 0;

     
                                                                            
                                                                             
                                                                            
                                                                            
                                                                             
                                                                           
                                                                      
                                                       
     
  scanPendingInsert(scan, tbm, &ntids);

     
                              
     
  startScan(scan);

  ItemPointerSetMin(&iptr);

  for (;;)
  {
    CHECK_FOR_INTERRUPTS();

    if (!scanGetItem(scan, iptr, &iptr, &recheck))
    {
      break;
    }

    if (ItemPointerIsLossyPage(&iptr))
    {
      tbm_add_page(tbm, ItemPointerGetBlockNumber(&iptr));
    }
    else
    {
      tbm_add_tuples(tbm, &iptr, 1, recheck);
    }
    ntids++;
  }

  return ntids;
}
