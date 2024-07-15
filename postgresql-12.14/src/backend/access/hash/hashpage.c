                                                                            
   
              
                                                                         
   
                                                                         
                                                                        
   
   
                  
                                        
   
         
                                                                        
                                                                          
                                                                     
                                                                        
                         
   
                                                                           
                                                                     
                                                              
   
                                                                  
                     
   
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "storage/predicate.h"

static bool
_hash_alloc_buckets(Relation rel, BlockNumber firstblock, uint32 nblocks);
static void
_hash_splitbucket(Relation rel, Buffer metabuf, Bucket obucket, Bucket nbucket, Buffer obuf, Buffer nbuf, HTAB *htab, uint32 maxbucket, uint32 highmask, uint32 lowmask);
static void
log_split_page(Relation rel, Buffer buf);

   
                                                                     
   
                                                            
                                                       
   
                                                                        
                                                                  
   
                                                                  
                                                                  
                                             
   
                                                              
                                                                    
                                                             
   
Buffer
_hash_getbuf(Relation rel, BlockNumber blkno, int access, int flags)
{
  Buffer buf;

  if (blkno == P_NEW)
  {
    elog(ERROR, "hash AM does not use P_NEW");
  }

  buf = ReadBuffer(rel, blkno);

  if (access != HASH_NOLOCK)
  {
    LockBuffer(buf, access);
  }

                                           

  _hash_checkpage(rel, buf, flags);

  return buf;
}

   
                                                                            
   
                                                                       
                                                              
   
Buffer
_hash_getbuf_with_condlock_cleanup(Relation rel, BlockNumber blkno, int flags)
{
  Buffer buf;

  if (blkno == P_NEW)
  {
    elog(ERROR, "hash AM does not use P_NEW");
  }

  buf = ReadBuffer(rel, blkno);

  if (!ConditionalLockBufferForCleanup(buf))
  {
    ReleaseBuffer(buf);
    return InvalidBuffer;
  }

                                           

  _hash_checkpage(rel, buf, flags);

  return buf;
}

   
                                                                      
   
                                                                      
                                                                   
                                                                 
                                                                
   
                                                          
                                                                  
                                             
   
                                                              
                                                                    
                                                             
   
Buffer
_hash_getinitbuf(Relation rel, BlockNumber blkno)
{
  Buffer buf;

  if (blkno == P_NEW)
  {
    elog(ERROR, "hash AM does not use P_NEW");
  }

  buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_ZERO_AND_LOCK, NULL);

                                           

                           
  _hash_pageinit(BufferGetPage(buf), BufferGetPageSize(buf));

  return buf;
}

   
                                                                    
   
void
_hash_initbuf(Buffer buf, uint32 max_bucket, uint32 num_bucket, uint32 flag, bool initpage)
{
  HashPageOpaque pageopaque;
  Page page;

  page = BufferGetPage(buf);

                           
  if (initpage)
  {
    _hash_pageinit(page, BufferGetPageSize(buf));
  }

  pageopaque = (HashPageOpaque)PageGetSpecialPointer(page);

     
                                                                          
                                                   
                                        
     
  pageopaque->hasho_prevblkno = max_bucket;
  pageopaque->hasho_nextblkno = InvalidBlockNumber;
  pageopaque->hasho_bucket = num_bucket;
  pageopaque->hasho_flag = flag;
  pageopaque->hasho_page_id = HASHO_PAGE_ID;
}

   
                                                                
   
                                                                         
                                                                  
                                                                        
                                                                        
                                                                     
   
                                                                      
                                                                      
                                                                         
                                                          
   
Buffer
_hash_getnewbuf(Relation rel, BlockNumber blkno, ForkNumber forkNum)
{
  BlockNumber nblocks = RelationGetNumberOfBlocksInFork(rel, forkNum);
  Buffer buf;

  if (blkno == P_NEW)
  {
    elog(ERROR, "hash AM does not use P_NEW");
  }
  if (blkno > nblocks)
  {
    elog(ERROR, "access to noncontiguous page in hash index \"%s\"", RelationGetRelationName(rel));
  }

                                                        
  if (blkno == nblocks)
  {
    buf = ReadBufferExtended(rel, forkNum, P_NEW, RBM_NORMAL, NULL);
    if (BufferGetBlockNumber(buf) != blkno)
    {
      elog(ERROR, "unexpected hash relation size: %u, should be %u", BufferGetBlockNumber(buf), blkno);
    }
    LockBuffer(buf, HASH_WRITE);
  }
  else
  {
    buf = ReadBufferExtended(rel, forkNum, blkno, RBM_ZERO_AND_LOCK, NULL);
  }

                                           

                           
  _hash_pageinit(BufferGetPage(buf), BufferGetPageSize(buf));

  return buf;
}

   
                                                                          
   
                                                                        
                                                                  
   
