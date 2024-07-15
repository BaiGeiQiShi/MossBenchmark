                                                                            
   
             
                                         
   
                                                                       
   
                           
                            
                                                        
                                                        
   
                                                                       
                          
   
                                                                          
                                                         
                                                                    
                             
                                                                        
                                                                             
                                                                         
                                                                          
                                                                           
                                                                       
                            
                                                                            
                                                       
                                                                            
                                                                             
                                                  
   
                                                                            
                                                                           
                                                                             
                                                                          
                                                                        
                                                                      
                                             
   
                                                                      
                                                                             
                                                                        
                                                                             
   
                                                                         
                                                                          
                                                                      
                                      
   
                                                                    
                                                                 
                                                                         
                                                                    
                                                  
   
                                                                       
                                                                          
                                
   
                                                                         
                                                                         
                                                                              
                                                                          
   
                                                                             
                                                                             
                                                              
                                                                             
                                                                           
                                                                          
                                                                              
                                                                             
                                                                           
                                                          
   
                                                                             
                                                                              
                                  
   
                                                                           
                                                                        
                                                                           
                                                                       
                                                                       
                                                                         
                                                                         
                                                                            
                                                                            
                                                                            
                                                                            
                                                                              
                                                                           
                                                                             
                                                                             
                                                                          
                                                                         
                                                                              
                                                                              
                                                                              
   
                                                                         
                                                                         
                                                                        
   
                                                                           
                                                                         
                                                                       
                                                                         
                                                                        
                             
   
                    
   
                                                                          
                                                                              
                                                                              
                                                                              
                                                                       
                                                                            
                                                                       
                
   
                                                                        
                                                                           
                                                                          
                                                                               
                                                                         
                                                                            
                                                                        
                                     
   
                                                                        
                                                                              
                                                                               
                                                                             
             
   
                                                                           
                                                                             
                                                                         
                                                                             
                                                                            
                                                                            
                                                                              
                                                                       
                             
   
                     
   
                                                                           
                                                                               
                                                                               
                                                                           
                             
   
                                                                              
                                                                            
                                                                            
                                                                             
                                                                              
                                                                             
                                                                            
                                                                         
                    
   
                                                                               
                                                                           
                                                                             
                
   
                                                                         
                                                                       
                                                                           
                                                                             
                                                                              
                                                                               
                                                                     
   
                                   
   
                                                                               
                                                                            
                                                                             
                                                                              
                                                                
                                                                              
                                                                          
                 
   
                                                                               
                                                                        
                                                                           
                                                                              
                      
   
                                                
   
                                                                      
                                                                          
                                                                
                                                                              
                                                                             
                                                                              
                                                                            
                                                      
   
                                                                         
                                                                        
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/execExpr.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/datum.h"

static void
select_current_set(AggState *aggstate, int setno, bool is_hash);
static void
initialize_phase(AggState *aggstate, int newphase);
static TupleTableSlot *
fetch_input_tuple(AggState *aggstate);
static void
initialize_aggregates(AggState *aggstate, AggStatePerGroup *pergroups, int numReset);
static void
advance_transition_function(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate);
static void
advance_aggregates(AggState *aggstate);
static void
process_ordered_aggregate_single(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate);
static void
process_ordered_aggregate_multi(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate);
static void
finalize_aggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull);
static void
finalize_partialaggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull);
static void
prepare_projection_slot(AggState *aggstate, TupleTableSlot *slot, int currentSet);
static void
finalize_aggregates(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroup);
static TupleTableSlot *
project_aggregates(AggState *aggstate);
static Bitmapset *
find_unaggregated_cols(AggState *aggstate);
static bool
find_unaggregated_cols_walker(Node *node, Bitmapset **colnos);
static void
build_hash_table(AggState *aggstate);
static TupleHashEntryData *
lookup_hash_entry(AggState *aggstate);
static void
lookup_hash_entries(AggState *aggstate);
static TupleTableSlot *
agg_retrieve_direct(AggState *aggstate);
static void
agg_fill_hash_table(AggState *aggstate);
static TupleTableSlot *
agg_retrieve_hash_table(AggState *aggstate);
static Datum
GetAggInitVal(Datum textInitVal, Oid transtype);
static void
build_pertrans_for_aggref(AggStatePerTrans pertrans, AggState *aggstate, EState *estate, Aggref *aggref, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, Oid *inputTypes, int numArguments);
static int
find_compatible_peragg(Aggref *newagg, AggState *aggstate, int lastaggno, List **same_input_transnos);
static int
find_compatible_pertrans(AggState *aggstate, Aggref *newagg, bool shareable, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, List *transnos);

   
                                                            
                  
   
static void
select_current_set(AggState *aggstate, int setno, bool is_hash)
{
                                                                   
  if (is_hash)
  {
    aggstate->curaggcontext = aggstate->hashcontext;
  }
  else
  {
    aggstate->curaggcontext = aggstate->aggcontexts[setno];
  }

  aggstate->current_set = setno;
}

   
                                                                         
                                                         
   
                                                                           
                                                                         
   
static void
initialize_phase(AggState *aggstate, int newphase)
{
  Assert(newphase <= 1 || newphase == aggstate->current_phase + 1);

     
                                                                     
                           
     
  if (aggstate->sort_in)
  {
    tuplesort_end(aggstate->sort_in);
    aggstate->sort_in = NULL;
  }

  if (newphase <= 1)
  {
       
                                              
       
    if (aggstate->sort_out)
    {
      tuplesort_end(aggstate->sort_out);
      aggstate->sort_out = NULL;
    }
  }
  else
  {
       
                                                                           
                                       
       
    aggstate->sort_in = aggstate->sort_out;
    aggstate->sort_out = NULL;
    Assert(aggstate->sort_in);
    tuplesort_performsort(aggstate->sort_in);
  }

     
                                                                         
                             
     
  if (newphase > 0 && newphase < aggstate->numphases - 1)
  {
    Sort *sortnode = aggstate->phases[newphase + 1].sortnode;
    PlanState *outerNode = outerPlanState(aggstate);
    TupleDesc tupDesc = ExecGetResultType(outerNode);

    aggstate->sort_out = tuplesort_begin_heap(tupDesc, sortnode->numCols, sortnode->sortColIdx, sortnode->sortOperators, sortnode->collations, sortnode->nullsFirst, work_mem, NULL, false);
  }

  aggstate->current_phase = newphase;
  aggstate->phase = &aggstate->phases[newphase];
}

   
                                                                             
                                                                              
           
   
                                                                            
                                        
   
static TupleTableSlot *
fetch_input_tuple(AggState *aggstate)
{
  TupleTableSlot *slot;

  if (aggstate->sort_in)
  {
                                                                       
    CHECK_FOR_INTERRUPTS();
    if (!tuplesort_gettupleslot(aggstate->sort_in, true, false, aggstate->sort_slot, NULL))
    {
      return NULL;
    }
    slot = aggstate->sort_slot;
  }
  else
  {
    slot = ExecProcNode(outerPlanState(aggstate));
  }

  if (!TupIsNull(slot) && aggstate->sort_out)
  {
    tuplesort_puttupleslot(aggstate->sort_out, slot);
  }

  return slot;
}

   
                                           
   
                                                               
                          
   
                                                                      
   
