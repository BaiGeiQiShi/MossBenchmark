                                                                            
   
               
                                       
   
                                                                          
                                  
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/paramassign.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)

typedef struct convert_testexpr_context
{
  PlannerInfo *root;
  List *subst_nodes;                                     
} convert_testexpr_context;

typedef struct process_sublinks_context
{
  PlannerInfo *root;
  bool isTopQual;
} process_sublinks_context;

typedef struct finalize_primnode_context
{
  PlannerInfo *root;
  Bitmapset *paramids;                                          
} finalize_primnode_context;

typedef struct inline_cte_walker_context
{
  const char *ctename;                                            
  int levelsup;
  Query *ctequery;                          
} inline_cte_walker_context;

static Node *
build_subplan(PlannerInfo *root, Plan *plan, PlannerInfo *subroot, List *plan_params, SubLinkType subLinkType, int subLinkId, Node *testexpr, List *testexpr_paramids, bool unknownEqFalse);
static List *
generate_subquery_params(PlannerInfo *root, List *tlist, List **paramIds);
static List *
generate_subquery_vars(PlannerInfo *root, List *tlist, Index varno);
static Node *
convert_testexpr(PlannerInfo *root, Node *testexpr, List *subst_nodes);
static Node *
convert_testexpr_mutator(Node *node, convert_testexpr_context *context);
static bool
subplan_is_hashable(Plan *plan);
static bool
testexpr_is_hashable(Node *testexpr, List *param_ids);
static bool
test_opexpr_is_hashable(OpExpr *testexpr, List *param_ids);
static bool
hash_ok_operator(OpExpr *expr);
static bool
SS_make_multiexprs_unique_walker(Node *node, void *context);
static bool
contain_dml(Node *node);
static bool
contain_dml_walker(Node *node, void *context);
static bool
contain_outer_selfref(Node *node);
static bool
contain_outer_selfref_walker(Node *node, Index *depth);
static void
inline_cte(PlannerInfo *root, CommonTableExpr *cte);
static bool
inline_cte_walker(Node *node, inline_cte_walker_context *context);
static bool
simplify_EXISTS_query(PlannerInfo *root, Query *query);
static Query *
convert_EXISTS_to_ANY(PlannerInfo *root, Query *subselect, Node **testexpr, List **paramIds);
static Node *
replace_correlation_vars_mutator(Node *node, PlannerInfo *root);
static Node *
process_sublinks_mutator(Node *node, process_sublinks_context *context);
static Bitmapset *
finalize_plan(PlannerInfo *root, Plan *plan, int gather_param, Bitmapset *valid_params, Bitmapset *scan_params);
static bool
finalize_primnode(Node *node, finalize_primnode_context *context);
static bool
finalize_agg_primnode(Node *node, finalize_primnode_context *context);

   
                                                                               
   
                                                                  
                                                                            
                                                                          
                                                                           
           
   
static void
get_first_col_type(Plan *plan, Oid *coltype, int32 *coltypmod, Oid *colcollation)
{
                                                                           
  if (plan->targetlist)
  {
    TargetEntry *tent = linitial_node(TargetEntry, plan->targetlist);

    if (!tent->resjunk)
    {
      *coltype = exprType((Node *)tent->expr);
      *coltypmod = exprTypmod((Node *)tent->expr);
      *colcollation = exprCollation((Node *)tent->expr);
      return;
    }
  }
  *coltype = VOIDOID;
  *coltypmod = -1;
  *colcollation = InvalidOid;
}

   
                                                                
   
                                                                               
                                                                             
   
                                                                          
                                                                   
                                                                      
                                                                      
   
                                                                             
                                                                        
                                                                               
                                                                        
                                                                   
                                                                              
                                                                         
                                                                            
                                                                              
                                                                              
   
static Node *
make_subplan(PlannerInfo *root, Query *orig_subquery, SubLinkType subLinkType, int subLinkId, Node *testexpr, bool isTopQual)
{
  Query *subquery;
  bool simple_exists = false;
  double tuple_fraction;
  PlannerInfo *subroot;
  RelOptInfo *final_rel;
  Path *best_path;
  Plan *plan;
  List *plan_params;
  Node *result;

     
                                                                             
                                                                            
                                                                          
                                                           
     
  subquery = copyObject(orig_subquery);

     
                                                                 
     
  if (subLinkType == EXISTS_SUBLINK)
  {
    simple_exists = simplify_EXISTS_query(root, subquery);
  }

     
                                                                             
                                                                          
                                                                             
                                                                            
                                                                           
                                                                      
     
                                                                      
                      
     
                                                                             
                                                                       
                                                                          
                                                                             
                                                                   
                                                                      
                                                               
     
  if (subLinkType == EXISTS_SUBLINK)
  {
    tuple_fraction = 1.0;                          
  }
  else if (subLinkType == ALL_SUBLINK || subLinkType == ANY_SUBLINK)
  {
    tuple_fraction = 0.5;          
  }
  else
  {
    tuple_fraction = 0.0;                       
  }

                                                               
  Assert(root->plan_params == NIL);

                                       
  subroot = subquery_planner(root->glob, subquery, root, false, tuple_fraction);

                                                          
  plan_params = root->plan_params;
  root->plan_params = NIL;

     
                                                                        
                                             
     
  final_rel = fetch_upper_rel(subroot, UPPERREL_FINAL, NULL);
  best_path = get_cheapest_fractional_path(final_rel, tuple_fraction);

  plan = create_plan(subroot, best_path);

                                                  
  result = build_subplan(root, plan, subroot, plan_params, subLinkType, subLinkId, testexpr, NIL, isTopQual);

     
                                                                             
                                                                           
                                                                            
                                                                             
                                                                            
                                                                             
                                                      
     
  if (simple_exists && IsA(result, SubPlan))
  {
    Node *newtestexpr;
    List *paramIds;

                                                     
    subquery = copyObject(orig_subquery);
                         
    simple_exists = simplify_EXISTS_query(root, subquery);
    Assert(simple_exists);
                                                    
    subquery = convert_EXISTS_to_ANY(root, subquery, &newtestexpr, &paramIds);
    if (subquery)
    {
                                                                    
      subroot = subquery_planner(root->glob, subquery, root, false, 0.0);

                                                              
      plan_params = root->plan_params;
      root->plan_params = NIL;

                                                    
      final_rel = fetch_upper_rel(subroot, UPPERREL_FINAL, NULL);
      best_path = final_rel->cheapest_total_path;

      plan = create_plan(subroot, best_path);

                                                     
                                                    
      if (subplan_is_hashable(plan))
      {
        SubPlan *hashplan;
        AlternativeSubPlan *asplan;

                                            
        hashplan = castNode(SubPlan, build_subplan(root, plan, subroot, plan_params, ANY_SUBLINK, 0, newtestexpr, paramIds, true));
                                           
        Assert(hashplan->parParam == NIL);
        Assert(hashplan->useHashTable);

                                                                  
        asplan = makeNode(AlternativeSubPlan);
        asplan->subplans = list_make2(result, hashplan);
        result = (Node *)asplan;
      }
    }
  }

  return result;
}

   
                                                                             
   
                                                                           
                                                                       
   
