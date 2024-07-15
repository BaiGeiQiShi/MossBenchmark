                                                                            
   
              
                                                    
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "storage/buf_internals.h"

#define CALC_NEW_BUCKET(old_bucket, lowmask) old_bucket | (lowmask + 1)

   
                                                                        
   
bool
_hash_checkqual(IndexScanDesc scan, IndexTuple itup)
{
     
                                                                          
                                                                          
                                                                           
                                            
     
#ifdef NOT_USED
  TupleDesc tupdesc = RelationGetDescr(scan->indexRelation);
  ScanKey key = scan->keyData;
  int scanKeySize = scan->numberOfKeys;

  while (scanKeySize > 0)
  {
    Datum datum;
    bool isNull;
    Datum test;

    datum = index_getattr(itup, key->sk_attno, tupdesc, &isNull);

                                  
    if (isNull)
    {
      return false;
    }
    if (key->sk_flags & SK_ISNULL)
    {
      return false;
    }

    test = FunctionCall2Coll(&key->sk_func, key->sk_collation, datum, key->sk_argument);

    if (!DatumGetBool(test))
    {
      return false;
    }

    key++;
    scanKeySize--;
  }
#endif

  return true;
}

   
                                                                        
   
                                                                            
                                                                            
   
uint32
_hash_datum2hashkey(Relation rel, Datum key)
{
  FmgrInfo *procinfo;
  Oid collation;

                                                
  procinfo = index_getprocinfo(rel, 1, HASHSTANDARD_PROC);
  collation = rel->rd_indcollation[0];

  return DatumGetUInt32(FunctionCall1Coll(procinfo, collation, key));
}

   
                                                                  
                                                     
   
                                                                           
                          
   
uint32
_hash_datum2hashkey_type(Relation rel, Datum key, Oid keytype)
{
  RegProcedure hash_proc;
  Oid collation;

                                                
  hash_proc = get_opfamily_proc(rel->rd_opfamily[0], keytype, keytype, HASHSTANDARD_PROC);
  if (!RegProcedureIsValid(hash_proc))
  {
    elog(ERROR, "missing support function %d(%u,%u) for index \"%s\"", HASHSTANDARD_PROC, keytype, keytype, RelationGetRelationName(rel));
  }
  collation = rel->rd_indcollation[0];

  return DatumGetUInt32(OidFunctionCall1Coll(hash_proc, collation, key));
}

   
                                                                       
   
Bucket
_hash_hashkey2bucket(uint32 hashkey, uint32 maxbucket, uint32 highmask, uint32 lowmask)
{
  Bucket bucket;

  bucket = hashkey & highmask;
  if (bucket > maxbucket)
  {
    bucket = bucket & lowmask;
  }

  return bucket;
}

   
                                        
   
uint32
_hash_log2(uint32 num)
{
  uint32 i, limit;

  limit = 1;
  for (i = 0; limit < num; limit <<= 1, i++)
    ;
  return i;
}

   
                                                                            
                 
   
uint32
_hash_spareindex(uint32 num_bucket)
{
  uint32 splitpoint_group;
  uint32 splitpoint_phases;

  splitpoint_group = _hash_log2(num_bucket);

  if (splitpoint_group < HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE)
  {
    return splitpoint_group;
  }

                                       
  splitpoint_phases = HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE;

                                                              
  splitpoint_phases += ((splitpoint_group - HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE) << HASH_SPLITPOINT_PHASE_BITS);

                                               
  splitpoint_phases += (((num_bucket - 1) >> (splitpoint_group - (HASH_SPLITPOINT_PHASE_BITS + 1))) & HASH_SPLITPOINT_PHASE_MASK);                        

  return splitpoint_phases;
}

   
                                                                            
                                     
   
uint32
_hash_get_totalbuckets(uint32 splitpoint_phase)
{
  uint32 splitpoint_group;
  uint32 total_buckets;
  uint32 phases_within_splitpoint_group;

  if (splitpoint_phase < HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE)
  {
    return (1 << splitpoint_phase);
  }

                              
  splitpoint_group = HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE;
  splitpoint_group += ((splitpoint_phase - HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE) >> HASH_SPLITPOINT_PHASE_BITS);

                                                   
  total_buckets = (1 << (splitpoint_group - 1));

                                                   
  phases_within_splitpoint_group = (((splitpoint_phase - HASH_SPLITPOINT_GROUPS_WITH_ONE_PHASE) & HASH_SPLITPOINT_PHASE_MASK) + 1);                              
  total_buckets += (((1 << (splitpoint_group - 1)) >> HASH_SPLITPOINT_PHASE_BITS) * phases_within_splitpoint_group);

  return total_buckets;
}

   
                                                                    
   
                                                                         
                                          
   
