                                                                            
   
                
                                      
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
                      
                                                
                                                        
                                              
                                              
   
          
                                                                   
                                                           
                                                                
                                                                  
                                               
   
                                                        
                                                         
                                                            
   
             
           
                                           
                      
                                   
                    
   
                                                               
                                                                     
                                                                     
                                                            
                                                            
           
   
                              
   
                        
   
          
                                                
                             
                                           
                           
                                             
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/execPartition.h"
#include "executor/nodeAppend.h"
#include "miscadmin.h"

                                             
struct ParallelAppendState
{
  LWLock pa_lock;                                                
  int pa_next_plan;                                        

     
                                                                            
                                                                         
                                                                         
                                                  
     
  bool pa_finished[FLEXIBLE_ARRAY_MEMBER];
};

#define INVALID_SUBPLAN_INDEX -1
#define NO_MATCHING_SUBPLANS -2

static TupleTableSlot *
ExecAppend(PlanState *pstate);
static bool
choose_next_subplan_locally(AppendState *node);
static bool
choose_next_subplan_for_leader(AppendState *node);
static bool
choose_next_subplan_for_worker(AppendState *node);
static void
mark_invalid_subplans_as_finished(AppendState *node);

                                                                    
                   
   
                                                  
   
                                                                    
                                                            
                                                                
                                                      
                                                                    
   
AppendState *
ExecInitAppend(Append *node, EState *estate, int eflags)
{
  AppendState *appendstate = makeNode(AppendState);
  PlanState **appendplanstates;
  Bitmapset *validsubplans;
  int nplans;
  int firstvalid;
  int i, j;
  ListCell *lc;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

     
                                                
     
  appendstate->ps.plan = (Plan *)node;
  appendstate->ps.state = estate;
  appendstate->ps.ExecProcNode = ExecAppend;

                                                                           
  appendstate->as_whichplan = INVALID_SUBPLAN_INDEX;

                                                                      
  if (node->part_prune_info != NULL)
  {
    PartitionPruneState *prunestate;

                                                                       
    ExecAssignExprContext(estate, &appendstate->ps);

                                                        
    prunestate = ExecCreatePartitionPruneState(&appendstate->ps, node->part_prune_info);
    appendstate->as_prune_state = prunestate;

                                                          
    if (prunestate->do_initial_prune)
    {
                                                            
      validsubplans = ExecFindInitialMatchingSubPlans(prunestate, list_length(node->appendplans));

         
                                                                    
                                                                         
                                                                   
                                                                       
                                                                        
                                                                       
                                                    
         
      if (bms_is_empty(validsubplans))
      {
        appendstate->as_whichplan = NO_MATCHING_SUBPLANS;

                                                                    
        validsubplans = bms_make_singleton(0);
      }

      nplans = bms_num_members(validsubplans);
    }
    else
    {
                                                 
      nplans = list_length(node->appendplans);
      Assert(nplans > 0);
      validsubplans = bms_add_range(NULL, 0, nplans - 1);
    }

       
                                                                        
                                                                        
       
    if (!prunestate->do_exec_prune)
    {
      Assert(nplans > 0);
      appendstate->as_valid_subplans = bms_add_range(NULL, 0, nplans - 1);
    }
  }
  else
  {
    nplans = list_length(node->appendplans);

       
                                                                           
                                                             
       
    Assert(nplans > 0);
    appendstate->as_valid_subplans = validsubplans = bms_add_range(NULL, 0, nplans - 1);
    appendstate->as_prune_state = NULL;
  }

     
                                            
     
  ExecInitResultTupleSlotTL(&appendstate->ps, &TTSOpsVirtual);

                                                                         
  appendstate->ps.resultopsset = true;
  appendstate->ps.resultopsfixed = false;

  appendplanstates = (PlanState **)palloc(nplans * sizeof(PlanState *));

     
                                                                          
                                                  
     
                                                         
     
  j = i = 0;
  firstvalid = nplans;
  foreach (lc, node->appendplans)
  {
    if (bms_is_member(i, validsubplans))
    {
      Plan *initNode = (Plan *)lfirst(lc);

         
                                                                      
               
         
      if (i >= node->first_partial_plan && j < firstvalid)
      {
        firstvalid = j;
      }

      appendplanstates[j++] = ExecInitNode(initNode, estate, eflags);
    }
    i++;
  }

  appendstate->as_first_partial_plan = firstvalid;
  appendstate->appendplans = appendplanstates;
  appendstate->as_nplans = nplans;

     
                                  
     

  appendstate->ps.ps_ProjInfo = NULL;

                                                          
  appendstate->choose_next_subplan = choose_next_subplan_locally;

  return appendstate;
}

                                                                    
                 
   
                                              
                                                                    
   
