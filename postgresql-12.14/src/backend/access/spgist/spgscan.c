                                                                            
   
             
                                           
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/spgist_private.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/datum.h"
#include "utils/float.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef void (*storeRes_func)(SpGistScanOpaque so, ItemPointer heapPtr, Datum leafValue, bool isNull, bool recheck, bool recheckDistances, double *distances);

   
                                                                    
                                                                            
         
   
static int
pairingheap_SpGistSearchItem_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{
  const SpGistSearchItem *sa = (const SpGistSearchItem *)a;
  const SpGistSearchItem *sb = (const SpGistSearchItem *)b;
  SpGistScanOpaque so = (SpGistScanOpaque)arg;
  int i;

  if (sa->isNull)
  {
    if (!sb->isNull)
    {
      return -1;
    }
  }
  else if (sb->isNull)
  {
    return 1;
  }
  else
  {
                                                
    for (i = 0; i < so->numberOfNonNullOrderBys; i++)
    {
      if (isnan(sa->distances[i]) && isnan(sb->distances[i]))
      {
        continue;                 
      }
      if (isnan(sa->distances[i]))
      {
        return -1;                   
      }
      if (isnan(sb->distances[i]))
      {
        return 1;                   
      }
      if (sa->distances[i] != sb->distances[i])
      {
        return (sa->distances[i] < sb->distances[i]) ? 1 : -1;
      }
    }
  }

                                                                        
  if (sa->isLeaf && !sb->isLeaf)
  {
    return 1;
  }
  if (!sa->isLeaf && sb->isLeaf)
  {
    return -1;
  }

  return 0;
}

static void
spgFreeSearchItem(SpGistScanOpaque so, SpGistSearchItem *item)
{
  if (!so->state.attLeafType.attbyval && DatumGetPointer(item->value) != NULL)
  {
    pfree(DatumGetPointer(item->value));
  }

  if (item->traversalValue)
  {
    pfree(item->traversalValue);
  }

  pfree(item);
}

   
                                 
   
                           
   
static void
spgAddSearchItemToQueue(SpGistScanOpaque so, SpGistSearchItem *item)
{
  pairingheap_add(so->scanQueue, &item->phNode);
}

static SpGistSearchItem *
spgAllocSearchItem(SpGistScanOpaque so, bool isnull, double *distances)
{
                                                       
  SpGistSearchItem *item = palloc(SizeOfSpGistSearchItem(isnull ? 0 : so->numberOfNonNullOrderBys));

  item->isNull = isnull;

  if (!isnull && so->numberOfNonNullOrderBys > 0)
  {
    memcpy(item->distances, distances, sizeof(item->distances[0]) * so->numberOfNonNullOrderBys);
  }

  return item;
}

static void
spgAddStartItem(SpGistScanOpaque so, bool isnull)
{
  SpGistSearchItem *startEntry = spgAllocSearchItem(so, isnull, so->zeroDistances);

  ItemPointerSet(&startEntry->heapPtr, isnull ? SPGIST_NULL_BLKNO : SPGIST_ROOT_BLKNO, FirstOffsetNumber);
  startEntry->isLeaf = false;
  startEntry->level = 0;
  startEntry->value = (Datum)0;
  startEntry->traversalValue = NULL;
  startEntry->recheck = false;
  startEntry->recheckDistances = false;

  spgAddSearchItemToQueue(so, startEntry);
}

   
                                                       
                              
   