static Node *
build_subplan(PlannerInfo *root, Plan *plan, PlannerInfo *subroot, List *plan_params, SubLinkType subLinkType, int subLinkId, Node *testexpr, List *testexpr_paramids, bool unknownEqFalse)
{
  Node *result;
  SubPlan *splan;
  bool isInitPlan;
  ListCell *lc;

     
                                                                            
                           
     
  splan = makeNode(SubPlan);
  splan->subLinkType = subLinkType;
  splan->subLinkId = subLinkId;
  splan->testexpr = NULL;
  splan->paramIds = NIL;
  get_first_col_type(plan, &splan->firstColType, &splan->firstColTypmod, &splan->firstColCollation);
  splan->useHashTable = false;
  splan->unknownEqFalse = unknownEqFalse;
  splan->parallel_safe = plan->parallel_safe;
  splan->setParam = NIL;
  splan->parParam = NIL;
  splan->args = NIL;

     
                                                                            
                                               
     
  foreach (lc, plan_params)
  {
    PlannerParamItem *pitem = (PlannerParamItem *)lfirst(lc);
    Node *arg = pitem->item;

       
                                                                        
                                                                
                    
       
                                                                       
                                                                          
                                                                
       
    if (IsA(arg, PlaceHolderVar) || IsA(arg, Aggref) || IsA(arg, GroupingFunc))
    {
      arg = SS_process_sublinks(root, arg, false);
    }

    splan->parParam = lappend_int(splan->parParam, pitem->paramId);
    splan->args = lappend(splan->args, arg);
  }

     
                                                                        
                                                                           
                                                                             
                                                                        
                                                                           
                                                                           
                                                                            
                                                                          
                
     
  if (splan->parParam == NIL && subLinkType == EXISTS_SUBLINK)
  {
    Param *prm;

    Assert(testexpr == NULL);
    prm = generate_new_exec_param(root, BOOLOID, -1, InvalidOid);
    splan->setParam = list_make1_int(prm->paramid);
    isInitPlan = true;
    result = (Node *)prm;
  }
  else if (splan->parParam == NIL && subLinkType == EXPR_SUBLINK)
  {
    TargetEntry *te = linitial(plan->targetlist);
    Param *prm;

    Assert(!te->resjunk);
    Assert(testexpr == NULL);
    prm = generate_new_exec_param(root, exprType((Node *)te->expr), exprTypmod((Node *)te->expr), exprCollation((Node *)te->expr));
    splan->setParam = list_make1_int(prm->paramid);
    isInitPlan = true;
    result = (Node *)prm;
  }
  else if (splan->parParam == NIL && subLinkType == ARRAY_SUBLINK)
  {
    TargetEntry *te = linitial(plan->targetlist);
    Oid arraytype;
    Param *prm;

    Assert(!te->resjunk);
    Assert(testexpr == NULL);
    arraytype = get_promoted_array_type(exprType((Node *)te->expr));
    if (!OidIsValid(arraytype))
    {
      elog(ERROR, "could not find array type for datatype %s", format_type_be(exprType((Node *)te->expr)));
    }
    prm = generate_new_exec_param(root, arraytype, exprTypmod((Node *)te->expr), exprCollation((Node *)te->expr));
    splan->setParam = list_make1_int(prm->paramid);
    isInitPlan = true;
    result = (Node *)prm;
  }
  else if (splan->parParam == NIL && subLinkType == ROWCOMPARE_SUBLINK)
  {
                           
    List *params;

    Assert(testexpr != NULL);
    params = generate_subquery_params(root, plan->targetlist, &splan->paramIds);
    result = convert_testexpr(root, testexpr, params);
    splan->setParam = list_copy(splan->paramIds);
    isInitPlan = true;

       
                                                                         
                                                                    
       
  }
  else if (subLinkType == MULTIEXPR_SUBLINK)
  {
       
                                                                           
                               
       
    List *params;

    Assert(testexpr == NULL);
    params = generate_subquery_params(root, plan->targetlist, &splan->setParam);

       
                                                               
                                                                
                               
       
    while (list_length(root->multiexpr_params) < subLinkId)
    {
      root->multiexpr_params = lappend(root->multiexpr_params, NIL);
    }
    lc = list_nth_cell(root->multiexpr_params, subLinkId - 1);
    Assert(lfirst(lc) == NIL);
    lfirst(lc) = params;

                                                          
    if (splan->parParam == NIL)
    {
      isInitPlan = true;
      result = (Node *)makeNullConst(RECORDOID, -1, InvalidOid);
    }
    else
    {
      isInitPlan = false;
      result = (Node *)splan;
    }
  }
  else
  {
       
                                                                          
                                                            
       
    if (testexpr && testexpr_paramids == NIL)
    {
      List *params;

      params = generate_subquery_params(root, plan->targetlist, &splan->paramIds);
      splan->testexpr = convert_testexpr(root, testexpr, params);
    }
    else
    {
      splan->testexpr = testexpr;
      splan->paramIds = testexpr_paramids;
    }

       
                                                                        
                                                                          
                                                                        
                                                                       
                                                                           
       
    if (subLinkType == ANY_SUBLINK && splan->parParam == NIL && subplan_is_hashable(plan) && testexpr_is_hashable(splan->testexpr, splan->paramIds))
    {
      splan->useHashTable = true;
    }

       
                                                                          
                                                                          
                                                                        
                                                                          
                                                                          
                                                                           
                                                                        
                                   
       
    else if (splan->parParam == NIL && enable_material && !ExecMaterializesOutput(nodeTag(plan)))
    {
      plan = materialize_finished_plan(plan);
    }

    result = (Node *)splan;
    isInitPlan = false;
  }

     
                                                              
     
  root->glob->subplans = lappend(root->glob->subplans, plan);
  root->glob->subroots = lappend(root->glob->subroots, subroot);
  splan->plan_id = list_length(root->glob->subplans);

  if (isInitPlan)
  {
    root->init_plans = lappend(root->init_plans, splan);
  }

     
                                                                         
                                                                            
                                                                            
                                                                          
                                                                        
     
  if (splan->parParam == NIL && !isInitPlan && !splan->useHashTable)
  {
    root->glob->rewindPlanIDs = bms_add_member(root->glob->rewindPlanIDs, splan->plan_id);
  }

                                              
  splan->plan_name = palloc(32 + 12 * list_length(splan->setParam));
  sprintf(splan->plan_name, "%s %d", isInitPlan ? "InitPlan" : "SubPlan", splan->plan_id);
  if (splan->setParam)
  {
    char *ptr = splan->plan_name + strlen(splan->plan_name);

    ptr += sprintf(ptr, " (returns ");
    foreach (lc, splan->setParam)
    {
      ptr += sprintf(ptr, "$%d%s", lfirst_int(lc), lnext(lc) ? "," : ")");
    }
  }

                                                        
  cost_subplan(root, splan, plan);

  return result;
}

   
                                                                            
                                                                         
   
                                                                 
   
static List *
generate_subquery_params(PlannerInfo *root, List *tlist, List **paramIds)
{
  List *result;
  List *ids;
  ListCell *lc;

  result = ids = NIL;
  foreach (lc, tlist)
  {
    TargetEntry *tent = (TargetEntry *)lfirst(lc);
    Param *param;

    if (tent->resjunk)
    {
      continue;
    }

    param = generate_new_exec_param(root, exprType((Node *)tent->expr), exprTypmod((Node *)tent->expr), exprCollation((Node *)tent->expr));
    result = lappend(result, param);
    ids = lappend_int(ids, param->paramid);
  }

  *paramIds = ids;
  return result;
}

   
                                                                        
                                                                         
                                                  
   
static List *
generate_subquery_vars(PlannerInfo *root, List *tlist, Index varno)
{
  List *result;
  ListCell *lc;

  result = NIL;
  foreach (lc, tlist)
  {
    TargetEntry *tent = (TargetEntry *)lfirst(lc);
    Var *var;

    if (tent->resjunk)
    {
      continue;
    }

    var = makeVarFromTargetEntry(varno, tent);
    result = lappend(result, var);
  }

  return result;
}

   
                                                                   
                                                                          
                                                                        
                                                                 
                                                       
   
static Node *
convert_testexpr(PlannerInfo *root, Node *testexpr, List *subst_nodes)
{
  convert_testexpr_context context;

  context.root = root;
  context.subst_nodes = subst_nodes;
  return convert_testexpr_mutator(testexpr, &context);
}

static Node *
convert_testexpr_mutator(Node *node, convert_testexpr_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_SUBLINK)
    {
      if (param->paramid <= 0 || param->paramid > list_length(context->subst_nodes))
      {
        elog(ERROR, "unexpected PARAM_SUBLINK ID: %d", param->paramid);
      }

         
                                                             
                                                                    
                                                     
         
      return (Node *)copyObject(list_nth(context->subst_nodes, param->paramid - 1));
    }
  }
  if (IsA(node, SubLink))
  {
       
                                                                       
                                                                           
                                                                           
       
                                                                       
                                                                         
                                                                         
                                                                        
                                                                      
                                                                  
                                                                    
                                                                       
       
                                                                       
                                                                        
                                                                    
       
    return node;
  }
  return expression_tree_mutator(node, convert_testexpr_mutator, (void *)context);
}

   
                                                                    
   
static bool
subplan_is_hashable(Plan *plan)
{
  double subquery_size;

     
                                                                            
                                                                             
                                                                            
                
     
  subquery_size = plan->plan_rows * (MAXALIGN(plan->plan_width) + MAXALIGN(SizeofHeapTupleHeader));
  if (subquery_size > work_mem * 1024L)
  {
    return false;
  }

  return true;
}

   
                                                                       
   
                                                                       
                                                       
   
