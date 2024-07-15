                                                                            
   
               
                                                                    
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
   
                      
                                                   
                                                   
                                              
   

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeLimit.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"

static void
recompute_limits(LimitState *node);
static int64
compute_tuples_needed(LimitState *node);

                                                                    
              
   
                                                                
                                                             
                                                                    
   
static TupleTableSlot *                              
ExecLimit(PlanState *pstate)
{
  LimitState *node = castNode(LimitState, pstate);
  ScanDirection direction;
  TupleTableSlot *slot;
  PlanState *outerPlan;

  CHECK_FOR_INTERRUPTS();

     
                                   
     
  direction = node->ps.state->es_direction;
  outerPlan = outerPlanState(node);

     
                                               
     
  switch (node->lstate)
  {
  case LIMIT_INITIAL:

       
                                                                       
                                                                      
                                                                      
                                          
       
    recompute_limits(node);

                   

  case LIMIT_RESCAN:

       
                                                                   
       
    if (!ScanDirectionIsForward(direction))
    {
      return NULL;
    }

       
                                                                
       
    if (node->count <= 0 && !node->noCount)
    {
      node->lstate = LIMIT_EMPTY;
      return NULL;
    }

       
                                                                 
       
    for (;;)
    {
      slot = ExecProcNode(outerPlan);
      if (TupIsNull(slot))
      {
           
                                                                
                              
           
        node->lstate = LIMIT_EMPTY;
        return NULL;
      }
      node->subSlot = slot;
      if (++node->position > node->offset)
      {
        break;
      }
    }

       
                                                    
       
    node->lstate = LIMIT_INWINDOW;
    break;

  case LIMIT_EMPTY:

       
                                                                  
                                                            
       
    return NULL;

  case LIMIT_INWINDOW:
    if (ScanDirectionIsForward(direction))
    {
         
                                                                    
                                                              
                                                                    
                                                           
         
                                                             
                                                                     
                                                                     
                                                                   
                       
         
      if (!node->noCount && node->position - node->offset >= node->count)
      {
        node->lstate = LIMIT_WINDOWEND;
        return NULL;
      }

         
                                              
         
      slot = ExecProcNode(outerPlan);
      if (TupIsNull(slot))
      {
        node->lstate = LIMIT_SUBPLANEOF;
        return NULL;
      }
      node->subSlot = slot;
      node->position++;
    }
    else
    {
         
                                                                    
                                                           
         
      if (node->position <= node->offset + 1)
      {
        node->lstate = LIMIT_WINDOWSTART;
        return NULL;
      }

         
                                                               
         
      slot = ExecProcNode(outerPlan);
      if (TupIsNull(slot))
      {
        elog(ERROR, "LIMIT subplan failed to run backwards");
      }
      node->subSlot = slot;
      node->position--;
    }
    break;

  case LIMIT_SUBPLANEOF:
    if (ScanDirectionIsForward(direction))
    {
      return NULL;
    }

       
                                                                      
                                                              
       
    slot = ExecProcNode(outerPlan);
    if (TupIsNull(slot))
    {
      elog(ERROR, "LIMIT subplan failed to run backwards");
    }
    node->subSlot = slot;
    node->lstate = LIMIT_INWINDOW;
                                                                     
    break;

  case LIMIT_WINDOWEND:
    if (ScanDirectionIsForward(direction))
    {
      return NULL;
    }

       
                                                                   
                                 
       
    slot = node->subSlot;
    node->lstate = LIMIT_INWINDOW;
                                                                     
    break;

  case LIMIT_WINDOWSTART:
    if (!ScanDirectionIsForward(direction))
    {
      return NULL;
    }

       
                                                              
                                                          
       
    slot = node->subSlot;
    node->lstate = LIMIT_INWINDOW;
                                                                    
    break;

  default:
    elog(ERROR, "impossible LIMIT state: %d", (int)node->lstate);
    slot = NULL;                          
    break;
  }

                                
  Assert(!TupIsNull(slot));

  return slot;
}

   
                                                                        
   
                                                                        
   
static void
recompute_limits(LimitState *node)
{
  ExprContext *econtext = node->ps.ps_ExprContext;
  Datum val;
  bool isNull;

  if (node->limitOffset)
  {
    val = ExecEvalExprSwitchContext(node->limitOffset, econtext, &isNull);
                                            
    if (isNull)
    {
      node->offset = 0;
    }
    else
    {
      node->offset = DatumGetInt64(val);
      if (node->offset < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_ROW_COUNT_IN_RESULT_OFFSET_CLAUSE), errmsg("OFFSET must not be negative")));
      }
    }
  }
  else
  {
                            
    node->offset = 0;
  }

  if (node->limitCount)
  {
    val = ExecEvalExprSwitchContext(node->limitCount, econtext, &isNull);
                                                      
    if (isNull)
    {
      node->count = 0;
      node->noCount = true;
    }
    else
    {
      node->count = DatumGetInt64(val);
      if (node->count < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_ROW_COUNT_IN_LIMIT_CLAUSE), errmsg("LIMIT must not be negative")));
      }
      node->noCount = false;
    }
  }
  else
  {
                           
    node->count = 0;
    node->noCount = true;
  }

                                       
  node->position = 0;
  node->subSlot = NULL;

                               
  node->lstate = LIMIT_RESCAN;

     
                                                                      
                                                                          
                                                                         
                                              
     
  ExecSetTupleBound(compute_tuples_needed(node), outerPlanState(node));
}

   
                                                                           
                                                                 
   
static int64
compute_tuples_needed(LimitState *node)
{
  if (node->noCount)
  {
    return -1;
  }
                                                                           
  return node->count + node->offset;
}

                                                                    
                  
   
                                                         
                        
                                                                    
   
LimitState *
ExecInitLimit(Limit *node, EState *estate, int eflags)
{
  LimitState *limitstate;
  Plan *outerPlan;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

     
                            
     
  limitstate = makeNode(LimitState);
  limitstate->ps.plan = (Plan *)node;
  limitstate->ps.state = estate;
  limitstate->ps.ExecProcNode = ExecLimit;

  limitstate->lstate = LIMIT_INITIAL;

     
                                  
     
                                                                      
                                                                    
     
  ExecAssignExprContext(estate, &limitstate->ps);

     
                           
     
  outerPlan = outerPlan(node);
  outerPlanState(limitstate) = ExecInitNode(outerPlan, estate, eflags);

     
                                  
     
  limitstate->limitOffset = ExecInitExpr((Expr *)node->limitOffset, (PlanState *)limitstate);
  limitstate->limitCount = ExecInitExpr((Expr *)node->limitCount, (PlanState *)limitstate);

     
                             
     
  ExecInitResultTypeTL(&limitstate->ps);

  limitstate->ps.resultopsset = true;
  limitstate->ps.resultops = ExecGetResultSlotOps(outerPlanState(limitstate), &limitstate->ps.resultopsfixed);

     
                                                                           
                        
     
  limitstate->ps.ps_ProjInfo = NULL;

  return limitstate;
}

                                                                    
                 
   
                                                              
                  
                                                                    
   
void
ExecEndLimit(LimitState *node)
{
  ExecFreeExprContext(&node->ps);
  ExecEndNode(outerPlanState(node));
}

void
ExecReScanLimit(LimitState *node)
{
     
                                                                            
                                                                         
                                                             
     
  recompute_limits(node);

     
                                                                        
                         
     
  if (node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}
