                                                                            
   
                    
                                                
                                                 
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
   
                      
                                          
                                                              
                                                                   
                                                       
                                                 
   
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeValuesscan.h"
#include "jit/jit.h"
#include "optimizer/clauses.h"
#include "utils/expandeddatum.h"

static TupleTableSlot *
ValuesNext(ValuesScanState *node);

                                                                    
                     
                                                                    
   

                                                                    
               
   
                                           
                                                                    
   
static TupleTableSlot *
ValuesNext(ValuesScanState *node)
{
  TupleTableSlot *slot;
  EState *estate;
  ExprContext *econtext;
  ScanDirection direction;
  int curr_idx;

     
                                                    
     
  estate = node->ss.ps.state;
  direction = estate->es_direction;
  slot = node->ss.ss_ScanTupleSlot;
  econtext = node->rowcontext;

     
                                                        
     
  if (ScanDirectionIsForward(direction))
  {
    if (node->curr_idx < node->array_len)
    {
      node->curr_idx++;
    }
  }
  else
  {
    if (node->curr_idx >= 0)
    {
      node->curr_idx--;
    }
  }

     
                                                                            
                                                                          
                                                                        
                                                              
     
  ExecClearTuple(slot);

  curr_idx = node->curr_idx;
  if (curr_idx >= 0 && curr_idx < node->array_len)
  {
    List *exprlist = node->exprlists[curr_idx];
    List *exprstatelist = node->exprstatelists[curr_idx];
    MemoryContext oldContext;
    Datum *values;
    bool *isnull;
    ListCell *lc;
    int resind;

       
                                                                         
                                                                         
                               
       
    ReScanExprContext(econtext);

       
                                                        
       
    oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

       
                                                                      
                                                                   
                                                                           
                                                                        
                                                                         
                                       
       
    if (exprstatelist == NIL)
    {
         
                                                                      
                                                                        
                                                                         
                                                       
         
                                                                       
                                                                        
                                                     
         
      exprstatelist = ExecInitExprList(exprlist, NULL);
    }

                                                                     
    Assert(list_length(exprstatelist) == slot->tts_tupleDescriptor->natts);

       
                                                                    
                                         
       
    values = slot->tts_values;
    isnull = slot->tts_isnull;

    resind = 0;
    foreach (lc, exprstatelist)
    {
      ExprState *estate = (ExprState *)lfirst(lc);
      Form_pg_attribute attr = TupleDescAttr(slot->tts_tupleDescriptor, resind);

      values[resind] = ExecEvalExpr(estate, econtext, &isnull[resind]);

         
                                                                      
                                                                     
                                                                       
                                                                    
         
      values[resind] = MakeExpandedObjectReadOnly(values[resind], isnull[resind], attr->attlen);

      resind++;
    }

    MemoryContextSwitchTo(oldContext);

       
                                     
       
    ExecStoreVirtualTuple(slot);
  }

  return slot;
}

   
                                                                             
   
static bool
ValuesRecheck(ValuesScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                         
   
                                                                        
           
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecValuesScan(PlanState *pstate)
{
  ValuesScanState *node = castNode(ValuesScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)ValuesNext, (ExecScanRecheckMtd)ValuesRecheck);
}

                                                                    
                       
                                                                    
   
ValuesScanState *
ExecInitValuesScan(ValuesScan *node, EState *estate, int eflags)
{
  ValuesScanState *scanstate;
  TupleDesc tupdesc;
  ListCell *vtl;
  int i;
  PlanState *planstate;

     
                                              
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                                   
     
  scanstate = makeNode(ValuesScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecValuesScan;

     
                                  
     
  planstate = &scanstate->ss.ps;

     
                                                                   
                                                                            
                                                                    
     
  ExecAssignExprContext(estate, planstate);
  scanstate->rowcontext = planstate->ps_ExprContext;
  ExecAssignExprContext(estate, planstate);

     
                                                               
     
  tupdesc = ExecTypeFromExprList((List *)linitial(node->values_lists));
  ExecInitScanTupleSlot(estate, &scanstate->ss, tupdesc, &TTSOpsVirtual);

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);

     
                               
     
  scanstate->curr_idx = -1;
  scanstate->array_len = list_length(node->values_lists);

     
                                                                      
                                                                       
                                                                   
                                                                             
                                                                           
                                                                           
                                                                      
                           
     
  scanstate->exprlists = (List **)palloc(scanstate->array_len * sizeof(List *));
  scanstate->exprstatelists = (List **)palloc0(scanstate->array_len * sizeof(List *));
  i = 0;
  foreach (vtl, node->values_lists)
  {
    List *exprs = castNode(List, lfirst(vtl));

    scanstate->exprlists[i] = exprs;

       
                                                                        
                                                  
       
    if (estate->es_subplanstates && contain_subplans((Node *)exprs))
    {
      int saved_jit_flags;

         
                                                                        
                                                                      
                                                                       
                                                                 
                                                                       
         
      saved_jit_flags = estate->es_jit_flags;
      estate->es_jit_flags = PGJIT_NONE;

      scanstate->exprstatelists[i] = ExecInitExprList(exprs, &scanstate->ss.ps);

      estate->es_jit_flags = saved_jit_flags;
    }
    i++;
  }

  return scanstate;
}

                                                                    
                      
   
                                                    
                                                                    
   
void
ExecEndValuesScan(ValuesScanState *node)
{
     
                            
     
  ExecFreeExprContext(&node->ss.ps);
  node->ss.ps.ps_ExprContext = node->rowcontext;
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

                                                                    
                         
   
                          
                                                                    
   
void
ExecReScanValuesScan(ValuesScanState *node)
{
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }

  ExecScanReScan(&node->ss);

  node->curr_idx = -1;
}
