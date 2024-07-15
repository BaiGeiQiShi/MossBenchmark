                                                                            
   
              
                                
   
   
                                                                         
                                                                        
   
                  
                                         
                                                                            
   
#include "postgres.h"

#include "access/bufmask.h"
#include "access/gist_private.h"
#include "access/gistxlog.h"
#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "storage/procarray.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static MemoryContext opCtx;                                    

   
                                                               
   
                                                                            
                                                                           
                                                                               
                                                                        
                                                                           
                                                                          
            
   
static void
gistRedoClearFollowRight(XLogReaderState *record, uint8 block_id)
{
  XLogRecPtr lsn = record->EndRecPtr;
  Buffer buffer;
  Page page;
  XLogRedoAction action;

     
                                                                            
                                                                       
     
  action = XLogReadBufferForRedo(record, block_id, &buffer);
  if (action == BLK_NEEDS_REDO || action == BLK_RESTORED)
  {
    page = BufferGetPage(buffer);

    GistPageSetNSN(page, lsn);
    GistClearFollowRight(page);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

   
                                            
   
static void
gistRedoPageUpdateRecord(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  gistxlogPageUpdate *xldata = (gistxlogPageUpdate *)XLogRecGetData(record);
  Buffer buffer;
  Page page;

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    char *begin;
    char *data;
    Size datalen;
    int ninserted PG_USED_FOR_ASSERTS_ONLY = 0;

    data = begin = XLogRecGetBlockData(record, 0, &datalen);

    page = (Page)BufferGetPage(buffer);

    if (xldata->ntodelete == 1 && xldata->ntoinsert == 1)
    {
         
                                                                    
                                                                       
         
      OffsetNumber offnum = *((OffsetNumber *)data);
      IndexTuple itup;
      Size itupsize;

      data += sizeof(OffsetNumber);
      itup = (IndexTuple)data;
      itupsize = IndexTupleSize(itup);
      if (!PageIndexTupleOverwrite(page, offnum, (Item)itup, itupsize))
      {
        elog(ERROR, "failed to add item to GiST index page, size %d bytes", (int)itupsize);
      }
      data += itupsize;
                                                          
      Assert(data - begin == datalen);
                                                         
      ninserted++;
    }
    else if (xldata->ntodelete > 0)
    {
                                               
      OffsetNumber *todelete = (OffsetNumber *)data;

      data += sizeof(OffsetNumber) * xldata->ntodelete;

      PageIndexMultiDelete(page, todelete, xldata->ntodelete);
      if (GistPageIsLeaf(page))
      {
        GistMarkTuplesDeleted(page);
      }
    }

                               
    if (data - begin < datalen)
    {
      OffsetNumber off = (PageIsEmpty(page)) ? FirstOffsetNumber : OffsetNumberNext(PageGetMaxOffsetNumber(page));

      while (data - begin < datalen)
      {
        IndexTuple itup = (IndexTuple)data;
        Size sz = IndexTupleSize(itup);
        OffsetNumber l;

        data += sz;

        l = PageAddItem(page, (Item)itup, sz, off, false, false);
        if (l == InvalidOffsetNumber)
        {
          elog(ERROR, "failed to add item to GiST index page, size %d bytes", (int)sz);
        }
        off++;
        ninserted++;
      }
    }

                                                                    
    Assert(ninserted == xldata->ntoinsert);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }

     
                                              
     
                                                                             
                                                                        
                                          
     
  if (XLogRecHasBlockRef(record, 1))
  {
    gistRedoClearFollowRight(record, 1);
  }

  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

   
                                                                               
                   
   
static void
gistRedoDeleteRecord(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  gistxlogDelete *xldata = (gistxlogDelete *)XLogRecGetData(record);
  Buffer buffer;
  Page page;

     
                                                                        
                      
     
                                                                             
                                                                        
                                                                       
                                                                           
                                                                        
                                                              
     
  if (InHotStandby)
  {
    RelFileNode rnode;

    XLogRecGetBlockTag(record, 0, &rnode, NULL, NULL);

    ResolveRecoveryConflictWithSnapshot(xldata->latestRemovedXid, rnode);
  }

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    page = (Page)BufferGetPage(buffer);

    if (XLogRecGetDataLen(record) > SizeOfGistxlogDelete)
    {
      OffsetNumber *todelete;

      todelete = (OffsetNumber *)((char *)xldata + SizeOfGistxlogDelete);

      PageIndexMultiDelete(page, todelete, xldata->ntodelete);
    }

    GistClearPageHasGarbage(page);
    GistMarkTuplesDeleted(page);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }

  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

   
                                       
   
static IndexTuple *
decodePageSplitRecord(char *begin, int len, int *n)
{
  char *ptr;
  int i = 0;
  IndexTuple *tuples;

                                    
  memcpy(n, begin, sizeof(int));
  ptr = begin + sizeof(int);

  tuples = palloc(*n * sizeof(IndexTuple));

  for (i = 0; i < *n; i++)
  {
    Assert(ptr - begin < len);
    tuples[i] = (IndexTuple)ptr;
    ptr += IndexTupleSize((IndexTuple)ptr);
  }
  Assert(ptr - begin == len);

  return tuples;
}

static void
gistRedoPageSplitRecord(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  gistxlogPageSplit *xldata = (gistxlogPageSplit *)XLogRecGetData(record);
  Buffer firstbuffer = InvalidBuffer;
  Buffer buffer;
  Page page;
  int i;
  bool isrootsplit = false;

     
                                                                       
                                                                           
                                                                          
                                                                          
                                           
     

                             
  for (i = 0; i < xldata->npage; i++)
  {
    int flags;
    char *data;
    Size datalen;
    int num;
    BlockNumber blkno;
    IndexTuple *tuples;

    XLogRecGetBlockTag(record, i + 1, NULL, NULL, &blkno);
    if (blkno == GIST_ROOT_BLKNO)
    {
      Assert(i == 0);
      isrootsplit = true;
    }

    buffer = XLogInitBufferForRedo(record, i + 1);
    page = (Page)BufferGetPage(buffer);
    data = XLogRecGetBlockData(record, i + 1, &datalen);

    tuples = decodePageSplitRecord(data, datalen, &num);

                          
    if (xldata->origleaf && blkno != GIST_ROOT_BLKNO)
    {
      flags = F_LEAF;
    }
    else
    {
      flags = 0;
    }
    GISTInitBuffer(buffer, flags);

                     
    gistfillbuffer(page, tuples, num, FirstOffsetNumber);

    if (blkno == GIST_ROOT_BLKNO)
    {
      GistPageGetOpaque(page)->rightlink = InvalidBlockNumber;
      GistPageSetNSN(page, xldata->orignsn);
      GistClearFollowRight(page);
    }
    else
    {
      if (i < xldata->npage - 1)
      {
        BlockNumber nextblkno;

        XLogRecGetBlockTag(record, i + 2, NULL, NULL, &nextblkno);
        GistPageGetOpaque(page)->rightlink = nextblkno;
      }
      else
      {
        GistPageGetOpaque(page)->rightlink = xldata->origrlink;
      }
      GistPageSetNSN(page, xldata->orignsn);
      if (i < xldata->npage - 1 && !isrootsplit && xldata->markfollowright)
      {
        GistMarkFollowRight(page);
      }
      else
      {
        GistClearFollowRight(page);
      }
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);

    if (i == 0)
    {
      firstbuffer = buffer;
    }
    else
    {
      UnlockReleaseBuffer(buffer);
    }
  }

                                                        
  if (XLogRecHasBlockRef(record, 0))
  {
    gistRedoClearFollowRight(record, 0);
  }

                                               
  UnlockReleaseBuffer(firstbuffer);
}

                        
static void
gistRedoPageDelete(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  gistxlogPageDelete *xldata = (gistxlogPageDelete *)XLogRecGetData(record);
  Buffer parentBuffer;
  Buffer leafBuffer;

  if (XLogReadBufferForRedo(record, 0, &leafBuffer) == BLK_NEEDS_REDO)
  {
    Page page = (Page)BufferGetPage(leafBuffer);

    GistPageSetDeleted(page, xldata->deleteXid);

    PageSetLSN(page, lsn);
    MarkBufferDirty(leafBuffer);
  }

  if (XLogReadBufferForRedo(record, 1, &parentBuffer) == BLK_NEEDS_REDO)
  {
    Page page = (Page)BufferGetPage(parentBuffer);

    PageIndexTupleDelete(page, xldata->downlinkOffset);

    PageSetLSN(page, lsn);
    MarkBufferDirty(parentBuffer);
  }

  if (BufferIsValid(parentBuffer))
  {
    UnlockReleaseBuffer(parentBuffer);
  }
  if (BufferIsValid(leafBuffer))
  {
    UnlockReleaseBuffer(leafBuffer);
  }
}

static void
gistRedoPageReuse(XLogReaderState *record)
{
  gistxlogPageReuse *xlrec = (gistxlogPageReuse *)XLogRecGetData(record);

     
                                                                        
                                                                 
     
                                                                 
                                                                            
                                                                   
                                                                       
                         
     
  if (InHotStandby)
  {
    FullTransactionId latestRemovedFullXid = xlrec->latestRemovedFullXid;
    FullTransactionId nextFullXid = ReadNextFullTransactionId();
    uint64 diff;

       
                                                              
                                                                        
                                                                          
                                                              
       
    nextFullXid = ReadNextFullTransactionId();
    diff = U64FromFullTransactionId(nextFullXid) - U64FromFullTransactionId(latestRemovedFullXid);
    if (diff < MaxTransactionId / 2)
    {
      TransactionId latestRemovedXid;

      latestRemovedXid = XidFromFullTransactionId(latestRemovedFullXid);
      ResolveRecoveryConflictWithSnapshot(latestRemovedXid, xlrec->node);
    }
  }
}

void
gist_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
  MemoryContext oldCxt;

     
                                                                         
                                                                           
                                                            
     

  oldCxt = MemoryContextSwitchTo(opCtx);
  switch (info)
  {
  case XLOG_GIST_PAGE_UPDATE:
    gistRedoPageUpdateRecord(record);
    break;
  case XLOG_GIST_DELETE:
    gistRedoDeleteRecord(record);
    break;
  case XLOG_GIST_PAGE_REUSE:
    gistRedoPageReuse(record);
    break;
  case XLOG_GIST_PAGE_SPLIT:
    gistRedoPageSplitRecord(record);
    break;
  case XLOG_GIST_PAGE_DELETE:
    gistRedoPageDelete(record);
    break;
  default:
    elog(PANIC, "gist_redo: unknown op code %u", info);
  }

  MemoryContextSwitchTo(oldCxt);
  MemoryContextReset(opCtx);
}

void
gist_xlog_startup(void)
{
  opCtx = createTempGistContext();
}

void
gist_xlog_cleanup(void)
{
  MemoryContextDelete(opCtx);
}

   
                                                             
   
void
gist_mask(char *pagedata, BlockNumber blkno)
{
  Page page = (Page)pagedata;

  mask_page_lsn_and_checksum(page);

  mask_page_hint_bits(page);
  mask_unused_space(page);

     
                                                                           
                                           
     
  GistPageSetNSN(page, (uint64)MASK_MARKER);

     
                                                                       
                                                                       
     
  GistMarkFollowRight(page);

  if (GistPageIsLeaf(page))
  {
       
                                                                         
                                                                        
                                    
       
    mask_lp_flags(page);
  }

     
                                                                          
                             
     
  GistClearPageHasGarbage(page);
}

   
                                     
   
XLogRecPtr
gistXLogSplit(bool page_is_leaf, SplitedPageLayout *dist, BlockNumber origrlink, GistNSN orignsn, Buffer leftchildbuf, bool markfollowright)
{
  gistxlogPageSplit xlrec;
  SplitedPageLayout *ptr;
  int npage = 0;
  XLogRecPtr recptr;
  int i;

  for (ptr = dist; ptr; ptr = ptr->next)
  {
    npage++;
  }

  xlrec.origrlink = origrlink;
  xlrec.orignsn = orignsn;
  xlrec.origleaf = page_is_leaf;
  xlrec.npage = (uint16)npage;
  xlrec.markfollowright = markfollowright;

  XLogBeginInsert();

     
                                                                      
                                                         
     
  if (BufferIsValid(leftchildbuf))
  {
    XLogRegisterBuffer(0, leftchildbuf, REGBUF_STANDARD);
  }

     
                                                                
                                                                        
                                                                           
                                                                    
                                                
     
  XLogRegisterData((char *)&xlrec, sizeof(gistxlogPageSplit));

  i = 1;
  for (ptr = dist; ptr; ptr = ptr->next)
  {
    XLogRegisterBuffer(i, ptr->buffer, REGBUF_WILL_INIT);
    XLogRegisterBufData(i, (char *)&(ptr->block.num), sizeof(int));
    XLogRegisterBufData(i, (char *)ptr->list, ptr->lenlist);
    i++;
  }

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_SPLIT);

  return recptr;
}

   
                                                                               
                                  
   
