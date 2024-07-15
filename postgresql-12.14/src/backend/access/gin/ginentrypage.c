                                                                            
   
                  
                                                 
   
   
                                                                         
                                                                        
   
                  
                                           
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "utils/rel.h"

static void
entrySplitPage(GinBtree btree, Buffer origbuf, GinBtreeStack *stack, GinBtreeEntryInsertData *insertData, BlockNumber updateblkno, Page *newlpage, Page *newrpage);

   
                                
   
                                                                          
                                                                          
   
                                                                          
                                                                        
                                                                              
                                                                           
                                                                                
                                                                           
                                                                         
                                           
   
IndexTuple
GinFormTuple(GinState *ginstate, OffsetNumber attnum, Datum key, GinNullCategory category, Pointer data, Size dataSize, int nipd, bool errorTooBig)
{
  Datum datums[2];
  bool isnull[2];
  IndexTuple itup;
  uint32 newsize;

                                                                     
  if (ginstate->oneCol)
  {
    datums[0] = key;
    isnull[0] = (category != GIN_CAT_NORM_KEY);
  }
  else
  {
    datums[0] = UInt16GetDatum(attnum);
    isnull[0] = false;
    datums[1] = key;
    isnull[1] = (category != GIN_CAT_NORM_KEY);
  }

  itup = index_form_tuple(ginstate->tupdesc[attnum - 1], datums, isnull);

     
                                                                          
                                           
     
                                                                             
                                                                           
                                                                          
                                                                         
                     
     
  newsize = IndexTupleSize(itup);

  if (IndexTupleHasNulls(itup))
  {
    uint32 minsize;

    Assert(category != GIN_CAT_NORM_KEY);
    minsize = GinCategoryOffset(itup, ginstate) + sizeof(GinNullCategory);
    newsize = Max(newsize, minsize);
  }

  newsize = SHORTALIGN(newsize);

  GinSetPostingOffset(itup, newsize);
  GinSetNPosting(itup, nipd);

     
                                                                           
                                
     
  newsize += dataSize;

  newsize = MAXALIGN(newsize);

  if (newsize > GinMaxItemSize)
  {
    if (errorTooBig)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", (Size)newsize, (Size)GinMaxItemSize, RelationGetRelationName(ginstate->index))));
    }
    pfree(itup);
    return NULL;
  }

     
                            
     
  if (newsize != IndexTupleSize(itup))
  {
    itup = repalloc(itup, newsize);

       
                                                                      
                                                                       
       
    memset((char *)itup + IndexTupleSize(itup), 0, newsize - IndexTupleSize(itup));
                                      
    itup->t_info &= ~INDEX_SIZE_MASK;
    itup->t_info |= newsize;
  }

     
                                           
     
  if (data)
  {
    char *ptr = GinGetPosting(itup);

    memcpy(ptr, data, dataSize);
  }

     
                                     
     
  if (category != GIN_CAT_NORM_KEY)
  {
    Assert(IndexTupleHasNulls(itup));
    GinSetNullCategory(itup, ginstate, category);
  }
  return itup;
}

   
                                             
   
                                                                             
               
   
ItemPointer
ginReadTuple(GinState *ginstate, OffsetNumber attnum, IndexTuple itup, int *nitems)
{
  Pointer ptr = GinGetPosting(itup);
  int nipd = GinGetNPosting(itup);
  ItemPointer ipd;
  int ndecoded;

  if (GinItupIsCompressed(itup))
  {
    if (nipd > 0)
    {
      ipd = ginPostingListDecode((GinPostingList *)ptr, &ndecoded);
      if (nipd != ndecoded)
      {
        elog(ERROR, "number of items mismatch in GIN entry tuple, %d in tuple header, %d decoded", nipd, ndecoded);
      }
    }
    else
    {
      ipd = palloc(0);
    }
  }
  else
  {
    ipd = (ItemPointer)palloc(sizeof(ItemPointerData) * nipd);
    memcpy(ipd, ptr, sizeof(ItemPointerData) * nipd);
  }
  *nitems = nipd;
  return ipd;
}

   
                                                                             
                                                       
   
                                                                            
                                        
   
static IndexTuple
GinFormInteriorTuple(IndexTuple itup, Page page, BlockNumber childblk)
{
  IndexTuple nitup;

  if (GinPageIsLeaf(page) && !GinIsPostingTree(itup))
  {
                                                                    
    uint32 origsize = GinGetPostingOffset(itup);

    origsize = MAXALIGN(origsize);
    nitup = (IndexTuple)palloc(origsize);
    memcpy(nitup, itup, origsize);
                                                      
    nitup->t_info &= ~INDEX_SIZE_MASK;
    nitup->t_info |= origsize;
  }
  else
  {
                              
    nitup = (IndexTuple)palloc(IndexTupleSize(itup));
    memcpy(nitup, itup, IndexTupleSize(itup));
  }

                                       
  GinSetDownlink(nitup, childblk);

  return nitup;
}

   
                                                             
                                                              
   
