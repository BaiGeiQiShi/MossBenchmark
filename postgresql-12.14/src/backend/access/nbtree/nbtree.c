                                                                            
   
            
                                                                       
               
   
         
                                                            
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/nbtree.h"
#include "access/nbtxlog.h"
#include "access/relscan.h"
#include "access/xlog.h"
#include "commands/progress.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "nodes/execnodes.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/condition_variable.h"
#include "storage/indexfsm.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/memutils.h"

                                          
typedef struct
{
  IndexVacuumInfo *info;
  IndexBulkDeleteResult *stats;
  IndexBulkDeleteCallback callback;
  void *callback_state;
  BTCycleId cycleid;
  BlockNumber lastBlockVacuumed;                                      
  BlockNumber lastBlockLocked;                                           
  BlockNumber totFreePages;                                      
  TransactionId oldestBtpoXact;
  MemoryContext pagedelcontext;
} BTVacState;

   
                                                                       
   
                                                                             
                                 
   
                                                                             
                                                     
   
                                                                               
                                                                          
   
typedef enum
{
  BTPARALLEL_NOT_INITIALIZED,
  BTPARALLEL_ADVANCING,
  BTPARALLEL_IDLE,
  BTPARALLEL_DONE
} BTPS_State;

   
                                                                              
                      
   
typedef struct BTParallelScanDescData
{
  BlockNumber btps_scanPage;                                         
  BTPS_State btps_pageStatus;                                   
                                                                   
                                                                     
  int btps_arrayKeyCount;                                              
                                                                   
  slock_t btps_mutex;                                       
  ConditionVariable btps_cv;                                         
} BTParallelScanDescData;

typedef struct BTParallelScanDescData *BTParallelScanDesc;

static void
btvacuumscan(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state, BTCycleId cycleid);
static void
btvacuumpage(BTVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno);

   
                                                                               
                  
   
Datum
bthandler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = BTMaxStrategyNumber;
  amroutine->amsupport = BTNProcs;
  amroutine->amcanorder = true;
  amroutine->amcanorderbyop = false;
  amroutine->amcanbackward = true;
  amroutine->amcanunique = true;
  amroutine->amcanmulticol = true;
  amroutine->amoptionalkey = true;
  amroutine->amsearcharray = true;
  amroutine->amsearchnulls = true;
  amroutine->amstorage = false;
  amroutine->amclusterable = true;
  amroutine->ampredlocks = true;
  amroutine->amcanparallel = true;
  amroutine->amcaninclude = true;
  amroutine->amkeytype = InvalidOid;

  amroutine->ambuild = btbuild;
  amroutine->ambuildempty = btbuildempty;
  amroutine->aminsert = btinsert;
  amroutine->ambulkdelete = btbulkdelete;
  amroutine->amvacuumcleanup = btvacuumcleanup;
  amroutine->amcanreturn = btcanreturn;
  amroutine->amcostestimate = btcostestimate;
  amroutine->amoptions = btoptions;
  amroutine->amproperty = btproperty;
  amroutine->ambuildphasename = btbuildphasename;
  amroutine->amvalidate = btvalidate;
  amroutine->ambeginscan = btbeginscan;
  amroutine->amrescan = btrescan;
  amroutine->amgettuple = btgettuple;
  amroutine->amgetbitmap = btgetbitmap;
  amroutine->amendscan = btendscan;
  amroutine->ammarkpos = btmarkpos;
  amroutine->amrestrpos = btrestrpos;
  amroutine->amestimateparallelscan = btestimateparallelscan;
  amroutine->aminitparallelscan = btinitparallelscan;
  amroutine->amparallelrescan = btparallelrescan;

  PG_RETURN_POINTER(amroutine);
}

   
                                                                           
   
void
btbuildempty(Relation index)
{
  Page metapage;

                           
  metapage = (Page)palloc(BLCKSZ);
  _bt_initmetapage(metapage, P_NONE, 0);

     
                                                                            
                                                                           
                                                             
                                                                         
                                       
     
  PageSetChecksumInplace(metapage, BTREE_METAPAGE);
  smgrwrite(RelationGetSmgr(index), INIT_FORKNUM, BTREE_METAPAGE, (char *)metapage, true);
  log_newpage(&RelationGetSmgr(index)->smgr_rnode.node, INIT_FORKNUM, BTREE_METAPAGE, metapage, true);

     
                                                                           
                                                                        
                                                                      
     
  smgrimmedsync(RelationGetSmgr(index), INIT_FORKNUM);
}

   
                                                     
   
                                                                        
                                 
   
