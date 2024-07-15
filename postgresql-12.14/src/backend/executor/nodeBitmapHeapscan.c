                                                                            
   
                        
                                                      
   
                                                                             
                                                                         
                                                                          
                                                                         
                                                                       
                                                                           
                                                                       
                                                                        
                                                                            
                                                                        
                                   
   
   
                                                                         
                                                                        
   
   
                  
                                               
   
                                                                            
   
   
                      
                                                            
                                             
                                                                
                                                          
                                                 
   
#include "postgres.h"

#include <math.h>

#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/visibilitymap.h"
#include "executor/execdebug.h"
#include "executor/nodeBitmapHeapscan.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/spccache.h"
#include "utils/snapmgr.h"

static TupleTableSlot *
BitmapHeapNext(BitmapHeapScanState *node);
static inline void
BitmapDoneInitializingSharedState(ParallelBitmapHeapState *pstate);
static inline void
BitmapAdjustPrefetchIterator(BitmapHeapScanState *node, TBMIterateResult *tbmres);
static inline void
BitmapAdjustPrefetchTarget(BitmapHeapScanState *node);
static inline void
BitmapPrefetch(BitmapHeapScanState *node, TableScanDesc scan);
static bool
BitmapShouldInitializeSharedState(ParallelBitmapHeapState *pstate);

                                                                    
                   
   
                                                                       
                                                                    
   
static TupleTableSlot *
BitmapHeapNext(BitmapHeapScanState *node)
{
  ExprContext *econtext;
  TableScanDesc scan;
  TIDBitmap *tbm;
  TBMIterator *tbmiterator = NULL;
  TBMSharedIterator *shared_tbmiterator = NULL;
  TBMIterateResult *tbmres;
  TupleTableSlot *slot;
  ParallelBitmapHeapState *pstate = node->pstate;
  dsa_area *dsa = node->ss.ps.state->es_query_dsa;

     
                                                        
     
  econtext = node->ss.ps.ps_ExprContext;
  slot = node->ss.ss_ScanTupleSlot;
  scan = node->ss.ss_currentScanDesc;
  tbm = node->tbm;
  if (pstate == NULL)
  {
    tbmiterator = node->tbmiterator;
  }
  else
  {
    shared_tbmiterator = node->shared_tbmiterator;
  }
  tbmres = node->tbmres;

     
                                                                             
                                    
     
                                                                       
                                                                    
                                                                            
                                                                       
                                                                           
                                                                             
                                                              
     
  if (!node->initialized)
  {
    if (!pstate)
    {
      tbm = (TIDBitmap *)MultiExecProcNode(outerPlanState(node));

      if (!tbm || !IsA(tbm, TIDBitmap))
      {
        elog(ERROR, "unrecognized result from subplan");
      }

      node->tbm = tbm;
      node->tbmiterator = tbmiterator = tbm_begin_iterate(tbm);
      node->tbmres = tbmres = NULL;

#ifdef USE_PREFETCH
      if (node->prefetch_maximum > 0)
      {
        node->prefetch_iterator = tbm_begin_iterate(tbm);
        node->prefetch_pages = 0;
        node->prefetch_target = -1;
      }
#endif                   
    }
    else
    {
         
                                                                   
                                                                         
                  
         
      if (BitmapShouldInitializeSharedState(pstate))
      {
        tbm = (TIDBitmap *)MultiExecProcNode(outerPlanState(node));
        if (!tbm || !IsA(tbm, TIDBitmap))
        {
          elog(ERROR, "unrecognized result from subplan");
        }

        node->tbm = tbm;

           
                                                                 
                                                                   
                                                  
           
        pstate->tbmiterator = tbm_prepare_shared_iterate(tbm);
#ifdef USE_PREFETCH
        if (node->prefetch_maximum > 0)
        {
          pstate->prefetch_iterator = tbm_prepare_shared_iterate(tbm);

             
                                                                    
                     
             
          pstate->prefetch_pages = 0;
          pstate->prefetch_target = -1;
        }
#endif

                                                                     
        BitmapDoneInitializingSharedState(pstate);
      }

                                                                         
      node->shared_tbmiterator = shared_tbmiterator = tbm_attach_shared_iterate(dsa, pstate->tbmiterator);
      node->tbmres = tbmres = NULL;

#ifdef USE_PREFETCH
      if (node->prefetch_maximum > 0)
      {
        node->shared_prefetch_iterator = tbm_attach_shared_iterate(dsa, pstate->prefetch_iterator);
      }
#endif                   
    }
    node->initialized = true;
  }

  for (;;)
  {
    bool skip_fetch;

    CHECK_FOR_INTERRUPTS();

       
                                          
       
    if (tbmres == NULL)
    {
      if (!pstate)
      {
        node->tbmres = tbmres = tbm_iterate(tbmiterator);
      }
      else
      {
        node->tbmres = tbmres = tbm_shared_iterate(shared_tbmiterator);
      }
      if (tbmres == NULL)
      {
                                           
        break;
      }

      BitmapAdjustPrefetchIterator(node, tbmres);

         
                                                                        
                                                                      
                                                                    
         
                                                                      
                                                                     
         
      skip_fetch = (node->can_skip_fetch && !tbmres->recheck && VM_ALL_VISIBLE(node->ss.ss_currentRelation, tbmres->blockno, &node->vmbuffer));

      if (skip_fetch)
      {
                                                   
        Assert(tbmres->ntuples >= 0);

           
                                                         
                                      
           
        node->return_empty_tuples = tbmres->ntuples;
      }
      else if (!table_scan_bitmap_next_block(scan, tbmres))
      {
                                                        
        continue;
      }

      if (tbmres->ntuples >= 0)
      {
        node->exact_pages++;
      }
      else
      {
        node->lossy_pages++;
      }

                                      
      BitmapAdjustPrefetchTarget(node);
    }
    else
    {
         
                                                 
         

#ifdef USE_PREFETCH

         
                                                                        
                                                                     
         
      if (!pstate)
      {
        if (node->prefetch_target < node->prefetch_maximum)
        {
          node->prefetch_target++;
        }
      }
      else if (pstate->prefetch_target < node->prefetch_maximum)
      {
                                                       
        SpinLockAcquire(&pstate->mutex);
        if (pstate->prefetch_target < node->prefetch_maximum)
        {
          pstate->prefetch_target++;
        }
        SpinLockRelease(&pstate->mutex);
      }
#endif                   
    }

       
                                                                           
                                                                           
                                                                           
                                                                          
                                                   
       
                                                                    
                                                                   
       
    BitmapPrefetch(node, scan);

    if (node->return_empty_tuples > 0)
    {
         
                                                                 
         
      ExecStoreAllNullTuple(slot);

      if (--node->return_empty_tuples == 0)
      {
                                                        
        node->tbmres = tbmres = NULL;
      }
    }
    else
    {
         
                                         
         
      if (!table_scan_bitmap_next_tuple(scan, tbmres, slot))
      {
                                                  
        node->tbmres = tbmres = NULL;
        continue;
      }

         
                                                                 
                                    
         
      if (tbmres->recheck)
      {
        econtext->ecxt_scantuple = slot;
        if (!ExecQualAndReset(node->bitmapqualorig, econtext))
        {
                                                                   
          InstrCountFiltered2(node, 1);
          ExecClearTuple(slot);
          continue;
        }
      }
    }

                                 
    return slot;
  }

     
                                                             
     
  return ExecClearTuple(slot);
}

   
                                                                   
   
                                                                             
                                            
   