static bool
testexpr_is_hashable(Node *testexpr, List *param_ids)
{
     
                                                                            
                                                               
     
  if (testexpr && IsA(testexpr, OpExpr))
  {
    if (test_opexpr_is_hashable((OpExpr *)testexpr, param_ids))
    {
      return true;
    }
  }
  else if (is_andclause(testexpr))
  {
    ListCell *l;

    foreach (l, ((BoolExpr *)testexpr)->args)
    {
      Node *andarg = (Node *)lfirst(l);

      if (!IsA(andarg, OpExpr))
      {
        return false;
      }
      if (!test_opexpr_is_hashable((OpExpr *)andarg, param_ids))
      {
        return false;
      }
    }
    return true;
  }

  return false;
}

static bool
test_opexpr_is_hashable(OpExpr *testexpr, List *param_ids)
{
     
                                                                       
                                                                    
                                                                             
                                                                          
                                                                          
                                             
     
  if (!hash_ok_operator(testexpr))
  {
    return false;
  }

     
                                                                          
                                                                           
                                                                           
                                                                          
                                                                           
                                                                         
                                                                             
                                                                        
                                                                  
     
  if (list_length(testexpr->args) != 2)
  {
    return false;
  }
  if (contain_exec_param((Node *)linitial(testexpr->args), param_ids))
  {
    return false;
  }
  if (contain_var_clause((Node *)lsecond(testexpr->args)))
  {
    return false;
  }
  return true;
}

   
                                         
   
                                                                          
                                   
   
static bool
hash_ok_operator(OpExpr *expr)
{
  Oid opid = expr->opno;

                                          
  if (list_length(expr->args) != 2)
  {
    return false;
  }
  if (opid == ARRAY_EQ_OP)
  {
                                                                          
                                                                         
    Node *leftarg = linitial(expr->args);

    return op_hashjoinable(opid, exprType(leftarg));
  }
  else
  {
                                                   
    HeapTuple tup;
    Form_pg_operator optup;

    tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(opid));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for operator %u", opid);
    }
    optup = (Form_pg_operator)GETSTRUCT(tup);
    if (!optup->oprcanhash || !func_strict(optup->oprcode))
    {
      ReleaseSysCache(tup);
      return false;
    }
    ReleaseSysCache(tup);
    return true;
  }
}

   
                             
   
                                                                      
                                                                              
                                                                          
                    
   
                                                                             
                                                                         
                                                                            
                                                                              
                                    
   
void
SS_make_multiexprs_unique(PlannerInfo *root, PlannerInfo *subroot)
{
  List *param_mapping = NIL;
  ListCell *lc;

     
                                                                            
                                  
     
  foreach (lc, subroot->parse->targetList)
  {
    TargetEntry *tent = (TargetEntry *)lfirst(lc);
    SubPlan *splan;
    Plan *plan;
    List *params;
    int oldId, newId;
    ListCell *lc2;

    if (!IsA(tent->expr, SubPlan))
    {
      continue;
    }
    splan = (SubPlan *)tent->expr;
    if (splan->subLinkType != MULTIEXPR_SUBLINK)
    {
      continue;
    }

                                               
    plan = (Plan *)list_nth(root->glob->subplans, splan->plan_id - 1);

       
                                                                          
                                                                         
                                                                           
                                                                       
                                                       
       
    params = generate_subquery_params(root, plan->targetlist, &splan->setParam);

       
                                                                         
                                                                    
                
       
    root->multiexpr_params = lappend(root->multiexpr_params, params);
    oldId = splan->subLinkId;
    newId = list_length(root->multiexpr_params);
    Assert(newId > oldId);
    splan->subLinkId = newId;

       
                                                                      
                                                                  
                                                                        
                  
       
    while (list_length(param_mapping) < oldId)
    {
      param_mapping = lappend_int(param_mapping, 0);
    }
    lc2 = list_nth_cell(param_mapping, oldId - 1);
    lfirst_int(lc2) = newId;
  }

     
                                                                             
                                                                           
                                                                          
                                                                             
                                                             
     
  if (param_mapping != NIL)
  {
    SS_make_multiexprs_unique_walker((Node *)subroot->parse->targetList, (void *)param_mapping);
  }
}

   
                                                                              
                                                                 
   
static bool
SS_make_multiexprs_unique_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    Param *p = (Param *)node;
    List *param_mapping = (List *)context;
    int subqueryid;
    int colno;
    int newId;

    if (p->paramkind != PARAM_MULTIEXPR)
    {
      return false;
    }
    subqueryid = p->paramid >> 16;
    colno = p->paramid & 0xFFFF;

       
                                                                       
                                            
       
    Assert(subqueryid > 0);
    if (subqueryid > list_length(param_mapping))
    {
      return false;
    }
    newId = list_nth_int(param_mapping, subqueryid - 1);
    if (newId == 0)
    {
      return false;
    }
    p->paramid = (newId << 16) + colno;
    return false;
  }
  return expression_tree_walker(node, SS_make_multiexprs_unique_walker, context);
}

   
                                                
   
                                                                       
                                                                             
                                 
   
                                                                   
                                                                  
                                                                          
   
void
SS_process_ctes(PlannerInfo *root)
{
  ListCell *lc;

  Assert(root->cte_plan_ids == NIL);

  foreach (lc, root->parse->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);
    CmdType cmdType = ((Query *)cte->ctequery)->commandType;
    Query *subquery;
    PlannerInfo *subroot;
    RelOptInfo *final_rel;
    Path *best_path;
    Plan *plan;
    SubPlan *splan;
    int paramid;

       
                                                                     
       
    if (cte->cterefcount == 0 && cmdType == CMD_SELECT)
    {
                                              
      root->cte_plan_ids = lappend_int(root->cte_plan_ids, -1);
      continue;
    }

       
                                                                           
                                                    
       
                                                         
       
                                                                  
       
                                
       
                                                                           
                                                                        
                                             
       
                                                                          
                                                                          
                                                          
       
                                                                           
                                                                          
                                                                         
                                                                           
                                                                      
                                                                        
                                                                         
                                                                         
                                                    
       
                                                                       
                                              
       
    if ((cte->ctematerialized == CTEMaterializeNever || (cte->ctematerialized == CTEMaterializeDefault && cte->cterefcount == 1)) && !cte->cterecursive && cmdType == CMD_SELECT && !contain_dml(cte->ctequery) && (cte->cterefcount <= 1 || !contain_outer_selfref(cte->ctequery)) && !contain_volatile_functions(cte->ctequery))
    {
      inline_cte(root, cte);
                                              
      root->cte_plan_ids = lappend_int(root->cte_plan_ids, -1);
      continue;
    }

       
                                                                           
                                     
       
    subquery = (Query *)copyObject(cte->ctequery);

                                                                 
    Assert(root->plan_params == NIL);

       
                                                                         
                                                           
       
    subroot = subquery_planner(root->glob, subquery, root, cte->cterecursive, 0.0);

       
                                                                      
                                                                          
                   
       
    if (root->plan_params)
    {
      elog(ERROR, "unexpected outer reference in CTE query");
    }

       
                                                                          
                                               
       
    final_rel = fetch_upper_rel(subroot, UPPERREL_FINAL, NULL);
    best_path = final_rel->cheapest_total_path;

    plan = create_plan(subroot, best_path);

       
                                                               
                                               
       
                                                                      
       
    splan = makeNode(SubPlan);
    splan->subLinkType = CTE_SUBLINK;
    splan->subLinkId = 0;
    splan->testexpr = NULL;
    splan->paramIds = NIL;
    get_first_col_type(plan, &splan->firstColType, &splan->firstColTypmod, &splan->firstColCollation);
    splan->useHashTable = false;
    splan->unknownEqFalse = false;

       
                                                        
                                                                           
                      
       
    splan->parallel_safe = false;
    splan->setParam = NIL;
    splan->parParam = NIL;
    splan->args = NIL;

       
                                                                       
                                                                           
                                                                  
                                      
       

       
                                                                     
                                                                          
                                                                       
                                                                         
                                                                    
                                                                
       
    paramid = assign_special_exec_param(root);
    splan->setParam = list_make1_int(paramid);

       
                                                                
       
    root->glob->subplans = lappend(root->glob->subplans, plan);
    root->glob->subroots = lappend(root->glob->subroots, subroot);
    splan->plan_id = list_length(root->glob->subplans);

    root->init_plans = lappend(root->init_plans, splan);

    root->cte_plan_ids = lappend_int(root->cte_plan_ids, splan->plan_id);

                                                
    splan->plan_name = psprintf("CTE %s", cte->ctename);

                                                          
    cost_subplan(root, splan, plan);
  }
}

   
                                                    
   
                                                            
   
static bool
contain_dml(Node *node)
{
  return contain_dml_walker(node, NULL);
}