bool
btinsert(Relation rel, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  bool result;
  IndexTuple itup;

                               
  itup = index_form_tuple(RelationGetDescr(rel), values, isnull);
  itup->t_tid = *ht_ctid;

  result = _bt_doinsert(rel, itup, checkUnique, heapRel);

  pfree(itup);

  return result;
}

   
                                                   
   
bool
btgettuple(IndexScanDesc scan, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  bool res;

                                     
  scan->xs_recheck = false;

     
                                                                        
                                                                        
                             
     
  if (so->numArrayKeys && !BTScanPosIsValid(so->currPos))
  {
                                                      
    if (so->numArrayKeys < 0)
    {
      return false;
    }

    _bt_start_array_keys(scan, dir);
  }

                                                                      
  do
  {
       
                                                                         
                                                                      
                                                      
       
    if (!BTScanPosIsValid(so->currPos))
    {
      res = _bt_first(scan, dir);
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

         
                                
         
      res = _bt_next(scan, dir);
    }

                                           
    if (res)
    {
      break;
    }
                                                                   
  } while (so->numArrayKeys && _bt_advance_array_keys(scan, dir));

  return res;
}

   
                                                                        
   
int64
btgetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  int64 ntids = 0;
  ItemPointer heapTid;

     
                                                 
     
  if (so->numArrayKeys)
  {
                                                      
    if (so->numArrayKeys < 0)
    {
      return ntids;
    }

    _bt_start_array_keys(scan, ForwardScanDirection);
  }

                                                                      
  do
  {
                                      
    if (_bt_first(scan, ForwardScanDirection))
    {
                                                
      heapTid = &scan->xs_heaptid;
      tbm_add_tuples(tbm, heapTid, 1, false);
      ntids++;

      for (;;)
      {
           
                                                                       
                                    
           
        if (++so->currPos.itemIndex > so->currPos.lastItem)
        {
                                                 
          if (!_bt_next(scan, ForwardScanDirection))
          {
            break;
          }
        }

                                                  
        heapTid = &so->currPos.items[so->currPos.itemIndex].heapTid;
        tbm_add_tuples(tbm, heapTid, 1, false);
        ntids++;
      }
    }
                                                         
  } while (so->numArrayKeys && _bt_advance_array_keys(scan, ForwardScanDirection));

  return ntids;
}

   
                                                  
   
IndexScanDesc
btbeginscan(Relation rel, int nkeys, int norderbys)
{
  IndexScanDesc scan;
  BTScanOpaque so;

                                     
  Assert(norderbys == 0);

                    
  scan = RelationGetIndexScan(rel, nkeys, norderbys);

                                  
  so = (BTScanOpaque)palloc(sizeof(BTScanOpaqueData));
  BTScanPosInvalidate(so->currPos);
  BTScanPosInvalidate(so->markPos);
  if (scan->numberOfKeys > 0)
  {
    so->keyData = (ScanKey)palloc(scan->numberOfKeys * sizeof(ScanKeyData));
  }
  else
  {
    so->keyData = NULL;
  }

  so->arrayKeyData = NULL;                                   
  so->numArrayKeys = 0;
  so->arrayKeys = NULL;
  so->arrayContext = NULL;

  so->killedItems = NULL;                   
  so->numKilled = 0;

     
                                                                         
                                                                             
                                                                            
     
  so->currTuples = so->markTuples = NULL;

  scan->xs_itupdesc = RelationGetDescr(rel);

  scan->opaque = so;

  return scan;
}

   
                                          
   
void
btrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;

                                                                 
  if (BTScanPosIsValid(so->currPos))
  {
                                                                 
    if (so->numKilled > 0)
    {
      _bt_killitems(scan);
    }
    BTScanPosUnpinIfPinned(so->currPos);
    BTScanPosInvalidate(so->currPos);
  }

  so->markItemIndex = -1;
  so->arrayKeyCount = 0;
  BTScanPosUnpinIfPinned(so->markPos);
  BTScanPosInvalidate(so->markPos);

     
                                                                           
                                                                    
                                                                            
                                       
     
                                                                        
                                                                          
                                                                           
                                                                            
                                                                     
                                                                           
                                                                            
                                                                         
                                                           
     
  if (scan->xs_want_itup && so->currTuples == NULL)
  {
    so->currTuples = (char *)palloc(BLCKSZ * 2);
    so->markTuples = so->currTuples + BLCKSZ;
  }

     
                                                                            
                      
     
  if (scankey && scan->numberOfKeys > 0)
  {
    memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
  }
  so->numberOfKeys = 0;                                        

                                                                  
  _bt_preprocess_array_keys(scan);
}

   
                                    
   
void
btendscan(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;

                                                                 
  if (BTScanPosIsValid(so->currPos))
  {
                                                                 
    if (so->numKilled > 0)
    {
      _bt_killitems(scan);
    }
    BTScanPosUnpinIfPinned(so->currPos);
  }

  so->markItemIndex = -1;
  BTScanPosUnpinIfPinned(so->markPos);

                                                                      

                       
  if (so->keyData != NULL)
  {
    pfree(so->keyData);
  }
                                                              
  if (so->arrayContext != NULL)
  {
    MemoryContextDelete(so->arrayContext);
  }
  if (so->killedItems != NULL)
  {
    pfree(so->killedItems);
  }
  if (so->currTuples != NULL)
  {
    pfree(so->currTuples);
  }
                                                          
  pfree(so);
}

   
                                             
   
void
btmarkpos(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;

                                                          
  BTScanPosUnpinIfPinned(so->markPos);

     
                                                                       
                                                                             
                                                                             
                                                              
     
  if (BTScanPosIsValid(so->currPos))
  {
    so->markItemIndex = so->currPos.itemIndex;
  }
  else
  {
    BTScanPosInvalidate(so->markPos);
    so->markItemIndex = -1;
  }

                                                           
  if (so->numArrayKeys)
  {
    _bt_mark_array_keys(scan);
  }
}

   
                                                       
   
void
btrestrpos(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;

                                                      
  if (so->numArrayKeys)
  {
    _bt_restore_array_keys(scan);
  }

  if (so->markItemIndex >= 0)
  {
       
                                                                         
                              
       
                                                                        
                 
       
    so->currPos.itemIndex = so->markItemIndex;
  }
  else
  {
       
                                                                           
                                                                     
                                                                           
                        
       
    if (BTScanPosIsValid(so->currPos))
    {
                                                                   
      if (so->numKilled > 0)
      {
        _bt_killitems(scan);
      }
      BTScanPosUnpinIfPinned(so->currPos);
    }

    if (BTScanPosIsValid(so->markPos))
    {
                                                                    
      if (BTScanPosIsPinned(so->markPos))
      {
        IncrBufferRefCount(so->markPos.buf);
      }
      memcpy(&so->currPos, &so->markPos, offsetof(BTScanPosData, items[1]) + so->markPos.lastItem * sizeof(BTScanPosItem));
      if (so->currTuples)
      {
        memcpy(so->currTuples, so->markTuples, so->markPos.nextTupleOffset);
      }
    }
    else
    {
      BTScanPosInvalidate(so->currPos);
    }
  }
}

   
                                                                         
   
Size
btestimateparallelscan(void)
{
  return sizeof(BTParallelScanDescData);
}

   
                                                                               
   
void
btinitparallelscan(void *target)
{
  BTParallelScanDesc bt_target = (BTParallelScanDesc)target;

  SpinLockInit(&bt_target->btps_mutex);
  bt_target->btps_scanPage = InvalidBlockNumber;
  bt_target->btps_pageStatus = BTPARALLEL_NOT_INITIALIZED;
  bt_target->btps_arrayKeyCount = 0;
  ConditionVariableInit(&bt_target->btps_cv);
}

   
                                             
   
void
btparallelrescan(IndexScanDesc scan)
{
  BTParallelScanDesc btscan;
  ParallelIndexScanDesc parallel_scan = scan->parallel_scan;

  Assert(parallel_scan);

  btscan = (BTParallelScanDesc)OffsetToPointer((void *)parallel_scan, parallel_scan->ps_offset);

     
                                                                          
                                                                            
                  
     
  SpinLockAcquire(&btscan->btps_mutex);
  btscan->btps_scanPage = InvalidBlockNumber;
  btscan->btps_pageStatus = BTPARALLEL_NOT_INITIALIZED;
  btscan->btps_arrayKeyCount = 0;
  SpinLockRelease(&btscan->btps_mutex);
}

   
                                                                            
                                                                      
                            
   
                                                                         
                                                                             
                    
   
                                                                         
                                                                           
                                                                              
                                                                                
                                                                             
                           
   
                                                                           
   
