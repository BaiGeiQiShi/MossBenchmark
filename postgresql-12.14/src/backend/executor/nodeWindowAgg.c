                                                                            
   
                   
                                         
   
                                                                            
                                                                             
                                                                           
                                                                         
                                                                        
                                                                         
                                                                            
                                                                       
   
                                                                          
                                                                     
                                                                           
                                                 
   
                                                                        
                                                                               
                                                                       
                                                                       
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "executor/executor.h"
#include "executor/nodeWindowAgg.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/regproc.h"
#include "utils/syscache.h"
#include "windowapi.h"

   
                                                                             
                                           
   
typedef struct WindowObjectData
{
  NodeTag type;
  WindowAggState *winstate;                            
  List *argstates;                                                  
  void *localmem;                                                   
  int markptr;                                                       
  int readptr;                                                       
  int64 markpos;                                                   
  int64 seekpos;                                                   
} WindowObjectData;

   
                                                                      
                                          
   
typedef struct WindowStatePerFuncData
{
                                                                          
  WindowFuncExprState *wfuncstate;
  WindowFunc *wfunc;

  int numArguments;                          

  FmgrInfo flinfo;                                           

  Oid winCollation;                                            

     
                                                                             
                                        
     
  int16 resulttypeLen;
  bool resulttypeByVal;

  bool plain_agg;                                             
  int aggno;                                          

  WindowObject winobj;                                         
} WindowStatePerFuncData;

   
                                                                    
   
typedef struct WindowStatePerAggData
{
                                    
  Oid transfn_oid;
  Oid invtransfn_oid;                        
  Oid finalfn_oid;                           

     
                                                                   
                                                                             
                          
     
  FmgrInfo transfn;
  FmgrInfo invtransfn;
  FmgrInfo finalfn;

  int numFinalArgs;                                             

     
                                           
     
  Datum initValue;
  bool initValueIsNull;

     
                                               
     
  Datum resultValue;
  bool resultValueIsNull;

     
                                                                     
                                                                       
     
  int16 inputtypeLen, resulttypeLen, transtypeLen;
  bool inputtypeByVal, resulttypeByVal, transtypeByVal;

  int wfuncno;                                      

                                                                           
  MemoryContext aggcontext;                                              

                                
  Datum transValue;                               
  bool transValueIsNull;

  int64 transValueCount;                                          

                                             
  bool restart;                                              
} WindowStatePerAggData;

static void
initialize_windowaggregate(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate);
static void
advance_windowaggregate(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate);
static bool
advance_windowaggregate_base(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate);
static void
finalize_windowaggregate(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate, Datum *result, bool *isnull);

static void
eval_windowaggregates(WindowAggState *winstate);
static void
eval_windowfunction(WindowAggState *winstate, WindowStatePerFunc perfuncstate, Datum *result, bool *isnull);

static void
begin_partition(WindowAggState *winstate);
static void
spool_tuples(WindowAggState *winstate, int64 pos);
static void
release_partition(WindowAggState *winstate);

static int
row_is_in_frame(WindowAggState *winstate, int64 pos, TupleTableSlot *slot);
static void
update_frameheadpos(WindowAggState *winstate);
static void
update_frametailpos(WindowAggState *winstate);
static void
update_grouptailpos(WindowAggState *winstate);

static WindowStatePerAggData *
initialize_peragg(WindowAggState *winstate, WindowFunc *wfunc, WindowStatePerAgg peraggstate);
static Datum
GetAggInitVal(Datum textInitVal, Oid transtype);

static bool
are_peers(WindowAggState *winstate, TupleTableSlot *slot1, TupleTableSlot *slot2);
static bool
window_gettupleslot(WindowObject winobj, int64 pos, TupleTableSlot *slot);

   
                              
                                                  
   
static void
initialize_windowaggregate(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate)
{
  MemoryContext oldContext;

     
                                                                            
                                                                            
                                                                            
     
  if (peraggstate->aggcontext != winstate->aggcontext)
  {
    MemoryContextResetAndDeleteChildren(peraggstate->aggcontext);
  }

  if (peraggstate->initValueIsNull)
  {
    peraggstate->transValue = peraggstate->initValue;
  }
  else
  {
    oldContext = MemoryContextSwitchTo(peraggstate->aggcontext);
    peraggstate->transValue = datumCopy(peraggstate->initValue, peraggstate->transtypeByVal, peraggstate->transtypeLen);
    MemoryContextSwitchTo(oldContext);
  }
  peraggstate->transValueIsNull = peraggstate->initValueIsNull;
  peraggstate->transValueCount = 0;
  peraggstate->resultValue = (Datum)0;
  peraggstate->resultValueIsNull = true;
}

   
                           
                                               
   
static void
advance_windowaggregate(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate)
{
  LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
  WindowFuncExprState *wfuncstate = perfuncstate->wfuncstate;
  int numArguments = perfuncstate->numArguments;
  Datum newVal;
  ListCell *arg;
  int i;
  MemoryContext oldContext;
  ExprContext *econtext = winstate->tmpcontext;
  ExprState *filter = wfuncstate->aggfilter;

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

                                  
  if (filter)
  {
    bool isnull;
    Datum res = ExecEvalExpr(filter, econtext, &isnull);

    if (isnull || !DatumGetBool(res))
    {
      MemoryContextSwitchTo(oldContext);
      return;
    }
  }

                                                                       
  i = 1;
  foreach (arg, wfuncstate->args)
  {
    ExprState *argstate = (ExprState *)lfirst(arg);

    fcinfo->args[i].value = ExecEvalExpr(argstate, econtext, &fcinfo->args[i].isnull);
    i++;
  }

  if (peraggstate->transfn.fn_strict)
  {
       
                                                                           
                                                                     
                      
       
    for (i = 1; i <= numArguments; i++)
    {
      if (fcinfo->args[i].isnull)
      {
        MemoryContextSwitchTo(oldContext);
        return;
      }
    }

       
                                                                          
                                                                       
                                                                          
                                     
       
                                                                           
                                                              
       
    if (peraggstate->transValueCount == 0 && peraggstate->transValueIsNull)
    {
      MemoryContextSwitchTo(peraggstate->aggcontext);
      peraggstate->transValue = datumCopy(fcinfo->args[1].value, peraggstate->transtypeByVal, peraggstate->transtypeLen);
      peraggstate->transValueIsNull = false;
      peraggstate->transValueCount = 1;
      MemoryContextSwitchTo(oldContext);
      return;
    }

    if (peraggstate->transValueIsNull)
    {
         
                                                                    
                                                                         
                                                                         
                                                                      
                                                                        
                                                                       
         
      MemoryContextSwitchTo(oldContext);
      Assert(!OidIsValid(peraggstate->invtransfn_oid));
      return;
    }
  }

     
                                                                            
                                                          
     
  InitFunctionCallInfoData(*fcinfo, &(peraggstate->transfn), numArguments + 1, perfuncstate->winCollation, (void *)winstate, NULL);
  fcinfo->args[0].value = peraggstate->transValue;
  fcinfo->args[0].isnull = peraggstate->transValueIsNull;
  winstate->curaggcontext = peraggstate->aggcontext;
  newVal = FunctionCallInvoke(fcinfo);
  winstate->curaggcontext = NULL;

     
                                                                     
                                     
     
  if (fcinfo->isnull && OidIsValid(peraggstate->invtransfn_oid))
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("moving-aggregate transition function must not return null")));
  }

     
                                                                       
                                                                            
                                                                          
                    
     
  peraggstate->transValueCount++;

     
                                                                          
                                                                          
                                                                             
                                                                     
                                                                    
     
  if (!peraggstate->transtypeByVal && DatumGetPointer(newVal) != DatumGetPointer(peraggstate->transValue))
  {
    if (!fcinfo->isnull)
    {
      MemoryContextSwitchTo(peraggstate->aggcontext);
      if (DatumIsReadWriteExpandedObject(newVal, false, peraggstate->transtypeLen) && MemoryContextGetParent(DatumGetEOHP(newVal)->eoh_context) == CurrentMemoryContext)
                        ;
      else
      {
        newVal = datumCopy(newVal, peraggstate->transtypeByVal, peraggstate->transtypeLen);
      }
    }
    if (!peraggstate->transValueIsNull)
    {
      if (DatumIsReadWriteExpandedObject(peraggstate->transValue, false, peraggstate->transtypeLen))
      {
        DeleteExpandedObject(peraggstate->transValue);
      }
      else
      {
        pfree(DatumGetPointer(peraggstate->transValue));
      }
    }
  }

  MemoryContextSwitchTo(oldContext);
  peraggstate->transValue = newVal;
  peraggstate->transValueIsNull = fcinfo->isnull;
}

   
                                
                                                
   
                                                                            
                                                                      
               
   
                                                                     
                                                                      
                                                   
   
static bool
advance_windowaggregate_base(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate)
{
  LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
  WindowFuncExprState *wfuncstate = perfuncstate->wfuncstate;
  int numArguments = perfuncstate->numArguments;
  Datum newVal;
  ListCell *arg;
  int i;
  MemoryContext oldContext;
  ExprContext *econtext = winstate->tmpcontext;
  ExprState *filter = wfuncstate->aggfilter;

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

                                  
  if (filter)
  {
    bool isnull;
    Datum res = ExecEvalExpr(filter, econtext, &isnull);

    if (isnull || !DatumGetBool(res))
    {
      MemoryContextSwitchTo(oldContext);
      return true;
    }
  }

                                                                       
  i = 1;
  foreach (arg, wfuncstate->args)
  {
    ExprState *argstate = (ExprState *)lfirst(arg);

    fcinfo->args[i].value = ExecEvalExpr(argstate, econtext, &fcinfo->args[i].isnull);
    i++;
  }

  if (peraggstate->invtransfn.fn_strict)
  {
       
                                                                      
                                                                       
                              
       
    for (i = 1; i <= numArguments; i++)
    {
      if (fcinfo->args[i].isnull)
      {
        MemoryContextSwitchTo(oldContext);
        return true;
      }
    }
  }

                                                                
  Assert(peraggstate->transValueCount > 0);

     
                                                                             
                                                                           
                                                                         
                                                                      
                                                           
                                                                
     
  if (peraggstate->transValueIsNull)
  {
    elog(ERROR, "aggregate transition value is NULL before inverse transition");
  }

     
                                                                       
                                                                            
                                                                            
                                                         
     
  if (peraggstate->transValueCount == 1)
  {
    MemoryContextSwitchTo(oldContext);
    initialize_windowaggregate(winstate, &winstate->perfunc[peraggstate->wfuncno], peraggstate);
    return true;
  }

     
                                                      
                                                                   
                          
     
  InitFunctionCallInfoData(*fcinfo, &(peraggstate->invtransfn), numArguments + 1, perfuncstate->winCollation, (void *)winstate, NULL);
  fcinfo->args[0].value = peraggstate->transValue;
  fcinfo->args[0].isnull = peraggstate->transValueIsNull;
  winstate->curaggcontext = peraggstate->aggcontext;
  newVal = FunctionCallInvoke(fcinfo);
  winstate->curaggcontext = NULL;

     
                                                                      
     
  if (fcinfo->isnull)
  {
    MemoryContextSwitchTo(oldContext);
    return false;
  }

                                                    
  peraggstate->transValueCount--;

     
                                                                          
                                                                             
                                                                     
                                                                            
                                                                        
     
                                                                         
                                                                      
     
  if (!peraggstate->transtypeByVal && DatumGetPointer(newVal) != DatumGetPointer(peraggstate->transValue))
  {
    if (!fcinfo->isnull)
    {
      MemoryContextSwitchTo(peraggstate->aggcontext);
      if (DatumIsReadWriteExpandedObject(newVal, false, peraggstate->transtypeLen) && MemoryContextGetParent(DatumGetEOHP(newVal)->eoh_context) == CurrentMemoryContext)
                        ;
      else
      {
        newVal = datumCopy(newVal, peraggstate->transtypeByVal, peraggstate->transtypeLen);
      }
    }
    if (!peraggstate->transValueIsNull)
    {
      if (DatumIsReadWriteExpandedObject(peraggstate->transValue, false, peraggstate->transtypeLen))
      {
        DeleteExpandedObject(peraggstate->transValue);
      }
      else
      {
        pfree(DatumGetPointer(peraggstate->transValue));
      }
    }
  }

  MemoryContextSwitchTo(oldContext);
  peraggstate->transValue = newVal;
  peraggstate->transValueIsNull = fcinfo->isnull;

  return true;
}

   
                            
                                               
   
