                                                                            
   
             
                                                                         
                                                                   
                                                                      
                                                                      
                                                                     
   
                                                                         
                                                                        
   
                  
                                      
   
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "access/xlog.h"
#include "commands/vacuum.h"
#include "catalog/pg_am.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/acl.h"
#include "postmaster/autovacuum.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/builtins.h"

                   
int gin_pending_list_limit = 0;

#define GIN_PAGE_FREESIZE (BLCKSZ - MAXALIGN(SizeOfPageHeaderData) - MAXALIGN(sizeof(GinPageOpaqueData)))

typedef struct KeyArray
{
  Datum *keys;                                       
  GinNullCategory *categories;                               
  int32 nvalues;                                                    
  int32 maxvalues;                                           
} KeyArray;

   
                                                                               
   
                                                  
   
static int32
writeListPage(Relation index, Buffer buffer, IndexTuple *tuples, int32 ntuples, BlockNumber rightlink)
{
  Page page = BufferGetPage(buffer);
  int32 i, freesize, size = 0;
  OffsetNumber l, off;
  PGAlignedBlock workspace;
  char *ptr;

  START_CRIT_SECTION();

  GinInitBuffer(buffer, GIN_LIST);

  off = FirstOffsetNumber;
  ptr = workspace.data;

  for (i = 0; i < ntuples; i++)
  {
    int this_size = IndexTupleSize(tuples[i]);

    memcpy(ptr, tuples[i], this_size);
    ptr += this_size;
    size += this_size;

    l = PageAddItem(page, (Item)tuples[i], this_size, off, false, false);

    if (l == InvalidOffsetNumber)
    {
      elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(index));
    }

    off++;
  }

  Assert(size <= BLCKSZ);                                

  GinPageGetOpaque(page)->rightlink = rightlink;

     
                                                                            
                                                                             
                     
     
  if (rightlink == InvalidBlockNumber)
  {
    GinPageSetFullRow(page);
    GinPageGetOpaque(page)->maxoff = 1;
  }
  else
  {
    GinPageGetOpaque(page)->maxoff = 0;
  }

  MarkBufferDirty(buffer);

  if (RelationNeedsWAL(index))
  {
    ginxlogInsertListPage data;
    XLogRecPtr recptr;

    data.rightlink = rightlink;
    data.ntuples = ntuples;

    XLogBeginInsert();
    XLogRegisterData((char *)&data, sizeof(ginxlogInsertListPage));

    XLogRegisterBuffer(0, buffer, REGBUF_WILL_INIT);
    XLogRegisterBufData(0, workspace.data, size);

    recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_INSERT_LISTPAGE);
    PageSetLSN(page, recptr);
  }

                                              
  freesize = PageGetExactFreeSpace(page);

  UnlockReleaseBuffer(buffer);

  END_CRIT_SECTION();

  return freesize;
}