static void
resetSpGistScanOpaque(SpGistScanOpaque so)
{
  MemoryContext oldCtx;

     
                                                                           
                                                                        
              
     
  MemoryContextReset(so->traversalCxt);

  oldCtx = MemoryContextSwitchTo(so->traversalCxt);

                                                        
  so->scanQueue = pairingheap_allocate(pairingheap_SpGistSearchItem_cmp, so);

  if (so->searchNulls)
  {
                                                        
    spgAddStartItem(so, true);
  }

  if (so->searchNonNulls)
  {
                                                            
    spgAddStartItem(so, false);
  }

  MemoryContextSwitchTo(oldCtx);

  if (so->numberOfOrderBys > 0)
  {
                                                   
    int i;

    for (i = 0; i < so->nPtrs; i++)
    {
      if (so->distances[i])
      {
        pfree(so->distances[i]);
      }
    }
  }

  if (so->want_itup)
  {
                                                              
    int i;

    for (i = 0; i < so->nPtrs; i++)
    {
      pfree(so->reconTups[i]);
    }
  }
  so->iPtr = so->nPtrs = 0;
}

   
                                                                     
   
                                                                          
   
                                                                            
                                                                          
                                                                        
                                                                      
                                                                          
   
static void
spgPrepareScanKeys(IndexScanDesc scan)
{
  SpGistScanOpaque so = (SpGistScanOpaque)scan->opaque;
  bool qual_ok;
  bool haveIsNull;
  bool haveNotNull;
  int nkeys;
  int i;

  so->numberOfOrderBys = scan->numberOfOrderBys;
  so->orderByData = scan->orderByData;

  if (so->numberOfOrderBys <= 0)
  {
    so->numberOfNonNullOrderBys = 0;
  }
  else
  {
    int j = 0;

       
                                                                        
              
       
    for (i = 0; i < scan->numberOfOrderBys; i++)
    {
      ScanKey skey = &so->orderByData[i];

      if (skey->sk_flags & SK_ISNULL)
      {
        so->nonNullOrderByOffsets[i] = -1;
      }
      else
      {
        if (i != j)
        {
          so->orderByData[j] = *skey;
        }

        so->nonNullOrderByOffsets[i] = j++;
      }
    }

    so->numberOfNonNullOrderBys = j;
  }

  if (scan->numberOfKeys <= 0)
  {
                                                   
    so->searchNulls = true;
    so->searchNonNulls = true;
    so->numberOfKeys = 0;
    return;
  }

                               
  qual_ok = true;
  haveIsNull = haveNotNull = false;
  nkeys = 0;
  for (i = 0; i < scan->numberOfKeys; i++)
  {
    ScanKey skey = &scan->keyData[i];

    if (skey->sk_flags & SK_SEARCHNULL)
    {
      haveIsNull = true;
    }
    else if (skey->sk_flags & SK_SEARCHNOTNULL)
    {
      haveNotNull = true;
    }
    else if (skey->sk_flags & SK_ISNULL)
    {
                                                            
      qual_ok = false;
      break;
    }
    else
    {
                                                     
      so->keyData[nkeys++] = *skey;
                                                           
      haveNotNull = true;
    }
  }

                                                                   
  if (haveIsNull && haveNotNull)
  {
    qual_ok = false;
  }

                    
  if (qual_ok)
  {
    so->searchNulls = haveIsNull;
    so->searchNonNulls = haveNotNull;
    so->numberOfKeys = nkeys;
  }
  else
  {
    so->searchNulls = false;
    so->searchNonNulls = false;
    so->numberOfKeys = 0;
  }
}

