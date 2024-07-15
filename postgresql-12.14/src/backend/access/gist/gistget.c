                                                                            
   
             
                                    
   
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/gist_private.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "pgstat.h"
#include "lib/pairingheap.h"
#include "utils/float.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                          
                        
   
                                                                          
                                                                           
                                                                           
                                                                      
   
static void
gistkillitems(IndexScanDesc scan)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId iid;
  int i;
  bool killedsomething = false;

  Assert(so->curBlkno != InvalidBlockNumber);
  Assert(!XLogRecPtrIsInvalid(so->curPageLSN));
  Assert(so->killedItems != NULL);

  buffer = ReadBuffer(scan->indexRelation, so->curBlkno);
  if (!BufferIsValid(buffer))
  {
    return;
  }

  LockBuffer(buffer, GIST_SHARE);
  gistcheckpage(scan->indexRelation, buffer);
  page = BufferGetPage(buffer);

     
                                                                            
                                                                           
           
     
  if (BufferGetLSNAtomic(buffer) != so->curPageLSN)
  {
    UnlockReleaseBuffer(buffer);
    so->numKilled = 0;                    
    return;
  }

  Assert(GistPageIsLeaf(page));

     
                                                                           
                                                      
     
  for (i = 0; i < so->numKilled; i++)
  {
    offnum = so->killedItems[i];
    iid = PageGetItemId(page, offnum);
    ItemIdMarkDead(iid);
    killedsomething = true;
  }

  if (killedsomething)
  {
    GistMarkPageHasGarbage(page);
    MarkBufferDirtyHint(buffer, true);
  }

  UnlockReleaseBuffer(buffer);

     
                                                                           
            
     
  so->numKilled = 0;
}

   
                                                                         
   
                                                                              
                                                                   
   
                                                                             
                                                                          
                                                                               
                                                                         
                                                                            
                                                                      
   
                                                                            
                                                                
   
                                                                         
                                                                             
   
                                                                              
                                                                             
                                                                   
   
static bool
gistindex_keytest(IndexScanDesc scan, IndexTuple tuple, Page page, OffsetNumber offset, bool *recheck_p, bool *recheck_distances_p)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;
  GISTSTATE *giststate = so->giststate;
  ScanKey key = scan->keyData;
  int keySize = scan->numberOfKeys;
  IndexOrderByDistance *distance_p;
  Relation r = scan->indexRelation;

  *recheck_p = false;
  *recheck_distances_p = false;

     
                                                                             
                                                                           
                      
     
  if (GistTupleIsInvalid(tuple))
  {
    int i;

    if (GistPageIsLeaf(page))                       
    {
      elog(ERROR, "invalid GiST tuple found on leaf page");
    }
    for (i = 0; i < scan->numberOfOrderBys; i++)
    {
      so->distances[i].value = -get_float8_infinity();
      so->distances[i].isnull = false;
    }
    return true;
  }

                                                                      
  while (keySize > 0)
  {
    Datum datum;
    bool isNull;

    datum = index_getattr(tuple, key->sk_attno, giststate->leafTupdesc, &isNull);

    if (key->sk_flags & SK_ISNULL)
    {
         
                                                                   
                                                                         
                                                                    
               
         
      if (key->sk_flags & SK_SEARCHNULL)
      {
        if (GistPageIsLeaf(page) && !isNull)
        {
          return false;
        }
      }
      else
      {
        Assert(key->sk_flags & SK_SEARCHNOTNULL);
        if (isNull)
        {
          return false;
        }
      }
    }
    else if (isNull)
    {
      return false;
    }
    else
    {
      Datum test;
      bool recheck;
      GISTENTRY de;

      gistdentryinit(giststate, key->sk_attno - 1, &de, datum, r, page, offset, false, isNull);

         
                                                                 
                                                                         
                                                                      
                                             
         
                                                                    
                                                                       
               
         
                                                                        
                                                            
         
      recheck = true;

      test = FunctionCall5Coll(&key->sk_func, key->sk_collation, PointerGetDatum(&de), key->sk_argument, Int16GetDatum(key->sk_strategy), ObjectIdGetDatum(key->sk_subtype), PointerGetDatum(&recheck));

      if (!DatumGetBool(test))
      {
        return false;
      }
      *recheck_p |= recheck;
    }

    key++;
    keySize--;
  }

                                                         
  key = scan->orderByData;
  distance_p = so->distances;
  keySize = scan->numberOfOrderBys;
  while (keySize > 0)
  {
    Datum datum;
    bool isNull;

    datum = index_getattr(tuple, key->sk_attno, giststate->leafTupdesc, &isNull);

    if ((key->sk_flags & SK_ISNULL) || isNull)
    {
                                            
      distance_p->value = 0.0;
      distance_p->isnull = true;
    }
    else
    {
      Datum dist;
      bool recheck;
      GISTENTRY de;

      gistdentryinit(giststate, key->sk_attno - 1, &de, datum, r, page, offset, false, isNull);

         
                                                                   
                                                                         
                                                                         
                                        
         
                                                                    
                                                                       
               
         
                                                                         
                                                                       
                                                                    
                                                                        
                                                             
         
      recheck = false;
      dist = FunctionCall5Coll(&key->sk_func, key->sk_collation, PointerGetDatum(&de), key->sk_argument, Int16GetDatum(key->sk_strategy), ObjectIdGetDatum(key->sk_subtype), PointerGetDatum(&recheck));
      *recheck_distances_p |= recheck;
      distance_p->value = DatumGetFloat8(dist);
      distance_p->isnull = false;
    }

    key++;
    distance_p++;
    keySize--;
  }

  return true;
}

   
                                                                             
                                                     
   
                                     
                                                                 
                                                                              
                                                   
                                                            
   
                                                                        
                                                                          
                                                                          
                                                                           
                                                                         
                                                                           
         
   
                                                                        
                                                                      
                                   
   