static IndexTuple
getRightMostTuple(Page page)
{
  OffsetNumber maxoff = PageGetMaxOffsetNumber(page);

  return (IndexTuple)PageGetItem(page, PageGetItemId(page, maxoff));
}

static bool
entryIsMoveRight(GinBtree btree, Page page)
{
  IndexTuple itup;
  OffsetNumber attnum;
  Datum key;
  GinNullCategory category;

  if (GinPageRightMost(page))
  {
    return false;
  }

  itup = getRightMostTuple(page);
  attnum = gintuple_get_attrnum(btree->ginstate, itup);
  key = gintuple_get_key(btree->ginstate, itup, &category);

  if (ginCompareAttEntries(btree->ginstate, btree->entryAttnum, btree->entryKey, btree->entryCategory, attnum, key, category) > 0)
  {
    return true;
  }

  return false;
}

   
                                                         
                                                               
   
static BlockNumber
entryLocateEntry(GinBtree btree, GinBtreeStack *stack)
{
  OffsetNumber low, high, maxoff;
  IndexTuple itup = NULL;
  int result;
  Page page = BufferGetPage(stack->buffer);

  Assert(!GinPageIsLeaf(page));
  Assert(!GinPageIsData(page));

  if (btree->fullScan)
  {
    stack->off = FirstOffsetNumber;
    stack->predictNumber *= PageGetMaxOffsetNumber(page);
    return btree->getLeftMostChild(btree, page);
  }

  low = FirstOffsetNumber;
  maxoff = high = PageGetMaxOffsetNumber(page);
  Assert(high >= low);

  high++;

  while (high > low)
  {
    OffsetNumber mid = low + ((high - low) / 2);

    if (mid == maxoff && GinPageRightMost(page))
    {
                          
      result = -1;
    }
    else
    {
      OffsetNumber attnum;
      Datum key;
      GinNullCategory category;

      itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, mid));
      attnum = gintuple_get_attrnum(btree->ginstate, itup);
      key = gintuple_get_key(btree->ginstate, itup, &category);
      result = ginCompareAttEntries(btree->ginstate, btree->entryAttnum, btree->entryKey, btree->entryCategory, attnum, key, category);
    }

    if (result == 0)
    {
      stack->off = mid;
      Assert(GinGetDownlink(itup) != GIN_ROOT_BLKNO);
      return GinGetDownlink(itup);
    }
    else if (result > 0)
    {
      low = mid + 1;
    }
    else
    {
      high = mid;
    }
  }

  Assert(high >= FirstOffsetNumber && high <= maxoff);

  stack->off = high;
  itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, high));
  Assert(GinGetDownlink(itup) != GIN_ROOT_BLKNO);
  return GinGetDownlink(itup);
}

   
                                                     
                                    
                                        
   
static bool
entryLocateLeafEntry(GinBtree btree, GinBtreeStack *stack)
{
  Page page = BufferGetPage(stack->buffer);
  OffsetNumber low, high;

  Assert(GinPageIsLeaf(page));
  Assert(!GinPageIsData(page));

  if (btree->fullScan)
  {
    stack->off = FirstOffsetNumber;
    return true;
  }

  low = FirstOffsetNumber;
  high = PageGetMaxOffsetNumber(page);

  if (high < low)
  {
    stack->off = FirstOffsetNumber;
    return false;
  }

  high++;

  while (high > low)
  {
    OffsetNumber mid = low + ((high - low) / 2);
    IndexTuple itup;
    OffsetNumber attnum;
    Datum key;
    GinNullCategory category;
    int result;

    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, mid));
    attnum = gintuple_get_attrnum(btree->ginstate, itup);
    key = gintuple_get_key(btree->ginstate, itup, &category);
    result = ginCompareAttEntries(btree->ginstate, btree->entryAttnum, btree->entryKey, btree->entryCategory, attnum, key, category);
    if (result == 0)
    {
      stack->off = mid;
      return true;
    }
    else if (result > 0)
    {
      low = mid + 1;
    }
    else
    {
      high = mid;
    }
  }

  stack->off = high;
  return false;
}

