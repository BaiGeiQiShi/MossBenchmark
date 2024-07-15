                                                                            
   
             
                                         
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/itup.h"
#include "access/xlog.h"
#include "pgstat.h"
#include "storage/checksum.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

                  
bool ignore_checksum_failure = false;

                                                                    
                               
                                                                    
   

   
            
                                        
                                                                           
                              
   
void
PageInit(Page page, Size pageSize, Size specialSize)
{
  PageHeader p = (PageHeader)page;

  specialSize = MAXALIGN(specialSize);

  Assert(pageSize == BLCKSZ);
  Assert(pageSize > specialSize + SizeOfPageHeaderData);

                                                                      
  MemSet(p, 0, pageSize);

  p->pd_flags = 0;
  p->pd_lower = SizeOfPageHeaderData;
  p->pd_upper = pageSize - specialSize;
  p->pd_special = pageSize - specialSize;
  PageSetPageSizeAndVersion(page, pageSize, PG_PAGE_LAYOUT_VERSION);
                                                                     
}

   
                  
                                                  
   
bool
PageIsVerified(Page page, BlockNumber blkno)
{
  return PageIsVerifiedExtended(page, blkno, PIV_LOG_WARNING | PIV_REPORT_STAT);
}

   
                          
                                                                   
   
                                                                            
                                                                          
                                                           
   
                                                                             
                                                                               
                                                                         
                                                                            
                                                                       
                                                                             
                                                                        
                                                                          
                                                 
   
                                                                       
                       
   
                                                                           
              
   