static inline void
BitmapDoneInitializingSharedState(ParallelBitmapHeapState *pstate)
{
  SpinLockAcquire(&pstate->mutex);
  pstate->state = BM_FINISHED;
  SpinLockRelease(&pstate->mutex);
  ConditionVariableBroadcast(&pstate->cv);
}

   
                                                               
   
static inline void
BitmapAdjustPrefetchIterator(BitmapHeapScanState *node, TBMIterateResult *tbmres)
{
#ifdef USE_PREFETCH
  ParallelBitmapHeapState *pstate = node->pstate;

  if (pstate == NULL)
  {
    TBMIterator *prefetch_iterator = node->prefetch_iterator;

    if (node->prefetch_pages > 0)
    {
                                                                 
      node->prefetch_pages--;
    }
    else if (prefetch_iterator)
    {
                                                                    
      TBMIterateResult *tbmpre = tbm_iterate(prefetch_iterator);

      if (tbmpre == NULL || tbmpre->blockno != tbmres->blockno)
      {
        elog(ERROR, "prefetch and main iterators are out of sync");
      }
    }
    return;
  }

  if (node->prefetch_maximum > 0)
  {
    TBMSharedIterator *prefetch_iterator = node->shared_prefetch_iterator;

    SpinLockAcquire(&pstate->mutex);
    if (pstate->prefetch_pages > 0)
    {
      pstate->prefetch_pages--;
      SpinLockRelease(&pstate->mutex);
    }
    else
    {
                                              
      SpinLockRelease(&pstate->mutex);

         
                                                                    
                                                                        
                                                               
                                                                       
                                                                     
               
         
      if (prefetch_iterator)
      {
        tbm_shared_iterate(prefetch_iterator);
      }
    }
  }
#endif                   
}

   
                                                           
   
                                                                   
                                                             
                                                                   
                                          
   