static void
finalize_windowaggregate(WindowAggState *winstate, WindowStatePerFunc perfuncstate, WindowStatePerAgg peraggstate, Datum *result, bool *isnull)
{
  MemoryContext oldContext;

  oldContext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

     
                                                                         
     
  if (OidIsValid(peraggstate->finalfn_oid))
  {
    LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
    int numFinalArgs = peraggstate->numFinalArgs;
    bool anynull;
    int i;

    InitFunctionCallInfoData(fcinfodata.fcinfo, &(peraggstate->finalfn), numFinalArgs, perfuncstate->winCollation, (void *)winstate, NULL);
    fcinfo->args[0].value = MakeExpandedObjectReadOnly(peraggstate->transValue, peraggstate->transValueIsNull, peraggstate->transtypeLen);
    fcinfo->args[0].isnull = peraggstate->transValueIsNull;
    anynull = peraggstate->transValueIsNull;

                                                          
    for (i = 1; i < numFinalArgs; i++)
    {
      fcinfo->args[i].value = (Datum)0;
      fcinfo->args[i].isnull = true;
      anynull = true;
    }

    if (fcinfo->flinfo->fn_strict && anynull)
    {
                                                         
      *result = (Datum)0;
      *isnull = true;
    }
    else
    {
      winstate->curaggcontext = peraggstate->aggcontext;
      *result = FunctionCallInvoke(fcinfo);
      winstate->curaggcontext = NULL;
      *isnull = fcinfo->isnull;
    }
  }
  else
  {
                                                                       
    *result = peraggstate->transValue;
    *isnull = peraggstate->transValueIsNull;
  }

     
                                                                     
     
  if (!peraggstate->resulttypeByVal && !*isnull && !MemoryContextContains(CurrentMemoryContext, DatumGetPointer(*result)))
  {
    *result = datumCopy(*result, peraggstate->resulttypeByVal, peraggstate->resulttypeLen);
  }
  MemoryContextSwitchTo(oldContext);
}

   
                         
                                                            
   
                                                                          
                                                                               
                                                                          
                                                                            
                                                                            
              
   
static void
eval_windowaggregates(WindowAggState *winstate)
{
  WindowStatePerAgg peraggstate;
  int wfuncno, numaggs, numaggs_restart, i;
  int64 aggregatedupto_nonrestarted;
  MemoryContext oldContext;
  ExprContext *econtext;
  WindowObject agg_winobj;
  TupleTableSlot *agg_row_slot;
  TupleTableSlot *temp_slot;

  numaggs = winstate->numaggs;
  if (numaggs == 0)
  {
    return;                    
  }

                                                   
  econtext = winstate->ss.ps.ps_ExprContext;
  agg_winobj = winstate->agg_winobj;
  agg_row_slot = winstate->agg_row_slot;
  temp_slot = winstate->temp_slot_1;

     
                                                                      
                                                                        
                                                                      
                                                                             
                                                                         
                                                                            
                                                                         
                                                                            
                                                                            
                                                                           
                                                                             
                                                             
     
                                                                            
                                                                          
                                                                           
                                                                          
                                                                          
                                                                       
                                                                            
                                                                          
                                                                          
                                                                           
                           
     
                                                                           
                                                                           
                                                                       
                                                                          
     
                                                                            
                                                                             
                                                                             
                                                                         
                                                                           
                                                             
     
                                                                         
                                                                            
                                                                         
     

     
                                            
     
                                                                             
                                                           
     
  update_frameheadpos(winstate);
  if (winstate->frameheadpos < winstate->aggregatedbase)
  {
    elog(ERROR, "window frame head moved backward");
  }

     
                                                                            
                                                                        
                                                                             
                                                                          
                                                                         
                                                                            
                                                                       
                                                                            
                                                           
     
  if (winstate->aggregatedbase == winstate->frameheadpos && (winstate->frameOptions & (FRAMEOPTION_END_UNBOUNDED_FOLLOWING | FRAMEOPTION_END_CURRENT_ROW)) && !(winstate->frameOptions & FRAMEOPTION_EXCLUSION) && winstate->aggregatedbase <= winstate->currentpos && winstate->aggregatedupto > winstate->currentpos)
  {
    for (i = 0; i < numaggs; i++)
    {
      peraggstate = &winstate->peragg[i];
      wfuncno = peraggstate->wfuncno;
      econtext->ecxt_aggvalues[wfuncno] = peraggstate->resultValue;
      econtext->ecxt_aggnulls[wfuncno] = peraggstate->resultValueIsNull;
    }
    return;
  }

               
                               
     
                                 
                                                               
                                                               
                                
                                        
                                                     
     
                                                                          
                                                                           
                       
               
     
  numaggs_restart = 0;
  for (i = 0; i < numaggs; i++)
  {
    peraggstate = &winstate->peragg[i];
    if (winstate->currentpos == 0 || (winstate->aggregatedbase != winstate->frameheadpos && !OidIsValid(peraggstate->invtransfn_oid)) || (winstate->frameOptions & FRAMEOPTION_EXCLUSION) || winstate->aggregatedupto <= winstate->frameheadpos)
    {
      peraggstate->restart = true;
      numaggs_restart++;
    }
    else
    {
      peraggstate->restart = false;
    }
  }

     
                                                                   
                                                                          
                                                                          
                                                                         
                                         
     
  while (numaggs_restart < numaggs && winstate->aggregatedbase < winstate->frameheadpos)
  {
       
                                                                           
                                           
       
    if (!window_gettupleslot(agg_winobj, winstate->aggregatedbase, temp_slot))
    {
      elog(ERROR, "could not re-fetch previously fetched frame row");
    }

                                                                 
    winstate->tmpcontext->ecxt_outertuple = temp_slot;

       
                                                                         
                                                                       
       
    for (i = 0; i < numaggs; i++)
    {
      bool ok;

      peraggstate = &winstate->peragg[i];
      if (peraggstate->restart)
      {
        continue;
      }

      wfuncno = peraggstate->wfuncno;
      ok = advance_windowaggregate_base(winstate, &winstate->perfunc[wfuncno], peraggstate);
      if (!ok)
      {
                                                                  
        peraggstate->restart = true;
        numaggs_restart++;
      }
    }

                                                        
    ResetExprContext(winstate->tmpcontext);

                                              
    winstate->aggregatedbase++;
    ExecClearTuple(temp_slot);
  }

     
                                                                      
                                                                          
                                          
     
  winstate->aggregatedbase = winstate->frameheadpos;

     
                                                                             
                                                            
     
  if (agg_winobj->markptr >= 0)
  {
    WinSetMarkPosition(agg_winobj, winstate->frameheadpos);
  }

     
                                                 
     
                                                                          
                                                                   
                                                                       
                                                                            
                                                                          
                                            
     
  if (numaggs_restart > 0)
  {
    MemoryContextResetAndDeleteChildren(winstate->aggcontext);
  }
  for (i = 0; i < numaggs; i++)
  {
    peraggstate = &winstate->peragg[i];

                                                                        
    Assert(peraggstate->aggcontext != winstate->aggcontext || numaggs_restart == 0 || peraggstate->restart);

    if (peraggstate->restart)
    {
      wfuncno = peraggstate->wfuncno;
      initialize_windowaggregate(winstate, &winstate->perfunc[wfuncno], peraggstate);
    }
    else if (!peraggstate->resultValueIsNull)
    {
      if (!peraggstate->resulttypeByVal)
      {
        pfree(DatumGetPointer(peraggstate->resultValue));
      }
      peraggstate->resultValue = (Datum)0;
      peraggstate->resultValueIsNull = true;
    }
  }

     
                                                                          
                                                                         
                                                                           
                                                                     
                                                                    
                                                                     
                                                                  
                                                 
     
  aggregatedupto_nonrestarted = winstate->aggregatedupto;
  if (numaggs_restart > 0 && winstate->aggregatedupto != winstate->frameheadpos)
  {
    winstate->aggregatedupto = winstate->frameheadpos;
    ExecClearTuple(agg_row_slot);
  }

     
                                                                      
     
                                                                            
                                                                             
            
     
  for (;;)
  {
    int ret;

                                             
    if (TupIsNull(agg_row_slot))
    {
      if (!window_gettupleslot(agg_winobj, winstate->aggregatedupto, agg_row_slot))
      {
        break;                               
      }
    }

       
                                                                       
                                                                         
       
    ret = row_is_in_frame(winstate, winstate->aggregatedupto, agg_row_slot);
    if (ret < 0)
    {
      break;
    }
    if (ret == 0)
    {
      goto next_tuple;
    }

                                                                 
    winstate->tmpcontext->ecxt_outertuple = agg_row_slot;

                                            
    for (i = 0; i < numaggs; i++)
    {
      peraggstate = &winstate->peragg[i];

                                                                     
      if (!peraggstate->restart && winstate->aggregatedupto < aggregatedupto_nonrestarted)
      {
        continue;
      }

      wfuncno = peraggstate->wfuncno;
      advance_windowaggregate(winstate, &winstate->perfunc[wfuncno], peraggstate);
    }

  next_tuple:
                                                        
    ResetExprContext(winstate->tmpcontext);

                                              
    winstate->aggregatedupto++;
    ExecClearTuple(agg_row_slot);
  }

                                                               
  Assert(aggregatedupto_nonrestarted <= winstate->aggregatedupto);

     
                                                        
     
  for (i = 0; i < numaggs; i++)
  {
    Datum *result;
    bool *isnull;

    peraggstate = &winstate->peragg[i];
    wfuncno = peraggstate->wfuncno;
    result = &econtext->ecxt_aggvalues[wfuncno];
    isnull = &econtext->ecxt_aggnulls[wfuncno];
    finalize_windowaggregate(winstate, &winstate->perfunc[wfuncno], peraggstate, result, isnull);

       
                                                               
       
                                                                          
                                                                         
                                                       
       
    if (!peraggstate->resulttypeByVal && !*isnull)
    {
      oldContext = MemoryContextSwitchTo(peraggstate->aggcontext);
      peraggstate->resultValue = datumCopy(*result, peraggstate->resulttypeByVal, peraggstate->resulttypeLen);
      MemoryContextSwitchTo(oldContext);
    }
    else
    {
      peraggstate->resultValue = *result;
    }
    peraggstate->resultValueIsNull = *isnull;
  }
}

   
                       
   
                                                                          
                                                                       
                                                                     
                                                                         
             
   
static void
eval_windowfunction(WindowAggState *winstate, WindowStatePerFunc perfuncstate, Datum *result, bool *isnull)
{
  LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
  MemoryContext oldContext;

  oldContext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

     
                                                                             
                                                                    
                                                                             
                                                                         
     
  InitFunctionCallInfoData(*fcinfo, &(perfuncstate->flinfo), perfuncstate->numArguments, perfuncstate->winCollation, (void *)perfuncstate->winobj, NULL);
                                                                 
  for (int argno = 0; argno < perfuncstate->numArguments; argno++)
  {
    fcinfo->args[argno].isnull = true;
  }
                                                                       
  winstate->curaggcontext = NULL;

  *result = FunctionCallInvoke(fcinfo);
  *isnull = fcinfo->isnull;

     
                                                                             
                                                                            
                                      
     
  if (!perfuncstate->resulttypeByVal && !fcinfo->isnull && !MemoryContextContains(CurrentMemoryContext, DatumGetPointer(*result)))
  {
    *result = datumCopy(*result, perfuncstate->resulttypeByVal, perfuncstate->resulttypeLen);
  }

  MemoryContextSwitchTo(oldContext);
}

   
                   
                                               
   
