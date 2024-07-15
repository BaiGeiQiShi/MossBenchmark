   
               
                                          
   
                                                                         
                                                                        
   
                  
                                         
   
#include "postgres.h"

#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_xlog.h"
#include "access/bufmask.h"
#include "access/xlogutils.h"

   
                        
   
static void
brin_xlog_createidx(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_brin_createidx *xlrec = (xl_brin_createidx *)XLogRecGetData(record);
  Buffer buf;
  Page page;

                                  
  buf = XLogInitBufferForRedo(record, 0);
  Assert(BufferIsValid(buf));
  page = (Page)BufferGetPage(buf);
  brin_metapage_init(page, xlrec->pagesPerRange, xlrec->version);
  PageSetLSN(page, lsn);
  MarkBufferDirty(buf);
  UnlockReleaseBuffer(buf);
}

   
                                                                             
           
   
static void
brin_xlog_insert_update(XLogReaderState *record, xl_brin_insert *xlrec)
{
  XLogRecPtr lsn = record->EndRecPtr;
  Buffer buffer;
  BlockNumber regpgno;
  Page page;
  XLogRedoAction action;

     
                                                                            
                        
     
  if (XLogRecGetInfo(record) & XLOG_BRIN_INIT_PAGE)
  {
    buffer = XLogInitBufferForRedo(record, 0);
    page = BufferGetPage(buffer);
    brin_page_init(page, BRIN_PAGETYPE_REGULAR);
    action = BLK_NEEDS_REDO;
  }
  else
  {
    action = XLogReadBufferForRedo(record, 0, &buffer);
  }

                                                 
  regpgno = BufferGetBlockNumber(buffer);

                                           
  if (action == BLK_NEEDS_REDO)
  {
    OffsetNumber offnum;
    BrinTuple *tuple;
    Size tuplen;

    tuple = (BrinTuple *)XLogRecGetBlockData(record, 0, &tuplen);

    Assert(tuple->bt_blkno == xlrec->heapBlk);

    page = (Page)BufferGetPage(buffer);
    offnum = xlrec->offnum;
    if (PageGetMaxOffsetNumber(page) + 1 < offnum)
    {
      elog(PANIC, "brin_xlog_insert_update: invalid max offset number");
    }

    offnum = PageAddItem(page, (Item)tuple, tuplen, offnum, true, false);
    if (offnum == InvalidOffsetNumber)
    {
      elog(PANIC, "brin_xlog_insert_update: failed to add tuple");
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

                         
  action = XLogReadBufferForRedo(record, 1, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
    ItemPointerData tid;

    ItemPointerSet(&tid, regpgno, xlrec->offnum);
    page = (Page)BufferGetPage(buffer);

    brinSetHeapBlockItemptr(buffer, xlrec->pagesPerRange, xlrec->heapBlk, tid);
    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

                                   
}

   
                                 
   
static void
brin_xlog_insert(XLogReaderState *record)
{
  xl_brin_insert *xlrec = (xl_brin_insert *)XLogRecGetData(record);

  brin_xlog_insert_update(record, xlrec);
}

   
                              
   
static void
brin_xlog_update(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_brin_update *xlrec = (xl_brin_update *)XLogRecGetData(record);
  Buffer buffer;
  XLogRedoAction action;

                                  
  action = XLogReadBufferForRedo(record, 2, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
    Page page;
    OffsetNumber offnum;

    page = (Page)BufferGetPage(buffer);

    offnum = xlrec->oldOffnum;

    PageIndexTupleDeleteNoCompact(page, offnum);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }

                                                                          
  brin_xlog_insert_update(record, &xlrec->insert);

  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

   
                                    
   
static void
brin_xlog_samepage_update(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_brin_samepage_update *xlrec;
  Buffer buffer;
  XLogRedoAction action;

  xlrec = (xl_brin_samepage_update *)XLogRecGetData(record);
  action = XLogReadBufferForRedo(record, 0, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
    Size tuplen;
    BrinTuple *brintuple;
    Page page;
    OffsetNumber offnum;

    brintuple = (BrinTuple *)XLogRecGetBlockData(record, 0, &tuplen);

    page = (Page)BufferGetPage(buffer);

    offnum = xlrec->offnum;

    if (!PageIndexTupleOverwrite(page, offnum, (Item)brintuple, tuplen))
    {
      elog(PANIC, "brin_xlog_samepage_update: failed to replace tuple");
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

                                   
}

   
                                  
   
static void
brin_xlog_revmap_extend(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_brin_revmap_extend *xlrec;
  Buffer metabuf;
  Buffer buf;
  Page page;
  BlockNumber targetBlk;
  XLogRedoAction action;

  xlrec = (xl_brin_revmap_extend *)XLogRecGetData(record);

  XLogRecGetBlockTag(record, 1, NULL, NULL, &targetBlk);
  Assert(xlrec->targetBlk == targetBlk);

                           
  action = XLogReadBufferForRedo(record, 0, &metabuf);
  if (action == BLK_NEEDS_REDO)
  {
    Page metapg;
    BrinMetaPageData *metadata;

    metapg = BufferGetPage(metabuf);
    metadata = (BrinMetaPageData *)PageGetContents(metapg);

    Assert(metadata->lastRevmapPage == xlrec->targetBlk - 1);
    metadata->lastRevmapPage = xlrec->targetBlk;

    PageSetLSN(metapg, lsn);

       
                                                                           
                                                                 
                                                                   
                                                                          
                                                         
       
    ((PageHeader)metapg)->pd_lower = ((char *)metadata + sizeof(BrinMetaPageData)) - (char *)metapg;

    MarkBufferDirty(metabuf);
  }

     
                                                                            
                 
     

  buf = XLogInitBufferForRedo(record, 1);
  page = (Page)BufferGetPage(buf);
  brin_page_init(page, BRIN_PAGETYPE_REVMAP);

  PageSetLSN(page, lsn);
  MarkBufferDirty(buf);

  UnlockReleaseBuffer(buf);
  if (BufferIsValid(metabuf))
  {
    UnlockReleaseBuffer(metabuf);
  }
}

static void
brin_xlog_desummarize_page(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_brin_desummarize *xlrec;
  Buffer buffer;
  XLogRedoAction action;

  xlrec = (xl_brin_desummarize *)XLogRecGetData(record);

                         
  action = XLogReadBufferForRedo(record, 0, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
    ItemPointerData iptr;

    ItemPointerSetInvalid(&iptr);
    brinSetHeapBlockItemptr(buffer, xlrec->pagesPerRange, xlrec->heapBlk, iptr);

    PageSetLSN(BufferGetPage(buffer), lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

                                                       
  action = XLogReadBufferForRedo(record, 1, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
    Page regPg = BufferGetPage(buffer);

    PageIndexTupleDeleteNoCompact(regPg, xlrec->regOffset);

    PageSetLSN(regPg, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

void
brin_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  switch (info & XLOG_BRIN_OPMASK)
  {
  case XLOG_BRIN_CREATE_INDEX:
    brin_xlog_createidx(record);
    break;
  case XLOG_BRIN_INSERT:
    brin_xlog_insert(record);
    break;
  case XLOG_BRIN_UPDATE:
    brin_xlog_update(record);
    break;
  case XLOG_BRIN_SAMEPAGE_UPDATE:
    brin_xlog_samepage_update(record);
    break;
  case XLOG_BRIN_REVMAP_EXTEND:
    brin_xlog_revmap_extend(record);
    break;
  case XLOG_BRIN_DESUMMARIZE:
    brin_xlog_desummarize_page(record);
    break;
  default:
    elog(PANIC, "brin_redo: unknown op code %u", info);
  }
}

   
                                                     
   
void
brin_mask(char *pagedata, BlockNumber blkno)
{
  Page page = (Page)pagedata;
  PageHeader pagehdr = (PageHeader)page;

  mask_page_lsn_and_checksum(page);

  mask_page_hint_bits(page);

     
                                                                       
                                                                            
                         
     
  if (BRIN_IS_REGULAR_PAGE(page) || (BRIN_IS_META_PAGE(page) && pagehdr->pd_lower > SizeOfPageHeaderData))
  {
    mask_unused_space(page);
  }

     
                                                                             
                                                             
     
  BrinPageFlags(page) &= ~BRIN_EVACUATE_PAGE;
}
