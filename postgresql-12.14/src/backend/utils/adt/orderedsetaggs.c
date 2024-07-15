                                                                            
   
                    
                                     
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "catalog/pg_aggregate.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/tuplesort.h"

   
                                              
   
                                                                             
                                                                             
                                                                           
                                                                          
                                                                             
                                                            
   
                                                                              
                                                                       
                                                                            
                                                                         
                                                                    
   

typedef struct OSAPerQueryState
{
                                                 
  Aggref *aggref;
                                                                       
  MemoryContext qcontext;
                                         
  ExprContext *econtext;
                                                                    
  bool rescan_needed;

                                                            

                                                            
  TupleDesc tupdesc;
                                                              
  TupleTableSlot *tupslot;
                                           
  int numSortCols;
  AttrNumber *sortColIdx;
  Oid *sortOperators;
  Oid *eqOperators;
  Oid *sortCollations;
  bool *sortNullsFirsts;
                                                            
  ExprState *compareTuple;

                                                            

                                                   
  Oid sortColType;
  int16 typLen;
  bool typByVal;
  char typAlign;
                                 
  Oid sortOperator;
  Oid eqOperator;
  Oid sortCollation;
  bool sortNullsFirst;
                                                            
  FmgrInfo equalfn;
} OSAPerQueryState;

typedef struct OSAPerGroupState
{
                                                       
  OSAPerQueryState *qstate;
                                                 
  MemoryContext gcontext;
                                               
  Tuplesortstate *sortstate;
                                                      
  int64 number_of_rows;
                                                   
  bool sort_done;
} OSAPerGroupState;