static void
makeSublist(Relation index, IndexTuple *tuples, int32 ntuples, GinMetaPageData *res)
{
  Buffer curBuffer = InvalidBuffer;
  Buffer prevBuffer = InvalidBuffer;
  int i, size = 0, tupsize;
  int startTuple = 0;

  Assert(ntuples > 0);

     
                             
     
  for (i = 0; i < ntuples; i++)
  {
    if (curBuffer == InvalidBuffer)
    {
      curBuffer = GinNewBuffer(index);

      if (prevBuffer != InvalidBuffer)
      {
        res->nPendingPages++;
        writeListPage(index, prevBuffer, tuples + startTuple, i - startTuple, BufferGetBlockNumber(curBuffer));
      }
      else
      {
        res->head = BufferGetBlockNumber(curBuffer);
      }

      prevBuffer = curBuffer;
      startTuple = i;
      size = 0;
    }

    tupsize = MAXALIGN(IndexTupleSize(tuples[i])) + sizeof(ItemIdData);

    if (size + tupsize > GinListPageSize)
    {
                                                     
      i--;
      curBuffer = InvalidBuffer;
    }
    else
    {
      size += tupsize;
    }
  }

     
                     
     
  res->tail = BufferGetBlockNumber(curBuffer);
  res->tailFreeSize = writeListPage(index, curBuffer, tuples + startTuple, ntuples - startTuple, InvalidBlockNumber);
  res->nPendingPages++;
                                    
  res->nPendingHeapTuples = 1;
}

   
                                                                   
                 
   
                                                                             
                    
   
void
ginHeapTupleFastInsert(GinState *ginstate, GinTupleCollector *collector)
{
  Relation index = ginstate->index;
  Buffer metabuffer;
  Page metapage;
  GinMetaPageData *metadata = NULL;
  Buffer buffer = InvalidBuffer;
  Page page = NULL;
  ginxlogUpdateMeta data;
  bool separateList = false;
  bool needCleanup = false;
  int cleanupSize;
  bool needWal;

  if (collector->ntuples == 0)
  {
    return;
  }

  needWal = RelationNeedsWAL(index);

  data.node = index->rd_node;
  data.ntuples = 0;
  data.newRightlink = data.prevTail = InvalidBlockNumber;

  metabuffer = ReadBuffer(index, GIN_METAPAGE_BLKNO);
  metapage = BufferGetPage(metabuffer);

     
                                                                             
                                                                             
                                                         
     
  CheckForSerializableConflictIn(index, NULL, metabuffer);

  if (collector->sumsize + collector->ntuples * sizeof(ItemIdData) > GinListPageSize)
  {
       
                                                           
       
    separateList = true;
  }
  else
  {
    LockBuffer(metabuffer, GIN_EXCLUSIVE);
    metadata = GinPageGetMeta(metapage);

    if (metadata->head == InvalidBlockNumber || collector->sumsize + collector->ntuples * sizeof(ItemIdData) > metadata->tailFreeSize)
    {
         
                                                                       
                                      
         
                                                       
         
      separateList = true;
      LockBuffer(metabuffer, GIN_UNLOCK);
    }
  }

  if (separateList)
  {
       
                                                                   
       
    GinMetaPageData sublist;

    memset(&sublist, 0, sizeof(GinMetaPageData));
    makeSublist(index, collector->tuples, collector->ntuples, &sublist);

       
                                        
       
    LockBuffer(metabuffer, GIN_EXCLUSIVE);
    metadata = GinPageGetMeta(metapage);

    if (metadata->head == InvalidBlockNumber)
    {
         
                                                                 
         
      START_CRIT_SECTION();

      metadata->head = sublist.head;
      metadata->tail = sublist.tail;
      metadata->tailFreeSize = sublist.tailFreeSize;

      metadata->nPendingPages = sublist.nPendingPages;
      metadata->nPendingHeapTuples = sublist.nPendingHeapTuples;

      if (needWal)
      {
        XLogBeginInsert();
      }
    }
    else
    {
         
                     
         
      data.prevTail = metadata->tail;
      data.newRightlink = sublist.head;

      buffer = ReadBuffer(index, metadata->tail);
      LockBuffer(buffer, GIN_EXCLUSIVE);
      page = BufferGetPage(buffer);

      Assert(GinPageGetOpaque(page)->rightlink == InvalidBlockNumber);

      START_CRIT_SECTION();

      GinPageGetOpaque(page)->rightlink = sublist.head;

      MarkBufferDirty(buffer);

      metadata->tail = sublist.tail;
      metadata->tailFreeSize = sublist.tailFreeSize;

      metadata->nPendingPages += sublist.nPendingPages;
      metadata->nPendingHeapTuples += sublist.nPendingHeapTuples;

      if (needWal)
      {
        XLogBeginInsert();
        XLogRegisterBuffer(1, buffer, REGBUF_STANDARD);
      }
    }
  }
  else
  {
       
                                                          
       
    OffsetNumber l, off;
    int i, tupsize;
    char *ptr;
    char *collectordata;

    buffer = ReadBuffer(index, metadata->tail);
    LockBuffer(buffer, GIN_EXCLUSIVE);
    page = BufferGetPage(buffer);

    off = (PageIsEmpty(page)) ? FirstOffsetNumber : OffsetNumberNext(PageGetMaxOffsetNumber(page));

    collectordata = ptr = (char *)palloc(collector->sumsize);

    data.ntuples = collector->ntuples;

    START_CRIT_SECTION();

    if (needWal)
    {
      XLogBeginInsert();
    }

       
                                       
       
    Assert(GinPageGetOpaque(page)->maxoff <= metadata->nPendingHeapTuples);
    GinPageGetOpaque(page)->maxoff++;
    metadata->nPendingHeapTuples++;

    for (i = 0; i < collector->ntuples; i++)
    {
      tupsize = IndexTupleSize(collector->tuples[i]);
      l = PageAddItem(page, (Item)collector->tuples[i], tupsize, off, false, false);

      if (l == InvalidOffsetNumber)
      {
        elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(index));
      }

      memcpy(ptr, collector->tuples[i], tupsize);
      ptr += tupsize;

      off++;
    }

    Assert((ptr - collectordata) <= collector->sumsize);
    if (needWal)
    {
      XLogRegisterBuffer(1, buffer, REGBUF_STANDARD);
      XLogRegisterBufData(1, collectordata, collector->sumsize);
    }

    metadata->tailFreeSize = PageGetExactFreeSpace(page);

    MarkBufferDirty(buffer);
  }

     
                                                                         
                                                                          
                                                                             
                                                                         
                               
     
  ((PageHeader)metapage)->pd_lower = ((char *)metadata + sizeof(GinMetaPageData)) - (char *)metapage;

     
                                       
     
  MarkBufferDirty(metabuffer);

  if (needWal)
  {
    XLogRecPtr recptr;

    memcpy(&data.metadata, metadata, sizeof(GinMetaPageData));

    XLogRegisterBuffer(0, metabuffer, REGBUF_WILL_INIT | REGBUF_STANDARD);
    XLogRegisterData((char *)&data, sizeof(ginxlogUpdateMeta));

    recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_UPDATE_META_PAGE);
    PageSetLSN(metapage, recptr);

    if (buffer != InvalidBuffer)
    {
      PageSetLSN(page, recptr);
    }
  }

  if (buffer != InvalidBuffer)
  {
    UnlockReleaseBuffer(buffer);
  }

     
                                                               
                                                                             
                                                                          
                                                                            
                                                          
                             
     
                                                                      
     
  cleanupSize = GinGetPendingListCleanupSize(index);
  if (metadata->nPendingPages * GIN_PAGE_FREESIZE > cleanupSize * 1024L)
  {
    needCleanup = true;
  }

  UnlockReleaseBuffer(metabuffer);

  END_CRIT_SECTION();

     
                                                                       
                                
     
  if (needCleanup)
  {
    ginInsertCleanup(ginstate, false, true, false, NULL);
  }
}

   
                                                                               
                                                                          
                                                               
                                                                         
                                                                     
                           
   
