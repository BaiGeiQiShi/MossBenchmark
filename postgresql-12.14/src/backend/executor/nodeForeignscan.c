                                                                            
   
                     
                                                 
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
   
                      
   
                                             
                                                             
                                                        
                                                          
   
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeForeignscan.h"
#include "foreign/fdwapi.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
ForeignNext(ForeignScanState *node);
static bool
ForeignRecheck(ForeignScanState *node, TupleTableSlot *slot);

                                                                    
                
   
                                            
                                                                    
   
static TupleTableSlot *
ForeignNext(ForeignScanState *node)
{
  TupleTableSlot *slot;
  ForeignScan *plan = (ForeignScan *)node->ss.ps.plan;
  ExprContext *econtext = node->ss.ps.ps_ExprContext;
  MemoryContext oldcontext;

                                                        
  oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
  if (plan->operation != CMD_SELECT)
  {
    slot = node->fdwroutine->IterateDirectModify(node);
  }
  else
  {
    slot = node->fdwroutine->IterateForeignScan(node);
  }
  MemoryContextSwitchTo(oldcontext);

     
                                                                       
             
     
  if (plan->fsSystemCol && !TupIsNull(slot))
  {
    slot->tts_tableOid = RelationGetRelid(node->ss.ss_currentRelation);
  }

  return slot;
}

   
                                                                              
   
static bool
ForeignRecheck(ForeignScanState *node, TupleTableSlot *slot)
{
  FdwRoutine *fdwroutine = node->fdwroutine;
  ExprContext *econtext;

     
                                                          
     
  econtext = node->ss.ps.ps_ExprContext;

                                                      
  econtext->ecxt_scantuple = slot;

  ResetExprContext(econtext);

     
                                                                             
                                                                            
                                                                            
                                                                             
                                                                           
                              
     
  if (fdwroutine->RecheckForeignScan && !fdwroutine->RecheckForeignScan(node, slot))
  {
    return false;
  }

  return ExecQual(node->fdw_recheck_quals, econtext);
}

                                                                    
                          
   
                                                                 
                
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecForeignScan(PlanState *pstate)
{
  ForeignScanState *node = castNode(ForeignScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)ForeignNext, (ExecScanRecheckMtd)ForeignRecheck);
}

                                                                    
                        
                                                                    
   
ForeignScanState *
ExecInitForeignScan(ForeignScan *node, EState *estate, int eflags)
{
  ForeignScanState *scanstate;
  Relation currentRelation = NULL;
  Index scanrelid = node->scan.scanrelid;
  Index tlistvarno;
  FdwRoutine *fdwroutine;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  scanstate = makeNode(ForeignScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecForeignScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                                                                             
                   
     
  if (scanrelid > 0)
  {
    currentRelation = ExecOpenScanRelation(estate, scanrelid, eflags);
    scanstate->ss.ss_currentRelation = currentRelation;
    fdwroutine = GetFdwRoutineForRelation(currentRelation, true);
  }
  else
  {
                                                                   
    fdwroutine = GetFdwRoutineByServerId(node->fs_server);
  }

     
                                                                      
                                                                             
     
  if (node->fdw_scan_tlist != NIL || currentRelation == NULL)
  {
    TupleDesc scan_tupdesc;

    scan_tupdesc = ExecTypeFromTL(node->fdw_scan_tlist);
    ExecInitScanTupleSlot(estate, &scanstate->ss, scan_tupdesc, &TTSOpsHeapTuple);
                                                                    
    tlistvarno = INDEX_VAR;
  }
  else
  {
    TupleDesc scan_tupdesc;

                                                                           
    scan_tupdesc = CreateTupleDescCopy(RelationGetDescr(currentRelation));
    ExecInitScanTupleSlot(estate, &scanstate->ss, scan_tupdesc, &TTSOpsHeapTuple);
                                                                    
    tlistvarno = scanrelid;
  }

                                           
  scanstate->ss.ps.scanopsfixed = false;
  scanstate->ss.ps.scanopsset = true;

     
                                                  
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfoWithVarno(&scanstate->ss, tlistvarno);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);
  scanstate->fdw_recheck_quals = ExecInitQual(node->fdw_recheck_quals, (PlanState *)scanstate);

     
                                   
     
  scanstate->fdwroutine = fdwroutine;
  scanstate->fdw_state = NULL;

                                  
  if (outerPlan(node))
  {
    outerPlanState(scanstate) = ExecInitNode(outerPlan(node), estate, eflags);
  }

     
                                          
     
  if (node->operation != CMD_SELECT)
  {
    fdwroutine->BeginDirectModify(scanstate, eflags);
  }
  else
  {
    fdwroutine->BeginForeignScan(scanstate, eflags);
  }

  return scanstate;
}

                                                                    
                       
   
                                                    
                                                                    
   
