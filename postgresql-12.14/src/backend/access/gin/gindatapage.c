                                                                            
   
                 
                                                   
   
   
                                                                         
                                                                        
   
                  
                                          
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "lib/ilist.h"
#include "miscadmin.h"
#include "storage/predicate.h"
#include "utils/rel.h"

   
                                                                             
   
                                                                             
                                                                        
                                                                             
                                                                         
                                                  
   
#define GinPostingListSegmentMaxSize 384
#define GinPostingListSegmentTargetSize 256
#define GinPostingListSegmentMinSize 128

   
                                                                        
                                                                         
                            
   
#define MinTuplesPerSegment ((GinPostingListSegmentMaxSize - 2) / 6)

   
                                                               
   
typedef struct
{
  dlist_head segments;                                 

     
                                                                             
                                                                
     
  dlist_node *lastleft;                                
  int lsize;                                         
  int rsize;                                          

  bool oldformat;                                        

     
                                                                        
                                                  
     
  char *walinfo;                    
  int walinfolen;                 
} disassembledLeaf;

typedef struct
{
  dlist_node node;                           

                  
                                                                          
                                                                  
     
                           
                                                                 
                
                                              
                                                                 
                                                                          
                                                                 
                        
                  
     
  char action;

  ItemPointerData *modifieditems;
  uint16 nmodifieditems;

     
                                                                             
                                                                            
                                                                       
                                                                        
                                                                             
     
  GinPostingList *seg;
  ItemPointer items;
  int nitems;                                              
} leafSegmentInfo;

static ItemPointer
dataLeafPageGetUncompressed(Page page, int *nitems);
static void
dataSplitPageInternal(GinBtree btree, Buffer origbuf, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, Page *newlpage, Page *newrpage);

static disassembledLeaf *
disassembleLeaf(Page page);
static bool
leafRepackItems(disassembledLeaf *leaf, ItemPointer remaining);
static bool
addItemsToLeaf(disassembledLeaf *leaf, ItemPointer newItems, int nNewItems);

static void
computeLeafRecompressWALData(disassembledLeaf *leaf);
static void
dataPlaceToPageLeafRecompress(Buffer buf, disassembledLeaf *leaf);
static void
dataPlaceToPageLeafSplit(disassembledLeaf *leaf, ItemPointerData lbound, ItemPointerData rbound, Page lpage, Page rpage);

   
                                                                            
                                
   
                                                                           
                                                                   
   
                                                                            
                                                                             
                                                                              
                             
   
ItemPointer
GinDataLeafPageGetItems(Page page, int *nitems, ItemPointerData advancePast)
{
  ItemPointer result;

  if (GinPageIsCompressed(page))
  {
    GinPostingList *seg = GinDataLeafPageGetPostingList(page);
    Size len = GinDataLeafPageGetPostingListSize(page);
    Pointer endptr = ((Pointer)seg) + len;
    GinPostingList *next;

                                                      
    if (ItemPointerIsValid(&advancePast))
    {
      next = GinNextPostingListSegment(seg);
      while ((Pointer)next < endptr && ginCompareItemPointers(&next->first, &advancePast) <= 0)
      {
        seg = next;
        next = GinNextPostingListSegment(seg);
      }
      len = endptr - (Pointer)seg;
    }

    if (len > 0)
    {
      result = ginPostingListDecodeAllSegments(seg, len, nitems);
    }
    else
    {
      result = NULL;
      *nitems = 0;
    }
  }
  else
  {
    ItemPointer tmp = dataLeafPageGetUncompressed(page, nitems);

    result = palloc((*nitems) * sizeof(ItemPointerData));
    memcpy(result, tmp, (*nitems) * sizeof(ItemPointerData));
  }

  return result;
}

   
                                                  
   
int
GinDataLeafPageGetItemsToTbm(Page page, TIDBitmap *tbm)
{
  ItemPointer uncompressed;
  int nitems;

  if (GinPageIsCompressed(page))
  {
    GinPostingList *segment = GinDataLeafPageGetPostingList(page);
    Size len = GinDataLeafPageGetPostingListSize(page);

    nitems = ginPostingListDecodeAllSegmentsToTbm(segment, len, tbm);
  }
  else
  {
    uncompressed = dataLeafPageGetUncompressed(page, &nitems);

    if (nitems > 0)
    {
      tbm_add_tuples(tbm, uncompressed, nitems, false);
    }
  }

  return nitems;
}

   
                                                                      
                                                                           
            
   
static ItemPointer
dataLeafPageGetUncompressed(Page page, int *nitems)
{
  ItemPointer items;

  Assert(!GinPageIsCompressed(page));

     
                                                                        
                                                                       
     
  items = (ItemPointer)GinDataPageGetData(page);
  *nitems = GinPageGetOpaque(page)->maxoff;

  return items;
}

   
                                                                             
        
   
                                                                             
   
static bool
dataIsMoveRight(GinBtree btree, Page page)
{
  ItemPointer iptr = GinDataPageGetRightBound(page);

  if (GinPageRightMost(page))
  {
    return false;
  }

  if (GinPageIsDeleted(page))
  {
    return true;
  }

  return (ginCompareItemPointers(&btree->itemptr, iptr) > 0) ? true : false;
}

   
                                                                         
                                                                   
   
