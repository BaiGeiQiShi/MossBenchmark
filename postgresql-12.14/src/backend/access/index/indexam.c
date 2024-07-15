                                                                            
   
             
                                          
   
                                                                         
                                                                        
   
   
                  
                                        
   
                      
                                                         
                                           
                                                               
                                                                       
                                              
                               
                                                         
                                         
                                             
                                                                           
                                                             
                                                                  
                                                        
                                                     
                                                       
                                                        
                                                 
                                                      
                                                             
                                                            
                                                  
                                                              
   
         
                                                      
                                                           
   
                                                                            
   

#include "postgres.h"

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "catalog/index.h"
#include "catalog/pg_type.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/snapmgr.h"

                                                                    
                                      
   
                                                                          
                                                                            
                                                                            
                                                                          
                                                                           
                                                                           
                                                              
                                                                    
   
#define RELATION_CHECKS (AssertMacro(RelationIsValid(indexRelation)), AssertMacro(PointerIsValid(indexRelation->rd_indam)), AssertMacro(!ReindexIsProcessingIndex(RelationGetRelid(indexRelation))))

#define SCAN_CHECKS (AssertMacro(IndexScanIsValid(scan)), AssertMacro(RelationIsValid(scan->indexRelation)), AssertMacro(PointerIsValid(scan->indexRelation->rd_indam)))

#define CHECK_REL_PROCEDURE(pname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (indexRelation->rd_indam->pname == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      elog(ERROR, "function %s is not defined for index %s", CppAsString(pname), RelationGetRelationName(indexRelation));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  } while (0)

#define CHECK_SCAN_PROCEDURE(pname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (scan->indexRelation->rd_indam->pname == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      elog(ERROR, "function %s is not defined for index %s", CppAsString(pname), RelationGetRelationName(scan->indexRelation));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } while (0)

static IndexScanDesc
index_beginscan_internal(Relation indexRelation, int nkeys, int norderbys, Snapshot snapshot, ParallelIndexScanDesc pscan, bool temp_snap);

                                                                    
                                    
                                                                    
   

                    
                                                        
   
                                                               
                                                              
                                                                 
                    
   
                                                    
   
                                                             
                                                           
                    
   
Relation
index_open(Oid relationId, LOCKMODE lockmode)
{
  Relation r;

  r = relation_open(relationId, lockmode);

  if (r->rd_rel->relkind != RELKIND_INDEX && r->rd_rel->relkind != RELKIND_PARTITIONED_INDEX)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not an index", RelationGetRelationName(r))));
  }

  return r;
}

                    
                                          
   
                                                                     
   
                                                                      
                                                                  
                    
   
void
index_close(Relation relation, LOCKMODE lockmode)
{
  LockRelId relid = relation->rd_lockInfo.lockRelId;

  Assert(lockmode >= NoLock && lockmode < MAX_LOCKMODES);

                                          
  RelationClose(relation);

  if (lockmode != NoLock)
  {
    UnlockRelationId(&relid, lockmode);
  }
}

                    
                                                         
                    
   
bool
index_insert(Relation indexRelation, Datum *values, bool *isnull, ItemPointer heap_t_ctid, Relation heapRelation, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  RELATION_CHECKS;
  CHECK_REL_PROCEDURE(aminsert);

  if (!(indexRelation->rd_indam->ampredlocks))
  {
    CheckForSerializableConflictIn(indexRelation, (HeapTuple)NULL, InvalidBuffer);
  }

  return indexRelation->rd_indam->aminsert(indexRelation, values, isnull, heap_t_ctid, heapRelation, checkUnique, indexInfo);
}

   
                                                              
   
                                                                    
   
IndexScanDesc
index_beginscan(Relation heapRelation, Relation indexRelation, Snapshot snapshot, int nkeys, int norderbys)
{
  IndexScanDesc scan;

  scan = index_beginscan_internal(indexRelation, nkeys, norderbys, snapshot, NULL, false);

     
                                                                            
                                 
     
  scan->heapRelation = heapRelation;
  scan->xs_snapshot = snapshot;

                                                 
  scan->xs_heapfetch = table_index_fetch_begin(heapRelation);

  return scan;
}

   
                                                                      
   
                                                                       
                                                             
   
