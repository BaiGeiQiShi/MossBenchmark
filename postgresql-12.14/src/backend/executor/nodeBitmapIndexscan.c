                                                                            
   
                         
                                                            
   
                                                                         
                                                                        
   
   
                  
                                                
   
                                                                            
   
   
                      
                                                           
                                                                 
                                                           
                                                  
   
#include "postgres.h"

#include "access/genam.h"
#include "executor/execdebug.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeIndexscan.h"
#include "miscadmin.h"
#include "utils/memutils.h"

                                                                    
                        
   
                                  
                                                                    
   
static TupleTableSlot *
ExecBitmapIndexScan(PlanState *pstate)
{
  elog(ERROR, "BitmapIndexScan node does not support ExecProcNode call convention");
  return NULL;
}

                                                                    
                                   
                                                                    
   
Node *
MultiExecBitmapIndexScan(BitmapIndexScanState *node)
{
  TIDBitmap *tbm;
  IndexScanDesc scandesc;
  double nTuples = 0;
  bool doscan;

                                                    
  if (node->ss.ps.instrument)
  {
    InstrStartNode(node->ss.ps.instrument);
  }

     
                                                        
     
  scandesc = node->biss_ScanDesc;

     
                                                                             
                                                                          
                                                                            
                                        
     
  if (!node->biss_RuntimeKeysReady && (node->biss_NumRuntimeKeys != 0 || node->biss_NumArrayKeys != 0))
  {
    ExecReScan((PlanState *)node);
    doscan = node->biss_RuntimeKeysReady;
  }
  else
  {
    doscan = true;
  }

     
                                                                           
                                                                            
                                                                        
                                                                  
     
  if (node->biss_result)
  {
    tbm = node->biss_result;
    node->biss_result = NULL;                          
  }
  else
  {
                                                        
    tbm = tbm_create(work_mem * 1024L, ((BitmapIndexScan *)node->ss.ps.plan)->isshared ? node->ss.ps.state->es_query_dsa : NULL);
  }

     
                                                
     
  while (doscan)
  {
    nTuples += (double)index_getbitmap(scandesc, tbm);

    CHECK_FOR_INTERRUPTS();

    doscan = ExecIndexAdvanceArrayKeys(node->biss_ArrayKeys, node->biss_NumArrayKeys);
    if (doscan)                       
    {
      index_rescan(node->biss_ScanDesc, node->biss_ScanKeys, node->biss_NumScanKeys, NULL, 0);
    }
  }

                                                    
  if (node->ss.ps.instrument)
  {
    InstrStopNode(node->ss.ps.instrument, nTuples);
  }

  return (Node *)tbm;
}

                                                                    
                                    
   
                                                                    
                                                                     
                                                                    
   
void
ExecReScanBitmapIndexScan(BitmapIndexScanState *node)
{
  ExprContext *econtext = node->biss_RuntimeContext;

     
                                                                         
                                                                         
                                
     
  if (econtext)
  {
    ResetExprContext(econtext);
  }

     
                                                                        
                                                                
     
                                                                         
                                                                          
                                     
     
  if (node->biss_NumRuntimeKeys != 0)
  {
    ExecIndexEvalRuntimeKeys(econtext, node->biss_RuntimeKeys, node->biss_NumRuntimeKeys);
  }
  if (node->biss_NumArrayKeys != 0)
  {
    node->biss_RuntimeKeysReady = ExecIndexEvalArrayKeys(econtext, node->biss_ArrayKeys, node->biss_NumArrayKeys);
  }
  else
  {
    node->biss_RuntimeKeysReady = true;
  }

                        
  if (node->biss_RuntimeKeysReady)
  {
    index_rescan(node->biss_ScanDesc, node->biss_ScanKeys, node->biss_NumScanKeys, NULL, 0);
  }
}

                                                                    
                           
                                                                    
   
void
ExecEndBitmapIndexScan(BitmapIndexScanState *node)
{
  Relation indexRelationDesc;
  IndexScanDesc indexScanDesc;

     
                                       
     
  indexRelationDesc = node->biss_RelationDesc;
  indexScanDesc = node->biss_ScanDesc;

     
                                                                     
     
#ifdef NOT_USED
  if (node->biss_RuntimeContext)
  {
    FreeExprContext(node->biss_RuntimeContext, true);
  }
#endif

     
                                                           
     
  if (indexScanDesc)
  {
    index_endscan(indexScanDesc);
  }
  if (indexRelationDesc)
  {
    index_close(indexRelationDesc, NoLock);
  }
}

                                                                    
                            
   
                                                    
                                                                    
   
BitmapIndexScanState *
ExecInitBitmapIndexScan(BitmapIndexScan *node, EState *estate, int eflags)
{
  BitmapIndexScanState *indexstate;
  LOCKMODE lockmode;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  indexstate = makeNode(BitmapIndexScanState);
  indexstate->ss.ps.plan = (Plan *)node;
  indexstate->ss.ps.state = estate;
  indexstate->ss.ps.ExecProcNode = ExecBitmapIndexScan;

                                                             
  indexstate->biss_result = NULL;

     
                                                                       
                                                                            
                                                                  
     

  indexstate->ss.ss_currentRelation = NULL;
  indexstate->ss.ss_currentScanDesc = NULL;

     
                                  
     
                                                                        
                                                      
     

     
                                  
     
                                                                            
     
                                                                         
                                                          
     

     
                                                                           
                                                                             
                                        
     
  if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
  {
    return indexstate;
  }

                                
  lockmode = exec_rt_fetch(node->scan.scanrelid, estate)->rellockmode;
  indexstate->biss_RelationDesc = index_open(node->indexid, lockmode);

     
                                          
     
  indexstate->biss_RuntimeKeysReady = false;
  indexstate->biss_RuntimeKeys = NULL;
  indexstate->biss_NumRuntimeKeys = 0;

     
                                                            
     
  ExecIndexBuildScanKeys((PlanState *)indexstate, indexstate->biss_RelationDesc, node->indexqual, false, &indexstate->biss_ScanKeys, &indexstate->biss_NumScanKeys, &indexstate->biss_RuntimeKeys, &indexstate->biss_NumRuntimeKeys, &indexstate->biss_ArrayKeys, &indexstate->biss_NumArrayKeys);

     
                                                                      
                                                                             
                                                                       
                                                                        
     
  if (indexstate->biss_NumRuntimeKeys != 0 || indexstate->biss_NumArrayKeys != 0)
  {
    ExprContext *stdecontext = indexstate->ss.ps.ps_ExprContext;

    ExecAssignExprContext(estate, &indexstate->ss.ps);
    indexstate->biss_RuntimeContext = indexstate->ss.ps.ps_ExprContext;
    indexstate->ss.ps.ps_ExprContext = stdecontext;
  }
  else
  {
    indexstate->biss_RuntimeContext = NULL;
  }

     
                                 
     
  indexstate->biss_ScanDesc = index_beginscan_bitmap(indexstate->biss_RelationDesc, estate->es_snapshot, indexstate->biss_NumScanKeys);

     
                                                                             
               
     
  if (indexstate->biss_NumRuntimeKeys == 0 && indexstate->biss_NumArrayKeys == 0)
  {
    index_rescan(indexstate->biss_ScanDesc, indexstate->biss_ScanKeys, indexstate->biss_NumScanKeys, NULL, 0);
  }

     
               
     
  return indexstate;
}
