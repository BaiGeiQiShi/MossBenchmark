                                                                            
   
          
                                                                     
   
                                                                         
                                                                        
   
   
                  
                                    
   
         
                                                            
   
                                                                            
   

#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "catalog/index.h"
#include "commands/progress.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "optimizer/plancat.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/rel.h"
#include "miscadmin.h"

                                                  
typedef struct
{
  HSpool *spool;                                    
  double indtuples;                                   
  Relation heapRel;                               
} HashBuildState;

static void
hashbuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state);

   
                                                                              
                  
   
Datum
hashhandler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = HTMaxStrategyNumber;
  amroutine->amsupport = HASHNProcs;
  amroutine->amcanorder = false;
  amroutine->amcanorderbyop = false;
  amroutine->amcanbackward = true;
  amroutine->amcanunique = false;
  amroutine->amcanmulticol = false;
  amroutine->amoptionalkey = false;
  amroutine->amsearcharray = false;
  amroutine->amsearchnulls = false;
  amroutine->amstorage = false;
  amroutine->amclusterable = false;
  amroutine->ampredlocks = true;
  amroutine->amcanparallel = false;
  amroutine->amcaninclude = false;
  amroutine->amkeytype = INT4OID;

  amroutine->ambuild = hashbuild;
  amroutine->ambuildempty = hashbuildempty;
  amroutine->aminsert = hashinsert;
  amroutine->ambulkdelete = hashbulkdelete;
  amroutine->amvacuumcleanup = hashvacuumcleanup;
  amroutine->amcanreturn = NULL;
  amroutine->amcostestimate = hashcostestimate;
  amroutine->amoptions = hashoptions;
  amroutine->amproperty = NULL;
  amroutine->ambuildphasename = NULL;
  amroutine->amvalidate = hashvalidate;
  amroutine->ambeginscan = hashbeginscan;
  amroutine->amrescan = hashrescan;
  amroutine->amgettuple = hashgettuple;
  amroutine->amgetbitmap = hashgetbitmap;
  amroutine->amendscan = hashendscan;
  amroutine->ammarkpos = NULL;
  amroutine->amrestrpos = NULL;
  amroutine->amestimateparallelscan = NULL;
  amroutine->aminitparallelscan = NULL;
  amroutine->amparallelrescan = NULL;

  PG_RETURN_POINTER(amroutine);
}

   
                                          
   
IndexBuildResult *
hashbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
  IndexBuildResult *result;
  BlockNumber relpages;
  double reltuples;
  double allvisfrac;
  uint32 num_buckets;
  long sort_threshold;
  HashBuildState buildstate;

     
                                                                           
                                               
     
  if (RelationGetNumberOfBlocks(index) != 0)
  {
    elog(ERROR, "index \"%s\" already contains data", RelationGetRelationName(index));
  }

                                                                  
  estimate_rel_size(heap, NULL, &relpages, &reltuples, &allvisfrac);

                                                                   
  num_buckets = _hash_init(index, reltuples, MAIN_FORKNUM);

     
                                                                     
                                                                             
                                                                           
                                                                            
                                                                          
                                                                        
                                                                       
                                                                        
                                                                             
                                                                            
                                                                         
                                                                          
     
                                                                             
                                                                           
                                          
     
  sort_threshold = (maintenance_work_mem * 1024L) / BLCKSZ;
  if (index->rd_rel->relpersistence != RELPERSISTENCE_TEMP)
  {
    sort_threshold = Min(sort_threshold, NBuffers);
  }
  else
  {
    sort_threshold = Min(sort_threshold, NLocBuffer);
  }

  if (num_buckets >= (uint32)sort_threshold)
  {
    buildstate.spool = _h_spoolinit(heap, index, num_buckets);
  }
  else
  {
    buildstate.spool = NULL;
  }

                                  
  buildstate.indtuples = 0;
  buildstate.heapRel = heap;

                        
  reltuples = table_index_build_scan(heap, index, indexInfo, true, true, hashbuildCallback, (void *)&buildstate, NULL);
  pgstat_progress_update_param(PROGRESS_CREATEIDX_TUPLES_TOTAL, buildstate.indtuples);

  if (buildstate.spool)
  {
                                                        
    _h_indexbuild(buildstate.spool, buildstate.heapRel);
    _h_spooldestroy(buildstate.spool);
  }

     
                       
     
  result = (IndexBuildResult *)palloc(sizeof(IndexBuildResult));

  result->heap_tuples = reltuples;
  result->index_tuples = buildstate.indtuples;

  return result;
}

   
                                                                            
   
void
hashbuildempty(Relation index)
{
  _hash_init(index, 0, INIT_FORKNUM);
}

   
                                                 
   
static void
hashbuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{
  HashBuildState *buildstate = (HashBuildState *)state;
  Datum index_values[1];
  bool index_isnull[1];
  IndexTuple itup;

                                                                      
  if (!_hash_convert_tuple(index, values, isnull, index_values, index_isnull))
  {
    return;
  }

                                                                         
  if (buildstate->spool)
  {
    _h_spool(buildstate->spool, &htup->t_self, index_values, index_isnull);
  }
  else
  {
                                                            
    itup = index_form_tuple(RelationGetDescr(index), index_values, index_isnull);
    itup->t_tid = htup->t_self;
    _hash_doinsert(index, itup, buildstate->heapRel);
    pfree(itup);
  }

  buildstate->indtuples += 1;
}

   
                                                            
   
                                                                     
                                                                      
   