IndexScanDesc
index_beginscan_bitmap(Relation indexRelation, Snapshot snapshot, int nkeys)
{
  IndexScanDesc scan;

  scan = index_beginscan_internal(indexRelation, nkeys, 0, snapshot, NULL, false);

     
                                                                            
                                 
     
  scan->xs_snapshot = snapshot;

  return scan;
}

   
                                                                         
   
static IndexScanDesc
index_beginscan_internal(Relation indexRelation, int nkeys, int norderbys, Snapshot snapshot, ParallelIndexScanDesc pscan, bool temp_snap)
{
  IndexScanDesc scan;

  RELATION_CHECKS;
  CHECK_REL_PROCEDURE(ambeginscan);

  if (!(indexRelation->rd_indam->ampredlocks))
  {
    PredicateLockRelation(indexRelation, snapshot);
  }

     
                                                                          
     
  RelationIncrementReferenceCount(indexRelation);

     
                                 
     
  scan = indexRelation->rd_indam->ambeginscan(indexRelation, nkeys, norderbys);
                                                 
  scan->parallel_scan = pscan;
  scan->xs_temp_snap = temp_snap;

  return scan;
}

                    
                                                 
   
                                                                         
                                                                               
                                                                             
                                                                             
                                                                            
                                                               
                                
                    
   
void
index_rescan(IndexScanDesc scan, ScanKey keys, int nkeys, ScanKey orderbys, int norderbys)
{
  SCAN_CHECKS;
  CHECK_SCAN_PROCEDURE(amrescan);

  Assert(nkeys == scan->numberOfKeys);
  Assert(norderbys == scan->numberOfOrderBys);

                                                                
  if (scan->xs_heapfetch)
  {
    table_index_fetch_reset(scan->xs_heapfetch);
  }

  scan->kill_prior_tuple = false;                 
  scan->xs_heap_continue = false;

  scan->indexRelation->rd_indam->amrescan(scan, keys, nkeys, orderbys, norderbys);
}

                    
                               
                    
   
void
index_endscan(IndexScanDesc scan)
{
  SCAN_CHECKS;
  CHECK_SCAN_PROCEDURE(amendscan);

                                                                
  if (scan->xs_heapfetch)
  {
    table_index_fetch_end(scan->xs_heapfetch);
    scan->xs_heapfetch = NULL;
  }

                         
  scan->indexRelation->rd_indam->amendscan(scan);

                                                          
  RelationDecrementReferenceCount(scan->indexRelation);

  if (scan->xs_temp_snap)
  {
    UnregisterSnapshot(scan->xs_snapshot);
  }

                                              
  IndexScanEnd(scan);
}

                    
                                          
                    
   
void
index_markpos(IndexScanDesc scan)
{
  SCAN_CHECKS;
  CHECK_SCAN_PROCEDURE(ammarkpos);

  scan->indexRelation->rd_indam->ammarkpos(scan);
}

                    
                                             
   
                                                                          
                                
   
                                                                          
                                                                               
                                                                           
                                                                           
                                                                       
                                                                               
                                                                  
                    
   
void
index_restrpos(IndexScanDesc scan)
{
  Assert(IsMVCCSnapshot(scan->xs_snapshot));

  SCAN_CHECKS;
  CHECK_SCAN_PROCEDURE(amrestrpos);

                                                                
  if (scan->xs_heapfetch)
  {
    table_index_fetch_reset(scan->xs_heapfetch);
  }

  scan->kill_prior_tuple = false;                 
  scan->xs_heap_continue = false;

  scan->indexRelation->rd_indam->amrestrpos(scan);
}

   
                                                                          
   
                                                                          
                                                                            
                             
   
Size
index_parallelscan_estimate(Relation indexRelation, Snapshot snapshot)
{
  Size nbytes;

  RELATION_CHECKS;

  nbytes = offsetof(ParallelIndexScanDescData, ps_snapshot_data);
  nbytes = add_size(nbytes, EstimateSnapshotSpace(snapshot));
  nbytes = MAXALIGN(nbytes);

     
                                                                   
                                                                          
                                            
     
  if (indexRelation->rd_indam->amestimateparallelscan != NULL)
  {
    nbytes = add_size(nbytes, indexRelation->rd_indam->amestimateparallelscan());
  }

  return nbytes;
}

   
                                                            
   
                                                                           
                                 
   
                                                                        
                                                                          
                                                                          
   
void
index_parallelscan_initialize(Relation heapRelation, Relation indexRelation, Snapshot snapshot, ParallelIndexScanDesc target)
{
  Size offset;

  RELATION_CHECKS;

  offset = add_size(offsetof(ParallelIndexScanDescData, ps_snapshot_data), EstimateSnapshotSpace(snapshot));
  offset = MAXALIGN(offset);

  target->ps_relid = RelationGetRelid(heapRelation);
  target->ps_indexid = RelationGetRelid(indexRelation);
  target->ps_offset = offset;
  SerializeSnapshot(snapshot, target->ps_snapshot_data);

                                                                          
  if (indexRelation->rd_indam->aminitparallelscan != NULL)
  {
    void *amtarget;

    amtarget = OffsetToPointer(target, offset);
    indexRelation->rd_indam->aminitparallelscan(amtarget);
  }
}

                    
                                                                  
                    
   
