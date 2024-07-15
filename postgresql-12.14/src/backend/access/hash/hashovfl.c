                                                                            
   
              
                                                                       
   
                                                                         
                                                                        
   
   
                  
                                        
   
         
                                                       
   
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "miscadmin.h"
#include "utils/rel.h"

static uint32
_hash_firstfreebit(uint32 map);

   
                                                                         
                                     
   
static BlockNumber
bitno_to_blkno(HashMetaPage metap, uint32 ovflbitnum)
{
  uint32 splitnum = metap->hashm_ovflpoint;
  uint32 i;

                                                           
  ovflbitnum += 1;

                                                               
  for (i = 1; i < splitnum && ovflbitnum > metap->hashm_spares[i]; i++)
              ;

     
                                                                          
                                         
     
  return (BlockNumber)(_hash_get_totalbuckets(i) + ovflbitnum);
}

   
                            
   
                                                                          
   
uint32
_hash_ovflblkno_to_bitno(HashMetaPage metap, BlockNumber ovflblkno)
{
  uint32 splitnum = metap->hashm_ovflpoint;
  uint32 i;
  uint32 bitnum;

                                                       
  for (i = 1; i <= splitnum; i++)
  {
    if (ovflblkno <= (BlockNumber)_hash_get_totalbuckets(i))
    {
      break;           
    }
    bitnum = ovflblkno - _hash_get_totalbuckets(i);

       
                                                                      
                                                                           
                                                      
                                        
       
    if (bitnum > metap->hashm_spares[i - 1] && bitnum <= metap->hashm_spares[i])
    {
      return bitnum - 1;                                       
    }
  }

  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid overflow block number %u", ovflblkno)));
  return 0;                          
}

   
                     
   
                                                                              
   
                                                                          
                                                                           
                                                                           
                                                                  
                                               
   
                                                                    
                                              
   
                                                                        
                                                                     
                                                                            
                                                               
   
Buffer
_hash_addovflpage(Relation rel, Buffer metabuf, Buffer buf, bool retain_pin)
{
  Buffer ovflbuf;
  Page page;
  Page ovflpage;
  HashPageOpaque pageopaque;
  HashPageOpaque ovflopaque;
  HashMetaPage metap;
  Buffer mapbuf = InvalidBuffer;
  Buffer newmapbuf = InvalidBuffer;
  BlockNumber blkno;
  uint32 orig_firstfree;
  uint32 splitnum;
  uint32 *freep = NULL;
  uint32 max_ovflpg;
  uint32 bit;
  uint32 bitmap_page_bit;
  uint32 first_page;
  uint32 last_bit;
  uint32 last_page;
  uint32 i, j;
  bool page_found = false;

     
                                                                             
                                                                            
                                                                            
                                                                             
                                                                         
                    
     
                                                                          
                                                                             
                                                                           
                                                                           
                                                                  
                                        
     
  LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

                             
  _hash_checkpage(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);

                                                                         
  for (;;)
  {
    BlockNumber nextblkno;

    page = BufferGetPage(buf);
    pageopaque = (HashPageOpaque)PageGetSpecialPointer(page);
    nextblkno = pageopaque->hasho_nextblkno;

    if (!BlockNumberIsValid(nextblkno))
    {
      break;
    }

                                                               
    if (retain_pin)
    {
                                                                 
      Assert((pageopaque->hasho_flag & LH_PAGE_TYPE) == LH_BUCKET_PAGE);
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    }
    else
    {
      _hash_relbuf(rel, buf);
    }

    retain_pin = false;

    buf = _hash_getbuf(rel, nextblkno, HASH_WRITE, LH_OVERFLOW_PAGE);
  }

                                           
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

  _hash_checkpage(rel, metabuf, LH_META_PAGE);
  metap = HashPageGetMeta(BufferGetPage(metabuf));

                                       
  orig_firstfree = metap->hashm_firstfree;
  first_page = orig_firstfree >> BMPG_SHIFT(metap);
  bit = orig_firstfree & BMPG_MASK(metap);
  i = first_page;
  j = bit / BITS_PER_MAP;
  bit &= ~(BITS_PER_MAP - 1);

                                                
  for (;;)
  {
    BlockNumber mapblkno;
    Page mappage;
    uint32 last_inpage;

                                                                 
    splitnum = metap->hashm_ovflpoint;
    max_ovflpg = metap->hashm_spares[splitnum] - 1;
    last_page = max_ovflpg >> BMPG_SHIFT(metap);
    last_bit = max_ovflpg & BMPG_MASK(metap);

    if (i > last_page)
    {
      break;
    }

    Assert(i < metap->hashm_nmaps);
    mapblkno = metap->hashm_mapp[i];

    if (i == last_page)
    {
      last_inpage = last_bit;
    }
    else
    {
      last_inpage = BMPGSZ_BIT(metap) - 1;
    }

                                                                      
    LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

    mapbuf = _hash_getbuf(rel, mapblkno, HASH_WRITE, LH_BITMAP_PAGE);
    mappage = BufferGetPage(mapbuf);
    freep = HashPageGetBitmap(mappage);

    for (; bit <= last_inpage; j++, bit += BITS_PER_MAP)
    {
      if (freep[j] != ALL_SET)
      {
        page_found = true;

                                                       
        LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

                                                   
        bit += _hash_firstfreebit(freep[j]);
        bitmap_page_bit = bit;

                                                
        bit += (i << BMPG_SHIFT(metap));
                                                             
        blkno = bitno_to_blkno(metap, bit);

                                              
        ovflbuf = _hash_getinitbuf(rel, blkno);

        goto found;
      }
    }

                                                             
    _hash_relbuf(rel, mapbuf);
    mapbuf = InvalidBuffer;
    i++;
    j = 0;                                       
    bit = 0;

                                                   
    LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);
  }

     
                                                                            
                                                                  
     
  if (last_bit == (uint32)(BMPGSZ_BIT(metap) - 1))
  {
       
                                                                     
                                                               
                                                                         
                                                                        
                                                                      
                                                    
       
    bit = metap->hashm_spares[splitnum];

                                           
    if (metap->hashm_nmaps >= HASH_MAX_BITMAPS)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of overflow pages in hash index \"%s\"", RelationGetRelationName(rel))));
    }

    newmapbuf = _hash_getnewbuf(rel, bitno_to_blkno(metap, bit), MAIN_FORKNUM);
  }
  else
  {
       
                                                                           
                                                              
       
  }

                                                  
  bit = BufferIsValid(newmapbuf) ? metap->hashm_spares[splitnum] + 1 : metap->hashm_spares[splitnum];
  blkno = bitno_to_blkno(metap, bit);

     
                                                                      
                                                                            
                                                                       
                                      
     
                                                                          
                                                                       
                                       
     
  ovflbuf = _hash_getnewbuf(rel, blkno, MAIN_FORKNUM);