IndexScanDesc
spgbeginscan(Relation rel, int keysz, int orderbysz)
{
  IndexScanDesc scan;
  SpGistScanOpaque so;
  int i;

  scan = RelationGetIndexScan(rel, keysz, orderbysz);

  so = (SpGistScanOpaque)palloc0(sizeof(SpGistScanOpaqueData));
  if (keysz > 0)
  {
    so->keyData = (ScanKey)palloc(sizeof(ScanKeyData) * keysz);
  }
  else
  {
    so->keyData = NULL;
  }
  initSpGistState(&so->state, scan->indexRelation);

  so->tempCxt = AllocSetContextCreate(CurrentMemoryContext, "SP-GiST search temporary context", ALLOCSET_DEFAULT_SIZES);
  so->traversalCxt = AllocSetContextCreate(CurrentMemoryContext, "SP-GiST traversal-value context", ALLOCSET_DEFAULT_SIZES);

                                                                            
  so->indexTupDesc = scan->xs_hitupdesc = RelationGetDescr(rel);

                                                         
  if (scan->numberOfOrderBys > 0)
  {
                                                                       
    so->orderByTypes = (Oid *)palloc(sizeof(Oid) * scan->numberOfOrderBys);
    so->nonNullOrderByOffsets = (int *)palloc(sizeof(int) * scan->numberOfOrderBys);

                                                                      
    so->zeroDistances = (double *)palloc(sizeof(double) * scan->numberOfOrderBys);
    so->infDistances = (double *)palloc(sizeof(double) * scan->numberOfOrderBys);

    for (i = 0; i < scan->numberOfOrderBys; i++)
    {
      so->zeroDistances[i] = 0.0;
      so->infDistances[i] = get_float8_infinity();
    }

    scan->xs_orderbyvals = (Datum *)palloc0(sizeof(Datum) * scan->numberOfOrderBys);
    scan->xs_orderbynulls = (bool *)palloc(sizeof(bool) * scan->numberOfOrderBys);
    memset(scan->xs_orderbynulls, true, sizeof(bool) * scan->numberOfOrderBys);
  }

  fmgr_info_copy(&so->innerConsistentFn, index_getprocinfo(rel, 1, SPGIST_INNER_CONSISTENT_PROC), CurrentMemoryContext);

  fmgr_info_copy(&so->leafConsistentFn, index_getprocinfo(rel, 1, SPGIST_LEAF_CONSISTENT_PROC), CurrentMemoryContext);

  so->indexCollation = rel->rd_indcollation[0];

  scan->opaque = so;

  return scan;
}

void
spgrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{
  SpGistScanOpaque so = (SpGistScanOpaque)scan->opaque;

                                        
  if (scankey && scan->numberOfKeys > 0)
  {
    memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
  }

                                          
  if (orderbys && scan->numberOfOrderBys > 0)
  {
    int i;

    memmove(scan->orderByData, orderbys, scan->numberOfOrderBys * sizeof(ScanKeyData));

    for (i = 0; i < scan->numberOfOrderBys; i++)
    {
      ScanKey skey = &scan->orderByData[i];

         
                                                                
                                                                 
                                                                     
         
                                                                       
                                                                         
                                                                        
                                                                     
                                                                    
                     
         
      so->orderByTypes[i] = get_func_rettype(skey->sk_func.fn_oid);
    }
  }

                                                             
  spgPrepareScanKeys(scan);

                                     
  resetSpGistScanOpaque(so);

                                    
  pgstat_count_index_scan(scan->indexRelation);
}

void
spgendscan(IndexScanDesc scan)
{
  SpGistScanOpaque so = (SpGistScanOpaque)scan->opaque;

  MemoryContextDelete(so->tempCxt);
  MemoryContextDelete(so->traversalCxt);

  if (so->keyData)
  {
    pfree(so->keyData);
  }

  if (so->state.deadTupleStorage)
  {
    pfree(so->state.deadTupleStorage);
  }

  if (scan->numberOfOrderBys > 0)
  {
    pfree(so->orderByTypes);
    pfree(so->nonNullOrderByOffsets);
    pfree(so->zeroDistances);
    pfree(so->infDistances);
    pfree(scan->xs_orderbyvals);
    pfree(scan->xs_orderbynulls);
  }

  pfree(so);
}

   
                                                              
   
static SpGistSearchItem *
spgNewHeapItem(SpGistScanOpaque so, int level, ItemPointer heapPtr, Datum leafValue, bool recheck, bool recheckDistances, bool isnull, double *distances)
{
  SpGistSearchItem *item = spgAllocSearchItem(so, isnull, distances);

  item->level = level;
  item->heapPtr = *heapPtr;

     
                                                                         
                                                                             
                                                                           
                                                                            
                                                                            
                                                                           
              
     
  if (so->want_itup)
  {
    item->value = isnull ? (Datum)0 : datumCopy(leafValue, so->state.attLeafType.attbyval, so->state.attLeafType.attlen);
  }
  else
  {
    item->value = (Datum)0;
  }
  item->traversalValue = NULL;
  item->isLeaf = true;
  item->recheck = recheck;
  item->recheckDistances = recheckDistances;

  return item;
}

   
                                                         
   
                                    
                                                                
   