static OffsetNumber
entryFindChildPtr(GinBtree btree, Page page, BlockNumber blkno, OffsetNumber storedOff)
{
  OffsetNumber i, maxoff = PageGetMaxOffsetNumber(page);
  IndexTuple itup;

  Assert(!GinPageIsLeaf(page));
  Assert(!GinPageIsData(page));

                                                   
  if (storedOff >= FirstOffsetNumber && storedOff <= maxoff)
  {
    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, storedOff));
    if (GinGetDownlink(itup) == blkno)
    {
      return storedOff;
    }

       
                                                                      
                         
       
    for (i = storedOff + 1; i <= maxoff; i++)
    {
      itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, i));
      if (GinGetDownlink(itup) == blkno)
      {
        return i;
      }
    }
    maxoff = storedOff - 1;
  }

                   
  for (i = FirstOffsetNumber; i <= maxoff; i++)
  {
    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, i));
    if (GinGetDownlink(itup) == blkno)
    {
      return i;
    }
  }

  return InvalidOffsetNumber;
}

static BlockNumber
entryGetLeftMostPage(GinBtree btree, Page page)
{
  IndexTuple itup;

  Assert(!GinPageIsLeaf(page));
  Assert(!GinPageIsData(page));
  Assert(PageGetMaxOffsetNumber(page) >= FirstOffsetNumber);

  itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, FirstOffsetNumber));
  return GinGetDownlink(itup);
}

static bool
entryIsEnoughSpace(GinBtree btree, Buffer buf, OffsetNumber off, GinBtreeEntryInsertData *insertData)
{
  Size releasedsz = 0;
  Size addedsz;
  Page page = BufferGetPage(buf);

  Assert(insertData->entry);
  Assert(!GinPageIsData(page));

  if (insertData->isDelete)
  {
    IndexTuple itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, off));

    releasedsz = MAXALIGN(IndexTupleSize(itup)) + sizeof(ItemIdData);
  }

  addedsz = MAXALIGN(IndexTupleSize(insertData->entry)) + sizeof(ItemIdData);

  if (PageGetFreeSpace(page) + releasedsz >= addedsz)
  {
    return true;
  }

  return false;
}

   
                                                      
                                                              
                           
   
static void
entryPreparePage(GinBtree btree, Page page, OffsetNumber off, GinBtreeEntryInsertData *insertData, BlockNumber updateblkno)
{
  Assert(insertData->entry);
  Assert(!GinPageIsData(page));

  if (insertData->isDelete)
  {
    Assert(GinPageIsLeaf(page));
    PageIndexTupleDelete(page, off);
  }

  if (!GinPageIsLeaf(page) && updateblkno != InvalidBlockNumber)
  {
    IndexTuple itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, off));

    GinSetDownlink(itup, updateblkno);
  }
}

   
                                            
   
                                                                           
                                                                          
                                                                  
   
                                                                       
                                                                
   
                                                                  
   
                                                                              
                                                                            
                         
   
static GinPlaceToPageRC
entryBeginPlaceToPage(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertPayload, BlockNumber updateblkno, void **ptp_workspace, Page *newlpage, Page *newrpage)
{
  GinBtreeEntryInsertData *insertData = insertPayload;
  OffsetNumber off = stack->off;

                                               
  if (!entryIsEnoughSpace(btree, buf, off, insertData))
  {
    entrySplitPage(btree, buf, stack, insertData, updateblkno, newlpage, newrpage);
    return GPTP_SPLIT;
  }

                                                   
  return GPTP_INSERT;
}

   
                                                                          
   
                                                                           
                                                                           
   
static void
entryExecPlaceToPage(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertPayload, BlockNumber updateblkno, void *ptp_workspace)
{
  GinBtreeEntryInsertData *insertData = insertPayload;
  Page page = BufferGetPage(buf);
  OffsetNumber off = stack->off;
  OffsetNumber placed;

  entryPreparePage(btree, page, off, insertData, updateblkno);

  placed = PageAddItem(page, (Item)insertData->entry, IndexTupleSize(insertData->entry), off, false, false);
  if (placed != off)
  {
    elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(btree->index));
  }

  if (RelationNeedsWAL(btree->index) && !btree->isBuild)
  {
       
                                                                        
                                                                          
                               
       
    static ginxlogInsertEntry data;

    data.isDelete = insertData->isDelete;
    data.offset = off;

    XLogRegisterBufData(0, (char *)&data, offsetof(ginxlogInsertEntry, tuple));
    XLogRegisterBufData(0, (char *)insertData->entry, IndexTupleSize(insertData->entry));
  }
}

   
                                         
   
                                                      
                                          
   
