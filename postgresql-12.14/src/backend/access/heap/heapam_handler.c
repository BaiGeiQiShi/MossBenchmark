                                                                            
   
                    
                                   
   
                                                                         
                                                                        
   
   
                  
                                              
   
   
         
                                                                          
                          
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "miscadmin.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/rewriteheap.h"
#include "access/tableam.h"
#include "access/tsmapi.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "commands/progress.h"
#include "executor/executor.h"
#include "optimizer/plancat.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/rel.h"

static void
reform_and_rewrite_tuple(HeapTuple tuple, Relation OldHeap, Relation NewHeap, Datum *values, bool *isnull, RewriteState rwstate);

static bool
SampleHeapTupleVisible(TableScanDesc scan, Buffer buffer, HeapTuple tuple, OffsetNumber tupoffset);

static BlockNumber
heapam_scan_get_blocks_done(HeapScanDesc hscan);

static const TableAmRoutine heapam_methods;

                                                                            
                                      
                                                                            
   

static const TupleTableSlotOps *
heapam_slot_callbacks(Relation relation)
{
  return &TTSOpsBufferHeapTuple;
}

                                                                            
                                    
                                                                            
   

static IndexFetchTableData *
heapam_index_fetch_begin(Relation rel)
{
  IndexFetchHeapData *hscan = palloc0(sizeof(IndexFetchHeapData));

  hscan->xs_base.rel = rel;
  hscan->xs_cbuf = InvalidBuffer;

  return &hscan->xs_base;
}

static void
heapam_index_fetch_reset(IndexFetchTableData *scan)
{
  IndexFetchHeapData *hscan = (IndexFetchHeapData *)scan;

  if (BufferIsValid(hscan->xs_cbuf))
  {
    ReleaseBuffer(hscan->xs_cbuf);
    hscan->xs_cbuf = InvalidBuffer;
  }
}

static void
heapam_index_fetch_end(IndexFetchTableData *scan)
{
  IndexFetchHeapData *hscan = (IndexFetchHeapData *)scan;

  heapam_index_fetch_reset(scan);

  pfree(hscan);
}

static bool
heapam_index_fetch_tuple(struct IndexFetchTableData *scan, ItemPointer tid, Snapshot snapshot, TupleTableSlot *slot, bool *call_again, bool *all_dead)
{
  IndexFetchHeapData *hscan = (IndexFetchHeapData *)scan;
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;
  bool got_heap_tuple;

  Assert(TTS_IS_BUFFERTUPLE(slot));

                                                                         
  if (!*call_again)
  {
                                                              
    Buffer prev_buf = hscan->xs_cbuf;

    hscan->xs_cbuf = ReleaseAndReadBuffer(hscan->xs_cbuf, hscan->xs_base.rel, ItemPointerGetBlockNumber(tid));

       
                                                               
       
    if (prev_buf != hscan->xs_cbuf)
    {
      heap_page_prune_opt(hscan->xs_base.rel, hscan->xs_cbuf);
    }
  }

                                                                    
  LockBuffer(hscan->xs_cbuf, BUFFER_LOCK_SHARE);
  got_heap_tuple = heap_hot_search_buffer(tid, hscan->xs_base.rel, hscan->xs_cbuf, snapshot, &bslot->base.tupdata, all_dead, !*call_again);
  bslot->base.tupdata.t_self = *tid;
  LockBuffer(hscan->xs_cbuf, BUFFER_LOCK_UNLOCK);

  if (got_heap_tuple)
  {
       
                                                                       
                         
       
    *call_again = !IsMVCCSnapshot(snapshot);

    slot->tts_tableOid = RelationGetRelid(scan->rel);
    ExecStoreBufferHeapTuple(&bslot->base.tupdata, slot, hscan->xs_cbuf);
  }
  else
  {
                                                 
    *call_again = false;
  }

  return got_heap_tuple;
}

                                                                            
                                                                           
                                                                            
   

static bool
heapam_fetch_row_version(Relation relation, ItemPointer tid, Snapshot snapshot, TupleTableSlot *slot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;
  Buffer buffer;

  Assert(TTS_IS_BUFFERTUPLE(slot));

  bslot->base.tupdata.t_self = *tid;
  if (heap_fetch(relation, snapshot, &bslot->base.tupdata, &buffer))
  {
                                                  
    ExecStorePinnedBufferHeapTuple(&bslot->base.tupdata, slot, buffer);
    slot->tts_tableOid = RelationGetRelid(relation);

    return true;
  }

  return false;
}

static bool
heapam_tuple_tid_valid(TableScanDesc scan, ItemPointer tid)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;

  return ItemPointerIsValid(tid) && ItemPointerGetBlockNumber(tid) < hscan->rs_nblocks;
}

static bool
heapam_tuple_satisfies_snapshot(Relation rel, TupleTableSlot *slot, Snapshot snapshot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;
  bool res;

  Assert(TTS_IS_BUFFERTUPLE(slot));
  Assert(BufferIsValid(bslot->buffer));

     
                                                                       
                                                 
     
  LockBuffer(bslot->buffer, BUFFER_LOCK_SHARE);
  res = HeapTupleSatisfiesVisibility(bslot->base.tuple, snapshot, bslot->buffer);
  LockBuffer(bslot->buffer, BUFFER_LOCK_UNLOCK);

  return res;
}

                                                                                
                                                                
                                                                                
   

static void
heapam_tuple_insert(Relation relation, TupleTableSlot *slot, CommandId cid, int options, BulkInsertState bistate)
{
  bool shouldFree = true;
  HeapTuple tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);

                                       
  slot->tts_tableOid = RelationGetRelid(relation);
  tuple->t_tableOid = slot->tts_tableOid;

                                                                 
  heap_insert(relation, tuple, cid, options, bistate);
  ItemPointerCopy(&tuple->t_self, &slot->tts_tid);

  if (shouldFree)
  {
    pfree(tuple);
  }
}

static void
heapam_tuple_insert_speculative(Relation relation, TupleTableSlot *slot, CommandId cid, int options, BulkInsertState bistate, uint32 specToken)
{
  bool shouldFree = true;
  HeapTuple tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);

                                       
  slot->tts_tableOid = RelationGetRelid(relation);
  tuple->t_tableOid = slot->tts_tableOid;

  HeapTupleHeaderSetSpeculativeToken(tuple->t_data, specToken);
  options |= HEAP_INSERT_SPECULATIVE;

                                                                 
  heap_insert(relation, tuple, cid, options, bistate);
  ItemPointerCopy(&tuple->t_self, &slot->tts_tid);

  if (shouldFree)
  {
    pfree(tuple);
  }
}

static void
heapam_tuple_complete_speculative(Relation relation, TupleTableSlot *slot, uint32 specToken, bool succeeded)
{
  bool shouldFree = true;
  HeapTuple tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);

                                            
  if (succeeded)
  {
    heap_finish_speculative(relation, &slot->tts_tid);
  }
  else
  {
    heap_abort_speculative(relation, &slot->tts_tid);
  }

  if (shouldFree)
  {
    pfree(tuple);
  }
}

static TM_Result
heapam_tuple_delete(Relation relation, ItemPointer tid, CommandId cid, Snapshot snapshot, Snapshot crosscheck, bool wait, TM_FailureData *tmfd, bool changingPart)
{
     
                                                                          
                                                                         
                                                 
     
  return heap_delete(relation, tid, cid, crosscheck, wait, tmfd, changingPart);
}

static TM_Result
heapam_tuple_update(Relation relation, ItemPointer otid, TupleTableSlot *slot, CommandId cid, Snapshot snapshot, Snapshot crosscheck, bool wait, TM_FailureData *tmfd, LockTupleMode *lockmode, bool *update_indexes)
{
  bool shouldFree = true;
  HeapTuple tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);
  TM_Result result;

                                       
  slot->tts_tableOid = RelationGetRelid(relation);
  tuple->t_tableOid = slot->tts_tableOid;

  result = heap_update(relation, otid, tuple, cid, crosscheck, wait, tmfd, lockmode);
  ItemPointerCopy(&tuple->t_self, &slot->tts_tid);

     
                                                               
     
                                                                          
                   
     
                                                                
     
  *update_indexes = result == TM_Ok && !HeapTupleIsHeapOnly(tuple);

  if (shouldFree)
  {
    pfree(tuple);
  }

  return result;
}

