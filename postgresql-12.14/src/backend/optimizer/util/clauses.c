                                                                            
   
             
                                                  
   
                                                                         
                                                                        
   
   
                  
                                          
   
           
                                 
                                                              
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_class.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/functions.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "rewrite/rewriteManip.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)

typedef struct
{
  PlannerInfo *root;
  AggSplit aggsplit;
  AggClauseCosts *costs;
} get_agg_clause_costs_context;

typedef struct
{
  ParamListInfo boundParams;
  PlannerInfo *root;
  List *active_fns;
  Node *case_val;
  bool estimate;
} eval_const_expressions_context;

typedef struct
{
  int nargs;
  List *args;
  int *usecounts;
} substitute_actual_parameters_context;

typedef struct
{
  int nargs;
  List *args;
  int sublevels_up;
} substitute_actual_srf_parameters_context;

typedef struct
{
  char *proname;
  char *prosrc;
} inline_error_callback_arg;

typedef struct
{
  char max_hazard;                                                 
  char max_interesting;                                           
  List *safe_param_ids;                                            
} max_parallel_hazard_context;

static bool
contain_agg_clause_walker(Node *node, void *context);
static bool
get_agg_clause_costs_walker(Node *node, get_agg_clause_costs_context *context);
static bool
find_window_functions_walker(Node *node, WindowFuncLists *lists);
static bool
contain_subplans_walker(Node *node, void *context);
static bool
contain_mutable_functions_walker(Node *node, void *context);
static bool
contain_volatile_functions_walker(Node *node, void *context);
static bool
contain_volatile_functions_not_nextval_walker(Node *node, void *context);
static bool
max_parallel_hazard_walker(Node *node, max_parallel_hazard_context *context);
static bool
contain_nonstrict_functions_walker(Node *node, void *context);
static bool
contain_exec_param_walker(Node *node, List *param_ids);
static bool
contain_context_dependent_node(Node *clause);
static bool
contain_context_dependent_node_walker(Node *node, int *flags);
static bool
contain_leaked_vars_walker(Node *node, void *context);
static Relids
find_nonnullable_rels_walker(Node *node, bool top_level);
static List *
find_nonnullable_vars_walker(Node *node, bool top_level);
static bool
is_strict_saop(ScalarArrayOpExpr *expr, bool falseOK);
static Node *
eval_const_expressions_mutator(Node *node, eval_const_expressions_context *context);
static bool
contain_non_const_walker(Node *node, void *context);
static bool
ece_function_is_safe(Oid funcid, eval_const_expressions_context *context);
static List *
simplify_or_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceTrue);
static List *
simplify_and_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceFalse);
static Node *
simplify_boolean_equality(Oid opno, List *args);
static Expr *
simplify_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List **args_p, bool funcvariadic, bool process_args, bool allow_non_const, eval_const_expressions_context *context);
static List *
reorder_function_arguments(List *args, HeapTuple func_tuple);
static List *
add_function_defaults(List *args, HeapTuple func_tuple);
static List *
fetch_function_defaults(HeapTuple func_tuple);
static void
recheck_cast_function_args(List *args, Oid result_type, HeapTuple func_tuple);
static Expr *
evaluate_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context);
static Expr *
inline_function(Oid funcid, Oid result_type, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context);
static Node *
substitute_actual_parameters(Node *expr, int nargs, List *args, int *usecounts);
static Node *
substitute_actual_parameters_mutator(Node *node, substitute_actual_parameters_context *context);
static void
sql_inline_error_callback(void *arg);
static Query *
substitute_actual_srf_parameters(Query *expr, int nargs, List *args);
static Node *
substitute_actual_srf_parameters_mutator(Node *node, substitute_actual_srf_parameters_context *context);
static bool
tlist_matches_coltypelist(List *tlist, List *coltypelist);

                                                                               
                                           
                                                                               

   
                      
                                                                       
   
                                          
   
                                                                           
                                                                            
                                                                           
   
                                                                      
                                                  
   
bool
contain_agg_clause(Node *clause)
{
  return contain_agg_clause_walker(clause, NULL);
}

static bool
contain_agg_clause_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
    Assert(((Aggref *)node)->agglevelsup == 0);
    return true;                                               
  }
  if (IsA(node, GroupingFunc))
  {
    Assert(((GroupingFunc *)node)->agglevelsup == 0);
    return true;                                               
  }
  Assert(!IsA(node, SubLink));
  return expression_tree_walker(node, contain_agg_clause_walker, context);
}

   
                        
                                                                  
                                             
   
                                                                            
                       
   
                                                                          
                                                               
   
                                                                              
                                                                          
                                                                          
                                                                          
                            
   
                                                                       
                                                                              
                              
   
                                                                           
                                                                            
                                                                           
   
void
get_agg_clause_costs(PlannerInfo *root, Node *clause, AggSplit aggsplit, AggClauseCosts *costs)
{
  get_agg_clause_costs_context context;

  context.root = root;
  context.aggsplit = aggsplit;
  context.costs = costs;
  (void)get_agg_clause_costs_walker(clause, &context);
}

static bool
get_agg_clause_costs_walker(Node *node, get_agg_clause_costs_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
    Aggref *aggref = (Aggref *)node;
    AggClauseCosts *costs = context->costs;
    HeapTuple aggTuple;
    Form_pg_aggregate aggform;
    Oid aggtransfn;
    Oid aggfinalfn;
    Oid aggcombinefn;
    Oid aggserialfn;
    Oid aggdeserialfn;
    Oid aggtranstype;
    int32 aggtransspace;
    QualCost argcosts;

    Assert(aggref->agglevelsup == 0);

       
                                                                           
                                                                       
                                                     
       
    aggTuple = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(aggref->aggfnoid));
    if (!HeapTupleIsValid(aggTuple))
    {
      elog(ERROR, "cache lookup failed for aggregate %u", aggref->aggfnoid);
    }
    aggform = (Form_pg_aggregate)GETSTRUCT(aggTuple);
    aggtransfn = aggform->aggtransfn;
    aggfinalfn = aggform->aggfinalfn;
    aggcombinefn = aggform->aggcombinefn;
    aggserialfn = aggform->aggserialfn;
    aggdeserialfn = aggform->aggdeserialfn;
    aggtranstype = aggform->aggtranstype;
    aggtransspace = aggform->aggtransspace;
    ReleaseSysCache(aggTuple);

       
                                                                          
                                                            
       
    if (OidIsValid(aggref->aggtranstype))
    {
      aggtranstype = aggref->aggtranstype;
    }
    else
    {
      Oid inputTypes[FUNC_MAX_ARGS];
      int numArguments;

                                                                      
      numArguments = get_aggregate_argtypes(aggref, inputTypes);

                                                                   
      aggtranstype = resolve_aggregate_transtype(aggref->aggfnoid, aggtranstype, inputTypes, numArguments);
      aggref->aggtranstype = aggtranstype;
    }

       
                                                                         
                                                                          
                                              
       
    costs->numAggs++;
    if (aggref->aggorder != NIL || aggref->aggdistinct != NIL)
    {
      costs->numOrderedAggs++;
      costs->hasNonPartial = true;
    }

       
                                                                        
                                      
       
    if (!costs->hasNonPartial)
    {
         
                                                                      
                       
         
      if (!OidIsValid(aggcombinefn))
      {
        costs->hasNonPartial = true;
      }

         
                                                                        
                                                                       
                                                              
         
      else if (aggtranstype == INTERNALOID && (!OidIsValid(aggserialfn) || !OidIsValid(aggdeserialfn)))
      {
        costs->hasNonSerial = true;
      }
    }

       
                                                                 
                           
       
    if (DO_AGGSPLIT_COMBINE(context->aggsplit))
    {
                                                             
      add_function_cost(context->root, aggcombinefn, NULL, &costs->transCost);
    }
    else
    {
      add_function_cost(context->root, aggtransfn, NULL, &costs->transCost);
    }
    if (DO_AGGSPLIT_DESERIALIZE(context->aggsplit) && OidIsValid(aggdeserialfn))
    {
      add_function_cost(context->root, aggdeserialfn, NULL, &costs->transCost);
    }
    if (DO_AGGSPLIT_SERIALIZE(context->aggsplit) && OidIsValid(aggserialfn))
    {
      add_function_cost(context->root, aggserialfn, NULL, &costs->finalCost);
    }
    if (!DO_AGGSPLIT_SKIPFINAL(context->aggsplit) && OidIsValid(aggfinalfn))
    {
      add_function_cost(context->root, aggfinalfn, NULL, &costs->finalCost);
    }

       
                                                                          
                                                   
       
    if (!DO_AGGSPLIT_COMBINE(context->aggsplit))
    {
                                                                  
      cost_qual_eval_node(&argcosts, (Node *)aggref->args, context->root);
      costs->transCost.startup += argcosts.startup;
      costs->transCost.per_tuple += argcosts.per_tuple;

         
                                                       
         
                                                                       
                                                                  
                  
         
      if (aggref->aggfilter)
      {
        cost_qual_eval_node(&argcosts, (Node *)aggref->aggfilter, context->root);
        costs->transCost.startup += argcosts.startup;
        costs->transCost.per_tuple += argcosts.per_tuple;
      }
    }

       
                                                                           
                            
       
    if (aggref->aggdirectargs)
    {
      cost_qual_eval_node(&argcosts, (Node *)aggref->aggdirectargs, context->root);
      costs->finalCost.startup += argcosts.startup;
      costs->finalCost.per_tuple += argcosts.per_tuple;
    }

       
                                                                   
                                                                 
                                                                       
                                           
       
    if (!get_typbyval(aggtranstype))
    {
      int32 avgwidth;

                                                              
      if (aggtransspace > 0)
      {
        avgwidth = aggtransspace;
      }
      else if (aggtransfn == F_ARRAY_APPEND)
      {
           
                                                                      
                                                                    
                                                                       
                                               
           
        avgwidth = ALLOCSET_SMALL_INITSIZE;
      }
      else
      {
           
                                                                   
                                                                    
                                                                      
                                 
           
        int32 aggtranstypmod = -1;

        if (aggref->args)
        {
          TargetEntry *tle = (TargetEntry *)linitial(aggref->args);

          if (aggtranstype == exprType((Node *)tle->expr))
          {
            aggtranstypmod = exprTypmod((Node *)tle->expr);
          }
        }

        avgwidth = get_typavgwidth(aggtranstype, aggtranstypmod);
      }

      avgwidth = MAXALIGN(avgwidth);
      costs->transitionSpace += avgwidth + 2 * sizeof(void *);
    }
    else if (aggtranstype == INTERNALOID)
    {
         
                                                                       
                                                                         
                                                                     
                                                                         
                                                                         
                                                               
                                   
         
      if (aggtransspace > 0)
      {
        costs->transitionSpace += aggtransspace;
      }
      else
      {
        costs->transitionSpace += ALLOCSET_DEFAULT_INITSIZE;
      }
    }

       
                                                                          
                                                                         
                                                                       
       
    return false;
  }
  Assert(!IsA(node, SubLink));
  return expression_tree_walker(node, get_agg_clause_costs_walker, (void *)context);
}

                                                                               
                                        
                                                                               

   
                           
                                                              
   
                                                                         
                                                                        
                              
   
bool
contain_window_function(Node *clause)
{
  return contain_windowfuncs(clause);
}

   
                         
                                                                         
                               
   
                                                                              
   
WindowFuncLists *
find_window_functions(Node *clause, Index maxWinRef)
{
  WindowFuncLists *lists = palloc(sizeof(WindowFuncLists));

  lists->numWindowFuncs = 0;
  lists->maxWinRef = maxWinRef;
  lists->windowFuncs = (List **)palloc0((maxWinRef + 1) * sizeof(List *));
  (void)find_window_functions_walker(clause, lists);
  return lists;
}

static bool
find_window_functions_walker(Node *node, WindowFuncLists *lists)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, WindowFunc))
  {
    WindowFunc *wfunc = (WindowFunc *)node;

                                                     
    if (wfunc->winref > lists->maxWinRef)
    {
      elog(ERROR, "WindowFunc contains out-of-range winref %u", wfunc->winref);
    }
                                                                     
    if (!list_member(lists->windowFuncs[wfunc->winref], wfunc))
    {
      lists->windowFuncs[wfunc->winref] = lappend(lists->windowFuncs[wfunc->winref], wfunc);
      lists->numWindowFuncs++;
    }

       
                                                                  
                                                                        
                                                                          
                                                                           
       
    return false;
  }
  Assert(!IsA(node, SubLink));
  return expression_tree_walker(node, find_window_functions_walker, (void *)lists);
}

                                                                               
                                           
                                                                               

   
                               
                                                                         
                                                             
   
                                                                            
                                                                             
                                                                           
   
                                                                               
   
double
expression_returns_set_rows(PlannerInfo *root, Node *clause)
{
  if (clause == NULL)
  {
    return 1.0;
  }
  if (IsA(clause, FuncExpr))
  {
    FuncExpr *expr = (FuncExpr *)clause;

    if (expr->funcretset)
    {
      return clamp_row_est(get_function_rows(root, expr->funcid, clause));
    }
  }
  if (IsA(clause, OpExpr))
  {
    OpExpr *expr = (OpExpr *)clause;

    if (expr->opretset)
    {
      set_opfuncid(expr);
      return clamp_row_est(get_function_rows(root, expr->opfuncid, clause));
    }
  }
  return 1.0;
}

                                                                               
                                
                                                                               

   
                    
                                                           
   
                                                                            
                                                                              
                                                                          
                                                                      
   
                                      
   
bool
contain_subplans(Node *clause)
{
  return contain_subplans_walker(clause, NULL);
}

