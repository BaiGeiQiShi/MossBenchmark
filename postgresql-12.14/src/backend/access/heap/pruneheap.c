                                                                            
   
               
                                                     
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/htup_details.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/snapmgr.h"
#include "utils/rel.h"

                                                      
typedef struct
{
  TransactionId new_prune_xid;                                       
  TransactionId latestRemovedXid;                                             
  int nredirected;                                                        
  int ndead;
  int nunused;
                                                             
  OffsetNumber redirected[MaxHeapTuplesPerPage * 2];
  OffsetNumber nowdead[MaxHeapTuplesPerPage];
  OffsetNumber nowunused[MaxHeapTuplesPerPage];
                                                                         
  bool marked[MaxHeapTuplesPerPage + 1];
} PruneState;

                     
static int
heap_prune_chain(Relation relation, Buffer buffer, OffsetNumber rootoffnum, TransactionId OldestXmin, PruneState *prstate);
static void
heap_prune_record_prunable(PruneState *prstate, TransactionId xid);
static void
heap_prune_record_redirect(PruneState *prstate, OffsetNumber offnum, OffsetNumber rdoffnum);
static void
heap_prune_record_dead(PruneState *prstate, OffsetNumber offnum);
static void
heap_prune_record_unused(PruneState *prstate, OffsetNumber offnum);

   
                                                                    
   
                                                                    
                                                                            
                                                     
   
                                                                              
                                      
   
                                                                         
   
                                                                            
                                                    
   
void
heap_page_prune_opt(Relation relation, Buffer buffer)
{
  Page page = BufferGetPage(buffer);
  Size minfree;
  TransactionId OldestXmin;

     
                                                                        
                                                                             
                                            
     
  if (RecoveryInProgress())
  {
    return;
  }

     
                                                                          
                                                                          
                                                                          
                                                                       
                                                       
     
                                                                           
                                                                        
                                                                       
                                                                             
                                                                             
               
     
  if (IsCatalogRelation(relation) || RelationIsAccessibleInLogicalDecoding(relation))
  {
    OldestXmin = RecentGlobalXmin;
  }
  else
  {
    OldestXmin = TransactionIdLimitedForOldSnapshots(RecentGlobalDataXmin, relation);
  }

  Assert(TransactionIdIsValid(OldestXmin));

     
                                          
     
                                                                          
                            
     
  if (!PageIsPrunable(page, OldestXmin))
  {
    return;
  }

     
                                                                             
                                                                            
                                                 
     
                                                                          
                                                                             
                                                                             
                                                                        
                                                                          
                                
     
  minfree = RelationGetTargetPageFreeSpace(relation, HEAP_DEFAULT_FILLFACTOR);
  minfree = Max(minfree, BLCKSZ / 10);

  if (PageIsFull(page) || PageGetHeapFreeSpace(page) < minfree)
  {
                                              
    if (!ConditionalLockBufferForCleanup(buffer))
    {
      return;
    }

       
                                                                        
                                                                     
                                                                          
                                       
       
    if (PageIsFull(page) || PageGetHeapFreeSpace(page) < minfree)
    {
      TransactionId ignore = InvalidTransactionId;                     
                                                               

                       
      (void)heap_page_prune(relation, buffer, OldestXmin, true, &ignore);
    }

                                 
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
  }
}

   
                                                         
   
                                                             
   
                                                                            
                                                    
   
                                                                          
                                                                            
                                                                           
                    
   
                                                               
                     
   