static bool
contain_dml_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Query))
  {
    Query *query = (Query *)node;

    if (query->commandType != CMD_SELECT || query->rowMarks != NIL)
    {
      return true;
    }

    return query_tree_walker(query, contain_dml_walker, context, 0);
  }
  return expression_tree_walker(node, contain_dml_walker, context);
}

   
                                                                         
   
static bool
contain_outer_selfref(Node *node)
{
  Index depth = 0;

     
                                                                       
                                       
     
  Assert(IsA(node, Query));

  return contain_outer_selfref_walker(node, &depth);
}

static bool
contain_outer_selfref_walker(Node *node, Index *depth)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, RangeTblEntry))
  {
    RangeTblEntry *rte = (RangeTblEntry *)node;

       
                                                                           
                          
       
    if (rte->rtekind == RTE_CTE && rte->self_reference && rte->ctelevelsup >= *depth)
    {
      return true;
    }
    return false;                                           
  }
  if (IsA(node, Query))
  {
                                                                
    Query *query = (Query *)node;
    bool result;

    (*depth)++;

    result = query_tree_walker(query, contain_outer_selfref_walker, (void *)depth, QTW_EXAMINE_RTES_BEFORE);

    (*depth)--;

    return result;
  }
  return expression_tree_walker(node, contain_outer_selfref_walker, (void *)depth);
}

   
                                                                          
   
static void
inline_cte(PlannerInfo *root, CommonTableExpr *cte)
{
  struct inline_cte_walker_context context;

  context.ctename = cte->ctename;
                                                                     
  context.levelsup = -1;
  context.ctequery = castNode(Query, cte->ctequery);

  (void)inline_cte_walker((Node *)root->parse, &context);
}

static bool
inline_cte_walker(Node *node, inline_cte_walker_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Query))
  {
    Query *query = (Query *)node;

    context->levelsup++;

       
                                                                   
                                                                         
                            
       
    (void)query_tree_walker(query, inline_cte_walker, context, QTW_EXAMINE_RTES_AFTER);

    context->levelsup--;

    return false;
  }
  else if (IsA(node, RangeTblEntry))
  {
    RangeTblEntry *rte = (RangeTblEntry *)node;

    if (rte->rtekind == RTE_CTE && strcmp(rte->ctename, context->ctename) == 0 && rte->ctelevelsup == context->levelsup)
    {
         
                                                                         
                                                                       
                         
         
      Query *newquery = copyObject(context->ctequery);

      if (context->levelsup > 0)
      {
        IncrementVarSublevelsUp((Node *)newquery, context->levelsup, 1);
      }

         
                                                      
         
                                                                         
                                                                         
                                                                 
                   
         
      rte->rtekind = RTE_SUBQUERY;
      rte->subquery = newquery;
      rte->security_barrier = false;

                                        
      rte->ctename = NULL;
      rte->ctelevelsup = 0;
      rte->self_reference = false;
      rte->coltypes = NIL;
      rte->coltypmods = NIL;
      rte->colcollations = NIL;
    }

    return false;
  }

  return expression_tree_walker(node, inline_cte_walker, context);
}

   
                                                                        
   
                                                                              
                                                                            
                                                                           
                                                                            
                           
   
                                                                           
                                                                          
                                                                         
                                                                        
                                                                         
   
                                                                             
                                                                          
                                                                              
                                                                        
                                                                        
                                                                            
                                                                     
                                                                            
                                      
   
                                                                       
                                                                               
                                                                             
                                                        
   
                                                                        
                                                                        
                        
   
JoinExpr *
convert_ANY_sublink_to_join(PlannerInfo *root, SubLink *sublink, Relids available_rels)
{
  JoinExpr *result;
  Query *parse = root->parse;
  Query *subselect = (Query *)sublink->subselect;
  Relids upper_varnos;
  int rtindex;
  RangeTblEntry *rte;
  RangeTblRef *rtr;
  List *subquery_vars;
  Node *quals;
  ParseState *pstate;

  Assert(sublink->subLinkType == ANY_SUBLINK);

     
                                                                             
                                            
     
  if (contain_vars_of_level((Node *)subselect, 1))
  {
    return NULL;
  }

     
                                                                          
                                                                           
                                   
     
  upper_varnos = pull_varnos(root, sublink->testexpr);
  if (bms_is_empty(upper_varnos))
  {
    return NULL;
  }

     
                                                                 
     
  if (!bms_is_subset(upper_varnos, available_rels))
  {
    return NULL;
  }

     
                                                                            
     
  if (contain_volatile_functions(sublink->testexpr))
  {
    return NULL;
  }

                                                                   
  pstate = make_parsestate(NULL);

     
                                                          
     
                                                                           
                                                                       
                                                                             
                    
     
  rte = addRangeTableEntryForSubquery(pstate, subselect, makeAlias("ANY_subquery", NIL), false, false);
  parse->rtable = lappend(parse->rtable, rte);
  rtindex = list_length(parse->rtable);

     
                                                      
     
  rtr = makeNode(RangeTblRef);
  rtr->rtindex = rtindex;

     
                                                              
     
  subquery_vars = generate_subquery_vars(root, subselect->targetList, rtindex);

     
                                                                             
     
  quals = convert_testexpr(root, sublink->testexpr, subquery_vars);

     
                                           
     
  result = makeNode(JoinExpr);
  result->jointype = JOIN_SEMI;
  result->isNatural = false;
  result->larg = NULL;                               
  result->rarg = (Node *)rtr;
  result->usingClause = NIL;
  result->quals = quals;
  result->alias = NULL;
  result->rtindex = 0;                                  

  return result;
}

   
                                                                              
   
                                                                           
                                                                               
                                                         
   
JoinExpr *
convert_EXISTS_sublink_to_join(PlannerInfo *root, SubLink *sublink, bool under_not, Relids available_rels)
{
  JoinExpr *result;
  Query *parse = root->parse;
  Query *subselect = (Query *)sublink->subselect;
  Node *whereClause;
  int rtoffset;
  int varno;
  Relids clause_varnos;
  Relids upper_varnos;

  Assert(sublink->subLinkType == EXISTS_SUBLINK);

     
                                                                          
                                                                       
                                                                            
                                                                          
                                                                           
                                          
     
  if (subselect->cteList)
  {
    return NULL;
  }

     
                                                                   
                    
     
  subselect = copyObject(subselect);

     
                                                                            
                                                                  
                                                                         
                                              
     
  if (!simplify_EXISTS_query(root, subselect))
  {
    return NULL;
  }

     
                                                                         
                                                                      
               
     
  whereClause = subselect->jointree->quals;
  subselect->jointree->quals = NULL;

     
                                                                         
                                                             
     
  if (contain_vars_of_level((Node *)subselect, 1))
  {
    return NULL;
  }

     
                                                                       
                                                  
     
  if (!contain_vars_of_level(whereClause, 1))
  {
    return NULL;
  }

     
                                                                       
     
  if (contain_volatile_functions(whereClause))
  {
    return NULL;
  }

     
                                                                        
     
  replace_empty_jointree(subselect);

     
                                                             
     
                                                                           
                                                                          
                                                
     
                                                                            
                                                                          
                                                                            
                                                                            
                                                                            
                                                                            
             
     
  rtoffset = list_length(parse->rtable);
  OffsetVarNodes((Node *)subselect, rtoffset, 0);
  OffsetVarNodes(whereClause, rtoffset, 0);

     
                                                                        
                                                                       
                         
     
  IncrementVarSublevelsUp((Node *)subselect, -1, 1);
  IncrementVarSublevelsUp(whereClause, -1, 1);

     
                                                                     
                                                                          
                                                                            
          
     
  clause_varnos = pull_varnos(root, whereClause);
  upper_varnos = NULL;
  while ((varno = bms_first_member(clause_varnos)) >= 0)
  {
    if (varno <= rtoffset)
    {
      upper_varnos = bms_add_member(upper_varnos, varno);
    }
  }
  bms_free(clause_varnos);
  Assert(!bms_is_empty(upper_varnos));

     
                                                                            
                                                   
     
  if (!bms_is_subset(upper_varnos, available_rels))
  {
    return NULL;
  }

                                                                    
  parse->rtable = list_concat(parse->rtable, subselect->rtable);

     
                                           
     
  result = makeNode(JoinExpr);
  result->jointype = under_not ? JOIN_ANTI : JOIN_SEMI;
  result->isNatural = false;
  result->larg = NULL;                               
                                                     
  if (list_length(subselect->jointree->fromlist) == 1)
  {
    result->rarg = (Node *)linitial(subselect->jointree->fromlist);
  }
  else
  {
    result->rarg = (Node *)subselect->jointree;
  }
  result->usingClause = NIL;
  result->quals = whereClause;
  result->alias = NULL;
  result->rtindex = 0;                                  

  return result;
}

   
                                                                           
   
                                                                           
                                                                               
                                                                             
                                                                             
                                                                         
   
                                                                               
                                                                           
                                                                               
                                          
   
                                                                   
   
