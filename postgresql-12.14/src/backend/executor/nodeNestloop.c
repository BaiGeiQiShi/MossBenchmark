                                                                            
   
                  
                                         
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
   
                       
                                                         
                                           
                                          
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeNestloop.h"
#include "miscadmin.h"
#include "utils/memutils.h"

                                                                    
                       
   
                
                                                               
                                        
   
                                                                  
   
                                                                      
                                                                       
                          
   
                                                                     
                                            
   
                                                                    
   
                
                                                                   
                                                                    
                           
                                                                   
                
   
                    
                                             
                                                
                                                                    
   
static TupleTableSlot *
ExecNestLoop(PlanState *pstate)
{
  NestLoopState *node = castNode(NestLoopState, pstate);
  NestLoop *nl;
  PlanState *innerPlan;
  PlanState *outerPlan;
  TupleTableSlot *outerTupleSlot;
  TupleTableSlot *innerTupleSlot;
  ExprState *joinqual;
  ExprState *otherqual;
  ExprContext *econtext;
  ListCell *lc;

  CHECK_FOR_INTERRUPTS();

     
                                   
     
  ENL1_printf("getting info from node");

  nl = (NestLoop *)node->js.ps.plan;
  joinqual = node->js.joinqual;
  otherqual = node->js.ps.qual;
  outerPlan = outerPlanState(node);
  innerPlan = innerPlanState(node);
  econtext = node->js.ps.ps_ExprContext;

     
                                                                      
                                                    
     
  ResetExprContext(econtext);

     
                                                                        
                            
     
  ENL1_printf("entering main loop");

  for (;;)
  {
       
                                                                       
                   
       
    if (node->nl_NeedNewOuter)
    {
      ENL1_printf("getting new outer tuple");
      outerTupleSlot = ExecProcNode(outerPlan);

         
                                                                        
         
      if (TupIsNull(outerTupleSlot))
      {
        ENL1_printf("no outer tuple, ending join");
        return NULL;
      }

      ENL1_printf("saving new outer tuple information");
      econtext->ecxt_outertuple = outerTupleSlot;
      node->nl_NeedNewOuter = false;
      node->nl_MatchedOuter = false;

         
                                                                       
                                                                         
         
      foreach (lc, nl->nestParams)
      {
        NestLoopParam *nlp = (NestLoopParam *)lfirst(lc);
        int paramno = nlp->paramno;
        ParamExecData *prm;

        prm = &(econtext->ecxt_param_exec_vals[paramno]);
                                                    
        Assert(IsA(nlp->paramval, Var));
        Assert(nlp->paramval->varno == OUTER_VAR);
        Assert(nlp->paramval->varattno > 0);
        prm->value = slot_getattr(outerTupleSlot, nlp->paramval->varattno, &(prm->isnull));
                                             
        innerPlan->chgParam = bms_add_member(innerPlan->chgParam, paramno);
      }

         
                                   
         
      ENL1_printf("rescanning inner plan");
      ExecReScan(innerPlan);
    }

       
                                                               
       
    ENL1_printf("getting new inner tuple");

    innerTupleSlot = ExecProcNode(innerPlan);
    econtext->ecxt_innertuple = innerTupleSlot;

    if (TupIsNull(innerTupleSlot))
    {
      ENL1_printf("no inner tuple, need new outer tuple");

      node->nl_NeedNewOuter = true;

      if (!node->nl_MatchedOuter && (node->js.jointype == JOIN_LEFT || node->js.jointype == JOIN_ANTI))
      {
           
                                                                     
                                                                  
                                                                     
                           
           
        econtext->ecxt_innertuple = node->nl_NullInnerTupleSlot;

        ENL1_printf("testing qualification for outer-join tuple");

        if (otherqual == NULL || ExecQual(otherqual, econtext))
        {
             
                                                                  
                                                        
                            
             
          ENL1_printf("qualification succeeded, projecting tuple");

          return ExecProject(node->js.ps.ps_ProjInfo);
        }
        else
        {
          InstrCountFiltered2(node, 1);
        }
      }

         
                                                                     
         
      continue;
    }

       
                                                                        
                                                                         
                      
       
                                                                       
                                               
       
    ENL1_printf("testing qualification");

    if (ExecQual(joinqual, econtext))
    {
      node->nl_MatchedOuter = true;

                                                           
      if (node->js.jointype == JOIN_ANTI)
      {
        node->nl_NeedNewOuter = true;
        continue;                            
      }

         
                                                                         
                                                                        
                      
         
      if (node->js.single_match)
      {
        node->nl_NeedNewOuter = true;
      }

      if (otherqual == NULL || ExecQual(otherqual, econtext))
      {
           
                                                                    
                                                                 
           
        ENL1_printf("qualification succeeded, projecting tuple");

        return ExecProject(node->js.ps.ps_ProjInfo);
      }
      else
      {
        InstrCountFiltered2(node, 1);
      }
    }
    else
    {
      InstrCountFiltered1(node, 1);
    }

       
                                                                 
       
    ResetExprContext(econtext);

    ENL1_printf("qualification failed, looping");
  }
}

                                                                    
                     
                                                                    
   