found:

     
                                                                            
                                                                         
                                                  
     
  START_CRIT_SECTION();

  if (page_found)
  {
    Assert(BufferIsValid(mapbuf));

                                          
    SETBIT(freep, bitmap_page_bit);
    MarkBufferDirty(mapbuf);
  }
  else
  {
                                                                 
    metap->hashm_spares[splitnum]++;

    if (BufferIsValid(newmapbuf))
    {
      _hash_initbitmapbuffer(newmapbuf, metap->hashm_bmsize, false);
      MarkBufferDirty(newmapbuf);

                                                                     
      metap->hashm_mapp[metap->hashm_nmaps] = BufferGetBlockNumber(newmapbuf);
      metap->hashm_nmaps++;
      metap->hashm_spares[splitnum]++;
    }

    MarkBufferDirty(metabuf);

       
                                                                         
                                                                
       
  }

     
                                                                         
                                                                           
     
  if (metap->hashm_firstfree == orig_firstfree)
  {
    metap->hashm_firstfree = bit + 1;
    MarkBufferDirty(metabuf);
  }

                                    
  ovflpage = BufferGetPage(ovflbuf);
  ovflopaque = (HashPageOpaque)PageGetSpecialPointer(ovflpage);
  ovflopaque->hasho_prevblkno = BufferGetBlockNumber(buf);
  ovflopaque->hasho_nextblkno = InvalidBlockNumber;
  ovflopaque->hasho_bucket = pageopaque->hasho_bucket;
  ovflopaque->hasho_flag = LH_OVERFLOW_PAGE;
  ovflopaque->hasho_page_id = HASHO_PAGE_ID;

  MarkBufferDirty(ovflbuf);

                                                      
  pageopaque->hasho_nextblkno = BufferGetBlockNumber(ovflbuf);

  MarkBufferDirty(buf);

                  
  if (RelationNeedsWAL(rel))
  {
    XLogRecPtr recptr;
    xl_hash_add_ovfl_page xlrec;

    xlrec.bmpage_found = page_found;
    xlrec.bmsize = metap->hashm_bmsize;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHashAddOvflPage);

    XLogRegisterBuffer(0, ovflbuf, REGBUF_WILL_INIT);
    XLogRegisterBufData(0, (char *)&pageopaque->hasho_bucket, sizeof(Bucket));

    XLogRegisterBuffer(1, buf, REGBUF_STANDARD);

    if (BufferIsValid(mapbuf))
    {
      XLogRegisterBuffer(2, mapbuf, REGBUF_STANDARD);
      XLogRegisterBufData(2, (char *)&bitmap_page_bit, sizeof(uint32));
    }

    if (BufferIsValid(newmapbuf))
    {
      XLogRegisterBuffer(3, newmapbuf, REGBUF_WILL_INIT);
    }

    XLogRegisterBuffer(4, metabuf, REGBUF_STANDARD);
    XLogRegisterBufData(4, (char *)&metap->hashm_firstfree, sizeof(uint32));

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_ADD_OVFL_PAGE);

    PageSetLSN(BufferGetPage(ovflbuf), recptr);
    PageSetLSN(BufferGetPage(buf), recptr);

    if (BufferIsValid(mapbuf))
    {
      PageSetLSN(BufferGetPage(mapbuf), recptr);
    }

    if (BufferIsValid(newmapbuf))
    {
      PageSetLSN(BufferGetPage(newmapbuf), recptr);
    }

    PageSetLSN(BufferGetPage(metabuf), recptr);
  }

  END_CRIT_SECTION();

  if (retain_pin)
  {
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
  }
  else
  {
    _hash_relbuf(rel, buf);
  }

  if (BufferIsValid(mapbuf))
  {
    _hash_relbuf(rel, mapbuf);
  }

  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

  if (BufferIsValid(newmapbuf))
  {
    _hash_relbuf(rel, newmapbuf);
  }

  return ovflbuf;
}

   
                        
   
                                                                         
   
static uint32
_hash_firstfreebit(uint32 map)
{
  uint32 i, mask;

  mask = 0x1;
  for (i = 0; i < BITS_PER_MAP; i++)
  {
    if (!(mask & map))
    {
      return i;
    }
    mask <<= 1;
  }

  elog(ERROR, "firstfreebit found no free bit");

  return 0;                          
}

   
                          
   
                                                                           
                                                                            
   
                                                                             
                                                                             
                                                                         
                                                                          
                                                                
   
                                                                           
                                                        
   
                                                                     
                                                              
   
                                                                          
                                                                          
                                                                              
                       
   
BlockNumber
_hash_freeovflpage(Relation rel, Buffer bucketbuf, Buffer ovflbuf, Buffer wbuf, IndexTuple *itups, OffsetNumber *itup_offsets, Size *tups_size, uint16 nitups, BufferAccessStrategy bstrategy)
{
  HashMetaPage metap;
  Buffer metabuf;
  Buffer mapbuf;
  BlockNumber ovflblkno;
  BlockNumber prevblkno;
  BlockNumber blkno;
  BlockNumber nextblkno;
  BlockNumber writeblkno;
  HashPageOpaque ovflopaque;
  Page ovflpage;
  Page mappage;
  uint32 *freep;
  uint32 ovflbitno;
  int32 bitmappage, bitmapbit;
  Bucket bucket PG_USED_FOR_ASSERTS_ONLY;
  Buffer prevbuf = InvalidBuffer;
  Buffer nextbuf = InvalidBuffer;
  bool update_metap = false;

                                            
  _hash_checkpage(rel, ovflbuf, LH_OVERFLOW_PAGE);
  ovflblkno = BufferGetBlockNumber(ovflbuf);
  ovflpage = BufferGetPage(ovflbuf);
  ovflopaque = (HashPageOpaque)PageGetSpecialPointer(ovflpage);
  nextblkno = ovflopaque->hasho_nextblkno;
  prevblkno = ovflopaque->hasho_prevblkno;
  writeblkno = BufferGetBlockNumber(wbuf);
  bucket = ovflopaque->hasho_bucket;

     
                                                                            
                                                                             
                                                                        
                                       
     
  if (BlockNumberIsValid(prevblkno))
  {
    if (prevblkno == writeblkno)
    {
      prevbuf = wbuf;
    }
    else
    {
      prevbuf = _hash_getbuf_with_strategy(rel, prevblkno, HASH_WRITE, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE, bstrategy);
    }
  }
  if (BlockNumberIsValid(nextblkno))
  {
    nextbuf = _hash_getbuf_with_strategy(rel, nextblkno, HASH_WRITE, LH_OVERFLOW_PAGE, bstrategy);
  }

                                                                         

                                                                      
  metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ, LH_META_PAGE);
  metap = HashPageGetMeta(BufferGetPage(metabuf));

                                 
  ovflbitno = _hash_ovflblkno_to_bitno(metap, ovflblkno);

  bitmappage = ovflbitno >> BMPG_SHIFT(metap);
  bitmapbit = ovflbitno & BMPG_MASK(metap);

  if (bitmappage >= metap->hashm_nmaps)
  {
    elog(ERROR, "invalid overflow bit number %u", ovflbitno);
  }
  blkno = metap->hashm_mapp[bitmappage];

                                                             
  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

                                                    
  mapbuf = _hash_getbuf(rel, blkno, HASH_WRITE, LH_BITMAP_PAGE);
  mappage = BufferGetPage(mapbuf);
  freep = HashPageGetBitmap(mappage);
  Assert(ISSET(freep, bitmapbit));

                                                      
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

                                                                         
  if (RelationNeedsWAL(rel))
  {
    XLogEnsureRecordSpace(HASH_XLOG_FREE_OVFL_BUFS, 4 + nitups);
  }

  START_CRIT_SECTION();

     
                                                                             
                                                                             
                                        
     
  if (nitups > 0)
  {
    _hash_pgaddmultitup(rel, wbuf, itups, itup_offsets, nitups);
    MarkBufferDirty(wbuf);
  }

     
                                                                        
                                                                           
                                                                         
                                                                     
                                     
     
  _hash_pageinit(ovflpage, BufferGetPageSize(ovflbuf));

  ovflopaque = (HashPageOpaque)PageGetSpecialPointer(ovflpage);

  ovflopaque->hasho_prevblkno = InvalidBlockNumber;
  ovflopaque->hasho_nextblkno = InvalidBlockNumber;
  ovflopaque->hasho_bucket = -1;
  ovflopaque->hasho_flag = LH_UNUSED_PAGE;
  ovflopaque->hasho_page_id = HASHO_PAGE_ID;

  MarkBufferDirty(ovflbuf);

  if (BufferIsValid(prevbuf))
  {
    Page prevpage = BufferGetPage(prevbuf);
    HashPageOpaque prevopaque = (HashPageOpaque)PageGetSpecialPointer(prevpage);

    Assert(prevopaque->hasho_bucket == bucket);
    prevopaque->hasho_nextblkno = nextblkno;
    MarkBufferDirty(prevbuf);
  }
  if (BufferIsValid(nextbuf))
  {
    Page nextpage = BufferGetPage(nextbuf);
    HashPageOpaque nextopaque = (HashPageOpaque)PageGetSpecialPointer(nextpage);

    Assert(nextopaque->hasho_bucket == bucket);
    nextopaque->hasho_prevblkno = prevblkno;
    MarkBufferDirty(nextbuf);
  }

                                                                        
  CLRBIT(freep, bitmapbit);
  MarkBufferDirty(mapbuf);

                                                                  
  if (ovflbitno < metap->hashm_firstfree)
  {
    metap->hashm_firstfree = ovflbitno;
    update_metap = true;
    MarkBufferDirty(metabuf);
  }

                  
  if (RelationNeedsWAL(rel))
  {
    xl_hash_squeeze_page xlrec;
    XLogRecPtr recptr;
    int i;

    xlrec.prevblkno = prevblkno;
    xlrec.nextblkno = nextblkno;
    xlrec.ntups = nitups;
    xlrec.is_prim_bucket_same_wrt = (wbuf == bucketbuf);
    xlrec.is_prev_bucket_same_wrt = (wbuf == prevbuf);

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHashSqueezePage);

       
                                                                          
                                           
       
    if (!xlrec.is_prim_bucket_same_wrt)
    {
      XLogRegisterBuffer(0, bucketbuf, REGBUF_STANDARD | REGBUF_NO_IMAGE);
    }

    XLogRegisterBuffer(1, wbuf, REGBUF_STANDARD);
    if (xlrec.ntups > 0)
    {
      XLogRegisterBufData(1, (char *)itup_offsets, nitups * sizeof(OffsetNumber));
      for (i = 0; i < nitups; i++)
      {
        XLogRegisterBufData(1, (char *)itups[i], tups_size[i]);
      }
    }

    XLogRegisterBuffer(2, ovflbuf, REGBUF_STANDARD);

       
                                                                          
                                                                    
                                                                         
                  
       
    if (BufferIsValid(prevbuf) && !xlrec.is_prev_bucket_same_wrt)
    {
      XLogRegisterBuffer(3, prevbuf, REGBUF_STANDARD);
    }

    if (BufferIsValid(nextbuf))
    {
      XLogRegisterBuffer(4, nextbuf, REGBUF_STANDARD);
    }

    XLogRegisterBuffer(5, mapbuf, REGBUF_STANDARD);
    XLogRegisterBufData(5, (char *)&bitmapbit, sizeof(uint32));

    if (update_metap)
    {
      XLogRegisterBuffer(6, metabuf, REGBUF_STANDARD);
      XLogRegisterBufData(6, (char *)&metap->hashm_firstfree, sizeof(uint32));
    }

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_SQUEEZE_PAGE);

    PageSetLSN(BufferGetPage(wbuf), recptr);
    PageSetLSN(BufferGetPage(ovflbuf), recptr);

    if (BufferIsValid(prevbuf) && !xlrec.is_prev_bucket_same_wrt)
    {
      PageSetLSN(BufferGetPage(prevbuf), recptr);
    }
    if (BufferIsValid(nextbuf))
    {
      PageSetLSN(BufferGetPage(nextbuf), recptr);
    }

    PageSetLSN(BufferGetPage(mapbuf), recptr);

    if (update_metap)
    {
      PageSetLSN(BufferGetPage(metabuf), recptr);
    }
  }

  END_CRIT_SECTION();

                                                                 
  if (BufferIsValid(prevbuf) && prevblkno != writeblkno)
  {
    _hash_relbuf(rel, prevbuf);
  }

  if (BufferIsValid(ovflbuf))
  {
    _hash_relbuf(rel, ovflbuf);
  }

  if (BufferIsValid(nextbuf))
  {
    _hash_relbuf(rel, nextbuf);
  }

  _hash_relbuf(rel, mapbuf);
  _hash_relbuf(rel, metabuf);

  return nextblkno;
}

   
                            
   
                                                                              
                              
   
