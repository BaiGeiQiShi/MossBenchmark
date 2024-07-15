   
                  
                                            
   
                                                                         
                                                                        
   
                  
                                            
   
#include "postgres.h"

#include "access/brin_pageops.h"
#include "access/brin_page.h"
#include "access/brin_revmap.h"
#include "access/brin_xlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/rel.h"

   
                                                                              
                                                   
   
#define BrinMaxItemSize MAXALIGN_DOWN(BLCKSZ - (MAXALIGN(SizeOfPageHeaderData + sizeof(ItemIdData)) + MAXALIGN(sizeof(BrinSpecialSpace))))

static Buffer
brin_getinsertbuffer(Relation irel, Buffer oldbuf, Size itemsz, bool *extended);
static Size
br_page_get_freespace(Page page);
static void
brin_initialize_empty_new_buffer(Relation idxrel, Buffer buffer);

   
                                                                          
                                                                               
                                                                               
   
                                                                              
                                        
   
                                                                               
                                                                                
                                                
   
bool
brin_doupdate(Relation idxrel, BlockNumber pagesPerRange, BrinRevmap *revmap, BlockNumber heapBlk, Buffer oldbuf, OffsetNumber oldoff, const BrinTuple *origtup, Size origsz, const BrinTuple *newtup, Size newsz, bool samepage)
{
  Page oldpage;
  ItemId oldlp;
  BrinTuple *oldtup;
  Size oldsz;
  Buffer newbuf;
  BlockNumber newblk = InvalidBlockNumber;
  bool extended;

  Assert(newsz == MAXALIGN(newsz));

                                               
  if (newsz > BrinMaxItemSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", newsz, BrinMaxItemSize, RelationGetRelationName(idxrel))));
    return false;                          
  }

                                                                        
  brinRevmapExtend(revmap, heapBlk);

  if (!samepage)
  {
                                              
    newbuf = brin_getinsertbuffer(idxrel, oldbuf, newsz, &extended);
    if (!BufferIsValid(newbuf))
    {
      Assert(!extended);
      return false;
    }

       
                                                                         
                                                                           
                                              
       
    if (newbuf == oldbuf)
    {
      Assert(!extended);
      newbuf = InvalidBuffer;
    }
    else
    {
      newblk = BufferGetBlockNumber(newbuf);
    }
  }
  else
  {
    LockBuffer(oldbuf, BUFFER_LOCK_EXCLUSIVE);
    newbuf = InvalidBuffer;
    extended = false;
  }
  oldpage = BufferGetPage(oldbuf);
  oldlp = PageGetItemId(oldpage, oldoff);

     
                                                                         
                                                                       
                                                                      
                                                                  
                                                                  
                         
     
  if (!BRIN_IS_REGULAR_PAGE(oldpage) || oldoff > PageGetMaxOffsetNumber(oldpage) || !ItemIdIsNormal(oldlp))
  {
    LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);

       
                                                                         
                                                                           
                        
       
    if (BufferIsValid(newbuf))
    {
      if (extended)
      {
        brin_initialize_empty_new_buffer(idxrel, newbuf);
      }
      UnlockReleaseBuffer(newbuf);
      if (extended)
      {
        FreeSpaceMapVacuumRange(idxrel, newblk, newblk + 1);
      }
    }
    return false;
  }

  oldsz = ItemIdGetLength(oldlp);
  oldtup = (BrinTuple *)PageGetItem(oldpage, oldlp);

     
                                                                       
     
  if (!brin_tuples_equal(oldtup, oldsz, origtup, origsz))
  {
    LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);
    if (BufferIsValid(newbuf))
    {
                                                                  
      if (extended)
      {
        brin_initialize_empty_new_buffer(idxrel, newbuf);
      }
      UnlockReleaseBuffer(newbuf);
      if (extended)
      {
        FreeSpaceMapVacuumRange(idxrel, newblk, newblk + 1);
      }
    }
    return false;
  }

     
                                                                      
     
                                                                           
     
                                                                           
                                                                            
                                                       
     
  if (((BrinPageFlags(oldpage) & BRIN_EVACUATE_PAGE) == 0) && brin_can_do_samepage_update(oldbuf, origsz, newsz))
  {
    START_CRIT_SECTION();
    if (!PageIndexTupleOverwrite(oldpage, oldoff, (Item)unconstify(BrinTuple *, newtup), newsz))
    {
      elog(ERROR, "failed to replace BRIN tuple");
    }
    MarkBufferDirty(oldbuf);

                    
    if (RelationNeedsWAL(idxrel))
    {
      xl_brin_samepage_update xlrec;
      XLogRecPtr recptr;
      uint8 info = XLOG_BRIN_SAMEPAGE_UPDATE;

      xlrec.offnum = oldoff;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, SizeOfBrinSamepageUpdate);

      XLogRegisterBuffer(0, oldbuf, REGBUF_STANDARD);
      XLogRegisterBufData(0, (char *)unconstify(BrinTuple *, newtup), newsz);

      recptr = XLogInsert(RM_BRIN_ID, info);

      PageSetLSN(oldpage, recptr);
    }

    END_CRIT_SECTION();

    LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);

    if (BufferIsValid(newbuf))
    {
                                                                  
      if (extended)
      {
        brin_initialize_empty_new_buffer(idxrel, newbuf);
      }
      UnlockReleaseBuffer(newbuf);
      if (extended)
      {
        FreeSpaceMapVacuumRange(idxrel, newblk, newblk + 1);
      }
    }

    return true;
  }
  else if (newbuf == InvalidBuffer)
  {
       
                                                                      
                   
       
    LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);
    return false;
  }
  else
  {
       
                                                                          
                                    
       
    Page newpage = BufferGetPage(newbuf);
    Buffer revmapbuf;
    ItemPointerData newtid;
    OffsetNumber newoff;
    Size freespace = 0;

    revmapbuf = brinLockRevmapPageForUpdate(revmap, heapBlk);

    START_CRIT_SECTION();

       
                                                                       
                                                                          
                             
       
    if (extended)
    {
      brin_page_init(newpage, BRIN_PAGETYPE_REGULAR);
    }

    PageIndexTupleDeleteNoCompact(oldpage, oldoff);
    newoff = PageAddItem(newpage, (Item)unconstify(BrinTuple *, newtup), newsz, InvalidOffsetNumber, false, false);
    if (newoff == InvalidOffsetNumber)
    {
      elog(ERROR, "failed to add BRIN tuple to new page");
    }
    MarkBufferDirty(oldbuf);
    MarkBufferDirty(newbuf);

                                    
    if (extended)
    {
      freespace = br_page_get_freespace(newpage);
    }

    ItemPointerSet(&newtid, newblk, newoff);
    brinSetHeapBlockItemptr(revmapbuf, pagesPerRange, heapBlk, newtid);
    MarkBufferDirty(revmapbuf);

                    
    if (RelationNeedsWAL(idxrel))
    {
      xl_brin_update xlrec;
      XLogRecPtr recptr;
      uint8 info;

      info = XLOG_BRIN_UPDATE | (extended ? XLOG_BRIN_INIT_PAGE : 0);

      xlrec.insert.offnum = newoff;
      xlrec.insert.heapBlk = heapBlk;
      xlrec.insert.pagesPerRange = pagesPerRange;
      xlrec.oldOffnum = oldoff;

      XLogBeginInsert();

                    
      XLogRegisterData((char *)&xlrec, SizeOfBrinUpdate);

      XLogRegisterBuffer(0, newbuf, REGBUF_STANDARD | (extended ? REGBUF_WILL_INIT : 0));
      XLogRegisterBufData(0, (char *)unconstify(BrinTuple *, newtup), newsz);

                       
      XLogRegisterBuffer(1, revmapbuf, 0);

                    
      XLogRegisterBuffer(2, oldbuf, REGBUF_STANDARD);

      recptr = XLogInsert(RM_BRIN_ID, info);

      PageSetLSN(oldpage, recptr);
      PageSetLSN(newpage, recptr);
      PageSetLSN(BufferGetPage(revmapbuf), recptr);
    }

    END_CRIT_SECTION();

    LockBuffer(revmapbuf, BUFFER_LOCK_UNLOCK);
    LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);
    UnlockReleaseBuffer(newbuf);

    if (extended)
    {
      RecordPageWithFreeSpace(idxrel, newblk, freespace);
      FreeSpaceMapVacuumRange(idxrel, newblk, newblk + 1);
    }

    return true;
  }
}

   
                                                          
   
bool
brin_can_do_samepage_update(Buffer buffer, Size origsz, Size newsz)
{
  return ((newsz <= origsz) || PageGetExactFreeSpace(BufferGetPage(buffer)) >= (newsz - origsz));
}

   
                                                                            
                                                                               
                            
   
                                                                           
                                                                          
                                                                      
   
                                                                   
   
OffsetNumber
brin_doinsert(Relation idxrel, BlockNumber pagesPerRange, BrinRevmap *revmap, Buffer *buffer, BlockNumber heapBlk, BrinTuple *tup, Size itemsz)
{
  Page page;
  BlockNumber blk;
  OffsetNumber off;
  Size freespace = 0;
  Buffer revmapbuf;
  ItemPointerData tid;
  bool extended;

  Assert(itemsz == MAXALIGN(itemsz));

                                                    
  if (itemsz > BrinMaxItemSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", itemsz, BrinMaxItemSize, RelationGetRelationName(idxrel))));
    return InvalidOffsetNumber;                          
  }

                                                                        
  brinRevmapExtend(revmap, heapBlk);

     
                                                                            
                                                       
     
  if (BufferIsValid(*buffer))
  {
       
                                                                       
                                                                       
                                  
       
    LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
    if (br_page_get_freespace(BufferGetPage(*buffer)) < itemsz)
    {
      UnlockReleaseBuffer(*buffer);
      *buffer = InvalidBuffer;
    }
  }

     
                                                                       
                        
     
  if (!BufferIsValid(*buffer))
  {
    do
    {
      *buffer = brin_getinsertbuffer(idxrel, InvalidBuffer, itemsz, &extended);
    } while (!BufferIsValid(*buffer));
  }
  else
  {
    extended = false;
  }

                                        
  revmapbuf = brinLockRevmapPageForUpdate(revmap, heapBlk);

  page = BufferGetPage(*buffer);
  blk = BufferGetBlockNumber(*buffer);

                                    
  START_CRIT_SECTION();
  if (extended)
  {
    brin_page_init(page, BRIN_PAGETYPE_REGULAR);
  }
  off = PageAddItem(page, (Item)tup, itemsz, InvalidOffsetNumber, false, false);
  if (off == InvalidOffsetNumber)
  {
    elog(ERROR, "failed to add BRIN tuple to new page");
  }
  MarkBufferDirty(*buffer);

                                  
  if (extended)
  {
    freespace = br_page_get_freespace(page);
  }

  ItemPointerSet(&tid, blk, off);
  brinSetHeapBlockItemptr(revmapbuf, pagesPerRange, heapBlk, tid);
  MarkBufferDirty(revmapbuf);

                  
  if (RelationNeedsWAL(idxrel))
  {
    xl_brin_insert xlrec;
    XLogRecPtr recptr;
    uint8 info;

    info = XLOG_BRIN_INSERT | (extended ? XLOG_BRIN_INIT_PAGE : 0);
    xlrec.heapBlk = heapBlk;
    xlrec.pagesPerRange = pagesPerRange;
    xlrec.offnum = off;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfBrinInsert);

    XLogRegisterBuffer(0, *buffer, REGBUF_STANDARD | (extended ? REGBUF_WILL_INIT : 0));
    XLogRegisterBufData(0, (char *)tup, itemsz);

    XLogRegisterBuffer(1, revmapbuf, 0);

    recptr = XLogInsert(RM_BRIN_ID, info);

    PageSetLSN(page, recptr);
    PageSetLSN(BufferGetPage(revmapbuf), recptr);
  }

  END_CRIT_SECTION();

                                                           
  LockBuffer(*buffer, BUFFER_LOCK_UNLOCK);
  LockBuffer(revmapbuf, BUFFER_LOCK_UNLOCK);

  BRIN_elog((DEBUG2, "inserted tuple (%u,%u) for range starting at %u", blk, off, heapBlk));

  if (extended)
  {
    RecordPageWithFreeSpace(idxrel, blk, freespace);
    FreeSpaceMapVacuumRange(idxrel, blk, blk + 1);
  }

  return off;
}

   
                                          
   
                                                               
   
void
brin_page_init(Page page, uint16 type)
{
  PageInit(page, BLCKSZ, sizeof(BrinSpecialSpace));

  BrinPageType(page) = type;
}

   
                                           
   
void
brin_metapage_init(Page page, BlockNumber pagesPerRange, uint16 version)
{
  BrinMetaPageData *metadata;

  brin_page_init(page, BRIN_PAGETYPE_META);

  metadata = (BrinMetaPageData *)PageGetContents(page);

  metadata->brinMagic = BRIN_META_MAGIC;
  metadata->brinVersion = version;
  metadata->pagesPerRange = pagesPerRange;

     
                                                                        
                                                                          
                                                  
     
  metadata->lastRevmapPage = 0;

     
                                                                         
                                                                          
               
     
  ((PageHeader)page)->pd_lower = ((char *)metadata + sizeof(BrinMetaPageData)) - (char *)page;
}

   
                                      
   
                                                            
   
                                                                           
                                                                           
                                                            
   
bool
brin_start_evacuating_page(Relation idxRel, Buffer buf)
{
  OffsetNumber off;
  OffsetNumber maxoff;
  Page page;

  page = BufferGetPage(buf);

  if (PageIsNew(page))
  {
    return false;
  }

  maxoff = PageGetMaxOffsetNumber(page);
  for (off = FirstOffsetNumber; off <= maxoff; off++)
  {
    ItemId lp;

    lp = PageGetItemId(page, off);
    if (ItemIdIsUsed(lp))
    {
         
                                                                     
                                                                         
                                                                       
                                                 
         
      BrinPageFlags(page) |= BRIN_EVACUATE_PAGE;
      MarkBufferDirtyHint(buf, true);

      return true;
    }
  }
  return false;
}

   
                                  
   
                                                                         
   
void
brin_evacuate_page(Relation idxRel, BlockNumber pagesPerRange, BrinRevmap *revmap, Buffer buf)
{
  OffsetNumber off;
  OffsetNumber maxoff;
  Page page;
  BrinTuple *btup = NULL;
  Size btupsz = 0;

  page = BufferGetPage(buf);

  Assert(BrinPageFlags(page) & BRIN_EVACUATE_PAGE);

  maxoff = PageGetMaxOffsetNumber(page);
  for (off = FirstOffsetNumber; off <= maxoff; off++)
  {
    BrinTuple *tup;
    Size sz;
    ItemId lp;

    CHECK_FOR_INTERRUPTS();

    lp = PageGetItemId(page, off);
    if (ItemIdIsUsed(lp))
    {
      sz = ItemIdGetLength(lp);
      tup = (BrinTuple *)PageGetItem(page, lp);
      tup = brin_copy_tuple(tup, sz, btup, &btupsz);

      LockBuffer(buf, BUFFER_LOCK_UNLOCK);

      if (!brin_doupdate(idxRel, pagesPerRange, revmap, tup->bt_blkno, buf, off, tup, sz, tup, sz, false))
      {
        off--;            
      }

      LockBuffer(buf, BUFFER_LOCK_SHARE);

                                                                         
      if (!BRIN_IS_REGULAR_PAGE(page))
      {
        break;
      }
    }
  }

  UnlockReleaseBuffer(buf);
}

   
                                                                       
                                  
   
                                                                             
                                                                              
                                
   
                                                                               
                                                                              
                                                                           
                                               
   
void
brin_page_cleanup(Relation idxrel, Buffer buf)
{
  Page page = BufferGetPage(buf);

     
                                                                            
          
     
                                                                           
                                                                       
                                                                            
                                                                             
                                       
     
  if (PageIsNew(page))
  {
    LockRelationForExtension(idxrel, ShareLock);
    UnlockRelationForExtension(idxrel, ShareLock);

    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
    if (PageIsNew(page))
    {
      brin_initialize_empty_new_buffer(idxrel, buf);
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
      return;
    }
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
  }

                                                      
  if (BRIN_IS_META_PAGE(BufferGetPage(buf)) || BRIN_IS_REVMAP_PAGE(BufferGetPage(buf)))
  {
    return;
  }

                                        
  RecordPageWithFreeSpace(idxrel, BufferGetBlockNumber(buf), br_page_get_freespace(page));
}

   
                                                                                
                                                                      
                                                                               
                                            
   
                                                                           
                                                                    
                  
   
                                                                             
                                                                               
                                                                             
                                                                            
                                                    
   
                                                                              
                                                                              
                                                                           
                                                         
   
                                                                            
                                                                        
                                                                               
                                                                
   