Buffer
_hash_getbuf_with_strategy(Relation rel, BlockNumber blkno, int access, int flags, BufferAccessStrategy bstrategy)
{
  Buffer buf;

  if (blkno == P_NEW)
  {
    elog(ERROR, "hash AM does not use P_NEW");
  }

  buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_NORMAL, bstrategy);

  if (access != HASH_NOLOCK)
  {
    LockBuffer(buf, access);
  }

                                           

  _hash_checkpage(rel, buf, flags);

  return buf;
}

   
                                              
   
                                             
   
void
_hash_relbuf(Relation rel, Buffer buf)
{
  UnlockReleaseBuffer(buf);
}

   
                                                  
   
                                                            
   
void
_hash_dropbuf(Relation rel, Buffer buf)
{
  ReleaseBuffer(buf);
}

   
                                                        
   
                                                                
                 
   
void
_hash_dropscanbuf(Relation rel, HashScanOpaque so)
{
                                                  
  if (BufferIsValid(so->hashso_bucket_buf) && so->hashso_bucket_buf != so->currPos.buf)
  {
    _hash_dropbuf(rel, so->hashso_bucket_buf);
  }
  so->hashso_bucket_buf = InvalidBuffer;

                                                                         
  if (BufferIsValid(so->hashso_split_bucket_buf) && so->hashso_split_bucket_buf != so->currPos.buf)
  {
    _hash_dropbuf(rel, so->hashso_split_bucket_buf);
  }
  so->hashso_split_bucket_buf = InvalidBuffer;

                                     
  if (BufferIsValid(so->currPos.buf))
  {
    _hash_dropbuf(rel, so->currPos.buf);
  }
  so->currPos.buf = InvalidBuffer;

                        
  so->hashso_buc_populated = false;
  so->hashso_buc_split = false;
}

   
                                                                 
                                                        
   
                                                                         
                                                                       
                                         
   
                                                                             
                                                                            
                                     
   
