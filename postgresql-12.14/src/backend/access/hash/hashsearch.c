                                                                            
   
                
                                          
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/rel.h"
#include "storage/predicate.h"

static bool
_hash_readpage(IndexScanDesc scan, Buffer *bufP, ScanDirection dir);
static int
_hash_load_qualified_items(IndexScanDesc scan, Page page, OffsetNumber offnum, ScanDirection dir);
static inline void
_hash_saveitem(HashScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup);
static void
_hash_readnext(IndexScanDesc scan, Buffer *bufp, Page *pagep, HashPageOpaque *opaquep);

   
                                                
   
                                                                
                                                                   
                                        
   
                                                               
                                                              
   
                                                               
                                                              
          
   
bool
_hash_next(IndexScanDesc scan, ScanDirection dir)
{
  Relation rel = scan->indexRelation;
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  HashScanPosItem *currItem;
  BlockNumber blkno;
  Buffer buf;
  bool end_of_scan = false;

     
                                                                            
                                                                             
                                                                             
                   
     
  if (ScanDirectionIsForward(dir))
  {
    if (++so->currPos.itemIndex > so->currPos.lastItem)
    {
      if (so->numKilled > 0)
      {
        _hash_kill_items(scan);
      }

      blkno = so->currPos.nextPage;
      if (BlockNumberIsValid(blkno))
      {
        buf = _hash_getbuf(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
        TestForOldSnapshot(scan->xs_snapshot, rel, BufferGetPage(buf));
        if (!_hash_readpage(scan, &buf, dir))
        {
          end_of_scan = true;
        }
      }
      else
      {
        end_of_scan = true;
      }
    }
  }
  else
  {
    if (--so->currPos.itemIndex < so->currPos.firstItem)
    {
      if (so->numKilled > 0)
      {
        _hash_kill_items(scan);
      }

      blkno = so->currPos.prevPage;
      if (BlockNumberIsValid(blkno))
      {
        buf = _hash_getbuf(rel, blkno, HASH_READ, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
        TestForOldSnapshot(scan->xs_snapshot, rel, BufferGetPage(buf));

           
                                                                    
                                                                       
                 
           
        if (buf == so->hashso_bucket_buf || buf == so->hashso_split_bucket_buf)
        {
          _hash_dropbuf(rel, buf);
        }

        if (!_hash_readpage(scan, &buf, dir))
        {
          end_of_scan = true;
        }
      }
      else
      {
        end_of_scan = true;
      }
    }
  }

  if (end_of_scan)
  {
    _hash_dropscanbuf(rel, so);
    HashScanPosInvalidate(so->currPos);
    return false;
  }

                                         
  currItem = &so->currPos.items[so->currPos.itemIndex];
  scan->xs_heaptid = currItem->heapTid;

  return true;
}

   
                                                                            
                                                                             
                                                                            
   
static void
_hash_readnext(IndexScanDesc scan, Buffer *bufp, Page *pagep, HashPageOpaque *opaquep)
{
  BlockNumber blkno;
  Relation rel = scan->indexRelation;
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  bool block_found = false;

  blkno = (*opaquep)->hasho_nextblkno;

     
                                                                            
                                                                  
     
  if (*bufp == so->hashso_bucket_buf || *bufp == so->hashso_split_bucket_buf)
  {
    LockBuffer(*bufp, BUFFER_LOCK_UNLOCK);
  }
  else
  {
    _hash_relbuf(rel, *bufp);
  }

  *bufp = InvalidBuffer;
                                                                    
  CHECK_FOR_INTERRUPTS();
  if (BlockNumberIsValid(blkno))
  {
    *bufp = _hash_getbuf(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
    block_found = true;
  }
  else if (so->hashso_buc_populated && !so->hashso_buc_split)
  {
       
                                                                      
                                      
       
    *bufp = so->hashso_split_bucket_buf;

       
                                                                         
                                                                      
       
    Assert(BufferIsValid(*bufp));

    LockBuffer(*bufp, BUFFER_LOCK_SHARE);
    PredicateLockPage(rel, BufferGetBlockNumber(*bufp), scan->xs_snapshot);

       
                                                                       
                           
       
    so->hashso_buc_split = true;

    block_found = true;
  }

  if (block_found)
  {
    *pagep = BufferGetPage(*bufp);
    TestForOldSnapshot(scan->xs_snapshot, rel, *pagep);
    *opaquep = (HashPageOpaque)PageGetSpecialPointer(*pagep);
  }
}

   
                                                                          
                                                                        
                                                                      
   
static void
_hash_readprev(IndexScanDesc scan, Buffer *bufp, Page *pagep, HashPageOpaque *opaquep)
{
  BlockNumber blkno;
  Relation rel = scan->indexRelation;
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  bool haveprevblk;

  blkno = (*opaquep)->hasho_prevblkno;

     
                                                                            
                                                                  
     
  if (*bufp == so->hashso_bucket_buf || *bufp == so->hashso_split_bucket_buf)
  {
    LockBuffer(*bufp, BUFFER_LOCK_UNLOCK);
    haveprevblk = false;
  }
  else
  {
    _hash_relbuf(rel, *bufp);
    haveprevblk = true;
  }

  *bufp = InvalidBuffer;
                                                                    
  CHECK_FOR_INTERRUPTS();

  if (haveprevblk)
  {
    Assert(BlockNumberIsValid(blkno));
    *bufp = _hash_getbuf(rel, blkno, HASH_READ, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
    *pagep = BufferGetPage(*bufp);
    TestForOldSnapshot(scan->xs_snapshot, rel, *pagep);
    *opaquep = (HashPageOpaque)PageGetSpecialPointer(*pagep);

       
                                                                           
                                                              
       
    if (*bufp == so->hashso_bucket_buf || *bufp == so->hashso_split_bucket_buf)
    {
      _hash_dropbuf(rel, *bufp);
    }
  }
  else if (so->hashso_buc_populated && so->hashso_buc_split)
  {
       
                                                                          
                                      
       
    *bufp = so->hashso_bucket_buf;

       
                                                                         
                                                                          
       
    Assert(BufferIsValid(*bufp));

    LockBuffer(*bufp, BUFFER_LOCK_SHARE);
    *pagep = BufferGetPage(*bufp);
    *opaquep = (HashPageOpaque)PageGetSpecialPointer(*pagep);

                                         
    while (BlockNumberIsValid((*opaquep)->hasho_nextblkno))
    {
      _hash_readnext(scan, bufp, pagep, opaquep);
    }

       
                                                                        
                               
       
    so->hashso_buc_split = false;
  }
}

   
                                                   
   
                                                                        
                                                                    
                
   
                                                                         
                                                                            
                                                                      
                                                           
                                                                      
   
                                                                        
                                                            
   
bool
_hash_first(IndexScanDesc scan, ScanDirection dir)
{
  Relation rel = scan->indexRelation;
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  ScanKey cur;
  uint32 hashkey;
  Bucket bucket;
  Buffer buf;
  Page page;
  HashPageOpaque opaque;
  HashScanPosItem *currItem;

  pgstat_count_index_scan(rel);

     
                                                                          
                                                                          
                                                                            
                                                            
     
  if (scan->numberOfKeys < 1)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("hash indexes do not support whole-index scans")));
  }

                                                                         
  cur = &scan->keyData[0];

                                                  
  Assert(cur->sk_attno == 1);
                                                   
  Assert(cur->sk_strategy == HTEqualStrategyNumber);

     
                                                                           
                         
     
  if (cur->sk_flags & SK_ISNULL)
  {
    return false;
  }

     
                                                                            
                                                                     
     
                                                                        
                                                                       
     
                                                                       
                                                                            
     
  if (cur->sk_subtype == rel->rd_opcintype[0] || cur->sk_subtype == InvalidOid)
  {
    hashkey = _hash_datum2hashkey(rel, cur->sk_argument);
  }
  else
  {
    hashkey = _hash_datum2hashkey_type(rel, cur->sk_argument, cur->sk_subtype);
  }

  so->hashso_sk_hash = hashkey;

  buf = _hash_getbucketbuf_from_hashkey(rel, hashkey, HASH_READ, NULL);
  PredicateLockPage(rel, BufferGetBlockNumber(buf), scan->xs_snapshot);
  page = BufferGetPage(buf);
  TestForOldSnapshot(scan->xs_snapshot, rel, page);
  opaque = (HashPageOpaque)PageGetSpecialPointer(page);
  bucket = opaque->hasho_bucket;

  so->hashso_bucket_buf = buf;

     
                                                                            
                                                                          
                                                                         
                                                                         
                                                                       
                                                                       
                                                                          
                                                                           
                                                                           
                                                                      
                                                                          
                                                                          
                                                                      
             
     
  if (H_BUCKET_BEING_POPULATED(opaque))
  {
    BlockNumber old_blkno;
    Buffer old_buf;

    old_blkno = _hash_get_oldblock_from_newbucket(rel, bucket);

       
                                                                        
                               
       
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);

    old_buf = _hash_getbuf(rel, old_blkno, HASH_READ, LH_BUCKET_PAGE);
    TestForOldSnapshot(scan->xs_snapshot, rel, BufferGetPage(old_buf));

       
                                                                  
                 
       
    so->hashso_split_bucket_buf = old_buf;
    LockBuffer(old_buf, BUFFER_LOCK_UNLOCK);

    LockBuffer(buf, BUFFER_LOCK_SHARE);
    page = BufferGetPage(buf);
    opaque = (HashPageOpaque)PageGetSpecialPointer(page);
    Assert(opaque->hasho_bucket == bucket);

    if (H_BUCKET_BEING_POPULATED(opaque))
    {
      so->hashso_buc_populated = true;
    }
    else
    {
      _hash_dropbuf(rel, so->hashso_split_bucket_buf);
      so->hashso_split_bucket_buf = InvalidBuffer;
    }
  }

                                                                      
  if (ScanDirectionIsBackward(dir))
  {
       
                                                                         
                           
       
    while (BlockNumberIsValid(opaque->hasho_nextblkno) || (so->hashso_buc_populated && !so->hashso_buc_split))
    {
      _hash_readnext(scan, &buf, &page, &opaque);
    }
  }

                                                    
  Assert(BufferIsInvalid(so->currPos.buf));
  so->currPos.buf = buf;

                                                                        
  if (!_hash_readpage(scan, &buf, dir))
  {
    return false;
  }

                                         
  currItem = &so->currPos.items[so->currPos.itemIndex];
  scan->xs_heaptid = currItem->heapTid;

                                                          
  return true;
}

   
                                                                          
   
                                                                      
                                                                       
                                                                       
                                                    
   
                                                                  
   
