                                                                            
   
               
                        
   
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/spgist_private.h"
#include "access/spgxlog.h"
#include "access/transam.h"
#include "access/xloginsert.h"
#include "catalog/storage_xlog.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "utils/snapmgr.h"

                                                      
typedef struct spgVacPendingItem
{
  ItemPointerData tid;                                             
  bool done;                                                    
  struct spgVacPendingItem *next;                
} spgVacPendingItem;

                                       
typedef struct spgBulkDeleteState
{
                                             
  IndexVacuumInfo *info;
  IndexBulkDeleteResult *stats;
  IndexBulkDeleteCallback callback;
  void *callback_state;

                                
  SpGistState spgstate;                                                    
  spgVacPendingItem *pendingList;                                
  TransactionId myXmin;                                                    
  BlockNumber lastFilledBlock;                                  
} spgBulkDeleteState;

   
                                                            
   
                                                                        
                                                                          
   
static void
spgAddPendingTID(spgBulkDeleteState *bds, ItemPointer tid)
{
  spgVacPendingItem *pitem;
  spgVacPendingItem **listLink;

                                              
  listLink = &bds->pendingList;
  while (*listLink != NULL)
  {
    pitem = *listLink;
    if (ItemPointerEquals(tid, &pitem->tid))
    {
      return;                                  
    }
    listLink = &pitem->next;
  }
                                      
  pitem = (spgVacPendingItem *)palloc(sizeof(spgVacPendingItem));
  pitem->tid = *tid;
  pitem->done = false;
  pitem->next = NULL;
  *listLink = pitem;
}

   
                     
   
static void
spgClearPendingList(spgBulkDeleteState *bds)
{
  spgVacPendingItem *pitem;
  spgVacPendingItem *nitem;

  for (pitem = bds->pendingList; pitem != NULL; pitem = nitem)
  {
    nitem = pitem->next;
                                                       
    Assert(pitem->done);
    pfree(pitem);
  }
  bds->pendingList = NULL;
}

   
                                         
   
                                                                       
                                                                           
                                                
   
                                                                              
                                                                          
                                                                            
                                                                          
                                                                              
                                                          
   
                                                                        
                                                                       
                                                                       
                                                                          
                                                               
   
