                                                                            
   
              
                                           
   
                                                                       
                                                                           
                                                                         
                                                                           
                                                                            
                        
   
                                                                         
                                                                          
                          
   
                                                                         
                                                                        
                                         
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/nbtree.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_type.h"
#include "executor/execExpr.h"
#include "executor/nodeSubplan.h"
#include "funcapi.h"
#include "jit/jit.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

typedef struct LastAttnumInfo
{
  AttrNumber last_inner;
  AttrNumber last_outer;
  AttrNumber last_scan;
} LastAttnumInfo;

static void
ExecReadyExpr(ExprState *state);
static void
ExecInitExprRec(Expr *node, ExprState *state, Datum *resv, bool *resnull);
static void
ExecInitFunc(ExprEvalStep *scratch, Expr *node, List *args, Oid funcid, Oid inputcollid, ExprState *state);
static void
ExecInitExprSlots(ExprState *state, Node *node);
static void
ExecPushExprSlots(ExprState *state, LastAttnumInfo *info);
static bool
get_last_attnums_walker(Node *node, LastAttnumInfo *info);
static void
ExecComputeSlotInfo(ExprState *state, ExprEvalStep *op);
static void
ExecInitWholeRowVar(ExprEvalStep *scratch, Var *variable, ExprState *state);
static void
ExecInitSubscriptingRef(ExprEvalStep *scratch, SubscriptingRef *sbsref, ExprState *state, Datum *resv, bool *resnull);
static bool
isAssignmentIndirectionExpr(Expr *expr);
static void
ExecInitCoerceToDomain(ExprEvalStep *scratch, CoerceToDomain *ctest, ExprState *state, Datum *resv, bool *resnull);
static void
ExecBuildAggTransCall(ExprState *state, AggState *aggstate, ExprEvalStep *scratch, FunctionCallInfo fcinfo, AggStatePerTrans pertrans, int transno, int setno, int setoff, bool ishash);

   
                                                          
   
                                                                        
                                                                            
                                                                       
                                                                             
                                                                              
                                                                 
   
                                                                              
                                                                           
                                                                    
   
                                                                           
                                                                             
                                                               
   
                                                                       
                                                                       
                                                                        
                                                                     
   
                                                         
                                                            
   
                                                                      
                                                                          
                                                                              
   
                                                                              
                                                                             
                                                                          
                                                                             
   
ExprState *
ExecInitExpr(Expr *node, PlanState *parent)
{
  ExprState *state;
  ExprEvalStep scratch = {0};

                                                                       
  if (node == NULL)
  {
    return NULL;
  }

                                                 
  state = makeNode(ExprState);
  state->expr = node;
  state->parent = parent;
  state->ext_params = NULL;

                                               
  ExecInitExprSlots(state, (Node *)node);

                                     
  ExecInitExprRec(node, state, &state->resvalue, &state->resnull);

                                   
  scratch.opcode = EEOP_DONE;
  ExprEvalPushStep(state, &scratch);

  ExecReadyExpr(state);

  return state;
}

   
                                                                              
   
                                                                               
                                                                           
   
ExprState *
ExecInitExprWithParams(Expr *node, ParamListInfo ext_params)
{
  ExprState *state;
  ExprEvalStep scratch = {0};

                                                                       
  if (node == NULL)
  {
    return NULL;
  }

                                                 
  state = makeNode(ExprState);
  state->expr = node;
  state->parent = NULL;
  state->ext_params = ext_params;

                                               
  ExecInitExprSlots(state, (Node *)node);

                                     
  ExecInitExprRec(node, state, &state->resvalue, &state->resnull);

                                   
  scratch.opcode = EEOP_DONE;
  ExprEvalPushStep(state, &scratch);

  ExecReadyExpr(state);

  return state;
}

   
                                                          
   
                                                                              
                                                                 
                             
   
                                                                               
                                                                           
                                                                           
                                                                         
                     
   
                                                                               
                                                                        
                                                                           
                 
   
ExprState *
ExecInitQual(List *qual, PlanState *parent)
{
  ExprState *state;
  ExprEvalStep scratch = {0};
  List *adjust_jumps = NIL;
  ListCell *lc;

                                                                       
  if (qual == NIL)
  {
    return NULL;
  }

  Assert(IsA(qual, List));

  state = makeNode(ExprState);
  state->expr = (Expr *)qual;
  state->parent = parent;
  state->ext_params = NULL;

                                                     
  state->flags = EEO_FLAG_IS_QUAL;

                                               
  ExecInitExprSlots(state, (Node *)qual);

     
                                                                             
                                                                        
                                                                          
                                                                            
                                      
     
  scratch.opcode = EEOP_QUAL;

     
                                                                           
     
  scratch.resvalue = &state->resvalue;
  scratch.resnull = &state->resnull;

  foreach (lc, qual)
  {
    Expr *node = (Expr *)lfirst(lc);

                                   
    ExecInitExprRec(node, state, &state->resvalue, &state->resnull);

                                                               
    scratch.d.qualexpr.jumpdone = -1;
    ExprEvalPushStep(state, &scratch);
    adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
  }

                           
  foreach (lc, adjust_jumps)
  {
    ExprEvalStep *as = &state->steps[lfirst_int(lc)];

    Assert(as->opcode == EEOP_QUAL);
    Assert(as->d.qualexpr.jumpdone == -1);
    as->d.qualexpr.jumpdone = state->steps_len;
  }

     
                                                                             
                                                                             
                           
     
  scratch.opcode = EEOP_DONE;
  ExprEvalPushStep(state, &scratch);

  ExecReadyExpr(state);

  return state;
}

   
                                                                        
   
                                                                           
                                                                         
                                                                          
                                
   
                                                                           
                                                                          
                                                                        
   
ExprState *
ExecInitCheck(List *qual, PlanState *parent)
{
                                                                        
  if (qual == NIL)
  {
    return NULL;
  }

  Assert(IsA(qual, List));

     
                                                                            
                                                                       
                                                                           
     
  return ExecInitExpr(make_ands_explicit(qual), parent);
}

   
                                                                              
   
List *
ExecInitExprList(List *nodes, PlanState *parent)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, nodes)
  {
    Expr *e = lfirst(lc);

    result = lappend(result, ExecInitExpr(e, parent));
  }

  return result;
}

   
                            
   
                                                                           
                                                                            
                                                                 
   
                                                                           
                                                                       
                                                                              
                                                                             
                                
   
                                                                             
                               
   
                                                                           
                                                                               
   
ProjectionInfo *
ExecBuildProjectionInfo(List *targetList, ExprContext *econtext, TupleTableSlot *slot, PlanState *parent, TupleDesc inputDesc)
{
  return ExecBuildProjectionInfoExt(targetList, econtext, slot, true, parent, inputDesc);
}

   
                               
   
                                         
   
                                                                               
                                                                            
                                                                              
                                                                    
   
ProjectionInfo *
ExecBuildProjectionInfoExt(List *targetList, ExprContext *econtext, TupleTableSlot *slot, bool assignJunkEntries, PlanState *parent, TupleDesc inputDesc)
{
  ProjectionInfo *projInfo = makeNode(ProjectionInfo);
  ExprState *state;
  ExprEvalStep scratch = {0};
  ListCell *lc;

  projInfo->pi_exprContext = econtext;
                                                                            
  projInfo->pi_state.tag.type = T_ExprState;
  state = &projInfo->pi_state;
  state->expr = (Expr *)targetList;
  state->parent = parent;
  state->ext_params = NULL;

  state->resultslot = slot;

                                               
  ExecInitExprSlots(state, (Node *)targetList);

                                     
  foreach (lc, targetList)
  {
    TargetEntry *tle = lfirst_node(TargetEntry, lc);
    Var *variable = NULL;
    AttrNumber attnum = 0;
    bool isSafeVar = false;

       
                                                                       
                                                                       
                                                                          
                                                                          
                                                                         
                                                  
       
    if (tle->expr != NULL && IsA(tle->expr, Var) && ((Var *)tle->expr)->varattno > 0 && (assignJunkEntries || !tle->resjunk))
    {
                                               
      variable = (Var *)tle->expr;
      attnum = variable->varattno;

      if (inputDesc == NULL)
      {
        isSafeVar = true;                                  
      }
      else if (attnum <= inputDesc->natts)
      {
        Form_pg_attribute attr = TupleDescAttr(inputDesc, attnum - 1);

           
                                                                      
                                                                
                                                              
           
        if (!attr->attisdropped && variable->vartype == attr->atttypid)
        {
          isSafeVar = true;
        }
      }
    }

    if (isSafeVar)
    {
                                                              
      switch (variable->varno)
      {
      case INNER_VAR:
                                               
        scratch.opcode = EEOP_ASSIGN_INNER_VAR;
        break;

      case OUTER_VAR:
                                               
        scratch.opcode = EEOP_ASSIGN_OUTER_VAR;
        break;

                                                  

      default:
                                                           
        scratch.opcode = EEOP_ASSIGN_SCAN_VAR;
        break;
      }

      scratch.d.assign_var.attnum = attnum - 1;
      scratch.d.assign_var.resultnum = tle->resno - 1;
      ExprEvalPushStep(state, &scratch);
    }
    else
    {
         
                                                            
         
                                                                    
                                                                     
                                                                     
                                                              
         
      ExecInitExprRec(tle->expr, state, &state->resvalue, &state->resnull);

                                                                       
      if (!assignJunkEntries && tle->resjunk)
      {
        continue;
      }

         
                                                                      
                                                                         
         
      if (get_typlen(exprType((Node *)tle->expr)) == -1)
      {
        scratch.opcode = EEOP_ASSIGN_TMP_MAKE_RO;
      }
      else
      {
        scratch.opcode = EEOP_ASSIGN_TMP;
      }
      scratch.d.assign_tmp.resultnum = tle->resno - 1;
      ExprEvalPushStep(state, &scratch);
    }
  }

  scratch.opcode = EEOP_DONE;
  ExprEvalPushStep(state, &scratch);

  ExecReadyExpr(state);

  return projInfo;
}

   
                                                                            
                      
   
                                                                        
                                                                        
                                                                         
                                                                              
                                                                           
                                          
   
ExprState *
ExecPrepareExpr(Expr *node, EState *estate)
{
  ExprState *result;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  node = expression_planner(node);

  result = ExecInitExpr(node, NULL);

  MemoryContextSwitchTo(oldcontext);

  return result;
}

   
                                                                      
                      
   
                                                                        
                                                                        
                                                                         
                                                                              
                                                                           
                                          
   
ExprState *
ExecPrepareQual(List *qual, EState *estate)
{
  ExprState *result;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  qual = (List *)expression_planner((Expr *)qual);

  result = ExecInitQual(qual, NULL);

  MemoryContextSwitchTo(oldcontext);

  return result;
}

   
                                                                           
                             
   
                                                          
   