static void
ordered_set_shutdown(Datum arg);

   
                                                     
   
static OSAPerGroupState *
ordered_set_startup(FunctionCallInfo fcinfo, bool use_tuples)
{
  OSAPerGroupState *osastate;
  OSAPerQueryState *qstate;
  MemoryContext gcontext;
  MemoryContext oldcontext;

     
                                                                          
                                                                             
                                                              
     
  if (AggCheckCallContext(fcinfo, &gcontext) != AGG_CONTEXT_AGGREGATE)
  {
    elog(ERROR, "ordered-set aggregate called in non-aggregate context");
  }

     
                                                                           
                                                    
     
  qstate = (OSAPerQueryState *)fcinfo->flinfo->fn_extra;
  if (qstate == NULL)
  {
    Aggref *aggref;
    MemoryContext qcontext;
    List *sortlist;
    int numSortCols;

                                                                
    aggref = AggGetAggref(fcinfo);
    if (!aggref)
    {
      elog(ERROR, "ordered-set aggregate called in non-aggregate context");
    }
    if (!AGGKIND_IS_ORDERED_SET(aggref->aggkind))
    {
      elog(ERROR, "ordered-set aggregate support function called for non-ordered-set aggregate");
    }

       
                                                                           
                                                                         
                                         
       
    qcontext = fcinfo->flinfo->fn_mcxt;
    oldcontext = MemoryContextSwitchTo(qcontext);

    qstate = (OSAPerQueryState *)palloc0(sizeof(OSAPerQueryState));
    qstate->aggref = aggref;
    qstate->qcontext = qcontext;

                                                                 
    qstate->rescan_needed = AggStateIsShared(fcinfo);

                                      
    sortlist = aggref->aggorder;
    numSortCols = list_length(sortlist);

    if (use_tuples)
    {
      bool ishypothetical = (aggref->aggkind == AGGKIND_HYPOTHETICAL);
      ListCell *lc;
      int i;

      if (ishypothetical)
      {
        numSortCols++;                                 
      }
      qstate->numSortCols = numSortCols;
      qstate->sortColIdx = (AttrNumber *)palloc(numSortCols * sizeof(AttrNumber));
      qstate->sortOperators = (Oid *)palloc(numSortCols * sizeof(Oid));
      qstate->eqOperators = (Oid *)palloc(numSortCols * sizeof(Oid));
      qstate->sortCollations = (Oid *)palloc(numSortCols * sizeof(Oid));
      qstate->sortNullsFirsts = (bool *)palloc(numSortCols * sizeof(bool));

      i = 0;
      foreach (lc, sortlist)
      {
        SortGroupClause *sortcl = (SortGroupClause *)lfirst(lc);
        TargetEntry *tle = get_sortgroupclause_tle(sortcl, aggref->args);

                                                      
        Assert(OidIsValid(sortcl->sortop));

        qstate->sortColIdx[i] = tle->resno;
        qstate->sortOperators[i] = sortcl->sortop;
        qstate->eqOperators[i] = sortcl->eqop;
        qstate->sortCollations[i] = exprCollation((Node *)tle->expr);
        qstate->sortNullsFirsts[i] = sortcl->nulls_first;
        i++;
      }

      if (ishypothetical)
      {
                                                                
        qstate->sortColIdx[i] = list_length(aggref->args) + 1;
        qstate->sortOperators[i] = Int4LessOperator;
        qstate->eqOperators[i] = Int4EqualOperator;
        qstate->sortCollations[i] = InvalidOid;
        qstate->sortNullsFirsts[i] = false;
        i++;
      }

      Assert(i == numSortCols);

         
                                                                
                                                  
         
      qstate->tupdesc = ExecTypeFromTL(aggref->args);

                                                                        
      if (ishypothetical)
      {
        TupleDesc newdesc;
        int natts = qstate->tupdesc->natts;

        newdesc = CreateTemplateTupleDesc(natts + 1);
        for (i = 1; i <= natts; i++)
        {
          TupleDescCopyEntry(newdesc, i, qstate->tupdesc, i);
        }

        TupleDescInitEntry(newdesc, (AttrNumber)++natts, "flag", INT4OID, -1, 0);

        FreeTupleDesc(qstate->tupdesc);
        qstate->tupdesc = newdesc;
      }

                                                        
      qstate->tupslot = MakeSingleTupleTableSlot(qstate->tupdesc, &TTSOpsMinimalTuple);
    }
    else
    {
                              
      SortGroupClause *sortcl;
      TargetEntry *tle;

      if (numSortCols != 1 || aggref->aggkind == AGGKIND_HYPOTHETICAL)
      {
        elog(ERROR, "ordered-set aggregate support function does not support multiple aggregated columns");
      }

      sortcl = (SortGroupClause *)linitial(sortlist);
      tle = get_sortgroupclause_tle(sortcl, aggref->args);

                                                    
      Assert(OidIsValid(sortcl->sortop));

                                   
      qstate->sortColType = exprType((Node *)tle->expr);
      qstate->sortOperator = sortcl->sortop;
      qstate->eqOperator = sortcl->eqop;
      qstate->sortCollation = exprCollation((Node *)tle->expr);
      qstate->sortNullsFirst = sortcl->nulls_first;

                              
      get_typlenbyvalalign(qstate->sortColType, &qstate->typLen, &qstate->typByVal, &qstate->typAlign);
    }

    fcinfo->flinfo->fn_extra = (void *)qstate;

    MemoryContextSwitchTo(oldcontext);
  }

                                                             
  oldcontext = MemoryContextSwitchTo(gcontext);

  osastate = (OSAPerGroupState *)palloc(sizeof(OSAPerGroupState));
  osastate->qstate = qstate;
  osastate->gcontext = gcontext;

     
                                  
     
  if (use_tuples)
  {
    osastate->sortstate = tuplesort_begin_heap(qstate->tupdesc, qstate->numSortCols, qstate->sortColIdx, qstate->sortOperators, qstate->sortCollations, qstate->sortNullsFirsts, work_mem, NULL, qstate->rescan_needed);
  }
  else
  {
    osastate->sortstate = tuplesort_begin_datum(qstate->sortColType, qstate->sortOperator, qstate->sortCollation, qstate->sortNullsFirst, work_mem, NULL, qstate->rescan_needed);
  }

  osastate->number_of_rows = 0;
  osastate->sort_done = false;

                                                                           
  AggRegisterCallback(fcinfo, ordered_set_shutdown, PointerGetDatum(osastate));

  MemoryContextSwitchTo(oldcontext);

  return osastate;
}

   
                                                                     
   
                                                                            
                                                                              
                                                                          
                                                                          
                         
   
                                                                          
                                                                               
                                                 
   
static void
ordered_set_shutdown(Datum arg)
{
  OSAPerGroupState *osastate = (OSAPerGroupState *)DatumGetPointer(arg);

                                               
  if (osastate->sortstate)
  {
    tuplesort_end(osastate->sortstate);
  }
  osastate->sortstate = NULL;
                                                                         
  if (osastate->qstate->tupslot)
  {
    ExecClearTuple(osastate->qstate->tupslot);
  }
}

   
                                                          
                                                                 
   
