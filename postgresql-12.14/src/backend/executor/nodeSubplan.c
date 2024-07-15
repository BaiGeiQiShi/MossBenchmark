                                                                            
   
                 
                                                              
   
                                                                           
                                                                            
                                                                               
                                                                          
                                                                           
                                                                         
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
   
                       
                                       
                                             
   
#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/htup_details.h"
#include "executor/executor.h"
#include "executor/nodeSubplan.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static Datum
ExecHashSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull);
static Datum
ExecScanSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull);
static void
buildSubPlanHash(SubPlanState *node, ExprContext *econtext);
static bool
findPartialMatch(TupleHashTable hashtable, TupleTableSlot *slot, FmgrInfo *eqfunctions);
static bool
slotAllNulls(TupleTableSlot *slot);
static bool
slotNoNulls(TupleTableSlot *slot);

                                                                    
                
   
                                                                    
                                                                    
   
Datum
ExecSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull)
{
  SubPlan *subplan = node->subplan;
  EState *estate = node->planstate->state;
  ScanDirection dir = estate->es_direction;
  Datum retval;

  CHECK_FOR_INTERRUPTS();

                               
  *isNull = false;

                     
  if (subplan->subLinkType == CTE_SUBLINK)
  {
    elog(ERROR, "CTE subplans should not be executed via ExecSubPlan");
  }
  if (subplan->setParam != NIL && subplan->subLinkType != MULTIEXPR_SUBLINK)
  {
    elog(ERROR, "cannot set parent params from subquery");
  }

                                              
  estate->es_direction = ForwardScanDirection;

                                              
  if (subplan->useHashTable)
  {
    retval = ExecHashSubPlan(node, econtext, isNull);
  }
  else
  {
    retval = ExecScanSubPlan(node, econtext, isNull);
  }

                              
  estate->es_direction = dir;

  return retval;
}

   
                                                                      
   
static Datum
ExecHashSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull)
{
  SubPlan *subplan = node->subplan;
  PlanState *planstate = node->planstate;
  TupleTableSlot *slot;

                                                  
  if (subplan->parParam != NIL || node->args != NIL)
  {
    elog(ERROR, "hashed subplan with direct correlation not supported");
  }

     
                                                                            
            
     
  if (node->hashtable == NULL || planstate->chgParam != NULL)
  {
    buildSubPlanHash(node, econtext);
  }

     
                                                                          
                    
     
  *isNull = false;
  if (!node->havehashrows && !node->havenullrows)
  {
    return BoolGetDatum(false);
  }

     
                                                                         
                                                    
     
  node->projLeft->pi_exprContext = econtext;
  slot = ExecProject(node->projLeft);

     
                                                                           
                                                                          
                                                                             
                                                                            
                                       
     

     
                                                                           
                                                                     
                                                                         
                                                                           
                                                                      
     
                                                                         
                                                                            
                                                                           
                                                                            
                                                                             
                                                       
     
  if (slotNoNulls(slot))
  {
    if (node->havehashrows && FindTupleHashEntry(node->hashtable, slot, node->cur_eq_comp, node->lhs_hash_funcs) != NULL)
    {
      ExecClearTuple(slot);
      return BoolGetDatum(true);
    }
    if (node->havenullrows && findPartialMatch(node->hashnulls, slot, node->cur_eq_funcs))
    {
      ExecClearTuple(slot);
      *isNull = true;
      return BoolGetDatum(false);
    }
    ExecClearTuple(slot);
    return BoolGetDatum(false);
  }

     
                                                                            
                                                                            
                                                                    
                                                                            
                                                                            
                                                                            
                                                                       
                                     
     
  if (node->hashnulls == NULL)
  {
    ExecClearTuple(slot);
    return BoolGetDatum(false);
  }
  if (slotAllNulls(slot))
  {
    ExecClearTuple(slot);
    *isNull = true;
    return BoolGetDatum(false);
  }
                                                                      
  if (node->havenullrows && findPartialMatch(node->hashnulls, slot, node->cur_eq_funcs))
  {
    ExecClearTuple(slot);
    *isNull = true;
    return BoolGetDatum(false);
  }
  if (node->havehashrows && findPartialMatch(node->hashtable, slot, node->cur_eq_funcs))
  {
    ExecClearTuple(slot);
    *isNull = true;
    return BoolGetDatum(false);
  }
  ExecClearTuple(slot);
  return BoolGetDatum(false);
}

   
                                                                           
   
static Datum
ExecScanSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull)
{
  SubPlan *subplan = node->subplan;
  PlanState *planstate = node->planstate;
  SubLinkType subLinkType = subplan->subLinkType;
  MemoryContext oldcontext;
  TupleTableSlot *slot;
  Datum result;
  bool found = false;                                             
  ListCell *pvar;
  ListCell *l;
  ArrayBuildStateAny *astate = NULL;

     
                                                                         
                                                                           
                                                                         
                                                                           
                                                                        
                                                                         
                                                                          
                                                                         
                                           
     
                                                                            
                                                                            
                                                                             
                                                                             
                                                                             
                                                                          
                                                                          
                                                                           
                                                                          
                                                                       
                                                                       
                                                                      
                                                                            
                                   
     
  if (subLinkType == MULTIEXPR_SUBLINK)
  {
    EState *estate = node->parent->state;

    foreach (l, subplan->setParam)
    {
      int paramid = lfirst_int(l);
      ParamExecData *prm = &(estate->es_param_exec_vals[paramid]);

      prm->execPlan = node;
    }
    *isNull = true;
    return (Datum)0;
  }

                                                                    
  if (subLinkType == ARRAY_SUBLINK)
  {
    astate = initArrayResultAny(subplan->firstColType, CurrentMemoryContext, true);
  }

     
                                                                            
                                                                          
                                      
     
  oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_query_memory);

     
                                                                       
                                                                         
                                                          
     
  Assert(list_length(subplan->parParam) == list_length(node->args));

  forboth(l, subplan->parParam, pvar, node->args)
  {
    int paramid = lfirst_int(l);
    ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

    prm->value = ExecEvalExprSwitchContext((ExprState *)lfirst(pvar), econtext, &(prm->isnull));
    planstate->chgParam = bms_add_member(planstate->chgParam, paramid);
  }

     
                                                                     
     
  ExecReScan(planstate);

     
                                                                             
                                                                          
                                                                            
                                                                 
                                                                          
                                                                            
                                                                            
                                                           
                         
     
                                                                         
                                                                             
                                                                             
                                                
     
                                                                             
                                                                         
                                                                             
                                                        
     
  result = BoolGetDatum(subLinkType == ALL_SUBLINK);
  *isNull = false;

  for (slot = ExecProcNode(planstate); !TupIsNull(slot); slot = ExecProcNode(planstate))
  {
    TupleDesc tdesc = slot->tts_tupleDescriptor;
    Datum rowresult;
    bool rownull;
    int col;
    ListCell *plst;

    if (subLinkType == EXISTS_SUBLINK)
    {
      found = true;
      result = BoolGetDatum(true);
      break;
    }

    if (subLinkType == EXPR_SUBLINK)
    {
                                                               
      if (found)
      {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("more than one row returned by a subquery used as an expression")));
      }
      found = true;

         
                                                                      
                                                                    
                                                                      
                                                                       
                                                                     
                  
         
      if (node->curTuple)
      {
        heap_freetuple(node->curTuple);
      }
      node->curTuple = ExecCopySlotHeapTuple(slot);

      result = heap_getattr(node->curTuple, 1, tdesc, isNull);
                                                                     
      continue;
    }

    if (subLinkType == ARRAY_SUBLINK)
    {
      Datum dvalue;
      bool disnull;

      found = true;
                                    
      Assert(subplan->firstColType == TupleDescAttr(tdesc, 0)->atttypid);
      dvalue = slot_getattr(slot, 1, &disnull);
      astate = accumArrayResultAny(astate, dvalue, disnull, subplan->firstColType, oldcontext);
                                                       
      continue;
    }

                                                                          
    if (subLinkType == ROWCOMPARE_SUBLINK && found)
    {
      ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("more than one row returned by a subquery used as an expression")));
    }

    found = true;

       
                                                                 
                                                                         
                             
       
    col = 1;
    foreach (plst, subplan->paramIds)
    {
      int paramid = lfirst_int(plst);
      ParamExecData *prmdata;

      prmdata = &(econtext->ecxt_param_exec_vals[paramid]);
      Assert(prmdata->execPlan == NULL);
      prmdata->value = slot_getattr(slot, col, &(prmdata->isnull));
      col++;
    }

    rowresult = ExecEvalExprSwitchContext(node->testexpr, econtext, &rownull);

    if (subLinkType == ANY_SUBLINK)
    {
                                                
      if (rownull)
      {
        *isNull = true;
      }
      else if (DatumGetBool(rowresult))
      {
        result = BoolGetDatum(true);
        *isNull = false;
        break;                                    
      }
    }
    else if (subLinkType == ALL_SUBLINK)
    {
                                                 
      if (rownull)
      {
        *isNull = true;
      }
      else if (!DatumGetBool(rowresult))
      {
        result = BoolGetDatum(false);
        *isNull = false;
        break;                                    
      }
    }
    else
    {
                                      
      result = rowresult;
      *isNull = rownull;
    }
  }

  MemoryContextSwitchTo(oldcontext);

  if (subLinkType == ARRAY_SUBLINK)
  {
                                                      
    result = makeArrayResultAny(astate, oldcontext, true);
  }
  else if (!found)
  {
       
                                                                      
                                                                   
                                           
       
    if (subLinkType == EXPR_SUBLINK || subLinkType == ROWCOMPARE_SUBLINK)
    {
      result = (Datum)0;
      *isNull = true;
    }
  }

  return result;
}

   
                                                                 
   
static void
buildSubPlanHash(SubPlanState *node, ExprContext *econtext)
{
  SubPlan *subplan = node->subplan;
  PlanState *planstate = node->planstate;
  int ncols = node->numCols;
  ExprContext *innerecontext = node->innerecontext;
  MemoryContext oldcontext;
  long nbuckets;
  TupleTableSlot *slot;

  Assert(subplan->subLinkType == ANY_SUBLINK);

     
                                                                          
                    
     
                                                                           
                                                                             
                                                                            
                                                                             
                                                                             
                                                                          
                                      
     
                                                                           
                                                          
     
  MemoryContextReset(node->hashtablecxt);
  node->havehashrows = false;
  node->havenullrows = false;

  nbuckets = (long)Min(planstate->plan->plan_rows, (double)LONG_MAX);
  if (nbuckets < 1)
  {
    nbuckets = 1;
  }

  if (node->hashtable)
  {
    ResetTupleHashTable(node->hashtable);
  }
  else
  {
    node->hashtable = BuildTupleHashTableExt(node->parent, node->descRight, ncols, node->keyColIdx, node->tab_eq_funcoids, node->tab_hash_funcs, node->tab_collations, nbuckets, 0, node->planstate->state->es_query_cxt, node->hashtablecxt, node->hashtempcxt, false);
  }

  if (!subplan->unknownEqFalse)
  {
    if (ncols == 1)
    {
      nbuckets = 1;                                  
    }
    else
    {
      nbuckets /= 16;
      if (nbuckets < 1)
      {
        nbuckets = 1;
      }
    }

    if (node->hashnulls)
    {
      ResetTupleHashTable(node->hashnulls);
    }
    else
    {
      node->hashnulls = BuildTupleHashTableExt(node->parent, node->descRight, ncols, node->keyColIdx, node->tab_eq_funcoids, node->tab_hash_funcs, node->tab_collations, nbuckets, 0, node->planstate->state->es_query_cxt, node->hashtablecxt, node->hashtempcxt, false);
    }
  }
  else
  {
    node->hashnulls = NULL;
  }

     
                                                                            
                                                               
     
  oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_query_memory);

     
                             
     
  ExecReScan(planstate);

     
                                                                            
                                                                           
     
  for (slot = ExecProcNode(planstate); !TupIsNull(slot); slot = ExecProcNode(planstate))
  {
    int col = 1;
    ListCell *plst;
    bool isnew;

       
                                                                        
                                                            
       
    foreach (plst, subplan->paramIds)
    {
      int paramid = lfirst_int(plst);
      ParamExecData *prmdata;

      prmdata = &(innerecontext->ecxt_param_exec_vals[paramid]);
      Assert(prmdata->execPlan == NULL);
      prmdata->value = slot_getattr(slot, col, &(prmdata->isnull));
      col++;
    }
    slot = ExecProject(node->projRight);

       
                                                                     
       
    if (slotNoNulls(slot))
    {
      (void)LookupTupleHashEntry(node->hashtable, slot, &isnew);
      node->havehashrows = true;
    }
    else if (node->hashnulls)
    {
      (void)LookupTupleHashEntry(node->hashnulls, slot, &isnew);
      node->havenullrows = true;
    }

       
                                                                          
                           
       
    ResetExprContext(innerecontext);
  }

     
                                                                           
                                                                       
                                                                          
                                                                           
                      
     
  ExecClearTuple(node->projRight->pi_state.resultslot);

  MemoryContextSwitchTo(oldcontext);
}

   
                     
                                                                      
            
   
                                                                        
                                                                         
   
                                                                 
                                                    
                                                  
                                                                            
                                                                      
   
