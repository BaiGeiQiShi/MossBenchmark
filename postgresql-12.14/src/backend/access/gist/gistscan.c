                                                                            
   
              
                                                      
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/gist_private.h"
#include "access/gistscan.h"
#include "access/relscan.h"
#include "utils/float.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                 
   
static int
pairingheap_GISTSearchItem_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{
  const GISTSearchItem *sa = (const GISTSearchItem *)a;
  const GISTSearchItem *sb = (const GISTSearchItem *)b;
  IndexScanDesc scan = (IndexScanDesc)arg;
  int i;

                                              
  for (i = 0; i < scan->numberOfOrderBys; i++)
  {
    if (sa->distances[i].isnull)
    {
      if (!sb->distances[i].isnull)
      {
        return -1;
      }
    }
    else if (sb->distances[i].isnull)
    {
      return 1;
    }
    else
    {
      int cmp = -float8_cmp_internal(sa->distances[i].value, sb->distances[i].value);

      if (cmp != 0)
      {
        return cmp;
      }
    }
  }

                                                                        
  if (GISTSearchItemIsHeap(*sa) && !GISTSearchItemIsHeap(*sb))
  {
    return 1;
  }
  if (!GISTSearchItemIsHeap(*sa) && GISTSearchItemIsHeap(*sb))
  {
    return -1;
  }

  return 0;
}

   
                                                    
   

IndexScanDesc
gistbeginscan(Relation r, int nkeys, int norderbys)
{
  IndexScanDesc scan;
  GISTSTATE *giststate;
  GISTScanOpaque so;
  MemoryContext oldCxt;

  scan = RelationGetIndexScan(r, nkeys, norderbys);

                                                                     
  giststate = initGISTstate(scan->indexRelation);

     
                                                                            
                                                        
     
  oldCxt = MemoryContextSwitchTo(giststate->scanCxt);

                              
  so = (GISTScanOpaque)palloc0(sizeof(GISTScanOpaqueData));
  so->giststate = giststate;
  giststate->tempCxt = createTempGistContext();
  so->queue = NULL;
  so->queueCxt = giststate->scanCxt;                     

                                                           
  so->distances = palloc(sizeof(so->distances[0]) * scan->numberOfOrderBys);
  so->qual_ok = true;                                  
  if (scan->numberOfOrderBys > 0)
  {
    scan->xs_orderbyvals = palloc0(sizeof(Datum) * scan->numberOfOrderBys);
    scan->xs_orderbynulls = palloc(sizeof(bool) * scan->numberOfOrderBys);
    memset(scan->xs_orderbynulls, true, sizeof(bool) * scan->numberOfOrderBys);
  }

  so->killedItems = NULL;                   
  so->numKilled = 0;
  so->curBlkno = InvalidBlockNumber;
  so->curPageLSN = InvalidXLogRecPtr;

  scan->opaque = so;

     
                                                                             
                                                                    
     

  MemoryContextSwitchTo(oldCxt);

  return scan;
}