NestLoopState *
ExecInitNestLoop(NestLoop *node, EState *estate, int eflags)
{
  NestLoopState *nlstate;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

  NL1_printf("ExecInitNestLoop: %s\n", "initializing node");

     
                            
     
  nlstate = makeNode(NestLoopState);
  nlstate->js.ps.plan = (Plan *)node;
  nlstate->js.ps.state = estate;
  nlstate->js.ps.ExecProcNode = ExecNestLoop;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &nlstate->js.ps);

     
                            
     
                                                                         
                                                                           
                                                                             
                                                                           
             
     
  outerPlanState(nlstate) = ExecInitNode(outerPlan(node), estate, eflags);
  if (node->nestParams == NIL)
  {
    eflags |= EXEC_FLAG_REWIND;
  }
  else
  {
    eflags &= ~EXEC_FLAG_REWIND;
  }
  innerPlanState(nlstate) = ExecInitNode(innerPlan(node), estate, eflags);

     
                                                  
     
  ExecInitResultTupleSlotTL(&nlstate->js.ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&nlstate->js.ps, NULL);

     
                                  
     
  nlstate->js.ps.qual = ExecInitQual(node->join.plan.qual, (PlanState *)nlstate);
  nlstate->js.jointype = node->join.jointype;
  nlstate->js.joinqual = ExecInitQual(node->join.joinqual, (PlanState *)nlstate);

     
                                                                         
     
  nlstate->js.single_match = (node->join.inner_unique || node->join.jointype == JOIN_SEMI);

                                                     
  switch (node->join.jointype)
  {
  case JOIN_INNER:
  case JOIN_SEMI:
    break;
  case JOIN_LEFT:
  case JOIN_ANTI:
    nlstate->nl_NullInnerTupleSlot = ExecInitNullTupleSlot(estate, ExecGetResultType(innerPlanState(nlstate)), &TTSOpsVirtual);
    break;
  default:
    elog(ERROR, "unrecognized join type: %d", (int)node->join.jointype);
  }

     
                                                  
     
  nlstate->nl_NeedNewOuter = true;
  nlstate->nl_MatchedOuter = false;

  NL1_printf("ExecInitNestLoop: %s\n", "node initialized");

  return nlstate;
}

                                                                    
                    
   
                                                  
                                                                    
   
void
ExecEndNestLoop(NestLoopState *node)
{
  NL1_printf("ExecEndNestLoop: %s\n", "ending node processing");

     
                          
     
  ExecFreeExprContext(&node->js.ps);

     
                               
     
  ExecClearTuple(node->js.ps.ps_ResultTupleSlot);

     
                         
     
  ExecEndNode(outerPlanState(node));
  ExecEndNode(innerPlanState(node));

  NL1_printf("ExecEndNestLoop: %s\n", "node processing ended");
}

                                                                    
                       
                                                                    
   
void
ExecReScanNestLoop(NestLoopState *node)
{
  PlanState *outerPlan = outerPlanState(node);

     
                                                                        
                                       
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }

     
                                                                      
                                                                             
                                             
     

  node->nl_NeedNewOuter = true;
  node->nl_MatchedOuter = false;
}