static bool
execTuplesUnequal(TupleTableSlot *slot1, TupleTableSlot *slot2, int numCols, AttrNumber *matchColIdx, FmgrInfo *eqfunctions, const Oid *collations, MemoryContext evalContext)
{
  MemoryContext oldContext;
  bool result;
  int i;

                                               
  MemoryContextReset(evalContext);
  oldContext = MemoryContextSwitchTo(evalContext);

     
                                                                          
                                                                      
                                                                          
                                                                      
     
  result = false;

  for (i = numCols; --i >= 0;)
  {
    AttrNumber att = matchColIdx[i];
    Datum attr1, attr2;
    bool isNull1, isNull2;

    attr1 = slot_getattr(slot1, att, &isNull1);

    if (isNull1)
    {
      continue;                                
    }

    attr2 = slot_getattr(slot2, att, &isNull2);

    if (isNull2)
    {
      continue;                                
    }

                                                   
    if (!DatumGetBool(FunctionCall2Coll(&eqfunctions[i], collations[i], attr1, attr2)))
    {
      result = true;                       
      break;
    }
  }

  MemoryContextSwitchTo(oldContext);

  return result;
}

   
                                                                     
                                     
   
                                                                       
                                                                       
                                                                   
   
                                                                          
                                                                      
   
static bool
findPartialMatch(TupleHashTable hashtable, TupleTableSlot *slot, FmgrInfo *eqfunctions)
{
  int numCols = hashtable->numCols;
  AttrNumber *keyColIdx = hashtable->keyColIdx;
  TupleHashIterator hashiter;
  TupleHashEntry entry;

  InitTupleHashIterator(hashtable, &hashiter);
  while ((entry = ScanTupleHashTable(hashtable, &hashiter)) != NULL)
  {
    CHECK_FOR_INTERRUPTS();

    ExecStoreMinimalTuple(entry->firstTuple, hashtable->tableslot, false);
    if (!execTuplesUnequal(slot, hashtable->tableslot, numCols, keyColIdx, eqfunctions, hashtable->tab_collations, hashtable->tempcxt))
    {
      TermTupleHashIterator(&hashiter);
      return true;
    }
  }
                                                 
  return false;
}

   
                                              
   
                                                                       
                               
   
