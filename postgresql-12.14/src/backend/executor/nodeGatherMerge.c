                                                                            
   
                     
                                                                    
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/relscan.h"
#include "access/xact.h"
#include "executor/execdebug.h"
#include "executor/execParallel.h"
#include "executor/nodeGatherMerge.h"
#include "executor/nodeSubplan.h"
#include "executor/tqueue.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                              
                                                                            
                                                                               
                                                                           
   
#define MAX_TUPLE_STORE 10

   
                                                                           
                                                                               
                                                                            
                                                                             
                                                                               
                             
   
typedef struct GMReaderTupleBuffer
{
  HeapTuple *tuple;                                      
  int nTuples;                                             
  int readCounter;                                      
  bool done;                                               
} GMReaderTupleBuffer;

static TupleTableSlot *
ExecGatherMerge(PlanState *pstate);
static int32
heap_compare_slots(Datum a, Datum b, void *arg);
static TupleTableSlot *
gather_merge_getnext(GatherMergeState *gm_state);
static HeapTuple
gm_readnext_tuple(GatherMergeState *gm_state, int nreader, bool nowait, bool *done);
static void
ExecShutdownGatherMergeWorkers(GatherMergeState *node);
static void
gather_merge_setup(GatherMergeState *gm_state);
static void
gather_merge_init(GatherMergeState *gm_state);
static void
gather_merge_clear_tuples(GatherMergeState *gm_state);
static bool
gather_merge_readnext(GatherMergeState *gm_state, int reader, bool nowait);
static void
load_tuple_array(GatherMergeState *gm_state, int reader);

                                                                    
                   
                                                                    
   