bool
PageIsVerifiedExtended(Page page, BlockNumber blkno, int flags)
{
  PageHeader p = (PageHeader)page;
  size_t *pagebytes;
  int i;
  bool checksum_failure = false;
  bool header_sane = false;
  bool all_zeroes = false;
  uint16 checksum = 0;

     
                                                                       
     
  if (!PageIsNew(page))
  {
    if (DataChecksumsEnabled())
    {
      checksum = pg_checksum_page((char *)page, blkno);

      if (checksum != p->pd_checksum)
      {
        checksum_failure = true;
      }
    }

       
                                                                         
                                                                          
                                                                      
                        
       
    if ((p->pd_flags & ~PD_VALID_FLAG_BITS) == 0 && p->pd_lower <= p->pd_upper && p->pd_upper <= p->pd_special && p->pd_special <= BLCKSZ && p->pd_special == MAXALIGN(p->pd_special))
    {
      header_sane = true;
    }

    if (header_sane && !checksum_failure)
    {
      return true;
    }
  }

     
                                                                        
                                                                           
                       
     
  StaticAssertStmt(BLCKSZ == (BLCKSZ / sizeof(size_t)) * sizeof(size_t), "BLCKSZ has to be a multiple of sizeof(size_t)");

  all_zeroes = true;
  pagebytes = (size_t *)page;
  for (i = 0; i < (BLCKSZ / sizeof(size_t)); i++)
  {
    if (pagebytes[i] != 0)
    {
      all_zeroes = false;
      break;
    }
  }

  if (all_zeroes)
  {
    return true;
  }

     
                                                                             
                          
     
  if (checksum_failure)
  {
    if ((flags & PIV_LOG_WARNING) != 0)
    {
      ereport(WARNING, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("page verification failed, calculated checksum %u but expected %u", checksum, p->pd_checksum)));
    }

    if ((flags & PIV_REPORT_STAT) != 0)
    {
      pgstat_report_checksum_failure();
    }

    if (header_sane && ignore_checksum_failure)
    {
      return true;
    }
  }

  return false;
}

   
                       
   
                                                                      
                                                                        
                                                                       
   
                                                                        
                                                                        
                                                                          
   
                                                                         
                                                                  
                                                                      
   
                                                                      
                                                                       
                              
   
                                                                         
                                            
   
                                                                        
                                                   
   
                                             
   
OffsetNumber
PageAddItemExtended(Page page, Item item, Size size, OffsetNumber offsetNumber, int flags)
{
  PageHeader phdr = (PageHeader)page;
  Size alignedSize;
  int lower;
  int upper;
  ItemId itemId;
  OffsetNumber limit;
  bool needshuffle = false;

     
                                           
     
  if (phdr->pd_lower < SizeOfPageHeaderData || phdr->pd_lower > phdr->pd_upper || phdr->pd_upper > phdr->pd_special || phdr->pd_special > BLCKSZ)
  {
    ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u", phdr->pd_lower, phdr->pd_upper, phdr->pd_special)));
  }

     
                                                  
     
  limit = OffsetNumberNext(PageGetMaxOffsetNumber(page));

                                   
  if (OffsetNumberIsValid(offsetNumber))
  {
                       
    if ((flags & PAI_OVERWRITE) != 0)
    {
      if (offsetNumber < limit)
      {
        itemId = PageGetItemId(phdr, offsetNumber);
        if (ItemIdIsUsed(itemId) || ItemIdHasStorage(itemId))
        {
          elog(WARNING, "will not overwrite a used ItemId");
          return InvalidOffsetNumber;
        }
      }
    }
    else
    {
      if (offsetNumber < limit)
      {
        needshuffle = true;                                   
      }
    }
  }
  else
  {
                                                             
                                                                
    if (PageHasFreeLinePointers(phdr))
    {
         
                                                                         
                                                                         
                  
         
      for (offsetNumber = 1; offsetNumber < limit; offsetNumber++)
      {
        itemId = PageGetItemId(phdr, offsetNumber);
        if (!ItemIdIsUsed(itemId) && !ItemIdHasStorage(itemId))
        {
          break;
        }
      }
      if (offsetNumber >= limit)
      {
                                            
        PageClearHasFreeLinePointers(phdr);
      }
    }
    else
    {
                                                                    
      offsetNumber = limit;
    }
  }

                                                                 
  if (offsetNumber > limit)
  {
    elog(WARNING, "specified item offset is too large");
    return InvalidOffsetNumber;
  }

                                                          
  if ((flags & PAI_IS_HEAP) != 0 && offsetNumber > MaxHeapTuplesPerPage)
  {
    elog(WARNING, "can't put more than MaxHeapTuplesPerPage items in a heap page");
    return InvalidOffsetNumber;
  }

     
                                                                      
     
                                                                    
                             
     
  if (offsetNumber == limit || needshuffle)
  {
    lower = phdr->pd_lower + sizeof(ItemIdData);
  }
  else
  {
    lower = phdr->pd_lower;
  }

  alignedSize = MAXALIGN(size);

  upper = (int)phdr->pd_upper - (int)alignedSize;

  if (lower > upper)
  {
    return InvalidOffsetNumber;
  }

     
                                                                             
     
  itemId = PageGetItemId(phdr, offsetNumber);

  if (needshuffle)
  {
    memmove(itemId + 1, itemId, (limit - offsetNumber) * sizeof(ItemIdData));
  }

                            
  ItemIdSetNormal(itemId, upper, size);

     
                                                                            
                                                                            
                                                                          
                                                                       
                                                                         
                                                                
     
                                                                          
                                                                           
                                                 
     
  VALGRIND_CHECK_MEM_IS_DEFINED(item, size);

                                          
  memcpy((char *)page + upper, item, size);

                          
  phdr->pd_lower = (LocationIndex)lower;
  phdr->pd_upper = (LocationIndex)upper;

  return offsetNumber;
}

   
                   
                                                                 
                                                                      
   
Page
PageGetTempPage(Page page)
{
  Size pageSize;
  Page temp;

  pageSize = PageGetPageSize(page);
  temp = (Page)palloc(pageSize);

  return temp;
}

   
                       
                                                                 
                                                                       
   
Page
PageGetTempPageCopy(Page page)
{
  Size pageSize;
  Page temp;

  pageSize = PageGetPageSize(page);
  temp = (Page)palloc(pageSize);

  memcpy(temp, page, pageSize);

  return temp;
}

   
                              
                                                                 
                                                                   
                                                                     
   
Page
PageGetTempPageCopySpecial(Page page)
{
  Size pageSize;
  Page temp;

  pageSize = PageGetPageSize(page);
  temp = (Page)palloc(pageSize);

  PageInit(temp, pageSize, PageGetSpecialSize(page));
  memcpy(PageGetSpecialPointer(temp), PageGetSpecialPointer(page), PageGetSpecialSize(page));

  return temp;
}

   
                       
                                                                        
                                    
   
void
PageRestoreTempPage(Page tempPage, Page oldPage)
{
  Size pageSize;

  pageSize = PageGetPageSize(tempPage);
  memcpy((char *)oldPage, (char *)tempPage, pageSize);

  pfree(tempPage);
}

   
                                                                        
   
typedef struct itemIdSortData
{
  uint16 offsetindex;                       
  int16 itemoff;                                    
  uint16 alignedlen;                               
} itemIdSortData;
typedef itemIdSortData *itemIdSort;

