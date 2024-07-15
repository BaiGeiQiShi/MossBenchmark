                                                                            
   
                 
                                        
   
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/spgist_private.h"
#include "access/spgxlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "utils/rel.h"

   
                                                                           
                                                                             
                                                                         
                                                                             
                                                        
   
typedef struct SPPageDesc
{
  BlockNumber blkno;                                            
  Buffer buffer;                                                   
  Page page;                                                
  OffsetNumber offnum;                                              
  int node;                                                       
} SPPageDesc;

   
                                                                        
                                                                       
                    
   
void
spgUpdateNodeLink(SpGistInnerTuple tup, int nodeN, BlockNumber blkno, OffsetNumber offset)
{
  int i;
  SpGistNodeTuple node;

  SGITITERATE(tup, i, node)
  {
    if (i == nodeN)
    {
      ItemPointerSet(&node->t_tid, blkno, offset);
      return;
    }
  }

  elog(ERROR, "failed to find requested node %d in SPGiST inner tuple", nodeN);
}

   
                                                                            
                                                                             
                                                        
   
                                                                           
                              
   
static SpGistInnerTuple
addNode(SpGistState *state, SpGistInnerTuple tuple, Datum label, int offset)
{
  SpGistNodeTuple node, *nodes;
  int i;

                                            
  if (offset < 0)
  {
    offset = tuple->nNodes;
  }
  else if (offset > tuple->nNodes)
  {
    elog(ERROR, "invalid offset for adding node to SPGiST inner tuple");
  }

  nodes = palloc(sizeof(SpGistNodeTuple) * (tuple->nNodes + 1));
  SGITITERATE(tuple, i, node)
  {
    if (i < offset)
    {
      nodes[i] = node;
    }
    else
    {
      nodes[i + 1] = node;
    }
  }

  nodes[offset] = spgFormNodeTuple(state, label, false);

  return spgFormInnerTuple(state, (tuple->prefixSize > 0), SGITDATUM(tuple, state), tuple->nNodes + 1, nodes);
}

                                                
static int
cmpOffsetNumbers(const void *a, const void *b)
{
  if (*(const OffsetNumber *)a == *(const OffsetNumber *)b)
  {
    return 0;
  }
  return (*(const OffsetNumber *)a > *(const OffsetNumber *)b) ? 1 : -1;
}

   
                                                                               
   
                                                                           
                                                                               
                                                                            
                                                       
   
                                                                          
                                                                      
                                                                       
                   
   
void
spgPageIndexMultiDelete(SpGistState *state, Page page, OffsetNumber *itemnos, int nitems, int firststate, int reststate, BlockNumber blkno, OffsetNumber offnum)
{
  OffsetNumber firstItem;
  OffsetNumber sortednos[MaxIndexTuplesPerPage];
  SpGistDeadTuple tuple = NULL;
  int i;

  if (nitems == 0)
  {
    return;                    
  }

     
                                                                            
                                                                          
                                                                        
                                                                         
                                       
     
  memcpy(sortednos, itemnos, sizeof(OffsetNumber) * nitems);
  if (nitems > 1)
  {
    qsort(sortednos, nitems, sizeof(OffsetNumber), cmpOffsetNumbers);
  }

  PageIndexMultiDelete(page, sortednos, nitems);

  firstItem = itemnos[0];

  for (i = 0; i < nitems; i++)
  {
    OffsetNumber itemno = sortednos[i];
    int tupstate;

    tupstate = (itemno == firstItem) ? firststate : reststate;
    if (tuple == NULL || tuple->tupstate != tupstate)
    {
      tuple = spgFormDeadTuple(state, tupstate, blkno, offnum);
    }

    if (PageAddItem(page, (Item)tuple, tuple->size, itemno, false, false) != itemno)
    {
      elog(ERROR, "failed to add item of size %u to SPGiST index page", tuple->size);
    }

    if (tupstate == SPGIST_REDIRECT)
    {
      SpGistPageGetOpaque(page)->nRedirection++;
    }
    else if (tupstate == SPGIST_PLACEHOLDER)
    {
      SpGistPageGetOpaque(page)->nPlaceholder++;
    }
  }
}

   
                                                                        
                                                                         
                
   
static void
saveNodeLink(Relation index, SPPageDesc *parent, BlockNumber blkno, OffsetNumber offnum)
{
  SpGistInnerTuple innerTuple;

  innerTuple = (SpGistInnerTuple)PageGetItem(parent->page, PageGetItemId(parent->page, parent->offnum));

  spgUpdateNodeLink(innerTuple, parent->node, blkno, offnum);

  MarkBufferDirty(parent->buffer);
}

   
                                                                          
   
static void
addLeafTuple(Relation index, SpGistState *state, SpGistLeafTuple leafTuple, SPPageDesc *current, SPPageDesc *parent, bool isNulls, bool isNew)
{
  spgxlogAddLeaf xlrec;

  xlrec.newPage = isNew;
  xlrec.storesNulls = isNulls;

                                            
  xlrec.offnumLeaf = InvalidOffsetNumber;
  xlrec.offnumHeadLeaf = InvalidOffsetNumber;
  xlrec.offnumParent = InvalidOffsetNumber;
  xlrec.nodeI = 0;

  START_CRIT_SECTION();

  if (current->offnum == InvalidOffsetNumber || SpGistBlockIsRoot(current->blkno))
  {
                                      
    leafTuple->nextOffset = InvalidOffsetNumber;
    current->offnum = SpGistPageAddNewItem(state, current->page, (Item)leafTuple, leafTuple->size, NULL, false);

    xlrec.offnumLeaf = current->offnum;

                                              
    if (parent->buffer != InvalidBuffer)
    {
      xlrec.offnumParent = parent->offnum;
      xlrec.nodeI = parent->node;

      saveNodeLink(index, parent, current->blkno, current->offnum);
    }
  }
  else
  {
       
                                                                          
                                                                         
                                                             
       
                                                                           
                                                                
       
    SpGistLeafTuple head;
    OffsetNumber offnum;

    head = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, current->offnum));
    if (head->tupstate == SPGIST_LIVE)
    {
      leafTuple->nextOffset = head->nextOffset;
      offnum = SpGistPageAddNewItem(state, current->page, (Item)leafTuple, leafTuple->size, NULL, false);

         
                                                                       
                                    
         
      head = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, current->offnum));
      head->nextOffset = offnum;

      xlrec.offnumLeaf = offnum;
      xlrec.offnumHeadLeaf = current->offnum;
    }
    else if (head->tupstate == SPGIST_DEAD)
    {
      leafTuple->nextOffset = InvalidOffsetNumber;
      PageIndexTupleDelete(current->page, current->offnum);
      if (PageAddItem(current->page, (Item)leafTuple, leafTuple->size, current->offnum, false, false) != current->offnum)
      {
        elog(ERROR, "failed to add item of size %u to SPGiST index page", leafTuple->size);
      }

                                                               
      xlrec.offnumLeaf = current->offnum;
      xlrec.offnumHeadLeaf = current->offnum;
    }
    else
    {
      elog(ERROR, "unexpected SPGiST tuple state: %d", head->tupstate);
    }
  }

  MarkBufferDirty(current->buffer);

  if (RelationNeedsWAL(index) && !state->isBuild)
  {
    XLogRecPtr recptr;
    int flags;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, sizeof(xlrec));
    XLogRegisterData((char *)leafTuple, leafTuple->size);

    flags = REGBUF_STANDARD;
    if (xlrec.newPage)
    {
      flags |= REGBUF_WILL_INIT;
    }
    XLogRegisterBuffer(0, current->buffer, flags);
    if (xlrec.offnumParent != InvalidOffsetNumber)
    {
      XLogRegisterBuffer(1, parent->buffer, REGBUF_STANDARD);
    }

    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_ADD_LEAF);

    PageSetLSN(current->page, recptr);

                                                      
    if (xlrec.offnumParent != InvalidOffsetNumber)
    {
      PageSetLSN(parent->page, recptr);
    }
  }

  END_CRIT_SECTION();
}

   
                                                                           
                                                                             
           
   
                                                                           
                                                                         
                                                                     
                                                                           
   
