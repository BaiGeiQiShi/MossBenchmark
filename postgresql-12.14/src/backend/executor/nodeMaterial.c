                                                                            
   
                  
                                               
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
   
                      
                                                         
                                                     
                                                   
   
   
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeMaterial.h"
#include "miscadmin.h"

                                                                    
                 
   
                                                                          
                                                                       
                                                                    
                                                                          
   
                                                                    
   
static TupleTableSlot *                                
ExecMaterial(PlanState *pstate)
{
  MaterialState *node = castNode(MaterialState, pstate);
  EState *estate;
  ScanDirection dir;
  bool forward;
  Tuplestorestate *tuplestorestate;
  bool eof_tuplestore;
  TupleTableSlot *slot;

  CHECK_FOR_INTERRUPTS();

     
                              
     
  estate = node->ss.ps.state;
  dir = estate->es_direction;
  forward = ScanDirectionIsForward(dir);
  tuplestorestate = node->tuplestorestate;

     
                                                                     
     
  if (tuplestorestate == NULL && node->eflags != 0)
  {
    tuplestorestate = tuplestore_begin_heap(true, false, work_mem);
    tuplestore_set_eflags(tuplestorestate, node->eflags);
    if (node->eflags & EXEC_FLAG_MARK)
    {
         
                                                                         
                                                   
         
      int ptrno PG_USED_FOR_ASSERTS_ONLY;

      ptrno = tuplestore_alloc_read_pointer(tuplestorestate, node->eflags);
      Assert(ptrno == 1);
    }
    node->tuplestorestate = tuplestorestate;
  }

     
                                                                             
                                       
     
  eof_tuplestore = (tuplestorestate == NULL) || tuplestore_ateof(tuplestorestate);

  if (!forward && eof_tuplestore)
  {
    if (!node->eof_underlying)
    {
         
                                                               
                                                                        
                                                                    
                
         
      if (!tuplestore_advance(tuplestorestate, forward))
      {
        return NULL;                                   
      }
    }
    eof_tuplestore = false;
  }

     
                                                                   
     
  slot = node->ss.ps.ps_ResultTupleSlot;
  if (!eof_tuplestore)
  {
    if (tuplestore_gettupleslot(tuplestorestate, forward, false, slot))
    {
      return slot;
    }
    if (forward)
    {
      eof_tuplestore = true;
    }
  }

     
                                                              
     
                                                                             
                                                                         
                                                                             
                    
     
  if (eof_tuplestore && !node->eof_underlying)
  {
    PlanState *outerNode;
    TupleTableSlot *outerslot;

       
                                                                          
                                            
       
    outerNode = outerPlanState(node);
    outerslot = ExecProcNode(outerNode);
    if (TupIsNull(outerslot))
    {
      node->eof_underlying = true;
      return NULL;
    }

       
                                                                         
                                                                        
                                                                 
       
    if (tuplestorestate)
    {
      tuplestore_puttupleslot(tuplestorestate, outerslot);
    }

    ExecCopySlot(slot, outerslot);
    return slot;
  }

     
                      
     
  return ExecClearTuple(slot);
}

                                                                    
                     
                                                                    
   
MaterialState *
ExecInitMaterial(Material *node, EState *estate, int eflags)
{
  MaterialState *matstate;
  Plan *outerPlan;

     
                            
     
  matstate = makeNode(MaterialState);
  matstate->ss.ps.plan = (Plan *)node;
  matstate->ss.ps.state = estate;
  matstate->ss.ps.ExecProcNode = ExecMaterial;

     
                                                                           
                                                                             
                                                                           
                                                                 
     
  matstate->eflags = (eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK));

     
                                                                           
                                                                         
                                                                             
                                                                        
                                
     
  if (eflags & EXEC_FLAG_BACKWARD)
  {
    matstate->eflags |= EXEC_FLAG_REWIND;
  }

  matstate->eof_underlying = false;
  matstate->tuplestorestate = NULL;

     
                                  
     
                                                                           
                              
     

     
                            
     
                                                                            
                   
     
  eflags &= ~(EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK);

  outerPlan = outerPlan(node);
  outerPlanState(matstate) = ExecInitNode(outerPlan, estate, eflags);

     
                                                                            
                                               
     
                                                                         
     
  ExecInitResultTupleSlotTL(&matstate->ss.ps, &TTSOpsMinimalTuple);
  matstate->ss.ps.ps_ProjInfo = NULL;

     
                            
     
  ExecCreateScanSlotFromOuterPlan(estate, &matstate->ss, &TTSOpsMinimalTuple);

  return matstate;
}

                                                                    
                    
                                                                    
   
void
ExecEndMaterial(MaterialState *node)
{
     
                               
     
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                  
     
  if (node->tuplestorestate != NULL)
  {
    tuplestore_end(node->tuplestorestate);
  }
  node->tuplestorestate = NULL;

     
                           
     
  ExecEndNode(outerPlanState(node));
}

                                                                    
                        
   
                                                                      
                                                                    
   
void
ExecMaterialMarkPos(MaterialState *node)
{
  Assert(node->eflags & EXEC_FLAG_MARK);

     
                                                  
     
  if (!node->tuplestorestate)
  {
    return;
  }

     
                                               
     
  tuplestore_copy_read_pointer(node->tuplestorestate, 0, 1);

     
                                                                          
     
  tuplestore_trim(node->tuplestorestate);
}

                                                                    
                         
   
                                                              
                                                                    
   
void
ExecMaterialRestrPos(MaterialState *node)
{
  Assert(node->eflags & EXEC_FLAG_MARK);

     
                                                  
     
  if (!node->tuplestorestate)
  {
    return;
  }

     
                                               
     
  tuplestore_copy_read_pointer(node->tuplestorestate, 1, 0);
}

                                                                    
                       
   
                                       
                                                                    
   
void
ExecReScanMaterial(MaterialState *node)
{
  PlanState *outerPlan = outerPlanState(node);

  ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

  if (node->eflags != 0)
  {
       
                                                                   
                                                                        
                                            
       
    if (!node->tuplestorestate)
    {
      return;
    }

       
                                                                    
                                                                          
                                                                   
                                                                        
                                                           
       
                                                                      
                                             
       
    if (outerPlan->chgParam != NULL || (node->eflags & EXEC_FLAG_REWIND) == 0)
    {
      tuplestore_end(node->tuplestorestate);
      node->tuplestorestate = NULL;
      if (outerPlan->chgParam == NULL)
      {
        ExecReScan(outerPlan);
      }
      node->eof_underlying = false;
    }
    else
    {
      tuplestore_rescan(node->tuplestorestate);
    }
  }
  else
  {
                                                                   

       
                                                                          
                           
       
    if (outerPlan->chgParam == NULL)
    {
      ExecReScan(outerPlan);
    }
    node->eof_underlying = false;
  }
}
