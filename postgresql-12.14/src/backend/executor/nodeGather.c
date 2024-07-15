                                                                            
   
                
                                                                
   
                                                                         
                                                                        
   
                                                                           
                                                                            
                                                                              
                                                                             
                                                                             
                                                                       
            
   
                                                                         
                                                                            
                                                                        
                                                                             
                                                                        
                                                                         
                                    
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include "access/relscan.h"
#include "access/xact.h"
#include "executor/execdebug.h"
#include "executor/execParallel.h"
#include "executor/nodeGather.h"
#include "executor/nodeSubplan.h"
#include "executor/tqueue.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
ExecGather(PlanState *pstate);
static TupleTableSlot *
gather_getnext(GatherState *gatherstate);
static HeapTuple
gather_readnext(GatherState *gatherstate);
static void
ExecShutdownGatherWorkers(GatherState *node);

                                                                    
                   
                                                                    
   
GatherState *
ExecInitGather(Gather *node, EState *estate, int eflags)
{
  GatherState *gatherstate;
  Plan *outerNode;
  TupleDesc tupDesc;

                                                
  Assert(innerPlan(node) == NULL);

     
                            
     
  gatherstate = makeNode(GatherState);
  gatherstate->ps.plan = (Plan *)node;
  gatherstate->ps.state = estate;
  gatherstate->ps.ExecProcNode = ExecGather;

  gatherstate->initialized = false;
  gatherstate->need_to_scan_locally = !node->single_copy && parallel_leader_participation;
  gatherstate->tuples_needed = -1;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &gatherstate->ps);

     
                               
     
  outerNode = outerPlan(node);
  outerPlanState(gatherstate) = ExecInitNode(outerNode, estate, eflags);
  tupDesc = ExecGetResultType(outerPlanState(gatherstate));

     
                                                        
                                                                          
                                                                           
                       
     
  gatherstate->ps.outeropsset = true;
  gatherstate->ps.outeropsfixed = false;

     
                                            
     
  ExecInitResultTypeTL(&gatherstate->ps);
  ExecConditionalAssignProjectionInfo(&gatherstate->ps, tupDesc, OUTER_VAR);

     
                                                                      
                    
     
  if (gatherstate->ps.ps_ProjInfo == NULL)
  {
    gatherstate->ps.resultopsset = true;
    gatherstate->ps.resultopsfixed = false;
  }

     
                                                                    
     
  gatherstate->funnel_slot = ExecInitExtraTupleSlot(estate, tupDesc, &TTSOpsHeapTuple);

     
                                                                           
                               
     
  Assert(!node->plan.qual);

  return gatherstate;
}

                                                                    
                     
   
                                                        
                               
                                                                    
   
static TupleTableSlot *
ExecGather(PlanState *pstate)
{
  GatherState *node = castNode(GatherState, pstate);
  TupleTableSlot *slot;
  ExprContext *econtext;

  CHECK_FOR_INTERRUPTS();

     
                                                                           
                                                                           
                                                                         
                                  
     
  if (!node->initialized)
  {
    EState *estate = node->ps.state;
    Gather *gather = (Gather *)node->ps.plan;

       
                                                                           
                                                               
       
    if (gather->num_workers > 0 && estate->es_use_parallel_mode)
    {
      ParallelContext *pcxt;

                                                                         
      if (!node->pei)
      {
        node->pei = ExecInitParallelPlan(node->ps.lefttree, estate, gather->initParam, gather->num_workers, node->tuples_needed);
      }
      else
      {
        ExecParallelReinitialize(node->ps.lefttree, node->pei, gather->initParam);
      }

         
                                                                  
                                          
         
      pcxt = node->pei->pcxt;
      LaunchParallelWorkers(pcxt);
                                                                 
      node->nworkers_launched = pcxt->nworkers_launched;

                                                           
      if (pcxt->nworkers_launched > 0)
      {
        ExecParallelCreateReaders(node->pei);
                                                             
        node->nreaders = pcxt->nworkers_launched;
        node->reader = (TupleQueueReader **)palloc(node->nreaders * sizeof(TupleQueueReader *));
        memcpy(node->reader, node->pei->reader, node->nreaders * sizeof(TupleQueueReader *));
      }
      else
      {
                                          
        node->nreaders = 0;
        node->reader = NULL;
      }
      node->nextreader = 0;
    }

                                                                        
    node->need_to_scan_locally = (node->nreaders == 0) || (!gather->single_copy && parallel_leader_participation);
    node->initialized = true;
  }

     
                                                                      
                                                    
     
  econtext = node->ps.ps_ExprContext;
  ResetExprContext(econtext);

     
                                                                            
                
     
  slot = gather_getnext(node);
  if (TupIsNull(slot))
  {
    return NULL;
  }

                                                 
  if (node->ps.ps_ProjInfo == NULL)
  {
    return slot;
  }

     
                                                               
     
  econtext->ecxt_outertuple = slot;
  return ExecProject(node->ps.ps_ProjInfo);
}

                                                                    
                  
   
                                                    
                                                                    
   