static void
begin_partition(WindowAggState *winstate)
{
  WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;
  PlanState *outerPlan = outerPlanState(winstate);
  int frameOptions = winstate->frameOptions;
  int numfuncs = winstate->numfuncs;
  int i;

  winstate->partition_spooled = false;
  winstate->framehead_valid = false;
  winstate->frametail_valid = false;
  winstate->grouptail_valid = false;
  winstate->spooled_rows = 0;
  winstate->currentpos = 0;
  winstate->frameheadpos = 0;
  winstate->frametailpos = 0;
  winstate->currentgroup = 0;
  winstate->frameheadgroup = 0;
  winstate->frametailgroup = 0;
  winstate->groupheadpos = 0;
  winstate->grouptailpos = -1;                              
  ExecClearTuple(winstate->agg_row_slot);
  if (winstate->framehead_slot)
  {
    ExecClearTuple(winstate->framehead_slot);
  }
  if (winstate->frametail_slot)
  {
    ExecClearTuple(winstate->frametail_slot);
  }

     
                                                                           
                                      
     
  if (TupIsNull(winstate->first_part_slot))
  {
    TupleTableSlot *outerslot = ExecProcNode(outerPlan);

    if (!TupIsNull(outerslot))
    {
      ExecCopySlot(winstate->first_part_slot, outerslot);
    }
    else
    {
                                                         
      winstate->partition_spooled = true;
      winstate->more_partitions = false;
      return;
    }
  }

                                                
  winstate->buffer = tuplestore_begin_heap(false, false, work_mem);

     
                                                                           
                                                                             
                                                                             
     
  winstate->current_ptr = 0;                                      

                                                           
  tuplestore_set_eflags(winstate->buffer, 0);

                                                      
  if (winstate->numaggs > 0)
  {
    WindowObject agg_winobj = winstate->agg_winobj;
    int readptr_flags = 0;

       
                                                                         
                                                        
       
    if (!(frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING) || (frameOptions & FRAMEOPTION_EXCLUSION))
    {
                                                                
      agg_winobj->markptr = tuplestore_alloc_read_pointer(winstate->buffer, 0);
                                                              
      readptr_flags |= EXEC_FLAG_BACKWARD;
    }

    agg_winobj->readptr = tuplestore_alloc_read_pointer(winstate->buffer, readptr_flags);
    agg_winobj->markpos = -1;
    agg_winobj->seekpos = -1;

                                                    
    winstate->aggregatedbase = 0;
    winstate->aggregatedupto = 0;
  }

                                                                   
  for (i = 0; i < numfuncs; i++)
  {
    WindowStatePerFunc perfuncstate = &(winstate->perfunc[i]);

    if (!perfuncstate->plain_agg)
    {
      WindowObject winobj = perfuncstate->winobj;

      winobj->markptr = tuplestore_alloc_read_pointer(winstate->buffer, 0);
      winobj->readptr = tuplestore_alloc_read_pointer(winstate->buffer, EXEC_FLAG_BACKWARD);
      winobj->markpos = -1;
      winobj->seekpos = -1;
    }
  }

     
                                                                          
                                                                            
                                                                           
                                                                        
                                                                           
                                                                        
     
  winstate->framehead_ptr = winstate->frametail_ptr = -1;                  

  if (frameOptions & (FRAMEOPTION_RANGE | FRAMEOPTION_GROUPS))
  {
    if (((frameOptions & FRAMEOPTION_START_CURRENT_ROW) && node->ordNumCols != 0) || (frameOptions & FRAMEOPTION_START_OFFSET))
    {
      winstate->framehead_ptr = tuplestore_alloc_read_pointer(winstate->buffer, 0);
    }
    if (((frameOptions & FRAMEOPTION_END_CURRENT_ROW) && node->ordNumCols != 0) || (frameOptions & FRAMEOPTION_END_OFFSET))
    {
      winstate->frametail_ptr = tuplestore_alloc_read_pointer(winstate->buffer, 0);
    }
  }

     
                                                                            
                                                                         
                                                                       
                                                                            
                                                                  
     
  winstate->grouptail_ptr = -1;

  if ((frameOptions & (FRAMEOPTION_EXCLUDE_GROUP | FRAMEOPTION_EXCLUDE_TIES)) && node->ordNumCols != 0)
  {
    winstate->grouptail_ptr = tuplestore_alloc_read_pointer(winstate->buffer, 0);
  }

     
                                                                           
                                                                            
     
  tuplestore_puttupleslot(winstate->buffer, winstate->first_part_slot);
  winstate->spooled_rows++;
}

   
                                                                            
                                                                            
   
