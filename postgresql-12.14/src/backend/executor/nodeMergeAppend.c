                                                                            
   
                     
                                           
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
                      
                                                           
                                                              
                                                         
                                                        
   
          
                                                                
                                                                        
                                                                     
                                           
   
                                                             
                                                         
                                                                 
   
             
           
                                           
                      
                                   
                    
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/execPartition.h"
#include "executor/nodeMergeAppend.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"

   
                                                                        
                                                                    
                                                             
   
typedef int32 SlotNumber;

static TupleTableSlot *
ExecMergeAppend(PlanState *pstate);
static int
heap_compare_slots(Datum a, Datum b, void *arg);

                                                                    
                        
   
                                                       
                                                                    
   
MergeAppendState *
ExecInitMergeAppend(MergeAppend *node, EState *estate, int eflags)
{
  MergeAppendState *mergestate = makeNode(MergeAppendState);
  PlanState **mergeplanstates;
  Bitmapset *validsubplans;
  int nplans;
  int i, j;
  ListCell *lc;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                                              
     
  mergestate->ps.plan = (Plan *)node;
  mergestate->ps.state = estate;
  mergestate->ps.ExecProcNode = ExecMergeAppend;
  mergestate->ms_noopscan = false;

                                                                      
  if (node->part_prune_info != NULL)
  {
    PartitionPruneState *prunestate;

                                                                       
    ExecAssignExprContext(estate, &mergestate->ps);

    prunestate = ExecCreatePartitionPruneState(&mergestate->ps, node->part_prune_info);
    mergestate->ms_prune_state = prunestate;

                                                          
    if (prunestate->do_initial_prune)
    {
                                                            
      validsubplans = ExecFindInitialMatchingSubPlans(prunestate, list_length(node->mergeplans));

         
                                                                    
                                                                         
                                                                       
                                                                       
                                                                        
                                                                      
                                    
         
      if (bms_is_empty(validsubplans))
      {
        mergestate->ms_noopscan = true;

                                                                    
        validsubplans = bms_make_singleton(0);
      }

      nplans = bms_num_members(validsubplans);
    }
    else
    {
                                                 
      nplans = list_length(node->mergeplans);
      Assert(nplans > 0);
      validsubplans = bms_add_range(NULL, 0, nplans - 1);
    }

       
                                                                        
                                                                        
       
    if (!prunestate->do_exec_prune)
    {
      Assert(nplans > 0);
      mergestate->ms_valid_subplans = bms_add_range(NULL, 0, nplans - 1);
    }
  }
  else
  {
    nplans = list_length(node->mergeplans);

       
                                                                           
                                                             
       
    Assert(nplans > 0);
    mergestate->ms_valid_subplans = validsubplans = bms_add_range(NULL, 0, nplans - 1);
    mergestate->ms_prune_state = NULL;
  }

  mergeplanstates = (PlanState **)palloc(nplans * sizeof(PlanState *));
  mergestate->mergeplans = mergeplanstates;
  mergestate->ms_nplans = nplans;

  mergestate->ms_slots = (TupleTableSlot **)palloc0(sizeof(TupleTableSlot *) * nplans);
  mergestate->ms_heap = binaryheap_allocate(nplans, heap_compare_slots, mergestate);

     
                                  
     
                                                                            
                                           
     
  ExecInitResultTupleSlotTL(&mergestate->ps, &TTSOpsVirtual);

                                                                         
  mergestate->ps.resultopsset = true;
  mergestate->ps.resultopsfixed = false;

     
                                                                          
                                                 
     
  j = i = 0;
  foreach (lc, node->mergeplans)
  {
    if (bms_is_member(i, validsubplans))
    {
      Plan *initNode = (Plan *)lfirst(lc);

      mergeplanstates[j++] = ExecInitNode(initNode, estate, eflags);
    }
    i++;
  }

  mergestate->ps.ps_ProjInfo = NULL;

     
                                     
     
  mergestate->ms_nkeys = node->numCols;
  mergestate->ms_sortkeys = palloc0(sizeof(SortSupportData) * node->numCols);

  for (i = 0; i < node->numCols; i++)
  {
    SortSupport sortKey = mergestate->ms_sortkeys + i;

    sortKey->ssup_cxt = CurrentMemoryContext;
    sortKey->ssup_collation = node->collations[i];
    sortKey->ssup_nulls_first = node->nullsFirst[i];
    sortKey->ssup_attno = node->sortColIdx[i];

       
                                                                      
                                                                      
                                                                    
                                                                           
                                         
       
    sortKey->abbreviate = false;

    PrepareSortSupportFromOrderingOp(node->sortOperators[i], sortKey);
  }

     
                                                         
     
  mergestate->ms_initialized = false;

  return mergestate;
}

                                                                    
                      
   
                                              
                                                                    
   