static TupleTableSlot *
ExecAppend(PlanState *pstate)
{
  AppendState *node = castNode(AppendState, pstate);

  if (node->as_whichplan < 0)
  {
       
                                                                
                   
       
    if (node->as_whichplan == INVALID_SUBPLAN_INDEX && !node->choose_next_subplan(node))
    {
      return ExecClearTuple(node->ps.ps_ResultTupleSlot);
    }

                                                         
    else if (node->as_whichplan == NO_MATCHING_SUBPLANS)
    {
      return ExecClearTuple(node->ps.ps_ResultTupleSlot);
    }
  }

  for (;;)
  {
    PlanState *subnode;
    TupleTableSlot *result;

    CHECK_FOR_INTERRUPTS();

       
                                                            
       
    Assert(node->as_whichplan >= 0 && node->as_whichplan < node->as_nplans);
    subnode = node->appendplans[node->as_whichplan];

       
                                    
       
    result = ExecProcNode(subnode);

    if (!TupIsNull(result))
    {
         
                                                                      
                                                            
                                                 
         
      return result;
    }

                                                 
    if (!node->choose_next_subplan(node))
    {
      return ExecClearTuple(node->ps.ps_ResultTupleSlot);
    }
  }
}

                                                                    
                  
   
                                                
   
                                 
                                                                    
   
void
ExecEndAppend(AppendState *node)
{
  PlanState **appendplans;
  int nplans;
  int i;

     
                                   
     
  appendplans = node->appendplans;
  nplans = node->as_nplans;

     
                                    
     
  for (i = 0; i < nplans; i++)
  {
    ExecEndNode(appendplans[i]);
  }
}