XLogRecPtr
gistXLogPageDelete(Buffer buffer, FullTransactionId xid, Buffer parentBuffer, OffsetNumber downlinkOffset)
{
  gistxlogPageDelete xlrec;
  XLogRecPtr recptr;

  xlrec.deleteXid = xid;
  xlrec.downlinkOffset = downlinkOffset;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfGistxlogPageDelete);

  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
  XLogRegisterBuffer(1, parentBuffer, REGBUF_STANDARD);

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_DELETE);

  return recptr;
}

   
                                                    
   
void
gistXLogPageReuse(Relation rel, BlockNumber blkno, FullTransactionId latestRemovedXid)
{
  gistxlogPageReuse xlrec_reuse;

     
                                                                          
                                                                             
                                     
     

                  
  xlrec_reuse.node = rel->rd_node;
  xlrec_reuse.block = blkno;
  xlrec_reuse.latestRemovedFullXid = latestRemovedXid;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec_reuse, SizeOfGistxlogPageReuse);

  XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_REUSE);
}

   
                                                                          
                                                                           
   
                                                                        
                                                                     
   
                                                                            
                                                                               
                                             
   
XLogRecPtr
gistXLogUpdate(Buffer buffer, OffsetNumber *todelete, int ntodelete, IndexTuple *itup, int ituplen, Buffer leftchildbuf)
{
  gistxlogPageUpdate xlrec;
  int i;
  XLogRecPtr recptr;

  xlrec.ntodelete = ntodelete;
  xlrec.ntoinsert = ituplen;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, sizeof(gistxlogPageUpdate));

  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
  XLogRegisterBufData(0, (char *)todelete, sizeof(OffsetNumber) * ntodelete);

                  
  for (i = 0; i < ituplen; i++)
  {
    XLogRegisterBufData(0, (char *)(itup[i]), IndexTupleSize(itup[i]));
  }

     
                                                                      
                                                         
     
  if (BufferIsValid(leftchildbuf))
  {
    XLogRegisterBuffer(1, leftchildbuf, REGBUF_STANDARD);
  }

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_PAGE_UPDATE);

  return recptr;
}

   
                                                                             
                                                                                
                                                                          
                                               
   
XLogRecPtr
gistXLogDelete(Buffer buffer, OffsetNumber *todelete, int ntodelete, TransactionId latestRemovedXid)
{
  gistxlogDelete xlrec;
  XLogRecPtr recptr;

  xlrec.latestRemovedXid = latestRemovedXid;
  xlrec.ntodelete = ntodelete;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfGistxlogDelete);

     
                                                                        
                                                                           
     
  XLogRegisterData((char *)todelete, ntodelete * sizeof(OffsetNumber));

  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

  recptr = XLogInsert(RM_GIST_ID, XLOG_GIST_DELETE);

  return recptr;
}