GatherMergeState *
ExecInitGatherMerge(GatherMerge *node, EState *estate, int eflags)
{
  GatherMergeState *gm_state;
  Plan *outerNode;
  TupleDesc tupDesc;

                                                      
  Assert(innerPlan(node) == NULL);

     
                            
     
  gm_state = makeNode(GatherMergeState);
  gm_state->ps.plan = (Plan *)node;
  gm_state->ps.state = estate;
  gm_state->ps.ExecProcNode = ExecGatherMerge;

  gm_state->initialized = false;
  gm_state->gm_initialized = false;
  gm_state->tuples_needed = -1;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &gm_state->ps);

     
                                                                             
                                  
     
  Assert(!node->plan.qual);

     
                               
     
  outerNode = outerPlan(node);
  outerPlanState(gm_state) = ExecInitNode(outerNode, estate, eflags);

     
                                                        
                                                                          
                                                                           
                       
     
  gm_state->ps.outeropsset = true;
  gm_state->ps.outeropsfixed = false;

     
                                                                          
                                                
     
  tupDesc = ExecGetResultType(outerPlanState(gm_state));
  gm_state->tupDesc = tupDesc;

     
                                            
     
  ExecInitResultTypeTL(&gm_state->ps);
  ExecConditionalAssignProjectionInfo(&gm_state->ps, tupDesc, OUTER_VAR);

     
                                                                      
                    
     
  if (gm_state->ps.ps_ProjInfo == NULL)
  {
    gm_state->ps.resultopsset = true;
    gm_state->ps.resultopsfixed = false;
  }

     
                                     
     
  if (node->numCols)
  {
    int i;

    gm_state->gm_nkeys = node->numCols;
    gm_state->gm_sortkeys = palloc0(sizeof(SortSupportData) * node->numCols);

    for (i = 0; i < node->numCols; i++)
    {
      SortSupport sortKey = gm_state->gm_sortkeys + i;

      sortKey->ssup_cxt = CurrentMemoryContext;
      sortKey->ssup_collation = node->collations[i];
      sortKey->ssup_nulls_first = node->nullsFirst[i];
      sortKey->ssup_attno = node->sortColIdx[i];

         
                                                                        
                                                   
         
      sortKey->abbreviate = false;

      PrepareSortSupportFromOrderingOp(node->sortOperators[i], sortKey);
    }
  }

                                                   
  gather_merge_setup(gm_state);

  return gm_state;
}

                                                                    
                          
   
                                                        
                               
                                                                    
   
static TupleTableSlot *
ExecGatherMerge(PlanState *pstate)
{
  GatherMergeState *node = castNode(GatherMergeState, pstate);
  TupleTableSlot *slot;
  ExprContext *econtext;

  CHECK_FOR_INTERRUPTS();

     
                                                                         
               
     
  if (!node->initialized)
  {
    EState *estate = node->ps.state;
    GatherMerge *gm = castNode(GatherMerge, node->ps.plan);

       
                                                                           
                                                               
       
    if (gm->num_workers > 0 && estate->es_use_parallel_mode)
    {
      ParallelContext *pcxt;

                                                                         
      if (!node->pei)
      {
        node->pei = ExecInitParallelPlan(node->ps.lefttree, estate, gm->initParam, gm->num_workers, node->tuples_needed);
      }
      else
      {
        ExecParallelReinitialize(node->ps.lefttree, node->pei, gm->initParam);
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
    }

                                                             
    if (parallel_leader_participation || node->nreaders == 0)
    {
      node->need_to_scan_locally = true;
    }
    node->initialized = true;
  }

     
                                                                      
                                                    
     
  econtext = node->ps.ps_ExprContext;
  ResetExprContext(econtext);

     
                                                                            
                
     
  slot = gather_merge_getnext(node);
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
ExecEndGatherMerge(GatherMergeState *node)
{
  ExecEndNode(outerPlanState(node));                                  
  ExecShutdownGatherMerge(node);
  ExecFreeExprContext(&node->ps);
  if (node->ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ps.ps_ResultTupleSlot);
  }
}

                                                                    
                            
   
                                                                       
                                                                    
   
void
ExecShutdownGatherMerge(GatherMergeState *node)
{
  ExecShutdownGatherMergeWorkers(node);

                                         
  if (node->pei != NULL)
  {
    ExecParallelCleanup(node->pei);
    node->pei = NULL;
  }
}

                                                                    
                                   
   
                                   
                                                                    
   
static void
ExecShutdownGatherMergeWorkers(GatherMergeState *node)
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
ExecReScanGatherMerge(GatherMergeState *node)
{
  GatherMerge *gm = (GatherMerge *)node->ps.plan;
  PlanState *outerPlan = outerPlanState(node);

                                                               
  ExecShutdownGatherMergeWorkers(node);

                                                                      
  gather_merge_clear_tuples(node);

                                                                   
  node->initialized = false;
  node->gm_initialized = false;

     
                                                                             
                                                                           
                                                                          
                                                                           
                                          
     
  if (gm->rescan_param >= 0)
  {
    outerPlan->chgParam = bms_add_member(outerPlan->chgParam, gm->rescan_param);
  }

     
                                                                        
                                                                       
                                                                             
                                                                             
                                                                           
                                                                             
                                                                         
                                                                             
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }
}

   
                                                                
   
                                                                       
                                                                      
                                                                         
                                      
   
                                                                          
                                                                          
                                                                          
                                             
   
static void
gather_merge_setup(GatherMergeState *gm_state)
{
  GatherMerge *gm = castNode(GatherMerge, gm_state->ps.plan);
  int nreaders = gm->num_workers;
  int i;

     
                                                                             
                                                                             
                                                                          
                                                                       
                                                                          
                                                                            
                                             
     
  gm_state->gm_slots = (TupleTableSlot **)palloc0((nreaders + 1) * sizeof(TupleTableSlot *));

                                                               
  gm_state->gm_tuple_buffers = (GMReaderTupleBuffer *)palloc0(nreaders * sizeof(GMReaderTupleBuffer));

  for (i = 0; i < nreaders; i++)
  {
                                                              
    gm_state->gm_tuple_buffers[i].tuple = (HeapTuple *)palloc0(sizeof(HeapTuple) * MAX_TUPLE_STORE);

                                          
    gm_state->gm_slots[i + 1] = ExecInitExtraTupleSlot(gm_state->ps.state, gm_state->tupDesc, &TTSOpsHeapTuple);
  }

                                            
  gm_state->gm_heap = binaryheap_allocate(nreaders + 1, heap_compare_slots, gm_state);
}

   
                                
   
                                                                          
                                                                             
             
   
static void
gather_merge_init(GatherMergeState *gm_state)
{
  int nreaders = gm_state->nreaders;
  bool nowait = true;
  int i;

                                                        
  Assert(nreaders <= castNode(GatherMerge, gm_state->ps.plan)->num_workers);

                                          
  gm_state->gm_slots[0] = NULL;

                                                            
  for (i = 0; i < nreaders; i++)
  {
                                    
    gm_state->gm_tuple_buffers[i].nTuples = 0;
    gm_state->gm_tuple_buffers[i].readCounter = 0;
                                     
    gm_state->gm_tuple_buffers[i].done = false;
                                     
    ExecClearTuple(gm_state->gm_slots[i + 1]);
  }

                                  
  binaryheap_reset(gm_state->gm_heap);

     
                                                                       
                                                                         
                                                                          
                                                                          
                                   
     
reread:
  for (i = 0; i <= nreaders; i++)
  {
    CHECK_FOR_INTERRUPTS();

                                                
    if ((i == 0) ? gm_state->need_to_scan_locally : !gm_state->gm_tuple_buffers[i - 1].done)
    {
      if (TupIsNull(gm_state->gm_slots[i]))
      {
                                                    
        if (gather_merge_readnext(gm_state, i, nowait))
        {
          binaryheap_add_unordered(gm_state->gm_heap, Int32GetDatum(i));
        }
      }
      else
      {
           
                                                                   
                                                              
           
        load_tuple_array(gm_state, i);
      }
    }
  }

                                                                   
  for (i = 1; i <= nreaders; i++)
  {
    if (!gm_state->gm_tuple_buffers[i - 1].done && TupIsNull(gm_state->gm_slots[i]))
    {
      nowait = false;
      goto reread;
    }
  }

                             
  binaryheap_build(gm_state->gm_heap);

  gm_state->gm_initialized = true;
}

   
                                                                  
                                
   
