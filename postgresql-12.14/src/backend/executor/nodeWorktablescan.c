                                                                            
   
                       
                                             
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeWorktablescan.h"

static TupleTableSlot *
WorkTableScanNext(WorkTableScanState *node);

                                                                    
                      
   
                                              
                                                                    
   
static TupleTableSlot *
WorkTableScanNext(WorkTableScanState *node)
{
  TupleTableSlot *slot;
  Tuplestorestate *tuplestorestate;

     
                                                    
     
                                                                             
                                                                             
                                                                      
                                                                        
                                                                         
                                                                  
                                                                     
     
                                                                         
                                                                         
                                                                         
     
  Assert(ScanDirectionIsForward(node->ss.ps.state->es_direction));

  tuplestorestate = node->rustate->working_table;

     
                                                                        
     
  slot = node->ss.ss_ScanTupleSlot;
  (void)tuplestore_gettupleslot(tuplestorestate, true, false, slot);
  return slot;
}

   
                                                                                    
   
static bool
WorkTableScanRecheck(WorkTableScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                            
   
                                                                            
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecWorkTableScan(PlanState *pstate)
{
  WorkTableScanState *node = castNode(WorkTableScanState, pstate);

     
                                                                         
                                                                             
                                                                     
                           
     
  if (node->rustate == NULL)
  {
    WorkTableScan *plan = (WorkTableScan *)node->ss.ps.plan;
    EState *estate = node->ss.ps.state;
    ParamExecData *param;

    param = &(estate->es_param_exec_vals[plan->wtParam]);
    Assert(param->execPlan == NULL);
    Assert(!param->isnull);
    node->rustate = castNode(RecursiveUnionState, DatumGetPointer(param->value));
    Assert(node->rustate);

       
                                                                          
                                                                
                                                                      
                                                
       
    ExecAssignScanType(&node->ss, ExecGetResultType(&node->rustate->ps));

       
                                                                          
                                      
       
    ExecAssignScanProjectionInfo(&node->ss);
  }

  return ExecScan(&node->ss, (ExecScanAccessMtd)WorkTableScanNext, (ExecScanRecheckMtd)WorkTableScanRecheck);
}

                                                                    
                          
                                                                    
   
WorkTableScanState *
ExecInitWorkTableScan(WorkTableScan *node, EState *estate, int eflags)
{
  WorkTableScanState *scanstate;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                                                 
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                                            
     
  scanstate = makeNode(WorkTableScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecWorkTableScan;
  scanstate->rustate = NULL;                           

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                                
     
  ExecInitResultTypeTL(&scanstate->ss.ps);

                                                
  scanstate->ss.ps.resultopsset = true;
  scanstate->ss.ps.resultopsfixed = false;

  ExecInitScanTupleSlot(estate, &scanstate->ss, NULL, &TTSOpsMinimalTuple);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);

     
                                                                        
              
     

  return scanstate;
}

                                                                    
                         
   
                                                    
                                                                    
   
void
ExecEndWorkTableScan(WorkTableScanState *node)
{
     
                      
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

                                                                    
                            
   
                          
                                                                    
   
void
ExecReScanWorkTableScan(WorkTableScanState *node)
{
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }

  ExecScanReScan(&node->ss);

                                                                      
  if (node->rustate)
  {
    tuplestore_rescan(node->rustate->working_table);
  }
}