static int
itemoffcompare(const void *itemidp1, const void *itemidp2)
{
                                        
  return ((itemIdSort)itemidp2)->itemoff - ((itemIdSort)itemidp1)->itemoff;
}

   
                                                                           
                                                
   
static void
compactify_tuples(itemIdSort itemidbase, int nitems, Page page)
{
  PageHeader phdr = (PageHeader)page;
  Offset upper;
  int i;

                                                               
  qsort((char *)itemidbase, nitems, sizeof(itemIdSortData), itemoffcompare);

  upper = phdr->pd_special;
  for (i = 0; i < nitems; i++)
  {
    itemIdSort itemidptr = &itemidbase[i];
    ItemId lp;

    lp = PageGetItemId(page, itemidptr->offsetindex + 1);
    upper -= itemidptr->alignedlen;
    memmove((char *)page + upper, (char *)page + itemidptr->itemoff, itemidptr->alignedlen);
    lp->lp_off = upper;
  }

  phdr->pd_upper = upper;
}

   
                           
   
                                     
                                                                     
   
                                                                             
   
                                                                       
   
void
PageRepairFragmentation(Page page)
{
  Offset pd_lower = ((PageHeader)page)->pd_lower;
  Offset pd_upper = ((PageHeader)page)->pd_upper;
  Offset pd_special = ((PageHeader)page)->pd_special;
  itemIdSortData itemidbase[MaxHeapTuplesPerPage];
  itemIdSort itemidptr;
  ItemId lp;
  int nline, nstorage, nunused;
  int i;
  Size totallen;

     
                                                                          
                                                                          
                                                                          
                                                                             
                                          
     
  if (pd_lower < SizeOfPageHeaderData || pd_lower > pd_upper || pd_upper > pd_special || pd_special > BLCKSZ || pd_special != MAXALIGN(pd_special))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u", pd_lower, pd_upper, pd_special)));
  }

     
                                                                           
     
  nline = PageGetMaxOffsetNumber(page);
  itemidptr = itemidbase;
  nunused = totallen = 0;
  for (i = FirstOffsetNumber; i <= nline; i++)
  {
    lp = PageGetItemId(page, i);
    if (ItemIdIsUsed(lp))
    {
      if (ItemIdHasStorage(lp))
      {
        itemidptr->offsetindex = i - 1;
        itemidptr->itemoff = ItemIdGetOffset(lp);
        if (unlikely(itemidptr->itemoff < (int)pd_upper || itemidptr->itemoff >= (int)pd_special))
        {
          ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted line pointer: %u", itemidptr->itemoff)));
        }
        itemidptr->alignedlen = MAXALIGN(ItemIdGetLength(lp));
        totallen += itemidptr->alignedlen;
        itemidptr++;
      }
    }
    else
    {
                                                                
      ItemIdSetUnused(lp);
      nunused++;
    }
  }

  nstorage = itemidptr - itemidbase;
  if (nstorage == 0)
  {
                                                            
    ((PageHeader)page)->pd_upper = pd_special;
  }
  else
  {
                                               
    if (totallen > (Size)(pd_special - pd_lower))
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted item lengths: total %u, available space %u", (unsigned int)totallen, pd_special - pd_lower)));
    }

    compactify_tuples(itemidbase, nstorage, page);
  }

                                    
  if (nunused > 0)
  {
    PageSetHasFreeLinePointers(page);
  }
  else
  {
    PageClearHasFreeLinePointers(page);
  }
}

   
                    
                                                                
                                                        
   
                                                               
                                       
   
Size
PageGetFreeSpace(Page page)
{
  int space;

     
                                                                         
               
     
  space = (int)((PageHeader)page)->pd_upper - (int)((PageHeader)page)->pd_lower;

  if (space < (int)sizeof(ItemIdData))
  {
    return 0;
  }
  space -= sizeof(ItemIdData);

  return (Size)space;
}

   
                                     
                                                                
                                                                
   
                                                               
                                       
   