static void
gather_merge_clear_tuples(GatherMergeState *gm_state)
{
  int i;

  for (i = 0; i < gm_state->nreaders; i++)
  {
    GMReaderTupleBuffer *tuple_buffer = &gm_state->gm_tuple_buffers[i];

    while (tuple_buffer->readCounter < tuple_buffer->nTuples)
    {
      heap_freetuple(tuple_buffer->tuple[tuple_buffer->readCounter++]);
    }

    ExecClearTuple(gm_state->gm_slots[i + 1]);
  }
}

   
                                         
   
                                           
   
static TupleTableSlot *
gather_merge_getnext(GatherMergeState *gm_state)
{
  int i;

  if (!gm_state->gm_initialized)
  {
       
                                                                           
                        
       
    gather_merge_init(gm_state);
  }
  else
  {
       
                                                                    
                                                                           
                                                                      
                                   
       
    i = DatumGetInt32(binaryheap_first(gm_state->gm_heap));

    if (gather_merge_readnext(gm_state, i, false))
    {
      binaryheap_replace_first(gm_state->gm_heap, Int32GetDatum(i));
    }
    else
    {
                                                 
      (void)binaryheap_remove_first(gm_state->gm_heap);
    }
  }

  if (binaryheap_empty(gm_state->gm_heap))
  {
                                                          
    gather_merge_clear_tuples(gm_state);
    return NULL;
  }
  else
  {
                                                                          
    i = DatumGetInt32(binaryheap_first(gm_state->gm_heap));
    return gm_state->gm_slots[i];
  }
}

   
                                                                          
                                                                        
   