static bool
simplify_EXISTS_query(PlannerInfo *root, Query *query)
{
     
                                                                       
                                                                             
                                                                       
                                                                         
                                                                            
             
     
  if (query->commandType != CMD_SELECT || query->setOperations || query->hasAggs || query->groupingSets || query->hasWindowFuncs || query->hasTargetSRFs || query->hasModifyingCTE || query->havingQual || query->limitOffset || query->rowMarks)
  {
    return false;
  }

     
                                                                       
                                                                             
                                                                           
                                                                             
                                                                      
     
  if (query->limitCount)
  {
       
                                                                         
                                                                          
                                                                        
                                                                           
                                                                        
       
    Node *node = eval_const_expressions(root, query->limitCount);
    Const *limit;

                                                                     
    query->limitCount = node;

    if (!IsA(node, Const))
    {
      return false;
    }

    limit = (Const *)node;
    Assert(limit->consttype == INT8OID);
    if (!limit->constisnull && DatumGetInt64(limit->constvalue) <= 0)
    {
      return false;
    }

                                                                       
    query->limitCount = NULL;
  }

     
                                                                        
                                                                        
                                                                             
                                                                        
                                                                         
     
  query->targetList = NIL;
  query->groupClause = NIL;
  query->windowClause = NIL;
  query->distinctClause = NIL;
  query->sortClause = NIL;
  query->hasDistinctOn = false;

  return true;
}

   
                                                                          
   
                                                                      
                                                                       
   
                                                                           
                                                                            
                                                                            
                                                                           
                                       
   
                             
   
static Query *
convert_EXISTS_to_ANY(PlannerInfo *root, Query *subselect, Node **testexpr, List **paramIds)
{
  Node *whereClause;
  List *leftargs, *rightargs, *opids, *opcollations, *newWhere, *tlist, *testlist, *paramids;
  ListCell *lc, *rc, *oc, *cc;
  AttrNumber resno;

     
                                                                             
                                                     
     
  Assert(subselect->targetList == NIL);

     
                                                                         
                                                                      
               
     
  whereClause = subselect->jointree->quals;
  subselect->jointree->quals = NULL;

     
                                                                         
                                                             
     
                                                                        
                                                                         
                                                                       
     
  if (contain_vars_of_level((Node *)subselect, 1))
  {
    return NULL;
  }

     
                                                                       
     
  if (contain_volatile_functions(whereClause))
  {
    return NULL;
  }

     
                                                                        
                                                                      
                                                                             
                                                                      
                                                                            
                                                                             
                                                                           
                                                                
     
                                                                          
                                                                            
                                                      
     
                                                                  
                                                                   
                                                                     
              
     
  whereClause = eval_const_expressions(root, whereClause);
  whereClause = (Node *)canonicalize_qual((Expr *)whereClause, false);
  whereClause = (Node *)make_ands_implicit((Expr *)whereClause);

     
                                                                           
                                                                        
                                                                             
                                                                         
                                                                
     
  leftargs = rightargs = opids = opcollations = newWhere = NIL;
  foreach (lc, (List *)whereClause)
  {
    OpExpr *expr = (OpExpr *)lfirst(lc);

    if (IsA(expr, OpExpr) && hash_ok_operator(expr))
    {
      Node *leftarg = (Node *)linitial(expr->args);
      Node *rightarg = (Node *)lsecond(expr->args);

      if (contain_vars_of_level(leftarg, 1))
      {
        leftargs = lappend(leftargs, leftarg);
        rightargs = lappend(rightargs, rightarg);
        opids = lappend_oid(opids, expr->opno);
        opcollations = lappend_oid(opcollations, expr->inputcollid);
        continue;
      }
      if (contain_vars_of_level(rightarg, 1))
      {
           
                                                                  
                                                                   
                                                                    
                                                                 
           
        expr->opno = get_commutator(expr->opno);
        if (OidIsValid(expr->opno) && hash_ok_operator(expr))
        {
          leftargs = lappend(leftargs, rightarg);
          rightargs = lappend(rightargs, leftarg);
          opids = lappend_oid(opids, expr->opno);
          opcollations = lappend_oid(opcollations, expr->inputcollid);
          continue;
        }
                                                                      
        return NULL;
      }
    }
                                             
    newWhere = lappend(newWhere, expr);
  }

     
                                                        
     
  if (leftargs == NIL)
  {
    return NULL;
  }

     
                                                                             
                                                                            
                                                                       
                                                                           
                                                             
     
                                                     
     
  if (contain_vars_of_level((Node *)newWhere, 1) || contain_vars_of_level((Node *)rightargs, 1))
  {
    return NULL;
  }
  if (root->parse->hasAggs && (contain_aggs_of_level((Node *)newWhere, 1) || contain_aggs_of_level((Node *)rightargs, 1)))
  {
    return NULL;
  }

     
                                                                          
                                                                            
                                                                           
                                 
     
  if (contain_vars_of_level((Node *)leftargs, 0))
  {
    return NULL;
  }

     
                                                                           
                                                                         
     
  if (contain_subplans((Node *)leftargs))
  {
    return NULL;
  }

     
                                                                 
     
  IncrementVarSublevelsUp((Node *)leftargs, -1, 1);

     
                                                  
     
  if (newWhere)
  {
    subselect->jointree->quals = (Node *)make_ands_explicit(newWhere);
  }

     
                                                                        
                                                                          
                                                                            
                                                                      
     
  tlist = testlist = paramids = NIL;
  resno = 1;
  forfour(lc, leftargs, rc, rightargs, oc, opids, cc, opcollations)
  {
    Node *leftarg = (Node *)lfirst(lc);
    Node *rightarg = (Node *)lfirst(rc);
    Oid opid = lfirst_oid(oc);
    Oid opcollation = lfirst_oid(cc);
    Param *param;

    param = generate_new_exec_param(root, exprType(rightarg), exprTypmod(rightarg), exprCollation(rightarg));
    tlist = lappend(tlist, makeTargetEntry((Expr *)rightarg, resno++, NULL, false));
    testlist = lappend(testlist, make_opclause(opid, BOOLOID, false, (Expr *)leftarg, (Expr *)param, InvalidOid, opcollation));
    paramids = lappend_int(paramids, param->paramid);
  }

                                                         
  subselect->targetList = tlist;
  *testexpr = (Node *)make_ands_explicit(testlist);
  *paramIds = paramids;

  return subselect;
}

   
                                                        
   
                                                             
   
                                                                              
                                                                              
                                                                           
                                                                              
                                                                             
                                                                       
                                                                           
                                                                              
                                                                              
                                                                           
                                                                              
                                  
   
                                                                            
                                                                               
                                                                               
                                                                              
                                                      
   