void
_hash_checkpage(Relation rel, Buffer buf, int flags)
{
  Page page = BufferGetPage(buf);

     
                                                           
                                                                         
                                                                         
                    
     
  if (PageIsNew(page))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains unexpected zero page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
  }

     
                                                          
     
  if (PageGetSpecialSize(page) != MAXALIGN(sizeof(HashPageOpaqueData)))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains corrupted page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
  }

  if (flags)
  {
    HashPageOpaque opaque = (HashPageOpaque)PageGetSpecialPointer(page);

    if ((opaque->hasho_flag & flags) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains corrupted page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
    }
  }

     
                                                                       
     
  if (flags == LH_META_PAGE)
  {
    HashMetaPage metap = HashPageGetMeta(page);

    if (metap->hashm_magic != HASH_MAGIC)
    {
      ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" is not a hash index", RelationGetRelationName(rel))));
    }

    if (metap->hashm_version != HASH_VERSION)
    {
      ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" has wrong hash version", RelationGetRelationName(rel)), errhint("Please REINDEX it.")));
    }
  }
}

bytea *
hashoptions(Datum reloptions, bool validate)
{
  return default_reloptions(reloptions, validate, RELOPT_KIND_HASH);
}

   
                                                                            
   
uint32
_hash_get_indextuple_hashkey(IndexTuple itup)
{
  char *attp;

     
                                                                         
                                                        
     
  attp = (char *)itup + IndexInfoFindDataOffset(itup->t_info);
  return *((uint32 *)attp);
}

   
                                                            
   
                                                                
                                                                       
                                   
   
                                                                             
                                                          
   
                                                                           
                                                                             
                           
   
bool
_hash_convert_tuple(Relation index, Datum *user_values, bool *user_isnull, Datum *index_values, bool *index_isnull)
{
  uint32 hashkey;

     
                                                                           
                                                                            
     
  if (user_isnull[0])
  {
    return false;
  }

  hashkey = _hash_datum2hashkey(index, user_values[0]);
  index_values[0] = UInt32GetDatum(hashkey);
  index_isnull[0] = false;
  return true;
}

   
                                                                    
                                                           
   
                                                                             
                            
   
                                                                             
                                                                       
                                                                           
                                      
   
OffsetNumber
_hash_binsearch(Page page, uint32 hash_value)
{
  OffsetNumber upper;
  OffsetNumber lower;

                                                       
  upper = PageGetMaxOffsetNumber(page) + 1;
  lower = FirstOffsetNumber;

  while (upper > lower)
  {
    OffsetNumber off;
    IndexTuple itup;
    uint32 hashkey;

    off = (upper + lower) / 2;
    Assert(OffsetNumberIsValid(off));

    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, off));
    hashkey = _hash_get_indextuple_hashkey(itup);
    if (hashkey < hash_value)
    {
      lower = off + 1;
    }
    else
    {
      upper = off;
    }
  }

  return lower;
}

   
                        
   
                                                                          
                                                                        
                                                                         
                                                              
   
OffsetNumber
_hash_binsearch_last(Page page, uint32 hash_value)
{
  OffsetNumber upper;
  OffsetNumber lower;

                                                       
  upper = PageGetMaxOffsetNumber(page);
  lower = FirstOffsetNumber - 1;

  while (upper > lower)
  {
    IndexTuple itup;
    OffsetNumber off;
    uint32 hashkey;

    off = (upper + lower + 1) / 2;
    Assert(OffsetNumberIsValid(off));

    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, off));
    hashkey = _hash_get_indextuple_hashkey(itup);
    if (hashkey > hash_value)
    {
      upper = off - 1;
    }
    else
    {
      lower = off;
    }
  }

  return lower;
}

   
                                                                           
                                                     
   