int
heap_page_prune(Relation relation, Buffer buffer, TransactionId OldestXmin, bool report_stats, TransactionId *latestRemovedXid)
{
  int ndeleted = 0;
  Page page = BufferGetPage(buffer);
  OffsetNumber offnum, maxoff;
  PruneState prstate;

     
                                                                         
                                                                           
                                                                          
                                                       
     
                                                                         
                                                                    
                                                                           
                                               
     
  prstate.new_prune_xid = InvalidTransactionId;
  prstate.latestRemovedXid = *latestRemovedXid;
  prstate.nredirected = prstate.ndead = prstate.nunused = 0;
  memset(prstate.marked, 0, sizeof(prstate.marked));

                     
  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemid;

                                                                    
    if (prstate.marked[offnum])
    {
      continue;
    }

                                                        
    itemid = PageGetItemId(page, offnum);
    if (!ItemIdIsUsed(itemid) || ItemIdIsDead(itemid))
    {
      continue;
    }

                                             
    ndeleted += heap_prune_chain(relation, buffer, offnum, OldestXmin, &prstate);
  }

                                                        
  START_CRIT_SECTION();

                                         
  if (prstate.nredirected > 0 || prstate.ndead > 0 || prstate.nunused > 0)
  {
       
                                                                           
                                                                           
       
    heap_page_prune_execute(buffer, prstate.redirected, prstate.nredirected, prstate.nowdead, prstate.ndead, prstate.nowunused, prstate.nunused);

       
                                                                          
                                       
       
    ((PageHeader)page)->pd_prune_xid = prstate.new_prune_xid;

       
                                                                     
                                                                          
                 
       
    PageClearFull(page);

    MarkBufferDirty(buffer);

       
                                                        
       
    if (RelationNeedsWAL(relation))
    {
      XLogRecPtr recptr;

      recptr = log_heap_clean(relation, buffer, prstate.redirected, prstate.nredirected, prstate.nowdead, prstate.ndead, prstate.nowunused, prstate.nunused, prstate.latestRemovedXid);

      PageSetLSN(BufferGetPage(buffer), recptr);
    }
  }
  else
  {
       
                                                                       
                                                                        
                                         
       
                                                                         
                                                                        
                            
       
    if (((PageHeader)page)->pd_prune_xid != prstate.new_prune_xid || PageIsFull(page))
    {
      ((PageHeader)page)->pd_prune_xid = prstate.new_prune_xid;
      PageClearFull(page);
      MarkBufferDirtyHint(buffer, true);
    }
  }

  END_CRIT_SECTION();

     
                                                                             
                                                                          
                                          
     
  if (report_stats && ndeleted > prstate.ndead)
  {
    pgstat_update_heap_dead_tuples(relation, ndeleted - prstate.ndead);
  }

  *latestRemovedXid = prstate.latestRemovedXid;

     
                                                             
     
                                                                          
                                                                            
                                                                       
                                                                           
     
                                                                             
                                                                        
                                                                        
     
                                                                          
                                              
     

  return ndeleted;
}

   
                                                                            
   
                                                                          
                                                                               
                                                                          
                                                                             
                                                              
   
                                                                          
                                                                          
                                                                        
                                                  
   
                                                              
   
                                                                               
                                                                             
                                                                             
                                                                               
                                                                          
                                   
   
                                                               
   
static int
heap_prune_chain(Relation relation, Buffer buffer, OffsetNumber rootoffnum, TransactionId OldestXmin, PruneState *prstate)
{
  int ndeleted = 0;
  Page dp = (Page)BufferGetPage(buffer);
  TransactionId priorXmax = InvalidTransactionId;
  ItemId rootlp;
  HeapTupleHeader htup;
  OffsetNumber latestdead = InvalidOffsetNumber, maxoff = PageGetMaxOffsetNumber(dp), offnum;
  OffsetNumber chainitems[MaxHeapTuplesPerPage];
  int nchain = 0, i;
  HeapTupleData tup;

  tup.t_tableOid = RelationGetRelid(relation);

  rootlp = PageGetItemId(dp, rootoffnum);

     
                                                                         
     
  if (ItemIdIsNormal(rootlp))
  {
    htup = (HeapTupleHeader)PageGetItem(dp, rootlp);

    tup.t_data = htup;
    tup.t_len = ItemIdGetLength(rootlp);
    ItemPointerSet(&(tup.t_self), BufferGetBlockNumber(buffer), rootoffnum);

    if (HeapTupleHeaderIsHeapOnly(htup))
    {
         
                                                                       
                                                                       
                                           
         
                                                                        
                                                                         
                                                                      
                                                                 
                                                    
                                                                   
                                                                         
                                                             
         
                                                                   
                                                                       
                                                      
         
      if (HeapTupleSatisfiesVacuum(&tup, OldestXmin, buffer) == HEAPTUPLE_DEAD && !HeapTupleHeaderIsHotUpdated(htup))
      {
        heap_prune_record_unused(prstate, rootoffnum);
        HeapTupleHeaderAdvanceLatestRemovedXid(htup, &prstate->latestRemovedXid);
        ndeleted++;
      }

                              
      return ndeleted;
    }
  }

                                 
  offnum = rootoffnum;

                                  
  for (;;)
  {
    ItemId lp;
    bool tupdead, recent_dead;

                            
    if (offnum < FirstOffsetNumber || offnum > maxoff)
    {
      break;
    }

                                                                          
    if (prstate->marked[offnum])
    {
      break;
    }

    lp = PageGetItemId(dp, offnum);

                                                       
    if (!ItemIdIsUsed(lp))
    {
      break;
    }

       
                                                                          
                                                                         
                                                 
       
    if (ItemIdIsRedirected(lp))
    {
      if (nchain > 0)
      {
        break;                            
      }
      chainitems[nchain++] = offnum;
      offnum = ItemIdGetRedirect(rootlp);
      continue;
    }

       
                                                                     
                                                                   
                  
       
    if (ItemIdIsDead(lp))
    {
      break;
    }

    Assert(ItemIdIsNormal(lp));
    htup = (HeapTupleHeader)PageGetItem(dp, lp);

    tup.t_data = htup;
    tup.t_len = ItemIdGetLength(lp);
    ItemPointerSet(&(tup.t_self), BufferGetBlockNumber(buffer), offnum);

       
                                                       
       
    if (TransactionIdIsValid(priorXmax) && !TransactionIdEquals(HeapTupleHeaderGetXmin(htup), priorXmax))
    {
      break;
    }

       
                                                       
       
    chainitems[nchain++] = offnum;

       
                                        
       
    tupdead = recent_dead = false;

    switch (HeapTupleSatisfiesVacuum(&tup, OldestXmin, buffer))
    {
    case HEAPTUPLE_DEAD:
      tupdead = true;
      break;

    case HEAPTUPLE_RECENTLY_DEAD:
      recent_dead = true;

         
                                                                    
                                                              
         
      heap_prune_record_prunable(prstate, HeapTupleHeaderGetUpdateXid(htup));
      break;

    case HEAPTUPLE_DELETE_IN_PROGRESS:

         
                                                                    
                                                              
         
      heap_prune_record_prunable(prstate, HeapTupleHeaderGetUpdateXid(htup));
      break;

    case HEAPTUPLE_LIVE:
    case HEAPTUPLE_INSERT_IN_PROGRESS:

         
                                                                
                                                                   
                                                                     
                                    
         
      break;

    default:
      elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
      break;
    }

       
                                                                
                                                                        
                                                                         
                                                                        
                                                                 
       
    if (tupdead)
    {
      latestdead = offnum;
      HeapTupleHeaderAdvanceLatestRemovedXid(htup, &prstate->latestRemovedXid);
    }
    else if (!recent_dead)
    {
      break;
    }

       
                                                                       
                         
       
    if (!HeapTupleHeaderIsHotUpdated(htup))
    {
      break;
    }

                                                                
    Assert(!HeapTupleHeaderIndicatesMovedPartitions(htup));

       
                                     
       
    Assert(ItemPointerGetBlockNumber(&htup->t_ctid) == BufferGetBlockNumber(buffer));
    offnum = ItemPointerGetOffsetNumber(&htup->t_ctid);
    priorXmax = HeapTupleHeaderGetUpdateXid(htup);
  }

     
                                                                             
                                                                             
                                          
     
  if (OffsetNumberIsValid(latestdead))
  {
       
                                                                        
                       
       
                                                                         
                                        
       
    for (i = 1; (i < nchain) && (chainitems[i - 1] != latestdead); i++)
    {
      heap_prune_record_unused(prstate, chainitems[i]);
      ndeleted++;
    }

       
                                                                         
                                                                      
                             
       
    if (ItemIdIsNormal(rootlp))
    {
      ndeleted++;
    }

       
                                                                         
                                                                          
                                                      
       
    if (i >= nchain)
    {
      heap_prune_record_dead(prstate, rootoffnum);
    }
    else
    {
      heap_prune_record_redirect(prstate, rootoffnum, chainitems[i]);
    }
  }
  else if (nchain < 2 && ItemIdIsRedirected(rootlp))
  {
       
                                                                        
                                                                          
                                                                       
                                                                       
                   
       
    heap_prune_record_dead(prstate, rootoffnum);
  }

  return ndeleted;
}

                                     
static void
heap_prune_record_prunable(PruneState *prstate, TransactionId xid)
{
     
                                                                          
                                                                    
     
  Assert(TransactionIdIsNormal(xid));
  if (!TransactionIdIsValid(prstate->new_prune_xid) || TransactionIdPrecedes(xid, prstate->new_prune_xid))
  {
    prstate->new_prune_xid = xid;
  }
}

                                          
static void
heap_prune_record_redirect(PruneState *prstate, OffsetNumber offnum, OffsetNumber rdoffnum)
{
  Assert(prstate->nredirected < MaxHeapTuplesPerPage);
  prstate->redirected[prstate->nredirected * 2] = offnum;
  prstate->redirected[prstate->nredirected * 2 + 1] = rdoffnum;
  prstate->nredirected++;
  Assert(!prstate->marked[offnum]);
  prstate->marked[offnum] = true;
  Assert(!prstate->marked[rdoffnum]);
  prstate->marked[rdoffnum] = true;
}

                                           
static void
heap_prune_record_dead(PruneState *prstate, OffsetNumber offnum)
{
  Assert(prstate->ndead < MaxHeapTuplesPerPage);
  prstate->nowdead[prstate->ndead] = offnum;
  prstate->ndead++;
  Assert(!prstate->marked[offnum]);
  prstate->marked[offnum] = true;
}

                                             
static void
heap_prune_record_unused(PruneState *prstate, OffsetNumber offnum)
{
  Assert(prstate->nunused < MaxHeapTuplesPerPage);
  prstate->nowunused[prstate->nunused] = offnum;
  prstate->nunused++;
  Assert(!prstate->marked[offnum]);
  prstate->marked[offnum] = true;
}

   
                                                              
                                                                   
                                             
   
                                                                  
                                                                      
                                                         
   
void
heap_page_prune_execute(Buffer buffer, OffsetNumber *redirected, int nredirected, OffsetNumber *nowdead, int ndead, OffsetNumber *nowunused, int nunused)
{
  Page page = (Page)BufferGetPage(buffer);
  OffsetNumber *offnum;
  int i;

                                           
  offnum = redirected;
  for (i = 0; i < nredirected; i++)
  {
    OffsetNumber fromoff = *offnum++;
    OffsetNumber tooff = *offnum++;
    ItemId fromlp = PageGetItemId(page, fromoff);

    ItemIdSetRedirect(fromlp, tooff);
  }

                                         
  offnum = nowdead;
  for (i = 0; i < ndead; i++)
  {
    OffsetNumber off = *offnum++;
    ItemId lp = PageGetItemId(page, off);

    ItemIdSetDead(lp);
  }

                                           
  offnum = nowunused;
  for (i = 0; i < nunused; i++)
  {
    OffsetNumber off = *offnum++;
    ItemId lp = PageGetItemId(page, off);

    ItemIdSetUnused(lp);
  }

     
                                                                             
                                   
     
  PageRepairFragmentation(page);
}

   
                                                                         
                                                                     
                            
   
                                                                            
                                                              
   
                                                                          
                                        
   
                                                                            
                                                                            
                                               
   
void
heap_get_root_tuples(Page page, OffsetNumber *root_offsets)
{
  OffsetNumber offnum, maxoff;

  MemSet(root_offsets, InvalidOffsetNumber, MaxHeapTuplesPerPage * sizeof(OffsetNumber));

  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId lp = PageGetItemId(page, offnum);
    HeapTupleHeader htup;
    OffsetNumber nextoffnum;
    TransactionId priorXmax;

                                    
    if (!ItemIdIsUsed(lp) || ItemIdIsDead(lp))
    {
      continue;
    }

    if (ItemIdIsNormal(lp))
    {
      htup = (HeapTupleHeader)PageGetItem(page, lp);

         
                                                                         
                                                                      
                   
         
      if (HeapTupleHeaderIsHeapOnly(htup))
      {
        continue;
      }

         
                                                                  
                                     
         
      root_offsets[offnum - 1] = offnum;

                                                                    
      if (!HeapTupleHeaderIsHotUpdated(htup))
      {
        continue;
      }

                                        
      nextoffnum = ItemPointerGetOffsetNumber(&htup->t_ctid);
      priorXmax = HeapTupleHeaderGetUpdateXid(htup);
    }
    else
    {
                                                                         
      Assert(ItemIdIsRedirected(lp));
                                        
      nextoffnum = ItemIdGetRedirect(lp);
      priorXmax = InvalidTransactionId;
    }

       
                                                                       
       
                                                                      
                                                                          
                                                                     
               
       
    for (;;)
    {
      lp = PageGetItemId(page, nextoffnum);

                                   
      if (!ItemIdIsNormal(lp))
      {
        break;
      }

      htup = (HeapTupleHeader)PageGetItem(page, lp);

      if (TransactionIdIsValid(priorXmax) && !TransactionIdEquals(priorXmax, HeapTupleHeaderGetXmin(htup)))
      {
        break;
      }

                                                        
      root_offsets[nextoffnum - 1] = offnum;

                                                
      if (!HeapTupleHeaderIsHotUpdated(htup))
      {
        break;
      }

                                                                  
      Assert(!HeapTupleHeaderIndicatesMovedPartitions(htup));

      nextoffnum = ItemPointerGetOffsetNumber(&htup->t_ctid);
      priorXmax = HeapTupleHeaderGetUpdateXid(htup);
    }
  }
}