static int
checkSplitConditions(Relation index, SpGistState *state, SPPageDesc *current, int *nToSplit)
{
  int i, n = 0, totalSize = 0;

  if (SpGistBlockIsRoot(current->blkno))
  {
                                                 
    *nToSplit = BLCKSZ;
    return BLCKSZ;
  }

  i = current->offnum;
  while (i != InvalidOffsetNumber)
  {
    SpGistLeafTuple it;

    Assert(i >= FirstOffsetNumber && i <= PageGetMaxOffsetNumber(current->page));
    it = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, i));
    if (it->tupstate == SPGIST_LIVE)
    {
      n++;
      totalSize += it->size + sizeof(ItemIdData);
    }
    else if (it->tupstate == SPGIST_DEAD)
    {
                                                              
      Assert(i == current->offnum);
      Assert(it->nextOffset == InvalidOffsetNumber);
                                                                       
    }
    else
    {
      elog(ERROR, "unexpected SPGiST tuple state: %d", it->tupstate);
    }

    i = it->nextOffset;
  }

  *nToSplit = n;

  return totalSize;
}

   
                                                                               
                                                                        
                                                                         
                                                                         
                                                      
   
static void
moveLeafs(Relation index, SpGistState *state, SPPageDesc *current, SPPageDesc *parent, SpGistLeafTuple newLeafTuple, bool isNulls)
{
  int i, nDelete, nInsert, size;
  Buffer nbuf;
  Page npage;
  SpGistLeafTuple it;
  OffsetNumber r = InvalidOffsetNumber, startOffset = InvalidOffsetNumber;
  bool replaceDead = false;
  OffsetNumber *toDelete;
  OffsetNumber *toInsert;
  BlockNumber nblkno;
  spgxlogMoveLeafs xlrec;
  char *leafdata, *leafptr;

                                      
  Assert(parent->buffer != InvalidBuffer);
  Assert(parent->buffer != current->buffer);

                                                                    
  i = PageGetMaxOffsetNumber(current->page);
  toDelete = (OffsetNumber *)palloc(sizeof(OffsetNumber) * i);
  toInsert = (OffsetNumber *)palloc(sizeof(OffsetNumber) * (i + 1));

  size = newLeafTuple->size + sizeof(ItemIdData);

  nDelete = 0;
  i = current->offnum;
  while (i != InvalidOffsetNumber)
  {
    SpGistLeafTuple it;

    Assert(i >= FirstOffsetNumber && i <= PageGetMaxOffsetNumber(current->page));
    it = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, i));

    if (it->tupstate == SPGIST_LIVE)
    {
      toDelete[nDelete] = i;
      size += it->size + sizeof(ItemIdData);
      nDelete++;
    }
    else if (it->tupstate == SPGIST_DEAD)
    {
                                                              
      Assert(i == current->offnum);
      Assert(it->nextOffset == InvalidOffsetNumber);
                                                               
      toDelete[nDelete] = i;
      nDelete++;
      replaceDead = true;
    }
    else
    {
      elog(ERROR, "unexpected SPGiST tuple state: %d", it->tupstate);
    }

    i = it->nextOffset;
  }

                                            
  nbuf = SpGistGetBuffer(index, GBUF_LEAF | (isNulls ? GBUF_NULLS : 0), size, &xlrec.newPage);
  npage = BufferGetPage(nbuf);
  nblkno = BufferGetBlockNumber(nbuf);
  Assert(nblkno != current->blkno);

  leafdata = leafptr = palloc(size);

  START_CRIT_SECTION();

                                                                
  nInsert = 0;
  if (!replaceDead)
  {
    for (i = 0; i < nDelete; i++)
    {
      it = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, toDelete[i]));
      Assert(it->tupstate == SPGIST_LIVE);

         
                                                                         
                                                                    
                                                             
         
      it->nextOffset = r;

      r = SpGistPageAddNewItem(state, npage, (Item)it, it->size, &startOffset, false);

      toInsert[nInsert] = r;
      nInsert++;

                                                     
      memcpy(leafptr, it, it->size);
      leafptr += it->size;
    }
  }

                                 
  newLeafTuple->nextOffset = r;
  r = SpGistPageAddNewItem(state, npage, (Item)newLeafTuple, newLeafTuple->size, &startOffset, false);
  toInsert[nInsert] = r;
  nInsert++;
  memcpy(leafptr, newLeafTuple, newLeafTuple->size);
  leafptr += newLeafTuple->size;

     
                                                                             
                                                                             
                                                               
     
  spgPageIndexMultiDelete(state, current->page, toDelete, nDelete, state->isBuild ? SPGIST_PLACEHOLDER : SPGIST_REDIRECT, SPGIST_PLACEHOLDER, nblkno, r);

                                                           
  saveNodeLink(index, parent, nblkno, r);

                               
  MarkBufferDirty(current->buffer);
  MarkBufferDirty(nbuf);

  if (RelationNeedsWAL(index) && !state->isBuild)
  {
    XLogRecPtr recptr;

                          
    STORE_STATE(state, xlrec.stateSrc);

    xlrec.nMoves = nDelete;
    xlrec.replaceDead = replaceDead;
    xlrec.storesNulls = isNulls;

    xlrec.offnumParent = parent->offnum;
    xlrec.nodeI = parent->node;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfSpgxlogMoveLeafs);
    XLogRegisterData((char *)toDelete, sizeof(OffsetNumber) * nDelete);
    XLogRegisterData((char *)toInsert, sizeof(OffsetNumber) * nInsert);
    XLogRegisterData((char *)leafdata, leafptr - leafdata);

    XLogRegisterBuffer(0, current->buffer, REGBUF_STANDARD);
    XLogRegisterBuffer(1, nbuf, REGBUF_STANDARD | (xlrec.newPage ? REGBUF_WILL_INIT : 0));
    XLogRegisterBuffer(2, parent->buffer, REGBUF_STANDARD);

    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_MOVE_LEAFS);

    PageSetLSN(current->page, recptr);
    PageSetLSN(npage, recptr);
    PageSetLSN(parent->page, recptr);
  }

  END_CRIT_SECTION();

                                                            
  SpGistSetLastUsedPage(index, nbuf);
  UnlockReleaseBuffer(nbuf);
}

   
                                                                            
   
                                                                       
                                                                        
                 
   
static void
setRedirectionTuple(SPPageDesc *current, OffsetNumber position, BlockNumber blkno, OffsetNumber offnum)
{
  SpGistDeadTuple dt;

  dt = (SpGistDeadTuple)PageGetItem(current->page, PageGetItemId(current->page, position));
  Assert(dt->tupstate == SPGIST_REDIRECT);
  Assert(ItemPointerGetBlockNumber(&dt->pointer) == SPGIST_METAPAGE_BLKNO);
  ItemPointerSet(&dt->pointer, blkno, offnum);
}

   
                                                                            
                                                      
                                                                           
                                                                          
   
                                                                          
   
                                                                         
                                                                                
                                                                             
                                                                            
                                                                            
                                                                       
                                                                               
                                                                         
                                                             
   
