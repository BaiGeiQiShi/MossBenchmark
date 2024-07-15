                                                                            
   
               
                                                   
   
   
                                                                         
                                                                        
   
                  
                                        
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "postmaster/autovacuum.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/memutils.h"

struct GinVacuumState
{
  Relation index;
  IndexBulkDeleteResult *result;
  IndexBulkDeleteCallback callback;
  void *callback_state;
  GinState ginstate;
  BufferAccessStrategy strategy;
  MemoryContext tmpCxt;
};

   
                                                                               
                                
   
                                                                            
                                                                          
                                     
   
ItemPointer
ginVacuumItemPointers(GinVacuumState *gvs, ItemPointerData *items, int nitem, int *nremaining)
{
  int i, remaining = 0;
  ItemPointer tmpitems = NULL;

     
                             
     
  for (i = 0; i < nitem; i++)
  {
    if (gvs->callback(items + i, gvs->callback_state))
    {
      gvs->result->tuples_removed += 1;
      if (!tmpitems)
      {
           
                                                                
                            
           
        tmpitems = palloc(sizeof(ItemPointerData) * nitem);
        memcpy(tmpitems, items, sizeof(ItemPointerData) * i);
      }
    }
    else
    {
      gvs->result->num_index_tuples += 1;
      if (tmpitems)
      {
        tmpitems[remaining] = items[i];
      }
      remaining++;
    }
  }

  *nremaining = remaining;
  return tmpitems;
}

   
                                                           
   
static void
xlogVacuumPage(Relation index, Buffer buffer)
{
  Page page = BufferGetPage(buffer);
  XLogRecPtr recptr;

                                                    
  Assert(!GinPageIsData(page));
  Assert(GinPageIsLeaf(page));

  if (!RelationNeedsWAL(index))
  {
    return;
  }

     
                                                                           
                                                                      
     
  XLogBeginInsert();
  XLogRegisterBuffer(0, buffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);

  recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_VACUUM_PAGE);
  PageSetLSN(page, recptr);
}

typedef struct DataPageDeleteStack
{
  struct DataPageDeleteStack *child;
  struct DataPageDeleteStack *parent;

  BlockNumber blkno;                           
  Buffer leftBuffer;                                                
                                  
  bool isRoot;
} DataPageDeleteStack;

   
                               
   
static void
ginDeletePage(GinVacuumState *gvs, BlockNumber deleteBlkno, BlockNumber leftBlkno, BlockNumber parentBlkno, OffsetNumber myoff, bool isParentRoot)
{
  Buffer dBuffer;
  Buffer lBuffer;
  Buffer pBuffer;
  Page page, parentPage;
  BlockNumber rightlink;

     
                                                                       
                                                                          
                                                                     
                                       
     
  lBuffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, leftBlkno, RBM_NORMAL, gvs->strategy);
  dBuffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, deleteBlkno, RBM_NORMAL, gvs->strategy);
  pBuffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, parentBlkno, RBM_NORMAL, gvs->strategy);

  page = BufferGetPage(dBuffer);
  rightlink = GinPageGetOpaque(page)->rightlink;

     
                                                                           
                    
     
  PredicateLockPageCombine(gvs->index, deleteBlkno, rightlink);

  START_CRIT_SECTION();

                                                            
  page = BufferGetPage(lBuffer);
  GinPageGetOpaque(page)->rightlink = rightlink;

                                   
  parentPage = BufferGetPage(pBuffer);
#ifdef USE_ASSERT_CHECKING
  do
  {
    PostingItem *tod = GinDataPageGetPostingItem(parentPage, myoff);

    Assert(PostingItemGetBlockNumber(tod) == deleteBlkno);
  } while (0);