static TM_Result
heapam_tuple_lock(Relation relation, ItemPointer tid, Snapshot snapshot, TupleTableSlot *slot, CommandId cid, LockTupleMode mode, LockWaitPolicy wait_policy, uint8 flags, TM_FailureData *tmfd)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;
  TM_Result result;
  Buffer buffer;
  HeapTuple tuple = &bslot->base.tupdata;
  bool follow_updates;

  follow_updates = (flags & TUPLE_LOCK_FLAG_LOCK_UPDATE_IN_PROGRESS) != 0;
  tmfd->traversed = false;

  Assert(TTS_IS_BUFFERTUPLE(slot));

tuple_lock_retry:
  tuple->t_self = *tid;
  result = heap_lock_tuple(relation, tuple, cid, mode, wait_policy, follow_updates, &buffer, tmfd);

  if (result == TM_Updated && (flags & TUPLE_LOCK_FLAG_FIND_LAST_VERSION))
  {
    ReleaseBuffer(buffer);
                                                           
    Assert(!HeapTupleHeaderIsSpeculative(tuple->t_data));

    if (!ItemPointerEquals(&tmfd->ctid, &tuple->t_self))
    {
      SnapshotData SnapshotDirty;
      TransactionId priorXmax;

                                                          
      *tid = tmfd->ctid;
                                                           
      priorXmax = tmfd->xmax;

                                                                    
      tmfd->traversed = true;

         
                            
         
                                                       
         
      InitDirtySnapshot(SnapshotDirty);
      for (;;)
      {
        if (ItemPointerIndicatesMovedPartitions(tid))
        {
          ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("tuple to be locked was already moved to another partition due to concurrent update")));
        }

        tuple->t_self = *tid;
        if (heap_fetch_extended(relation, &SnapshotDirty, tuple, &buffer, true))
        {
             
                                                                    
                                                                    
                                                                     
                                                                     
                                                               
                                                                  
                                                                
                                                                
             
          if (!TransactionIdEquals(HeapTupleHeaderGetXmin(tuple->t_data), priorXmax))
          {
            ReleaseBuffer(buffer);
            return TM_Deleted;
          }

                                                     
          if (TransactionIdIsValid(SnapshotDirty.xmin))
          {
            ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("t_xmin %u is uncommitted in tuple (%u,%u) to be updated in table \"%s\"", SnapshotDirty.xmin, ItemPointerGetBlockNumber(&tuple->t_self), ItemPointerGetOffsetNumber(&tuple->t_self), RelationGetRelationName(relation))));
          }

             
                                                                    
                                                               
             
          if (TransactionIdIsValid(SnapshotDirty.xmax))
          {
            ReleaseBuffer(buffer);
            switch (wait_policy)
            {
            case LockWaitBlock:
              XactLockTableWait(SnapshotDirty.xmax, relation, &tuple->t_self, XLTW_FetchUpdated);
              break;
            case LockWaitSkip:
              if (!ConditionalXactLockTableWait(SnapshotDirty.xmax))
              {
                                             
                return TM_WouldBlock;
              }
              break;
            case LockWaitError:
              if (!ConditionalXactLockTableWait(SnapshotDirty.xmax))
              {
                ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on row in relation \"%s\"", RelationGetRelationName(relation))));
              }
              break;
            }
            continue;                                     
          }

             
                                                                   
                                                                  
                                                                   
                                                                  
                                                                    
                                                     
                                                                   
                                                                    
                                                                 
                                                                     
             
          if (TransactionIdIsCurrentTransactionId(priorXmax) && HeapTupleHeaderGetCmin(tuple->t_data) >= cid)
          {
            tmfd->xmax = priorXmax;

               
                                                                 
                      
               
            tmfd->cmax = HeapTupleHeaderGetCmin(tuple->t_data);
            ReleaseBuffer(buffer);
            return TM_SelfModified;
          }

             
                                                            
             
          ReleaseBuffer(buffer);
          goto tuple_lock_retry;
        }

           
                                                                 
                                                                    
                    
           
        if (tuple->t_data == NULL)
        {
          Assert(!BufferIsValid(buffer));
          return TM_Deleted;
        }

           
                                                                     
           
        if (!TransactionIdEquals(HeapTupleHeaderGetXmin(tuple->t_data), priorXmax))
        {
          ReleaseBuffer(buffer);
          return TM_Deleted;
        }

           
                                                          
                                                                       
                                                                      
                                                                 
                                                                    
                                                                 
                                                                    
                          
           
                                                                  
                                                                  
                                                            
           
        if (ItemPointerEquals(&tuple->t_self, &tuple->t_data->t_ctid))
        {
                                           
          ReleaseBuffer(buffer);
          return TM_Deleted;
        }

                                                 
        *tid = tuple->t_data->t_ctid;
                                                             
        priorXmax = HeapTupleHeaderGetUpdateXid(tuple->t_data);
        ReleaseBuffer(buffer);
                                              
      }
    }
    else
    {
                                         
      return TM_Deleted;
    }
  }

  slot->tts_tableOid = RelationGetRelid(relation);
  tuple->t_tableOid = slot->tts_tableOid;

                                                
  ExecStorePinnedBufferHeapTuple(tuple, slot, buffer);

  return result;
}

static void
heapam_finish_bulk_insert(Relation relation, int options)
{
     
                                                                       
                                                                    
     
  if (options & HEAP_INSERT_SKIP_WAL)
  {
    heap_sync(relation);
  }
}

                                                                            
                                      
                                                                            
   

static void
heapam_relation_set_new_filenode(Relation rel, const RelFileNode *newrnode, char persistence, TransactionId *freezeXid, MultiXactId *minmulti)
{
  SMgrRelation srel;

     
                                                                          
                                                                         
              
     
  *freezeXid = RecentXmin;

     
                                                                         
                                                                            
                                                                     
                                            
     
                                                                    
     
  *minmulti = GetOldestMultiXactId();

  srel = RelationCreateStorage(*newrnode, persistence);

     
                                                                           
                                                                           
                                                                            
                                                                             
                                                                        
                                                                           
                                                                        
     
  if (persistence == RELPERSISTENCE_UNLOGGED)
  {
    Assert(rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_MATVIEW || rel->rd_rel->relkind == RELKIND_TOASTVALUE);
    smgrcreate(srel, INIT_FORKNUM, false);
    log_smgrcreate(newrnode, INIT_FORKNUM);
    smgrimmedsync(srel, INIT_FORKNUM);
  }

  smgrclose(srel);
}

static void
heapam_relation_nontransactional_truncate(Relation rel)
{
  RelationTruncate(rel, 0);
}

static void
heapam_relation_copy_data(Relation rel, const RelFileNode *newrnode)
{
  SMgrRelation dstrel;

  dstrel = smgropen(*newrnode, rel->rd_backend);

     
                                                                            
                                                                           
                                                                            
                                        
     
  FlushRelationBuffers(rel);

     
                                                                          
                         
     
                                                               
                              
     
  RelationCreateStorage(*newrnode, rel->rd_rel->relpersistence);

                      
  RelationCopyStorage(RelationGetSmgr(rel), dstrel, MAIN_FORKNUM, rel->rd_rel->relpersistence);

                                         
  for (ForkNumber forkNum = MAIN_FORKNUM + 1; forkNum <= MAX_FORKNUM; forkNum++)
  {
    if (smgrexists(RelationGetSmgr(rel), forkNum))
    {
      smgrcreate(dstrel, forkNum, false);

         
                                                                        
                                            
         
      if (rel->rd_rel->relpersistence == RELPERSISTENCE_PERMANENT || (rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED && forkNum == INIT_FORKNUM))
      {
        log_smgrcreate(newrnode, forkNum);
      }
      RelationCopyStorage(RelationGetSmgr(rel), dstrel, forkNum, rel->rd_rel->relpersistence);
    }
  }

                                            
  RelationDropStorage(rel);
  smgrclose(dstrel);
}