static bool
spgLeafTest(SpGistScanOpaque so, SpGistSearchItem *item, SpGistLeafTuple leafTuple, bool isnull, bool *reportedSome, storeRes_func storeRes)
{
  Datum leafValue;
  double *distances;
  bool result;
  bool recheck;
  bool recheckDistances;

  if (isnull)
  {
                                                                         
    Assert(so->searchNulls);
    leafValue = (Datum)0;
    distances = NULL;
    recheck = false;
    recheckDistances = false;
    result = true;
  }
  else
  {
    spgLeafConsistentIn in;
    spgLeafConsistentOut out;

                                                      
    MemoryContext oldCxt = MemoryContextSwitchTo(so->tempCxt);

    in.scankeys = so->keyData;
    in.nkeys = so->numberOfKeys;
    in.orderbys = so->orderByData;
    in.norderbys = so->numberOfNonNullOrderBys;
    in.reconstructedValue = item->value;
    in.traversalValue = item->traversalValue;
    in.level = item->level;
    in.returnData = so->want_itup;
    in.leafDatum = SGLTDATUM(leafTuple, &so->state);

    out.leafValue = (Datum)0;
    out.recheck = false;
    out.distances = NULL;
    out.recheckDistances = false;

    result = DatumGetBool(FunctionCall2Coll(&so->leafConsistentFn, so->indexCollation, PointerGetDatum(&in), PointerGetDatum(&out)));
    recheck = out.recheck;
    recheckDistances = out.recheckDistances;
    leafValue = out.leafValue;
    distances = out.distances;

    MemoryContextSwitchTo(oldCxt);
  }

  if (result)
  {
                                  
    if (so->numberOfNonNullOrderBys > 0)
    {
                                                            
      MemoryContext oldCxt = MemoryContextSwitchTo(so->traversalCxt);
      SpGistSearchItem *heapItem = spgNewHeapItem(so, item->level, &leafTuple->heapPtr, leafValue, recheck, recheckDistances, isnull, distances);

      spgAddSearchItemToQueue(so, heapItem);

      MemoryContextSwitchTo(oldCxt);
    }
    else
    {
                                                           
      Assert(!recheckDistances);
      storeRes(so, &leafTuple->heapPtr, leafValue, isnull, recheck, false, NULL);
      *reportedSome = true;
    }
  }

  return result;
}

                                                       
static void
spgInitInnerConsistentIn(spgInnerConsistentIn *in, SpGistScanOpaque so, SpGistSearchItem *item, SpGistInnerTuple innerTuple)
{
  in->scankeys = so->keyData;
  in->orderbys = so->orderByData;
  in->nkeys = so->numberOfKeys;
  in->norderbys = so->numberOfNonNullOrderBys;
  in->reconstructedValue = item->value;
  in->traversalMemoryContext = so->traversalCxt;
  in->traversalValue = item->traversalValue;
  in->level = item->level;
  in->returnData = so->want_itup;
  in->allTheSame = innerTuple->allTheSame;
  in->hasPrefix = (innerTuple->prefixSize > 0);
  in->prefixDatum = SGITDATUM(innerTuple, &so->state);
  in->nNodes = innerTuple->nNodes;
  in->nodeLabels = spgExtractNodeLabels(&so->state, innerTuple);
}

static SpGistSearchItem *
spgMakeInnerItem(SpGistScanOpaque so, SpGistSearchItem *parentItem, SpGistNodeTuple tuple, spgInnerConsistentOut *out, int i, bool isnull, double *distances)
{
  SpGistSearchItem *item = spgAllocSearchItem(so, isnull, distances);

  item->heapPtr = tuple->t_tid;
  item->level = out->levelAdds ? parentItem->level + out->levelAdds[i] : parentItem->level;

                                           
  item->value = out->reconstructedValues ? datumCopy(out->reconstructedValues[i], so->state.attLeafType.attbyval, so->state.attLeafType.attlen) : (Datum)0;

     
                                                            
                                                                          
                 
     
  item->traversalValue = out->traversalValues ? out->traversalValues[i] : NULL;

  item->isLeaf = false;
  item->recheck = false;
  item->recheckDistances = false;

  return item;
}