bool
hashinsert(Relation rel, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  Datum index_values[1];
  bool index_isnull[1];
  IndexTuple itup;

                                                                      
  if (!_hash_convert_tuple(rel, values, isnull, index_values, index_isnull))
  {
    return false;
  }

                                                          
  itup = index_form_tuple(RelationGetDescr(rel), index_values, index_isnull);
  itup->t_tid = *ht_ctid;

  _hash_doinsert(rel, itup, heapRel);

  pfree(itup);

  return false;
}

   
                                                     
   
bool
hashgettuple(IndexScanDesc scan, ScanDirection dir)
{
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  bool res;

                                                                       
  scan->xs_recheck = true;

     
                                                                           
                                                                             
                                     
     
  if (!HashScanPosIsValid(so->currPos))
  {
    res = _hash_first(scan, dir);
  }
  else
  {
       
                                                                    
       
    if (scan->kill_prior_tuple)
    {
         
                                                                         
                                                                        
                                                                        
                                                                       
                                                                       
                  
         
      if (so->killedItems == NULL)
      {
        so->killedItems = (int *)palloc(MaxIndexTuplesPerPage * sizeof(int));
      }

      if (so->numKilled < MaxIndexTuplesPerPage)
      {
        so->killedItems[so->numKilled++] = so->currPos.itemIndex;
      }
    }

       
                              
       
    res = _hash_next(scan, dir);
  }

  return res;
}

   
                                             
   
int64
hashgetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  bool res;
  int64 ntids = 0;
  HashScanPosItem *currItem;

  res = _hash_first(scan, ForwardScanDirection);

  while (res)
  {
    currItem = &so->currPos.items[so->currPos.itemIndex];

       
                                                                      
                                                                        
                                                                   
       
    tbm_add_tuples(tbm, &(currItem->heapTid), 1, true);
    ntids++;

    res = _hash_next(scan, ForwardScanDirection);
  }

  return ntids;
}

   
                                                   
   
IndexScanDesc
hashbeginscan(Relation rel, int nkeys, int norderbys)
{
  IndexScanDesc scan;
  HashScanOpaque so;

                                     
  Assert(norderbys == 0);

  scan = RelationGetIndexScan(rel, nkeys, norderbys);

  so = (HashScanOpaque)palloc(sizeof(HashScanOpaqueData));
  HashScanPosInvalidate(so->currPos);
  so->hashso_bucket_buf = InvalidBuffer;
  so->hashso_split_bucket_buf = InvalidBuffer;

  so->hashso_buc_populated = false;
  so->hashso_buc_split = false;

  so->killedItems = NULL;
  so->numKilled = 0;

  scan->opaque = so;

  return scan;
}

   
                                            
   
void
hashrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  Relation rel = scan->indexRelation;

  if (HashScanPosIsValid(so->currPos))
  {
                                                                 
    if (so->numKilled > 0)
    {
      _hash_kill_items(scan);
    }
  }

  _hash_dropscanbuf(rel, so);

                                                               
  HashScanPosInvalidate(so->currPos);

                                              
  if (scankey && scan->numberOfKeys > 0)
  {
    memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
  }

  so->hashso_buc_populated = false;
  so->hashso_buc_split = false;
}

   
                                      
   
void
hashendscan(IndexScanDesc scan)
{
  HashScanOpaque so = (HashScanOpaque)scan->opaque;
  Relation rel = scan->indexRelation;

  if (HashScanPosIsValid(so->currPos))
  {
                                                                 
    if (so->numKilled > 0)
    {
      _hash_kill_items(scan);
    }
  }

  _hash_dropscanbuf(rel, so);

  if (so->killedItems != NULL)
  {
    pfree(so->killedItems);
  }
  pfree(so);
  scan->opaque = NULL;
}

   
                                                                        
                                                                           
                                                                              
   
                                                                          
           
   
                                                                              
   