void
ginHeapTupleFastCollect(GinState *ginstate, GinTupleCollector *collector, OffsetNumber attnum, Datum value, bool isNull, ItemPointer ht_ctid)
{
  Datum *entries;
  GinNullCategory *categories;
  int32 i, nentries;

     
                                                                  
     
  entries = ginExtractEntries(ginstate, attnum, value, isNull, &nentries, &categories);

     
                                                                 
     
  if (nentries < 0 || collector->ntuples + nentries > MaxAllocSize / sizeof(IndexTuple))
  {
    elog(ERROR, "too many entries for GIN index");
  }

     
                                                             
     
  if (collector->tuples == NULL)
  {
       
                                                                        
                                                                     
                                                  
       
    collector->lentuples = 16;
    while (collector->lentuples < nentries)
    {
      collector->lentuples *= 2;
    }

    collector->tuples = (IndexTuple *)palloc(sizeof(IndexTuple) * collector->lentuples);
  }
  else if (collector->lentuples < collector->ntuples + nentries)
  {
       
                                                                      
                                                             
                                                                      
       
    do
    {
      collector->lentuples *= 2;
    } while (collector->lentuples < collector->ntuples + nentries);

    collector->tuples = (IndexTuple *)repalloc(collector->tuples, sizeof(IndexTuple) * collector->lentuples);
  }

     
                                                                            
                                                   
     
  for (i = 0; i < nentries; i++)
  {
    IndexTuple itup;

    itup = GinFormTuple(ginstate, attnum, entries[i], categories[i], NULL, 0, 0, true);
    itup->t_tid = *ht_ctid;
    collector->tuples[collector->ntuples++] = itup;
    collector->sumsize += IndexTupleSize(itup);
  }
}

   
                                                                  
                                                                        
   
                                                                     
   