static bool
contain_subplans_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, SubPlan) || IsA(node, AlternativeSubPlan) || IsA(node, SubLink))
  {
    return true;                                               
  }
  return expression_tree_walker(node, contain_subplans_walker, context);
}

                                                                               
                                        
                                                                               

   
                             
                                                               
   
                                                                      
                                                                     
                                                                              
                                
   
                                                                         
                                                                          
   
bool
contain_mutable_functions(Node *clause)
{
  return contain_mutable_functions_walker(clause, NULL);
}

static bool
contain_mutable_functions_checker(Oid func_id, void *context)
{
  return (func_volatile(func_id) != PROVOLATILE_IMMUTABLE);
}

static bool
contain_mutable_functions_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
                                                  
  if (check_functions_in_node(node, contain_mutable_functions_checker, context))
  {
    return true;
  }

  if (IsA(node, SQLValueFunction))
  {
                                                     
    return true;
  }

  if (IsA(node, NextValueExpr))
  {
                                   
    return true;
  }

     
                                                                         
                                                                            
                                                                          
                                                                             
                                                                            
                                                                           
                                                   
     

                                  
  if (IsA(node, Query))
  {
                                 
    return query_tree_walker((Query *)node, contain_mutable_functions_walker, context, 0);
  }
  return expression_tree_walker(node, contain_mutable_functions_walker, context);
}

                                                                               
                                         
                                                                               

   
                              
                                                                
   
                                                                       
                                                                 
                                                                     
   
                                                                         
                                                                          
                                                                       
                                                                          
                                                                        
                                                                           
                                                                        
                                                                         
               
   
bool
contain_volatile_functions(Node *clause)
{
  return contain_volatile_functions_walker(clause, NULL);
}

static bool
contain_volatile_functions_checker(Oid func_id, void *context)
{
  return (func_volatile(func_id) == PROVOLATILE_VOLATILE);
}

static bool
contain_volatile_functions_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
                                                   
  if (check_functions_in_node(node, contain_volatile_functions_checker, context))
  {
    return true;
  }

  if (IsA(node, NextValueExpr))
  {
                                   
    return true;
  }

     
                                                                      
                                                                 
                                                                            
     

                                  
  if (IsA(node, Query))
  {
                                 
    return query_tree_walker((Query *)node, contain_volatile_functions_walker, context, 0);
  }
  return expression_tree_walker(node, contain_volatile_functions_walker, context);
}

   
                                                                            
                                                             
   
bool
contain_volatile_functions_not_nextval(Node *clause)
{
  return contain_volatile_functions_not_nextval_walker(clause, NULL);
}

static bool
contain_volatile_functions_not_nextval_checker(Oid func_id, void *context)
{
  return (func_id != F_NEXTVAL_OID && func_volatile(func_id) == PROVOLATILE_VOLATILE);
}

static bool
contain_volatile_functions_not_nextval_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
                                                   
  if (check_functions_in_node(node, contain_volatile_functions_not_nextval_checker, context))
  {
    return true;
  }

     
                                                                      
                                                                 
                                                                            
                                                                       
                                  
     

                                  
  if (IsA(node, Query))
  {
                                 
    return query_tree_walker((Query *)node, contain_volatile_functions_not_nextval_walker, context, 0);
  }
  return expression_tree_walker(node, contain_volatile_functions_not_nextval_walker, context);
}

                                                                               
                                                                   
                                                                               

   
                       
                                                            
   
                                                                          
                                                                          
                                                                               
                                                                        
                                                                            
                                                       
   
char
max_parallel_hazard(Query *parse)
{
  max_parallel_hazard_context context;

  context.max_hazard = PROPARALLEL_SAFE;
  context.max_interesting = PROPARALLEL_UNSAFE;
  context.safe_param_ids = NIL;
  (void)max_parallel_hazard_walker((Node *)parse, &context);
  return context.max_hazard;
}

   
                    
                                                                        
   
                                                                      
                                                       
   
bool
is_parallel_safe(PlannerInfo *root, Node *node)
{
  max_parallel_hazard_context context;
  PlannerInfo *proot;
  ListCell *l;

     
                                                                         
                                                                            
                                                                            
                                                               
     
  if (root->glob->maxParallelHazard == PROPARALLEL_SAFE && root->glob->paramExecTypes == NIL)
  {
    return true;
  }
                                                                           
  context.max_hazard = PROPARALLEL_SAFE;
  context.max_interesting = PROPARALLEL_RESTRICTED;
  context.safe_param_ids = NIL;

     
                                                                            
                                                                          
                                                        
     
  for (proot = root; proot != NULL; proot = proot->parent_root)
  {
    foreach (l, proot->init_plans)
    {
      SubPlan *initsubplan = (SubPlan *)lfirst(l);
      ListCell *l2;

      foreach (l2, initsubplan->setParam)
      {
        context.safe_param_ids = lcons_int(lfirst_int(l2), context.safe_param_ids);
      }
    }
  }

  return !max_parallel_hazard_walker(node, &context);
}

                                               
static bool
max_parallel_hazard_test(char proparallel, max_parallel_hazard_context *context)
{
  switch (proparallel)
  {
  case PROPARALLEL_SAFE:
                                         
    break;
  case PROPARALLEL_RESTRICTED:
                                           
    Assert(context->max_hazard != PROPARALLEL_UNSAFE);
    context->max_hazard = proparallel;
                                                           
    if (context->max_interesting == proparallel)
    {
      return true;
    }
    break;
  case PROPARALLEL_UNSAFE:
    context->max_hazard = proparallel;
                                                         
    return true;
  default:
    elog(ERROR, "unrecognized proparallel value \"%c\"", proparallel);
    break;
  }
  return false;
}

                                      
static bool
max_parallel_hazard_checker(Oid func_id, void *context)
{
  return max_parallel_hazard_test(func_parallel(func_id), (max_parallel_hazard_context *)context);
}

static bool
max_parallel_hazard_walker(Node *node, max_parallel_hazard_context *context)
{
  if (node == NULL)
  {
    return false;
  }

                                                    
  if (check_functions_in_node(node, max_parallel_hazard_checker, context))
  {
    return true;
  }

     
                                                                       
                                                                          
                                                                           
                                                                        
                                                                        
                                                                         
                                                                      
                                                                        
                                                                     
     
  if (IsA(node, CoerceToDomain))
  {
    if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
    {
      return true;
    }
  }

  else if (IsA(node, NextValueExpr))
  {
    if (max_parallel_hazard_test(PROPARALLEL_UNSAFE, context))
    {
      return true;
    }
  }

     
                                                                          
                                                                           
                                                                            
                                                                         
                                                                            
                       
     
  else if (IsA(node, WindowFunc))
  {
    if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
    {
      return true;
    }
  }

     
                                                                         
     
  else if (IsA(node, RestrictInfo))
  {
    RestrictInfo *rinfo = (RestrictInfo *)node;

    return max_parallel_hazard_walker((Node *)rinfo->clause, context);
  }

     
                                                                             
                                      
     
  else if (IsA(node, SubLink))
  {
    if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
    {
      return true;
    }
  }

     
                                                                     
                                                                            
                                                                           
                                                              
     
  else if (IsA(node, SubPlan))
  {
    SubPlan *subplan = (SubPlan *)node;
    List *save_safe_param_ids;

    if (!subplan->parallel_safe && max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
    {
      return true;
    }
    save_safe_param_ids = context->safe_param_ids;
    context->safe_param_ids = list_concat(list_copy(subplan->paramIds), context->safe_param_ids);
    if (max_parallel_hazard_walker(subplan->testexpr, context))
    {
      return true;                                        
    }
    context->safe_param_ids = save_safe_param_ids;
                                                                       
    if (max_parallel_hazard_walker((Node *)subplan->args, context))
    {
      return true;
    }
                                                       
    return false;
  }

     
                                                                            
                                                                     
                                                                       
                                                                         
                                                   
     
  else if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_EXTERN)
    {
      return false;
    }

    if (param->paramkind != PARAM_EXEC || !list_member_int(context->safe_param_ids, param->paramid))
    {
      if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
      {
        return true;
      }
    }
    return false;                            
  }

     
                                                                      
                                                                           
                           
     
  else if (IsA(node, Query))
  {
    Query *query = (Query *)node;

                                                           
    if (query->rowMarks != NULL)
    {
      context->max_hazard = PROPARALLEL_UNSAFE;
      return true;
    }

                                 
    return query_tree_walker(query, max_parallel_hazard_walker, context, 0);
  }

                                  
  return expression_tree_walker(node, max_parallel_hazard_walker, context);
}

                                                                               
                                          
                                                                               

   
                               
                                                                 
   
                                                                          
                                                    
   
                                                                              
                                                                              
                                                                               
                                                                  
   
bool
contain_nonstrict_functions(Node *clause)
{
  return contain_nonstrict_functions_walker(clause, NULL);
}

static bool
contain_nonstrict_functions_checker(Oid func_id, void *context)
{
  return !func_strict(func_id);
}

static bool
contain_nonstrict_functions_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
                                                            
    return true;
  }
  if (IsA(node, GroupingFunc))
  {
       
                                                                         
                                
       
    return true;
  }
  if (IsA(node, WindowFunc))
  {
                                                                 
    return true;
  }
  if (IsA(node, SubscriptingRef))
  {
       
                                                                        
              
       
    if (((SubscriptingRef *)node)->refassgnexpr != NULL)
    {
      return true;
    }

                                         
  }
  if (IsA(node, DistinctExpr))
  {
                                                   
    return true;
  }
  if (IsA(node, NullIfExpr))
  {
                                         
    return true;
  }
  if (IsA(node, BoolExpr))
  {
    BoolExpr *expr = (BoolExpr *)node;

    switch (expr->boolop)
    {
    case AND_EXPR:
    case OR_EXPR:
                                             
      return true;
    default:
      break;
    }
  }
  if (IsA(node, SubLink))
  {
                                                                     
    return true;
  }
  if (IsA(node, SubPlan))
  {
    return true;
  }
  if (IsA(node, AlternativeSubPlan))
  {
    return true;
  }
  if (IsA(node, FieldStore))
  {
    return true;
  }
  if (IsA(node, CoerceViaIO))
  {
       
                                                                          
                                                                          
                                                           
       
    return contain_nonstrict_functions_walker((Node *)((CoerceViaIO *)node)->arg, context);
  }
  if (IsA(node, ArrayCoerceExpr))
  {
       
                                                                        
                                                                       
                                  
       
    return contain_nonstrict_functions_walker((Node *)((ArrayCoerceExpr *)node)->arg, context);
  }
  if (IsA(node, CaseExpr))
  {
    return true;
  }
  if (IsA(node, ArrayExpr))
  {
    return true;
  }
  if (IsA(node, RowExpr))
  {
    return true;
  }
  if (IsA(node, RowCompareExpr))
  {
    return true;
  }
  if (IsA(node, CoalesceExpr))
  {
    return true;
  }
  if (IsA(node, MinMaxExpr))
  {
    return true;
  }
  if (IsA(node, XmlExpr))
  {
    return true;
  }
  if (IsA(node, NullTest))
  {
    return true;
  }
  if (IsA(node, BooleanTest))
  {
    return true;
  }

                                             
  if (check_functions_in_node(node, contain_nonstrict_functions_checker, context))
  {
    return true;
  }

  return expression_tree_walker(node, contain_nonstrict_functions_walker, context);
}

                                                                               
                             
                                                                               

   
                      
                                                               
   
                                                                           
                                                                    
               
   
bool
contain_exec_param(Node *clause, List *param_ids)
{
  return contain_exec_param_walker(clause, param_ids);
}

static bool
contain_exec_param_walker(Node *node, List *param_ids)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    Param *p = (Param *)node;

    if (p->paramkind == PARAM_EXEC && list_member_int(param_ids, p->paramid))
    {
      return true;
    }
  }
  return expression_tree_walker(node, contain_exec_param_walker, param_ids);
}

                                                                               
                                              
                                                                               

   
                                  
                                                                     
   
                                                                              
                                                                               
                                                                               
                                                                             
                                                                
   
                                                                             
                                                                               
                                                                          
                                                                             
                                                       
   
static bool
contain_context_dependent_node(Node *clause)
{
  int flags = 0;

  return contain_context_dependent_node_walker(clause, &flags);
}

#define CCDN_CASETESTEXPR_OK 0x0001                              

static bool
contain_context_dependent_node_walker(Node *node, int *flags)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, CaseTestExpr))
  {
    return !(*flags & CCDN_CASETESTEXPR_OK);
  }
  else if (IsA(node, CaseExpr))
  {
    CaseExpr *caseexpr = (CaseExpr *)node;

       
                                                                           
                                                                    
                                                          
       
    if (caseexpr->arg)
    {
      int save_flags = *flags;
      bool res;

         
                                                                        
                                                                         
                                                                    
                                                                      
                                                                         
                                                           
         
      *flags |= CCDN_CASETESTEXPR_OK;
      res = expression_tree_walker(node, contain_context_dependent_node_walker, (void *)flags);
      *flags = save_flags;
      return res;
    }
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
    ArrayCoerceExpr *ac = (ArrayCoerceExpr *)node;
    int save_flags;
    bool res;

                                    
    if (contain_context_dependent_node_walker((Node *)ac->arg, flags))
    {
      return true;
    }

                                                                      
    save_flags = *flags;
    *flags |= CCDN_CASETESTEXPR_OK;
    res = contain_context_dependent_node_walker((Node *)ac->elemexpr, flags);
    *flags = save_flags;
    return res;
  }
  return expression_tree_walker(node, contain_context_dependent_node_walker, (void *)flags);
}

                                                                               
                                                               
                                                                               

   
                       
                                                                      
                                                                       
                     
   
                                                                            
                                                                               
                                                                              
            
   
bool
contain_leaked_vars(Node *clause)
{
  return contain_leaked_vars_walker(clause, NULL);
}

