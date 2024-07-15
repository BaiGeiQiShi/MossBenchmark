                                                                            
   
                
                                                                
   
                                                                     
                                                                             
                                                                       
                                                                        
                                                                        
                                                                        
                   
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
   
                      
                                                         
                                                  
                                               
   
         
                                                   
                  
   

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeUnique.h"
#include "miscadmin.h"
#include "utils/memutils.h"

                                                                    
               
                                                                    
   
static TupleTableSlot *                              
ExecUnique(PlanState *pstate)
{
  UniqueState *node = castNode(UniqueState, pstate);
  ExprContext *econtext = node->ps.ps_ExprContext;
  TupleTableSlot *resultTupleSlot;
  TupleTableSlot *slot;
  PlanState *outerPlan;

  CHECK_FOR_INTERRUPTS();

     
                                   
     
  outerPlan = outerPlanState(node);
  resultTupleSlot = node->ps.ps_ResultTupleSlot;

     
                                                                       
                                                                           
                                            
     
  for (;;)
  {
       
                                            
       
    slot = ExecProcNode(outerPlan);
    if (TupIsNull(slot))
    {
                                         
      ExecClearTuple(resultTupleSlot);
      return NULL;
    }

       
                                                       
       
    if (TupIsNull(resultTupleSlot))
    {
      break;
    }

       
                                                                           
                                                                    
                
       
    econtext->ecxt_innertuple = slot;
    econtext->ecxt_outertuple = resultTupleSlot;
    if (!ExecQualAndReset(node->eqfunction, econtext))
    {
      break;
    }
  }

     
                                                                           
                                                                        
                                                                      
                                     
     
  return ExecCopySlot(resultTupleSlot, slot);
}

                                                                    
                   
   
                                                          
                        
                                                                    
   
UniqueState *
ExecInitUnique(Unique *node, EState *estate, int eflags)
{
  UniqueState *uniquestate;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  uniquestate = makeNode(UniqueState);
  uniquestate->ps.plan = (Plan *)node;
  uniquestate->ps.state = estate;
  uniquestate->ps.ExecProcNode = ExecUnique;

     
                               
     
  ExecAssignExprContext(estate, &uniquestate->ps);

     
                                
     
  outerPlanState(uniquestate) = ExecInitNode(outerPlan(node), estate, eflags);

     
                                                                         
                                                             
     
  ExecInitResultTupleSlotTL(&uniquestate->ps, &TTSOpsMinimalTuple);
  uniquestate->ps.ps_ProjInfo = NULL;

     
                                                
     
  uniquestate->eqfunction = execTuplesMatchPrepare(ExecGetResultType(outerPlanState(uniquestate)), node->numCols, node->uniqColIdx, node->uniqOperators, node->uniqCollations, &uniquestate->ps);

  return uniquestate;
}

                                                                    
                  
   
                                                              
                  
                                                                    
   
void
ExecEndUnique(UniqueState *node)
{
                            
  ExecClearTuple(node->ps.ps_ResultTupleSlot);

  ExecFreeExprContext(&node->ps);

  ExecEndNode(outerPlanState(node));
}

void
ExecReScanUnique(UniqueState *node)
{
                                                                
  ExecClearTuple(node->ps.ps_ResultTupleSlot);

     
                                                                        
                         
     
  if (node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}
