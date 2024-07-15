   
                 
                               
   
                                                                                
                                                                             
                                                                            
                                                                              
                                             
   
                                                                               
                                                                          
                                                                      
   
                                                                         
                                                                        
   
                  
                                           
   
#include "postgres.h"

#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_revmap.h"
#include "access/brin_tuple.h"
#include "access/brin_xlog.h"
#include "access/rmgr.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/rel.h"

   
                                                                                
                                                                               
                                
   
#define HEAPBLK_TO_REVMAP_BLK(pagesPerRange, heapBlk) ((heapBlk / pagesPerRange) / REVMAP_PAGE_MAXITEMS)
#define HEAPBLK_TO_REVMAP_INDEX(pagesPerRange, heapBlk) ((heapBlk / pagesPerRange) % REVMAP_PAGE_MAXITEMS)

struct BrinRevmap
{
  Relation rm_irel;
  BlockNumber rm_pagesPerRange;
  BlockNumber rm_lastRevmapPage;                               
  Buffer rm_metaBuf;
  Buffer rm_currBuf;
};

                                      

static BlockNumber
revmap_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk);
static Buffer
revmap_get_buffer(BrinRevmap *revmap, BlockNumber heapBlk);
static BlockNumber
revmap_extend_and_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk);
static void
revmap_physical_extend(BrinRevmap *revmap);

   
                                                                       
                                                    
   
BrinRevmap *
brinRevmapInitialize(Relation idxrel, BlockNumber *pagesPerRange, Snapshot snapshot)
{
  BrinRevmap *revmap;
  Buffer meta;
  BrinMetaPageData *metadata;
  Page page;

  meta = ReadBuffer(idxrel, BRIN_METAPAGE_BLKNO);
  LockBuffer(meta, BUFFER_LOCK_SHARE);
  page = BufferGetPage(meta);
  TestForOldSnapshot(snapshot, idxrel, page);
  metadata = (BrinMetaPageData *)PageGetContents(page);

  revmap = palloc(sizeof(BrinRevmap));
  revmap->rm_irel = idxrel;
  revmap->rm_pagesPerRange = metadata->pagesPerRange;
  revmap->rm_lastRevmapPage = metadata->lastRevmapPage;
  revmap->rm_metaBuf = meta;
  revmap->rm_currBuf = InvalidBuffer;

  *pagesPerRange = metadata->pagesPerRange;

  LockBuffer(meta, BUFFER_LOCK_UNLOCK);

  return revmap;
}

   
                                                             
   
void
brinRevmapTerminate(BrinRevmap *revmap)
{
  ReleaseBuffer(revmap->rm_metaBuf);
  if (revmap->rm_currBuf != InvalidBuffer)
  {
    ReleaseBuffer(revmap->rm_currBuf);
  }
  pfree(revmap);
}

   
                                                           
   
void
brinRevmapExtend(BrinRevmap *revmap, BlockNumber heapBlk)
{
  BlockNumber mapBlk PG_USED_FOR_ASSERTS_ONLY;

  mapBlk = revmap_extend_and_get_blkno(revmap, heapBlk);

                                                         
  Assert(mapBlk != InvalidBlockNumber && mapBlk != BRIN_METAPAGE_BLKNO && mapBlk <= revmap->rm_lastRevmapPage);
}

   
                                                                              
                                                                        
                                                                              
                         
   
                                                                             
                                                                       
   
Buffer
brinLockRevmapPageForUpdate(BrinRevmap *revmap, BlockNumber heapBlk)
{
  Buffer rmBuf;

  rmBuf = revmap_get_buffer(revmap, heapBlk);
  LockBuffer(rmBuf, BUFFER_LOCK_EXCLUSIVE);

  return rmBuf;
}

   
                                                                              
                                                                     
                                                                
   
                                                                         
                    
   
                                                                 
   