static void
vacuumLeafPage(spgBulkDeleteState *bds, Relation index, Buffer buffer, bool forPending)
{
  Page page = BufferGetPage(buffer);
  spgxlogVacuumLeaf xlrec;
  OffsetNumber toDead[MaxIndexTuplesPerPage];
  OffsetNumber toPlaceholder[MaxIndexTuplesPerPage];
  OffsetNumber moveSrc[MaxIndexTuplesPerPage];
  OffsetNumber moveDest[MaxIndexTuplesPerPage];
  OffsetNumber chainSrc[MaxIndexTuplesPerPage];
  OffsetNumber chainDest[MaxIndexTuplesPerPage];
  OffsetNumber predecessor[MaxIndexTuplesPerPage + 1];
  bool deletable[MaxIndexTuplesPerPage + 1];
  int nDeletable;
  OffsetNumber i, max = PageGetMaxOffsetNumber(page);

  memset(predecessor, 0, sizeof(predecessor));
  memset(deletable, 0, sizeof(deletable));
  nDeletable = 0;

                                                              
  for (i = FirstOffsetNumber; i <= max; i++)
  {
    SpGistLeafTuple lt;

    lt = (SpGistLeafTuple)PageGetItem(page, PageGetItemId(page, i));
    if (lt->tupstate == SPGIST_LIVE)
    {
      Assert(ItemPointerIsValid(&lt->heapPtr));

      if (bds->callback(&lt->heapPtr, bds->callback_state))
      {
        bds->stats->tuples_removed += 1;
        deletable[i] = true;
        nDeletable++;
      }
      else
      {
        if (!forPending)
        {
          bds->stats->num_index_tuples += 1;
        }
      }

                                     
      if (lt->nextOffset != InvalidOffsetNumber)
      {
                                                  
        if (lt->nextOffset < FirstOffsetNumber || lt->nextOffset > max || predecessor[lt->nextOffset] != InvalidOffsetNumber)
        {
          elog(ERROR, "inconsistent tuple chain links in page %u of index \"%s\"", BufferGetBlockNumber(buffer), RelationGetRelationName(index));
        }
        predecessor[lt->nextOffset] = i;
      }
    }
    else if (lt->tupstate == SPGIST_REDIRECT)
    {
      SpGistDeadTuple dt = (SpGistDeadTuple)lt;

      Assert(dt->nextOffset == InvalidOffsetNumber);
      Assert(ItemPointerIsValid(&dt->pointer));

         
                                                                      
                                        
         
                                                                    
                                                                   
                                                                        
                                                               
         
      if (TransactionIdFollowsOrEquals(dt->xid, bds->myXmin))
      {
        spgAddPendingTID(bds, &dt->pointer);
      }
    }
    else
    {
      Assert(lt->nextOffset == InvalidOffsetNumber);
    }
  }

  if (nDeletable == 0)
  {
    return;                         
  }

               
                                                                        
                                                                          
                                                                         
                                                                     
                                                                         
     
                                                             
                                                                           
                                                                          
                                                                           
                                                  
                                                                           
                                                                
     
                                                                       
                                                                       
                                                                         
                                                              
               
     
  xlrec.nDead = xlrec.nPlaceholder = xlrec.nMove = xlrec.nChain = 0;

  for (i = FirstOffsetNumber; i <= max; i++)
  {
    SpGistLeafTuple head;
    bool interveningDeletable;
    OffsetNumber prevLive;
    OffsetNumber j;

    head = (SpGistLeafTuple)PageGetItem(page, PageGetItemId(page, i));
    if (head->tupstate != SPGIST_LIVE)
    {
      continue;                              
    }
    if (predecessor[i] != 0)
    {
      continue;                       
    }

                        
    interveningDeletable = false;
    prevLive = deletable[i] ? InvalidOffsetNumber : i;

                                 
    j = head->nextOffset;
    while (j != InvalidOffsetNumber)
    {
      SpGistLeafTuple lt;

      lt = (SpGistLeafTuple)PageGetItem(page, PageGetItemId(page, j));
      if (lt->tupstate != SPGIST_LIVE)
      {
                                                
        elog(ERROR, "unexpected SPGiST tuple state: %d", lt->tupstate);
      }

      if (deletable[j])
      {
                                                            
        toPlaceholder[xlrec.nPlaceholder] = j;
        xlrec.nPlaceholder++;
                                                                  
        interveningDeletable = true;
      }
      else if (prevLive == InvalidOffsetNumber)
      {
           
                                                                      
                                 
           
        moveSrc[xlrec.nMove] = j;
        moveDest[xlrec.nMove] = i;
        xlrec.nMove++;
                                                          
        prevLive = i;
        interveningDeletable = false;
      }
      else
      {
           
                                                                      
                                                  
           
        if (interveningDeletable)
        {
          chainSrc[xlrec.nChain] = prevLive;
          chainDest[xlrec.nChain] = j;
          xlrec.nChain++;
        }
        prevLive = j;
        interveningDeletable = false;
      }

      j = lt->nextOffset;
    }

    if (prevLive == InvalidOffsetNumber)
    {
                                                                    
      toDead[xlrec.nDead] = i;
      xlrec.nDead++;
    }
    else if (interveningDeletable)
    {
                                                                  
      chainSrc[xlrec.nChain] = prevLive;
      chainDest[xlrec.nChain] = InvalidOffsetNumber;
      xlrec.nChain++;
    }
  }

                        
  if (nDeletable != xlrec.nDead + xlrec.nPlaceholder + xlrec.nMove)
  {
    elog(ERROR, "inconsistent counts of deletable tuples");
  }

                      
  START_CRIT_SECTION();

  spgPageIndexMultiDelete(&bds->spgstate, page, toDead, xlrec.nDead, SPGIST_DEAD, SPGIST_DEAD, InvalidBlockNumber, InvalidOffsetNumber);

  spgPageIndexMultiDelete(&bds->spgstate, page, toPlaceholder, xlrec.nPlaceholder, SPGIST_PLACEHOLDER, SPGIST_PLACEHOLDER, InvalidBlockNumber, InvalidOffsetNumber);

     
                                                                            
                                                                    
                                                                       
                                                                         
                                     
     
  for (i = 0; i < xlrec.nMove; i++)
  {
    ItemId idSrc = PageGetItemId(page, moveSrc[i]);
    ItemId idDest = PageGetItemId(page, moveDest[i]);
    ItemIdData tmp;

    tmp = *idSrc;
    *idSrc = *idDest;
    *idDest = tmp;
  }

  spgPageIndexMultiDelete(&bds->spgstate, page, moveSrc, xlrec.nMove, SPGIST_PLACEHOLDER, SPGIST_PLACEHOLDER, InvalidBlockNumber, InvalidOffsetNumber);

  for (i = 0; i < xlrec.nChain; i++)
  {
    SpGistLeafTuple lt;

    lt = (SpGistLeafTuple)PageGetItem(page, PageGetItemId(page, chainSrc[i]));
    Assert(lt->tupstate == SPGIST_LIVE);
    lt->nextOffset = chainDest[i];
  }

  MarkBufferDirty(buffer);

  if (RelationNeedsWAL(index))
  {
    XLogRecPtr recptr;

    XLogBeginInsert();

    STORE_STATE(&bds->spgstate, xlrec.stateSrc);

    XLogRegisterData((char *)&xlrec, SizeOfSpgxlogVacuumLeaf);
                                                                    
    XLogRegisterData((char *)toDead, sizeof(OffsetNumber) * xlrec.nDead);
    XLogRegisterData((char *)toPlaceholder, sizeof(OffsetNumber) * xlrec.nPlaceholder);
    XLogRegisterData((char *)moveSrc, sizeof(OffsetNumber) * xlrec.nMove);
    XLogRegisterData((char *)moveDest, sizeof(OffsetNumber) * xlrec.nMove);
    XLogRegisterData((char *)chainSrc, sizeof(OffsetNumber) * xlrec.nChain);
    XLogRegisterData((char *)chainDest, sizeof(OffsetNumber) * xlrec.nChain);

    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_VACUUM_LEAF);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();
}

   
                                             
   
                                                                       
   
static void
vacuumLeafRoot(spgBulkDeleteState *bds, Relation index, Buffer buffer)
{
  Page page = BufferGetPage(buffer);
  spgxlogVacuumRoot xlrec;
  OffsetNumber toDelete[MaxIndexTuplesPerPage];
  OffsetNumber i, max = PageGetMaxOffsetNumber(page);

  xlrec.nDelete = 0;

                                                              
  for (i = FirstOffsetNumber; i <= max; i++)
  {
    SpGistLeafTuple lt;

    lt = (SpGistLeafTuple)PageGetItem(page, PageGetItemId(page, i));
    if (lt->tupstate == SPGIST_LIVE)
    {
      Assert(ItemPointerIsValid(&lt->heapPtr));

      if (bds->callback(&lt->heapPtr, bds->callback_state))
      {
        bds->stats->tuples_removed += 1;
        toDelete[xlrec.nDelete] = i;
        xlrec.nDelete++;
      }
      else
      {
        bds->stats->num_index_tuples += 1;
      }
    }
    else
    {
                                             
      elog(ERROR, "unexpected SPGiST tuple state: %d", lt->tupstate);
    }
  }

  if (xlrec.nDelete == 0)
  {
    return;                         
  }

                     
  START_CRIT_SECTION();

                                                                          
  PageIndexMultiDelete(page, toDelete, xlrec.nDelete);

  MarkBufferDirty(buffer);

  if (RelationNeedsWAL(index))
  {
    XLogRecPtr recptr;

    XLogBeginInsert();

                            
    STORE_STATE(&bds->spgstate, xlrec.stateSrc);

    XLogRegisterData((char *)&xlrec, SizeOfSpgxlogVacuumRoot);
                                                                    
    XLogRegisterData((char *)toDelete, sizeof(OffsetNumber) * xlrec.nDelete);

    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_VACUUM_ROOT);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();
}

   
                                                              
   
                                                                      
                                                                       
                         
   
                                                                       
   
static void
vacuumRedirectAndPlaceholder(Relation index, Buffer buffer)
{
  Page page = BufferGetPage(buffer);
  SpGistPageOpaque opaque = SpGistPageGetOpaque(page);
  OffsetNumber i, max = PageGetMaxOffsetNumber(page), firstPlaceholder = InvalidOffsetNumber;
  bool hasNonPlaceholder = false;
  bool hasUpdate = false;
  OffsetNumber itemToPlaceholder[MaxIndexTuplesPerPage];
  OffsetNumber itemnos[MaxIndexTuplesPerPage];
  spgxlogVacuumRedirect xlrec;

  xlrec.nToPlaceholder = 0;
  xlrec.newestRedirectXid = InvalidTransactionId;

  START_CRIT_SECTION();

     
                                                                             
                                                                      
     
  for (i = max; i >= FirstOffsetNumber && (opaque->nRedirection > 0 || !hasNonPlaceholder); i--)
  {
    SpGistDeadTuple dt;

    dt = (SpGistDeadTuple)PageGetItem(page, PageGetItemId(page, i));

    if (dt->tupstate == SPGIST_REDIRECT && TransactionIdPrecedes(dt->xid, RecentGlobalXmin))
    {
      dt->tupstate = SPGIST_PLACEHOLDER;
      Assert(opaque->nRedirection > 0);
      opaque->nRedirection--;
      opaque->nPlaceholder++;

                                                           
      if (!TransactionIdIsValid(xlrec.newestRedirectXid) || TransactionIdPrecedes(xlrec.newestRedirectXid, dt->xid))
      {
        xlrec.newestRedirectXid = dt->xid;
      }

      ItemPointerSetInvalid(&dt->pointer);

      itemToPlaceholder[xlrec.nToPlaceholder] = i;
      xlrec.nToPlaceholder++;

      hasUpdate = true;
    }

    if (dt->tupstate == SPGIST_PLACEHOLDER)
    {
      if (!hasNonPlaceholder)
      {
        firstPlaceholder = i;
      }
    }
    else
    {
      hasNonPlaceholder = true;
    }
  }

     
                                                                          
                                                                           
                                                               
     
  if (firstPlaceholder != InvalidOffsetNumber)
  {
       
                                                                          
       
    for (i = firstPlaceholder; i <= max; i++)
    {
      itemnos[i - firstPlaceholder] = i;
    }

    i = max - firstPlaceholder + 1;
    Assert(opaque->nPlaceholder >= i);
    opaque->nPlaceholder -= i;

                                                                     
    PageIndexMultiDelete(page, itemnos, i);

    hasUpdate = true;
  }

  xlrec.firstPlaceholder = firstPlaceholder;

  if (hasUpdate)
  {
    MarkBufferDirty(buffer);
  }

  if (hasUpdate && RelationNeedsWAL(index))
  {
    XLogRecPtr recptr;

    XLogBeginInsert();

    XLogRegisterData((char *)&xlrec, SizeOfSpgxlogVacuumRedirect);
    XLogRegisterData((char *)itemToPlaceholder, sizeof(OffsetNumber) * xlrec.nToPlaceholder);

    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_VACUUM_REDIRECT);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();
}

   
                                             
   
static void
spgvacuumpage(spgBulkDeleteState *bds, BlockNumber blkno)
{
  Relation index = bds->info->index;
  Buffer buffer;
  Page page;

                                                                 
  vacuum_delay_point();

  buffer = ReadBufferExtended(index, MAIN_FORKNUM, blkno, RBM_NORMAL, bds->info->strategy);
  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
  page = (Page)BufferGetPage(buffer);

  if (PageIsNew(page))
  {
       
                                                                     
                                                           
       
  }
  else if (PageIsEmpty(page))
  {
                       
  }
  else if (SpGistPageIsLeaf(page))
  {
    if (SpGistBlockIsRoot(blkno))
    {
      vacuumLeafRoot(bds, index, buffer);
                                                    
    }
    else
    {
      vacuumLeafPage(bds, index, buffer, false);
      vacuumRedirectAndPlaceholder(index, buffer);
    }
  }
  else
  {
                    
    vacuumRedirectAndPlaceholder(index, buffer);
  }

     
                                                                           
                                                                             
                                                                          
                     
     
  if (!SpGistBlockIsRoot(blkno))
  {
    if (PageIsNew(page) || PageIsEmpty(page))
    {
      RecordFreeIndexPage(index, blkno);
      bds->stats->pages_deleted++;
    }
    else
    {
      SpGistSetLastUsedPage(index, buffer);
      bds->lastFilledBlock = blkno;
    }
  }

  UnlockReleaseBuffer(buffer);
}

   
                                                               
   
static void
spgprocesspending(spgBulkDeleteState *bds)
{
  Relation index = bds->info->index;
  spgVacPendingItem *pitem;
  spgVacPendingItem *nitem;
  BlockNumber blkno;
  Buffer buffer;
  Page page;

  for (pitem = bds->pendingList; pitem != NULL; pitem = pitem->next)
  {
    if (pitem->done)
    {
      continue;                                
    }

                                                                   
    vacuum_delay_point();

                                     
    blkno = ItemPointerGetBlockNumber(&pitem->tid);
    buffer = ReadBufferExtended(index, MAIN_FORKNUM, blkno, RBM_NORMAL, bds->info->strategy);
    LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    page = (Page)BufferGetPage(buffer);

    if (PageIsNew(page) || SpGistPageIsDeleted(page))
    {
                                                    
    }
    else if (SpGistPageIsLeaf(page))
    {
      if (SpGistBlockIsRoot(blkno))
      {
                                               
        elog(ERROR, "redirection leads to root page of index \"%s\"", RelationGetRelationName(index));
      }

                                          
      vacuumLeafPage(bds, index, buffer, true);
                                                   
      vacuumRedirectAndPlaceholder(index, buffer);

      SpGistSetLastUsedPage(index, buffer);

         
                                                                    
                                                                      
         
      pitem->done = true;
      for (nitem = pitem->next; nitem != NULL; nitem = nitem->next)
      {
        if (ItemPointerGetBlockNumber(&nitem->tid) == blkno)
        {
          nitem->done = true;
        }
      }
    }
    else
    {
         
                                                                        
                                                                         
                                                                         
                                                                     
                                     
         
      for (nitem = pitem; nitem != NULL; nitem = nitem->next)
      {
        if (nitem->done)
        {
          continue;
        }
        if (ItemPointerGetBlockNumber(&nitem->tid) == blkno)
        {
          OffsetNumber offset;
          SpGistInnerTuple innerTuple;

          offset = ItemPointerGetOffsetNumber(&nitem->tid);
          innerTuple = (SpGistInnerTuple)PageGetItem(page, PageGetItemId(page, offset));
          if (innerTuple->tupstate == SPGIST_LIVE)
          {
            SpGistNodeTuple node;
            int i;

            SGITITERATE(innerTuple, i, node)
            {
              if (ItemPointerIsValid(&node->t_tid))
              {
                spgAddPendingTID(bds, &node->t_tid);
              }
            }
          }
          else if (innerTuple->tupstate == SPGIST_REDIRECT)
          {
                                                      
            spgAddPendingTID(bds, &((SpGistDeadTuple)innerTuple)->pointer);
          }
          else
          {
            elog(ERROR, "unexpected SPGiST tuple state: %d", innerTuple->tupstate);
          }

          nitem->done = true;
        }
      }
    }

    UnlockReleaseBuffer(buffer);
  }

  spgClearPendingList(bds);
}

   
                             
   
static void
spgvacuumscan(spgBulkDeleteState *bds)
{
  Relation index = bds->info->index;
  bool needLock;
  BlockNumber num_pages, blkno;

                                            
  initSpGistState(&bds->spgstate, index);
  bds->pendingList = NULL;
  bds->myXmin = GetActiveSnapshot()->xmin;
  bds->lastFilledBlock = SPGIST_LAST_FIXED_BLKNO;

     
                                                                           
                                                      
     
  bds->stats->estimated_count = false;
  bds->stats->num_index_tuples = 0;
  bds->stats->pages_deleted = 0;

                                                     
  needLock = !RELATION_IS_LOCAL(index);

     
                                                                          
                                                                    
                                                                          
                                                                         
                                                                           
                        
     
  blkno = SPGIST_METAPAGE_BLKNO + 1;
  for (;;)
  {
                                         
    if (needLock)
    {
      LockRelationForExtension(index, ExclusiveLock);
    }
    num_pages = RelationGetNumberOfBlocks(index);
    if (needLock)
    {
      UnlockRelationForExtension(index, ExclusiveLock);
    }

                                                  
    if (blkno >= num_pages)
    {
      break;
    }
                                                              
    for (; blkno < num_pages; blkno++)
    {
      spgvacuumpage(bds, blkno);
                                                  
      if (bds->pendingList != NULL)
      {
        spgprocesspending(bds);
      }
    }
  }

                                                       
  SpGistUpdateMetaPage(index);

     
                                                                      
                                                                            
                                                                     
                                                                          
                                                                       
                                                                      
                 
     
                                                                             
          
     
  if (bds->stats->pages_deleted > 0)
  {
    IndexFreeSpaceMapVacuum(index);
  }

     
                                
     
                                                                          
                                                                            
                                                                     
     
                                                                           
                                                                       
                                                                        
                                                                           
                                               
     
#ifdef NOT_USED
  if (num_pages > bds->lastFilledBlock + 1)
  {
    BlockNumber lastBlock = num_pages - 1;

    num_pages = bds->lastFilledBlock + 1;
    RelationTruncate(index, num_pages);
    bds->stats->pages_removed += lastBlock - bds->lastFilledBlock;
    bds->stats->pages_deleted -= lastBlock - bds->lastFilledBlock;
  }
#endif

                          
  bds->stats->num_pages = num_pages;
  bds->stats->pages_free = bds->stats->pages_deleted;
}

   
                                                                        
                                                                           
                                                                              
   
                                                                              
   
IndexBulkDeleteResult *
spgbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  spgBulkDeleteState bds;

                                                                         
  if (stats == NULL)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
  }
  bds.info = info;
  bds.stats = stats;
  bds.callback = callback;
  bds.callback_state = callback_state;

  spgvacuumscan(&bds);

  return stats;
}

                                                                
static bool
dummy_callback(ItemPointer itemptr, void *state)
{
  return false;
}

   
                        
   
                                                                              
   
IndexBulkDeleteResult *
spgvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
  spgBulkDeleteState bds;

                                  
  if (info->analyze_only)
  {
    return stats;
  }

     
                                                                         
                                                                          
                                                                           
                                                                  
     
  if (stats == NULL)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
    bds.info = info;
    bds.stats = stats;
    bds.callback = dummy_callback;
    bds.callback_state = NULL;

    spgvacuumscan(&bds);
  }

     
                                                                            
                                                                             
                                                                            
                                         
     
  if (!info->estimated_count)
  {
    if (stats->num_index_tuples > info->num_heap_tuples)
    {
      stats->num_index_tuples = info->num_heap_tuples;
    }
  }

  return stats;
}