static void
heapam_relation_copy_for_cluster(Relation OldHeap, Relation NewHeap, Relation OldIndex, bool use_sort, TransactionId OldestXmin, TransactionId *xid_cutoff, MultiXactId *multi_cutoff, double *num_tuples, double *tups_vacuumed, double *tups_recently_dead)
{
  RewriteState rwstate;
  IndexScanDesc indexScan;
  TableScanDesc tableScan;
  HeapScanDesc heapScan;
  bool use_wal;
  bool is_system_catalog;
  Tuplesortstate *tuplesort;
  TupleDesc oldTupDesc = RelationGetDescr(OldHeap);
  TupleDesc newTupDesc = RelationGetDescr(NewHeap);
  TupleTableSlot *slot;
  int natts;
  Datum *values;
  bool *isnull;
  BufferHeapTupleTableSlot *hslot;
  BlockNumber prev_cblock = InvalidBlockNumber;

                                         
  is_system_catalog = IsSystemRelation(OldHeap);

     
                                                                          
                                        
     
  use_wal = XLogIsNeeded() && RelationNeedsWAL(NewHeap);

                                                                
  Assert(RelationGetTargetBlock(NewHeap) == InvalidBlockNumber);

                                        
  natts = newTupDesc->natts;
  values = (Datum *)palloc(natts * sizeof(Datum));
  isnull = (bool *)palloc(natts * sizeof(bool));

                                        
  rwstate = begin_heap_rewrite(OldHeap, NewHeap, OldestXmin, *xid_cutoff, *multi_cutoff, use_wal);

                                
  if (use_sort)
  {
    tuplesort = tuplesort_begin_cluster(oldTupDesc, OldIndex, maintenance_work_mem, NULL, false);
  }
  else
  {
    tuplesort = NULL;
  }

     
                                                                         
                                                                    
                                                       
     
  if (OldIndex != NULL && !use_sort)
  {
    const int ci_index[] = {PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_INDEX_RELID};
    int64 ci_val[2];

                                              
    ci_val[0] = PROGRESS_CLUSTER_PHASE_INDEX_SCAN_HEAP;
    ci_val[1] = RelationGetRelid(OldIndex);
    pgstat_progress_update_multi_param(2, ci_index, ci_val);

    tableScan = NULL;
    heapScan = NULL;
    indexScan = index_beginscan(OldHeap, OldIndex, SnapshotAny, 0, 0);
    index_rescan(indexScan, NULL, 0, NULL, 0);
  }
  else
  {
                                                               
    pgstat_progress_update_param(PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_PHASE_SEQ_SCAN_HEAP);

    tableScan = table_beginscan(OldHeap, SnapshotAny, 0, (ScanKey)NULL);
    heapScan = (HeapScanDesc)tableScan;
    indexScan = NULL;

                               
    pgstat_progress_update_param(PROGRESS_CLUSTER_TOTAL_HEAP_BLKS, heapScan->rs_nblocks);
  }

  slot = table_slot_create(OldHeap, NULL);
  hslot = (BufferHeapTupleTableSlot *)slot;

     
                                                                         
                                                                       
                                                                            
                               
     
  for (;;)
  {
    HeapTuple tuple;
    Buffer buf;
    bool isdead;

    CHECK_FOR_INTERRUPTS();

    if (indexScan != NULL)
    {
      if (!index_getnext_slot(indexScan, ForwardScanDirection, slot))
      {
        break;
      }

                                                                    
      if (indexScan->xs_recheck)
      {
        elog(ERROR, "CLUSTER does not support lossy index conditions");
      }
    }
    else
    {
      if (!table_scan_getnextslot(tableScan, ForwardScanDirection, slot))
      {
           
                                                                    
                                                                      
                                                                      
                                                                      
                                                                   
                          
           
        pgstat_progress_update_param(PROGRESS_CLUSTER_HEAP_BLKS_SCANNED, heapScan->rs_nblocks);
        break;
      }

         
                                                                     
                 
         
                                                                         
                                                                      
                                                                        
                                                                         
         
      if (prev_cblock != heapScan->rs_cblock)
      {
        pgstat_progress_update_param(PROGRESS_CLUSTER_HEAP_BLKS_SCANNED, (heapScan->rs_cblock + heapScan->rs_nblocks - heapScan->rs_startblock) % heapScan->rs_nblocks + 1);
        prev_cblock = heapScan->rs_cblock;
      }
    }

    tuple = ExecFetchSlotHeapTuple(slot, false, NULL);
    buf = hslot->buffer;

    LockBuffer(buf, BUFFER_LOCK_SHARE);

    switch (HeapTupleSatisfiesVacuum(tuple, OldestXmin, buf))
    {
    case HEAPTUPLE_DEAD:
                           
      isdead = true;
      break;
    case HEAPTUPLE_RECENTLY_DEAD:
      *tups_recently_dead += 1;
                        
    case HEAPTUPLE_LIVE:
                                               
      isdead = false;
      break;
    case HEAPTUPLE_INSERT_IN_PROGRESS:

         
                                                                    
                                                                   
                                                            
                                                                     
                                                                    
                                     
         
      if (!is_system_catalog && !TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple->t_data)))
      {
        elog(WARNING, "concurrent insert in progress within table \"%s\"", RelationGetRelationName(OldHeap));
      }
                         
      isdead = false;
      break;
    case HEAPTUPLE_DELETE_IN_PROGRESS:

         
                                                       
         
      if (!is_system_catalog && !TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetUpdateXid(tuple->t_data)))
      {
        elog(WARNING, "concurrent delete in progress within table \"%s\"", RelationGetRelationName(OldHeap));
      }
                                  
      *tups_recently_dead += 1;
      isdead = false;
      break;
    default:
      elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
      isdead = false;                          
      break;
    }

    LockBuffer(buf, BUFFER_LOCK_UNLOCK);

    if (isdead)
    {
      *tups_vacuumed += 1;
                                                        
      if (rewrite_heap_dead_tuple(rwstate, tuple))
      {
                                                              
        *tups_vacuumed += 1;
        *tups_recently_dead -= 1;
      }
      continue;
    }

    *num_tuples += 1;
    if (tuplesort != NULL)
    {
      tuplesort_putheaptuple(tuplesort, tuple);

         
                                                                    
                 
         
      pgstat_progress_update_param(PROGRESS_CLUSTER_HEAP_TUPLES_SCANNED, *num_tuples);
    }
    else
    {
      const int ct_index[] = {PROGRESS_CLUSTER_HEAP_TUPLES_SCANNED, PROGRESS_CLUSTER_HEAP_TUPLES_WRITTEN};
      int64 ct_val[2];

      reform_and_rewrite_tuple(tuple, OldHeap, NewHeap, values, isnull, rwstate);

         
                                                                    
                                              
         
      ct_val[0] = *num_tuples;
      ct_val[1] = *num_tuples;
      pgstat_progress_update_multi_param(2, ct_index, ct_val);
    }
  }

  if (indexScan != NULL)
  {
    index_endscan(indexScan);
  }
  if (tableScan != NULL)
  {
    table_endscan(tableScan);
  }
  if (slot)
  {
    ExecDropSingleTupleTableSlot(slot);
  }

     
                                                                             
                                                             
     
  if (tuplesort != NULL)
  {
    double n_tuples = 0;

                                               
    pgstat_progress_update_param(PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_PHASE_SORT_TUPLES);

    tuplesort_performsort(tuplesort);

                                                 
    pgstat_progress_update_param(PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_PHASE_WRITE_NEW_HEAP);

    for (;;)
    {
      HeapTuple tuple;

      CHECK_FOR_INTERRUPTS();

      tuple = tuplesort_getheaptuple(tuplesort, true);
      if (tuple == NULL)
      {
        break;
      }

      n_tuples += 1;
      reform_and_rewrite_tuple(tuple, OldHeap, NewHeap, values, isnull, rwstate);
                           
      pgstat_progress_update_param(PROGRESS_CLUSTER_HEAP_TUPLES_WRITTEN, n_tuples);
    }

    tuplesort_end(tuplesort);
  }

                                                           
  end_heap_rewrite(rwstate);

                
  pfree(values);
  pfree(isnull);
}