void
brinSetHeapBlockItemptr(Buffer buf, BlockNumber pagesPerRange, BlockNumber heapBlk, ItemPointerData tid)
{
  RevmapContents *contents;
  ItemPointerData *iptr;
  Page page;

                                                            
  page = BufferGetPage(buf);
  contents = (RevmapContents *)PageGetContents(page);
  iptr = (ItemPointerData *)contents->rm_tids;
  iptr += HEAPBLK_TO_REVMAP_INDEX(pagesPerRange, heapBlk);

  if (ItemPointerIsValid(&tid))
  {
    ItemPointerSet(iptr, ItemPointerGetBlockNumber(&tid), ItemPointerGetOffsetNumber(&tid));
  }
  else
  {
    ItemPointerSetInvalid(iptr);
  }
}

   
                                               
   
                                                                         
                                                                               
                                                                           
                                                                            
                                                                               
                                    
   
                                                                              
                                                                              
               
   
                                                                               
                         
   
BrinTuple *
brinGetTupleForHeapBlock(BrinRevmap *revmap, BlockNumber heapBlk, Buffer *buf, OffsetNumber *off, Size *size, int mode, Snapshot snapshot)
{
  Relation idxRel = revmap->rm_irel;
  BlockNumber mapBlk;
  RevmapContents *contents;
  ItemPointerData *iptr;
  BlockNumber blk;
  Page page;
  ItemId lp;
  BrinTuple *tup;
  ItemPointerData previptr;

                                                                         
  heapBlk = (heapBlk / revmap->rm_pagesPerRange) * revmap->rm_pagesPerRange;

     
                                                                            
                                                                           
                     
     
  mapBlk = revmap_get_blkno(revmap, heapBlk);
  if (mapBlk == InvalidBlockNumber)
  {
    *off = InvalidOffsetNumber;
    return NULL;
  }

  ItemPointerSetInvalid(&previptr);
  for (;;)
  {
    CHECK_FOR_INTERRUPTS();

    if (revmap->rm_currBuf == InvalidBuffer || BufferGetBlockNumber(revmap->rm_currBuf) != mapBlk)
    {
      if (revmap->rm_currBuf != InvalidBuffer)
      {
        ReleaseBuffer(revmap->rm_currBuf);
      }

      Assert(mapBlk != InvalidBlockNumber);
      revmap->rm_currBuf = ReadBuffer(revmap->rm_irel, mapBlk);
    }

    LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_SHARE);

    contents = (RevmapContents *)PageGetContents(BufferGetPage(revmap->rm_currBuf));
    iptr = contents->rm_tids;
    iptr += HEAPBLK_TO_REVMAP_INDEX(revmap->rm_pagesPerRange, heapBlk);

    if (!ItemPointerIsValid(iptr))
    {
      LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_UNLOCK);
      return NULL;
    }

       
                                                                          
                                                                           
                                                                           
                                                              
       
    if (ItemPointerIsValid(&previptr) && ItemPointerEquals(&previptr, iptr))
    {
      ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg_internal("corrupted BRIN index: inconsistent range map")));
    }
    previptr = *iptr;

    blk = ItemPointerGetBlockNumber(iptr);
    *off = ItemPointerGetOffsetNumber(iptr);

    LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_UNLOCK);

                                                                       
    if (!BufferIsValid(*buf) || BufferGetBlockNumber(*buf) != blk)
    {
      if (BufferIsValid(*buf))
      {
        ReleaseBuffer(*buf);
      }
      *buf = ReadBuffer(idxRel, blk);
    }
    LockBuffer(*buf, mode);
    page = BufferGetPage(*buf);
    TestForOldSnapshot(snapshot, idxRel, page);

                                                 
    if (BRIN_IS_REGULAR_PAGE(page))
    {
         
                                                                       
                                                                     
                                          
         
      if (*off > PageGetMaxOffsetNumber(page))
      {
        LockBuffer(*buf, BUFFER_LOCK_UNLOCK);
        return NULL;
      }

      lp = PageGetItemId(page, *off);
      if (ItemIdIsUsed(lp))
      {
        tup = (BrinTuple *)PageGetItem(page, lp);

        if (tup->bt_blkno == heapBlk)
        {
          if (size)
          {
            *size = ItemIdGetLength(lp);
          }
                         
          return tup;
        }
      }
    }

       
                                                                 
       
    LockBuffer(*buf, BUFFER_LOCK_UNLOCK);
  }
                                            
  return NULL;
}

   
                                                                
   
                                                          
   
                                        
   
bool
brinRevmapDesummarizeRange(Relation idxrel, BlockNumber heapBlk)
{
  BrinRevmap *revmap;
  BlockNumber pagesPerRange;
  RevmapContents *contents;
  ItemPointerData *iptr;
  ItemPointerData invalidIptr;
  BlockNumber revmapBlk;
  Buffer revmapBuf;
  Buffer regBuf;
  Page revmapPg;
  Page regPg;
  OffsetNumber revmapOffset;
  OffsetNumber regOffset;
  ItemId lp;

  revmap = brinRevmapInitialize(idxrel, &pagesPerRange, NULL);

  revmapBlk = revmap_get_blkno(revmap, heapBlk);
  if (!BlockNumberIsValid(revmapBlk))
  {
                                                                     
    brinRevmapTerminate(revmap);
    return true;
  }

                                                                    
  revmapBuf = brinLockRevmapPageForUpdate(revmap, heapBlk);
  revmapPg = BufferGetPage(revmapBuf);
  revmapOffset = HEAPBLK_TO_REVMAP_INDEX(revmap->rm_pagesPerRange, heapBlk);

  contents = (RevmapContents *)PageGetContents(revmapPg);
  iptr = contents->rm_tids;
  iptr += revmapOffset;

  if (!ItemPointerIsValid(iptr))
  {
                                                          
    LockBuffer(revmapBuf, BUFFER_LOCK_UNLOCK);
    brinRevmapTerminate(revmap);
    return true;
  }

  regBuf = ReadBuffer(idxrel, ItemPointerGetBlockNumber(iptr));
  LockBuffer(regBuf, BUFFER_LOCK_EXCLUSIVE);
  regPg = BufferGetPage(regBuf);
     
                                                                     
                              
     

                                                                      
  if (!BRIN_IS_REGULAR_PAGE(regPg))
  {
    LockBuffer(revmapBuf, BUFFER_LOCK_UNLOCK);
    LockBuffer(regBuf, BUFFER_LOCK_UNLOCK);
    brinRevmapTerminate(revmap);
    return false;
  }

  regOffset = ItemPointerGetOffsetNumber(iptr);
  if (regOffset > PageGetMaxOffsetNumber(regPg))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("corrupted BRIN index: inconsistent range map")));
  }

  lp = PageGetItemId(regPg, regOffset);
  if (!ItemIdIsUsed(lp))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("corrupted BRIN index: inconsistent range map")));
  }

     
                                                                            
                                                                             
                                                                           
                                                             
     

  START_CRIT_SECTION();

  ItemPointerSetInvalid(&invalidIptr);
  brinSetHeapBlockItemptr(revmapBuf, revmap->rm_pagesPerRange, heapBlk, invalidIptr);
  PageIndexTupleDeleteNoCompact(regPg, regOffset);
                                     

  MarkBufferDirty(regBuf);
  MarkBufferDirty(revmapBuf);

  if (RelationNeedsWAL(idxrel))
  {
    xl_brin_desummarize xlrec;
    XLogRecPtr recptr;

    xlrec.pagesPerRange = revmap->rm_pagesPerRange;
    xlrec.heapBlk = heapBlk;
    xlrec.regOffset = regOffset;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfBrinDesummarize);
    XLogRegisterBuffer(0, revmapBuf, 0);
    XLogRegisterBuffer(1, regBuf, REGBUF_STANDARD);
    recptr = XLogInsert(RM_BRIN_ID, XLOG_BRIN_DESUMMARIZE);
    PageSetLSN(revmapPg, recptr);
    PageSetLSN(regPg, recptr);
  }

  END_CRIT_SECTION();

  UnlockReleaseBuffer(regBuf);
  LockBuffer(revmapBuf, BUFFER_LOCK_UNLOCK);
  brinRevmapTerminate(revmap);

  return true;
}

   
                                                                           
                                                                               
                       
   
static BlockNumber
revmap_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk)
{
  BlockNumber targetblk;

                                                             
  targetblk = HEAPBLK_TO_REVMAP_BLK(revmap->rm_pagesPerRange, heapBlk) + 1;

                                                         
  if (targetblk <= revmap->rm_lastRevmapPage)
  {
    return targetblk;
  }

  return InvalidBlockNumber;
}

   
                                                                            
                                                                            
                                                                             
                                                                       
   