void
index_parallelrescan(IndexScanDesc scan)
{
  SCAN_CHECKS;

  if (scan->xs_heapfetch)
  {
    table_index_fetch_reset(scan->xs_heapfetch);
  }

                                                                        
  if (scan->indexRelation->rd_indam->amparallelrescan != NULL)
  {
    scan->indexRelation->rd_indam->amparallelrescan(scan);
  }
}

   
                                                       
   
                                                                    
   
IndexScanDesc
index_beginscan_parallel(Relation heaprel, Relation indexrel, int nkeys, int norderbys, ParallelIndexScanDesc pscan)
{
  Snapshot snapshot;
  IndexScanDesc scan;

  Assert(RelationGetRelid(heaprel) == pscan->ps_relid);
  snapshot = RestoreSnapshot(pscan->ps_snapshot_data);
  RegisterSnapshot(snapshot);
  scan = index_beginscan_internal(indexrel, nkeys, norderbys, snapshot, pscan, true);

     
                                                                            
                                     
     
  scan->heapRelation = heaprel;
  scan->xs_snapshot = snapshot;

                                                 
  scan->xs_heapfetch = table_index_fetch_begin(heaprel);

  return scan;
}

                    
                                                    
   
                                                        
                                             
                    
   
ItemPointer
index_getnext_tid(IndexScanDesc scan, ScanDirection direction)
{
  bool found;

  SCAN_CHECKS;
  CHECK_SCAN_PROCEDURE(amgettuple);

  Assert(TransactionIdIsValid(RecentGlobalXmin));

     
                                                                           
                                                                       
                                                                           
                                            
     
  found = scan->indexRelation->rd_indam->amgettuple(scan, direction);

                                              
  scan->kill_prior_tuple = false;
  scan->xs_heap_continue = false;

                                                 
  if (!found)
  {
                                                                  
    if (scan->xs_heapfetch)
    {
      table_index_fetch_reset(scan->xs_heapfetch);
    }

    return NULL;
  }
  Assert(ItemPointerIsValid(&scan->xs_heaptid));

  pgstat_count_index_tuples(scan->indexRelation, 1);

                                             
  return &scan->xs_heaptid;
}

                    
                                                      
   
                                                                         
                                                                             
                                                                             
                                                                              
                             
   
                                                                             
                                                                            
          
   
                                                                           
                                                                        
                                                                
                    
   
bool
index_fetch_heap(IndexScanDesc scan, TupleTableSlot *slot)
{
  bool all_dead = false;
  bool found;

  found = table_index_fetch_tuple(scan->xs_heapfetch, &scan->xs_heaptid, scan->xs_snapshot, slot, &scan->xs_heap_continue, &all_dead);

  if (found)
  {
    pgstat_count_heap_fetch(scan->indexRelation);
  }

     
                                                                            
                                                                          
                                                                        
                                                                     
                             
     
  if (!scan->xactStartedInRecovery)
  {
    scan->kill_prior_tuple = all_dead;
  }

  return found;
}

                    
                                                        
   
                                                                               
                                                                       
   
                                                                               
                                                                            
          
   
                                                                           
                                                                        
                                                                
                    
   
bool
index_getnext_slot(IndexScanDesc scan, ScanDirection direction, TupleTableSlot *slot)
{
  for (;;)
  {
    if (!scan->xs_heap_continue)
    {
      ItemPointer tid;

                                                     
      tid = index_getnext_tid(scan, direction);

                                                     
      if (tid == NULL)
      {
        break;
      }

      Assert(ItemPointerEquals(tid, &scan->xs_heaptid));
    }

       
                                                                         
                                                                         
                  
       
    Assert(ItemPointerIsValid(&scan->xs_heaptid));
    if (index_fetch_heap(scan, slot))
    {
      return true;
    }
  }

  return false;
}

                    
                                                                
   
                                                                          
                                                                           
                                                                        
                                                                          
          
   
                                                                           
                                                                     
                    
   
int64
index_getbitmap(IndexScanDesc scan, TIDBitmap *bitmap)
{
  int64 ntids;

  SCAN_CHECKS;
  CHECK_SCAN_PROCEDURE(amgetbitmap);

                                       
  scan->kill_prior_tuple = false;

     
                                                   
     
  ntids = scan->indexRelation->rd_indam->amgetbitmap(scan, bitmap);

  pgstat_count_index_tuples(scan->indexRelation, ntids);

  return ntids;
}

                    
                                                          
   
                                                              
                  
   
                                                              
                    
   
IndexBulkDeleteResult *
index_bulk_delete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  Relation indexRelation = info->index;

  RELATION_CHECKS;
  CHECK_REL_PROCEDURE(ambulkdelete);

  return indexRelation->rd_indam->ambulkdelete(info, stats, callback, callback_state);
}

                    
                                                                
   
                                                              
                    
   