static bool
heapam_scan_analyze_next_block(TableScanDesc scan, BlockNumber blockno, BufferAccessStrategy bstrategy)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;

     
                                                                       
                                                                             
                                                                         
                                                                         
                                                                          
                                                                          
              
     
  hscan->rs_cblock = blockno;
  hscan->rs_cindex = FirstOffsetNumber;
  hscan->rs_cbuf = ReadBufferExtended(scan->rs_rd, MAIN_FORKNUM, blockno, RBM_NORMAL, bstrategy);
  LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_SHARE);

                                                                    
  return true;
}

static bool
heapam_scan_analyze_next_tuple(TableScanDesc scan, TransactionId OldestXmin, double *liverows, double *deadrows, TupleTableSlot *slot)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;
  Page targpage;
  OffsetNumber maxoffset;
  BufferHeapTupleTableSlot *hslot;

  Assert(TTS_IS_BUFFERTUPLE(slot));

  hslot = (BufferHeapTupleTableSlot *)slot;
  targpage = BufferGetPage(hscan->rs_cbuf);
  maxoffset = PageGetMaxOffsetNumber(targpage);

                                                       
  for (; hscan->rs_cindex <= maxoffset; hscan->rs_cindex++)
  {
    ItemId itemid;
    HeapTuple targtuple = &hslot->base.tupdata;
    bool sample_it = false;

    itemid = PageGetItemId(targpage, hscan->rs_cindex);

       
                                                                        
                                                                           
                                                              
                                        
       
    if (!ItemIdIsNormal(itemid))
    {
      if (ItemIdIsDead(itemid))
      {
        *deadrows += 1;
      }
      continue;
    }

    ItemPointerSet(&targtuple->t_self, hscan->rs_cblock, hscan->rs_cindex);

    targtuple->t_tableOid = RelationGetRelid(scan->rs_rd);
    targtuple->t_data = (HeapTupleHeader)PageGetItem(targpage, itemid);
    targtuple->t_len = ItemIdGetLength(itemid);

    switch (HeapTupleSatisfiesVacuum(targtuple, OldestXmin, hscan->rs_cbuf))
    {
    case HEAPTUPLE_LIVE:
      sample_it = true;
      *liverows += 1;
      break;

    case HEAPTUPLE_DEAD:
    case HEAPTUPLE_RECENTLY_DEAD:
                                             
      *deadrows += 1;
      break;

    case HEAPTUPLE_INSERT_IN_PROGRESS:

         
                                                                  
                                                                   
                                                                   
                                                                   
                                                                   
                                                                     
                                                                     
                                                                  
                                       
         
                                                                   
                                                                    
                                                                     
                                                                
                                                                  
                     
         
      if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(targtuple->t_data)))
      {
        sample_it = true;
        *liverows += 1;
      }
      break;

    case HEAPTUPLE_DELETE_IN_PROGRESS:

         
                                                                 
                                                                     
                                                             
                                
         
                                                                    
                                                                    
                                                                  
                                                                    
                
         
                                                                     
                                                                   
                                                               
         
                                                                    
                                                                   
                                                                   
                                                                  
                                               
         
      if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetUpdateXid(targtuple->t_data)))
      {
        *deadrows += 1;
      }
      else
      {
        sample_it = true;
        *liverows += 1;
      }
      break;

    default:
      elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
      break;
    }

    if (sample_it)
    {
      ExecStoreBufferHeapTuple(targtuple, slot, hscan->rs_cbuf);
      hscan->rs_cindex++;

                                                      
      return true;
    }
  }

                                                
  UnlockReleaseBuffer(hscan->rs_cbuf);
  hscan->rs_cbuf = InvalidBuffer;

                                                              
  ExecClearTuple(slot);

  return false;
}

