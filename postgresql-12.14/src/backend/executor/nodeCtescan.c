                                                                            
   
                 
                                       
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeCtescan.h"
#include "miscadmin.h"

static TupleTableSlot *
CteScanNext(CteScanState *node);

                                                                    
                
   
                                        
                                                                    
   
static TupleTableSlot *
CteScanNext(CteScanState *node)
{
  EState *estate;
  ScanDirection dir;
  bool forward;
  Tuplestorestate *tuplestorestate;
  bool eof_tuplestore;
  TupleTableSlot *slot;

     
                              
     
  estate = node->ss.ps.state;
  dir = estate->es_direction;
  forward = ScanDirectionIsForward(dir);
  tuplestorestate = node->leader->cte_table;
  tuplestore_select_read_pointer(tuplestorestate, node->readptr);
  slot = node->ss.ss_ScanTupleSlot;

     
                                                                             
                                       
     
  eof_tuplestore = tuplestore_ateof(tuplestorestate);

  if (!forward && eof_tuplestore)
  {
    if (!node->leader->eof_cte)
    {
         
                                                               
                                                                        
                                                                    
                
         
      if (!tuplestore_advance(tuplestorestate, forward))
      {
        return NULL;                                   
      }
    }
    eof_tuplestore = false;
  }

     
                                                                   
     
                                                                         
                                                                             
                                                     
     
  if (!eof_tuplestore)
  {
    if (tuplestore_gettupleslot(tuplestorestate, forward, true, slot))
    {
      return slot;
    }
    if (forward)
    {
      eof_tuplestore = true;
    }
  }

     
                                                                
     
                                                                            
                                                                           
                                                                             
                    
     
  if (eof_tuplestore && !node->leader->eof_cte)
  {
    TupleTableSlot *cteslot;

       
                                                                          
                                            
       
    cteslot = ExecProcNode(node->cteplanstate);
    if (TupIsNull(cteslot))
    {
      node->leader->eof_cte = true;
      return NULL;
    }

       
                                                                   
                                                                      
                                        
       
    tuplestore_select_read_pointer(tuplestorestate, node->readptr);

       
                                                                         
                                                                          
                                                                        
                                                                           
                  
       
    tuplestore_puttupleslot(tuplestorestate, cteslot);

       
                                                                         
                                                                         
                                                                       
             
       
    return ExecCopySlot(slot, cteslot);
  }

     
                      
     
  return ExecClearTuple(slot);
}

   
                                                                              
   
static bool
CteScanRecheck(CteScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                      
   
                                                                      
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecCteScan(PlanState *pstate)
{
  CteScanState *node = castNode(CteScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)CteScanNext, (ExecScanRecheckMtd)CteScanRecheck);
}

                                                                    
                    
                                                                    
   
CteScanState *
ExecInitCteScan(CteScan *node, EState *estate, int eflags)
{
  CteScanState *scanstate;
  ParamExecData *prmdata;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

     
                                                                             
                                                                         
                                                                        
                                                       
     
                                                                             
                                                                         
                                                                          
                                                                            
                               
     
  eflags |= EXEC_FLAG_REWIND;

     
                                           
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                                      
     
  scanstate = makeNode(CteScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecCteScan;
  scanstate->eflags = eflags;
  scanstate->cte_table = NULL;
  scanstate->eof_cte = false;

     
                                                          
     
  scanstate->cteplanstate = (PlanState *)list_nth(estate->es_subplanstates, node->ctePlanId - 1);

     
                                                                            
                                                                         
                                                                             
                                               
     
  prmdata = &(estate->es_param_exec_vals[node->cteParam]);
  Assert(prmdata->execPlan == NULL);
  Assert(!prmdata->isnull);
  scanstate->leader = castNode(CteScanState, DatumGetPointer(prmdata->value));
  if (scanstate->leader == NULL)
  {
                         
    prmdata->value = PointerGetDatum(scanstate);
    scanstate->leader = scanstate;
    scanstate->cte_table = tuplestore_begin_heap(true, false, work_mem);
    tuplestore_set_eflags(scanstate->cte_table, scanstate->eflags);
    scanstate->readptr = 0;
  }
  else
  {
                        
                                                               
    scanstate->readptr = tuplestore_alloc_read_pointer(scanstate->leader->cte_table, scanstate->eflags);
    tuplestore_select_read_pointer(scanstate->leader->cte_table, scanstate->readptr);
    tuplestore_rescan(scanstate->leader->cte_table);
  }

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                                                                        
                                                                
     
  ExecInitScanTupleSlot(estate, &scanstate->ss, ExecGetResultType(scanstate->cteplanstate), &TTSOpsMinimalTuple);

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);

  return scanstate;
}

                                                                    
                   
   
                                                    
                                                                    
   
void
ExecEndCteScan(CteScanState *node)
{
     
                      
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                              
     
  if (node->leader == node)
  {
    tuplestore_end(node->cte_table);
    node->cte_table = NULL;
  }
}

                                                                    
                      
   
                          
                                                                    
   
void
ExecReScanCteScan(CteScanState *node)
{
  Tuplestorestate *tuplestorestate = node->leader->cte_table;

  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }

  ExecScanReScan(&node->ss);

     
                                                                           
                                                                           
                                                                           
                                                                       
                                                                      
                                                   
     
  if (node->leader->cteplanstate->chgParam != NULL)
  {
    tuplestore_clear(tuplestorestate);
    node->leader->eof_cte = false;
  }
  else
  {
       
                                                                    
                                                                          
                                                       
       
    tuplestore_select_read_pointer(tuplestorestate, node->readptr);
    tuplestore_rescan(tuplestorestate);
  }
}