static void
shiftList(Relation index, Buffer metabuffer, BlockNumber newHead, bool fill_fsm, IndexBulkDeleteResult *stats)
{
  Page metapage;
  GinMetaPageData *metadata;
  BlockNumber blknoToDelete;

  metapage = BufferGetPage(metabuffer);
  metadata = GinPageGetMeta(metapage);
  blknoToDelete = metadata->head;

  do
  {
    Page page;
    int i;
    int64 nDeletedHeapTuples = 0;
    ginxlogDeleteListPages data;
    Buffer buffers[GIN_NDELETE_AT_ONCE];
    BlockNumber freespace[GIN_NDELETE_AT_ONCE];

    data.ndeleted = 0;
    while (data.ndeleted < GIN_NDELETE_AT_ONCE && blknoToDelete != newHead)
    {
      freespace[data.ndeleted] = blknoToDelete;
      buffers[data.ndeleted] = ReadBuffer(index, blknoToDelete);
      LockBuffer(buffers[data.ndeleted], GIN_EXCLUSIVE);
      page = BufferGetPage(buffers[data.ndeleted]);

      data.ndeleted++;

      Assert(!GinPageIsDeleted(page));

      nDeletedHeapTuples += GinPageGetOpaque(page)->maxoff;
      blknoToDelete = GinPageGetOpaque(page)->rightlink;
    }

    if (stats)
    {
      stats->pages_deleted += data.ndeleted;
    }

       
                                                                     
                                                                     
                         
       
    if (RelationNeedsWAL(index))
    {
      XLogEnsureRecordSpace(data.ndeleted, 0);
    }

    START_CRIT_SECTION();

    metadata->head = blknoToDelete;

    Assert(metadata->nPendingPages >= data.ndeleted);
    metadata->nPendingPages -= data.ndeleted;
    Assert(metadata->nPendingHeapTuples >= nDeletedHeapTuples);
    metadata->nPendingHeapTuples -= nDeletedHeapTuples;

    if (blknoToDelete == InvalidBlockNumber)
    {
      metadata->tail = InvalidBlockNumber;
      metadata->tailFreeSize = 0;
      metadata->nPendingPages = 0;
      metadata->nPendingHeapTuples = 0;
    }

       
                                                                           
                                                                 
                                                                   
                                                                          
                                                         
       
    ((PageHeader)metapage)->pd_lower = ((char *)metadata + sizeof(GinMetaPageData)) - (char *)metapage;

    MarkBufferDirty(metabuffer);

    for (i = 0; i < data.ndeleted; i++)
    {
      page = BufferGetPage(buffers[i]);
      GinPageGetOpaque(page)->flags = GIN_DELETED;
      MarkBufferDirty(buffers[i]);
    }

    if (RelationNeedsWAL(index))
    {
      XLogRecPtr recptr;

      XLogBeginInsert();
      XLogRegisterBuffer(0, metabuffer, REGBUF_WILL_INIT | REGBUF_STANDARD);
      for (i = 0; i < data.ndeleted; i++)
      {
        XLogRegisterBuffer(i + 1, buffers[i], REGBUF_WILL_INIT);
      }

      memcpy(&data.metadata, metadata, sizeof(GinMetaPageData));

      XLogRegisterData((char *)&data, sizeof(ginxlogDeleteListPages));

      recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_DELETE_LISTPAGE);
      PageSetLSN(metapage, recptr);

      for (i = 0; i < data.ndeleted; i++)
      {
        page = BufferGetPage(buffers[i]);
        PageSetLSN(page, recptr);
      }
    }

    for (i = 0; i < data.ndeleted; i++)
    {
      UnlockReleaseBuffer(buffers[i]);
    }

    END_CRIT_SECTION();

    for (i = 0; fill_fsm && i < data.ndeleted; i++)
    {
      RecordFreeIndexPage(index, freespace[i]);
    }

  } while (blknoToDelete != newHead);
}

                               
static void
initKeyArray(KeyArray *keys, int32 maxvalues)
{
  keys->keys = (Datum *)palloc(sizeof(Datum) * maxvalues);
  keys->categories = (GinNullCategory *)palloc(sizeof(GinNullCategory) * maxvalues);
  keys->nvalues = 0;
  keys->maxvalues = maxvalues;
}

                                               
static void
addDatum(KeyArray *keys, Datum datum, GinNullCategory category)
{
  if (keys->nvalues >= keys->maxvalues)
  {
    keys->maxvalues *= 2;
    keys->keys = (Datum *)repalloc(keys->keys, sizeof(Datum) * keys->maxvalues);
    keys->categories = (GinNullCategory *)repalloc(keys->categories, sizeof(GinNullCategory) * keys->maxvalues);
  }

  keys->keys[keys->nvalues] = datum;
  keys->categories[keys->nvalues] = category;
  keys->nvalues++;
}

   
                                                                           
                   
   
                                                                         
   
                                                                         
          
   
static void
processPendingPage(BuildAccumulator *accum, KeyArray *ka, Page page, OffsetNumber startoff)
{
  ItemPointerData heapptr;
  OffsetNumber i, maxoff;
  OffsetNumber attrnum;

                          
  ka->nvalues = 0;

  maxoff = PageGetMaxOffsetNumber(page);
  Assert(maxoff >= FirstOffsetNumber);
  ItemPointerSetInvalid(&heapptr);
  attrnum = 0;

  for (i = startoff; i <= maxoff; i = OffsetNumberNext(i))
  {
    IndexTuple itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, i));
    OffsetNumber curattnum;
    Datum curkey;
    GinNullCategory curcategory;

                                                
    curattnum = gintuple_get_attrnum(accum->ginstate, itup);

    if (!ItemPointerIsValid(&heapptr))
    {
      heapptr = itup->t_tid;
      attrnum = curattnum;
    }
    else if (!(ItemPointerEquals(&heapptr, &itup->t_tid) && curattnum == attrnum))
    {
         
                                                                         
                                                                       
                       
         
      ginInsertBAEntries(accum, &heapptr, attrnum, ka->keys, ka->categories, ka->nvalues);
      ka->nvalues = 0;
      heapptr = itup->t_tid;
      attrnum = curattnum;
    }

                             
    curkey = gintuple_get_key(accum->ginstate, itup, &curcategory);
    addDatum(ka, curkey, curcategory);
  }

                                   
  ginInsertBAEntries(accum, &heapptr, attrnum, ka->keys, ka->categories, ka->nvalues);
}

   
                                                              
   
                                                                       
                                                                             
                                                                              
                    
   
                                                                     
                                                                    
        
   
                                                                        
   