static double
heapam_index_build_range_scan(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo, bool allow_sync, bool anyvisible, bool progress, BlockNumber start_blockno, BlockNumber numblocks, IndexBuildCallback callback, void *callback_state, TableScanDesc scan)
{
  HeapScanDesc hscan;
  bool is_system_catalog;
  bool checking_uniqueness;
  HeapTuple heapTuple;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  double reltuples;
  ExprState *predicate;
  TupleTableSlot *slot;
  EState *estate;
  ExprContext *econtext;
  Snapshot snapshot;
  bool need_unregister_snapshot = false;
  TransactionId OldestXmin;
  BlockNumber previous_blkno = InvalidBlockNumber;
  BlockNumber root_blkno = InvalidBlockNumber;
  OffsetNumber root_offsets[MaxHeapTuplesPerPage];

     
                   
     
  Assert(OidIsValid(indexRelation->rd_rel->relam));

                                         
  is_system_catalog = IsSystemRelation(heapRelation);

                                                                   
  checking_uniqueness = (indexInfo->ii_Unique || indexInfo->ii_ExclusionOps != NULL);

     
                                                                            
                                     
     
  Assert(!(anyvisible && checking_uniqueness));

     
                                                                          
                                                         
     
  estate = CreateExecutorState();
  econtext = GetPerTupleExprContext(estate);
  slot = table_slot_create(heapRelation, NULL);

                                                                    
  econtext->ecxt_scantuple = slot;

                                                     
  predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);

     
                                                                             
                                                                         
                                                                       
                                                                            
                                                  
     
  OldestXmin = InvalidTransactionId;

                                        
  if (!IsBootstrapProcessingMode() && !indexInfo->ii_Concurrent)
  {
    OldestXmin = GetOldestXmin(heapRelation, PROCARRAY_FLAGS_VACUUM);
  }

  if (!scan)
  {
       
                           
       
                                                                       
                                                                       
       
    if (!TransactionIdIsValid(OldestXmin))
    {
      snapshot = RegisterSnapshot(GetTransactionSnapshot());
      need_unregister_snapshot = true;
    }
    else
    {
      snapshot = SnapshotAny;
    }

    scan = table_beginscan_strat(heapRelation,               
        snapshot,                                            
        0,                                                         
        NULL,                                                
        true,                                                                 
        allow_sync);                                             
  }
  else
  {
       
                             
       
                                                                         
                                                                       
                                                        
       
    Assert(!IsBootstrapProcessingMode());
    Assert(allow_sync);
    snapshot = scan->rs_snapshot;
  }

  hscan = (HeapScanDesc)scan;

                                        
  if (progress)
  {
    BlockNumber nblocks;

    if (hscan->rs_base.rs_parallel != NULL)
    {
      ParallelBlockTableScanDesc pbscan;

      pbscan = (ParallelBlockTableScanDesc)hscan->rs_base.rs_parallel;
      nblocks = pbscan->phs_nblocks;
    }
    else
    {
      nblocks = hscan->rs_nblocks;
    }

    pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_TOTAL, nblocks);
  }

     
                                                                    
                                                                         
                                                                            
                                                         
     
  Assert(snapshot == SnapshotAny || IsMVCCSnapshot(snapshot));
  Assert(snapshot == SnapshotAny ? TransactionIdIsValid(OldestXmin) : !TransactionIdIsValid(OldestXmin));
  Assert(snapshot == SnapshotAny || !anyvisible);

                              
  if (!allow_sync)
  {
    heap_setscanlimits(scan, start_blockno, numblocks);
  }
  else
  {
                                                          
    Assert(start_blockno == 0);
    Assert(numblocks == InvalidBlockNumber);
  }

  reltuples = 0;

     
                                           
     
  while ((heapTuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    bool tupleIsAlive;

    CHECK_FOR_INTERRUPTS();

                                            
    if (progress)
    {
      BlockNumber blocks_done = heapam_scan_get_blocks_done(hscan);

      if (blocks_done != previous_blkno)
      {
        pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE, blocks_done);
        previous_blkno = blocks_done;
      }
    }

       
                                                                         
                                                                         
                                                                          
                                                                          
                                                                         
                                                       
                                                                         
             
       
                                                                  
                                                                   
                                                                          
                                                                  
                                   
       
                                                                      
                                                                        
                                                                           
                                                                           
                         
       
                                                                           
                                                                        
                                                                       
                                                      
       
                                                                       
                                                                        
                                              
       
    if (hscan->rs_cblock != root_blkno)
    {
      Page page = BufferGetPage(hscan->rs_cbuf);

      LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_SHARE);
      heap_get_root_tuples(page, root_offsets);
      LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);

      root_blkno = hscan->rs_cblock;
    }

    if (snapshot == SnapshotAny)
    {
                                      
      bool indexIt;
      TransactionId xwait;

    recheck:

         
                                                                      
                                                                       
                                                                       
                                                                    
         
      LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_SHARE);

         
                                                                         
                                                                       
                                                                         
                                                                  
                 
         
      switch (HeapTupleSatisfiesVacuum(heapTuple, OldestXmin, hscan->rs_cbuf))
      {
      case HEAPTUPLE_DEAD:
                                               
        indexIt = false;
        tupleIsAlive = false;
        break;
      case HEAPTUPLE_LIVE:
                                                    
        indexIt = true;
        tupleIsAlive = true;
                                   
        reltuples += 1;
        break;
      case HEAPTUPLE_RECENTLY_DEAD:

           
                                                              
                                                             
                                                                   
                                                          
           
                                                                  
                                                                   
                                                                 
                                       
           
                                                                  
                                                                   
           
        if (HeapTupleIsHotUpdated(heapTuple))
        {
          indexIt = false;
                                                          
          indexInfo->ii_BrokenHotChain = true;
        }
        else
        {
          indexIt = true;
        }
                                                                 
        tupleIsAlive = false;
        break;
      case HEAPTUPLE_INSERT_IN_PROGRESS:

           
                                                              
                                          
           
        if (anyvisible)
        {
          indexIt = true;
          tupleIsAlive = true;
          reltuples += 1;
          break;
        }

           
                                                                  
                                                                  
                                                              
                                                                
                                                                
                    
           
        xwait = HeapTupleHeaderGetXmin(heapTuple->t_data);
        if (!TransactionIdIsCurrentTransactionId(xwait))
        {
          if (!is_system_catalog)
          {
            elog(WARNING, "concurrent insert in progress within table \"%s\"", RelationGetRelationName(heapRelation));
          }

             
                                                              
                                                           
                                                              
                                                    
             
          if (checking_uniqueness)
          {
               
                                                               
               
            LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);
            XactLockTableWait(xwait, heapRelation, &heapTuple->t_self, XLTW_InsertIndexUnique);
            CHECK_FOR_INTERRUPTS();
            goto recheck;
          }
        }
        else
        {
             
                                  
                                                     
                                                              
                                                   
             
          reltuples += 1;
        }

           
                                                               
                                      
           
        indexIt = true;
        tupleIsAlive = true;
        break;
      case HEAPTUPLE_DELETE_IN_PROGRESS:

           
                                                               
                                                                 
                                                      
           
        if (anyvisible)
        {
          indexIt = true;
          tupleIsAlive = false;
          reltuples += 1;
          break;
        }

        xwait = HeapTupleHeaderGetUpdateXid(heapTuple->t_data);
        if (!TransactionIdIsCurrentTransactionId(xwait))
        {
          if (!is_system_catalog)
          {
            elog(WARNING, "concurrent delete in progress within table \"%s\"", RelationGetRelationName(heapRelation));
          }

             
                                                              
                                                       
                                                                 
                                                             
             
                                                              
                                                              
                                                               
                                                              
                                                                
                                                            
                                                             
             
          if (checking_uniqueness || HeapTupleIsHotUpdated(heapTuple))
          {
               
                                                               
               
            LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);
            XactLockTableWait(xwait, heapRelation, &heapTuple->t_self, XLTW_InsertIndexUnique);
            CHECK_FOR_INTERRUPTS();
            goto recheck;
          }

             
                                                                
                                                
             
          indexIt = true;

             
                                                                
                                                     
                                       
                                                                
                                            
             
          reltuples += 1;
        }
        else if (HeapTupleIsHotUpdated(heapTuple))
        {
             
                                                               
                                                              
                                                                
                                               
             
          indexIt = false;
                                                          
          indexInfo->ii_BrokenHotChain = true;
        }
        else
        {
             
                                                                 
                                                             
                                                           
             
          indexIt = true;
        }
                                                                 
        tupleIsAlive = false;
        break;
      default:
        elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
        indexIt = tupleIsAlive = false;                          
        break;
      }

      LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);

      if (!indexIt)
      {
        continue;
      }
    }
    else
    {
                                                
      tupleIsAlive = true;
      reltuples += 1;
    }

    MemoryContextReset(econtext->ecxt_per_tuple_memory);

                                                       
    ExecStoreBufferHeapTuple(heapTuple, slot, hscan->rs_cbuf);

       
                                                                 
                  
       
    if (predicate != NULL)
    {
      if (!ExecQual(predicate, econtext))
      {
        continue;
      }
    }

       
                                                                        
                                                                           
                                  
       
    FormIndexDatum(indexInfo, slot, estate, values, isnull);

       
                                                                          
                                                                           
                                                       
       

    if (HeapTupleIsHeapOnly(heapTuple))
    {
         
                                                                         
                                                            
         
      HeapTupleData rootTuple;
      OffsetNumber offnum;

      rootTuple = *heapTuple;
      offnum = ItemPointerGetOffsetNumber(&heapTuple->t_self);

         
                                                            
                                                                
                                  
         
      if (root_offsets[offnum - 1] == InvalidOffsetNumber)
      {
        Page page = BufferGetPage(hscan->rs_cbuf);

        LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_SHARE);
        heap_get_root_tuples(page, root_offsets);
        LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);
      }

      if (!OffsetNumberIsValid(root_offsets[offnum - 1]))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("failed to find parent tuple for heap-only tuple at (%u,%u) in table \"%s\"", ItemPointerGetBlockNumber(&heapTuple->t_self), offnum, RelationGetRelationName(heapRelation))));
      }

      ItemPointerSetOffsetNumber(&rootTuple.t_self, root_offsets[offnum - 1]);

                                                               
      callback(indexRelation, &rootTuple, values, isnull, tupleIsAlive, callback_state);
    }
    else
    {
                                                               
      callback(indexRelation, heapTuple, values, isnull, tupleIsAlive, callback_state);
    }
  }

                                           
  if (progress)
  {
    BlockNumber blks_done;

    if (hscan->rs_base.rs_parallel != NULL)
    {
      ParallelBlockTableScanDesc pbscan;

      pbscan = (ParallelBlockTableScanDesc)hscan->rs_base.rs_parallel;
      blks_done = pbscan->phs_nblocks;
    }
    else
    {
      blks_done = hscan->rs_nblocks;
    }

    pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE, blks_done);
  }

  table_endscan(scan);

                                                                   
  if (need_unregister_snapshot)
  {
    UnregisterSnapshot(snapshot);
  }

  ExecDropSingleTupleTableSlot(slot);

  FreeExecutorState(estate);

                                                           
  indexInfo->ii_ExpressionsState = NIL;
  indexInfo->ii_PredicateState = NULL;

  return reltuples;
}