Size
PageGetFreeSpaceForMultipleTuples(Page page, int ntups)
{
  int space;

     
                                                                         
               
     
  space = (int)((PageHeader)page)->pd_upper - (int)((PageHeader)page)->pd_lower;

  if (space < (int)(ntups * sizeof(ItemIdData)))
  {
    return 0;
  }
  space -= ntups * sizeof(ItemIdData);

  return (Size)space;
}

   
                         
                                                                
                                                                 
   
Size
PageGetExactFreeSpace(Page page)
{
  int space;

     
                                                                         
               
     
  space = (int)((PageHeader)page)->pd_upper - (int)((PageHeader)page)->pd_lower;

  if (space < 0)
  {
    return 0;
  }

  return (Size)space;
}

   
                        
                                                                
                                                        
   
                                                                             
                                                                            
                                                                
                                                                             
                                                                            
                                                                          
                                                                            
                                                              
   
Size
PageGetHeapFreeSpace(Page page)
{
  Size space;

  space = PageGetFreeSpace(page);
  if (space > 0)
  {
    OffsetNumber offnum, nline;

       
                                                                         
       
    nline = PageGetMaxOffsetNumber(page);
    if (nline >= MaxHeapTuplesPerPage)
    {
      if (PageHasFreeLinePointers((PageHeader)page))
      {
           
                                                                    
                                      
           
        for (offnum = FirstOffsetNumber; offnum <= nline; offnum = OffsetNumberNext(offnum))
        {
          ItemId lp = PageGetItemId(page, offnum);

          if (!ItemIdIsUsed(lp))
          {
            break;
          }
        }

        if (offnum > nline)
        {
             
                                                                    
                                                            
             
          space = 0;
        }
      }
      else
      {
           
                                                                      
                                                 
           
        space = 0;
      }
    }
  }
  return space;
}

   
                        
   
                                                                      
   
                                                                             
   
void
PageIndexTupleDelete(Page page, OffsetNumber offnum)
{
  PageHeader phdr = (PageHeader)page;
  char *addr;
  ItemId tup;
  Size size;
  unsigned offset;
  int nbytes;
  int offidx;
  int nline;

     
                                                                
     
  if (phdr->pd_lower < SizeOfPageHeaderData || phdr->pd_lower > phdr->pd_upper || phdr->pd_upper > phdr->pd_special || phdr->pd_special > BLCKSZ || phdr->pd_special != MAXALIGN(phdr->pd_special))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u", phdr->pd_lower, phdr->pd_upper, phdr->pd_special)));
  }

  nline = PageGetMaxOffsetNumber(page);
  if ((int)offnum <= 0 || (int)offnum > nline)
  {
    elog(ERROR, "invalid index offnum: %u", offnum);
  }

                                            
  offidx = offnum - 1;

  tup = PageGetItemId(page, offnum);
  Assert(ItemIdHasStorage(tup));
  size = ItemIdGetLength(tup);
  offset = ItemIdGetOffset(tup);

  if (offset < phdr->pd_upper || (offset + size) > phdr->pd_special || offset != MAXALIGN(offset))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted line pointer: offset = %u, size = %u", offset, (unsigned int)size)));
  }

                                              
  size = MAXALIGN(size);

     
                                                                            
                                                                         
                                                                            
             
     
  nbytes = phdr->pd_lower - ((char *)&phdr->pd_linp[offidx + 1] - (char *)phdr);

  if (nbytes > 0)
  {
    memmove((char *)&(phdr->pd_linp[offidx]), (char *)&(phdr->pd_linp[offidx + 1]), nbytes);
  }

     
                                                                         
                                                                             
                                                                           
                                                                           
     

                                
  addr = (char *)page + phdr->pd_upper;

  if (offset > phdr->pd_upper)
  {
    memmove(addr + size, addr, offset - phdr->pd_upper);
  }

                                           
  phdr->pd_upper += size;
  phdr->pd_lower -= sizeof(ItemIdData);

     
                                                              
     
                                                                        
                                               
     
  if (!PageIsEmpty(page))
  {
    int i;

    nline--;                                            
    for (i = 1; i <= nline; i++)
    {
      ItemId ii = PageGetItemId(phdr, i);

      Assert(ItemIdHasStorage(ii));
      if (ItemIdGetOffset(ii) <= offset)
      {
        ii->lp_off += size;
      }
    }
  }
}

   
                        
   
                                                                     
                                                                     
                                                                        
                                                       
   
