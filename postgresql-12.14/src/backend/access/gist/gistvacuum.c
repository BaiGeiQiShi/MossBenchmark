                                                                            
   
                
                                                                   
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/gist_private.h"
#include "access/transam.h"
#include "commands/vacuum.h"
#include "lib/integerset.h"
#include "miscadmin.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "utils/memutils.h"

   
                                    
   
typedef struct
{
  IndexBulkDeleteResult stats;                    

     
                                                                             
                                                                            
            
     
  IntegerSet *internal_page_set;
  IntegerSet *empty_leaf_set;
  MemoryContext page_set_context;
} GistBulkDeleteResult;

                                            
typedef struct
{
  IndexVacuumInfo *info;
  GistBulkDeleteResult *stats;
  IndexBulkDeleteCallback callback;
  void *callback_state;
  GistNSN startNSN;
} GistVacState;

static void
gistvacuumscan(IndexVacuumInfo *info, GistBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state);
static void
gistvacuumpage(GistVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno);
static void
gistvacuum_delete_empty_pages(IndexVacuumInfo *info, GistBulkDeleteResult *stats);
static bool
gistdeletepage(IndexVacuumInfo *info, GistBulkDeleteResult *stats, Buffer buffer, OffsetNumber downlink, Buffer leafBuffer);

                                                                
static GistBulkDeleteResult *
create_GistBulkDeleteResult(void)
{
  GistBulkDeleteResult *gist_stats;

  gist_stats = (GistBulkDeleteResult *)palloc0(sizeof(GistBulkDeleteResult));
  gist_stats->page_set_context = GenerationContextCreate(CurrentMemoryContext, "GiST VACUUM page set context", 16 * 1024);

  return gist_stats;
}

   
                                                  
   
IndexBulkDeleteResult *
gistbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  GistBulkDeleteResult *gist_stats = (GistBulkDeleteResult *)stats;

                                                                         
  if (gist_stats == NULL)
  {
    gist_stats = create_GistBulkDeleteResult();
  }

  gistvacuumscan(info, gist_stats, callback, callback_state);

  return (IndexBulkDeleteResult *)gist_stats;
}

   
                                                                          
   
IndexBulkDeleteResult *
gistvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
  GistBulkDeleteResult *gist_stats = (GistBulkDeleteResult *)stats;

                                  
  if (info->analyze_only)
  {
    return stats;
  }

     
                                                                            
                                                                         
                                                                         
     
  if (gist_stats == NULL)
  {
    gist_stats = create_GistBulkDeleteResult();
    gistvacuumscan(info, gist_stats, NULL, NULL);
  }

     
                                                                         
                         
     
  gistvacuum_delete_empty_pages(info, gist_stats);

                                                              
  MemoryContextDelete(gist_stats->page_set_context);
  gist_stats->page_set_context = NULL;
  gist_stats->internal_page_set = NULL;
  gist_stats->empty_leaf_set = NULL;

     
                                                                            
                                                                             
                                                                            
                                         
     
  if (!info->estimated_count)
  {
    if (gist_stats->stats.num_index_tuples > info->num_heap_tuples)
    {
      gist_stats->stats.num_index_tuples = info->num_heap_tuples;
    }
  }

  return (IndexBulkDeleteResult *)gist_stats;
}

   
                                                            
   
                                                                            
                                                                  
                                                                        
              
   
                                                                         
                                                                         
                                                                             
                                                                                
                                                                     
   
                                                                              
   