static void
heapam_index_validate_scan(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo, Snapshot snapshot, ValidateIndexState *state)
{
  TableScanDesc scan;
  HeapScanDesc hscan;
  HeapTuple heapTuple;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  ExprState *predicate;
  TupleTableSlot *slot;
  EState *estate;
  ExprContext *econtext;
  BlockNumber root_blkno = InvalidBlockNumber;
  OffsetNumber root_offsets[MaxHeapTuplesPerPage];
  bool in_index[MaxHeapTuplesPerPage];
  BlockNumber previous_blkno = InvalidBlockNumber;

                                     
  ItemPointer indexcursor = NULL;
  ItemPointerData decoded;
  bool tuplesort_empty = false;

     
                   
     
  Assert(OidIsValid(indexRelation->rd_rel->relam));

     
                                                                          
                                                         
     
  estate = CreateExecutorState();
  econtext = GetPerTupleExprContext(estate);
  slot = MakeSingleTupleTableSlot(RelationGetDescr(heapRelation), &TTSOpsHeapTuple);

                                                                    
  econtext->ecxt_scantuple = slot;

                                                     
  predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);

     
                                                                       
                                                                            
                                                                         
                            
     
  scan = table_beginscan_strat(heapRelation,               
      snapshot,                                            
      0,                                                         
      NULL,                                                
      true,                                                                 
      false);                                                     
  hscan = (HeapScanDesc)scan;

  pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_TOTAL, hscan->rs_nblocks);

     
                                            
     
  while ((heapTuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    ItemPointer heapcursor = &heapTuple->t_self;
    ItemPointerData rootTuple;
    OffsetNumber root_offnum;

    CHECK_FOR_INTERRUPTS();

    state->htups += 1;

    if ((previous_blkno == InvalidBlockNumber) || (hscan->rs_cblock != previous_blkno))
    {
      pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE, hscan->rs_cblock);
      previous_blkno = hscan->rs_cblock;
    }

       
                                                                         
                                                                           
                                                                      
       
                                                                      
                                                                     
                                                                           
                                                                        
                                                                           
                                                          
                                                                         
                                                                   
       
    if (hscan->rs_cblock != root_blkno)
    {
      Page page = BufferGetPage(hscan->rs_cbuf);

      LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_SHARE);
      heap_get_root_tuples(page, root_offsets);
      LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);

      memset(in_index, 0, sizeof(in_index));

      root_blkno = hscan->rs_cblock;
    }

                                              
    rootTuple = *heapcursor;
    root_offnum = ItemPointerGetOffsetNumber(heapcursor);

    if (HeapTupleIsHeapOnly(heapTuple))
    {
      root_offnum = root_offsets[root_offnum - 1];
      if (!OffsetNumberIsValid(root_offnum))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("failed to find parent tuple for heap-only tuple at (%u,%u) in table \"%s\"", ItemPointerGetBlockNumber(heapcursor), ItemPointerGetOffsetNumber(heapcursor), RelationGetRelationName(heapRelation))));
      }
      ItemPointerSetOffsetNumber(&rootTuple, root_offnum);
    }

       
                                                                          
                               
       
    while (!tuplesort_empty && (!indexcursor || ItemPointerCompare(indexcursor, &rootTuple) < 0))
    {
      Datum ts_val;
      bool ts_isnull;

      if (indexcursor)
      {
           
                                                                      
           
        if (ItemPointerGetBlockNumber(indexcursor) == root_blkno)
        {
          in_index[ItemPointerGetOffsetNumber(indexcursor) - 1] = true;
        }
      }

      tuplesort_empty = !tuplesort_getdatum(state->tuplesort, true, &ts_val, &ts_isnull, NULL);
      Assert(tuplesort_empty || !ts_isnull);
      if (!tuplesort_empty)
      {
        itemptr_decode(&decoded, DatumGetInt64(ts_val));
        indexcursor = &decoded;

                                                                     
#ifndef USE_FLOAT8_BYVAL
        pfree(DatumGetPointer(ts_val));
#endif
      }
      else
      {
                     
        indexcursor = NULL;
      }
    }

       
                                                                          
                                                                
       
    if ((tuplesort_empty || ItemPointerCompare(indexcursor, &rootTuple) > 0) && !in_index[root_offnum - 1])
    {
      MemoryContextReset(econtext->ecxt_per_tuple_memory);

                                                         
      ExecStoreHeapTuple(heapTuple, slot, false);

         
                                                                   
                    
         
      if (predicate != NULL)
      {
        if (!ExecQual(predicate, econtext))
        {
          continue;
        }
      }

         
                                                                       
                                                                     
                                               
         
      FormIndexDatum(indexInfo, slot, estate, values, isnull);

         
                                                                        
                                                                      
                                                                   
         

         
                                                                    
                                                                        
                                                                        
                                                                      
                                                                 
                                                                       
                                                                    
                                                                      
                       
         

      index_insert(indexRelation, values, isnull, &rootTuple, heapRelation, indexInfo->ii_Unique ? UNIQUE_CHECK_YES : UNIQUE_CHECK_NO, indexInfo);

      state->tups_inserted += 1;
    }
  }

  table_endscan(scan);

  ExecDropSingleTupleTableSlot(slot);

  FreeExecutorState(estate);

                                                           
  indexInfo->ii_ExpressionsState = NIL;
  indexInfo->ii_PredicateState = NULL;
}

   
                                                                      
                                                                        
                                                                            
                                      
   
static BlockNumber
heapam_scan_get_blocks_done(HeapScanDesc hscan)
{
  ParallelBlockTableScanDesc bpscan = NULL;
  BlockNumber startblock;
  BlockNumber blocks_done;

  if (hscan->rs_base.rs_parallel != NULL)
  {
    bpscan = (ParallelBlockTableScanDesc)hscan->rs_base.rs_parallel;
    startblock = bpscan->phs_startblock;
  }
  else
  {
    startblock = hscan->rs_startblock;
  }

     
                                                                          
               
     
  if (hscan->rs_cblock > startblock)
  {
    blocks_done = hscan->rs_cblock - startblock;
  }
  else
  {
    BlockNumber nblocks;

    nblocks = bpscan != NULL ? bpscan->phs_nblocks : hscan->rs_nblocks;
    blocks_done = nblocks - startblock + hscan->rs_cblock;
  }

  return blocks_done;
}

                                                                            
                                           
                                                                            
   

static uint64
heapam_relation_size(Relation rel, ForkNumber forkNumber)
{
  uint64 nblocks = 0;
  SMgrRelation reln;

     
                                                                          
                                                                         
                                                   
     
  reln = RelationGetSmgr(rel);

                                                                    
  if (forkNumber == InvalidForkNumber)
  {
    for (int i = 0; i < MAX_FORKNUM; i++)
    {
      nblocks += smgrnblocks(reln, i);
    }
  }
  else
  {
    nblocks = smgrnblocks(reln, forkNumber);
  }

  return nblocks * BLCKSZ;
}

   
                                                                        
                                                                      
                                                                     
                                                              
   
static bool
heapam_relation_needs_toast_table(Relation rel)
{
  int32 data_length = 0;
  bool maxlength_unknown = false;
  bool has_toastable_attrs = false;
  TupleDesc tupdesc = rel->rd_att;
  int32 tuple_length;
  int i;

  for (i = 0; i < tupdesc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

    if (att->attisdropped)
    {
      continue;
    }
    data_length = att_align_nominal(data_length, att->attalign);
    if (att->attlen > 0)
    {
                                                  
      data_length += att->attlen;
    }
    else
    {
      int32 maxlen = type_maximum_size(att->atttypid, att->atttypmod);

      if (maxlen < 0)
      {
        maxlength_unknown = true;
      }
      else
      {
        data_length += maxlen;
      }
      if (att->attstorage != 'p')
      {
        has_toastable_attrs = true;
      }
    }
  }
  if (!has_toastable_attrs)
  {
    return false;                        
  }
  if (maxlength_unknown)
  {
    return true;                                  
  }
  tuple_length = MAXALIGN(SizeofHeapTupleHeader + BITMAPLEN(tupdesc->natts)) + MAXALIGN(data_length);
  return (tuple_length > TOAST_TUPLE_THRESHOLD);
}

                                                                            
                                             
                                                                            
   