static void
spgInnerTest(SpGistScanOpaque so, SpGistSearchItem *item, SpGistInnerTuple innerTuple, bool isnull)
{
  MemoryContext oldCxt = MemoryContextSwitchTo(so->tempCxt);
  spgInnerConsistentOut out;
  int nNodes = innerTuple->nNodes;
  int i;

  memset(&out, 0, sizeof(out));

  if (!isnull)
  {
    spgInnerConsistentIn in;

    spgInitInnerConsistentIn(&in, so, item, innerTuple);

                                                  
    FunctionCall2Coll(&so->innerConsistentFn, so->indexCollation, PointerGetDatum(&in), PointerGetDatum(&out));
  }
  else
  {
                                          
    out.nNodes = nNodes;
    out.nodeNumbers = (int *)palloc(sizeof(int) * nNodes);
    for (i = 0; i < nNodes; i++)
    {
      out.nodeNumbers[i] = i;
    }
  }

                                                            
  if (innerTuple->allTheSame && out.nNodes != 0 && out.nNodes != nNodes)
  {
    elog(ERROR, "inconsistent inner_consistent results for allTheSame inner tuple");
  }

  if (out.nNodes)
  {
                               
    SpGistNodeTuple node;
    SpGistNodeTuple *nodes = (SpGistNodeTuple *)palloc(sizeof(SpGistNodeTuple) * nNodes);

    SGITITERATE(innerTuple, i, node) { nodes[i] = node; }

    MemoryContextSwitchTo(so->traversalCxt);

    for (i = 0; i < out.nNodes; i++)
    {
      int nodeN = out.nodeNumbers[i];
      SpGistSearchItem *innerItem;
      double *distances;

      Assert(nodeN >= 0 && nodeN < nNodes);

      node = nodes[nodeN];

      if (!ItemPointerIsValid(&node->t_tid))
      {
        continue;
      }

         
                                                                      
                                                                        
         
      distances = out.distances ? out.distances[i] : so->infDistances;

      innerItem = spgMakeInnerItem(so, item, node, &out, i, isnull, distances);

      spgAddSearchItemToQueue(so, innerItem);
    }
  }

  MemoryContextSwitchTo(oldCxt);
}

                                                                                
static SpGistSearchItem *
spgGetNextQueueItem(SpGistScanOpaque so)
{
  if (pairingheap_is_empty(so->scanQueue))
  {
    return NULL;                                     
  }

                                                      
  return (SpGistSearchItem *)pairingheap_remove_first(so->scanQueue);
}

enum SpGistSpecialOffsetNumbers
{
  SpGistBreakOffsetNumber = InvalidOffsetNumber,
  SpGistRedirectOffsetNumber = MaxOffsetNumber + 1,
  SpGistErrorOffsetNumber = MaxOffsetNumber + 2
};