static void
initialize_aggregate(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{
     
                                                                        
     
  if (pertrans->numSortCols > 0)
  {
       
                                                                   
                                      
       
    if (pertrans->sortstates[aggstate->current_set])
    {
      tuplesort_end(pertrans->sortstates[aggstate->current_set]);
    }

       
                                                                       
                                                         
                                          
       
    if (pertrans->numInputs == 1)
    {
      Form_pg_attribute attr = TupleDescAttr(pertrans->sortdesc, 0);

      pertrans->sortstates[aggstate->current_set] = tuplesort_begin_datum(attr->atttypid, pertrans->sortOperators[0], pertrans->sortCollations[0], pertrans->sortNullsFirst[0], work_mem, NULL, false);
    }
    else
    {
      pertrans->sortstates[aggstate->current_set] = tuplesort_begin_heap(pertrans->sortdesc, pertrans->numSortCols, pertrans->sortColIdx, pertrans->sortOperators, pertrans->sortCollations, pertrans->sortNullsFirst, work_mem, NULL, false);
    }
  }

     
                                              
     
                                                                            
                                                               
     
  if (pertrans->initValueIsNull)
  {
    pergroupstate->transValue = pertrans->initValue;
  }
  else
  {
    MemoryContext oldContext;

    oldContext = MemoryContextSwitchTo(aggstate->curaggcontext->ecxt_per_tuple_memory);
    pergroupstate->transValue = datumCopy(pertrans->initValue, pertrans->transtypeByVal, pertrans->transtypeLen);
    MemoryContextSwitchTo(oldContext);
  }
  pergroupstate->transValueIsNull = pertrans->initValueIsNull;

     
                                                                        
                                                                           
                                                                           
                                                                             
                            
     
  pergroupstate->noTransValue = pertrans->initValueIsNull;
}

   
                                                                               
   
                                                                              
                                                                               
                                                                          
                          
   
                                                                              
                                               
   
                                                                      
   
static void
initialize_aggregates(AggState *aggstate, AggStatePerGroup *pergroups, int numReset)
{
  int transno;
  int numGroupingSets = Max(aggstate->phase->numsets, 1);
  int setno = 0;
  int numTrans = aggstate->numtrans;
  AggStatePerTrans transstates = aggstate->pertrans;

  if (numReset == 0)
  {
    numReset = numGroupingSets;
  }

  for (setno = 0; setno < numReset; setno++)
  {
    AggStatePerGroup pergroup = pergroups[setno];

    select_current_set(aggstate, setno, false);

    for (transno = 0; transno < numTrans; transno++)
    {
      AggStatePerTrans pertrans = &transstates[transno];
      AggStatePerGroup pergroupstate = &pergroup[transno];

      initialize_aggregate(aggstate, pertrans, pergroupstate);
    }
  }
}

   
                                                                              
                                                                             
   
                                                                               
                                                                               
                                                                              
                                                                       
   
                                                             
   
static void
advance_transition_function(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{
  FunctionCallInfo fcinfo = pertrans->transfn_fcinfo;
  MemoryContext oldContext;
  Datum newVal;

  if (pertrans->transfn.fn_strict)
  {
       
                                                                           
                                       
       
    int numTransInputs = pertrans->numTransInputs;
    int i;

    for (i = 1; i <= numTransInputs; i++)
    {
      if (fcinfo->args[i].isnull)
      {
        return;
      }
    }
    if (pergroupstate->noTransValue)
    {
         
                                                                         
                                                                         
                                                                        
                                                           
         
                                                                         
                                                                   
         
      oldContext = MemoryContextSwitchTo(aggstate->curaggcontext->ecxt_per_tuple_memory);
      pergroupstate->transValue = datumCopy(fcinfo->args[1].value, pertrans->transtypeByVal, pertrans->transtypeLen);
      pergroupstate->transValueIsNull = false;
      pergroupstate->noTransValue = false;
      MemoryContextSwitchTo(oldContext);
      return;
    }
    if (pergroupstate->transValueIsNull)
    {
         
                                                                    
                                                                         
                                                                        
                                                            
         
      return;
    }
  }

                                                                         
  oldContext = MemoryContextSwitchTo(aggstate->tmpcontext->ecxt_per_tuple_memory);

                                                       
  aggstate->curpertrans = pertrans;

     
                                        
     
  fcinfo->args[0].value = pergroupstate->transValue;
  fcinfo->args[0].isnull = pergroupstate->transValueIsNull;
  fcinfo->isnull = false;                                          

  newVal = FunctionCallInvoke(fcinfo);

  aggstate->curpertrans = NULL;

     
                                                                          
                                                                          
                                                                             
                                                                     
                                                                    
     
                                                                   
                                                                  
                                                                     
                                                                  
                                                                     
                                                                    
                                                                    
                                                       
     
  if (!pertrans->transtypeByVal && DatumGetPointer(newVal) != DatumGetPointer(pergroupstate->transValue))
  {
    newVal = ExecAggTransReparent(aggstate, pertrans, newVal, fcinfo->isnull, pergroupstate->transValue, pergroupstate->transValueIsNull);
  }

  pergroupstate->transValue = newVal;
  pergroupstate->transValueIsNull = fcinfo->isnull;

  MemoryContextSwitchTo(oldContext);
}

   
                                                                           
                                                                       
                               
   
                                                                               
                                                                             
               
   
                                                                      
   
static void
advance_aggregates(AggState *aggstate)
{
  bool dummynull;

  ExecEvalExprSwitchContext(aggstate->phase->evaltrans, aggstate->tmpcontext, &dummynull);
}

   
                                                                    
                                                                
                                                                        
                                                                     
                                                              
   
                                                                        
                                                                       
                                              
   
                                                                      
                                                                    
                                                                       
                                                                       
                          
   
                                                               
                           
   
                                                                      
   
static void
process_ordered_aggregate_single(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{
  Datum oldVal = (Datum)0;
  bool oldIsNull = true;
  bool haveOldVal = false;
  MemoryContext workcontext = aggstate->tmpcontext->ecxt_per_tuple_memory;
  MemoryContext oldContext;
  bool isDistinct = (pertrans->numDistinctCols > 0);
  Datum newAbbrevVal = (Datum)0;
  Datum oldAbbrevVal = (Datum)0;
  FunctionCallInfo fcinfo = pertrans->transfn_fcinfo;
  Datum *newVal;
  bool *isNull;

  Assert(pertrans->numDistinctCols < 2);

  tuplesort_performsort(pertrans->sortstates[aggstate->current_set]);

                                                                        
  newVal = &fcinfo->args[1].value;
  isNull = &fcinfo->args[1].isnull;

     
                                                                             
                                                                         
                                                
     

  while (tuplesort_getdatum(pertrans->sortstates[aggstate->current_set], true, newVal, isNull, &newAbbrevVal))
  {
       
                                                                           
                                         
       
    MemoryContextReset(workcontext);
    oldContext = MemoryContextSwitchTo(workcontext);

       
                                                               
       
    if (isDistinct && haveOldVal && ((oldIsNull && *isNull) || (!oldIsNull && !*isNull && oldAbbrevVal == newAbbrevVal && DatumGetBool(FunctionCall2Coll(&pertrans->equalfnOne, pertrans->aggCollation, oldVal, *newVal)))))
    {
                                              
      if (!pertrans->inputtypeByVal && !*isNull)
      {
        pfree(DatumGetPointer(*newVal));
      }
    }
    else
    {
      advance_transition_function(aggstate, pertrans, pergroupstate);
                                        
      if (!oldIsNull && !pertrans->inputtypeByVal)
      {
        pfree(DatumGetPointer(oldVal));
      }
                                                                   
      oldVal = *newVal;
      oldAbbrevVal = newAbbrevVal;
      oldIsNull = *isNull;
      haveOldVal = true;
    }

    MemoryContextSwitchTo(oldContext);
  }

  if (!oldIsNull && !pertrans->inputtypeByVal)
  {
    pfree(DatumGetPointer(oldVal));
  }

  tuplesort_end(pertrans->sortstates[aggstate->current_set]);
  pertrans->sortstates[aggstate->current_set] = NULL;
}

   
                                                                    
                                                                     
                                                                        
                                                                     
                                                              
   
                                                               
                           
   
                                                                      
   
static void
process_ordered_aggregate_multi(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{
  ExprContext *tmpcontext = aggstate->tmpcontext;
  FunctionCallInfo fcinfo = pertrans->transfn_fcinfo;
  TupleTableSlot *slot1 = pertrans->sortslot;
  TupleTableSlot *slot2 = pertrans->uniqslot;
  int numTransInputs = pertrans->numTransInputs;
  int numDistinctCols = pertrans->numDistinctCols;
  Datum newAbbrevVal = (Datum)0;
  Datum oldAbbrevVal = (Datum)0;
  bool haveOldValue = false;
  TupleTableSlot *save = aggstate->tmpcontext->ecxt_outertuple;
  int i;

  tuplesort_performsort(pertrans->sortstates[aggstate->current_set]);

  ExecClearTuple(slot1);
  if (slot2)
  {
    ExecClearTuple(slot2);
  }

  while (tuplesort_gettupleslot(pertrans->sortstates[aggstate->current_set], true, true, slot1, &newAbbrevVal))
  {
    CHECK_FOR_INTERRUPTS();

    tmpcontext->ecxt_outertuple = slot1;
    tmpcontext->ecxt_innertuple = slot2;

    if (numDistinctCols == 0 || !haveOldValue || newAbbrevVal != oldAbbrevVal || !ExecQual(pertrans->equalfnMulti, tmpcontext))
    {
         
                                                                       
                      
         
      slot_getsomeattrs(slot1, numTransInputs);

                                   
                                                                        
      for (i = 0; i < numTransInputs; i++)
      {
        fcinfo->args[i + 1].value = slot1->tts_values[i];
        fcinfo->args[i + 1].isnull = slot1->tts_isnull[i];
      }

      advance_transition_function(aggstate, pertrans, pergroupstate);

      if (numDistinctCols > 0)
      {
                                                                
        TupleTableSlot *tmpslot = slot2;

        slot2 = slot1;
        slot1 = tmpslot;
                                                                
        oldAbbrevVal = newAbbrevVal;
        haveOldValue = true;
      }
    }

                                 
    ResetExprContext(tmpcontext);

    ExecClearTuple(slot1);
  }

  if (slot2)
  {
    ExecClearTuple(slot2);
  }

  tuplesort_end(pertrans->sortstates[aggstate->current_set]);
  pertrans->sortstates[aggstate->current_set] = NULL;

                                                                   
  tmpcontext->ecxt_outertuple = save;
}

   
                                             
   
                                                               
                           
   
                                                                   
                                                                        
   
                                                                        
                                                                          
                             
   
static void
finalize_aggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull)
{
  LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
  bool anynull = false;
  MemoryContext oldContext;
  int i;
  ListCell *lc;
  AggStatePerTrans pertrans = &aggstate->pertrans[peragg->transno];

  oldContext = MemoryContextSwitchTo(aggstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

     
                                                                           
                                                                          
                                                                             
                                     
     
  i = 1;
  foreach (lc, peragg->aggdirectargs)
  {
    ExprState *expr = (ExprState *)lfirst(lc);

    fcinfo->args[i].value = ExecEvalExpr(expr, aggstate->ss.ps.ps_ExprContext, &fcinfo->args[i].isnull);
    anynull |= fcinfo->args[i].isnull;
    i++;
  }

     
                                                                         
     
  if (OidIsValid(peragg->finalfn_oid))
  {
    int numFinalArgs = peragg->numFinalArgs;

                                                       
    aggstate->curperagg = peragg;

    InitFunctionCallInfoData(*fcinfo, &peragg->finalfn, numFinalArgs, pertrans->aggCollation, (void *)aggstate, NULL);

                                            
    fcinfo->args[0].value = MakeExpandedObjectReadOnly(pergroupstate->transValue, pergroupstate->transValueIsNull, pertrans->transtypeLen);
    fcinfo->args[0].isnull = pergroupstate->transValueIsNull;
    anynull |= pergroupstate->transValueIsNull;

                                                          
    for (; i < numFinalArgs; i++)
    {
      fcinfo->args[i].value = (Datum)0;
      fcinfo->args[i].isnull = true;
      anynull = true;
    }

    if (fcinfo->flinfo->fn_strict && anynull)
    {
                                                         
      *resultVal = (Datum)0;
      *resultIsNull = true;
    }
    else
    {
      *resultVal = FunctionCallInvoke(fcinfo);
      *resultIsNull = fcinfo->isnull;
    }
    aggstate->curperagg = NULL;
  }
  else
  {
                                                                       
    *resultVal = pergroupstate->transValue;
    *resultIsNull = pergroupstate->transValueIsNull;
  }

     
                                                                     
     
  if (!peragg->resulttypeByVal && !*resultIsNull && !MemoryContextContains(CurrentMemoryContext, DatumGetPointer(*resultVal)))
  {
    *resultVal = datumCopy(*resultVal, peragg->resulttypeByVal, peragg->resulttypeLen);
  }

  MemoryContextSwitchTo(oldContext);
}

   
                                                      
   
                                                                            
                                                                        
   
static void
finalize_partialaggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull)
{
  AggStatePerTrans pertrans = &aggstate->pertrans[peragg->transno];
  MemoryContext oldContext;

  oldContext = MemoryContextSwitchTo(aggstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

     
                                                                         
                  
     
  if (OidIsValid(pertrans->serialfn_oid))
  {
                                                                     
    if (pertrans->serialfn.fn_strict && pergroupstate->transValueIsNull)
    {
      *resultVal = (Datum)0;
      *resultIsNull = true;
    }
    else
    {
      FunctionCallInfo fcinfo = pertrans->serialfn_fcinfo;

      fcinfo->args[0].value = MakeExpandedObjectReadOnly(pergroupstate->transValue, pergroupstate->transValueIsNull, pertrans->transtypeLen);
      fcinfo->args[0].isnull = pergroupstate->transValueIsNull;
      fcinfo->isnull = false;

      *resultVal = FunctionCallInvoke(fcinfo);
      *resultIsNull = fcinfo->isnull;
    }
  }
  else
  {
                                                                       
    *resultVal = pergroupstate->transValue;
    *resultIsNull = pergroupstate->transValueIsNull;
  }

                                                                       
  if (!peragg->resulttypeByVal && !*resultIsNull && !MemoryContextContains(CurrentMemoryContext, DatumGetPointer(*resultVal)))
  {
    *resultVal = datumCopy(*resultVal, peragg->resulttypeByVal, peragg->resulttypeLen);
  }

  MemoryContextSwitchTo(oldContext);
}

   
                                                                               
                          
   
                                                                            
                                                                            
                                                          
   
                                    
   
                                                                               
                                                                       
               
   
                                                                               
                                                                            
           
   
                                                                               
                                 
   
                                                                              
                          
   
static void
prepare_projection_slot(AggState *aggstate, TupleTableSlot *slot, int currentSet)
{
  if (aggstate->phase->grouped_cols)
  {
    Bitmapset *grouped_cols = aggstate->phase->grouped_cols[currentSet];

    aggstate->grouped_cols = grouped_cols;

    if (TTS_EMPTY(slot))
    {
         
                                                                        
                                                                  
                    
         
      ExecStoreAllNullTuple(slot);
    }
    else if (aggstate->all_grouped_cols)
    {
      ListCell *lc;

                                                      
      slot_getsomeattrs(slot, linitial_int(aggstate->all_grouped_cols));

      foreach (lc, aggstate->all_grouped_cols)
      {
        int attnum = lfirst_int(lc);

        if (!bms_is_member(attnum, grouped_cols))
        {
          slot->tts_isnull[attnum - 1] = true;
        }
      }
    }
  }
}

   
                                                            
   
                                                                                
                                                                                
                                                                 
   
                                                                 
   
static void
finalize_aggregates(AggState *aggstate, AggStatePerAgg peraggs, AggStatePerGroup pergroup)
{
  ExprContext *econtext = aggstate->ss.ps.ps_ExprContext;
  Datum *aggvalues = econtext->ecxt_aggvalues;
  bool *aggnulls = econtext->ecxt_aggnulls;
  int aggno;
  int transno;

     
                                                                       
                                              
     
  for (transno = 0; transno < aggstate->numtrans; transno++)
  {
    AggStatePerTrans pertrans = &aggstate->pertrans[transno];
    AggStatePerGroup pergroupstate;

    pergroupstate = &pergroup[transno];

    if (pertrans->numSortCols > 0)
    {
      Assert(aggstate->aggstrategy != AGG_HASHED && aggstate->aggstrategy != AGG_MIXED);

      if (pertrans->numInputs == 1)
      {
        process_ordered_aggregate_single(aggstate, pertrans, pergroupstate);
      }
      else
      {
        process_ordered_aggregate_multi(aggstate, pertrans, pergroupstate);
      }
    }
  }

     
                              
     
  for (aggno = 0; aggno < aggstate->numaggs; aggno++)
  {
    AggStatePerAgg peragg = &peraggs[aggno];
    int transno = peragg->transno;
    AggStatePerGroup pergroupstate;

    pergroupstate = &pergroup[transno];

    if (DO_AGGSPLIT_SKIPFINAL(aggstate->aggsplit))
    {
      finalize_partialaggregate(aggstate, peragg, pergroupstate, &aggvalues[aggno], &aggnulls[aggno]);
    }
    else
    {
      finalize_aggregate(aggstate, peragg, pergroupstate, &aggvalues[aggno], &aggnulls[aggno]);
    }
  }
}

   
                                                                             
                                                                       
                                   
   
static TupleTableSlot *
project_aggregates(AggState *aggstate)
{
  ExprContext *econtext = aggstate->ss.ps.ps_ExprContext;

     
                                                                             
     
  if (ExecQual(aggstate->ss.ps.qual, econtext))
  {
       
                                                                        
                                       
       
    return ExecProject(aggstate->ss.ps.ps_ProjInfo);
  }
  else
  {
    InstrCountFiltered1(aggstate, 1);
  }

  return NULL;
}

   
                          
                                                                       
                                                          
   
static Bitmapset *
find_unaggregated_cols(AggState *aggstate)
{
  Agg *node = (Agg *)aggstate->ss.ps.plan;
  Bitmapset *colnos;

  colnos = NULL;
  (void)find_unaggregated_cols_walker((Node *)node->plan.targetlist, &colnos);
  (void)find_unaggregated_cols_walker((Node *)node->plan.qual, &colnos);
  return colnos;
}

static bool
find_unaggregated_cols_walker(Node *node, Bitmapset **colnos)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

                                                          
    Assert(var->varno == OUTER_VAR);
    Assert(var->varlevelsup == 0);
    *colnos = bms_add_member(*colnos, var->varattno);
    return false;
  }
  if (IsA(node, Aggref) || IsA(node, GroupingFunc))
  {
                                             
    return false;
  }
  return expression_tree_walker(node, find_unaggregated_cols_walker, (void *)colnos);
}

   
                                               
   
                                                                      
                                                                          
                                                                             
                                                                              
                   
   
                                                                               
                                               
   
                                                                              
                                                                             
                                         
   
static void
build_hash_table(AggState *aggstate)
{
  MemoryContext tmpmem = aggstate->tmpcontext->ecxt_per_tuple_memory;
  Size additionalsize;
  int i;

  Assert(aggstate->aggstrategy == AGG_HASHED || aggstate->aggstrategy == AGG_MIXED);

  additionalsize = aggstate->numtrans * sizeof(AggStatePerGroupData);

  for (i = 0; i < aggstate->num_hashes; ++i)
  {
    AggStatePerHash perhash = &aggstate->perhash[i];

    Assert(perhash->aggnode->numGroups > 0);

    if (perhash->hashtable)
    {
      ResetTupleHashTable(perhash->hashtable);
    }
    else
    {
      perhash->hashtable = BuildTupleHashTableExt(&aggstate->ss.ps, perhash->hashslot->tts_tupleDescriptor, perhash->numCols, perhash->hashGrpColIdxHash, perhash->eqfuncoids, perhash->hashfunctions, perhash->aggnode->grpCollations, perhash->aggnode->numGroups, additionalsize, aggstate->ss.ps.state->es_query_cxt, aggstate->hashcontext->ecxt_per_tuple_memory, tmpmem, DO_AGGSPLIT_SKIPFINAL(aggstate->aggsplit));
    }
  }
}

   
                                                                              
                                                                           
                                                                        
                                                                               
                                                                         
                                                                       
                                                                             
                                                                             
                                                   
   
                                                                            
                                                                               
                                                    
   
                                                                            
                                                                          
                                                                               
                                                                         
                                                                           
                  
   
                                                                             
                                                         
   
static void
find_hash_columns(AggState *aggstate)
{
  Bitmapset *base_colnos;
  List *outerTlist = outerPlanState(aggstate)->plan->targetlist;
  int numHashes = aggstate->num_hashes;
  EState *estate = aggstate->ss.ps.state;
  int j;

                                                       
  base_colnos = find_unaggregated_cols(aggstate);

  for (j = 0; j < numHashes; ++j)
  {
    AggStatePerHash perhash = &aggstate->perhash[j];
    Bitmapset *colnos = bms_copy(base_colnos);
    AttrNumber *grpColIdx = perhash->aggnode->grpColIdx;
    List *hashTlist = NIL;
    TupleDesc hashDesc;
    int maxCols;
    int i;

    perhash->largestGrpColIdx = 0;

       
                                                                           
                                                                         
                                                                         
                                                                        
                                 
       
    if (aggstate->phases[0].grouped_cols)
    {
      Bitmapset *grouped_cols = aggstate->phases[0].grouped_cols[j];
      ListCell *lc;

      foreach (lc, aggstate->all_grouped_cols)
      {
        int attnum = lfirst_int(lc);

        if (!bms_is_member(attnum, grouped_cols))
        {
          colnos = bms_del_member(colnos, attnum);
        }
      }
    }

       
                                                                       
                                                                          
                                                                          
                 
       
    maxCols = bms_num_members(colnos) + perhash->numCols;

    perhash->hashGrpColIdxInput = palloc(maxCols * sizeof(AttrNumber));
    perhash->hashGrpColIdxHash = palloc(perhash->numCols * sizeof(AttrNumber));

                                                
    for (i = 0; i < perhash->numCols; i++)
    {
      colnos = bms_add_member(colnos, grpColIdx[i]);
    }

       
                                                                      
                                                                         
                                                                        
                                                                      
                                      
       
    for (i = 0; i < perhash->numCols; i++)
    {
      perhash->hashGrpColIdxInput[i] = grpColIdx[i];
      perhash->hashGrpColIdxHash[i] = i + 1;
      perhash->numhashGrpCols++;
                                         
      bms_del_member(colnos, grpColIdx[i]);
    }

                                       
    while ((i = bms_first_member(colnos)) >= 0)
    {
      perhash->hashGrpColIdxInput[perhash->numhashGrpCols] = i;
      perhash->numhashGrpCols++;
    }

                                                        
    for (i = 0; i < perhash->numhashGrpCols; i++)
    {
      int varNumber = perhash->hashGrpColIdxInput[i] - 1;

      hashTlist = lappend(hashTlist, list_nth(outerTlist, varNumber));
      perhash->largestGrpColIdx = Max(varNumber + 1, perhash->largestGrpColIdx);
    }

    hashDesc = ExecTypeFromTL(hashTlist);

    execTuplesHashPrepare(perhash->numCols, perhash->aggnode->grpOperators, &perhash->eqfuncoids, &perhash->hashfunctions);
    perhash->hashslot = ExecAllocTableSlot(&estate->es_tupleTable, hashDesc, &TTSOpsMinimalTuple);

    list_free(hashTlist);
    bms_free(colnos);
  }

  bms_free(base_colnos);
}

   
                                                           
   
                                                                       
                                                                           
                                                                            
               
   
Size
hash_agg_entry_size(int numAggs)
{
  Size entrysize;

                                        
  entrysize = sizeof(TupleHashEntryData) + numAggs * sizeof(AggStatePerGroupData);
  entrysize = MAXALIGN(entrysize);

  return entrysize;
}

   
                                                                               
                                                                                
                                                                             
                     
   
                                                                      
   
static TupleHashEntryData *
lookup_hash_entry(AggState *aggstate)
{
  TupleTableSlot *inputslot = aggstate->tmpcontext->ecxt_outertuple;
  AggStatePerHash perhash = &aggstate->perhash[aggstate->current_set];
  TupleTableSlot *hashslot = perhash->hashslot;
  TupleHashEntryData *entry;
  bool isnew;
  int i;

                                                      
  slot_getsomeattrs(inputslot, perhash->largestGrpColIdx);
  ExecClearTuple(hashslot);

  for (i = 0; i < perhash->numhashGrpCols; i++)
  {
    int varNumber = perhash->hashGrpColIdxInput[i] - 1;

    hashslot->tts_values[i] = inputslot->tts_values[varNumber];
    hashslot->tts_isnull[i] = inputslot->tts_isnull[varNumber];
  }
  ExecStoreVirtualTuple(hashslot);

                                                                   
  entry = LookupTupleHashEntry(perhash->hashtable, hashslot, &isnew);

  if (isnew)
  {
    AggStatePerGroup pergroup;
    int transno;

    pergroup = (AggStatePerGroup)MemoryContextAlloc(perhash->hashtable->tablecxt, sizeof(AggStatePerGroupData) * aggstate->numtrans);
    entry->additional = pergroup;

       
                                                                        
                                                       
       
    for (transno = 0; transno < aggstate->numtrans; transno++)
    {
      AggStatePerTrans pertrans = &aggstate->pertrans[transno];
      AggStatePerGroup pergroupstate = &pergroup[transno];

      initialize_aggregate(aggstate, pertrans, pergroupstate);
    }
  }

  return entry;
}

   
                                                                           
                                                                            
   
                                                             
   
static void
lookup_hash_entries(AggState *aggstate)
{
  int numHashes = aggstate->num_hashes;
  AggStatePerGroup *pergroup = aggstate->hash_pergroup;
  int setno;

  for (setno = 0; setno < numHashes; setno++)
  {
    select_current_set(aggstate, setno, true);
    pergroup[setno] = lookup_hash_entry(aggstate)->additional;
  }
}

   
             
   
                                                                        
                                                                       
                                                                        
                                                                     
                                                                           
                                                                          
                                                                          
                                                                            
                       
   
static TupleTableSlot *
ExecAgg(PlanState *pstate)
{
  AggState *node = castNode(AggState, pstate);
  TupleTableSlot *result = NULL;

  CHECK_FOR_INTERRUPTS();

  if (!node->agg_done)
  {
                                    
    switch (node->phase->aggstrategy)
    {
    case AGG_HASHED:
      if (!node->table_filled)
      {
        agg_fill_hash_table(node);
      }
                       
    case AGG_MIXED:
      result = agg_retrieve_hash_table(node);
      break;
    case AGG_PLAIN:
    case AGG_SORTED:
      result = agg_retrieve_direct(node);
      break;
    }

    if (!TupIsNull(result))
    {
      return result;
    }
  }

  return NULL;
}

   
                               
   
static TupleTableSlot *
agg_retrieve_direct(AggState *aggstate)
{
  Agg *node = aggstate->phase->aggnode;
  ExprContext *econtext;
  ExprContext *tmpcontext;
  AggStatePerAgg peragg;
  AggStatePerGroup *pergroups;
  TupleTableSlot *outerslot;
  TupleTableSlot *firstSlot;
  TupleTableSlot *result;
  bool hasGroupingSets = aggstate->phase->numsets > 0;
  int numGroupingSets = Max(aggstate->phase->numsets, 1);
  int currentSet;
  int nextSetSize;
  int numReset;
  int i;

     
                              
     
                                                         
     
                                                          
     
  econtext = aggstate->ss.ps.ps_ExprContext;
  tmpcontext = aggstate->tmpcontext;

  peragg = aggstate->peragg;
  pergroups = aggstate->pergroups;
  firstSlot = aggstate->ss.ss_ScanTupleSlot;

     
                                                          
                          
     
                                                                           
                                                                   
                                                                          
                                           
     
  while (!aggstate->agg_done)
  {
       
                                                                     
                                                                         
                                                                      
                                                                         
                            
       
                                                                          
                                                                    
                                                                       
                  
       
    ReScanExprContext(econtext);

       
                                                                           
       
    if (aggstate->projected_set >= 0 && aggstate->projected_set < numGroupingSets)
    {
      numReset = aggstate->projected_set + 1;
    }
    else
    {
      numReset = numGroupingSets;
    }

       
                                                                          
                                                                          
                                                                         
                    
       

    for (i = 0; i < numReset; i++)
    {
      ReScanExprContext(aggstate->aggcontexts[i]);
    }

       
                                                                          
                                                          
       
    if (aggstate->input_done == true && aggstate->projected_set >= (numGroupingSets - 1))
    {
      if (aggstate->current_phase < aggstate->numphases - 1)
      {
        initialize_phase(aggstate, aggstate->current_phase + 1);
        aggstate->input_done = false;
        aggstate->projected_set = -1;
        numGroupingSets = Max(aggstate->phase->numsets, 1);
        node = aggstate->phase->aggnode;
        numReset = numGroupingSets;
      }
      else if (aggstate->aggstrategy == AGG_MIXED)
      {
           
                                                                   
                                                           
           
        initialize_phase(aggstate, 0);
        aggstate->table_filled = true;
        ResetTupleHashIterator(aggstate->perhash[0].hashtable, &aggstate->perhash[0].hashiter);
        select_current_set(aggstate, 0, true);
        return agg_retrieve_hash_table(aggstate);
      }
      else
      {
        aggstate->agg_done = true;
        break;
      }
    }

       
                                                                         
                                                                           
                                                       
       
    if (aggstate->projected_set >= 0 && aggstate->projected_set < (numGroupingSets - 1))
    {
      nextSetSize = aggstate->phase->gset_lengths[aggstate->projected_set + 1];
    }
    else
    {
      nextSetSize = 0;
    }

                 
                                                                          
       
                               
                                                                    
                         
          
                                                                         
            
             
                                                                         
                                                                  
             
                                                                        
                                 
                 
       
    tmpcontext->ecxt_innertuple = econtext->ecxt_outertuple;
    if (aggstate->input_done || (node->aggstrategy != AGG_PLAIN && aggstate->projected_set != -1 && aggstate->projected_set < (numGroupingSets - 1) && nextSetSize > 0 && !ExecQualAndReset(aggstate->phase->eqfunctions[nextSetSize - 1], tmpcontext)))
    {
      aggstate->projected_set += 1;

      Assert(aggstate->projected_set < numGroupingSets);
      Assert(nextSetSize > 0 || aggstate->input_done);
    }
    else
    {
         
                                                                  
                                                                    
                                                
         
      aggstate->projected_set = 0;

         
                                                                    
                                       
         
      if (aggstate->grp_firstTuple == NULL)
      {
        outerslot = fetch_input_tuple(aggstate);
        if (!TupIsNull(outerslot))
        {
             
                                                                    
                                                                 
             
          aggstate->grp_firstTuple = ExecCopySlotHeapTuple(outerslot);
        }
        else
        {
                                                    
          if (hasGroupingSets)
          {
               
                                                                
                                                               
                                                              
                                                                   
                                                        
               
                                                                   
                                              
               
            aggstate->input_done = true;

            while (aggstate->phase->gset_lengths[aggstate->projected_set] > 0)
            {
              aggstate->projected_set += 1;
              if (aggstate->projected_set >= numGroupingSets)
              {
                   
                                                               
                                                           
                                                             
                                     
                   
                break;
              }
            }

            if (aggstate->projected_set >= numGroupingSets)
            {
              continue;
            }
          }
          else
          {
            aggstate->agg_done = true;
                                                                     
            if (node->aggstrategy != AGG_PLAIN)
            {
              return NULL;
            }
          }
        }
      }

         
                                                               
         
      initialize_aggregates(aggstate, pergroups, numReset);

      if (aggstate->grp_firstTuple != NULL)
      {
           
                                                                      
                                                                  
                                  
           
        ExecForceStoreHeapTuple(aggstate->grp_firstTuple, firstSlot, true);
        aggstate->grp_firstTuple = NULL;                              

                                                      
        tmpcontext->ecxt_outertuple = firstSlot;

           
                                                                       
                                                                      
           
        for (;;)
        {
             
                                                                   
                                                       
             
          if (aggstate->aggstrategy == AGG_MIXED && aggstate->current_phase == 1)
          {
            lookup_hash_entries(aggstate);
          }

                                                             
          advance_aggregates(aggstate);

                                                              
          ResetExprContext(tmpcontext);

          outerslot = fetch_input_tuple(aggstate);
          if (TupIsNull(outerslot))
          {
                                                     
            if (hasGroupingSets)
            {
              aggstate->input_done = true;
              break;
            }
            else
            {
              aggstate->agg_done = true;
              break;
            }
          }
                                                       
          tmpcontext->ecxt_outertuple = outerslot;

             
                                                                     
                       
             
          if (node->aggstrategy != AGG_PLAIN)
          {
            tmpcontext->ecxt_innertuple = firstSlot;
            if (!ExecQual(aggstate->phase->eqfunctions[node->numCols - 1], tmpcontext))
            {
              aggstate->grp_firstTuple = ExecCopySlotHeapTuple(outerslot);
              break;
            }
          }
        }
      }

         
                                                                  
                                                                         
                                                                         
                                                                      
                                                                   
                                                       
         
      econtext->ecxt_outertuple = firstSlot;
    }

    Assert(aggstate->projected_set >= 0);

    currentSet = aggstate->projected_set;

    prepare_projection_slot(aggstate, econtext->ecxt_outertuple, currentSet);

    select_current_set(aggstate, currentSet, false);

    finalize_aggregates(aggstate, peragg, pergroups[currentSet]);

       
                                                                       
                                                               
       
    result = project_aggregates(aggstate);
    if (result)
    {
      return result;
    }
  }

                      
  return NULL;
}

   
                                                            
   
static void
agg_fill_hash_table(AggState *aggstate)
{
  TupleTableSlot *outerslot;
  ExprContext *tmpcontext = aggstate->tmpcontext;

     
                                                                          
                             
     
  for (;;)
  {
    outerslot = fetch_input_tuple(aggstate);
    if (TupIsNull(outerslot))
    {
      break;
    }

                                                               
    tmpcontext->ecxt_outertuple = outerslot;

                                         
    lookup_hash_entries(aggstate);

                                                       
    advance_aggregates(aggstate);

       
                                                                         
                                
       
    ResetExprContext(aggstate->tmpcontext);
  }

  aggstate->table_filled = true;
                                               
  select_current_set(aggstate, 0, true);
  ResetTupleHashIterator(aggstate->perhash[0].hashtable, &aggstate->perhash[0].hashiter);
}

   
                                                              
   
static TupleTableSlot *
agg_retrieve_hash_table(AggState *aggstate)
{
  ExprContext *econtext;
  AggStatePerAgg peragg;
  AggStatePerGroup pergroup;
  TupleHashEntryData *entry;
  TupleTableSlot *firstSlot;
  TupleTableSlot *result;
  AggStatePerHash perhash;

     
                               
     
                                                          
     
  econtext = aggstate->ss.ps.ps_ExprContext;
  peragg = aggstate->peragg;
  firstSlot = aggstate->ss.ss_ScanTupleSlot;

     
                                                                        
                                                                 
     
  perhash = &aggstate->perhash[aggstate->current_set];

     
                                                            
                          
     
  while (!aggstate->agg_done)
  {
    TupleTableSlot *hashslot = perhash->hashslot;
    int i;

    CHECK_FOR_INTERRUPTS();

       
                                             
       
    entry = ScanTupleHashTable(perhash->hashtable, &perhash->hashiter);
    if (entry == NULL)
    {
      int nextset = aggstate->current_set + 1;

      if (nextset < aggstate->num_hashes)
      {
           
                                                                      
                 
           
        select_current_set(aggstate, nextset, true);

        perhash = &aggstate->perhash[aggstate->current_set];

        ResetTupleHashIterator(perhash->hashtable, &perhash->hashiter);

        continue;
      }
      else
      {
                                         
        aggstate->agg_done = true;
        return NULL;
      }
    }

       
                                                         
       
                                                                           
                                                                           
                                        
       
    ResetExprContext(econtext);

       
                                                                   
                
       
    ExecStoreMinimalTuple(entry->firstTuple, hashslot, false);
    slot_getallattrs(hashslot);

    ExecClearTuple(firstSlot);
    memset(firstSlot->tts_isnull, true, firstSlot->tts_tupleDescriptor->natts * sizeof(bool));

    for (i = 0; i < perhash->numhashGrpCols; i++)
    {
      int varNumber = perhash->hashGrpColIdxInput[i] - 1;

      firstSlot->tts_values[varNumber] = hashslot->tts_values[i];
      firstSlot->tts_isnull[varNumber] = hashslot->tts_isnull[i];
    }
    ExecStoreVirtualTuple(firstSlot);

    pergroup = (AggStatePerGroup)entry->additional;

       
                                                                
                                                           
       
    econtext->ecxt_outertuple = firstSlot;

    prepare_projection_slot(aggstate, econtext->ecxt_outertuple, aggstate->current_set);

    finalize_aggregates(aggstate, peragg, pergroup);

    result = project_aggregates(aggstate);
    if (result)
    {
      return result;
    }
  }

                      
  return NULL;
}

                     
               
   
                                                                     
                                              
   
                     
   
AggState *
ExecInitAgg(Agg *node, EState *estate, int eflags)
{
  AggState *aggstate;
  AggStatePerAgg peraggs;
  AggStatePerTrans pertransstates;
  AggStatePerGroup *pergroups;
  Plan *outerPlan;
  ExprContext *econtext;
  TupleDesc scanDesc;
  int numaggs, transno, aggno;
  int phase;
  int phaseidx;
  ListCell *l;
  Bitmapset *all_grouped_cols = NULL;
  int numGroupingSets = 1;
  int numPhases;
  int numHashes;
  int i = 0;
  int j = 0;
  bool use_hashing = (node->aggstrategy == AGG_HASHED || node->aggstrategy == AGG_MIXED);

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  aggstate = makeNode(AggState);
  aggstate->ss.ps.plan = (Plan *)node;
  aggstate->ss.ps.state = estate;
  aggstate->ss.ps.ExecProcNode = ExecAgg;

  aggstate->aggs = NIL;
  aggstate->numaggs = 0;
  aggstate->numtrans = 0;
  aggstate->aggstrategy = node->aggstrategy;
  aggstate->aggsplit = node->aggsplit;
  aggstate->maxsets = 0;
  aggstate->projected_set = -1;
  aggstate->current_set = 0;
  aggstate->peragg = NULL;
  aggstate->pertrans = NULL;
  aggstate->curperagg = NULL;
  aggstate->curpertrans = NULL;
  aggstate->input_done = false;
  aggstate->agg_done = false;
  aggstate->pergroups = NULL;
  aggstate->grp_firstTuple = NULL;
  aggstate->sort_in = NULL;
  aggstate->sort_out = NULL;

     
                                                                
     
  numPhases = (use_hashing ? 1 : 2);
  numHashes = (use_hashing ? 1 : 0);

     
                                                                      
                                                                            
                                                                             
     
  if (node->groupingSets)
  {
    numGroupingSets = list_length(node->groupingSets);

    foreach (l, node->chain)
    {
      Agg *agg = lfirst(l);

      numGroupingSets = Max(numGroupingSets, list_length(agg->groupingSets));

         
                                                                    
                                    
         
      if (agg->aggstrategy != AGG_HASHED)
      {
        ++numPhases;
      }
      else
      {
        ++numHashes;
      }
    }
  }

  aggstate->maxsets = numGroupingSets;
  aggstate->numphases = numPhases;

  aggstate->aggcontexts = (ExprContext **)palloc0(sizeof(ExprContext *) * numGroupingSets);

     
                                                                 
                                                                          
                                                                           
                                                                       
                                                                             
                                                                          
                  
     
                                                                           
                                                                    
                                                                           
                                                                      
     
  ExecAssignExprContext(estate, &aggstate->ss.ps);
  aggstate->tmpcontext = aggstate->ss.ps.ps_ExprContext;

  for (i = 0; i < numGroupingSets; ++i)
  {
    ExecAssignExprContext(estate, &aggstate->ss.ps);
    aggstate->aggcontexts[i] = aggstate->ss.ps.ps_ExprContext;
  }

  if (use_hashing)
  {
    ExecAssignExprContext(estate, &aggstate->ss.ps);
    aggstate->hashcontext = aggstate->ss.ps.ps_ExprContext;
  }

  ExecAssignExprContext(estate, &aggstate->ss.ps);

     
                             
     
                                                                            
                                                      
     
  if (node->aggstrategy == AGG_HASHED)
  {
    eflags &= ~EXEC_FLAG_REWIND;
  }
  outerPlan = outerPlan(node);
  outerPlanState(aggstate) = ExecInitNode(outerPlan, estate, eflags);

     
                                   
     
  aggstate->ss.ps.outerops = ExecGetResultSlotOps(outerPlanState(&aggstate->ss), &aggstate->ss.ps.outeropsfixed);
  aggstate->ss.ps.outeropsset = true;

  ExecCreateScanSlotFromOuterPlan(estate, &aggstate->ss, aggstate->ss.ps.outerops);
  scanDesc = aggstate->ss.ss_ScanTupleSlot->tts_tupleDescriptor;

     
                                                                          
                                                                       
     
  if (numPhases > 2)
  {
    aggstate->sort_slot = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);

       
                                                                        
                                                                         
                                                                           
                                                                           
                                    
       
                                                                    
                                                                         
                                                                   
                                                                     
                                                                         
                                          
       
    if (aggstate->ss.ps.outeropsfixed && aggstate->ss.ps.outerops != &TTSOpsMinimalTuple)
    {
      aggstate->ss.ps.outeropsfixed = false;
    }
  }

     
                                                  
     
  ExecInitResultTupleSlotTL(&aggstate->ss.ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&aggstate->ss.ps, NULL);

     
                                  
     
                                                                         
                                                                             
                                                                      
                                                                        
                                                                            
     
                                                                           
                                                                           
                                                                         
     
  aggstate->ss.ps.qual = ExecInitQual(node->plan.qual, (PlanState *)aggstate);

     
                                                                       
     
  numaggs = aggstate->numaggs;
  Assert(numaggs == list_length(aggstate->aggs));

     
                                                                        
                                                                 
     
  aggstate->phases = palloc0(numPhases * sizeof(AggStatePerPhaseData));

  aggstate->num_hashes = numHashes;
  if (numHashes)
  {
    aggstate->perhash = palloc0(sizeof(AggStatePerHashData) * numHashes);
    aggstate->phases[0].numsets = 0;
    aggstate->phases[0].gset_lengths = palloc(numHashes * sizeof(int));
    aggstate->phases[0].grouped_cols = palloc(numHashes * sizeof(Bitmapset *));
  }

  phase = 0;
  for (phaseidx = 0; phaseidx <= list_length(node->chain); ++phaseidx)
  {
    Agg *aggnode;
    Sort *sortnode;

    if (phaseidx > 0)
    {
      aggnode = list_nth_node(Agg, node->chain, phaseidx - 1);
      sortnode = castNode(Sort, aggnode->plan.lefttree);
    }
    else
    {
      aggnode = node;
      sortnode = NULL;
    }

    Assert(phase <= 1 || sortnode);

    if (aggnode->aggstrategy == AGG_HASHED || aggnode->aggstrategy == AGG_MIXED)
    {
      AggStatePerPhase phasedata = &aggstate->phases[0];
      AggStatePerHash perhash;
      Bitmapset *cols = NULL;

      Assert(phase == 0);
      i = phasedata->numsets++;
      perhash = &aggstate->perhash[i];

                                                                    
      phasedata->aggnode = node;
      phasedata->aggstrategy = node->aggstrategy;

                                                                        
      perhash->aggnode = aggnode;

      phasedata->gset_lengths[i] = perhash->numCols = aggnode->numCols;

      for (j = 0; j < aggnode->numCols; ++j)
      {
        cols = bms_add_member(cols, aggnode->grpColIdx[j]);
      }

      phasedata->grouped_cols[i] = cols;

      all_grouped_cols = bms_add_members(all_grouped_cols, cols);
      continue;
    }
    else
    {
      AggStatePerPhase phasedata = &aggstate->phases[++phase];
      int num_sets;

      phasedata->numsets = num_sets = list_length(aggnode->groupingSets);

      if (num_sets)
      {
        phasedata->gset_lengths = palloc(num_sets * sizeof(int));
        phasedata->grouped_cols = palloc(num_sets * sizeof(Bitmapset *));

        i = 0;
        foreach (l, aggnode->groupingSets)
        {
          int current_length = list_length(lfirst(l));
          Bitmapset *cols = NULL;

                                                 
          for (j = 0; j < current_length; ++j)
          {
            cols = bms_add_member(cols, aggnode->grpColIdx[j]);
          }

          phasedata->grouped_cols[i] = cols;
          phasedata->gset_lengths[i] = current_length;

          ++i;
        }

        all_grouped_cols = bms_add_members(all_grouped_cols, phasedata->grouped_cols[0]);
      }
      else
      {
        Assert(phaseidx == 0);

        phasedata->gset_lengths = NULL;
        phasedata->grouped_cols = NULL;
      }

         
                                                                         
         
      if (aggnode->aggstrategy == AGG_SORTED)
      {
        int i = 0;

        Assert(aggnode->numCols > 0);

           
                                                                     
                                
           
        phasedata->eqfunctions = (ExprState **)palloc0(aggnode->numCols * sizeof(ExprState *));

                                   
        for (i = 0; i < phasedata->numsets; i++)
        {
          int length = phasedata->gset_lengths[i];

                                                    
          if (length == 0)
          {
            continue;
          }

                                                              
          if (phasedata->eqfunctions[length - 1] != NULL)
          {
            continue;
          }

          phasedata->eqfunctions[length - 1] = execTuplesMatchPrepare(scanDesc, length, aggnode->grpColIdx, aggnode->grpOperators, aggnode->grpCollations, (PlanState *)aggstate);
        }

                                                                  
        if (phasedata->eqfunctions[aggnode->numCols - 1] == NULL)
        {
          phasedata->eqfunctions[aggnode->numCols - 1] = execTuplesMatchPrepare(scanDesc, aggnode->numCols, aggnode->grpColIdx, aggnode->grpOperators, aggnode->grpCollations, (PlanState *)aggstate);
        }
      }

      phasedata->aggnode = aggnode;
      phasedata->aggstrategy = aggnode->aggstrategy;
      phasedata->sortnode = sortnode;
    }
  }

     
                                                          
     
  i = -1;
  while ((i = bms_next_member(all_grouped_cols, i)) >= 0)
  {
    aggstate->all_grouped_cols = lcons_int(i, aggstate->all_grouped_cols);
  }

     
                                                                          
                                                 
     
  econtext = aggstate->ss.ps.ps_ExprContext;
  econtext->ecxt_aggvalues = (Datum *)palloc0(sizeof(Datum) * numaggs);
  econtext->ecxt_aggnulls = (bool *)palloc0(sizeof(bool) * numaggs);

  peraggs = (AggStatePerAgg)palloc0(sizeof(AggStatePerAggData) * numaggs);
  pertransstates = (AggStatePerTrans)palloc0(sizeof(AggStatePerTransData) * numaggs);

  aggstate->peragg = peraggs;
  aggstate->pertrans = pertransstates;

  aggstate->all_pergroups = (AggStatePerGroup *)palloc0(sizeof(AggStatePerGroup) * (numGroupingSets + numHashes));
  pergroups = aggstate->all_pergroups;

  if (node->aggstrategy != AGG_HASHED)
  {
    for (i = 0; i < numGroupingSets; i++)
    {
      pergroups[i] = (AggStatePerGroup)palloc0(sizeof(AggStatePerGroupData) * numaggs);
    }

    aggstate->pergroups = pergroups;
    pergroups += numGroupingSets;
  }

     
                                                   
     
  if (use_hashing)
  {
                                                      
    aggstate->hash_pergroup = pergroups;

    find_hash_columns(aggstate);

                                                                     
    if (!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
    {
      build_hash_table(aggstate);
    }

    aggstate->table_filled = false;
  }

     
                                                                             
                                                                          
                                                                             
                                                          
     
  if (node->aggstrategy == AGG_HASHED)
  {
    aggstate->current_phase = 0;
    initialize_phase(aggstate, 0);
    select_current_set(aggstate, 0, true);
  }
  else
  {
    aggstate->current_phase = 1;
    initialize_phase(aggstate, 1);
    select_current_set(aggstate, 0, false);
  }

                       
                                                                    
                                                          
     
                                                                           
                                                                            
                                                                         
                                                              
     
                
     
                                                                
     
                                                
     
                                                                       
                                                                          
     
                                                                       
                                                                    
                                                                    
     
                                         
     
                                                                           
                                                                       
                                                                       
                                                                      
                                                  
     
                                                                             
                                                                            
                                                                      
                                     
                       
     
  aggno = -1;
  transno = -1;
  foreach (l, aggstate->aggs)
  {
    AggrefExprState *aggrefstate = (AggrefExprState *)lfirst(l);
    Aggref *aggref = aggrefstate->aggref;
    AggStatePerAgg peragg;
    AggStatePerTrans pertrans;
    int existing_aggno;
    int existing_transno;
    List *same_input_transnos;
    Oid inputTypes[FUNC_MAX_ARGS];
    int numArguments;
    int numDirectArgs;
    HeapTuple aggTuple;
    Form_pg_aggregate aggform;
    AclResult aclresult;
    Oid transfn_oid, finalfn_oid;
    bool shareable;
    Oid serialfn_oid, deserialfn_oid;
    Expr *finalfnexpr;
    Oid aggtranstype;
    Datum textInitVal;
    Datum initValue;
    bool initValueIsNull;

                                                                 
    Assert(aggref->agglevelsup == 0);
                                             
    Assert(aggref->aggsplit == aggstate->aggsplit);

                                                                  
    existing_aggno = find_compatible_peragg(aggref, aggstate, aggno, &same_input_transnos);
    if (existing_aggno != -1)
    {
         
                                                                        
                              
         
      aggrefstate->aggno = existing_aggno;
      continue;
    }

                                                                        
    peragg = &peraggs[++aggno];
    peragg->aggref = aggref;
    aggrefstate->aggno = aggno;

                                    
    aggTuple = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(aggref->aggfnoid));
    if (!HeapTupleIsValid(aggTuple))
    {
      elog(ERROR, "cache lookup failed for aggregate %u", aggref->aggfnoid);
    }
    aggform = (Form_pg_aggregate)GETSTRUCT(aggTuple);

                                                     
    aclresult = pg_proc_aclcheck(aggref->aggfnoid, GetUserId(), ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_AGGREGATE, get_func_name(aggref->aggfnoid));
    }
    InvokeFunctionExecuteHook(aggref->aggfnoid);

                                                                     
    aggtranstype = aggref->aggtranstype;
    Assert(OidIsValid(aggtranstype));

       
                                                                         
                                                                     
       
    if (DO_AGGSPLIT_COMBINE(aggstate->aggsplit))
    {
      transfn_oid = aggform->aggcombinefn;

                                                 
      if (!OidIsValid(transfn_oid))
      {
        elog(ERROR, "combinefn not set for aggregate function");
      }
    }
    else
    {
      transfn_oid = aggform->aggtransfn;
    }

                                                                         
    if (DO_AGGSPLIT_SKIPFINAL(aggstate->aggsplit))
    {
      peragg->finalfn_oid = finalfn_oid = InvalidOid;
    }
    else
    {
      peragg->finalfn_oid = finalfn_oid = aggform->aggfinalfn;
    }

       
                                                                          
                                                                           
                                                                         
       
    shareable = (aggform->aggfinalmodify != AGGMODIFY_READ_WRITE) || (finalfn_oid == InvalidOid);
    peragg->shareable = shareable;

    serialfn_oid = InvalidOid;
    deserialfn_oid = InvalidOid;

       
                                                                          
                                                    
       
    if (aggtranstype == INTERNALOID)
    {
         
                                                                        
                                                                    
                                 
         
      if (DO_AGGSPLIT_SERIALIZE(aggstate->aggsplit))
      {
                                                               
        Assert(DO_AGGSPLIT_SKIPFINAL(aggstate->aggsplit));

        if (!OidIsValid(aggform->aggserialfn))
        {
          elog(ERROR, "serialfunc not provided for serialization aggregation");
        }
        serialfn_oid = aggform->aggserialfn;
      }

                                                  
      if (DO_AGGSPLIT_DESERIALIZE(aggstate->aggsplit))
      {
                                                              
        Assert(DO_AGGSPLIT_COMBINE(aggstate->aggsplit));

        if (!OidIsValid(aggform->aggdeserialfn))
        {
          elog(ERROR, "deserialfunc not provided for deserialization aggregation");
        }
        deserialfn_oid = aggform->aggdeserialfn;
      }
    }

                                                                         
    {
      HeapTuple procTuple;
      Oid aggOwner;

      procTuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(aggref->aggfnoid));
      if (!HeapTupleIsValid(procTuple))
      {
        elog(ERROR, "cache lookup failed for function %u", aggref->aggfnoid);
      }
      aggOwner = ((Form_pg_proc)GETSTRUCT(procTuple))->proowner;
      ReleaseSysCache(procTuple);

      aclresult = pg_proc_aclcheck(transfn_oid, aggOwner, ACL_EXECUTE);
      if (aclresult != ACLCHECK_OK)
      {
        aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(transfn_oid));
      }
      InvokeFunctionExecuteHook(transfn_oid);
      if (OidIsValid(finalfn_oid))
      {
        aclresult = pg_proc_aclcheck(finalfn_oid, aggOwner, ACL_EXECUTE);
        if (aclresult != ACLCHECK_OK)
        {
          aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(finalfn_oid));
        }
        InvokeFunctionExecuteHook(finalfn_oid);
      }
      if (OidIsValid(serialfn_oid))
      {
        aclresult = pg_proc_aclcheck(serialfn_oid, aggOwner, ACL_EXECUTE);
        if (aclresult != ACLCHECK_OK)
        {
          aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(serialfn_oid));
        }
        InvokeFunctionExecuteHook(serialfn_oid);
      }
      if (OidIsValid(deserialfn_oid))
      {
        aclresult = pg_proc_aclcheck(deserialfn_oid, aggOwner, ACL_EXECUTE);
        if (aclresult != ACLCHECK_OK)
        {
          aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(deserialfn_oid));
        }
        InvokeFunctionExecuteHook(deserialfn_oid);
      }
    }

       
                                                                      
                                                                        
                                              
       
    numArguments = get_aggregate_argtypes(aggref, inputTypes);

                                              
    numDirectArgs = list_length(aggref->aggdirectargs);

                                                          
    if (aggform->aggfinalextra)
    {
      peragg->numFinalArgs = numArguments + 1;
    }
    else
    {
      peragg->numFinalArgs = numDirectArgs + 1;
    }

                                                    
    peragg->aggdirectargs = ExecInitExprList(aggref->aggdirectargs, (PlanState *)aggstate);

       
                                                                           
                                              
       
    if (OidIsValid(finalfn_oid))
    {
      build_aggregate_finalfn_expr(inputTypes, peragg->numFinalArgs, aggtranstype, aggref->aggtype, aggref->inputcollid, finalfn_oid, &finalfnexpr);
      fmgr_info(finalfn_oid, &peragg->finalfn);
      fmgr_info_set_expr((Node *)finalfnexpr, &peragg->finalfn);
    }

                                                    
    get_typlenbyval(aggref->aggtype, &peragg->resulttypeLen, &peragg->resulttypeByVal);

       
                                                                          
                                                            
       
    textInitVal = SysCacheGetAttr(AGGFNOID, aggTuple, Anum_pg_aggregate_agginitval, &initValueIsNull);
    if (initValueIsNull)
    {
      initValue = (Datum)0;
    }
    else
    {
      initValue = GetAggInitVal(textInitVal, aggtranstype);
    }

       
                                                                       
                                                                         
       
                                                                      
                                                                       
                                                   
       
    existing_transno = find_compatible_pertrans(aggstate, aggref, shareable, transfn_oid, aggtranstype, serialfn_oid, deserialfn_oid, initValue, initValueIsNull, same_input_transnos);
    if (existing_transno != -1)
    {
         
                                                                        
                                                                        
         
      pertrans = &pertransstates[existing_transno];
      pertrans->aggshared = true;
      peragg->transno = existing_transno;
    }
    else
    {
      pertrans = &pertransstates[++transno];
      build_pertrans_for_aggref(pertrans, aggstate, estate, aggref, transfn_oid, aggtranstype, serialfn_oid, deserialfn_oid, initValue, initValueIsNull, inputTypes, numArguments);
      peragg->transno = transno;
    }
    ReleaseSysCache(aggTuple);
  }

     
                                                                           
                                                                         
     
  aggstate->numaggs = aggno + 1;
  aggstate->numtrans = transno + 1;

     
                                                                           
                                                                             
                                                                           
                                                                           
                                                                            
                                                                            
                                                                            
                                    
     
  if (numaggs != list_length(aggstate->aggs))
  {
    ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("aggregate function calls cannot be nested")));
  }

     
                                                                         
                                                                        
                                                                      
                                                                         
                                
     
  for (phaseidx = 0; phaseidx < aggstate->numphases; phaseidx++)
  {
    AggStatePerPhase phase = &aggstate->phases[phaseidx];
    bool dohash = false;
    bool dosort = false;

                                           
    if (!phase->aggnode)
    {
      continue;
    }

    if (aggstate->aggstrategy == AGG_MIXED && phaseidx == 1)
    {
         
                                                                     
                                  
         
      dohash = true;
      dosort = true;
    }
    else if (aggstate->aggstrategy == AGG_MIXED && phaseidx == 0)
    {
         
                                                                         
                                                                    
                         
         
      continue;
    }
    else if (phase->aggstrategy == AGG_PLAIN || phase->aggstrategy == AGG_SORTED)
    {
      dohash = false;
      dosort = true;
    }
    else if (phase->aggstrategy == AGG_HASHED)
    {
      dohash = true;
      dosort = false;
    }
    else
    {
      Assert(false);
    }

    phase->evaltrans = ExecBuildAggTrans(aggstate, phase, dosort, dohash);
  }

  return aggstate;
}

   
                                                                       
   
                                                                            
                                                                           
                                                                          
                                                        
   