static void
gistScanPage(IndexScanDesc scan, GISTSearchItem *pageItem, IndexOrderByDistance *myDistances, TIDBitmap *tbm, int64 *ntids)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;
  GISTSTATE *giststate = so->giststate;
  Relation r = scan->indexRelation;
  Buffer buffer;
  Page page;
  GISTPageOpaque opaque;
  OffsetNumber maxoff;
  OffsetNumber i;
  MemoryContext oldcxt;

  Assert(!GISTSearchItemIsHeap(*pageItem));

  buffer = ReadBuffer(scan->indexRelation, pageItem->blkno);
  LockBuffer(buffer, GIST_SHARE);
  PredicateLockPage(r, BufferGetBlockNumber(buffer), scan->xs_snapshot);
  gistcheckpage(scan->indexRelation, buffer);
  page = BufferGetPage(buffer);
  TestForOldSnapshot(scan->xs_snapshot, r, page);
  opaque = GistPageGetOpaque(page);

     
                                                                           
                                                                            
                                                                       
                                                       
     
  if (!XLogRecPtrIsInvalid(pageItem->data.parentlsn) && (GistFollowRight(page) || pageItem->data.parentlsn < GistPageGetNSN(page)) && opaque->rightlink != InvalidBlockNumber                   )
  {
                                                                
    GISTSearchItem *item;

                                                     
    Assert(myDistances != NULL);

    oldcxt = MemoryContextSwitchTo(so->queueCxt);

                                                                    
    item = palloc(SizeOfGISTSearchItem(scan->numberOfOrderBys));
    item->blkno = opaque->rightlink;
    item->data.parentlsn = pageItem->data.parentlsn;

                                                                        
    memcpy(item->distances, myDistances, sizeof(item->distances[0]) * scan->numberOfOrderBys);

    pairingheap_add(so->queue, &item->phNode);

    MemoryContextSwitchTo(oldcxt);
  }

     
                                                                      
                                                                      
                                                                      
                                                                       
                                                                        
                                           
     
  if (GistPageIsDeleted(page))
  {
    UnlockReleaseBuffer(buffer);
    return;
  }

  so->nPageData = so->curPageData = 0;
  scan->xs_hitup = NULL;                                   
  if (so->pageDataCxt)
  {
    MemoryContextReset(so->pageDataCxt);
  }

     
                                                                           
                                                                           
                                                                    
     
  so->curPageLSN = BufferGetLSNAtomic(buffer);

     
                              
     
  maxoff = PageGetMaxOffsetNumber(page);
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    ItemId iid = PageGetItemId(page, i);
    IndexTuple it;
    bool match;
    bool recheck;
    bool recheck_distances;

       
                                                                          
                                             
       
    if (scan->ignore_killed_tuples && ItemIdIsDead(iid))
    {
      continue;
    }

    it = (IndexTuple)PageGetItem(page, iid);

       
                                                                         
                       
       
    oldcxt = MemoryContextSwitchTo(so->giststate->tempCxt);

    match = gistindex_keytest(scan, it, page, i, &recheck, &recheck_distances);

    MemoryContextSwitchTo(oldcxt);
    MemoryContextReset(so->giststate->tempCxt);

                                          
    if (!match)
    {
      continue;
    }

    if (tbm && GistPageIsLeaf(page))
    {
         
                                                                      
                                         
         
      tbm_add_tuples(tbm, &it->t_tid, 1, recheck);
      (*ntids)++;
    }
    else if (scan->numberOfOrderBys == 0 && GistPageIsLeaf(page))
    {
         
                                                              
         
      so->pageData[so->nPageData].heapPtr = it->t_tid;
      so->pageData[so->nPageData].recheck = recheck;
      so->pageData[so->nPageData].offnum = i;

         
                                                                         
                                                         
         
      if (scan->xs_want_itup)
      {
        oldcxt = MemoryContextSwitchTo(so->pageDataCxt);
        so->pageData[so->nPageData].recontup = gistFetchTuple(giststate, r, it);
        MemoryContextSwitchTo(oldcxt);
      }
      so->nPageData++;
    }
    else
    {
         
                                                                      
                                                                  
                 
         
      GISTSearchItem *item;
      int nOrderBys = scan->numberOfOrderBys;

      oldcxt = MemoryContextSwitchTo(so->queueCxt);

                                                   
      item = palloc(SizeOfGISTSearchItem(scan->numberOfOrderBys));

      if (GistPageIsLeaf(page))
      {
                                                
        item->blkno = InvalidBlockNumber;
        item->data.heap.heapPtr = it->t_tid;
        item->data.heap.recheck = recheck;
        item->data.heap.recheckDistances = recheck_distances;

           
                                                                      
           
        if (scan->xs_want_itup)
        {
          item->data.heap.recontup = gistFetchTuple(giststate, r, it);
        }
      }
      else
      {
                                                
        item->blkno = ItemPointerGetBlockNumber(&it->t_tid);

           
                                                                   
                                                              
                       
           
        item->data.parentlsn = BufferGetLSNAtomic(buffer);
      }

                                                            
      memcpy(item->distances, so->distances, sizeof(item->distances[0]) * nOrderBys);

      pairingheap_add(so->queue, &item->phNode);

      MemoryContextSwitchTo(oldcxt);
    }
  }

  UnlockReleaseBuffer(buffer);
}

   
                                                  
   
                                                                                
   
static GISTSearchItem *
getNextGISTSearchItem(GISTScanOpaque so)
{
  GISTSearchItem *item;

  if (!pairingheap_is_empty(so->queue))
  {
    item = (GISTSearchItem *)pairingheap_remove_first(so->queue);
  }
  else
  {
                                        
    item = NULL;
  }

                                                      
  return item;
}

   
                                              
   
static bool
getNextNearest(IndexScanDesc scan)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;
  bool res = false;

  if (scan->xs_hitup)
  {
                                        
    pfree(scan->xs_hitup);
    scan->xs_hitup = NULL;
  }

  do
  {
    GISTSearchItem *item = getNextGISTSearchItem(so);

    if (!item)
    {
      break;
    }

    if (GISTSearchItemIsHeap(*item))
    {
                                                           
      scan->xs_heaptid = item->data.heap.heapPtr;
      scan->xs_recheck = item->data.heap.recheck;

      index_store_float8_orderby_distances(scan, so->orderByTypes, item->distances, item->data.heap.recheckDistances);

                                                                       
      if (scan->xs_want_itup)
      {
        scan->xs_hitup = item->data.heap.recontup;
      }
      res = true;
    }
    else
    {
                                                             
      CHECK_FOR_INTERRUPTS();

      gistScanPage(scan, item, item->distances, NULL, NULL);
    }

    pfree(item);
  } while (!res);

  return res;
}

   
                                                    
   
bool
gistgettuple(IndexScanDesc scan, ScanDirection dir)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;

  if (dir != ForwardScanDirection)
  {
    elog(ERROR, "GiST only supports forward scan direction");
  }

  if (!so->qual_ok)
  {
    return false;
  }

  if (so->firstCall)
  {
                                                    
    GISTSearchItem fakeItem;

    pgstat_count_index_scan(scan->indexRelation);

    so->firstCall = false;
    so->curPageData = so->nPageData = 0;
    scan->xs_hitup = NULL;
    if (so->pageDataCxt)
    {
      MemoryContextReset(so->pageDataCxt);
    }

    fakeItem.blkno = GIST_ROOT_BLKNO;
    memset(&fakeItem.data.parentlsn, 0, sizeof(GistNSN));
    gistScanPage(scan, &fakeItem, NULL, NULL, NULL);
  }

  if (scan->numberOfOrderBys > 0)
  {
                                                    
    return getNextNearest(scan);
  }
  else
  {
                                           
    for (;;)
    {
      if (so->curPageData < so->nPageData)
      {
        if (scan->kill_prior_tuple && so->curPageData > 0)
        {

          if (so->killedItems == NULL)
          {
            MemoryContext oldCxt = MemoryContextSwitchTo(so->giststate->scanCxt);

            so->killedItems = (OffsetNumber *)palloc(MaxIndexTuplesPerPage * sizeof(OffsetNumber));

            MemoryContextSwitchTo(oldCxt);
          }
          if (so->numKilled < MaxIndexTuplesPerPage)
          {
            so->killedItems[so->numKilled++] = so->pageData[so->curPageData - 1].offnum;
          }
        }
                                                          
        scan->xs_heaptid = so->pageData[so->curPageData].heapPtr;
        scan->xs_recheck = so->pageData[so->curPageData].recheck;

                                                                        
        if (scan->xs_want_itup)
        {
          scan->xs_hitup = so->pageData[so->curPageData].recontup;
        }

        so->curPageData++;

        return true;
      }

         
                                                                  
                   
         
      if (scan->kill_prior_tuple && so->curPageData > 0 && so->curPageData == so->nPageData)
      {

        if (so->killedItems == NULL)
        {
          MemoryContext oldCxt = MemoryContextSwitchTo(so->giststate->scanCxt);

          so->killedItems = (OffsetNumber *)palloc(MaxIndexTuplesPerPage * sizeof(OffsetNumber));

          MemoryContextSwitchTo(oldCxt);
        }
        if (so->numKilled < MaxIndexTuplesPerPage)
        {
          so->killedItems[so->numKilled++] = so->pageData[so->curPageData - 1].offnum;
        }
      }
                                                
      do
      {
        GISTSearchItem *item;

        if ((so->curBlkno != InvalidBlockNumber) && (so->numKilled > 0))
        {
          gistkillitems(scan);
        }

        item = getNextGISTSearchItem(so);

        if (!item)
        {
          return false;
        }

        CHECK_FOR_INTERRUPTS();

                                                                         
        so->curBlkno = item->blkno;

           
                                                                     
                                                                   
                                                                       
                        
           
        gistScanPage(scan, item, item->distances, NULL, NULL);

        pfree(item);
      } while (so->nPageData == 0);
    }
  }
}

   
                                                               
   
int64
gistgetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;
  int64 ntids = 0;
  GISTSearchItem fakeItem;

  if (!so->qual_ok)
  {
    return 0;
  }

  pgstat_count_index_scan(scan->indexRelation);

                                                  
  so->curPageData = so->nPageData = 0;
  scan->xs_hitup = NULL;
  if (so->pageDataCxt)
  {
    MemoryContextReset(so->pageDataCxt);
  }

  fakeItem.blkno = GIST_ROOT_BLKNO;
  memset(&fakeItem.data.parentlsn, 0, sizeof(GistNSN));
  gistScanPage(scan, &fakeItem, NULL, tbm, &ntids);

     
                                                                           
                                                                           
     
  for (;;)
  {
    GISTSearchItem *item = getNextGISTSearchItem(so);

    if (!item)
    {
      break;
    }

    CHECK_FOR_INTERRUPTS();

    gistScanPage(scan, item, item->distances, tbm, &ntids);

    pfree(item);
  }

  return ntids;
}

   
                                                         
   
                                                                       
                                                                          
                                                                   
   
bool
gistcanreturn(Relation index, int attno)
{
  if (attno > IndexRelationGetNumberOfKeyAttributes(index) || OidIsValid(index_getprocid(index, attno, GIST_FETCH_PROC)) || !OidIsValid(index_getprocid(index, attno, GIST_COMPRESS_PROC)))
  {
    return true;
  }
  else
  {
    return false;
  }
}