void
gistrescan(IndexScanDesc scan, ScanKey key, int nkeys, ScanKey orderbys, int norderbys)
{
                                                 
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;
  bool first_time;
  int i;
  MemoryContext oldCxt;

                                                    

     
                                                                        
                                                                           
                                                                             
                                                                         
                                                                           
                                                                            
                                                                            
                         
     
  if (so->queue == NULL)
  {
                            
    Assert(so->queueCxt == so->giststate->scanCxt);
    first_time = true;
  }
  else if (so->queueCxt == so->giststate->scanCxt)
  {
                             
    so->queueCxt = AllocSetContextCreate(so->giststate->scanCxt, "GiST queue context", ALLOCSET_DEFAULT_SIZES);
    first_time = false;
  }
  else
  {
                                     
    MemoryContextReset(so->queueCxt);
    first_time = false;
  }

     
                                                                             
                                                                          
                                                  
     
  if (scan->xs_want_itup && !scan->xs_hitupdesc)
  {
    int natts;
    int nkeyatts;
    int attno;

       
                                                                        
                                                                        
                                                                          
              
       
    natts = RelationGetNumberOfAttributes(scan->indexRelation);
    nkeyatts = IndexRelationGetNumberOfKeyAttributes(scan->indexRelation);
    so->giststate->fetchTupdesc = CreateTemplateTupleDesc(natts);
    for (attno = 1; attno <= nkeyatts; attno++)
    {
      TupleDescInitEntry(so->giststate->fetchTupdesc, attno, NULL, scan->indexRelation->rd_opcintype[attno - 1], -1, 0);
    }

    for (; attno <= natts; attno++)
    {
                                                    
      TupleDescInitEntry(so->giststate->fetchTupdesc, attno, NULL, TupleDescAttr(so->giststate->leafTupdesc, attno - 1)->atttypid, -1, 0);
    }
    scan->xs_hitupdesc = so->giststate->fetchTupdesc;

                                                                         
    so->pageDataCxt = AllocSetContextCreate(so->giststate->scanCxt, "GiST page data context", ALLOCSET_DEFAULT_SIZES);
  }

                                                       
  oldCxt = MemoryContextSwitchTo(so->queueCxt);
  so->queue = pairingheap_allocate(pairingheap_GISTSearchItem_cmp, scan);
  MemoryContextSwitchTo(oldCxt);

  so->firstCall = true;

                                              
  if (key && scan->numberOfKeys > 0)
  {
    void **fn_extras = NULL;

       
                                                                   
                                                                      
                                                      
       
    if (!first_time)
    {
      fn_extras = (void **)palloc(scan->numberOfKeys * sizeof(void *));
      for (i = 0; i < scan->numberOfKeys; i++)
      {
        fn_extras[i] = scan->keyData[i].sk_func.fn_extra;
      }
    }

    memmove(scan->keyData, key, scan->numberOfKeys * sizeof(ScanKeyData));

       
                                                                           
                                                                      
                                                                       
                                                                       
              
       
                                                                      
                                                                        
                                                   
       
    so->qual_ok = true;

    for (i = 0; i < scan->numberOfKeys; i++)
    {
      ScanKey skey = scan->keyData + i;

         
                                                                       
                                                      
         
      fmgr_info_copy(&(skey->sk_func), &(so->giststate->consistentFn[skey->sk_attno - 1]), so->giststate->scanCxt);

                                                              
      if (!first_time)
      {
        skey->sk_func.fn_extra = fn_extras[i];
      }

      if (skey->sk_flags & SK_ISNULL)
      {
        if (!(skey->sk_flags & (SK_SEARCHNULL | SK_SEARCHNOTNULL)))
        {
          so->qual_ok = false;
        }
      }
    }

    if (!first_time)
    {
      pfree(fn_extras);
    }
  }

                                                  
  if (orderbys && scan->numberOfOrderBys > 0)
  {
    void **fn_extras = NULL;

                                                               
    if (!first_time)
    {
      fn_extras = (void **)palloc(scan->numberOfOrderBys * sizeof(void *));
      for (i = 0; i < scan->numberOfOrderBys; i++)
      {
        fn_extras[i] = scan->orderByData[i].sk_func.fn_extra;
      }
    }

    memmove(scan->orderByData, orderbys, scan->numberOfOrderBys * sizeof(ScanKeyData));

    so->orderByTypes = (Oid *)palloc(scan->numberOfOrderBys * sizeof(Oid));

       
                                                                         
                                                                        
                                                                       
                                                                       
              
       
    for (i = 0; i < scan->numberOfOrderBys; i++)
    {
      ScanKey skey = scan->orderByData + i;
      FmgrInfo *finfo = &(so->giststate->distanceFn[skey->sk_attno - 1]);

                                                          
      if (!OidIsValid(finfo->fn_oid))
      {
        elog(ERROR, "missing support function %d for attribute %d of index \"%s\"", GIST_DISTANCE_PROC, skey->sk_attno, RelationGetRelationName(scan->indexRelation));
      }

         
                                                                
                                                                        
                                                           
         
                                                                       
                                                                         
                                                                        
                                                                     
                                                                    
                     
         
      so->orderByTypes[i] = get_func_rettype(skey->sk_func.fn_oid);

         
                                                                        
                                                  
         
      fmgr_info_copy(&(skey->sk_func), finfo, so->giststate->scanCxt);

                                                              
      if (!first_time)
      {
        skey->sk_func.fn_extra = fn_extras[i];
      }
    }

    if (!first_time)
    {
      pfree(fn_extras);
    }
  }

                                                                            
  scan->xs_hitup = NULL;
}

void
gistendscan(IndexScanDesc scan)
{
  GISTScanOpaque so = (GISTScanOpaque)scan->opaque;

     
                                                                           
                                                                    
     
  freeGISTstate(so->giststate);
}
