                                                                            
   
                       
                                          
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   
   
                      
                                       
                                         
                                                               
                                                           
                                                
                                               
                                                   
                                                             
                                 
                                                               
                        
                                                                     
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/tupdesc.h"
#include "access/visibilitymap.h"
#include "executor/execdebug.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeIndexscan.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
IndexOnlyNext(IndexOnlyScanState *node);
static void
StoreIndexTuple(TupleTableSlot *slot, IndexTuple itup, TupleDesc itupdesc);

                                                                    
                  
   
                                                          
                                                                    
   
static TupleTableSlot *
IndexOnlyNext(IndexOnlyScanState *node)
{
  EState *estate;
  ExprContext *econtext;
  ScanDirection direction;
  IndexScanDesc scandesc;
  TupleTableSlot *slot;
  ItemPointer tid;

     
                                                        
     
  estate = node->ss.ps.state;
  direction = estate->es_direction;
                                                          
  if (ScanDirectionIsBackward(((IndexOnlyScan *)node->ss.ps.plan)->indexorderdir))
  {
    if (ScanDirectionIsForward(direction))
    {
      direction = BackwardScanDirection;
    }
    else if (ScanDirectionIsBackward(direction))
    {
      direction = ForwardScanDirection;
    }
  }
  scandesc = node->ioss_ScanDesc;
  econtext = node->ss.ps.ps_ExprContext;
  slot = node->ss.ss_ScanTupleSlot;

  if (scandesc == NULL)
  {
       
                                                                         
                                                                    
                 
       
    scandesc = index_beginscan(node->ss.ss_currentRelation, node->ioss_RelationDesc, estate->es_snapshot, node->ioss_NumScanKeys, node->ioss_NumOrderByKeys);

    node->ioss_ScanDesc = scandesc;

                                       
    node->ioss_ScanDesc->xs_want_itup = true;
    node->ioss_VMBuffer = InvalidBuffer;

       
                                                                        
                                          
       
    if (node->ioss_NumRuntimeKeys == 0 || node->ioss_RuntimeKeysReady)
    {
      index_rescan(scandesc, node->ioss_ScanKeys, node->ioss_NumScanKeys, node->ioss_OrderByKeys, node->ioss_NumOrderByKeys);
    }
  }

     
                                                              
     
  while ((tid = index_getnext_tid(scandesc, direction)) != NULL)
  {
    bool tuple_from_heap = false;

    CHECK_FOR_INTERRUPTS();

       
                                                                       
                                                                      
                                                                        
       
                                                                          
                                                                        
                                                                           
               
       
                                                                        
                                                                           
                                                                         
                                                                       
                                                                         
                                                                          
                                                                        
                                                      
       
                                                                        
                                                                  
                                                                       
                                                                          
                                                                   
                                                              
                                                                         
                                                                         
                                                                
                                                                       
                                                                
       
                                                                         
                                                                
       
    if (!VM_ALL_VISIBLE(scandesc->heapRelation, ItemPointerGetBlockNumber(tid), &node->ioss_VMBuffer))
    {
         
                                                              
         
      InstrCountTuples2(node, 1);
      if (!index_fetch_heap(scandesc, node->ioss_TableSlot))
      {
        continue;                                             
      }

      ExecClearTuple(node->ioss_TableSlot);

         
                                                                       
                                                                       
                                                                      
                                                                         
         
      if (scandesc->xs_heap_continue)
      {
        elog(ERROR, "non-MVCC snapshots are not supported in index-only scans");
      }

         
                                                                       
                                                                        
                                                                         
                                                            
         

      tuple_from_heap = true;
    }

       
                                                                         
                                                                          
                                                                         
                                                                      
       
    if (scandesc->xs_hitup)
    {
         
                                                                         
                                                                     
                                        
         
      Assert(slot->tts_tupleDescriptor->natts == scandesc->xs_hitupdesc->natts);
      ExecForceStoreHeapTuple(scandesc->xs_hitup, slot, false);
    }
    else if (scandesc->xs_itup)
    {
      StoreIndexTuple(slot, scandesc->xs_itup, scandesc->xs_itupdesc);
    }
    else
    {
      elog(ERROR, "no data returned for index-only scan");
    }

       
                                                                   
       
    if (scandesc->xs_recheck)
    {
      econtext->ecxt_scantuple = slot;
      if (!ExecQualAndReset(node->recheckqual, econtext))
      {
                                                                 
        InstrCountFiltered2(node, 1);
        continue;
      }
    }

       
                                                                      
                                                                       
                                                                     
                                                                     
                                                                           
                                            
       
    if (scandesc->numberOfOrderBys > 0 && scandesc->xs_recheckorderby)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("lossy distance functions are not supported in index-only scans")));
    }

       
                                                                         
                                                                         
       
    if (!tuple_from_heap)
    {
      PredicateLockPage(scandesc->heapRelation, ItemPointerGetBlockNumber(tid), estate->es_snapshot);
    }

    return slot;
  }

     
                                                                           
                
     
  return ExecClearTuple(slot);
}

   
                   
                                                  
   
                                                                   
                                         
   
static void
StoreIndexTuple(TupleTableSlot *slot, IndexTuple itup, TupleDesc itupdesc)
{
     
                                                                             
                                                                        
                                                                          
                                                                             
                                                  
     
  Assert(slot->tts_tupleDescriptor->natts == itupdesc->natts);

  ExecClearTuple(slot);
  index_deform_tuple(itup, itupdesc, slot->tts_values, slot->tts_isnull);
  ExecStoreVirtualTuple(slot);
}

   
                                                                                
   
                                                                          
                                                                            
                                                                        
                                                 
   