static BlockNumber
dataLocateItem(GinBtree btree, GinBtreeStack *stack)
{
  OffsetNumber low, high, maxoff;
  PostingItem *pitem = NULL;
  int result;
  Page page = BufferGetPage(stack->buffer);

  Assert(!GinPageIsLeaf(page));
  Assert(GinPageIsData(page));

  if (btree->fullScan)
  {
    stack->off = FirstOffsetNumber;
    stack->predictNumber *= GinPageGetOpaque(page)->maxoff;
    return btree->getLeftMostChild(btree, page);
  }

  low = FirstOffsetNumber;
  maxoff = high = GinPageGetOpaque(page)->maxoff;
  Assert(high >= low);

  high++;

  while (high > low)
  {
    OffsetNumber mid = low + ((high - low) / 2);

    pitem = GinDataPageGetPostingItem(page, mid);

    if (mid == maxoff)
    {
         
                                                                      
                         
         
      result = -1;
    }
    else
    {
      pitem = GinDataPageGetPostingItem(page, mid);
      result = ginCompareItemPointers(&btree->itemptr, &(pitem->key));
    }

    if (result == 0)
    {
      stack->off = mid;
      return PostingItemGetBlockNumber(pitem);
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
  pitem = GinDataPageGetPostingItem(page, high);
  return PostingItemGetBlockNumber(pitem);
}

   
                                                                      
   
static OffsetNumber
dataFindChildPtr(GinBtree btree, Page page, BlockNumber blkno, OffsetNumber storedOff)
{
  OffsetNumber i, maxoff = GinPageGetOpaque(page)->maxoff;
  PostingItem *pitem;

  Assert(!GinPageIsLeaf(page));
  Assert(GinPageIsData(page));

                                                  
  if (storedOff >= FirstOffsetNumber && storedOff <= maxoff)
  {
    pitem = GinDataPageGetPostingItem(page, storedOff);
    if (PostingItemGetBlockNumber(pitem) == blkno)
    {
      return storedOff;
    }

       
                                                                      
                         
       
    for (i = storedOff + 1; i <= maxoff; i++)
    {
      pitem = GinDataPageGetPostingItem(page, i);
      if (PostingItemGetBlockNumber(pitem) == blkno)
      {
        return i;
      }
    }

    maxoff = storedOff - 1;
  }

                   
  for (i = FirstOffsetNumber; i <= maxoff; i++)
  {
    pitem = GinDataPageGetPostingItem(page, i);
    if (PostingItemGetBlockNumber(pitem) == blkno)
    {
      return i;
    }
  }

  return InvalidOffsetNumber;
}

   
                                  
   
static BlockNumber
dataGetLeftMostPage(GinBtree btree, Page page)
{
  PostingItem *pitem;

  Assert(!GinPageIsLeaf(page));
  Assert(GinPageIsData(page));
  Assert(GinPageGetOpaque(page)->maxoff >= FirstOffsetNumber);

  pitem = GinDataPageGetPostingItem(page, FirstOffsetNumber);
  return PostingItemGetBlockNumber(pitem);
}

   
                                       
   
void
GinDataPageAddPostingItem(Page page, PostingItem *data, OffsetNumber offset)
{
  OffsetNumber maxoff = GinPageGetOpaque(page)->maxoff;
  char *ptr;

  Assert(PostingItemGetBlockNumber(data) != InvalidBlockNumber);
  Assert(!GinPageIsLeaf(page));

  if (offset == InvalidOffsetNumber)
  {
    ptr = (char *)GinDataPageGetPostingItem(page, maxoff + 1);
  }
  else
  {
    ptr = (char *)GinDataPageGetPostingItem(page, offset);
    if (offset != maxoff + 1)
    {
      memmove(ptr + sizeof(PostingItem), ptr, (maxoff - offset + 1) * sizeof(PostingItem));
    }
  }
  memcpy(ptr, data, sizeof(PostingItem));

  maxoff++;
  GinPageGetOpaque(page)->maxoff = maxoff;

     
                                                                      
                                                                         
                            
     
  GinDataPageSetDataSize(page, maxoff * sizeof(PostingItem));
}

   
                                          
   
void
GinPageDeletePostingItem(Page page, OffsetNumber offset)
{
  OffsetNumber maxoff = GinPageGetOpaque(page)->maxoff;

  Assert(!GinPageIsLeaf(page));
  Assert(offset >= FirstOffsetNumber && offset <= maxoff);

  if (offset != maxoff)
  {
    memmove(GinDataPageGetPostingItem(page, offset), GinDataPageGetPostingItem(page, offset + 1), sizeof(PostingItem) * (maxoff - offset));
  }

  maxoff--;
  GinPageGetOpaque(page)->maxoff = maxoff;

  GinDataPageSetDataSize(page, maxoff * sizeof(PostingItem));
}

   
                                               
   
                                                                           
                                                                          
                                                                  
   
                                                                       
                                                                
   
                                                                  
   
static GinPlaceToPageRC
dataBeginPlaceToPageLeaf(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertdata, void **ptp_workspace, Page *newlpage, Page *newrpage)
{
  GinBtreeDataLeafInsertData *items = insertdata;
  ItemPointer newItems = &items->items[items->curitem];
  int maxitems = items->nitem - items->curitem;
  Page page = BufferGetPage(buf);
  int i;
  ItemPointerData rbound;
  ItemPointerData lbound;
  bool needsplit;
  bool append;
  int segsize;
  Size freespace;
  disassembledLeaf *leaf;
  leafSegmentInfo *lastleftinfo;
  ItemPointerData maxOldItem;
  ItemPointerData remaining;

  rbound = *GinDataPageGetRightBound(page);

     
                                                          
     
  if (!GinPageRightMost(page))
  {
    for (i = 0; i < maxitems; i++)
    {
      if (ginCompareItemPointers(&newItems[i], &rbound) > 0)
      {
           
                                                                     
                                                                  
                                            
           
        Assert(i > 0);
        break;
      }
    }
    maxitems = i;
  }

                                        
  leaf = disassembleLeaf(page);

     
                                                                         
                                            
     
  if (!dlist_is_empty(&leaf->segments))
  {
    lastleftinfo = dlist_container(leafSegmentInfo, node, dlist_tail_node(&leaf->segments));
    if (!lastleftinfo->items)
    {
      lastleftinfo->items = ginPostingListDecode(lastleftinfo->seg, &lastleftinfo->nitems);
    }
    maxOldItem = lastleftinfo->items[lastleftinfo->nitems - 1];
    if (ginCompareItemPointers(&newItems[0], &maxOldItem) >= 0)
    {
      append = true;
    }
    else
    {
      append = false;
    }
  }
  else
  {
    ItemPointerSetMin(&maxOldItem);
    append = true;
  }

     
                                                                             
                                                                            
                                                                           
                                                                        
                                                                 
     
  if (GinPageIsCompressed(page))
  {
    freespace = GinDataLeafPageGetFreeSpace(page);
  }
  else
  {
    freespace = 0;
  }
  if (append)
  {
       
                                                                         
                                                                        
                                                                           
                                                                           
                                                                          
       
    maxitems = Min(maxitems, freespace + GinDataPageMaxDataSize);
  }
  else
  {
       
                                                                          
                                         
       
                                                                         
                                                                           
                                          
       
    int nnewsegments;

    nnewsegments = freespace / GinPostingListSegmentMaxSize;
    nnewsegments += GinDataPageMaxDataSize / GinPostingListSegmentMaxSize;
    maxitems = Min(maxitems, nnewsegments * MinTuplesPerSegment);
  }

                                             
  if (!addItemsToLeaf(leaf, newItems, maxitems))
  {
                                                          
    items->curitem += maxitems;

    return GPTP_NO_WORK;
  }

     
                                                                            
     
  needsplit = leafRepackItems(leaf, &remaining);

     
                                
     
                                                                        
                                        
     
  if (ItemPointerIsValid(&remaining))
  {
    if (!append || ItemPointerCompare(&maxOldItem, &remaining) >= 0)
    {
      elog(ERROR, "could not split GIN page; all old items didn't fit");
    }

                                                  
    for (i = 0; i < maxitems; i++)
    {
      if (ginCompareItemPointers(&newItems[i], &remaining) >= 0)
      {
        break;
      }
    }
    if (i == 0)
    {
      elog(ERROR, "could not split GIN page; no new items fit");
    }
    maxitems = i;
  }

  if (!needsplit)
  {
       
                                                                           
                                                           
       
    if (RelationNeedsWAL(btree->index) && !btree->isBuild)
    {
      computeLeafRecompressWALData(leaf);
    }

       
                                                      
                                                                    
       
    *ptp_workspace = leaf;

    if (append)
    {
      elog(DEBUG2, "appended %d new items to block %u; %d bytes (%d to go)", maxitems, BufferGetBlockNumber(buf), (int)leaf->lsize, items->nitem - items->curitem - maxitems);
    }
    else
    {
      elog(DEBUG2, "inserted %d new items to block %u; %d bytes (%d to go)", maxitems, BufferGetBlockNumber(buf), (int)leaf->lsize, items->nitem - items->curitem - maxitems);
    }
  }
  else
  {
       
                      
       
                                                                         
                                                                        
                                                                         
                                                                          
                                                                       
                                                                       
                                                                      
                               
       
                                                                      
                                                                        
                                                                          
                                                                           
               
       
    if (!btree->isBuild)
    {
      while (dlist_has_prev(&leaf->segments, leaf->lastleft))
      {
        lastleftinfo = dlist_container(leafSegmentInfo, node, leaf->lastleft);

                                     
        if (lastleftinfo->action != GIN_SEGMENT_DELETE)
        {
          segsize = SizeOfGinPostingList(lastleftinfo->seg);

             
                                                                   
                                                                    
                                                                    
                                 
             
          if ((leaf->lsize - segsize) - (leaf->rsize + segsize) < 0)
          {
            break;
          }
          if (append)
          {
            if ((leaf->lsize - segsize) < (BLCKSZ * 3) / 4)
            {
              break;
            }
          }

          leaf->lsize -= segsize;
          leaf->rsize += segsize;
        }
        leaf->lastleft = dlist_prev_node(&leaf->segments, leaf->lastleft);
      }
    }
    Assert(leaf->lsize <= GinDataPageMaxDataSize);
    Assert(leaf->rsize <= GinDataPageMaxDataSize);

       
                                                                          
                                
       
    lastleftinfo = dlist_container(leafSegmentInfo, node, leaf->lastleft);
    if (!lastleftinfo->items)
    {
      lastleftinfo->items = ginPostingListDecode(lastleftinfo->seg, &lastleftinfo->nitems);
    }
    lbound = lastleftinfo->items[lastleftinfo->nitems - 1];

       
                                                                      
       
    *newlpage = palloc(BLCKSZ);
    *newrpage = palloc(BLCKSZ);

    dataPlaceToPageLeafSplit(leaf, lbound, rbound, *newlpage, *newrpage);

    Assert(GinPageRightMost(page) || ginCompareItemPointers(GinDataPageGetRightBound(*newlpage), GinDataPageGetRightBound(*newrpage)) < 0);

    if (append)
    {
      elog(DEBUG2, "appended %d items to block %u; split %d/%d (%d to go)", maxitems, BufferGetBlockNumber(buf), (int)leaf->lsize, (int)leaf->rsize, items->nitem - items->curitem - maxitems);
    }
    else
    {
      elog(DEBUG2, "inserted %d items to block %u; split %d/%d (%d to go)", maxitems, BufferGetBlockNumber(buf), (int)leaf->lsize, (int)leaf->rsize, items->nitem - items->curitem - maxitems);
    }
  }

  items->curitem += maxitems;

  return needsplit ? GPTP_SPLIT : GPTP_INSERT;
}

   
                                                                          
   
                                                                           
                                                                           
   
static void
dataExecPlaceToPageLeaf(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertdata, void *ptp_workspace)
{
  disassembledLeaf *leaf = (disassembledLeaf *)ptp_workspace;

                             
  dataPlaceToPageLeafRecompress(buf, leaf);

                                                                          
  if (RelationNeedsWAL(btree->index) && !btree->isBuild)
  {
    XLogRegisterBufData(0, leaf->walinfo, leaf->walinfolen);
  }
}

   
                                    
   
void
ginVacuumPostingTreeLeaf(Relation indexrel, Buffer buffer, GinVacuumState *gvs)
{
  Page page = BufferGetPage(buffer);
  disassembledLeaf *leaf;
  bool removedsomething = false;
  dlist_iter iter;

  leaf = disassembleLeaf(page);

                            
  dlist_foreach(iter, &leaf->segments)
  {
    leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, iter.cur);
    int oldsegsize;
    ItemPointer cleaned;
    int ncleaned;

    if (!seginfo->items)
    {
      seginfo->items = ginPostingListDecode(seginfo->seg, &seginfo->nitems);
    }
    if (seginfo->seg)
    {
      oldsegsize = SizeOfGinPostingList(seginfo->seg);
    }
    else
    {
      oldsegsize = GinDataPageMaxDataSize;
    }

    cleaned = ginVacuumItemPointers(gvs, seginfo->items, seginfo->nitems, &ncleaned);
    pfree(seginfo->items);
    seginfo->items = NULL;
    seginfo->nitems = 0;
    if (cleaned)
    {
      if (ncleaned > 0)
      {
        int npacked;

        seginfo->seg = ginCompressPostingList(cleaned, ncleaned, oldsegsize, &npacked);
                                                                      
        if (npacked != ncleaned)
        {
          elog(ERROR, "could not fit vacuumed posting list");
        }
        seginfo->action = GIN_SEGMENT_REPLACE;
      }
      else
      {
        seginfo->seg = NULL;
        seginfo->items = NULL;
        seginfo->action = GIN_SEGMENT_DELETE;
      }
      seginfo->nitems = ncleaned;

      removedsomething = true;
    }
  }

     
                                                                    
     
                                                                           
                                                                           
                                                                          
                                                                             
                                                                      
                                                                       
             
     
                                                                             
                                                                         
                                                                             
                                                                            
                               
     
  if (removedsomething)
  {
    bool modified;

       
                                                                          
                                                                         
              
       
    modified = false;
    dlist_foreach(iter, &leaf->segments)
    {
      leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, iter.cur);

      if (seginfo->action != GIN_SEGMENT_UNMODIFIED)
      {
        modified = true;
      }
      if (modified && seginfo->action != GIN_SEGMENT_DELETE)
      {
        int segsize = SizeOfGinPostingList(seginfo->seg);
        GinPostingList *tmp = (GinPostingList *)palloc(segsize);

        memcpy(tmp, seginfo->seg, segsize);
        seginfo->seg = tmp;
      }
    }

    if (RelationNeedsWAL(indexrel))
    {
      computeLeafRecompressWALData(leaf);
    }

                               
    START_CRIT_SECTION();

    dataPlaceToPageLeafRecompress(buffer, leaf);

    MarkBufferDirty(buffer);

    if (RelationNeedsWAL(indexrel))
    {
      XLogRecPtr recptr;

      XLogBeginInsert();
      XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
      XLogRegisterBufData(0, leaf->walinfo, leaf->walinfolen);
      recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_VACUUM_DATA_LEAF_PAGE);
      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();
  }
}

   
                                                                         
                                                                       
                                                                  
   
static void
computeLeafRecompressWALData(disassembledLeaf *leaf)
{
  int nmodified = 0;
  char *walbufbegin;
  char *walbufend;
  dlist_iter iter;
  int segno;
  ginxlogRecompressDataLeaf *recompress_xlog;

                                   
  dlist_foreach(iter, &leaf->segments)
  {
    leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, iter.cur);

    if (seginfo->action != GIN_SEGMENT_UNMODIFIED)
    {
      nmodified++;
    }
  }

  walbufbegin = palloc(sizeof(ginxlogRecompressDataLeaf) + BLCKSZ +                                               
                       nmodified * 2                                                                 
  );
  walbufend = walbufbegin;

  recompress_xlog = (ginxlogRecompressDataLeaf *)walbufend;
  walbufend += sizeof(ginxlogRecompressDataLeaf);

  recompress_xlog->nactions = nmodified;

  segno = 0;
  dlist_foreach(iter, &leaf->segments)
  {
    leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, iter.cur);
    int segsize = 0;
    int datalen;
    uint8 action = seginfo->action;

    if (action == GIN_SEGMENT_UNMODIFIED)
    {
      segno++;
      continue;
    }

    if (action != GIN_SEGMENT_DELETE)
    {
      segsize = SizeOfGinPostingList(seginfo->seg);
    }

       
                                                                          
                                                                     
                
       
    if (action == GIN_SEGMENT_ADDITEMS && seginfo->nmodifieditems * sizeof(ItemPointerData) > segsize)
    {
      action = GIN_SEGMENT_REPLACE;
    }

    *((uint8 *)(walbufend++)) = segno;
    *(walbufend++) = action;

    switch (action)
    {
    case GIN_SEGMENT_DELETE:
      datalen = 0;
      break;

    case GIN_SEGMENT_ADDITEMS:
      datalen = seginfo->nmodifieditems * sizeof(ItemPointerData);
      memcpy(walbufend, &seginfo->nmodifieditems, sizeof(uint16));
      memcpy(walbufend + sizeof(uint16), seginfo->modifieditems, datalen);
      datalen += sizeof(uint16);
      break;

    case GIN_SEGMENT_INSERT:
    case GIN_SEGMENT_REPLACE:
      datalen = SHORTALIGN(segsize);
      memcpy(walbufend, seginfo->seg, segsize);
      break;

    default:
      elog(ERROR, "unexpected GIN leaf action %d", action);
    }
    walbufend += datalen;

    if (action != GIN_SEGMENT_INSERT)
    {
      segno++;
    }
  }

                                                
  leaf->walinfo = walbufbegin;
  leaf->walinfolen = walbufend - walbufbegin;
}

   
                                                                    
   
                                                                              
   
                                                                          
                                                                       
                                           
   
static void
dataPlaceToPageLeafRecompress(Buffer buf, disassembledLeaf *leaf)
{
  Page page = BufferGetPage(buf);
  char *ptr;
  int newsize;
  bool modified = false;
  dlist_iter iter;
  int segsize;

     
                                                                             
                                                                         
          
     
  if (!GinPageIsCompressed(page))
  {
    Assert(leaf->oldformat);
    GinPageSetCompressed(page);
    GinPageGetOpaque(page)->maxoff = InvalidOffsetNumber;
    modified = true;
  }

  ptr = (char *)GinDataLeafPageGetPostingList(page);
  newsize = 0;
  dlist_foreach(iter, &leaf->segments)
  {
    leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, iter.cur);

    if (seginfo->action != GIN_SEGMENT_UNMODIFIED)
    {
      modified = true;
    }

    if (seginfo->action != GIN_SEGMENT_DELETE)
    {
      segsize = SizeOfGinPostingList(seginfo->seg);

      if (modified)
      {
        memcpy(ptr, seginfo->seg, segsize);
      }

      ptr += segsize;
      newsize += segsize;
    }
  }

  Assert(newsize <= GinDataPageMaxDataSize);
  GinDataPageSetDataSize(page, newsize);
}

   
                                                                        
                                         
   
                                                                           
                                                                           
                                 
   