Datum
ordered_set_transition(PG_FUNCTION_ARGS)
{
  OSAPerGroupState *osastate;

                                                            
  if (PG_ARGISNULL(0))
  {
    osastate = ordered_set_startup(fcinfo, false);
  }
  else
  {
    osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);
  }

                                                                           
  if (!PG_ARGISNULL(1))
  {
    tuplesort_putdatum(osastate->sortstate, PG_GETARG_DATUM(1), false);
    osastate->number_of_rows++;
  }

  PG_RETURN_POINTER(osastate);
}

   
                                                          
                                                        
   
Datum
ordered_set_transition_multi(PG_FUNCTION_ARGS)
{
  OSAPerGroupState *osastate;
  TupleTableSlot *slot;
  int nargs;
  int i;

                                                            
  if (PG_ARGISNULL(0))
  {
    osastate = ordered_set_startup(fcinfo, true);
  }
  else
  {
    osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);
  }

                                                                           
  slot = osastate->qstate->tupslot;
  ExecClearTuple(slot);
  nargs = PG_NARGS() - 1;
  for (i = 0; i < nargs; i++)
  {
    slot->tts_values[i] = PG_GETARG_DATUM(i + 1);
    slot->tts_isnull[i] = PG_ARGISNULL(i + 1);
  }
  if (osastate->qstate->aggref->aggkind == AGGKIND_HYPOTHETICAL)
  {
                                                                      
    slot->tts_values[i] = Int32GetDatum(0);
    slot->tts_isnull[i] = false;
    i++;
  }
  Assert(i == slot->tts_tupleDescriptor->natts);
  ExecStoreVirtualTuple(slot);

                                              
  tuplesort_puttupleslot(osastate->sortstate, slot);
  osastate->number_of_rows++;

  PG_RETURN_POINTER(osastate);
}

   
                                                                          
   
Datum
percentile_disc_final(PG_FUNCTION_ARGS)
{
  OSAPerGroupState *osastate;
  double percentile;
  Datum val;
  bool isnull;
  int64 rownum;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                             
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }

  percentile = PG_GETARG_FLOAT8(1);

  if (percentile < 0 || percentile > 1 || isnan(percentile))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("percentile value %g is not between 0 and 1", percentile)));
  }

                                                         
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);

                                                                     
  if (osastate->number_of_rows == 0)
  {
    PG_RETURN_NULL();
  }

                                                    
  if (!osastate->sort_done)
  {
    tuplesort_performsort(osastate->sortstate);
    osastate->sort_done = true;
  }
  else
  {
    tuplesort_rescan(osastate->sortstate);
  }

               
                                                           
                                                                         
                                                                   
               
     
  rownum = (int64)ceil(percentile * osastate->number_of_rows);
  Assert(rownum <= osastate->number_of_rows);

  if (rownum > 1)
  {
    if (!tuplesort_skiptuples(osastate->sortstate, rownum - 1, true))
    {
      elog(ERROR, "missing row in percentile_disc");
    }
  }

  if (!tuplesort_getdatum(osastate->sortstate, true, &val, &isnull, NULL))
  {
    elog(ERROR, "missing row in percentile_disc");
  }

                                                                         
  if (isnull)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_DATUM(val);
  }
}

   
                                                                         
                                                                         
                              
   
typedef Datum (*LerpFunc)(Datum lo, Datum hi, double pct);

static Datum
float8_lerp(Datum lo, Datum hi, double pct)
{
  double loval = DatumGetFloat8(lo);
  double hival = DatumGetFloat8(hi);

  return Float8GetDatum(loval + (pct * (hival - loval)));
}