uint32
_hash_init(Relation rel, double num_tuples, ForkNumber forkNum)
{
  Buffer metabuf;
  Buffer buf;
  Buffer bitmapbuf;
  Page pg;
  HashMetaPage metap;
  RegProcedure procid;
  int32 data_width;
  int32 item_width;
  int32 ffactor;
  uint32 num_buckets;
  uint32 i;
  bool use_wal;

                    
  if (RelationGetNumberOfBlocksInFork(rel, forkNum) != 0)
  {
    elog(ERROR, "cannot initialize non-empty hash index \"%s\"", RelationGetRelationName(rel));
  }

     
                                                                             
                                                                         
             
     
  use_wal = RelationNeedsWAL(rel) || forkNum == INIT_FORKNUM;

     
                                                                             
                                                                           
                                                                        
                                                                             
     
  data_width = sizeof(uint32);
  item_width = MAXALIGN(sizeof(IndexTupleData)) + MAXALIGN(data_width) + sizeof(ItemIdData);                               
  ffactor = RelationGetTargetPageUsage(rel, HASH_DEFAULT_FILLFACTOR) / item_width;
                            
  if (ffactor < 10)
  {
    ffactor = 10;
  }

  procid = index_getprocid(rel, 1, HASHSTANDARD_PROC);

     
                                                                         
                                                                          
                                                                             
                                
     
                                                                         
                                         
     
  metabuf = _hash_getnewbuf(rel, HASH_METAPAGE, forkNum);
  _hash_init_metabuffer(metabuf, num_tuples, procid, ffactor, false);
  MarkBufferDirty(metabuf);

  pg = BufferGetPage(metabuf);
  metap = HashPageGetMeta(pg);

                  
  if (use_wal)
  {
    xl_hash_init_meta_page xlrec;
    XLogRecPtr recptr;

    xlrec.num_tuples = num_tuples;
    xlrec.procid = metap->hashm_procid;
    xlrec.ffactor = metap->hashm_ffactor;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHashInitMetaPage);
    XLogRegisterBuffer(0, metabuf, REGBUF_WILL_INIT | REGBUF_STANDARD);

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_INIT_META_PAGE);

    PageSetLSN(BufferGetPage(metabuf), recptr);
  }

  num_buckets = metap->hashm_maxbucket + 1;

     
                                                                      
                                                                           
                                                                          
                                                                    
     
  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

     
                                                
     
  for (i = 0; i < num_buckets; i++)
  {
    BlockNumber blkno;

                                             
    CHECK_FOR_INTERRUPTS();

    blkno = BUCKET_TO_BLKNO(metap, i);
    buf = _hash_getnewbuf(rel, blkno, forkNum);
    _hash_initbuf(buf, metap->hashm_maxbucket, i, LH_BUCKET_PAGE, false);
    MarkBufferDirty(buf);

    if (use_wal)
    {
      log_newpage(&rel->rd_node, forkNum, blkno, BufferGetPage(buf), true);
    }
    _hash_relbuf(rel, buf);
  }

                                             
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

     
                            
     
  bitmapbuf = _hash_getnewbuf(rel, num_buckets + 1, forkNum);
  _hash_initbitmapbuffer(bitmapbuf, metap->hashm_bmsize, false);
  MarkBufferDirty(bitmapbuf);

                                                                 
                                         
  if (metap->hashm_nmaps >= HASH_MAX_BITMAPS)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of overflow pages in hash index \"%s\"", RelationGetRelationName(rel))));
  }

  metap->hashm_mapp[metap->hashm_nmaps] = num_buckets + 1;

  metap->hashm_nmaps++;
  MarkBufferDirty(metabuf);

                  
  if (use_wal)
  {
    xl_hash_init_bitmap_page xlrec;
    XLogRecPtr recptr;

    xlrec.bmsize = metap->hashm_bmsize;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHashInitBitmapPage);
    XLogRegisterBuffer(0, bitmapbuf, REGBUF_WILL_INIT);

       
                                                                           
                                                                         
           
       
    XLogRegisterBuffer(1, metabuf, REGBUF_STANDARD);

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_INIT_BITMAP_PAGE);

    PageSetLSN(BufferGetPage(bitmapbuf), recptr);
    PageSetLSN(BufferGetPage(metabuf), recptr);
  }

                
  _hash_relbuf(rel, bitmapbuf);
  _hash_relbuf(rel, metabuf);

  return num_buckets;
}

   
                                                                            
   
void
_hash_init_metabuffer(Buffer buf, double num_tuples, RegProcedure procid, uint16 ffactor, bool initpage)
{
  HashMetaPage metap;
  HashPageOpaque pageopaque;
  Page page;
  double dnumbuckets;
  uint32 num_buckets;
  uint32 spare_index;
  uint32 i;

     
                                                                        
                                                                          
                                                                        
                                                                             
                                                              
                          
     
  dnumbuckets = num_tuples / ffactor;
  if (dnumbuckets <= 2.0)
  {
    num_buckets = 2;
  }
  else if (dnumbuckets >= (double)0x40000000)
  {
    num_buckets = 0x40000000;
  }
  else
  {
    num_buckets = _hash_get_totalbuckets(_hash_spareindex(dnumbuckets));
  }

  spare_index = _hash_spareindex(num_buckets);
  Assert(spare_index < HASH_MAX_SPLITPOINTS);

  page = BufferGetPage(buf);
  if (initpage)
  {
    _hash_pageinit(page, BufferGetPageSize(buf));
  }

  pageopaque = (HashPageOpaque)PageGetSpecialPointer(page);
  pageopaque->hasho_prevblkno = InvalidBlockNumber;
  pageopaque->hasho_nextblkno = InvalidBlockNumber;
  pageopaque->hasho_bucket = -1;
  pageopaque->hasho_flag = LH_META_PAGE;
  pageopaque->hasho_page_id = HASHO_PAGE_ID;

  metap = HashPageGetMeta(page);

  metap->hashm_magic = HASH_MAGIC;
  metap->hashm_version = HASH_VERSION;
  metap->hashm_ntuples = 0;
  metap->hashm_nmaps = 0;
  metap->hashm_ffactor = ffactor;
  metap->hashm_bsize = HashGetMaxBitmapSize(page);
                                                                 
  for (i = _hash_log2(metap->hashm_bsize); i > 0; --i)
  {
    if ((1 << i) <= metap->hashm_bsize)
    {
      break;
    }
  }
  Assert(i > 0);
  metap->hashm_bmsize = 1 << i;
  metap->hashm_bmshift = i + BYTE_TO_BIT;
  Assert((1 << BMPG_SHIFT(metap)) == (BMPG_MASK(metap) + 1));

     
                                                                            
                                                                            
                                                                           
     
  metap->hashm_procid = procid;

     
                                                                          
                                                                      
     
  metap->hashm_maxbucket = num_buckets - 1;

     
                                                                   
                                      
     
  metap->hashm_highmask = (1 << (_hash_log2(num_buckets + 1))) - 1;
  metap->hashm_lowmask = (metap->hashm_highmask >> 1);

  MemSet(metap->hashm_spares, 0, sizeof(metap->hashm_spares));
  MemSet(metap->hashm_mapp, 0, sizeof(metap->hashm_mapp));

                                                                       
  metap->hashm_spares[spare_index] = 1;
  metap->hashm_ovflpoint = spare_index;
  metap->hashm_firstfree = 0;

     
                                                                         
                                                                          
               
     
  ((PageHeader)page)->pd_lower = ((char *)metap + sizeof(HashMetaPageData)) - (char *)page;
}

   
                                                         
   
void
_hash_pageinit(Page page, Size size)
{
  PageInit(page, size, sizeof(HashPageOpaqueData));
}

   
                                                                
   
                                                                        
               
   
                                                                      
                                                       
   
                                                                    
                                             
   