static bool
contain_leaked_vars_checker(Oid func_id, void *context)
{
  return !get_func_leakproof(func_id);
}

static bool
contain_leaked_vars_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }

  switch (nodeTag(node))
  {
  case T_Var:
  case T_Const:
  case T_Param:
  case T_ArrayExpr:
  case T_FieldSelect:
  case T_FieldStore:
  case T_NamedArgExpr:
  case T_BoolExpr:
  case T_RelabelType:
  case T_CollateExpr:
  case T_CaseExpr:
  case T_CaseTestExpr:
  case T_RowExpr:
  case T_SQLValueFunction:
  case T_NullTest:
  case T_BooleanTest:
  case T_NextValueExpr:
  case T_List:

       
                                                                  
                                                      
       
    break;

  case T_FuncExpr:
  case T_OpExpr:
  case T_DistinctExpr:
  case T_NullIfExpr:
  case T_ScalarArrayOpExpr:
  case T_CoerceViaIO:
  case T_ArrayCoerceExpr:

       
                                                                   
                              
       
    if (check_functions_in_node(node, contain_leaked_vars_checker, context) && contain_var_clause(node))
    {
      return true;
    }
    break;

  case T_SubscriptingRef:
  {
    SubscriptingRef *sbsref = (SubscriptingRef *)node;

       
                                                                 
               
       
    if (sbsref->refassgnexpr != NULL)
    {
                                                        
      if (contain_var_clause(node))
      {
        return true;
      }
    }
  }
  break;

  case T_RowCompareExpr:
  {
       
                                                                 
                                                                 
                                               
       
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;
    ListCell *opid;
    ListCell *larg;
    ListCell *rarg;

    forthree(opid, rcexpr->opnos, larg, rcexpr->largs, rarg, rcexpr->rargs)
    {
      Oid funcid = get_opcode(lfirst_oid(opid));

      if (!get_func_leakproof(funcid) && (contain_var_clause((Node *)lfirst(larg)) || contain_var_clause((Node *)lfirst(rarg))))
      {
        return true;
      }
    }
  }
  break;

  case T_MinMaxExpr:
  {
       
                                                                   
                     
       
    MinMaxExpr *minmaxexpr = (MinMaxExpr *)node;
    TypeCacheEntry *typentry;
    bool leakproof;

                                                                
    typentry = lookup_type_cache(minmaxexpr->minmaxtype, TYPECACHE_CMP_PROC);
    if (OidIsValid(typentry->cmp_proc))
    {
      leakproof = get_func_leakproof(typentry->cmp_proc);
    }
    else
    {
         
                                                            
                                              
         
      leakproof = false;
    }

    if (!leakproof && contain_var_clause((Node *)minmaxexpr->args))
    {
      return true;
    }
  }
  break;

  case T_CurrentOfExpr:

       
                                                              
                                                                    
                                                                      
                                          
       
    return false;

  default:

       
                                                                     
                                                                       
                                           
       
    return true;
  }
  return expression_tree_walker(node, contain_leaked_vars_walker, context);
}

   
                         
                                                                      
   
                                                                           
                                                                            
                                                                            
                                     
   
                                                                             
                                                                            
                                                                          
                                                                             
                                                                              
           
   
                                                                          
                                                                       
                                                                               
                                                                         
                                                                         
                                                                            
   
                                                                              
                                                                               
                                                                              
                                              
   
                                                                             
                                                                              
   
Relids
find_nonnullable_rels(Node *clause)
{
  return find_nonnullable_rels_walker(clause, true);
}

static Relids
find_nonnullable_rels_walker(Node *node, bool top_level)
{
  Relids result = NULL;
  ListCell *l;

  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == 0)
    {
      result = bms_make_singleton(var->varno);
    }
  }
  else if (IsA(node, List))
  {
       
                                                                          
                                                                        
                                                                    
                                                                    
                                                                        
                                                                          
                                  
       
    foreach (l, (List *)node)
    {
      result = bms_join(result, find_nonnullable_rels_walker(lfirst(l), top_level));
    }
  }
  else if (IsA(node, FuncExpr))
  {
    FuncExpr *expr = (FuncExpr *)node;

    if (func_strict(expr->funcid))
    {
      result = find_nonnullable_rels_walker((Node *)expr->args, false);
    }
  }
  else if (IsA(node, OpExpr))
  {
    OpExpr *expr = (OpExpr *)node;

    set_opfuncid(expr);
    if (func_strict(expr->opfuncid))
    {
      result = find_nonnullable_rels_walker((Node *)expr->args, false);
    }
  }
  else if (IsA(node, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *)node;

    if (is_strict_saop(expr, true))
    {
      result = find_nonnullable_rels_walker((Node *)expr->args, false);
    }
  }
  else if (IsA(node, BoolExpr))
  {
    BoolExpr *expr = (BoolExpr *)node;

    switch (expr->boolop)
    {
    case AND_EXPR:
                                                               
      if (top_level)
      {
        result = find_nonnullable_rels_walker((Node *)expr->args, top_level);
        break;
      }

         
                                                                    
                                                                 
                                                                   
                                                                   
                                              
         
                     
    case OR_EXPR:

         
                                                                 
                                                                    
                                                  
         
      foreach (l, expr->args)
      {
        Relids subresult;

        subresult = find_nonnullable_rels_walker(lfirst(l), top_level);
        if (result == NULL)                       
        {
          result = subresult;
        }
        else
        {
          result = bms_int_members(result, subresult);
        }

           
                                                                   
                                                              
           
        if (bms_is_empty(result))
        {
          break;
        }
      }
      break;
    case NOT_EXPR:
                                                   
      result = find_nonnullable_rels_walker((Node *)expr->args, false);
      break;
    default:
      elog(ERROR, "unrecognized boolop: %d", (int)expr->boolop);
      break;
    }
  }
  else if (IsA(node, RelabelType))
  {
    RelabelType *expr = (RelabelType *)node;

    result = find_nonnullable_rels_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, CoerceViaIO))
  {
                                                     
    CoerceViaIO *expr = (CoerceViaIO *)node;

    result = find_nonnullable_rels_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
                                                                       
    ArrayCoerceExpr *expr = (ArrayCoerceExpr *)node;

    result = find_nonnullable_rels_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, ConvertRowtypeExpr))
  {
                                                     
    ConvertRowtypeExpr *expr = (ConvertRowtypeExpr *)node;

    result = find_nonnullable_rels_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, CollateExpr))
  {
    CollateExpr *expr = (CollateExpr *)node;

    result = find_nonnullable_rels_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, NullTest))
  {
                                                                     
    NullTest *expr = (NullTest *)node;

    if (top_level && expr->nulltesttype == IS_NOT_NULL && !expr->argisrow)
    {
      result = find_nonnullable_rels_walker((Node *)expr->arg, false);
    }
  }
  else if (IsA(node, BooleanTest))
  {
                                                                
    BooleanTest *expr = (BooleanTest *)node;

    if (top_level && (expr->booltesttype == IS_TRUE || expr->booltesttype == IS_FALSE || expr->booltesttype == IS_NOT_UNKNOWN))
    {
      result = find_nonnullable_rels_walker((Node *)expr->arg, false);
    }
  }
  else if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

       
                                                                         
                
       
    result = find_nonnullable_rels_walker((Node *)phv->phexpr, top_level);

       
                                                                          
                                                                        
                                                                           
                                                                          
                                                                       
                                                                           
                                     
       
    if (phv->phlevelsup == 0 && bms_membership(phv->phrels) == BMS_SINGLETON)
    {
      result = bms_add_members(result, phv->phrels);
    }
  }
  return result;
}

   
                         
                                                                 
   
                                                                              
                                                                               
                                                                              
                        
   
                                                                             
                                                                            
                                                                          
                                                                             
                                                                              
           
   
                                                                               
                                                                
   
                                                                              
                                                                               
                                                                              
                                              
   
                                                                             
                                                                              
   
List *
find_nonnullable_vars(Node *clause)
{
  return find_nonnullable_vars_walker(clause, true);
}

static List *
find_nonnullable_vars_walker(Node *node, bool top_level)
{
  List *result = NIL;
  ListCell *l;

  if (node == NULL)
  {
    return NIL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == 0)
    {
      result = list_make1(var);
    }
  }
  else if (IsA(node, List))
  {
       
                                                                          
                                                                        
                                                                    
                                                                    
                                                                        
                                                                          
                                  
       
    foreach (l, (List *)node)
    {
      result = list_concat(result, find_nonnullable_vars_walker(lfirst(l), top_level));
    }
  }
  else if (IsA(node, FuncExpr))
  {
    FuncExpr *expr = (FuncExpr *)node;

    if (func_strict(expr->funcid))
    {
      result = find_nonnullable_vars_walker((Node *)expr->args, false);
    }
  }
  else if (IsA(node, OpExpr))
  {
    OpExpr *expr = (OpExpr *)node;

    set_opfuncid(expr);
    if (func_strict(expr->opfuncid))
    {
      result = find_nonnullable_vars_walker((Node *)expr->args, false);
    }
  }
  else if (IsA(node, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *)node;

    if (is_strict_saop(expr, true))
    {
      result = find_nonnullable_vars_walker((Node *)expr->args, false);
    }
  }
  else if (IsA(node, BoolExpr))
  {
    BoolExpr *expr = (BoolExpr *)node;

    switch (expr->boolop)
    {
    case AND_EXPR:
                                                               
      if (top_level)
      {
        result = find_nonnullable_vars_walker((Node *)expr->args, top_level);
        break;
      }

         
                                                                    
                                                                 
                                                                   
                                                                   
                                              
         
                     
    case OR_EXPR:

         
                                                                 
                                                                    
                                                  
         
      foreach (l, expr->args)
      {
        List *subresult;

        subresult = find_nonnullable_vars_walker(lfirst(l), top_level);
        if (result == NIL)                       
        {
          result = subresult;
        }
        else
        {
          result = list_intersection(result, subresult);
        }

           
                                                                   
                                                              
           
        if (result == NIL)
        {
          break;
        }
      }
      break;
    case NOT_EXPR:
                                                   
      result = find_nonnullable_vars_walker((Node *)expr->args, false);
      break;
    default:
      elog(ERROR, "unrecognized boolop: %d", (int)expr->boolop);
      break;
    }
  }
  else if (IsA(node, RelabelType))
  {
    RelabelType *expr = (RelabelType *)node;

    result = find_nonnullable_vars_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, CoerceViaIO))
  {
                                                     
    CoerceViaIO *expr = (CoerceViaIO *)node;

    result = find_nonnullable_vars_walker((Node *)expr->arg, false);
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
                                                                       
    ArrayCoerceExpr *expr = (ArrayCoerceExpr *)node;

    result = find_nonnullable_vars_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, ConvertRowtypeExpr))
  {
                                                     
    ConvertRowtypeExpr *expr = (ConvertRowtypeExpr *)node;

    result = find_nonnullable_vars_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, CollateExpr))
  {
    CollateExpr *expr = (CollateExpr *)node;

    result = find_nonnullable_vars_walker((Node *)expr->arg, top_level);
  }
  else if (IsA(node, NullTest))
  {
                                                                     
    NullTest *expr = (NullTest *)node;

    if (top_level && expr->nulltesttype == IS_NOT_NULL && !expr->argisrow)
    {
      result = find_nonnullable_vars_walker((Node *)expr->arg, false);
    }
  }
  else if (IsA(node, BooleanTest))
  {
                                                                
    BooleanTest *expr = (BooleanTest *)node;

    if (top_level && (expr->booltesttype == IS_TRUE || expr->booltesttype == IS_FALSE || expr->booltesttype == IS_NOT_UNKNOWN))
    {
      result = find_nonnullable_vars_walker((Node *)expr->arg, false);
    }
  }
  else if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    result = find_nonnullable_vars_walker((Node *)phv->phexpr, top_level);
  }
  return result;
}

   
                         
                                                                           
   
                                                                             
                                                                             
                                                                          
                                                                
   
                                                                               
                                                                
   