static void
dataPlaceToPageLeafSplit(disassembledLeaf *leaf, ItemPointerData lbound, ItemPointerData rbound, Page lpage, Page rpage)
{
  char *ptr;
  int segsize;
  int lsize;
  int rsize;
  dlist_node *node;
  dlist_node *firstright;
  leafSegmentInfo *seginfo;

                                                                       
  GinInitPage(lpage, GIN_DATA | GIN_LEAF | GIN_COMPRESSED, BLCKSZ);
  GinInitPage(rpage, GIN_DATA | GIN_LEAF | GIN_COMPRESSED, BLCKSZ);

     
                                                 
     
                                                                            
                               
     
  lsize = 0;
  ptr = (char *)GinDataLeafPageGetPostingList(lpage);
  firstright = dlist_next_node(&leaf->segments, leaf->lastleft);
  for (node = dlist_head_node(&leaf->segments); node != firstright; node = dlist_next_node(&leaf->segments, node))
  {
    seginfo = dlist_container(leafSegmentInfo, node, node);

    if (seginfo->action != GIN_SEGMENT_DELETE)
    {
      segsize = SizeOfGinPostingList(seginfo->seg);
      memcpy(ptr, seginfo->seg, segsize);
      ptr += segsize;
      lsize += segsize;
    }
  }
  Assert(lsize == leaf->lsize);
  GinDataPageSetDataSize(lpage, lsize);
  *GinDataPageGetRightBound(lpage) = lbound;

                                                   
  ptr = (char *)GinDataLeafPageGetPostingList(rpage);
  rsize = 0;
  for (node = firstright;; node = dlist_next_node(&leaf->segments, node))
  {
    seginfo = dlist_container(leafSegmentInfo, node, node);

    if (seginfo->action != GIN_SEGMENT_DELETE)
    {
      segsize = SizeOfGinPostingList(seginfo->seg);
      memcpy(ptr, seginfo->seg, segsize);
      ptr += segsize;
      rsize += segsize;
    }

    if (!dlist_has_next(&leaf->segments, node))
    {
      break;
    }
  }
  Assert(rsize == leaf->rsize);
  GinDataPageSetDataSize(rpage, rsize);
  *GinDataPageGetRightBound(rpage) = rbound;
}

   
                                                    
   
                                                                           
                                                                          
                                                                  
   
                                                                       
                                                                
   
                                                                  
   
                                                                              
                                                                            
                         
   