#endif
  GinPageDeletePostingItem(parentPage, myoff);

  page = BufferGetPage(dBuffer);

     
                                                                        
                 
     

     
                                                                      
              
     
  GinPageSetDeleted(page);
  GinPageSetDeleteXid(page, ReadNewTransactionId());

  MarkBufferDirty(pBuffer);
  MarkBufferDirty(lBuffer);
  MarkBufferDirty(dBuffer);

  if (RelationNeedsWAL(gvs->index))
  {
    XLogRecPtr recptr;
    ginxlogDeletePage data;

       
                                                                      
                                                                       
                                                                          
                                                                          
                                                                     
              
       
    XLogBeginInsert();
    XLogRegisterBuffer(0, dBuffer, 0);
    XLogRegisterBuffer(1, pBuffer, REGBUF_STANDARD);
    XLogRegisterBuffer(2, lBuffer, 0);

    data.parentOffset = myoff;
    data.rightLink = GinPageGetOpaque(page)->rightlink;
    data.deleteXid = GinPageGetDeleteXid(page);

    XLogRegisterData((char *)&data, sizeof(ginxlogDeletePage));

    recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_DELETE_PAGE);
    PageSetLSN(page, recptr);
    PageSetLSN(parentPage, recptr);
    PageSetLSN(BufferGetPage(lBuffer), recptr);
  }

  ReleaseBuffer(pBuffer);
  ReleaseBuffer(lBuffer);
  ReleaseBuffer(dBuffer);

  END_CRIT_SECTION();

  gvs->result->pages_deleted++;
}

   
                                                                               
                                                                            
                                                                            
                                                                          
                   
   
static bool
ginScanToDelete(GinVacuumState *gvs, BlockNumber blkno, bool isRoot, DataPageDeleteStack *parent, OffsetNumber myoff)
{
  DataPageDeleteStack *me;
  Buffer buffer;
  Page page;
  bool meDelete = false;
  bool isempty;

  if (isRoot)
  {
    me = parent;
  }
  else
  {
    if (!parent->child)
    {
      me = (DataPageDeleteStack *)palloc0(sizeof(DataPageDeleteStack));
      me->parent = parent;
      parent->child = me;
      me->leftBuffer = InvalidBuffer;
    }
    else
    {
      me = parent->child;
    }
  }

  buffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, blkno, RBM_NORMAL, gvs->strategy);

  if (!isRoot)
  {
    LockBuffer(buffer, GIN_EXCLUSIVE);
  }

  page = BufferGetPage(buffer);

  Assert(GinPageIsData(page));

  if (!GinPageIsLeaf(page))
  {
    OffsetNumber i;

    me->blkno = blkno;
    for (i = FirstOffsetNumber; i <= GinPageGetOpaque(page)->maxoff; i++)
    {
      PostingItem *pitem = GinDataPageGetPostingItem(page, i);

      if (ginScanToDelete(gvs, PostingItemGetBlockNumber(pitem), false, me, i))
      {
        i--;
      }
    }

    if (GinPageRightMost(page) && BufferIsValid(me->child->leftBuffer))
    {
      UnlockReleaseBuffer(me->child->leftBuffer);
      me->child->leftBuffer = InvalidBuffer;
    }
  }

  if (GinPageIsLeaf(page))
  {
    isempty = GinDataLeafPageIsEmpty(page);
  }
  else
  {
    isempty = GinPageGetOpaque(page)->maxoff < FirstOffsetNumber;
  }

  if (isempty)
  {
                                                       
    if (BufferIsValid(me->leftBuffer) && !GinPageRightMost(page))
    {
      Assert(!isRoot);
      ginDeletePage(gvs, blkno, BufferGetBlockNumber(me->leftBuffer), me->parent->blkno, myoff, me->parent->isRoot);
      meDelete = true;
    }
  }

  if (!meDelete)
  {
    if (BufferIsValid(me->leftBuffer))
    {
      UnlockReleaseBuffer(me->leftBuffer);
    }
    me->leftBuffer = buffer;
  }
  else
  {
    if (!isRoot)
    {
      LockBuffer(buffer, GIN_UNLOCK);
    }

    ReleaseBuffer(buffer);
  }

  if (isRoot)
  {
    ReleaseBuffer(buffer);
  }

  return meDelete;
}

   
                                                                                
                               
   