void
ExecReScanAppend(AppendState *node)
{
  int i;

     
                                                                             
                                                                          
                               
     
  if (node->as_prune_state && bms_overlap(node->ps.chgParam, node->as_prune_state->execparamids))
  {
    bms_free(node->as_valid_subplans);
    node->as_valid_subplans = NULL;
  }

  for (i = 0; i < node->as_nplans; i++)
  {
    PlanState *subnode = node->appendplans[i];

       
                                                                  
                                           
       
    if (node->ps.chgParam != NULL)
    {
      UpdateChangedParamSet(subnode, node->ps.chgParam);
    }

       
                                                                          
                           
       
    if (subnode->chgParam == NULL)
    {
      ExecReScan(subnode);
    }
  }

                                                                           
  node->as_whichplan = INVALID_SUBPLAN_INDEX;
}

                                                                    
                                
                                                                    
   

                                                                    
                       
   
                                                           
                                                           
                                                                    
   
void
ExecAppendEstimate(AppendState *node, ParallelContext *pcxt)
{
  node->pstate_len = add_size(offsetof(ParallelAppendState, pa_finished), sizeof(bool) * node->as_nplans);

  shm_toc_estimate_chunk(&pcxt->estimator, node->pstate_len);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

                                                                    
                            
   
                                             
                                                                    
   
void
ExecAppendInitializeDSM(AppendState *node, ParallelContext *pcxt)
{
  ParallelAppendState *pstate;

  pstate = shm_toc_allocate(pcxt->toc, node->pstate_len);
  memset(pstate, 0, node->pstate_len);
  LWLockInitialize(&pstate->pa_lock, LWTRANCHE_PARALLEL_APPEND);
  shm_toc_insert(pcxt->toc, node->ps.plan->plan_node_id, pstate);

  node->as_pstate = pstate;
  node->choose_next_subplan = choose_next_subplan_for_leader;
}

                                                                    
                              
   
                                                      
                                                                    
   
void
ExecAppendReInitializeDSM(AppendState *node, ParallelContext *pcxt)
{
  ParallelAppendState *pstate = node->as_pstate;

  pstate->pa_next_plan = 0;
  memset(pstate->pa_finished, 0, sizeof(bool) * node->as_nplans);
}

                                                                    
                               
   
                                                                      
                                                                    
                                                                    
   
void
ExecAppendInitializeWorker(AppendState *node, ParallelWorkerContext *pwcxt)
{
  node->as_pstate = shm_toc_lookup(pwcxt->toc, node->ps.plan->plan_node_id, false);
  node->choose_next_subplan = choose_next_subplan_for_worker;
}

                                                                    
                                
   
                                                         
                                          
                                                                    
   
static bool
choose_next_subplan_locally(AppendState *node)
{
  int whichplan = node->as_whichplan;
  int nextplan;

                                                            
  Assert(whichplan != NO_MATCHING_SUBPLANS);

     
                                                                            
                                                                        
                                                                     
                                                                         
                  
     
  if (whichplan == INVALID_SUBPLAN_INDEX)
  {
    if (node->as_valid_subplans == NULL)
    {
      node->as_valid_subplans = ExecFindMatchingSubPlans(node->as_prune_state);
    }

    whichplan = -1;
  }

                                                     
  Assert(whichplan >= -1 && whichplan <= node->as_nplans);

  if (ScanDirectionIsForward(node->ps.state->es_direction))
  {
    nextplan = bms_next_member(node->as_valid_subplans, whichplan);
  }
  else
  {
    nextplan = bms_prev_member(node->as_valid_subplans, whichplan);
  }

  if (nextplan < 0)
  {
    return false;
  }

  node->as_whichplan = nextplan;

  return true;
}

                                                                    
                                   
   
                                                                 
                                                                  
                                                        
                                                                    
   
static bool
choose_next_subplan_for_leader(AppendState *node)
{
  ParallelAppendState *pstate = node->as_pstate;

                                                              
  Assert(ScanDirectionIsForward(node->ps.state->es_direction));

                                                            
  Assert(node->as_whichplan != NO_MATCHING_SUBPLANS);

  LWLockAcquire(&pstate->pa_lock, LW_EXCLUSIVE);

  if (node->as_whichplan != INVALID_SUBPLAN_INDEX)
  {
                                                  
    node->as_pstate->pa_finished[node->as_whichplan] = true;
  }
  else
  {
                                  
    node->as_whichplan = node->as_nplans - 1;

       
                                                                        
                                                                           
                            
       
    if (node->as_valid_subplans == NULL)
    {
      node->as_valid_subplans = ExecFindMatchingSubPlans(node->as_prune_state);

         
                                                                       
                                         
         
      mark_invalid_subplans_as_finished(node);
    }
  }

                                                
  while (pstate->pa_finished[node->as_whichplan])
  {
    if (node->as_whichplan == 0)
    {
      pstate->pa_next_plan = INVALID_SUBPLAN_INDEX;
      node->as_whichplan = INVALID_SUBPLAN_INDEX;
      LWLockRelease(&pstate->pa_lock);
      return false;
    }

       
                                                                         
                                           
       
    node->as_whichplan--;
  }

                                                     
  if (node->as_whichplan < node->as_first_partial_plan)
  {
    node->as_pstate->pa_finished[node->as_whichplan] = true;
  }

  LWLockRelease(&pstate->pa_lock);

  return true;
}

                                                                    
                                   
   
                                                               
                                
   
                                                               
                                                           
                                                               
                                                              
                                                              
                                                                    
   
static bool
choose_next_subplan_for_worker(AppendState *node)
{
  ParallelAppendState *pstate = node->as_pstate;

                                                              
  Assert(ScanDirectionIsForward(node->ps.state->es_direction));

                                                            
  Assert(node->as_whichplan != NO_MATCHING_SUBPLANS);

  LWLockAcquire(&pstate->pa_lock, LW_EXCLUSIVE);

                                                
  if (node->as_whichplan != INVALID_SUBPLAN_INDEX)
  {
    node->as_pstate->pa_finished[node->as_whichplan] = true;
  }

     
                                                                      
                                                                             
                      
     
  else if (node->as_valid_subplans == NULL)
  {
    node->as_valid_subplans = ExecFindMatchingSubPlans(node->as_prune_state);
    mark_invalid_subplans_as_finished(node);
  }

                                                                
  if (pstate->pa_next_plan == INVALID_SUBPLAN_INDEX)
  {
    LWLockRelease(&pstate->pa_lock);
    return false;
  }

                                                            
  node->as_whichplan = pstate->pa_next_plan;

                                                      
  while (pstate->pa_finished[pstate->pa_next_plan])
  {
    int nextplan;

    nextplan = bms_next_member(node->as_valid_subplans, pstate->pa_next_plan);
    if (nextplan >= 0)
    {
                                           
      pstate->pa_next_plan = nextplan;
    }
    else if (node->as_whichplan > node->as_first_partial_plan)
    {
         
                                                                       
                                                          
         
      nextplan = bms_next_member(node->as_valid_subplans, node->as_first_partial_plan - 1);
      pstate->pa_next_plan = nextplan < 0 ? node->as_whichplan : nextplan;
    }
    else
    {
         
                                                                      
                                               
         
      pstate->pa_next_plan = node->as_whichplan;
    }

    if (pstate->pa_next_plan == node->as_whichplan)
    {
                                   
      pstate->pa_next_plan = INVALID_SUBPLAN_INDEX;
      LWLockRelease(&pstate->pa_lock);
      return false;
    }
  }

                                                                       
  node->as_whichplan = pstate->pa_next_plan;
  pstate->pa_next_plan = bms_next_member(node->as_valid_subplans, pstate->pa_next_plan);

     
                                                                            
                               
     
  if (pstate->pa_next_plan < 0)
  {
    int nextplan = bms_next_member(node->as_valid_subplans, node->as_first_partial_plan - 1);

    if (nextplan >= 0)
    {
      pstate->pa_next_plan = nextplan;
    }
    else
    {
         
                                                                         
                                                                     
                               
         
      pstate->pa_next_plan = INVALID_SUBPLAN_INDEX;
    }
  }

                                                     
  if (node->as_whichplan < node->as_first_partial_plan)
  {
    node->as_pstate->pa_finished[node->as_whichplan] = true;
  }

  LWLockRelease(&pstate->pa_lock);

  return true;
}

   
                                     
                                                                         
             
   
                                                                         
                    
   
static void
mark_invalid_subplans_as_finished(AppendState *node)
{
  int i;

                                                             
  Assert(node->as_pstate);

                                                                       
  Assert(node->as_prune_state);

                                            
  if (bms_num_members(node->as_valid_subplans) == node->as_nplans)
  {
    return;
  }

                                            
  for (i = 0; i < node->as_nplans; i++)
  {
    if (!bms_is_member(i, node->as_valid_subplans))
    {
      node->as_pstate->pa_finished[i] = true;
    }
  }
}