Node *
SS_replace_correlation_vars(PlannerInfo *root, Node *expr)
{
                                                    
  return replace_correlation_vars_mutator(expr, root);
}

static Node *
replace_correlation_vars_mutator(Node *node, PlannerInfo *root)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    if (((Var *)node)->varlevelsup > 0)
    {
      return (Node *)replace_outer_var(root, (Var *)node);
    }
  }
  if (IsA(node, PlaceHolderVar))
  {
    if (((PlaceHolderVar *)node)->phlevelsup > 0)
    {
      return (Node *)replace_outer_placeholdervar(root, (PlaceHolderVar *)node);
    }
  }
  if (IsA(node, Aggref))
  {
    if (((Aggref *)node)->agglevelsup > 0)
    {
      return (Node *)replace_outer_agg(root, (Aggref *)node);
    }
  }
  if (IsA(node, GroupingFunc))
  {
    if (((GroupingFunc *)node)->agglevelsup > 0)
    {
      return (Node *)replace_outer_grouping(root, (GroupingFunc *)node);
    }
  }
  return expression_tree_mutator(node, replace_correlation_vars_mutator, (void *)root);
}

   
                                                        
   
                                                                              
                                                                             
                                                     
   
Node *
SS_process_sublinks(PlannerInfo *root, Node *expr, bool isQual)
{
  process_sublinks_context context;

  context.root = root;
  context.isTopQual = isQual;
  return process_sublinks_mutator(expr, &context);
}

static Node *
process_sublinks_mutator(Node *node, process_sublinks_context *context)
{
  process_sublinks_context locContext;

  locContext.root = context->root;

  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, SubLink))
  {
    SubLink *sublink = (SubLink *)node;
    Node *testexpr;

       
                                                                         
                                      
       
    locContext.isTopQual = false;
    testexpr = process_sublinks_mutator(sublink->testexpr, &locContext);

       
                                                               
       
    return make_subplan(context->root, (Query *)sublink->subselect, sublink->subLinkType, sublink->subLinkId, testexpr, context->isTopQual);
  }

     
                                                                 
                                                                             
                                                                     
                                                                       
                                         
     
  if (IsA(node, PlaceHolderVar))
  {
    if (((PlaceHolderVar *)node)->phlevelsup > 0)
    {
      return node;
    }
  }
  else if (IsA(node, Aggref))
  {
    if (((Aggref *)node)->agglevelsup > 0)
    {
      return node;
    }
  }
  else if (IsA(node, GroupingFunc))
  {
    if (((GroupingFunc *)node)->agglevelsup > 0)
    {
      return node;
    }
  }

     
                                                                          
                                                                          
                                                    
     
  Assert(!IsA(node, SubPlan));
  Assert(!IsA(node, AlternativeSubPlan));
  Assert(!IsA(node, Query));

     
                                                                         
                                                                            
                                                                 
     
                                                                             
                                                                             
                                 
     
                                                                        
                                                                           
                                                                           
                                                                 
     
  if (is_andclause(node))
  {
    List *newargs = NIL;
    ListCell *l;

                                 
    locContext.isTopQual = context->isTopQual;

    foreach (l, ((BoolExpr *)node)->args)
    {
      Node *newarg;

      newarg = process_sublinks_mutator(lfirst(l), &locContext);
      if (is_andclause(newarg))
      {
        newargs = list_concat(newargs, ((BoolExpr *)newarg)->args);
      }
      else
      {
        newargs = lappend(newargs, newarg);
      }
    }
    return (Node *)make_andclause(newargs);
  }

  if (is_orclause(node))
  {
    List *newargs = NIL;
    ListCell *l;

                                 
    locContext.isTopQual = context->isTopQual;

    foreach (l, ((BoolExpr *)node)->args)
    {
      Node *newarg;

      newarg = process_sublinks_mutator(lfirst(l), &locContext);
      if (is_orclause(newarg))
      {
        newargs = list_concat(newargs, ((BoolExpr *)newarg)->args);
      }
      else
      {
        newargs = lappend(newargs, newarg);
      }
    }
    return (Node *)make_orclause(newargs);
  }

     
                                                                          
                                                   
     
  locContext.isTopQual = false;

  return expression_tree_mutator(node, process_sublinks_mutator, (void *)&locContext);
}

   
                                                                              
   
                                                                              
                                                                       
                                                                             
                                                                           
                                                                          
                                                                              
                                                                            
                                                              
   
void
SS_identify_outer_params(PlannerInfo *root)
{
  Bitmapset *outer_params;
  PlannerInfo *proot;
  ListCell *l;

     
                                                                            
                                     
     
  if (root->glob->paramExecTypes == NIL)
  {
    return;
  }

     
                                                                             
                                                                    
                                                                             
                                
     
  outer_params = NULL;
  for (proot = root->parent_root; proot != NULL; proot = proot->parent_root)
  {
                                                             
    foreach (l, proot->plan_params)
    {
      PlannerParamItem *pitem = (PlannerParamItem *)lfirst(l);

      outer_params = bms_add_member(outer_params, pitem->paramId);
    }
                                                      
    foreach (l, proot->init_plans)
    {
      SubPlan *initsubplan = (SubPlan *)lfirst(l);
      ListCell *l2;

      foreach (l2, initsubplan->setParam)
      {
        outer_params = bms_add_member(outer_params, lfirst_int(l2));
      }
    }
                                                                     
    if (proot->wt_param_id >= 0)
    {
      outer_params = bms_add_member(outer_params, proot->wt_param_id);
    }
  }
  root->outer_params = outer_params;
}

   
                                                                               
   
                                                                            
                                                                            
                                                                              
                                                                         
                                                     
   
                                                                            
                                                                          
                                                                           
                                        
   
