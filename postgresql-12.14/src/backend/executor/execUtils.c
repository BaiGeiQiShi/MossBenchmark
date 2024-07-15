                                                                            
   
               
                                             
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
   
                      
                                                              
                      
                      
                                
                    
                      
   
                                                                   
        
   
                                                                  
   
                                                                    
   
                                                                      
   
                                                                
   
                                                                       
                                                                         
   
                                                                   
                      
   
          
                                                              
                                                                 
   

#include "postgres.h"

#include "access/parallel.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "executor/executor.h"
#include "executor/execPartition.h"
#include "jit/jit.h"
#include "mb/pg_wchar.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/typcache.h"

static bool
tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc);
static void
ShutdownExprContext(ExprContext *econtext, bool isCommit);

                                                                    
                                                      
                                                                    
   

                    
                        
   
                                                               
                                                       
   
                                                                       
                                                                       
                                                                       
                         
                    
   
EState *
CreateExecutorState(void)
{
  EState *estate;
  MemoryContext qcontext;
  MemoryContext oldcontext;

     
                                                         
     
  qcontext = AllocSetContextCreate(CurrentMemoryContext, "ExecutorState", ALLOCSET_DEFAULT_SIZES);

     
                                                                            
                                                           
     
  oldcontext = MemoryContextSwitchTo(qcontext);

  estate = makeNode(EState);

     
                                                           
     
  estate->es_direction = ForwardScanDirection;
  estate->es_snapshot = InvalidSnapshot;                                             
  estate->es_crosscheck_snapshot = InvalidSnapshot;                    
  estate->es_range_table = NIL;
  estate->es_range_table_array = NULL;
  estate->es_range_table_size = 0;
  estate->es_relations = NULL;
  estate->es_rowmarks = NULL;
  estate->es_plannedstmt = NULL;

  estate->es_junkFilter = NULL;

  estate->es_output_cid = (CommandId)0;

  estate->es_result_relations = NULL;
  estate->es_num_result_relations = 0;
  estate->es_result_relation_info = NULL;

  estate->es_root_result_relations = NULL;
  estate->es_num_root_result_relations = 0;

  estate->es_tuple_routing_result_relations = NIL;

  estate->es_trig_target_relations = NIL;

  estate->es_param_list_info = NULL;
  estate->es_param_exec_vals = NULL;

  estate->es_queryEnv = NULL;

  estate->es_query_cxt = qcontext;

  estate->es_tupleTable = NIL;

  estate->es_processed = 0;

  estate->es_top_eflags = 0;
  estate->es_instrument = 0;
  estate->es_finished = false;

  estate->es_exprcontexts = NIL;

  estate->es_subplanstates = NIL;

  estate->es_auxmodifytables = NIL;

  estate->es_per_tuple_exprcontext = NULL;

  estate->es_sourceText = NULL;

  estate->es_use_parallel_mode = false;

  estate->es_jit_flags = 0;
  estate->es_jit = NULL;

     
                                         
     
  MemoryContextSwitchTo(oldcontext);

  return estate;
}

                    
                      
   
                                                                
   
                                                                             
                                                                          
                                                                               
                                                                            
                                                                   
   
                                                                        
                            
                    
   
void
FreeExecutorState(EState *estate)
{
     
                                                                           
                                                                            
                                                                          
                                
     
  while (estate->es_exprcontexts)
  {
       
                                                                        
                                   
       
    FreeExprContext((ExprContext *)linitial(estate->es_exprcontexts), true);
                                                      
  }

                                         
  if (estate->es_jit)
  {
    jit_release_context(estate->es_jit);
    estate->es_jit = NULL;
  }

                                                 
  if (estate->es_partition_directory)
  {
    DestroyPartitionDirectory(estate->es_partition_directory);
    estate->es_partition_directory = NULL;
  }

     
                                                                      
                                               
     
  MemoryContextDelete(estate->es_query_cxt);
}

                    
                      
   
                                                                 
   
                                                                          
                                                                          
                                                                           
                   
   
                                                                 
                    
   
ExprContext *
CreateExprContext(EState *estate)
{
  ExprContext *econtext;
  MemoryContext oldcontext;

                                                                       
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  econtext = makeNode(ExprContext);

                                        
  econtext->ecxt_scantuple = NULL;
  econtext->ecxt_innertuple = NULL;
  econtext->ecxt_outertuple = NULL;

  econtext->ecxt_per_query_memory = estate->es_query_cxt;

     
                                                                      
     
  econtext->ecxt_per_tuple_memory = AllocSetContextCreate(estate->es_query_cxt, "ExprContext", ALLOCSET_DEFAULT_SIZES);

  econtext->ecxt_param_exec_vals = estate->es_param_exec_vals;
  econtext->ecxt_param_list_info = estate->es_param_list_info;

  econtext->ecxt_aggvalues = NULL;
  econtext->ecxt_aggnulls = NULL;

  econtext->caseValue_datum = (Datum)0;
  econtext->caseValue_isNull = true;

  econtext->domainValue_datum = (Datum)0;
  econtext->domainValue_isNull = true;

  econtext->ecxt_estate = estate;

  econtext->ecxt_callbacks = NULL;

     
                                                                             
                                                                       
                                                                           
     
  estate->es_exprcontexts = lcons(econtext, estate->es_exprcontexts);

  MemoryContextSwitchTo(oldcontext);

  return econtext;
}

                    
                                
   
                                                           
   
                                                                          
                                                                         
                                                                        
   
                                                                      
                                                        
   
                                                                    
                                                                   
                                                                       
                    
                    
   