void
_hash_expandtable(Relation rel, Buffer metabuf)
{
  HashMetaPage metap;
  Bucket old_bucket;
  Bucket new_bucket;
  uint32 spare_ndx;
  BlockNumber start_oblkno;
  BlockNumber start_nblkno;
  Buffer buf_nblkno;
  Buffer buf_oblkno;
  Page opage;
  Page npage;
  HashPageOpaque oopaque;
  HashPageOpaque nopaque;
  uint32 maxbucket;
  uint32 highmask;
  uint32 lowmask;
  bool metap_update_masks = false;
  bool metap_update_splitpoint = false;

restart_expand:

     
                                                                     
                                                                        
     
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

  _hash_checkpage(rel, metabuf, LH_META_PAGE);
  metap = HashPageGetMeta(BufferGetPage(metabuf));

     
                                                                            
                                            
     
                                                        
     
  if (metap->hashm_ntuples <= (double)metap->hashm_ffactor * (metap->hashm_maxbucket + 1))
  {
    goto fail;
  }

     
                                                                       
            
     
                                                                           
                                                                           
                                                                  
                                                                        
                                                                            
                                                                           
                                                                     
     
                                                                           
                   
     
  if (metap->hashm_maxbucket >= (uint32)0x7FFFFFFE)
  {
    goto fail;
  }

     
                                                                             
                                                            
     
                                                                       
                                      
     
                                                                     
                                                                       
                                                                        
                                                                           
     
  new_bucket = metap->hashm_maxbucket + 1;

  old_bucket = (new_bucket & metap->hashm_lowmask);

  start_oblkno = BUCKET_TO_BLKNO(metap, old_bucket);

  buf_oblkno = _hash_getbuf_with_condlock_cleanup(rel, start_oblkno, LH_BUCKET_PAGE);
  if (!buf_oblkno)
  {
    goto fail;
  }

  opage = BufferGetPage(buf_oblkno);
  oopaque = (HashPageOpaque)PageGetSpecialPointer(opage);

     
                                                                       
                                                                             
                                                                             
                                                                     
                                                                            
                                                                        
     
  if (H_BUCKET_BEING_SPLIT(oopaque))
  {
       
                                                                           
                                                                        
                         
       
    maxbucket = metap->hashm_maxbucket;
    highmask = metap->hashm_highmask;
    lowmask = metap->hashm_lowmask;

       
                                                                          
              
       
    LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
    LockBuffer(buf_oblkno, BUFFER_LOCK_UNLOCK);

    _hash_finish_split(rel, metabuf, buf_oblkno, old_bucket, maxbucket, highmask, lowmask);

                                                             
    _hash_dropbuf(rel, buf_oblkno);

    goto restart_expand;
  }

     
                                                                        
                                                                         
                                                                             
                                                                  
                                                                          
                                                                      
                                                                            
                                                                          
                                                                            
                                                                             
                                               
     
  if (H_NEEDS_SPLIT_CLEANUP(oopaque))
  {
       
                                                                        
                                                                          
                             
       
    maxbucket = metap->hashm_maxbucket;
    highmask = metap->hashm_highmask;
    lowmask = metap->hashm_lowmask;

                                    
    LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

    hashbucketcleanup(rel, old_bucket, buf_oblkno, start_oblkno, NULL, maxbucket, highmask, lowmask, NULL, NULL, true, NULL, NULL);

    _hash_dropbuf(rel, buf_oblkno);

    goto restart_expand;
  }

     
                                                       
     
                                                                             
                                                                            
                                                                        
                                                                    
     
  start_nblkno = BUCKET_TO_BLKNO(metap, new_bucket);

     
                                                                         
                   
     
  spare_ndx = _hash_spareindex(new_bucket + 1);
  if (spare_ndx > metap->hashm_ovflpoint)
  {
    uint32 buckets_to_add;

    Assert(spare_ndx == metap->hashm_ovflpoint + 1);

       
                                                                       
                                                                      
                                                                         
                                                                          
       
    buckets_to_add = _hash_get_totalbuckets(spare_ndx) - new_bucket;
    if (!_hash_alloc_buckets(rel, start_nblkno, buckets_to_add))
    {
                                                   
      _hash_relbuf(rel, buf_oblkno);
      goto fail;
    }
  }

     
                                                                            
                                                                           
                 
     
                                                                          
                                                                            
                                                                            
                                                                            
                                                                       
     
  buf_nblkno = _hash_getnewbuf(rel, start_nblkno, MAIN_FORKNUM);
  if (!IsBufferCleanupOK(buf_nblkno))
  {
    _hash_relbuf(rel, buf_oblkno);
    _hash_relbuf(rel, buf_nblkno);
    goto fail;
  }

     
                                                                             
                                                                           
                                                                             
              
     
  START_CRIT_SECTION();

     
                                                                           
     
  metap->hashm_maxbucket = new_bucket;

  if (new_bucket > metap->hashm_highmask)
  {
                                 
    metap->hashm_lowmask = metap->hashm_highmask;
    metap->hashm_highmask = new_bucket | metap->hashm_lowmask;
    metap_update_masks = true;
  }

     
                                                                           
                                                                             
                                            
     
  if (spare_ndx > metap->hashm_ovflpoint)
  {
    metap->hashm_spares[spare_ndx] = metap->hashm_spares[metap->hashm_ovflpoint];
    metap->hashm_ovflpoint = spare_ndx;
    metap_update_splitpoint = true;
  }

  MarkBufferDirty(metabuf);

     
                                                                         
                                                                        
                                                                           
                                                                        
                                                                       
     
  maxbucket = metap->hashm_maxbucket;
  highmask = metap->hashm_highmask;
  lowmask = metap->hashm_lowmask;

  opage = BufferGetPage(buf_oblkno);
  oopaque = (HashPageOpaque)PageGetSpecialPointer(opage);

     
                                                                     
                                                                            
                                                                            
                                                                      
     
  oopaque->hasho_flag |= LH_BUCKET_BEING_SPLIT;
  oopaque->hasho_prevblkno = maxbucket;

  MarkBufferDirty(buf_oblkno);

  npage = BufferGetPage(buf_nblkno);

     
                                                                           
                           
     
  nopaque = (HashPageOpaque)PageGetSpecialPointer(npage);
  nopaque->hasho_prevblkno = maxbucket;
  nopaque->hasho_nextblkno = InvalidBlockNumber;
  nopaque->hasho_bucket = new_bucket;
  nopaque->hasho_flag = LH_BUCKET_PAGE | LH_BUCKET_BEING_POPULATED;
  nopaque->hasho_page_id = HASHO_PAGE_ID;

  MarkBufferDirty(buf_nblkno);

                  
  if (RelationNeedsWAL(rel))
  {
    xl_hash_split_allocate_page xlrec;
    XLogRecPtr recptr;

    xlrec.new_bucket = maxbucket;
    xlrec.old_bucket_flag = oopaque->hasho_flag;
    xlrec.new_bucket_flag = nopaque->hasho_flag;
    xlrec.flags = 0;

    XLogBeginInsert();

    XLogRegisterBuffer(0, buf_oblkno, REGBUF_STANDARD);
    XLogRegisterBuffer(1, buf_nblkno, REGBUF_WILL_INIT);
    XLogRegisterBuffer(2, metabuf, REGBUF_STANDARD);

    if (metap_update_masks)
    {
      xlrec.flags |= XLH_SPLIT_META_UPDATE_MASKS;
      XLogRegisterBufData(2, (char *)&metap->hashm_lowmask, sizeof(uint32));
      XLogRegisterBufData(2, (char *)&metap->hashm_highmask, sizeof(uint32));
    }

    if (metap_update_splitpoint)
    {
      xlrec.flags |= XLH_SPLIT_META_UPDATE_SPLITPOINT;
      XLogRegisterBufData(2, (char *)&metap->hashm_ovflpoint, sizeof(uint32));
      XLogRegisterBufData(2, (char *)&metap->hashm_spares[metap->hashm_ovflpoint], sizeof(uint32));
    }

    XLogRegisterData((char *)&xlrec, SizeOfHashSplitAllocPage);

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_SPLIT_ALLOCATE_PAGE);

    PageSetLSN(BufferGetPage(buf_oblkno), recptr);
    PageSetLSN(BufferGetPage(buf_nblkno), recptr);
    PageSetLSN(BufferGetPage(metabuf), recptr);
  }

  END_CRIT_SECTION();

                               
  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

                                          
  _hash_splitbucket(rel, metabuf, old_bucket, new_bucket, buf_oblkno, buf_nblkno, NULL, maxbucket, highmask, lowmask);

                                                          
  _hash_dropbuf(rel, buf_oblkno);
  _hash_dropbuf(rel, buf_nblkno);

  return;

                                                                      