IndexBulkDeleteResult *
index_vacuum_cleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
  Relation indexRelation = info->index;

  RELATION_CHECKS;
  CHECK_REL_PROCEDURE(amvacuumcleanup);

  return indexRelation->rd_indam->amvacuumcleanup(info, stats);
}

                    
                     
   
                                                                        
            
                    
   
bool
index_can_return(Relation indexRelation, int attno)
{
  RELATION_CHECKS;

                                                                   
  if (indexRelation->rd_indam->amcanreturn == NULL)
  {
    return false;
  }

  return indexRelation->rd_indam->amcanreturn(indexRelation, attno);
}

                    
                    
   
                                                                     
                                                                       
                                                                       
                                                                        
                                                                
                                                                    
                                                         
                                                 
   
                                                                   
                                                                      
                                                                        
                                                                    
                                                                     
                                                                         
                                                                      
                                                                           
               
   
                                                                   
                                  
                    
   
RegProcedure
index_getprocid(Relation irel, AttrNumber attnum, uint16 procnum)
{
  RegProcedure *loc;
  int nproc;
  int procindex;

  nproc = irel->rd_indam->amsupport;

  Assert(procnum > 0 && procnum <= (uint16)nproc);

  procindex = (nproc * (attnum - 1)) + (procnum - 1);

  loc = irel->rd_support;

  Assert(loc != NULL);

  return loc[procindex];
}

                    
                      
   
                                                               
                                                                 
                                                               
   
                                                                           
                                                                         
                                                                             
                    
   
FmgrInfo *
index_getprocinfo(Relation irel, AttrNumber attnum, uint16 procnum)
{
  FmgrInfo *locinfo;
  int nproc;
  int procindex;

  nproc = irel->rd_indam->amsupport;

  Assert(procnum > 0 && procnum <= (uint16)nproc);

  procindex = (nproc * (attnum - 1)) + (procnum - 1);

  locinfo = irel->rd_supportinfo;

  Assert(locinfo != NULL);

  locinfo += procindex;

                                                        
  if (locinfo->fn_oid == InvalidOid)
  {
    RegProcedure *loc = irel->rd_support;
    RegProcedure procId;

    Assert(loc != NULL);

    procId = loc[procindex];

       
                                                                         
                                                                     
                                                                          
                                                             
       
    if (!RegProcedureIsValid(procId))
    {
      elog(ERROR, "missing support function %d for attribute %d of index \"%s\"", procnum, attnum, RelationGetRelationName(irel));
    }

    fmgr_info_cxt(procId, locinfo, irel->rd_indexcxt);
  }

  return locinfo;
}

                    
                                         
   
                                                                 
                                                                        
                            
                    
   
void
index_store_float8_orderby_distances(IndexScanDesc scan, Oid *orderByTypes, IndexOrderByDistance *distances, bool recheckOrderBy)
{
  int i;

  Assert(distances || !recheckOrderBy);

  scan->xs_recheckorderby = recheckOrderBy;

  for (i = 0; i < scan->numberOfOrderBys; i++)
  {
    if (orderByTypes[i] == FLOAT8OID)
    {
#ifndef USE_FLOAT8_BYVAL
                                                           
      if (!scan->xs_orderbynulls[i])
      {
        pfree(DatumGetPointer(scan->xs_orderbyvals[i]));
      }
#endif
      if (distances && !distances[i].isnull)
      {
        scan->xs_orderbyvals[i] = Float8GetDatum(distances[i].value);
        scan->xs_orderbynulls[i] = false;
      }
      else
      {
        scan->xs_orderbyvals[i] = (Datum)0;
        scan->xs_orderbynulls[i] = true;
      }
    }
    else if (orderByTypes[i] == FLOAT4OID)
    {
                                                               
#ifndef USE_FLOAT4_BYVAL
                                                           
      if (!scan->xs_orderbynulls[i])
      {
        pfree(DatumGetPointer(scan->xs_orderbyvals[i]));
      }
#endif
      if (distances && !distances[i].isnull)
      {
        scan->xs_orderbyvals[i] = Float4GetDatum((float4)distances[i].value);
        scan->xs_orderbynulls[i] = false;
      }
      else
      {
        scan->xs_orderbyvals[i] = (Datum)0;
        scan->xs_orderbynulls[i] = true;
      }
    }
    else
    {
         
                                                                      
                                                                      
                                                                      
                                                                   
                                                                       
              
         
      if (scan->xs_recheckorderby)
      {
        elog(ERROR, "ORDER BY operator must return float8 or float4 if the distance function is lossy");
      }
      scan->xs_orderbynulls[i] = true;
    }
  }
}