static inline void
BitmapAdjustPrefetchTarget(BitmapHeapScanState *node)
{
#ifdef USE_PREFETCH
  ParallelBitmapHeapState *pstate = node->pstate;

  if (pstate == NULL)
  {
    if (node->prefetch_target >= node->prefetch_maximum)
                                      ;
    else if (node->prefetch_target >= node->prefetch_maximum / 2)
    {
      node->prefetch_target = node->prefetch_maximum;
    }
    else if (node->prefetch_target > 0)
    {
      node->prefetch_target *= 2;
    }
    else
    {
      node->prefetch_target++;
    }
    return;
  }

                                                                 
  if (pstate->prefetch_target < node->prefetch_maximum)
  {
    SpinLockAcquire(&pstate->mutex);
    if (pstate->prefetch_target >= node->prefetch_maximum)
                                      ;
    else if (pstate->prefetch_target >= node->prefetch_maximum / 2)
    {
      pstate->prefetch_target = node->prefetch_maximum;
    }
    else if (pstate->prefetch_target > 0)
    {
      pstate->prefetch_target *= 2;
    }
    else
    {
      pstate->prefetch_target++;
    }
    SpinLockRelease(&pstate->mutex);
  }
#endif                   
}

   
                                                                           
   
static inline void
BitmapPrefetch(BitmapHeapScanState *node, TableScanDesc scan)
{
#ifdef USE_PREFETCH
  ParallelBitmapHeapState *pstate = node->pstate;

  if (pstate == NULL)
  {
    TBMIterator *prefetch_iterator = node->prefetch_iterator;

    if (prefetch_iterator)
    {
      while (node->prefetch_pages < node->prefetch_target)
      {
        TBMIterateResult *tbmpre = tbm_iterate(prefetch_iterator);
        bool skip_fetch;

        if (tbmpre == NULL)
        {
                                         
          tbm_end_iterate(prefetch_iterator);
          node->prefetch_iterator = NULL;
          break;
        }
        node->prefetch_pages++;

           
                                                                     
                                                                     
                                                                 
                            
           
                                                                 
                                                                     
                                                                      
                                      
           
        skip_fetch = (node->can_skip_fetch && (node->tbmres ? !node->tbmres->recheck : false) && VM_ALL_VISIBLE(node->ss.ss_currentRelation, tbmpre->blockno, &node->pvmbuffer));

        if (!skip_fetch)
        {
          PrefetchBuffer(scan->rs_rd, MAIN_FORKNUM, tbmpre->blockno);
        }
      }
    }

    return;
  }

  if (pstate->prefetch_pages < pstate->prefetch_target)
  {
    TBMSharedIterator *prefetch_iterator = node->shared_prefetch_iterator;

    if (prefetch_iterator)
    {
      while (1)
      {
        TBMIterateResult *tbmpre;
        bool do_prefetch = false;
        bool skip_fetch;

           
                                                                      
                                                                    
           
        SpinLockAcquire(&pstate->mutex);
        if (pstate->prefetch_pages < pstate->prefetch_target)
        {
          pstate->prefetch_pages++;
          do_prefetch = true;
        }
        SpinLockRelease(&pstate->mutex);

        if (!do_prefetch)
        {
          return;
        }

        tbmpre = tbm_shared_iterate(prefetch_iterator);
        if (tbmpre == NULL)
        {
                                         
          tbm_end_shared_iterate(prefetch_iterator);
          node->shared_prefetch_iterator = NULL;
          break;
        }

                                                                   
        skip_fetch = (node->can_skip_fetch && (node->tbmres ? !node->tbmres->recheck : false) && VM_ALL_VISIBLE(node->ss.ss_currentRelation, tbmpre->blockno, &node->pvmbuffer));

        if (!skip_fetch)
        {
          PrefetchBuffer(scan->rs_rd, MAIN_FORKNUM, tbmpre->blockno);
        }
      }
    }
  }
#endif                   
}

   
                                                                                 
   
static bool
BitmapHeapRecheck(BitmapHeapScanState *node, TupleTableSlot *slot)
{
  ExprContext *econtext;

     
                                                        
     
  econtext = node->ss.ps.ps_ExprContext;

                                                         
  econtext->ecxt_scantuple = slot;
  return ExecQualAndReset(node->bitmapqualorig, econtext);
}

                                                                    
                             
                                                                    
   