void
_hash_initbitmapbuffer(Buffer buf, uint16 bmsize, bool initpage)
{
  Page pg;
  HashPageOpaque op;
  uint32 *freep;

  pg = BufferGetPage(buf);

                           
  if (initpage)
  {
    _hash_pageinit(pg, BufferGetPageSize(buf));
  }

                                           
  op = (HashPageOpaque)PageGetSpecialPointer(pg);
  op->hasho_prevblkno = InvalidBlockNumber;
  op->hasho_nextblkno = InvalidBlockNumber;
  op->hasho_bucket = -1;
  op->hasho_flag = LH_BITMAP_PAGE;
  op->hasho_page_id = HASHO_PAGE_ID;

                                
  freep = HashPageGetBitmap(pg);
  MemSet(freep, 0xFF, bmsize);

     
                                                                            
                                                                            
                                       
     
  ((PageHeader)pg)->pd_lower = ((char *)freep + bmsize) - (char *)pg;
}

   
                                    
   
                                                                 
                                                                    
                                                                    
                                                                    
                                                                  
                                                                     
                                                                   
                                               
   
                                                                       
                                                                   
                                                                              
                                                                        
                                                                       
   
                                                                      
                                                                        
                                                                           
                                                                       
                                                                      
                                                                        
   
                                                                              
                    
   
                                                                           
                                                        
   
void
_hash_squeezebucket(Relation rel, Bucket bucket, BlockNumber bucket_blkno, Buffer bucket_buf, BufferAccessStrategy bstrategy)
{
  BlockNumber wblkno;
  BlockNumber rblkno;
  Buffer wbuf;
  Buffer rbuf;
  Page wpage;
  Page rpage;
  HashPageOpaque wopaque;
  HashPageOpaque ropaque;

     
                                                   
     
  wblkno = bucket_blkno;
  wbuf = bucket_buf;
  wpage = BufferGetPage(wbuf);
  wopaque = (HashPageOpaque)PageGetSpecialPointer(wpage);

     
                                                                            
                                                                  
     
  if (!BlockNumberIsValid(wopaque->hasho_nextblkno))
  {
    LockBuffer(wbuf, BUFFER_LOCK_UNLOCK);
    return;
  }

     
                                                                           
                                                                            
                                                                           
                                                          
     
  rbuf = InvalidBuffer;
  ropaque = wopaque;
  do
  {
    rblkno = ropaque->hasho_nextblkno;
    if (rbuf != InvalidBuffer)
    {
      _hash_relbuf(rel, rbuf);
    }
    rbuf = _hash_getbuf_with_strategy(rel, rblkno, HASH_WRITE, LH_OVERFLOW_PAGE, bstrategy);
    rpage = BufferGetPage(rbuf);
    ropaque = (HashPageOpaque)PageGetSpecialPointer(rpage);
    Assert(ropaque->hasho_bucket == bucket);
  } while (BlockNumberIsValid(ropaque->hasho_nextblkno));

     
                         
     
  for (;;)
  {
    OffsetNumber roffnum;
    OffsetNumber maxroffnum;
    OffsetNumber deletable[MaxOffsetNumber];
    IndexTuple itups[MaxIndexTuplesPerPage];
    Size tups_size[MaxIndexTuplesPerPage];
    OffsetNumber itup_offsets[MaxIndexTuplesPerPage];
    uint16 ndeletable = 0;
    uint16 nitups = 0;
    Size all_tups_size = 0;
    int i;
    bool retain_pin = false;

  readpage:
                                        
    maxroffnum = PageGetMaxOffsetNumber(rpage);
    for (roffnum = FirstOffsetNumber; roffnum <= maxroffnum; roffnum = OffsetNumberNext(roffnum))
    {
      IndexTuple itup;
      Size itemsz;

                            
      if (ItemIdIsDead(PageGetItemId(rpage, roffnum)))
      {
        continue;
      }

      itup = (IndexTuple)PageGetItem(rpage, PageGetItemId(rpage, roffnum));
      itemsz = IndexTupleSize(itup);
      itemsz = MAXALIGN(itemsz);

         
                                                                     
                                                                      
                        
         
      while (PageGetFreeSpaceForMultipleTuples(wpage, nitups + 1) < (all_tups_size + itemsz))
      {
        Buffer next_wbuf = InvalidBuffer;
        bool tups_moved = false;

        Assert(!PageIsEmpty(wpage));

        if (wblkno == bucket_blkno)
        {
          retain_pin = true;
        }

        wblkno = wopaque->hasho_nextblkno;
        Assert(BlockNumberIsValid(wblkno));

                                                                         
        if (wblkno != rblkno)
        {
          next_wbuf = _hash_getbuf_with_strategy(rel, wblkno, HASH_WRITE, LH_OVERFLOW_PAGE, bstrategy);
        }

        if (nitups > 0)
        {
          Assert(nitups == ndeletable);

             
                                                                  
                           
             
          if (RelationNeedsWAL(rel))
          {
            XLogEnsureRecordSpace(0, 3 + nitups);
          }

          START_CRIT_SECTION();

             
                                                                 
                                                                  
                                                                
                                    
             
          _hash_pgaddmultitup(rel, wbuf, itups, itup_offsets, nitups);
          MarkBufferDirty(wbuf);

                                                            
          PageIndexMultiDelete(rpage, deletable, ndeletable);
          MarkBufferDirty(rbuf);

                          
          if (RelationNeedsWAL(rel))
          {
            XLogRecPtr recptr;
            xl_hash_move_page_contents xlrec;

            xlrec.ntups = nitups;
            xlrec.is_prim_bucket_same_wrt = (wbuf == bucket_buf) ? true : false;

            XLogBeginInsert();
            XLogRegisterData((char *)&xlrec, SizeOfHashMovePageContents);

               
                                                                   
                                                                  
               
            if (!xlrec.is_prim_bucket_same_wrt)
            {
              XLogRegisterBuffer(0, bucket_buf, REGBUF_STANDARD | REGBUF_NO_IMAGE);
            }

            XLogRegisterBuffer(1, wbuf, REGBUF_STANDARD);
            XLogRegisterBufData(1, (char *)itup_offsets, nitups * sizeof(OffsetNumber));
            for (i = 0; i < nitups; i++)
            {
              XLogRegisterBufData(1, (char *)itups[i], tups_size[i]);
            }

            XLogRegisterBuffer(2, rbuf, REGBUF_STANDARD);
            XLogRegisterBufData(2, (char *)deletable, ndeletable * sizeof(OffsetNumber));

            recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_MOVE_PAGE_CONTENTS);

            PageSetLSN(BufferGetPage(wbuf), recptr);
            PageSetLSN(BufferGetPage(rbuf), recptr);
          }

          END_CRIT_SECTION();

          tups_moved = true;
        }

           
                                                                      
                        
           
        if (retain_pin)
        {
          LockBuffer(wbuf, BUFFER_LOCK_UNLOCK);
        }
        else
        {
          _hash_relbuf(rel, wbuf);
        }

                                                            
        if (rblkno == wblkno)
        {
          _hash_relbuf(rel, rbuf);
          return;
        }

        wbuf = next_wbuf;
        wpage = BufferGetPage(wbuf);
        wopaque = (HashPageOpaque)PageGetSpecialPointer(wpage);
        Assert(wopaque->hasho_bucket == bucket);
        retain_pin = false;

                     
        for (i = 0; i < nitups; i++)
        {
          pfree(itups[i]);
        }
        nitups = 0;
        all_tups_size = 0;
        ndeletable = 0;

           
                                                                     
                                    
           
        if (tups_moved)
        {
          goto readpage;
        }
      }

                                                        
      deletable[ndeletable++] = roffnum;

         
                                                                        
                                                                      
                             
         
      itups[nitups] = CopyIndexTuple(itup);
      tups_size[nitups++] = itemsz;
      all_tups_size += itemsz;
    }

       
                                                                         
                                                                        
                                                                 
                                                                
       
                                                                          
                                                               
                                                                       
                                                                    
       
    rblkno = ropaque->hasho_prevblkno;
    Assert(BlockNumberIsValid(rblkno));

                                                 
    _hash_freeovflpage(rel, bucket_buf, rbuf, wbuf, itups, itup_offsets, tups_size, nitups, bstrategy);

                 
    for (i = 0; i < nitups; i++)
    {
      pfree(itups[i]);
    }

                                                   
    if (rblkno == wblkno)
    {
                                                                         
      if (wblkno == bucket_blkno)
      {
        LockBuffer(wbuf, BUFFER_LOCK_UNLOCK);
      }
      else
      {
        _hash_relbuf(rel, wbuf);
      }
      return;
    }

    rbuf = _hash_getbuf_with_strategy(rel, rblkno, HASH_WRITE, LH_OVERFLOW_PAGE, bstrategy);
    rpage = BufferGetPage(rbuf);
    ropaque = (HashPageOpaque)PageGetSpecialPointer(rpage);
    Assert(ropaque->hasho_bucket == bucket);
  }

                  
}