static void
load_tuple_array(GatherMergeState *gm_state, int reader)
{
  GMReaderTupleBuffer *tuple_buffer;
  int i;

                                                
  if (reader == 0)
  {
    return;
  }

  tuple_buffer = &gm_state->gm_tuple_buffers[reader - 1];

                                                                    
  if (tuple_buffer->nTuples == tuple_buffer->readCounter)
  {
    tuple_buffer->nTuples = tuple_buffer->readCounter = 0;
  }

                                                  
  for (i = tuple_buffer->nTuples; i < MAX_TUPLE_STORE; i++)
  {
    HeapTuple tuple;

    tuple = gm_readnext_tuple(gm_state, reader, true, &tuple_buffer->done);
    if (!HeapTupleIsValid(tuple))
    {
      break;
    }
    tuple_buffer->tuple[i] = tuple;
    tuple_buffer->nTuples++;
  }
}

   
                                                                      
   
                                                                         
                                                                     
                             
   
static bool
gather_merge_readnext(GatherMergeState *gm_state, int reader, bool nowait)
{
  GMReaderTupleBuffer *tuple_buffer;
  HeapTuple tup;

     
                                                                            
                                                 
     
  if (reader == 0)
  {
    if (gm_state->need_to_scan_locally)
    {
      PlanState *outerPlan = outerPlanState(gm_state);
      TupleTableSlot *outerTupleSlot;
      EState *estate = gm_state->ps.state;

                                                          
      estate->es_query_dsa = gm_state->pei ? gm_state->pei->area : NULL;
      outerTupleSlot = ExecProcNode(outerPlan);
      estate->es_query_dsa = NULL;

      if (!TupIsNull(outerTupleSlot))
      {
        gm_state->gm_slots[0] = outerTupleSlot;
        return true;
      }
                                                                 
      gm_state->need_to_scan_locally = false;
    }
    return false;
  }

                                                                
  tuple_buffer = &gm_state->gm_tuple_buffers[reader - 1];

  if (tuple_buffer->nTuples > tuple_buffer->readCounter)
  {
                                                                  
    tup = tuple_buffer->tuple[tuple_buffer->readCounter++];
  }
  else if (tuple_buffer->done)
  {
                                          
    return false;
  }
  else
  {
                                     
    tup = gm_readnext_tuple(gm_state, reader, nowait, &tuple_buffer->done);
    if (!HeapTupleIsValid(tup))
    {
      return false;
    }

       
                                                                        
                                           
       
    load_tuple_array(gm_state, reader);
  }

  Assert(HeapTupleIsValid(tup));

                                                    
  ExecStoreHeapTuple(tup,                             
      gm_state->gm_slots[reader],                           
                                                 
      true);                                                         

  return true;
}

   
                                              
   
static HeapTuple
gm_readnext_tuple(GatherMergeState *gm_state, int nreader, bool nowait, bool *done)
{
  TupleQueueReader *reader;
  HeapTuple tup;

                                                                   
  CHECK_FOR_INTERRUPTS();

     
                              
     
                                                                             
                                                                         
                                                                       
            
     
  reader = gm_state->reader[nreader - 1];
  tup = TupleQueueReaderNext(reader, nowait, done);

  return tup;
}

   
                                                                        
                                                                    
                                                             
   
typedef int32 SlotNumber;

   
                                              
   
static int32
heap_compare_slots(Datum a, Datum b, void *arg)
{
  GatherMergeState *node = (GatherMergeState *)arg;
  SlotNumber slot1 = DatumGetInt32(a);
  SlotNumber slot2 = DatumGetInt32(b);

  TupleTableSlot *s1 = node->gm_slots[slot1];
  TupleTableSlot *s2 = node->gm_slots[slot2];
  int nkey;

  Assert(!TupIsNull(s1));
  Assert(!TupIsNull(s2));

  for (nkey = 0; nkey < node->gm_nkeys; nkey++)
  {
    SortSupport sortKey = node->gm_sortkeys + nkey;
    AttrNumber attno = sortKey->ssup_attno;
    Datum datum1, datum2;
    bool isNull1, isNull2;
    int compare;

    datum1 = slot_getattr(s1, attno, &isNull1);
    datum2 = slot_getattr(s2, attno, &isNull2);

    compare = ApplySortComparator(datum1, isNull1, datum2, isNull2, sortKey);
    if (compare != 0)
    {
      INVERT_COMPARE_RESULT(compare);
      return compare;
    }
  }
  return 0;
}