static GinPlaceToPageRC
dataBeginPlaceToPageInternal(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, void **ptp_workspace, Page *newlpage, Page *newrpage)
{
  Page page = BufferGetPage(buf);

                                               
  if (GinNonLeafDataPageGetFreeSpace(page) < sizeof(PostingItem))
  {
    dataSplitPageInternal(btree, buf, stack, insertdata, updateblkno, newlpage, newrpage);
    return GPTP_SPLIT;
  }

                                                   
  return GPTP_INSERT;
}

   
                                                                          
   
                                                                           
                                                                           
   
static void
dataExecPlaceToPageInternal(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, void *ptp_workspace)
{
  Page page = BufferGetPage(buf);
  OffsetNumber off = stack->off;
  PostingItem *pitem;

                                                                         
  pitem = GinDataPageGetPostingItem(page, off);
  PostingItemSetBlockNumber(pitem, updateblkno);

                    
  pitem = (PostingItem *)insertdata;
  GinDataPageAddPostingItem(page, pitem, off);

  if (RelationNeedsWAL(btree->index) && !btree->isBuild)
  {
       
                                                                        
                                                                          
                               
       
    static ginxlogInsertDataInternal data;

    data.offset = off;
    data.newitem = *pitem;

    XLogRegisterBufData(0, (char *)&data, sizeof(ginxlogInsertDataInternal));
  }
}

   
                                                       
   
                                                                           
                                                                          
                                                                  
   
                                                                       
                                                                
   
                                                                  
   
                                                                              
                                                                            
                         
   
                                                                              
                     
   
static GinPlaceToPageRC
dataBeginPlaceToPage(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, void **ptp_workspace, Page *newlpage, Page *newrpage)
{
  Page page = BufferGetPage(buf);

  Assert(GinPageIsData(page));

  if (GinPageIsLeaf(page))
  {
    return dataBeginPlaceToPageLeaf(btree, buf, stack, insertdata, ptp_workspace, newlpage, newrpage);
  }
  else
  {
    return dataBeginPlaceToPageInternal(btree, buf, stack, insertdata, updateblkno, ptp_workspace, newlpage, newrpage);
  }
}

   
                                                                          
   
                                                                           
                                                                           
   
                                                                              
                     
   
static void
dataExecPlaceToPage(GinBtree btree, Buffer buf, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, void *ptp_workspace)
{
  Page page = BufferGetPage(buf);

  if (GinPageIsLeaf(page))
  {
    dataExecPlaceToPageLeaf(btree, buf, stack, insertdata, ptp_workspace);
  }
  else
  {
    dataExecPlaceToPageInternal(btree, buf, stack, insertdata, updateblkno, ptp_workspace);
  }
}

   
                                            
   
                                                      
                                          
   