static void
gistvacuumscan(IndexVacuumInfo *info, GistBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  Relation rel = info->index;
  GistVacState vstate;
  BlockNumber num_pages;
  bool needLock;
  BlockNumber blkno;
  MemoryContext oldctx;

     
                                                                           
                                                       
     
  stats->stats.estimated_count = false;
  stats->stats.num_index_tuples = 0;
  stats->stats.pages_deleted = 0;
  stats->stats.pages_free = 0;
  MemoryContextReset(stats->page_set_context);

     
                                                                             
                                                                           
                                                                            
                                         
     
  oldctx = MemoryContextSwitchTo(stats->page_set_context);
  stats->internal_page_set = intset_create();
  stats->empty_leaf_set = intset_create();
  MemoryContextSwitchTo(oldctx);

                                                  
  vstate.info = info;
  vstate.stats = stats;
  vstate.callback = callback;
  vstate.callback_state = callback_state;
  if (RelationNeedsWAL(rel))
  {
    vstate.startNSN = GetInsertRecPtr();
  }
  else
  {
    vstate.startNSN = gistGetFakeLSN(rel);
  }

     
                                                                         
                                                                            
                                                                             
                                                                         
                                                                           
                                                                           
                                                                        
                                                                         
                                                                        
                                                                        
                                                                             
                                                                          
                                                                             
                                                                            
                                                                           
                                                                             
                                                                           
           
     
                                                                          
                                   
     
  needLock = !RELATION_IS_LOCAL(rel);

  blkno = GIST_ROOT_BLKNO;
  for (;;)
  {
                                         
    if (needLock)
    {
      LockRelationForExtension(rel, ExclusiveLock);
    }
    num_pages = RelationGetNumberOfBlocks(rel);
    if (needLock)
    {
      UnlockRelationForExtension(rel, ExclusiveLock);
    }

                                                  
    if (blkno >= num_pages)
    {
      break;
    }
                                                              
    for (; blkno < num_pages; blkno++)
    {
      gistvacuumpage(&vstate, blkno, blkno);
    }
  }

     
                                                                           
                                                                            
                                                                     
                                                                          
                                                                       
                                                                      
                 
     
                                                                           
                 
     
  if (stats->stats.pages_free > 0)
  {
    IndexFreeSpaceMapVacuum(rel);
  }

                         
  stats->stats.num_pages = num_pages;
}

   
                                      
   
                                                                        
                                                                      
                                                
   
                                                                         
                                                                          
                                                 
   
static void
gistvacuumpage(GistVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno)
{
  GistBulkDeleteResult *stats = vstate->stats;
  IndexVacuumInfo *info = vstate->info;
  IndexBulkDeleteCallback callback = vstate->callback;
  void *callback_state = vstate->callback_state;
  Relation rel = info->index;
  Buffer buffer;
  Page page;
  BlockNumber recurse_to;

restart:
  recurse_to = InvalidBlockNumber;

                                                                 
  vacuum_delay_point();

  buffer = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);

     
                                                                         
                     
     
  LockBuffer(buffer, GIST_EXCLUSIVE);
  page = (Page)BufferGetPage(buffer);

  if (gistPageRecyclable(page))
  {
                                   
    RecordFreeIndexPage(rel, blkno);
    stats->stats.pages_free++;
    stats->stats.pages_deleted++;
  }
  else if (GistPageIsDeleted(page))
  {
                                                
    stats->stats.pages_deleted++;
  }
  else if (GistPageIsLeaf(page))
  {
    OffsetNumber todelete[MaxOffsetNumber];
    int ntodelete = 0;
    int nremain;
    GISTPageOpaque opaque = GistPageGetOpaque(page);
    OffsetNumber maxoff = PageGetMaxOffsetNumber(page);

       
                                                                        
                                                                          
                                                                        
                                                                         
       
                                                                           
                                                                       
                                                   
       
    if ((GistFollowRight(page) || vstate->startNSN < GistPageGetNSN(page)) && (opaque->rightlink != InvalidBlockNumber) && (opaque->rightlink < orig_blkno))
    {
      recurse_to = opaque->rightlink;
    }

       
                                                                          
                                 
       
    if (callback)
    {
      OffsetNumber off;

      for (off = FirstOffsetNumber; off <= maxoff; off = OffsetNumberNext(off))
      {
        ItemId iid = PageGetItemId(page, off);
        IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);

        if (callback(&(idxtuple->t_tid), callback_state))
        {
          todelete[ntodelete++] = off;
        }
      }
    }

       
                                                                         
                                      
       
    if (ntodelete > 0)
    {
      START_CRIT_SECTION();

      MarkBufferDirty(buffer);

      PageIndexMultiDelete(page, todelete, ntodelete);
      GistMarkTuplesDeleted(page);

      if (RelationNeedsWAL(rel))
      {
        XLogRecPtr recptr;

        recptr = gistXLogUpdate(buffer, todelete, ntodelete, NULL, 0, InvalidBuffer);
        PageSetLSN(page, recptr);
      }
      else
      {
        PageSetLSN(page, gistGetFakeLSN(rel));
      }

      END_CRIT_SECTION();

      stats->stats.tuples_removed += ntodelete;
                                 
      maxoff = PageGetMaxOffsetNumber(page);
    }

    nremain = maxoff - FirstOffsetNumber + 1;
    if (nremain == 0)
    {
         
                                                                       
                                                                     
         
                                                                        
                                                                         
                
         
      if (blkno == orig_blkno)
      {
        intset_add_member(stats->empty_leaf_set, blkno);
      }
    }
    else
    {
      stats->stats.num_index_tuples += nremain;
    }
  }
  else
  {
       
                                                                          
                                                                        
                                                                         
                                                                    
                                                                     
       
    OffsetNumber maxoff = PageGetMaxOffsetNumber(page);
    OffsetNumber off;

    for (off = FirstOffsetNumber; off <= maxoff; off = OffsetNumberNext(off))
    {
      ItemId iid = PageGetItemId(page, off);
      IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);

      if (GistTupleIsInvalid(idxtuple))
      {
        ereport(LOG, (errmsg("index \"%s\" contains an inner tuple marked as invalid", RelationGetRelationName(rel)), errdetail("This is caused by an incomplete page split at crash recovery before upgrading to PostgreSQL 9.1."), errhint("Please REINDEX it.")));
      }
    }

       
                                                                         
                                                                    
                                    
       
    if (blkno == orig_blkno)
    {
      intset_add_member(stats->internal_page_set, blkno);
    }
  }

  UnlockReleaseBuffer(buffer);

     
                                                                         
                                                                          
                                                                             
                                                                            
                                                    
     
  if (recurse_to != InvalidBlockNumber)
  {
    blkno = recurse_to;
    goto restart;
  }
}

   
                                                                       
   
static void
gistvacuum_delete_empty_pages(IndexVacuumInfo *info, GistBulkDeleteResult *stats)
{
  Relation rel = info->index;
  BlockNumber empty_pages_remaining;
  uint64 blkno;

     
                                                                       
     
  empty_pages_remaining = intset_num_entries(stats->empty_leaf_set);
  intset_begin_iterate(stats->internal_page_set);
  while (empty_pages_remaining > 0 && intset_iterate_next(stats->internal_page_set, &blkno))
  {
    Buffer buffer;
    Page page;
    OffsetNumber off, maxoff;
    OffsetNumber todelete[MaxOffsetNumber];
    BlockNumber leafs_to_delete[MaxOffsetNumber];
    int ntodelete;
    int deleted;

    buffer = ReadBufferExtended(rel, MAIN_FORKNUM, (BlockNumber)blkno, RBM_NORMAL, info->strategy);

    LockBuffer(buffer, GIST_SHARE);
    page = (Page)BufferGetPage(buffer);

    if (PageIsNew(page) || GistPageIsDeleted(page) || GistPageIsLeaf(page))
    {
         
                                                                        
                                   
         
      Assert(false);
      UnlockReleaseBuffer(buffer);
      continue;
    }

       
                                                                          
              
       
    maxoff = PageGetMaxOffsetNumber(page);
    ntodelete = 0;
    for (off = FirstOffsetNumber; off <= maxoff && ntodelete < maxoff - 1; off = OffsetNumberNext(off))
    {
      ItemId iid = PageGetItemId(page, off);
      IndexTuple idxtuple = (IndexTuple)PageGetItem(page, iid);
      BlockNumber leafblk;

      leafblk = ItemPointerGetBlockNumber(&(idxtuple->t_tid));
      if (intset_is_member(stats->empty_leaf_set, leafblk))
      {
        leafs_to_delete[ntodelete] = leafblk;
        todelete[ntodelete++] = off;
      }
    }

       
                                                                    
                                                                          
                                                                          
                                              
       
                                                                       
                                                                   
                                                                          
                                                                         
                                                                           
                                       
       
    LockBuffer(buffer, GIST_UNLOCK);

    deleted = 0;
    for (int i = 0; i < ntodelete; i++)
    {
      Buffer leafbuf;

         
                                                                     
                                     
         
      if (PageGetMaxOffsetNumber(page) == FirstOffsetNumber)
      {
        break;
      }

      leafbuf = ReadBufferExtended(rel, MAIN_FORKNUM, leafs_to_delete[i], RBM_NORMAL, info->strategy);
      LockBuffer(leafbuf, GIST_EXCLUSIVE);
      gistcheckpage(rel, leafbuf);

      LockBuffer(buffer, GIST_EXCLUSIVE);
      if (gistdeletepage(info, stats, buffer, todelete[i] - deleted, leafbuf))
      {
        deleted++;
      }
      LockBuffer(buffer, GIST_UNLOCK);

      UnlockReleaseBuffer(leafbuf);
    }

    ReleaseBuffer(buffer);

                      
    stats->stats.pages_removed += deleted;

       
                                                                           
                                            
       
    empty_pages_remaining -= ntodelete;
  }
}

   
                                                                             
                                     
   
                                                                                
                                                                              
                                                                             
                              
   
                                                                           
                 
   