bool
_bt_parallel_seize(IndexScanDesc scan, BlockNumber *pageno)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  BTPS_State pageStatus;
  bool exit_loop = false;
  bool status = true;
  ParallelIndexScanDesc parallel_scan = scan->parallel_scan;
  BTParallelScanDesc btscan;

  *pageno = P_NONE;

  btscan = (BTParallelScanDesc)OffsetToPointer((void *)parallel_scan, parallel_scan->ps_offset);

  while (1)
  {
    SpinLockAcquire(&btscan->btps_mutex);
    pageStatus = btscan->btps_pageStatus;

    if (so->arrayKeyCount < btscan->btps_arrayKeyCount)
    {
                                                                        
      status = false;
    }
    else if (pageStatus == BTPARALLEL_DONE)
    {
         
                                                                        
                                          
         
      status = false;
    }
    else if (pageStatus != BTPARALLEL_ADVANCING)
    {
         
                                                                         
                                        
         
      btscan->btps_pageStatus = BTPARALLEL_ADVANCING;
      *pageno = btscan->btps_scanPage;
      exit_loop = true;
    }
    SpinLockRelease(&btscan->btps_mutex);
    if (exit_loop || !status)
    {
      break;
    }
    ConditionVariableSleep(&btscan->btps_cv, WAIT_EVENT_BTREE_PAGE);
  }
  ConditionVariableCancelSleep();

  return status;
}

   
                                                                             
                                                                           
                                      
   
void
_bt_parallel_release(IndexScanDesc scan, BlockNumber scan_page)
{
  ParallelIndexScanDesc parallel_scan = scan->parallel_scan;
  BTParallelScanDesc btscan;

  btscan = (BTParallelScanDesc)OffsetToPointer((void *)parallel_scan, parallel_scan->ps_offset);

  SpinLockAcquire(&btscan->btps_mutex);
  btscan->btps_scanPage = scan_page;
  btscan->btps_pageStatus = BTPARALLEL_IDLE;
  SpinLockRelease(&btscan->btps_mutex);
  ConditionVariableSignal(&btscan->btps_cv);
}

   
                                                              
   
                                                                           
                                                                             
                             
   
void
_bt_parallel_done(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  ParallelIndexScanDesc parallel_scan = scan->parallel_scan;
  BTParallelScanDesc btscan;
  bool status_changed = false;

                                          
  if (parallel_scan == NULL)
  {
    return;
  }

  btscan = (BTParallelScanDesc)OffsetToPointer((void *)parallel_scan, parallel_scan->ps_offset);

     
                                                                       
                                                         
                             
     
  SpinLockAcquire(&btscan->btps_mutex);
  if (so->arrayKeyCount >= btscan->btps_arrayKeyCount && btscan->btps_pageStatus != BTPARALLEL_DONE)
  {
    btscan->btps_pageStatus = BTPARALLEL_DONE;
    status_changed = true;
  }
  SpinLockRelease(&btscan->btps_mutex);

                                                                  
  if (status_changed)
  {
    ConditionVariableBroadcast(&btscan->btps_cv);
  }
}

   
                                                                             
           
   
                                                                         
          
   
void
_bt_parallel_advance_array_keys(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  ParallelIndexScanDesc parallel_scan = scan->parallel_scan;
  BTParallelScanDesc btscan;

  btscan = (BTParallelScanDesc)OffsetToPointer((void *)parallel_scan, parallel_scan->ps_offset);

  so->arrayKeyCount++;
  SpinLockAcquire(&btscan->btps_mutex);
  if (btscan->btps_pageStatus == BTPARALLEL_DONE)
  {
    btscan->btps_scanPage = InvalidBlockNumber;
    btscan->btps_pageStatus = BTPARALLEL_NOT_INITIALIZED;
    btscan->btps_arrayKeyCount++;
  }
  SpinLockRelease(&btscan->btps_mutex);
}

   
                                                               
   
                                                                           
                              
   
                                                                       
                                                                           
                                                                   
   
