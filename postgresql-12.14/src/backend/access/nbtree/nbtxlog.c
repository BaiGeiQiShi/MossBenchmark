                                                                            
   
             
                                  
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/bufmask.h"
#include "access/nbtree.h"
#include "access/nbtxlog.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "access/xlogutils.h"
#include "storage/procarray.h"
#include "miscadmin.h"

   
                                                               
   
                                                                        
                                                                         
                                                                         
                                                                        
   
static void
_bt_restore_page(Page page, char *from, int len)
{
  IndexTupleData itupdata;
  Size itemsz;
  char *end = from + len;
  Item items[MaxIndexTuplesPerPage];
  uint16 itemsizes[MaxIndexTuplesPerPage];
  int i;
  int nitems;

     
                                                                             
                                                                         
                                               
     
  i = 0;
  while (from < end)
  {
       
                                                                     
                                                                         
                                                                      
                                                                     
                              
       
    memcpy(&itupdata, from, sizeof(IndexTupleData));
    itemsz = IndexTupleSize(&itupdata);
    itemsz = MAXALIGN(itemsz);

    items[i] = (Item)from;
    itemsizes[i] = itemsz;
    i++;

    from += itemsz;
  }
  nitems = i;

  for (i = nitems - 1; i >= 0; i--)
  {
    if (PageAddItem(page, items[i], itemsizes[i], nitems - i, false, false) == InvalidOffsetNumber)
    {
      elog(PANIC, "_bt_restore_page: cannot add item to page");
    }
  }
}

static void
_bt_restore_meta(XLogReaderState *record, uint8 block_id)
{
  XLogRecPtr lsn = record->EndRecPtr;
  Buffer metabuf;
  Page metapg;
  BTMetaPageData *md;
  BTPageOpaque pageop;
  xl_btree_metadata *xlrec;
  char *ptr;
  Size len;

  metabuf = XLogInitBufferForRedo(record, block_id);
  ptr = XLogRecGetBlockData(record, block_id, &len);

  Assert(len == sizeof(xl_btree_metadata));
  Assert(BufferGetBlockNumber(metabuf) == BTREE_METAPAGE);
  xlrec = (xl_btree_metadata *)ptr;
  metapg = BufferGetPage(metabuf);

  _bt_pageinit(metapg, BufferGetPageSize(metabuf));

  md = BTPageGetMeta(metapg);
  md->btm_magic = BTREE_MAGIC;
  md->btm_version = xlrec->version;
  md->btm_root = xlrec->root;
  md->btm_level = xlrec->level;
  md->btm_fastroot = xlrec->fastroot;
  md->btm_fastlevel = xlrec->fastlevel;
                                                                   
  Assert(md->btm_version >= BTREE_NOVAC_VERSION);
  md->btm_oldest_btpo_xact = xlrec->oldest_btpo_xact;
  md->btm_last_cleanup_num_heap_tuples = xlrec->last_cleanup_num_heap_tuples;

  pageop = (BTPageOpaque)PageGetSpecialPointer(metapg);
  pageop->btpo_flags = BTP_META;

     
                                                                         
                                                                          
               
     
  ((PageHeader)metapg)->pd_lower = ((char *)md + sizeof(BTMetaPageData)) - (char *)metapg;

  PageSetLSN(metapg, lsn);
  MarkBufferDirty(metabuf);
  UnlockReleaseBuffer(metabuf);
}

   
                                                                       
   
                                                                           
                                                                 
   
static void
_bt_clear_incomplete_split(XLogReaderState *record, uint8 block_id)
{
  XLogRecPtr lsn = record->EndRecPtr;
  Buffer buf;

  if (XLogReadBufferForRedo(record, block_id, &buf) == BLK_NEEDS_REDO)
  {
    Page page = (Page)BufferGetPage(buf);
    BTPageOpaque pageop = (BTPageOpaque)PageGetSpecialPointer(page);

    Assert(P_INCOMPLETE_SPLIT(pageop));
    pageop->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;

    PageSetLSN(page, lsn);
    MarkBufferDirty(buf);
  }
  if (BufferIsValid(buf))
  {
    UnlockReleaseBuffer(buf);
  }
}