static void
dataSplitPageInternal(GinBtree btree, Buffer origbuf, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, Page *newlpage, Page *newrpage)
{
  Page oldpage = BufferGetPage(origbuf);
  OffsetNumber off = stack->off;
  int nitems = GinPageGetOpaque(oldpage)->maxoff;
  int nleftitems;
  int nrightitems;
  Size pageSize = PageGetPageSize(oldpage);
  ItemPointerData oldbound = *GinDataPageGetRightBound(oldpage);
  ItemPointer bound;
  Page lpage;
  Page rpage;
  OffsetNumber separator;
  PostingItem allitems[(BLCKSZ / sizeof(PostingItem)) + 1];

  lpage = PageGetTempPage(oldpage);
  rpage = PageGetTempPage(oldpage);
  GinInitPage(lpage, GinPageGetOpaque(oldpage)->flags, pageSize);
  GinInitPage(rpage, GinPageGetOpaque(oldpage)->flags, pageSize);

     
                                                                            
                              
     
  memcpy(allitems, GinDataPageGetPostingItem(oldpage, FirstOffsetNumber), (off - 1) * sizeof(PostingItem));

  allitems[off - 1] = *((PostingItem *)insertdata);
  memcpy(&allitems[off], GinDataPageGetPostingItem(oldpage, off), (nitems - (off - 1)) * sizeof(PostingItem));
  nitems++;

                                                      
  PostingItemSetBlockNumber(&allitems[off], updateblkno);

     
                                                                           
                                                                         
                                                     
     
  if (btree->isBuild && GinPageRightMost(oldpage))
  {
    separator = GinNonLeafDataPageGetFreeSpace(rpage) / sizeof(PostingItem);
  }
  else
  {
    separator = nitems / 2;
  }
  nleftitems = separator;
  nrightitems = nitems - separator;

  memcpy(GinDataPageGetPostingItem(lpage, FirstOffsetNumber), allitems, nleftitems * sizeof(PostingItem));
  GinPageGetOpaque(lpage)->maxoff = nleftitems;
  memcpy(GinDataPageGetPostingItem(rpage, FirstOffsetNumber), &allitems[separator], nrightitems * sizeof(PostingItem));
  GinPageGetOpaque(rpage)->maxoff = nrightitems;

     
                                                                            
     
  GinDataPageSetDataSize(lpage, nleftitems * sizeof(PostingItem));
  GinDataPageSetDataSize(rpage, nrightitems * sizeof(PostingItem));

                                        
  bound = GinDataPageGetRightBound(lpage);
  *bound = GinDataPageGetPostingItem(lpage, nleftitems)->key;

                                         
  *GinDataPageGetRightBound(rpage) = oldbound;

                                   
  *newlpage = lpage;
  *newrpage = rpage;
}

   
                                                                            
   
static void *
dataPrepareDownlink(GinBtree btree, Buffer lbuf)
{
  PostingItem *pitem = palloc(sizeof(PostingItem));
  Page lpage = BufferGetPage(lbuf);

  PostingItemSetBlockNumber(pitem, BufferGetBlockNumber(lbuf));
  pitem->key = *GinDataPageGetRightBound(lpage);

  return pitem;
}

   
                                                    
                                                  
   
