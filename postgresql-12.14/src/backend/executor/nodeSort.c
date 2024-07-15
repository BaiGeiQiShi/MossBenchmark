                                                                            
   
              
                                              
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include "access/parallel.h"
#include "executor/execdebug.h"
#include "executor/nodeSort.h"
#include "miscadmin.h"
#include "utils/tuplesort.h"

                                                                    
             
   
                                                                     
                                                                     
                                                                
   
                
               
   
                    
                                                                
                                                                    
   
static TupleTableSlot *
ExecSort(PlanState *pstate)
{
  SortState *node = castNode(SortState, pstate);
  EState *estate;
  ScanDirection dir;
  Tuplesortstate *tuplesortstate;
  TupleTableSlot *slot;

  CHECK_FOR_INTERRUPTS();

     
                              
     
  SO1_printf("ExecSort: %s\n", "entering routine");

  estate = node->ss.ps.state;
  dir = estate->es_direction;
  tuplesortstate = (Tuplesortstate *)node->tuplesortstate;

     
                                                                             
                                                                     
     

  if (!node->sort_Done)
  {
    Sort *plannode = (Sort *)node->ss.ps.plan;
    PlanState *outerNode;
    TupleDesc tupDesc;

    SO1_printf("ExecSort: %s\n", "sorting subplan");

       
                                                                        
                    
       
    estate->es_direction = ForwardScanDirection;

       
                                    
       
    SO1_printf("ExecSort: %s\n", "calling tuplesort_begin");

    outerNode = outerPlanState(node);
    tupDesc = ExecGetResultType(outerNode);

    tuplesortstate = tuplesort_begin_heap(tupDesc, plannode->numCols, plannode->sortColIdx, plannode->sortOperators, plannode->collations, plannode->nullsFirst, work_mem, NULL, node->randomAccess);
    if (node->bounded)
    {
      tuplesort_set_bound(tuplesortstate, node->bound);
    }
    node->tuplesortstate = (void *)tuplesortstate;

       
                                                              
       

    for (;;)
    {
      slot = ExecProcNode(outerNode);

      if (TupIsNull(slot))
      {
        break;
      }

      tuplesort_puttupleslot(tuplesortstate, slot);
    }

       
                          
       
    tuplesort_performsort(tuplesortstate);

       
                                           
       
    estate->es_direction = dir;

       
                                           
       
    node->sort_Done = true;
    node->bounded_Done = node->bounded;
    node->bound_Done = node->bound;
    if (node->shared_info && node->am_worker)
    {
      TuplesortInstrumentation *si;

      Assert(IsParallelWorker());
      Assert(ParallelWorkerNumber <= node->shared_info->num_workers);
      si = &node->shared_info->sinstrument[ParallelWorkerNumber];
      tuplesort_get_stats(tuplesortstate, si);
    }
    SO1_printf("ExecSort: %s\n", "sorting done");
  }

  SO1_printf("ExecSort: %s\n", "retrieving tuple from tuplesort");

     
                                                                         
                                                                             
                                    
     
  slot = node->ss.ps.ps_ResultTupleSlot;
  (void)tuplesort_gettupleslot(tuplesortstate, ScanDirectionIsForward(dir), false, slot, NULL);
  return slot;
}

                                                                    
                 
   
                                                             
                                                               
                                                                    
   
SortState *
ExecInitSort(Sort *node, EState *estate, int eflags)
{
  SortState *sortstate;

  SO1_printf("ExecInitSort: %s\n", "initializing sort node");

     
                            
     
  sortstate = makeNode(SortState);
  sortstate->ss.ps.plan = (Plan *)node;
  sortstate->ss.ps.state = estate;
  sortstate->ss.ps.ExecProcNode = ExecSort;

     
                                                                          
                                                                        
                                                            
     
  sortstate->randomAccess = (eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0;

  sortstate->bounded = false;
  sortstate->sort_Done = false;
  sortstate->tuplesortstate = NULL;

     
                                  
     
                                                                            
                              
     

     
                            
     
                                                                            
                   
     
  eflags &= ~(EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK);

  outerPlanState(sortstate) = ExecInitNode(outerPlan(node), estate, eflags);

     
                                    
     
  ExecCreateScanSlotFromOuterPlan(estate, &sortstate->ss, &TTSOpsVirtual);

     
                                                                            
                                               
     
  ExecInitResultTupleSlotTL(&sortstate->ss.ps, &TTSOpsMinimalTuple);
  sortstate->ss.ps.ps_ProjInfo = NULL;

  SO1_printf("ExecInitSort: %s\n", "sort node initialized");

  return sortstate;
}

                                                                    
                      
                                                                    
   
void
ExecEndSort(SortState *node)
{
  SO1_printf("ExecEndSort: %s\n", "shutting down sort node");

     
                               
     
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
                                              
  ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

     
                                 
     
  if (node->tuplesortstate != NULL)
  {
    tuplesort_end((Tuplesortstate *)node->tuplesortstate);
  }
  node->tuplesortstate = NULL;

     
                           
     
  ExecEndNode(outerPlanState(node));

  SO1_printf("ExecEndSort: %s\n", "sort node shutdown");
}

                                                                    
                    
   
                                                                     
                                                                    
   
void
ExecSortMarkPos(SortState *node)
{
     
                                           
     
  if (!node->sort_Done)
  {
    return;
  }

  tuplesort_markpos((Tuplesortstate *)node->tuplesortstate);
}

                                                                    
                     
   
                                                                  
                                                                    
   
void
ExecSortRestrPos(SortState *node)
{
     
                                            
     
  if (!node->sort_Done)
  {
    return;
  }

     
                                                        
     
  tuplesort_restorepos((Tuplesortstate *)node->tuplesortstate);
}

void
ExecReScanSort(SortState *node)
{
  PlanState *outerPlan = outerPlanState(node);

     
                                                                           
                                                                        
                        
     
  if (!node->sort_Done)
  {
    return;
  }

                                              
  ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

     
                                                                            
                                                                        
                                                                       
     
                                                                
     
  if (outerPlan->chgParam != NULL || node->bounded != node->bounded_Done || node->bound != node->bound_Done || !node->randomAccess)
  {
    node->sort_Done = false;
    tuplesort_end((Tuplesortstate *)node->tuplesortstate);
    node->tuplesortstate = NULL;

       
                                                                          
                           
       
    if (outerPlan->chgParam == NULL)
    {
      ExecReScan(outerPlan);
    }
  }
  else
  {
    tuplesort_rescan((Tuplesortstate *)node->tuplesortstate);
  }
}

                                                                    
                               
                                                                    
   

                                                                    
                     
   
                                                          
                                                                    
   
void
ExecSortEstimate(SortState *node, ParallelContext *pcxt)
{
  Size size;

                                                          
  if (!node->ss.ps.instrument || pcxt->nworkers == 0)
  {
    return;
  }

  size = mul_size(pcxt->nworkers, sizeof(TuplesortInstrumentation));
  size = add_size(size, offsetof(SharedSortInfo, sinstrument));
  shm_toc_estimate_chunk(&pcxt->estimator, size);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

                                                                    
                          
   
                                              
                                                                    
   
void
ExecSortInitializeDSM(SortState *node, ParallelContext *pcxt)
{
  Size size;

                                                          
  if (!node->ss.ps.instrument || pcxt->nworkers == 0)
  {
    return;
  }

  size = offsetof(SharedSortInfo, sinstrument) + pcxt->nworkers * sizeof(TuplesortInstrumentation);
  node->shared_info = shm_toc_allocate(pcxt->toc, size);
                                                     
  memset(node->shared_info, 0, size);
  node->shared_info->num_workers = pcxt->nworkers;
  shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, node->shared_info);
}

                                                                    
                             
   
                                                    
                                                                    
   
void
ExecSortInitializeWorker(SortState *node, ParallelWorkerContext *pwcxt)
{
  node->shared_info = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, true);
  node->am_worker = true;
}

                                                                    
                                    
   
                                                         
                                                                    
   
void
ExecSortRetrieveInstrumentation(SortState *node)
{
  Size size;
  SharedSortInfo *si;

  if (node->shared_info == NULL)
  {
    return;
  }

  size = offsetof(SharedSortInfo, sinstrument) + node->shared_info->num_workers * sizeof(TuplesortInstrumentation);
  si = palloc(size);
  memcpy(si, node->shared_info, size);
  node->shared_info = si;
}