static void
build_pertrans_for_aggref(AggStatePerTrans pertrans, AggState *aggstate, EState *estate, Aggref *aggref, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, Oid *inputTypes, int numArguments)
{
  int numGroupingSets = Max(aggstate->maxsets, 1);
  Expr *serialfnexpr = NULL;
  Expr *deserialfnexpr = NULL;
  ListCell *lc;
  int numInputs;
  int numDirectArgs;
  List *sortlist;
  int numSortCols;
  int numDistinctCols;
  int i;

                                          
  pertrans->aggref = aggref;
  pertrans->aggshared = false;
  pertrans->aggCollation = aggref->inputcollid;
  pertrans->transfn_oid = aggtransfn;
  pertrans->serialfn_oid = aggserialfn;
  pertrans->deserialfn_oid = aggdeserialfn;
  pertrans->initValue = initValue;
  pertrans->initValueIsNull = initValueIsNull;

                                            
  numDirectArgs = list_length(aggref->aggdirectargs);

                                                    
  pertrans->numInputs = numInputs = list_length(aggref->args);

  pertrans->aggtranstype = aggtranstype;

     
                                                                    
                                                                          
                                                                     
                                                   
     
  if (DO_AGGSPLIT_COMBINE(aggstate->aggsplit))
  {
    Expr *combinefnexpr;
    size_t numTransArgs;

       
                                                                       
                                                                    
                            
       
    pertrans->numTransInputs = 1;

                                                  
    numTransArgs = pertrans->numTransInputs + 1;

    build_aggregate_combinefn_expr(aggtranstype, aggref->inputcollid, aggtransfn, &combinefnexpr);
    fmgr_info(aggtransfn, &pertrans->transfn);
    fmgr_info_set_expr((Node *)combinefnexpr, &pertrans->transfn);

    pertrans->transfn_fcinfo = (FunctionCallInfo)palloc(SizeForFunctionCallInfo(2));
    InitFunctionCallInfoData(*pertrans->transfn_fcinfo, &pertrans->transfn, numTransArgs, pertrans->aggCollation, (void *)aggstate, NULL);

       
                                                                        
                                                                          
                                                               
       
    if (pertrans->transfn.fn_strict && aggtranstype == INTERNALOID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("combine function with transition type %s must not be declared STRICT", format_type_be(aggtranstype))));
    }
  }
  else
  {
    Expr *transfnexpr;
    size_t numTransArgs;

                                                          
    if (AGGKIND_IS_ORDERED_SET(aggref->aggkind))
    {
      pertrans->numTransInputs = numInputs;
    }
    else
    {
      pertrans->numTransInputs = numArguments;
    }

                                                  
    numTransArgs = pertrans->numTransInputs + 1;

       
                                                                          
                           
       
    build_aggregate_transfn_expr(inputTypes, numArguments, numDirectArgs, aggref->aggvariadic, aggtranstype, aggref->inputcollid, aggtransfn, InvalidOid, &transfnexpr, NULL);
    fmgr_info(aggtransfn, &pertrans->transfn);
    fmgr_info_set_expr((Node *)transfnexpr, &pertrans->transfn);

    pertrans->transfn_fcinfo = (FunctionCallInfo)palloc(SizeForFunctionCallInfo(numTransArgs));
    InitFunctionCallInfoData(*pertrans->transfn_fcinfo, &pertrans->transfn, numTransArgs, pertrans->aggCollation, (void *)aggstate, NULL);

       
                                                                         
                                                                           
                                                                           
                                                                          
                                                                         
                         
       
    if (pertrans->transfn.fn_strict && pertrans->initValueIsNull)
    {
      if (numArguments <= numDirectArgs || !IsBinaryCoercible(inputTypes[numDirectArgs], aggtranstype))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate %u needs to have compatible input type and transition type", aggref->aggfnoid)));
      }
    }
  }

                                                 
  get_typlenbyval(aggtranstype, &pertrans->transtypeLen, &pertrans->transtypeByVal);

  if (OidIsValid(aggserialfn))
  {
    build_aggregate_serialfn_expr(aggserialfn, &serialfnexpr);
    fmgr_info(aggserialfn, &pertrans->serialfn);
    fmgr_info_set_expr((Node *)serialfnexpr, &pertrans->serialfn);

    pertrans->serialfn_fcinfo = (FunctionCallInfo)palloc(SizeForFunctionCallInfo(1));
    InitFunctionCallInfoData(*pertrans->serialfn_fcinfo, &pertrans->serialfn, 1, InvalidOid, (void *)aggstate, NULL);
  }

  if (OidIsValid(aggdeserialfn))
  {
    build_aggregate_deserialfn_expr(aggdeserialfn, &deserialfnexpr);
    fmgr_info(aggdeserialfn, &pertrans->deserialfn);
    fmgr_info_set_expr((Node *)deserialfnexpr, &pertrans->deserialfn);

    pertrans->deserialfn_fcinfo = (FunctionCallInfo)palloc(SizeForFunctionCallInfo(2));
    InitFunctionCallInfoData(*pertrans->deserialfn_fcinfo, &pertrans->deserialfn, 2, InvalidOid, (void *)aggstate, NULL);
  }

     
                                                                         
                                                                         
                                                                         
                                                                      
     
                                                                             
                                                                
     
  if (AGGKIND_IS_ORDERED_SET(aggref->aggkind))
  {
    sortlist = NIL;
    numSortCols = numDistinctCols = 0;
  }
  else if (aggref->aggdistinct)
  {
    sortlist = aggref->aggdistinct;
    numSortCols = numDistinctCols = list_length(sortlist);
    Assert(numSortCols >= list_length(aggref->aggorder));
  }
  else
  {
    sortlist = aggref->aggorder;
    numSortCols = list_length(sortlist);
    numDistinctCols = 0;
  }

  pertrans->numSortCols = numSortCols;
  pertrans->numDistinctCols = numDistinctCols;

     
                                                                          
                                                                 
                              
     
  if (numSortCols > 0 || aggref->aggfilter)
  {
    pertrans->sortdesc = ExecTypeFromTL(aggref->args);
    pertrans->sortslot = ExecInitExtraTupleSlot(estate, pertrans->sortdesc, &TTSOpsMinimalTuple);
  }

  if (numSortCols > 0)
  {
       
                                                                       
             
       
    Assert(aggstate->aggstrategy != AGG_HASHED && aggstate->aggstrategy != AGG_MIXED);

                                                                
    if (numInputs == 1)
    {
      get_typlenbyval(inputTypes[numDirectArgs], &pertrans->inputtypeLen, &pertrans->inputtypeByVal);
    }
    else if (numDistinctCols > 0)
    {
                                                            
      pertrans->uniqslot = ExecInitExtraTupleSlot(estate, pertrans->sortdesc, &TTSOpsMinimalTuple);
    }

                                                    
    pertrans->sortColIdx = (AttrNumber *)palloc(numSortCols * sizeof(AttrNumber));
    pertrans->sortOperators = (Oid *)palloc(numSortCols * sizeof(Oid));
    pertrans->sortCollations = (Oid *)palloc(numSortCols * sizeof(Oid));
    pertrans->sortNullsFirst = (bool *)palloc(numSortCols * sizeof(bool));

    i = 0;
    foreach (lc, sortlist)
    {
      SortGroupClause *sortcl = (SortGroupClause *)lfirst(lc);
      TargetEntry *tle = get_sortgroupclause_tle(sortcl, aggref->args);

                                                    
      Assert(OidIsValid(sortcl->sortop));

      pertrans->sortColIdx[i] = tle->resno;
      pertrans->sortOperators[i] = sortcl->sortop;
      pertrans->sortCollations[i] = exprCollation((Node *)tle->expr);
      pertrans->sortNullsFirst[i] = sortcl->nulls_first;
      i++;
    }
    Assert(i == numSortCols);
  }

  if (aggref->aggdistinct)
  {
    Oid *ops;

    Assert(numArguments > 0);
    Assert(list_length(aggref->aggdistinct) == numDistinctCols);

    ops = palloc(numDistinctCols * sizeof(Oid));

    i = 0;
    foreach (lc, aggref->aggdistinct)
    {
      ops[i++] = ((SortGroupClause *)lfirst(lc))->eqop;
    }

                                                  
    if (numDistinctCols == 1)
    {
      fmgr_info(get_opcode(ops[0]), &pertrans->equalfnOne);
    }
    else
    {
      pertrans->equalfnMulti = execTuplesMatchPrepare(pertrans->sortdesc, numDistinctCols, pertrans->sortColIdx, ops, pertrans->sortCollations, &aggstate->ss.ps);
    }
    pfree(ops);
  }

  pertrans->sortstates = (Tuplesortstate **)palloc0(sizeof(Tuplesortstate *) * numGroupingSets);
}