static void
spool_tuples(WindowAggState *winstate, int64 pos)
{
  WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;
  PlanState *outerPlan;
  TupleTableSlot *outerslot;
  MemoryContext oldcontext;

  if (!winstate->buffer)
  {
    return;                          
  }
  if (winstate->partition_spooled)
  {
    return;                                   
  }

     
                                                                          
                                                                           
                                                             
     
                                                                          
                                       
     
  if (!tuplestore_in_memory(winstate->buffer))
  {
    pos = -1;
  }

  outerPlan = outerPlanState(winstate);

                                                  
  oldcontext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_query_memory);

  while (winstate->spooled_rows <= pos || pos == -1)
  {
    outerslot = ExecProcNode(outerPlan);
    if (TupIsNull(outerslot))
    {
                                                 
      winstate->partition_spooled = true;
      winstate->more_partitions = false;
      break;
    }

    if (node->partNumCols > 0)
    {
      ExprContext *econtext = winstate->tmpcontext;

      econtext->ecxt_innertuple = winstate->first_part_slot;
      econtext->ecxt_outertuple = outerslot;

                                                                      
      if (!ExecQualAndReset(winstate->partEqfunction, econtext))
      {
           
                                                                
           
        ExecCopySlot(winstate->first_part_slot, outerslot);
        winstate->partition_spooled = true;
        winstate->more_partitions = true;
        break;
      }
    }

                                                            
    tuplestore_puttupleslot(winstate->buffer, outerslot);
    winstate->spooled_rows++;
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                     
                                                        
                                     
   
static void
release_partition(WindowAggState *winstate)
{
  int i;

  for (i = 0; i < winstate->numfuncs; i++)
  {
    WindowStatePerFunc perfuncstate = &(winstate->perfunc[i]);

                                                                   
    if (perfuncstate->winobj)
    {
      perfuncstate->winobj->localmem = NULL;
    }
  }

     
                                                                            
                                                                             
                                                                           
                                                                            
     
  MemoryContextResetAndDeleteChildren(winstate->partcontext);
  MemoryContextResetAndDeleteChildren(winstate->aggcontext);
  for (i = 0; i < winstate->numaggs; i++)
  {
    if (winstate->peragg[i].aggcontext != winstate->aggcontext)
    {
      MemoryContextResetAndDeleteChildren(winstate->peragg[i].aggcontext);
    }
  }

  if (winstate->buffer)
  {
    tuplestore_end(winstate->buffer);
  }
  winstate->buffer = NULL;
  winstate->partition_spooled = false;
}

   
                   
                                                                          
                              
   
                                                                            
                                                                            
          
   
            
                                                                         
                                                                       
                             
   
                                      
   
static int
row_is_in_frame(WindowAggState *winstate, int64 pos, TupleTableSlot *slot)
{
  int frameOptions = winstate->frameOptions;

  Assert(pos >= 0);                        

     
                                                                             
                                                                     
     
  update_frameheadpos(winstate);
  if (pos < winstate->frameheadpos)
  {
    return 0;
  }

     
                                                                             
                                                                            
                           
     
  if (frameOptions & FRAMEOPTION_END_CURRENT_ROW)
  {
    if (frameOptions & FRAMEOPTION_ROWS)
    {
                                                   
      if (pos > winstate->currentpos)
      {
        return -1;
      }
    }
    else if (frameOptions & (FRAMEOPTION_RANGE | FRAMEOPTION_GROUPS))
    {
                                                          
      if (pos > winstate->currentpos && !are_peers(winstate, slot, winstate->ss.ss_ScanTupleSlot))
      {
        return -1;
      }
    }
    else
    {
      Assert(false);
    }
  }
  else if (frameOptions & FRAMEOPTION_END_OFFSET)
  {
    if (frameOptions & FRAMEOPTION_ROWS)
    {
      int64 offset = DatumGetInt64(winstate->endOffsetValue);

                                                            
      if (frameOptions & FRAMEOPTION_END_OFFSET_PRECEDING)
      {
        offset = -offset;
      }

      if (pos > winstate->currentpos + offset)
      {
        return -1;
      }
    }
    else if (frameOptions & (FRAMEOPTION_RANGE | FRAMEOPTION_GROUPS))
    {
                                                          
      update_frametailpos(winstate);
      if (pos >= winstate->frametailpos)
      {
        return -1;
      }
    }
    else
    {
      Assert(false);
    }
  }

                              
  if (frameOptions & FRAMEOPTION_EXCLUDE_CURRENT_ROW)
  {
    if (pos == winstate->currentpos)
    {
      return 0;
    }
  }
  else if ((frameOptions & FRAMEOPTION_EXCLUDE_GROUP) || ((frameOptions & FRAMEOPTION_EXCLUDE_TIES) && pos != winstate->currentpos))
  {
    WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;

                                                            
    if (node->ordNumCols == 0)
    {
      return 0;
    }
                                               
    if (pos >= winstate->groupheadpos)
    {
      update_grouptailpos(winstate);
      if (pos < winstate->grouptailpos)
      {
        return 0;
      }
    }
  }

                                     
  return 1;
}

   
                       
                                               
   
                                                                              
                                                                             
                                                         
   
                                      
   
static void
update_frameheadpos(WindowAggState *winstate)
{
  WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;
  int frameOptions = winstate->frameOptions;
  MemoryContext oldcontext;

  if (winstate->framehead_valid)
  {
    return;                                    
  }

                                                 
  oldcontext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_query_memory);

  if (frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING)
  {
                                                                 
    winstate->frameheadpos = 0;
    winstate->framehead_valid = true;
  }
  else if (frameOptions & FRAMEOPTION_START_CURRENT_ROW)
  {
    if (frameOptions & FRAMEOPTION_ROWS)
    {
                                                           
      winstate->frameheadpos = winstate->currentpos;
      winstate->framehead_valid = true;
    }
    else if (frameOptions & (FRAMEOPTION_RANGE | FRAMEOPTION_GROUPS))
    {
                                                              
      if (node->ordNumCols == 0)
      {
        winstate->frameheadpos = 0;
        winstate->framehead_valid = true;
        MemoryContextSwitchTo(oldcontext);
        return;
      }

         
                                                                      
                                                                         
                                                                     
                                                                     
                                                              
         
      tuplestore_select_read_pointer(winstate->buffer, winstate->framehead_ptr);
      if (winstate->frameheadpos == 0 && TupIsNull(winstate->framehead_slot))
      {
                                                                       
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->framehead_slot))
        {
          elog(ERROR, "unexpected end of tuplestore");
        }
      }

      while (!TupIsNull(winstate->framehead_slot))
      {
        if (are_peers(winstate, winstate->framehead_slot, winstate->ss.ss_ScanTupleSlot))
        {
          break;                                         
        }
                                                                  
        winstate->frameheadpos++;
        spool_tuples(winstate, winstate->frameheadpos);
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->framehead_slot))
        {
          break;                       
        }
      }
      winstate->framehead_valid = true;
    }
    else
    {
      Assert(false);
    }
  }
  else if (frameOptions & FRAMEOPTION_START_OFFSET)
  {
    if (frameOptions & FRAMEOPTION_ROWS)
    {
                                                                    
      int64 offset = DatumGetInt64(winstate->startOffsetValue);

      if (frameOptions & FRAMEOPTION_START_OFFSET_PRECEDING)
      {
        offset = -offset;
      }

      winstate->frameheadpos = winstate->currentpos + offset;
                                                
      if (winstate->frameheadpos < 0)
      {
        winstate->frameheadpos = 0;
      }
      else if (winstate->frameheadpos > winstate->currentpos + 1)
      {
                                                                 
        spool_tuples(winstate, winstate->frameheadpos - 1);
        if (winstate->frameheadpos > winstate->spooled_rows)
        {
          winstate->frameheadpos = winstate->spooled_rows;
        }
      }
      winstate->framehead_valid = true;
    }
    else if (frameOptions & FRAMEOPTION_RANGE)
    {
         
                                                                      
                                                                        
                                                            
                                                                    
                                                                        
                               
         
      int sortCol = node->ordColIdx[0];
      bool sub, less;

                                           
      Assert(node->ordNumCols == 1);

                                                
      if (frameOptions & FRAMEOPTION_START_OFFSET_PRECEDING)
      {
        sub = true;                                            
      }
      else
      {
        sub = false;             
      }
      less = false;                                          
                                                        
      if (!winstate->inRangeAsc)
      {
        sub = !sub;
        less = true;
      }

      tuplestore_select_read_pointer(winstate->buffer, winstate->framehead_ptr);
      if (winstate->frameheadpos == 0 && TupIsNull(winstate->framehead_slot))
      {
                                                                       
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->framehead_slot))
        {
          elog(ERROR, "unexpected end of tuplestore");
        }
      }

      while (!TupIsNull(winstate->framehead_slot))
      {
        Datum headval, currval;
        bool headisnull, currisnull;

        headval = slot_getattr(winstate->framehead_slot, sortCol, &headisnull);
        currval = slot_getattr(winstate->ss.ss_ScanTupleSlot, sortCol, &currisnull);
        if (headisnull || currisnull)
        {
                                                             
          if (winstate->inRangeNullsFirst)
          {
                                                              
            if (!headisnull || currisnull)
            {
              break;
            }
          }
          else
          {
                                                                   
            if (headisnull || !currisnull)
            {
              break;
            }
          }
        }
        else
        {
          if (DatumGetBool(FunctionCall5Coll(&winstate->startInRangeFunc, winstate->inRangeColl, headval, currval, winstate->startOffsetValue, BoolGetDatum(sub), BoolGetDatum(less))))
          {
            break;                                         
          }
        }
                                                                  
        winstate->frameheadpos++;
        spool_tuples(winstate, winstate->frameheadpos);
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->framehead_slot))
        {
          break;                       
        }
      }
      winstate->framehead_valid = true;
    }
    else if (frameOptions & FRAMEOPTION_GROUPS)
    {
         
                                                                         
                                                                        
                                                            
                                                                    
                                                                        
                               
         
      int64 offset = DatumGetInt64(winstate->startOffsetValue);
      int64 minheadgroup;

      if (frameOptions & FRAMEOPTION_START_OFFSET_PRECEDING)
      {
        minheadgroup = winstate->currentgroup - offset;
      }
      else
      {
        minheadgroup = winstate->currentgroup + offset;
      }

      tuplestore_select_read_pointer(winstate->buffer, winstate->framehead_ptr);
      if (winstate->frameheadpos == 0 && TupIsNull(winstate->framehead_slot))
      {
                                                                       
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->framehead_slot))
        {
          elog(ERROR, "unexpected end of tuplestore");
        }
      }

      while (!TupIsNull(winstate->framehead_slot))
      {
        if (winstate->frameheadgroup >= minheadgroup)
        {
          break;                                         
        }
        ExecCopySlot(winstate->temp_slot_2, winstate->framehead_slot);
                                                                  
        winstate->frameheadpos++;
        spool_tuples(winstate, winstate->frameheadpos);
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->framehead_slot))
        {
          break;                       
        }
        if (!are_peers(winstate, winstate->temp_slot_2, winstate->framehead_slot))
        {
          winstate->frameheadgroup++;
        }
      }
      ExecClearTuple(winstate->temp_slot_2);
      winstate->framehead_valid = true;
    }
    else
    {
      Assert(false);
    }
  }
  else
  {
    Assert(false);
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                       
                                               
   
                                                                              
                                                                             
                                                         
   
                                      
   
static void
update_frametailpos(WindowAggState *winstate)
{
  WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;
  int frameOptions = winstate->frameOptions;
  MemoryContext oldcontext;

  if (winstate->frametail_valid)
  {
    return;                                    
  }

                                                 
  oldcontext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_query_memory);

  if (frameOptions & FRAMEOPTION_END_UNBOUNDED_FOLLOWING)
  {
                                                                      
    spool_tuples(winstate, -1);
    winstate->frametailpos = winstate->spooled_rows;
    winstate->frametail_valid = true;
  }
  else if (frameOptions & FRAMEOPTION_END_CURRENT_ROW)
  {
    if (frameOptions & FRAMEOPTION_ROWS)
    {
                                                                     
      winstate->frametailpos = winstate->currentpos + 1;
      winstate->frametail_valid = true;
    }
    else if (frameOptions & (FRAMEOPTION_RANGE | FRAMEOPTION_GROUPS))
    {
                                                              
      if (node->ordNumCols == 0)
      {
        spool_tuples(winstate, -1);
        winstate->frametailpos = winstate->spooled_rows;
        winstate->frametail_valid = true;
        MemoryContextSwitchTo(oldcontext);
        return;
      }

         
                                                                        
                                                                        
                                                                         
                                                                       
                                                                        
                               
         
      tuplestore_select_read_pointer(winstate->buffer, winstate->frametail_ptr);
      if (winstate->frametailpos == 0 && TupIsNull(winstate->frametail_slot))
      {
                                                                       
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->frametail_slot))
        {
          elog(ERROR, "unexpected end of tuplestore");
        }
      }

      while (!TupIsNull(winstate->frametail_slot))
      {
        if (winstate->frametailpos > winstate->currentpos && !are_peers(winstate, winstate->frametail_slot, winstate->ss.ss_ScanTupleSlot))
        {
          break;                                 
        }
                                                                  
        winstate->frametailpos++;
        spool_tuples(winstate, winstate->frametailpos);
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->frametail_slot))
        {
          break;                       
        }
      }
      winstate->frametail_valid = true;
    }
    else
    {
      Assert(false);
    }
  }
  else if (frameOptions & FRAMEOPTION_END_OFFSET)
  {
    if (frameOptions & FRAMEOPTION_ROWS)
    {
                                                                    
      int64 offset = DatumGetInt64(winstate->endOffsetValue);

      if (frameOptions & FRAMEOPTION_END_OFFSET_PRECEDING)
      {
        offset = -offset;
      }

      winstate->frametailpos = winstate->currentpos + offset + 1;
                                                         
      if (winstate->frametailpos < 0)
      {
        winstate->frametailpos = 0;
      }
      else if (winstate->frametailpos > winstate->currentpos + 1)
      {
                                                                 
        spool_tuples(winstate, winstate->frametailpos - 1);
        if (winstate->frametailpos > winstate->spooled_rows)
        {
          winstate->frametailpos = winstate->spooled_rows;
        }
      }
      winstate->frametail_valid = true;
    }
    else if (frameOptions & FRAMEOPTION_RANGE)
    {
         
                                                                  
                                                                        
                                                                       
                                                                         
                                                                     
                                                              
         
      int sortCol = node->ordColIdx[0];
      bool sub, less;

                                           
      Assert(node->ordNumCols == 1);

                                                
      if (frameOptions & FRAMEOPTION_END_OFFSET_PRECEDING)
      {
        sub = true;                                          
      }
      else
      {
        sub = false;             
      }
      less = true;                                          
                                                        
      if (!winstate->inRangeAsc)
      {
        sub = !sub;
        less = false;
      }

      tuplestore_select_read_pointer(winstate->buffer, winstate->frametail_ptr);
      if (winstate->frametailpos == 0 && TupIsNull(winstate->frametail_slot))
      {
                                                                       
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->frametail_slot))
        {
          elog(ERROR, "unexpected end of tuplestore");
        }
      }

      while (!TupIsNull(winstate->frametail_slot))
      {
        Datum tailval, currval;
        bool tailisnull, currisnull;

        tailval = slot_getattr(winstate->frametail_slot, sortCol, &tailisnull);
        currval = slot_getattr(winstate->ss.ss_ScanTupleSlot, sortCol, &currisnull);
        if (tailisnull || currisnull)
        {
                                                             
          if (winstate->inRangeNullsFirst)
          {
                                                             
            if (!tailisnull)
            {
              break;
            }
          }
          else
          {
                                                                  
            if (!currisnull)
            {
              break;
            }
          }
        }
        else
        {
          if (!DatumGetBool(FunctionCall5Coll(&winstate->endInRangeFunc, winstate->inRangeColl, tailval, currval, winstate->endOffsetValue, BoolGetDatum(sub), BoolGetDatum(less))))
          {
            break;                                         
          }
        }
                                                                  
        winstate->frametailpos++;
        spool_tuples(winstate, winstate->frametailpos);
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->frametail_slot))
        {
          break;                       
        }
      }
      winstate->frametail_valid = true;
    }
    else if (frameOptions & FRAMEOPTION_GROUPS)
    {
         
                                                                     
                                                                       
                                                                        
                                                                         
                                                                        
                                                              
         
      int64 offset = DatumGetInt64(winstate->endOffsetValue);
      int64 maxtailgroup;

      if (frameOptions & FRAMEOPTION_END_OFFSET_PRECEDING)
      {
        maxtailgroup = winstate->currentgroup - offset;
      }
      else
      {
        maxtailgroup = winstate->currentgroup + offset;
      }

      tuplestore_select_read_pointer(winstate->buffer, winstate->frametail_ptr);
      if (winstate->frametailpos == 0 && TupIsNull(winstate->frametail_slot))
      {
                                                                       
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->frametail_slot))
        {
          elog(ERROR, "unexpected end of tuplestore");
        }
      }

      while (!TupIsNull(winstate->frametail_slot))
      {
        if (winstate->frametailgroup > maxtailgroup)
        {
          break;                                         
        }
        ExecCopySlot(winstate->temp_slot_2, winstate->frametail_slot);
                                                                  
        winstate->frametailpos++;
        spool_tuples(winstate, winstate->frametailpos);
        if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->frametail_slot))
        {
          break;                       
        }
        if (!are_peers(winstate, winstate->temp_slot_2, winstate->frametail_slot))
        {
          winstate->frametailgroup++;
        }
      }
      ExecClearTuple(winstate->temp_slot_2);
      winstate->frametail_valid = true;
    }
    else
    {
      Assert(false);
    }
  }
  else
  {
    Assert(false);
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                       
                                               
   
                                      
   
static void
update_grouptailpos(WindowAggState *winstate)
{
  WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;
  MemoryContext oldcontext;

  if (winstate->grouptail_valid)
  {
    return;                                    
  }

                                                 
  oldcontext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_query_memory);

                                                          
  if (node->ordNumCols == 0)
  {
    spool_tuples(winstate, -1);
    winstate->grouptailpos = winstate->spooled_rows;
    winstate->grouptail_valid = true;
    MemoryContextSwitchTo(oldcontext);
    return;
  }

     
                                                                            
                                                                             
                                                                           
                                                                            
                     
     
  Assert(winstate->grouptailpos <= winstate->currentpos);
  tuplestore_select_read_pointer(winstate->buffer, winstate->grouptail_ptr);
  for (;;)
  {
                                                              
    winstate->grouptailpos++;
    spool_tuples(winstate, winstate->grouptailpos);
    if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->temp_slot_2))
    {
      break;                       
    }
    if (winstate->grouptailpos > winstate->currentpos && !are_peers(winstate, winstate->temp_slot_2, winstate->ss.ss_ScanTupleSlot))
    {
      break;                                 
    }
  }
  ExecClearTuple(winstate->temp_slot_2);
  winstate->grouptail_valid = true;

  MemoryContextSwitchTo(oldcontext);
}

                     
                 
   
                                                            
                                                                   
                                                                 
                                                                    
                     
   
static TupleTableSlot *
ExecWindowAgg(PlanState *pstate)
{
  WindowAggState *winstate = castNode(WindowAggState, pstate);
  ExprContext *econtext;
  int i;
  int numfuncs;

  CHECK_FOR_INTERRUPTS();

  if (winstate->all_done)
  {
    return NULL;
  }

     
                                                                        
                                                                          
                                                                            
     
  if (winstate->all_first)
  {
    int frameOptions = winstate->frameOptions;
    ExprContext *econtext = winstate->ss.ps.ps_ExprContext;
    Datum value;
    bool isnull;
    int16 len;
    bool byval;

    if (frameOptions & FRAMEOPTION_START_OFFSET)
    {
      Assert(winstate->startOffset != NULL);
      value = ExecEvalExprSwitchContext(winstate->startOffset, econtext, &isnull);
      if (isnull)
      {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("frame starting offset must not be null")));
      }
                                                  
      get_typlenbyval(exprType((Node *)winstate->startOffset->expr), &len, &byval);
      winstate->startOffsetValue = datumCopy(value, byval, len);
      if (frameOptions & (FRAMEOPTION_ROWS | FRAMEOPTION_GROUPS))
      {
                                       
        int64 offset = DatumGetInt64(value);

        if (offset < 0)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("frame starting offset must not be negative")));
        }
      }
    }
    if (frameOptions & FRAMEOPTION_END_OFFSET)
    {
      Assert(winstate->endOffset != NULL);
      value = ExecEvalExprSwitchContext(winstate->endOffset, econtext, &isnull);
      if (isnull)
      {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("frame ending offset must not be null")));
      }
                                                  
      get_typlenbyval(exprType((Node *)winstate->endOffset->expr), &len, &byval);
      winstate->endOffsetValue = datumCopy(value, byval, len);
      if (frameOptions & (FRAMEOPTION_ROWS | FRAMEOPTION_GROUPS))
      {
                                       
        int64 offset = DatumGetInt64(value);

        if (offset < 0)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("frame ending offset must not be negative")));
        }
      }
    }
    winstate->all_first = false;
  }

  if (winstate->buffer == NULL)
  {
                                                                
    begin_partition(winstate);
                                                                      
  }
  else
  {
                                              
    winstate->currentpos++;
                                                   
    winstate->framehead_valid = false;
    winstate->frametail_valid = false;
                                                               
  }

     
                                                                         
             
     
  spool_tuples(winstate, winstate->currentpos);

                                                                          
  if (winstate->partition_spooled && winstate->currentpos >= winstate->spooled_rows)
  {
    release_partition(winstate);

    if (winstate->more_partitions)
    {
      begin_partition(winstate);
      Assert(winstate->spooled_rows > 0);
    }
    else
    {
      winstate->all_done = true;
      return NULL;
    }
  }

                                                   
  econtext = winstate->ss.ps.ps_ExprContext;

                                                          
  ResetExprContext(econtext);

     
                                                                          
                                                                          
                                                                          
                                                                             
                                            
     
                                                                            
                                                                            
                                                                             
                           
     
                                                                       
     
  tuplestore_select_read_pointer(winstate->buffer, winstate->current_ptr);
  if ((winstate->frameOptions & (FRAMEOPTION_GROUPS | FRAMEOPTION_EXCLUDE_GROUP | FRAMEOPTION_EXCLUDE_TIES)) && winstate->currentpos > 0)
  {
    ExecCopySlot(winstate->temp_slot_2, winstate->ss.ss_ScanTupleSlot);
    if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->ss.ss_ScanTupleSlot))
    {
      elog(ERROR, "unexpected end of tuplestore");
    }
    if (!are_peers(winstate, winstate->temp_slot_2, winstate->ss.ss_ScanTupleSlot))
    {
      winstate->currentgroup++;
      winstate->groupheadpos = winstate->currentpos;
      winstate->grouptail_valid = false;
    }
    ExecClearTuple(winstate->temp_slot_2);
  }
  else
  {
    if (!tuplestore_gettupleslot(winstate->buffer, true, true, winstate->ss.ss_ScanTupleSlot))
    {
      elog(ERROR, "unexpected end of tuplestore");
    }
  }

     
                                    
     
  numfuncs = winstate->numfuncs;
  for (i = 0; i < numfuncs; i++)
  {
    WindowStatePerFunc perfuncstate = &(winstate->perfunc[i]);

    if (perfuncstate->plain_agg)
    {
      continue;
    }
    eval_windowfunction(winstate, perfuncstate, &(econtext->ecxt_aggvalues[perfuncstate->wfuncstate->wfuncno]), &(econtext->ecxt_aggnulls[perfuncstate->wfuncstate->wfuncno]));
  }

     
                         
     
  if (winstate->numaggs > 0)
  {
    eval_windowaggregates(winstate);
  }

     
                                                                       
                                                                         
                                                                         
                                                                          
                                                                           
                                                                         
                                                                           
                                                    
     
  if (winstate->framehead_ptr >= 0)
  {
    update_frameheadpos(winstate);
  }
  if (winstate->frametail_ptr >= 0)
  {
    update_frametailpos(winstate);
  }
  if (winstate->grouptail_ptr >= 0)
  {
    update_grouptailpos(winstate);
  }

     
                                                             
     
  tuplestore_trim(winstate->buffer);

     
                                                                             
                                                                          
                                         
     
  econtext->ecxt_outertuple = winstate->ss.ss_ScanTupleSlot;

  return ExecProject(winstate->ss.ps.ps_ProjInfo);
}

                     
                     
   
                                                                           
                                             
                     
   
WindowAggState *
ExecInitWindowAgg(WindowAgg *node, EState *estate, int eflags)
{
  WindowAggState *winstate;
  Plan *outerPlan;
  ExprContext *econtext;
  ExprContext *tmpcontext;
  WindowStatePerFunc perfunc;
  WindowStatePerAgg peragg;
  int frameOptions = node->frameOptions;
  int numfuncs, wfuncno, numaggs, aggno;
  TupleDesc scanDesc;
  ListCell *l;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  winstate = makeNode(WindowAggState);
  winstate->ss.ps.plan = (Plan *)node;
  winstate->ss.ps.state = estate;
  winstate->ss.ps.ExecProcNode = ExecWindowAgg;

     
                                                                       
                                                                            
                                                     
     
  ExecAssignExprContext(estate, &winstate->ss.ps);
  tmpcontext = winstate->ss.ps.ps_ExprContext;
  winstate->tmpcontext = tmpcontext;
  ExecAssignExprContext(estate, &winstate->ss.ps);

                                                                           
  winstate->partcontext = AllocSetContextCreate(CurrentMemoryContext, "WindowAgg Partition", ALLOCSET_DEFAULT_SIZES);

     
                                                              
     
                                                                         
               
     
  winstate->aggcontext = AllocSetContextCreate(CurrentMemoryContext, "WindowAgg Aggregates", ALLOCSET_DEFAULT_SIZES);

     
                                                                        
                                                                          
     
  Assert(node->plan.qual == NIL);
  winstate->ss.ps.qual = NULL;

     
                            
     
  outerPlan = outerPlan(node);
  outerPlanState(winstate) = ExecInitNode(outerPlan, estate, eflags);

     
                                                                           
                                                                
     
  ExecCreateScanSlotFromOuterPlan(estate, &winstate->ss, &TTSOpsMinimalTuple);
  scanDesc = winstate->ss.ss_ScanTupleSlot->tts_tupleDescriptor;

                                                                           
  winstate->ss.ps.outeropsset = true;
  winstate->ss.ps.outerops = &TTSOpsMinimalTuple;
  winstate->ss.ps.outeropsfixed = true;

     
                                
     
  winstate->first_part_slot = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);
  winstate->agg_row_slot = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);
  winstate->temp_slot_1 = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);
  winstate->temp_slot_2 = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);

     
                                                                           
                                                                             
                
     
  winstate->framehead_slot = winstate->frametail_slot = NULL;

  if (frameOptions & (FRAMEOPTION_RANGE | FRAMEOPTION_GROUPS))
  {
    if (((frameOptions & FRAMEOPTION_START_CURRENT_ROW) && node->ordNumCols != 0) || (frameOptions & FRAMEOPTION_START_OFFSET))
    {
      winstate->framehead_slot = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);
    }
    if (((frameOptions & FRAMEOPTION_END_CURRENT_ROW) && node->ordNumCols != 0) || (frameOptions & FRAMEOPTION_END_OFFSET))
    {
      winstate->frametail_slot = ExecInitExtraTupleSlot(estate, scanDesc, &TTSOpsMinimalTuple);
    }
  }

     
                                                  
     
  ExecInitResultTupleSlotTL(&winstate->ss.ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&winstate->ss.ps, NULL);

                                        
  if (node->partNumCols > 0)
  {
    winstate->partEqfunction = execTuplesMatchPrepare(scanDesc, node->partNumCols, node->partColIdx, node->partOperators, node->partCollations, &winstate->ss.ps);
  }

  if (node->ordNumCols > 0)
  {
    winstate->ordEqfunction = execTuplesMatchPrepare(scanDesc, node->ordNumCols, node->ordColIdx, node->ordOperators, node->ordCollations, &winstate->ss.ps);
  }

     
                                                                      
     
  numfuncs = winstate->numfuncs;
  numaggs = winstate->numaggs;
  econtext = winstate->ss.ps.ps_ExprContext;
  econtext->ecxt_aggvalues = (Datum *)palloc0(sizeof(Datum) * numfuncs);
  econtext->ecxt_aggnulls = (bool *)palloc0(sizeof(bool) * numfuncs);

     
                                                   
     
  perfunc = (WindowStatePerFunc)palloc0(sizeof(WindowStatePerFuncData) * numfuncs);
  peragg = (WindowStatePerAgg)palloc0(sizeof(WindowStatePerAggData) * numaggs);
  winstate->perfunc = perfunc;
  winstate->peragg = peragg;

  wfuncno = -1;
  aggno = -1;
  foreach (l, winstate->funcs)
  {
    WindowFuncExprState *wfuncstate = (WindowFuncExprState *)lfirst(l);
    WindowFunc *wfunc = wfuncstate->wfunc;
    WindowStatePerFunc perfuncstate;
    AclResult aclresult;
    int i;

    if (wfunc->winref != node->winref)                          
    {
      elog(ERROR, "WindowFunc with winref %u assigned to WindowAgg with winref %u", wfunc->winref, node->winref);
    }

                                                       
    for (i = 0; i <= wfuncno; i++)
    {
      if (equal(wfunc, perfunc[i].wfunc) && !contain_volatile_functions((Node *)wfunc))
      {
        break;
      }
    }
    if (i <= wfuncno)
    {
                                                               
      wfuncstate->wfuncno = i;
      continue;
    }

                                             
    perfuncstate = &perfunc[++wfuncno];

                                                                            
    wfuncstate->wfuncno = wfuncno;

                                                  
    aclresult = pg_proc_aclcheck(wfunc->winfnoid, GetUserId(), ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(wfunc->winfnoid));
    }
    InvokeFunctionExecuteHook(wfunc->winfnoid);

                                       
    perfuncstate->wfuncstate = wfuncstate;
    perfuncstate->wfunc = wfunc;
    perfuncstate->numArguments = list_length(wfuncstate->args);

    fmgr_info_cxt(wfunc->winfnoid, &perfuncstate->flinfo, econtext->ecxt_per_query_memory);
    fmgr_info_set_expr((Node *)wfunc, &perfuncstate->flinfo);

    perfuncstate->winCollation = wfunc->inputcollid;

    get_typlenbyval(wfunc->wintype, &perfuncstate->resulttypeLen, &perfuncstate->resulttypeByVal);

       
                                                                         
                               
       
    perfuncstate->plain_agg = wfunc->winagg;
    if (wfunc->winagg)
    {
      WindowStatePerAgg peraggstate;

      perfuncstate->aggno = ++aggno;
      peraggstate = &winstate->peragg[aggno];
      initialize_peragg(winstate, wfunc, peraggstate);
      peraggstate->wfuncno = wfuncno;
    }
    else
    {
      WindowObject winobj = makeNode(WindowObjectData);

      winobj->winstate = winstate;
      winobj->argstates = wfuncstate->args;
      winobj->localmem = NULL;
      perfuncstate->winobj = winobj;
    }
  }

                                                                          
  winstate->numfuncs = wfuncno + 1;
  winstate->numaggs = aggno + 1;

                                                     
  if (winstate->numaggs > 0)
  {
    WindowObject agg_winobj = makeNode(WindowObjectData);

    agg_winobj->winstate = winstate;
    agg_winobj->argstates = NIL;
    agg_winobj->localmem = NULL;
                                                                   
    agg_winobj->markptr = -1;
    agg_winobj->readptr = -1;
    winstate->agg_winobj = agg_winobj;
  }

                                                        
  winstate->frameOptions = frameOptions;

                                                 
  winstate->startOffset = ExecInitExpr((Expr *)node->startOffset, (PlanState *)winstate);
  winstate->endOffset = ExecInitExpr((Expr *)node->endOffset, (PlanState *)winstate);

                                                   
  if (OidIsValid(node->startInRangeFunc))
  {
    fmgr_info(node->startInRangeFunc, &winstate->startInRangeFunc);
  }
  if (OidIsValid(node->endInRangeFunc))
  {
    fmgr_info(node->endInRangeFunc, &winstate->endInRangeFunc);
  }
  winstate->inRangeColl = node->inRangeColl;
  winstate->inRangeAsc = node->inRangeAsc;
  winstate->inRangeNullsFirst = node->inRangeNullsFirst;

  winstate->all_first = true;
  winstate->partition_spooled = false;
  winstate->more_partitions = false;

  return winstate;
}

                     
                    
                     
   