static Buffer
revmap_get_buffer(BrinRevmap *revmap, BlockNumber heapBlk)
{
  BlockNumber mapBlk;

                                                                   
  mapBlk = revmap_get_blkno(revmap, heapBlk);

  if (mapBlk == InvalidBlockNumber)
  {
    elog(ERROR, "revmap does not cover heap block %u", heapBlk);
  }

                                                         
  Assert(mapBlk != BRIN_METAPAGE_BLKNO && mapBlk <= revmap->rm_lastRevmapPage);

     
                                                                           
                                                                             
                                          
     
  if (revmap->rm_currBuf == InvalidBuffer || mapBlk != BufferGetBlockNumber(revmap->rm_currBuf))
  {
    if (revmap->rm_currBuf != InvalidBuffer)
    {
      ReleaseBuffer(revmap->rm_currBuf);
    }

    revmap->rm_currBuf = ReadBuffer(revmap->rm_irel, mapBlk);
  }

  return revmap->rm_currBuf;
}

   
                                                                           
                                                                              
                           
   
static BlockNumber
revmap_extend_and_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk)
{
  BlockNumber targetblk;

                                                             
  targetblk = HEAPBLK_TO_REVMAP_BLK(revmap->rm_pagesPerRange, heapBlk) + 1;

                                       
  while (targetblk > revmap->rm_lastRevmapPage)
  {
    CHECK_FOR_INTERRUPTS();
    revmap_physical_extend(revmap);
  }

  return targetblk;
}

   
                                                                                
                                                                                
   