static Datum
GetAggInitVal(Datum textInitVal, Oid transtype)
{
  Oid typinput, typioparam;
  char *strInitVal;
  Datum initVal;

  getTypeInputInfo(transtype, &typinput, &typioparam);
  strInitVal = TextDatumGetCString(textInitVal);
  initVal = OidInputFunctionCall(typinput, strInitVal, typioparam, -1);
  pfree(strInitVal);
  return initVal;
}

   
                                                                               
   
                                                                                
                                                                             
                             
   
                                                                                
                                                                              
                                                                             
                                         
   
static int
find_compatible_peragg(Aggref *newagg, AggState *aggstate, int lastaggno, List **same_input_transnos)
{
  int aggno;
  AggStatePerAgg peraggs;

  *same_input_transnos = NIL;

                                                                          
  if (contain_volatile_functions((Node *)newagg))
  {
    return -1;
  }

  peraggs = aggstate->peragg;

     
                                                                       
                                                                           
                                                                         
                                                                            
                                                                          
                                                                         
                                                      
     
  for (aggno = 0; aggno <= lastaggno; aggno++)
  {
    AggStatePerAgg peragg;
    Aggref *existingRef;

    peragg = &peraggs[aggno];
    existingRef = peragg->aggref;

                                                                
    if (newagg->inputcollid != existingRef->inputcollid || newagg->aggtranstype != existingRef->aggtranstype || newagg->aggstar != existingRef->aggstar || newagg->aggvariadic != existingRef->aggvariadic || newagg->aggkind != existingRef->aggkind || !equal(newagg->args, existingRef->args) || !equal(newagg->aggorder, existingRef->aggorder) || !equal(newagg->aggdistinct, existingRef->aggdistinct) || !equal(newagg->aggfilter, existingRef->aggfilter))
    {
      continue;
    }

                                                                     
    if (newagg->aggfnoid == existingRef->aggfnoid && newagg->aggtype == existingRef->aggtype && newagg->aggcollid == existingRef->aggcollid && equal(newagg->aggdirectargs, existingRef->aggdirectargs))
    {
      list_free(*same_input_transnos);
      *same_input_transnos = NIL;
      return aggno;
    }

       
                                                                         
                                                                         
                                                                          
                                                                           
                                                                           
       
    if (peragg->shareable)
    {
      *same_input_transnos = lappend_int(*same_input_transnos, peragg->transno);
    }
  }

  return -1;
}

   
                                                                            
          
   
                                                                      
                                                                            
                       
   