static bool
_hash_readpage(IndexScanDesc scan, Buffer *bufP, ScanDirection dir)
{
  Relation rel = scan->indexRelation;
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  Buffer buf;
  Page page;
  HashPageOpaque opaque;
  OffsetNumber offnum;
  uint16 itemIndex;

  buf = *bufP;
  Assert(BufferIsValid(buf));
  _hash_checkpage(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
  page = BufferGetPage(buf);
  opaque = (HashPageOpaque)PageGetSpecialPointer(page);

  so->currPos.buf = buf;
  so->currPos.currPage = BufferGetBlockNumber(buf);

  if (ScanDirectionIsForward(dir))
  {
    BlockNumber prev_blkno = InvalidBlockNumber;

    for (;;)
    {
                                                               
      offnum = _hash_binsearch(page, so->hashso_sk_hash);

      itemIndex = _hash_load_qualified_items(scan, page, offnum, dir);

      if (itemIndex != 0)
      {
        break;
      }

         
                                                                         
                                                                       
                       
         
      if (so->numKilled > 0)
      {
        _hash_kill_items(scan);
      }

         
                                                                         
                       
         
      if (so->currPos.buf == so->hashso_bucket_buf || so->currPos.buf == so->hashso_split_bucket_buf)
      {
        prev_blkno = InvalidBlockNumber;
      }
      else
      {
        prev_blkno = opaque->hasho_prevblkno;
      }

      _hash_readnext(scan, &buf, &page, &opaque);
      if (BufferIsValid(buf))
      {
        so->currPos.buf = buf;
        so->currPos.currPage = BufferGetBlockNumber(buf);
      }
      else
      {
           
                                                                   
                                                               
                                                                     
                                                          
                                                                     
                             
           
        so->currPos.prevPage = prev_blkno;
        so->currPos.nextPage = InvalidBlockNumber;
        so->currPos.buf = buf;
        return false;
      }
    }

    so->currPos.firstItem = 0;
    so->currPos.lastItem = itemIndex - 1;
    so->currPos.itemIndex = 0;
  }
  else
  {
    BlockNumber next_blkno = InvalidBlockNumber;

    for (;;)
    {
                                                               
      offnum = _hash_binsearch_last(page, so->hashso_sk_hash);

      itemIndex = _hash_load_qualified_items(scan, page, offnum, dir);

      if (itemIndex != MaxIndexTuplesPerPage)
      {
        break;
      }

         
                                                                         
                                                                       
                           
         
      if (so->numKilled > 0)
      {
        _hash_kill_items(scan);
      }

      if (so->currPos.buf == so->hashso_bucket_buf || so->currPos.buf == so->hashso_split_bucket_buf)
      {
        next_blkno = opaque->hasho_nextblkno;
      }

      _hash_readprev(scan, &buf, &page, &opaque);
      if (BufferIsValid(buf))
      {
        so->currPos.buf = buf;
        so->currPos.currPage = BufferGetBlockNumber(buf);
      }
      else
      {
           
                                                                   
                                                               
                                                                     
                                                          
                                                                     
                             
           
        so->currPos.prevPage = InvalidBlockNumber;
        so->currPos.nextPage = next_blkno;
        so->currPos.buf = buf;
        return false;
      }
    }

    so->currPos.firstItem = itemIndex;
    so->currPos.lastItem = MaxIndexTuplesPerPage - 1;
    so->currPos.itemIndex = MaxIndexTuplesPerPage - 1;
  }

  if (so->currPos.buf == so->hashso_bucket_buf || so->currPos.buf == so->hashso_split_bucket_buf)
  {
    so->currPos.prevPage = InvalidBlockNumber;
    so->currPos.nextPage = opaque->hasho_nextblkno;
    LockBuffer(so->currPos.buf, BUFFER_LOCK_UNLOCK);
  }
  else
  {
    so->currPos.prevPage = opaque->hasho_prevblkno;
    so->currPos.nextPage = opaque->hasho_nextblkno;
    _hash_relbuf(rel, so->currPos.buf);
    so->currPos.buf = InvalidBuffer;
  }

  Assert(so->currPos.firstItem <= so->currPos.lastItem);
  return true;
}

   
                                                          
                                                         
   
static int
_hash_load_qualified_items(IndexScanDesc scan, Page page, OffsetNumber offnum, ScanDirection dir)
{
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  IndexTuple itup;
  int itemIndex;
  OffsetNumber maxoff;

  maxoff = PageGetMaxOffsetNumber(page);

  if (ScanDirectionIsForward(dir))
  {
                                         
    itemIndex = 0;

    while (offnum <= maxoff)
    {
      Assert(offnum >= FirstOffsetNumber);
      itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offnum));

         
                                                                        
                                                                     
                                         
         
      if ((so->hashso_buc_populated && !so->hashso_buc_split && (itup->t_info & INDEX_MOVED_BY_SPLIT_MASK)) || (scan->ignore_killed_tuples && (ItemIdIsDead(PageGetItemId(page, offnum)))))
      {
        offnum = OffsetNumberNext(offnum);                   
        continue;
      }

      if (so->hashso_sk_hash == _hash_get_indextuple_hashkey(itup) && _hash_checkqual(scan, itup))
      {
                                                
        _hash_saveitem(so, itemIndex, offnum, itup);
        itemIndex++;
      }
      else
      {
           
                                                                      
                 
           
        break;
      }

      offnum = OffsetNumberNext(offnum);
    }

    Assert(itemIndex <= MaxIndexTuplesPerPage);
    return itemIndex;
  }
  else
  {
                                          
    itemIndex = MaxIndexTuplesPerPage;

    while (offnum >= FirstOffsetNumber)
    {
      Assert(offnum <= maxoff);
      itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offnum));

         
                                                                        
                                                                     
                                         
         
      if ((so->hashso_buc_populated && !so->hashso_buc_split && (itup->t_info & INDEX_MOVED_BY_SPLIT_MASK)) || (scan->ignore_killed_tuples && (ItemIdIsDead(PageGetItemId(page, offnum)))))
      {
        offnum = OffsetNumberPrev(offnum);                
        continue;
      }

      if (so->hashso_sk_hash == _hash_get_indextuple_hashkey(itup) && _hash_checkqual(scan, itup))
      {
        itemIndex--;
                                                
        _hash_saveitem(so, itemIndex, offnum, itup);
      }
      else
      {
           
                                                                      
                 
           
        break;
      }

      offnum = OffsetNumberPrev(offnum);
    }

    Assert(itemIndex >= 0);
    return itemIndex;
  }
}

                                                          
static inline void
_hash_saveitem(HashScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup)
{
  HashScanPosItem *currItem = &so->currPos.items[itemIndex];

  currItem->heapTid = itup->t_tid;
  currItem->indexOffset = offnum;
}