static Datum
interval_lerp(Datum lo, Datum hi, double pct)
{
  Datum diff_result = DirectFunctionCall2(interval_mi, hi, lo);
  Datum mul_result = DirectFunctionCall2(interval_mul, diff_result, Float8GetDatumFast(pct));

  return DirectFunctionCall2(interval_pl, mul_result, lo);
}

   
                         
   
static Datum
percentile_cont_final_common(FunctionCallInfo fcinfo, Oid expect_type, LerpFunc lerpfunc)
{
  OSAPerGroupState *osastate;
  double percentile;
  int64 first_row = 0;
  int64 second_row = 0;
  Datum val;
  Datum first_val;
  Datum second_val;
  double proportion;
  bool isnull;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                             
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }

  percentile = PG_GETARG_FLOAT8(1);

  if (percentile < 0 || percentile > 1 || isnan(percentile))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("percentile value %g is not between 0 and 1", percentile)));
  }

                                                         
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);

                                                                     
  if (osastate->number_of_rows == 0)
  {
    PG_RETURN_NULL();
  }

  Assert(expect_type == osastate->qstate->sortColType);

                                                    
  if (!osastate->sort_done)
  {
    tuplesort_performsort(osastate->sortstate);
    osastate->sort_done = true;
  }
  else
  {
    tuplesort_rescan(osastate->sortstate);
  }

  first_row = floor(percentile * (osastate->number_of_rows - 1));
  second_row = ceil(percentile * (osastate->number_of_rows - 1));

  Assert(first_row < osastate->number_of_rows);

  if (!tuplesort_skiptuples(osastate->sortstate, first_row, true))
  {
    elog(ERROR, "missing row in percentile_cont");
  }

  if (!tuplesort_getdatum(osastate->sortstate, true, &first_val, &isnull, NULL))
  {
    elog(ERROR, "missing row in percentile_cont");
  }
  if (isnull)
  {
    PG_RETURN_NULL();
  }

  if (first_row == second_row)
  {
    val = first_val;
  }
  else
  {
    if (!tuplesort_getdatum(osastate->sortstate, true, &second_val, &isnull, NULL))
    {
      elog(ERROR, "missing row in percentile_cont");
    }

    if (isnull)
    {
      PG_RETURN_NULL();
    }

    proportion = (percentile * (osastate->number_of_rows - 1)) - first_row;
    val = lerpfunc(first_val, second_val, proportion);
  }

  PG_RETURN_DATUM(val);
}

   
                                                                         
   
Datum
percentile_cont_float8_final(PG_FUNCTION_ARGS)
{
  return percentile_cont_final_common(fcinfo, FLOAT8OID, float8_lerp);
}

   
                                                                           
   
Datum
percentile_cont_interval_final(PG_FUNCTION_ARGS)
{
  return percentile_cont_final_common(fcinfo, INTERVALOID, interval_lerp);
}

   
                                                   
   
                                                                  
                                    
   