ExprContext *
CreateStandaloneExprContext(void)
{
  ExprContext *econtext;

                                                                      
  econtext = makeNode(ExprContext);

                                        
  econtext->ecxt_scantuple = NULL;
  econtext->ecxt_innertuple = NULL;
  econtext->ecxt_outertuple = NULL;

  econtext->ecxt_per_query_memory = CurrentMemoryContext;

     
                                                                      
     
  econtext->ecxt_per_tuple_memory = AllocSetContextCreate(CurrentMemoryContext, "ExprContext", ALLOCSET_DEFAULT_SIZES);

  econtext->ecxt_param_exec_vals = NULL;
  econtext->ecxt_param_list_info = NULL;

  econtext->ecxt_aggvalues = NULL;
  econtext->ecxt_aggnulls = NULL;

  econtext->caseValue_datum = (Datum)0;
  econtext->caseValue_isNull = true;

  econtext->domainValue_datum = (Datum)0;
  econtext->domainValue_isNull = true;

  econtext->ecxt_estate = NULL;

  econtext->ecxt_callbacks = NULL;

  return econtext;
}

                    
                    
   
                                                                
                        
   
                                                                       
                                                                             
   
                                                                          
                                                                            
                                                                            
                                                               
   
                                                                 
                    
   
void
FreeExprContext(ExprContext *econtext, bool isCommit)
{
  EState *estate;

                                     
  ShutdownExprContext(econtext, isCommit);
                                    
  MemoryContextDelete(econtext->ecxt_per_tuple_memory);
                                              
  estate = econtext->ecxt_estate;
  if (estate)
  {
    estate->es_exprcontexts = list_delete_ptr(estate->es_exprcontexts, econtext);
  }
                                       
  pfree(econtext);
}

   
                     
   
                                                                   
                                                                         
                                                                           
   
                                                                 
   
void
ReScanExprContext(ExprContext *econtext)
{
                                     
  ShutdownExprContext(econtext, true);
                                    
  MemoryContextReset(econtext->ecxt_per_tuple_memory);
}

   
                                                       
   
                                                                
                 
   