static bool
ginVacuumPostingTreeLeaves(GinVacuumState *gvs, BlockNumber blkno)
{
  Buffer buffer;
  Page page;
  bool hasVoidPage = false;
  MemoryContext oldCxt;

                                                                             
  while (true)
  {
    PostingItem *pitem;

    buffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, blkno, RBM_NORMAL, gvs->strategy);
    LockBuffer(buffer, GIN_SHARE);
    page = BufferGetPage(buffer);

    Assert(GinPageIsData(page));

    if (GinPageIsLeaf(page))
    {
      LockBuffer(buffer, GIN_UNLOCK);
      LockBuffer(buffer, GIN_EXCLUSIVE);
      break;
    }

    Assert(PageGetMaxOffsetNumber(page) >= FirstOffsetNumber);

    pitem = GinDataPageGetPostingItem(page, FirstOffsetNumber);
    blkno = PostingItemGetBlockNumber(pitem);
    Assert(blkno != InvalidBlockNumber);

    UnlockReleaseBuffer(buffer);
  }

                                                                        
  while (true)
  {
    oldCxt = MemoryContextSwitchTo(gvs->tmpCxt);
    ginVacuumPostingTreeLeaf(gvs->index, buffer, gvs);
    MemoryContextSwitchTo(oldCxt);
    MemoryContextReset(gvs->tmpCxt);

    if (GinDataLeafPageIsEmpty(page))
    {
      hasVoidPage = true;
    }

    blkno = GinPageGetOpaque(page)->rightlink;

    UnlockReleaseBuffer(buffer);

    if (blkno == InvalidBlockNumber)
    {
      break;
    }

    buffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, blkno, RBM_NORMAL, gvs->strategy);
    LockBuffer(buffer, GIN_EXCLUSIVE);
    page = BufferGetPage(buffer);
  }

  return hasVoidPage;
}