static bool
IndexOnlyRecheck(IndexOnlyScanState *node, TupleTableSlot *slot)
{
  elog(ERROR, "EvalPlanQual recheck is not supported in index-only scans");
  return false;                          
}

                                                                    
                            
                                                                    
   
static TupleTableSlot *
ExecIndexOnlyScan(PlanState *pstate)
{
  IndexOnlyScanState *node = castNode(IndexOnlyScanState, pstate);

     
                                                                             
     
  if (node->ioss_NumRuntimeKeys != 0 && !node->ioss_RuntimeKeysReady)
  {
    ExecReScan((PlanState *)node);
  }

  return ExecScan(&node->ss, (ExecScanAccessMtd)IndexOnlyNext, (ExecScanRecheckMtd)IndexOnlyRecheck);
}

                                                                    
                                  
   
                                                                    
                                                                     
   
                                                          
                                                              
                                                                   
                                                                    
   
void
ExecReScanIndexOnlyScan(IndexOnlyScanState *node)
{
     
                                                                        
                                                                            
                                                                      
                                                                             
                   
     
  if (node->ioss_NumRuntimeKeys != 0)
  {
    ExprContext *econtext = node->ioss_RuntimeContext;

    ResetExprContext(econtext);
    ExecIndexEvalRuntimeKeys(econtext, node->ioss_RuntimeKeys, node->ioss_NumRuntimeKeys);
  }
  node->ioss_RuntimeKeysReady = true;

                        
  if (node->ioss_ScanDesc)
  {
    index_rescan(node->ioss_ScanDesc, node->ioss_ScanKeys, node->ioss_NumScanKeys, node->ioss_OrderByKeys, node->ioss_NumOrderByKeys);
  }

  ExecScanReScan(&node->ss);
}

                                                                    
                         
                                                                    
   