void
SS_charge_for_initplans(PlannerInfo *root, RelOptInfo *final_rel)
{
  Cost initplan_cost;
  ListCell *lc;

                                     
  if (root->init_plans == NIL)
  {
    return;
  }

     
                                                                             
                                                                            
                                                                             
                                                           
     
  initplan_cost = 0;
  foreach (lc, root->init_plans)
  {
    SubPlan *initsubplan = (SubPlan *)lfirst(lc);

    initplan_cost += initsubplan->startup_cost + initsubplan->per_call_cost;
  }

     
                                                   
     
  foreach (lc, final_rel->pathlist)
  {
    Path *path = (Path *)lfirst(lc);

    path->startup_cost += initplan_cost;
    path->total_cost += initplan_cost;
    path->parallel_safe = false;
  }

     
                                                                      
                                                    
     
  final_rel->partial_pathlist = NIL;
  final_rel->consider_parallel = false;

                                                            
}

   
                                                               
   
                                                                            
                                                                             
                                                                              
                                                                        
                                                                            
                                                                        
                                  
   
void
SS_attach_initplans(PlannerInfo *root, Plan *plan)
{
  plan->initPlan = root->init_plans;
}

   
                                                                          
   
                                                                           
                                                                            
   
                                                                            
                                           
   
void
SS_finalize_plan(PlannerInfo *root, Plan *plan)
{
                                                        
  (void)finalize_plan(root, plan, -1, root->outer_params, NULL);
}

   
                                                      
   
                                                                        
                           
   
                                                                      
                                                                  
   
                                                                            
                                                                          
              
   
                                                                          
                                                                       
                                                                      
                                                      
   
                                                                      
   
                                                                              
                                                                               
                                                                               
                                                                              
                                                                              
                                                                          
                                                                              
                                                                             
                                                                             
                                
   
static Bitmapset *
finalize_plan(PlannerInfo *root, Plan *plan, int gather_param, Bitmapset *valid_params, Bitmapset *scan_params)
{
  finalize_primnode_context context;
  int locally_added_param;
  Bitmapset *nestloop_params;
  Bitmapset *initExtParam;
  Bitmapset *initSetParam;
  Bitmapset *child_params;
  ListCell *l;

  if (plan == NULL)
  {
    return NULL;
  }

  context.root = root;
  context.paramids = NULL;                               
  locally_added_param = -1;                      
  nestloop_params = NULL;                         

     
                                                                        
                                                                     
                                                
     
  initExtParam = initSetParam = NULL;
  foreach (l, plan->initPlan)
  {
    SubPlan *initsubplan = (SubPlan *)lfirst(l);
    Plan *initplan = planner_subplan_get_plan(root, initsubplan);
    ListCell *l2;

    initExtParam = bms_add_members(initExtParam, initplan->extParam);
    foreach (l2, initsubplan->setParam)
    {
      initSetParam = bms_add_member(initSetParam, lfirst_int(l2));
    }
  }

                                                                         
  if (initSetParam)
  {
    valid_params = bms_union(valid_params, initSetParam);
  }

     
                                                                             
                                                                             
                                                                             
                    
     

                                          
  finalize_primnode((Node *)plan->targetlist, &context);
  finalize_primnode((Node *)plan->qual, &context);

     
                                                                            
                                        
     
  if (plan->parallel_aware)
  {
    if (gather_param < 0)
    {
      elog(ERROR, "parallel-aware plan node is not below a Gather");
    }
    context.paramids = bms_add_member(context.paramids, gather_param);
  }

                                                  
  switch (nodeTag(plan))
  {
  case T_Result:
    finalize_primnode(((Result *)plan)->resconstantqual, &context);
    break;

  case T_SeqScan:
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_SampleScan:
    finalize_primnode((Node *)((SampleScan *)plan)->tablesample, &context);
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_IndexScan:
    finalize_primnode((Node *)((IndexScan *)plan)->indexqual, &context);
    finalize_primnode((Node *)((IndexScan *)plan)->indexorderby, &context);

       
                                                                      
                                                               
                         
       
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_IndexOnlyScan:
    finalize_primnode((Node *)((IndexOnlyScan *)plan)->indexqual, &context);
    finalize_primnode((Node *)((IndexOnlyScan *)plan)->recheckqual, &context);
    finalize_primnode((Node *)((IndexOnlyScan *)plan)->indexorderby, &context);

       
                                                                       
       
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_BitmapIndexScan:
    finalize_primnode((Node *)((BitmapIndexScan *)plan)->indexqual, &context);

       
                                                                      
                                      
       
    break;

  case T_BitmapHeapScan:
    finalize_primnode((Node *)((BitmapHeapScan *)plan)->bitmapqualorig, &context);
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_TidScan:
    finalize_primnode((Node *)((TidScan *)plan)->tidquals, &context);
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_SubqueryScan:
  {
    SubqueryScan *sscan = (SubqueryScan *)plan;
    RelOptInfo *rel;
    Bitmapset *subquery_params;

                                                   
    rel = find_base_rel(root, sscan->scan.scanrelid);
    subquery_params = rel->subroot->outer_params;
    if (gather_param >= 0)
    {
      subquery_params = bms_add_member(bms_copy(subquery_params), gather_param);
    }
    finalize_plan(rel->subroot, sscan->subplan, gather_param, subquery_params, NULL);

                                                             
    context.paramids = bms_add_members(context.paramids, sscan->subplan->extParam);
                                         
    context.paramids = bms_add_members(context.paramids, scan_params);
  }
  break;

  case T_FunctionScan:
  {
    FunctionScan *fscan = (FunctionScan *)plan;
    ListCell *lc;

       
                                                             
                                                          
                                                         
                                    
       
    foreach (lc, fscan->functions)
    {
      RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);
      finalize_primnode_context funccontext;

      funccontext = context;
      funccontext.paramids = NULL;

      finalize_primnode(rtfunc->funcexpr, &funccontext);

                                          
      rtfunc->funcparams = funccontext.paramids;

                                                        
      context.paramids = bms_add_members(context.paramids, funccontext.paramids);
    }

    context.paramids = bms_add_members(context.paramids, scan_params);
  }
  break;

  case T_TableFuncScan:
    finalize_primnode((Node *)((TableFuncScan *)plan)->tablefunc, &context);
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_ValuesScan:
    finalize_primnode((Node *)((ValuesScan *)plan)->values_lists, &context);
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_CteScan:
  {
       
                                                            
                                                               
                                                                 
                                                                   
                                                                 
                                                              
                                                            
                                                             
                                                          
       
    int plan_id = ((CteScan *)plan)->ctePlanId;
    Plan *cteplan;

                         
    if (plan_id < 1 || plan_id > list_length(root->glob->subplans))
    {
      elog(ERROR, "could not find plan for CteScan referencing plan ID %d", plan_id);
    }
    cteplan = (Plan *)list_nth(root->glob->subplans, plan_id - 1);
    context.paramids = bms_add_members(context.paramids, cteplan->extParam);

#ifdef NOT_USED
                          
    context.paramids = bms_add_member(context.paramids, ((CteScan *)plan)->cteParam);
#endif

    context.paramids = bms_add_members(context.paramids, scan_params);
  }
  break;

  case T_WorkTableScan:
    context.paramids = bms_add_member(context.paramids, ((WorkTableScan *)plan)->wtParam);
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_NamedTuplestoreScan:
    context.paramids = bms_add_members(context.paramids, scan_params);
    break;

  case T_ForeignScan:
  {
    ForeignScan *fscan = (ForeignScan *)plan;

    finalize_primnode((Node *)fscan->fdw_exprs, &context);
    finalize_primnode((Node *)fscan->fdw_recheck_quals, &context);

                                                        
    context.paramids = bms_add_members(context.paramids, scan_params);
  }
  break;

  case T_CustomScan:
  {
    CustomScan *cscan = (CustomScan *)plan;
    ListCell *lc;

    finalize_primnode((Node *)cscan->custom_exprs, &context);
                                                           
    context.paramids = bms_add_members(context.paramids, scan_params);

                            
    foreach (lc, cscan->custom_plans)
    {
      context.paramids = bms_add_members(context.paramids, finalize_plan(root, (Plan *)lfirst(lc), gather_param, valid_params, scan_params));
    }
  }
  break;

  case T_ModifyTable:
  {
    ModifyTable *mtplan = (ModifyTable *)plan;
    ListCell *l;

                                                           
    locally_added_param = mtplan->epqParam;
    valid_params = bms_add_member(bms_copy(valid_params), locally_added_param);
    scan_params = bms_add_member(bms_copy(scan_params), locally_added_param);
    finalize_primnode((Node *)mtplan->returningLists, &context);
    finalize_primnode((Node *)mtplan->onConflictSet, &context);
    finalize_primnode((Node *)mtplan->onConflictWhere, &context);
                                                                   
    foreach (l, mtplan->plans)
    {
      context.paramids = bms_add_members(context.paramids, finalize_plan(root, (Plan *)lfirst(l), gather_param, valid_params, scan_params));
    }
  }
  break;

  case T_Append:
  {
    ListCell *l;

    foreach (l, ((Append *)plan)->appendplans)
    {
      context.paramids = bms_add_members(context.paramids, finalize_plan(root, (Plan *)lfirst(l), gather_param, valid_params, scan_params));
    }
  }
  break;

  case T_MergeAppend:
  {
    ListCell *l;

    foreach (l, ((MergeAppend *)plan)->mergeplans)
    {
      context.paramids = bms_add_members(context.paramids, finalize_plan(root, (Plan *)lfirst(l), gather_param, valid_params, scan_params));
    }
  }
  break;

  case T_BitmapAnd:
  {
    ListCell *l;

    foreach (l, ((BitmapAnd *)plan)->bitmapplans)
    {
      context.paramids = bms_add_members(context.paramids, finalize_plan(root, (Plan *)lfirst(l), gather_param, valid_params, scan_params));
    }
  }
  break;

  case T_BitmapOr:
  {
    ListCell *l;

    foreach (l, ((BitmapOr *)plan)->bitmapplans)
    {
      context.paramids = bms_add_members(context.paramids, finalize_plan(root, (Plan *)lfirst(l), gather_param, valid_params, scan_params));
    }
  }
  break;

  case T_NestLoop:
  {
    ListCell *l;

    finalize_primnode((Node *)((Join *)plan)->joinqual, &context);
                                                                  
    foreach (l, ((NestLoop *)plan)->nestParams)
    {
      NestLoopParam *nlp = (NestLoopParam *)lfirst(l);

      nestloop_params = bms_add_member(nestloop_params, nlp->paramno);
    }
  }
  break;

  case T_MergeJoin:
    finalize_primnode((Node *)((Join *)plan)->joinqual, &context);
    finalize_primnode((Node *)((MergeJoin *)plan)->mergeclauses, &context);
    break;

  case T_HashJoin:
    finalize_primnode((Node *)((Join *)plan)->joinqual, &context);
    finalize_primnode((Node *)((HashJoin *)plan)->hashclauses, &context);
    break;

  case T_Limit:
    finalize_primnode(((Limit *)plan)->limitOffset, &context);
    finalize_primnode(((Limit *)plan)->limitCount, &context);
    break;

  case T_RecursiveUnion:
                                                      
    locally_added_param = ((RecursiveUnion *)plan)->wtParam;
    valid_params = bms_add_member(bms_copy(valid_params), locally_added_param);
                                                     
    break;

  case T_LockRows:
                                                           
    locally_added_param = ((LockRows *)plan)->epqParam;
    valid_params = bms_add_member(bms_copy(valid_params), locally_added_param);
    scan_params = bms_add_member(bms_copy(scan_params), locally_added_param);
    break;

  case T_Agg:
  {
    Agg *agg = (Agg *)plan;

       
                                                                 
                                                                 
       
    if (agg->aggstrategy == AGG_HASHED)
    {
      finalize_primnode_context aggcontext;

      aggcontext.root = root;
      aggcontext.paramids = NULL;
      finalize_agg_primnode((Node *)agg->plan.targetlist, &aggcontext);
      finalize_agg_primnode((Node *)agg->plan.qual, &aggcontext);
      agg->aggParams = aggcontext.paramids;
    }
  }
  break;

  case T_WindowAgg:
    finalize_primnode(((WindowAgg *)plan)->startOffset, &context);
    finalize_primnode(((WindowAgg *)plan)->endOffset, &context);
    break;

  case T_Gather:
                                                                   
    locally_added_param = ((Gather *)plan)->rescan_param;
    if (locally_added_param >= 0)
    {
      valid_params = bms_add_member(bms_copy(valid_params), locally_added_param);

         
                                                                  
                                                                    
                                                   
         
      Assert(gather_param < 0);
                                                                
      gather_param = locally_added_param;
    }
                                                          
    break;

  case T_GatherMerge:
                                                                   
    locally_added_param = ((GatherMerge *)plan)->rescan_param;
    if (locally_added_param >= 0)
    {
      valid_params = bms_add_member(bms_copy(valid_params), locally_added_param);

         
                                                                  
                                                                    
                                                   
         
      Assert(gather_param < 0);
                                                                
      gather_param = locally_added_param;
    }
                                                          
    break;

  case T_ProjectSet:
  case T_Hash:
  case T_Material:
  case T_Sort:
  case T_Unique:
  case T_SetOp:
  case T_Group:
                                                  
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(plan));
  }

                                                  
  child_params = finalize_plan(root, plan->lefttree, gather_param, valid_params, scan_params);
  context.paramids = bms_add_members(context.paramids, child_params);

  if (nestloop_params)
  {
                                                                           
    child_params = finalize_plan(root, plan->righttree, gather_param, bms_union(nestloop_params, valid_params), scan_params);
                                                                 
    child_params = bms_difference(child_params, nestloop_params);
    bms_free(nestloop_params);
  }
  else
  {
                   
    child_params = finalize_plan(root, plan->righttree, gather_param, valid_params, scan_params);
  }
  context.paramids = bms_add_members(context.paramids, child_params);

     
                                                                          
                                                                           
                                                                            
                                
     
  if (locally_added_param >= 0)
  {
    context.paramids = bms_del_member(context.paramids, locally_added_param);
  }

                                                                         

  if (!bms_is_subset(context.paramids, valid_params))
  {
    elog(ERROR, "plan should not reference subplan's variable");
  }

     
                                                                         
                                                                       
                                                                          
                                                                          
                                                                           
     

                                                                
  plan->allParam = bms_union(context.paramids, initExtParam);
  plan->allParam = bms_add_members(plan->allParam, initSetParam);
                                                    
  plan->extParam = bms_union(context.paramids, initExtParam);
                                      
  plan->extParam = bms_del_members(plan->extParam, initSetParam);

     
                                                                           
                                  
     
  if (bms_is_empty(plan->extParam))
  {
    plan->extParam = NULL;
  }
  if (bms_is_empty(plan->allParam))
  {
    plan->allParam = NULL;
  }

  return plan->allParam;
}

   
                                                                              
                                      
   
