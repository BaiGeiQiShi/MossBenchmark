                                                                            
   
               
                                                                             
   
                                                                         
                                                                        
   
   
               
                                                                             
                                                                     
                                                                           
                                                                         
                              
   
                  
                                      
   
                                                                            
   

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeGroup.h"
#include "miscadmin.h"
#include "utils/memutils.h"

   
                
   
                                                              
   
static TupleTableSlot *
ExecGroup(PlanState *pstate)
{
  GroupState *node = castNode(GroupState, pstate);
  ExprContext *econtext;
  TupleTableSlot *firsttupleslot;
  TupleTableSlot *outerslot;

  CHECK_FOR_INTERRUPTS();

     
                              
     
  if (node->grp_done)
  {
    return NULL;
  }
  econtext = node->ss.ps.ps_ExprContext;

     
                                                                     
     
  firsttupleslot = node->ss.ss_ScanTupleSlot;

     
                                                                            
                                                              
     

     
                                                                            
                          
     
  if (TupIsNull(firsttupleslot))
  {
    outerslot = ExecProcNode(outerPlanState(node));
    if (TupIsNull(outerslot))
    {
                                          
      node->grp_done = true;
      return NULL;
    }
                                        
    ExecCopySlot(firsttupleslot, outerslot);

       
                                                                         
                                                   
       
    econtext->ecxt_outertuple = firsttupleslot;

       
                                                                           
                                   
       
    if (ExecQual(node->ss.ps.qual, econtext))
    {
         
                                                                         
         
      return ExecProject(node->ss.ps.ps_ProjInfo);
    }
    else
    {
      InstrCountFiltered1(node, 1);
    }
  }

     
                                                                        
                                                                            
                                                    
     
  for (;;)
  {
       
                                                                
       
    for (;;)
    {
      outerslot = ExecProcNode(outerPlanState(node));
      if (TupIsNull(outerslot))
      {
                                           
        node->grp_done = true;
        return NULL;
      }

         
                                                                       
                                                     
         
      econtext->ecxt_innertuple = firsttupleslot;
      econtext->ecxt_outertuple = outerslot;
      if (!ExecQualAndReset(node->eqfunction, econtext))
      {
        break;
      }
    }

       
                                                                           
                  
       
                                                                  
    ExecCopySlot(firsttupleslot, outerslot);
    econtext->ecxt_outertuple = firsttupleslot;

       
                                                                           
                                                       
       
    if (ExecQual(node->ss.ps.qual, econtext))
    {
         
                                                                         
         
      return ExecProject(node->ss.ps.ps_ProjInfo);
    }
    else
    {
      InstrCountFiltered1(node, 1);
    }
  }
}

                     
                 
   
                                                                       
                                             
                     
   
GroupState *
ExecInitGroup(Group *node, EState *estate, int eflags)
{
  GroupState *grpstate;
  const TupleTableSlotOps *tts_ops;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  grpstate = makeNode(GroupState);
  grpstate->ss.ps.plan = (Plan *)node;
  grpstate->ss.ps.state = estate;
  grpstate->ss.ps.ExecProcNode = ExecGroup;
  grpstate->grp_done = false;

     
                               
     
  ExecAssignExprContext(estate, &grpstate->ss.ps);

     
                            
     
  outerPlanState(grpstate) = ExecInitNode(outerPlan(node), estate, eflags);

     
                                    
     
  tts_ops = ExecGetResultSlotOps(outerPlanState(&grpstate->ss), NULL);
  ExecCreateScanSlotFromOuterPlan(estate, &grpstate->ss, tts_ops);

     
                                                  
     
  ExecInitResultTupleSlotTL(&grpstate->ss.ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&grpstate->ss.ps, NULL);

     
                                  
     
  grpstate->ss.ps.qual = ExecInitQual(node->plan.qual, (PlanState *)grpstate);

     
                                                
     
  grpstate->eqfunction = execTuplesMatchPrepare(ExecGetResultType(outerPlanState(grpstate)), node->numCols, node->grpColIdx, node->grpOperators, node->grpCollations, &grpstate->ss.ps);

  return grpstate;
}

                            
                       
   
                           
   
void
ExecEndGroup(GroupState *node)
{
  PlanState *outerPlan;

  ExecFreeExprContext(&node->ss.ps);

                            
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

  outerPlan = outerPlanState(node);
  ExecEndNode(outerPlan);
}

void
ExecReScanGroup(GroupState *node)
{
  PlanState *outerPlan = outerPlanState(node);

  node->grp_done = false;
                              
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                                                        
                         
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }
}