BlockNumber
_hash_get_oldblock_from_newbucket(Relation rel, Bucket new_bucket)
{
  Bucket old_bucket;
  uint32 mask;
  Buffer metabuf;
  HashMetaPage metap;
  BlockNumber blkno;

     
                                                                             
                                                                    
                                                                          
                                                                           
                                                                            
                 
     
  mask = (((uint32)1) << (fls(new_bucket) - 1)) - 1;
  old_bucket = new_bucket & mask;

  metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ, LH_META_PAGE);
  metap = HashPageGetMeta(BufferGetPage(metabuf));

  blkno = BUCKET_TO_BLKNO(metap, old_bucket);

  _hash_relbuf(rel, metabuf);

  return blkno;
}

   
                                                                           
                                                         
   
                                                                              
                                                                             
                                                                            
           
   
BlockNumber
_hash_get_newblock_from_oldbucket(Relation rel, Bucket old_bucket)
{
  Bucket new_bucket;
  Buffer metabuf;
  HashMetaPage metap;
  BlockNumber blkno;

  metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ, LH_META_PAGE);
  metap = HashPageGetMeta(BufferGetPage(metabuf));

  new_bucket = _hash_get_newbucket_from_oldbucket(rel, old_bucket, metap->hashm_lowmask, metap->hashm_maxbucket);
  blkno = BUCKET_TO_BLKNO(metap, new_bucket);

  _hash_relbuf(rel, metabuf);

  return blkno;
}

   
                                                                           
                                                      
   
                                                                           
                                                                            
                                                                        
                                                                         
                                                                               
                                                                        
           
   
Bucket
_hash_get_newbucket_from_oldbucket(Relation rel, Bucket old_bucket, uint32 lowmask, uint32 maxbucket)
{
  Bucket new_bucket;

  new_bucket = CALC_NEW_BUCKET(old_bucket, lowmask);
  if (new_bucket > maxbucket)
  {
    lowmask = lowmask >> 1;
    new_bucket = CALC_NEW_BUCKET(old_bucket, lowmask);
  }

  return new_bucket;
}

   
                                                                          
                        
   
                                                                               
                                                                          
                                 
   
                                                                           
                                                                           
                                          
   
                                                                          
                                                                         
   
                                                                         
           
   
                                                                           
                                                                            
                                                                             
                                                                           
                                                                          
                                                                           
                                                              
   
void
_hash_kill_items(IndexScanDesc scan)
{
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  Relation rel = scan->indexRelation;
  BlockNumber blkno;
  Buffer buf;
  Page page;
  HashPageOpaque opaque;
  OffsetNumber offnum, maxoff;
  int numKilled = so->numKilled;
  int i;
  bool killedsomething = false;
  bool havePin = false;

  Assert(so->numKilled > 0);
  Assert(so->killedItems != NULL);
  Assert(HashScanPosIsValid(so->currPos));

     
                                                                           
            
     
  so->numKilled = 0;

  blkno = so->currPos.currPage;
  if (HashScanPosIsPinned(so->currPos))
  {
       
                                                                    
                           
       
    havePin = true;
    buf = so->currPos.buf;
    LockBuffer(buf, BUFFER_LOCK_SHARE);
  }
  else
  {
    buf = _hash_getbuf(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
  }

  page = BufferGetPage(buf);
  opaque = (HashPageOpaque)PageGetSpecialPointer(page);
  maxoff = PageGetMaxOffsetNumber(page);

  for (i = 0; i < numKilled; i++)
  {
    int itemIndex = so->killedItems[i];
    HashScanPosItem *currItem = &so->currPos.items[itemIndex];

    offnum = currItem->indexOffset;

    Assert(itemIndex >= so->currPos.firstItem && itemIndex <= so->currPos.lastItem);

    while (offnum <= maxoff)
    {
      ItemId iid = PageGetItemId(page, offnum);
      IndexTuple ituple = (IndexTuple)PageGetItem(page, iid);

      if (ItemPointerEquals(&ituple->t_tid, &currItem->heapTid))
      {
                            
        ItemIdMarkDead(iid);
        killedsomething = true;
        break;                               
      }
      offnum = OffsetNumberNext(offnum);
    }
  }

     
                                                                            
                                                      
                                                                  
     
  if (killedsomething)
  {
    opaque->hasho_flag |= LH_PAGE_HAS_DEAD_TUPLES;
    MarkBufferDirtyHint(buf, true);
  }

  if (so->hashso_bucket_buf == so->currPos.buf || havePin)
  {
    LockBuffer(so->currPos.buf, BUFFER_LOCK_UNLOCK);
  }
  else
  {
    _hash_relbuf(rel, buf);
  }
}