struct pct_info
{
  int64 first_row;                            
  int64 second_row;                                     
  double proportion;                             
  int idx;                                                     
};

   
                                                                  
   
static int
pct_info_cmp(const void *pa, const void *pb)
{
  const struct pct_info *a = (const struct pct_info *)pa;
  const struct pct_info *b = (const struct pct_info *)pb;

  if (a->first_row != b->first_row)
  {
    return (a->first_row < b->first_row) ? -1 : 1;
  }
  if (a->second_row != b->second_row)
  {
    return (a->second_row < b->second_row) ? -1 : 1;
  }
  return 0;
}

   
                                                                 
   
static struct pct_info *
setup_pct_info(int num_percentiles, Datum *percentiles_datum, bool *percentiles_null, int64 rowcount, bool continuous)
{
  struct pct_info *pct_info;
  int i;

  pct_info = (struct pct_info *)palloc(num_percentiles * sizeof(struct pct_info));

  for (i = 0; i < num_percentiles; i++)
  {
    pct_info[i].idx = i;

    if (percentiles_null[i])
    {
                                             
      pct_info[i].first_row = 0;
      pct_info[i].second_row = 0;
      pct_info[i].proportion = 0;
    }
    else
    {
      double p = DatumGetFloat8(percentiles_datum[i]);

      if (p < 0 || p > 1 || isnan(p))
      {
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("percentile value %g is not between 0 and 1", p)));
      }

      if (continuous)
      {
        pct_info[i].first_row = 1 + floor(p * (rowcount - 1));
        pct_info[i].second_row = 1 + ceil(p * (rowcount - 1));
        pct_info[i].proportion = (p * (rowcount - 1)) - floor(p * (rowcount - 1));
      }
      else
      {
                     
                                                                 
                                                       
                                                        
                     
           
        int64 row = (int64)ceil(p * rowcount);

        row = Max(1, row);
        pct_info[i].first_row = row;
        pct_info[i].second_row = row;
        pct_info[i].proportion = 0;
      }
    }
  }

     
                                                                            
                                                               
     
  qsort(pct_info, num_percentiles, sizeof(struct pct_info), pct_info_cmp);

  return pct_info;
}

   
                                                                              
   
Datum
percentile_disc_multi_final(PG_FUNCTION_ARGS)
{
  OSAPerGroupState *osastate;
  ArrayType *param;
  Datum *percentiles_datum;
  bool *percentiles_null;
  int num_percentiles;
  struct pct_info *pct_info;
  Datum *result_datum;
  bool *result_isnull;
  int64 rownum = 0;
  Datum val = (Datum)0;
  bool isnull = true;
  int i;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                                         
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);

                                                                     
  if (osastate->number_of_rows == 0)
  {
    PG_RETURN_NULL();
  }

                                              
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }
  param = PG_GETARG_ARRAYTYPE_P(1);

  deconstruct_array(param, FLOAT8OID,
                                          
      8, FLOAT8PASSBYVAL, 'd', &percentiles_datum, &percentiles_null, &num_percentiles);

  if (num_percentiles == 0)
  {
    PG_RETURN_POINTER(construct_empty_array(osastate->qstate->sortColType));
  }

  pct_info = setup_pct_info(num_percentiles, percentiles_datum, percentiles_null, osastate->number_of_rows, false);

  result_datum = (Datum *)palloc(num_percentiles * sizeof(Datum));
  result_isnull = (bool *)palloc(num_percentiles * sizeof(bool));

     
                                                                           
                                                                            
     
  for (i = 0; i < num_percentiles; i++)
  {
    int idx = pct_info[i].idx;

    if (pct_info[i].first_row > 0)
    {
      break;
    }

    result_datum[idx] = (Datum)0;
    result_isnull[idx] = true;
  }

     
                                                                          
                                   
     
  if (i < num_percentiles)
  {
                                                      
    if (!osastate->sort_done)
    {
      tuplesort_performsort(osastate->sortstate);
      osastate->sort_done = true;
    }
    else
    {
      tuplesort_rescan(osastate->sortstate);
    }

    for (; i < num_percentiles; i++)
    {
      int64 target_row = pct_info[i].first_row;
      int idx = pct_info[i].idx;

                                                       
      if (target_row > rownum)
      {
        if (!tuplesort_skiptuples(osastate->sortstate, target_row - rownum - 1, true))
        {
          elog(ERROR, "missing row in percentile_disc");
        }

        if (!tuplesort_getdatum(osastate->sortstate, true, &val, &isnull, NULL))
        {
          elog(ERROR, "missing row in percentile_disc");
        }

        rownum = target_row;
      }

      result_datum[idx] = val;
      result_isnull[idx] = isnull;
    }
  }

                                                            
  PG_RETURN_POINTER(construct_md_array(result_datum, result_isnull, ARR_NDIM(param), ARR_DIMS(param), ARR_LBOUND(param), osastate->qstate->sortColType, osastate->qstate->typLen, osastate->qstate->typByVal, osastate->qstate->typAlign));
}

   
                                                                      
   
static Datum
percentile_cont_multi_final_common(FunctionCallInfo fcinfo, Oid expect_type, int16 typLen, bool typByVal, char typAlign, LerpFunc lerpfunc)
{
  OSAPerGroupState *osastate;
  ArrayType *param;
  Datum *percentiles_datum;
  bool *percentiles_null;
  int num_percentiles;
  struct pct_info *pct_info;
  Datum *result_datum;
  bool *result_isnull;
  int64 rownum = 0;
  Datum first_val = (Datum)0;
  Datum second_val = (Datum)0;
  bool isnull;
  int i;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                                         
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);

                                                                     
  if (osastate->number_of_rows == 0)
  {
    PG_RETURN_NULL();
  }

  Assert(expect_type == osastate->qstate->sortColType);

                                              
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }
  param = PG_GETARG_ARRAYTYPE_P(1);

  deconstruct_array(param, FLOAT8OID,
                                          
      8, FLOAT8PASSBYVAL, 'd', &percentiles_datum, &percentiles_null, &num_percentiles);

  if (num_percentiles == 0)
  {
    PG_RETURN_POINTER(construct_empty_array(osastate->qstate->sortColType));
  }

  pct_info = setup_pct_info(num_percentiles, percentiles_datum, percentiles_null, osastate->number_of_rows, true);

  result_datum = (Datum *)palloc(num_percentiles * sizeof(Datum));
  result_isnull = (bool *)palloc(num_percentiles * sizeof(bool));

     
                                                                           
                                                                            
     
  for (i = 0; i < num_percentiles; i++)
  {
    int idx = pct_info[i].idx;

    if (pct_info[i].first_row > 0)
    {
      break;
    }

    result_datum[idx] = (Datum)0;
    result_isnull[idx] = true;
  }

     
                                                                          
                                   
     
  if (i < num_percentiles)
  {
                                                      
    if (!osastate->sort_done)
    {
      tuplesort_performsort(osastate->sortstate);
      osastate->sort_done = true;
    }
    else
    {
      tuplesort_rescan(osastate->sortstate);
    }

    for (; i < num_percentiles; i++)
    {
      int64 first_row = pct_info[i].first_row;
      int64 second_row = pct_info[i].second_row;
      int idx = pct_info[i].idx;

         
                                                                         
                                                                       
                                                                      
                                                                  
         
      if (first_row > rownum)
      {
        if (!tuplesort_skiptuples(osastate->sortstate, first_row - rownum - 1, true))
        {
          elog(ERROR, "missing row in percentile_cont");
        }

        if (!tuplesort_getdatum(osastate->sortstate, true, &first_val, &isnull, NULL) || isnull)
        {
          elog(ERROR, "missing row in percentile_cont");
        }

        rownum = first_row;
                                                                
        second_val = first_val;
      }
      else if (first_row == rownum)
      {
           
                                                                    
                                                                      
                                                                   
           
        first_val = second_val;
      }

                                      
      if (second_row > rownum)
      {
        if (!tuplesort_getdatum(osastate->sortstate, true, &second_val, &isnull, NULL) || isnull)
        {
          elog(ERROR, "missing row in percentile_cont");
        }
        rownum++;
      }
                                                            
      Assert(second_row == rownum);

                                      
      if (second_row > first_row)
      {
        result_datum[idx] = lerpfunc(first_val, second_val, pct_info[i].proportion);
      }
      else
      {
        result_datum[idx] = first_val;
      }

      result_isnull[idx] = false;
    }
  }

                                                            
  PG_RETURN_POINTER(construct_md_array(result_datum, result_isnull, ARR_NDIM(param), ARR_DIMS(param), ARR_LBOUND(param), expect_type, typLen, typByVal, typAlign));
}

   
                                                                            
   
Datum
percentile_cont_float8_multi_final(PG_FUNCTION_ARGS)
{
  return percentile_cont_multi_final_common(fcinfo, FLOAT8OID,
                                          
      8, FLOAT8PASSBYVAL, 'd', float8_lerp);
}

   
                                                                               
   
Datum
percentile_cont_interval_multi_final(PG_FUNCTION_ARGS)
{
  return percentile_cont_multi_final_common(fcinfo, INTERVALOID,
                                            
      16, false, 'd', interval_lerp);
}

   
                                                        
   
Datum
mode_final(PG_FUNCTION_ARGS)
{
  OSAPerGroupState *osastate;
  Datum val;
  bool isnull;
  Datum mode_val = (Datum)0;
  int64 mode_freq = 0;
  Datum last_val = (Datum)0;
  int64 last_val_freq = 0;
  bool last_val_is_mode = false;
  FmgrInfo *equalfn;
  Datum abbrev_val = (Datum)0;
  Datum last_abbrev_val = (Datum)0;
  bool shouldfree;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                                         
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);

                                                                     
  if (osastate->number_of_rows == 0)
  {
    PG_RETURN_NULL();
  }

                                                                            
  equalfn = &(osastate->qstate->equalfn);
  if (!OidIsValid(equalfn->fn_oid))
  {
    fmgr_info_cxt(get_opcode(osastate->qstate->eqOperator), equalfn, osastate->qstate->qcontext);
  }

  shouldfree = !(osastate->qstate->typByVal);

                                                    
  if (!osastate->sort_done)
  {
    tuplesort_performsort(osastate->sortstate);
    osastate->sort_done = true;
  }
  else
  {
    tuplesort_rescan(osastate->sortstate);
  }

                                         
  while (tuplesort_getdatum(osastate->sortstate, true, &val, &isnull, &abbrev_val))
  {
                                                             
    if (isnull)
    {
      continue;
    }

    if (last_val_freq == 0)
    {
                                                       
      mode_val = last_val = val;
      mode_freq = last_val_freq = 1;
      last_val_is_mode = true;
      last_abbrev_val = abbrev_val;
    }
    else if (abbrev_val == last_abbrev_val && DatumGetBool(FunctionCall2Coll(equalfn, PG_GET_COLLATION(), val, last_val)))
    {
                                                   
      if (last_val_is_mode)
      {
        mode_freq++;                                     
      }
      else if (++last_val_freq > mode_freq)
      {
                                       
        if (shouldfree)
        {
          pfree(DatumGetPointer(mode_val));
        }
        mode_val = last_val;
        mode_freq = last_val_freq;
        last_val_is_mode = true;
      }
      if (shouldfree)
      {
        pfree(DatumGetPointer(val));
      }
    }
    else
    {
                                       
      if (shouldfree && !last_val_is_mode)
      {
        pfree(DatumGetPointer(last_val));
      }
      last_val = val;
                                                                     
      last_abbrev_val = abbrev_val;
      last_val_freq = 1;
      last_val_is_mode = false;
    }

    CHECK_FOR_INTERRUPTS();
  }

  if (shouldfree && !last_val_is_mode)
  {
    pfree(DatumGetPointer(last_val));
  }

  if (mode_freq)
  {
    PG_RETURN_DATUM(mode_val);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                                            
                                                                          
                                                                       
   
static void
hypothetical_check_argtypes(FunctionCallInfo fcinfo, int nargs, TupleDesc tupdesc)
{
  int i;

                                              
  if (!tupdesc || (nargs + 1) != tupdesc->natts || TupleDescAttr(tupdesc, nargs)->atttypid != INT4OID)
  {
    elog(ERROR, "type mismatch in hypothetical-set function");
  }

                                                                 
  for (i = 0; i < nargs; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

    if (get_fn_expr_argtype(fcinfo->flinfo, i + 1) != attr->atttypid)
    {
      elog(ERROR, "type mismatch in hypothetical-set function");
    }
  }
}

   
                                    
   
                                                                        
                   
                                                                  
   
static int64
hypothetical_rank_common(FunctionCallInfo fcinfo, int flag, int64 *number_of_rows)
{
  int nargs = PG_NARGS() - 1;
  int64 rank = 1;
  OSAPerGroupState *osastate;
  TupleTableSlot *slot;
  int i;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                                           
  if (PG_ARGISNULL(0))
  {
    *number_of_rows = 0;
    return 1;
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);
  *number_of_rows = osastate->number_of_rows;

                                                                    
  if (nargs % 2 != 0)
  {
    elog(ERROR, "wrong number of arguments in hypothetical-set function");
  }
  nargs /= 2;

  hypothetical_check_argtypes(fcinfo, nargs, osastate->qstate->tupdesc);

                                                                           
  Assert(!osastate->sort_done);

                                                 
  slot = osastate->qstate->tupslot;
  ExecClearTuple(slot);
  for (i = 0; i < nargs; i++)
  {
    slot->tts_values[i] = PG_GETARG_DATUM(i + 1);
    slot->tts_isnull[i] = PG_ARGISNULL(i + 1);
  }
  slot->tts_values[i] = Int32GetDatum(flag);
  slot->tts_isnull[i] = false;
  ExecStoreVirtualTuple(slot);

  tuplesort_puttupleslot(osastate->sortstate, slot);

                       
  tuplesort_performsort(osastate->sortstate);
  osastate->sort_done = true;

                                                 
  while (tuplesort_gettupleslot(osastate->sortstate, true, true, slot, NULL))
  {
    bool isnull;
    Datum d = slot_getattr(slot, nargs + 1, &isnull);

    if (!isnull && DatumGetInt32(d) != 0)
    {
      break;
    }

    rank++;

    CHECK_FOR_INTERRUPTS();
  }

  ExecClearTuple(slot);

  return rank;
}

   
                                      
   
Datum
hypothetical_rank_final(PG_FUNCTION_ARGS)
{
  int64 rank;
  int64 rowcount;

  rank = hypothetical_rank_common(fcinfo, -1, &rowcount);

  PG_RETURN_INT64(rank);
}

   
                                                        
   
Datum
hypothetical_percent_rank_final(PG_FUNCTION_ARGS)
{
  int64 rank;
  int64 rowcount;
  double result_val;

  rank = hypothetical_rank_common(fcinfo, -1, &rowcount);

  if (rowcount == 0)
  {
    PG_RETURN_FLOAT8(0);
  }

  result_val = (double)(rank - 1) / (double)(rowcount);

  PG_RETURN_FLOAT8(result_val);
}

   
                                                             
   
Datum
hypothetical_cume_dist_final(PG_FUNCTION_ARGS)
{
  int64 rank;
  int64 rowcount;
  double result_val;

  rank = hypothetical_rank_common(fcinfo, 1, &rowcount);

  result_val = (double)(rank) / (double)(rowcount + 1);

  PG_RETURN_FLOAT8(result_val);
}

   
                                                                   
   
Datum
hypothetical_dense_rank_final(PG_FUNCTION_ARGS)
{
  ExprContext *econtext;
  ExprState *compareTuple;
  int nargs = PG_NARGS() - 1;
  int64 rank = 1;
  int64 duplicate_count = 0;
  OSAPerGroupState *osastate;
  int numDistinctCols;
  Datum abbrevVal = (Datum)0;
  Datum abbrevOld = (Datum)0;
  TupleTableSlot *slot;
  TupleTableSlot *extraslot;
  TupleTableSlot *slot2;
  int i;

  Assert(AggCheckCallContext(fcinfo, NULL) == AGG_CONTEXT_AGGREGATE);

                                                           
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_INT64(rank);
  }

  osastate = (OSAPerGroupState *)PG_GETARG_POINTER(0);
  econtext = osastate->qstate->econtext;
  if (!econtext)
  {
    MemoryContext oldcontext;

                                                                       
    oldcontext = MemoryContextSwitchTo(osastate->qstate->qcontext);
    osastate->qstate->econtext = CreateStandaloneExprContext();
    econtext = osastate->qstate->econtext;
    MemoryContextSwitchTo(oldcontext);
  }

                                                                    
  if (nargs % 2 != 0)
  {
    elog(ERROR, "wrong number of arguments in hypothetical-set function");
  }
  nargs /= 2;

  hypothetical_check_argtypes(fcinfo, nargs, osastate->qstate->tupdesc);

     
                                                                           
                                  
     
  numDistinctCols = osastate->qstate->numSortCols - 1;

                                                    
  compareTuple = osastate->qstate->compareTuple;
  if (compareTuple == NULL)
  {
    AttrNumber *sortColIdx = osastate->qstate->sortColIdx;
    MemoryContext oldContext;

    oldContext = MemoryContextSwitchTo(osastate->qstate->qcontext);
    compareTuple = execTuplesMatchPrepare(osastate->qstate->tupdesc, numDistinctCols, sortColIdx, osastate->qstate->eqOperators, osastate->qstate->sortCollations, NULL);
    MemoryContextSwitchTo(oldContext);
    osastate->qstate->compareTuple = compareTuple;
  }

                                                                           
  Assert(!osastate->sort_done);

                                                 
  slot = osastate->qstate->tupslot;
  ExecClearTuple(slot);
  for (i = 0; i < nargs; i++)
  {
    slot->tts_values[i] = PG_GETARG_DATUM(i + 1);
    slot->tts_isnull[i] = PG_ARGISNULL(i + 1);
  }
  slot->tts_values[i] = Int32GetDatum(-1);
  slot->tts_isnull[i] = false;
  ExecStoreVirtualTuple(slot);

  tuplesort_puttupleslot(osastate->sortstate, slot);

                       
  tuplesort_performsort(osastate->sortstate);
  osastate->sort_done = true;

     
                                                                          
                                                                      
                                                         
     
  extraslot = MakeSingleTupleTableSlot(osastate->qstate->tupdesc, &TTSOpsMinimalTuple);
  slot2 = extraslot;

                                                 
  while (tuplesort_gettupleslot(osastate->sortstate, true, true, slot, &abbrevVal))
  {
    bool isnull;
    Datum d = slot_getattr(slot, nargs + 1, &isnull);
    TupleTableSlot *tmpslot;

    if (!isnull && DatumGetInt32(d) != 0)
    {
      break;
    }

                                   
    econtext->ecxt_outertuple = slot;
    econtext->ecxt_innertuple = slot2;

    if (!TupIsNull(slot2) && abbrevVal == abbrevOld && ExecQualAndReset(compareTuple, econtext))
    {
      duplicate_count++;
    }

    tmpslot = slot2;
    slot2 = slot;
    slot = tmpslot;
                                                            
    abbrevOld = abbrevVal;

    rank++;

    CHECK_FOR_INTERRUPTS();
  }

  ExecClearTuple(slot);
  ExecClearTuple(slot2);

  ExecDropSingleTupleTableSlot(extraslot);

  rank = rank - duplicate_count;

  PG_RETURN_INT64(rank);
}