static int
find_compatible_pertrans(AggState *aggstate, Aggref *newagg, bool shareable, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, List *transnos)
{
  ListCell *lc;

                                                                
  if (!shareable)
  {
    return -1;
  }

  foreach (lc, transnos)
  {
    int transno = lfirst_int(lc);
    AggStatePerTrans pertrans = &aggstate->pertrans[transno];

       
                                                                           
                              
       
    if (aggtransfn != pertrans->transfn_oid || aggtranstype != pertrans->aggtranstype)
    {
      continue;
    }

       
                                                                      
                                                                        
                                                                   
                                                                          
                      
       
    if (aggserialfn != pertrans->serialfn_oid || aggdeserialfn != pertrans->deserialfn_oid)
    {
      continue;
    }

       
                                                      
       
    if (initValueIsNull && pertrans->initValueIsNull)
    {
      return transno;
    }

    if (!initValueIsNull && !pertrans->initValueIsNull && datumIsEqual(initValue, pertrans->initValue, pertrans->transtypeByVal, pertrans->transtypeLen))
    {
      return transno;
    }
  }
  return -1;
}

void
ExecEndAgg(AggState *node)
{
  PlanState *outerPlan;
  int transno;
  int numGroupingSets = Max(node->maxsets, 1);
  int setno;

                                                    

  if (node->sort_in)
  {
    tuplesort_end(node->sort_in);
  }
  if (node->sort_out)
  {
    tuplesort_end(node->sort_out);
  }

  for (transno = 0; transno < node->numtrans; transno++)
  {
    AggStatePerTrans pertrans = &node->pertrans[transno];

    for (setno = 0; setno < numGroupingSets; setno++)
    {
      if (pertrans->sortstates[setno])
      {
        tuplesort_end(pertrans->sortstates[setno]);
      }
    }
  }

                                                              
  for (setno = 0; setno < numGroupingSets; setno++)
  {
    ReScanExprContext(node->aggcontexts[setno]);
  }
  if (node->hashcontext)
  {
    ReScanExprContext(node->hashcontext);
  }

     
                                                                  
                                                                            
               
     
  ExecFreeExprContext(&node->ss.ps);

                            
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

  outerPlan = outerPlanState(node);
  ExecEndNode(outerPlan);
}