void
ExecEndForeignScan(ForeignScanState *node)
{
  ForeignScan *plan = (ForeignScan *)node->ss.ps.plan;

                             
  if (plan->operation != CMD_SELECT)
  {
    node->fdwroutine->EndDirectModify(node);
  }
  else
  {
    node->fdwroutine->EndForeignScan(node);
  }

                                 
  if (outerPlanState(node))
  {
    ExecEndNode(outerPlanState(node));
  }

                            
  ExecFreeExprContext(&node->ss.ps);

                                 
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

                                                                    
                          
   
                          
                                                                    
   
void
ExecReScanForeignScan(ForeignScanState *node)
{
  PlanState *outerPlan = outerPlanState(node);

  node->fdwroutine->ReScanForeignScan(node);

     
                                                                        
                                                                             
                               
     
  if (outerPlan != NULL && outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }

  ExecScanReScan(&node->ss);
}

                                                                    
                            
   
                                                                  
                                                                    
   
void
ExecForeignScanEstimate(ForeignScanState *node, ParallelContext *pcxt)
{
  FdwRoutine *fdwroutine = node->fdwroutine;

  if (fdwroutine->EstimateDSMForeignScan)
  {
    node->pscan_len = fdwroutine->EstimateDSMForeignScan(node, pcxt);
    shm_toc_estimate_chunk(&pcxt->estimator, node->pscan_len);
    shm_toc_estimate_keys(&pcxt->estimator, 1);
  }
}

                                                                    
                                 
   
                                                     
                                                                    
   
void
ExecForeignScanInitializeDSM(ForeignScanState *node, ParallelContext *pcxt)
{
  FdwRoutine *fdwroutine = node->fdwroutine;

  if (fdwroutine->InitializeDSMForeignScan)
  {
    int plan_node_id = node->ss.ps.plan->plan_node_id;
    void *coordinate;

    coordinate = shm_toc_allocate(pcxt->toc, node->pscan_len);
    fdwroutine->InitializeDSMForeignScan(node, pcxt, coordinate);
    shm_toc_insert(pcxt->toc, plan_node_id, coordinate);
  }
}

                                                                    
                                   
   
                                                      
                                                                    
   
void
ExecForeignScanReInitializeDSM(ForeignScanState *node, ParallelContext *pcxt)
{
  FdwRoutine *fdwroutine = node->fdwroutine;

  if (fdwroutine->ReInitializeDSMForeignScan)
  {
    int plan_node_id = node->ss.ps.plan->plan_node_id;
    void *coordinate;

    coordinate = shm_toc_lookup(pcxt->toc, plan_node_id, false);
    fdwroutine->ReInitializeDSMForeignScan(node, pcxt, coordinate);
  }
}

                                                                    
                                    
   
                                                                      
                                                                    
   
void
ExecForeignScanInitializeWorker(ForeignScanState *node, ParallelWorkerContext *pwcxt)
{
  FdwRoutine *fdwroutine = node->fdwroutine;

  if (fdwroutine->InitializeWorkerForeignScan)
  {
    int plan_node_id = node->ss.ps.plan->plan_node_id;
    void *coordinate;

    coordinate = shm_toc_lookup(pwcxt->toc, plan_node_id, false);
    fdwroutine->InitializeWorkerForeignScan(node, pwcxt->toc, coordinate);
  }
}

                                                                    
                            
   
                                                               
                                          
                                                                    
   
void
ExecShutdownForeignScan(ForeignScanState *node)
{
  FdwRoutine *fdwroutine = node->fdwroutine;

  if (fdwroutine->ShutdownForeignScan)
  {
    fdwroutine->ShutdownForeignScan(node);
  }
}