ExprContext *
MakePerTupleExprContext(EState *estate)
{
  if (estate->es_per_tuple_exprcontext == NULL)
  {
    estate->es_per_tuple_exprcontext = CreateExprContext(estate);
  }

  return estate->es_per_tuple_exprcontext;
}

                                                                    
                                                 
   
                                                                          
                                          
                                                                    
   

                    
                          
   
                                                                     
                                                           
                                                                 
                                                              
                    
   
void
ExecAssignExprContext(EState *estate, PlanState *planstate)
{
  planstate->ps_ExprContext = CreateExprContext(estate);
}

                    
                      
                    
   
TupleDesc
ExecGetResultType(PlanState *planstate)
{
  return planstate->ps_ResultTupleDesc;
}

   
                                                                       
   
const TupleTableSlotOps *
ExecGetResultSlotOps(PlanState *planstate, bool *isfixed)
{
  if (planstate->resultopsset && planstate->resultops)
  {
    if (isfixed)
    {
      *isfixed = planstate->resultopsfixed;
    }
    return planstate->resultops;
  }

  if (isfixed)
  {
    if (planstate->resultopsset)
    {
      *isfixed = planstate->resultopsfixed;
    }
    else if (planstate->ps_ResultTupleSlot)
    {
      *isfixed = TTS_FIXED(planstate->ps_ResultTupleSlot);
    }
    else
    {
      *isfixed = false;
    }
  }

  if (!planstate->ps_ResultTupleSlot)
  {
    return &TTSOpsVirtual;
  }

  return planstate->ps_ResultTupleSlot->tts_ops;
}

                    
                             
   
                                                               
   
                                                                          
                                                                 
                    
   
void
ExecAssignProjectionInfo(PlanState *planstate, TupleDesc inputDesc)
{
  planstate->ps_ProjInfo = ExecBuildProjectionInfo(planstate->plan->targetlist, planstate->ps_ExprContext, planstate->ps_ResultTupleSlot, planstate, inputDesc);
}

                    
                                        
   
                                                                               
                                     
                    
   
void
ExecConditionalAssignProjectionInfo(PlanState *planstate, TupleDesc inputDesc, Index varno)
{
  if (tlist_matches_tupdesc(planstate, planstate->plan->targetlist, varno, inputDesc))
  {
    planstate->ps_ProjInfo = NULL;
    planstate->resultopsset = planstate->scanopsset;
    planstate->resultopsfixed = planstate->scanopsfixed;
    planstate->resultops = planstate->scanops;
  }
  else
  {
    if (!planstate->ps_ResultTupleSlot)
    {
      ExecInitResultSlot(planstate, &TTSOpsVirtual);
      planstate->resultops = &TTSOpsVirtual;
      planstate->resultopsfixed = true;
      planstate->resultopsset = true;
    }
    ExecAssignProjectionInfo(planstate, inputDesc);
  }
}

static bool
tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc)
{
  int numattrs = tupdesc->natts;
  int attrno;
  ListCell *tlist_item = list_head(tlist);

                                  
  for (attrno = 1; attrno <= numattrs; attrno++)
  {
    Form_pg_attribute att_tup = TupleDescAttr(tupdesc, attrno - 1);
    Var *var;

    if (tlist_item == NULL)
    {
      return false;                      
    }
    var = (Var *)((TargetEntry *)lfirst(tlist_item))->expr;
    if (!var || !IsA(var, Var))
    {
      return false;                           
    }
                                                  
    Assert(var->varno == varno);
    Assert(var->varlevelsup == 0);
    if (var->varattno != attrno)
    {
      return false;                   
    }
    if (att_tup->attisdropped)
    {
      return false;                                     
    }
    if (att_tup->atthasmissing)
    {
      return false;                                              
    }

       
                                                                          
                                                                     
                                                                          
                                                                      
                                                                         
                                                                    
                                                                           
                              
       
    if (var->vartype != att_tup->atttypid || (var->vartypmod != att_tup->atttypmod && var->vartypmod != -1))
    {
      return false;                    
    }

    tlist_item = lnext(tlist_item);
  }

  if (tlist_item)
  {
    return false;                     
  }

  return true;
}

                    
                        
   
                                                                        
                                                                               
                                                                               
                                                                            
   
                                                                        
                                                                         
                                                                           
                                                                           
                                                                           
                                   
                    
   