static bool
gistdeletepage(IndexVacuumInfo *info, GistBulkDeleteResult *stats, Buffer parentBuffer, OffsetNumber downlink, Buffer leafBuffer)
{
  Page parentPage = BufferGetPage(parentBuffer);
  Page leafPage = BufferGetPage(leafBuffer);
  ItemId iid;
  IndexTuple idxtuple;
  XLogRecPtr recptr;
  FullTransactionId txid;

     
                                                       
     
  if (!GistPageIsLeaf(leafPage))
  {
                                                         
    Assert(false);
    return false;
  }

  if (GistFollowRight(leafPage))
  {
    return false;                                              
  }

  if (PageGetMaxOffsetNumber(leafPage) != InvalidOffsetNumber)
  {
    return false;                        
  }

     
                                                                          
                                                                            
                                                                            
                                                                          
                                  
     
  if (PageIsNew(parentPage) || GistPageIsDeleted(parentPage) || GistPageIsLeaf(parentPage))
  {
                                                            
    Assert(false);
    return false;
  }

  if (PageGetMaxOffsetNumber(parentPage) < downlink || PageGetMaxOffsetNumber(parentPage) <= FirstOffsetNumber)
  {
    return false;
  }

  iid = PageGetItemId(parentPage, downlink);
  idxtuple = (IndexTuple)PageGetItem(parentPage, iid);
  if (BufferGetBlockNumber(leafBuffer) != ItemPointerGetBlockNumber(&(idxtuple->t_tid)))
  {
    return false;
  }

     
                                          
     
                                                                             
                                                                            
                                                                           
                                                                        
                                                                            
                                                      
     
  txid = ReadNextFullTransactionId();

  START_CRIT_SECTION();

                                
  MarkBufferDirty(leafBuffer);
  GistPageSetDeleted(leafPage, txid);
  stats->stats.pages_deleted++;

                                           
  MarkBufferDirty(parentBuffer);
  PageIndexTupleDelete(parentPage, downlink);

  if (RelationNeedsWAL(info->index))
  {
    recptr = gistXLogPageDelete(leafBuffer, txid, parentBuffer, downlink);
  }
  else
  {
    recptr = gistGetFakeLSN(info->index);
  }
  PageSetLSN(parentPage, recptr);
  PageSetLSN(leafPage, recptr);

  END_CRIT_SECTION();

  return true;
}