void
ExecEndWindowAgg(WindowAggState *node)
{
  PlanState *outerPlan;
  int i;

  release_partition(node);

  ExecClearTuple(node->ss.ss_ScanTupleSlot);
  ExecClearTuple(node->first_part_slot);
  ExecClearTuple(node->agg_row_slot);
  ExecClearTuple(node->temp_slot_1);
  ExecClearTuple(node->temp_slot_2);
  if (node->framehead_slot)
  {
    ExecClearTuple(node->framehead_slot);
  }
  if (node->frametail_slot)
  {
    ExecClearTuple(node->frametail_slot);
  }

     
                                  
     
  ExecFreeExprContext(&node->ss.ps);
  node->ss.ps.ps_ExprContext = node->tmpcontext;
  ExecFreeExprContext(&node->ss.ps);

  for (i = 0; i < node->numaggs; i++)
  {
    if (node->peragg[i].aggcontext != node->aggcontext)
    {
      MemoryContextDelete(node->peragg[i].aggcontext);
    }
  }
  MemoryContextDelete(node->partcontext);
  MemoryContextDelete(node->aggcontext);

  pfree(node->perfunc);
  pfree(node->peragg);

  outerPlan = outerPlanState(node);
  ExecEndNode(outerPlan);
}

                     
                       
                     
   