fail:

                                                       
  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
}

   
                                                                            
   
                                                                           
                                                                               
                                                                          
                                                              
   
                                                                              
                                                                             
                                                                         
                                                                           
                                                                         
                                                                   
   
                                                                             
                                                                               
                                                                           
                                                                               
                                          
   
                                                                    
                         
   
static bool
_hash_alloc_buckets(Relation rel, BlockNumber firstblock, uint32 nblocks)
{
  BlockNumber lastblock;
  PGAlignedBlock zerobuf;
  Page page;
  HashPageOpaque ovflopaque;

  lastblock = firstblock + nblocks - 1;

     
                                                                             
                        
     
  if (lastblock < firstblock || lastblock == InvalidBlockNumber)
  {
    return false;
  }

  page = (Page)zerobuf.data;

     
                                                                 
                                                                             
                                                               
     
  _hash_pageinit(page, BLCKSZ);

  ovflopaque = (HashPageOpaque)PageGetSpecialPointer(page);

  ovflopaque->hasho_prevblkno = InvalidBlockNumber;
  ovflopaque->hasho_nextblkno = InvalidBlockNumber;
  ovflopaque->hasho_bucket = -1;
  ovflopaque->hasho_flag = LH_UNUSED_PAGE;
  ovflopaque->hasho_page_id = HASHO_PAGE_ID;

  if (RelationNeedsWAL(rel))
  {
    log_newpage(&rel->rd_node, MAIN_FORKNUM, lastblock, zerobuf.data, true);
  }

  PageSetChecksumInplace(page, lastblock);
  smgrextend(RelationGetSmgr(rel), MAIN_FORKNUM, lastblock, zerobuf.data, false);

  return true;
}

   
                                                                     
   
                                                                               
                                                                                
                                                                                
                                                                           
                                                                             
           
   
                                                                          
                                                                        
                             
   
                                                                     
                                                      
   
                                                                    
                                                                    
                                                                     
   
                                                                         
                                                                           
                                 
   
                                                                         
                                                                              
                                                                             
                                                                               
                                                                              
                           
   