void
ExecFreeExprContext(PlanState *planstate)
{
     
                                                                        
                                           
     
  planstate->ps_ExprContext = NULL;
}

                                                                    
                          
                                                                    
   

                    
                       
                    
   
void
ExecAssignScanType(ScanState *scanstate, TupleDesc tupDesc)
{
  TupleTableSlot *slot = scanstate->ss_ScanTupleSlot;

  ExecSetSlotDescriptor(slot, tupDesc);
}

                    
                                    
                    
   
void
ExecCreateScanSlotFromOuterPlan(EState *estate, ScanState *scanstate, const TupleTableSlotOps *tts_ops)
{
  PlanState *outerPlan;
  TupleDesc tupDesc;

  outerPlan = outerPlanState(scanstate);
  tupDesc = ExecGetResultType(outerPlan);

  ExecInitScanTupleSlot(estate, scanstate, tupDesc, tts_ops);
}

                                                                    
                                 
   
                                                               
                                                 
   
                                                                      
                                                                       
                         
                                                                    
   
bool
ExecRelationIsTargetRelation(EState *estate, Index scanrelid)
{
  ResultRelInfo *resultRelInfos;
  int i;

  resultRelInfos = estate->es_result_relations;
  for (i = 0; i < estate->es_num_result_relations; i++)
  {
    if (resultRelInfos[i].ri_RangeTableIndex == scanrelid)
    {
      return true;
    }
  }
  return false;
}

                                                                    
                         
   
                                                                         
                                                              
                                                                    
   
Relation
ExecOpenScanRelation(EState *estate, Index scanrelid, int eflags)
{
  Relation rel;

                          
  rel = ExecGetRangeTableRelation(estate, scanrelid);

     
                                                                            
                                                                           
                                                        
     
  if ((eflags & (EXEC_FLAG_EXPLAIN_ONLY | EXEC_FLAG_WITH_NO_DATA)) == 0 && !RelationIsScannable(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("materialized view \"%s\" has not been populated", RelationGetRelationName(rel)), errhint("Use the REFRESH MATERIALIZED VIEW command.")));
  }

  return rel;
}

   
                      
                                               
   
                                                                              
                                                                          
                                                
                                                                              
   
void
ExecInitRangeTable(EState *estate, List *rangeTable)
{
  Index rti;
  ListCell *lc;

                                           
  estate->es_range_table = rangeTable;

                                                  
  estate->es_range_table_size = list_length(rangeTable);
  estate->es_range_table_array = (RangeTblEntry **)palloc(estate->es_range_table_size * sizeof(RangeTblEntry *));
  rti = 0;
  foreach (lc, rangeTable)
  {
    estate->es_range_table_array[rti++] = lfirst_node(RangeTblEntry, lc);
  }

     
                                                                       
                                                                             
                                
     
  estate->es_relations = (Relation *)palloc0(estate->es_range_table_size * sizeof(Relation));

     
                                                                        
                               
     
  estate->es_rowmarks = NULL;
}

   
                             
                                                                   
   
                                                        
   
Relation
ExecGetRangeTableRelation(EState *estate, Index rti)
{
  Relation rel;

  Assert(rti > 0 && rti <= estate->es_range_table_size);

  rel = estate->es_relations[rti - 1];
  if (rel == NULL)
  {
                                                  
    RangeTblEntry *rte = exec_rt_fetch(rti, estate);

    Assert(rte->rtekind == RTE_RELATION);

    if (!IsParallelWorker())
    {
         
                                                                         
                                                                      
                                                                        
                                                                        
                           
         
      rel = table_open(rte->relid, NoLock);
      Assert(rte->rellockmode == AccessShareLock || CheckRelationLockedByMe(rel, rte->rellockmode, false));
    }
    else
    {
         
                                                                      
                                                                       
                                            
         
      rel = table_open(rte->relid, rte->rellockmode);
    }

    estate->es_relations[rti - 1] = rel;
  }

  return rel;
}

   
                         
                                                         
   