void
ExecReScanAgg(AggState *node)
{
  ExprContext *econtext = node->ss.ps.ps_ExprContext;
  PlanState *outerPlan = outerPlanState(node);
  Agg *aggnode = (Agg *)node->ss.ps.plan;
  int transno;
  int numGroupingSets = Max(node->maxsets, 1);
  int setno;

  node->agg_done = false;

  if (node->aggstrategy == AGG_HASHED)
  {
       
                                                                          
                                                                           
                                                                        
                                            
       
    if (!node->table_filled)
    {
      return;
    }

       
                                                                       
                                                                       
                                                                       
                                                                  
       
    if (outerPlan->chgParam == NULL && !bms_overlap(node->ss.ps.chgParam, aggnode->aggParams))
    {
      ResetTupleHashIterator(node->perhash[0].hashtable, &node->perhash[0].hashiter);
      select_current_set(node, 0, true);
      return;
    }
  }

                                                    
  for (transno = 0; transno < node->numtrans; transno++)
  {
    for (setno = 0; setno < numGroupingSets; setno++)
    {
      AggStatePerTrans pertrans = &node->pertrans[transno];

      if (pertrans->sortstates[setno])
      {
        tuplesort_end(pertrans->sortstates[setno]);
        pertrans->sortstates[setno] = NULL;
      }
    }
  }

     
                                                                       
                                                                             
                                                                         
                                                                           
                                                                   
     

  for (setno = 0; setno < numGroupingSets; setno++)
  {
    ReScanExprContext(node->aggcontexts[setno]);
  }

                                                            
  if (node->grp_firstTuple != NULL)
  {
    heap_freetuple(node->grp_firstTuple);
    node->grp_firstTuple = NULL;
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

                                 
  MemSet(econtext->ecxt_aggvalues, 0, sizeof(Datum) * node->numaggs);
  MemSet(econtext->ecxt_aggnulls, 0, sizeof(bool) * node->numaggs);

     
                                                                            
                                                                             
                                             
     
  if (node->aggstrategy == AGG_HASHED || node->aggstrategy == AGG_MIXED)
  {
    ReScanExprContext(node->hashcontext);
                                     
    build_hash_table(node);
    node->table_filled = false;
                                                         
  }

  if (node->aggstrategy != AGG_HASHED)
  {
       
                                                                        
       
    for (setno = 0; setno < numGroupingSets; setno++)
    {
      MemSet(node->pergroups[setno], 0, sizeof(AggStatePerGroupData) * node->numaggs);
    }

                          
    initialize_phase(node, 1);

    node->input_done = false;
    node->projected_set = -1;
  }

  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }
}

                                                                         
                                      
                                                                         

   
                                                                                
   
                                                                            
                                                                      
                                                                         
                                                                          
                                                                            
                                               
   
                                                                         
                                                                             
                                                                             
                                                                             
                                                                             
                                                                      
   
int
AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext)
{
  if (fcinfo->context && IsA(fcinfo->context, AggState))
  {
    if (aggcontext)
    {
      AggState *aggstate = ((AggState *)fcinfo->context);
      ExprContext *cxt = aggstate->curaggcontext;

      *aggcontext = cxt->ecxt_per_tuple_memory;
    }
    return AGG_CONTEXT_AGGREGATE;
  }
  if (fcinfo->context && IsA(fcinfo->context, WindowAggState))
  {
    if (aggcontext)
    {
      *aggcontext = ((WindowAggState *)fcinfo->context)->curaggcontext;
    }
    return AGG_CONTEXT_WINDOW;
  }

                                                                 
  if (aggcontext)
  {
    *aggcontext = NULL;
  }
  return 0;
}

   
                                                                        
   
                                                                     
                                                                           
   
                                                                       
                                                                            
                                                                           
                                                                             
                                                                             
                                                              
   
                                                                           
                                                                            
                                                           
   
Aggref *
AggGetAggref(FunctionCallInfo fcinfo)
{
  if (fcinfo->context && IsA(fcinfo->context, AggState))
  {
    AggState *aggstate = (AggState *)fcinfo->context;
    AggStatePerAgg curperagg;
    AggStatePerTrans curpertrans;

                                                          
    curperagg = aggstate->curperagg;

    if (curperagg)
    {
      return curperagg->aggref;
    }

                                                                 
    curpertrans = aggstate->curpertrans;

    if (curpertrans)
    {
      return curpertrans->aggref;
    }
  }
  return NULL;
}

   
                                                                            
   
                                                                           
                                                                          
                                                                           
                                                   
   
                                                                               
   