ExprState *
ExecPrepareCheck(List *qual, EState *estate)
{
  ExprState *result;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  qual = (List *)expression_planner((Expr *)qual);

  result = ExecInitCheck(qual, NULL);

  MemoryContextSwitchTo(oldcontext);

  return result;
}

   
                                                                        
                         
   
                                      
   
List *
ExecPrepareExprList(List *nodes, EState *estate)
{
  List *result = NIL;
  MemoryContext oldcontext;
  ListCell *lc;

                                                                    
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  foreach (lc, nodes)
  {
    Expr *e = (Expr *)lfirst(lc);

    result = lappend(result, ExecPrepareExpr(e, estate));
  }

  MemoryContextSwitchTo(oldcontext);

  return result;
}

   
                                           
   
                                                                            
           
   
                                                                  
                                                                        
                                                                          
                              
   
bool
ExecCheck(ExprState *state, ExprContext *econtext)
{
  Datum ret;
  bool isnull;

                                                                            
  if (state == NULL)
  {
    return true;
  }

                                                                  
  Assert(!(state->flags & EEO_FLAG_IS_QUAL));

  ret = ExecEvalExprSwitchContext(state, econtext, &isnull);

  if (isnull)
  {
    return true;
  }

  return DatumGetBool(ret);
}

   
                                                                           
                                              
   
                                                                   
                                                                           
                                                             
                               
   
static void
ExecReadyExpr(ExprState *state)
{
  if (jit_compile_expr(state))
  {
    return;
  }

  ExecReadyInterpretedExpr(state);
}

   
                                                                              
                                                    
   
                                 
                                                                         
                                                               
   