List *
find_forced_null_vars(Node *node)
{
  List *result = NIL;
  Var *var;
  ListCell *l;

  if (node == NULL)
  {
    return NIL;
  }
                                                  
  var = find_forced_null_var(node);
  if (var)
  {
    result = list_make1(var);
  }
                                        
  else if (IsA(node, List))
  {
       
                                                                          
                                                                     
       
    foreach (l, (List *)node)
    {
      result = list_concat(result, find_forced_null_vars(lfirst(l)));
    }
  }
  else if (IsA(node, BoolExpr))
  {
    BoolExpr *expr = (BoolExpr *)node;

       
                                                                    
                                                                         
                                                   
       
    if (expr->boolop == AND_EXPR)
    {
                                                               
      result = find_forced_null_vars((Node *)expr->args);
    }
  }
  return result;
}

   
                        
                                                                    
                                                                      
                                                                     
   
                                                                           
                                                                             
                                                                           
                                                                              
                                                                           
                                                                                
   
Var *
find_forced_null_var(Node *node)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, NullTest))
  {
                               
    NullTest *expr = (NullTest *)node;

    if (expr->nulltesttype == IS_NULL && !expr->argisrow)
    {
      Var *var = (Var *)expr->arg;

      if (var && IsA(var, Var) && var->varlevelsup == 0)
      {
        return var;
      }
    }
  }
  else if (IsA(node, BooleanTest))
  {
                                                     
    BooleanTest *expr = (BooleanTest *)node;

    if (expr->booltesttype == IS_UNKNOWN)
    {
      Var *var = (Var *)expr->arg;

      if (var && IsA(var, Var) && var->varlevelsup == 0)
      {
        return var;
      }
    }
  }
  return NULL;
}

   
                                               
   
                                                                         
                                                                   
   
                                                                       
                                                                 
                                                                
   
                                                                          
                                                                   
   
static bool
is_strict_saop(ScalarArrayOpExpr *expr, bool falseOK)
{
  Node *rightop;

                                              
  set_sa_opfuncid(expr);
  if (!func_strict(expr->opfuncid))
  {
    return false;
  }
                                                        
  if (expr->useOr && falseOK)
  {
    return true;
  }
                                                                
  Assert(list_length(expr->args) == 2);
  rightop = (Node *)lsecond(expr->args);
  if (rightop && IsA(rightop, Const))
  {
    Datum arraydatum = ((Const *)rightop)->constvalue;
    bool arrayisnull = ((Const *)rightop)->constisnull;
    ArrayType *arrayval;
    int nitems;

    if (arrayisnull)
    {
      return false;
    }
    arrayval = DatumGetArrayTypeP(arraydatum);
    nitems = ArrayGetNItems(ARR_NDIM(arrayval), ARR_DIMS(arrayval));
    if (nitems > 0)
    {
      return true;
    }
  }
  else if (rightop && IsA(rightop, ArrayExpr))
  {
    ArrayExpr *arrayexpr = (ArrayExpr *)rightop;

    if (arrayexpr->elements != NIL && !arrayexpr->multidims)
    {
      return true;
    }
  }
  return false;
}

                                                                               
                                        
                                                                               

   
                             
                                                                           
                                                                             
                                                                           
                                                                         
                                                                         
                                                                           
                                                                            
                                                                    
   
                                                                        
                                                                            
                                                                             
                                                                            
                                                                         
                                       
   
bool
is_pseudo_constant_clause(Node *clause)
{
     
                                                                         
                                                                            
                                                                        
                                            
     
  if (!contain_var_clause(clause) && !contain_volatile_functions(clause))
  {
    return true;
  }
  return false;
}

   
                                    
                                                                           
                                                                          
   
bool
is_pseudo_constant_clause_relids(Node *clause, Relids relids)
{
  if (bms_is_empty(relids) && !contain_volatile_functions(clause))
  {
    return true;
  }
  return false;
}

                                                                               
                        
                                                   
                        
                                                                               

   
             
                             
   
                                                                     
   
int
NumRelids(Node *clause)
{
  return NumRelids_new(NULL, clause);
}

int
NumRelids_new(PlannerInfo *root, Node *clause)
{
  Relids varnos = pull_varnos(root, clause);
  int result = bms_num_members(varnos);

  bms_free(varnos);
  return result;
}

   
                                                   
   
                                             
   
void
CommuteOpExpr(OpExpr *clause)
{
  Oid opoid;
  Node *temp;

                                                       
  if (!is_opclause(clause) || list_length(clause->args) != 2)
  {
    elog(ERROR, "cannot commute non-binary-operator clause");
  }

  opoid = get_commutator(clause->opno);

  if (!OidIsValid(opoid))
  {
    elog(ERROR, "could not find commutator for operator %u", clause->opno);
  }

     
                                 
     
  clause->opno = opoid;
  clause->opfuncid = InvalidOid;
                                                                     

  temp = linitial(clause->args);
  linitial(clause->args) = lsecond(clause->args);
  lsecond(clause->args) = temp;
}

   
                                                                          
                                                                           
                                                                           
                                                    
   
                                                                             
                                                                             
                         
   