static bool
finalize_primnode(Node *node, finalize_primnode_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    if (((Param *)node)->paramkind == PARAM_EXEC)
    {
      int paramid = ((Param *)node)->paramid;

      context->paramids = bms_add_member(context->paramids, paramid);
    }
    return false;                         
  }
  if (IsA(node, SubPlan))
  {
    SubPlan *subplan = (SubPlan *)node;
    Plan *plan = planner_subplan_get_plan(context->root, subplan);
    ListCell *lc;
    Bitmapset *subparamids;

                                                          
    finalize_primnode(subplan->testexpr, context);

       
                                                                          
                                                                  
                                                                           
                                                                           
                                                                           
                                                            
       
    foreach (lc, subplan->paramIds)
    {
      context->paramids = bms_del_member(context->paramids, lfirst_int(lc));
    }

                                
    finalize_primnode((Node *)subplan->args, context);

       
                                                                         
                                                                        
                             
       
    subparamids = bms_copy(plan->extParam);
    foreach (lc, subplan->parParam)
    {
      subparamids = bms_del_member(subparamids, lfirst_int(lc));
    }
    context->paramids = bms_join(context->paramids, subparamids);

    return false;                         
  }
  return expression_tree_walker(node, finalize_primnode, (void *)context);
}

   
                                                                              
                                                                          
                                
   
static bool
finalize_agg_primnode(Node *node, finalize_primnode_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
    Aggref *agg = (Aggref *)node;

                                                             
    finalize_primnode((Node *)agg->args, context);
    finalize_primnode((Node *)agg->aggfilter, context);
    return false;                                            
  }
  return expression_tree_walker(node, finalize_agg_primnode, (void *)context);
}

   
                                                                         
   
                                                                              
   
                                                                            
                                                                           
                   
   
Param *
SS_make_initplan_output_param(PlannerInfo *root, Oid resulttype, int32 resulttypmod, Oid resultcollation)
{
  return generate_new_exec_param(root, resulttype, resulttypmod, resultcollation);
}

   
                                                                       
   
                                                                      
                                                                           
                                                                         
   
void
SS_make_initplan_from_plan(PlannerInfo *root, PlannerInfo *subroot, Plan *plan, Param *prm)
{
  SubPlan *node;

     
                                                              
     
  root->glob->subplans = lappend(root->glob->subplans, plan);
  root->glob->subroots = lappend(root->glob->subroots, subroot);

     
                                                                           
                                                                        
                              
     
  node = makeNode(SubPlan);
  node->subLinkType = EXPR_SUBLINK;
  node->subLinkId = 0;
  node->plan_id = list_length(root->glob->subplans);
  node->plan_name = psprintf("InitPlan %d (returns $%d)", node->plan_id, prm->paramid);
  get_first_col_type(plan, &node->firstColType, &node->firstColTypmod, &node->firstColCollation);
  node->setParam = list_make1_int(prm->paramid);

  root->init_plans = lappend(root->init_plans, node);

     
                                                                     
                                           
     

                                                          
  cost_subplan(subroot, node, plan);
}
