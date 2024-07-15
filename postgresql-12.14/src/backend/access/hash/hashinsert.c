                                                                            
   
                
                                                 
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "miscadmin.h"
#include "utils/rel.h"
#include "storage/lwlock.h"
#include "storage/buf_internals.h"
#include "storage/predicate.h"

static void
_hash_vacuum_one_page(Relation rel, Relation hrel, Buffer metabuf, Buffer buf);

   
                                                                 
   
                                                                       
                                                            
   
void
_hash_doinsert(Relation rel, IndexTuple itup, Relation heapRel)
{
  Buffer buf = InvalidBuffer;
  Buffer bucket_buf;
  Buffer metabuf;
  HashMetaPage metap;
  HashMetaPage usedmetap = NULL;
  Page metapage;
  Page page;
  HashPageOpaque pageopaque;
  Size itemsz;
  bool do_expand;
  uint32 hashkey;
  Bucket bucket;
  OffsetNumber itup_off;

     
                                                                            
     
  hashkey = _hash_get_indextuple_hashkey(itup);

                             
  itemsz = IndexTupleSize(itup);
  itemsz = MAXALIGN(itemsz);                                             
                                                        

restart_insert:

     
                                                                      
                                                                             
                     
     
  metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_NOLOCK, LH_META_PAGE);
  metapage = BufferGetPage(metabuf);

     
                                                                           
                                                                            
                                        
     
                                                                
     
  if (itemsz > HashMaxItemSize(metapage))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds hash maximum %zu", itemsz, HashMaxItemSize(metapage)), errhint("Values larger than a buffer page cannot be indexed.")));
  }

                                                           
  buf = _hash_getbucketbuf_from_hashkey(rel, hashkey, HASH_WRITE, &usedmetap);
  Assert(usedmetap != NULL);

  CheckForSerializableConflictIn(rel, NULL, buf);

                                                                           
  bucket_buf = buf;

  page = BufferGetPage(buf);
  pageopaque = (HashPageOpaque)PageGetSpecialPointer(page);
  bucket = pageopaque->hasho_bucket;

     
                                                                        
                                                                    
                                                                          
                                                                         
                                                                          
                                                                         
                                     
     
  if (H_BUCKET_BEING_SPLIT(pageopaque) && IsBufferCleanupOK(buf))
  {
                                                                         
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);

    _hash_finish_split(rel, metabuf, buf, bucket, usedmetap->hashm_maxbucket, usedmetap->hashm_highmask, usedmetap->hashm_lowmask);

                                                                    
    _hash_dropbuf(rel, buf);
    _hash_dropbuf(rel, metabuf);
    goto restart_insert;
  }

                        
  while (PageGetFreeSpace(page) < itemsz)
  {
    BlockNumber nextblkno;

       
                                                                       
                                                                   
                                                                    
       
    if (H_HAS_DEAD_TUPLES(pageopaque))
    {

      if (IsBufferCleanupOK(buf))
      {
        _hash_vacuum_one_page(rel, heapRel, metabuf, buf);

        if (PageGetFreeSpace(page) >= itemsz)
        {
          break;                                   
        }
      }
    }

       
                                                         
       
    nextblkno = pageopaque->hasho_nextblkno;

    if (BlockNumberIsValid(nextblkno))
    {
         
                                                                      
                                                                    
                                                                        
                                                                         
                                                                     
         
      if (buf != bucket_buf)
      {
        _hash_relbuf(rel, buf);
      }
      else
      {
        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
      }
      buf = _hash_getbuf(rel, nextblkno, HASH_WRITE, LH_OVERFLOW_PAGE);
      page = BufferGetPage(buf);
    }
    else
    {
         
                                                                     
                                                               
         

                                                           
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);

                                        
      buf = _hash_addovflpage(rel, metabuf, buf, (buf == bucket_buf) ? true : false);
      page = BufferGetPage(buf);

                                            
      Assert(PageGetFreeSpace(page) >= itemsz);
    }
    pageopaque = (HashPageOpaque)PageGetSpecialPointer(page);
    Assert((pageopaque->hasho_flag & LH_PAGE_TYPE) == LH_OVERFLOW_PAGE);
    Assert(pageopaque->hasho_bucket == bucket);
  }

     
                                                                        
                                                             
     
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

                                                                  
  START_CRIT_SECTION();

                                                          
  itup_off = _hash_pgaddtup(rel, buf, itemsz, itup);
  MarkBufferDirty(buf);

                           
  metap = HashPageGetMeta(metapage);
  metap->hashm_ntuples += 1;

                                                             
  do_expand = metap->hashm_ntuples > (double)metap->hashm_ffactor * (metap->hashm_maxbucket + 1);

  MarkBufferDirty(metabuf);

                  
  if (RelationNeedsWAL(rel))
  {
    xl_hash_insert xlrec;
    XLogRecPtr recptr;

    xlrec.offnum = itup_off;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHashInsert);

    XLogRegisterBuffer(1, metabuf, REGBUF_STANDARD);

    XLogRegisterBuffer(0, buf, REGBUF_STANDARD);
    XLogRegisterBufData(0, (char *)itup, IndexTupleSize(itup));

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_INSERT);

    PageSetLSN(BufferGetPage(buf), recptr);
    PageSetLSN(BufferGetPage(metabuf), recptr);
  }

  END_CRIT_SECTION();

                                           
  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

     
                                                                        
           
     
  _hash_relbuf(rel, buf);
  if (buf != bucket_buf)
  {
    _hash_dropbuf(rel, bucket_buf);
  }

                                             
  if (do_expand)
  {
    _hash_expandtable(rel, metabuf);
  }

                                            
  _hash_dropbuf(rel, metabuf);
}

   
                                                                      
   
                                                                               
                                                                              
                      
   
                                                                             
                                                                           
                                     
   
OffsetNumber
_hash_pgaddtup(Relation rel, Buffer buf, Size itemsize, IndexTuple itup)
{
  OffsetNumber itup_off;
  Page page;
  uint32 hashkey;

  _hash_checkpage(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
  page = BufferGetPage(buf);

                                                                           
  hashkey = _hash_get_indextuple_hashkey(itup);
  itup_off = _hash_binsearch(page, hashkey);

  if (PageAddItem(page, (Item)itup, itemsize, itup_off, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR, "failed to add index item to \"%s\"", RelationGetRelationName(rel));
  }

  return itup_off;
}

   
                                                                           
                 
   
                                                                        
                     
   
                                                                      
   
void
_hash_pgaddmultitup(Relation rel, Buffer buf, IndexTuple *itups, OffsetNumber *itup_offsets, uint16 nitups)
{
  OffsetNumber itup_off;
  Page page;
  uint32 hashkey;
  int i;

  _hash_checkpage(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
  page = BufferGetPage(buf);

  for (i = 0; i < nitups; i++)
  {
    Size itemsize;

    itemsize = IndexTupleSize(itups[i]);
    itemsize = MAXALIGN(itemsize);

                                                                             
    hashkey = _hash_get_indextuple_hashkey(itups[i]);
    itup_off = _hash_binsearch(page, hashkey);

    itup_offsets[i] = itup_off;

    if (PageAddItem(page, (Item)itups[i], itemsize, itup_off, false, false) == InvalidOffsetNumber)
    {
      elog(ERROR, "failed to add index item to \"%s\"", RelationGetRelationName(rel));
    }
  }
}

   
                                                       
   
                                                                            
                                                                 
   

static void
_hash_vacuum_one_page(Relation rel, Relation hrel, Buffer metabuf, Buffer buf)
{
  OffsetNumber deletable[MaxOffsetNumber];
  int ndeletable = 0;
  OffsetNumber offnum, maxoff;
  Page page = BufferGetPage(buf);
  HashPageOpaque pageopaque;
  HashMetaPage metap;

                                                                 
  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemId = PageGetItemId(page, offnum);

    if (ItemIdIsDead(itemId))
    {
      deletable[ndeletable++] = offnum;
    }
  }

  if (ndeletable > 0)
  {
    TransactionId latestRemovedXid;

    latestRemovedXid = index_compute_xid_horizon_for_tuples(rel, hrel, buf, deletable, ndeletable);

       
                                                                      
       
    LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

                                                    
    START_CRIT_SECTION();

    PageIndexMultiDelete(page, deletable, ndeletable);

       
                                                                      
                                                                           
                                                                         
                                                                           
                                                                      
               
       
    pageopaque = (HashPageOpaque)PageGetSpecialPointer(page);
    pageopaque->hasho_flag &= ~LH_PAGE_HAS_DEAD_TUPLES;

    metap = HashPageGetMeta(BufferGetPage(metabuf));
    metap->hashm_ntuples -= ndeletable;

    MarkBufferDirty(buf);
    MarkBufferDirty(metabuf);

                    
    if (RelationNeedsWAL(rel))
    {
      xl_hash_vacuum_one_page xlrec;
      XLogRecPtr recptr;

      xlrec.latestRemovedXid = latestRemovedXid;
      xlrec.ntuples = ndeletable;

      XLogBeginInsert();
      XLogRegisterBuffer(0, buf, REGBUF_STANDARD);
      XLogRegisterData((char *)&xlrec, SizeOfHashVacuumOnePage);

         
                                                                      
                                                                     
                         
         
      XLogRegisterData((char *)deletable, ndeletable * sizeof(OffsetNumber));

      XLogRegisterBuffer(1, metabuf, REGBUF_STANDARD);

      recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_VACUUM_ONE_PAGE);

      PageSetLSN(BufferGetPage(buf), recptr);
      PageSetLSN(BufferGetPage(metabuf), recptr);
    }

    END_CRIT_SECTION();

       
                                                                      
              
       
    LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
  }
}