MemoryContext
AggGetTempMemoryContext(FunctionCallInfo fcinfo)
{
  if (fcinfo->context && IsA(fcinfo->context, AggState))
  {
    AggState *aggstate = (AggState *)fcinfo->context;

    return aggstate->tmpcontext->ecxt_per_tuple_memory;
  }
  return NULL;
}

   
                                                                  
   
                                                                     
                                                                    
                                            
   
                                                                
                                                                      
                                                                    
                                                                     
                                                                   
                                                              
   
bool
AggStateIsShared(FunctionCallInfo fcinfo)
{
  if (fcinfo->context && IsA(fcinfo->context, AggState))
  {
    AggState *aggstate = (AggState *)fcinfo->context;
    AggStatePerAgg curperagg;
    AggStatePerTrans curpertrans;

                                                          
    curperagg = aggstate->curperagg;

    if (curperagg)
    {
      return aggstate->pertrans[curperagg->transno].aggshared;
    }

                                                                 
    curpertrans = aggstate->curpertrans;

    if (curpertrans)
    {
      return curpertrans->aggshared;
    }
  }
  return true;
}

   
                                                                      
   
                                                                             
                                                                             
                                                                            
                                                                               
                                                                              
                                                                             
                                                                               
                                                                               
                                                       
   
                                                                               
   
void
AggRegisterCallback(FunctionCallInfo fcinfo, ExprContextCallbackFunction func, Datum arg)
{
  if (fcinfo->context && IsA(fcinfo->context, AggState))
  {
    AggState *aggstate = (AggState *)fcinfo->context;
    ExprContext *cxt = aggstate->curaggcontext;

    RegisterExprContextCallback(cxt, func, arg);

    return;
  }
  elog(ERROR, "aggregate function cannot register a callback in this context");
}

   
                                                                     
   
                                                                           
                                                                           
                                                                     
   
                                                                       
                 
   
Datum
aggregate_dummy(PG_FUNCTION_ARGS)
{
  elog(ERROR, "aggregate function %u called as normal function", fcinfo->flinfo->fn_oid);
  return (Datum)0;                          
}