static OffsetNumber
spgTestLeafTuple(SpGistScanOpaque so, SpGistSearchItem *item, Page page, OffsetNumber offset, bool isnull, bool isroot, bool *reportedSome, storeRes_func storeRes)
{
  SpGistLeafTuple leafTuple = (SpGistLeafTuple)PageGetItem(page, PageGetItemId(page, offset));

  if (leafTuple->tupstate != SPGIST_LIVE)
  {
    if (!isroot)                                        
    {
      if (leafTuple->tupstate == SPGIST_REDIRECT)
      {
                                                        
        Assert(offset == ItemPointerGetOffsetNumber(&item->heapPtr));
                                                  
        item->heapPtr = ((SpGistDeadTuple)leafTuple)->pointer;
        Assert(ItemPointerGetBlockNumber(&item->heapPtr) != SPGIST_METAPAGE_BLKNO);
        return SpGistRedirectOffsetNumber;
      }

      if (leafTuple->tupstate == SPGIST_DEAD)
      {
                                                 
        Assert(offset == ItemPointerGetOffsetNumber(&item->heapPtr));
                                          
        Assert(leafTuple->nextOffset == InvalidOffsetNumber);
        return SpGistBreakOffsetNumber;
      }
    }

                                               
    elog(ERROR, "unexpected SPGiST tuple state: %d", leafTuple->tupstate);
    return SpGistErrorOffsetNumber;
  }

  Assert(ItemPointerIsValid(&leafTuple->heapPtr));

  spgLeafTest(so, item, leafTuple, isnull, reportedSome, storeRes);

  return leafTuple->nextOffset;
}

   
                                                                              
               
   
                                                                             
                                                                
   
static void
spgWalk(Relation index, SpGistScanOpaque so, bool scanWholeIndex, storeRes_func storeRes, Snapshot snapshot)
{
  Buffer buffer = InvalidBuffer;
  bool reportedSome = false;

  while (scanWholeIndex || !reportedSome)
  {
    SpGistSearchItem *item = spgGetNextQueueItem(so);

    if (item == NULL)
    {
      break;                                     
    }

  redirect:
                                                             
    CHECK_FOR_INTERRUPTS();

    if (item->isLeaf)
    {
                                                                           
      Assert(so->numberOfNonNullOrderBys > 0);
      storeRes(so, &item->heapPtr, item->value, item->isNull, item->recheck, item->recheckDistances, item->distances);
      reportedSome = true;
    }
    else
    {
      BlockNumber blkno = ItemPointerGetBlockNumber(&item->heapPtr);
      OffsetNumber offset = ItemPointerGetOffsetNumber(&item->heapPtr);
      Page page;
      bool isnull;

      if (buffer == InvalidBuffer)
      {
        buffer = ReadBuffer(index, blkno);
        LockBuffer(buffer, BUFFER_LOCK_SHARE);
      }
      else if (blkno != BufferGetBlockNumber(buffer))
      {
        UnlockReleaseBuffer(buffer);
        buffer = ReadBuffer(index, blkno);
        LockBuffer(buffer, BUFFER_LOCK_SHARE);
      }

                                                                    

      page = BufferGetPage(buffer);
      TestForOldSnapshot(snapshot, index, page);

      isnull = SpGistPageStoresNulls(page) ? true : false;

      if (SpGistPageIsLeaf(page))
      {
                                                                      
        OffsetNumber max = PageGetMaxOffsetNumber(page);

        if (SpGistBlockIsRoot(blkno))
        {
                                                           
          for (offset = FirstOffsetNumber; offset <= max; offset++)
          {
            (void)spgTestLeafTuple(so, item, page, offset, isnull, true, &reportedSome, storeRes);
          }
        }
        else
        {
                                                                 
          while (offset != InvalidOffsetNumber)
          {
            Assert(offset >= FirstOffsetNumber && offset <= max);
            offset = spgTestLeafTuple(so, item, page, offset, isnull, false, &reportedSome, storeRes);
            if (offset == SpGistRedirectOffsetNumber)
            {
              goto redirect;
            }
          }
        }
      }
      else                    
      {
        SpGistInnerTuple innerTuple = (SpGistInnerTuple)PageGetItem(page, PageGetItemId(page, offset));

        if (innerTuple->tupstate != SPGIST_LIVE)
        {
          if (innerTuple->tupstate == SPGIST_REDIRECT)
          {
                                                      
            item->heapPtr = ((SpGistDeadTuple)innerTuple)->pointer;
            Assert(ItemPointerGetBlockNumber(&item->heapPtr) != SPGIST_METAPAGE_BLKNO);
            goto redirect;
          }
          elog(ERROR, "unexpected SPGiST tuple state: %d", innerTuple->tupstate);
        }

        spgInnerTest(so, item, innerTuple, isnull);
      }
    }

                                  
    spgFreeSearchItem(so, item);
                                                              
    MemoryContextReset(so->tempCxt);
  }

  if (buffer != InvalidBuffer)
  {
    UnlockReleaseBuffer(buffer);
  }
}

                                            
static void
storeBitmap(SpGistScanOpaque so, ItemPointer heapPtr, Datum leafValue, bool isnull, bool recheck, bool recheckDistances, double *distances)
{
  Assert(!recheckDistances && !distances);
  tbm_add_tuples(so->tbm, heapPtr, 1, recheck);
  so->ntids++;
}