static bool
_bt_vacuum_needs_cleanup(IndexVacuumInfo *info)
{
  Buffer metabuf;
  Page metapg;
  BTMetaPageData *metad;
  bool result = false;

  metabuf = _bt_getbuf(info->index, BTREE_METAPAGE, BT_READ);
  metapg = BufferGetPage(metabuf);
  metad = BTPageGetMeta(metapg);

  if (metad->btm_version < BTREE_NOVAC_VERSION)
  {
       
                                                                   
                                             
       
    result = true;
  }
  else if (TransactionIdIsValid(metad->btm_oldest_btpo_xact) && TransactionIdPrecedes(metad->btm_oldest_btpo_xact, RecentGlobalXmin))
  {
       
                                                                           
                                                                          
                                          
       
    result = true;
  }
  else
  {
    StdRdOptions *relopts;
    float8 cleanup_scale_factor;
    float8 prev_num_heap_tuples;

       
                                                                         
                                                                          
                                                                          
                                                                    
                              
       
    relopts = (StdRdOptions *)info->index->rd_options;
    cleanup_scale_factor = (relopts && relopts->vacuum_cleanup_index_scale_factor >= 0) ? relopts->vacuum_cleanup_index_scale_factor : vacuum_cleanup_index_scale_factor;
    prev_num_heap_tuples = metad->btm_last_cleanup_num_heap_tuples;

    if (cleanup_scale_factor <= 0 || prev_num_heap_tuples <= 0 || (info->num_heap_tuples - prev_num_heap_tuples) / prev_num_heap_tuples >= cleanup_scale_factor)
    {
      result = true;
    }
  }

  _bt_relbuf(info->index, metabuf);
  return result;
}

   
                                                                        
                                                                           
                                                                              
   
                                                                              
   
IndexBulkDeleteResult *
btbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
  Relation rel = info->index;
  BTCycleId cycleid;

                                                                         
  if (stats == NULL)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
  }

                                                          
                                                                     
  PG_ENSURE_ERROR_CLEANUP(_bt_end_vacuum_callback, PointerGetDatum(rel));
  {
    cycleid = _bt_start_vacuum(rel);

    btvacuumscan(info, stats, callback, callback_state, cycleid);
  }
  PG_END_ENSURE_ERROR_CLEANUP(_bt_end_vacuum_callback, PointerGetDatum(rel));
  _bt_end_vacuum(rel);

  return stats;
}

   
                        
   
                                                                              
   
IndexBulkDeleteResult *
btvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
                                  
  if (info->analyze_only)
  {
    return stats;
  }

     
                                                                          
                                                                             
                                                                             
                                                                    
                                      
     
                                                                         
                                                         
     
  if (stats == NULL)
  {
                                    
    if (!_bt_vacuum_needs_cleanup(info))
    {
      return NULL;
    }

    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
    btvacuumscan(info, stats, NULL, NULL, 0);
  }

     
                                                                            
                                                                             
                                                                            
                                         
     
  if (!info->estimated_count)
  {
    if (stats->num_index_tuples > info->num_heap_tuples)
    {
      stats->num_index_tuples = info->num_heap_tuples;
    }
  }

  return stats;
}

   
                                                          
   
                                                                             
                                                                         
                                                                          
                                                                       
                                                                           
                                                                              
                  
   
                                                                             
                                                     
   