static void
revmap_physical_extend(BrinRevmap *revmap)
{
  Buffer buf;
  Page page;
  Page metapage;
  BrinMetaPageData *metadata;
  BlockNumber mapBlk;
  BlockNumber nblocks;
  Relation irel = revmap->rm_irel;
  bool needLock = !RELATION_IS_LOCAL(irel);

     
                                                                            
                                                                             
                                                                   
     
  LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_EXCLUSIVE);
  metapage = BufferGetPage(revmap->rm_metaBuf);
  metadata = (BrinMetaPageData *)PageGetContents(metapage);

     
                                                                      
                                                                
     
  if (metadata->lastRevmapPage != revmap->rm_lastRevmapPage)
  {
    revmap->rm_lastRevmapPage = metadata->lastRevmapPage;
    LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
    return;
  }
  mapBlk = metadata->lastRevmapPage + 1;

  nblocks = RelationGetNumberOfBlocks(irel);
  if (mapBlk < nblocks)
  {
    buf = ReadBuffer(irel, mapBlk);
    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
    page = BufferGetPage(buf);
  }
  else
  {
    if (needLock)
    {
      LockRelationForExtension(irel, ExclusiveLock);
    }

    buf = ReadBuffer(irel, P_NEW);
    if (BufferGetBlockNumber(buf) != mapBlk)
    {
         
                                                               
                                                                       
                                                                       
                                              
         
      if (needLock)
      {
        UnlockRelationForExtension(irel, ExclusiveLock);
      }
      LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
      ReleaseBuffer(buf);
      return;
    }
    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
    page = BufferGetPage(buf);

    if (needLock)
    {
      UnlockRelationForExtension(irel, ExclusiveLock);
    }
  }

                                                          
  if (!PageIsNew(page) && !BRIN_IS_REGULAR_PAGE(page))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("unexpected page type 0x%04X in BRIN index \"%s\" block %u", BrinPageType(page), RelationGetRelationName(irel), BufferGetBlockNumber(buf))));
  }

                                                      
  if (brin_start_evacuating_page(irel, buf))
  {
    LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
    brin_evacuate_page(irel, revmap->rm_pagesPerRange, revmap, buf);

                                
    return;
  }

     
                                                                             
                                                                 
     
  START_CRIT_SECTION();

                                                                   
  brin_page_init(page, BRIN_PAGETYPE_REVMAP);
  MarkBufferDirty(buf);

  metadata->lastRevmapPage = mapBlk;

     
                                                                         
                                                                          
                                                                             
                                                                         
                               
     
  ((PageHeader)metapage)->pd_lower = ((char *)metadata + sizeof(BrinMetaPageData)) - (char *)metapage;

  MarkBufferDirty(revmap->rm_metaBuf);

  if (RelationNeedsWAL(revmap->rm_irel))
  {
    xl_brin_revmap_extend xlrec;
    XLogRecPtr recptr;

    xlrec.targetBlk = mapBlk;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfBrinRevmapExtend);
    XLogRegisterBuffer(0, revmap->rm_metaBuf, REGBUF_STANDARD);

    XLogRegisterBuffer(1, buf, REGBUF_WILL_INIT);

    recptr = XLogInsert(RM_BRIN_ID, XLOG_BRIN_REVMAP_EXTEND);
    PageSetLSN(metapage, recptr);
    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);

  UnlockReleaseBuffer(buf);
}