static Buffer
brin_getinsertbuffer(Relation irel, Buffer oldbuf, Size itemsz, bool *extended)
{
  BlockNumber oldblk;
  BlockNumber newblk;
  Page page;
  Size freespace;

                                 
  Assert(itemsz <= BrinMaxItemSize);

  if (BufferIsValid(oldbuf))
  {
    oldblk = BufferGetBlockNumber(oldbuf);
  }
  else
  {
    oldblk = InvalidBlockNumber;
  }

                                                                     
  newblk = RelationGetTargetBlock(irel);
  if (newblk == InvalidBlockNumber)
  {
    newblk = GetPageWithFreeSpace(irel, itemsz);
  }

     
                                                                           
                                                                           
                                                                         
                                  
     
  for (;;)
  {
    Buffer buf;
    bool extensionLockHeld = false;

    CHECK_FOR_INTERRUPTS();

    *extended = false;

    if (newblk == InvalidBlockNumber)
    {
         
                                                                   
                                                                         
               
         
      if (!RELATION_IS_LOCAL(irel))
      {
        LockRelationForExtension(irel, ExclusiveLock);
        extensionLockHeld = true;
      }
      buf = ReadBuffer(irel, P_NEW);
      newblk = BufferGetBlockNumber(buf);
      *extended = true;

      BRIN_elog((DEBUG2, "brin_getinsertbuffer: extending to page %u", BufferGetBlockNumber(buf)));
    }
    else if (newblk == oldblk)
    {
         
                                                                       
                                   
         
      buf = oldbuf;
    }
    else
    {
      buf = ReadBuffer(irel, newblk);
    }

       
                                                                           
                                                                           
                                                                        
                             
       
    if (BufferIsValid(oldbuf) && oldblk < newblk)
    {
      LockBuffer(oldbuf, BUFFER_LOCK_EXCLUSIVE);
      if (!BRIN_IS_REGULAR_PAGE(BufferGetPage(oldbuf)))
      {
        LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);

           
                                                              
                                                                     
                                                                      
                                                                   
                                                                       
                     
           
        if (*extended)
        {
          brin_initialize_empty_new_buffer(irel, buf);
        }

        if (extensionLockHeld)
        {
          UnlockRelationForExtension(irel, ExclusiveLock);
        }

        ReleaseBuffer(buf);

        if (*extended)
        {
          FreeSpaceMapVacuumRange(irel, newblk, newblk + 1);
                                                          
          *extended = false;
        }

        return InvalidBuffer;
      }
    }

    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

    if (extensionLockHeld)
    {
      UnlockRelationForExtension(irel, ExclusiveLock);
    }

    page = BufferGetPage(buf);

       
                                                                         
                                                                          
                                                                        
                                                            
       
    freespace = *extended ? BrinMaxItemSize : br_page_get_freespace(page);
    if (freespace >= itemsz)
    {
      RelationSetTargetBlock(irel, newblk);

         
                                                                       
                                                                        
                                                                      
                                       
         
      if (BufferIsValid(oldbuf) && oldblk > newblk)
      {
        LockBuffer(oldbuf, BUFFER_LOCK_EXCLUSIVE);
        Assert(BRIN_IS_REGULAR_PAGE(BufferGetPage(oldbuf)));
      }

      return buf;
    }

                               

       
                                                                          
                                                                           
                                                                         
                  
       
    if (*extended)
    {
      brin_initialize_empty_new_buffer(irel, buf);
                                                                 

      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", itemsz, freespace, RelationGetRelationName(irel))));
      return InvalidBuffer;                          
    }

    if (newblk != oldblk)
    {
      UnlockReleaseBuffer(buf);
    }
    if (BufferIsValid(oldbuf) && oldblk <= newblk)
    {
      LockBuffer(oldbuf, BUFFER_LOCK_UNLOCK);
    }

       
                                                                        
                                                         
       
    newblk = RecordAndGetPageWithFreeSpace(irel, newblk, freespace, itemsz);
  }
}

   
                                                                             
                    
   
                                                                          
                                                                             
                                                                               
                                                                          
                                                                           
                                                                           
   
                                                                             
                                                
   
static void
brin_initialize_empty_new_buffer(Relation idxrel, Buffer buffer)
{
  Page page;

  BRIN_elog((DEBUG2, "brin_initialize_empty_new_buffer: initializing blank page %u", BufferGetBlockNumber(buffer)));

  START_CRIT_SECTION();
  page = BufferGetPage(buffer);
  brin_page_init(page, BRIN_PAGETYPE_REGULAR);
  MarkBufferDirty(buffer);
  log_newpage_buffer(buffer, true);
  END_CRIT_SECTION();

     
                                                                           
                                                                           
                                                        
     
  RecordPageWithFreeSpace(idxrel, BufferGetBlockNumber(buffer), br_page_get_freespace(page));
}

   
                                                                 
   
                                                                  
                                       
   
static Size
br_page_get_freespace(Page page)
{
  if (!BRIN_IS_REGULAR_PAGE(page) || (BrinPageFlags(page) & BRIN_EVACUATE_PAGE) != 0)
  {
    return 0;
  }
  else
  {
    return PageGetFreeSpace(page);
  }
}