IndexBulkDeleteResult *
hashbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  Relation rel = info->index;
  double tuples_removed;
  double num_index_tuples;
  double orig_ntuples;
  Bucket orig_maxbucket;
  Bucket cur_maxbucket;
  Bucket cur_bucket;
  Buffer metabuf = InvalidBuffer;
  HashMetaPage metap;
  HashMetaPage cachedmetap;

  tuples_removed = 0;
  num_index_tuples = 0;

     
                                                                          
                                                                          
                                                                           
                          
     
  cachedmetap = _hash_getcachedmetap(rel, &metabuf, false);
  Assert(cachedmetap != NULL);

  orig_maxbucket = cachedmetap->hashm_maxbucket;
  orig_ntuples = cachedmetap->hashm_ntuples;

                                           
  cur_bucket = 0;
  cur_maxbucket = orig_maxbucket;

loop_top:
  while (cur_bucket <= cur_maxbucket)
  {
    BlockNumber bucket_blkno;
    BlockNumber blkno;
    Buffer bucket_buf;
    Buffer buf;
    HashPageOpaque bucket_opaque;
    Page page;
    bool split_cleanup = false;

                                            
    bucket_blkno = BUCKET_TO_BLKNO(cachedmetap, cur_bucket);

    blkno = bucket_blkno;

       
                                                                           
                                                              
       
    buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);
    LockBufferForCleanup(buf);
    _hash_checkpage(rel, buf, LH_BUCKET_PAGE);

    page = BufferGetPage(buf);
    bucket_opaque = (HashPageOpaque)PageGetSpecialPointer(page);

       
                                                                           
                                                                        
                                                                         
       
    if (!H_BUCKET_BEING_SPLIT(bucket_opaque) && H_NEEDS_SPLIT_CLEANUP(bucket_opaque))
    {
      split_cleanup = true;

         
                                                                        
                                                                   
                                                                         
                                                                        
                                                                        
                                                                     
                                          
         
      Assert(bucket_opaque->hasho_prevblkno != InvalidBlockNumber);
      if (bucket_opaque->hasho_prevblkno > cachedmetap->hashm_maxbucket)
      {
        cachedmetap = _hash_getcachedmetap(rel, &metabuf, true);
        Assert(cachedmetap != NULL);
      }
    }

    bucket_buf = buf;

    hashbucketcleanup(rel, cur_bucket, bucket_buf, blkno, info->strategy, cachedmetap->hashm_maxbucket, cachedmetap->hashm_highmask, cachedmetap->hashm_lowmask, &tuples_removed, &num_index_tuples, split_cleanup, callback, callback_state);

    _hash_dropbuf(rel, bucket_buf);

                                
    cur_bucket++;
  }

  if (BufferIsInvalid(metabuf))
  {
    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_NOLOCK, LH_META_PAGE);
  }

                                                                
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);
  metap = HashPageGetMeta(BufferGetPage(metabuf));

  if (cur_maxbucket != metap->hashm_maxbucket)
  {
                                                                   
    LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
    cachedmetap = _hash_getcachedmetap(rel, &metabuf, true);
    Assert(cachedmetap != NULL);
    cur_maxbucket = cachedmetap->hashm_maxbucket;
    goto loop_top;
  }

                                                                 
  START_CRIT_SECTION();

  if (orig_maxbucket == metap->hashm_maxbucket && orig_ntuples == metap->hashm_ntuples)
  {
       
                                                                     
                                    
       
    metap->hashm_ntuples = num_index_tuples;
  }
  else
  {
       
                                                               
                                                                           
                                                                          
                                                            
       
    if (metap->hashm_ntuples > tuples_removed)
    {
      metap->hashm_ntuples -= tuples_removed;
    }
    else
    {
      metap->hashm_ntuples = 0;
    }
    num_index_tuples = metap->hashm_ntuples;
  }

  MarkBufferDirty(metabuf);

                  
  if (RelationNeedsWAL(rel))
  {
    xl_hash_update_meta_page xlrec;
    XLogRecPtr recptr;

    xlrec.ntuples = metap->hashm_ntuples;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHashUpdateMetaPage);

    XLogRegisterBuffer(0, metabuf, REGBUF_STANDARD);

    recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_UPDATE_META_PAGE);
    PageSetLSN(BufferGetPage(metabuf), recptr);
  }

  END_CRIT_SECTION();

  _hash_relbuf(rel, metabuf);

                         
  if (stats == NULL)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
  }
  stats->estimated_count = false;
  stats->num_index_tuples = num_index_tuples;
  stats->tuples_removed += tuples_removed;
                                                

  return stats;
}

   
                        
   
                                                                              
   
IndexBulkDeleteResult *
hashvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
  Relation rel = info->index;
  BlockNumber num_pages;

                                                                         
                                                   
  if (stats == NULL)
  {
    return NULL;
  }

                         
  num_pages = RelationGetNumberOfBlocks(rel);
  stats->num_pages = num_pages;

  return stats;
}

   
                                                                       
   
                                                                            
                                                                            
                                                                          
                                                                  
   
                                                                            
                                                                            
                                                                            
                                                                           
                                                                             
                                                                              
                                                                        
                                                                                
                                                                         
   
                                                                              
                    
   
void
hashbucketcleanup(Relation rel, Bucket cur_bucket, Buffer bucket_buf, BlockNumber bucket_blkno, BufferAccessStrategy bstrategy, uint32 maxbucket, uint32 highmask, uint32 lowmask, double *tuples_removed, double *num_index_tuples, bool split_cleanup, IndexBulkDeleteCallback callback, void *callback_state)
{
  BlockNumber blkno;
  Buffer buf;
  Bucket new_bucket PG_USED_FOR_ASSERTS_ONLY = InvalidBucket;
  bool bucket_dirty = false;

  blkno = bucket_blkno;
  buf = bucket_buf;

  if (split_cleanup)
  {
    new_bucket = _hash_get_newbucket_from_oldbucket(rel, cur_bucket, lowmask, maxbucket);
  }

                                
  for (;;)
  {
    HashPageOpaque opaque;
    OffsetNumber offno;
    OffsetNumber maxoffno;
    Buffer next_buf;
    Page page;
    OffsetNumber deletable[MaxOffsetNumber];
    int ndeletable = 0;
    bool retain_pin = false;
    bool clear_dead_marking = false;

    vacuum_delay_point();

    page = BufferGetPage(buf);
    opaque = (HashPageOpaque)PageGetSpecialPointer(page);

                                 
    maxoffno = PageGetMaxOffsetNumber(page);
    for (offno = FirstOffsetNumber; offno <= maxoffno; offno = OffsetNumberNext(offno))
    {
      ItemPointer htup;
      IndexTuple itup;
      Bucket bucket;
      bool kill_tuple = false;

      itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offno));
      htup = &(itup->t_tid);

         
                                                                        
                                                                        
         
      if (callback && callback(htup, callback_state))
      {
        kill_tuple = true;
        if (tuples_removed)
        {
          *tuples_removed += 1;
        }
      }
      else if (split_cleanup)
      {
                                                        
        bucket = _hash_hashkey2bucket(_hash_get_indextuple_hashkey(itup), maxbucket, highmask, lowmask);
                                        
        if (bucket != cur_bucket)
        {
             
                                                                    
                                                                 
                                                                   
                                            
             
          Assert(bucket == new_bucket);
          kill_tuple = true;
        }
      }

      if (kill_tuple)
      {
                                        
        deletable[ndeletable++] = offno;
      }
      else
      {
                                           
        if (num_index_tuples)
        {
          *num_index_tuples += 1;
        }
      }
    }

                                                                       
    if (blkno == bucket_blkno)
    {
      retain_pin = true;
    }
    else
    {
      retain_pin = false;
    }

    blkno = opaque->hasho_nextblkno;

       
                                                                       
       
    if (ndeletable > 0)
    {
                                                      
      START_CRIT_SECTION();

      PageIndexMultiDelete(page, deletable, ndeletable);
      bucket_dirty = true;

         
                                                                         
                                                    
                                       
         
      if (tuples_removed && *tuples_removed > 0 && H_HAS_DEAD_TUPLES(opaque))
      {
        opaque->hasho_flag &= ~LH_PAGE_HAS_DEAD_TUPLES;
        clear_dead_marking = true;
      }

      MarkBufferDirty(buf);

                      
      if (RelationNeedsWAL(rel))
      {
        xl_hash_delete xlrec;
        XLogRecPtr recptr;

        xlrec.clear_dead_marking = clear_dead_marking;
        xlrec.is_primary_bucket_page = (buf == bucket_buf) ? true : false;

        XLogBeginInsert();
        XLogRegisterData((char *)&xlrec, SizeOfHashDelete);

           
                                                                      
                                                       
           
        if (!xlrec.is_primary_bucket_page)
        {
          XLogRegisterBuffer(0, bucket_buf, REGBUF_STANDARD | REGBUF_NO_IMAGE);
        }

        XLogRegisterBuffer(1, buf, REGBUF_STANDARD);
        XLogRegisterBufData(1, (char *)deletable, ndeletable * sizeof(OffsetNumber));

        recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_DELETE);
        PageSetLSN(BufferGetPage(buf), recptr);
      }

      END_CRIT_SECTION();
    }

                                                      
    if (!BlockNumberIsValid(blkno))
    {
      break;
    }

    next_buf = _hash_getbuf_with_strategy(rel, blkno, HASH_WRITE, LH_OVERFLOW_PAGE, bstrategy);

       
                                                                          
            
       
    if (retain_pin)
    {
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    }
    else
    {
      _hash_relbuf(rel, buf);
    }

    buf = next_buf;
  }

     
                                                                            
                                                                          
                          
     
  if (buf != bucket_buf)
  {
    _hash_relbuf(rel, buf);
    LockBuffer(bucket_buf, BUFFER_LOCK_EXCLUSIVE);
  }

     
                                                                           
                                                                            
                                                                           
                      
     
  if (split_cleanup)
  {
    HashPageOpaque bucket_opaque;
    Page page;

    page = BufferGetPage(bucket_buf);
    bucket_opaque = (HashPageOpaque)PageGetSpecialPointer(page);

                                                    
    START_CRIT_SECTION();

    bucket_opaque->hasho_flag &= ~LH_BUCKET_NEEDS_SPLIT_CLEANUP;
    MarkBufferDirty(bucket_buf);

                    
    if (RelationNeedsWAL(rel))
    {
      XLogRecPtr recptr;

      XLogBeginInsert();
      XLogRegisterBuffer(0, bucket_buf, REGBUF_STANDARD);

      recptr = XLogInsert(RM_HASH_ID, XLOG_HASH_SPLIT_CLEANUP);
      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();
  }

     
                                                                            
                                                                     
                                                               
     
  if (bucket_dirty && IsBufferCleanupOK(bucket_buf))
  {
    _hash_squeezebucket(rel, cur_bucket, bucket_blkno, bucket_buf, bstrategy);
  }
  else
  {
    LockBuffer(bucket_buf, BUFFER_LOCK_UNLOCK);
  }
}