static void
_hash_splitbucket(Relation rel, Buffer metabuf, Bucket obucket, Bucket nbucket, Buffer obuf, Buffer nbuf, HTAB *htab, uint32 maxbucket, uint32 highmask, uint32 lowmask)
{
  Buffer bucket_obuf;
  Buffer bucket_nbuf;
  Page opage;
  Page npage;
  HashPageOpaque oopaque;
  HashPageOpaque nopaque;
  OffsetNumber itup_offsets[MaxIndexTuplesPerPage];
  IndexTuple itups[MaxIndexTuplesPerPage];
  Size all_tups_size = 0;
  int i;
  uint16 nitups = 0;

  bucket_obuf = obuf;
  opage = BufferGetPage(obuf);
  oopaque = (HashPageOpaque)PageGetSpecialPointer(opage);

  bucket_nbuf = nbuf;
  npage = BufferGetPage(nbuf);
  nopaque = (HashPageOpaque)PageGetSpecialPointer(npage);

                                                               
  PredicateLockPageSplit(rel, BufferGetBlockNumber(bucket_obuf), BufferGetBlockNumber(bucket_nbuf));

     
                                                                           
                                                                            
                                                                             
                                  
     
  for (;;)
  {
    BlockNumber oblkno;
    OffsetNumber ooffnum;
    OffsetNumber omaxoffnum;

                                     
    omaxoffnum = PageGetMaxOffsetNumber(opage);
    for (ooffnum = FirstOffsetNumber; ooffnum <= omaxoffnum; ooffnum = OffsetNumberNext(ooffnum))
    {
      IndexTuple itup;
      Size itemsz;
      Bucket bucket;
      bool found = false;

                            
      if (ItemIdIsDead(PageGetItemId(opage, ooffnum)))
      {
        continue;
      }

         
                                                                        
                                                                     
                                                                       
                                                                       
             
         
      itup = (IndexTuple)PageGetItem(opage, PageGetItemId(opage, ooffnum));

      if (htab)
      {
        (void)hash_search(htab, &itup->t_tid, HASH_FIND, &found);
      }

      if (found)
      {
        continue;
      }

      bucket = _hash_hashkey2bucket(_hash_get_indextuple_hashkey(itup), maxbucket, highmask, lowmask);

      if (bucket == nbucket)
      {
        IndexTuple new_itup;

           
                                                                    
           
        new_itup = CopyIndexTuple(itup);

           
                                                                   
                                                                       
           
        new_itup->t_info |= INDEX_MOVED_BY_SPLIT_MASK;

           
                                                                       
                                                                      
                                                                   
           
        itemsz = IndexTupleSize(new_itup);
        itemsz = MAXALIGN(itemsz);

        if (PageGetFreeSpaceForMultipleTuples(npage, nitups + 1) < (all_tups_size + itemsz))
        {
             
                                                                 
                                                              
             
          START_CRIT_SECTION();

          _hash_pgaddmultitup(rel, nbuf, itups, itup_offsets, nitups);
          MarkBufferDirty(nbuf);
                                                                 
          log_split_page(rel, nbuf);

          END_CRIT_SECTION();

                                       
          LockBuffer(nbuf, BUFFER_LOCK_UNLOCK);

                       
          for (i = 0; i < nitups; i++)
          {
            pfree(itups[i]);
          }
          nitups = 0;
          all_tups_size = 0;

                                            
          nbuf = _hash_addovflpage(rel, metabuf, nbuf, (nbuf == bucket_nbuf) ? true : false);
          npage = BufferGetPage(nbuf);
          nopaque = (HashPageOpaque)PageGetSpecialPointer(npage);
        }

        itups[nitups++] = new_itup;
        all_tups_size += itemsz;
      }
      else
      {
           
                                                           
           
        Assert(bucket == obucket);
      }
    }

    oblkno = oopaque->hasho_nextblkno;

                                                  
    if (obuf == bucket_obuf)
    {
      LockBuffer(obuf, BUFFER_LOCK_UNLOCK);
    }
    else
    {
      _hash_relbuf(rel, obuf);
    }

                                                           
    if (!BlockNumberIsValid(oblkno))
    {
         
                                                                       
                                                
         
      START_CRIT_SECTION();

      _hash_pgaddmultitup(rel, nbuf, itups, itup_offsets, nitups);
      MarkBufferDirty(nbuf);
                                                             
      log_split_page(rel, nbuf);

      END_CRIT_SECTION();

      if (nbuf == bucket_nbuf)
      {
        LockBuffer(nbuf, BUFFER_LOCK_UNLOCK);
      }
      else
      {
        _hash_relbuf(rel, nbuf);
      }

                   
      for (i = 0; i < nitups; i++)
      {
        pfree(itups[i]);
      }
      break;
    }

                                        
    obuf = _hash_getbuf(rel, oblkno, HASH_READ, LH_OVERFLOW_PAGE);
    opage = BufferGetPage(obuf);
    oopaque = (HashPageOpaque)PageGetSpecialPointer(opage);
  }

     
                                                                          
                                                                    
               
     
                                                                            
                                     
     
  LockBuffer(bucket_obuf, BUFFER_LOCK_EXCLUSIVE);
  opage = BufferGetPage(bucket_obuf);
  oopaque = (HashPageOpaque)PageGetSpecialPointer(opage);

  LockBuffer(bucket_nbuf, BUFFER_LOCK_EXCLUSIVE);
  npage = BufferGetPage(bucket_nbuf);
  nopaque = (HashPageOpaque)PageGetSpecialPointer(npage);

  START_CRIT_SECTION();

  oopaque->hasho_flag &= ~LH_BUCKET_BEING_SPLIT;
  nopaque->hasho_flag &= ~LH_BUCKET_BEING_POPULATED;

     
                                                                          
                                                                        
                                                                          
                                               
     
  oopaque->hasho_flag |= LH_BUCKET_NEEDS_SPLIT_CLEANUP;

     
                                                                         
                                   
     
  MarkBufferDirty(bucket_obuf);
  MarkBufferDirty(bucket_nbuf);

  if (RelationNeedsWAL(rel))
  {
    XLogRecPtr recptr;
    xl_hash_split_complete xlrec;

    xlrec.old_bucket_flag = oopaque->hasho_flag;
    xlrec.new_bucket_flag = nopaque->hasho_flag;

    XLogBeginInsert();

    XLogRegisterData((char *)&xlrec, SizeOfHashSplitComplete);

    XLogRegisterBuffer(0, bucket_obuf, REGBUF_STANDARD);
    XLogRegisterBuffer(1, bucket_nbuf, REGBUF_STANDARD);

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_SPLIT_COMPLETE);

    PageSetLSN(BufferGetPage(bucket_obuf), recptr);
    PageSetLSN(BufferGetPage(bucket_nbuf), recptr);
  }

  END_CRIT_SECTION();

     
                                                                            
                                                                             
                                                                         
                                                                         
                                                                           
                 
     
  if (IsBufferCleanupOK(bucket_obuf))
  {
    LockBuffer(bucket_nbuf, BUFFER_LOCK_UNLOCK);
    hashbucketcleanup(rel, obucket, bucket_obuf, BufferGetBlockNumber(bucket_obuf), NULL, maxbucket, highmask, lowmask, NULL, NULL, true, NULL, NULL);
  }
  else
  {
    LockBuffer(bucket_nbuf, BUFFER_LOCK_UNLOCK);
    LockBuffer(bucket_obuf, BUFFER_LOCK_UNLOCK);
  }
}

   
                                                                             
   
                                                                          
                                                                        
                                                                        
   
                                                                             
                                                                           
                                                                              
           
   