static bool
checkAllTheSame(spgPickSplitIn *in, spgPickSplitOut *out, bool tooBig, bool *includeNew)
{
  int theNode;
  int limit;
  int i;

                                                                
  *includeNew = true;

                                                                        
  if (in->nTuples <= 1)
  {
    return false;
  }

                                                                          
  limit = tooBig ? in->nTuples - 1 : in->nTuples;

                                                       
  theNode = out->mapTuplesToNodes[0];
  for (i = 1; i < limit; i++)
  {
    if (out->mapTuplesToNodes[i] != theNode)
    {
      return false;
    }
  }

                                                            

                                                                          
  if (tooBig && out->mapTuplesToNodes[in->nTuples - 1] != theNode)
  {
    *includeNew = false;
  }

  out->nNodes = 8;                                      

                                                                        
  for (i = 0; i < in->nTuples; i++)
  {
    out->mapTuplesToNodes[i] = i % out->nNodes;
  }

                                                                          
  if (out->nodeLabels)
  {
    Datum theLabel = out->nodeLabels[theNode];

    out->nodeLabels = (Datum *)palloc(sizeof(Datum) * out->nNodes);
    for (i = 0; i < out->nNodes; i++)
    {
      out->nodeLabels[i] = theLabel;
    }
  }

                                                                     

  return true;
}

   
                                                                               
                                                                        
                             
   
                                                                           
                                                                           
                                                                      
                                                                       
                                                                              
   
                                                                 
   
                                                                               
                                                                              
                                                                            
                                                                          
                                                                         
                                                                             
                                                                            
                                                                            
                                                                              
                                                                        
                                      
   
static bool
doPickSplit(Relation index, SpGistState *state, SPPageDesc *current, SPPageDesc *parent, SpGistLeafTuple newLeafTuple, int level, bool isNulls, bool isNew)
{
  bool insertedNew = false;
  spgPickSplitIn in;
  spgPickSplitOut out;
  FmgrInfo *procinfo;
  bool includeNew;
  int i, max, n;
  SpGistInnerTuple innerTuple;
  SpGistNodeTuple node, *nodes;
  Buffer newInnerBuffer, newLeafBuffer;
  ItemPointerData *heapPtrs;
  uint8 *leafPageSelect;
  int *leafSizes;
  OffsetNumber *toDelete;
  OffsetNumber *toInsert;
  OffsetNumber redirectTuplePos = InvalidOffsetNumber;
  OffsetNumber startOffsets[2];
  SpGistLeafTuple *newLeafs;
  int spaceToDelete;
  int currentFreeSpace;
  int totalLeafSizes;
  bool allTheSame;
  spgxlogPickSplit xlrec;
  char *leafdata, *leafptr;
  SPPageDesc saveCurrent;
  int nToDelete, nToInsert, maxToInclude;

  in.level = level;

     
                                                                
     
  max = PageGetMaxOffsetNumber(current->page);
  n = max + 1;
  in.datums = (Datum *)palloc(sizeof(Datum) * n);
  heapPtrs = (ItemPointerData *)palloc(sizeof(ItemPointerData) * n);
  toDelete = (OffsetNumber *)palloc(sizeof(OffsetNumber) * n);
  toInsert = (OffsetNumber *)palloc(sizeof(OffsetNumber) * n);
  newLeafs = (SpGistLeafTuple *)palloc(sizeof(SpGistLeafTuple) * n);
  leafPageSelect = (uint8 *)palloc(sizeof(uint8) * n);

  STORE_STATE(state, xlrec.stateSrc);

     
                                                                         
                                                                         
                                                                       
                                                                
     
                                                                            
                                                                         
                                                                             
                                                                             
                                                                           
                                                               
     
  nToInsert = 0;
  nToDelete = 0;
  spaceToDelete = 0;
  if (SpGistBlockIsRoot(current->blkno))
  {
       
                                                                        
                                                                           
                                                     
       
    for (i = FirstOffsetNumber; i <= max; i++)
    {
      SpGistLeafTuple it;

      it = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, i));
      if (it->tupstate == SPGIST_LIVE)
      {
        in.datums[nToInsert] = SGLTDATUM(it, state);
        heapPtrs[nToInsert] = it->heapPtr;
        nToInsert++;
        toDelete[nToDelete] = i;
        nToDelete++;
                                                                      
        spaceToDelete += it->size + sizeof(ItemIdData);
      }
      else                                    
      {
        elog(ERROR, "unexpected SPGiST tuple state: %d", it->tupstate);
      }
    }
  }
  else
  {
                                                                
    i = current->offnum;
    while (i != InvalidOffsetNumber)
    {
      SpGistLeafTuple it;

      Assert(i >= FirstOffsetNumber && i <= max);
      it = (SpGistLeafTuple)PageGetItem(current->page, PageGetItemId(current->page, i));
      if (it->tupstate == SPGIST_LIVE)
      {
        in.datums[nToInsert] = SGLTDATUM(it, state);
        heapPtrs[nToInsert] = it->heapPtr;
        nToInsert++;
        toDelete[nToDelete] = i;
        nToDelete++;
                                                                  
        Assert(it->size >= SGDTSIZE);
        spaceToDelete += it->size - SGDTSIZE;
      }
      else if (it->tupstate == SPGIST_DEAD)
      {
                                                                
        Assert(i == current->offnum);
        Assert(it->nextOffset == InvalidOffsetNumber);
        toDelete[nToDelete] = i;
        nToDelete++;
                                                           
      }
      else
      {
        elog(ERROR, "unexpected SPGiST tuple state: %d", it->tupstate);
      }

      i = it->nextOffset;
    }
  }
  in.nTuples = nToInsert;

     
                                                                           
                                                                          
                                                                             
                                                                    
     
  in.datums[in.nTuples] = SGLTDATUM(newLeafTuple, state);
  heapPtrs[in.nTuples] = newLeafTuple->heapPtr;
  in.nTuples++;

  memset(&out, 0, sizeof(out));

  if (!isNulls)
  {
       
                                                
       
    procinfo = index_getprocinfo(index, 1, SPGIST_PICKSPLIT_PROC);
    FunctionCall2Coll(procinfo, index->rd_indcollation[0], PointerGetDatum(&in), PointerGetDatum(&out));

       
                                                                 
       
    totalLeafSizes = 0;
    for (i = 0; i < in.nTuples; i++)
    {
      newLeafs[i] = spgFormLeafTuple(state, heapPtrs + i, out.leafTupleDatums[i], false);
      totalLeafSizes += newLeafs[i]->size + sizeof(ItemIdData);
    }
  }
  else
  {
       
                                                               
                                                                     
       
    out.hasPrefix = false;
    out.nNodes = 1;
    out.nodeLabels = NULL;
    out.mapTuplesToNodes = palloc0(sizeof(int) * in.nTuples);

       
                                                                 
       
    totalLeafSizes = 0;
    for (i = 0; i < in.nTuples; i++)
    {
      newLeafs[i] = spgFormLeafTuple(state, heapPtrs + i, (Datum)0, true);
      totalLeafSizes += newLeafs[i]->size + sizeof(ItemIdData);
    }
  }

     
                                                                           
                                                                             
                                                               
                                                                            
                      
     
  allTheSame = checkAllTheSame(&in, &out, totalLeafSizes > SPGIST_PAGE_CAPACITY, &includeNew);

     
                                                                     
                              
     
  if (includeNew)
  {
    maxToInclude = in.nTuples;
  }
  else
  {
    maxToInclude = in.nTuples - 1;
    totalLeafSizes -= newLeafs[in.nTuples - 1]->size + sizeof(ItemIdData);
  }

     
                                                                         
                                                                           
                                                       
     
  nodes = (SpGistNodeTuple *)palloc(sizeof(SpGistNodeTuple) * out.nNodes);
  leafSizes = (int *)palloc0(sizeof(int) * out.nNodes);

     
                                                      
     
  for (i = 0; i < out.nNodes; i++)
  {
    Datum label = (Datum)0;
    bool labelisnull = (out.nodeLabels == NULL);

    if (!labelisnull)
    {
      label = out.nodeLabels[i];
    }
    nodes[i] = spgFormNodeTuple(state, label, labelisnull);
  }
  innerTuple = spgFormInnerTuple(state, out.hasPrefix, out.prefixDatum, out.nNodes, nodes);
  innerTuple->allTheSame = allTheSame;

     
                                                                             
                                          
     
  SGITITERATE(innerTuple, i, node) { nodes[i] = node; }

     
                                                                            
     
  for (i = 0; i < maxToInclude; i++)
  {
    n = out.mapTuplesToNodes[i];
    if (n < 0 || n >= out.nNodes)
    {
      elog(ERROR, "inconsistent result of SPGiST picksplit function");
    }
    leafSizes[n] += newLeafs[i]->size + sizeof(ItemIdData);
  }

     
                                                                            
                                                                             
                                                                         
                                                                          
                                                                         
                                                                      
                                                           
     
  xlrec.initInner = false;
  if (parent->buffer != InvalidBuffer && !SpGistBlockIsRoot(parent->blkno) && (SpGistPageGetFreeSpace(parent->page, 1) >= innerTuple->size + sizeof(ItemIdData)))
  {
                                                 
    newInnerBuffer = parent->buffer;
  }
  else if (parent->buffer != InvalidBuffer)
  {
                                                                 
    newInnerBuffer = SpGistGetBuffer(index, GBUF_INNER_PARITY(parent->blkno + 1) | (isNulls ? GBUF_NULLS : 0), innerTuple->size + sizeof(ItemIdData), &xlrec.initInner);
  }
  else
  {
                                                              
    newInnerBuffer = InvalidBuffer;
  }

     
                                                                             
                                                                    
                                                                       
                                                                     
                                                                          
                                                                
     
                                                                            
                                                                           
                                                                          
                                                                         
            
     
                                                                            
                                                                            
                                 
     
  if (!SpGistBlockIsRoot(current->blkno))
  {
    currentFreeSpace = PageGetExactFreeSpace(current->page) + spaceToDelete;
  }
  else
  {
    currentFreeSpace = 0;                                              
  }

  xlrec.initDest = false;

  if (totalLeafSizes <= currentFreeSpace)
  {
                                                      
    newLeafBuffer = InvalidBuffer;
                                                                   
    if (includeNew)
    {
      nToInsert++;
      insertedNew = true;
    }
    for (i = 0; i < nToInsert; i++)
    {
      leafPageSelect[i] = 0;                             
    }
  }
  else if (in.nTuples == 1 && totalLeafSizes > SPGIST_PAGE_CAPACITY)
  {
       
                                                                        
                                                                         
                                            
       
    newLeafBuffer = InvalidBuffer;
    Assert(includeNew);
    Assert(nToInsert == 0);
  }
  else
  {
                                        
    uint8 *nodePageSelect;
    int curspace;
    int newspace;

    newLeafBuffer = SpGistGetBuffer(index, GBUF_LEAF | (isNulls ? GBUF_NULLS : 0), Min(totalLeafSizes, SPGIST_PAGE_CAPACITY), &xlrec.initDest);

       
                                                                         
                                                                       
                                                    
       
    nodePageSelect = (uint8 *)palloc(sizeof(uint8) * out.nNodes);

    curspace = currentFreeSpace;
    newspace = PageGetExactFreeSpace(BufferGetPage(newLeafBuffer));
    for (i = 0; i < out.nNodes; i++)
    {
      if (leafSizes[i] <= curspace)
      {
        nodePageSelect[i] = 0;                             
        curspace -= leafSizes[i];
      }
      else
      {
        nodePageSelect[i] = 1;                              
        newspace -= leafSizes[i];
      }
    }
    if (curspace >= 0 && newspace >= 0)
    {
                                                                       
      if (includeNew)
      {
        nToInsert++;
        insertedNew = true;
      }
    }
    else if (includeNew)
    {
                                                             
      int nodeOfNewTuple = out.mapTuplesToNodes[in.nTuples - 1];

      leafSizes[nodeOfNewTuple] -= newLeafs[in.nTuples - 1]->size + sizeof(ItemIdData);

                                                                     
      curspace = currentFreeSpace;
      newspace = PageGetExactFreeSpace(BufferGetPage(newLeafBuffer));
      for (i = 0; i < out.nNodes; i++)
      {
        if (leafSizes[i] <= curspace)
        {
          nodePageSelect[i] = 0;                             
          curspace -= leafSizes[i];
        }
        else
        {
          nodePageSelect[i] = 1;                              
          newspace -= leafSizes[i];
        }
      }
      if (curspace < 0 || newspace < 0)
      {
        elog(ERROR, "failed to divide leaf tuple groups across pages");
      }
    }
    else
    {
                                                                       
      elog(ERROR, "failed to divide leaf tuple groups across pages");
    }
                                                                    
    for (i = 0; i < nToInsert; i++)
    {
      n = out.mapTuplesToNodes[i];
      leafPageSelect[i] = nodePageSelect[n];
    }
  }

                                  
  xlrec.nDelete = 0;
  xlrec.initSrc = isNew;
  xlrec.storesNulls = isNulls;
  xlrec.isRootSplit = SpGistBlockIsRoot(current->blkno);

  leafdata = leafptr = (char *)palloc(totalLeafSizes);

                                                            
  START_CRIT_SECTION();

     
                                                                             
                                                                           
                                                                            
     
  if (!SpGistBlockIsRoot(current->blkno))
  {
       
                                                                      
                                                                           
                                                                
       
    if (state->isBuild && nToDelete + SpGistPageGetOpaque(current->page)->nPlaceholder == PageGetMaxOffsetNumber(current->page))
    {
      SpGistInitBuffer(current->buffer, SPGIST_LEAF | (isNulls ? SPGIST_NULLS : 0));
      xlrec.initSrc = true;
    }
    else if (isNew)
    {
                                                                    
      Assert(nToDelete == 0);
    }
    else
    {
      xlrec.nDelete = nToDelete;

      if (!state->isBuild)
      {
           
                                                                     
                                                                      
                                                                       
                                                            
           
        if (nToDelete > 0)
        {
          redirectTuplePos = toDelete[0];
        }
        spgPageIndexMultiDelete(state, current->page, toDelete, nToDelete, SPGIST_REDIRECT, SPGIST_PLACEHOLDER, SPGIST_METAPAGE_BLKNO, FirstOffsetNumber);
      }
      else
      {
           
                                                                      
                                                   
           
        spgPageIndexMultiDelete(state, current->page, toDelete, nToDelete, SPGIST_PLACEHOLDER, SPGIST_PLACEHOLDER, InvalidBlockNumber, InvalidOffsetNumber);
      }
    }
  }

     
                                                                           
            
     
  startOffsets[0] = startOffsets[1] = InvalidOffsetNumber;
  for (i = 0; i < nToInsert; i++)
  {
    SpGistLeafTuple it = newLeafs[i];
    Buffer leafBuffer;
    BlockNumber leafBlock;
    OffsetNumber newoffset;

                                    
    leafBuffer = leafPageSelect[i] ? newLeafBuffer : current->buffer;
    leafBlock = BufferGetBlockNumber(leafBuffer);

                                                    
    n = out.mapTuplesToNodes[i];

    if (ItemPointerIsValid(&nodes[n]->t_tid))
    {
      Assert(ItemPointerGetBlockNumber(&nodes[n]->t_tid) == leafBlock);
      it->nextOffset = ItemPointerGetOffsetNumber(&nodes[n]->t_tid);
    }
    else
    {
      it->nextOffset = InvalidOffsetNumber;
    }

                           
    newoffset = SpGistPageAddNewItem(state, BufferGetPage(leafBuffer), (Item)it, it->size, &startOffsets[leafPageSelect[i]], false);
    toInsert[i] = newoffset;

                                            
    ItemPointerSet(&nodes[n]->t_tid, leafBlock, newoffset);

                                            
    memcpy(leafptr, newLeafs[i], newLeafs[i]->size);
    leafptr += newLeafs[i]->size;
  }

     
                                                                            
                                                                     
                   
     
  if (newLeafBuffer != InvalidBuffer)
  {
    MarkBufferDirty(newLeafBuffer);
  }

                                                                      
  saveCurrent = *current;

     
                              
     
  if (newInnerBuffer == parent->buffer && newInnerBuffer != InvalidBuffer)
  {
       
                                           
       
    Assert(current->buffer != parent->buffer);

                                                  
    current->blkno = parent->blkno;
    current->buffer = parent->buffer;
    current->page = parent->page;
    xlrec.offnumInner = current->offnum = SpGistPageAddNewItem(state, current->page, (Item)innerTuple, innerTuple->size, NULL, false);

       
                                                          
       
    xlrec.innerIsParent = true;
    xlrec.offnumParent = parent->offnum;
    xlrec.nodeI = parent->node;
    saveNodeLink(index, parent, current->blkno, current->offnum);

       
                                                       
       
    if (redirectTuplePos != InvalidOffsetNumber)
    {
      setRedirectionTuple(&saveCurrent, redirectTuplePos, current->blkno, current->offnum);
    }

                                                          
    MarkBufferDirty(saveCurrent.buffer);
  }
  else if (parent->buffer != InvalidBuffer)
  {
       
                                                    
       
    Assert(newInnerBuffer != InvalidBuffer);

                                                  
    current->buffer = newInnerBuffer;
    current->blkno = BufferGetBlockNumber(current->buffer);
    current->page = BufferGetPage(current->buffer);
    xlrec.offnumInner = current->offnum = SpGistPageAddNewItem(state, current->page, (Item)innerTuple, innerTuple->size, NULL, false);

                                                          
    MarkBufferDirty(current->buffer);

       
                                                          
       
    xlrec.innerIsParent = (parent->buffer == current->buffer);
    xlrec.offnumParent = parent->offnum;
    xlrec.nodeI = parent->node;
    saveNodeLink(index, parent, current->blkno, current->offnum);

       
                                                       
       
    if (redirectTuplePos != InvalidOffsetNumber)
    {
      setRedirectionTuple(&saveCurrent, redirectTuplePos, current->blkno, current->offnum);
    }

                                                          
    MarkBufferDirty(saveCurrent.buffer);
  }
  else
  {
       
                                                                        
                                                   
       
    Assert(SpGistBlockIsRoot(current->blkno));
    Assert(redirectTuplePos == InvalidOffsetNumber);

    SpGistInitBuffer(current->buffer, (isNulls ? SPGIST_NULLS : 0));
    xlrec.initInner = true;
    xlrec.innerIsParent = false;

    xlrec.offnumInner = current->offnum = PageAddItem(current->page, (Item)innerTuple, innerTuple->size, InvalidOffsetNumber, false, false);
    if (current->offnum != FirstOffsetNumber)
    {
      elog(ERROR, "failed to add item of size %u to SPGiST index page", innerTuple->size);
    }

                                                         
    xlrec.offnumParent = InvalidOffsetNumber;
    xlrec.nodeI = 0;

                                                          
    MarkBufferDirty(current->buffer);

                                                          
    saveCurrent.buffer = InvalidBuffer;
  }

  if (RelationNeedsWAL(index) && !state->isBuild)
  {
    XLogRecPtr recptr;
    int flags;

    XLogBeginInsert();

    xlrec.nInsert = nToInsert;
    XLogRegisterData((char *)&xlrec, SizeOfSpgxlogPickSplit);

    XLogRegisterData((char *)toDelete, sizeof(OffsetNumber) * xlrec.nDelete);
    XLogRegisterData((char *)toInsert, sizeof(OffsetNumber) * xlrec.nInsert);
    XLogRegisterData((char *)leafPageSelect, sizeof(uint8) * xlrec.nInsert);
    XLogRegisterData((char *)innerTuple, innerTuple->size);
    XLogRegisterData(leafdata, leafptr - leafdata);

                       
    if (BufferIsValid(saveCurrent.buffer))
    {
      flags = REGBUF_STANDARD;
      if (xlrec.initSrc)
      {
        flags |= REGBUF_WILL_INIT;
      }
      XLogRegisterBuffer(0, saveCurrent.buffer, flags);
    }

                       
    if (BufferIsValid(newLeafBuffer))
    {
      flags = REGBUF_STANDARD;
      if (xlrec.initDest)
      {
        flags |= REGBUF_WILL_INIT;
      }
      XLogRegisterBuffer(1, newLeafBuffer, flags);
    }

                    
    flags = REGBUF_STANDARD;
    if (xlrec.initInner)
    {
      flags |= REGBUF_WILL_INIT;
    }
    XLogRegisterBuffer(2, current->buffer, flags);

                                                   
    if (parent->buffer != InvalidBuffer)
    {
      if (parent->buffer != current->buffer)
      {
        XLogRegisterBuffer(3, parent->buffer, REGBUF_STANDARD);
      }
      else
      {
        Assert(xlrec.innerIsParent);
      }
    }

                              
    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_PICKSPLIT);

                                                
    if (newLeafBuffer != InvalidBuffer)
    {
      Page page = BufferGetPage(newLeafBuffer);

      PageSetLSN(page, recptr);
    }

    if (saveCurrent.buffer != InvalidBuffer)
    {
      Page page = BufferGetPage(saveCurrent.buffer);

      PageSetLSN(page, recptr);
    }

    PageSetLSN(current->page, recptr);

    if (parent->buffer != InvalidBuffer)
    {
      PageSetLSN(parent->page, recptr);
    }
  }

  END_CRIT_SECTION();

                                                        
  if (newLeafBuffer != InvalidBuffer)
  {
    SpGistSetLastUsedPage(index, newLeafBuffer);
    UnlockReleaseBuffer(newLeafBuffer);
  }
  if (saveCurrent.buffer != InvalidBuffer)
  {
    SpGistSetLastUsedPage(index, saveCurrent.buffer);
    UnlockReleaseBuffer(saveCurrent.buffer);
  }

  return insertedNew;
}

   
                                                                          
   
static void
spgMatchNodeAction(Relation index, SpGistState *state, SpGistInnerTuple innerTuple, SPPageDesc *current, SPPageDesc *parent, int nodeN)
{
  int i;
  SpGistNodeTuple node;

                                             
  if (parent->buffer != InvalidBuffer && parent->buffer != current->buffer)
  {
    SpGistSetLastUsedPage(index, parent->buffer);
    UnlockReleaseBuffer(parent->buffer);
  }

                                                               
  parent->blkno = current->blkno;
  parent->buffer = current->buffer;
  parent->page = current->page;
  parent->offnum = current->offnum;
  parent->node = nodeN;

                        
  SGITITERATE(innerTuple, i, node)
  {
    if (i == nodeN)
    {
      break;
    }
  }

  if (i != nodeN)
  {
    elog(ERROR, "failed to find requested node %d in SPGiST inner tuple", nodeN);
  }

                                                      
  if (ItemPointerIsValid(&node->t_tid))
  {
    current->blkno = ItemPointerGetBlockNumber(&node->t_tid);
    current->offnum = ItemPointerGetOffsetNumber(&node->t_tid);
  }
  else
  {
                                                             
    current->blkno = InvalidBlockNumber;
    current->offnum = InvalidOffsetNumber;
  }

  current->buffer = InvalidBuffer;
  current->page = NULL;
}

   
                                                               
   
static void
spgAddNodeAction(Relation index, SpGistState *state, SpGistInnerTuple innerTuple, SPPageDesc *current, SPPageDesc *parent, int nodeN, Datum nodeLabel)
{
  SpGistInnerTuple newInnerTuple;
  spgxlogAddNode xlrec;

                                      
  Assert(!SpGistPageStoresNulls(current->page));

                                                      
  newInnerTuple = addNode(state, innerTuple, nodeLabel, nodeN);

                          
  STORE_STATE(state, xlrec.stateSrc);
  xlrec.offnum = current->offnum;

                                                                        
  xlrec.parentBlk = -1;
  xlrec.offnumParent = InvalidOffsetNumber;
  xlrec.nodeI = 0;

                                                        
  xlrec.offnumNew = InvalidOffsetNumber;
  xlrec.newPage = false;

  if (PageGetExactFreeSpace(current->page) >= newInnerTuple->size - innerTuple->size)
  {
       
                                                              
       
    START_CRIT_SECTION();

    PageIndexTupleDelete(current->page, current->offnum);
    if (PageAddItem(current->page, (Item)newInnerTuple, newInnerTuple->size, current->offnum, false, false) != current->offnum)
    {
      elog(ERROR, "failed to add item of size %u to SPGiST index page", newInnerTuple->size);
    }

    MarkBufferDirty(current->buffer);

    if (RelationNeedsWAL(index) && !state->isBuild)
    {
      XLogRecPtr recptr;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, sizeof(xlrec));
      XLogRegisterData((char *)newInnerTuple, newInnerTuple->size);

      XLogRegisterBuffer(0, current->buffer, REGBUF_STANDARD);

      recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_ADD_NODE);

      PageSetLSN(current->page, recptr);
    }

    END_CRIT_SECTION();
  }
  else
  {
       
                                                           
       
    SpGistDeadTuple dt;
    SPPageDesc saveCurrent;

       
                                                                         
                                                                          
                                                                        
       
    if (SpGistBlockIsRoot(current->blkno))
    {
      elog(ERROR, "cannot enlarge root tuple any more");
    }
    Assert(parent->buffer != InvalidBuffer);

    saveCurrent = *current;

    xlrec.offnumParent = parent->offnum;
    xlrec.nodeI = parent->node;

       
                                                                           
                                    
       
    current->buffer = SpGistGetBuffer(index, GBUF_INNER_PARITY(current->blkno), newInnerTuple->size + sizeof(ItemIdData), &xlrec.newPage);
    current->blkno = BufferGetBlockNumber(current->buffer);
    current->page = BufferGetPage(current->buffer);

       
                                                                           
                                                                          
                                                                          
                                                                     
                                                                           
             
       
    if (current->blkno == saveCurrent.blkno)
    {
      elog(ERROR, "SPGiST new buffer shouldn't be same as old buffer");
    }

       
                                                                          
                                                                 
       
    if (parent->buffer == saveCurrent.buffer)
    {
      xlrec.parentBlk = 0;
    }
    else if (parent->buffer == current->buffer)
    {
      xlrec.parentBlk = 1;
    }
    else
    {
      xlrec.parentBlk = 2;
    }

    START_CRIT_SECTION();

                        
    xlrec.offnumNew = current->offnum = SpGistPageAddNewItem(state, current->page, (Item)newInnerTuple, newInnerTuple->size, NULL, false);

    MarkBufferDirty(current->buffer);

                                                             
    saveNodeLink(index, parent, current->blkno, current->offnum);

       
                                                                          
                                                                       
                                                                        
                                                                          
                                              
       
    if (state->isBuild)
    {
      dt = spgFormDeadTuple(state, SPGIST_PLACEHOLDER, InvalidBlockNumber, InvalidOffsetNumber);
    }
    else
    {
      dt = spgFormDeadTuple(state, SPGIST_REDIRECT, current->blkno, current->offnum);
    }

    PageIndexTupleDelete(saveCurrent.page, saveCurrent.offnum);
    if (PageAddItem(saveCurrent.page, (Item)dt, dt->size, saveCurrent.offnum, false, false) != saveCurrent.offnum)
    {
      elog(ERROR, "failed to add item of size %u to SPGiST index page", dt->size);
    }

    if (state->isBuild)
    {
      SpGistPageGetOpaque(saveCurrent.page)->nPlaceholder++;
    }
    else
    {
      SpGistPageGetOpaque(saveCurrent.page)->nRedirection++;
    }

    MarkBufferDirty(saveCurrent.buffer);

    if (RelationNeedsWAL(index) && !state->isBuild)
    {
      XLogRecPtr recptr;
      int flags;

      XLogBeginInsert();

                     
      XLogRegisterBuffer(0, saveCurrent.buffer, REGBUF_STANDARD);
                    
      flags = REGBUF_STANDARD;
      if (xlrec.newPage)
      {
        flags |= REGBUF_WILL_INIT;
      }
      XLogRegisterBuffer(1, current->buffer, flags);
                                                        
      if (xlrec.parentBlk == 2)
      {
        XLogRegisterBuffer(2, parent->buffer, REGBUF_STANDARD);
      }

      XLogRegisterData((char *)&xlrec, sizeof(xlrec));
      XLogRegisterData((char *)newInnerTuple, newInnerTuple->size);

      recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_ADD_NODE);

                                                                  
      PageSetLSN(current->page, recptr);
      PageSetLSN(parent->page, recptr);
      PageSetLSN(saveCurrent.page, recptr);
    }

    END_CRIT_SECTION();

                                                                   
    if (saveCurrent.buffer != current->buffer && saveCurrent.buffer != parent->buffer)
    {
      SpGistSetLastUsedPage(index, saveCurrent.buffer);
      UnlockReleaseBuffer(saveCurrent.buffer);
    }
  }
}

   
                                                                             
   
static void
spgSplitNodeAction(Relation index, SpGistState *state, SpGistInnerTuple innerTuple, SPPageDesc *current, spgChooseOut *out)
{
  SpGistInnerTuple prefixTuple, postfixTuple;
  SpGistNodeTuple node, *nodes;
  BlockNumber postfixBlkno;
  OffsetNumber postfixOffset;
  int i;
  spgxlogSplitTuple xlrec;
  Buffer newBuffer = InvalidBuffer;

                                      
  Assert(!SpGistPageStoresNulls(current->page));

                                         
  if (out->result.splitTuple.prefixNNodes <= 0 || out->result.splitTuple.prefixNNodes > SGITMAXNNODES)
  {
    elog(ERROR, "invalid number of prefix nodes: %d", out->result.splitTuple.prefixNNodes);
  }
  if (out->result.splitTuple.childNodeN < 0 || out->result.splitTuple.childNodeN >= out->result.splitTuple.prefixNNodes)
  {
    elog(ERROR, "invalid child node number: %d", out->result.splitTuple.childNodeN);
  }

     
                                                                            
                                                 
     
  nodes = (SpGistNodeTuple *)palloc(sizeof(SpGistNodeTuple) * out->result.splitTuple.prefixNNodes);

  for (i = 0; i < out->result.splitTuple.prefixNNodes; i++)
  {
    Datum label = (Datum)0;
    bool labelisnull;

    labelisnull = (out->result.splitTuple.prefixNodeLabels == NULL);
    if (!labelisnull)
    {
      label = out->result.splitTuple.prefixNodeLabels[i];
    }
    nodes[i] = spgFormNodeTuple(state, label, labelisnull);
  }

  prefixTuple = spgFormInnerTuple(state, out->result.splitTuple.prefixHasPrefix, out->result.splitTuple.prefixPrefixDatum, out->result.splitTuple.prefixNNodes, nodes);

                                                             
  if (prefixTuple->size > innerTuple->size)
  {
    elog(ERROR, "SPGiST inner-tuple split must not produce longer prefix");
  }

     
                                                                          
                                                                      
               
     
  nodes = palloc(sizeof(SpGistNodeTuple) * innerTuple->nNodes);
  SGITITERATE(innerTuple, i, node) { nodes[i] = node; }

  postfixTuple = spgFormInnerTuple(state, out->result.splitTuple.postfixHasPrefix, out->result.splitTuple.postfixPrefixDatum, innerTuple->nNodes, nodes);

                                                         
  postfixTuple->allTheSame = innerTuple->allTheSame;

                                
  xlrec.newPage = false;

     
                                                                             
                                                                  
     
                                                                          
                                           
     
  if (SpGistBlockIsRoot(current->blkno) || SpGistPageGetFreeSpace(current->page, 1) + innerTuple->size < prefixTuple->size + postfixTuple->size + sizeof(ItemIdData))
  {
       
                                                                       
                           
       
    newBuffer = SpGistGetBuffer(index, GBUF_INNER_PARITY(current->blkno + 1), postfixTuple->size + sizeof(ItemIdData), &xlrec.newPage);
  }

  START_CRIT_SECTION();

     
                                       
     
  PageIndexTupleDelete(current->page, current->offnum);
  xlrec.offnumPrefix = PageAddItem(current->page, (Item)prefixTuple, prefixTuple->size, current->offnum, false, false);
  if (xlrec.offnumPrefix != current->offnum)
  {
    elog(ERROR, "failed to add item of size %u to SPGiST index page", prefixTuple->size);
  }

     
                                             
     
  if (newBuffer == InvalidBuffer)
  {
    postfixBlkno = current->blkno;
    xlrec.offnumPostfix = postfixOffset = SpGistPageAddNewItem(state, current->page, (Item)postfixTuple, postfixTuple->size, NULL, false);
    xlrec.postfixBlkSame = true;
  }
  else
  {
    postfixBlkno = BufferGetBlockNumber(newBuffer);
    xlrec.offnumPostfix = postfixOffset = SpGistPageAddNewItem(state, BufferGetPage(newBuffer), (Item)postfixTuple, postfixTuple->size, NULL, false);
    MarkBufferDirty(newBuffer);
    xlrec.postfixBlkSame = false;
  }

     
                                                                             
                                                                        
                                                                          
                                                                        
                                                                  
     
  spgUpdateNodeLink(prefixTuple, out->result.splitTuple.childNodeN, postfixBlkno, postfixOffset);
  prefixTuple = (SpGistInnerTuple)PageGetItem(current->page, PageGetItemId(current->page, current->offnum));
  spgUpdateNodeLink(prefixTuple, out->result.splitTuple.childNodeN, postfixBlkno, postfixOffset);

  MarkBufferDirty(current->buffer);

  if (RelationNeedsWAL(index) && !state->isBuild)
  {
    XLogRecPtr recptr;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, sizeof(xlrec));
    XLogRegisterData((char *)prefixTuple, prefixTuple->size);
    XLogRegisterData((char *)postfixTuple, postfixTuple->size);

    XLogRegisterBuffer(0, current->buffer, REGBUF_STANDARD);
    if (newBuffer != InvalidBuffer)
    {
      int flags;

      flags = REGBUF_STANDARD;
      if (xlrec.newPage)
      {
        flags |= REGBUF_WILL_INIT;
      }
      XLogRegisterBuffer(1, newBuffer, flags);
    }

    recptr = XLogInsert(RM_SPGIST_ID, XLOG_SPGIST_SPLIT_TUPLE);

    PageSetLSN(current->page, recptr);

    if (newBuffer != InvalidBuffer)
    {
      PageSetLSN(BufferGetPage(newBuffer), recptr);
    }
  }

  END_CRIT_SECTION();

                                                        
  if (newBuffer != InvalidBuffer)
  {
    SpGistSetLastUsedPage(index, newBuffer);
    UnlockReleaseBuffer(newBuffer);
  }
}

   
                                   
   
                                                                         
                                                                            
                                                                 
   
bool
spgdoinsert(Relation index, SpGistState *state, ItemPointer heapPtr, Datum datum, bool isnull)
{
  bool result = true;
  int level = 0;
  Datum leafDatum;
  int leafSize;
  int bestLeafSize;
  int numNoProgressCycles = 0;
  SPPageDesc current, parent;
  FmgrInfo *procinfo = NULL;

     
                                                                        
                               
     
  if (!isnull)
  {
    procinfo = index_getprocinfo(index, 1, SPGIST_CHOOSE_PROC);
  }

     
                                       
     
                                                                            
                                                                          
                                                                          
                                                                      
                                                                       
                      
     
  if (!isnull)
  {
    if (OidIsValid(index_getprocid(index, 1, SPGIST_COMPRESS_PROC)))
    {
      FmgrInfo *compressProcinfo = NULL;

      compressProcinfo = index_getprocinfo(index, 1, SPGIST_COMPRESS_PROC);
      leafDatum = FunctionCall1Coll(compressProcinfo, index->rd_indcollation[0], datum);
    }
    else
    {
      Assert(state->attLeafType.type == state->attType.type);

      if (state->attType.attlen == -1)
      {
        leafDatum = PointerGetDatum(PG_DETOAST_DATUM(datum));
      }
      else
      {
        leafDatum = datum;
      }
    }
  }
  else
  {
    leafDatum = (Datum)0;
  }

     
                                                                       
     
                                                                           
                                                                      
     
  if (!isnull)
  {
    leafSize = SGLTHDRSZ + sizeof(ItemIdData) + SpGistGetTypeSize(&state->attLeafType, leafDatum);
  }
  else
  {
    leafSize = SGDTSIZE + sizeof(ItemIdData);
  }

  if (leafSize > SPGIST_PAGE_CAPACITY && (isnull || !state->config.longValuesOK))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", leafSize - sizeof(ItemIdData), SPGIST_PAGE_CAPACITY - sizeof(ItemIdData), RelationGetRelationName(index)), errhint("Values larger than a buffer page cannot be indexed.")));
  }
  bestLeafSize = leafSize;

                                                         
  current.blkno = isnull ? SPGIST_NULL_BLKNO : SPGIST_ROOT_BLKNO;
  current.buffer = InvalidBuffer;
  current.page = NULL;
  current.offnum = FirstOffsetNumber;
  current.node = -1;

                                          
  parent.blkno = InvalidBlockNumber;
  parent.buffer = InvalidBuffer;
  parent.page = NULL;
  parent.offnum = InvalidOffsetNumber;
  parent.node = -1;

     
                                                                             
                                                                             
                                                                             
                                                                   
     
  CHECK_FOR_INTERRUPTS();

  for (;;)
  {
    bool isNew = false;

       
                                                                         
                                                                    
                                                                         
                                                                          
                                                                         
                                                                         
                                                                       
                                                   
       
    if (INTERRUPTS_PENDING_CONDITION())
    {
      result = false;
      break;
    }

    if (current.blkno == InvalidBlockNumber)
    {
         
                                                                         
                                                                       
                                                                     
                                                   
         
      current.buffer = SpGistGetBuffer(index, GBUF_LEAF | (isnull ? GBUF_NULLS : 0), Min(leafSize, SPGIST_PAGE_CAPACITY), &isNew);
      current.blkno = BufferGetBlockNumber(current.buffer);
    }
    else if (parent.buffer == InvalidBuffer)
    {
                                                                   
      current.buffer = ReadBuffer(index, current.blkno);
      LockBuffer(current.buffer, BUFFER_LOCK_EXCLUSIVE);
    }
    else if (current.blkno != parent.blkno)
    {
                                       
      current.buffer = ReadBuffer(index, current.blkno);

         
                                                                   
                                                                         
                                                                        
                                                                  
         
                                                                      
                                                                        
                                                                       
                                                                        
         
      if (!ConditionalLockBuffer(current.buffer))
      {
        ReleaseBuffer(current.buffer);
        UnlockReleaseBuffer(parent.buffer);
        return false;
      }
    }
    else
    {
                                                                    
      current.buffer = parent.buffer;
    }
    current.page = BufferGetPage(current.buffer);

                                                       
    if (isnull ? !SpGistPageStoresNulls(current.page) : SpGistPageStoresNulls(current.page))
    {
      elog(ERROR, "SPGiST index page %u has wrong nulls flag", current.blkno);
    }

    if (SpGistPageIsLeaf(current.page))
    {
      SpGistLeafTuple leafTuple;
      int nToSplit, sizeToSplit;

      leafTuple = spgFormLeafTuple(state, heapPtr, leafDatum, isnull);
      if (leafTuple->size + sizeof(ItemIdData) <= SpGistPageGetFreeSpace(current.page, 1))
      {
                                                          
        addLeafTuple(index, state, leafTuple, &current, &parent, isnull, isNew);
        break;
      }
      else if ((sizeToSplit = checkSplitConditions(index, state, &current, &nToSplit)) < SPGIST_PAGE_CAPACITY / 2 && nToSplit < 64 && leafTuple->size + sizeof(ItemIdData) + sizeToSplit <= SPGIST_PAGE_CAPACITY)
      {
           
                                                                      
                                                                
           
        Assert(!isNew);
        moveLeafs(index, state, &current, &parent, leafTuple, isnull);
        break;                 
      }
      else
      {
                       
        if (doPickSplit(index, state, &current, &parent, leafTuple, level, isnull, isNew))
        {
          break;                                       
        }

                                                 
        pfree(leafTuple);

           
                                                                    
           
        Assert(!SpGistPageIsLeaf(current.page));
        goto process_inner_tuple;
      }
    }
    else                    
    {
         
                                                                       
                                                       
         
      SpGistInnerTuple innerTuple;
      spgChooseIn in;
      spgChooseOut out;

         
                                                                      
                                                                    
                                                               
                                                                  
         
    process_inner_tuple:
      if (INTERRUPTS_PENDING_CONDITION())
      {
        result = false;
        break;
      }

      innerTuple = (SpGistInnerTuple)PageGetItem(current.page, PageGetItemId(current.page, current.offnum));

      in.datum = datum;
      in.leafDatum = leafDatum;
      in.level = level;
      in.allTheSame = innerTuple->allTheSame;
      in.hasPrefix = (innerTuple->prefixSize > 0);
      in.prefixDatum = SGITDATUM(innerTuple, state);
      in.nNodes = innerTuple->nNodes;
      in.nodeLabels = spgExtractNodeLabels(state, innerTuple);

      memset(&out, 0, sizeof(out));

      if (!isnull)
      {
                                            
        FunctionCall2Coll(procinfo, index->rd_indcollation[0], PointerGetDatum(&in), PointerGetDatum(&out));
      }
      else
      {
                                                                
        out.resultType = spgMatchNode;
      }

      if (innerTuple->allTheSame)
      {
           
                                                                     
                                                                      
                                                         
           
        if (out.resultType == spgAddNode)
        {
          elog(ERROR, "cannot add a node to an allTheSame inner tuple");
        }
        else if (out.resultType == spgMatchNode)
        {
          out.result.matchNode.nodeN = random() % innerTuple->nNodes;
        }
      }

      switch (out.resultType)
      {
      case spgMatchNode:
                                        
        spgMatchNodeAction(index, state, innerTuple, &current, &parent, out.result.matchNode.nodeN);
                                                 
        level += out.result.matchNode.levelAdd;
                                                      
        if (!isnull)
        {
          leafDatum = out.result.matchNode.restDatum;
          leafSize = SGLTHDRSZ + sizeof(ItemIdData) + SpGistGetTypeSize(&state->attLeafType, leafDatum);
        }

           
                                                                  
                                                                  
           
                                                              
                                                                   
                                                             
                                                              
                                                                
                                                                  
                                                              
                                                               
                                                               
                                                                  
                                                                 
                                               
           
        if (leafSize > SPGIST_PAGE_CAPACITY)
        {
          bool ok = false;

          if (state->config.longValuesOK && !isnull)
          {
            if (leafSize < bestLeafSize)
            {
              ok = true;
              bestLeafSize = leafSize;
              numNoProgressCycles = 0;
            }
            else if (++numNoProgressCycles < 10)
            {
              ok = true;
            }
          }
          if (!ok)
          {
            ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", leafSize - sizeof(ItemIdData), SPGIST_PAGE_CAPACITY - sizeof(ItemIdData), RelationGetRelationName(index)), errhint("Values larger than a buffer page cannot be indexed.")));
          }
        }

           
                                                                  
                                                              
                                                                
                                
           
        break;
      case spgAddNode:
                                                                
        if (in.nodeLabels == NULL)
        {
          elog(ERROR, "cannot add a node to an inner tuple without node labels");
        }
                                                  
        spgAddNodeAction(index, state, innerTuple, &current, &parent, out.result.addNode.nodeN, out.result.addNode.nodeLabel);

           
                                                                   
                                                   
           
        goto process_inner_tuple;
        break;
      case spgSplitTuple:
                                            
        spgSplitNodeAction(index, state, innerTuple, &current, &out);

                                                 
        goto process_inner_tuple;
        break;
      default:
        elog(ERROR, "unrecognized SPGiST choose result: %d", (int)out.resultType);
        break;
      }
    }
  }               

     
                                                                          
                                               
     
  if (current.buffer != InvalidBuffer)
  {
    SpGistSetLastUsedPage(index, current.buffer);
    UnlockReleaseBuffer(current.buffer);
  }
  if (parent.buffer != InvalidBuffer && parent.buffer != current.buffer)
  {
    SpGistSetLastUsedPage(index, parent.buffer);
    UnlockReleaseBuffer(parent.buffer);
  }

     
                                                                           
                                                                           
                                                                         
           
     
  Assert(INTERRUPTS_CAN_BE_PROCESSED());

     
                                                                        
                                                                          
                                                                            
                               
     
  CHECK_FOR_INTERRUPTS();

  return result;
}