static bool
rowtype_field_matches(Oid rowtypeid, int fieldnum, Oid expectedtype, int32 expectedtypmod, Oid expectedcollation)
{
  TupleDesc tupdesc;
  Form_pg_attribute attr;

                                                                       
  if (rowtypeid == RECORDOID)
  {
    return true;
  }
  tupdesc = lookup_rowtype_tupdesc_domain(rowtypeid, -1, false);
  if (fieldnum <= 0 || fieldnum > tupdesc->natts)
  {
    ReleaseTupleDesc(tupdesc);
    return false;
  }
  attr = TupleDescAttr(tupdesc, fieldnum - 1);
  if (attr->attisdropped || attr->atttypid != expectedtype || attr->atttypmod != expectedtypmod || attr->attcollation != expectedcollation)
  {
    ReleaseTupleDesc(tupdesc);
    return false;
  }
  ReleaseTupleDesc(tupdesc);
  return true;
}

                       
                          
   
                                                                
                                                                     
                                                                    
                                                                     
                                                                      
                                                                     
                                                                      
                                                                       
   
                                                                    
                                                                    
                                                                  
                                                                 
                                 
   
                                                                     
                                                                      
                                                                        
                                                               
   
                                                                      
                                                                       
   
                                                                          
                                                                 
   
                                                                          
                                                                 
   
                                                                         
                                                                        
                                                                        
                       
   
Node *
eval_const_expressions(PlannerInfo *root, Node *node)
{
  eval_const_expressions_context context;

  if (root)
  {
    context.boundParams = root->glob->boundParams;                   
  }
  else
  {
    context.boundParams = NULL;
  }
  context.root = root;                                             
  context.active_fns = NIL;                                           
  context.case_val = NULL;                              
  context.estimate = false;                                
  return eval_const_expressions_mutator(node, &context);
}

                       
                             
   
                                                                     
                                                                     
                                                                          
                                                                          
   
                                                              
                                                                            
                                                                          
                                                                             
                         
                                                                 
                                                                  
                       
   
Node *
estimate_expression_value(PlannerInfo *root, Node *node)
{
  eval_const_expressions_context context;

  context.boundParams = root->glob->boundParams;                   
                                                                         
  context.root = NULL;
  context.active_fns = NIL;                                           
  context.case_val = NULL;                              
  context.estimate = true;                                 
  return eval_const_expressions_mutator(node, &context);
}

   
                                                                          
                                                                         
                                                                          
                                                                            
                                                                            
                                                                          
                                                                       
   
#define ece_generic_processing(node) expression_tree_mutator((Node *)(node), eval_const_expressions_mutator, (void *)context)

   
                                                                         
                                                                         
                                                            
   
#define ece_all_arguments_const(node) (!expression_tree_walker((Node *)(node), contain_non_const_walker, NULL))

                                              
#define ece_evaluate_expr(node) ((Node *)evaluate_expr((Expr *)(node), exprType((Node *)(node)), exprTypmod((Node *)(node)), exprCollation((Node *)(node))))

   
                                                                      
   
static Node *
eval_const_expressions_mutator(Node *node, eval_const_expressions_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  switch (nodeTag(node))
  {
  case T_Param:
  {
    Param *param = (Param *)node;
    ParamListInfo paramLI = context->boundParams;

                                                                
    if (param->paramkind == PARAM_EXTERN && paramLI != NULL && param->paramid > 0 && param->paramid <= paramLI->numParams)
    {
      ParamExternData *prm;
      ParamExternData prmdata;

         
                                                                
                                                               
                                                   
         
      if (paramLI->paramFetch != NULL)
      {
        prm = paramLI->paramFetch(paramLI, param->paramid, true, &prmdata);
      }
      else
      {
        prm = &paramLI->params[param->paramid - 1];
      }

         
                                                             
                                                                 
                                                               
                                         
         
      if (OidIsValid(prm->ptype) && prm->ptype == param->paramtype)
      {
                                               
        if (context->estimate || (prm->pflags & PARAM_FLAG_CONST))
        {
             
                                                          
                                                        
                                                
                                                           
             
          int16 typLen;
          bool typByVal;
          Datum pval;

          get_typlenbyval(param->paramtype, &typLen, &typByVal);
          if (prm->isnull || typByVal)
          {
            pval = prm->value;
          }
          else
          {
            pval = datumCopy(prm->value, typByVal, typLen);
          }
          return (Node *)makeConst(param->paramtype, param->paramtypmod, param->paramcollid, (int)typLen, pval, prm->isnull, typByVal);
        }
      }
    }

       
                                                           
                
       
    return (Node *)copyObject(param);
  }
  case T_WindowFunc:
  {
    WindowFunc *expr = (WindowFunc *)node;
    Oid funcid = expr->winfnoid;
    List *args;
    Expr *aggfilter;
    HeapTuple func_tuple;
    WindowFunc *newexpr;

       
                                                                  
                                                               
                                                               
                                                                 
                                          
       
    func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
    if (!HeapTupleIsValid(func_tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", funcid);
    }

    args = expand_function_arguments(expr->args, expr->wintype, func_tuple);

    ReleaseSysCache(func_tuple);

                                                               
    args = (List *)expression_tree_mutator((Node *)args, eval_const_expressions_mutator, (void *)context);
                                                    
    aggfilter = (Expr *)eval_const_expressions_mutator((Node *)expr->aggfilter, context);

                                                   
    newexpr = makeNode(WindowFunc);
    newexpr->winfnoid = expr->winfnoid;
    newexpr->wintype = expr->wintype;
    newexpr->wincollid = expr->wincollid;
    newexpr->inputcollid = expr->inputcollid;
    newexpr->args = args;
    newexpr->aggfilter = aggfilter;
    newexpr->winref = expr->winref;
    newexpr->winstar = expr->winstar;
    newexpr->winagg = expr->winagg;
    newexpr->location = expr->location;

    return (Node *)newexpr;
  }
  case T_FuncExpr:
  {
    FuncExpr *expr = (FuncExpr *)node;
    List *args = expr->args;
    Expr *simple;
    FuncExpr *newexpr;

       
                                                                   
                                                                  
                                                                  
                                                              
                             
       
    simple = simplify_function(expr->funcid, expr->funcresulttype, exprTypmod(node), expr->funccollid, expr->inputcollid, &args, expr->funcvariadic, true, true, context);
    if (simple)                                 
    {
      return (Node *)simple;
    }

       
                                                                 
                                                        
                                                              
                                                           
       
    newexpr = makeNode(FuncExpr);
    newexpr->funcid = expr->funcid;
    newexpr->funcresulttype = expr->funcresulttype;
    newexpr->funcretset = expr->funcretset;
    newexpr->funcvariadic = expr->funcvariadic;
    newexpr->funcformat = expr->funcformat;
    newexpr->funccollid = expr->funccollid;
    newexpr->inputcollid = expr->inputcollid;
    newexpr->args = args;
    newexpr->location = expr->location;
    return (Node *)newexpr;
  }
  case T_OpExpr:
  {
    OpExpr *expr = (OpExpr *)node;
    List *args = expr->args;
    Expr *simple;
    OpExpr *newexpr;

       
                                                                 
                                
       
    set_opfuncid(expr);

       
                                                                   
                               
       
    simple = simplify_function(expr->opfuncid, expr->opresulttype, -1, expr->opcollid, expr->inputcollid, &args, false, true, true, context);
    if (simple)                                 
    {
      return (Node *)simple;
    }

       
                                                                  
                                                            
                              
       
    if (expr->opno == BooleanEqualOperator || expr->opno == BooleanNotEqualOperator)
    {
      simple = (Expr *)simplify_boolean_equality(expr->opno, args);
      if (simple)                                 
      {
        return (Node *)simple;
      }
    }

       
                                                                 
                                                      
                                      
       
    newexpr = makeNode(OpExpr);
    newexpr->opno = expr->opno;
    newexpr->opfuncid = expr->opfuncid;
    newexpr->opresulttype = expr->opresulttype;
    newexpr->opretset = expr->opretset;
    newexpr->opcollid = expr->opcollid;
    newexpr->inputcollid = expr->inputcollid;
    newexpr->args = args;
    newexpr->location = expr->location;
    return (Node *)newexpr;
  }
  case T_DistinctExpr:
  {
    DistinctExpr *expr = (DistinctExpr *)node;
    List *args;
    ListCell *arg;
    bool has_null_input = false;
    bool all_null_input = true;
    bool has_nonconst_input = false;
    Expr *simple;
    DistinctExpr *newexpr;

       
                                                                  
                                                         
                                                                 
             
       
    args = (List *)expression_tree_mutator((Node *)expr->args, eval_const_expressions_mutator, (void *)context);

       
                                                                   
                                                            
                      
       
    foreach (arg, args)
    {
      if (IsA(lfirst(arg), Const))
      {
        has_null_input |= ((Const *)lfirst(arg))->constisnull;
        all_null_input &= ((Const *)lfirst(arg))->constisnull;
      }
      else
      {
        has_nonconst_input = true;
      }
    }

                                                   
    if (!has_nonconst_input)
    {
                                        
      if (all_null_input)
      {
        return makeBoolConst(false, false);
      }

                                   
      if (has_null_input)
      {
        return makeBoolConst(true, false);
      }

                                                      
                                                   

         
                                                          
                                           
         
      set_opfuncid((OpExpr *)expr);                   
                                                     

         
                                                                 
                                     
         
      simple = simplify_function(expr->opfuncid, expr->opresulttype, -1, expr->opcollid, expr->inputcollid, &args, false, false, false, context);
      if (simple)                                 
      {
           
                                                             
                      
           
        Const *csimple = castNode(Const, simple);

        csimple->constvalue = BoolGetDatum(!DatumGetBool(csimple->constvalue));
        return (Node *)csimple;
      }
    }

       
                                                                 
                                                            
                                      
       
    newexpr = makeNode(DistinctExpr);
    newexpr->opno = expr->opno;
    newexpr->opfuncid = expr->opfuncid;
    newexpr->opresulttype = expr->opresulttype;
    newexpr->opretset = expr->opretset;
    newexpr->opcollid = expr->opcollid;
    newexpr->inputcollid = expr->inputcollid;
    newexpr->args = args;
    newexpr->location = expr->location;
    return (Node *)newexpr;
  }
  case T_ScalarArrayOpExpr:
  {
    ScalarArrayOpExpr *saop;

                                                        
    saop = (ScalarArrayOpExpr *)ece_generic_processing(node);

                                               
    set_sa_opfuncid(saop);

       
                                                                 
                              
       
    if (ece_all_arguments_const(saop) && ece_function_is_safe(saop->opfuncid, context))
    {
      return ece_evaluate_expr(saop);
    }
    return (Node *)saop;
  }
  case T_BoolExpr:
  {
    BoolExpr *expr = (BoolExpr *)node;

    switch (expr->boolop)
    {
    case OR_EXPR:
    {
      List *newargs;
      bool haveNull = false;
      bool forceTrue = false;

      newargs = simplify_or_arguments(expr->args, context, &haveNull, &forceTrue);
      if (forceTrue)
      {
        return makeBoolConst(true, false);
      }
      if (haveNull)
      {
        newargs = lappend(newargs, makeBoolConst(false, true));
      }
                                                        
      if (newargs == NIL)
      {
        return makeBoolConst(false, false);
      }

         
                                                      
                
         
      if (list_length(newargs) == 1)
      {
        return (Node *)linitial(newargs);
      }
                                         
      return (Node *)make_orclause(newargs);
    }
    case AND_EXPR:
    {
      List *newargs;
      bool haveNull = false;
      bool forceFalse = false;

      newargs = simplify_and_arguments(expr->args, context, &haveNull, &forceFalse);
      if (forceFalse)
      {
        return makeBoolConst(false, false);
      }
      if (haveNull)
      {
        newargs = lappend(newargs, makeBoolConst(false, true));
      }
                                                      
      if (newargs == NIL)
      {
        return makeBoolConst(true, false);
      }

         
                                                      
                
         
      if (list_length(newargs) == 1)
      {
        return (Node *)linitial(newargs);
      }
                                          
      return (Node *)make_andclause(newargs);
    }
    case NOT_EXPR:
    {
      Node *arg;

      Assert(list_length(expr->args) == 1);
      arg = eval_const_expressions_mutator(linitial(expr->args), context);

         
                                                       
                       
         
      return negate_clause(arg);
    }
    default:
      elog(ERROR, "unrecognized boolop: %d", (int)expr->boolop);
      break;
    }
    break;
  }
  case T_SubPlan:
  case T_AlternativeSubPlan:

       
                                                                       
       
                                                                    
                                                       
       
    return node;
  case T_RelabelType:
  {
    RelabelType *relabel = (RelabelType *)node;
    Node *arg;

                                
    arg = eval_const_expressions_mutator((Node *)relabel->arg, context);
                                                          
    return applyRelabelType(arg, relabel->resulttype, relabel->resulttypmod, relabel->resultcollid, relabel->relabelformat, relabel->location, true);
  }
  case T_CoerceViaIO:
  {
    CoerceViaIO *expr = (CoerceViaIO *)node;
    List *args;
    Oid outfunc;
    bool outtypisvarlena;
    Oid infunc;
    Oid intypioparam;
    Expr *simple;
    CoerceViaIO *newexpr;

                                                     
    args = list_make1(expr->arg);

       
                                                               
                                                                   
                                                                  
                                                             
       
                                                                
                                                                   
       
    getTypeOutputInfo(exprType((Node *)expr->arg), &outfunc, &outtypisvarlena);
    getTypeInputInfo(expr->resulttype, &infunc, &intypioparam);

    simple = simplify_function(outfunc, CSTRINGOID, -1, InvalidOid, InvalidOid, &args, false, true, true, context);
    if (simple)                                        
    {
         
                                                               
                                                                 
                   
         
      args = list_make3(simple, makeConst(OIDOID, -1, InvalidOid, sizeof(Oid), ObjectIdGetDatum(intypioparam), false, true), makeConst(INT4OID, -1, InvalidOid, sizeof(int32), Int32GetDatum(-1), false, true));

      simple = simplify_function(infunc, expr->resulttype, -1, expr->resultcollid, InvalidOid, &args, false, false, true, context);
      if (simple)                                       
      {
        return (Node *)simple;
      }
    }

       
                                                                 
                                                           
                                     
       
    newexpr = makeNode(CoerceViaIO);
    newexpr->arg = (Expr *)linitial(args);
    newexpr->resulttype = expr->resulttype;
    newexpr->resultcollid = expr->resultcollid;
    newexpr->coerceformat = expr->coerceformat;
    newexpr->location = expr->location;
    return (Node *)newexpr;
  }
  case T_ArrayCoerceExpr:
  {
    ArrayCoerceExpr *ac = makeNode(ArrayCoerceExpr);
    Node *save_case_val;

       
                                                                 
                                                                 
                                                         
       
    memcpy(ac, node, sizeof(ArrayCoerceExpr));
    ac->arg = (Expr *)eval_const_expressions_mutator((Node *)ac->arg, context);

       
                                                                   
                                                               
       
    save_case_val = context->case_val;
    context->case_val = NULL;

    ac->elemexpr = (Expr *)eval_const_expressions_mutator((Node *)ac->elemexpr, context);

    context->case_val = save_case_val;

       
                                                              
                                                                 
                                                               
                                                                  
                                                                 
                                                              
       
    if (ac->arg && IsA(ac->arg, Const) && ac->elemexpr && !IsA(ac->elemexpr, CoerceToDomain) && !contain_mutable_functions((Node *)ac->elemexpr))
    {
      return ece_evaluate_expr(ac);
    }

    return (Node *)ac;
  }
  case T_CollateExpr:
  {
       
                                                                 
                                                                 
                                                                
                                                                   
                                                        
       
    CollateExpr *collate = (CollateExpr *)node;
    Node *arg;

                                
    arg = eval_const_expressions_mutator((Node *)collate->arg, context);
                                                          
    return applyRelabelType(arg, exprType(arg), exprTypmod(arg), collate->collOid, COERCE_IMPLICIT_CAST, collate->location, true);
  }
  case T_CaseExpr:
  {
                 
                                                                
                          
                                              
                                              
                                                              
                                                             
                                                            
                                                                 
       
                                                        
                                                                  
                                                           
                                                            
                           
                                          
                                                               
                                                                
                                                             
                                        
                                       
                                                       
                                
                                          
                                                                  
                                                               
                                                                   
                                 
                 
       
    CaseExpr *caseexpr = (CaseExpr *)node;
    CaseExpr *newcase;
    Node *save_case_val;
    Node *newarg;
    List *newargs;
    bool const_true_cond;
    Node *defresult = NULL;
    ListCell *arg;

                                              
    newarg = eval_const_expressions_mutator((Node *)caseexpr->arg, context);

                                                 
    save_case_val = context->case_val;
    if (newarg && IsA(newarg, Const))
    {
      context->case_val = newarg;
      newarg = NULL;                                    
    }
    else
    {
      context->case_val = NULL;
    }

                                   
    newargs = NIL;
    const_true_cond = false;
    foreach (arg, caseexpr->args)
    {
      CaseWhen *oldcasewhen = lfirst_node(CaseWhen, arg);
      Node *casecond;
      Node *caseresult;

                                                      
      casecond = eval_const_expressions_mutator((Node *)oldcasewhen->expr, context);

         
                                                                 
                                                              
                     
         
      if (casecond && IsA(casecond, Const))
      {
        Const *const_input = (Const *)casecond;

        if (const_input->constisnull || !DatumGetBool(const_input->constvalue))
        {
          continue;                                       
        }
                                     
        const_true_cond = true;
      }

                                                    
      caseresult = eval_const_expressions_mutator((Node *)oldcasewhen->result, context);

                                                                
      if (!const_true_cond)
      {
        CaseWhen *newcasewhen = makeNode(CaseWhen);

        newcasewhen->expr = (Expr *)casecond;
        newcasewhen->result = (Expr *)caseresult;
        newcasewhen->location = oldcasewhen->location;
        newargs = lappend(newargs, newcasewhen);
        continue;
      }

         
                                                          
                                                              
                             
         
      defresult = caseresult;
      break;
    }

                                                                  
    if (!const_true_cond)
    {
      defresult = eval_const_expressions_mutator((Node *)caseexpr->defresult, context);
    }

    context->case_val = save_case_val;

       
                                                                 
              
       
    if (newargs == NIL)
    {
      return defresult;
    }
                                           
    newcase = makeNode(CaseExpr);
    newcase->casetype = caseexpr->casetype;
    newcase->casecollid = caseexpr->casecollid;
    newcase->arg = (Expr *)newarg;
    newcase->args = newargs;
    newcase->defresult = (Expr *)defresult;
    newcase->location = caseexpr->location;
    return (Node *)newcase;
  }
  case T_CaseTestExpr:
  {
       
                                                             
                                                                
                                     
       
    if (context->case_val)
    {
      return copyObject(context->case_val);
    }
    else
    {
      return copyObject(node);
    }
  }
  case T_SubscriptingRef:
  case T_ArrayExpr:
  case T_RowExpr:
  case T_MinMaxExpr:
  {
       
                                                               
                                                              
                                                      
       
                                                                 
                                                                
                                                      
       

                                                        
    node = ece_generic_processing(node);
                                                                
    if (ece_all_arguments_const(node))
    {
      return ece_evaluate_expr(node);
    }
    return node;
  }
  case T_CoalesceExpr:
  {
    CoalesceExpr *coalesceexpr = (CoalesceExpr *)node;
    CoalesceExpr *newcoalesce;
    List *newargs;
    ListCell *arg;

    newargs = NIL;
    foreach (arg, coalesceexpr->args)
    {
      Node *e;

      e = eval_const_expressions_mutator((Node *)lfirst(arg), context);

         
                                                           
                                                               
                                                            
                                                               
                                                           
                  
         
      if (IsA(e, Const))
      {
        if (((Const *)e)->constisnull)
        {
          continue;                         
        }
        if (newargs == NIL)
        {
          return e;                 
        }
        newargs = lappend(newargs, e);
        break;
      }
      newargs = lappend(newargs, e);
    }

       
                                                                   
            
       
    if (newargs == NIL)
    {
      return (Node *)makeNullConst(coalesceexpr->coalescetype, -1, coalesceexpr->coalescecollid);
    }

    newcoalesce = makeNode(CoalesceExpr);
    newcoalesce->coalescetype = coalesceexpr->coalescetype;
    newcoalesce->coalescecollid = coalesceexpr->coalescecollid;
    newcoalesce->args = newargs;
    newcoalesce->location = coalesceexpr->location;
    return (Node *)newcoalesce;
  }
  case T_SQLValueFunction:
  {
       
                                                                 
                                                                 
                                                     
       
    SQLValueFunction *svf = (SQLValueFunction *)node;

    if (context->estimate)
    {
      return (Node *)evaluate_expr((Expr *)svf, svf->type, svf->typmod, InvalidOid);
    }
    else
    {
      return copyObject((Node *)svf);
    }
  }
  case T_FieldSelect:
  {
       
                                                                   
                                                                  
                                                                 
                                                                
                                                                 
                                  
       
                                                            
                                                                  
                                                                  
                                                                  
                                                                  
                                                                   
                                                                 
                                                                 
                                                                  
       
                                                                 
                                                                   
                                                                   
                                                            
                                                                  
       
    FieldSelect *fselect = (FieldSelect *)node;
    FieldSelect *newfselect;
    Node *arg;

    arg = eval_const_expressions_mutator((Node *)fselect->arg, context);
    if (arg && IsA(arg, Var) && ((Var *)arg)->varattno == InvalidAttrNumber && ((Var *)arg)->varlevelsup == 0)
    {
      if (rowtype_field_matches(((Var *)arg)->vartype, fselect->fieldnum, fselect->resulttype, fselect->resulttypmod, fselect->resultcollid))
      {
        return (Node *)makeVar(((Var *)arg)->varno, fselect->fieldnum, fselect->resulttype, fselect->resulttypmod, fselect->resultcollid, ((Var *)arg)->varlevelsup);
      }
    }
    if (arg && IsA(arg, RowExpr))
    {
      RowExpr *rowexpr = (RowExpr *)arg;

      if (fselect->fieldnum > 0 && fselect->fieldnum <= list_length(rowexpr->args))
      {
        Node *fld = (Node *)list_nth(rowexpr->args, fselect->fieldnum - 1);

        if (rowtype_field_matches(rowexpr->row_typeid, fselect->fieldnum, fselect->resulttype, fselect->resulttypmod, fselect->resultcollid) && fselect->resulttype == exprType(fld) && fselect->resulttypmod == exprTypmod(fld) && fselect->resultcollid == exprCollation(fld))
        {
          return fld;
        }
      }
    }
    newfselect = makeNode(FieldSelect);
    newfselect->arg = (Expr *)arg;
    newfselect->fieldnum = fselect->fieldnum;
    newfselect->resulttype = fselect->resulttype;
    newfselect->resulttypmod = fselect->resulttypmod;
    newfselect->resultcollid = fselect->resultcollid;
    if (arg && IsA(arg, Const))
    {
      Const *con = (Const *)arg;

      if (rowtype_field_matches(con->consttype, newfselect->fieldnum, newfselect->resulttype, newfselect->resulttypmod, newfselect->resultcollid))
      {
        return ece_evaluate_expr(newfselect);
      }
    }
    return (Node *)newfselect;
  }
  case T_NullTest:
  {
    NullTest *ntest = (NullTest *)node;
    NullTest *newntest;
    Node *arg;

    arg = eval_const_expressions_mutator((Node *)ntest->arg, context);
    if (ntest->argisrow && arg && IsA(arg, RowExpr))
    {
         
                                                                
                                                          
                                                               
                          
         
      RowExpr *rarg = (RowExpr *)arg;
      List *newargs = NIL;
      ListCell *l;

      foreach (l, rarg->args)
      {
        Node *relem = (Node *)lfirst(l);

           
                                                               
                                                          
           
        if (relem && IsA(relem, Const))
        {
          Const *carg = (Const *)relem;

          if (carg->constisnull ? (ntest->nulltesttype == IS_NOT_NULL) : (ntest->nulltesttype == IS_NULL))
          {
            return makeBoolConst(false, false);
          }
          continue;
        }

           
                                                            
                                                          
                                                               
                                    
           
        newntest = makeNode(NullTest);
        newntest->arg = (Expr *)relem;
        newntest->nulltesttype = ntest->nulltesttype;
        newntest->argisrow = false;
        newntest->location = ntest->location;
        newargs = lappend(newargs, newntest);
      }
                                                            
      if (newargs == NIL)
      {
        return makeBoolConst(true, false);
      }
                                                       
      if (list_length(newargs) == 1)
      {
        return (Node *)linitial(newargs);
      }
                                    
      return (Node *)make_andclause(newargs);
    }
    if (!ntest->argisrow && arg && IsA(arg, Const))
    {
      Const *carg = (Const *)arg;
      bool result;

      switch (ntest->nulltesttype)
      {
      case IS_NULL:
        result = carg->constisnull;
        break;
      case IS_NOT_NULL:
        result = !carg->constisnull;
        break;
      default:
        elog(ERROR, "unrecognized nulltesttype: %d", (int)ntest->nulltesttype);
        result = false;                          
        break;
      }

      return makeBoolConst(result, false);
    }

    newntest = makeNode(NullTest);
    newntest->arg = (Expr *)arg;
    newntest->nulltesttype = ntest->nulltesttype;
    newntest->argisrow = ntest->argisrow;
    newntest->location = ntest->location;
    return (Node *)newntest;
  }
  case T_BooleanTest:
  {
       
                                                                
                                                                
                                                                   
                                                                  
                                                              
       
    BooleanTest *btest = (BooleanTest *)node;
    BooleanTest *newbtest;
    Node *arg;

    arg = eval_const_expressions_mutator((Node *)btest->arg, context);
    if (arg && IsA(arg, Const))
    {
      Const *carg = (Const *)arg;
      bool result;

      switch (btest->booltesttype)
      {
      case IS_TRUE:
        result = (!carg->constisnull && DatumGetBool(carg->constvalue));
        break;
      case IS_NOT_TRUE:
        result = (carg->constisnull || !DatumGetBool(carg->constvalue));
        break;
      case IS_FALSE:
        result = (!carg->constisnull && !DatumGetBool(carg->constvalue));
        break;
      case IS_NOT_FALSE:
        result = (carg->constisnull || DatumGetBool(carg->constvalue));
        break;
      case IS_UNKNOWN:
        result = carg->constisnull;
        break;
      case IS_NOT_UNKNOWN:
        result = !carg->constisnull;
        break;
      default:
        elog(ERROR, "unrecognized booltesttype: %d", (int)btest->booltesttype);
        result = false;                          
        break;
      }

      return makeBoolConst(result, false);
    }

    newbtest = makeNode(BooleanTest);
    newbtest->arg = (Expr *)arg;
    newbtest->booltesttype = btest->booltesttype;
    newbtest->location = btest->location;
    return (Node *)newbtest;
  }
  case T_CoerceToDomain:
  {
       
                                                                  
                                                               
                                                             
                                                                  
                                                   
       
                                                               
                                                                   
       
    CoerceToDomain *cdomain = (CoerceToDomain *)node;
    CoerceToDomain *newcdomain;
    Node *arg;

    arg = eval_const_expressions_mutator((Node *)cdomain->arg, context);
    if (context->estimate || !DomainHasConstraints(cdomain->resulttype))
    {
                                                            
      if (context->root && !context->estimate)
      {
        record_plan_type_dependency(context->root, cdomain->resulttype);
      }

                                                                 
      return applyRelabelType(arg, cdomain->resulttype, cdomain->resulttypmod, cdomain->resultcollid, cdomain->coercionformat, cdomain->location, true);
    }

    newcdomain = makeNode(CoerceToDomain);
    newcdomain->arg = (Expr *)arg;
    newcdomain->resulttype = cdomain->resulttype;
    newcdomain->resulttypmod = cdomain->resulttypmod;
    newcdomain->resultcollid = cdomain->resultcollid;
    newcdomain->coercionformat = cdomain->coercionformat;
    newcdomain->location = cdomain->location;
    return (Node *)newcdomain;
  }
  case T_PlaceHolderVar:

       
                                                              
                                                                       
                                                                     
                                                                      
                                              
       
    if (context->estimate)
    {
      PlaceHolderVar *phv = (PlaceHolderVar *)node;

      return eval_const_expressions_mutator((Node *)phv->phexpr, context);
    }
    break;
  case T_ConvertRowtypeExpr:
  {
    ConvertRowtypeExpr *cre = castNode(ConvertRowtypeExpr, node);
    Node *arg;
    ConvertRowtypeExpr *newcre;

    arg = eval_const_expressions_mutator((Node *)cre->arg, context);

    newcre = makeNode(ConvertRowtypeExpr);
    newcre->resulttype = cre->resulttype;
    newcre->convertformat = cre->convertformat;
    newcre->location = cre->location;

       
                                                                  
                                                               
                                                     
                                                         
                                                                   
                                                              
                                                     
       
                                                              
                                                          
       
    if (arg != NULL && IsA(arg, ConvertRowtypeExpr))
    {
      ConvertRowtypeExpr *argcre = (ConvertRowtypeExpr *)arg;

      arg = (Node *)argcre->arg;

         
                                                              
                             
         
      if (newcre->convertformat == COERCE_IMPLICIT_CAST)
      {
        newcre->convertformat = argcre->convertformat;
      }
    }

    newcre->arg = (Expr *)arg;

    if (arg != NULL && IsA(arg, Const))
    {
      return ece_evaluate_expr((Node *)newcre);
    }
    return (Node *)newcre;
  }
  default:
    break;
  }

     
                                                                      
                                                                            
                                                                            
                                                                        
                            
     
  return ece_generic_processing(node);
}

   
                                                                     
   
                                                                            
                                                                            
                                                                          
                                                                             
                                       
   
static bool
contain_non_const_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Const))
  {
    return false;
  }
  if (IsA(node, List))
  {
    return expression_tree_walker(node, contain_non_const_walker, context);
  }
                                                           
  return true;
}

   
                                                                                
   
static bool
ece_function_is_safe(Oid funcid, eval_const_expressions_context *context)
{
  char provolatile = func_volatile(funcid);

     
                                                                             
                                                                            
                                                                            
                                                                            
                                   
     
  if (provolatile == PROVOLATILE_IMMUTABLE)
  {
    return true;
  }
  if (context->estimate && provolatile == PROVOLATILE_STABLE)
  {
    return true;
  }
  return false;
}

   
                                                                            
   
                                                                  
                                                        
   
                                                              
                       
                                         
                               
                        
                                                                               
                                                                               
                                                   
   
                                                                           
                                                                              
                                                            
   
static List *
simplify_or_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceTrue)
{
  List *newargs = NIL;
  List *unprocessed_args;

     
                                                                       
                                                                         
     
                                                                          
                                                                        
                                                                            
                                                                          
                                                                            
                                                       
     
  unprocessed_args = list_copy(args);
  while (unprocessed_args)
  {
    Node *arg = (Node *)linitial(unprocessed_args);

    unprocessed_args = list_delete_first(unprocessed_args);

                                                 
    if (is_orclause(arg))
    {
      List *subargs = list_copy(((BoolExpr *)arg)->args);

                                                                 
      if (!unprocessed_args)
      {
        unprocessed_args = subargs;
      }
      else
      {
        List *oldhdr = unprocessed_args;

        unprocessed_args = list_concat(subargs, unprocessed_args);
        pfree(oldhdr);
      }
      continue;
    }

                                        
    arg = eval_const_expressions_mutator(arg, context);

       
                                                                        
                                                                          
                                                                      
                                                
       
    if (is_orclause(arg))
    {
      List *subargs = list_copy(((BoolExpr *)arg)->args);

      unprocessed_args = list_concat(subargs, unprocessed_args);
      continue;
    }

       
                                                                       
                       
       
    if (IsA(arg, Const))
    {
      Const *const_input = (Const *)arg;

      if (const_input->constisnull)
      {
        *haveNull = true;
      }
      else if (DatumGetBool(const_input->constvalue))
      {
        *forceTrue = true;

           
                                                                  
                                                             
                                                                
           
        return NIL;
      }
                                                           
      continue;
    }

                                                           
    newargs = lappend(newargs, arg);
  }

  return newargs;
}

   
                                                                             
   
                                                                   
                                                         
   
                                                               
                       
                                        
                                 
                        
                                                                             
                                                                              
                                                        
   
                                                                            
                                                                               
                                                            
   
static List *
simplify_and_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceFalse)
{
  List *newargs = NIL;
  List *unprocessed_args;

                                             
  unprocessed_args = list_copy(args);
  while (unprocessed_args)
  {
    Node *arg = (Node *)linitial(unprocessed_args);

    unprocessed_args = list_delete_first(unprocessed_args);

                                                  
    if (is_andclause(arg))
    {
      List *subargs = list_copy(((BoolExpr *)arg)->args);

                                                                 
      if (!unprocessed_args)
      {
        unprocessed_args = subargs;
      }
      else
      {
        List *oldhdr = unprocessed_args;

        unprocessed_args = list_concat(subargs, unprocessed_args);
        pfree(oldhdr);
      }
      continue;
    }

                                         
    arg = eval_const_expressions_mutator(arg, context);

       
                                                                         
                                                                           
                                                                      
                                                
       
    if (is_andclause(arg))
    {
      List *subargs = list_copy(((BoolExpr *)arg)->args);

      unprocessed_args = list_concat(subargs, unprocessed_args);
      continue;
    }

       
                                                                        
                       
       
    if (IsA(arg, Const))
    {
      Const *const_input = (Const *)arg;

      if (const_input->constisnull)
      {
        *haveNull = true;
      }
      else if (!DatumGetBool(const_input->constvalue))
      {
        *forceFalse = true;

           
                                                                   
                                                             
                                                                
           
        return NIL;
      }
                                                          
      continue;
    }

                                                           
    newargs = lappend(newargs, arg);
  }

  return newargs;
}

   
                                                                           
                           
   
                                                                             
                                                                    
                            
   
                                                                            
                                                                
                                                                              
                                                                          
                                    
   
                                                                          
                                                       
   
static Node *
simplify_boolean_equality(Oid opno, List *args)
{
  Node *leftop;
  Node *rightop;

  Assert(list_length(args) == 2);
  leftop = linitial(args);
  rightop = lsecond(args);
  if (leftop && IsA(leftop, Const))
  {
    Assert(!((Const *)leftop)->constisnull);
    if (opno == BooleanEqualOperator)
    {
      if (DatumGetBool(((Const *)leftop)->constvalue))
      {
        return rightop;                 
      }
      else
      {
        return negate_clause(rightop);                  
      }
    }
    else
    {
      if (DatumGetBool(((Const *)leftop)->constvalue))
      {
        return negate_clause(rightop);                  
      }
      else
      {
        return rightop;                   
      }
    }
  }
  if (rightop && IsA(rightop, Const))
  {
    Assert(!((Const *)rightop)->constisnull);
    if (opno == BooleanEqualOperator)
    {
      if (DatumGetBool(((Const *)rightop)->constvalue))
      {
        return leftop;                 
      }
      else
      {
        return negate_clause(leftop);                  
      }
    }
    else
    {
      if (DatumGetBool(((Const *)rightop)->constvalue))
      {
        return negate_clause(leftop);                  
      }
      else
      {
        return leftop;                   
      }
    }
  }
  return NULL;
}

   
                                                                          
                                                                 
   
                                                                            
                                                                      
                                                                      
                                                                        
                                                     
   
                                                                    
                               
   
                                                                            
                                                                            
                                                                          
                                                                
                                                                            
                                                                          
             
   
static Expr *
simplify_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List **args_p, bool funcvariadic, bool process_args, bool allow_non_const, eval_const_expressions_context *context)
{
  List *args = *args_p;
  HeapTuple func_tuple;
  Form_pg_proc func_form;
  Expr *newexpr;

     
                                                                          
                                                                       
                                                                      
                                                                         
                                                                       
                                           
     
                                                                         
                                                                             
                                                                     
     
  func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(func_tuple))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  func_form = (Form_pg_proc)GETSTRUCT(func_tuple);

     
                                                                       
     
                                                                   
                                                                          
     
  if (process_args)
  {
    args = expand_function_arguments(args, result_type, func_tuple);
    args = (List *)expression_tree_mutator((Node *)args, eval_const_expressions_mutator, (void *)context);
                                                              
    *args_p = args;
  }

                                                               

  newexpr = evaluate_function(funcid, result_type, result_typmod, result_collid, input_collid, args, funcvariadic, func_tuple, context);

  if (!newexpr && allow_non_const && OidIsValid(func_form->prosupport))
  {
       
                                                                  
                                                                  
                                                                       
                                                                      
                                           
       
    SupportRequestSimplify req;
    FuncExpr fexpr;

    fexpr.xpr.type = T_FuncExpr;
    fexpr.funcid = funcid;
    fexpr.funcresulttype = result_type;
    fexpr.funcretset = func_form->proretset;
    fexpr.funcvariadic = funcvariadic;
    fexpr.funcformat = COERCE_EXPLICIT_CALL;
    fexpr.funccollid = result_collid;
    fexpr.inputcollid = input_collid;
    fexpr.args = args;
    fexpr.location = -1;

    req.type = T_SupportRequestSimplify;
    req.root = context->root;
    req.fcall = &fexpr;

    newexpr = (Expr *)DatumGetPointer(OidFunctionCall1(func_form->prosupport, PointerGetDatum(&req)));

                                               
    Assert(newexpr != (Expr *)&fexpr);
  }

  if (!newexpr && allow_non_const)
  {
    newexpr = inline_function(funcid, result_type, result_collid, input_collid, args, funcvariadic, func_tuple, context);
  }

  ReleaseSysCache(func_tuple);

  return newexpr;
}

   
                                                                             
                                         
   
                                                                         
             
   
                                                                           
                                                                          
                                                            
   
List *
expand_function_arguments(List *args, Oid result_type, HeapTuple func_tuple)
{
  Form_pg_proc funcform = (Form_pg_proc)GETSTRUCT(func_tuple);
  bool has_named_args = false;
  ListCell *lc;

                                       
  foreach (lc, args)
  {
    Node *arg = (Node *)lfirst(lc);

    if (IsA(arg, NamedArgExpr))
    {
      has_named_args = true;
      break;
    }
  }

                                                       
  if (has_named_args)
  {
    args = reorder_function_arguments(args, func_tuple);
                                                        
    recheck_cast_function_args(args, result_type, func_tuple);
  }
  else if (list_length(args) < funcform->pronargs)
  {
                                                              
    args = add_function_defaults(args, func_tuple);
                                                        
    recheck_cast_function_args(args, result_type, func_tuple);
  }

  return args;
}

   
                                                                              
   
                                                                            
                                                                  
   