static TupleTableSlot *
ExecMergeAppend(PlanState *pstate)
{
  MergeAppendState *node = castNode(MergeAppendState, pstate);
  TupleTableSlot *result;
  SlotNumber i;

  CHECK_FOR_INTERRUPTS();

  if (!node->ms_initialized)
  {
                                                   
    if (node->ms_noopscan)
    {
      return ExecClearTuple(node->ps.ps_ResultTupleSlot);
    }

       
                                                                        
                                                                           
                            
       
    if (node->ms_valid_subplans == NULL)
    {
      node->ms_valid_subplans = ExecFindMatchingSubPlans(node->ms_prune_state);
    }

       
                                                                         
                            
       
    i = -1;
    while ((i = bms_next_member(node->ms_valid_subplans, i)) >= 0)
    {
      node->ms_slots[i] = ExecProcNode(node->mergeplans[i]);
      if (!TupIsNull(node->ms_slots[i]))
      {
        binaryheap_add_unordered(node->ms_heap, Int32GetDatum(i));
      }
    }
    binaryheap_build(node->ms_heap);
    node->ms_initialized = true;
  }
  else
  {
       
                                                                         
                                                                     
                                                                     
                                                                         
                                                                           
                                            
       
    i = DatumGetInt32(binaryheap_first(node->ms_heap));
    node->ms_slots[i] = ExecProcNode(node->mergeplans[i]);
    if (!TupIsNull(node->ms_slots[i]))
    {
      binaryheap_replace_first(node->ms_heap, Int32GetDatum(i));
    }
    else
    {
      (void)binaryheap_remove_first(node->ms_heap);
    }
  }

  if (binaryheap_empty(node->ms_heap))
  {
                                                            
    result = ExecClearTuple(node->ps.ps_ResultTupleSlot);
  }
  else
  {
    i = DatumGetInt32(binaryheap_first(node->ms_heap));
    result = node->ms_slots[i];
  }

  return result;
}

   
                                              
   
static int32
heap_compare_slots(Datum a, Datum b, void *arg)
{
  MergeAppendState *node = (MergeAppendState *)arg;
  SlotNumber slot1 = DatumGetInt32(a);
  SlotNumber slot2 = DatumGetInt32(b);

  TupleTableSlot *s1 = node->ms_slots[slot1];
  TupleTableSlot *s2 = node->ms_slots[slot2];
  int nkey;

  Assert(!TupIsNull(s1));
  Assert(!TupIsNull(s2));

  for (nkey = 0; nkey < node->ms_nkeys; nkey++)
  {
    SortSupport sortKey = node->ms_sortkeys + nkey;
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

                                                                    
                       
   
                                                     
   
                                 
                                                                    
   
void
ExecEndMergeAppend(MergeAppendState *node)
{
  PlanState **mergeplans;
  int nplans;
  int i;

     
                                   
     
  mergeplans = node->mergeplans;
  nplans = node->ms_nplans;

     
                                    
     
  for (i = 0; i < nplans; i++)
  {
    ExecEndNode(mergeplans[i]);
  }
}

void
ExecReScanMergeAppend(MergeAppendState *node)
{
  int i;

     
                                                                             
                                                                          
                               
     
  if (node->ms_prune_state && bms_overlap(node->ps.chgParam, node->ms_prune_state->execparamids))
  {
    bms_free(node->ms_valid_subplans);
    node->ms_valid_subplans = NULL;
  }

  for (i = 0; i < node->ms_nplans; i++)
  {
    PlanState *subnode = node->mergeplans[i];

       
                                                                  
                                           
       
    if (node->ps.chgParam != NULL)
    {
      UpdateChangedParamSet(subnode, node->ps.chgParam);
    }

       
                                                                          
                           
       
    if (subnode->chgParam == NULL)
    {
      ExecReScan(subnode);
    }
  }
  binaryheap_reset(node->ms_heap);
  node->ms_initialized = false;
}