static void
entrySplitPage(GinBtree btree, Buffer origbuf, GinBtreeStack *stack, GinBtreeEntryInsertData *insertData, BlockNumber updateblkno, Page *newlpage, Page *newrpage)
{
  OffsetNumber off = stack->off;
  OffsetNumber i, maxoff, separator = InvalidOffsetNumber;
  Size totalsize = 0;
  Size lsize = 0, size;
  char *ptr;
  IndexTuple itup;
  Page page;
  Page lpage = PageGetTempPageCopy(BufferGetPage(origbuf));
  Page rpage = PageGetTempPageCopy(BufferGetPage(origbuf));
  Size pageSize = PageGetPageSize(lpage);
  PGAlignedBlock tupstore[2];                                          

  entryPreparePage(btree, lpage, off, insertData, updateblkno);

     
                                                                             
                                                 
     
  maxoff = PageGetMaxOffsetNumber(lpage);
  ptr = tupstore[0].data;
  for (i = FirstOffsetNumber; i <= maxoff; i++)
  {
    if (i == off)
    {
      size = MAXALIGN(IndexTupleSize(insertData->entry));
      memcpy(ptr, insertData->entry, size);
      ptr += size;
      totalsize += size + sizeof(ItemIdData);
    }

    itup = (IndexTuple)PageGetItem(lpage, PageGetItemId(lpage, i));
    size = MAXALIGN(IndexTupleSize(itup));
    memcpy(ptr, itup, size);
    ptr += size;
    totalsize += size + sizeof(ItemIdData);
  }

  if (off == maxoff + 1)
  {
    size = MAXALIGN(IndexTupleSize(insertData->entry));
    memcpy(ptr, insertData->entry, size);
    ptr += size;
    totalsize += size + sizeof(ItemIdData);
  }

     
                                                                          
           
     
  GinInitPage(rpage, GinPageGetOpaque(lpage)->flags, pageSize);
  GinInitPage(lpage, GinPageGetOpaque(rpage)->flags, pageSize);

  ptr = tupstore[0].data;
  maxoff++;
  lsize = 0;

  page = lpage;
  for (i = FirstOffsetNumber; i <= maxoff; i++)
  {
    itup = (IndexTuple)ptr;

       
                                                                        
                                   
       
    if (lsize > totalsize / 2)
    {
      if (separator == InvalidOffsetNumber)
      {
        separator = i - 1;
      }
      page = rpage;
    }
    else
    {
      lsize += MAXALIGN(IndexTupleSize(itup)) + sizeof(ItemIdData);
    }

    if (PageAddItem(page, (Item)itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
    {
      elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(btree->index));
    }
    ptr += MAXALIGN(IndexTupleSize(itup));
  }

                                   
  *newlpage = lpage;
  *newrpage = rpage;
}

   
                                                                            
   
static void *
entryPrepareDownlink(GinBtree btree, Buffer lbuf)
{
  GinBtreeEntryInsertData *insertData;
  Page lpage = BufferGetPage(lbuf);
  BlockNumber lblkno = BufferGetBlockNumber(lbuf);
  IndexTuple itup;

  itup = getRightMostTuple(lpage);

  insertData = palloc(sizeof(GinBtreeEntryInsertData));
  insertData->entry = GinFormInteriorTuple(itup, lpage, lblkno);
  insertData->isDelete = false;

  return insertData;
}

   
                                                 
                                                  
   
void
ginEntryFillRoot(GinBtree btree, Page root, BlockNumber lblkno, Page lpage, BlockNumber rblkno, Page rpage)
{
  IndexTuple itup;

  itup = GinFormInteriorTuple(getRightMostTuple(lpage), lpage, lblkno);
  if (PageAddItem(root, (Item)itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR, "failed to add item to index root page");
  }
  pfree(itup);

  itup = GinFormInteriorTuple(getRightMostTuple(rpage), rpage, rblkno);
  if (PageAddItem(root, (Item)itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR, "failed to add item to index root page");
  }
  pfree(itup);
}

   
                                         
   
                                                                     
                                                                       
   
void
ginPrepareEntryScan(GinBtree btree, OffsetNumber attnum, Datum key, GinNullCategory category, GinState *ginstate)
{
  memset(btree, 0, sizeof(GinBtreeData));

  btree->index = ginstate->index;
  btree->rootBlkno = GIN_ROOT_BLKNO;
  btree->ginstate = ginstate;

  btree->findChildPage = entryLocateEntry;
  btree->getLeftMostChild = entryGetLeftMostPage;
  btree->isMoveRight = entryIsMoveRight;
  btree->findItem = entryLocateLeafEntry;
  btree->findChildPtr = entryFindChildPtr;
  btree->beginPlaceToPage = entryBeginPlaceToPage;
  btree->execPlaceToPage = entryExecPlaceToPage;
  btree->fillRoot = ginEntryFillRoot;
  btree->prepareDownlink = entryPrepareDownlink;

  btree->isData = false;
  btree->fullScan = false;
  btree->isBuild = false;

  btree->entryAttnum = attnum;
  btree->entryKey = key;
  btree->entryCategory = category;
}