void
ExecEndIndexOnlyScan(IndexOnlyScanState *node)
{
  Relation indexRelationDesc;
  IndexScanDesc indexScanDesc;

     
                                       
     
  indexRelationDesc = node->ioss_RelationDesc;
  indexScanDesc = node->ioss_ScanDesc;

                                      
  if (node->ioss_VMBuffer != InvalidBuffer)
  {
    ReleaseBuffer(node->ioss_VMBuffer);
    node->ioss_VMBuffer = InvalidBuffer;
  }

     
                                                                        
     
#ifdef NOT_USED
  ExecFreeExprContext(&node->ss.ps);
  if (node->ioss_RuntimeContext)
  {
    FreeExprContext(node->ioss_RuntimeContext, true);
  }
#endif

     
                                 
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                                           
     
  if (indexScanDesc)
  {
    index_endscan(indexScanDesc);
  }
  if (indexRelationDesc)
  {
    index_close(indexRelationDesc, NoLock);
  }
}

                                                                    
                         
   
                                                                            
                                                                      
                                                                    
   
void
ExecIndexOnlyMarkPos(IndexOnlyScanState *node)
{
  EState *estate = node->ss.ps.state;
  EPQState *epqstate = estate->es_epq_active;

  if (epqstate != NULL)
  {
       
                                                                          
                                                                           
                                                                 
                                                                          
                                                                       
                                                                    
                                                   
       
    Index scanrelid = ((Scan *)node->ss.ps.plan)->scanrelid;

    Assert(scanrelid > 0);
    if (epqstate->relsubs_slot[scanrelid - 1] != NULL || epqstate->relsubs_rowmark[scanrelid - 1] != NULL)
    {
                                  
      if (!epqstate->relsubs_done[scanrelid - 1])
      {
        elog(ERROR, "unexpected ExecIndexOnlyMarkPos call in EPQ recheck");
      }
      return;
    }
  }

  index_markpos(node->ioss_ScanDesc);
}

                                                                    
                          
                                                                    
   