static List *
reorder_function_arguments(List *args, HeapTuple func_tuple)
{
  Form_pg_proc funcform = (Form_pg_proc)GETSTRUCT(func_tuple);
  int pronargs = funcform->pronargs;
  int nargsprovided = list_length(args);
  Node *argarray[FUNC_MAX_ARGS];
  ListCell *lc;
  int i;

  Assert(nargsprovided <= pronargs);
  if (pronargs < 0 || pronargs > FUNC_MAX_ARGS)
  {
    elog(ERROR, "too many function arguments");
  }
  memset(argarray, 0, pronargs * sizeof(Node *));

                                                                        
  i = 0;
  foreach (lc, args)
  {
    Node *arg = (Node *)lfirst(lc);

    if (!IsA(arg, NamedArgExpr))
    {
                                                                  
      Assert(argarray[i] == NULL);
      argarray[i++] = arg;
    }
    else
    {
      NamedArgExpr *na = (NamedArgExpr *)arg;

      Assert(argarray[na->argnumber] == NULL);
      argarray[na->argnumber] = (Node *)na->arg;
    }
  }

     
                                                                           
                                                                 
     
  if (nargsprovided < pronargs)
  {
    List *defaults = fetch_function_defaults(func_tuple);

    i = pronargs - funcform->pronargdefaults;
    foreach (lc, defaults)
    {
      if (argarray[i] == NULL)
      {
        argarray[i] = (Node *)lfirst(lc);
      }
      i++;
    }
  }

                                                     
  args = NIL;
  for (i = 0; i < pronargs; i++)
  {
    Assert(argarray[i] != NULL);
    args = lappend(args, argarray[i]);
  }

  return args;
}

   
                                                                           
   
                                                                          
                                                           
   