static void
heapam_estimate_rel_size(Relation rel, int32 *attr_widths, BlockNumber *pages, double *tuples, double *allvisfrac)
{
  BlockNumber curpages;
  BlockNumber relpages;
  double reltuples;
  BlockNumber relallvisible;
  double density;

                                           
  curpages = RelationGetNumberOfBlocks(rel);

                                                         
  relpages = (BlockNumber)rel->rd_rel->relpages;
  reltuples = (double)rel->rd_rel->reltuples;
  relallvisible = (BlockNumber)rel->rd_rel->relallvisible;

     
                                                                           
                                                                 
                                                                           
                                                                             
                                                                           
                                  
     
                                                                             
                                                                             
                                                                          
                                                                             
                                                                        
                                                                          
                                
     
                                                                             
                                                                  
                                                                     
                                                                         
                     
     
                                                                           
                                                                           
                                     
     
  if (curpages < 10 && relpages == 0 && !rel->rd_rel->relhassubclass)
  {
    curpages = 10;
  }

                                
  *pages = curpages;
                                          
  if (curpages == 0)
  {
    *tuples = 0;
    *allvisfrac = 0;
    return;
  }

                                                             
  if (relpages > 0)
  {
    density = reltuples / (double)relpages;
  }
  else
  {
       
                                                                         
                                                                      
                                                                        
                                                                         
                                                                   
                                                
       
                                                                          
                                                                        
                                                                        
                                                                          
       
    int32 tuple_width;

    tuple_width = get_rel_data_width(rel, attr_widths);
    tuple_width += MAXALIGN(SizeofHeapTupleHeader);
    tuple_width += sizeof(ItemIdData);
                                                    
    density = (BLCKSZ - SizeOfPageHeaderData) / tuple_width;
  }
  *tuples = rint(density * (double)curpages);

     
                                                                          
                                                                           
                                                                             
                                       
     
  if (relallvisible == 0 || curpages <= 0)
  {
    *allvisfrac = 0;
  }
  else if ((double)relallvisible >= curpages)
  {
    *allvisfrac = 1;
  }
  else
  {
    *allvisfrac = (double)relallvisible / curpages;
  }
}

                                                                            
                                              
                                                                            
   

static bool
heapam_scan_bitmap_next_block(TableScanDesc scan, TBMIterateResult *tbmres)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;
  BlockNumber page = tbmres->blockno;
  Buffer buffer;
  Snapshot snapshot;
  int ntup;

  hscan->rs_cindex = 0;
  hscan->rs_ntuples = 0;

     
                                                                     
                                                                         
                                                                     
               
     
  if (page >= hscan->rs_nblocks)
  {
    return false;
  }

     
                                                                             
     
  hscan->rs_cbuf = ReleaseAndReadBuffer(hscan->rs_cbuf, scan->rs_rd, page);
  hscan->rs_cblock = page;
  buffer = hscan->rs_cbuf;
  snapshot = scan->rs_snapshot;

  ntup = 0;

     
                                                                     
     
  heap_page_prune_opt(scan->rs_rd, buffer);

     
                                                                         
                                                                      
                                                                    
     
  LockBuffer(buffer, BUFFER_LOCK_SHARE);

     
                                                                    
     
  if (tbmres->ntuples >= 0)
  {
       
                                                                          
                                                                         
               
       
    int curslot;

    for (curslot = 0; curslot < tbmres->ntuples; curslot++)
    {
      OffsetNumber offnum = tbmres->offsets[curslot];
      ItemPointerData tid;
      HeapTupleData heapTuple;

      ItemPointerSet(&tid, page, offnum);
      if (heap_hot_search_buffer(&tid, scan->rs_rd, buffer, snapshot, &heapTuple, NULL, true))
      {
        hscan->rs_vistuples[ntup++] = ItemPointerGetOffsetNumber(&tid);
      }
    }
  }
  else
  {
       
                                                                          
                                                                          
       
    Page dp = (Page)BufferGetPage(buffer);
    OffsetNumber maxoff = PageGetMaxOffsetNumber(dp);
    OffsetNumber offnum;

    for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
    {
      ItemId lp;
      HeapTupleData loctup;
      bool valid;

      lp = PageGetItemId(dp, offnum);
      if (!ItemIdIsNormal(lp))
      {
        continue;
      }
      loctup.t_data = (HeapTupleHeader)PageGetItem((Page)dp, lp);
      loctup.t_len = ItemIdGetLength(lp);
      loctup.t_tableOid = scan->rs_rd->rd_id;
      ItemPointerSet(&loctup.t_self, page, offnum);
      valid = HeapTupleSatisfiesVisibility(&loctup, snapshot, buffer);
      if (valid)
      {
        hscan->rs_vistuples[ntup++] = offnum;
        PredicateLockTuple(scan->rs_rd, &loctup, snapshot);
      }
      CheckForSerializableConflictOut(valid, scan->rs_rd, &loctup, buffer, snapshot);
    }
  }

  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

  Assert(ntup <= MaxHeapTuplesPerPage);
  hscan->rs_ntuples = ntup;

  return ntup > 0;
}

static bool
heapam_scan_bitmap_next_tuple(TableScanDesc scan, TBMIterateResult *tbmres, TupleTableSlot *slot)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;
  OffsetNumber targoffset;
  Page dp;
  ItemId lp;

     
                                                                
     
  if (hscan->rs_cindex < 0 || hscan->rs_cindex >= hscan->rs_ntuples)
  {
    return false;
  }

  targoffset = hscan->rs_vistuples[hscan->rs_cindex];
  dp = (Page)BufferGetPage(hscan->rs_cbuf);
  lp = PageGetItemId(dp, targoffset);
  Assert(ItemIdIsNormal(lp));

  hscan->rs_ctup.t_data = (HeapTupleHeader)PageGetItem((Page)dp, lp);
  hscan->rs_ctup.t_len = ItemIdGetLength(lp);
  hscan->rs_ctup.t_tableOid = scan->rs_rd->rd_id;
  ItemPointerSet(&hscan->rs_ctup.t_self, hscan->rs_cblock, targoffset);

  pgstat_count_heap_fetch(scan->rs_rd);

     
                                                                        
                                   
     
  ExecStoreBufferHeapTuple(&hscan->rs_ctup, slot, hscan->rs_cbuf);

  hscan->rs_cindex++;

  return true;
}

static bool
heapam_scan_sample_next_block(TableScanDesc scan, SampleScanState *scanstate)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;
  TsmRoutine *tsm = scanstate->tsmroutine;
  BlockNumber blockno;

                                                     
  if (hscan->rs_nblocks == 0)
  {
    return false;
  }

  if (tsm->NextSampleBlock)
  {
    blockno = tsm->NextSampleBlock(scanstate, hscan->rs_nblocks);
    hscan->rs_cblock = blockno;
  }
  else
  {
                                     

    if (hscan->rs_cblock == InvalidBlockNumber)
    {
      Assert(!hscan->rs_inited);
      blockno = hscan->rs_startblock;
    }
    else
    {
      Assert(hscan->rs_inited);

      blockno = hscan->rs_cblock + 1;

      if (blockno >= hscan->rs_nblocks)
      {
                                                                   
        blockno = 0;
      }

         
                                                                    
         
                                                                      
                                                                      
                                                                         
                                                                         
                                                                         
                                                                      
         
      if (scan->rs_flags & SO_ALLOW_SYNC)
      {
        ss_report_location(scan->rs_rd, blockno);
      }

      if (blockno == hscan->rs_startblock)
      {
        blockno = InvalidBlockNumber;
      }
    }
  }

  if (!BlockNumberIsValid(blockno))
  {
    if (BufferIsValid(hscan->rs_cbuf))
    {
      ReleaseBuffer(hscan->rs_cbuf);
    }
    hscan->rs_cbuf = InvalidBuffer;
    hscan->rs_cblock = InvalidBlockNumber;
    hscan->rs_inited = false;

    return false;
  }

  heapgetpage(scan, blockno);
  hscan->rs_inited = true;

  return true;
}