void
ExecReScanWindowAgg(WindowAggState *node)
{
  PlanState *outerPlan = outerPlanState(node);
  ExprContext *econtext = node->ss.ps.ps_ExprContext;

  node->all_done = false;
  node->all_first = true;

                                
  release_partition(node);

                                                               
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
  ExecClearTuple(node->first_part_slot);
  ExecClearTuple(node->agg_row_slot);
  ExecClearTuple(node->temp_slot_1);
  ExecClearTuple(node->temp_slot_2);
  if (node->framehead_slot)
  {
    ExecClearTuple(node->framehead_slot);
  }
  if (node->frametail_slot)
  {
    ExecClearTuple(node->frametail_slot);
  }

                                   
  MemSet(econtext->ecxt_aggvalues, 0, sizeof(Datum) * node->numfuncs);
  MemSet(econtext->ecxt_aggnulls, 0, sizeof(bool) * node->numfuncs);

     
                                                                        
                         
     
  if (outerPlan->chgParam == NULL)
  {
    ExecReScan(outerPlan);
  }
}

   
                     
   
                                                                            
   
static WindowStatePerAggData *
initialize_peragg(WindowAggState *winstate, WindowFunc *wfunc, WindowStatePerAgg peraggstate)
{
  Oid inputTypes[FUNC_MAX_ARGS];
  int numArguments;
  HeapTuple aggTuple;
  Form_pg_aggregate aggform;
  Oid aggtranstype;
  AttrNumber initvalAttNo;
  AclResult aclresult;
  bool use_ma_code;
  Oid transfn_oid, invtransfn_oid, finalfn_oid;
  bool finalextra;
  char finalmodify;
  Expr *transfnexpr, *invtransfnexpr, *finalfnexpr;
  Datum textInitVal;
  int i;
  ListCell *lc;

  numArguments = list_length(wfunc->args);

  i = 0;
  foreach (lc, wfunc->args)
  {
    inputTypes[i++] = exprType((Node *)lfirst(lc));
  }

  aggTuple = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(wfunc->winfnoid));
  if (!HeapTupleIsValid(aggTuple))
  {
    elog(ERROR, "cache lookup failed for aggregate %u", wfunc->winfnoid);
  }
  aggform = (Form_pg_aggregate)GETSTRUCT(aggTuple);

     
                                                                            
                                                                      
     
                                                                          
                                                                        
                                                                     
                                                                             
                                                                           
                                                                           
                                                           
     
  if (!OidIsValid(aggform->aggminvtransfn))
  {
    use_ma_code = false;                   
  }
  else if (aggform->aggmfinalmodify == AGGMODIFY_READ_ONLY && aggform->aggfinalmodify != AGGMODIFY_READ_ONLY)
  {
    use_ma_code = true;                                
  }
  else if (winstate->frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING)
  {
    use_ma_code = false;                            
  }
  else if (contain_volatile_functions((Node *)wfunc))
  {
    use_ma_code = false;                                       
  }
  else
  {
    use_ma_code = true;                        
  }
  if (use_ma_code)
  {
    peraggstate->transfn_oid = transfn_oid = aggform->aggmtransfn;
    peraggstate->invtransfn_oid = invtransfn_oid = aggform->aggminvtransfn;
    peraggstate->finalfn_oid = finalfn_oid = aggform->aggmfinalfn;
    finalextra = aggform->aggmfinalextra;
    finalmodify = aggform->aggmfinalmodify;
    aggtranstype = aggform->aggmtranstype;
    initvalAttNo = Anum_pg_aggregate_aggminitval;
  }
  else
  {
    peraggstate->transfn_oid = transfn_oid = aggform->aggtransfn;
    peraggstate->invtransfn_oid = invtransfn_oid = InvalidOid;
    peraggstate->finalfn_oid = finalfn_oid = aggform->aggfinalfn;
    finalextra = aggform->aggfinalextra;
    finalmodify = aggform->aggfinalmodify;
    aggtranstype = aggform->aggtranstype;
    initvalAttNo = Anum_pg_aggregate_agginitval;
  }

     
                                                                             
                                                            
     

                                                                       
  {
    HeapTuple procTuple;
    Oid aggOwner;

    procTuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(wfunc->winfnoid));
    if (!HeapTupleIsValid(procTuple))
    {
      elog(ERROR, "cache lookup failed for function %u", wfunc->winfnoid);
    }
    aggOwner = ((Form_pg_proc)GETSTRUCT(procTuple))->proowner;
    ReleaseSysCache(procTuple);

    aclresult = pg_proc_aclcheck(transfn_oid, aggOwner, ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(transfn_oid));
    }
    InvokeFunctionExecuteHook(transfn_oid);

    if (OidIsValid(invtransfn_oid))
    {
      aclresult = pg_proc_aclcheck(invtransfn_oid, aggOwner, ACL_EXECUTE);
      if (aclresult != ACLCHECK_OK)
      {
        aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(invtransfn_oid));
      }
      InvokeFunctionExecuteHook(invtransfn_oid);
    }

    if (OidIsValid(finalfn_oid))
    {
      aclresult = pg_proc_aclcheck(finalfn_oid, aggOwner, ACL_EXECUTE);
      if (aclresult != ACLCHECK_OK)
      {
        aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(finalfn_oid));
      }
      InvokeFunctionExecuteHook(finalfn_oid);
    }
  }

     
                                                                             
                                                                            
                                                                  
     
  if (finalmodify != AGGMODIFY_READ_ONLY)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("aggregate function %s does not support use as a window function", format_procedure(wfunc->winfnoid))));
  }

                                                        
  if (finalextra)
  {
    peraggstate->numFinalArgs = numArguments + 1;
  }
  else
  {
    peraggstate->numFinalArgs = 1;
  }

                                                               
  aggtranstype = resolve_aggregate_transtype(wfunc->winfnoid, aggtranstype, inputTypes, numArguments);

                                                                   
  build_aggregate_transfn_expr(inputTypes, numArguments, 0,                                          
      false,                                                                                      
      aggtranstype, wfunc->inputcollid, transfn_oid, invtransfn_oid, &transfnexpr, &invtransfnexpr);

                                                                    
  fmgr_info(transfn_oid, &peraggstate->transfn);
  fmgr_info_set_expr((Node *)transfnexpr, &peraggstate->transfn);

  if (OidIsValid(invtransfn_oid))
  {
    fmgr_info(invtransfn_oid, &peraggstate->invtransfn);
    fmgr_info_set_expr((Node *)invtransfnexpr, &peraggstate->invtransfn);
  }

  if (OidIsValid(finalfn_oid))
  {
    build_aggregate_finalfn_expr(inputTypes, peraggstate->numFinalArgs, aggtranstype, wfunc->wintype, wfunc->inputcollid, finalfn_oid, &finalfnexpr);
    fmgr_info(finalfn_oid, &peraggstate->finalfn);
    fmgr_info_set_expr((Node *)finalfnexpr, &peraggstate->finalfn);
  }

                                         
  get_typlenbyval(wfunc->wintype, &peraggstate->resulttypeLen, &peraggstate->resulttypeByVal);
  get_typlenbyval(aggtranstype, &peraggstate->transtypeLen, &peraggstate->transtypeByVal);

     
                                                                        
                                                          
     
  textInitVal = SysCacheGetAttr(AGGFNOID, aggTuple, initvalAttNo, &peraggstate->initValueIsNull);

  if (peraggstate->initValueIsNull)
  {
    peraggstate->initValue = (Datum)0;
  }
  else
  {
    peraggstate->initValue = GetAggInitVal(textInitVal, aggtranstype);
  }

     
                                                                            
                                                                         
                                                                           
                                                                        
                                                                       
     
  if (peraggstate->transfn.fn_strict && peraggstate->initValueIsNull)
  {
    if (numArguments < 1 || !IsBinaryCoercible(inputTypes[0], aggtranstype))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate %u needs to have compatible input type and transition type", wfunc->winfnoid)));
    }
  }

     
                                                                        
                                                                         
                                                       
                                                                            
                                                                          
                                                                  
     
  if (OidIsValid(invtransfn_oid) && peraggstate->transfn.fn_strict != peraggstate->invtransfn.fn_strict)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("strictness of aggregate's forward and inverse transition functions must match")));
  }

     
                                                 
     
                                                                            
                                                                          
                                                                          
                                                                          
                                                                           
                                                                         
                                                                           
                                                                           
                                                                             
                                         
     
  if (OidIsValid(invtransfn_oid))
  {
    peraggstate->aggcontext = AllocSetContextCreate(CurrentMemoryContext, "WindowAgg Per Aggregate", ALLOCSET_DEFAULT_SIZES);
  }
  else
  {
    peraggstate->aggcontext = winstate->aggcontext;
  }

  ReleaseSysCache(aggTuple);

  return peraggstate;
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

   
             
                                                                              
   
                                                     
   
