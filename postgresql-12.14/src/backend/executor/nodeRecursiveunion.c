                                                                            
   
                        
                                              
   
                                                                            
                                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                               
   
                                                                            
   
#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeRecursiveunion.h"
#include "miscadmin.h"
#include "utils/memutils.h"

   
                                       
   
static void
build_hash_table(RecursiveUnionState *rustate)
{
  RecursiveUnion *node = (RecursiveUnion *)rustate->ps.plan;
  TupleDesc desc = ExecGetResultType(outerPlanState(rustate));

  Assert(node->numCols > 0);
  Assert(node->numGroups > 0);

  rustate->hashtable = BuildTupleHashTableExt(&rustate->ps, desc, node->numCols, node->dupColIdx, rustate->eqfuncoids, rustate->hashfunctions, node->dupCollations, node->numGroups, 0, rustate->ps.state->es_query_cxt, rustate->tableContext, rustate->tempContext, false);
}

                                                                    
                             
   
                                                                
                      
   
                                                              
   
                              
   
                
                                                                          
                                                  
                                                     
                       
                      
                                                                    
   
static TupleTableSlot *
ExecRecursiveUnion(PlanState *pstate)
{
  RecursiveUnionState *node = castNode(RecursiveUnionState, pstate);
  PlanState *outerPlan = outerPlanState(node);
  PlanState *innerPlan = innerPlanState(node);
  RecursiveUnion *plan = (RecursiveUnion *)node->ps.plan;
  TupleTableSlot *slot;
  bool isnew;

  CHECK_FOR_INTERRUPTS();

                                      
  if (!node->recursing)
  {
    for (;;)
    {
      slot = ExecProcNode(outerPlan);
      if (TupIsNull(slot))
      {
        break;
      }
      if (plan->numCols > 0)
      {
                                                                  
        LookupTupleHashEntry(node->hashtable, slot, &isnew);
                                                                 
        MemoryContextReset(node->tempContext);
                                          
        if (!isnew)
        {
          continue;
        }
      }
                                                                  
      tuplestore_puttupleslot(node->working_table, slot);
                                 
      return slot;
    }
    node->recursing = true;
  }

                                 
  for (;;)
  {
    slot = ExecProcNode(innerPlan);
    if (TupIsNull(slot))
    {
                                                             
      if (node->intermediate_empty)
      {
        break;
      }

                                           
      tuplestore_end(node->working_table);

                                                    
      node->working_table = node->intermediate_table;

                                               
      node->intermediate_table = tuplestore_begin_heap(false, false, work_mem);
      node->intermediate_empty = true;

                                    
      innerPlan->chgParam = bms_add_member(innerPlan->chgParam, plan->wtParam);

                                                     
      continue;
    }

    if (plan->numCols > 0)
    {
                                                                
      LookupTupleHashEntry(node->hashtable, slot, &isnew);
                                                               
      MemoryContextReset(node->tempContext);
                                        
      if (!isnew)
      {
        continue;
      }
    }

                                                                 
    node->intermediate_empty = false;
    tuplestore_puttupleslot(node->intermediate_table, slot);
                           
    return slot;
  }

  return NULL;
}

                                                                    
                               
                                                                    
   
RecursiveUnionState *
ExecInitRecursiveUnion(RecursiveUnion *node, EState *estate, int eflags)
{
  RecursiveUnionState *rustate;
  ParamExecData *prmdata;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  rustate = makeNode(RecursiveUnionState);
  rustate->ps.plan = (Plan *)node;
  rustate->ps.state = estate;
  rustate->ps.ExecProcNode = ExecRecursiveUnion;

  rustate->eqfuncoids = NULL;
  rustate->hashfunctions = NULL;
  rustate->hashtable = NULL;
  rustate->tempContext = NULL;
  rustate->tableContext = NULL;

                                   
  rustate->recursing = false;
  rustate->intermediate_empty = true;
  rustate->working_table = tuplestore_begin_heap(false, false, work_mem);
  rustate->intermediate_table = tuplestore_begin_heap(false, false, work_mem);

     
                                                                           
                                                                            
                                                                          
                           
     
  if (node->numCols > 0)
  {
    rustate->tempContext = AllocSetContextCreate(CurrentMemoryContext, "RecursiveUnion", ALLOCSET_DEFAULT_SIZES);
    rustate->tableContext = AllocSetContextCreate(CurrentMemoryContext, "RecursiveUnion hash table", ALLOCSET_DEFAULT_SIZES);
  }

     
                                                                          
                                         
     
  prmdata = &(estate->es_param_exec_vals[node->wtParam]);
  Assert(prmdata->execPlan == NULL);
  prmdata->value = PointerGetDatum(rustate);
  prmdata->isnull = false;

     
                                  
     
                                                                            
                                   
     
  Assert(node->plan.qual == NIL);

     
                                                                          
                                            
     
  ExecInitResultTypeTL(&rustate->ps);

     
                                                                             
                                                                             
                   
     
  rustate->ps.ps_ProjInfo = NULL;

     
                            
     
  outerPlanState(rustate) = ExecInitNode(outerPlan(node), estate, eflags);
  innerPlanState(rustate) = ExecInitNode(innerPlan(node), estate, eflags);

     
                                                                            
                 
     
  if (node->numCols > 0)
  {
    execTuplesHashPrepare(node->numCols, node->dupOperators, &rustate->eqfuncoids, &rustate->hashfunctions);
    build_hash_table(rustate);
  }

  return rustate;
}

                                                                    
                              
   
                                                    
                                                                    
   
void
ExecEndRecursiveUnion(RecursiveUnionState *node)
{
                           
  tuplestore_end(node->working_table);
  tuplestore_end(node->intermediate_table);

                                                 
  if (node->tempContext)
  {
    MemoryContextDelete(node->tempContext);
  }
  if (node->tableContext)
  {
    MemoryContextDelete(node->tableContext);
  }

     
                         
     
  ExecEndNode(outerPlanState(node));
  ExecEndNode(innerPlanState(node));
}

                                                                    
                             
   
                          
                                                                    
   
void
ExecReScanRecursiveUnion(RecursiveUnionState *node)
{
  PlanState *outerPlan = outerPlanState(node);
  PlanState *innerPlan = innerPlanState(node);
  RecursiveUnion *plan = (RecursiveUnion *)node->ps.plan;

     
                                                                            
                                           
     
  innerPlan->chgParam = bms_add_member(innerPlan->chgParam, plan->wtParam);

     
                                                                        
                                                                           
                         
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }

                                     
  if (node->tableContext)
  {
    MemoryContextResetAndDeleteChildren(node->tableContext);
  }

                                 
  if (plan->numCols > 0)
  {
    ResetTupleHashTable(node->hashtable);
  }

                              
  node->recursing = false;
  node->intermediate_empty = true;
  tuplestore_clear(node->working_table);
  tuplestore_clear(node->intermediate_table);
}