static void
ginVacuumPostingTree(GinVacuumState *gvs, BlockNumber rootBlkno)
{
  if (ginVacuumPostingTreeLeaves(gvs, rootBlkno))
  {
       
                                                                        
                             
       
    Buffer buffer;
    DataPageDeleteStack root, *ptr, *tmp;

    buffer = ReadBufferExtended(gvs->index, MAIN_FORKNUM, rootBlkno, RBM_NORMAL, gvs->strategy);

       
                                                                 
                           
       
    LockBufferForCleanup(buffer);

    memset(&root, 0, sizeof(DataPageDeleteStack));
    root.leftBuffer = InvalidBuffer;
    root.isRoot = true;

    ginScanToDelete(gvs, rootBlkno, true, &root, InvalidOffsetNumber);

    ptr = root.child;

    while (ptr)
    {
      tmp = ptr->child;
      pfree(ptr);
      ptr = tmp;
    }

    UnlockReleaseBuffer(buffer);
  }
}

   
                                                         
                                                                     
                                           
   
static Page
ginVacuumEntryPage(GinVacuumState *gvs, Buffer buffer, BlockNumber *roots, uint32 *nroot)
{
  Page origpage = BufferGetPage(buffer), tmppage;
  OffsetNumber i, maxoff = PageGetMaxOffsetNumber(origpage);

  tmppage = origpage;

  *nroot = 0;

  for (i = FirstOffsetNumber; i <= maxoff; i++)
  {
    IndexTuple itup = (IndexTuple)PageGetItem(tmppage, PageGetItemId(tmppage, i));

    if (GinIsPostingTree(itup))
    {
         
                                                                     
                                                                        
         
      roots[*nroot] = GinGetDownlink(itup);
      (*nroot)++;
    }
    else if (GinGetNPosting(itup) > 0)
    {
      int nitems;
      ItemPointer items_orig;
      bool free_items_orig;
      ItemPointer items;

                                                     
      if (GinItupIsCompressed(itup))
      {
        items_orig = ginPostingListDecode((GinPostingList *)GinGetPosting(itup), &nitems);
        free_items_orig = true;
      }
      else
      {
        items_orig = (ItemPointer)GinGetPosting(itup);
        nitems = GinGetNPosting(itup);
        free_items_orig = false;
      }

                                                                    
      items = ginVacuumItemPointers(gvs, items_orig, nitems, &nitems);

      if (free_items_orig)
      {
        pfree(items_orig);
      }

                                                                  
      if (items)
      {
        OffsetNumber attnum;
        Datum key;
        GinNullCategory category;
        GinPostingList *plist;
        int plistsize;

        if (nitems > 0)
        {
          plist = ginCompressPostingList(items, nitems, GinMaxItemSize, NULL);
          plistsize = SizeOfGinPostingList(plist);
        }
        else
        {
          plist = NULL;
          plistsize = 0;
        }

           
                                                                   
                 
           
        if (tmppage == origpage)
        {
             
                                                                 
                                                           
             
          tmppage = PageGetTempPageCopy(origpage);

                                            
          itup = (IndexTuple)PageGetItem(tmppage, PageGetItemId(tmppage, i));
        }

        attnum = gintuple_get_attrnum(&gvs->ginstate, itup);
        key = gintuple_get_key(&gvs->ginstate, itup, &category);
        itup = GinFormTuple(&gvs->ginstate, attnum, key, category, (char *)plist, plistsize, nitems, true);
        if (plist)
        {
          pfree(plist);
        }
        PageIndexTupleDelete(tmppage, i);

        if (PageAddItem(tmppage, (Item)itup, IndexTupleSize(itup), i, false, false) != i)
        {
          elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(gvs->index));
        }

        pfree(itup);
        pfree(items);
      }
    }
  }

  return (tmppage == origpage) ? NULL : tmppage;
}

IndexBulkDeleteResult *
ginbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  Relation index = info->index;
  BlockNumber blkno = GIN_ROOT_BLKNO;
  GinVacuumState gvs;
  Buffer buffer;
  BlockNumber rootOfPostingTree[BLCKSZ / (sizeof(IndexTupleData) + sizeof(ItemId))];
  uint32 nRoot;

  gvs.tmpCxt = AllocSetContextCreate(CurrentMemoryContext, "Gin vacuum temporary context", ALLOCSET_DEFAULT_SIZES);
  gvs.index = index;
  gvs.callback = callback;
  gvs.callback_state = callback_state;
  gvs.strategy = info->strategy;
  initGinState(&gvs.ginstate, index);

                           
  if (stats == NULL)
  {
                                            
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));

       
                                       
       
    ginInsertCleanup(&gvs.ginstate, !IsAutoVacuumWorkerProcess(), false, true, stats);
  }

                                           
  stats->num_index_tuples = 0;
  gvs.result = stats;

  buffer = ReadBufferExtended(index, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);

                      
  for (;;)
  {
    Page page = BufferGetPage(buffer);
    IndexTuple itup;

    LockBuffer(buffer, GIN_SHARE);

    Assert(!GinPageIsData(page));

    if (GinPageIsLeaf(page))
    {
      LockBuffer(buffer, GIN_UNLOCK);
      LockBuffer(buffer, GIN_EXCLUSIVE);

      if (blkno == GIN_ROOT_BLKNO && !GinPageIsLeaf(page))
      {
        LockBuffer(buffer, GIN_UNLOCK);
        continue;                        
      }
      break;
    }

    Assert(PageGetMaxOffsetNumber(page) >= FirstOffsetNumber);

    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, FirstOffsetNumber));
    blkno = GinGetDownlink(itup);
    Assert(blkno != InvalidBlockNumber);

    UnlockReleaseBuffer(buffer);
    buffer = ReadBufferExtended(index, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);
  }

                                                         

  for (;;)
  {
    Page page = BufferGetPage(buffer);
    Page resPage;
    uint32 i;

    Assert(!GinPageIsData(page));

    resPage = ginVacuumEntryPage(&gvs, buffer, rootOfPostingTree, &nRoot);

    blkno = GinPageGetOpaque(page)->rightlink;

    if (resPage)
    {
      START_CRIT_SECTION();
      PageRestoreTempPage(resPage, page);
      MarkBufferDirty(buffer);
      xlogVacuumPage(gvs.index, buffer);
      UnlockReleaseBuffer(buffer);
      END_CRIT_SECTION();
    }
    else
    {
      UnlockReleaseBuffer(buffer);
    }

    vacuum_delay_point();

    for (i = 0; i < nRoot; i++)
    {
      ginVacuumPostingTree(&gvs, rootOfPostingTree[i]);
      vacuum_delay_point();
    }

    if (blkno == InvalidBlockNumber)                     
    {
      break;
    }

    buffer = ReadBufferExtended(index, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);
    LockBuffer(buffer, GIN_EXCLUSIVE);
  }

  MemoryContextDelete(gvs.tmpCxt);

  return gvs.result;
}