static List *
add_function_defaults(List *args, HeapTuple func_tuple)
{
  Form_pg_proc funcform = (Form_pg_proc)GETSTRUCT(func_tuple);
  int nargsprovided = list_length(args);
  List *defaults;
  int ndelete;

                                                              
  defaults = fetch_function_defaults(func_tuple);

                                                
  ndelete = nargsprovided + list_length(defaults) - funcform->pronargs;
  if (ndelete < 0)
  {
    elog(ERROR, "not enough default arguments");
  }
  while (ndelete-- > 0)
  {
    defaults = list_delete_first(defaults);
  }

                                                                         
  return list_concat(list_copy(args), defaults);
}

   
                                                                                
   
static List *
fetch_function_defaults(HeapTuple func_tuple)
{
  List *defaults;
  Datum proargdefaults;
  bool isnull;
  char *str;

                                                               
  proargdefaults = SysCacheGetAttr(PROCOID, func_tuple, Anum_pg_proc_proargdefaults, &isnull);
  if (isnull)
  {
    elog(ERROR, "not enough default arguments");
  }
  str = TextDatumGetCString(proargdefaults);
  defaults = castNode(List, stringToNode(str));
  pfree(str);
  return defaults;
}

   
                                                                            
                          
   
                                                                         
                                                                           
                                                                           
                             
   
                                                                 
                                   
   
                                                                      
                                                         
   
static void
recheck_cast_function_args(List *args, Oid result_type, HeapTuple func_tuple)
{
  Form_pg_proc funcform = (Form_pg_proc)GETSTRUCT(func_tuple);
  int nargs;
  Oid actual_arg_types[FUNC_MAX_ARGS];
  Oid declared_arg_types[FUNC_MAX_ARGS];
  Oid rettype;
  ListCell *lc;

  if (list_length(args) > FUNC_MAX_ARGS)
  {
    elog(ERROR, "too many function arguments");
  }
  nargs = 0;
  foreach (lc, args)
  {
    actual_arg_types[nargs++] = exprType((Node *)lfirst(lc));
  }
  Assert(nargs == funcform->pronargs);
  memcpy(declared_arg_types, funcform->proargtypes.values, funcform->pronargs * sizeof(Oid));
  rettype = enforce_generic_type_consistency(actual_arg_types, declared_arg_types, nargs, funcform->prorettype, false);
                                                                     
  if (rettype != result_type)
  {
    elog(ERROR, "function's resolved result type changed during planning");
  }

                                                      
  make_fn_arguments(NULL, args, actual_arg_types, declared_arg_types);
}

   
                                                          
   
                                                                             
                                                                              
                                                                        
                                                                        
   
                                                                    
                          
   
static Expr *
evaluate_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context)
{
  Form_pg_proc funcform = (Form_pg_proc)GETSTRUCT(func_tuple);
  bool has_nonconst_input = false;
  bool has_null_input = false;
  ListCell *arg;
  FuncExpr *newexpr;

     
                                         
     
  if (funcform->proretset)
  {
    return NULL;
  }

     
                                                                            
                                                                     
     
                                                                         
                                                                        
                                                                          
                                                                           
                                                                            
                                                               
     
  if (funcform->prorettype == RECORDOID)
  {
    return NULL;
  }

     
                                                                    
     
  foreach (arg, args)
  {
    if (IsA(lfirst(arg), Const))
    {
      has_null_input |= ((Const *)lfirst(arg))->constisnull;
    }
    else
    {
      has_nonconst_input = true;
    }
  }

     
                                                                            
                                                                           
                                                                     
                                          
     
  if (funcform->proisstrict && has_null_input)
  {
    return (Expr *)makeNullConst(result_type, result_typmod, result_collid);
  }

     
                                                                      
                                                                       
                                
     
  if (has_nonconst_input)
  {
    return NULL;
  }

     
                                                                             
                                                                            
                                                                            
                                                                            
                                   
     
  if (funcform->provolatile == PROVOLATILE_IMMUTABLE)
              ;
  else if (context->estimate && funcform->provolatile == PROVOLATILE_STABLE)
              ;
  else
  {
    return NULL;
  }

     
                                                            
     
                                                                            
     
  newexpr = makeNode(FuncExpr);
  newexpr->funcid = funcid;
  newexpr->funcresulttype = result_type;
  newexpr->funcretset = false;
  newexpr->funcvariadic = funcvariadic;
  newexpr->funcformat = COERCE_EXPLICIT_CALL;                     
  newexpr->funccollid = result_collid;                            
  newexpr->inputcollid = input_collid;
  newexpr->args = args;
  newexpr->location = -1;

  return evaluate_expr((Expr *)newexpr, result_type, result_typmod, result_collid);
}

   
                                                         
   
                                                                  
                                                                          
                                                                          
                                                                      
   
                                                                   
                                                                    
                                                                    
                                                                       
                                                                       
                                                                      
                                                                         
                                                                         
                                                                          
                         
   
                                                                          
                               
   
                                                                         
                                                                         
                                  
   
                                                                              
   
                                                                    
                          
   
static Expr *
inline_function(Oid funcid, Oid result_type, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context)
{
  Form_pg_proc funcform = (Form_pg_proc)GETSTRUCT(func_tuple);
  char *src;
  Datum tmp;
  bool isNull;
  bool modifyTargetList;
  MemoryContext oldcxt;
  MemoryContext mycxt;
  inline_error_callback_arg callback_arg;
  ErrorContextCallback sqlerrcontext;
  FuncExpr *fexpr;
  SQLFunctionParseInfoPtr pinfo;
  ParseState *pstate;
  List *raw_parsetree_list;
  Query *querytree;
  Node *newexpr;
  int *usecounts;
  ListCell *arg;
  int i;

     
                                                                            
                                                                    
     
  if (funcform->prolang != SQLlanguageId || funcform->prokind != PROKIND_FUNCTION || funcform->prosecdef || funcform->proretset || funcform->prorettype == RECORDOID || !heap_attisnull(func_tuple, Anum_pg_proc_proconfig, NULL) || funcform->pronargs != list_length(args))
  {
    return NULL;
  }

                                                                        
  if (list_member_oid(context->active_fns, funcid))
  {
    return NULL;
  }

                                                              
  if (pg_proc_aclcheck(funcid, GetUserId(), ACL_EXECUTE) != ACLCHECK_OK)
  {
    return NULL;
  }

                                                                
  if (FmgrHookIsNeeded(funcid))
  {
    return NULL;
  }

     
                                                                          
                                
     
  mycxt = AllocSetContextCreate(CurrentMemoryContext, "inline_function", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(mycxt);

                               
  tmp = SysCacheGetAttr(PROCOID, func_tuple, Anum_pg_proc_prosrc, &isNull);
  if (isNull)
  {
    elog(ERROR, "null prosrc for function %u", funcid);
  }
  src = TextDatumGetCString(tmp);

     
                                                                          
                                                         
     
  callback_arg.proname = NameStr(funcform->proname);
  callback_arg.prosrc = src;

  sqlerrcontext.callback = sql_inline_error_callback;
  sqlerrcontext.arg = (void *)&callback_arg;
  sqlerrcontext.previous = error_context_stack;
  error_context_stack = &sqlerrcontext;

     
                                                                             
                                                                             
                                                                           
                                                                         
     
  fexpr = makeNode(FuncExpr);
  fexpr->funcid = funcid;
  fexpr->funcresulttype = result_type;
  fexpr->funcretset = false;
  fexpr->funcvariadic = funcvariadic;
  fexpr->funcformat = COERCE_EXPLICIT_CALL;                     
  fexpr->funccollid = result_collid;                            
  fexpr->inputcollid = input_collid;
  fexpr->args = args;
  fexpr->location = -1;

  pinfo = prepare_sql_fn_parse_info(func_tuple, (Node *)fexpr, input_collid);

     
                                                                             
                                                                          
                                                                       
                                   
     
  raw_parsetree_list = pg_parse_query(src);
  if (list_length(raw_parsetree_list) != 1)
  {
    goto fail;
  }

  pstate = make_parsestate(NULL);
  pstate->p_sourcetext = src;
  sql_fn_parser_setup(pstate, pinfo);

  querytree = transformTopLevelStmt(pstate, linitial(raw_parsetree_list));

  free_parsestate(pstate);

     
                                                              
     
                                                                        
                                                                           
                                                                       
                                                                  
     
  if (!IsA(querytree, Query) || querytree->commandType != CMD_SELECT || querytree->hasAggs || querytree->hasWindowFuncs || querytree->hasTargetSRFs || querytree->hasSubLinks || querytree->cteList || querytree->rtable || querytree->jointree->fromlist || querytree->jointree->quals || querytree->groupClause || querytree->groupingSets || querytree->havingQual || querytree->windowClause || querytree->distinctClause || querytree->sortClause || querytree->limitOffset || querytree->limitCount || querytree->setOperations || list_length(querytree->targetList) != 1)
  {
    goto fail;
  }

     
                                                                         
                                                                            
                                                                             
                                                                             
                           
     
                                                                           
                                                                  
     
  if (check_sql_fn_retval(funcid, result_type, list_make1(querytree), &modifyTargetList, NULL))
  {
    goto fail;                                      
  }

                                            
  newexpr = (Node *)((TargetEntry *)linitial(querytree->targetList))->expr;

     
                                                                        
                                                                            
                                                                         
                                                                           
                                                                             
                                         
     
  if (exprType(newexpr) != result_type)
  {
    goto fail;
  }

                                                                          
  Assert(!modifyTargetList);

     
                                                                       
                                                                             
                                                                            
                                                                         
                                                                          
                                                                   
     
  if (funcform->provolatile == PROVOLATILE_IMMUTABLE && contain_mutable_functions(newexpr))
  {
    goto fail;
  }
  else if (funcform->provolatile == PROVOLATILE_STABLE && contain_volatile_functions(newexpr))
  {
    goto fail;
  }

  if (funcform->proisstrict && contain_nonstrict_functions(newexpr))
  {
    goto fail;
  }

     
                                                                             
                                                                     
     
  if (contain_context_dependent_node((Node *)args))
  {
    goto fail;
  }

     
                                                                           
                                                                         
                                                                           
                  
     
  usecounts = (int *)palloc0(funcform->pronargs * sizeof(int));
  newexpr = substitute_actual_parameters(newexpr, funcform->pronargs, args, usecounts);

                                     
  i = 0;
  foreach (arg, args)
  {
    Node *param = lfirst(arg);

    if (usecounts[i] == 0)
    {
                                                           
      if (funcform->proisstrict)
      {
        goto fail;
      }
    }
    else if (usecounts[i] != 1)
    {
                                                                      
      QualCost eval_cost;

         
                                                                        
                                                                  
                                                                   
                     
         
      if (contain_subplans(param))
      {
        goto fail;
      }
      cost_qual_eval(&eval_cost, list_make1(param), NULL);
      if (eval_cost.startup + eval_cost.per_tuple > 10 * cpu_operator_cost)
      {
        goto fail;
      }

         
                                                                     
                     
         
      if (contain_volatile_functions(param))
      {
        goto fail;
      }
    }
    i++;
  }

     
                                                                          
                                                        
     
  MemoryContextSwitchTo(oldcxt);

  newexpr = copyObject(newexpr);

  MemoryContextDelete(mycxt);

     
                                                                           
                                                                      
                                                                            
                                                                     
     
  if (OidIsValid(result_collid))
  {
    Oid exprcoll = exprCollation(newexpr);

    if (OidIsValid(exprcoll) && exprcoll != result_collid)
    {
      CollateExpr *newnode = makeNode(CollateExpr);

      newnode->arg = (Expr *)newexpr;
      newnode->collOid = result_collid;
      newnode->location = -1;

      newexpr = (Node *)newnode;
    }
  }

     
                                                                           
                                                              
     
  if (context->root)
  {
    record_plan_function_dependency(context->root, funcid);
  }

     
                                                                            
                                                                   
     
  context->active_fns = lcons_oid(funcid, context->active_fns);
  newexpr = eval_const_expressions_mutator(newexpr, context);
  context->active_fns = list_delete_first(context->active_fns);

  error_context_stack = sqlerrcontext.previous;

  return (Expr *)newexpr;

                                                                          
fail:
  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(mycxt);
  error_context_stack = sqlerrcontext.previous;

  return NULL;
}

   
                                                        
   
static Node *
substitute_actual_parameters(Node *expr, int nargs, List *args, int *usecounts)
{
  substitute_actual_parameters_context context;

  context.nargs = nargs;
  context.args = args;
  context.usecounts = usecounts;

  return substitute_actual_parameters_mutator(expr, &context);
}

static Node *
substitute_actual_parameters_mutator(Node *node, substitute_actual_parameters_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind != PARAM_EXTERN)
    {
      elog(ERROR, "unexpected paramkind: %d", (int)param->paramkind);
    }
    if (param->paramid <= 0 || param->paramid > context->nargs)
    {
      elog(ERROR, "invalid paramid: %d", param->paramid);
    }

                                  
    context->usecounts[param->paramid - 1]++;

                                                                         
                                                                   
    return list_nth(context->args, param->paramid - 1);
  }
  return expression_tree_mutator(node, substitute_actual_parameters_mutator, (void *)context);
}

   
                                                                  
   
static void
sql_inline_error_callback(void *arg)
{
  inline_error_callback_arg *callback_arg = (inline_error_callback_arg *)arg;
  int syntaxerrposition;

                                                                       
  syntaxerrposition = geterrposition();
  if (syntaxerrposition > 0)
  {
    errposition(0);
    internalerrposition(syntaxerrposition);
    internalerrquery(callback_arg->prosrc);
  }

  errcontext("SQL function \"%s\" during inlining", callback_arg->proname);
}

   
                                                     
   
                                                                        
                                                                     
   