static TupleTableSlot *
ExecBitmapHeapScan(PlanState *pstate)
{
  BitmapHeapScanState *node = castNode(BitmapHeapScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)BitmapHeapNext, (ExecScanRecheckMtd)BitmapHeapRecheck);
}

                                                                    
                                   
                                                                    
   
void
ExecReScanBitmapHeapScan(BitmapHeapScanState *node)
{
  PlanState *outerPlan = outerPlanState(node);

                                      
  table_rescan(node->ss.ss_currentScanDesc, NULL);

                                          
  if (node->tbmiterator)
  {
    tbm_end_iterate(node->tbmiterator);
  }
  if (node->prefetch_iterator)
  {
    tbm_end_iterate(node->prefetch_iterator);
  }
  if (node->shared_tbmiterator)
  {
    tbm_end_shared_iterate(node->shared_tbmiterator);
  }
  if (node->shared_prefetch_iterator)
  {
    tbm_end_shared_iterate(node->shared_prefetch_iterator);
  }
  if (node->tbm)
  {
    tbm_free(node->tbm);
  }
  if (node->vmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(node->vmbuffer);
  }
  if (node->pvmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(node->pvmbuffer);
  }
  node->tbm = NULL;
  node->tbmiterator = NULL;
  node->tbmres = NULL;
  node->prefetch_iterator = NULL;
  node->initialized = false;
  node->shared_tbmiterator = NULL;
  node->shared_prefetch_iterator = NULL;
  node->vmbuffer = InvalidBuffer;
  node->pvmbuffer = InvalidBuffer;

  ExecScanReScan(&node->ss);

     
                                                                        
                         
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }
}

                                                                    
                          
                                                                    
   
void
ExecEndBitmapHeapScan(BitmapHeapScanState *node)
{
  TableScanDesc scanDesc;

     
                                       
     
  scanDesc = node->ss.ss_currentScanDesc;

     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                                 
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                         
     
  ExecEndNode(outerPlanState(node));

     
                                        
     
  if (node->tbmiterator)
  {
    tbm_end_iterate(node->tbmiterator);
  }
  if (node->prefetch_iterator)
  {
    tbm_end_iterate(node->prefetch_iterator);
  }
  if (node->tbm)
  {
    tbm_free(node->tbm);
  }
  if (node->shared_tbmiterator)
  {
    tbm_end_shared_iterate(node->shared_tbmiterator);
  }
  if (node->shared_prefetch_iterator)
  {
    tbm_end_shared_iterate(node->shared_prefetch_iterator);
  }
  if (node->vmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(node->vmbuffer);
  }
  if (node->pvmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(node->pvmbuffer);
  }

     
                     
     
  table_endscan(scanDesc);
}

                                                                    
                           
   
                                              
                                                                    
   