void
PageIndexMultiDelete(Page page, OffsetNumber *itemnos, int nitems)
{
  PageHeader phdr = (PageHeader)page;
  Offset pd_lower = phdr->pd_lower;
  Offset pd_upper = phdr->pd_upper;
  Offset pd_special = phdr->pd_special;
  itemIdSortData itemidbase[MaxIndexTuplesPerPage];
  ItemIdData newitemids[MaxIndexTuplesPerPage];
  itemIdSort itemidptr;
  ItemId lp;
  int nline, nused;
  Size totallen;
  Size size;
  unsigned offset;
  int nextitm;
  OffsetNumber offnum;

  Assert(nitems <= MaxIndexTuplesPerPage);

     
                                                            
                                                                        
                                                                      
                         
     
                                      
     
  if (nitems <= 2)
  {
    while (--nitems >= 0)
    {
      PageIndexTupleDelete(page, itemnos[nitems]);
    }
    return;
  }

     
                                                                
     
  if (pd_lower < SizeOfPageHeaderData || pd_lower > pd_upper || pd_upper > pd_special || pd_special > BLCKSZ || pd_special != MAXALIGN(pd_special))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u", pd_lower, pd_upper, pd_special)));
  }

     
                                                                          
                                                                        
                              
     
  nline = PageGetMaxOffsetNumber(page);
  itemidptr = itemidbase;
  totallen = 0;
  nused = 0;
  nextitm = 0;
  for (offnum = FirstOffsetNumber; offnum <= nline; offnum = OffsetNumberNext(offnum))
  {
    lp = PageGetItemId(page, offnum);
    Assert(ItemIdHasStorage(lp));
    size = ItemIdGetLength(lp);
    offset = ItemIdGetOffset(lp);
    if (offset < pd_upper || (offset + size) > pd_special || offset != MAXALIGN(offset))
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted line pointer: offset = %u, size = %u", offset, (unsigned int)size)));
    }

    if (nextitm < nitems && offnum == itemnos[nextitm])
    {
                                   
      nextitm++;
    }
    else
    {
      itemidptr->offsetindex = nused;                       
      itemidptr->itemoff = offset;
      itemidptr->alignedlen = MAXALIGN(size);
      totallen += itemidptr->alignedlen;
      newitemids[nused] = *lp;
      itemidptr++;
      nused++;
    }
  }

                                                         
  if (nextitm != nitems)
  {
    elog(ERROR, "incorrect index offsets supplied");
  }

  if (totallen > (Size)(pd_special - pd_lower))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted item lengths: total %u, available space %u", (unsigned int)totallen, pd_special - pd_lower)));
  }

     
                                                                             
                                   
     
  memcpy(phdr->pd_linp, newitemids, nused * sizeof(ItemIdData));
  phdr->pd_lower = SizeOfPageHeaderData + nused * sizeof(ItemIdData);

                                     
  compactify_tuples(itemidbase, nused, page);
}

   
                                 
   
                                                                           
                                                                           
                                              
   
                                                                             
                                                                            
   
void
PageIndexTupleDeleteNoCompact(Page page, OffsetNumber offnum)
{
  PageHeader phdr = (PageHeader)page;
  char *addr;
  ItemId tup;
  Size size;
  unsigned offset;
  int nline;

     
                                                                
     
  if (phdr->pd_lower < SizeOfPageHeaderData || phdr->pd_lower > phdr->pd_upper || phdr->pd_upper > phdr->pd_special || phdr->pd_special > BLCKSZ || phdr->pd_special != MAXALIGN(phdr->pd_special))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u", phdr->pd_lower, phdr->pd_upper, phdr->pd_special)));
  }

  nline = PageGetMaxOffsetNumber(page);
  if ((int)offnum <= 0 || (int)offnum > nline)
  {
    elog(ERROR, "invalid index offnum: %u", offnum);
  }

  tup = PageGetItemId(page, offnum);
  Assert(ItemIdHasStorage(tup));
  size = ItemIdGetLength(tup);
  offset = ItemIdGetOffset(tup);

  if (offset < phdr->pd_upper || (offset + size) > phdr->pd_special || offset != MAXALIGN(offset))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted line pointer: offset = %u, size = %u", offset, (unsigned int)size)));
  }

                                              
  size = MAXALIGN(size);

     
                                                                         
                                                                         
                                                                      
     
  if ((int)offnum < nline)
  {
    ItemIdSetUnused(tup);
  }
  else
  {
    phdr->pd_lower -= sizeof(ItemIdData);
    nline--;                                            
  }

     
                                                                         
                                                                             
                                                                           
                                                                           
     

                                
  addr = (char *)page + phdr->pd_upper;

  if (offset > phdr->pd_upper)
  {
    memmove(addr + size, addr, offset - phdr->pd_upper);
  }

                                          
  phdr->pd_upper += size;

     
                                                              
     
                                                                        
                                               
     
  if (!PageIsEmpty(page))
  {
    int i;

    for (i = 1; i <= nline; i++)
    {
      ItemId ii = PageGetItemId(phdr, i);

      if (ItemIdHasStorage(ii) && ItemIdGetOffset(ii) <= offset)
      {
        ii->lp_off += size;
      }
    }
  }
}

   
                           
   
                                               
   
                                                                        
                                                                       
                                                                      
                                                                    
                                                                
                                                                         
                                                               
   
                                                                         
                                                               
   