static bool
are_peers(WindowAggState *winstate, TupleTableSlot *slot1, TupleTableSlot *slot2)
{
  WindowAgg *node = (WindowAgg *)winstate->ss.ps.plan;
  ExprContext *econtext = winstate->tmpcontext;

                                                          
  if (node->ordNumCols == 0)
  {
    return true;
  }

  econtext->ecxt_outertuple = slot1;
  econtext->ecxt_innertuple = slot2;
  return ExecQualAndReset(winstate->ordEqfunction, econtext);
}

   
                       
                                                                  
                                   
   
                                                    
   
static bool
window_gettupleslot(WindowObject winobj, int64 pos, TupleTableSlot *slot)
{
  WindowAggState *winstate = winobj->winstate;
  MemoryContext oldcontext;

                                        
  CHECK_FOR_INTERRUPTS();

                                                   
  if (pos < 0)
  {
    return false;
  }

                                                    
  spool_tuples(winstate, pos);

  if (pos >= winstate->spooled_rows)
  {
    return false;
  }

  if (pos < winobj->markpos)
  {
    elog(ERROR, "cannot fetch row before WindowObject's mark position");
  }

  oldcontext = MemoryContextSwitchTo(winstate->ss.ps.ps_ExprContext->ecxt_per_query_memory);

  tuplestore_select_read_pointer(winstate->buffer, winobj->readptr);

     
                                                                         
     
  if (winobj->seekpos < pos - 1)
  {
    if (!tuplestore_skiptuples(winstate->buffer, pos - 1 - winobj->seekpos, true))
    {
      elog(ERROR, "unexpected end of tuplestore");
    }
    winobj->seekpos = pos - 1;
  }
  else if (winobj->seekpos > pos + 1)
  {
    if (!tuplestore_skiptuples(winstate->buffer, winobj->seekpos - (pos + 1), false))
    {
      elog(ERROR, "unexpected end of tuplestore");
    }
    winobj->seekpos = pos + 1;
  }
  else if (winobj->seekpos == pos)
  {
       
                                                                        
                                                                         
                                                                        
                                                                        
                  
       
    tuplestore_advance(winstate->buffer, true);
    winobj->seekpos++;
  }

     
                                                                          
                                                               
     
  if (winobj->seekpos > pos)
  {
    if (!tuplestore_gettupleslot(winstate->buffer, false, true, slot))
    {
      elog(ERROR, "unexpected end of tuplestore");
    }
    winobj->seekpos--;
  }
  else
  {
    if (!tuplestore_gettupleslot(winstate->buffer, true, true, slot))
    {
      elog(ERROR, "unexpected end of tuplestore");
    }
    winobj->seekpos++;
  }

  Assert(winobj->seekpos == pos);

  MemoryContextSwitchTo(oldcontext);

  return true;
}

                                                                         
                                   
                                                                         

   
                              
                                                                   
   
                                                                         
                                                                            
   
                                                                          
                                                                           
                                                                             
                       
   
void *
WinGetPartitionLocalMemory(WindowObject winobj, Size sz)
{
  Assert(WindowObjectIsValid(winobj));
  if (winobj->localmem == NULL)
  {
    winobj->localmem = MemoryContextAllocZero(winobj->winstate->partcontext, sz);
  }
  return winobj->localmem;
}

   
                         
                                                                           
               
   
int64
WinGetCurrentPosition(WindowObject winobj)
{
  Assert(WindowObjectIsValid(winobj));
  return winobj->winstate->currentpos;
}

   
                           
                                                                    
   
                                                                        
                                                                      
                                                                            
   