static bool
heapam_scan_sample_next_tuple(TableScanDesc scan, SampleScanState *scanstate, TupleTableSlot *slot)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;
  TsmRoutine *tsm = scanstate->tsmroutine;
  BlockNumber blockno = hscan->rs_cblock;
  bool pagemode = (scan->rs_flags & SO_ALLOW_PAGEMODE) != 0;

  Page page;
  bool all_visible;
  OffsetNumber maxoffset;

     
                                                                   
                        
     
  if (!pagemode)
  {
    LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_SHARE);
  }

  page = (Page)BufferGetPage(hscan->rs_cbuf);
  all_visible = PageIsAllVisible(page) && !scan->rs_snapshot->takenDuringRecovery;
  maxoffset = PageGetMaxOffsetNumber(page);

  for (;;)
  {
    OffsetNumber tupoffset;

    CHECK_FOR_INTERRUPTS();

                                                                        
    tupoffset = tsm->NextSampleTuple(scanstate, blockno, maxoffset);

    if (OffsetNumberIsValid(tupoffset))
    {
      ItemId itemid;
      bool visible;
      HeapTuple tuple = &(hscan->rs_ctup);

                                        
      itemid = PageGetItemId(page, tupoffset);
      if (!ItemIdIsNormal(itemid))
      {
        continue;
      }

      tuple->t_data = (HeapTupleHeader)PageGetItem(page, itemid);
      tuple->t_len = ItemIdGetLength(itemid);
      ItemPointerSet(&(tuple->t_self), blockno, tupoffset);

      if (all_visible)
      {
        visible = true;
      }
      else
      {
        visible = SampleHeapTupleVisible(scan, hscan->rs_cbuf, tuple, tupoffset);
      }

                                                    
      if (!pagemode)
      {
        CheckForSerializableConflictOut(visible, scan->rs_rd, tuple, hscan->rs_cbuf, scan->rs_snapshot);
      }

                                          
      if (!visible)
      {
        continue;
      }

                                           
      if (!pagemode)
      {
        LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);
      }

      ExecStoreBufferHeapTuple(tuple, slot, hscan->rs_cbuf);

                                                             
      pgstat_count_heap_getnext(scan->rs_rd);

      return true;
    }
    else
    {
         
                                                                         
                                            
         
      if (!pagemode)
      {
        LockBuffer(hscan->rs_cbuf, BUFFER_LOCK_UNLOCK);
      }

      ExecClearTuple(slot);
      return false;
    }
  }

  Assert(0);
}

                                                                                
                                    
                                                                                
   

   
                                           
   
                                                               
   
                                                                       
                                                                      
                                                                    
                                                                
   
                                                                   
                                                                    
                     
   
                                                            
   
static void
reform_and_rewrite_tuple(HeapTuple tuple, Relation OldHeap, Relation NewHeap, Datum *values, bool *isnull, RewriteState rwstate)
{
  TupleDesc oldTupDesc = RelationGetDescr(OldHeap);
  TupleDesc newTupDesc = RelationGetDescr(NewHeap);
  HeapTuple copiedTuple;
  int i;

  heap_deform_tuple(tuple, oldTupDesc, values, isnull);

                                               
  for (i = 0; i < newTupDesc->natts; i++)
  {
    if (TupleDescAttr(newTupDesc, i)->attisdropped)
    {
      isnull[i] = true;
    }
  }

  copiedTuple = heap_form_tuple(newTupDesc, values, isnull);

                                             
  rewrite_heap_tuple(rwstate, tuple, copiedTuple);

  heap_freetuple(copiedTuple);
}

   
                                  
   
static bool
SampleHeapTupleVisible(TableScanDesc scan, Buffer buffer, HeapTuple tuple, OffsetNumber tupoffset)
{
  HeapScanDesc hscan = (HeapScanDesc)scan;

  if (scan->rs_flags & SO_ALLOW_PAGEMODE)
  {
       
                                                                         
                                                           
       
                                                                           
                                                                          
                                                                          
                                        
       
    int start = 0, end = hscan->rs_ntuples - 1;

    while (start <= end)
    {
      int mid = (start + end) / 2;
      OffsetNumber curoffset = hscan->rs_vistuples[mid];

      if (tupoffset == curoffset)
      {
        return true;
      }
      else if (tupoffset < curoffset)
      {
        end = mid - 1;
      }
      else
      {
        start = mid + 1;
      }
    }

    return false;
  }
  else
  {
                                                             
    return HeapTupleSatisfiesVisibility(tuple, scan->rs_snapshot, buffer);
  }
}

                                                                            
                                               
                                                                            
   

static const TableAmRoutine heapam_methods = {.type = T_TableAmRoutine,

    .slot_callbacks = heapam_slot_callbacks,

    .scan_begin = heap_beginscan,
    .scan_end = heap_endscan,
    .scan_rescan = heap_rescan,
    .scan_getnextslot = heap_getnextslot,

    .parallelscan_estimate = table_block_parallelscan_estimate,
    .parallelscan_initialize = table_block_parallelscan_initialize,
    .parallelscan_reinitialize = table_block_parallelscan_reinitialize,

    .index_fetch_begin = heapam_index_fetch_begin,
    .index_fetch_reset = heapam_index_fetch_reset,
    .index_fetch_end = heapam_index_fetch_end,
    .index_fetch_tuple = heapam_index_fetch_tuple,

    .tuple_insert = heapam_tuple_insert,
    .tuple_insert_speculative = heapam_tuple_insert_speculative,
    .tuple_complete_speculative = heapam_tuple_complete_speculative,
    .multi_insert = heap_multi_insert,
    .tuple_delete = heapam_tuple_delete,
    .tuple_update = heapam_tuple_update,
    .tuple_lock = heapam_tuple_lock,
    .finish_bulk_insert = heapam_finish_bulk_insert,

    .tuple_fetch_row_version = heapam_fetch_row_version,
    .tuple_get_latest_tid = heap_get_latest_tid,
    .tuple_tid_valid = heapam_tuple_tid_valid,
    .tuple_satisfies_snapshot = heapam_tuple_satisfies_snapshot,
    .compute_xid_horizon_for_tuples = heap_compute_xid_horizon_for_tuples,

    .relation_set_new_filenode = heapam_relation_set_new_filenode,
    .relation_nontransactional_truncate = heapam_relation_nontransactional_truncate,
    .relation_copy_data = heapam_relation_copy_data,
    .relation_copy_for_cluster = heapam_relation_copy_for_cluster,
    .relation_vacuum = heap_vacuum_rel,
    .scan_analyze_next_block = heapam_scan_analyze_next_block,
    .scan_analyze_next_tuple = heapam_scan_analyze_next_tuple,
    .index_build_range_scan = heapam_index_build_range_scan,
    .index_validate_scan = heapam_index_validate_scan,

    .relation_size = heapam_relation_size,
    .relation_needs_toast_table = heapam_relation_needs_toast_table,

    .relation_estimate_size = heapam_estimate_rel_size,

    .scan_bitmap_next_block = heapam_scan_bitmap_next_block,
    .scan_bitmap_next_tuple = heapam_scan_bitmap_next_tuple,
    .scan_sample_next_block = heapam_scan_sample_next_block,
    .scan_sample_next_tuple = heapam_scan_sample_next_tuple};

const TableAmRoutine *
GetHeapamTableAmRoutine(void)
{
  return &heapam_methods;
}

Datum
heap_tableam_handler(PG_FUNCTION_ARGS)
{
  PG_RETURN_POINTER(&heapam_methods);
}