int64
spggetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{
  SpGistScanOpaque so = (SpGistScanOpaque)scan->opaque;

                                                                           
  so->want_itup = false;

  so->tbm = tbm;
  so->ntids = 0;

  spgWalk(scan->indexRelation, so, true, storeBitmap, scan->xs_snapshot);

  return so->ntids;
}

                                           
static void
storeGettuple(SpGistScanOpaque so, ItemPointer heapPtr, Datum leafValue, bool isnull, bool recheck, bool recheckDistances, double *nonNullDistances)
{
  Assert(so->nPtrs < MaxIndexTuplesPerPage);
  so->heapPtrs[so->nPtrs] = *heapPtr;
  so->recheck[so->nPtrs] = recheck;
  so->recheckDistances[so->nPtrs] = recheckDistances;

  if (so->numberOfOrderBys > 0)
  {
    if (isnull || so->numberOfNonNullOrderBys <= 0)
    {
      so->distances[so->nPtrs] = NULL;
    }
    else
    {
      IndexOrderByDistance *distances = palloc(sizeof(distances[0]) * so->numberOfOrderBys);
      int i;

      for (i = 0; i < so->numberOfOrderBys; i++)
      {
        int offset = so->nonNullOrderByOffsets[i];

        if (offset >= 0)
        {
                                            
          distances[i].value = nonNullDistances[offset];
          distances[i].isnull = false;
        }
        else
        {
                                         
          distances[i].value = 0.0;
          distances[i].isnull = true;
        }
      }

      so->distances[so->nPtrs] = distances;
    }
  }

  if (so->want_itup)
  {
       
                                                                          
                                                                
       
    so->reconTups[so->nPtrs] = heap_form_tuple(so->indexTupDesc, &leafValue, &isnull);
  }
  so->nPtrs++;
}

bool
spggettuple(IndexScanDesc scan, ScanDirection dir)
{
  SpGistScanOpaque so = (SpGistScanOpaque)scan->opaque;

  if (dir != ForwardScanDirection)
  {
    elog(ERROR, "SP-GiST only supports forward scan direction");
  }

                                                                           
  so->want_itup = scan->xs_want_itup;

  for (;;)
  {
    if (so->iPtr < so->nPtrs)
    {
                                                
      scan->xs_heaptid = so->heapPtrs[so->iPtr];
      scan->xs_recheck = so->recheck[so->iPtr];
      scan->xs_hitup = so->reconTups[so->iPtr];

      if (so->numberOfOrderBys > 0)
      {
        index_store_float8_orderby_distances(scan, so->orderByTypes, so->distances[so->iPtr], so->recheckDistances[so->iPtr]);
      }
      so->iPtr++;
      return true;
    }

    if (so->numberOfOrderBys > 0)
    {
                                                     
      int i;

      for (i = 0; i < so->nPtrs; i++)
      {
        if (so->distances[i])
        {
          pfree(so->distances[i]);
        }
      }
    }

    if (so->want_itup)
    {
                                                                
      int i;

      for (i = 0; i < so->nPtrs; i++)
      {
        pfree(so->reconTups[i]);
      }
    }
    so->iPtr = so->nPtrs = 0;

    spgWalk(scan->indexRelation, so, false, storeGettuple, scan->xs_snapshot);

    if (so->nPtrs == 0)
    {
      break;                               
    }
  }

  return false;
}

bool
spgcanreturn(Relation index, int attno)
{
  SpGistCache *cache;

                                                           
  cache = spgGetCache(index);

  return cache->config.canReturnData;
}