Expr *
evaluate_expr(Expr *expr, Oid result_type, int32 result_typmod, Oid result_collation)
{
  EState *estate;
  ExprState *exprstate;
  MemoryContext oldcontext;
  Datum const_val;
  bool const_is_null;
  int16 resultTypLen;
  bool resultTypByVal;

     
                                             
     
  estate = CreateExecutorState();

                                                                      
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

                                              
  fix_opfuncids((Node *)expr);

     
                                                                      
                                                                          
     
  exprstate = ExecInitExpr(expr, NULL);

     
                      
     
                                                                           
                                                                     
                                                                             
                                                         
     
  const_val = ExecEvalExprSwitchContext(exprstate, GetPerTupleExprContext(estate), &const_is_null);

                                             
  get_typlenbyval(result_type, &resultTypLen, &resultTypByVal);

                                        
  MemoryContextSwitchTo(oldcontext);

     
                                                                  
     
                                                                           
                                                                         
                                                                            
                                                                           
     
  if (!const_is_null)
  {
    if (resultTypLen == -1)
    {
      const_val = PointerGetDatum(PG_DETOAST_DATUM_COPY(const_val));
    }
    else
    {
      const_val = datumCopy(const_val, resultTypByVal, resultTypLen);
    }
  }

                                            
  FreeExecutorState(estate);

     
                                    
     
  return (Expr *)makeConst(result_type, result_typmod, result_collation, resultTypLen, const_val, const_is_null, resultTypByVal);
}

   
                                 
                                                                     
   
                                                                            
                                                                              
                                                                       
   
                                                                       
                                                                       
                               
   
Query *
inline_set_returning_function(PlannerInfo *root, RangeTblEntry *rte)
{
  RangeTblFunction *rtfunc;
  FuncExpr *fexpr;
  Oid func_oid;
  HeapTuple func_tuple;
  Form_pg_proc funcform;
  char *src;
  Datum tmp;
  bool isNull;
  bool modifyTargetList;
  MemoryContext oldcxt;
  MemoryContext mycxt;
  List *saveInvalItems;
  inline_error_callback_arg callback_arg;
  ErrorContextCallback sqlerrcontext;
  SQLFunctionParseInfoPtr pinfo;
  List *raw_parsetree_list;
  List *querytree_list;
  Query *querytree;

  Assert(rte->rtekind == RTE_FUNCTION);

     
                                                                            
                                                                           
                                                                          
                                   
     
  check_stack_depth();

                                                                      
  if (rte->funcordinality)
  {
    return NULL;
  }

                                                   
  if (list_length(rte->functions) != 1)
  {
    return NULL;
  }
  rtfunc = (RangeTblFunction *)linitial(rte->functions);

  if (!IsA(rtfunc->funcexpr, FuncExpr))
  {
    return NULL;
  }
  fexpr = (FuncExpr *)rtfunc->funcexpr;

  func_oid = fexpr->funcid;

     
                                                                        
                                                                          
          
     
  if (!fexpr->funcretset)
  {
    return NULL;
  }

     
                                                                         
                                                                        
                                                                       
                                                                             
                                                                           
                                                                       
                              
     
  if (contain_volatile_functions((Node *)fexpr->args) || contain_subplans((Node *)fexpr->args))
  {
    return NULL;
  }

                                                              
  if (pg_proc_aclcheck(func_oid, GetUserId(), ACL_EXECUTE) != ACLCHECK_OK)
  {
    return NULL;
  }

                                                                
  if (FmgrHookIsNeeded(func_oid))
  {
    return NULL;
  }

     
                                                            
     
  func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(func_oid));
  if (!HeapTupleIsValid(func_tuple))
  {
    elog(ERROR, "cache lookup failed for function %u", func_oid);
  }
  funcform = (Form_pg_proc)GETSTRUCT(func_tuple);

     
                                                                            
                                                                        
                                                                          
                                                                            
                                                                            
                                                                             
                                                                          
                                                          
     
  if (funcform->prolang != SQLlanguageId || funcform->prokind != PROKIND_FUNCTION || funcform->proisstrict || funcform->provolatile == PROVOLATILE_VOLATILE || funcform->prorettype == VOIDOID || funcform->prosecdef || !funcform->proretset || !heap_attisnull(func_tuple, Anum_pg_proc_proconfig, NULL))
  {
    ReleaseSysCache(func_tuple);
    return NULL;
  }

     
                                                                          
                                
     
  mycxt = AllocSetContextCreate(CurrentMemoryContext, "inline_set_returning_function", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(mycxt);

     
                                                                             
                                                                             
                                                                            
                                                                             
                                                   
     
  saveInvalItems = root->glob->invalItems;
  root->glob->invalItems = NIL;

                               
  tmp = SysCacheGetAttr(PROCOID, func_tuple, Anum_pg_proc_prosrc, &isNull);
  if (isNull)
  {
    elog(ERROR, "null prosrc for function %u", func_oid);
  }
  src = TextDatumGetCString(tmp);

     
                                                                          
                                                         
     
  callback_arg.proname = NameStr(funcform->proname);
  callback_arg.prosrc = src;

  sqlerrcontext.callback = sql_inline_error_callback;
  sqlerrcontext.arg = (void *)&callback_arg;
  sqlerrcontext.previous = error_context_stack;
  error_context_stack = &sqlerrcontext;

     
                                                                            
                                                                             
                                                                             
                                                                             
           
     
  fexpr = (FuncExpr *)eval_const_expressions(root, (Node *)fexpr);

                                                                       
  if (!IsA(fexpr, FuncExpr) || fexpr->funcid != func_oid)
  {
    goto fail;
  }

                                                     
  if (list_length(fexpr->args) != funcform->pronargs)
  {
    goto fail;
  }

     
                                                                          
                                                    
                                
     
  pinfo = prepare_sql_fn_parse_info(func_tuple, (Node *)fexpr, fexpr->inputcollid);

     
                                                                          
                                                                           
             
     
  raw_parsetree_list = pg_parse_query(src);
  if (list_length(raw_parsetree_list) != 1)
  {
    goto fail;
  }

  querytree_list = pg_analyze_and_rewrite_params(linitial(raw_parsetree_list), src, (ParserSetupHook)sql_fn_parser_setup, pinfo, NULL);
  if (list_length(querytree_list) != 1)
  {
    goto fail;
  }
  querytree = linitial(querytree_list);

     
                                                
     
  if (!IsA(querytree, Query) || querytree->commandType != CMD_SELECT)
  {
    goto fail;
  }

     
                                                                         
                                                                            
                                                                             
                                                                    
                                                            
     
                                                                             
                                                                    
                                                                             
                                                                         
            
     
  if (!check_sql_fn_retval(func_oid, fexpr->funcresulttype, querytree_list, &modifyTargetList, NULL) && (get_typtype(fexpr->funcresulttype) == TYPTYPE_COMPOSITE || fexpr->funcresulttype == RECORDOID))
  {
    goto fail;                                          
  }

     
                                                                          
                                                                         
                                  
     
  if (modifyTargetList)
  {
    goto fail;
  }

     
                                                                         
                                                                            
                                                                    
                                                                        
                                                                       
                                                   
     
  if (fexpr->funcresulttype == RECORDOID && get_func_result_type(func_oid, NULL, NULL) == TYPEFUNC_RECORD && !tlist_matches_coltypelist(querytree->targetList, rtfunc->funccoltypes))
  {
    goto fail;
  }

     
                                                          
     
  querytree = substitute_actual_srf_parameters(querytree, funcform->pronargs, fexpr->args);

     
                                                                            
         
     
  MemoryContextSwitchTo(oldcxt);

  querytree = copyObject(querytree);

                                       
  root->glob->invalItems = list_concat(saveInvalItems, copyObject(root->glob->invalItems));

  MemoryContextDelete(mycxt);
  error_context_stack = sqlerrcontext.previous;
  ReleaseSysCache(func_tuple);

     
                                                                             
                                                           
     

     
                                                                           
                                                              
     
  record_plan_function_dependency(root, func_oid);

  return querytree;

                                                                          
fail:
  MemoryContextSwitchTo(oldcxt);
  root->glob->invalItems = saveInvalItems;
  MemoryContextDelete(mycxt);
  error_context_stack = sqlerrcontext.previous;
  ReleaseSysCache(func_tuple);

  return NULL;
}

   
                                                        
   
                                                                     
                               
   
static Query *
substitute_actual_srf_parameters(Query *expr, int nargs, List *args)
{
  substitute_actual_srf_parameters_context context;

  context.nargs = nargs;
  context.args = args;
  context.sublevels_up = 1;

  return query_tree_mutator(expr, substitute_actual_srf_parameters_mutator, &context, 0);
}

static Node *
substitute_actual_srf_parameters_mutator(Node *node, substitute_actual_srf_parameters_context *context)
{
  Node *result;

  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Query))
  {
    context->sublevels_up++;
    result = (Node *)query_tree_mutator((Query *)node, substitute_actual_srf_parameters_mutator, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_EXTERN)
    {
      if (param->paramid <= 0 || param->paramid > context->nargs)
      {
        elog(ERROR, "invalid paramid: %d", param->paramid);
      }

         
                                                                        
                        
         
      result = copyObject(list_nth(context->args, param->paramid - 1));
      IncrementVarSublevelsUp(result, context->sublevels_up, 0);
      return result;
    }
  }
  return expression_tree_mutator(node, substitute_actual_srf_parameters_mutator, (void *)context);
}

   
                                                                       
                                                              
   
                                                                        
                                                                           
                                                                   
   
                                                                            
                                                                        
                                                       
   
static bool
tlist_matches_coltypelist(List *tlist, List *coltypelist)
{
  ListCell *tlistitem;
  ListCell *clistitem;

  clistitem = list_head(coltypelist);
  foreach (tlistitem, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tlistitem);
    Oid coltype;

    if (tle->resjunk)
    {
      continue;                          
    }

    if (clistitem == NULL)
    {
      return false;                           
    }

    coltype = lfirst_oid(clistitem);
    clistitem = lnext(clistitem);

    if (exprType((Node *)tle->expr) != coltype)
    {
      return false;                           
    }
  }

  if (clistitem != NULL)
  {
    return false;                          
  }

  return true;
}