void
UpdateChangedParamSet(PlanState *node, Bitmapset *newchg)
{
  Bitmapset *parmset;

     
                                                                            
                                                  
     
  parmset = bms_intersect(node->plan->allParam, newchg);

     
                                                                           
                                                                
     
  if (!bms_is_empty(parmset))
  {
    node->chgParam = bms_join(node->chgParam, parmset);
  }
  else
  {
    bms_free(parmset);
  }
}

   
                        
                                                           
   
                                                                           
                                   
   
                                                                               
                                                                         
                                                                         
                                                                         
                                          
   
int
executor_errposition(EState *estate, int location)
{
  int pos;

                                          
  if (location < 0)
  {
    return 0;
  }
                                                         
  if (estate == NULL || estate->es_sourceText == NULL)
  {
    return 0;
  }
                                          
  pos = pg_mbstrlen_with_len(estate->es_sourceText, location) + 1;
                                            
  return errposition(pos);
}

   
                                                   
   
                                                                        
                                                                       
                                                                         
                                                                    
                                                                        
                
   
void
RegisterExprContextCallback(ExprContext *econtext, ExprContextCallbackFunction function, Datum arg)
{
  ExprContext_CB *ecxt_callback;

                                                   
  ecxt_callback = (ExprContext_CB *)MemoryContextAlloc(econtext->ecxt_per_query_memory, sizeof(ExprContext_CB));

  ecxt_callback->function = function;
  ecxt_callback->arg = arg;

                                                             
  ecxt_callback->next = econtext->ecxt_callbacks;
  econtext->ecxt_callbacks = ecxt_callback;
}

   
                                                     
   
                                                                   
                                                                      
   