int64
WinGetPartitionRowCount(WindowObject winobj)
{
  Assert(WindowObjectIsValid(winobj));
  spool_tuples(winobj->winstate, -1);
  return winobj->winstate->spooled_rows;
}

   
                      
                                                                           
                                                                          
                                             
   
                                                                             
                                                                           
                                 
   
void
WinSetMarkPosition(WindowObject winobj, int64 markpos)
{
  WindowAggState *winstate;

  Assert(WindowObjectIsValid(winobj));
  winstate = winobj->winstate;

  if (markpos < winobj->markpos)
  {
    elog(ERROR, "cannot move WindowObject's mark position backward");
  }
  tuplestore_select_read_pointer(winstate->buffer, winobj->markptr);
  if (markpos > winobj->markpos)
  {
    tuplestore_skiptuples(winstate->buffer, markpos - winobj->markpos, true);
    winobj->markpos = markpos;
  }
  tuplestore_select_read_pointer(winstate->buffer, winobj->readptr);
  if (markpos > winobj->seekpos)
  {
    tuplestore_skiptuples(winstate->buffer, markpos - winobj->seekpos, true);
    winobj->seekpos = markpos;
  }
}

   
                   
                                                                          
                                                        
   
                                                     
   
bool
WinRowsArePeers(WindowObject winobj, int64 pos1, int64 pos2)
{
  WindowAggState *winstate;
  WindowAgg *node;
  TupleTableSlot *slot1;
  TupleTableSlot *slot2;
  bool res;

  Assert(WindowObjectIsValid(winobj));
  winstate = winobj->winstate;
  node = (WindowAgg *)winstate->ss.ps.plan;

                                                                      
  if (node->ordNumCols == 0)
  {
    return true;
  }

     
                                                                    
                                                                  
     
  slot1 = winstate->temp_slot_1;
  slot2 = winstate->temp_slot_2;

  if (!window_gettupleslot(winobj, pos1, slot1))
  {
    elog(ERROR, "specified position is out of window: " INT64_FORMAT, pos1);
  }
  if (!window_gettupleslot(winobj, pos2, slot2))
  {
    elog(ERROR, "specified position is out of window: " INT64_FORMAT, pos2);
  }

  res = are_peers(winstate, slot1, slot2);

  ExecClearTuple(slot1);
  ExecClearTuple(slot2);

  return res;
}

   
                            
                                                                    
                                                                    
                                                      
   
                                                       
                                                         
                                                                        
                                                                            
                              
                                                             
                                                                       
                                                                          
   
                                                                              
                                                    
   
Datum
WinGetFuncArgInPartition(WindowObject winobj, int argno, int relpos, int seektype, bool set_mark, bool *isnull, bool *isout)
{
  WindowAggState *winstate;
  ExprContext *econtext;
  TupleTableSlot *slot;
  bool gottuple;
  int64 abs_pos;

  Assert(WindowObjectIsValid(winobj));
  winstate = winobj->winstate;
  econtext = winstate->ss.ps.ps_ExprContext;
  slot = winstate->temp_slot_1;

  switch (seektype)
  {
  case WINDOW_SEEK_CURRENT:
    abs_pos = winstate->currentpos + relpos;
    break;
  case WINDOW_SEEK_HEAD:
    abs_pos = relpos;
    break;
  case WINDOW_SEEK_TAIL:
    spool_tuples(winstate, -1);
    abs_pos = winstate->spooled_rows - 1 + relpos;
    break;
  default:
    elog(ERROR, "unrecognized window seek type: %d", seektype);
    abs_pos = 0;                          
    break;
  }

  gottuple = window_gettupleslot(winobj, abs_pos, slot);

  if (!gottuple)
  {
    if (isout)
    {
      *isout = true;
    }
    *isnull = true;
    return (Datum)0;
  }
  else
  {
    if (isout)
    {
      *isout = false;
    }
    if (set_mark)
    {
      WinSetMarkPosition(winobj, abs_pos);
    }
    econtext->ecxt_outertuple = slot;
    return ExecEvalExpr((ExprState *)list_nth(winobj->argstates, argno), econtext, isnull);
  }
}

   
                        
                                                                    
                                                                       
                                                                     
                                                                       
                                                                  
   
                                                       
                                                         
                                                  
                                                                            
                                       
                                                             
                                                                       
                                                                      
   
                                                                         
                                                                         
   
                                                                        
                                                                        
                                                   
   
                                                                            
                                                                           
                                                                          
                                                                           
                                                                        
                                                                            
                                                                          
                 
   
Datum
WinGetFuncArgInFrame(WindowObject winobj, int argno, int relpos, int seektype, bool set_mark, bool *isnull, bool *isout)
{
  WindowAggState *winstate;
  ExprContext *econtext;
  TupleTableSlot *slot;
  int64 abs_pos;
  int64 mark_pos;

  Assert(WindowObjectIsValid(winobj));
  winstate = winobj->winstate;
  econtext = winstate->ss.ps.ps_ExprContext;
  slot = winstate->temp_slot_1;

  switch (seektype)
  {
  case WINDOW_SEEK_CURRENT:
    elog(ERROR, "WINDOW_SEEK_CURRENT is not supported for WinGetFuncArgInFrame");
    abs_pos = mark_pos = 0;                          
    break;
  case WINDOW_SEEK_HEAD:
                                                                
    if (relpos < 0)
    {
      goto out_of_frame;
    }
    update_frameheadpos(winstate);
    abs_pos = winstate->frameheadpos + relpos;
    mark_pos = abs_pos;

       
                                                                       
                                                                   
                                                                       
                                    
       
                                                                
                                                                       
                                                                      
                                                                    
                                                             
       
    switch (winstate->frameOptions & FRAMEOPTION_EXCLUSION)
    {
    case 0:
                                
      break;
    case FRAMEOPTION_EXCLUDE_CURRENT_ROW:
      if (abs_pos >= winstate->currentpos && winstate->currentpos >= winstate->frameheadpos)
      {
        abs_pos++;
      }
      break;
    case FRAMEOPTION_EXCLUDE_GROUP:
      update_grouptailpos(winstate);
      if (abs_pos >= winstate->groupheadpos && winstate->grouptailpos > winstate->frameheadpos)
      {
        int64 overlapstart = Max(winstate->groupheadpos, winstate->frameheadpos);

        abs_pos += winstate->grouptailpos - overlapstart;
      }
      break;
    case FRAMEOPTION_EXCLUDE_TIES:
      update_grouptailpos(winstate);
      if (abs_pos >= winstate->groupheadpos && winstate->grouptailpos > winstate->frameheadpos)
      {
        int64 overlapstart = Max(winstate->groupheadpos, winstate->frameheadpos);

        if (abs_pos == overlapstart)
        {
          abs_pos = winstate->currentpos;
        }
        else
        {
          abs_pos += winstate->grouptailpos - overlapstart - 1;
        }
      }
      break;
    default:
      elog(ERROR, "unrecognized frame option state: 0x%x", winstate->frameOptions);
      break;
    }
    break;
  case WINDOW_SEEK_TAIL:
                                                                
    if (relpos > 0)
    {
      goto out_of_frame;
    }
    update_frametailpos(winstate);
    abs_pos = winstate->frametailpos - 1 + relpos;

       
                                                                      
                                                                       
                                                                       
                                                                      
                                                                   
                                                                      
                                                                  
       
    switch (winstate->frameOptions & FRAMEOPTION_EXCLUSION)
    {
    case 0:
                                
      mark_pos = abs_pos;
      break;
    case FRAMEOPTION_EXCLUDE_CURRENT_ROW:
      if (abs_pos <= winstate->currentpos && winstate->currentpos < winstate->frametailpos)
      {
        abs_pos--;
      }
      update_frameheadpos(winstate);
      if (abs_pos < winstate->frameheadpos)
      {
        goto out_of_frame;
      }
      mark_pos = winstate->frameheadpos;
      break;
    case FRAMEOPTION_EXCLUDE_GROUP:
      update_grouptailpos(winstate);
      if (abs_pos < winstate->grouptailpos && winstate->groupheadpos < winstate->frametailpos)
      {
        int64 overlapend = Min(winstate->grouptailpos, winstate->frametailpos);

        abs_pos -= overlapend - winstate->groupheadpos;
      }
      update_frameheadpos(winstate);
      if (abs_pos < winstate->frameheadpos)
      {
        goto out_of_frame;
      }
      mark_pos = winstate->frameheadpos;
      break;
    case FRAMEOPTION_EXCLUDE_TIES:
      update_grouptailpos(winstate);
      if (abs_pos < winstate->grouptailpos && winstate->groupheadpos < winstate->frametailpos)
      {
        int64 overlapend = Min(winstate->grouptailpos, winstate->frametailpos);

        if (abs_pos == overlapend - 1)
        {
          abs_pos = winstate->currentpos;
        }
        else
        {
          abs_pos -= overlapend - 1 - winstate->groupheadpos;
        }
      }
      update_frameheadpos(winstate);
      if (abs_pos < winstate->frameheadpos)
      {
        goto out_of_frame;
      }
      mark_pos = winstate->frameheadpos;
      break;
    default:
      elog(ERROR, "unrecognized frame option state: 0x%x", winstate->frameOptions);
      mark_pos = 0;                          
      break;
    }
    break;
  default:
    elog(ERROR, "unrecognized window seek type: %d", seektype);
    abs_pos = mark_pos = 0;                          
    break;
  }

  if (!window_gettupleslot(winobj, abs_pos, slot))
  {
    goto out_of_frame;
  }

                                                                       
  if (row_is_in_frame(winstate, abs_pos, slot) <= 0)
  {
    goto out_of_frame;
  }

  if (isout)
  {
    *isout = false;
  }
  if (set_mark)
  {
    WinSetMarkPosition(winobj, mark_pos);
  }
  econtext->ecxt_outertuple = slot;
  return ExecEvalExpr((ExprState *)list_nth(winobj->argstates, argno), econtext, isnull);

out_of_frame:
  if (isout)
  {
    *isout = true;
  }
  *isnull = true;
  return (Datum)0;
}

   
                        
                                                                         
   
                                                       
                                                             
   
                                                                    
                                                                           
                                                                        
                                                                      
                                                             
   
Datum
WinGetFuncArgCurrent(WindowObject winobj, int argno, bool *isnull)
{
  WindowAggState *winstate;
  ExprContext *econtext;

  Assert(WindowObjectIsValid(winobj));
  winstate = winobj->winstate;

  econtext = winstate->ss.ps.ps_ExprContext;

  econtext->ecxt_outertuple = winstate->ss.ss_ScanTupleSlot;
  return ExecEvalExpr((ExprState *)list_nth(winobj->argstates, argno), econtext, isnull);
}