static void
btree_xlog_insert(bool isleaf, bool ismeta, XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_btree_insert *xlrec = (xl_btree_insert *)XLogRecGetData(record);
  Buffer buffer;
  Page page;

     
                                                                             
                                                                         
                                                                         
                                                                       
                                                                         
                                                                          
                                  
     
  if (!isleaf)
  {
    _bt_clear_incomplete_split(record, 1);
  }
  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    Size datalen;
    char *datapos = XLogRecGetBlockData(record, 0, &datalen);

    page = BufferGetPage(buffer);

    if (PageAddItem(page, (Item)datapos, datalen, xlrec->offnum, false, false) == InvalidOffsetNumber)
    {
      elog(PANIC, "btree_xlog_insert: failed to add item");
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

     
                                                                             
                                                                    
                                                                      
                                                                          
                                      
     
  if (ismeta)
  {
    _bt_restore_meta(record, 2);
  }
}

static void
btree_xlog_split(bool onleft, XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_btree_split *xlrec = (xl_btree_split *)XLogRecGetData(record);
  bool isleaf = (xlrec->level == 0);
  Buffer lbuf;
  Buffer rbuf;
  Page rpage;
  BTPageOpaque ropaque;
  char *datapos;
  Size datalen;
  BlockNumber leftsib;
  BlockNumber rightsib;
  BlockNumber rnext;

  XLogRecGetBlockTag(record, 0, NULL, NULL, &leftsib);
  XLogRecGetBlockTag(record, 1, NULL, NULL, &rightsib);
  if (!XLogRecGetBlockTag(record, 2, NULL, NULL, &rnext))
  {
    rnext = P_NONE;
  }

     
                                                                           
                                                                           
                                     
     
  if (!isleaf)
  {
    _bt_clear_incomplete_split(record, 3);
  }

                                                         
  rbuf = XLogInitBufferForRedo(record, 1);
  datapos = XLogRecGetBlockData(record, 1, &datalen);
  rpage = (Page)BufferGetPage(rbuf);

  _bt_pageinit(rpage, BufferGetPageSize(rbuf));
  ropaque = (BTPageOpaque)PageGetSpecialPointer(rpage);

  ropaque->btpo_prev = leftsib;
  ropaque->btpo_next = rnext;
  ropaque->btpo.level = xlrec->level;
  ropaque->btpo_flags = isleaf ? BTP_LEAF : 0;
  ropaque->btpo_cycleid = 0;

  _bt_restore_page(rpage, datapos, datalen);

  PageSetLSN(rpage, lsn);
  MarkBufferDirty(rbuf);

                                                    
  if (XLogReadBufferForRedo(record, 0, &lbuf) == BLK_NEEDS_REDO)
  {
       
                                                                         
                                                                           
                                                                         
                                                                       
                                                                       
                                
       
    Page lpage = (Page)BufferGetPage(lbuf);
    BTPageOpaque lopaque = (BTPageOpaque)PageGetSpecialPointer(lpage);
    OffsetNumber off;
    IndexTuple newitem = NULL, left_hikey = NULL;
    Size newitemsz = 0, left_hikeysz = 0;
    Page newlpage;
    OffsetNumber leftoff;

    datapos = XLogRecGetBlockData(record, 0, &datalen);

    if (onleft)
    {
      newitem = (IndexTuple)datapos;
      newitemsz = MAXALIGN(IndexTupleSize(newitem));
      datapos += newitemsz;
      datalen -= newitemsz;
    }

                                                                     
    left_hikey = (IndexTuple)datapos;
    left_hikeysz = MAXALIGN(IndexTupleSize(left_hikey));
    datapos += left_hikeysz;
    datalen -= left_hikeysz;

    Assert(datalen == 0);

    newlpage = PageGetTempPageCopySpecial(lpage);

                      
    leftoff = P_HIKEY;
    if (PageAddItem(newlpage, (Item)left_hikey, left_hikeysz, P_HIKEY, false, false) == InvalidOffsetNumber)
    {
      elog(PANIC, "failed to add high key to left page after split");
    }
    leftoff = OffsetNumberNext(leftoff);

    for (off = P_FIRSTDATAKEY(lopaque); off < xlrec->firstright; off++)
    {
      ItemId itemid;
      Size itemsz;
      IndexTuple item;

                                                            
      if (onleft && off == xlrec->newitemoff)
      {
        if (PageAddItem(newlpage, (Item)newitem, newitemsz, leftoff, false, false) == InvalidOffsetNumber)
        {
          elog(ERROR, "failed to add new item to left page after split");
        }
        leftoff = OffsetNumberNext(leftoff);
      }

      itemid = PageGetItemId(lpage, off);
      itemsz = ItemIdGetLength(itemid);
      item = (IndexTuple)PageGetItem(lpage, itemid);
      if (PageAddItem(newlpage, (Item)item, itemsz, leftoff, false, false) == InvalidOffsetNumber)
      {
        elog(ERROR, "failed to add old item to left page after split");
      }
      leftoff = OffsetNumberNext(leftoff);
    }

                                                            
    if (onleft && off == xlrec->newitemoff)
    {
      if (PageAddItem(newlpage, (Item)newitem, newitemsz, leftoff, false, false) == InvalidOffsetNumber)
      {
        elog(ERROR, "failed to add new item to left page after split");
      }
      leftoff = OffsetNumberNext(leftoff);
    }

    PageRestoreTempPage(newlpage, lpage);

                           
    lopaque->btpo_flags = BTP_INCOMPLETE_SPLIT;
    if (isleaf)
    {
      lopaque->btpo_flags |= BTP_LEAF;
    }
    lopaque->btpo_next = rightsib;
    lopaque->btpo_cycleid = 0;

    PageSetLSN(lpage, lsn);
    MarkBufferDirty(lbuf);
  }

     
                                                                             
                                                     
     
  if (BufferIsValid(lbuf))
  {
    UnlockReleaseBuffer(lbuf);
  }
  UnlockReleaseBuffer(rbuf);

     
                                                                      
     
                                                                           
                                                                            
                                                                           
                                                              
     
  if (rnext != P_NONE)
  {
    Buffer buffer;

    if (XLogReadBufferForRedo(record, 2, &buffer) == BLK_NEEDS_REDO)
    {
      Page page = (Page)BufferGetPage(buffer);
      BTPageOpaque pageop = (BTPageOpaque)PageGetSpecialPointer(page);

      pageop->btpo_prev = rightsib;

      PageSetLSN(page, lsn);
      MarkBufferDirty(buffer);
    }
    if (BufferIsValid(buffer))
    {
      UnlockReleaseBuffer(buffer);
    }
  }
}

static void
btree_xlog_vacuum(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  Buffer buffer;
  Page page;
  BTPageOpaque opaque;
#ifdef UNUSED
  xl_btree_vacuum *xlrec = (xl_btree_vacuum *)XLogRecGetData(record);

     
                                                                            
                                                                             
                                             
     
                                                                          
                                                                           
                                                                          
                                                                         
                                                                     
                                                
     
                                                                         
                                                                       
                                                                     
                           
     
                                                                          
                                                                            
                                                                             
                                                                            
                                                                           
                                                                      
     
                                                                            
                                      
     
                                                                            
                                                                         
                        
     
                                                                        
                                                                             
                                                                          
                                                                          
                                                                            
                                 
     
  if (HotStandbyActiveInReplay() && BlockNumberIsValid(xlrec->lastBlockVacuumed))
  {
    RelFileNode thisrnode;
    BlockNumber thisblkno;
    BlockNumber blkno;

    XLogRecGetBlockTag(record, 0, &thisrnode, NULL, &thisblkno);

    for (blkno = xlrec->lastBlockVacuumed + 1; blkno < thisblkno; blkno++)
    {
         
                                                                 
                                                                     
                                                                         
                                                                      
                                                                         
                                                                        
         
                                                                       
                                                                   
                                                                       
                                                                     
                                                                  
         
      buffer = XLogReadBufferExtended(thisrnode, MAIN_FORKNUM, blkno, RBM_NORMAL_NO_LOG);
      if (BufferIsValid(buffer))
      {
        LockBufferForCleanup(buffer);
        UnlockReleaseBuffer(buffer);
      }
    }
  }
#endif

     
                                                                          
                                          
     
  if (XLogReadBufferForRedoExtended(record, 0, RBM_NORMAL, true, &buffer) == BLK_NEEDS_REDO)
  {
    char *ptr;
    Size len;

    ptr = XLogRecGetBlockData(record, 0, &len);

    page = (Page)BufferGetPage(buffer);

    if (len > 0)
    {
      OffsetNumber *unused;
      OffsetNumber *unend;

      unused = (OffsetNumber *)ptr;
      unend = (OffsetNumber *)((char *)ptr + len);

      if ((unend - unused) > 0)
      {
        PageIndexMultiDelete(page, unused, unend - unused);
      }
    }

       
                                                                          
                                 
       
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    opaque->btpo_flags &= ~BTP_HAS_GARBAGE;

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

static void
btree_xlog_delete(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_btree_delete *xlrec = (xl_btree_delete *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  BTPageOpaque opaque;

     
                                                                        
                      
     
                                                                        
                                                                         
                                                                            
                                                                           
                                                                        
                                                               
     
  if (InHotStandby)
  {
    RelFileNode rnode;

    XLogRecGetBlockTag(record, 0, &rnode, NULL, NULL);

    ResolveRecoveryConflictWithSnapshot(xlrec->latestRemovedXid, rnode);
  }

     
                                                                      
                                
     
  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    page = (Page)BufferGetPage(buffer);

    if (XLogRecGetDataLen(record) > SizeOfBtreeDelete)
    {
      OffsetNumber *unused;

      unused = (OffsetNumber *)((char *)xlrec + SizeOfBtreeDelete);

      PageIndexMultiDelete(page, unused, xlrec->nitems);
    }

       
                                                                          
                                 
       
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    opaque->btpo_flags &= ~BTP_HAS_GARBAGE;

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

static void
btree_xlog_mark_page_halfdead(uint8 info, XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_btree_mark_page_halfdead *xlrec = (xl_btree_mark_page_halfdead *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  BTPageOpaque pageop;
  IndexTupleData trunctuple;

     
                                                                      
                                                                            
                                                                            
                                                                          
                                                   
     

                   
  if (XLogReadBufferForRedo(record, 1, &buffer) == BLK_NEEDS_REDO)
  {
    OffsetNumber poffset;
    ItemId itemid;
    IndexTuple itup;
    OffsetNumber nextoffset;
    BlockNumber rightsib;

    page = (Page)BufferGetPage(buffer);
    pageop = (BTPageOpaque)PageGetSpecialPointer(page);

    poffset = xlrec->poffset;

    nextoffset = OffsetNumberNext(poffset);
    itemid = PageGetItemId(page, nextoffset);
    itup = (IndexTuple)PageGetItem(page, itemid);
    rightsib = BTreeInnerTupleGetDownLink(itup);

    itemid = PageGetItemId(page, poffset);
    itup = (IndexTuple)PageGetItem(page, itemid);
    BTreeInnerTupleSetDownLink(itup, rightsib);
    nextoffset = OffsetNumberNext(poffset);
    PageIndexTupleDelete(page, nextoffset);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

                                                
  buffer = XLogInitBufferForRedo(record, 0);
  page = (Page)BufferGetPage(buffer);

  _bt_pageinit(page, BufferGetPageSize(buffer));
  pageop = (BTPageOpaque)PageGetSpecialPointer(page);

  pageop->btpo_prev = xlrec->leftblk;
  pageop->btpo_next = xlrec->rightblk;
  pageop->btpo.level = 0;
  pageop->btpo_flags = BTP_HALF_DEAD | BTP_LEAF;
  pageop->btpo_cycleid = 0;

     
                                                                       
                       
     
  MemSet(&trunctuple, 0, sizeof(IndexTupleData));
  trunctuple.t_info = sizeof(IndexTupleData);
  BTreeTupleSetTopParent(&trunctuple, xlrec->topparent);

  if (PageAddItem(page, (Item)&trunctuple, sizeof(IndexTupleData), P_HIKEY, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR, "could not add dummy high key to half-dead page");
  }

  PageSetLSN(page, lsn);
  MarkBufferDirty(buffer);
  UnlockReleaseBuffer(buffer);
}

static void
btree_xlog_unlink_page(uint8 info, XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_btree_unlink_page *xlrec = (xl_btree_unlink_page *)XLogRecGetData(record);
  BlockNumber leftsib;
  BlockNumber rightsib;
  Buffer buffer;
  Page page;
  BTPageOpaque pageop;

  leftsib = xlrec->leftsib;
  rightsib = xlrec->rightsib;

     
                                                                      
                                                                            
                                                                            
                                                                          
                                                   
     

                                      
  if (XLogReadBufferForRedo(record, 2, &buffer) == BLK_NEEDS_REDO)
  {
    page = (Page)BufferGetPage(buffer);
    pageop = (BTPageOpaque)PageGetSpecialPointer(page);
    pageop->btpo_prev = leftsib;

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

                                              
  if (leftsib != P_NONE)
  {
    if (XLogReadBufferForRedo(record, 1, &buffer) == BLK_NEEDS_REDO)
    {
      page = (Page)BufferGetPage(buffer);
      pageop = (BTPageOpaque)PageGetSpecialPointer(page);
      pageop->btpo_next = rightsib;

      PageSetLSN(page, lsn);
      MarkBufferDirty(buffer);
    }
    if (BufferIsValid(buffer))
    {
      UnlockReleaseBuffer(buffer);
    }
  }

                                                 
  buffer = XLogInitBufferForRedo(record, 0);
  page = (Page)BufferGetPage(buffer);

  _bt_pageinit(page, BufferGetPageSize(buffer));
  pageop = (BTPageOpaque)PageGetSpecialPointer(page);

  pageop->btpo_prev = leftsib;
  pageop->btpo_next = rightsib;
  pageop->btpo.xact = xlrec->btpo_xact;
  pageop->btpo_flags = BTP_DELETED;
  pageop->btpo_cycleid = 0;

  PageSetLSN(page, lsn);
  MarkBufferDirty(buffer);
  UnlockReleaseBuffer(buffer);

     
                                                                           
                                                                         
             
     
  if (XLogRecHasBlockRef(record, 3))
  {
       
                                                                       
                                                          
       
    IndexTupleData trunctuple;

    buffer = XLogInitBufferForRedo(record, 3);
    page = (Page)BufferGetPage(buffer);

    _bt_pageinit(page, BufferGetPageSize(buffer));
    pageop = (BTPageOpaque)PageGetSpecialPointer(page);

    pageop->btpo_flags = BTP_HALF_DEAD | BTP_LEAF;
    pageop->btpo_prev = xlrec->leafleftsib;
    pageop->btpo_next = xlrec->leafrightsib;
    pageop->btpo.level = 0;
    pageop->btpo_cycleid = 0;

                                
    MemSet(&trunctuple, 0, sizeof(IndexTupleData));
    trunctuple.t_info = sizeof(IndexTupleData);
    BTreeTupleSetTopParent(&trunctuple, xlrec->topparent);

    if (PageAddItem(page, (Item)&trunctuple, sizeof(IndexTupleData), P_HIKEY, false, false) == InvalidOffsetNumber)
    {
      elog(ERROR, "could not add dummy high key to half-dead page");
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
    UnlockReleaseBuffer(buffer);
  }

                                 
  if (info == XLOG_BTREE_UNLINK_PAGE_META)
  {
    _bt_restore_meta(record, 4);
  }
}

static void
btree_xlog_newroot(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_btree_newroot *xlrec = (xl_btree_newroot *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  BTPageOpaque pageop;
  char *ptr;
  Size len;

  buffer = XLogInitBufferForRedo(record, 0);
  page = (Page)BufferGetPage(buffer);

  _bt_pageinit(page, BufferGetPageSize(buffer));
  pageop = (BTPageOpaque)PageGetSpecialPointer(page);

  pageop->btpo_flags = BTP_ROOT;
  pageop->btpo_prev = pageop->btpo_next = P_NONE;
  pageop->btpo.level = xlrec->level;
  if (xlrec->level == 0)
  {
    pageop->btpo_flags |= BTP_LEAF;
  }
  pageop->btpo_cycleid = 0;

  if (xlrec->level > 0)
  {
    ptr = XLogRecGetBlockData(record, 0, &len);
    _bt_restore_page(page, ptr, len);

                                                       
    _bt_clear_incomplete_split(record, 1);
  }

  PageSetLSN(page, lsn);
  MarkBufferDirty(buffer);
  UnlockReleaseBuffer(buffer);

  _bt_restore_meta(record, 2);
}

static void
btree_xlog_reuse_page(XLogReaderState *record)
{
  xl_btree_reuse_page *xlrec = (xl_btree_reuse_page *)XLogRecGetData(record);

     
                                                                        
                                                                       
     
                                                                 
                                                                             
                                                                   
                                                                       
                         
     
  if (InHotStandby)
  {
    ResolveRecoveryConflictWithSnapshot(xlrec->latestRemovedXid, xlrec->node);
  }
}

void
btree_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  switch (info)
  {
  case XLOG_BTREE_INSERT_LEAF:
    btree_xlog_insert(true, false, record);
    break;
  case XLOG_BTREE_INSERT_UPPER:
    btree_xlog_insert(false, false, record);
    break;
  case XLOG_BTREE_INSERT_META:
    btree_xlog_insert(false, true, record);
    break;
  case XLOG_BTREE_SPLIT_L:
    btree_xlog_split(true, record);
    break;
  case XLOG_BTREE_SPLIT_R:
    btree_xlog_split(false, record);
    break;
  case XLOG_BTREE_VACUUM:
    btree_xlog_vacuum(record);
    break;
  case XLOG_BTREE_DELETE:
    btree_xlog_delete(record);
    break;
  case XLOG_BTREE_MARK_PAGE_HALFDEAD:
    btree_xlog_mark_page_halfdead(info, record);
    break;
  case XLOG_BTREE_UNLINK_PAGE:
  case XLOG_BTREE_UNLINK_PAGE_META:
    btree_xlog_unlink_page(info, record);
    break;
  case XLOG_BTREE_NEWROOT:
    btree_xlog_newroot(record);
    break;
  case XLOG_BTREE_REUSE_PAGE:
    btree_xlog_reuse_page(record);
    break;
  case XLOG_BTREE_META_CLEANUP:
    _bt_restore_meta(record, 0);
    break;
  default:
    elog(PANIC, "btree_redo: unknown op code %u", info);
  }
}

   
                                                                 
   
void
btree_mask(char *pagedata, BlockNumber blkno)
{
  Page page = (Page)pagedata;
  BTPageOpaque maskopaq;

  mask_page_lsn_and_checksum(page);

  mask_page_hint_bits(page);
  mask_unused_space(page);

  maskopaq = (BTPageOpaque)PageGetSpecialPointer(page);

  if (P_ISDELETED(maskopaq))
  {
       
                                                                           
                                                                
       
    mask_page_content(page);
  }
  else if (P_ISLEAF(maskopaq))
  {
       
                                                                          
                                                                        
                                                        
       
    mask_lp_flags(page);
  }

     
                                                                     
                                                      
     
  maskopaq->btpo_flags &= ~BTP_HAS_GARBAGE;

     
                                                                         
                                                                             
                                               
     
  maskopaq->btpo_flags &= ~BTP_SPLIT_END;
  maskopaq->btpo_cycleid = 0;
}