void
_hash_finish_split(Relation rel, Buffer metabuf, Buffer obuf, Bucket obucket, uint32 maxbucket, uint32 highmask, uint32 lowmask)
{
  HASHCTL hash_ctl;
  HTAB *tidhtab;
  Buffer bucket_nbuf = InvalidBuffer;
  Buffer nbuf;
  Page npage;
  BlockNumber nblkno;
  BlockNumber bucket_nblkno;
  HashPageOpaque npageopaque;
  Bucket nbucket;
  bool found;

                                                 
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(ItemPointerData);
  hash_ctl.entrysize = sizeof(ItemPointerData);
  hash_ctl.hcxt = CurrentMemoryContext;

  tidhtab = hash_create("bucket ctids", 256,                             
      &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  bucket_nblkno = nblkno = _hash_get_newblock_from_oldbucket(rel, obucket);

     
                                                      
     
  for (;;)
  {
    OffsetNumber noffnum;
    OffsetNumber nmaxoffnum;

    nbuf = _hash_getbuf(rel, nblkno, HASH_READ, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);

                                                                           
    if (nblkno == bucket_nblkno)
    {
      bucket_nbuf = nbuf;
    }

    npage = BufferGetPage(nbuf);
    npageopaque = (HashPageOpaque)PageGetSpecialPointer(npage);

                                     
    nmaxoffnum = PageGetMaxOffsetNumber(npage);
    for (noffnum = FirstOffsetNumber; noffnum <= nmaxoffnum; noffnum = OffsetNumberNext(noffnum))
    {
      IndexTuple itup;

                                                             
      itup = (IndexTuple)PageGetItem(npage, PageGetItemId(npage, noffnum));

      (void)hash_search(tidhtab, &itup->t_tid, HASH_ENTER, &found);

      Assert(!found);
    }

    nblkno = npageopaque->hasho_nextblkno;

       
                                                                     
                                         
       
    if (nbuf == bucket_nbuf)
    {
      LockBuffer(nbuf, BUFFER_LOCK_UNLOCK);
    }
    else
    {
      _hash_relbuf(rel, nbuf);
    }

                                                           
    if (!BlockNumberIsValid(nblkno))
    {
      break;
    }
  }

     
                                                                          
                                                                            
                                                                        
            
     
  if (!ConditionalLockBufferForCleanup(obuf))
  {
    hash_destroy(tidhtab);
    return;
  }
  if (!ConditionalLockBufferForCleanup(bucket_nbuf))
  {
    LockBuffer(obuf, BUFFER_LOCK_UNLOCK);
    hash_destroy(tidhtab);
    return;
  }

  npage = BufferGetPage(bucket_nbuf);
  npageopaque = (HashPageOpaque)PageGetSpecialPointer(npage);
  nbucket = npageopaque->hasho_bucket;

  _hash_splitbucket(rel, metabuf, obucket, nbucket, obuf, bucket_nbuf, tidhtab, maxbucket, highmask, lowmask);

  _hash_dropbuf(rel, bucket_nbuf);
  hash_destroy(tidhtab);
}

   
                                               
   
                                                                         
                              
   
                                                                              
       
   
static void
log_split_page(Relation rel, Buffer buf)
{
  if (RelationNeedsWAL(rel))
  {
    XLogRecPtr recptr;

    XLogBeginInsert();

    XLogRegisterBuffer(0, buf, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_SPLIT_PAGE);

    PageSetLSN(BufferGetPage(buf), recptr);
  }
}

   
                                                           
   
                                                                            
                                                                          
                                                                         
                                      
   
                                                                              
   
HashMetaPage
_hash_getcachedmetap(Relation rel, Buffer *metabuf, bool force_refresh)
{
  Page page;

  Assert(metabuf);
  if (force_refresh || rel->rd_amcache == NULL)
  {
    char *cache = NULL;

       
                                                                        
                                                                      
                                                                      
                                                  
       
    if (rel->rd_amcache == NULL)
    {
      cache = MemoryContextAlloc(rel->rd_indexcxt, sizeof(HashMetaPageData));
    }

                            
    if (BufferIsValid(*metabuf))
    {
      LockBuffer(*metabuf, BUFFER_LOCK_SHARE);
    }
    else
    {
      *metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ, LH_META_PAGE);
    }
    page = BufferGetPage(*metabuf);

                             
    if (rel->rd_amcache == NULL)
    {
      rel->rd_amcache = cache;
    }
    memcpy(rel->rd_amcache, HashPageGetMeta(page), sizeof(HashMetaPageData));

                                                  
    LockBuffer(*metabuf, BUFFER_LOCK_UNLOCK);
  }

  return (HashMetaPage)rel->rd_amcache;
}

   
                                                                              
                      
   
                                                                              
                                                                            
                                                                              
                                                                              
                                                                           
   
                                                                             
                                                          
   
                                                                        
                                                                              
                                                             
   
Buffer
_hash_getbucketbuf_from_hashkey(Relation rel, uint32 hashkey, int access, HashMetaPage *cachedmetap)
{
  HashMetaPage metap;
  Buffer buf;
  Buffer metabuf = InvalidBuffer;
  Page page;
  Bucket bucket;
  BlockNumber blkno;
  HashPageOpaque opaque;

                                                                 
  Assert(access == HASH_READ || access == HASH_WRITE);

  metap = _hash_getcachedmetap(rel, &metabuf, false);
  Assert(metap != NULL);

     
                                                            
     
  for (;;)
  {
       
                                                                      
       
    bucket = _hash_hashkey2bucket(hashkey, metap->hashm_maxbucket, metap->hashm_highmask, metap->hashm_lowmask);

    blkno = BUCKET_TO_BLKNO(metap, bucket);

                                                      
    buf = _hash_getbuf(rel, blkno, access, LH_BUCKET_PAGE);
    page = BufferGetPage(buf);
    opaque = (HashPageOpaque)PageGetSpecialPointer(page);
    Assert(opaque->hasho_bucket == bucket);
    Assert(opaque->hasho_prevblkno != InvalidBlockNumber);

       
                                                     
       
    if (opaque->hasho_prevblkno <= metap->hashm_maxbucket)
    {
      break;
    }

                                                                      
    _hash_relbuf(rel, buf);
    metap = _hash_getcachedmetap(rel, &metabuf, true);
    Assert(metap != NULL);
  }

  if (BufferIsValid(metabuf))
  {
    _hash_dropbuf(rel, metabuf);
  }

  if (cachedmetap)
  {
    *cachedmetap = metap;
  }

  return buf;
}