static void
btvacuumscan(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state, BTCycleId cycleid)
{
  Relation rel = info->index;
  BTVacState vstate;
  BlockNumber num_pages;
  BlockNumber blkno;
  bool needLock;

     
                                                                           
                                                      
     
  stats->estimated_count = false;
  stats->num_index_tuples = 0;
  stats->pages_deleted = 0;

                                                
  vstate.info = info;
  vstate.stats = stats;
  vstate.callback = callback;
  vstate.callback_state = callback_state;
  vstate.cycleid = cycleid;
  vstate.lastBlockVacuumed = BTREE_METAPAGE;                                
  vstate.lastBlockLocked = BTREE_METAPAGE;
  vstate.totFreePages = 0;
  vstate.oldestBtpoXact = InvalidTransactionId;

                                                               
  vstate.pagedelcontext = AllocSetContextCreate(CurrentMemoryContext, "_bt_pagedel", ALLOCSET_DEFAULT_SIZES);

     
                                                                          
                                                                    
                                                                          
                                                                         
                                                                        
                                                                         
                                                                          
                                                                      
                                                                           
                                                                           
                                                                        
                                                                         
                                                                           
                                                                           
                                                                          
                                                                         
                                                                             
                                              
     
                                                                          
                                   
     
  needLock = !RELATION_IS_LOCAL(rel);

  blkno = BTREE_METAPAGE + 1;
  for (;;)
  {
                                         
    if (needLock)
    {
      LockRelationForExtension(rel, ExclusiveLock);
    }
    num_pages = RelationGetNumberOfBlocks(rel);
    if (needLock)
    {
      UnlockRelationForExtension(rel, ExclusiveLock);
    }

    if (info->report_progress)
    {
      pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_TOTAL, num_pages);
    }

                                                  
    if (blkno >= num_pages)
    {
      break;
    }
                                                              
    for (; blkno < num_pages; blkno++)
    {
      btvacuumpage(&vstate, blkno, blkno);
      if (info->report_progress)
      {
        pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE, blkno);
      }
    }
  }

     
                                                                           
                                                                             
                                   
     
                                                                            
                                                                           
                                                                            
                                                                             
                                                                            
                                                                           
                                                                      
                                                                           
     
  if (XLogStandbyInfoActive() && vstate.lastBlockVacuumed < vstate.lastBlockLocked)
  {
    Buffer buf;

       
                                                                          
                                                                         
                                                                         
                                                     
       
    buf = ReadBufferExtended(rel, MAIN_FORKNUM, vstate.lastBlockLocked, RBM_NORMAL, info->strategy);
    LockBufferForCleanup(buf);
    _bt_checkpage(rel, buf);
    _bt_delitems_vacuum(rel, buf, NULL, 0, vstate.lastBlockVacuumed);
    _bt_relbuf(rel, buf);
  }

  MemoryContextDelete(vstate.pagedelcontext);

     
                                                                           
                                                                            
                                                                     
                                                                          
                                                                       
                                                                      
                 
     
                                                                           
                 
     
  if (vstate.totFreePages > 0)
  {
    IndexFreeSpaceMapVacuum(rel);
  }

     
                                                                             
                                                                           
     
                                                                            
                                                                          
                                                                            
                                                                            
                                                                        
                                                                           
                                        
     
  _bt_update_meta_cleanup_info(rel, vstate.oldestBtpoXact, info->num_heap_tuples);

                         
  stats->num_pages = num_pages;
  stats->pages_free = vstate.totFreePages;
}

   
                                    
   
                                                                      
                                                                      
                                                
   
                                                                         
                                                                        
                                                 
   
static void
btvacuumpage(BTVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno)
{
  IndexVacuumInfo *info = vstate->info;
  IndexBulkDeleteResult *stats = vstate->stats;
  IndexBulkDeleteCallback callback = vstate->callback;
  void *callback_state = vstate->callback_state;
  Relation rel = info->index;
  bool delete_now;
  BlockNumber recurse_to;
  Buffer buf;
  Page page;
  BTPageOpaque opaque = NULL;

restart:
  delete_now = false;
  recurse_to = P_NONE;

                                                                 
  vacuum_delay_point();

     
                                                              
                                                                      
                                                                          
                             
     
  buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_NORMAL, info->strategy);
  LockBuffer(buf, BT_READ);
  page = BufferGetPage(buf);
  if (!PageIsNew(page))
  {
    _bt_checkpage(rel, buf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  }

     
                                                                         
                                                                         
                                                                      
     
  if (blkno != orig_blkno)
  {
    if (_bt_page_recyclable(page) || P_IGNORE(opaque) || !P_ISLEAF(opaque) || opaque->btpo_cycleid != vstate->cycleid)
    {
      _bt_relbuf(rel, buf);
      return;
    }
  }

                                             
  if (_bt_page_recyclable(page))
  {
                                                                     
    RecordFreeIndexPage(rel, blkno);
    vstate->totFreePages++;
    stats->pages_deleted++;
  }
  else if (P_ISDELETED(opaque))
  {
       
                                                                      
                    
       
    stats->pages_deleted++;

                                       
    if (!TransactionIdIsValid(vstate->oldestBtpoXact) || TransactionIdPrecedes(opaque->btpo.xact, vstate->oldestBtpoXact))
    {
      vstate->oldestBtpoXact = opaque->btpo.xact;
    }
  }
  else if (P_ISHALFDEAD(opaque))
  {
       
                                                              
                                               
       
    delete_now = true;
  }
  else if (P_ISLEAF(opaque))
  {
    OffsetNumber deletable[MaxOffsetNumber];
    int ndeletable;
    OffsetNumber offnum, minoff, maxoff;

       
                                                                          
                                                                       
                                                                          
                                               
       
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    LockBufferForCleanup(buf);

       
                                                                          
                             
       
    if (blkno > vstate->lastBlockLocked)
    {
      vstate->lastBlockLocked = blkno;
    }

       
                                                                        
                                                                          
                                                                        
                                                                         
                                                                   
       
    if (vstate->cycleid != 0 && opaque->btpo_cycleid == vstate->cycleid && !(opaque->btpo_flags & BTP_SPLIT_END) && !P_RIGHTMOST(opaque) && opaque->btpo_next < orig_blkno)
    {
      recurse_to = opaque->btpo_next;
    }

       
                                                                           
                          
       
    ndeletable = 0;
    minoff = P_FIRSTDATAKEY(opaque);
    maxoff = PageGetMaxOffsetNumber(page);
    if (callback)
    {
      for (offnum = minoff; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
      {
        IndexTuple itup;
        ItemPointer htup;

        itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offnum));
        htup = &(itup->t_tid);

           
                                                       
                                                                       
                                                                   
                                                                      
                                                                     
                                                                      
                                                                      
                                                                   
                                                               
                                                                     
                                                                   
                                                                   
                                                                      
                                                                    
                                                                 
                                                       
                                                                   
                                                                     
                   
           
        if (callback(htup, callback_state))
        {
          deletable[ndeletable++] = offnum;
        }
      }
    }

       
                                                                          
                                                     
       
    if (ndeletable > 0)
    {
         
                                                                      
                                                                         
                                                                      
                                                                         
                                                                 
                                                                       
                                                                       
         
                                                                    
                                                                      
                                                                         
               
         
      _bt_delitems_vacuum(rel, buf, deletable, ndeletable, vstate->lastBlockVacuumed);

         
                                                          
                                           
         
      if (blkno > vstate->lastBlockVacuumed)
      {
        vstate->lastBlockVacuumed = blkno;
      }

      stats->tuples_removed += ndeletable;
                                 
      maxoff = PageGetMaxOffsetNumber(page);
    }
    else
    {
         
                                                                      
                                                                        
                                                                         
                                                                      
                
         
                                                                         
                     
         
      if (vstate->cycleid != 0 && opaque->btpo_cycleid == vstate->cycleid)
      {
        opaque->btpo_cycleid = 0;
        MarkBufferDirtyHint(buf, true);
      }
    }

       
                                                                       
                                                                      
                                                                           
                                       
       
    if (minoff > maxoff)
    {
      delete_now = (blkno == orig_blkno);
    }
    else
    {
      stats->num_index_tuples += maxoff - minoff + 1;
    }
  }

  if (delete_now)
  {
    MemoryContext oldcontext;

                                                               
    MemoryContextReset(vstate->pagedelcontext);
    oldcontext = MemoryContextSwitchTo(vstate->pagedelcontext);

       
                                                                         
                                                                         
                                                 
       
    stats->pages_deleted += _bt_pagedel(rel, buf, &vstate->oldestBtpoXact);

    MemoryContextSwitchTo(oldcontext);
                                                  
  }
  else
  {
    _bt_relbuf(rel, buf);
  }

     
                                                                         
                                                                          
                                                                            
                                                                            
                                                    
     
  if (recurse_to != P_NONE)
  {
    blkno = recurse_to;
    goto restart;
  }
}

   
                                                                          
   
                                         
   
bool
btcanreturn(Relation index, int attno)
{
  return true;
}
