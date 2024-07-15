                                                                         
   
             
                                                                 
   
                                                                         
                                                                        
   
   
                  
                                        
   
         
                                                                              
                                                                             
                                                                                 
   
                                                                         
   
#include "postgres.h"

#include "access/heapam.h"               
#include "access/tableam.h"
#include "access/xact.h"
#include "storage/bufmgr.h"
#include "storage/shmem.h"

                   
char *default_table_access_method = DEFAULT_TABLE_ACCESS_METHOD;
bool synchronize_seqscans = true;

                                                                                
                   
                                                                                
   

const TupleTableSlotOps *
table_slot_callbacks(Relation relation)
{
  const TupleTableSlotOps *tts_cb;

  if (relation->rd_tableam)
  {
    tts_cb = relation->rd_tableam->slot_callbacks(relation);
  }
  else if (relation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
       
                                                                        
                                                                      
                                                                       
              
       
    tts_cb = &TTSOpsHeapTuple;
  }
  else
  {
       
                                                                         
                                                                       
                                                                       
                       
       
    Assert(relation->rd_rel->relkind == RELKIND_VIEW || relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);
    tts_cb = &TTSOpsVirtual;
  }

  return tts_cb;
}

TupleTableSlot *
table_slot_create(Relation relation, List **reglist)
{
  const TupleTableSlotOps *tts_cb;
  TupleTableSlot *slot;

  tts_cb = table_slot_callbacks(relation);
  slot = MakeSingleTupleTableSlot(RelationGetDescr(relation), tts_cb);

  if (reglist)
  {
    *reglist = lappend(*reglist, slot);
  }

  return slot;
}

                                                                                
                         
                                                                                
   

TableScanDesc
table_beginscan_catalog(Relation relation, int nkeys, struct ScanKeyData *key)
{
  uint32 flags = SO_TYPE_SEQSCAN | SO_ALLOW_STRAT | SO_ALLOW_SYNC | SO_ALLOW_PAGEMODE | SO_TEMP_SNAPSHOT;
  Oid relid = RelationGetRelid(relation);
  Snapshot snapshot = RegisterSnapshot(GetCatalogSnapshot(relid));

  return relation->rd_tableam->scan_begin(relation, snapshot, nkeys, key, NULL, flags);
}

void
table_scan_update_snapshot(TableScanDesc scan, Snapshot snapshot)
{
  Assert(IsMVCCSnapshot(snapshot));

  RegisterSnapshot(snapshot);
  scan->rs_snapshot = snapshot;
  scan->rs_flags |= SO_TEMP_SNAPSHOT;
}

                                                                                
                                          
                                                                                
   

Size
table_parallelscan_estimate(Relation rel, Snapshot snapshot)
{
  Size sz = 0;

  if (IsMVCCSnapshot(snapshot))
  {
    sz = add_size(sz, EstimateSnapshotSpace(snapshot));
  }
  else
  {
    Assert(snapshot == SnapshotAny);
  }

  sz = add_size(sz, rel->rd_tableam->parallelscan_estimate(rel));

  return sz;
}

void
table_parallelscan_initialize(Relation rel, ParallelTableScanDesc pscan, Snapshot snapshot)
{
  Size snapshot_off = rel->rd_tableam->parallelscan_initialize(rel, pscan);

  pscan->phs_snapshot_off = snapshot_off;

  if (IsMVCCSnapshot(snapshot))
  {
    SerializeSnapshot(snapshot, (char *)pscan + pscan->phs_snapshot_off);
    pscan->phs_snapshot_any = false;
  }
  else
  {
    Assert(snapshot == SnapshotAny);
    pscan->phs_snapshot_any = true;
  }
}

TableScanDesc
table_beginscan_parallel(Relation relation, ParallelTableScanDesc parallel_scan)
{
  Snapshot snapshot;
  uint32 flags = SO_TYPE_SEQSCAN | SO_ALLOW_STRAT | SO_ALLOW_SYNC | SO_ALLOW_PAGEMODE;

  Assert(RelationGetRelid(relation) == parallel_scan->phs_relid);

  if (!parallel_scan->phs_snapshot_any)
  {
                                               
    snapshot = RestoreSnapshot((char *)parallel_scan + parallel_scan->phs_snapshot_off);
    RegisterSnapshot(snapshot);
    flags |= SO_TEMP_SNAPSHOT;
  }
  else
  {
                                                       
    snapshot = SnapshotAny;
  }

  return relation->rd_tableam->scan_begin(relation, snapshot, 0, NULL, parallel_scan, flags);
}

                                                                                
                                 
                                                                                
   

   
                                                                          
                                                                           
                                                                         
                                                                            
                                                       
   
bool
table_index_fetch_tuple_check(Relation rel, ItemPointer tid, Snapshot snapshot, bool *all_dead)
{
  IndexFetchTableData *scan;
  TupleTableSlot *slot;
  bool call_again = false;
  bool found;

  slot = table_slot_create(rel, NULL);
  scan = table_index_fetch_begin(rel);
  found = table_index_fetch_tuple(scan, tid, snapshot, slot, &call_again, all_dead);
  table_index_fetch_end(scan);
  ExecDropSingleTupleTableSlot(slot);

  return found;
}

                                                                            
                                                               
                                                                            
   

void
table_tuple_get_latest_tid(TableScanDesc scan, ItemPointer tid)
{
  Relation rel = scan->rs_rd;
  const TableAmRoutine *tableam = rel->rd_tableam;

     
                                                                            
               
     
  if (!tableam->tuple_tid_valid(scan, tid))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("tid (%u, %u) is not valid for relation \"%s\"", ItemPointerGetBlockNumberNoCheck(tid), ItemPointerGetOffsetNumberNoCheck(tid), RelationGetRelationName(rel))));
  }

  tableam->tuple_get_latest_tid(scan, tid);
}

                                                                                
                                                  
                                                                                
   

   
                                              
   
                                                                               
                                                                      
   
