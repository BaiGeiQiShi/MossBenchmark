                                                                            
   
                
                                                      
   
               
   
                                                                     
                                  
   
                   
   
                                             
   
                                                                    
                             
   
                                                                 
                                                                       
             
   
                                    
   
                                        
   
                                 
          
                         
   
                                                                  
                                                             
                                                                 
                                                                 
                                             
   
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeResult.h"
#include "miscadmin.h"
#include "utils/memutils.h"

                                                                    
                     
   
                                                             
                                                         
                                                            
                                      
   
                                                           
                                                                   
                                                          
                                                                    
   
static TupleTableSlot *
ExecResult(PlanState *pstate)
{
  ResultState *node = castNode(ResultState, pstate);
  TupleTableSlot *outerTupleSlot;
  PlanState *outerPlan;
  ExprContext *econtext;

  CHECK_FOR_INTERRUPTS();

  econtext = node->ps.ps_ExprContext;

     
                                                                     
     
  if (node->rs_checkqual)
  {
    bool qualResult = ExecQual(node->resconstantqual, econtext);

    node->rs_checkqual = false;
    if (!qualResult)
    {
      node->rs_done = true;
      return NULL;
    }
  }

     
                                                                      
                                                    
     
  ResetExprContext(econtext);

     
                                                                     
                                                                      
                                                                           
                  
     
  while (!node->rs_done)
  {
    outerPlan = outerPlanState(node);

    if (outerPlan != NULL)
    {
         
                                                                      
         
      outerTupleSlot = ExecProcNode(outerPlan);

      if (TupIsNull(outerTupleSlot))
      {
        return NULL;
      }

         
                                                                         
                                                 
         
      econtext->ecxt_outertuple = outerTupleSlot;
    }
    else
    {
         
                                                                         
                                                                
         
      node->rs_done = true;
    }

                                                                  
    return ExecProject(node->ps.ps_ProjInfo);
  }

  return NULL;
}

                                                                    
                      
                                                                    
   
void
ExecResultMarkPos(ResultState *node)
{
  PlanState *outerPlan = outerPlanState(node);

  if (outerPlan != NULL)
  {
    ExecMarkPos(outerPlan);
  }
  else
  {
    elog(DEBUG2, "Result nodes do not support mark/restore");
  }
}

                                                                    
                       
                                                                    
   
void
ExecResultRestrPos(ResultState *node)
{
  PlanState *outerPlan = outerPlanState(node);

  if (outerPlan != NULL)
  {
    ExecRestrPos(outerPlan);
  }
  else
  {
    elog(ERROR, "Result nodes do not support mark/restore");
  }
}

                                                                    
                   
   
                                                               
                                                            
                   
                                                                    
   
ResultState *
ExecInitResult(Result *node, EState *estate, int eflags)
{
  ResultState *resstate;

                                   
  Assert(!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)) || outerPlan(node) != NULL);

     
                            
     
  resstate = makeNode(ResultState);
  resstate->ps.plan = (Plan *)node;
  resstate->ps.state = estate;
  resstate->ps.ExecProcNode = ExecResult;

  resstate->rs_done = false;
  resstate->rs_checkqual = (node->resconstantqual == NULL) ? false : true;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &resstate->ps);

     
                            
     
  outerPlanState(resstate) = ExecInitNode(outerPlan(node), estate, eflags);

     
                             
     
  Assert(innerPlan(node) == NULL);

     
                                                  
     
  ExecInitResultTupleSlotTL(&resstate->ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&resstate->ps, NULL);

     
                                  
     
  resstate->ps.qual = ExecInitQual(node->plan.qual, (PlanState *)resstate);
  resstate->resconstantqual = ExecInitQual((List *)node->resconstantqual, (PlanState *)resstate);

  return resstate;
}

                                                                    
                  
   
                                                  
                                                                    
   
void
ExecEndResult(ResultState *node)
{
     
                          
     
  ExecFreeExprContext(&node->ps);

     
                               
     
  ExecClearTuple(node->ps.ps_ResultTupleSlot);

     
                        
     
  ExecEndNode(outerPlanState(node));
}

void
ExecReScanResult(ResultState *node)
{
  node->rs_done = false;
  node->rs_checkqual = (node->resconstantqual == NULL) ? false : true;

     
                                                                        
                         
     
  if (node->ps.lefttree && node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}