IndexBulkDeleteResult *
ginvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
  Relation index = info->index;
  bool needLock;
  BlockNumber npages, blkno;
  BlockNumber totFreePages;
  GinState ginstate;
  GinStatsData idxStat;

     
                                                                       
                                                 
     
  if (info->analyze_only)
  {
    if (IsAutoVacuumWorkerProcess())
    {
      initGinState(&ginstate, index);
      ginInsertCleanup(&ginstate, false, true, true, stats);
    }
    return stats;
  }

     
                                                                        
                   
     
  if (stats == NULL)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
    initGinState(&ginstate, index);
    ginInsertCleanup(&ginstate, !IsAutoVacuumWorkerProcess(), false, true, stats);
  }

  memset(&idxStat, 0, sizeof(idxStat));

     
                                                                      
                                                                            
                                                                        
     
  stats->num_index_tuples = info->num_heap_tuples;
  stats->estimated_count = info->estimated_count;

     
                                                  
     
  needLock = !RELATION_IS_LOCAL(index);

  if (needLock)
  {
    LockRelationForExtension(index, ExclusiveLock);
  }
  npages = RelationGetNumberOfBlocks(index);
  if (needLock)
  {
    UnlockRelationForExtension(index, ExclusiveLock);
  }

  totFreePages = 0;

  for (blkno = GIN_ROOT_BLKNO; blkno < npages; blkno++)
  {
    Buffer buffer;
    Page page;

    vacuum_delay_point();

    buffer = ReadBufferExtended(index, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);
    LockBuffer(buffer, GIN_SHARE);
    page = (Page)BufferGetPage(buffer);

    if (GinPageIsRecyclable(page))
    {
      Assert(blkno != GIN_ROOT_BLKNO);
      RecordFreeIndexPage(index, blkno);
      totFreePages++;
    }
    else if (GinPageIsData(page))
    {
      idxStat.nDataPages++;
    }
    else if (!GinPageIsList(page))
    {
      idxStat.nEntryPages++;

      if (GinPageIsLeaf(page))
      {
        idxStat.nEntries += PageGetMaxOffsetNumber(page);
      }
    }

    UnlockReleaseBuffer(buffer);
  }

                                                               
  idxStat.nTotalPages = npages;
  ginUpdateStats(info->index, &idxStat, false);

                               
  IndexFreeSpaceMapVacuum(info->index);

  stats->pages_free = totFreePages;

  if (needLock)
  {
    LockRelationForExtension(index, ExclusiveLock);
  }
  stats->num_pages = RelationGetNumberOfBlocks(index);
  if (needLock)
  {
    UnlockRelationForExtension(index, ExclusiveLock);
  }

  return stats;
}