void
ginDataFillRoot(GinBtree btree, Page root, BlockNumber lblkno, Page lpage, BlockNumber rblkno, Page rpage)
{
  PostingItem li, ri;

  li.key = *GinDataPageGetRightBound(lpage);
  PostingItemSetBlockNumber(&li, lblkno);
  GinDataPageAddPostingItem(root, &li, InvalidOffsetNumber);

  ri.key = *GinDataPageGetRightBound(rpage);
  PostingItemSetBlockNumber(&ri, rblkno);
  GinDataPageAddPostingItem(root, &ri, InvalidOffsetNumber);
}

                                                        

   
                                                    
   
static disassembledLeaf *
disassembleLeaf(Page page)
{
  disassembledLeaf *leaf;
  GinPostingList *seg;
  Pointer segbegin;
  Pointer segend;

  leaf = palloc0(sizeof(disassembledLeaf));
  dlist_init(&leaf->segments);

  if (GinPageIsCompressed(page))
  {
       
                                                    
       
    seg = GinDataLeafPageGetPostingList(page);
    segbegin = (Pointer)seg;
    segend = segbegin + GinDataLeafPageGetPostingListSize(page);
    while ((Pointer)seg < segend)
    {
      leafSegmentInfo *seginfo = palloc(sizeof(leafSegmentInfo));

      seginfo->action = GIN_SEGMENT_UNMODIFIED;
      seginfo->seg = seg;
      seginfo->items = NULL;
      seginfo->nitems = 0;
      dlist_push_tail(&leaf->segments, &seginfo->node);

      seg = GinNextPostingListSegment(seg);
    }
    leaf->oldformat = false;
  }
  else
  {
       
                                                                     
                                                                         
                                                                      
       
    ItemPointer uncompressed;
    int nuncompressed;
    leafSegmentInfo *seginfo;

    uncompressed = dataLeafPageGetUncompressed(page, &nuncompressed);

    if (nuncompressed > 0)
    {
      seginfo = palloc(sizeof(leafSegmentInfo));

      seginfo->action = GIN_SEGMENT_REPLACE;
      seginfo->seg = NULL;
      seginfo->items = palloc(nuncompressed * sizeof(ItemPointerData));
      memcpy(seginfo->items, uncompressed, nuncompressed * sizeof(ItemPointerData));
      seginfo->nitems = nuncompressed;

      dlist_push_tail(&leaf->segments, &seginfo->node);
    }

    leaf->oldformat = true;
  }

  return leaf;
}

   
                                        
   
                                                                          
                              
   
                                                                       
                                             
   
static bool
addItemsToLeaf(disassembledLeaf *leaf, ItemPointer newItems, int nNewItems)
{
  dlist_iter iter;
  ItemPointer nextnew = newItems;
  int newleft = nNewItems;
  bool modified = false;
  leafSegmentInfo *newseg;

     
                                                                             
                        
     
  if (dlist_is_empty(&leaf->segments))
  {
    newseg = palloc(sizeof(leafSegmentInfo));
    newseg->seg = NULL;
    newseg->items = newItems;
    newseg->nitems = nNewItems;
    newseg->action = GIN_SEGMENT_INSERT;
    dlist_push_tail(&leaf->segments, &newseg->node);
    return true;
  }

  dlist_foreach(iter, &leaf->segments)
  {
    leafSegmentInfo *cur = (leafSegmentInfo *)dlist_container(leafSegmentInfo, node, iter.cur);
    int nthis;
    ItemPointer tmpitems;
    int ntmpitems;

       
                                                         
       
    if (!dlist_has_next(&leaf->segments, iter.cur))
    {
      nthis = newleft;
    }
    else
    {
      leafSegmentInfo *next;
      ItemPointerData next_first;

      next = (leafSegmentInfo *)dlist_container(leafSegmentInfo, node, dlist_next_node(&leaf->segments, iter.cur));
      if (next->items)
      {
        next_first = next->items[0];
      }
      else
      {
        Assert(next->seg != NULL);
        next_first = next->seg->first;
      }

      nthis = 0;
      while (nthis < newleft && ginCompareItemPointers(&nextnew[nthis], &next_first) < 0)
      {
        nthis++;
      }
    }
    if (nthis == 0)
    {
      continue;
    }

                                                      
    if (!cur->items)
    {
      cur->items = ginPostingListDecode(cur->seg, &cur->nitems);
    }

       
                                                                        
                                                                        
                                                                         
       
    if (!dlist_has_next(&leaf->segments, iter.cur) && ginCompareItemPointers(&cur->items[cur->nitems - 1], &nextnew[0]) < 0 && cur->seg != NULL && SizeOfGinPostingList(cur->seg) >= GinPostingListSegmentTargetSize)
    {
      newseg = palloc(sizeof(leafSegmentInfo));
      newseg->seg = NULL;
      newseg->items = nextnew;
      newseg->nitems = nthis;
      newseg->action = GIN_SEGMENT_INSERT;
      dlist_push_tail(&leaf->segments, &newseg->node);
      modified = true;
      break;
    }

    tmpitems = ginMergeItemPointers(cur->items, cur->nitems, nextnew, nthis, &ntmpitems);
    if (ntmpitems != cur->nitems)
    {
         
                                                                      
                                                                      
                                                                      
                   
         
      if (ntmpitems == nthis + cur->nitems && cur->action == GIN_SEGMENT_UNMODIFIED)
      {
        cur->action = GIN_SEGMENT_ADDITEMS;
        cur->modifieditems = nextnew;
        cur->nmodifieditems = nthis;
      }
      else
      {
        cur->action = GIN_SEGMENT_REPLACE;
      }

      cur->items = tmpitems;
      cur->nitems = ntmpitems;
      cur->seg = NULL;
      modified = true;
    }

    nextnew += nthis;
    newleft -= nthis;
    if (newleft == 0)
    {
      break;
    }
  }

  return modified;
}

   
                                                      
   
                                                                        
                                                                            
                                                   
   
                                             
   
static bool
leafRepackItems(disassembledLeaf *leaf, ItemPointer remaining)
{
  int pgused = 0;
  bool needsplit = false;
  dlist_iter iter;
  int segsize;
  leafSegmentInfo *nextseg;
  int npacked;
  bool modified;
  dlist_node *cur_node;
  dlist_node *next_node;

  ItemPointerSetInvalid(remaining);

     
                                                                           
                      
     
  for (cur_node = dlist_head_node(&leaf->segments); cur_node != NULL; cur_node = next_node)
  {
    leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, cur_node);

    if (dlist_has_next(&leaf->segments, cur_node))
    {
      next_node = dlist_next_node(&leaf->segments, cur_node);
    }
    else
    {
      next_node = NULL;
    }

                                                 
    if (seginfo->action != GIN_SEGMENT_DELETE)
    {
      if (seginfo->seg == NULL)
      {
        if (seginfo->nitems > GinPostingListSegmentMaxSize)
        {
          npacked = 0;                                   
        }
        else
        {
          seginfo->seg = ginCompressPostingList(seginfo->items, seginfo->nitems, GinPostingListSegmentMaxSize, &npacked);
        }
        if (npacked != seginfo->nitems)
        {
             
                                                               
                                                                    
                                                                    
                                                              
             
          if (seginfo->seg)
          {
            pfree(seginfo->seg);
          }
          seginfo->seg = ginCompressPostingList(seginfo->items, seginfo->nitems, GinPostingListSegmentTargetSize, &npacked);
          if (seginfo->action != GIN_SEGMENT_INSERT)
          {
            seginfo->action = GIN_SEGMENT_REPLACE;
          }

          nextseg = palloc(sizeof(leafSegmentInfo));
          nextseg->action = GIN_SEGMENT_INSERT;
          nextseg->seg = NULL;
          nextseg->items = &seginfo->items[npacked];
          nextseg->nitems = seginfo->nitems - npacked;
          next_node = &nextseg->node;
          dlist_insert_after(cur_node, next_node);
        }
      }

         
                                                                       
         
      if (SizeOfGinPostingList(seginfo->seg) < GinPostingListSegmentMinSize && next_node)
      {
        int nmerged;

        nextseg = dlist_container(leafSegmentInfo, node, next_node);

        if (seginfo->items == NULL)
        {
          seginfo->items = ginPostingListDecode(seginfo->seg, &seginfo->nitems);
        }
        if (nextseg->items == NULL)
        {
          nextseg->items = ginPostingListDecode(nextseg->seg, &nextseg->nitems);
        }
        nextseg->items = ginMergeItemPointers(seginfo->items, seginfo->nitems, nextseg->items, nextseg->nitems, &nmerged);
        Assert(nmerged == seginfo->nitems + nextseg->nitems);
        nextseg->nitems = nmerged;
        nextseg->seg = NULL;

        nextseg->action = GIN_SEGMENT_REPLACE;
        nextseg->modifieditems = NULL;
        nextseg->nmodifieditems = 0;

        if (seginfo->action == GIN_SEGMENT_INSERT)
        {
          dlist_delete(cur_node);
          continue;
        }
        else
        {
          seginfo->action = GIN_SEGMENT_DELETE;
          seginfo->seg = NULL;
        }
      }

      seginfo->items = NULL;
      seginfo->nitems = 0;
    }

    if (seginfo->action == GIN_SEGMENT_DELETE)
    {
      continue;
    }

       
                                                                      
                                                                          
       
    segsize = SizeOfGinPostingList(seginfo->seg);
    if (pgused + segsize > GinDataPageMaxDataSize)
    {
      if (!needsplit)
      {
                                  
        Assert(pgused > 0);
        leaf->lastleft = dlist_prev_node(&leaf->segments, cur_node);
        needsplit = true;
        leaf->lsize = pgused;
        pgused = 0;
      }
      else
      {
           
                                                                      
                
           
        *remaining = seginfo->seg->first;

           
                                                               
           
        while (dlist_has_next(&leaf->segments, cur_node))
        {
          dlist_delete(dlist_next_node(&leaf->segments, cur_node));
        }
        dlist_delete(cur_node);
        break;
      }
    }

    pgused += segsize;
  }

  if (!needsplit)
  {
    leaf->lsize = pgused;
    leaf->rsize = 0;
  }
  else
  {
    leaf->rsize = pgused;
  }

  Assert(leaf->lsize <= GinDataPageMaxDataSize);
  Assert(leaf->rsize <= GinDataPageMaxDataSize);

     
                                                                         
                                                                      
                                    
     
  modified = false;
  dlist_foreach(iter, &leaf->segments)
  {
    leafSegmentInfo *seginfo = dlist_container(leafSegmentInfo, node, iter.cur);

    if (!modified && seginfo->action != GIN_SEGMENT_UNMODIFIED)
    {
      modified = true;
    }
    else if (modified && seginfo->action == GIN_SEGMENT_UNMODIFIED)
    {
      GinPostingList *tmp;

      segsize = SizeOfGinPostingList(seginfo->seg);
      tmp = palloc(segsize);
      memcpy(tmp, seginfo->seg, segsize);
      seginfo->seg = tmp;
    }
  }

  return needsplit;
}

                                                                 

   
                                                                        
                                               
   
                                                       
   