bool
PageIndexTupleOverwrite(Page page, OffsetNumber offnum, Item newtup, Size newsize)
{
  PageHeader phdr = (PageHeader)page;
  ItemId tupid;
  int oldsize;
  unsigned offset;
  Size alignednewsize;
  int size_diff;
  int itemcount;

     
                                                                
     
  if (phdr->pd_lower < SizeOfPageHeaderData || phdr->pd_lower > phdr->pd_upper || phdr->pd_upper > phdr->pd_special || phdr->pd_special > BLCKSZ || phdr->pd_special != MAXALIGN(phdr->pd_special))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u", phdr->pd_lower, phdr->pd_upper, phdr->pd_special)));
  }

  itemcount = PageGetMaxOffsetNumber(page);
  if ((int)offnum <= 0 || (int)offnum > itemcount)
  {
    elog(ERROR, "invalid index offnum: %u", offnum);
  }

  tupid = PageGetItemId(page, offnum);
  Assert(ItemIdHasStorage(tupid));
  oldsize = ItemIdGetLength(tupid);
  offset = ItemIdGetOffset(tupid);

  if (offset < phdr->pd_upper || (offset + oldsize) > phdr->pd_special || offset != MAXALIGN(offset))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("corrupted line pointer: offset = %u, size = %u", offset, (unsigned int)oldsize)));
  }

     
                                                                            
     
  oldsize = MAXALIGN(oldsize);
  alignednewsize = MAXALIGN(newsize);
  if (alignednewsize > oldsize + (phdr->pd_upper - phdr->pd_lower))
  {
    return false;
  }

     
                                                                           
                                                                          
                                                                             
                                                                           
                                                                          
                                                          
     
  size_diff = oldsize - (int)alignednewsize;
  if (size_diff != 0)
  {
    char *addr = (char *)page + phdr->pd_upper;
    int i;

                                                         
    memmove(addr + size_diff, addr, offset - phdr->pd_upper);

                                            
    phdr->pd_upper += size_diff;

                                           
    for (i = FirstOffsetNumber; i <= itemcount; i++)
    {
      ItemId ii = PageGetItemId(phdr, i);

                                                                       
      if (ItemIdHasStorage(ii) && ItemIdGetOffset(ii) <= offset)
      {
        ii->lp_off += size_diff;
      }
    }
  }

                                                                      
  ItemIdSetNormal(tupid, offset + size_diff, newsize);

                                     
  memcpy(PageGetItem(page, tupid), newtup, newsize);

  return true;
}

   
                                              
   
                                                                             
                                                                             
                                                                              
                                                                               
                                                                       
                        
   
                                                                            
                                                                         
                                            
   
char *
PageSetChecksumCopy(Page page, BlockNumber blkno)
{
  static char *pageCopy = NULL;

                                                                   
  if (PageIsNew(page) || !DataChecksumsEnabled())
  {
    return (char *)page;
  }

     
                                                                        
                                                                           
                                                                            
                                                                          
     
  if (pageCopy == NULL)
  {
    pageCopy = MemoryContextAlloc(TopMemoryContext, BLCKSZ);
  }

  memcpy(pageCopy, (char *)page, BLCKSZ);
  ((PageHeader)pageCopy)->pd_checksum = pg_checksum_page(pageCopy, blkno);
  return pageCopy;
}

   
                                              
   
                                                                              
                    
   
void
PageSetChecksumInplace(Page page, BlockNumber blkno)
{
                                                
  if (PageIsNew(page) || !DataChecksumsEnabled())
  {
    return;
  }

  ((PageHeader)page)->pd_checksum = pg_checksum_page((char *)page, blkno);
}