static void
ExecInitExprRec(Expr *node, ExprState *state, Datum *resv, bool *resnull)
{
  ExprEvalStep scratch = {0};

                                                                      
  check_stack_depth();

                                                                
  Assert(resv != NULL && resnull != NULL);
  scratch.resvalue = resv;
  scratch.resnull = resnull;

                                                           
  switch (nodeTag(node))
  {
  case T_Var:
  {
    Var *variable = (Var *)node;

    if (variable->varattno == InvalidAttrNumber)
    {
                         
      ExecInitWholeRowVar(&scratch, variable, state);
    }
    else if (variable->varattno <= 0)
    {
                         
      scratch.d.var.attnum = variable->varattno;
      scratch.d.var.vartype = variable->vartype;
      switch (variable->varno)
      {
      case INNER_VAR:
        scratch.opcode = EEOP_INNER_SYSVAR;
        break;
      case OUTER_VAR:
        scratch.opcode = EEOP_OUTER_SYSVAR;
        break;

                                                  

      default:
        scratch.opcode = EEOP_SCAN_SYSVAR;
        break;
      }
    }
    else
    {
                               
      scratch.d.var.attnum = variable->varattno - 1;
      scratch.d.var.vartype = variable->vartype;
      switch (variable->varno)
      {
      case INNER_VAR:
        scratch.opcode = EEOP_INNER_VAR;
        break;
      case OUTER_VAR:
        scratch.opcode = EEOP_OUTER_VAR;
        break;

                                                  

      default:
        scratch.opcode = EEOP_SCAN_VAR;
        break;
      }
    }

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_Const:
  {
    Const *con = (Const *)node;

    scratch.opcode = EEOP_CONST;
    scratch.d.constval.value = con->constvalue;
    scratch.d.constval.isnull = con->constisnull;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_Param:
  {
    Param *param = (Param *)node;
    ParamListInfo params;

    switch (param->paramkind)
    {
    case PARAM_EXEC:
      scratch.opcode = EEOP_PARAM_EXEC;
      scratch.d.param.paramid = param->paramid;
      scratch.d.param.paramtype = param->paramtype;
      ExprEvalPushStep(state, &scratch);
      break;
    case PARAM_EXTERN:

         
                                                         
                                                        
                                                          
                                                           
         
      if (state->ext_params)
      {
        params = state->ext_params;
      }
      else if (state->parent && state->parent->state)
      {
        params = state->parent->state->es_param_list_info;
      }
      else
      {
        params = NULL;
      }
      if (params && params->paramCompile)
      {
        params->paramCompile(params, param, state, resv, resnull);
      }
      else
      {
        scratch.opcode = EEOP_PARAM_EXTERN;
        scratch.d.param.paramid = param->paramid;
        scratch.d.param.paramtype = param->paramtype;
        ExprEvalPushStep(state, &scratch);
      }
      break;
    default:
      elog(ERROR, "unrecognized paramkind: %d", (int)param->paramkind);
      break;
    }
    break;
  }

  case T_Aggref:
  {
    Aggref *aggref = (Aggref *)node;
    AggrefExprState *astate = makeNode(AggrefExprState);

    scratch.opcode = EEOP_AGGREF;
    scratch.d.aggref.astate = astate;
    astate->aggref = aggref;

    if (state->parent && IsA(state->parent, AggState))
    {
      AggState *aggstate = (AggState *)state->parent;

      aggstate->aggs = lcons(astate, aggstate->aggs);
      aggstate->numaggs++;
    }
    else
    {
                             
      elog(ERROR, "Aggref found in non-Agg plan node");
    }

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_GroupingFunc:
  {
    GroupingFunc *grp_node = (GroupingFunc *)node;
    Agg *agg;

    if (!state->parent || !IsA(state->parent, AggState) || !IsA(state->parent->plan, Agg))
    {
      elog(ERROR, "GroupingFunc found in non-Agg plan node");
    }

    scratch.opcode = EEOP_GROUPING_FUNC;
    scratch.d.grouping_func.parent = (AggState *)state->parent;

    agg = (Agg *)(state->parent->plan);

    if (agg->groupingSets)
    {
      scratch.d.grouping_func.clauses = grp_node->cols;
    }
    else
    {
      scratch.d.grouping_func.clauses = NIL;
    }

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_WindowFunc:
  {
    WindowFunc *wfunc = (WindowFunc *)node;
    WindowFuncExprState *wfstate = makeNode(WindowFuncExprState);

    wfstate->wfunc = wfunc;

    if (state->parent && IsA(state->parent, WindowAggState))
    {
      WindowAggState *winstate = (WindowAggState *)state->parent;
      int nfuncs;

      winstate->funcs = lcons(wfstate, winstate->funcs);
      nfuncs = ++winstate->numfuncs;
      if (wfunc->winagg)
      {
        winstate->numaggs++;
      }

                                                              
      wfstate->args = ExecInitExprList(wfunc->args, state->parent);
      wfstate->aggfilter = ExecInitExpr(wfunc->aggfilter, state->parent);

         
                                                            
                                                               
                                                              
                                                
         
      if (nfuncs != winstate->numfuncs)
      {
        ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("window function calls cannot be nested")));
      }
    }
    else
    {
                             
      elog(ERROR, "WindowFunc found in non-WindowAgg plan node");
    }

    scratch.opcode = EEOP_WINDOW_FUNC;
    scratch.d.window_func.wfstate = wfstate;
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_SubscriptingRef:
  {
    SubscriptingRef *sbsref = (SubscriptingRef *)node;

    ExecInitSubscriptingRef(&scratch, sbsref, state, resv, resnull);
    break;
  }

  case T_FuncExpr:
  {
    FuncExpr *func = (FuncExpr *)node;

    ExecInitFunc(&scratch, node, func->args, func->funcid, func->inputcollid, state);
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_OpExpr:
  {
    OpExpr *op = (OpExpr *)node;

    ExecInitFunc(&scratch, node, op->args, op->opfuncid, op->inputcollid, state);
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_DistinctExpr:
  {
    DistinctExpr *op = (DistinctExpr *)node;

    ExecInitFunc(&scratch, node, op->args, op->opfuncid, op->inputcollid, state);

       
                                                           
       
                                                             
                                                                  
                                                                   
                                                               
                                  
       
    scratch.opcode = EEOP_DISTINCT;
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_NullIfExpr:
  {
    NullIfExpr *op = (NullIfExpr *)node;

    ExecInitFunc(&scratch, node, op->args, op->opfuncid, op->inputcollid, state);

       
                                                         
       
                                                             
                                                                  
                                                                   
                                                               
                                  
       
    scratch.opcode = EEOP_NULLIF;
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_ScalarArrayOpExpr:
  {
    ScalarArrayOpExpr *opexpr = (ScalarArrayOpExpr *)node;
    Expr *scalararg;
    Expr *arrayarg;
    FmgrInfo *finfo;
    FunctionCallInfo fcinfo;
    AclResult aclresult;

    Assert(list_length(opexpr->args) == 2);
    scalararg = (Expr *)linitial(opexpr->args);
    arrayarg = (Expr *)lsecond(opexpr->args);

                                           
    aclresult = pg_proc_aclcheck(opexpr->opfuncid, GetUserId(), ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(opexpr->opfuncid));
    }
    InvokeFunctionExecuteHook(opexpr->opfuncid);

                                                    
    finfo = palloc0(sizeof(FmgrInfo));
    fcinfo = palloc0(SizeForFunctionCallInfo(2));
    fmgr_info(opexpr->opfuncid, finfo);
    fmgr_info_set_expr((Node *)node, finfo);
    InitFunctionCallInfoData(*fcinfo, finfo, 2, opexpr->inputcollid, NULL, NULL);

                                                              
    ExecInitExprRec(scalararg, state, &fcinfo->args[0].value, &fcinfo->args[0].isnull);

       
                                                                  
                                                                 
                                                             
                                       
       
    ExecInitExprRec(arrayarg, state, resv, resnull);

                                   
    scratch.opcode = EEOP_SCALARARRAYOP;
    scratch.d.scalararrayop.element_type = InvalidOid;
    scratch.d.scalararrayop.useOr = opexpr->useOr;
    scratch.d.scalararrayop.finfo = finfo;
    scratch.d.scalararrayop.fcinfo_data = fcinfo;
    scratch.d.scalararrayop.fn_addr = finfo->fn_addr;
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_BoolExpr:
  {
    BoolExpr *boolexpr = (BoolExpr *)node;
    int nargs = list_length(boolexpr->args);
    List *adjust_jumps = NIL;
    int off;
    ListCell *lc;

                                                             
    if (boolexpr->boolop != NOT_EXPR)
    {
      scratch.d.boolexpr.anynull = (bool *)palloc(sizeof(bool));
    }

       
                                                            
                                                          
       
                                                                 
                                                                 
                             
       
                                                                   
                                                                 
                                                              
                                                   
       
    off = 0;
    foreach (lc, boolexpr->args)
    {
      Expr *arg = (Expr *)lfirst(lc);

                                                      
      ExecInitExprRec(arg, state, resv, resnull);

                                             
      switch (boolexpr->boolop)
      {
      case AND_EXPR:
        Assert(nargs >= 2);

        if (off == 0)
        {
          scratch.opcode = EEOP_BOOL_AND_STEP_FIRST;
        }
        else if (off + 1 == nargs)
        {
          scratch.opcode = EEOP_BOOL_AND_STEP_LAST;
        }
        else
        {
          scratch.opcode = EEOP_BOOL_AND_STEP;
        }
        break;
      case OR_EXPR:
        Assert(nargs >= 2);

        if (off == 0)
        {
          scratch.opcode = EEOP_BOOL_OR_STEP_FIRST;
        }
        else if (off + 1 == nargs)
        {
          scratch.opcode = EEOP_BOOL_OR_STEP_LAST;
        }
        else
        {
          scratch.opcode = EEOP_BOOL_OR_STEP;
        }
        break;
      case NOT_EXPR:
        Assert(nargs == 1);

        scratch.opcode = EEOP_BOOL_NOT_STEP;
        break;
      default:
        elog(ERROR, "unrecognized boolop: %d", (int)boolexpr->boolop);
        break;
      }

      scratch.d.boolexpr.jumpdone = -1;
      ExprEvalPushStep(state, &scratch);
      adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
      off++;
    }

                             
    foreach (lc, adjust_jumps)
    {
      ExprEvalStep *as = &state->steps[lfirst_int(lc)];

      Assert(as->d.boolexpr.jumpdone == -1);
      as->d.boolexpr.jumpdone = state->steps_len;
    }

    break;
  }

  case T_SubPlan:
  {
    SubPlan *subplan = (SubPlan *)node;
    SubPlanState *sstate;

    if (!state->parent)
    {
      elog(ERROR, "SubPlan found with no parent plan");
    }

    sstate = ExecInitSubPlan(subplan, state->parent);

                                                          
    state->parent->subPlan = lappend(state->parent->subPlan, sstate);

    scratch.opcode = EEOP_SUBPLAN;
    scratch.d.subplan.sstate = sstate;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_AlternativeSubPlan:
  {
    AlternativeSubPlan *asplan = (AlternativeSubPlan *)node;
    AlternativeSubPlanState *asstate;

    if (!state->parent)
    {
      elog(ERROR, "AlternativeSubPlan found with no parent plan");
    }

    asstate = ExecInitAlternativeSubPlan(asplan, state->parent);

    scratch.opcode = EEOP_ALTERNATIVE_SUBPLAN;
    scratch.d.alternative_subplan.asstate = asstate;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_FieldSelect:
  {
    FieldSelect *fselect = (FieldSelect *)node;

                                                       
    ExecInitExprRec(fselect->arg, state, resv, resnull);

                           
    scratch.opcode = EEOP_FIELDSELECT;
    scratch.d.fieldselect.fieldnum = fselect->fieldnum;
    scratch.d.fieldselect.resulttype = fselect->resulttype;
    scratch.d.fieldselect.rowcache.cacheptr = NULL;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_FieldStore:
  {
    FieldStore *fstore = (FieldStore *)node;
    TupleDesc tupDesc;
    ExprEvalRowtypeCache *rowcachep;
    Datum *values;
    bool *nulls;
    int ncolumns;
    ListCell *l1, *l2;

                                                              
    tupDesc = lookup_rowtype_tupdesc(fstore->resulttype, -1);
    ncolumns = tupDesc->natts;
    DecrTupleDescRefCount(tupDesc);

                                            
    values = (Datum *)palloc(sizeof(Datum) * ncolumns);
    nulls = (bool *)palloc(sizeof(bool) * ncolumns);

                                                          
    rowcachep = palloc(sizeof(ExprEvalRowtypeCache));
    rowcachep->cacheptr = NULL;

                                                         
    ExecInitExprRec(fstore->arg, state, resv, resnull);

                                                         
    scratch.opcode = EEOP_FIELDSTORE_DEFORM;
    scratch.d.fieldstore.fstore = fstore;
    scratch.d.fieldstore.rowcache = rowcachep;
    scratch.d.fieldstore.values = values;
    scratch.d.fieldstore.nulls = nulls;
    scratch.d.fieldstore.ncolumns = ncolumns;
    ExprEvalPushStep(state, &scratch);

                                                               
    forboth(l1, fstore->newvals, l2, fstore->fieldnums)
    {
      Expr *e = (Expr *)lfirst(l1);
      AttrNumber fieldnum = lfirst_int(l2);
      Datum *save_innermost_caseval;
      bool *save_innermost_casenull;

      if (fieldnum <= 0 || fieldnum > ncolumns)
      {
        elog(ERROR, "field number %d is out of range in FieldStore", fieldnum);
      }

         
                                                             
                                                              
                                                   
                                                               
                                                               
                                                                 
                                                                 
                                                  
                                                             
                  
         
                                                                
                                                              
                                                               
                                                          
                                                              
                                                              
                                                                
                                                               
                                                            
                                                               
                                              
         
      save_innermost_caseval = state->innermost_caseval;
      save_innermost_casenull = state->innermost_casenull;
      state->innermost_caseval = &values[fieldnum - 1];
      state->innermost_casenull = &nulls[fieldnum - 1];

      ExecInitExprRec(e, state, &values[fieldnum - 1], &nulls[fieldnum - 1]);

      state->innermost_caseval = save_innermost_caseval;
      state->innermost_casenull = save_innermost_casenull;
    }

                                    
    scratch.opcode = EEOP_FIELDSTORE_FORM;
    scratch.d.fieldstore.fstore = fstore;
    scratch.d.fieldstore.rowcache = rowcachep;
    scratch.d.fieldstore.values = values;
    scratch.d.fieldstore.nulls = nulls;
    scratch.d.fieldstore.ncolumns = ncolumns;
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_RelabelType:
  {
                                                        
    RelabelType *relabel = (RelabelType *)node;

    ExecInitExprRec(relabel->arg, state, resv, resnull);
    break;
  }

  case T_CoerceViaIO:
  {
    CoerceViaIO *iocoerce = (CoerceViaIO *)node;
    Oid iofunc;
    bool typisvarlena;
    Oid typioparam;
    FunctionCallInfo fcinfo_in;

                                                   
    ExecInitExprRec(iocoerce->arg, state, resv, resnull);

       
                                                           
                                                                  
                                       
       
                                                                
                                                          
       
    scratch.opcode = EEOP_IOCOERCE;

                                                  
    scratch.d.iocoerce.finfo_out = palloc0(sizeof(FmgrInfo));
    scratch.d.iocoerce.fcinfo_data_out = palloc0(SizeForFunctionCallInfo(1));

    getTypeOutputInfo(exprType((Node *)iocoerce->arg), &iofunc, &typisvarlena);
    fmgr_info(iofunc, scratch.d.iocoerce.finfo_out);
    fmgr_info_set_expr((Node *)node, scratch.d.iocoerce.finfo_out);
    InitFunctionCallInfoData(*scratch.d.iocoerce.fcinfo_data_out, scratch.d.iocoerce.finfo_out, 1, InvalidOid, NULL, NULL);

                                                 
    scratch.d.iocoerce.finfo_in = palloc0(sizeof(FmgrInfo));
    scratch.d.iocoerce.fcinfo_data_in = palloc0(SizeForFunctionCallInfo(3));

    getTypeInputInfo(iocoerce->resulttype, &iofunc, &typioparam);
    fmgr_info(iofunc, scratch.d.iocoerce.finfo_in);
    fmgr_info_set_expr((Node *)node, scratch.d.iocoerce.finfo_in);
    InitFunctionCallInfoData(*scratch.d.iocoerce.fcinfo_data_in, scratch.d.iocoerce.finfo_in, 3, InvalidOid, NULL, NULL);

       
                                                                   
                                          
       
    fcinfo_in = scratch.d.iocoerce.fcinfo_data_in;
    fcinfo_in->args[1].value = ObjectIdGetDatum(typioparam);
    fcinfo_in->args[1].isnull = false;
    fcinfo_in->args[2].value = Int32GetDatum(-1);
    fcinfo_in->args[2].isnull = false;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_ArrayCoerceExpr:
  {
    ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;
    Oid resultelemtype;
    ExprState *elemstate;

                                                   
    ExecInitExprRec(acoerce->arg, state, resv, resnull);

    resultelemtype = get_element_type(acoerce->resulttype);
    if (!OidIsValid(resultelemtype))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("target type is not an array")));
    }

       
                                                                  
                                                                  
                                                               
                                                                  
       
    elemstate = makeNode(ExprState);
    elemstate->expr = acoerce->elemexpr;
    elemstate->parent = state->parent;
    elemstate->ext_params = state->ext_params;

    elemstate->innermost_caseval = (Datum *)palloc(sizeof(Datum));
    elemstate->innermost_casenull = (bool *)palloc(sizeof(bool));

    ExecInitExprRec(acoerce->elemexpr, elemstate, &elemstate->resvalue, &elemstate->resnull);

    if (elemstate->steps_len == 1 && elemstate->steps[0].opcode == EEOP_CASE_TESTVAL)
    {
                                                              
      elemstate = NULL;
    }
    else
    {
                                              
      scratch.opcode = EEOP_DONE;
      ExprEvalPushStep(elemstate, &scratch);
                                       
      ExecReadyExpr(elemstate);
    }

    scratch.opcode = EEOP_ARRAYCOERCE;
    scratch.d.arraycoerce.elemexprstate = elemstate;
    scratch.d.arraycoerce.resultelemtype = resultelemtype;

    if (elemstate)
    {
                                          
      scratch.d.arraycoerce.amstate = (ArrayMapState *)palloc0(sizeof(ArrayMapState));
    }
    else
    {
                                                            
      scratch.d.arraycoerce.amstate = NULL;
    }

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_ConvertRowtypeExpr:
  {
    ConvertRowtypeExpr *convert = (ConvertRowtypeExpr *)node;
    ExprEvalRowtypeCache *rowcachep;

                                                             
    rowcachep = palloc(2 * sizeof(ExprEvalRowtypeCache));
    rowcachep[0].cacheptr = NULL;
    rowcachep[1].cacheptr = NULL;

                                                   
    ExecInitExprRec(convert->arg, state, resv, resnull);

                                  
    scratch.opcode = EEOP_CONVERT_ROWTYPE;
    scratch.d.convert_rowtype.inputtype = exprType((Node *)convert->arg);
    scratch.d.convert_rowtype.outputtype = convert->resulttype;
    scratch.d.convert_rowtype.incache = &rowcachep[0];
    scratch.d.convert_rowtype.outcache = &rowcachep[1];
    scratch.d.convert_rowtype.map = NULL;

    ExprEvalPushStep(state, &scratch);
    break;
  }

                                                                      
  case T_CaseExpr:
  {
    CaseExpr *caseExpr = (CaseExpr *)node;
    List *adjust_jumps = NIL;
    Datum *caseval = NULL;
    bool *casenull = NULL;
    ListCell *lc;

       
                                                                
                                                                   
           
       
    if (caseExpr->arg != NULL)
    {
                                                             
      caseval = palloc(sizeof(Datum));
      casenull = palloc(sizeof(bool));

      ExecInitExprRec(caseExpr->arg, state, caseval, casenull);

         
                                                                
                                                      
         
      if (get_typlen(exprType((Node *)caseExpr->arg)) == -1)
      {
                                     
        scratch.opcode = EEOP_MAKE_READONLY;
        scratch.resvalue = caseval;
        scratch.resnull = casenull;
        scratch.d.make_readonly.value = caseval;
        scratch.d.make_readonly.isnull = casenull;
        ExprEvalPushStep(state, &scratch);
                                                       
        scratch.resvalue = resv;
        scratch.resnull = resnull;
      }
    }

       
                                                                
                                                      
                                                                   
                                                               
       
    foreach (lc, caseExpr->args)
    {
      CaseWhen *when = (CaseWhen *)lfirst(lc);
      Datum *save_innermost_caseval;
      bool *save_innermost_casenull;
      int whenstep;

         
                                                              
                                                               
                                                                
                                         
         
                                                               
                                                                 
                                     
         
      save_innermost_caseval = state->innermost_caseval;
      save_innermost_casenull = state->innermost_casenull;
      state->innermost_caseval = caseval;
      state->innermost_casenull = casenull;

                                                           
      ExecInitExprRec(when->expr, state, resv, resnull);

      state->innermost_caseval = save_innermost_caseval;
      state->innermost_casenull = save_innermost_casenull;

                                                            
      scratch.opcode = EEOP_JUMP_IF_NOT_TRUE;
      scratch.d.jump.jumpdone = -1;                     
      ExprEvalPushStep(state, &scratch);
      whenstep = state->steps_len - 1;

         
                                                               
                                              
         
      ExecInitExprRec(when->result, state, resv, resnull);

                                                        
      scratch.opcode = EEOP_JUMP;
      scratch.d.jump.jumpdone = -1;                     
      ExprEvalPushStep(state, &scratch);

         
                                                                
                                         
         
      adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);

         
                                                                
                                                          
         
      state->steps[whenstep].d.jump.jumpdone = state->steps_len;
    }

                                                 
    Assert(caseExpr->defresult);

                                                         
    ExecInitExprRec(caseExpr->defresult, state, resv, resnull);

                             
    foreach (lc, adjust_jumps)
    {
      ExprEvalStep *as = &state->steps[lfirst_int(lc)];

      Assert(as->opcode == EEOP_JUMP);
      Assert(as->d.jump.jumpdone == -1);
      as->d.jump.jumpdone = state->steps_len;
    }

    break;
  }

  case T_CaseTestExpr:
  {
       
                                                                 
                                                                
                                                                   
                                                              
                                                                   
                                                              
                            
       
    scratch.opcode = EEOP_CASE_TESTVAL;
    scratch.d.casetest.value = state->innermost_caseval;
    scratch.d.casetest.isnull = state->innermost_casenull;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_ArrayExpr:
  {
    ArrayExpr *arrayexpr = (ArrayExpr *)node;
    int nelems = list_length(arrayexpr->elements);
    ListCell *lc;
    int elemoff;

       
                                                                
                                                         
                                           
       
    scratch.opcode = EEOP_ARRAYEXPR;
    scratch.d.arrayexpr.elemvalues = (Datum *)palloc(sizeof(Datum) * nelems);
    scratch.d.arrayexpr.elemnulls = (bool *)palloc(sizeof(bool) * nelems);
    scratch.d.arrayexpr.nelems = nelems;

                                       
    scratch.d.arrayexpr.multidims = arrayexpr->multidims;
    scratch.d.arrayexpr.elemtype = arrayexpr->element_typeid;

                                                  
    get_typlenbyvalalign(arrayexpr->element_typeid, &scratch.d.arrayexpr.elemlength, &scratch.d.arrayexpr.elembyval, &scratch.d.arrayexpr.elemalign);

                                           
    elemoff = 0;
    foreach (lc, arrayexpr->elements)
    {
      Expr *e = (Expr *)lfirst(lc);

      ExecInitExprRec(e, state, &scratch.d.arrayexpr.elemvalues[elemoff], &scratch.d.arrayexpr.elemnulls[elemoff]);
      elemoff++;
    }

                                            
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_RowExpr:
  {
    RowExpr *rowexpr = (RowExpr *)node;
    int nelems = list_length(rowexpr->args);
    TupleDesc tupdesc;
    int i;
    ListCell *l;

                                                 
    if (rowexpr->row_typeid == RECORDOID)
    {
                                                          
      tupdesc = ExecTypeFromExprList(rowexpr->args);
                                                  
      ExecTypeSetColNames(tupdesc, rowexpr->colnames);
                                                          
      BlessTupleDesc(tupdesc);
    }
    else
    {
                                                    
      tupdesc = lookup_rowtype_tupdesc_copy(rowexpr->row_typeid, -1);
    }

       
                                                                   
                                                                
                                                                
                                                              
                                                                 
                                       
       
    Assert(nelems <= tupdesc->natts);
    nelems = Max(nelems, tupdesc->natts);

       
                                                                  
                                                 
       
    scratch.opcode = EEOP_ROW;
    scratch.d.row.tupdesc = tupdesc;

                                               
    scratch.d.row.elemvalues = (Datum *)palloc(sizeof(Datum) * nelems);
    scratch.d.row.elemnulls = (bool *)palloc(sizeof(bool) * nelems);
                                                                  
    memset(scratch.d.row.elemnulls, true, sizeof(bool) * nelems);

                                                         
    i = 0;
    foreach (l, rowexpr->args)
    {
      Form_pg_attribute att = TupleDescAttr(tupdesc, i);
      Expr *e = (Expr *)lfirst(l);

      if (!att->attisdropped)
      {
           
                                                            
                                                         
                                                            
                 
           
        if (exprType((Node *)e) != att->atttypid)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("ROW() column has type %s instead of type %s", format_type_be(exprType((Node *)e)), format_type_be(att->atttypid))));
        }
      }
      else
      {
           
                                                            
                                                         
                                     
           
        e = (Expr *)makeNullConst(INT4OID, -1, InvalidOid);
      }

                                                                
      ExecInitExprRec(e, state, &scratch.d.row.elemvalues[i], &scratch.d.row.elemnulls[i]);
      i++;
    }

                                         
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_RowCompareExpr:
  {
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;
    int nopers = list_length(rcexpr->opnos);
    List *adjust_jumps = NIL;
    ListCell *l_left_expr, *l_right_expr, *l_opno, *l_opfamily, *l_inputcollid;
    ListCell *lc;

       
                                                                
                                                                  
                                                                
       
    Assert(list_length(rcexpr->largs) == nopers);
    Assert(list_length(rcexpr->rargs) == nopers);
    Assert(list_length(rcexpr->opfamilies) == nopers);
    Assert(list_length(rcexpr->inputcollids) == nopers);

    forfive(l_left_expr, rcexpr->largs, l_right_expr, rcexpr->rargs, l_opno, rcexpr->opnos, l_opfamily, rcexpr->opfamilies, l_inputcollid, rcexpr->inputcollids)
    {
      Expr *left_expr = (Expr *)lfirst(l_left_expr);
      Expr *right_expr = (Expr *)lfirst(l_right_expr);
      Oid opno = lfirst_oid(l_opno);
      Oid opfamily = lfirst_oid(l_opfamily);
      Oid inputcollid = lfirst_oid(l_inputcollid);
      int strategy;
      Oid lefttype;
      Oid righttype;
      Oid proc;
      FmgrInfo *finfo;
      FunctionCallInfo fcinfo;

      get_op_opfamily_properties(opno, opfamily, false, &strategy, &lefttype, &righttype);
      proc = get_opfamily_proc(opfamily, lefttype, righttype, BTORDER_PROC);
      if (!OidIsValid(proc))
      {
        elog(ERROR, "missing support function %d(%u,%u) in opfamily %u", BTORDER_PROC, lefttype, righttype, opfamily);
      }

                                                      
      finfo = palloc0(sizeof(FmgrInfo));
      fcinfo = palloc0(SizeForFunctionCallInfo(2));
      fmgr_info(proc, finfo);
      fmgr_info_set_expr((Node *)node, finfo);
      InitFunctionCallInfoData(*fcinfo, finfo, 2, inputcollid, NULL, NULL);

         
                                                            
                                                             
                                                           
                                 
         

                                                             
      ExecInitExprRec(left_expr, state, &fcinfo->args[0].value, &fcinfo->args[0].isnull);
      ExecInitExprRec(right_expr, state, &fcinfo->args[1].value, &fcinfo->args[1].isnull);

      scratch.opcode = EEOP_ROWCOMPARE_STEP;
      scratch.d.rowcompare_step.finfo = finfo;
      scratch.d.rowcompare_step.fcinfo_data = fcinfo;
      scratch.d.rowcompare_step.fn_addr = finfo->fn_addr;
                                     
      scratch.d.rowcompare_step.jumpnull = -1;
      scratch.d.rowcompare_step.jumpdone = -1;

      ExprEvalPushStep(state, &scratch);
      adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
    }

       
                                                                   
                                  
       
    if (nopers == 0)
    {
      scratch.opcode = EEOP_CONST;
      scratch.d.constval.value = Int32GetDatum(0);
      scratch.d.constval.isnull = false;
      ExprEvalPushStep(state, &scratch);
    }

                                                     
    scratch.opcode = EEOP_ROWCOMPARE_FINAL;
    scratch.d.rowcompare_final.rctype = rcexpr->rctype;
    ExprEvalPushStep(state, &scratch);

                              
    foreach (lc, adjust_jumps)
    {
      ExprEvalStep *as = &state->steps[lfirst_int(lc)];

      Assert(as->opcode == EEOP_ROWCOMPARE_STEP);
      Assert(as->d.rowcompare_step.jumpdone == -1);
      Assert(as->d.rowcompare_step.jumpnull == -1);

                                         
      as->d.rowcompare_step.jumpdone = state->steps_len - 1;
                                            
      as->d.rowcompare_step.jumpnull = state->steps_len;
    }

    break;
  }

  case T_CoalesceExpr:
  {
    CoalesceExpr *coalesce = (CoalesceExpr *)node;
    List *adjust_jumps = NIL;
    ListCell *lc;

                                            
    Assert(coalesce->args != NIL);

       
                                                                 
                                                        
       
    foreach (lc, coalesce->args)
    {
      Expr *e = (Expr *)lfirst(lc);

                                                         
      ExecInitExprRec(e, state, resv, resnull);

                                                          
      scratch.opcode = EEOP_JUMP_IF_NOT_NULL;
      scratch.d.jump.jumpdone = -1;                   
      ExprEvalPushStep(state, &scratch);

      adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
    }

       
                                                                  
                                                            
                 
       

                             
    foreach (lc, adjust_jumps)
    {
      ExprEvalStep *as = &state->steps[lfirst_int(lc)];

      Assert(as->opcode == EEOP_JUMP_IF_NOT_NULL);
      Assert(as->d.jump.jumpdone == -1);
      as->d.jump.jumpdone = state->steps_len;
    }

    break;
  }

  case T_MinMaxExpr:
  {
    MinMaxExpr *minmaxexpr = (MinMaxExpr *)node;
    int nelems = list_length(minmaxexpr->args);
    TypeCacheEntry *typentry;
    FmgrInfo *finfo;
    FunctionCallInfo fcinfo;
    ListCell *lc;
    int off;

                                                                
    typentry = lookup_type_cache(minmaxexpr->minmaxtype, TYPECACHE_CMP_PROC);
    if (!OidIsValid(typentry->cmp_proc))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify a comparison function for type %s", format_type_be(minmaxexpr->minmaxtype))));
    }

       
                                                          
                                                                 
                                                                
                  
       

                                 
    finfo = palloc0(sizeof(FmgrInfo));
    fcinfo = palloc0(SizeForFunctionCallInfo(2));
    fmgr_info(typentry->cmp_proc, finfo);
    fmgr_info_set_expr((Node *)node, finfo);
    InitFunctionCallInfoData(*fcinfo, finfo, 2, minmaxexpr->inputcollid, NULL, NULL);

    scratch.opcode = EEOP_MINMAX;
                                           
    scratch.d.minmax.values = (Datum *)palloc(sizeof(Datum) * nelems);
    scratch.d.minmax.nulls = (bool *)palloc(sizeof(bool) * nelems);
    scratch.d.minmax.nelems = nelems;

    scratch.d.minmax.op = minmaxexpr->op;
    scratch.d.minmax.finfo = finfo;
    scratch.d.minmax.fcinfo_data = fcinfo;

                                                        
    off = 0;
    foreach (lc, minmaxexpr->args)
    {
      Expr *e = (Expr *)lfirst(lc);

      ExecInitExprRec(e, state, &scratch.d.minmax.values[off], &scratch.d.minmax.nulls[off]);
      off++;
    }

                                       
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_SQLValueFunction:
  {
    SQLValueFunction *svf = (SQLValueFunction *)node;

    scratch.opcode = EEOP_SQLVALUEFUNCTION;
    scratch.d.sqlvaluefunction.svf = svf;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_XmlExpr:
  {
    XmlExpr *xexpr = (XmlExpr *)node;
    int nnamed = list_length(xexpr->named_args);
    int nargs = list_length(xexpr->args);
    int off;
    ListCell *arg;

    scratch.opcode = EEOP_XMLEXPR;
    scratch.d.xmlexpr.xexpr = xexpr;

                                                      
    if (nnamed)
    {
      scratch.d.xmlexpr.named_argvalue = (Datum *)palloc(sizeof(Datum) * nnamed);
      scratch.d.xmlexpr.named_argnull = (bool *)palloc(sizeof(bool) * nnamed);
    }
    else
    {
      scratch.d.xmlexpr.named_argvalue = NULL;
      scratch.d.xmlexpr.named_argnull = NULL;
    }

    if (nargs)
    {
      scratch.d.xmlexpr.argvalue = (Datum *)palloc(sizeof(Datum) * nargs);
      scratch.d.xmlexpr.argnull = (bool *)palloc(sizeof(bool) * nargs);
    }
    else
    {
      scratch.d.xmlexpr.argvalue = NULL;
      scratch.d.xmlexpr.argnull = NULL;
    }

                                    
    off = 0;
    foreach (arg, xexpr->named_args)
    {
      Expr *e = (Expr *)lfirst(arg);

      ExecInitExprRec(e, state, &scratch.d.xmlexpr.named_argvalue[off], &scratch.d.xmlexpr.named_argnull[off]);
      off++;
    }

    off = 0;
    foreach (arg, xexpr->args)
    {
      Expr *e = (Expr *)lfirst(arg);

      ExecInitExprRec(e, state, &scratch.d.xmlexpr.argvalue[off], &scratch.d.xmlexpr.argnull[off]);
      off++;
    }

                                                
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_NullTest:
  {
    NullTest *ntest = (NullTest *)node;

    if (ntest->nulltesttype == IS_NULL)
    {
      if (ntest->argisrow)
      {
        scratch.opcode = EEOP_NULLTEST_ROWISNULL;
      }
      else
      {
        scratch.opcode = EEOP_NULLTEST_ISNULL;
      }
    }
    else if (ntest->nulltesttype == IS_NOT_NULL)
    {
      if (ntest->argisrow)
      {
        scratch.opcode = EEOP_NULLTEST_ROWISNOTNULL;
      }
      else
      {
        scratch.opcode = EEOP_NULLTEST_ISNOTNULL;
      }
    }
    else
    {
      elog(ERROR, "unrecognized nulltesttype: %d", (int)ntest->nulltesttype);
    }
                                                  
    scratch.d.nulltest_row.rowcache.cacheptr = NULL;

                                                      
    ExecInitExprRec(ntest->arg, state, resv, resnull);

                                             
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_BooleanTest:
  {
    BooleanTest *btest = (BooleanTest *)node;

       
                                                                  
                                                                  
                                                                
             
       
    ExecInitExprRec(btest->arg, state, resv, resnull);

    switch (btest->booltesttype)
    {
    case IS_TRUE:
      scratch.opcode = EEOP_BOOLTEST_IS_TRUE;
      break;
    case IS_NOT_TRUE:
      scratch.opcode = EEOP_BOOLTEST_IS_NOT_TRUE;
      break;
    case IS_FALSE:
      scratch.opcode = EEOP_BOOLTEST_IS_FALSE;
      break;
    case IS_NOT_FALSE:
      scratch.opcode = EEOP_BOOLTEST_IS_NOT_FALSE;
      break;
    case IS_UNKNOWN:
                                       
      scratch.opcode = EEOP_NULLTEST_ISNULL;
      break;
    case IS_NOT_UNKNOWN:
                                           
      scratch.opcode = EEOP_NULLTEST_ISNOTNULL;
      break;
    default:
      elog(ERROR, "unrecognized booltesttype: %d", (int)btest->booltesttype);
    }

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_CoerceToDomain:
  {
    CoerceToDomain *ctest = (CoerceToDomain *)node;

    ExecInitCoerceToDomain(&scratch, ctest, state, resv, resnull);
    break;
  }

  case T_CoerceToDomainValue:
  {
       
                                                                   
                                                                  
                                                               
                                                          
                                                             
                            
       
    scratch.opcode = EEOP_DOMAIN_TESTVAL;
                                                              
    scratch.d.casetest.value = state->innermost_domainval;
    scratch.d.casetest.isnull = state->innermost_domainnull;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_CurrentOfExpr:
  {
    scratch.opcode = EEOP_CURRENTOFEXPR;
    ExprEvalPushStep(state, &scratch);
    break;
  }

  case T_NextValueExpr:
  {
    NextValueExpr *nve = (NextValueExpr *)node;

    scratch.opcode = EEOP_NEXTVALUEEXPR;
    scratch.d.nextvalueexpr.seqid = nve->seqid;
    scratch.d.nextvalueexpr.seqtypid = nve->typeId;

    ExprEvalPushStep(state, &scratch);
    break;
  }

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
}

   
                                                               
   
                                                                           
                                                                          
   
void
ExprEvalPushStep(ExprState *es, const ExprEvalStep *s)
{
  if (es->steps_alloc == 0)
  {
    es->steps_alloc = 16;
    es->steps = palloc(sizeof(ExprEvalStep) * es->steps_alloc);
  }
  else if (es->steps_alloc == es->steps_len)
  {
    es->steps_alloc *= 2;
    es->steps = repalloc(es->steps, sizeof(ExprEvalStep) * es->steps_alloc);
  }

  memcpy(&es->steps[es->steps_len++], s, sizeof(ExprEvalStep));
}

   
                                                                             
                                                                        
                                                    
   
                                                                         
                                                          
   
static void
ExecInitFunc(ExprEvalStep *scratch, Expr *node, List *args, Oid funcid, Oid inputcollid, ExprState *state)
{
  int nargs = list_length(args);
  AclResult aclresult;
  FmgrInfo *flinfo;
  FunctionCallInfo fcinfo;
  int argno;
  ListCell *lc;

                                         
  aclresult = pg_proc_aclcheck(funcid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(funcid));
  }
  InvokeFunctionExecuteHook(funcid);

     
                                                                          
                                                                         
                                                                             
                          
     
  if (nargs > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("cannot pass more than %d argument to a function", "cannot pass more than %d arguments to a function", FUNC_MAX_ARGS, FUNC_MAX_ARGS)));
  }

                                                                           
  scratch->d.func.finfo = palloc0(sizeof(FmgrInfo));
  scratch->d.func.fcinfo_data = palloc0(SizeForFunctionCallInfo(nargs));
  flinfo = scratch->d.func.finfo;
  fcinfo = scratch->d.func.fcinfo_data;

                                                  
  fmgr_info(funcid, flinfo);
  fmgr_info_set_expr((Node *)node, flinfo);

                                                        
  InitFunctionCallInfoData(*fcinfo, flinfo, nargs, inputcollid, NULL, NULL);

                                                                        
  scratch->d.func.fn_addr = flinfo->fn_addr;
  scratch->d.func.nargs = nargs;

                                              
  if (flinfo->fn_retset)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set"), state->parent ? executor_errposition(state->parent->state, exprLocation((Node *)node)) : 0));
  }

                                                                        
  argno = 0;
  foreach (lc, args)
  {
    Expr *arg = (Expr *)lfirst(lc);

    if (IsA(arg, Const))
    {
         
                                                                
                                                   
         
      Const *con = (Const *)arg;

      fcinfo->args[argno].value = con->constvalue;
      fcinfo->args[argno].isnull = con->constisnull;
    }
    else
    {
      ExecInitExprRec(arg, state, &fcinfo->args[argno].value, &fcinfo->args[argno].isnull);
    }
    argno++;
  }

                                                                         
  if (pgstat_track_functions <= flinfo->fn_stats)
  {
    if (flinfo->fn_strict && nargs > 0)
    {
      scratch->opcode = EEOP_FUNCEXPR_STRICT;
    }
    else
    {
      scratch->opcode = EEOP_FUNCEXPR;
    }
  }
  else
  {
    if (flinfo->fn_strict && nargs > 0)
    {
      scratch->opcode = EEOP_FUNCEXPR_STRICT_FUSAGE;
    }
    else
    {
      scratch->opcode = EEOP_FUNCEXPR_FUSAGE;
    }
  }
}

   
                                                                         
                                          
   
static void
ExecInitExprSlots(ExprState *state, Node *node)
{
  LastAttnumInfo info = {0, 0, 0};

     
                                                      
     
  get_last_attnums_walker(node, &info);

  ExecPushExprSlots(state, &info);
}

   
                                                                       
                                                                              
                        
   
static void
ExecPushExprSlots(ExprState *state, LastAttnumInfo *info)
{
  ExprEvalStep scratch = {0};

  scratch.resvalue = NULL;
  scratch.resnull = NULL;

                            
  if (info->last_inner > 0)
  {
    scratch.opcode = EEOP_INNER_FETCHSOME;
    scratch.d.fetch.last_var = info->last_inner;
    scratch.d.fetch.fixed = false;
    scratch.d.fetch.kind = NULL;
    scratch.d.fetch.known_desc = NULL;
    ExecComputeSlotInfo(state, &scratch);
    ExprEvalPushStep(state, &scratch);
  }
  if (info->last_outer > 0)
  {
    scratch.opcode = EEOP_OUTER_FETCHSOME;
    scratch.d.fetch.last_var = info->last_outer;
    scratch.d.fetch.fixed = false;
    scratch.d.fetch.kind = NULL;
    scratch.d.fetch.known_desc = NULL;
    ExecComputeSlotInfo(state, &scratch);
    ExprEvalPushStep(state, &scratch);
  }
  if (info->last_scan > 0)
  {
    scratch.opcode = EEOP_SCAN_FETCHSOME;
    scratch.d.fetch.last_var = info->last_scan;
    scratch.d.fetch.fixed = false;
    scratch.d.fetch.kind = NULL;
    scratch.d.fetch.known_desc = NULL;
    ExecComputeSlotInfo(state, &scratch);
    ExprEvalPushStep(state, &scratch);
  }
}

   
                                                                    
   
static bool
get_last_attnums_walker(Node *node, LastAttnumInfo *info)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *variable = (Var *)node;
    AttrNumber attnum = variable->varattno;

    switch (variable->varno)
    {
    case INNER_VAR:
      info->last_inner = Max(info->last_inner, attnum);
      break;

    case OUTER_VAR:
      info->last_outer = Max(info->last_outer, attnum);
      break;

                                                

    default:
      info->last_scan = Max(info->last_scan, attnum);
      break;
    }
    return false;
  }

     
                                                                       
                                                                           
                                                                      
                       
     
  if (IsA(node, Aggref))
  {
    return false;
  }
  if (IsA(node, WindowFunc))
  {
    return false;
  }
  if (IsA(node, GroupingFunc))
  {
    return false;
  }
  return expression_tree_walker(node, get_last_attnums_walker, (void *)info);
}

   
                                                            
   
                                                                      
                                                                         
                          
   
static void
ExecComputeSlotInfo(ExprState *state, ExprEvalStep *op)
{
  PlanState *parent = state->parent;
  TupleDesc desc = NULL;
  const TupleTableSlotOps *tts_ops = NULL;
  bool isfixed = false;

  if (op->d.fetch.known_desc != NULL)
  {
    desc = op->d.fetch.known_desc;
    tts_ops = op->d.fetch.kind;
    isfixed = op->d.fetch.kind != NULL;
  }
  else if (!parent)
  {
    isfixed = false;
  }
  else if (op->opcode == EEOP_INNER_FETCHSOME)
  {
    PlanState *is = innerPlanState(parent);

    if (parent->inneropsset && !parent->inneropsfixed)
    {
      isfixed = false;
    }
    else if (parent->inneropsset && parent->innerops)
    {
      isfixed = true;
      tts_ops = parent->innerops;
      desc = ExecGetResultType(is);
    }
    else if (is)
    {
      tts_ops = ExecGetResultSlotOps(is, &isfixed);
      desc = ExecGetResultType(is);
    }
  }
  else if (op->opcode == EEOP_OUTER_FETCHSOME)
  {
    PlanState *os = outerPlanState(parent);

    if (parent->outeropsset && !parent->outeropsfixed)
    {
      isfixed = false;
    }
    else if (parent->outeropsset && parent->outerops)
    {
      isfixed = true;
      tts_ops = parent->outerops;
      desc = ExecGetResultType(os);
    }
    else if (os)
    {
      tts_ops = ExecGetResultSlotOps(os, &isfixed);
      desc = ExecGetResultType(os);
    }
  }
  else if (op->opcode == EEOP_SCAN_FETCHSOME)
  {
    desc = parent->scandesc;

    if (parent && parent->scanops)
    {
      tts_ops = parent->scanops;
    }

    if (parent->scanopsset)
    {
      isfixed = parent->scanopsfixed;
    }
  }

  if (isfixed && desc != NULL && tts_ops != NULL)
  {
    op->d.fetch.fixed = true;
    op->d.fetch.kind = tts_ops;
    op->d.fetch.known_desc = desc;
  }
  else
  {
    op->d.fetch.fixed = false;
    op->d.fetch.kind = NULL;
    op->d.fetch.known_desc = NULL;
  }
}

   
                                                            
                                          
   
static void
ExecInitWholeRowVar(ExprEvalStep *scratch, Var *variable, ExprState *state)
{
  PlanState *parent = state->parent;

                                  
  scratch->opcode = EEOP_WHOLEROW;
  scratch->d.wholerow.var = variable;
  scratch->d.wholerow.first = true;
  scratch->d.wholerow.slow = false;
  scratch->d.wholerow.tupdesc = NULL;                        
  scratch->d.wholerow.junkFilter = NULL;

     
                                                                         
                                                                            
                                                                      
                                                                            
                                                                             
                                                                            
                                                                           
                                                                           
                                                                        
                  
     
  if (parent)
  {
    PlanState *subplan = NULL;

    switch (nodeTag(parent))
    {
    case T_SubqueryScanState:
      subplan = ((SubqueryScanState *)parent)->subplan;
      break;
    case T_CteScanState:
      subplan = ((CteScanState *)parent)->cteplanstate;
      break;
    default:
      break;
    }

    if (subplan)
    {
      bool junk_filter_needed = false;
      ListCell *tlist;

                                                                      
      foreach (tlist, subplan->plan->targetlist)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(tlist);

        if (tle->resjunk)
        {
          junk_filter_needed = true;
          break;
        }
      }

                                           
      if (junk_filter_needed)
      {
        scratch->d.wholerow.junkFilter = ExecInitJunkFilter(subplan->plan->targetlist, ExecInitExtraTupleSlot(parent->state, NULL, &TTSOpsVirtual));
      }
    }
  }
}

   
                                                       
   
static void
ExecInitSubscriptingRef(ExprEvalStep *scratch, SubscriptingRef *sbsref, ExprState *state, Datum *resv, bool *resnull)
{
  bool isAssignment = (sbsref->refassgnexpr != NULL);
  SubscriptingRefState *sbsrefstate = palloc0(sizeof(SubscriptingRefState));
  List *adjust_jumps = NIL;
  ListCell *lc;
  int i;

                                                    
  sbsrefstate->isassignment = isAssignment;
  sbsrefstate->refelemtype = sbsref->refelemtype;
  sbsrefstate->refattrlength = get_typlen(sbsref->refcontainertype);
  get_typlenbyvalalign(sbsref->refelemtype, &sbsrefstate->refelemlength, &sbsrefstate->refelembyval, &sbsrefstate->refelemalign);

     
                                                                             
                                                                             
                                                                         
                  
     
  ExecInitExprRec(sbsref->refexpr, state, resv, resnull);

     
                                                                            
                                                                         
                                       
     
  if (!isAssignment)
  {
    scratch->opcode = EEOP_JUMP_IF_NULL;
    scratch->d.jump.jumpdone = -1;                   
    ExprEvalPushStep(state, scratch);
    adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
  }

                                                      
  if (list_length(sbsref->refupperindexpr) > MAXDIM)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("number of array dimensions (%d) exceeds the maximum allowed (%d)", list_length(sbsref->refupperindexpr), MAXDIM)));
  }

  if (list_length(sbsref->reflowerindexpr) > MAXDIM)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("number of array dimensions (%d) exceeds the maximum allowed (%d)", list_length(sbsref->reflowerindexpr), MAXDIM)));
  }

                                 
  i = 0;
  foreach (lc, sbsref->refupperindexpr)
  {
    Expr *e = (Expr *)lfirst(lc);

                                                                  
    if (!e)
    {
      sbsrefstate->upperprovided[i] = false;
      i++;
      continue;
    }

    sbsrefstate->upperprovided[i] = true;

                                                                       
    ExecInitExprRec(e, state, &sbsrefstate->subscriptvalue, &sbsrefstate->subscriptnull);

                                                                      
    scratch->opcode = EEOP_SBSREF_SUBSCRIPT;
    scratch->d.sbsref_subscript.state = sbsrefstate;
    scratch->d.sbsref_subscript.off = i;
    scratch->d.sbsref_subscript.isupper = true;
    scratch->d.sbsref_subscript.jumpdone = -1;                   
    ExprEvalPushStep(state, scratch);
    adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
    i++;
  }
  sbsrefstate->numupper = i;

                                           
  i = 0;
  foreach (lc, sbsref->reflowerindexpr)
  {
    Expr *e = (Expr *)lfirst(lc);

                                                                  
    if (!e)
    {
      sbsrefstate->lowerprovided[i] = false;
      i++;
      continue;
    }

    sbsrefstate->lowerprovided[i] = true;

                                                                       
    ExecInitExprRec(e, state, &sbsrefstate->subscriptvalue, &sbsrefstate->subscriptnull);

                                                                      
    scratch->opcode = EEOP_SBSREF_SUBSCRIPT;
    scratch->d.sbsref_subscript.state = sbsrefstate;
    scratch->d.sbsref_subscript.off = i;
    scratch->d.sbsref_subscript.isupper = false;
    scratch->d.sbsref_subscript.jumpdone = -1;                   
    ExprEvalPushStep(state, scratch);
    adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
    i++;
  }
  sbsrefstate->numlower = i;

                                                                 
  if (sbsrefstate->numlower != 0 && sbsrefstate->numupper != sbsrefstate->numlower)
  {
    elog(ERROR, "upper and lower index lists are not same length");
  }

  if (isAssignment)
  {
    Datum *save_innermost_caseval;
    bool *save_innermost_casenull;

       
                                                                 
                                                                         
                                                                       
                                                                        
                                                                        
                                                                       
                                                                      
                                                                           
                                                                    
       
                                                                           
                                               
       
    if (isAssignmentIndirectionExpr(sbsref->refassgnexpr))
    {
      scratch->opcode = EEOP_SBSREF_OLD;
      scratch->d.sbsref.state = sbsrefstate;
      ExprEvalPushStep(state, scratch);
    }

                                                                 
    save_innermost_caseval = state->innermost_caseval;
    save_innermost_casenull = state->innermost_casenull;
    state->innermost_caseval = &sbsrefstate->prevvalue;
    state->innermost_casenull = &sbsrefstate->prevnull;

                                                                  
    ExecInitExprRec(sbsref->refassgnexpr, state, &sbsrefstate->replacevalue, &sbsrefstate->replacenull);

    state->innermost_caseval = save_innermost_caseval;
    state->innermost_casenull = save_innermost_casenull;

                                    
    scratch->opcode = EEOP_SBSREF_ASSIGN;
    scratch->d.sbsref.state = sbsrefstate;
    ExprEvalPushStep(state, scratch);
  }
  else
  {
                                     
    scratch->opcode = EEOP_SBSREF_FETCH;
    scratch->d.sbsref.state = sbsrefstate;
    ExprEvalPushStep(state, scratch);
  }

                           
  foreach (lc, adjust_jumps)
  {
    ExprEvalStep *as = &state->steps[lfirst_int(lc)];

    if (as->opcode == EEOP_SBSREF_SUBSCRIPT)
    {
      Assert(as->d.sbsref_subscript.jumpdone == -1);
      as->d.sbsref_subscript.jumpdone = state->steps_len;
    }
    else
    {
      Assert(as->opcode == EEOP_JUMP_IF_NULL);
      Assert(as->d.jump.jumpdone == -1);
      as->d.jump.jumpdone = state->steps_len;
    }
  }
}

   
                                                                            
                                                                           
                
   
                                                                          
                                       
   
                                                                             
                                                                             
                                                                         
                                                                             
                                                                         
                                                                              
                                                                             
                 
   
static bool
isAssignmentIndirectionExpr(Expr *expr)
{
  if (expr == NULL)
  {
    return false;                    
  }
  if (IsA(expr, FieldStore))
  {
    FieldStore *fstore = (FieldStore *)expr;

    if (fstore->arg && IsA(fstore->arg, CaseTestExpr))
    {
      return true;
    }
  }
  else if (IsA(expr, SubscriptingRef))
  {
    SubscriptingRef *sbsRef = (SubscriptingRef *)expr;

    if (sbsRef->refexpr && IsA(sbsRef->refexpr, CaseTestExpr))
    {
      return true;
    }
  }
  else if (IsA(expr, CoerceToDomain))
  {
    CoerceToDomain *cd = (CoerceToDomain *)expr;

    return isAssignmentIndirectionExpr(cd->arg);
  }
  return false;
}

   
                                                      
   
static void
ExecInitCoerceToDomain(ExprEvalStep *scratch, CoerceToDomain *ctest, ExprState *state, Datum *resv, bool *resnull)
{
  ExprEvalStep scratch2 = {0};
  DomainConstraintRef *constraint_ref;
  Datum *domainval = NULL;
  bool *domainnull = NULL;
  Datum *save_innermost_domainval;
  bool *save_innermost_domainnull;
  ListCell *l;

  scratch->d.domaincheck.resulttype = ctest->resulttype;
                                               
  scratch->d.domaincheck.checkvalue = NULL;
  scratch->d.domaincheck.checknull = NULL;

     
                                                                           
                                                                            
                           
     
  ExecInitExprRec(ctest->arg, state, resv, resnull);

     
                                                                          
                                                                            
                                                                           
                                                                         
                                                                           
                                                       
     

     
                                                         
     
                                                                         
                                                                        
                                                                             
                             
     
  constraint_ref = (DomainConstraintRef *)palloc(sizeof(DomainConstraintRef));
  InitDomainConstraintRef(ctest->resulttype, constraint_ref, CurrentMemoryContext, false);

     
                                                                            
                                                                             
                        
     
  foreach (l, constraint_ref->constraints)
  {
    DomainConstraintState *con = (DomainConstraintState *)lfirst(l);

    scratch->d.domaincheck.constraintname = con->name;

    switch (con->constrainttype)
    {
    case DOM_CONSTRAINT_NOTNULL:
      scratch->opcode = EEOP_DOMAIN_NOTNULL;
      ExprEvalPushStep(state, scratch);
      break;
    case DOM_CONSTRAINT_CHECK:
                                                                
      if (scratch->d.domaincheck.checkvalue == NULL)
      {
        scratch->d.domaincheck.checkvalue = (Datum *)palloc(sizeof(Datum));
        scratch->d.domaincheck.checknull = (bool *)palloc(sizeof(bool));
      }

         
                                                                    
                                 
         
      if (domainval == NULL)
      {
           
                                                                  
                                                        
           
        if (get_typlen(ctest->resulttype) == -1)
        {
                                                               
          domainval = (Datum *)palloc(sizeof(Datum));
          domainnull = (bool *)palloc(sizeof(bool));

                                  
          scratch2.opcode = EEOP_MAKE_READONLY;
          scratch2.resvalue = domainval;
          scratch2.resnull = domainnull;
          scratch2.d.make_readonly.value = resv;
          scratch2.d.make_readonly.isnull = resnull;
          ExprEvalPushStep(state, &scratch2);
        }
        else
        {
                                                          
          domainval = resv;
          domainnull = resnull;
        }
      }

         
                                                                   
                                                                   
                                                                   
                         
         
      save_innermost_domainval = state->innermost_domainval;
      save_innermost_domainnull = state->innermost_domainnull;
      state->innermost_domainval = domainval;
      state->innermost_domainnull = domainnull;

                                           
      ExecInitExprRec(con->check_expr, state, scratch->d.domaincheck.checkvalue, scratch->d.domaincheck.checknull);

      state->innermost_domainval = save_innermost_domainval;
      state->innermost_domainnull = save_innermost_domainnull;

                           
      scratch->opcode = EEOP_DOMAIN_CHECK;
      ExprEvalPushStep(state, scratch);

      break;
    default:
      elog(ERROR, "unrecognized constraint type: %d", (int)con->constrainttype);
      break;
    }
  }
}

   
                                                                              
                                                                            
                                                                              
                                                               
   
                                                                           
                                                                             
                                                                      
                                                                   
   
ExprState *
ExecBuildAggTrans(AggState *aggstate, AggStatePerPhase phase, bool doSort, bool doHash)
{
  ExprState *state = makeNode(ExprState);
  PlanState *parent = &aggstate->ss.ps;
  ExprEvalStep scratch = {0};
  int transno = 0;
  int setoff = 0;
  bool isCombine = DO_AGGSPLIT_COMBINE(aggstate->aggsplit);
  LastAttnumInfo deform = {0, 0, 0};

  state->expr = (Expr *)aggstate;
  state->parent = parent;

  scratch.resvalue = &state->resvalue;
  scratch.resnull = &state->resnull;

     
                                                                         
                    
     
  for (transno = 0; transno < aggstate->numtrans; transno++)
  {
    AggStatePerTrans pertrans = &aggstate->pertrans[transno];

    get_last_attnums_walker((Node *)pertrans->aggref->aggdirectargs, &deform);
    get_last_attnums_walker((Node *)pertrans->aggref->args, &deform);
    get_last_attnums_walker((Node *)pertrans->aggref->aggorder, &deform);
    get_last_attnums_walker((Node *)pertrans->aggref->aggdistinct, &deform);
    get_last_attnums_walker((Node *)pertrans->aggref->aggfilter, &deform);
  }
  ExecPushExprSlots(state, &deform);

     
                                                                             
     
  for (transno = 0; transno < aggstate->numtrans; transno++)
  {
    AggStatePerTrans pertrans = &aggstate->pertrans[transno];
    int argno;
    int setno;
    FunctionCallInfo trans_fcinfo = pertrans->transfn_fcinfo;
    ListCell *arg;
    ListCell *bail;
    List *adjust_bailout = NIL;
    NullableDatum *strictargs = NULL;
    bool *strictnulls = NULL;

       
                                                                      
                                                                          
                                                                      
                          
       
    if (pertrans->aggref->aggfilter && !isCombine)
    {
                                      
      ExecInitExprRec(pertrans->aggref->aggfilter, state, &state->resvalue, &state->resnull);
                                 
      scratch.opcode = EEOP_JUMP_IF_NOT_TRUE;
      scratch.d.jump.jumpdone = -1;                   
      ExprEvalPushStep(state, &scratch);
      adjust_bailout = lappend_int(adjust_bailout, state->steps_len - 1);
    }

       
                                                         
       
    argno = 0;
    if (isCombine)
    {
         
                                                                        
                                                                       
                           
         
      TargetEntry *source_tle;

      Assert(pertrans->numSortCols == 0);
      Assert(list_length(pertrans->aggref->args) == 1);

      strictargs = trans_fcinfo->args + 1;
      source_tle = (TargetEntry *)linitial(pertrans->aggref->args);

         
                                                                     
                                                    
         
      if (!OidIsValid(pertrans->deserialfn_oid))
      {
           
                                                                  
                 
           
        ExecInitExprRec(source_tle->expr, state, &trans_fcinfo->args[argno + 1].value, &trans_fcinfo->args[argno + 1].isnull);
      }
      else
      {
        FunctionCallInfo ds_fcinfo = pertrans->deserialfn_fcinfo;

                               
        ExecInitExprRec(source_tle->expr, state, &ds_fcinfo->args[0].value, &ds_fcinfo->args[0].isnull);

                                                           
        ds_fcinfo->args[1].value = PointerGetDatum(NULL);
        ds_fcinfo->args[1].isnull = false;

           
                                                                  
                 
           
        if (pertrans->deserialfn.fn_strict)
        {
          scratch.opcode = EEOP_AGG_STRICT_DESERIALIZE;
        }
        else
        {
          scratch.opcode = EEOP_AGG_DESERIALIZE;
        }

        scratch.d.agg_deserialize.aggstate = aggstate;
        scratch.d.agg_deserialize.fcinfo_data = ds_fcinfo;
        scratch.d.agg_deserialize.jumpnull = -1;                   
        scratch.resvalue = &trans_fcinfo->args[argno + 1].value;
        scratch.resnull = &trans_fcinfo->args[argno + 1].isnull;

        ExprEvalPushStep(state, &scratch);
                                                                   
        if (pertrans->deserialfn.fn_strict)
        {
          adjust_bailout = lappend_int(adjust_bailout, state->steps_len - 1);
        }

                                                       
        scratch.resvalue = &state->resvalue;
        scratch.resnull = &state->resnull;
      }
      argno++;
    }
    else if (pertrans->numSortCols == 0)
    {
         
                                                                 
         
      strictargs = trans_fcinfo->args + 1;

      foreach (arg, pertrans->aggref->args)
      {
        TargetEntry *source_tle = (TargetEntry *)lfirst(arg);

           
                                                                  
                 
           
        ExecInitExprRec(source_tle->expr, state, &trans_fcinfo->args[argno + 1].value, &trans_fcinfo->args[argno + 1].isnull);
        argno++;
      }
    }
    else if (pertrans->numInputs == 1)
    {
         
                                                                        
         
      TargetEntry *source_tle = (TargetEntry *)linitial(pertrans->aggref->args);

      Assert(list_length(pertrans->aggref->args) == 1);

      ExecInitExprRec(source_tle->expr, state, &state->resvalue, &state->resnull);
      strictnulls = &state->resnull;
      argno++;
    }
    else
    {
         
                                                                         
         
      Datum *values = pertrans->sortslot->tts_values;
      bool *nulls = pertrans->sortslot->tts_isnull;

      strictnulls = nulls;

      foreach (arg, pertrans->aggref->args)
      {
        TargetEntry *source_tle = (TargetEntry *)lfirst(arg);

        ExecInitExprRec(source_tle->expr, state, &values[argno], &nulls[argno]);
        argno++;
      }
    }
    Assert(pertrans->numInputs == argno);

       
                                                                           
                                                                       
                                   
       
    if (trans_fcinfo->flinfo->fn_strict && pertrans->numTransInputs > 0)
    {
      if (strictnulls)
      {
        scratch.opcode = EEOP_AGG_STRICT_INPUT_CHECK_NULLS;
      }
      else
      {
        scratch.opcode = EEOP_AGG_STRICT_INPUT_CHECK_ARGS;
      }
      scratch.d.agg_strict_input_check.nulls = strictnulls;
      scratch.d.agg_strict_input_check.args = strictargs;
      scratch.d.agg_strict_input_check.jumpnull = -1;                   
      scratch.d.agg_strict_input_check.nargs = pertrans->numTransInputs;
      ExprEvalPushStep(state, &scratch);
      adjust_bailout = lappend_int(adjust_bailout, state->steps_len - 1);
    }

       
                                                                      
                                                                          
                   
       
    setoff = 0;
    if (doSort)
    {
      int processGroupingSets = Max(phase->numsets, 1);

      for (setno = 0; setno < processGroupingSets; setno++)
      {
        ExecBuildAggTransCall(state, aggstate, &scratch, trans_fcinfo, pertrans, transno, setno, setoff, false);
        setoff++;
      }
    }

    if (doHash)
    {
      int numHashes = aggstate->num_hashes;

                                                                  
      if (aggstate->aggstrategy != AGG_HASHED)
      {
        setoff = aggstate->maxsets;
      }
      else
      {
        setoff = 0;
      }

      for (setno = 0; setno < numHashes; setno++)
      {
        ExecBuildAggTransCall(state, aggstate, &scratch, trans_fcinfo, pertrans, transno, setno, setoff, true);
        setoff++;
      }
    }

                                              
    foreach (bail, adjust_bailout)
    {
      ExprEvalStep *as = &state->steps[lfirst_int(bail)];

      if (as->opcode == EEOP_JUMP_IF_NOT_TRUE)
      {
        Assert(as->d.jump.jumpdone == -1);
        as->d.jump.jumpdone = state->steps_len;
      }
      else if (as->opcode == EEOP_AGG_STRICT_INPUT_CHECK_ARGS || as->opcode == EEOP_AGG_STRICT_INPUT_CHECK_NULLS)
      {
        Assert(as->d.agg_strict_input_check.jumpnull == -1);
        as->d.agg_strict_input_check.jumpnull = state->steps_len;
      }
      else if (as->opcode == EEOP_AGG_STRICT_DESERIALIZE)
      {
        Assert(as->d.agg_deserialize.jumpnull == -1);
        as->d.agg_deserialize.jumpnull = state->steps_len;
      }
    }
  }

  scratch.resvalue = NULL;
  scratch.resnull = NULL;
  scratch.opcode = EEOP_DONE;
  ExprEvalPushStep(state, &scratch);

  ExecReadyExpr(state);

  return state;
}

   
                                                                        
                                                                       
                                                                  
   
static void
ExecBuildAggTransCall(ExprState *state, AggState *aggstate, ExprEvalStep *scratch, FunctionCallInfo fcinfo, AggStatePerTrans pertrans, int transno, int setno, int setoff, bool ishash)
{
  int adjust_init_jumpnull = -1;
  int adjust_strict_jumpnull = -1;
  ExprContext *aggcontext;

  if (ishash)
  {
    aggcontext = aggstate->hashcontext;
  }
  else
  {
    aggcontext = aggstate->aggcontexts[setno];
  }

     
                                                                        
                                                                           
                                                                           
                                                                             
                            
     
  if (pertrans->numSortCols == 0 && fcinfo->flinfo->fn_strict && pertrans->initValueIsNull)
  {
    scratch->opcode = EEOP_AGG_INIT_TRANS;
    scratch->d.agg_init_trans.aggstate = aggstate;
    scratch->d.agg_init_trans.pertrans = pertrans;
    scratch->d.agg_init_trans.setno = setno;
    scratch->d.agg_init_trans.setoff = setoff;
    scratch->d.agg_init_trans.transno = transno;
    scratch->d.agg_init_trans.aggcontext = aggcontext;
    scratch->d.agg_init_trans.jumpnull = -1;                   
    ExprEvalPushStep(state, scratch);

                                             
    adjust_init_jumpnull = state->steps_len - 1;
  }

  if (pertrans->numSortCols == 0 && fcinfo->flinfo->fn_strict)
  {
    scratch->opcode = EEOP_AGG_STRICT_TRANS_CHECK;
    scratch->d.agg_strict_trans_check.aggstate = aggstate;
    scratch->d.agg_strict_trans_check.setno = setno;
    scratch->d.agg_strict_trans_check.setoff = setoff;
    scratch->d.agg_strict_trans_check.transno = transno;
    scratch->d.agg_strict_trans_check.jumpnull = -1;                   
    ExprEvalPushStep(state, scratch);

       
                                                                        
                                                                          
                                                                    
       
    adjust_strict_jumpnull = state->steps_len - 1;
  }

                                                    
  if (pertrans->numSortCols == 0 && pertrans->transtypeByVal)
  {
    scratch->opcode = EEOP_AGG_PLAIN_TRANS_BYVAL;
  }
  else if (pertrans->numSortCols == 0)
  {
    scratch->opcode = EEOP_AGG_PLAIN_TRANS;
  }
  else if (pertrans->numInputs == 1)
  {
    scratch->opcode = EEOP_AGG_ORDERED_TRANS_DATUM;
  }
  else
  {
    scratch->opcode = EEOP_AGG_ORDERED_TRANS_TUPLE;
  }

  scratch->d.agg_trans.aggstate = aggstate;
  scratch->d.agg_trans.pertrans = pertrans;
  scratch->d.agg_trans.setno = setno;
  scratch->d.agg_trans.setoff = setoff;
  scratch->d.agg_trans.transno = transno;
  scratch->d.agg_trans.aggcontext = aggcontext;
  ExprEvalPushStep(state, scratch);

                                                                  
  if (adjust_init_jumpnull != -1)
  {
    ExprEvalStep *as = &state->steps[adjust_init_jumpnull];

    Assert(as->d.agg_init_trans.jumpnull == -1);
    as->d.agg_init_trans.jumpnull = state->steps_len;
  }
  if (adjust_strict_jumpnull != -1)
  {
    ExprEvalStep *as = &state->steps[adjust_strict_jumpnull];

    Assert(as->d.agg_strict_trans_check.jumpnull == -1);
    as->d.agg_strict_trans_check.jumpnull = state->steps_len;
  }
}

   
                                                                               
                                                                            
                                                       
   
                                                       
                                                    
                                                
                                                                        
                                
   
ExprState *
ExecBuildGroupingEqual(TupleDesc ldesc, TupleDesc rdesc, const TupleTableSlotOps *lops, const TupleTableSlotOps *rops, int numCols, const AttrNumber *keyColIdx, const Oid *eqfunctions, const Oid *collations, PlanState *parent)
{
  ExprState *state = makeNode(ExprState);
  ExprEvalStep scratch = {0};
  int natt;
  int maxatt = -1;
  List *adjust_jumps = NIL;
  ListCell *lc;

     
                                                                          
                                 
     
  if (numCols == 0)
  {
    return NULL;
  }

  state->expr = NULL;
  state->flags = EEO_FLAG_IS_QUAL;
  state->parent = parent;

  scratch.resvalue = &state->resvalue;
  scratch.resnull = &state->resnull;

                                    
  for (natt = 0; natt < numCols; natt++)
  {
    int attno = keyColIdx[natt];

    if (attno > maxatt)
    {
      maxatt = attno;
    }
  }
  Assert(maxatt >= 0);

                         
  scratch.opcode = EEOP_INNER_FETCHSOME;
  scratch.d.fetch.last_var = maxatt;
  scratch.d.fetch.fixed = false;
  scratch.d.fetch.known_desc = ldesc;
  scratch.d.fetch.kind = lops;
  ExecComputeSlotInfo(state, &scratch);
  ExprEvalPushStep(state, &scratch);

  scratch.opcode = EEOP_OUTER_FETCHSOME;
  scratch.d.fetch.last_var = maxatt;
  scratch.d.fetch.fixed = false;
  scratch.d.fetch.known_desc = rdesc;
  scratch.d.fetch.kind = rops;
  ExecComputeSlotInfo(state, &scratch);
  ExprEvalPushStep(state, &scratch);

     
                                                                            
                                                                          
     
  for (natt = numCols; --natt >= 0;)
  {
    int attno = keyColIdx[natt];
    Form_pg_attribute latt = TupleDescAttr(ldesc, attno - 1);
    Form_pg_attribute ratt = TupleDescAttr(rdesc, attno - 1);
    Oid foid = eqfunctions[natt];
    Oid collid = collations[natt];
    FmgrInfo *finfo;
    FunctionCallInfo fcinfo;
    AclResult aclresult;

                                           
    aclresult = pg_proc_aclcheck(foid, GetUserId(), ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(foid));
    }

    InvokeFunctionExecuteHook(foid);

                                                    
    finfo = palloc0(sizeof(FmgrInfo));
    fcinfo = palloc0(SizeForFunctionCallInfo(2));
    fmgr_info(foid, finfo);
    fmgr_info_set_expr(NULL, finfo);
    InitFunctionCallInfoData(*fcinfo, finfo, 2, collid, NULL, NULL);

                  
    scratch.opcode = EEOP_INNER_VAR;
    scratch.d.var.attnum = attno - 1;
    scratch.d.var.vartype = latt->atttypid;
    scratch.resvalue = &fcinfo->args[0].value;
    scratch.resnull = &fcinfo->args[0].isnull;
    ExprEvalPushStep(state, &scratch);

                   
    scratch.opcode = EEOP_OUTER_VAR;
    scratch.d.var.attnum = attno - 1;
    scratch.d.var.vartype = ratt->atttypid;
    scratch.resvalue = &fcinfo->args[1].value;
    scratch.resnull = &fcinfo->args[1].isnull;
    ExprEvalPushStep(state, &scratch);

                               
    scratch.opcode = EEOP_NOT_DISTINCT;
    scratch.d.func.finfo = finfo;
    scratch.d.func.fcinfo_data = fcinfo;
    scratch.d.func.fn_addr = finfo->fn_addr;
    scratch.d.func.nargs = 2;
    scratch.resvalue = &state->resvalue;
    scratch.resnull = &state->resnull;
    ExprEvalPushStep(state, &scratch);

                                                                    
    scratch.opcode = EEOP_QUAL;
    scratch.d.qualexpr.jumpdone = -1;
    scratch.resvalue = &state->resvalue;
    scratch.resnull = &state->resnull;
    ExprEvalPushStep(state, &scratch);
    adjust_jumps = lappend_int(adjust_jumps, state->steps_len - 1);
  }

                           
  foreach (lc, adjust_jumps)
  {
    ExprEvalStep *as = &state->steps[lfirst_int(lc)];

    Assert(as->opcode == EEOP_QUAL);
    Assert(as->d.qualexpr.jumpdone == -1);
    as->d.qualexpr.jumpdone = state->steps_len;
  }

  scratch.resvalue = NULL;
  scratch.resnull = NULL;
  scratch.opcode = EEOP_DONE;
  ExprEvalPushStep(state, &scratch);

  ExecReadyExpr(state);

  return state;
}