void
ginInsertCleanup(GinState *ginstate, bool full_clean, bool fill_fsm, bool forceCleanup, IndexBulkDeleteResult *stats)
{
  Relation index = ginstate->index;
  Buffer metabuffer, buffer;
  Page metapage, page;
  GinMetaPageData *metadata;
  MemoryContext opCtx, oldCtx;
  BuildAccumulator accum;
  KeyArray datums;
  BlockNumber blkno, blknoFinish;
  bool cleanupFinish = false;
  bool fsm_vac = false;
  Size workMemory;

     
                                                                           
                                                                         
                                                                           
                                 
     

  if (forceCleanup)
  {
       
                                                                           
                                                               
       
    LockPage(index, GIN_METAPAGE_BLKNO, ExclusiveLock);
    workMemory = (IsAutoVacuumWorkerProcess() && autovacuum_work_mem != -1) ? autovacuum_work_mem : maintenance_work_mem;
  }
  else
  {
       
                                                                          
                                                                       
             
       
    if (!ConditionalLockPage(index, GIN_METAPAGE_BLKNO, ExclusiveLock))
    {
      return;
    }
    workMemory = work_mem;
  }

  metabuffer = ReadBuffer(index, GIN_METAPAGE_BLKNO);
  LockBuffer(metabuffer, GIN_SHARE);
  metapage = BufferGetPage(metabuffer);
  metadata = GinPageGetMeta(metapage);

  if (metadata->head == InvalidBlockNumber)
  {
                       
    UnlockReleaseBuffer(metabuffer);
    UnlockPage(index, GIN_METAPAGE_BLKNO, ExclusiveLock);
    return;
  }

     
                                                                            
                                            
     
  blknoFinish = metadata->tail;

     
                                        
     
  blkno = metadata->head;
  buffer = ReadBuffer(index, blkno);
  LockBuffer(buffer, GIN_SHARE);
  page = BufferGetPage(buffer);

  LockBuffer(metabuffer, GIN_UNLOCK);

     
                                                       
     
  opCtx = AllocSetContextCreate(CurrentMemoryContext, "GIN insert cleanup temporary context", ALLOCSET_DEFAULT_SIZES);

  oldCtx = MemoryContextSwitchTo(opCtx);

  initKeyArray(&datums, 128);
  ginInitBA(&accum);
  accum.ginstate = ginstate;

     
                                                                          
                                                                             
                                                         
     
  for (;;)
  {
    Assert(!GinPageIsDeleted(page));

       
                                                                         
                                                                      
                                                                          
              
       
    if (blkno == blknoFinish && full_clean == false)
    {
      cleanupFinish = true;
    }

       
                                     
       
    processPendingPage(&accum, &datums, page, FirstOffsetNumber);

    vacuum_delay_point();

       
                                                                         
                                                                        
             
       
    if (GinPageGetOpaque(page)->rightlink == InvalidBlockNumber || (GinPageHasFullRow(page) && (accum.allocatedMemory >= workMemory * 1024L)))
    {
      ItemPointerData *list;
      uint32 nlist;
      Datum key;
      GinNullCategory category;
      OffsetNumber maxoff, attnum;

         
                                                                      
                                                                       
                       
         
      maxoff = PageGetMaxOffsetNumber(page);
      LockBuffer(buffer, GIN_UNLOCK);

         
                                                               
                                                                         
               
         
      ginBeginBAScan(&accum);
      while ((list = ginGetBAEntry(&accum, &attnum, &key, &category, &nlist)) != NULL)
      {
        ginEntryInsert(ginstate, attnum, key, category, list, nlist, NULL);
        vacuum_delay_point();
      }

         
                                             
         
      LockBuffer(metabuffer, GIN_EXCLUSIVE);
      LockBuffer(buffer, GIN_SHARE);

      Assert(!GinPageIsDeleted(page));

         
                                                                       
                                                                        
                                                                       
                                                                   
                                                                         
                                                                 
         
      if (PageGetMaxOffsetNumber(page) != maxoff)
      {
        ginInitBA(&accum);
        processPendingPage(&accum, &datums, page, maxoff + 1);

        ginBeginBAScan(&accum);
        while ((list = ginGetBAEntry(&accum, &attnum, &key, &category, &nlist)) != NULL)
        {
          ginEntryInsert(ginstate, attnum, key, category, list, nlist, NULL);
        }
      }

         
                                                               
         
      blkno = GinPageGetOpaque(page)->rightlink;
      UnlockReleaseBuffer(buffer);                                
                                                

         
                                                                        
                                               
         
      shiftList(index, metabuffer, blkno, fill_fsm, stats);

                                                                
      fsm_vac = true;

      Assert(blkno == metadata->head);
      LockBuffer(metabuffer, GIN_UNLOCK);

         
                                                                        
                                                                    
         
      if (blkno == InvalidBlockNumber || cleanupFinish)
      {
        break;
      }

         
                                                     
         
      MemoryContextReset(opCtx);
      initKeyArray(&datums, datums.maxvalues);
      ginInitBA(&accum);
    }
    else
    {
      blkno = GinPageGetOpaque(page)->rightlink;
      UnlockReleaseBuffer(buffer);
    }

       
                                      
       
    vacuum_delay_point();
    buffer = ReadBuffer(index, blkno);
    LockBuffer(buffer, GIN_SHARE);
    page = BufferGetPage(buffer);
  }

  UnlockPage(index, GIN_METAPAGE_BLKNO, ExclusiveLock);
  ReleaseBuffer(metabuffer);

     
                                                                          
                                                                          
                     
     
  if (fsm_vac && fill_fsm)
  {
    IndexFreeSpaceMapVacuum(index);
  }

                                
  MemoryContextSwitchTo(oldCtx);
  MemoryContextDelete(opCtx);
}

   
                                                          
   
Datum
gin_clean_pending_list(PG_FUNCTION_ARGS)
{
  Oid indexoid = PG_GETARG_OID(0);
  Relation indexRel = index_open(indexoid, RowExclusiveLock);
  IndexBulkDeleteResult stats;
  GinState ginstate;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("GIN pending list cannot be cleaned up during recovery.")));
  }

                           
  if (indexRel->rd_rel->relkind != RELKIND_INDEX || indexRel->rd_rel->relam != GIN_AM_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a GIN index", RelationGetRelationName(indexRel))));
  }

     
                                                                        
                                                                          
                              
     
  if (RELATION_IS_OTHER_TEMP(indexRel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot access temporary indexes of other sessions")));
  }

                                                                            
  if (!pg_class_ownercheck(indexoid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_INDEX, RelationGetRelationName(indexRel));
  }

  memset(&stats, 0, sizeof(stats));
  initGinState(&ginstate, indexRel);
  ginInsertCleanup(&ginstate, true, true, true, &stats);

  index_close(indexRel, RowExclusiveLock);

  PG_RETURN_INT64((int64)stats.pages_deleted);
}