void
ExecEndGather(GatherState *node)
{
  ExecEndNode(outerPlanState(node));                                  
  ExecShutdownGather(node);
  ExecFreeExprContext(&node->ps);
  if (node->ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ps.ps_ResultTupleSlot);
  }
}

   
                                                                             
                                                                        
                                                                       
   
static TupleTableSlot *
gather_getnext(GatherState *gatherstate)
{
  PlanState *outerPlan = outerPlanState(gatherstate);
  TupleTableSlot *outerTupleSlot;
  TupleTableSlot *fslot = gatherstate->funnel_slot;
  HeapTuple tup;

  while (gatherstate->nreaders > 0 || gatherstate->need_to_scan_locally)
  {
    CHECK_FOR_INTERRUPTS();

    if (gatherstate->nreaders > 0)
    {
      tup = gather_readnext(gatherstate);

      if (HeapTupleIsValid(tup))
      {
        ExecStoreHeapTuple(tup,                     
            fslot,                                           
            true);                                                 
        return fslot;
      }
    }

    if (gatherstate->need_to_scan_locally)
    {
      EState *estate = gatherstate->ps.state;

                                                          
      estate->es_query_dsa = gatherstate->pei ? gatherstate->pei->area : NULL;
      outerTupleSlot = ExecProcNode(outerPlan);
      estate->es_query_dsa = NULL;

      if (!TupIsNull(outerTupleSlot))
      {
        return outerTupleSlot;
      }

      gatherstate->need_to_scan_locally = false;
    }
  }

  return ExecClearTuple(fslot);
}

   
                                                             
   
static HeapTuple
gather_readnext(GatherState *gatherstate)
{
  int nvisited = 0;

  for (;;)
  {
    TupleQueueReader *reader;
    HeapTuple tup;
    bool readerdone;

                                                                     
    CHECK_FOR_INTERRUPTS();

       
                                                                      
       
                                                                         
                                                                     
                                                                         
                          
       
    Assert(gatherstate->nextreader < gatherstate->nreaders);
    reader = gatherstate->reader[gatherstate->nextreader];
    tup = TupleQueueReaderNext(reader, true, &readerdone);

       
                                                                          
                                                            
       
    if (readerdone)
    {
      Assert(!tup);
      --gatherstate->nreaders;
      if (gatherstate->nreaders == 0)
      {
        ExecShutdownGatherWorkers(gatherstate);
        return NULL;
      }
      memmove(&gatherstate->reader[gatherstate->nextreader], &gatherstate->reader[gatherstate->nextreader + 1], sizeof(TupleQueueReader *) * (gatherstate->nreaders - gatherstate->nextreader));
      if (gatherstate->nextreader >= gatherstate->nreaders)
      {
        gatherstate->nextreader = 0;
      }
      continue;
    }

                                       
    if (tup)
    {
      return tup;
    }

       
                                                                        
                                                                       
                                                                        
                                                                       
                                                                      
       
    gatherstate->nextreader++;
    if (gatherstate->nextreader >= gatherstate->nreaders)
    {
      gatherstate->nextreader = 0;
    }

                                                             
    nvisited++;
    if (nvisited >= gatherstate->nreaders)
    {
         
                                                                    
                                                                 
         
      if (gatherstate->need_to_scan_locally)
      {
        return NULL;
      }

                                                       
      (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, WAIT_EVENT_EXECUTE_GATHER);
      ResetLatch(MyLatch);
      nvisited = 0;
    }
  }
}

                                                                    
                              
   
                                   
                                                                    
   
static void
ExecShutdownGatherWorkers(GatherState *node)
{
  if (node->pei != NULL)
  {
    ExecParallelFinish(node->pei);
  }

                                        
  if (node->reader)
  {
    pfree(node->reader);
  }
  node->reader = NULL;
}

                                                                    
                       
   
                                                                       
                                                                    
   
void
ExecShutdownGather(GatherState *node)
{
  ExecShutdownGatherWorkers(node);

                                         
  if (node->pei != NULL)
  {
    ExecParallelCleanup(node->pei);
    node->pei = NULL;
  }
}

                                                                    
                     
                                                                    
   

                                                                    
                     
   
                                               
                                                                    
   
void
ExecReScanGather(GatherState *node)
{
  Gather *gather = (Gather *)node->ps.plan;
  PlanState *outerPlan = outerPlanState(node);

                                                               
  ExecShutdownGatherWorkers(node);

                                                                   
  node->initialized = false;

     
                                                                             
                                                                           
                                                                          
                                                                           
                                          
     
  if (gather->rescan_param >= 0)
  {
    outerPlan->chgParam = bms_add_member(outerPlan->chgParam, gather->rescan_param);
  }

     
                                                                        
                                                                       
                                                                             
                                                                             
                                                                           
                                                                             
                                                                         
                                                                             
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }
}