void
UnregisterExprContextCallback(ExprContext *econtext, ExprContextCallbackFunction function, Datum arg)
{
  ExprContext_CB **prev_callback;
  ExprContext_CB *ecxt_callback;

  prev_callback = &econtext->ecxt_callbacks;

  while ((ecxt_callback = *prev_callback) != NULL)
  {
    if (ecxt_callback->function == function && ecxt_callback->arg == arg)
    {
      *prev_callback = ecxt_callback->next;
      pfree(ecxt_callback);
    }
    else
    {
      prev_callback = &ecxt_callback->next;
    }
  }
}

   
                                                                 
   
                                                                         
                                                
   
                                                                          
                                      
   
static void
ShutdownExprContext(ExprContext *econtext, bool isCommit)
{
  ExprContext_CB *ecxt_callback;
  MemoryContext oldcontext;

                                                             
  if (econtext->ecxt_callbacks == NULL)
  {
    return;
  }

     
                                                                            
                                                     
     
  oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

     
                                                                
     
  while ((ecxt_callback = econtext->ecxt_callbacks) != NULL)
  {
    econtext->ecxt_callbacks = ecxt_callback->next;
    if (isCommit)
    {
      ecxt_callback->function(ecxt_callback->arg);
    }
    pfree(ecxt_callback);
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                       
                      
   
                                                                
                                  
                                                               
                                                                     
                                                                    
                         
   
Datum
GetAttributeByName(HeapTupleHeader tuple, const char *attname, bool *isNull)
{
  AttrNumber attrno;
  Datum result;
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupDesc;
  HeapTupleData tmptup;
  int i;

  if (attname == NULL)
  {
    elog(ERROR, "invalid attribute name");
  }

  if (isNull == NULL)
  {
    elog(ERROR, "a NULL isNull pointer was passed");
  }

  if (tuple == NULL)
  {
                                                         
    *isNull = true;
    return (Datum)0;
  }

  tupType = HeapTupleHeaderGetTypeId(tuple);
  tupTypmod = HeapTupleHeaderGetTypMod(tuple);
  tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

  attrno = InvalidAttrNumber;
  for (i = 0; i < tupDesc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupDesc, i);

    if (namestrcmp(&(att->attname), attname) == 0)
    {
      attrno = att->attnum;
      break;
    }
  }

  if (attrno == InvalidAttrNumber)
  {
    elog(ERROR, "attribute \"%s\" does not exist", attname);
  }

     
                                                                            
                                                                        
              
     
  tmptup.t_len = HeapTupleHeaderGetDatumLength(tuple);
  ItemPointerSetInvalid(&(tmptup.t_self));
  tmptup.t_tableOid = InvalidOid;
  tmptup.t_data = tuple;

  result = heap_getattr(&tmptup, attrno, tupDesc, isNull);

  ReleaseTupleDesc(tupDesc);

  return result;
}

Datum
GetAttributeByNum(HeapTupleHeader tuple, AttrNumber attrno, bool *isNull)
{
  Datum result;
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupDesc;
  HeapTupleData tmptup;

  if (!AttributeNumberIsValid(attrno))
  {
    elog(ERROR, "invalid attribute number %d", attrno);
  }

  if (isNull == NULL)
  {
    elog(ERROR, "a NULL isNull pointer was passed");
  }

  if (tuple == NULL)
  {
                                                         
    *isNull = true;
    return (Datum)0;
  }

  tupType = HeapTupleHeaderGetTypeId(tuple);
  tupTypmod = HeapTupleHeaderGetTypMod(tuple);
  tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

     
                                                                            
                                                                        
              
     
  tmptup.t_len = HeapTupleHeaderGetDatumLength(tuple);
  ItemPointerSetInvalid(&(tmptup.t_self));
  tmptup.t_tableOid = InvalidOid;
  tmptup.t_data = tuple;

  result = heap_getattr(&tmptup, attrno, tupDesc, isNull);

  ReleaseTupleDesc(tupDesc);

  return result;
}

   
                                                             
   
int
ExecTargetListLength(List *targetlist)
{
                                                         
  return list_length(targetlist);
}

   
                                                               
   
int
ExecCleanTargetListLength(List *targetlist)
{
  int len = 0;
  ListCell *tl;

  foreach (tl, targetlist)
  {
    TargetEntry *curTle = lfirst_node(TargetEntry, tl);

    if (!curTle->resjunk)
    {
      len++;
    }
  }
  return len;
}

   
                                                             
   
TupleTableSlot *
ExecGetTriggerOldSlot(EState *estate, ResultRelInfo *relInfo)
{
  if (relInfo->ri_TrigOldSlot == NULL)
  {
    Relation rel = relInfo->ri_RelationDesc;
    MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

    relInfo->ri_TrigOldSlot = ExecInitExtraTupleSlot(estate, RelationGetDescr(rel), table_slot_callbacks(rel));

    MemoryContextSwitchTo(oldcontext);
  }

  return relInfo->ri_TrigOldSlot;
}

   
                                                             
   
TupleTableSlot *
ExecGetTriggerNewSlot(EState *estate, ResultRelInfo *relInfo)
{
  if (relInfo->ri_TrigNewSlot == NULL)
  {
    Relation rel = relInfo->ri_RelationDesc;
    MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

    relInfo->ri_TrigNewSlot = ExecInitExtraTupleSlot(estate, RelationGetDescr(rel), table_slot_callbacks(rel));

    MemoryContextSwitchTo(oldcontext);
  }

  return relInfo->ri_TrigNewSlot;
}

   
                                                                  
   
TupleTableSlot *
ExecGetReturningSlot(EState *estate, ResultRelInfo *relInfo)
{
  if (relInfo->ri_ReturningSlot == NULL)
  {
    Relation rel = relInfo->ri_RelationDesc;
    MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

    relInfo->ri_ReturningSlot = ExecInitExtraTupleSlot(estate, RelationGetDescr(rel), table_slot_callbacks(rel));

    MemoryContextSwitchTo(oldcontext);
  }

  return relInfo->ri_ReturningSlot;
}

                                                         
Bitmapset *
ExecGetInsertedCols(ResultRelInfo *relinfo, EState *estate)
{
     
                                                                             
                                                                             
                                                                           
                                          
     
  if (relinfo->ri_RangeTableIndex != 0)
  {
    RangeTblEntry *rte = exec_rt_fetch(relinfo->ri_RangeTableIndex, estate);

    return rte->insertedCols;
  }
  else if (relinfo->ri_RootResultRelInfo)
  {
    ResultRelInfo *rootRelInfo = relinfo->ri_RootResultRelInfo;
    RangeTblEntry *rte = exec_rt_fetch(rootRelInfo->ri_RangeTableIndex, estate);
    PartitionRoutingInfo *partrouteinfo = relinfo->ri_PartitionInfo;

    if (partrouteinfo->pi_RootToPartitionMap != NULL)
    {
      return execute_attr_map_cols(rte->insertedCols, partrouteinfo->pi_RootToPartitionMap);
    }
    else
    {
      return rte->insertedCols;
    }
  }
  else
  {
       
                                                                      
                                                                         
                                                                          
                                 
       
    return NULL;
  }
}

                                                        
Bitmapset *
ExecGetUpdatedCols(ResultRelInfo *relinfo, EState *estate)
{
                                 
  if (relinfo->ri_RangeTableIndex != 0)
  {
    RangeTblEntry *rte = exec_rt_fetch(relinfo->ri_RangeTableIndex, estate);

    return rte->updatedCols;
  }
  else if (relinfo->ri_RootResultRelInfo)
  {
    ResultRelInfo *rootRelInfo = relinfo->ri_RootResultRelInfo;
    RangeTblEntry *rte = exec_rt_fetch(rootRelInfo->ri_RangeTableIndex, estate);
    PartitionRoutingInfo *partrouteinfo = relinfo->ri_PartitionInfo;

    if (partrouteinfo->pi_RootToPartitionMap != NULL)
    {
      return execute_attr_map_cols(rte->updatedCols, partrouteinfo->pi_RootToPartitionMap);
    }
    else
    {
      return rte->updatedCols;
    }
  }
  else
  {
    return NULL;
  }
}

                                                                  
Bitmapset *
ExecGetExtraUpdatedCols(ResultRelInfo *relinfo, EState *estate)
{
                                 
  if (relinfo->ri_RangeTableIndex != 0)
  {
    RangeTblEntry *rte = exec_rt_fetch(relinfo->ri_RangeTableIndex, estate);

    return rte->extraUpdatedCols;
  }
  else if (relinfo->ri_RootResultRelInfo)
  {
    ResultRelInfo *rootRelInfo = relinfo->ri_RootResultRelInfo;
    RangeTblEntry *rte = exec_rt_fetch(rootRelInfo->ri_RangeTableIndex, estate);
    PartitionRoutingInfo *partrouteinfo = relinfo->ri_PartitionInfo;

    if (partrouteinfo->pi_RootToPartitionMap != NULL)
    {
      return execute_attr_map_cols(rte->extraUpdatedCols, partrouteinfo->pi_RootToPartitionMap);
    }
    else
    {
      return rte->extraUpdatedCols;
    }
  }
  else
  {
    return NULL;
  }
}

                                                               
Bitmapset *
ExecGetAllUpdatedCols(ResultRelInfo *relinfo, EState *estate)
{
  return bms_union(ExecGetUpdatedCols(relinfo, estate), ExecGetExtraUpdatedCols(relinfo, estate));
}