BlockNumber
createPostingTree(Relation index, ItemPointerData *items, uint32 nitems, GinStatsData *buildStats, Buffer entrybuffer)
{
  BlockNumber blkno;
  Buffer buffer;
  Page tmppage;
  Page page;
  Pointer ptr;
  int nrootitems;
  int rootsize;
  bool is_build = (buildStats != NULL);

                                                    
  tmppage = (Page)palloc(BLCKSZ);
  GinInitPage(tmppage, GIN_DATA | GIN_LEAF | GIN_COMPRESSED, BLCKSZ);
  GinPageGetOpaque(tmppage)->rightlink = InvalidBlockNumber;

     
                                                                            
                                              
     
  nrootitems = 0;
  rootsize = 0;
  ptr = (Pointer)GinDataLeafPageGetPostingList(tmppage);
  while (nrootitems < nitems)
  {
    GinPostingList *segment;
    int npacked;
    int segsize;

    segment = ginCompressPostingList(&items[nrootitems], nitems - nrootitems, GinPostingListSegmentMaxSize, &npacked);
    segsize = SizeOfGinPostingList(segment);
    if (rootsize + segsize > GinDataPageMaxDataSize)
    {
      break;
    }

    memcpy(ptr, segment, segsize);
    ptr += segsize;
    rootsize += segsize;
    nrootitems += npacked;
    pfree(segment);
  }
  GinDataPageSetDataSize(tmppage, rootsize);

     
                                                                          
     
  buffer = GinNewBuffer(index);
  page = BufferGetPage(buffer);
  blkno = BufferGetBlockNumber(buffer);

     
                                                                           
                                
     
  PredicateLockPageSplit(index, BufferGetBlockNumber(entrybuffer), blkno);

  START_CRIT_SECTION();

  PageRestoreTempPage(tmppage, page);
  MarkBufferDirty(buffer);

  if (RelationNeedsWAL(index) && !is_build)
  {
    XLogRecPtr recptr;
    ginxlogCreatePostingTree data;

    data.size = rootsize;

    XLogBeginInsert();
    XLogRegisterData((char *)&data, sizeof(ginxlogCreatePostingTree));

    XLogRegisterData((char *)GinDataLeafPageGetPostingList(page), rootsize);
    XLogRegisterBuffer(0, buffer, REGBUF_WILL_INIT);

    recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_CREATE_PTREE);
    PageSetLSN(page, recptr);
  }

  UnlockReleaseBuffer(buffer);

  END_CRIT_SECTION();

                                                           
  if (buildStats)
  {
    buildStats->nDataPages++;
  }

  elog(DEBUG2, "created GIN posting tree with %d items", nrootitems);

     
                                                               
     
  if (nitems > nrootitems)
  {
    ginInsertItemPointers(index, blkno, items + nrootitems, nitems - nrootitems, buildStats);
  }

  return blkno;
}