BitmapHeapScanState *
ExecInitBitmapHeapScan(BitmapHeapScan *node, EState *estate, int eflags)
{
  BitmapHeapScanState *scanstate;
  Relation currentRelation;
  int io_concurrency;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                                                                         
                   
     
  Assert(IsMVCCSnapshot(estate->es_snapshot));

     
                            
     
  scanstate = makeNode(BitmapHeapScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecBitmapHeapScan;

  scanstate->tbm = NULL;
  scanstate->tbmiterator = NULL;
  scanstate->tbmres = NULL;
  scanstate->return_empty_tuples = 0;
  scanstate->vmbuffer = InvalidBuffer;
  scanstate->pvmbuffer = InvalidBuffer;
  scanstate->exact_pages = 0;
  scanstate->lossy_pages = 0;
  scanstate->prefetch_iterator = NULL;
  scanstate->prefetch_pages = 0;
  scanstate->prefetch_target = 0;
                            
  scanstate->prefetch_maximum = target_prefetch_pages;
  scanstate->pscan_len = 0;
  scanstate->initialized = false;
  scanstate->shared_tbmiterator = NULL;
  scanstate->shared_prefetch_iterator = NULL;
  scanstate->pstate = NULL;

     
                                                                       
                                                                          
                                                                      
                                                                             
                                                                  
     
  scanstate->can_skip_fetch = (node->scan.plan.qual == NIL && node->scan.plan.targetlist == NIL);

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                            
     
  currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid, eflags);

     
                            
     
  outerPlanState(scanstate) = ExecInitNode(outerPlan(node), estate, eflags);

     
                                                     
     
  ExecInitScanTupleSlot(estate, &scanstate->ss, RelationGetDescr(currentRelation), table_slot_callbacks(currentRelation));

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);
  scanstate->bitmapqualorig = ExecInitQual(node->bitmapqualorig, (PlanState *)scanstate);

     
                                                                         
                                                                        
                                                                            
                           
     
  io_concurrency = get_tablespace_io_concurrency(currentRelation->rd_rel->reltablespace);
  if (io_concurrency != effective_io_concurrency)
  {
    double maximum;

    if (ComputeIoConcurrency(io_concurrency, &maximum))
    {
      scanstate->prefetch_maximum = rint(maximum);
    }
  }

  scanstate->ss.ss_currentRelation = currentRelation;

  scanstate->ss.ss_currentScanDesc = table_beginscan_bm(currentRelation, estate->es_snapshot, 0, NULL);

     
               
     
  return scanstate;
}

                   
                                      
   
                                                                       
                                                                    
                                                                        
                                                                         
                   
   
static bool
BitmapShouldInitializeSharedState(ParallelBitmapHeapState *pstate)
{
  SharedBitmapState state;

  while (1)
  {
    SpinLockAcquire(&pstate->mutex);
    state = pstate->state;
    if (pstate->state == BM_INITIAL)
    {
      pstate->state = BM_INPROGRESS;
    }
    SpinLockRelease(&pstate->mutex);

                                                         
    if (state != BM_INPROGRESS)
    {
      break;
    }

                                            
    ConditionVariableSleep(&pstate->cv, WAIT_EVENT_PARALLEL_BITMAP_SCAN);
  }

  ConditionVariableCancelSleep();

  return (state == BM_INITIAL);
}

                                                                    
                           
   
                                                           
                                                           
                                                                    
   
void
ExecBitmapHeapEstimate(BitmapHeapScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;

  node->pscan_len = add_size(offsetof(ParallelBitmapHeapState, phs_snapshot_data), EstimateSnapshotSpace(estate->es_snapshot));

  shm_toc_estimate_chunk(&pcxt->estimator, node->pscan_len);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

                                                                    
                                
   
                                                   
                                                                    
   
void
ExecBitmapHeapInitializeDSM(BitmapHeapScanState *node, ParallelContext *pcxt)
{
  ParallelBitmapHeapState *pstate;
  EState *estate = node->ss.ps.state;
  dsa_area *dsa = node->ss.ps.state->es_query_dsa;

                                                                    
  if (dsa == NULL)
  {
    return;
  }

  pstate = shm_toc_allocate(pcxt->toc, node->pscan_len);

  pstate->tbmiterator = 0;
  pstate->prefetch_iterator = 0;

                            
  SpinLockInit(&pstate->mutex);
  pstate->prefetch_pages = 0;
  pstate->prefetch_target = 0;
  pstate->state = BM_INITIAL;

  ConditionVariableInit(&pstate->cv);
  SerializeSnapshot(estate->es_snapshot, pstate->phs_snapshot_data);

  shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, pstate);
  node->pstate = pstate;
}

                                                                    
                                  
   
                                                      
                                                                    
   
void
ExecBitmapHeapReInitializeDSM(BitmapHeapScanState *node, ParallelContext *pcxt)
{
  ParallelBitmapHeapState *pstate = node->pstate;
  dsa_area *dsa = node->ss.ps.state->es_query_dsa;

                                                            
  if (dsa == NULL)
  {
    return;
  }

  pstate->state = BM_INITIAL;

  if (DsaPointerIsValid(pstate->tbmiterator))
  {
    tbm_free_shared_area(dsa, pstate->tbmiterator);
  }

  if (DsaPointerIsValid(pstate->prefetch_iterator))
  {
    tbm_free_shared_area(dsa, pstate->prefetch_iterator);
  }

  pstate->tbmiterator = InvalidDsaPointer;
  pstate->prefetch_iterator = InvalidDsaPointer;
}

                                                                    
                                   
   
                                                       
                                                                    
   
void
ExecBitmapHeapInitializeWorker(BitmapHeapScanState *node, ParallelWorkerContext *pwcxt)
{
  ParallelBitmapHeapState *pstate;
  Snapshot snapshot;

  Assert(node->ss.ps.state->es_query_dsa != NULL);

  pstate = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, false);
  node->pstate = pstate;

  snapshot = RestoreSnapshot(pstate->phs_snapshot_data);
  table_scan_update_snapshot(node->ss.ss_currentScanDesc, snapshot);
}
