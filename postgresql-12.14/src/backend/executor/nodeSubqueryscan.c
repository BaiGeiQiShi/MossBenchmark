                                                                            
   
                      
                                                                          
   
                                                                            
                                                                          
   
   
                                                                         
                                                                        
   
   
                  
                                             
   
                                                                            
   
   
                      
                                         
                                                                
                                                                       
                                                          
                                                 
   
   
#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeSubqueryscan.h"

static TupleTableSlot *
SubqueryNext(SubqueryScanState *node);

                                                                    
                     
                                                                    
   
                                                                    
                 
   
                                             
                                                                    
   
static TupleTableSlot *
SubqueryNext(SubqueryScanState *node)
{
  TupleTableSlot *slot;

     
                                            
     
  slot = ExecProcNode(node->subplan);

     
                                                                           
                                                                         
                             
     
  return slot;
}

   
                                                                               
   
static bool
SubqueryRecheck(SubqueryScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                           
   
                                                                    
           
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecSubqueryScan(PlanState *pstate)
{
  SubqueryScanState *node = castNode(SubqueryScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)SubqueryNext, (ExecScanRecheckMtd)SubqueryRecheck);
}

                                                                    
                         
                                                                    
   
SubqueryScanState *
ExecInitSubqueryScan(SubqueryScan *node, EState *estate, int eflags)
{
  SubqueryScanState *subquerystate;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

                                                          
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                            
     
  subquerystate = makeNode(SubqueryScanState);
  subquerystate->ss.ps.plan = (Plan *)node;
  subquerystate->ss.ps.state = estate;
  subquerystate->ss.ps.ExecProcNode = ExecSubqueryScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &subquerystate->ss.ps);

     
                         
     
  subquerystate->subplan = ExecInitNode(node->subplan, estate, eflags);

     
                                                                            
     
  ExecInitScanTupleSlot(estate, &subquerystate->ss, ExecGetResultType(subquerystate->subplan), ExecGetResultSlotOps(subquerystate->subplan, NULL));

     
                                                                           
                                      
     
  subquerystate->ss.ps.scanopsset = true;
  subquerystate->ss.ps.scanops = ExecGetResultSlotOps(subquerystate->subplan, &subquerystate->ss.ps.scanopsfixed);
  subquerystate->ss.ps.resultopsset = true;
  subquerystate->ss.ps.resultops = subquerystate->ss.ps.scanops;
  subquerystate->ss.ps.resultopsfixed = subquerystate->ss.ps.scanopsfixed;

     
                                            
     
  ExecInitResultTypeTL(&subquerystate->ss.ps);
  ExecAssignScanProjectionInfo(&subquerystate->ss);

     
                                  
     
  subquerystate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)subquerystate);

  return subquerystate;
}

                                                                    
                        
   
                                                    
                                                                    
   
void
ExecEndSubqueryScan(SubqueryScanState *node)
{
     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                                     
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                         
     
  ExecEndNode(node->subplan);
}

                                                                    
                           
   
                          
                                                                    
   
void
ExecReScanSubqueryScan(SubqueryScanState *node)
{
  ExecScanReScan(&node->ss);

     
                                                               
                                                                            
                                                                           
     
  if (node->ss.ps.chgParam != NULL)
  {
    UpdateChangedParamSet(node->subplan, node->ss.ps.chgParam);
  }

     
                                                                        
                         
     
  if (node->subplan->chgParam == NULL)
  {
    ExecReScan(node->subplan);
  }
}
