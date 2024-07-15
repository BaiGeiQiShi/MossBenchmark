                                                                            
   
                    
                                                                           
   
               
   
                                                                           
                                                                         
                                                                         
                                                                      
                                                            
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeProjectSet.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/memutils.h"

static TupleTableSlot *
ExecProjectSRF(ProjectSetState *node, bool continuing);

                                                                    
                         
   
                                                                      
                          
                                                                    
   
static TupleTableSlot *
ExecProjectSet(PlanState *pstate)
{
  ProjectSetState *node = castNode(ProjectSetState, pstate);
  TupleTableSlot *outerTupleSlot;
  TupleTableSlot *resultSlot;
  PlanState *outerPlan;
  ExprContext *econtext;

  CHECK_FOR_INTERRUPTS();

  econtext = node->ps.ps_ExprContext;

     
                                                                             
                                                                             
                                                          
     
  ResetExprContext(econtext);

     
                                                                            
                                                                        
                                                       
     
  if (node->pending_srf_tuples)
  {
    resultSlot = ExecProjectSRF(node, true);

    if (resultSlot != NULL)
    {
      return resultSlot;
    }
  }

     
                                                                      
                                                                          
                                                                         
                                                                        
            
     
  MemoryContextReset(node->argcontext);

     
                                                       
     
  for (;;)
  {
       
                                                                    
       
    outerPlan = outerPlanState(node);
    outerTupleSlot = ExecProcNode(outerPlan);

    if (TupIsNull(outerTupleSlot))
    {
      return NULL;
    }

       
                                                                       
                                               
       
    econtext->ecxt_outertuple = outerTupleSlot;

                                  
    resultSlot = ExecProjectSRF(node, false);

       
                                                                          
                                                                       
                              
       
    if (resultSlot)
    {
      return resultSlot;
    }
  }

  return NULL;
}

                                                                    
                   
   
                                                                         
   
                                                                       
                                                                       
   
                                                       
   
                                                                    
   
static TupleTableSlot *
ExecProjectSRF(ProjectSetState *node, bool continuing)
{
  TupleTableSlot *resultSlot = node->ps.ps_ResultTupleSlot;
  ExprContext *econtext = node->ps.ps_ExprContext;
  MemoryContext oldcontext;
  bool hassrf PG_USED_FOR_ASSERTS_ONLY;
  bool hasresult;
  int argno;

  ExecClearTuple(resultSlot);

                                                                     
  oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

     
                                                                           
                                                
     
  node->pending_srf_tuples = false;

  hassrf = hasresult = false;
  for (argno = 0; argno < node->nelems; argno++)
  {
    Node *elem = node->elems[argno];
    ExprDoneCond *isdone = &node->elemdone[argno];
    Datum *result = &resultSlot->tts_values[argno];
    bool *isnull = &resultSlot->tts_isnull[argno];

    if (continuing && *isdone == ExprEndResult)
    {
         
                                                                         
                                                       
         
      *result = (Datum)0;
      *isnull = true;
      hassrf = true;
    }
    else if (IsA(elem, SetExprState))
    {
         
                                                                       
         
      *result = ExecMakeFunctionResultSet((SetExprState *)elem, econtext, node->argcontext, isnull, isdone);

      if (*isdone != ExprEndResult)
      {
        hasresult = true;
      }
      if (*isdone == ExprMultipleResult)
      {
        node->pending_srf_tuples = true;
      }
      hassrf = true;
    }
    else
    {
                                                             
      *result = ExecEvalExpr((ExprState *)elem, econtext, isnull);
      *isdone = ExprSingleResult;
    }
  }

  MemoryContextSwitchTo(oldcontext);

                                                        
  Assert(hassrf);

     
                                                                          
               
     
  if (hasresult)
  {
    ExecStoreVirtualTuple(resultSlot);
    return resultSlot;
  }

  return NULL;
}

                                                                    
                       
   
                                                                   
                                                            
                   
                                                                    
   
ProjectSetState *
ExecInitProjectSet(ProjectSet *node, EState *estate, int eflags)
{
  ProjectSetState *state;
  ListCell *lc;
  int off;

                                   
  Assert(!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)));

     
                            
     
  state = makeNode(ProjectSetState);
  state->ps.plan = (Plan *)node;
  state->ps.state = estate;
  state->ps.ExecProcNode = ExecProjectSet;

  state->pending_srf_tuples = false;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &state->ps);

     
                            
     
  outerPlanState(state) = ExecInitNode(outerPlan(node), estate, eflags);

     
                             
     
  Assert(innerPlan(node) == NULL);

     
                                                
     
  ExecInitResultTupleSlotTL(&state->ps, &TTSOpsVirtual);

                                                                           
  state->nelems = list_length(node->plan.targetlist);
  state->elems = (Node **)palloc(sizeof(Node *) * state->nelems);
  state->elemdone = (ExprDoneCond *)palloc(sizeof(ExprDoneCond) * state->nelems);

     
                                                             
                                                                      
                                                       
                                                 
     
  off = 0;
  foreach (lc, node->plan.targetlist)
  {
    TargetEntry *te = (TargetEntry *)lfirst(lc);
    Expr *expr = te->expr;

    if ((IsA(expr, FuncExpr) && ((FuncExpr *)expr)->funcretset) || (IsA(expr, OpExpr) && ((OpExpr *)expr)->opretset))
    {
      state->elems[off] = (Node *)ExecInitFunctionResultSet(expr, state->ps.ps_ExprContext, &state->ps);
    }
    else
    {
      Assert(!expression_returns_set((Node *)expr));
      state->elems[off] = (Node *)ExecInitExpr(expr, &state->ps);
    }

    off++;
  }

                                                     
  Assert(node->plan.qual == NIL);

     
                                                                       
                                                                             
                                                                     
                                                                            
                                                                             
                
     
  state->argcontext = AllocSetContextCreate(CurrentMemoryContext, "tSRF function arguments", ALLOCSET_DEFAULT_SIZES);

  return state;
}

                                                                    
                      
   
                                                  
                                                                    
   
void
ExecEndProjectSet(ProjectSetState *node)
{
     
                          
     
  ExecFreeExprContext(&node->ps);

     
                               
     
  ExecClearTuple(node->ps.ps_ResultTupleSlot);

     
                        
     
  ExecEndNode(outerPlanState(node));
}

void
ExecReScanProjectSet(ProjectSetState *node)
{
                                              
  node->pending_srf_tuples = false;

     
                                                                        
                         
     
  if (node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}