void
simple_table_tuple_insert(Relation rel, TupleTableSlot *slot)
{
  table_tuple_insert(rel, slot, GetCurrentCommandId(true), 0, NULL);
}

   
                                              
   
                                                                         
                                                                          
                                                                        
                  
   
void
simple_table_tuple_delete(Relation rel, ItemPointer tid, Snapshot snapshot)
{
  TM_Result result;
  TM_FailureData tmfd;

  result = table_tuple_delete(rel, tid, GetCurrentCommandId(true), snapshot, InvalidSnapshot, true                      , &tmfd, false /* changingPart */);

  switch (result)
  {
  case TM_SelfModified:
                                                       
    elog(ERROR, "tuple already updated by self");
    break;

  case TM_Ok:
                           
    break;

  case TM_Updated:
    elog(ERROR, "tuple concurrently updated");
    break;

  case TM_Deleted:
    elog(ERROR, "tuple concurrently deleted");
    break;

  default:
    elog(ERROR, "unrecognized table_tuple_delete status: %u", result);
    break;
  }
}

   
                                               
   
                                                                         
                                                                          
                                                                        
                  
   
void
simple_table_tuple_update(Relation rel, ItemPointer otid, TupleTableSlot *slot, Snapshot snapshot, bool *update_indexes)
{
  TM_Result result;
  TM_FailureData tmfd;
  LockTupleMode lockmode;

  result = table_tuple_update(rel, otid, slot, GetCurrentCommandId(true), snapshot, InvalidSnapshot, true                      , &tmfd, &lockmode, update_indexes);

  switch (result)
  {
  case TM_SelfModified:
                                                       
    elog(ERROR, "tuple already updated by self");
    break;

  case TM_Ok:
                           
    break;

  case TM_Updated:
    elog(ERROR, "tuple concurrently updated");
    break;

  case TM_Deleted:
    elog(ERROR, "tuple concurrently deleted");
    break;

  default:
    elog(ERROR, "unrecognized table_tuple_update status: %u", result);
    break;
  }
}

                                                                                
                                                                        
                                                                                
   

Size
table_block_parallelscan_estimate(Relation rel)
{
  return sizeof(ParallelBlockTableScanDescData);
}

Size
table_block_parallelscan_initialize(Relation rel, ParallelTableScanDesc pscan)
{
  ParallelBlockTableScanDesc bpscan = (ParallelBlockTableScanDesc)pscan;

  bpscan->base.phs_relid = RelationGetRelid(rel);
  bpscan->phs_nblocks = RelationGetNumberOfBlocks(rel);
                                                                        
  bpscan->base.phs_syncscan = synchronize_seqscans && !RelationUsesLocalBuffers(rel) && bpscan->phs_nblocks > NBuffers / 4;
  SpinLockInit(&bpscan->phs_mutex);
  bpscan->phs_startblock = InvalidBlockNumber;
  pg_atomic_init_u64(&bpscan->phs_nallocated, 0);

  return sizeof(ParallelBlockTableScanDescData);
}

void
table_block_parallelscan_reinitialize(Relation rel, ParallelTableScanDesc pscan)
{
  ParallelBlockTableScanDesc bpscan = (ParallelBlockTableScanDesc)pscan;

  pg_atomic_write_u64(&bpscan->phs_nallocated, 0);
}

   
                                      
   
                                                                             
                                                                             
                               
   
void
table_block_parallelscan_startblock_init(Relation rel, ParallelBlockTableScanDesc pbscan)
{
  BlockNumber sync_startpage = InvalidBlockNumber;

retry:
                          
  SpinLockAcquire(&pbscan->phs_mutex);

     
                                                                          
                                                                             
                                                                          
                                                                        
                                                                         
                                                                      
                                                                     
              
     
  if (pbscan->phs_startblock == InvalidBlockNumber)
  {
    if (!pbscan->base.phs_syncscan)
    {
      pbscan->phs_startblock = 0;
    }
    else if (sync_startpage != InvalidBlockNumber)
    {
      pbscan->phs_startblock = sync_startpage;
    }
    else
    {
      SpinLockRelease(&pbscan->phs_mutex);
      sync_startpage = ss_get_location(rel, pbscan->phs_nblocks);
      goto retry;
    }
  }
  SpinLockRelease(&pbscan->phs_mutex);
}

   
                             
   
                                                                        
                                                                          
                                                                            
                                              
   
BlockNumber
table_block_parallelscan_nextpage(Relation rel, ParallelBlockTableScanDesc pbscan)
{
  BlockNumber page;
  uint64 nallocated;

     
                                                                         
                                                                       
                
     
                                                                            
                                                                         
                                                                             
                                                                         
                                                                             
              
     
                                                                          
                                            
     
  nallocated = pg_atomic_fetch_add_u64(&pbscan->phs_nallocated, 1);
  if (nallocated >= pbscan->phs_nblocks)
  {
    page = InvalidBlockNumber;                                     
  }
  else
  {
    page = (nallocated + pbscan->phs_startblock) % pbscan->phs_nblocks;
  }

     
                                                                         
                                                                             
                                                                         
                                                                            
                                                                
     
  if (pbscan->base.phs_syncscan)
  {
    if (page != InvalidBlockNumber)
    {
      ss_report_location(rel, page);
    }
    else if (nallocated == pbscan->phs_nblocks)
    {
      ss_report_location(rel, pbscan->phs_startblock);
    }
  }

  return page;
}