static bool
slotAllNulls(TupleTableSlot *slot)
{
  int ncols = slot->tts_tupleDescriptor->natts;
  int i;

  for (i = 1; i <= ncols; i++)
  {
    if (!slot_attisnull(slot, i))
    {
      return false;
    }
  }
  return true;
}

   
                                               
   
                                                                       
                               
   
static bool
slotNoNulls(TupleTableSlot *slot)
{
  int ncols = slot->tts_tupleDescriptor->natts;
  int i;

  for (i = 1; i <= ncols; i++)
  {
    if (slot_attisnull(slot, i))
    {
      return false;
    }
  }
  return true;
}

                                                                    
                    
   
                                                                          
                                                                            
                                                                          
                                                                           
                                               
                                                                    
   
SubPlanState *
ExecInitSubPlan(SubPlan *subplan, PlanState *parent)
{
  SubPlanState *sstate = makeNode(SubPlanState);
  EState *estate = parent->state;

  sstate->subplan = subplan;

                                                            
  sstate->planstate = (PlanState *)list_nth(estate->es_subplanstates, subplan->plan_id - 1);

     
                                                                          
                                                                  
     
  if (sstate->planstate == NULL)
  {
    elog(ERROR, "subplan \"%s\" was not initialized", subplan->plan_name);
  }

                                   
  sstate->parent = parent;

                                 
  sstate->testexpr = ExecInitExpr((Expr *)subplan->testexpr, parent);
  sstate->args = ExecInitExprList(subplan->args, parent);

     
                         
     
  sstate->curTuple = NULL;
  sstate->curArray = PointerGetDatum(NULL);
  sstate->projLeft = NULL;
  sstate->projRight = NULL;
  sstate->hashtable = NULL;
  sstate->hashnulls = NULL;
  sstate->hashtablecxt = NULL;
  sstate->hashtempcxt = NULL;
  sstate->innerecontext = NULL;
  sstate->keyColIdx = NULL;
  sstate->tab_eq_funcoids = NULL;
  sstate->tab_hash_funcs = NULL;
  sstate->tab_eq_funcs = NULL;
  sstate->tab_collations = NULL;
  sstate->lhs_hash_funcs = NULL;
  sstate->cur_eq_funcs = NULL;

     
                                                                           
                                                                        
                                                                            
                     
     
                                                                             
                                     
     
                                                                          
                                                     
     
  if (subplan->setParam != NIL && subplan->subLinkType != CTE_SUBLINK)
  {
    ListCell *lst;

    foreach (lst, subplan->setParam)
    {
      int paramid = lfirst_int(lst);
      ParamExecData *prm = &(estate->es_param_exec_vals[paramid]);

      prm->execPlan = sstate;
    }
  }

     
                                                                             
                                                           
     
  if (subplan->useHashTable)
  {
    int ncols, i;
    TupleDesc tupDescLeft;
    TupleDesc tupDescRight;
    Oid *cross_eq_funcoids;
    TupleTableSlot *slot;
    List *oplist, *lefttlist, *righttlist;
    ListCell *l;

                                                            
    sstate->hashtablecxt = AllocSetContextCreate(CurrentMemoryContext, "Subplan HashTable Context", ALLOCSET_DEFAULT_SIZES);
                                                                    
    sstate->hashtempcxt = AllocSetContextCreate(CurrentMemoryContext, "Subplan HashTable Temp Context", ALLOCSET_SMALL_SIZES);
                                                               
    sstate->innerecontext = CreateExprContext(estate);

       
                                                                 
                                                                         
                                                                        
                                                                           
                                                                        
                                                                   
                                                                  
                        
       
                                                                        
                                                               
       
    if (IsA(subplan->testexpr, OpExpr))
    {
                                     
      oplist = list_make1(subplan->testexpr);
    }
    else if (is_andclause(subplan->testexpr))
    {
                                        
      oplist = castNode(BoolExpr, subplan->testexpr)->args;
    }
    else
    {
                                                             
      elog(ERROR, "unrecognized testexpr type: %d", (int)nodeTag(subplan->testexpr));
      oplist = NIL;                          
    }
    ncols = list_length(oplist);

    lefttlist = righttlist = NIL;
    sstate->numCols = ncols;
    sstate->keyColIdx = (AttrNumber *)palloc(ncols * sizeof(AttrNumber));
    sstate->tab_eq_funcoids = (Oid *)palloc(ncols * sizeof(Oid));
    sstate->tab_collations = (Oid *)palloc(ncols * sizeof(Oid));
    sstate->tab_hash_funcs = (FmgrInfo *)palloc(ncols * sizeof(FmgrInfo));
    sstate->tab_eq_funcs = (FmgrInfo *)palloc(ncols * sizeof(FmgrInfo));
    sstate->lhs_hash_funcs = (FmgrInfo *)palloc(ncols * sizeof(FmgrInfo));
    sstate->cur_eq_funcs = (FmgrInfo *)palloc(ncols * sizeof(FmgrInfo));
                                                                         
    cross_eq_funcoids = (Oid *)palloc(ncols * sizeof(Oid));

    i = 1;
    foreach (l, oplist)
    {
      OpExpr *opexpr = lfirst_node(OpExpr, l);
      Expr *expr;
      TargetEntry *tle;
      Oid rhs_eq_oper;
      Oid left_hashfn;
      Oid right_hashfn;

      Assert(list_length(opexpr->args) == 2);

                                     
      expr = (Expr *)linitial(opexpr->args);
      tle = makeTargetEntry(expr, i, NULL, false);
      lefttlist = lappend(lefttlist, tle);

                                      
      expr = (Expr *)lsecond(opexpr->args);
      tle = makeTargetEntry(expr, i, NULL, false);
      righttlist = lappend(righttlist, tle);

                                                                 
      cross_eq_funcoids[i - 1] = opexpr->opfuncid;
      fmgr_info(opexpr->opfuncid, &sstate->cur_eq_funcs[i - 1]);
      fmgr_info_set_expr((Node *)opexpr, &sstate->cur_eq_funcs[i - 1]);

                                                          
      if (!get_compatible_hash_operators(opexpr->opno, NULL, &rhs_eq_oper))
      {
        elog(ERROR, "could not find compatible hash operator for operator %u", opexpr->opno);
      }
      sstate->tab_eq_funcoids[i - 1] = get_opcode(rhs_eq_oper);
      fmgr_info(sstate->tab_eq_funcoids[i - 1], &sstate->tab_eq_funcs[i - 1]);

                                                
      if (!get_op_hash_functions(opexpr->opno, &left_hashfn, &right_hashfn))
      {
        elog(ERROR, "could not find hash function for hash operator %u", opexpr->opno);
      }
      fmgr_info(left_hashfn, &sstate->lhs_hash_funcs[i - 1]);
      fmgr_info(right_hashfn, &sstate->tab_hash_funcs[i - 1]);

                         
      sstate->tab_collations[i - 1] = opexpr->inputcollid;

                                                 
      sstate->keyColIdx[i - 1] = i;

      i++;
    }

       
                                                                         
                                                                        
                                                                    
                                                                      
                                                                          
                          
       
    tupDescLeft = ExecTypeFromTL(lefttlist);
    slot = ExecInitExtraTupleSlot(estate, tupDescLeft, &TTSOpsVirtual);
    sstate->projLeft = ExecBuildProjectionInfo(lefttlist, NULL, slot, parent, NULL);

    sstate->descRight = tupDescRight = ExecTypeFromTL(righttlist);
    slot = ExecInitExtraTupleSlot(estate, tupDescRight, &TTSOpsVirtual);
    sstate->projRight = ExecBuildProjectionInfo(righttlist, sstate->innerecontext, slot, sstate->planstate, NULL);

       
                                                                       
                                
       
    sstate->cur_eq_comp = ExecBuildGroupingEqual(tupDescLeft, tupDescRight, &TTSOpsVirtual, &TTSOpsMinimalTuple, ncols, sstate->keyColIdx, cross_eq_funcoids, sstate->tab_collations, parent);
  }

  return sstate;
}

                                                                    
                     
   
                                                       
   
                                                                          
                                                                            
                                                                            
                                                                           
                                                                       
                                            
   
                                                                             
                                                                         
                                                                             
                                                                          
                                                                        
                                                                          
                                                                           
                                                                          
                                                                              
                                             
                                                                    
   