static void
ginPrepareDataScan(GinBtree btree, Relation index, BlockNumber rootBlkno)
{
  memset(btree, 0, sizeof(GinBtreeData));

  btree->index = index;
  btree->rootBlkno = rootBlkno;

  btree->findChildPage = dataLocateItem;
  btree->getLeftMostChild = dataGetLeftMostPage;
  btree->isMoveRight = dataIsMoveRight;
  btree->findItem = NULL;
  btree->findChildPtr = dataFindChildPtr;
  btree->beginPlaceToPage = dataBeginPlaceToPage;
  btree->execPlaceToPage = dataExecPlaceToPage;
  btree->fillRoot = ginDataFillRoot;
  btree->prepareDownlink = dataPrepareDownlink;

  btree->isData = true;
  btree->fullScan = false;
  btree->isBuild = false;
}

   
                                                                             
   
void
ginInsertItemPointers(Relation index, BlockNumber rootBlkno, ItemPointerData *items, uint32 nitem, GinStatsData *buildStats)
{
  GinBtreeData btree;
  GinBtreeDataLeafInsertData insertdata;
  GinBtreeStack *stack;

  ginPrepareDataScan(&btree, index, rootBlkno);
  btree.isBuild = (buildStats != NULL);
  insertdata.items = items;
  insertdata.nitem = nitem;
  insertdata.curitem = 0;

  while (insertdata.curitem < insertdata.nitem)
  {
                                                                    
    btree.itemptr = insertdata.items[insertdata.curitem];
    stack = ginFindLeafPage(&btree, false, true, NULL);

    ginInsertValue(&btree, stack, &insertdata, buildStats);
  }
}

   
                                        
   
GinBtreeStack *
ginScanBeginPostingTree(GinBtree btree, Relation index, BlockNumber rootBlkno, Snapshot snapshot)
{
  GinBtreeStack *stack;

  ginPrepareDataScan(btree, index, rootBlkno);

  btree->fullScan = true;

  stack = ginFindLeafPage(btree, true, false, snapshot);

  return stack;
}