void
ExecIndexOnlyRestrPos(IndexOnlyScanState *node)
{
  EState *estate = node->ss.ps.state;
  EPQState *epqstate = estate->es_epq_active;

  if (estate->es_epq_active != NULL)
  {
                                          
    Index scanrelid = ((Scan *)node->ss.ps.plan)->scanrelid;

    Assert(scanrelid > 0);
    if (epqstate->relsubs_slot[scanrelid - 1] != NULL || epqstate->relsubs_rowmark[scanrelid - 1] != NULL)
    {
                                  
      if (!epqstate->relsubs_done[scanrelid - 1])
      {
        elog(ERROR, "unexpected ExecIndexOnlyRestrPos call in EPQ recheck");
      }
      return;
    }
  }

  index_restrpos(node->ioss_ScanDesc);
}

                                                                    
                          
   
                                                            
                                                       
   
                                                               
                                                          
                       
                                                                    
   
IndexOnlyScanState *
ExecInitIndexOnlyScan(IndexOnlyScan *node, EState *estate, int eflags)
{
  IndexOnlyScanState *indexstate;
  Relation currentRelation;
  LOCKMODE lockmode;
  TupleDesc tupDesc;

     
                            
     
  indexstate = makeNode(IndexOnlyScanState);
  indexstate->ss.ps.plan = (Plan *)node;
  indexstate->ss.ps.state = estate;
  indexstate->ss.ps.ExecProcNode = ExecIndexOnlyScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &indexstate->ss.ps);

     
                            
     
  currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid, eflags);

  indexstate->ss.ss_currentRelation = currentRelation;
  indexstate->ss.ss_currentScanDesc = NULL;                        

     
                                                                     
                                                                   
                                                                          
                                                                            
                            
     
  tupDesc = ExecTypeFromTL(node->indextlist);
  ExecInitScanTupleSlot(estate, &indexstate->ss, tupDesc, &TTSOpsVirtual);

     
                                                                             
                                                                             
     
  indexstate->ioss_TableSlot = ExecAllocTableSlot(&estate->es_tupleTable, RelationGetDescr(currentRelation), table_slot_callbacks(currentRelation));

     
                                                                             
                                                                      
     
  ExecInitResultTypeTL(&indexstate->ss.ps);
  ExecAssignScanProjectionInfoWithVarno(&indexstate->ss, INDEX_VAR);

     
                                  
     
                                                                            
                                                          
     
  indexstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)indexstate);
  indexstate->recheckqual = ExecInitQual(node->recheckqual, (PlanState *)indexstate);

     
                                                                           
                                                                             
                                        
     
  if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
  {
    return indexstate;
  }

                                
  lockmode = exec_rt_fetch(node->scan.scanrelid, estate)->rellockmode;
  indexstate->ioss_RelationDesc = index_open(node->indexid, lockmode);

     
                                          
     
  indexstate->ioss_RuntimeKeysReady = false;
  indexstate->ioss_RuntimeKeys = NULL;
  indexstate->ioss_NumRuntimeKeys = 0;

     
                                                            
     
  ExecIndexBuildScanKeys((PlanState *)indexstate, indexstate->ioss_RelationDesc, node->indexqual, false, &indexstate->ioss_ScanKeys, &indexstate->ioss_NumScanKeys, &indexstate->ioss_RuntimeKeys, &indexstate->ioss_NumRuntimeKeys, NULL,                   
      NULL);

     
                                                                        
     
  ExecIndexBuildScanKeys((PlanState *)indexstate, indexstate->ioss_RelationDesc, node->indexorderby, true, &indexstate->ioss_OrderByKeys, &indexstate->ioss_NumOrderByKeys, &indexstate->ioss_RuntimeKeys, &indexstate->ioss_NumRuntimeKeys, NULL,                   
      NULL);

     
                                                                           
                                                                            
                                                                            
                  
     
  if (indexstate->ioss_NumRuntimeKeys != 0)
  {
    ExprContext *stdecontext = indexstate->ss.ps.ps_ExprContext;

    ExecAssignExprContext(estate, &indexstate->ss.ps);
    indexstate->ioss_RuntimeContext = indexstate->ss.ps.ps_ExprContext;
    indexstate->ss.ps.ps_ExprContext = stdecontext;
  }
  else
  {
    indexstate->ioss_RuntimeContext = NULL;
  }

     
               
     
  return indexstate;
}

                                                                    
                                     
                                                                    
   

                                                                    
                              
   
                                                           
                                                           
                                                                    
   
void
ExecIndexOnlyScanEstimate(IndexOnlyScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;

  node->ioss_PscanLen = index_parallelscan_estimate(node->ioss_RelationDesc, estate->es_snapshot);
  shm_toc_estimate_chunk(&pcxt->estimator, node->ioss_PscanLen);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

                                                                    
                                   
   
                                                  
                                                                    
   
void
ExecIndexOnlyScanInitializeDSM(IndexOnlyScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;
  ParallelIndexScanDesc piscan;

  piscan = shm_toc_allocate(pcxt->toc, node->ioss_PscanLen);
  index_parallelscan_initialize(node->ss.ss_currentRelation, node->ioss_RelationDesc, estate->es_snapshot, piscan);
  shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, piscan);
  node->ioss_ScanDesc = index_beginscan_parallel(node->ss.ss_currentRelation, node->ioss_RelationDesc, node->ioss_NumScanKeys, node->ioss_NumOrderByKeys, piscan);
  node->ioss_ScanDesc->xs_want_itup = true;
  node->ioss_VMBuffer = InvalidBuffer;

     
                                                                           
                                   
     
  if (node->ioss_NumRuntimeKeys == 0 || node->ioss_RuntimeKeysReady)
  {
    index_rescan(node->ioss_ScanDesc, node->ioss_ScanKeys, node->ioss_NumScanKeys, node->ioss_OrderByKeys, node->ioss_NumOrderByKeys);
  }
}

                                                                    
                                     
   
                                                      
                                                                    
   
void
ExecIndexOnlyScanReInitializeDSM(IndexOnlyScanState *node, ParallelContext *pcxt)
{
  index_parallelrescan(node->ioss_ScanDesc);
}

                                                                    
                                      
   
                                                       
                                                                    
   
void
ExecIndexOnlyScanInitializeWorker(IndexOnlyScanState *node, ParallelWorkerContext *pwcxt)
{
  ParallelIndexScanDesc piscan;

  piscan = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, false);
  node->ioss_ScanDesc = index_beginscan_parallel(node->ss.ss_currentRelation, node->ioss_RelationDesc, node->ioss_NumScanKeys, node->ioss_NumOrderByKeys, piscan);
  node->ioss_ScanDesc->xs_want_itup = true;

     
                                                                           
                                   
     
  if (node->ioss_NumRuntimeKeys == 0 || node->ioss_RuntimeKeysReady)
  {
    index_rescan(node->ioss_ScanDesc, node->ioss_ScanKeys, node->ioss_NumScanKeys, node->ioss_OrderByKeys, node->ioss_NumOrderByKeys);
  }
}