void
ExecSetParamPlan(SubPlanState *node, ExprContext *econtext)
{
  SubPlan *subplan = node->subplan;
  PlanState *planstate = node->planstate;
  SubLinkType subLinkType = subplan->subLinkType;
  EState *estate = planstate->state;
  ScanDirection dir = estate->es_direction;
  MemoryContext oldcontext;
  TupleTableSlot *slot;
  ListCell *pvar;
  ListCell *l;
  bool found = false;
  ArrayBuildStateAny *astate = NULL;

  if (subLinkType == ANY_SUBLINK || subLinkType == ALL_SUBLINK)
  {
    elog(ERROR, "ANY/ALL subselect unsupported as initplan");
  }
  if (subLinkType == CTE_SUBLINK)
  {
    elog(ERROR, "CTE subplans should not be executed via ExecSetParamPlan");
  }

     
                                                                            
                                                                      
     
  estate->es_direction = ForwardScanDirection;

                                                                    
  if (subLinkType == ARRAY_SUBLINK)
  {
    astate = initArrayResultAny(subplan->firstColType, CurrentMemoryContext, true);
  }

     
                                              
     
  oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_query_memory);

     
                                                                       
                                                                         
                                                                         
                                                                         
     
  Assert(subplan->parParam == NIL || subLinkType == MULTIEXPR_SUBLINK);
  Assert(list_length(subplan->parParam) == list_length(node->args));

  forboth(l, subplan->parParam, pvar, node->args)
  {
    int paramid = lfirst_int(l);
    ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

    prm->value = ExecEvalExprSwitchContext((ExprState *)lfirst(pvar), econtext, &(prm->isnull));
    planstate->chgParam = bms_add_member(planstate->chgParam, paramid);
  }

     
                                                                         
                                   
     
  for (slot = ExecProcNode(planstate); !TupIsNull(slot); slot = ExecProcNode(planstate))
  {
    TupleDesc tdesc = slot->tts_tupleDescriptor;
    int i = 1;

    if (subLinkType == EXISTS_SUBLINK)
    {
                                             
      int paramid = linitial_int(subplan->setParam);
      ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

      prm->execPlan = NULL;
      prm->value = BoolGetDatum(true);
      prm->isnull = false;
      found = true;
      break;
    }

    if (subLinkType == ARRAY_SUBLINK)
    {
      Datum dvalue;
      bool disnull;

      found = true;
                                    
      Assert(subplan->firstColType == TupleDescAttr(tdesc, 0)->atttypid);
      dvalue = slot_getattr(slot, 1, &disnull);
      astate = accumArrayResultAny(astate, dvalue, disnull, subplan->firstColType, oldcontext);
                                                       
      continue;
    }

    if (found && (subLinkType == EXPR_SUBLINK || subLinkType == MULTIEXPR_SUBLINK || subLinkType == ROWCOMPARE_SUBLINK))
    {
      ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("more than one row returned by a subquery used as an expression")));
    }

    found = true;

       
                                                                         
                                                                         
                                                                         
                                                             
       
    if (node->curTuple)
    {
      heap_freetuple(node->curTuple);
    }
    node->curTuple = ExecCopySlotHeapTuple(slot);

       
                                                                     
       
    foreach (l, subplan->setParam)
    {
      int paramid = lfirst_int(l);
      ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

      prm->execPlan = NULL;
      prm->value = heap_getattr(node->curTuple, i, tdesc, &(prm->isnull));
      i++;
    }
  }

  if (subLinkType == ARRAY_SUBLINK)
  {
                                           
    int paramid = linitial_int(subplan->setParam);
    ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

       
                                                                         
                                                                          
                                                     
       
    if (node->curArray != PointerGetDatum(NULL))
    {
      pfree(DatumGetPointer(node->curArray));
    }
    node->curArray = makeArrayResultAny(astate, econtext->ecxt_per_query_memory, true);
    prm->execPlan = NULL;
    prm->value = node->curArray;
    prm->isnull = false;
  }
  else if (!found)
  {
    if (subLinkType == EXISTS_SUBLINK)
    {
                                             
      int paramid = linitial_int(subplan->setParam);
      ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

      prm->execPlan = NULL;
      prm->value = BoolGetDatum(false);
      prm->isnull = false;
    }
    else
    {
                                                                      
      foreach (l, subplan->setParam)
      {
        int paramid = lfirst_int(l);
        ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

        prm->execPlan = NULL;
        prm->value = (Datum)0;
        prm->isnull = true;
      }
    }
  }

  MemoryContextSwitchTo(oldcontext);

                              
  estate->es_direction = dir;
}

   
                         
   
                                                                            
                                                                             
                                         
   
                                                                             
                                                                              
                     
   
void
ExecSetParamPlanMulti(const Bitmapset *params, ExprContext *econtext)
{
  int paramid;

  paramid = -1;
  while ((paramid = bms_next_member(params, paramid)) >= 0)
  {
    ParamExecData *prm = &(econtext->ecxt_param_exec_vals[paramid]);

    if (prm->execPlan != NULL)
    {
                                                    
      ExecSetParamPlan(prm->execPlan, econtext);
                                                                
      Assert(prm->execPlan == NULL);
    }
  }
}

   
                                             
   
void
ExecReScanSetParamPlan(SubPlanState *node, PlanState *parent)
{
  PlanState *planstate = node->planstate;
  SubPlan *subplan = node->subplan;
  EState *estate = parent->state;
  ListCell *l;

                     
  if (subplan->parParam != NIL)
  {
    elog(ERROR, "direct correlated subquery unsupported as initplan");
  }
  if (subplan->setParam == NIL)
  {
    elog(ERROR, "setParam list of initplan is empty");
  }
  if (bms_is_empty(planstate->plan->extParam))
  {
    elog(ERROR, "extParam set of initplan is empty");
  }

     
                                                                             
     

     
                                                                     
     
                                                                          
                                                                          
                                                                             
                                                           
     
  foreach (l, subplan->setParam)
  {
    int paramid = lfirst_int(l);
    ParamExecData *prm = &(estate->es_param_exec_vals[paramid]);

    if (subplan->subLinkType != CTE_SUBLINK)
    {
      prm->execPlan = node;
    }

    parent->chgParam = bms_add_member(parent->chgParam, paramid);
  }
}

   
                              
   
                                                                     
   
AlternativeSubPlanState *
ExecInitAlternativeSubPlan(AlternativeSubPlan *asplan, PlanState *parent)
{
  AlternativeSubPlanState *asstate = makeNode(AlternativeSubPlanState);
  double num_calls;
  SubPlan *subplan1;
  SubPlan *subplan2;
  Cost cost1;
  Cost cost2;
  ListCell *lc;

  asstate->subplan = asplan;

     
                                                                           
                          
     
  foreach (lc, asplan->subplans)
  {
    SubPlan *sp = lfirst_node(SubPlan, lc);
    SubPlanState *sps = ExecInitSubPlan(sp, parent);

    asstate->subplans = lappend(asstate->subplans, sps);
    parent->subPlan = lappend(parent->subPlan, sps);
  }

     
                                                                             
                                                                     
                                                                            
                                                                           
                                                     
     
  num_calls = parent->plan->plan_rows;

     
                                                                           
                                                                     
     
  Assert(list_length(asplan->subplans) == 2);
  subplan1 = (SubPlan *)linitial(asplan->subplans);
  subplan2 = (SubPlan *)lsecond(asplan->subplans);

  cost1 = subplan1->startup_cost + num_calls * subplan1->per_call_cost;
  cost2 = subplan2->startup_cost + num_calls * subplan2->per_call_cost;

  if (cost1 < cost2)
  {
    asstate->active = 0;
  }
  else
  {
    asstate->active = 1;
  }

  return asstate;
}

   
                          
   
                                                 
   
                                                                           
                                                                        
   
Datum
ExecAlternativeSubPlan(AlternativeSubPlanState *node, ExprContext *econtext, bool *isNull)
{
                                               
  SubPlanState *activesp = list_nth_node(SubPlanState, node->subplans, node->active);

  return ExecSubPlan(activesp, econtext, isNull);
}
