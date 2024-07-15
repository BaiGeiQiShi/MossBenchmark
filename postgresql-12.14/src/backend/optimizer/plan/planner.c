                                                                            
   
             
                                             
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "jit/jit.h"
#include "lib/bipartite_match.h"
#include "lib/knapsack.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/paramassign.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "parser/parse_agg.h"
#include "partitioning/partdesc.h"
#include "rewrite/rewriteManip.h"
#include "storage/dsm_impl.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

                    
double cursor_tuple_fraction = DEFAULT_CURSOR_TUPLE_FRACTION;
int force_parallel_mode = FORCE_PARALLEL_OFF;
bool parallel_leader_participation = true;

                                                  
planner_hook_type planner_hook = NULL;

                                                                              
create_upper_paths_hook_type create_upper_paths_hook = NULL;

                                                     
#define EXPRKIND_QUAL 0
#define EXPRKIND_TARGET 1
#define EXPRKIND_RTFUNC 2
#define EXPRKIND_RTFUNC_LATERAL 3
#define EXPRKIND_VALUES 4
#define EXPRKIND_VALUES_LATERAL 5
#define EXPRKIND_LIMIT 6
#define EXPRKIND_APPINFO 7
#define EXPRKIND_PHV 8
#define EXPRKIND_TABLESAMPLE 9
#define EXPRKIND_ARBITER_ELEM 10
#define EXPRKIND_TABLEFUNC 11
#define EXPRKIND_TABLEFUNC_LATERAL 12

                                               
typedef struct
{
  List *activeWindows;                             
  List *groupClause;                                     
} standard_qp_extra;

   
                                  
   

typedef struct
{
  List *rollups;
  List *hash_sets_idx;
  double dNumHashGroups;
  bool any_hashable;
  Bitmapset *unsortable_refs;
  Bitmapset *unhashable_refs;
  List *unsortable_sets;
  int *tleref_to_colnum_map;
} grouping_sets_data;

   
                                                                             
                                                               
   
typedef struct
{
  WindowClause *wc;
  List *uniqueOrder;                                           
                                             
} WindowClauseSortData;

                     
static Node *
preprocess_expression(PlannerInfo *root, Node *expr, int kind);
static void
preprocess_qual_conditions(PlannerInfo *root, Node *jtnode);
static void
inheritance_planner(PlannerInfo *root);
static void
grouping_planner(PlannerInfo *root, bool inheritance_update, double tuple_fraction);
static grouping_sets_data *
preprocess_grouping_sets(PlannerInfo *root);
static List *
remap_to_groupclause_idx(List *groupClause, List *gsets, int *tleref_to_colnum_map);
static void
preprocess_rowmarks(PlannerInfo *root);
static double
preprocess_limit(PlannerInfo *root, double tuple_fraction, int64 *offset_est, int64 *count_est);
static void
remove_useless_groupby_columns(PlannerInfo *root);
static List *
preprocess_groupclause(PlannerInfo *root, List *force);
static List *
extract_rollup_sets(List *groupingSets);
static List *
reorder_grouping_sets(List *groupingSets, List *sortclause);
static void
standard_qp_callback(PlannerInfo *root, void *extra);
static double
get_number_of_groups(PlannerInfo *root, double path_rows, grouping_sets_data *gd, List *target_list);
static RelOptInfo *
create_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, const AggClauseCosts *agg_costs, grouping_sets_data *gd);
static bool
is_degenerate_grouping(PlannerInfo *root);
static void
create_degenerate_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel);
static RelOptInfo *
make_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, Node *havingQual);
static void
create_ordinary_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, GroupPathExtraData *extra, RelOptInfo **partially_grouped_rel_p);
static void
consider_groupingsets_paths(PlannerInfo *root, RelOptInfo *grouped_rel, Path *path, bool is_sorted, bool can_hash, grouping_sets_data *gd, const AggClauseCosts *agg_costs, double dNumGroups);
static RelOptInfo *
create_window_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *input_target, PathTarget *output_target, bool output_target_parallel_safe, WindowFuncLists *wflists, List *activeWindows);
static void
create_one_window_path(PlannerInfo *root, RelOptInfo *window_rel, Path *path, PathTarget *input_target, PathTarget *output_target, WindowFuncLists *wflists, List *activeWindows);
static RelOptInfo *
create_distinct_paths(PlannerInfo *root, RelOptInfo *input_rel);
static RelOptInfo *
create_ordered_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, double limit_tuples);
static PathTarget *
make_group_input_target(PlannerInfo *root, PathTarget *final_target);
static PathTarget *
make_partial_grouping_target(PlannerInfo *root, PathTarget *grouping_target, Node *havingQual);
static List *
postprocess_setop_tlist(List *new_tlist, List *orig_tlist);
static List *
select_active_windows(PlannerInfo *root, WindowFuncLists *wflists);
static PathTarget *
make_window_input_target(PlannerInfo *root, PathTarget *final_target, List *activeWindows);
static List *
make_pathkeys_for_window(PlannerInfo *root, WindowClause *wc, List *tlist);
static PathTarget *
make_sort_input_target(PlannerInfo *root, PathTarget *final_target, bool *have_postponed_srfs);
static void
adjust_paths_for_srfs(PlannerInfo *root, RelOptInfo *rel, List *targets, List *targets_contain_srfs);
static void
add_paths_to_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, double dNumGroups, GroupPathExtraData *extra);
static RelOptInfo *
create_partial_grouping_paths(PlannerInfo *root, RelOptInfo *grouped_rel, RelOptInfo *input_rel, grouping_sets_data *gd, GroupPathExtraData *extra, bool force_rel_creation);
static void
gather_grouping_paths(PlannerInfo *root, RelOptInfo *rel);
static bool
can_partial_agg(PlannerInfo *root, const AggClauseCosts *agg_costs);
static void
apply_scanjoin_target_to_paths(PlannerInfo *root, RelOptInfo *rel, List *scanjoin_targets, List *scanjoin_targets_contain_srfs, bool scanjoin_target_parallel_safe, bool tlist_same_exprs);
static void
create_partitionwise_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, PartitionwiseAggregateType patype, GroupPathExtraData *extra);
static bool
group_by_has_partkey(RelOptInfo *input_rel, List *targetList, List *groupClause);
static int
common_prefix_cmp(const void *a, const void *b);

                                                                               
   
                                  
   
                                                                        
                                                                        
                                                                        
                       
   
                                                                            
                                                                                
   
                                                                               
PlannedStmt *
planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
  PlannedStmt *result;

  if (planner_hook)
  {
    result = (*planner_hook)(parse, cursorOptions, boundParams);
  }
  else
  {
    result = standard_planner(parse, cursorOptions, boundParams);
  }
  return result;
}

PlannedStmt *
standard_planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
  PlannedStmt *result;
  PlannerGlobal *glob;
  double tuple_fraction;
  PlannerInfo *root;
  RelOptInfo *final_rel;
  Path *best_path;
  Plan *top_plan;
  ListCell *lp, *lr;

     
                                                                           
                                                                           
                                                                           
                  
     
  glob = makeNode(PlannerGlobal);

  glob->boundParams = boundParams;
  glob->subplans = NIL;
  glob->subroots = NIL;
  glob->rewindPlanIDs = NULL;
  glob->finalrtable = NIL;
  glob->finalrowmarks = NIL;
  glob->resultRelations = NIL;
  glob->rootResultRelations = NIL;
  glob->relationOids = NIL;
  glob->invalItems = NIL;
  glob->paramExecTypes = NIL;
  glob->lastPHId = 0;
  glob->lastRowMarkId = 0;
  glob->lastPlanNodeId = 0;
  glob->transientPlan = false;
  glob->dependsOnRole = false;

     
                                                                          
                                                                          
                                                                           
                                                                    
                                              
     
                                                                     
                                                                            
                                                                            
                                                                        
                                                                       
                                                                        
                                                                            
                                                                             
                  
     
                                                                          
                                                                 
                                                                         
                                                  
     
  if ((cursorOptions & CURSOR_OPT_PARALLEL_OK) != 0 && IsUnderPostmaster && parse->commandType == CMD_SELECT && !parse->hasModifyingCTE && max_parallel_workers_per_gather > 0 && !IsParallelWorker())
  {
                                                          
    glob->maxParallelHazard = max_parallel_hazard(parse);
    glob->parallelModeOK = (glob->maxParallelHazard != PROPARALLEL_UNSAFE);
  }
  else
  {
                                                           
    glob->maxParallelHazard = PROPARALLEL_UNSAFE;
    glob->parallelModeOK = false;
  }

     
                                                                           
                                                                            
                                                                 
     
                                                                            
                                                                           
                                                                        
                                                                           
                                                                            
                                                                   
                                                                       
                                                                           
                                                                        
                                                                           
                                                                  
     
  glob->parallelModeNeeded = glob->parallelModeOK && (force_parallel_mode != FORCE_PARALLEL_OFF);

                                                                   
  if (cursorOptions & CURSOR_OPT_FAST_PLAN)
  {
       
                                                                           
                                                                        
                                                                    
                                                                         
                                      
       
    tuple_fraction = cursor_tuple_fraction;

       
                                                                           
                                                                           
                                                                         
       
    if (tuple_fraction >= 1.0)
    {
      tuple_fraction = 0.0;
    }
    else if (tuple_fraction <= 0.0)
    {
      tuple_fraction = 1e-10;
    }
  }
  else
  {
                                                      
    tuple_fraction = 0.0;
  }

                                                                 
  root = subquery_planner(glob, parse, NULL, false, tuple_fraction);

                                                
  final_rel = fetch_upper_rel(root, UPPERREL_FINAL, NULL);
  best_path = get_cheapest_fractional_path(final_rel, tuple_fraction);

  top_plan = create_plan(root, best_path);

     
                                                                      
                                                                   
     
  if (cursorOptions & CURSOR_OPT_SCROLL)
  {
    if (!ExecSupportsBackwardScan(top_plan))
    {
      top_plan = materialize_finished_plan(top_plan);
    }
  }

     
                                                                         
                                  
     
  if (force_parallel_mode != FORCE_PARALLEL_OFF && top_plan->parallel_safe)
  {
    Gather *gather = makeNode(Gather);

       
                                                                          
                                                                           
                                  
       
    gather->plan.initPlan = top_plan->initPlan;
    top_plan->initPlan = NIL;

    gather->plan.targetlist = top_plan->targetlist;
    gather->plan.qual = NIL;
    gather->plan.lefttree = top_plan;
    gather->plan.righttree = NULL;
    gather->num_workers = 1;
    gather->single_copy = true;
    gather->invisible = (force_parallel_mode == FORCE_PARALLEL_REGRESS);

       
                                                                         
                                     
       
    gather->rescan_param = -1;

       
                                                                         
                                                                          
       
    gather->plan.startup_cost = top_plan->startup_cost + parallel_setup_cost;
    gather->plan.total_cost = top_plan->total_cost + parallel_setup_cost + parallel_tuple_cost * top_plan->plan_rows;
    gather->plan.plan_rows = top_plan->plan_rows;
    gather->plan.plan_width = top_plan->plan_width;
    gather->plan.parallel_aware = false;
    gather->plan.parallel_safe = false;

                                               
    root->glob->parallelModeNeeded = true;

    top_plan = &gather->plan;
  }

     
                                                                         
                                                                            
                                                                            
                                                                   
     
  if (glob->paramExecTypes != NIL)
  {
    Assert(list_length(glob->subplans) == list_length(glob->subroots));
    forboth(lp, glob->subplans, lr, glob->subroots)
    {
      Plan *subplan = (Plan *)lfirst(lp);
      PlannerInfo *subroot = lfirst_node(PlannerInfo, lr);

      SS_finalize_plan(subroot, subplan);
    }
    SS_finalize_plan(root, top_plan);
  }

                                 
  Assert(glob->finalrtable == NIL);
  Assert(glob->finalrowmarks == NIL);
  Assert(glob->resultRelations == NIL);
  Assert(glob->rootResultRelations == NIL);
  top_plan = set_plan_references(root, top_plan);
                                                                  
  Assert(list_length(glob->subplans) == list_length(glob->subroots));
  forboth(lp, glob->subplans, lr, glob->subroots)
  {
    Plan *subplan = (Plan *)lfirst(lp);
    PlannerInfo *subroot = lfirst_node(PlannerInfo, lr);

    lfirst(lp) = set_plan_references(subroot, subplan);
  }

                                    
  result = makeNode(PlannedStmt);

  result->commandType = parse->commandType;
  result->queryId = parse->queryId;
  result->hasReturning = (parse->returningList != NIL);
  result->hasModifyingCTE = parse->hasModifyingCTE;
  result->canSetTag = parse->canSetTag;
  result->transientPlan = glob->transientPlan;
  result->dependsOnRole = glob->dependsOnRole;
  result->parallelModeNeeded = glob->parallelModeNeeded;
  result->planTree = top_plan;
  result->rtable = glob->finalrtable;
  result->resultRelations = glob->resultRelations;
  result->rootResultRelations = glob->rootResultRelations;
  result->subplans = glob->subplans;
  result->rewindPlanIDs = glob->rewindPlanIDs;
  result->rowMarks = glob->finalrowmarks;
  result->relationOids = glob->relationOids;
  result->invalItems = glob->invalItems;
  result->paramExecTypes = glob->paramExecTypes;
                                                                
  result->utilityStmt = parse->utilityStmt;
  result->stmt_location = parse->stmt_location;
  result->stmt_len = parse->stmt_len;

  result->jitFlags = PGJIT_NONE;
  if (jit_enabled && jit_above_cost >= 0 && top_plan->total_cost > jit_above_cost)
  {
    result->jitFlags |= PGJIT_PERFORM;

       
                                                                         
       
    if (jit_optimize_above_cost >= 0 && top_plan->total_cost > jit_optimize_above_cost)
    {
      result->jitFlags |= PGJIT_OPT3;
    }
    if (jit_inline_above_cost >= 0 && top_plan->total_cost > jit_inline_above_cost)
    {
      result->jitFlags |= PGJIT_INLINE;
    }

       
                                                
       
    if (jit_expressions)
    {
      result->jitFlags |= PGJIT_EXPR;
    }
    if (jit_tuple_deforming)
    {
      result->jitFlags |= PGJIT_DEFORM;
    }
  }

  if (glob->partition_directory != NULL)
  {
    DestroyPartitionDirectory(glob->partition_directory);
  }

  return result;
}

                       
                    
                                                                     
                                         
   
                                                         
                                                             
                                                                             
                                                           
                                                                         
                                                                           
   
                                                                        
                                                                    
                                                                           
                                                                         
                                                      
   
                                                                         
                                                        
   
                                                                            
                                                                        
                                                                           
                                                                          
                                                                          
                       
   
PlannerInfo *
subquery_planner(PlannerGlobal *glob, Query *parse, PlannerInfo *parent_root, bool hasRecursion, double tuple_fraction)
{
  PlannerInfo *root;
  List *newWithCheckOptions;
  List *newHaving;
  bool hasOuterJoins;
  bool hasResultRTEs;
  RelOptInfo *final_rel;
  ListCell *l;

                                                             
  root = makeNode(PlannerInfo);
  root->parse = parse;
  root->glob = glob;
  root->query_level = parent_root ? parent_root->query_level + 1 : 1;
  root->parent_root = parent_root;
  root->plan_params = NIL;
  root->outer_params = NULL;
  root->planner_cxt = CurrentMemoryContext;
  root->init_plans = NIL;
  root->cte_plan_ids = NIL;
  root->multiexpr_params = NIL;
  root->eq_classes = NIL;
  root->append_rel_list = NIL;
  root->rowMarks = NIL;
  memset(root->upper_rels, 0, sizeof(root->upper_rels));
  memset(root->upper_targets, 0, sizeof(root->upper_targets));
  root->processed_tlist = NIL;
  root->grouping_map = NULL;
  root->minmax_aggs = NIL;
  root->qual_security_level = 0;
  root->inhTargetKind = INHKIND_NONE;
  root->hasRecursion = hasRecursion;
  if (hasRecursion)
  {
    root->wt_param_id = assign_special_exec_param(root);
  }
  else
  {
    root->wt_param_id = -1;
  }
  root->non_recursive_path = NULL;
  root->partColsUpdated = false;

     
                                                                            
                                                                           
     
  if (parse->cteList)
  {
    SS_process_ctes(root);
  }

     
                                                                             
                                                                           
     
  replace_empty_jointree(parse);

     
                                                                            
                                                                         
                                                                             
                                            
     
  if (parse->hasSubLinks)
  {
    pull_up_sublinks(root);
  }

     
                                                                         
                                                                    
                                                                        
     
  inline_set_returning_functions(root);

     
                                                                            
            
     
  pull_up_subqueries(root);

     
                                                                           
                                                                             
                                                                        
                                                                         
     
  if (parse->setOperations)
  {
    flatten_simple_union_all(root);
  }

     
                                                                             
                                                                           
                                                                         
                                                                          
                                                                           
                                                                            
                                                                          
                                                   
     
  root->hasJoinRTEs = false;
  root->hasLateralRTEs = false;
  hasOuterJoins = false;
  hasResultRTEs = false;
  foreach (l, parse->rtable)
  {
    RangeTblEntry *rte = lfirst_node(RangeTblEntry, l);

    switch (rte->rtekind)
    {
    case RTE_RELATION:
      if (rte->inh)
      {
           
                                                                   
                                                              
                                
           
                                                                 
                                                                 
                                                                 
                                                              
                                     
           
        rte->inh = has_subclass(rte->relid);
      }
      break;
    case RTE_JOIN:
      root->hasJoinRTEs = true;
      if (IS_OUTER_JOIN(rte->jointype))
      {
        hasOuterJoins = true;
      }
      break;
    case RTE_RESULT:
      hasResultRTEs = true;
      break;
    default:
                                            
      break;
    }

    if (rte->lateral)
    {
      root->hasLateralRTEs = true;
    }

       
                                                                         
                                                                           
                                                                          
                                          
       
    if (rte->securityQuals)
    {
      root->qual_security_level = Max(root->qual_security_level, list_length(rte->securityQuals));
    }
  }

     
                                                                        
                                                     
     
  preprocess_rowmarks(root);

     
                                                                        
                                                                            
                                                                       
     
  root->hasHavingQual = (parse->havingQual != NULL);

                                                                 
  root->hasPseudoConstantQuals = false;

     
                                                                           
                                                                       
                                                                         
                             
     
  parse->targetList = (List *)preprocess_expression(root, (Node *)parse->targetList, EXPRKIND_TARGET);

                                                                       
  if (parse->hasTargetSRFs)
  {
    parse->hasTargetSRFs = expression_returns_set((Node *)parse->targetList);
  }

  newWithCheckOptions = NIL;
  foreach (l, parse->withCheckOptions)
  {
    WithCheckOption *wco = lfirst_node(WithCheckOption, l);

    wco->qual = preprocess_expression(root, wco->qual, EXPRKIND_QUAL);
    if (wco->qual != NULL)
    {
      newWithCheckOptions = lappend(newWithCheckOptions, wco);
    }
  }
  parse->withCheckOptions = newWithCheckOptions;

  parse->returningList = (List *)preprocess_expression(root, (Node *)parse->returningList, EXPRKIND_TARGET);

  preprocess_qual_conditions(root, (Node *)parse->jointree);

  parse->havingQual = preprocess_expression(root, parse->havingQual, EXPRKIND_QUAL);

  foreach (l, parse->windowClause)
  {
    WindowClause *wc = lfirst_node(WindowClause, l);

                                                                
    wc->startOffset = preprocess_expression(root, wc->startOffset, EXPRKIND_LIMIT);
    wc->endOffset = preprocess_expression(root, wc->endOffset, EXPRKIND_LIMIT);
  }

  parse->limitOffset = preprocess_expression(root, parse->limitOffset, EXPRKIND_LIMIT);
  parse->limitCount = preprocess_expression(root, parse->limitCount, EXPRKIND_LIMIT);

  if (parse->onConflict)
  {
    parse->onConflict->arbiterElems = (List *)preprocess_expression(root, (Node *)parse->onConflict->arbiterElems, EXPRKIND_ARBITER_ELEM);
    parse->onConflict->arbiterWhere = preprocess_expression(root, parse->onConflict->arbiterWhere, EXPRKIND_QUAL);
    parse->onConflict->onConflictSet = (List *)preprocess_expression(root, (Node *)parse->onConflict->onConflictSet, EXPRKIND_TARGET);
    parse->onConflict->onConflictWhere = preprocess_expression(root, parse->onConflict->onConflictWhere, EXPRKIND_QUAL);
                                                                     
  }

  root->append_rel_list = (List *)preprocess_expression(root, (Node *)root->append_rel_list, EXPRKIND_APPINFO);

                                                       
  foreach (l, parse->rtable)
  {
    RangeTblEntry *rte = lfirst_node(RangeTblEntry, l);
    int kind;
    ListCell *lcsq;

    if (rte->rtekind == RTE_RELATION)
    {
      if (rte->tablesample)
      {
        rte->tablesample = (TableSampleClause *)preprocess_expression(root, (Node *)rte->tablesample, EXPRKIND_TABLESAMPLE);
      }
    }
    else if (rte->rtekind == RTE_SUBQUERY)
    {
         
                                                                     
                                                                         
                                                                   
                                                                     
                                                          
         
      if (rte->lateral && root->hasJoinRTEs)
      {
        rte->subquery = (Query *)flatten_join_alias_vars(root->parse, (Node *)rte->subquery);
      }
    }
    else if (rte->rtekind == RTE_FUNCTION)
    {
                                                       
      kind = rte->lateral ? EXPRKIND_RTFUNC_LATERAL : EXPRKIND_RTFUNC;
      rte->functions = (List *)preprocess_expression(root, (Node *)rte->functions, kind);
    }
    else if (rte->rtekind == RTE_TABLEFUNC)
    {
                                                       
      kind = rte->lateral ? EXPRKIND_TABLEFUNC_LATERAL : EXPRKIND_TABLEFUNC;
      rte->tablefunc = (TableFunc *)preprocess_expression(root, (Node *)rte->tablefunc, kind);
    }
    else if (rte->rtekind == RTE_VALUES)
    {
                                             
      kind = rte->lateral ? EXPRKIND_VALUES_LATERAL : EXPRKIND_VALUES;
      rte->values_lists = (List *)preprocess_expression(root, (Node *)rte->values_lists, kind);
    }

       
                                                                      
                                                                          
                                                                          
                                                                
       
    foreach (lcsq, rte->securityQuals)
    {
      lfirst(lcsq) = preprocess_expression(root, (Node *)lfirst(lcsq), EXPRKIND_QUAL);
    }
  }

     
                                                                            
                                                                          
                                                                        
                                                                            
                                                                            
                                                                            
                                                                            
                                                                            
                                           
     
  if (root->hasJoinRTEs)
  {
    foreach (l, parse->rtable)
    {
      RangeTblEntry *rte = lfirst_node(RangeTblEntry, l);

      rte->joinaliasvars = NIL;
    }
  }

     
                                                                          
                                                                          
                                                                          
                                                                            
                                                                             
                                                                             
                                                                           
                                             
     
                                                                           
                                                                  
                                                                      
                                                                       
                                                                         
                                                                          
                       
     
                                                                       
                                                                           
                                                                  
                                                                             
                                                                             
                                                                        
                                                                           
                                  
     
                                                                 
                                                                             
                
     
  newHaving = NIL;
  foreach (l, (List *)parse->havingQual)
  {
    Node *havingclause = (Node *)lfirst(l);

    if ((parse->groupClause && parse->groupingSets) || contain_agg_clause(havingclause) || contain_volatile_functions(havingclause) || contain_subplans(havingclause))
    {
                             
      newHaving = lappend(newHaving, havingclause);
    }
    else if (parse->groupClause && !parse->groupingSets)
    {
                            
      parse->jointree->quals = (Node *)lappend((List *)parse->jointree->quals, havingclause);
    }
    else
    {
                                                  
      parse->jointree->quals = (Node *)lappend((List *)parse->jointree->quals, copyObject(havingclause));
      newHaving = lappend(newHaving, havingclause);
    }
  }
  parse->havingQual = (Node *)newHaving;

                                             
  remove_useless_groupby_columns(root);

     
                                                                          
                                                               
                    
     
  if (hasOuterJoins)
  {
    reduce_outer_joins(root);
  }

     
                                                                          
                                                                        
                                                        
     
  if (hasResultRTEs)
  {
    remove_useless_result_rtes(root);
  }

     
                                                                          
                                                                     
     
  if (parse->resultRelation && rt_fetch(parse->resultRelation, parse->rtable)->inh)
  {
    inheritance_planner(root);
  }
  else
  {
    grouping_planner(root, false, tuple_fraction);
  }

     
                                                                            
                                           
     
  SS_identify_outer_params(root);

     
                                                                             
                                                                      
                                                                 
                                                                
     
  final_rel = fetch_upper_rel(root, UPPERREL_FINAL, NULL);
  SS_charge_for_initplans(root, final_rel);

     
                                                                          
                                                                           
                                                                    
     
  set_cheapest(final_rel);

  return root;
}

   
                         
                                                                
                                                                 
                                                         
   
static Node *
preprocess_expression(PlannerInfo *root, Node *expr, int kind)
{
     
                                                                           
                                                                            
                                      
     
  if (expr == NULL)
  {
    return NULL;
  }

     
                                                                       
                                                                            
                                                                             
                                                                             
                                                                             
                                                                             
                                                             
     
  if (root->hasJoinRTEs && !(kind == EXPRKIND_RTFUNC || kind == EXPRKIND_VALUES || kind == EXPRKIND_TABLESAMPLE || kind == EXPRKIND_TABLEFUNC))
  {
    expr = flatten_join_alias_vars(root->parse, expr);
  }

     
                                    
     
                                                                             
                                                                          
                                                                             
                                                                           
                                                                            
                      
     
                                                                            
                                                                         
                                                                             
                                                            
     
  expr = eval_const_expressions(root, expr);

     
                                                    
     
  if (kind == EXPRKIND_QUAL)
  {
    expr = (Node *)canonicalize_qual((Expr *)expr, false);

#ifdef OPTIMIZER_DEBUG
    printf("After canonicalize_qual()\n");
    pprint(expr);
#endif
  }

                                   
  if (root->parse->hasSubLinks)
  {
    expr = SS_process_sublinks(root, expr, (kind == EXPRKIND_QUAL));
  }

     
                                                                             
                                     
     

                                                                          
  if (root->query_level > 1)
  {
    expr = SS_replace_correlation_vars(root, expr);
  }

     
                                                                          
                                                                           
                                                                  
                                                       
     
  if (kind == EXPRKIND_QUAL)
  {
    expr = (Node *)make_ands_implicit((Expr *)expr);
  }

  return expr;
}

   
                              
                                                                    
                                                             
   
static void
preprocess_qual_conditions(PlannerInfo *root, Node *jtnode)
{
  if (jtnode == NULL)
  {
    return;
  }
  if (IsA(jtnode, RangeTblRef))
  {
                            
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      preprocess_qual_conditions(root, lfirst(l));
    }

    f->quals = preprocess_expression(root, f->quals, EXPRKIND_QUAL);
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

    preprocess_qual_conditions(root, j->larg);
    preprocess_qual_conditions(root, j->rarg);

    j->quals = preprocess_expression(root, j->quals, EXPRKIND_QUAL);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                             
                                                                            
   
                                                                            
                                                                               
                                                                              
                                                                              
                                                                          
                                                                         
   
Expr *
preprocess_phv_expression(PlannerInfo *root, Expr *expr)
{
  return (Expr *)preprocess_expression(root, (Node *)expr, EXPRKIND_PHV);
}

   
                       
                                                                
                      
   
                                                                              
                                                                              
                                                                            
                                                                         
                                                                   
                                                                             
                                             
   
                                                                   
                                                        
   
                                                                               
                                
   
static void
inheritance_planner(PlannerInfo *root)
{
  Query *parse = root->parse;
  int top_parentRTindex = parse->resultRelation;
  List *select_rtable;
  List *select_appinfos;
  List *child_appinfos;
  List *old_child_rtis;
  List *new_child_rtis;
  Bitmapset *subqueryRTindexes;
  Index next_subquery_rti;
  int nominalRelation = -1;
  Index rootRelation = 0;
  List *final_rtable = NIL;
  List *final_rowmarks = NIL;
  int save_rel_array_size = 0;
  RelOptInfo **save_rel_array = NULL;
  AppendRelInfo **save_append_rel_array = NULL;
  List *subpaths = NIL;
  List *subroots = NIL;
  List *resultRelations = NIL;
  List *withCheckOptionLists = NIL;
  List *returningLists = NIL;
  List *rowMarks;
  RelOptInfo *final_rel;
  ListCell *lc;
  ListCell *lc2;
  Index rti;
  RangeTblEntry *parent_rte;
  Bitmapset *parent_relids;
  Query **parent_parses;

                                                 
  Assert(parse->commandType == CMD_UPDATE || parse->commandType == CMD_DELETE);

     
                                                                           
                                                                         
                                                                           
                                                                          
                                                                            
                                                                       
                                                                           
                                                                           
                                                                            
                                                                            
                                                                           
                                                                          
                                      
     
                                                                             
     
  subqueryRTindexes = NULL;
  rti = 1;
  foreach (lc, parse->rtable)
  {
    RangeTblEntry *rte = lfirst_node(RangeTblEntry, lc);

    if (rte->rtekind == RTE_SUBQUERY)
    {
      subqueryRTindexes = bms_add_member(subqueryRTindexes, rti);
    }
    rti++;
  }

     
                                                                         
                                                                            
                                                                            
                                                                            
                                                             
     
  parent_rte = rt_fetch(top_parentRTindex, parse->rtable);
  Assert(parent_rte->inh);
  if (parent_rte->relkind == RELKIND_PARTITIONED_TABLE)
  {
    nominalRelation = top_parentRTindex;
    rootRelation = top_parentRTindex;
  }

     
                                                                        
                                                                           
                                                                         
                                                                           
                                                                       
                                                                          
                                                                           
                                                                  
                                                                
     
  {
    PlannerInfo *subroot;

       
                                                                          
       
    subroot = makeNode(PlannerInfo);
    memcpy(subroot, root, sizeof(PlannerInfo));

       
                                                                         
                                                                           
                                                                          
                                                        
                                  
       
    subroot->parse = copyObject(root->parse);

    subroot->parse->commandType = CMD_SELECT;
    subroot->parse->resultRelation = 0;

       
                                                           
                                                                         
                                                                        
                        
       
    subroot->append_rel_list = copyObject(root->append_rel_list);

       
                                                        
       
    subroot->rowMarks = copyObject(root->rowMarks);

                                                             
    Assert(subroot->join_info_list == NIL);
                                                         
    Assert(subroot->placeholder_list == NIL);

                                                             
    grouping_planner(subroot, true, 0.0                          );

                                   
    select_rtable = subroot->parse->rtable;
    select_appinfos = subroot->append_rel_list;

       
                                                                   
                                                                
                                                          
       
    root->partColsUpdated = subroot->partColsUpdated;
  }

               
                                                                            
                                                                            
                                                                           
                                             
                                                                      
                                                                          
                                                                         
                                                                         
                                                                          
                                                                  
                                                                         
                     
     
                                                                           
                                                                        
                   
     
                                                                       
                                                                            
                                                                      
                                                                             
                                                                           
               
               
     
  child_appinfos = NIL;
  old_child_rtis = NIL;
  new_child_rtis = NIL;
  parent_relids = bms_make_singleton(top_parentRTindex);
  foreach (lc, select_appinfos)
  {
    AppendRelInfo *appinfo = lfirst_node(AppendRelInfo, lc);
    RangeTblEntry *child_rte;

                                                                 
    if (!bms_is_member(appinfo->parent_relid, parent_relids))
    {
      continue;
    }

                                                        
    child_appinfos = lappend(child_appinfos, appinfo);

                                        
    child_rte = rt_fetch(appinfo->child_relid, select_rtable);

                                              
    parse->rtable = lappend(parse->rtable, child_rte);

                                                     
    old_child_rtis = lappend_int(old_child_rtis, appinfo->child_relid);

                                               
    new_child_rtis = lappend_int(new_child_rtis, list_length(parse->rtable));

                                                              
    if (child_rte->inh)
    {
      Assert(child_rte->relkind == RELKIND_PARTITIONED_TABLE);
      parent_relids = bms_add_member(parent_relids, appinfo->child_relid);
    }
  }

     
                                                                            
                                                                         
                                                                             
                                                                           
                                                                           
     
  forboth(lc, old_child_rtis, lc2, new_child_rtis)
  {
    int old_child_rti = lfirst_int(lc);
    int new_child_rti = lfirst_int(lc2);

    if (old_child_rti == new_child_rti)
    {
      continue;                    
    }

    Assert(old_child_rti > new_child_rti);

    ChangeVarNodes((Node *)child_appinfos, old_child_rti, new_child_rti, 0);
  }

     
                                                                          
                                                                           
                                                                          
                                                 
     
  next_subquery_rti = list_length(parse->rtable) + 1;
  if (subqueryRTindexes != NULL)
  {
    int n_children = list_length(child_appinfos);

    while (n_children-- > 1)
    {
      int oldrti = -1;

      while ((oldrti = bms_next_member(subqueryRTindexes, oldrti)) >= 0)
      {
        RangeTblEntry *subqrte;

        subqrte = rt_fetch(oldrti, parse->rtable);
        parse->rtable = lappend(parse->rtable, copyObject(subqrte));
      }
    }
  }

     
                                                                           
                                                                         
                                                                      
                                                                             
                                                                             
                                                                           
                                                
     
  parent_parses = (Query **)palloc0((list_length(select_rtable) + 1) * sizeof(Query *));
  parent_parses[top_parentRTindex] = parse;

     
                                                                        
     
  foreach (lc, child_appinfos)
  {
    AppendRelInfo *appinfo = lfirst_node(AppendRelInfo, lc);
    Index this_subquery_rti = next_subquery_rti;
    Query *parent_parse;
    PlannerInfo *subroot;
    RangeTblEntry *child_rte;
    RelOptInfo *sub_final_rel;
    Path *subpath;

       
                                                                          
                                                                     
                                    
       
    parent_parse = parent_parses[appinfo->parent_relid];
    Assert(parent_parse != NULL);

       
                                                                        
                                                         
       
    subroot = makeNode(PlannerInfo);
    memcpy(subroot, root, sizeof(PlannerInfo));

       
                                                                        
                                                                  
                                                                       
                                            
       
    subroot->parse = (Query *)adjust_appendrel_attrs(subroot, (Node *)parent_parse, 1, &appinfo);

       
                                                                           
                                                                       
       
    parent_rte = rt_fetch(appinfo->parent_relid, subroot->parse->rtable);
    child_rte = rt_fetch(appinfo->child_relid, subroot->parse->rtable);
    child_rte->securityQuals = parent_rte->securityQuals;
    parent_rte->securityQuals = NIL;

       
                                                                        
                                                                          
                                  
       
    subroot->inhTargetKind = (rootRelation != 0) ? INHKIND_PARTITIONED : INHKIND_INHERITED;

       
                                                                      
                                                                          
                                                                         
                                                                       
                                                                         
                                                                        
       
                                                                    
                                                                     
                                                                    
                          
       
    if (child_rte->inh)
    {
      Assert(child_rte->relkind == RELKIND_PARTITIONED_TABLE);
      parent_parses[appinfo->child_relid] = subroot->parse;
      continue;
    }

       
                                                                      
                                                                           
                                                                   
                                                                    
                                                                        
                                                                         
                                                                     
                                      
       
                                                                     
                                                                         
                                                                       
                                                                           
                                                                       
                                                                       
                                                                       
                                                                       
                                                 
       
    if (nominalRelation < 0)
    {
      nominalRelation = appinfo->child_relid;
    }

       
                                                                       
                                                                  
                                                                         
                                                                         
                           
       
    subroot->append_rel_list = copyObject(root->append_rel_list);
    subroot->rowMarks = copyObject(root->rowMarks);

       
                                                                     
                                                                  
       
    if (final_rtable != NIL && subqueryRTindexes != NULL)
    {
      int oldrti = -1;

      while ((oldrti = bms_next_member(subqueryRTindexes, oldrti)) >= 0)
      {
        Index newrti = next_subquery_rti++;

        ChangeVarNodes((Node *)subroot->parse, oldrti, newrti, 0);
        ChangeVarNodes((Node *)subroot->append_rel_list, oldrti, newrti, 0);
        ChangeVarNodes((Node *)subroot->rowMarks, oldrti, newrti, 0);
      }
    }

                                                             
    Assert(subroot->join_info_list == NIL);
                                                         
    Assert(subroot->placeholder_list == NIL);

                                             
    if (root->multiexpr_params)
    {
      SS_make_multiexprs_unique(root, subroot);
    }

                                                             
    grouping_planner(subroot, true, 0.0                          );

       
                                                                          
                                                                   
                            
       
    sub_final_rel = fetch_upper_rel(subroot, UPPERREL_FINAL, NULL);
    set_cheapest(sub_final_rel);
    subpath = sub_final_rel->cheapest_total_path;

       
                                                                          
                             
       
    if (IS_DUMMY_REL(sub_final_rel))
    {
      continue;
    }

       
                                                                         
                                                                         
                                                                        
                                                                          
                                                                     
                                    
       
    if (final_rtable == NIL)
    {
      final_rtable = subroot->parse->rtable;
      final_rowmarks = subroot->rowMarks;
    }
    else
    {
      Assert(list_length(final_rtable) == list_length(subroot->parse->rtable));
      if (subqueryRTindexes != NULL)
      {
        int oldrti = -1;

        while ((oldrti = bms_next_member(subqueryRTindexes, oldrti)) >= 0)
        {
          Index newrti = this_subquery_rti++;
          RangeTblEntry *subqrte;
          ListCell *newrticell;

          subqrte = rt_fetch(newrti, subroot->parse->rtable);
          newrticell = list_nth_cell(final_rtable, newrti - 1);
          lfirst(newrticell) = subqrte;
        }
      }
    }

       
                                                                        
                                                                         
                                                                          
                                                                 
       
    Assert(subroot->simple_rel_array_size >= save_rel_array_size);
    for (rti = 1; rti < save_rel_array_size; rti++)
    {
      RelOptInfo *brel = save_rel_array[rti];

      if (brel)
      {
        subroot->simple_rel_array[rti] = brel;
      }
    }
    save_rel_array_size = subroot->simple_rel_array_size;
    save_rel_array = subroot->simple_rel_array;
    save_append_rel_array = subroot->append_rel_array;

       
                                                                           
                                                                 
                   
       
    root->init_plans = subroot->init_plans;

                                 
    subpaths = lappend(subpaths, subpath);

                                              
    subroots = lappend(subroots, subroot);

                                                  
    resultRelations = lappend_int(resultRelations, appinfo->child_relid);

                                                                   
    if (parse->withCheckOptions)
    {
      withCheckOptionLists = lappend(withCheckOptionLists, subroot->parse->withCheckOptions);
    }
    if (parse->returningList)
    {
      returningLists = lappend(returningLists, subroot->parse->returningList);
    }

    Assert(!parse->onConflict);
  }

                                                             
  final_rel = fetch_upper_rel(root, UPPERREL_FINAL, NULL);

     
                                                                          
                                                                           
                          
     

  if (subpaths == NIL)
  {
       
                                                                       
                                                                          
                                                                       
                                                                         
                                                                          
                                                 
       
    Path *dummy_path;

                                                 
    root->processed_tlist = preprocess_targetlist(root);
    final_rel->reltarget = create_pathtarget(root, root->processed_tlist);

                                                        
    dummy_path = (Path *)create_append_path(NULL, final_rel, NIL, NIL, NIL, NULL, 0, false, NIL, -1);

                                                                       
    subpaths = list_make1(dummy_path);
    subroots = list_make1(root);
    resultRelations = list_make1_int(parse->resultRelation);
    if (parse->withCheckOptions)
    {
      withCheckOptionLists = list_make1(parse->withCheckOptions);
    }
    if (parse->returningList)
    {
      returningLists = list_make1(parse->returningList);
    }
                                                     
    root->partColsUpdated = false;
  }
  else
  {
       
                                                                      
                                                                         
                                                        
       
    parse->rtable = final_rtable;
    root->simple_rel_array_size = save_rel_array_size;
    root->simple_rel_array = save_rel_array;
    root->append_rel_array = save_append_rel_array;

                                                         
    root->simple_rte_array = (RangeTblEntry **)palloc0((list_length(final_rtable) + 1) * sizeof(RangeTblEntry *));
    rti = 1;
    foreach (lc, final_rtable)
    {
      RangeTblEntry *rte = lfirst_node(RangeTblEntry, lc);

      root->simple_rte_array[rti++] = rte;
    }

                                         
    root->rowMarks = final_rowmarks;
  }

     
                                                                          
                                                                           
                          
     
  if (parse->rowMarks)
  {
    rowMarks = NIL;
  }
  else
  {
    rowMarks = root->rowMarks;
  }

                                                                           
  add_path(final_rel, (Path *)create_modifytable_path(root, final_rel, parse->commandType, parse->canSetTag, nominalRelation, rootRelation, root->partColsUpdated, resultRelations, subpaths, subroots, withCheckOptionLists, returningLists, rowMarks, NULL, assign_special_exec_param(root)));
}

                       
                    
                                                                   
   
                                                                         
                                      
   
                                                                              
                                                                       
                                                                               
                   
   
                                                                         
                                             
                                                        
                                                                           
                                  
                                                                          
                                                         
   
                                                                       
                                                           
                                                                  
   
                                                                               
                                
                       
   
static void
grouping_planner(PlannerInfo *root, bool inheritance_update, double tuple_fraction)
{
  Query *parse = root->parse;
  int64 offset_est = 0;
  int64 count_est = 0;
  double limit_tuples = -1.0;
  bool have_postponed_srfs = false;
  PathTarget *final_target;
  List *final_targets;
  List *final_targets_contain_srfs;
  bool final_target_parallel_safe;
  RelOptInfo *current_rel;
  RelOptInfo *final_rel;
  FinalPathExtraData extra;
  ListCell *lc;

                                                                 
  if (parse->limitCount || parse->limitOffset)
  {
    tuple_fraction = preprocess_limit(root, tuple_fraction, &offset_est, &count_est);

       
                                                                          
                                                     
       
    if (count_est > 0 && offset_est >= 0)
    {
      limit_tuples = (double)count_est + (double)offset_est;
    }
  }

                                                              
  root->tuple_fraction = tuple_fraction;

  if (parse->setOperations)
  {
       
                                                                        
                                                                         
                                                                           
                                                                      
                                                                
                             
       
    if (parse->sortClause)
    {
      root->tuple_fraction = 0.0;
    }

       
                                                                          
                                                                         
                                                                  
                            
       
    current_rel = plan_set_operations(root);

       
                                                                          
                                                                          
                                                                        
                                                                         
                       
       
    Assert(parse->commandType == CMD_SELECT);

                                                                        
    root->processed_tlist = postprocess_setop_tlist(copyObject(root->processed_tlist), parse->targetList);

                                                                    
    final_target = current_rel->cheapest_total_path->pathtarget;

                                              
    final_target_parallel_safe = is_parallel_safe(root, (Node *)final_target->exprs);

                                                          
    Assert(!parse->hasTargetSRFs);
    final_targets = final_targets_contain_srfs = NIL;

       
                                                                    
                                              
       
    if (parse->rowMarks)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                  
                                                                                           
                         errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT", LCS_asString(linitial_node(RowMarkClause, parse->rowMarks)->strength))));
    }

       
                                                                      
       
    Assert(parse->distinctClause == NIL);
    root->sort_pathkeys = make_pathkeys_for_sortclauses(root, parse->sortClause, root->processed_tlist);
  }
  else
  {
                                                
    PathTarget *sort_input_target;
    List *sort_input_targets;
    List *sort_input_targets_contain_srfs;
    bool sort_input_target_parallel_safe;
    PathTarget *grouping_target;
    List *grouping_targets;
    List *grouping_targets_contain_srfs;
    bool grouping_target_parallel_safe;
    PathTarget *scanjoin_target;
    List *scanjoin_targets;
    List *scanjoin_targets_contain_srfs;
    bool scanjoin_target_parallel_safe;
    bool scanjoin_target_same_exprs;
    bool have_grouping;
    AggClauseCosts agg_costs;
    WindowFuncLists *wflists = NULL;
    List *activeWindows = NIL;
    grouping_sets_data *gset_data = NULL;
    standard_qp_extra qp_extra;

                                                            
    Assert(!root->hasRecursion);

                                                              
    if (parse->groupingSets)
    {
      gset_data = preprocess_grouping_sets(root);
    }
    else
    {
                                                      
      if (parse->groupClause)
      {
        parse->groupClause = preprocess_groupclause(root, NIL);
      }
    }

       
                                                                        
                                                                           
                                                                           
                                                                         
                                                                     
       
    root->processed_tlist = preprocess_targetlist(root);

       
                                                                          
                                                                        
                                                                          
                                                                          
                           
       
                                                                          
                                                                        
                                                                        
                                                                       
                                     
       
    MemSet(&agg_costs, 0, sizeof(AggClauseCosts));
    if (parse->hasAggs)
    {
      get_agg_clause_costs(root, (Node *)root->processed_tlist, AGGSPLIT_SIMPLE, &agg_costs);
      get_agg_clause_costs(root, parse->havingQual, AGGSPLIT_SIMPLE, &agg_costs);
    }

       
                                                                         
                                                                          
                                                                        
                                                                 
       
    if (parse->hasWindowFuncs)
    {
      wflists = find_window_functions((Node *)root->processed_tlist, list_length(parse->windowClause));
      if (wflists->numWindowFuncs > 0)
      {
        activeWindows = select_active_windows(root, wflists);
      }
      else
      {
        parse->hasWindowFuncs = false;
      }
    }

       
                                                                      
                                                                         
                                                                   
                                
       
    if (parse->hasAggs)
    {
      preprocess_minmax_aggregates(root);
    }

       
                                                                          
                                                                          
                                                                 
                                                              
       
    if (parse->groupClause || parse->groupingSets || parse->distinctClause || parse->hasAggs || parse->hasWindowFuncs || parse->hasTargetSRFs || root->hasHavingQual)
    {
      root->limit_tuples = -1.0;
    }
    else
    {
      root->limit_tuples = limit_tuples;
    }

                                                    
    qp_extra.activeWindows = activeWindows;
    qp_extra.groupClause = (gset_data ? (gset_data->rollups ? linitial_node(RollupData, gset_data->rollups)->groupClause : NIL) : parse->groupClause);

       
                                                                        
                                                                   
                                                                         
                                                                          
                                                         
       
    current_rel = query_planner(root, standard_qp_callback, &qp_extra);

       
                                                                
       
                                                                      
                                                                      
                                                                       
                                                                        
                                                  
       
    final_target = create_pathtarget(root, root->processed_tlist);
    final_target_parallel_safe = is_parallel_safe(root, (Node *)final_target->exprs);

       
                                                                         
                                                                          
           
       
    if (parse->sortClause)
    {
      sort_input_target = make_sort_input_target(root, final_target, &have_postponed_srfs);
      sort_input_target_parallel_safe = is_parallel_safe(root, (Node *)sort_input_target->exprs);
    }
    else
    {
      sort_input_target = final_target;
      sort_input_target_parallel_safe = final_target_parallel_safe;
    }

       
                                                                     
                                                                 
                                                  
       
    if (activeWindows)
    {
      grouping_target = make_window_input_target(root, final_target, activeWindows);
      grouping_target_parallel_safe = is_parallel_safe(root, (Node *)grouping_target->exprs);
    }
    else
    {
      grouping_target = sort_input_target;
      grouping_target_parallel_safe = sort_input_target_parallel_safe;
    }

       
                                                                       
                                                                       
                                    
       
    have_grouping = (parse->groupClause || parse->groupingSets || parse->hasAggs || root->hasHavingQual);
    if (have_grouping)
    {
      scanjoin_target = make_group_input_target(root, final_target);
      scanjoin_target_parallel_safe = is_parallel_safe(root, (Node *)scanjoin_target->exprs);
    }
    else
    {
      scanjoin_target = grouping_target;
      scanjoin_target_parallel_safe = grouping_target_parallel_safe;
    }

       
                                                                         
                                                                           
                                                                           
                                                                      
       
    if (parse->hasTargetSRFs)
    {
                                                                        
      split_pathtarget_at_srfs(root, final_target, sort_input_target, &final_targets, &final_targets_contain_srfs);
      final_target = linitial_node(PathTarget, final_targets);
      Assert(!linitial_int(final_targets_contain_srfs));
                                                              
      split_pathtarget_at_srfs(root, sort_input_target, grouping_target, &sort_input_targets, &sort_input_targets_contain_srfs);
      sort_input_target = linitial_node(PathTarget, sort_input_targets);
      Assert(!linitial_int(sort_input_targets_contain_srfs));
                                                            
      split_pathtarget_at_srfs(root, grouping_target, scanjoin_target, &grouping_targets, &grouping_targets_contain_srfs);
      grouping_target = linitial_node(PathTarget, grouping_targets);
      Assert(!linitial_int(grouping_targets_contain_srfs));
                                                                     
      split_pathtarget_at_srfs(root, scanjoin_target, NULL, &scanjoin_targets, &scanjoin_targets_contain_srfs);
      scanjoin_target = linitial_node(PathTarget, scanjoin_targets);
      Assert(!linitial_int(scanjoin_targets_contain_srfs));
    }
    else
    {
                                                                    
      final_targets = final_targets_contain_srfs = NIL;
      sort_input_targets = sort_input_targets_contain_srfs = NIL;
      grouping_targets = grouping_targets_contain_srfs = NIL;
      scanjoin_targets = list_make1(scanjoin_target);
      scanjoin_targets_contain_srfs = NIL;
    }

                                 
    scanjoin_target_same_exprs = list_length(scanjoin_targets) == 1 && equal(scanjoin_target->exprs, current_rel->reltarget->exprs);
    apply_scanjoin_target_to_paths(root, current_rel, scanjoin_targets, scanjoin_targets_contain_srfs, scanjoin_target_parallel_safe, scanjoin_target_same_exprs);

       
                                                                    
                                                                      
                                                                           
                                                                           
                                                                          
       
    root->upper_targets[UPPERREL_FINAL] = final_target;
    root->upper_targets[UPPERREL_ORDERED] = final_target;
    root->upper_targets[UPPERREL_DISTINCT] = sort_input_target;
    root->upper_targets[UPPERREL_WINDOW] = sort_input_target;
    root->upper_targets[UPPERREL_GROUP_AGG] = grouping_target;

       
                                                                          
                                                                      
              
       
    if (have_grouping)
    {
      current_rel = create_grouping_paths(root, current_rel, grouping_target, grouping_target_parallel_safe, &agg_costs, gset_data);
                                                          
      if (parse->hasTargetSRFs)
      {
        adjust_paths_for_srfs(root, current_rel, grouping_targets, grouping_targets_contain_srfs);
      }
    }

       
                                                                          
                                                                   
       
    if (activeWindows)
    {
      current_rel = create_window_paths(root, current_rel, grouping_target, sort_input_target, sort_input_target_parallel_safe, wflists, activeWindows);
                                                            
      if (parse->hasTargetSRFs)
      {
        adjust_paths_for_srfs(root, current_rel, sort_input_targets, sort_input_targets_contain_srfs);
      }
    }

       
                                                                          
                                                                   
       
    if (parse->distinctClause)
    {
      current_rel = create_distinct_paths(root, current_rel);
    }
  }                                

     
                                                                            
                                                                           
                                                                  
                                                                       
                     
     
  if (parse->sortClause)
  {
    current_rel = create_ordered_paths(root, current_rel, final_target, final_target_parallel_safe, have_postponed_srfs ? -1.0 : limit_tuples);
                                                     
    if (parse->hasTargetSRFs)
    {
      adjust_paths_for_srfs(root, current_rel, final_targets, final_targets_contain_srfs);
    }
  }

     
                                                             
     
  final_rel = fetch_upper_rel(root, UPPERREL_FINAL, NULL);

     
                                                                             
                                                                             
                                                                           
                                                                             
            
     
  if (current_rel->consider_parallel && is_parallel_safe(root, parse->limitOffset) && is_parallel_safe(root, parse->limitCount))
  {
    final_rel->consider_parallel = true;
  }

     
                                                                        
     
  final_rel->serverid = current_rel->serverid;
  final_rel->userid = current_rel->userid;
  final_rel->useridiscurrent = current_rel->useridiscurrent;
  final_rel->fdwroutine = current_rel->fdwroutine;

     
                                                                         
                                                                
     
  foreach (lc, current_rel->pathlist)
  {
    Path *path = (Path *)lfirst(lc);

       
                                                                           
                                                                       
                                                                     
                                                                         
                                             
       
    if (parse->rowMarks)
    {
      path = (Path *)create_lockrows_path(root, final_rel, path, root->rowMarks, assign_special_exec_param(root));
    }

       
                                                              
       
    if (limit_needed(parse))
    {
      path = (Path *)create_limit_path(root, final_rel, path, parse->limitOffset, parse->limitCount, offset_est, count_est);
    }

       
                                                                           
                                                      
       
    if (parse->commandType != CMD_SELECT && !inheritance_update)
    {
      Index rootRelation;
      List *withCheckOptionLists;
      List *returningLists;
      List *rowMarks;

         
                                                                  
                                                  
         
      if (rt_fetch(parse->resultRelation, parse->rtable)->relkind == RELKIND_PARTITIONED_TABLE)
      {
        rootRelation = parse->resultRelation;
      }
      else
      {
        rootRelation = 0;
      }

         
                                                                       
                 
         
      if (parse->withCheckOptions)
      {
        withCheckOptionLists = list_make1(parse->withCheckOptions);
      }
      else
      {
        withCheckOptionLists = NIL;
      }

      if (parse->returningList)
      {
        returningLists = list_make1(parse->returningList);
      }
      else
      {
        returningLists = NIL;
      }

         
                                                                         
                                                                       
                                           
         
      if (parse->rowMarks)
      {
        rowMarks = NIL;
      }
      else
      {
        rowMarks = root->rowMarks;
      }

      path = (Path *)create_modifytable_path(root, final_rel, parse->commandType, parse->canSetTag, parse->resultRelation, rootRelation, false, list_make1_int(parse->resultRelation), list_make1(path), list_make1(root), withCheckOptionLists, returningLists, rowMarks, parse->onConflict, assign_special_exec_param(root));
    }

                                     
    add_path(final_rel, path);
  }

     
                                                                            
                                  
     
  if (final_rel->consider_parallel && root->query_level > 1 && !limit_needed(parse))
  {
    Assert(!parse->rowMarks && parse->commandType == CMD_SELECT);
    foreach (lc, current_rel->partial_pathlist)
    {
      Path *partial_path = (Path *)lfirst(lc);

      add_partial_path(final_rel, partial_path);
    }
  }

  extra.limit_needed = limit_needed(parse);
  extra.limit_tuples = limit_tuples;
  extra.count_est = count_est;
  extra.offset_est = offset_est;

     
                                                                          
                                          
     
  if (final_rel->fdwroutine && final_rel->fdwroutine->GetForeignUpperPaths)
  {
    final_rel->fdwroutine->GetForeignUpperPaths(root, UPPERREL_FINAL, current_rel, final_rel, &extra);
  }

                                                   
  if (create_upper_paths_hook)
  {
    (*create_upper_paths_hook)(root, UPPERREL_FINAL, current_rel, final_rel, &extra);
  }

                                                                    
}

   
                                                                                
                                                                                
                                                                            
                   
   
static grouping_sets_data *
preprocess_grouping_sets(PlannerInfo *root)
{
  Query *parse = root->parse;
  List *sets;
  int maxref = 0;
  ListCell *lc;
  ListCell *lc_set;
  grouping_sets_data *gd = palloc0(sizeof(grouping_sets_data));

  parse->groupingSets = expand_grouping_sets(parse->groupingSets, -1);

  gd->any_hashable = false;
  gd->unhashable_refs = NULL;
  gd->unsortable_refs = NULL;
  gd->unsortable_sets = NIL;

  if (parse->groupClause)
  {
    ListCell *lc;

    foreach (lc, parse->groupClause)
    {
      SortGroupClause *gc = lfirst_node(SortGroupClause, lc);
      Index ref = gc->tleSortGroupRef;

      if (ref > maxref)
      {
        maxref = ref;
      }

      if (!gc->hashable)
      {
        gd->unhashable_refs = bms_add_member(gd->unhashable_refs, ref);
      }

      if (!OidIsValid(gc->sortop))
      {
        gd->unsortable_refs = bms_add_member(gd->unsortable_refs, ref);
      }
    }
  }

                                              
  gd->tleref_to_colnum_map = (int *)palloc((maxref + 1) * sizeof(int));

     
                                                                           
                                                       
                                                                            
           
     
  if (!bms_is_empty(gd->unsortable_refs))
  {
    List *sortable_sets = NIL;

    foreach (lc, parse->groupingSets)
    {
      List *gset = (List *)lfirst(lc);

      if (bms_overlap_list(gd->unsortable_refs, gset))
      {
        GroupingSetData *gs = makeNode(GroupingSetData);

        gs->set = gset;
        gd->unsortable_sets = lappend(gd->unsortable_sets, gs);

           
                                                                    
                                                                     
                                                                   
           
                                                                
                                                               
           
        if (bms_overlap_list(gd->unhashable_refs, gset))
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not implement GROUP BY"), errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
        }
      }
      else
      {
        sortable_sets = lappend(sortable_sets, gset);
      }
    }

    if (sortable_sets)
    {
      sets = extract_rollup_sets(sortable_sets);
    }
    else
    {
      sets = NIL;
    }
  }
  else
  {
    sets = extract_rollup_sets(parse->groupingSets);
  }

  foreach (lc_set, sets)
  {
    List *current_sets = (List *)lfirst(lc_set);
    RollupData *rollup = makeNode(RollupData);
    GroupingSetData *gs;

       
                                                                     
                                                                       
                                                                           
                               
       
                                                                      
                                                                          
                                                
       
    current_sets = reorder_grouping_sets(current_sets, (list_length(sets) == 1 ? parse->sortClause : NIL));

       
                                                             
       
    gs = linitial_node(GroupingSetData, current_sets);

       
                                                                          
                                                                         
                                                                    
       
                                                                           
                                                                        
                                                                      
       
    if (gs->set)
    {
      rollup->groupClause = preprocess_groupclause(root, gs->set);
    }
    else
    {
      rollup->groupClause = NIL;
    }

       
                                                                         
                                                                       
                                                                        
                  
       
    if (gs->set && !bms_overlap_list(gd->unhashable_refs, gs->set))
    {
      rollup->hashable = true;
      gd->any_hashable = true;
    }

       
                                                                        
                                                                           
                                                                   
                                                                     
                                            
       
    rollup->gsets = remap_to_groupclause_idx(rollup->groupClause, current_sets, gd->tleref_to_colnum_map);
    rollup->gsets_data = current_sets;

    gd->rollups = lappend(gd->rollups, rollup);
  }

  if (gd->unsortable_sets)
  {
       
                                                                       
                                                                 
                                                                       
       
    gd->hash_sets_idx = remap_to_groupclause_idx(parse->groupClause, gd->unsortable_sets, gd->tleref_to_colnum_map);
    gd->any_hashable = true;
  }

  return gd;
}

   
                                                                             
                                                                      
   
static List *
remap_to_groupclause_idx(List *groupClause, List *gsets, int *tleref_to_colnum_map)
{
  int ref = 0;
  List *result = NIL;
  ListCell *lc;

  foreach (lc, groupClause)
  {
    SortGroupClause *gc = lfirst_node(SortGroupClause, lc);

    tleref_to_colnum_map[gc->tleSortGroupRef] = ref++;
  }

  foreach (lc, gsets)
  {
    List *set = NIL;
    ListCell *lc2;
    GroupingSetData *gs = lfirst_node(GroupingSetData, lc);

    foreach (lc2, gs->set)
    {
      set = lappend_int(set, tleref_to_colnum_map[lfirst_int(lc2)]);
    }

    result = lappend(result, set);
  }

  return result;
}

   
                                                       
   
static void
preprocess_rowmarks(PlannerInfo *root)
{
  Query *parse = root->parse;
  Bitmapset *rels;
  List *prowmarks;
  ListCell *l;
  int i;

  if (parse->rowMarks)
  {
       
                                                                  
                                                                        
                                                                      
                                                                     
       
    CheckSelectLocking(parse, linitial_node(RowMarkClause, parse->rowMarks)->strength);
  }
  else
  {
       
                                                              
                     
       
    if (parse->commandType != CMD_UPDATE && parse->commandType != CMD_DELETE)
    {
      return;
    }
  }

     
                                                                           
                                                                          
                                                    
     
  rels = get_relids_in_jointree((Node *)parse->jointree, false);
  if (parse->resultRelation)
  {
    rels = bms_del_member(rels, parse->resultRelation);
  }

     
                                                           
     
  prowmarks = NIL;
  foreach (l, parse->rowMarks)
  {
    RowMarkClause *rc = lfirst_node(RowMarkClause, l);
    RangeTblEntry *rte = rt_fetch(rc->rti, parse->rtable);
    PlanRowMark *newrc;

       
                                                                          
                                                                     
                                                                      
       
    Assert(rc->rti != parse->resultRelation);

       
                                                                         
                                                                           
                                                                          
                                             
       
    if (rte->rtekind != RTE_RELATION)
    {
      continue;
    }

    rels = bms_del_member(rels, rc->rti);

    newrc = makeNode(PlanRowMark);
    newrc->rti = newrc->prti = rc->rti;
    newrc->rowmarkId = ++(root->glob->lastRowMarkId);
    newrc->markType = select_rowmark_type(rte, rc->strength);
    newrc->allMarkTypes = (1 << newrc->markType);
    newrc->strength = rc->strength;
    newrc->waitPolicy = rc->waitPolicy;
    newrc->isParent = false;

    prowmarks = lappend(prowmarks, newrc);
  }

     
                                                                      
     
  i = 0;
  foreach (l, parse->rtable)
  {
    RangeTblEntry *rte = lfirst_node(RangeTblEntry, l);
    PlanRowMark *newrc;

    i++;
    if (!bms_is_member(i, rels))
    {
      continue;
    }

    newrc = makeNode(PlanRowMark);
    newrc->rti = newrc->prti = i;
    newrc->rowmarkId = ++(root->glob->lastRowMarkId);
    newrc->markType = select_rowmark_type(rte, LCS_NONE);
    newrc->allMarkTypes = (1 << newrc->markType);
    newrc->strength = LCS_NONE;
    newrc->waitPolicy = LockWaitBlock;                     
    newrc->isParent = false;

    prowmarks = lappend(prowmarks, newrc);
  }

  root->rowMarks = prowmarks;
}

   
                                               
   
RowMarkType
select_rowmark_type(RangeTblEntry *rte, LockClauseStrength strength)
{
  if (rte->rtekind != RTE_RELATION)
  {
                                                       
    return ROW_MARK_COPY;
  }
  else if (rte->relkind == RELKIND_FOREIGN_TABLE)
  {
                                                             
    FdwRoutine *fdwroutine = GetFdwRoutineByRelId(rte->relid);

    if (fdwroutine->GetForeignRowMarkType != NULL)
    {
      return fdwroutine->GetForeignRowMarkType(rte, strength);
    }
                                                 
    return ROW_MARK_COPY;
  }
  else
  {
                                                        
    switch (strength)
    {
    case LCS_NONE:

         
                                                                  
                  
         
      return ROW_MARK_REFERENCE;
      break;
    case LCS_FORKEYSHARE:
      return ROW_MARK_KEYSHARE;
      break;
    case LCS_FORSHARE:
      return ROW_MARK_SHARE;
      break;
    case LCS_FORNOKEYUPDATE:
      return ROW_MARK_NOKEYEXCLUSIVE;
      break;
    case LCS_FORUPDATE:
      return ROW_MARK_EXCLUSIVE;
      break;
    }
    elog(ERROR, "unrecognized LockClauseStrength %d", (int)strength);
    return ROW_MARK_EXCLUSIVE;                          
  }
}

   
                                                                        
   
                                                                           
                                                                           
                                                                        
                                                                         
                                                                        
                                                                              
                                                                             
                                                                      
   
                                                                       
                                                                              
                                                                             
                  
   
static double
preprocess_limit(PlannerInfo *root, double tuple_fraction, int64 *offset_est, int64 *count_est)
{
  Query *parse = root->parse;
  Node *est;
  double limit_fraction;

                                                   
  Assert(parse->limitCount || parse->limitOffset);

     
                                                                        
                                                                         
     
  if (parse->limitCount)
  {
    est = estimate_expression_value(root, parse->limitCount);
    if (est && IsA(est, Const))
    {
      if (((Const *)est)->constisnull)
      {
                                                    
        *count_est = 0;                           
      }
      else
      {
        *count_est = DatumGetInt64(((Const *)est)->constvalue);
        if (*count_est <= 0)
        {
          *count_est = 1;                          
        }
      }
    }
    else
    {
      *count_est = -1;                     
    }
  }
  else
  {
    *count_est = 0;                  
  }

  if (parse->limitOffset)
  {
    est = estimate_expression_value(root, parse->limitOffset);
    if (est && IsA(est, Const))
    {
      if (((Const *)est)->constisnull)
      {
                                                            
        *offset_est = 0;                           
      }
      else
      {
        *offset_est = DatumGetInt64(((Const *)est)->constvalue);
        if (*offset_est < 0)
        {
          *offset_est = 0;                           
        }
      }
    }
    else
    {
      *offset_est = -1;                     
    }
  }
  else
  {
    *offset_est = 0;                  
  }

  if (*count_est != 0)
  {
       
                                                                     
                                                                        
                                                                         
       
    if (*count_est < 0 || *offset_est < 0)
    {
                                                         
      limit_fraction = 0.10;
    }
    else
    {
                                                                      
      limit_fraction = (double)*count_est + (double)*offset_est;
    }

       
                                                                      
                                                                       
                                                                          
                                                                           
                   
       
    if (tuple_fraction >= 1.0)
    {
      if (limit_fraction >= 1.0)
      {
                           
        tuple_fraction = Min(tuple_fraction, limit_fraction);
      }
      else
      {
                                                                   
      }
    }
    else if (tuple_fraction > 0.0)
    {
      if (limit_fraction >= 1.0)
      {
                                                          
        tuple_fraction = limit_fraction;
      }
      else
      {
                             
        tuple_fraction = Min(tuple_fraction, limit_fraction);
      }
    }
    else
    {
                                               
      tuple_fraction = limit_fraction;
    }
  }
  else if (*offset_est != 0 && tuple_fraction > 0.0)
  {
       
                                                                       
                                                                           
                                                                          
                                                                           
                                      
       
                                                                 
       
    if (*offset_est < 0)
    {
      limit_fraction = 0.10;
    }
    else
    {
      limit_fraction = (double)*offset_est;
    }

       
                                                                        
                                                                  
                                                                          
                                                          
       
    if (tuple_fraction >= 1.0)
    {
      if (limit_fraction >= 1.0)
      {
                                                 
        tuple_fraction += limit_fraction;
      }
      else
      {
                                                          
        tuple_fraction = limit_fraction;
      }
    }
    else
    {
      if (limit_fraction >= 1.0)
      {
                                                                   
      }
      else
      {
                                                   
        tuple_fraction += limit_fraction;
        if (tuple_fraction >= 1.0)
        {
          tuple_fraction = 0.0;                       
        }
      }
    }
  }

  return tuple_fraction;
}

   
                                                         
   
                                                                               
                                                                            
                                                                             
                                                                             
                                                                         
              
   
                                                                              
                                                                           
                                                                      
   
bool
limit_needed(Query *parse)
{
  Node *node;

  node = parse->limitCount;
  if (node)
  {
    if (IsA(node, Const))
    {
                                                  
      if (!((Const *)node)->constisnull)
      {
        return true;                                  
      }
    }
    else
    {
      return true;                         
    }
  }

  node = parse->limitOffset;
  if (node)
  {
    if (IsA(node, Const))
    {
                                                           
      if (!((Const *)node)->constisnull)
      {
        int64 offset = DatumGetInt64(((Const *)node)->constvalue);

        if (offset != 0)
        {
          return true;                                  
        }
      }
    }
    else
    {
      return true;                          
    }
  }

  return false;                                   
}

   
                                  
                                                                        
                                                            
   
                                                                              
                                                                              
                                                                          
                                                                            
                                                                       
                                                                        
   
                                                                           
                                                                               
                                                                              
                                         
   
static void
remove_useless_groupby_columns(PlannerInfo *root)
{
  Query *parse = root->parse;
  Bitmapset **groupbyattnos;
  Bitmapset **surplusvars;
  ListCell *lc;
  int relid;

                                                                          
  if (list_length(parse->groupClause) < 2)
  {
    return;
  }

                                                                            
  if (parse->groupingSets)
  {
    return;
  }

     
                                                                           
                                                                          
                              
     
  groupbyattnos = (Bitmapset **)palloc0(sizeof(Bitmapset *) * (list_length(parse->rtable) + 1));
  foreach (lc, parse->groupClause)
  {
    SortGroupClause *sgc = lfirst_node(SortGroupClause, lc);
    TargetEntry *tle = get_sortgroupclause_tle(sgc, parse->targetList);
    Var *var = (Var *)tle->expr;

       
                                                         
       
                                                                          
                                                                          
                                                                           
                                
       
    if (!IsA(var, Var) || var->varlevelsup > 0)
    {
      continue;
    }

                                       
    relid = var->varno;
    Assert(relid <= list_length(parse->rtable));
    groupbyattnos[relid] = bms_add_member(groupbyattnos[relid], var->varattno - FirstLowInvalidHeapAttributeNumber);
  }

     
                                                                            
                                                                             
                                                                             
                                                                      
     
  surplusvars = NULL;                                           
  relid = 0;
  foreach (lc, parse->rtable)
  {
    RangeTblEntry *rte = lfirst_node(RangeTblEntry, lc);
    Bitmapset *relattnos;
    Bitmapset *pkattnos;
    Oid constraintOid;

    relid++;

                                                                 
    if (rte->rtekind != RTE_RELATION)
    {
      continue;
    }

       
                                                                        
                                                                      
                        
       
    if (rte->inh && rte->relkind != RELKIND_PARTITIONED_TABLE)
    {
      continue;
    }

                                                                     
    relattnos = groupbyattnos[relid];
    if (bms_membership(relattnos) != BMS_MULTIPLE)
    {
      continue;
    }

       
                                                                     
                                                     
       
    pkattnos = get_primary_key_attnos(rte->relid, false, &constraintOid);
    if (pkattnos == NULL)
    {
      continue;
    }

       
                                                                       
                                                       
       
    if (bms_subset_compare(pkattnos, relattnos) == BMS_SUBSET1)
    {
         
                                                                         
                                                                   
         
      if (surplusvars == NULL)
      {
        surplusvars = (Bitmapset **)palloc0(sizeof(Bitmapset *) * (list_length(parse->rtable) + 1));
      }

                                                        
      surplusvars[relid] = bms_difference(relattnos, pkattnos);

                                                                         
      parse->constraintDeps = lappend_oid(parse->constraintDeps, constraintOid);
    }
  }

     
                                                                             
                                                                       
                                     
     
  if (surplusvars != NULL)
  {
    List *new_groupby = NIL;

    foreach (lc, parse->groupClause)
    {
      SortGroupClause *sgc = lfirst_node(SortGroupClause, lc);
      TargetEntry *tle = get_sortgroupclause_tle(sgc, parse->targetList);
      Var *var = (Var *)tle->expr;

         
                                                                      
                            
         
      if (!IsA(var, Var) || var->varlevelsup > 0 || !bms_is_member(var->varattno - FirstLowInvalidHeapAttributeNumber, surplusvars[var->varno]))
      {
        new_groupby = lappend(new_groupby, sgc);
      }
    }

    parse->groupClause = new_groupby;
  }
}

   
                                                                   
   
                                                                    
                                                                      
                                                                           
                                                                      
   
                                                                           
                                                                   
                                                                       
                                                                          
   
                                                                        
                                                           
   
                                                                              
                                                                            
                                                                               
                               
   
static List *
preprocess_groupclause(PlannerInfo *root, List *force)
{
  Query *parse = root->parse;
  List *new_groupclause = NIL;
  bool partial_match;
  ListCell *sl;
  ListCell *gl;

                                                        
  if (force)
  {
    foreach (sl, force)
    {
      Index ref = lfirst_int(sl);
      SortGroupClause *cl = get_sortgroupref_clause(ref, parse->groupClause);

      new_groupclause = lappend(new_groupclause, cl);
    }

    return new_groupclause;
  }

                                                 
  if (parse->sortClause == NIL)
  {
    return parse->groupClause;
  }

     
                                                                        
                                                              
     
                                                                        
     
  foreach (sl, parse->sortClause)
  {
    SortGroupClause *sc = lfirst_node(SortGroupClause, sl);

    foreach (gl, parse->groupClause)
    {
      SortGroupClause *gc = lfirst_node(SortGroupClause, gl);

      if (equal(gc, sc))
      {
        new_groupclause = lappend(new_groupclause, gc);
        break;
      }
    }
    if (gl == NULL)
    {
      break;                                 
    }
  }

                                                                  
  partial_match = (sl != NULL);

                                                           
  if (new_groupclause == NIL)
  {
    return parse->groupClause;
  }

     
                                                                           
                                                                           
                                                                           
                                                                           
                                                                         
                  
     
  foreach (gl, parse->groupClause)
  {
    SortGroupClause *gc = lfirst_node(SortGroupClause, gl);

    if (list_member_ptr(new_groupclause, gc))
    {
      continue;                                  
    }
    if (partial_match)
    {
      return parse->groupClause;                                       
    }
    if (!OidIsValid(gc->sortop))
    {
      return parse->groupClause;                                        
    }
    new_groupclause = lappend(new_groupclause, gc);
  }

                                                        
  Assert(list_length(parse->groupClause) == list_length(new_groupclause));
  return new_groupclause;
}

   
                                                                         
                                                                              
   
                                                                          
                                    
   
                                                                            
                                                                                
                                                                           
                                                                               
                                                                         
                                                                              
                                                                     
                                                                               
                                                                              
                                                                               
        
   
static List *
extract_rollup_sets(List *groupingSets)
{
  int num_sets_raw = list_length(groupingSets);
  int num_empty = 0;
  int num_sets = 0;                    
  int num_chains = 0;
  List *result = NIL;
  List **results;
  List **orig_sets;
  Bitmapset **set_masks;
  int *chains;
  short **adjacency;
  short *adjacency_buf;
  BipartiteMatchState *state;
  int i;
  int j;
  int j_size;
  ListCell *lc1 = list_head(groupingSets);
  ListCell *lc;

     
                                                                             
                                                                          
                                                                
     
  while (lc1 && lfirst(lc1) == NIL)
  {
    ++num_empty;
    lc1 = lnext(lc1);
  }

                                                                     
  if (!lc1)
  {
    return list_make1(groupingSets);
  }

               
                                                                            
                                                                      
                                                                        
                                                     
     
                                                           
     
                                                   
                                                    
                                                                   
     
                                                                 
     
                                                                         
                                                              
               
     
  orig_sets = palloc0((num_sets_raw + 1) * sizeof(List *));
  set_masks = palloc0((num_sets_raw + 1) * sizeof(Bitmapset *));
  adjacency = palloc0((num_sets_raw + 1) * sizeof(short *));
  adjacency_buf = palloc((num_sets_raw + 1) * sizeof(short));

  j_size = 0;
  j = 0;
  i = 1;

  for_each_cell(lc, lc1)
  {
    List *candidate = (List *)lfirst(lc);
    Bitmapset *candidate_set = NULL;
    ListCell *lc2;
    int dup_of = 0;

    foreach (lc2, candidate)
    {
      candidate_set = bms_add_member(candidate_set, lfirst_int(lc2));
    }

                                                                         
    if (j_size == list_length(candidate))
    {
      int k;

      for (k = j; k < i; ++k)
      {
        if (bms_equal(set_masks[k], candidate_set))
        {
          dup_of = k;
          break;
        }
      }
    }
    else if (j_size < list_length(candidate))
    {
      j_size = list_length(candidate);
      j = i;
    }

    if (dup_of > 0)
    {
      orig_sets[dup_of] = lappend(orig_sets[dup_of], candidate);
      bms_free(candidate_set);
    }
    else
    {
      int k;
      int n_adj = 0;

      orig_sets[i] = list_make1(candidate);
      set_masks[i] = candidate_set;

                                                                      

      for (k = j - 1; k > 0; --k)
      {
        if (bms_is_subset(set_masks[k], candidate_set))
        {
          adjacency_buf[++n_adj] = k;
        }
      }

      if (n_adj > 0)
      {
        adjacency_buf[0] = n_adj;
        adjacency[i] = palloc((n_adj + 1) * sizeof(short));
        memcpy(adjacency[i], adjacency_buf, (n_adj + 1) * sizeof(short));
      }
      else
      {
        adjacency[i] = NULL;
      }

      ++i;
    }
  }

  num_sets = i - 1;

     
                                                        
     
  state = BipartiteMatch(num_sets, num_sets, adjacency);

     
                                                                          
                                                                          
                                                                            
                     
     
  chains = palloc0((num_sets + 1) * sizeof(int));

  for (i = 1; i <= num_sets; ++i)
  {
    int u = state->pair_vu[i];
    int v = state->pair_uv[i];

    if (u > 0 && u < i)
    {
      chains[i] = chains[u];
    }
    else if (v > 0 && v < i)
    {
      chains[i] = chains[v];
    }
    else
    {
      chains[i] = ++num_chains;
    }
  }

                           
  results = palloc0((num_chains + 1) * sizeof(List *));

  for (i = 1; i <= num_sets; ++i)
  {
    int c = chains[i];

    Assert(c > 0);

    results[c] = list_concat(results[c], orig_sets[i]);
  }

                                                   
  while (num_empty-- > 0)
  {
    results[1] = lcons(NIL, results[1]);
  }

                        
  for (i = 1; i <= num_chains; ++i)
  {
    result = lappend(result, results[i]);
  }

     
                          
     
                                                                         
                                             
     
  BipartiteMatchFree(state);
  pfree(results);
  pfree(chains);
  for (i = 1; i <= num_sets; ++i)
  {
    if (adjacency[i])
    {
      pfree(adjacency[i]);
    }
  }
  pfree(adjacency);
  pfree(adjacency_buf);
  pfree(orig_sets);
  for (i = 1; i <= num_sets; ++i)
  {
    bms_free(set_masks[i]);
  }
  pfree(set_masks);

  return result;
}

   
                                                                               
                                                                       
   
                                                                              
                                                                              
                                                                   
   
                                                                          
                                                                          
                                                                                
                                  
   
static List *
reorder_grouping_sets(List *groupingsets, List *sortclause)
{
  ListCell *lc;
  List *previous = NIL;
  List *result = NIL;

  foreach (lc, groupingsets)
  {
    List *candidate = (List *)lfirst(lc);
    List *new_elems = list_difference_int(candidate, previous);
    GroupingSetData *gs = makeNode(GroupingSetData);

    while (list_length(sortclause) > list_length(previous) && list_length(new_elems) > 0)
    {
      SortGroupClause *sc = list_nth(sortclause, list_length(previous));
      int ref = sc->tleSortGroupRef;

      if (list_member_int(new_elems, ref))
      {
        previous = lappend_int(previous, ref);
        new_elems = list_delete_int(new_elems, ref);
      }
      else
      {
                                                         
        sortclause = NIL;
        break;
      }
    }

       
                                                                      
                                                                          
       
    previous = list_concat(previous, new_elems);

    gs->set = list_copy(previous);
    result = lcons(gs, result);
  }

  list_free(previous);

  return result;
}

   
                                                                    
   
static void
standard_qp_callback(PlannerInfo *root, void *extra)
{
  Query *parse = root->parse;
  standard_qp_extra *qp_extra = (standard_qp_extra *)extra;
  List *tlist = root->processed_tlist;
  List *activeWindows = qp_extra->activeWindows;

     
                                                                            
                                                                            
                                                           
     
  if (qp_extra->groupClause && grouping_is_sortable(qp_extra->groupClause))
  {
    root->group_pathkeys = make_pathkeys_for_sortclauses(root, qp_extra->groupClause, tlist);
  }
  else
  {
    root->group_pathkeys = NIL;
  }

                                                                    
  if (activeWindows != NIL)
  {
    WindowClause *wc = linitial_node(WindowClause, activeWindows);

    root->window_pathkeys = make_pathkeys_for_window(root, wc, tlist);
  }
  else
  {
    root->window_pathkeys = NIL;
  }

  if (parse->distinctClause && grouping_is_sortable(parse->distinctClause))
  {
    root->distinct_pathkeys = make_pathkeys_for_sortclauses(root, parse->distinctClause, tlist);
  }
  else
  {
    root->distinct_pathkeys = NIL;
  }

  root->sort_pathkeys = make_pathkeys_for_sortclauses(root, parse->sortClause, tlist);

     
                                                                    
     
                                                                         
                                                                       
                                                                             
                                                                             
                                                                      
                                                                           
                             
     
                                                                             
                                                                           
                                                                            
                                                                           
                                                                      
                            
     
  if (root->group_pathkeys)
  {
    root->query_pathkeys = root->group_pathkeys;
  }
  else if (root->window_pathkeys)
  {
    root->query_pathkeys = root->window_pathkeys;
  }
  else if (list_length(root->distinct_pathkeys) > list_length(root->sort_pathkeys))
  {
    root->query_pathkeys = root->distinct_pathkeys;
  }
  else if (root->sort_pathkeys)
  {
    root->query_pathkeys = root->sort_pathkeys;
  }
  else
  {
    root->query_pathkeys = NIL;
  }
}

   
                                                                              
   
                                                        
                                                                            
                                                               
   
                                                                              
                                                                      
                                                                         
   
static double
get_number_of_groups(PlannerInfo *root, double path_rows, grouping_sets_data *gd, List *target_list)
{
  Query *parse = root->parse;
  double dNumGroups;

  if (parse->groupClause)
  {
    List *groupExprs;

    if (parse->groupingSets)
    {
                                                      
      ListCell *lc;
      ListCell *lc2;

      Assert(gd);                          

      dNumGroups = 0;

      foreach (lc, gd->rollups)
      {
        RollupData *rollup = lfirst_node(RollupData, lc);
        ListCell *lc;

        groupExprs = get_sortgrouplist_exprs(rollup->groupClause, target_list);

        rollup->numGroups = 0.0;

        forboth(lc, rollup->gsets, lc2, rollup->gsets_data)
        {
          List *gset = (List *)lfirst(lc);
          GroupingSetData *gs = lfirst_node(GroupingSetData, lc2);
          double numGroups = estimate_num_groups(root, groupExprs, path_rows, &gset);

          gs->numGroups = numGroups;
          rollup->numGroups += numGroups;
        }

        dNumGroups += rollup->numGroups;
      }

      if (gd->hash_sets_idx)
      {
        ListCell *lc;

        gd->dNumHashGroups = 0;

        groupExprs = get_sortgrouplist_exprs(parse->groupClause, target_list);

        forboth(lc, gd->hash_sets_idx, lc2, gd->unsortable_sets)
        {
          List *gset = (List *)lfirst(lc);
          GroupingSetData *gs = lfirst_node(GroupingSetData, lc2);
          double numGroups = estimate_num_groups(root, groupExprs, path_rows, &gset);

          gs->numGroups = numGroups;
          gd->dNumHashGroups += numGroups;
        }

        dNumGroups += gd->dNumHashGroups;
      }
    }
    else
    {
                          
      groupExprs = get_sortgrouplist_exprs(parse->groupClause, target_list);

      dNumGroups = estimate_num_groups(root, groupExprs, path_rows, NULL);
    }
  }
  else if (parse->groupingSets)
  {
                                                             
    dNumGroups = list_length(parse->groupingSets);
  }
  else if (parse->hasAggs || root->hasHavingQual)
  {
                                           
    dNumGroups = 1;
  }
  else
  {
                      
    dNumGroups = 1;
  }

  return dNumGroups;
}

   
                         
   
                                                                          
                                                                          
                                                                          
                                                                           
                                                                          
                                                        
   
                                             
                                                          
                                                                                
                                                                            
   
                                                                           
                               
   
static RelOptInfo *
create_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, const AggClauseCosts *agg_costs, grouping_sets_data *gd)
{
  Query *parse = root->parse;
  RelOptInfo *grouped_rel;
  RelOptInfo *partially_grouped_rel;

     
                                                                       
                        
     
  grouped_rel = make_grouping_rel(root, input_rel, target, target_parallel_safe, parse->havingQual);

     
                                                                         
                               
     
  if (is_degenerate_grouping(root))
  {
    create_degenerate_grouping_paths(root, input_rel, grouped_rel);
  }
  else
  {
    int flags = 0;
    GroupPathExtraData extra;

       
                                                             
                                                                         
                                                             
                                                                     
                                             
       
                                                                           
                                                                        
                                            
       
    if ((gd && gd->rollups != NIL) || grouping_is_sortable(parse->groupClause))
    {
      flags |= GROUPING_CAN_USE_SORT;
    }

       
                                                                          
                 
       
                                                                     
                                                                       
                                                                           
                                                                          
                     
       
                                                                          
                                                                     
                                                                        
                                                                        
                                                                           
                                                     
       
                                                                         
                                                              
       
    if ((parse->groupClause != NIL && agg_costs->numOrderedAggs == 0 && (gd ? gd->any_hashable : grouping_is_hashable(parse->groupClause))))
    {
      flags |= GROUPING_CAN_USE_HASH;
    }

       
                                                          
       
    if (can_partial_agg(root, agg_costs))
    {
      flags |= GROUPING_CAN_PARTIAL_AGG;
    }

    extra.flags = flags;
    extra.target_parallel_safe = target_parallel_safe;
    extra.havingQual = parse->havingQual;
    extra.targetList = parse->targetList;
    extra.partial_costs_set = false;

       
                                                                          
                                                                    
                                                                           
                                                                        
       
    if (enable_partitionwise_aggregate && !parse->groupingSets)
    {
      extra.patype = PARTITIONWISE_AGGREGATE_FULL;
    }
    else
    {
      extra.patype = PARTITIONWISE_AGGREGATE_NONE;
    }

    create_ordinary_grouping_paths(root, input_rel, grouped_rel, agg_costs, gd, &extra, &partially_grouped_rel);
  }

  set_cheapest(grouped_rel);
  return grouped_rel;
}

   
                     
   
                                                       
   
                                                           
                                                             
   
static RelOptInfo *
make_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, Node *havingQual)
{
  RelOptInfo *grouped_rel;

  if (IS_OTHER_REL(input_rel))
  {
    grouped_rel = fetch_upper_rel(root, UPPERREL_GROUP_AGG, input_rel->relids);
    grouped_rel->reloptkind = RELOPT_OTHER_UPPER_REL;
  }
  else
  {
       
                                                                      
                                                                    
                   
       
    grouped_rel = fetch_upper_rel(root, UPPERREL_GROUP_AGG, NULL);
  }

                   
  grouped_rel->reltarget = target;

     
                                                                           
                                                                           
                                                     
     
  if (input_rel->consider_parallel && target_parallel_safe && is_parallel_safe(root, (Node *)havingQual))
  {
    grouped_rel->consider_parallel = true;
  }

     
                                                                        
     
  grouped_rel->serverid = input_rel->serverid;
  grouped_rel->userid = input_rel->userid;
  grouped_rel->useridiscurrent = input_rel->useridiscurrent;
  grouped_rel->fdwroutine = input_rel->fdwroutine;

  return grouped_rel;
}

   
                          
   
                                                                            
                                                                            
                                 
   
static bool
is_degenerate_grouping(PlannerInfo *root)
{
  Query *parse = root->parse;

  return (root->hasHavingQual || parse->groupingSets) && !parse->hasAggs && parse->groupClause == NIL;
}

   
                                    
   
                                                                        
                                                                              
                                                                           
                                                                              
                                                                              
                                                                             
                                                                              
                       
   
static void
create_degenerate_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel)
{
  Query *parse = root->parse;
  int nrows;
  Path *path;

  nrows = list_length(parse->groupingSets);
  if (nrows > 1)
  {
       
                                                                         
                                                                          
                                                                        
                                                                     
                 
       
    List *paths = NIL;

    while (--nrows >= 0)
    {
      path = (Path *)create_group_result_path(root, grouped_rel, grouped_rel->reltarget, (List *)parse->havingQual);
      paths = lappend(paths, path);
    }
    path = (Path *)create_append_path(root, grouped_rel, paths, NIL, NIL, NULL, 0, false, NIL, -1);
  }
  else
  {
                                                          
    path = (Path *)create_group_result_path(root, grouped_rel, grouped_rel->reltarget, (List *)parse->havingQual);
  }

  add_path(grouped_rel, path);
}

   
                                  
   
                                                                          
   
                                                                           
                                                                          
                                                                            
                                                                                
   
                                                                                
                                                          
   
static void
create_ordinary_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, GroupPathExtraData *extra, RelOptInfo **partially_grouped_rel_p)
{
  Path *cheapest_path = input_rel->cheapest_total_path;
  RelOptInfo *partially_grouped_rel = NULL;
  double dNumGroups;
  PartitionwiseAggregateType patype = PARTITIONWISE_AGGREGATE_NONE;

     
                                                                           
                                                                             
                                                                   
                                                         
     
  if (extra->patype != PARTITIONWISE_AGGREGATE_NONE && IS_PARTITIONED_REL(input_rel))
  {
       
                                                                          
                                                                         
                                                                         
                                                                         
                                                                         
                                                                       
                           
       
    if (extra->patype == PARTITIONWISE_AGGREGATE_FULL && group_by_has_partkey(input_rel, extra->targetList, root->parse->groupClause))
    {
      patype = PARTITIONWISE_AGGREGATE_FULL;
    }
    else if ((extra->flags & GROUPING_CAN_PARTIAL_AGG) != 0)
    {
      patype = PARTITIONWISE_AGGREGATE_PARTIAL;
    }
    else
    {
      patype = PARTITIONWISE_AGGREGATE_NONE;
    }
  }

     
                                                                             
                                                                            
                                                       
     
  if ((extra->flags & GROUPING_CAN_PARTIAL_AGG) != 0)
  {
    bool force_rel_creation;

       
                                                                     
                                                                       
                    
       
    force_rel_creation = (patype == PARTITIONWISE_AGGREGATE_PARTIAL);

    partially_grouped_rel = create_partial_grouping_paths(root, grouped_rel, input_rel, gd, extra, force_rel_creation);
  }

                          
  *partially_grouped_rel_p = partially_grouped_rel;

                                                               
  if (patype != PARTITIONWISE_AGGREGATE_NONE)
  {
    create_partitionwise_grouping_paths(root, input_rel, grouped_rel, partially_grouped_rel, agg_costs, gd, patype, extra);
  }

                                                         
  if (extra->patype == PARTITIONWISE_AGGREGATE_PARTIAL)
  {
    Assert(partially_grouped_rel);

    if (partially_grouped_rel->pathlist)
    {
      set_cheapest(partially_grouped_rel);
    }

    return;
  }

                                                   
  if (partially_grouped_rel && partially_grouped_rel->partial_pathlist)
  {
    gather_grouping_paths(root, partially_grouped_rel);
    set_cheapest(partially_grouped_rel);
  }

     
                                
     
  dNumGroups = get_number_of_groups(root, cheapest_path->rows, gd, extra->targetList);

                                  
  add_paths_to_grouping_rel(root, input_rel, grouped_rel, partially_grouped_rel, agg_costs, gd, dNumGroups, extra);

                                                                    
  if (grouped_rel->pathlist == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not implement GROUP BY"), errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
  }

     
                                                                          
                                          
     
  if (grouped_rel->fdwroutine && grouped_rel->fdwroutine->GetForeignUpperPaths)
  {
    grouped_rel->fdwroutine->GetForeignUpperPaths(root, UPPERREL_GROUP_AGG, input_rel, grouped_rel, extra);
  }

                                                   
  if (create_upper_paths_hook)
  {
    (*create_upper_paths_hook)(root, UPPERREL_GROUP_AGG, input_rel, grouped_rel, extra);
  }
}

   
                                                                                
                                                                            
                                                                         
                                                               
   
static void
consider_groupingsets_paths(PlannerInfo *root, RelOptInfo *grouped_rel, Path *path, bool is_sorted, bool can_hash, grouping_sets_data *gd, const AggClauseCosts *agg_costs, double dNumGroups)
{
  Query *parse = root->parse;

     
                                                                            
                                      
     
                                                                           
                                                                           
                                                             
     
                                                                         
                                                                        
     
  if (!is_sorted)
  {
    List *new_rollups = NIL;
    RollupData *unhashed_rollup = NULL;
    List *sets_data;
    List *empty_sets_data = NIL;
    List *empty_sets = NIL;
    ListCell *lc;
    ListCell *l_start = list_head(gd->rollups);
    AggStrategy strat = AGG_HASHED;
    double hashsize;
    double exclude_groups = 0.0;

    Assert(can_hash);

       
                                                                        
                                                                         
                                                                         
                                                                          
       
                                                                          
                                                                           
                                                                    
                                                                 
       
                                                                          
                                                                         
                                                                   
       
                                                                        
                                                                  
                                                                          
                                                           
       
    if (l_start != NULL && pathkeys_contained_in(root->group_pathkeys, path->pathkeys))
    {
      unhashed_rollup = lfirst_node(RollupData, l_start);
      exclude_groups = unhashed_rollup->numGroups;
      l_start = lnext(l_start);
    }

    hashsize = estimate_hashagg_tablesize(path, agg_costs, dNumGroups - exclude_groups);

       
                                                                       
                                                                           
                                                         
       
    if (hashsize > work_mem * 1024L && gd->rollups)
    {
      return;                      
    }

       
                                                                           
                                                      
       
    sets_data = list_copy(gd->unsortable_sets);

    for_each_cell(lc, l_start)
    {
      RollupData *rollup = lfirst_node(RollupData, lc);

         
                                                                        
                                                                        
                                                                         
                                                                     
                  
         
                                                                        
                                                                    
                          
         
      if (!rollup->hashable)
      {
        return;
      }
      else
      {
        sets_data = list_concat(sets_data, list_copy(rollup->gsets_data));
      }
    }
    foreach (lc, sets_data)
    {
      GroupingSetData *gs = lfirst_node(GroupingSetData, lc);
      List *gset = gs->set;
      RollupData *rollup;

      if (gset == NIL)
      {
                                                  
        empty_sets_data = lappend(empty_sets_data, gs);
        empty_sets = lappend(empty_sets, NIL);
      }
      else
      {
        rollup = makeNode(RollupData);

        rollup->groupClause = preprocess_groupclause(root, gset);
        rollup->gsets_data = list_make1(gs);
        rollup->gsets = remap_to_groupclause_idx(rollup->groupClause, rollup->gsets_data, gd->tleref_to_colnum_map);
        rollup->numGroups = gs->numGroups;
        rollup->hashable = true;
        rollup->is_hashed = true;
        new_rollups = lappend(new_rollups, rollup);
      }
    }

       
                                                                      
                                                
       
    if (new_rollups == NIL)
    {
      return;
    }

       
                                                                      
                     
       
    Assert(!unhashed_rollup || !empty_sets);

    if (unhashed_rollup)
    {
      new_rollups = lappend(new_rollups, unhashed_rollup);
      strat = AGG_MIXED;
    }
    else if (empty_sets)
    {
      RollupData *rollup = makeNode(RollupData);

      rollup->groupClause = NIL;
      rollup->gsets_data = empty_sets_data;
      rollup->gsets = empty_sets;
      rollup->numGroups = list_length(empty_sets);
      rollup->hashable = false;
      rollup->is_hashed = false;
      new_rollups = lappend(new_rollups, rollup);
      strat = AGG_MIXED;
    }

    add_path(grouped_rel, (Path *)create_groupingsets_path(root, grouped_rel, path, (List *)parse->havingQual, strat, new_rollups, agg_costs, dNumGroups));
    return;
  }

     
                                                                  
     
  if (list_length(gd->rollups) == 0)
  {
    return;
  }

     
                                                                             
                                                                           
                                          
     
                                                                        
                                                                    
     
  if (can_hash && gd->any_hashable)
  {
    List *rollups = NIL;
    List *hash_sets = list_copy(gd->unsortable_sets);
    double availspace = (work_mem * 1024.0);
    ListCell *lc;

       
                                                                       
       
    availspace -= estimate_hashagg_tablesize(path, agg_costs, gd->dNumHashGroups);

    if (availspace > 0 && list_length(gd->rollups) > 1)
    {
      double scale;
      int num_rollups = list_length(gd->rollups);
      int k_capacity;
      int *k_weights = palloc(num_rollups * sizeof(int));
      Bitmapset *hash_items = NULL;
      int i;

         
                                                                    
                                                                        
                                                                      
                                                                       
                                                                   
                                                                         
                                                                         
         
                                                                        
                                                                        
                                                                         
                                                                         
                                                                        
                                                                        
                                                                       
                         
         
                                                                      
                                                                       
                                                                   
         
      scale = Max(availspace / (20.0 * num_rollups), 1.0);
      k_capacity = (int)floor(availspace / scale);

         
                                                                       
                                                                       
                                                                        
                                             
         
      i = 0;
      for_each_cell(lc, lnext(list_head(gd->rollups)))
      {
        RollupData *rollup = lfirst_node(RollupData, lc);

        if (rollup->hashable)
        {
          double sz = estimate_hashagg_tablesize(path, agg_costs, rollup->numGroups);

             
                                                                  
                                                 
             
          k_weights[i] = (int)Min(floor(sz / scale), k_capacity + 1.0);
          ++i;
        }
      }

         
                                                                  
                                                                      
                                                                    
                   
         
      if (i > 0)
      {
        hash_items = DiscreteKnapsack(k_capacity, i, k_weights, NULL);
      }

      if (!bms_is_empty(hash_items))
      {
        rollups = list_make1(linitial(gd->rollups));

        i = 0;
        for_each_cell(lc, lnext(list_head(gd->rollups)))
        {
          RollupData *rollup = lfirst_node(RollupData, lc);

          if (rollup->hashable)
          {
            if (bms_is_member(i, hash_items))
            {
              hash_sets = list_concat(hash_sets, list_copy(rollup->gsets_data));
            }
            else
            {
              rollups = lappend(rollups, rollup);
            }
            ++i;
          }
          else
          {
            rollups = lappend(rollups, rollup);
          }
        }
      }
    }

    if (!rollups && hash_sets)
    {
      rollups = list_copy(gd->rollups);
    }

    foreach (lc, hash_sets)
    {
      GroupingSetData *gs = lfirst_node(GroupingSetData, lc);
      RollupData *rollup = makeNode(RollupData);

      Assert(gs->set != NIL);

      rollup->groupClause = preprocess_groupclause(root, gs->set);
      rollup->gsets_data = list_make1(gs);
      rollup->gsets = remap_to_groupclause_idx(rollup->groupClause, rollup->gsets_data, gd->tleref_to_colnum_map);
      rollup->numGroups = gs->numGroups;
      rollup->hashable = true;
      rollup->is_hashed = true;
      rollups = lcons(rollup, rollups);
    }

    if (rollups)
    {
      add_path(grouped_rel, (Path *)create_groupingsets_path(root, grouped_rel, path, (List *)parse->havingQual, AGG_MIXED, rollups, agg_costs, dNumGroups));
    }
  }

     
                                     
     
  if (!gd->unsortable_sets)
  {
    add_path(grouped_rel, (Path *)create_groupingsets_path(root, grouped_rel, path, (List *)parse->havingQual, AGG_SORTED, gd->rollups, agg_costs, dNumGroups));
  }
}

   
                       
   
                                                                         
   
                                             
                                                    
                                                               
                                            
                                                  
   
                                                                     
   
static RelOptInfo *
create_window_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *input_target, PathTarget *output_target, bool output_target_parallel_safe, WindowFuncLists *wflists, List *activeWindows)
{
  RelOptInfo *window_rel;
  ListCell *lc;

                                                           
  window_rel = fetch_upper_rel(root, UPPERREL_WINDOW, NULL);

     
                                                                          
                                                                        
                                                                      
     
  if (input_rel->consider_parallel && output_target_parallel_safe && is_parallel_safe(root, (Node *)activeWindows))
  {
    window_rel->consider_parallel = true;
  }

     
                                                                       
     
  window_rel->serverid = input_rel->serverid;
  window_rel->userid = input_rel->userid;
  window_rel->useridiscurrent = input_rel->useridiscurrent;
  window_rel->fdwroutine = input_rel->fdwroutine;

     
                                                                    
                                                                           
                                                                      
     
  foreach (lc, input_rel->pathlist)
  {
    Path *path = (Path *)lfirst(lc);

    if (path == input_rel->cheapest_total_path || pathkeys_contained_in(root->window_pathkeys, path->pathkeys))
    {
      create_one_window_path(root, window_rel, path, input_target, output_target, wflists, activeWindows);
    }
  }

     
                                                                          
                                          
     
  if (window_rel->fdwroutine && window_rel->fdwroutine->GetForeignUpperPaths)
  {
    window_rel->fdwroutine->GetForeignUpperPaths(root, UPPERREL_WINDOW, input_rel, window_rel, NULL);
  }

                                                   
  if (create_upper_paths_hook)
  {
    (*create_upper_paths_hook)(root, UPPERREL_WINDOW, input_rel, window_rel, NULL);
  }

                                   
  set_cheapest(window_rel);

  return window_rel;
}

   
                                                                       
                                 
   
                                          
                                                      
                                                    
                                                               
                                            
                                                  
   
static void
create_one_window_path(PlannerInfo *root, RelOptInfo *window_rel, Path *path, PathTarget *input_target, PathTarget *output_target, WindowFuncLists *wflists, List *activeWindows)
{
  PathTarget *window_target;
  ListCell *l;

     
                                                                             
                                                                          
                                                                           
                                
     
                                                                          
                                                                           
                                                                           
                                                                            
                                                                        
                                                                            
                                                                           
                                                 
     
  window_target = input_target;

  foreach (l, activeWindows)
  {
    WindowClause *wc = lfirst_node(WindowClause, l);
    List *window_pathkeys;

    window_pathkeys = make_pathkeys_for_window(root, wc, root->processed_tlist);

                           
    if (!pathkeys_contained_in(window_pathkeys, path->pathkeys))
    {
      path = (Path *)create_sort_path(root, window_rel, path, window_pathkeys, -1.0);
    }

    if (lnext(l))
    {
         
                                                                   
                                                                    
                                                    
         
                                                                         
                                                                
         
      ListCell *lc2;

      window_target = copy_pathtarget(window_target);
      foreach (lc2, wflists->windowFuncs[wc->winref])
      {
        WindowFunc *wfunc = lfirst_node(WindowFunc, lc2);

        add_column_to_pathtarget(window_target, (Expr *)wfunc, 0);
        window_target->width += get_typavgwidth(wfunc->wintype, -1);
      }
    }
    else
    {
                                                            
      window_target = output_target;
    }

    path = (Path *)create_windowagg_path(root, window_rel, path, window_target, wflists->windowFuncs[wc->winref], wc);
  }

  add_path(window_rel, path);
}

   
                         
   
                                                                         
   
                                             
   
                                                                          
                                       
   
static RelOptInfo *
create_distinct_paths(PlannerInfo *root, RelOptInfo *input_rel)
{
  Query *parse = root->parse;
  Path *cheapest_input_path = input_rel->cheapest_total_path;
  RelOptInfo *distinct_rel;
  double numDistinctRows;
  bool allow_hash;
  Path *path;
  ListCell *lc;

                                                             
  distinct_rel = fetch_upper_rel(root, UPPERREL_DISTINCT, NULL);

     
                                                                      
                                                                         
                                                                          
                                                                          
                                    
     
  distinct_rel->consider_parallel = input_rel->consider_parallel;

     
                                                                         
     
  distinct_rel->serverid = input_rel->serverid;
  distinct_rel->userid = input_rel->userid;
  distinct_rel->useridiscurrent = input_rel->useridiscurrent;
  distinct_rel->fdwroutine = input_rel->fdwroutine;

                                                      
  if (parse->groupClause || parse->groupingSets || parse->hasAggs || root->hasHavingQual)
  {
       
                                                                          
                                                                         
                               
       
    numDistinctRows = cheapest_input_path->rows;
  }
  else
  {
       
                                                                        
       
    List *distinctExprs;

    distinctExprs = get_sortgrouplist_exprs(parse->distinctClause, parse->targetList);
    numDistinctRows = estimate_num_groups(root, distinctExprs, cheapest_input_path->rows, NULL);
  }

     
                                                                   
     
  if (grouping_is_sortable(parse->distinctClause))
  {
       
                                                                      
                                                                          
                                                
       
                                                                      
                                                                       
                                                                        
                                                                       
                                                                        
                   
       
    List *needed_pathkeys;

    if (parse->hasDistinctOn && list_length(root->distinct_pathkeys) < list_length(root->sort_pathkeys))
    {
      needed_pathkeys = root->sort_pathkeys;
    }
    else
    {
      needed_pathkeys = root->distinct_pathkeys;
    }

    foreach (lc, input_rel->pathlist)
    {
      Path *path = (Path *)lfirst(lc);

      if (pathkeys_contained_in(needed_pathkeys, path->pathkeys))
      {
        add_path(distinct_rel, (Path *)create_upper_unique_path(root, distinct_rel, path, list_length(root->distinct_pathkeys), numDistinctRows));
      }
    }

                                                                     
    if (list_length(root->distinct_pathkeys) < list_length(root->sort_pathkeys))
    {
      needed_pathkeys = root->sort_pathkeys;
                                                       
      Assert(pathkeys_contained_in(root->distinct_pathkeys, needed_pathkeys));
    }
    else
    {
      needed_pathkeys = root->distinct_pathkeys;
    }

    path = cheapest_input_path;
    if (!pathkeys_contained_in(needed_pathkeys, path->pathkeys))
    {
      path = (Path *)create_sort_path(root, distinct_rel, path, needed_pathkeys, -1.0);
    }

    add_path(distinct_rel, (Path *)create_upper_unique_path(root, distinct_rel, path, list_length(root->distinct_pathkeys), numDistinctRows));
  }

     
                                                                   
     
                                                                            
                                                                             
                                                                        
                                                                            
                                                                          
               
     
                                                                           
                                                        
     
  if (distinct_rel->pathlist == NIL)
  {
    allow_hash = true;                              
  }
  else if (parse->hasDistinctOn || !enable_hashagg)
  {
    allow_hash = false;                                        
  }
  else
  {
    Size hashentrysize;

                                                         
    hashentrysize = MAXALIGN(cheapest_input_path->pathtarget->width) + MAXALIGN(SizeofMinimalTupleHeader);
                                          
    hashentrysize += hash_agg_entry_size(0);

                                                                         
    allow_hash = (hashentrysize * numDistinctRows <= work_mem * 1024L);
  }

  if (allow_hash && grouping_is_hashable(parse->distinctClause))
  {
                                                           
    add_path(distinct_rel, (Path *)create_agg_path(root, distinct_rel, cheapest_input_path, cheapest_input_path->pathtarget, AGG_HASHED, AGGSPLIT_SIMPLE, parse->distinctClause, NIL, NULL, numDistinctRows));
  }

                                                                    
  if (distinct_rel->pathlist == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not implement DISTINCT"), errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
  }

     
                                                                          
                                          
     
  if (distinct_rel->fdwroutine && distinct_rel->fdwroutine->GetForeignUpperPaths)
  {
    distinct_rel->fdwroutine->GetForeignUpperPaths(root, UPPERREL_DISTINCT, input_rel, distinct_rel, NULL);
  }

                                                   
  if (create_upper_paths_hook)
  {
    (*create_upper_paths_hook)(root, UPPERREL_DISTINCT, input_rel, distinct_rel, NULL);
  }

                                   
  set_cheapest(distinct_rel);

  return distinct_rel;
}

   
                        
   
                                                                  
   
                                                               
                                                                 
                                 
   
                                             
                                                       
                                                                 
                                           
   
static RelOptInfo *
create_ordered_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, double limit_tuples)
{
  Path *cheapest_input_path = input_rel->cheapest_total_path;
  RelOptInfo *ordered_rel;
  ListCell *lc;

                                                            
  ordered_rel = fetch_upper_rel(root, UPPERREL_ORDERED, NULL);

     
                                                                           
                                                                           
                                   
     
  if (input_rel->consider_parallel && target_parallel_safe)
  {
    ordered_rel->consider_parallel = true;
  }

     
                                                                        
     
  ordered_rel->serverid = input_rel->serverid;
  ordered_rel->userid = input_rel->userid;
  ordered_rel->useridiscurrent = input_rel->useridiscurrent;
  ordered_rel->fdwroutine = input_rel->fdwroutine;

  foreach (lc, input_rel->pathlist)
  {
    Path *path = (Path *)lfirst(lc);
    bool is_sorted;

    is_sorted = pathkeys_contained_in(root->sort_pathkeys, path->pathkeys);
    if (path == cheapest_input_path || is_sorted)
    {
      if (!is_sorted)
      {
                                                               
        path = (Path *)create_sort_path(root, ordered_rel, path, root->sort_pathkeys, limit_tuples);
      }

                                         
      if (path->pathtarget != target)
      {
        path = apply_projection_to_path(root, ordered_rel, path, target);
      }

      add_path(ordered_rel, path);
    }
  }

     
                                                                         
                                                                           
                                                                          
                                                                          
                                                                             
                                                                       
                                                                      
                                                                       
     
  if (ordered_rel->consider_parallel && root->sort_pathkeys != NIL && input_rel->partial_pathlist != NIL)
  {
    Path *cheapest_partial_path;

    cheapest_partial_path = linitial(input_rel->partial_pathlist);

       
                                                                       
                                       
       
    if (!pathkeys_contained_in(root->sort_pathkeys, cheapest_partial_path->pathkeys))
    {
      Path *path;
      double total_groups;

      path = (Path *)create_sort_path(root, ordered_rel, cheapest_partial_path, root->sort_pathkeys, limit_tuples);

      total_groups = cheapest_partial_path->rows * cheapest_partial_path->parallel_workers;
      path = (Path *)create_gather_merge_path(root, ordered_rel, path, path->pathtarget, root->sort_pathkeys, NULL, &total_groups);

                                         
      if (path->pathtarget != target)
      {
        path = apply_projection_to_path(root, ordered_rel, path, target);
      }

      add_path(ordered_rel, path);
    }
  }

     
                                                                          
                                          
     
  if (ordered_rel->fdwroutine && ordered_rel->fdwroutine->GetForeignUpperPaths)
  {
    ordered_rel->fdwroutine->GetForeignUpperPaths(root, UPPERREL_ORDERED, input_rel, ordered_rel, NULL);
  }

                                                   
  if (create_upper_paths_hook)
  {
    (*create_upper_paths_hook)(root, UPPERREL_ORDERED, input_rel, ordered_rel, NULL);
  }

     
                                                                         
                       
     
  Assert(ordered_rel->pathlist != NIL);

  return ordered_rel;
}

   
                           
                                                                          
   
                                                                          
                                                                          
                                                                        
                              
   
                                                                         
                                                                           
                                                                      
                                                                           
                                                                           
                                                                        
                                   
                                                 
                                                   
            
                                                                      
                                                               
   
                                                                        
   
                                                                          
                    
   
static PathTarget *
make_group_input_target(PlannerInfo *root, PathTarget *final_target)
{
  Query *parse = root->parse;
  PathTarget *input_target;
  List *non_group_cols;
  List *non_group_vars;
  int i;
  ListCell *lc;

     
                                                                            
                                                               
     
  input_target = create_empty_pathtarget();
  non_group_cols = NIL;

  i = 0;
  foreach (lc, final_target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);
    Index sgref = get_pathtarget_sortgroupref(final_target, i);

    if (sgref && parse->groupClause && get_sortgroupref_clause_noerr(sgref, parse->groupClause) != NULL)
    {
         
                                                                      
         
      add_column_to_pathtarget(input_target, expr, sgref);
    }
    else
    {
         
                                                                        
                                  
         
      non_group_cols = lappend(non_group_cols, expr);
    }

    i++;
  }

     
                                                                   
     
  if (parse->havingQual)
  {
    non_group_cols = lappend(non_group_cols, parse->havingQual);
  }

     
                                                                          
                                                                       
                                                                      
                                                                          
                                                                       
                                               
     
  non_group_vars = pull_var_clause((Node *)non_group_cols, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);
  add_new_columns_to_pathtarget(input_target, non_group_vars);

                      
  list_free(non_group_vars);
  list_free(non_group_cols);

                                                           
  return set_pathtarget_cost_width(root, input_target);
}

   
                                
                                                                     
                                                              
   
                                                                         
                                                                         
                                                                        
   
                                                                       
                                                                              
                                                                             
   
                                                                               
                                            
   
static PathTarget *
make_partial_grouping_target(PlannerInfo *root, PathTarget *grouping_target, Node *havingQual)
{
  Query *parse = root->parse;
  PathTarget *partial_target;
  List *non_group_cols;
  List *non_group_exprs;
  int i;
  ListCell *lc;

  partial_target = create_empty_pathtarget();
  non_group_cols = NIL;

  i = 0;
  foreach (lc, grouping_target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);
    Index sgref = get_pathtarget_sortgroupref(grouping_target, i);

    if (sgref && parse->groupClause && get_sortgroupref_clause_noerr(sgref, parse->groupClause) != NULL)
    {
         
                                                                        
                                                                        
         
      add_column_to_pathtarget(partial_target, expr, sgref);
    }
    else
    {
         
                                                                        
                                  
         
      non_group_cols = lappend(non_group_cols, expr);
    }

    i++;
  }

     
                                                                           
     
  if (havingQual)
  {
    non_group_cols = lappend(non_group_cols, havingQual);
  }

     
                                                                      
                                                                             
                                                                            
                                                                             
                                                                      
     
  non_group_exprs = pull_var_clause((Node *)non_group_cols, PVC_INCLUDE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);

  add_new_columns_to_pathtarget(partial_target, non_group_exprs);

     
                                                                            
                                                                           
                                                         
     
  foreach (lc, partial_target->exprs)
  {
    Aggref *aggref = (Aggref *)lfirst(lc);

    if (IsA(aggref, Aggref))
    {
      Aggref *newaggref;

         
                                                                        
                                                                      
         
      newaggref = makeNode(Aggref);
      memcpy(newaggref, aggref, sizeof(Aggref));

                                                     
      mark_partial_aggref(newaggref, AGGSPLIT_INITIAL_SERIAL);

      lfirst(lc) = newaggref;
    }
  }

                      
  list_free(non_group_exprs);
  list_free(non_group_cols);

                                                           
  return set_pathtarget_cost_width(root, partial_target);
}

   
                       
                                                                       
   
                                                                              
   
void
mark_partial_aggref(Aggref *agg, AggSplit aggsplit)
{
                                                     
  Assert(OidIsValid(agg->aggtranstype));
                                                              
  Assert(agg->aggsplit == AGGSPLIT_SIMPLE);

                                                                  
  agg->aggsplit = aggsplit;

     
                                                                          
                                                                       
                                            
     
  if (DO_AGGSPLIT_SKIPFINAL(aggsplit))
  {
    if (agg->aggtranstype == INTERNALOID && DO_AGGSPLIT_SERIALIZE(aggsplit))
    {
      agg->aggtype = BYTEAOID;
    }
    else
    {
      agg->aggtype = agg->aggtranstype;
    }
  }
}

   
                           
                                                          
   
                                                                          
                                                                         
                                                                        
                                                                           
                                           
   
static List *
postprocess_setop_tlist(List *new_tlist, List *orig_tlist)
{
  ListCell *l;
  ListCell *orig_tlist_item = list_head(orig_tlist);

  foreach (l, new_tlist)
  {
    TargetEntry *new_tle = lfirst_node(TargetEntry, l);
    TargetEntry *orig_tle;

                                                
    if (new_tle->resjunk)
    {
      continue;
    }

    Assert(orig_tlist_item != NULL);
    orig_tle = lfirst_node(TargetEntry, orig_tlist_item);
    orig_tlist_item = lnext(orig_tlist_item);
    if (orig_tle->resjunk)                        
    {
      elog(ERROR, "resjunk output columns are not implemented");
    }
    Assert(new_tle->resno == orig_tle->resno);
    new_tle->ressortgroupref = orig_tle->ressortgroupref;
  }
  if (orig_tlist_item != NULL)
  {
    elog(ERROR, "resjunk output columns are not implemented");
  }
  return new_tlist;
}

   
                         
                                                                       
                                                                      
   
static List *
select_active_windows(PlannerInfo *root, WindowFuncLists *wflists)
{
  List *windowClause = root->parse->windowClause;
  List *result = NIL;
  ListCell *lc;
  int nActive = 0;
  WindowClauseSortData *actives = palloc(sizeof(WindowClauseSortData) * list_length(windowClause));

                                                       
  foreach (lc, windowClause)
  {
    WindowClause *wc = lfirst_node(WindowClause, lc);

                                                                    
    Assert(wc->winref <= wflists->maxWinRef);
    if (wflists->windowFuncs[wc->winref] == NIL)
    {
      continue;
    }

    actives[nActive].wc = wc;                      

       
                                                                       
                                                                           
                                                                           
                                                                         
                                                                         
                                                                        
             
       
                                                                           
                                                                  
                                                                
       
    actives[nActive].uniqueOrder = list_concat_unique(list_copy(wc->partitionClause), wc->orderClause);
    nActive++;
  }

     
                                                                          
                                                                             
                                                                           
                                      
     
                                                                            
                                                                         
                                                                       
                                                                          
                                                                           
                                                                         
                                                                        
                                            
     
                                                                           
                                                                          
                                                                             
                               
     
  qsort(actives, nActive, sizeof(WindowClauseSortData), common_prefix_cmp);

                                                             
  for (int i = 0; i < nActive; i++)
  {
    result = lappend(result, actives[i].wc);
  }

  pfree(actives);

  return result;
}

   
                     
                                                        
   
                                                                             
                                                                               
                                                               
   
static int
common_prefix_cmp(const void *a, const void *b)
{
  const WindowClauseSortData *wcsa = a;
  const WindowClauseSortData *wcsb = b;
  ListCell *item_a;
  ListCell *item_b;

  forboth(item_a, wcsa->uniqueOrder, item_b, wcsb->uniqueOrder)
  {
    SortGroupClause *sca = lfirst_node(SortGroupClause, item_a);
    SortGroupClause *scb = lfirst_node(SortGroupClause, item_b);

    if (sca->tleSortGroupRef > scb->tleSortGroupRef)
    {
      return -1;
    }
    else if (sca->tleSortGroupRef < scb->tleSortGroupRef)
    {
      return 1;
    }
    else if (sca->sortop > scb->sortop)
    {
      return -1;
    }
    else if (sca->sortop < scb->sortop)
    {
      return 1;
    }
    else if (sca->nulls_first && !scb->nulls_first)
    {
      return -1;
    }
    else if (!sca->nulls_first && scb->nulls_first)
    {
      return 1;
    }
                                                                         
  }

  if (list_length(wcsa->uniqueOrder) > list_length(wcsb->uniqueOrder))
  {
    return -1;
  }
  else if (list_length(wcsa->uniqueOrder) < list_length(wcsb->uniqueOrder))
  {
    return 1;
  }

  return 0;
}

   
                            
                                                                           
   
                                                                           
                                                                     
                                                                               
                                                                            
                                                                            
                                                                              
                                   
   
                                                                               
                                                                            
                                                                         
                                                                      
                                                                       
                                                              
                                                                            
                                                                        
                            
   
                                                                        
                                                                        
                                                              
   
                                                                        
                                                                          
                            
   
                                                                            
                                   
   
static PathTarget *
make_window_input_target(PlannerInfo *root, PathTarget *final_target, List *activeWindows)
{
  Query *parse = root->parse;
  PathTarget *input_target;
  Bitmapset *sgrefs;
  List *flattenable_cols;
  List *flattenable_vars;
  int i;
  ListCell *lc;

  Assert(parse->hasWindowFuncs);

     
                                                                           
                                                      
     
  sgrefs = NULL;
  foreach (lc, activeWindows)
  {
    WindowClause *wc = lfirst_node(WindowClause, lc);
    ListCell *lc2;

    foreach (lc2, wc->partitionClause)
    {
      SortGroupClause *sortcl = lfirst_node(SortGroupClause, lc2);

      sgrefs = bms_add_member(sgrefs, sortcl->tleSortGroupRef);
    }
    foreach (lc2, wc->orderClause)
    {
      SortGroupClause *sortcl = lfirst_node(SortGroupClause, lc2);

      sgrefs = bms_add_member(sgrefs, sortcl->tleSortGroupRef);
    }
  }

                                                            
  foreach (lc, parse->groupClause)
  {
    SortGroupClause *grpcl = lfirst_node(SortGroupClause, lc);

    sgrefs = bms_add_member(sgrefs, grpcl->tleSortGroupRef);
  }

     
                                                                             
                                             
     
  input_target = create_empty_pathtarget();
  flattenable_cols = NIL;

  i = 0;
  foreach (lc, final_target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);
    Index sgref = get_pathtarget_sortgroupref(final_target, i);

       
                                                                          
                                                                       
                                                
       
    if (sgref != 0 && bms_is_member(sgref, sgrefs))
    {
         
                                                                      
                       
         
      add_column_to_pathtarget(input_target, expr, sgref);
    }
    else
    {
         
                                                                        
                                        
         
      flattenable_cols = lappend(flattenable_cols, expr);
    }

    i++;
  }

     
                                                                             
                                                                          
                                                                           
     
                                                                          
                                                                            
                                                                  
                                                                     
     
  flattenable_vars = pull_var_clause((Node *)flattenable_cols, PVC_INCLUDE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);
  add_new_columns_to_pathtarget(input_target, flattenable_vars);

                      
  list_free(flattenable_vars);
  list_free(flattenable_cols);

                                                           
  return set_pathtarget_cost_width(root, input_target);
}

   
                            
                                                                  
                                
   
                                                                           
                                                                             
                                                                   
   
static List *
make_pathkeys_for_window(PlannerInfo *root, WindowClause *wc, List *tlist)
{
  List *window_pathkeys;
  List *window_sortclauses;

                                 
  if (!grouping_is_sortable(wc->partitionClause))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not implement window PARTITION BY"), errdetail("Window partitioning columns must be of sortable datatypes.")));
  }
  if (!grouping_is_sortable(wc->orderClause))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not implement window ORDER BY"), errdetail("Window ordering columns must be of sortable datatypes.")));
  }

                                        
  window_sortclauses = list_concat(list_copy(wc->partitionClause), list_copy(wc->orderClause));
  window_pathkeys = make_pathkeys_for_sortclauses(root, window_sortclauses, tlist);
  list_free(window_sortclauses);
  return window_pathkeys;
}

   
                          
                                                                     
   
                                                                              
                                                                             
                                                                              
                  
   
                                                                            
                                                                        
                                                                            
                                                                             
                                                                             
                                                                             
                                                                             
                        
   
                                                                              
                                                                              
                                                                           
                                                                             
                                                                              
                                                                             
                                                                            
                                                                          
                                                                          
                                                                          
                                                                        
                                                                        
                                                                              
                                                                              
                                                                        
                                                                      
   
                                                                   
                                                                         
                                                                          
                                                                         
                                                                       
                                                                   
   
                                                                         
                                                                         
                                                                         
                                                                           
                                                                     
                                                                     
   
                                                                            
                                                                          
                                                                           
                                                                    
                                                                            
                     
   
                                                                        
                                                          
   
                                                                            
                                                                      
                                                                            
   
                                                                             
                                                  
   
static PathTarget *
make_sort_input_target(PlannerInfo *root, PathTarget *final_target, bool *have_postponed_srfs)
{
  Query *parse = root->parse;
  PathTarget *input_target;
  int ncols;
  bool *col_is_srf;
  bool *postpone_col;
  bool have_srf;
  bool have_volatile;
  bool have_expensive;
  bool have_srf_sortcols;
  bool postpone_srfs;
  List *postponable_cols;
  List *postponable_vars;
  int i;
  ListCell *lc;

                                                    
  Assert(parse->sortClause);

  *have_postponed_srfs = false;                     

                                                        
  ncols = list_length(final_target->exprs);
  col_is_srf = (bool *)palloc0(ncols * sizeof(bool));
  postpone_col = (bool *)palloc0(ncols * sizeof(bool));
  have_srf = have_volatile = have_expensive = have_srf_sortcols = false;

  i = 0;
  foreach (lc, final_target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);

       
                                                                       
                                                                        
                                                                         
                                                                        
                                                                       
                                                   
       
    if (get_pathtarget_sortgroupref(final_target, i) == 0)
    {
         
                                                                        
                                                                  
         
      if (parse->hasTargetSRFs && expression_returns_set((Node *)expr))
      {
                                                              
        col_is_srf[i] = true;
        have_srf = true;
      }
      else if (contain_volatile_functions((Node *)expr))
      {
                                      
        postpone_col[i] = true;
        have_volatile = true;
      }
      else
      {
           
                                                                      
                                                                      
                                   
           
        QualCost cost;

        cost_qual_eval_node(&cost, (Node *)expr, root);

           
                                                               
                                                                       
                              
           
        if (cost.per_tuple > 10 * cpu_operator_cost)
        {
          postpone_col[i] = true;
          have_expensive = true;
        }
      }
    }
    else
    {
                                                                 
      if (!have_srf_sortcols && parse->hasTargetSRFs && expression_returns_set((Node *)expr))
      {
        have_srf_sortcols = true;
      }
    }

    i++;
  }

     
                                                                             
     
  postpone_srfs = (have_srf && !have_srf_sortcols);

     
                                                                        
     
  if (!(postpone_srfs || have_volatile || (have_expensive && (parse->limitCount || root->tuple_fraction > 0))))
  {
    return final_target;
  }

     
                                                                        
                                                                           
                                                                             
                
     
  *have_postponed_srfs = postpone_srfs;

     
                                                                             
                                                                          
                           
     
  input_target = create_empty_pathtarget();
  postponable_cols = NIL;

  i = 0;
  foreach (lc, final_target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);

    if (postpone_col[i] || (postpone_srfs && col_is_srf[i]))
    {
      postponable_cols = lappend(postponable_cols, expr);
    }
    else
    {
      add_column_to_pathtarget(input_target, expr, get_pathtarget_sortgroupref(final_target, i));
    }

    i++;
  }

     
                                                                  
                                                                       
                                                                  
                                                                        
                                        
     
  postponable_vars = pull_var_clause((Node *)postponable_cols, PVC_INCLUDE_AGGREGATES | PVC_INCLUDE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);
  add_new_columns_to_pathtarget(input_target, postponable_vars);

                      
  list_free(postponable_vars);
  list_free(postponable_cols);

                                                                    
  return set_pathtarget_cost_width(root, input_target);
}

   
                                
                                                                       
                                                               
   
                                                                 
   
                                                           
   
Path *
get_cheapest_fractional_path(RelOptInfo *rel, double tuple_fraction)
{
  Path *best_path = rel->cheapest_total_path;
  ListCell *l;

                                                                            
  if (tuple_fraction <= 0.0)
  {
    return best_path;
  }

                                                                            
  if (tuple_fraction >= 1.0 && best_path->rows > 0)
  {
    tuple_fraction /= best_path->rows;
  }

  foreach (l, rel->pathlist)
  {
    Path *path = (Path *)lfirst(l);

    if (path == rel->cheapest_total_path || compare_fractional_path_costs(best_path, path, tuple_fraction) <= 0)
    {
      continue;
    }

    best_path = path;
  }

  return best_path;
}

   
                         
                                                                     
   
                                                                           
                                                                               
                                                                               
                                                                             
                                                                          
                                                        
   
                                                             
                                                                            
                      
   
static void
adjust_paths_for_srfs(PlannerInfo *root, RelOptInfo *rel, List *targets, List *targets_contain_srfs)
{
  ListCell *lc;

  Assert(list_length(targets) == list_length(targets_contain_srfs));
  Assert(!linitial_int(targets_contain_srfs));

                                                           
  if (list_length(targets) == 1)
  {
    return;
  }

     
                                                            
     
                                                                       
                                                                          
                                                                            
                                                                           
                                                                         
                                                         
     
  foreach (lc, rel->pathlist)
  {
    Path *subpath = (Path *)lfirst(lc);
    Path *newpath = subpath;
    ListCell *lc1, *lc2;

    Assert(subpath->param_info == NULL);
    forboth(lc1, targets, lc2, targets_contain_srfs)
    {
      PathTarget *thistarget = lfirst_node(PathTarget, lc1);
      bool contains_srfs = (bool)lfirst_int(lc2);

                                                                     
      if (contains_srfs)
      {
        newpath = (Path *)create_set_projection_path(root, rel, newpath, thistarget);
      }
      else
      {
        newpath = (Path *)apply_projection_to_path(root, rel, newpath, thistarget);
      }
    }
    lfirst(lc) = newpath;
    if (subpath == rel->cheapest_startup_path)
    {
      rel->cheapest_startup_path = newpath;
    }
    if (subpath == rel->cheapest_total_path)
    {
      rel->cheapest_total_path = newpath;
    }
  }

                                          
  foreach (lc, rel->partial_pathlist)
  {
    Path *subpath = (Path *)lfirst(lc);
    Path *newpath = subpath;
    ListCell *lc1, *lc2;

    Assert(subpath->param_info == NULL);
    forboth(lc1, targets, lc2, targets_contain_srfs)
    {
      PathTarget *thistarget = lfirst_node(PathTarget, lc1);
      bool contains_srfs = (bool)lfirst_int(lc2);

                                                                     
      if (contains_srfs)
      {
        newpath = (Path *)create_set_projection_path(root, rel, newpath, thistarget);
      }
      else
      {
                                                                      
        newpath = (Path *)create_projection_path(root, rel, newpath, thistarget);
      }
    }
    lfirst(lc) = newpath;
  }
}

   
                      
                                                                  
   
                                                                           
                                                                      
                                                                          
                                                                            
   
                                                                            
                                                                           
                                                                             
                                                                             
                                                                            
                                                                               
                                                   
   
                                                                              
                                                                             
                                                                              
                                                             
   
                                                                             
                                                                             
                                                                            
                                                                         
                                                                            
   
Expr *
expression_planner(Expr *expr)
{
  Node *result;

     
                                                                         
                                
     
  result = eval_const_expressions(NULL, (Node *)expr);

                                          
  fix_opfuncids(result);

  return (Expr *)result;
}

   
                                
                                                                  
                                                                       
   
                                                                         
                                                                               
                                                                            
                                                                          
   
Expr *
expression_planner_with_deps(Expr *expr, List **relationOids, List **invalItems)
{
  Node *result;
  PlannerGlobal glob;
  PlannerInfo root;

                                                                   
  MemSet(&glob, 0, sizeof(glob));
  glob.type = T_PlannerGlobal;
  glob.relationOids = NIL;
  glob.invalItems = NIL;

  MemSet(&root, 0, sizeof(root));
  root.type = T_PlannerInfo;
  root.glob = &glob;

     
                                                                         
                                                                          
                              
     
  result = eval_const_expressions(&root, (Node *)expr);

                                          
  fix_opfuncids(result);

     
                                                                        
                                         
     
  (void)extract_query_dependencies_walker(result, &root);

  *relationOids = glob.relationOids;
  *invalItems = glob.invalItems;

  return (Expr *)result;
}

   
                         
                                                                   
   
                                                                        
                                                                      
                                                                             
                                                          
   
                                                                        
   
bool
plan_cluster_use_sort(Oid tableOid, Oid indexOid)
{
  PlannerInfo *root;
  Query *query;
  PlannerGlobal *glob;
  RangeTblEntry *rte;
  RelOptInfo *rel;
  IndexOptInfo *indexInfo;
  QualCost indexExprCost;
  Cost comparisonCost;
  Path *seqScanPath;
  Path seqScanAndSortPath;
  IndexPath *indexScanPath;
  ListCell *lc;

                                                                           
  if (!enable_indexscan)
  {
    return true;               
  }

                                         
  query = makeNode(Query);
  query->commandType = CMD_SELECT;

  glob = makeNode(PlannerGlobal);

  root = makeNode(PlannerInfo);
  root->parse = query;
  root->glob = glob;
  root->query_level = 1;
  root->planner_cxt = CurrentMemoryContext;
  root->wt_param_id = -1;

                                       
  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RELATION;
  rte->relid = tableOid;
  rte->relkind = RELKIND_RELATION;                          
  rte->rellockmode = AccessShareLock;
  rte->lateral = false;
  rte->inh = false;
  rte->inFromCl = true;
  query->rtable = list_make1(rte);

                                    
  setup_simple_rel_arrays(root);

                        
  rel = build_simple_rel(root, 1, NULL);

                                                
  indexInfo = NULL;
  foreach (lc, rel->indexlist)
  {
    indexInfo = lfirst_node(IndexOptInfo, lc);
    if (indexInfo->indexoid == indexOid)
    {
      break;
    }
  }

     
                                                                           
                                                                          
                                                                         
                                                                           
                                                        
     
  if (lc == NULL)                       
  {
    return true;               
  }

     
                                                                   
                                                                          
     
  rel->rows = rel->tuples;
  rel->reltarget->width = get_relation_data_width(tableOid, NULL);

  root->total_table_pages = rel->pages;

     
                                                                       
                                                                            
                                                                    
                                                                
     
  cost_qual_eval(&indexExprCost, indexInfo->indexprs, root);
  comparisonCost = 2.0 * (indexExprCost.startup + indexExprCost.per_tuple);

                                            
  seqScanPath = create_seqscan_path(root, rel, NULL, 0);
  cost_sort(&seqScanAndSortPath, root, NIL, seqScanPath->total_cost, rel->tuples, rel->reltarget->width, comparisonCost, maintenance_work_mem, -1.0);

                                       
  indexScanPath = create_index_path(root, indexInfo, NIL, NIL, NIL, NIL, ForwardScanDirection, false, NULL, 1.0, false);

  return (seqScanAndSortPath.total_cost < indexScanPath->path.total_cost);
}

   
                             
                                                                 
                                        
   
                                                                             
                                                                             
   
                                                                           
                                                                               
                                                                          
                      
   
                                                                           
          
   
int
plan_create_index_workers(Oid tableOid, Oid indexOid)
{
  PlannerInfo *root;
  Query *query;
  PlannerGlobal *glob;
  RangeTblEntry *rte;
  Relation heap;
  Relation index;
  RelOptInfo *rel;
  int parallel_workers;
  BlockNumber heap_blocks;
  double reltuples;
  double allvisfrac;

     
                                                                           
                                   
     
  if (!IsUnderPostmaster || max_parallel_maintenance_workers == 0)
  {
    return 0;
  }

                                          
  query = makeNode(Query);
  query->commandType = CMD_SELECT;

  glob = makeNode(PlannerGlobal);

  root = makeNode(PlannerInfo);
  root->parse = query;
  root->glob = glob;
  root->query_level = 1;
  root->planner_cxt = CurrentMemoryContext;
  root->wt_param_id = -1;

     
                          
     
                                                                
                                                                      
                                                                   
                         
     
  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RELATION;
  rte->relid = tableOid;
  rte->relkind = RELKIND_RELATION;                          
  rte->rellockmode = AccessShareLock;
  rte->lateral = false;
  rte->inh = true;
  rte->inFromCl = true;
  query->rtable = list_make1(rte);

                                    
  setup_simple_rel_arrays(root);

                        
  rel = build_simple_rel(root, 1, NULL);

                                                     
  heap = table_open(tableOid, NoLock);
  index = index_open(indexOid, NoLock);

     
                                        
     
                                                                             
                                                                            
           
     
  if (heap->rd_rel->relpersistence == RELPERSISTENCE_TEMP || !is_parallel_safe(root, (Node *)RelationGetIndexExpressions(index)) || !is_parallel_safe(root, (Node *)RelationGetIndexPredicate(index)))
  {
    parallel_workers = 0;
    goto done;
  }

     
                                                                             
                                                                            
                                                                             
                                                                           
                      
     
  if (rel->rel_parallel_workers != -1)
  {
    parallel_workers = Min(rel->rel_parallel_workers, max_parallel_maintenance_workers);
    goto done;
  }

     
                                                                       
                                                         
     
  estimate_rel_size(heap, NULL, &heap_blocks, &reltuples, &allvisfrac);

     
                                                                         
           
     
  parallel_workers = compute_parallel_worker(rel, heap_blocks, -1, max_parallel_maintenance_workers);

     
                                                                    
     
                                                                        
                                                                   
                                                                       
                                                                          
                                                                           
                              
     
  while (parallel_workers > 0 && maintenance_work_mem / (parallel_workers + 1) < 32768L)
  {
    parallel_workers--;
  }

done:
  index_close(index, NoLock);
  table_close(heap, NoLock);

  return parallel_workers;
}

   
                             
   
                                               
   
static void
add_paths_to_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, double dNumGroups, GroupPathExtraData *extra)
{
  Query *parse = root->parse;
  Path *cheapest_path = input_rel->cheapest_total_path;
  ListCell *lc;
  bool can_hash = (extra->flags & GROUPING_CAN_USE_HASH) != 0;
  bool can_sort = (extra->flags & GROUPING_CAN_USE_SORT) != 0;
  List *havingQual = (List *)extra->havingQual;
  AggClauseCosts *agg_final_costs = &extra->agg_final_costs;

  if (can_sort)
  {
       
                                                                          
                                        
       
    foreach (lc, input_rel->pathlist)
    {
      Path *path = (Path *)lfirst(lc);
      bool is_sorted;

      is_sorted = pathkeys_contained_in(root->group_pathkeys, path->pathkeys);
      if (path == cheapest_path || is_sorted)
      {
                                                                     
        if (!is_sorted)
        {
          path = (Path *)create_sort_path(root, grouped_rel, path, root->group_pathkeys, -1.0);
        }

                                              
        if (parse->groupingSets)
        {
          consider_groupingsets_paths(root, grouped_rel, path, true, can_hash, gd, agg_costs, dNumGroups);
        }
        else if (parse->hasAggs)
        {
             
                                                                     
                         
             
          add_path(grouped_rel, (Path *)create_agg_path(root, grouped_rel, path, grouped_rel->reltarget, parse->groupClause ? AGG_SORTED : AGG_PLAIN, AGGSPLIT_SIMPLE, parse->groupClause, havingQual, agg_costs, dNumGroups));
        }
        else if (parse->groupClause)
        {
             
                                                                    
                               
             
          add_path(grouped_rel, (Path *)create_group_path(root, grouped_rel, path, parse->groupClause, havingQual, dNumGroups));
        }
        else
        {
                                                          
          Assert(false);
        }
      }
    }

       
                                                                   
                                                        
       
    if (partially_grouped_rel != NULL)
    {
      foreach (lc, partially_grouped_rel->pathlist)
      {
        Path *path = (Path *)lfirst(lc);

           
                                                                     
                                                   
           
        if (!pathkeys_contained_in(root->group_pathkeys, path->pathkeys))
        {
          if (path != partially_grouped_rel->cheapest_total_path)
          {
            continue;
          }
          path = (Path *)create_sort_path(root, grouped_rel, path, root->group_pathkeys, -1.0);
        }

        if (parse->hasAggs)
        {
          add_path(grouped_rel, (Path *)create_agg_path(root, grouped_rel, path, grouped_rel->reltarget, parse->groupClause ? AGG_SORTED : AGG_PLAIN, AGGSPLIT_FINAL_DESERIAL, parse->groupClause, havingQual, agg_final_costs, dNumGroups));
        }
        else
        {
          add_path(grouped_rel, (Path *)create_group_path(root, grouped_rel, path, parse->groupClause, havingQual, dNumGroups));
        }
      }
    }
  }

  if (can_hash)
  {
    double hashaggtablesize;

    if (parse->groupingSets)
    {
         
                                                                    
         
      consider_groupingsets_paths(root, grouped_rel, cheapest_path, false, true, gd, agg_costs, dNumGroups);
    }
    else
    {
      hashaggtablesize = estimate_hashagg_tablesize(cheapest_path, agg_costs, dNumGroups);

         
                                                                    
                                                                        
                                                                         
                                    
         
      if (hashaggtablesize < work_mem * 1024L || grouped_rel->pathlist == NIL)
      {
           
                                                                   
                                           
           
        add_path(grouped_rel, (Path *)create_agg_path(root, grouped_rel, cheapest_path, grouped_rel->reltarget, AGG_HASHED, AGGSPLIT_SIMPLE, parse->groupClause, havingQual, agg_costs, dNumGroups));
      }
    }

       
                                                                       
                                                                           
                                                                   
       
    if (partially_grouped_rel && partially_grouped_rel->pathlist)
    {
      Path *path = partially_grouped_rel->cheapest_total_path;

      hashaggtablesize = estimate_hashagg_tablesize(path, agg_final_costs, dNumGroups);

      if (hashaggtablesize < work_mem * 1024L)
      {
        add_path(grouped_rel, (Path *)create_agg_path(root, grouped_rel, path, grouped_rel->reltarget, AGG_HASHED, AGGSPLIT_FINAL_DESERIAL, parse->groupClause, havingQual, agg_final_costs, dNumGroups));
      }
    }
  }

     
                                                                          
                                                                           
                                                                        
                                        
     
  if (grouped_rel->partial_pathlist != NIL)
  {
    gather_grouping_paths(root, grouped_rel);
  }
}

   
                                 
   
                                                                              
                                                                            
                                                                                
                                                                           
                                
   
                                                                            
                                                                             
         
   
                                                                              
                                            
   
static RelOptInfo *
create_partial_grouping_paths(PlannerInfo *root, RelOptInfo *grouped_rel, RelOptInfo *input_rel, grouping_sets_data *gd, GroupPathExtraData *extra, bool force_rel_creation)
{
  Query *parse = root->parse;
  RelOptInfo *partially_grouped_rel;
  AggClauseCosts *agg_partial_costs = &extra->agg_partial_costs;
  AggClauseCosts *agg_final_costs = &extra->agg_final_costs;
  Path *cheapest_partial_path = NULL;
  Path *cheapest_total_path = NULL;
  double dNumPartialGroups = 0;
  double dNumPartialPartialGroups = 0;
  ListCell *lc;
  bool can_hash = (extra->flags & GROUPING_CAN_USE_HASH) != 0;
  bool can_sort = (extra->flags & GROUPING_CAN_USE_SORT) != 0;

     
                                                                          
                                                                            
                                                                     
                                                                         
                                                                  
     
  if (input_rel->pathlist != NIL && extra->patype == PARTITIONWISE_AGGREGATE_PARTIAL)
  {
    cheapest_total_path = input_rel->cheapest_total_path;
  }

     
                                                                         
                                                                            
                                          
     
  if (grouped_rel->consider_parallel && input_rel->partial_pathlist != NIL)
  {
    cheapest_partial_path = linitial(input_rel->partial_pathlist);
  }

     
                                                                           
                                                                     
                                                                        
     
  if (cheapest_total_path == NULL && cheapest_partial_path == NULL && !force_rel_creation)
  {
    return NULL;
  }

     
                                                                     
                                                   
     
  partially_grouped_rel = fetch_upper_rel(root, UPPERREL_PARTIAL_GROUP_AGG, grouped_rel->relids);
  partially_grouped_rel->consider_parallel = grouped_rel->consider_parallel;
  partially_grouped_rel->reloptkind = grouped_rel->reloptkind;
  partially_grouped_rel->serverid = grouped_rel->serverid;
  partially_grouped_rel->userid = grouped_rel->userid;
  partially_grouped_rel->useridiscurrent = grouped_rel->useridiscurrent;
  partially_grouped_rel->fdwroutine = grouped_rel->fdwroutine;

     
                                                                             
                                                                         
                                                                          
                                                                        
     
  partially_grouped_rel->reltarget = make_partial_grouping_target(root, grouped_rel->reltarget, extra->havingQual);

  if (!extra->partial_costs_set)
  {
       
                                                                   
                                           
       
    MemSet(agg_partial_costs, 0, sizeof(AggClauseCosts));
    MemSet(agg_final_costs, 0, sizeof(AggClauseCosts));
    if (parse->hasAggs)
    {
      List *partial_target_exprs;

                         
      partial_target_exprs = partially_grouped_rel->reltarget->exprs;
      get_agg_clause_costs(root, (Node *)partial_target_exprs, AGGSPLIT_INITIAL_SERIAL, agg_partial_costs);

                       
      get_agg_clause_costs(root, (Node *)grouped_rel->reltarget->exprs, AGGSPLIT_FINAL_DESERIAL, agg_final_costs);
      get_agg_clause_costs(root, extra->havingQual, AGGSPLIT_FINAL_DESERIAL, agg_final_costs);
    }

    extra->partial_costs_set = true;
  }

                                          
  if (cheapest_total_path != NULL)
  {
    dNumPartialGroups = get_number_of_groups(root, cheapest_total_path->rows, gd, extra->targetList);
  }
  if (cheapest_partial_path != NULL)
  {
    dNumPartialPartialGroups = get_number_of_groups(root, cheapest_partial_path->rows, gd, extra->targetList);
  }

  if (can_sort && cheapest_total_path != NULL)
  {
                                                  
    Assert(parse->hasAggs || parse->groupClause);

       
                                                                          
                                          
       
    foreach (lc, input_rel->pathlist)
    {
      Path *path = (Path *)lfirst(lc);
      bool is_sorted;

      is_sorted = pathkeys_contained_in(root->group_pathkeys, path->pathkeys);
      if (path == cheapest_total_path || is_sorted)
      {
                                                                 
        if (!is_sorted)
        {
          path = (Path *)create_sort_path(root, partially_grouped_rel, path, root->group_pathkeys, -1.0);
        }

        if (parse->hasAggs)
        {
          add_path(partially_grouped_rel, (Path *)create_agg_path(root, partially_grouped_rel, path, partially_grouped_rel->reltarget, parse->groupClause ? AGG_SORTED : AGG_PLAIN, AGGSPLIT_INITIAL_SERIAL, parse->groupClause, NIL, agg_partial_costs, dNumPartialGroups));
        }
        else
        {
          add_path(partially_grouped_rel, (Path *)create_group_path(root, partially_grouped_rel, path, parse->groupClause, NIL, dNumPartialGroups));
        }
      }
    }
  }

  if (can_sort && cheapest_partial_path != NULL)
  {
                                                        
    foreach (lc, input_rel->partial_pathlist)
    {
      Path *path = (Path *)lfirst(lc);
      bool is_sorted;

      is_sorted = pathkeys_contained_in(root->group_pathkeys, path->pathkeys);
      if (path == cheapest_partial_path || is_sorted)
      {
                                                                 
        if (!is_sorted)
        {
          path = (Path *)create_sort_path(root, partially_grouped_rel, path, root->group_pathkeys, -1.0);
        }

        if (parse->hasAggs)
        {
          add_partial_path(partially_grouped_rel, (Path *)create_agg_path(root, partially_grouped_rel, path, partially_grouped_rel->reltarget, parse->groupClause ? AGG_SORTED : AGG_PLAIN, AGGSPLIT_INITIAL_SERIAL, parse->groupClause, NIL, agg_partial_costs, dNumPartialPartialGroups));
        }
        else
        {
          add_partial_path(partially_grouped_rel, (Path *)create_group_path(root, partially_grouped_rel, path, parse->groupClause, NIL, dNumPartialPartialGroups));
        }
      }
    }
  }

  if (can_hash && cheapest_total_path != NULL)
  {
    double hashaggtablesize;

                       
    Assert(parse->hasAggs || parse->groupClause);

    hashaggtablesize = estimate_hashagg_tablesize(cheapest_total_path, agg_partial_costs, dNumPartialGroups);

       
                                                                      
                                                        
       
    if (hashaggtablesize < work_mem * 1024L && cheapest_total_path != NULL)
    {
      add_path(partially_grouped_rel, (Path *)create_agg_path(root, partially_grouped_rel, cheapest_total_path, partially_grouped_rel->reltarget, AGG_HASHED, AGGSPLIT_INITIAL_SERIAL, parse->groupClause, NIL, agg_partial_costs, dNumPartialGroups));
    }
  }

  if (can_hash && cheapest_partial_path != NULL)
  {
    double hashaggtablesize;

    hashaggtablesize = estimate_hashagg_tablesize(cheapest_partial_path, agg_partial_costs, dNumPartialPartialGroups);

                                        
    if (hashaggtablesize < work_mem * 1024L && cheapest_partial_path != NULL)
    {
      add_partial_path(partially_grouped_rel, (Path *)create_agg_path(root, partially_grouped_rel, cheapest_partial_path, partially_grouped_rel->reltarget, AGG_HASHED, AGGSPLIT_INITIAL_SERIAL, parse->groupClause, NIL, agg_partial_costs, dNumPartialPartialGroups));
    }
  }

     
                                                                          
                                                            
     
  if (partially_grouped_rel->fdwroutine && partially_grouped_rel->fdwroutine->GetForeignUpperPaths)
  {
    FdwRoutine *fdwroutine = partially_grouped_rel->fdwroutine;

    fdwroutine->GetForeignUpperPaths(root, UPPERREL_PARTIAL_GROUP_AGG, input_rel, partially_grouped_rel, extra);
  }

  return partially_grouped_rel;
}

   
                                                                             
                      
   
                                                                               
                                                                               
                 
   
                                                                            
                                                                              
                                                                         
                            
   
static void
gather_grouping_paths(PlannerInfo *root, RelOptInfo *rel)
{
  Path *cheapest_partial_path;

                                                                         
  generate_gather_paths(root, rel, true);

                                                                 
  cheapest_partial_path = linitial(rel->partial_pathlist);
  if (!pathkeys_contained_in(root->group_pathkeys, cheapest_partial_path->pathkeys))
  {
    Path *path;
    double total_groups;

    total_groups = cheapest_partial_path->rows * cheapest_partial_path->parallel_workers;
    path = (Path *)create_sort_path(root, rel, cheapest_partial_path, root->group_pathkeys, -1.0);
    path = (Path *)create_gather_merge_path(root, rel, path, rel->reltarget, root->group_pathkeys, NULL, &total_groups);

    add_path(rel, path);
  }
}

   
                   
   
                                                                              
                                                
   
static bool
can_partial_agg(PlannerInfo *root, const AggClauseCosts *agg_costs)
{
  Query *parse = root->parse;

  if (!parse->hasAggs && parse->groupClause == NIL)
  {
       
                                                                          
                                             
       
    return false;
  }
  else if (parse->groupingSets)
  {
                                                            
    return false;
  }
  else if (agg_costs->hasNonPartial || agg_costs->hasNonSerial)
  {
                                                
    return false;
  }

                              
  return true;
}

   
                                  
   
                                                                             
                                                                              
                                                                              
                                                                         
                                                                           
   
                                                                            
                                                                              
                                                                      
                                                                               
   
static void
apply_scanjoin_target_to_paths(PlannerInfo *root, RelOptInfo *rel, List *scanjoin_targets, List *scanjoin_targets_contain_srfs, bool scanjoin_target_parallel_safe, bool tlist_same_exprs)
{
  bool rel_is_partitioned = IS_PARTITIONED_REL(rel);
  PathTarget *scanjoin_target;
  ListCell *lc;

                                      
  check_stack_depth();

     
                                                                       
                                                                             
                                                                           
                                                                         
                                                                           
                                                                           
                                                                             
                                                                            
                                                                     
                                                                             
                                                                         
     
                                                                            
                                                                        
                                                                           
                                                      
     
  if (rel_is_partitioned)
  {
    rel->pathlist = NIL;
  }

     
                                                                        
                  
     
  if (!scanjoin_target_parallel_safe)
  {
       
                                                                      
                                                                           
                                                                          
                                                                     
                                                                           
                                                                   
                
       
    generate_gather_paths(root, rel, false);

                                                    
    rel->partial_pathlist = NIL;
    rel->consider_parallel = false;
  }

                                                                          
  if (rel_is_partitioned)
  {
    rel->partial_pathlist = NIL;
  }

                                          
  scanjoin_target = linitial_node(PathTarget, scanjoin_targets);

     
                                                                
     
                                                                          
                                                                         
                                                                       
                                                                            
                                          
     
  foreach (lc, rel->pathlist)
  {
    Path *subpath = (Path *)lfirst(lc);

                                                        
    Assert(subpath->param_info == NULL);

    if (tlist_same_exprs)
    {
      subpath->pathtarget->sortgrouprefs = scanjoin_target->sortgrouprefs;
    }
    else
    {
      Path *newpath;

      newpath = (Path *)create_projection_path(root, rel, subpath, scanjoin_target);
      lfirst(lc) = newpath;
    }
  }

                                                          
  foreach (lc, rel->partial_pathlist)
  {
    Path *subpath = (Path *)lfirst(lc);

                                                        
    Assert(subpath->param_info == NULL);

    if (tlist_same_exprs)
    {
      subpath->pathtarget->sortgrouprefs = scanjoin_target->sortgrouprefs;
    }
    else
    {
      Path *newpath;

      newpath = (Path *)create_projection_path(root, rel, subpath, scanjoin_target);
      lfirst(lc) = newpath;
    }
  }

     
                                                                            
                                                                            
                                                                             
           
     
  if (root->parse->hasTargetSRFs)
  {
    adjust_paths_for_srfs(root, rel, scanjoin_targets, scanjoin_targets_contain_srfs);
  }

     
                                                                           
                                                                           
                                                                           
                                                                      
                                          
     
                                                                            
                                                                          
                                                                
     
  rel->reltarget = llast_node(PathTarget, scanjoin_targets);

     
                                                                            
                                                                         
                                                                         
                                                                        
                                                                        
     
  if (rel_is_partitioned)
  {
    List *live_children = NIL;
    int partition_idx;

                                
    for (partition_idx = 0; partition_idx < rel->nparts; partition_idx++)
    {
      RelOptInfo *child_rel = rel->part_rels[partition_idx];
      AppendRelInfo **appinfos;
      int nappinfos;
      List *child_scanjoin_targets = NIL;
      ListCell *lc;

                                                    
      if (child_rel == NULL || IS_DUMMY_REL(child_rel))
      {
        continue;
      }

                                                       
      appinfos = find_appinfos_by_relids(root, child_rel->relids, &nappinfos);
      foreach (lc, scanjoin_targets)
      {
        PathTarget *target = lfirst_node(PathTarget, lc);

        target = copy_pathtarget(target);
        target->exprs = (List *)adjust_appendrel_attrs(root, (Node *)target->exprs, nappinfos, appinfos);
        child_scanjoin_targets = lappend(child_scanjoin_targets, target);
      }
      pfree(appinfos);

                                         
      apply_scanjoin_target_to_paths(root, child_rel, child_scanjoin_targets, scanjoin_targets_contain_srfs, scanjoin_target_parallel_safe, tlist_same_exprs);

                                                     
      if (!IS_DUMMY_REL(child_rel))
      {
        live_children = lappend(live_children, child_rel);
      }
    }

                                                                     
    add_paths_to_append_rel(root, rel, live_children);
  }

     
                                                                             
                                                                            
                                                                           
                                                                             
                                                                     
     
  if (rel->consider_parallel && !IS_OTHER_REL(rel))
  {
    generate_gather_paths(root, rel, false);
  }

     
                                                                             
                                                                          
                    
     
  set_cheapest(rel);
}

   
                                       
   
                                                                                
                                                                           
                                                                             
                                                                              
                                           
   
                                                                            
                                                                             
                                                                            
                                                                        
                                                                            
                                                                               
                                                
   
static void
create_partitionwise_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, PartitionwiseAggregateType patype, GroupPathExtraData *extra)
{
  int nparts = input_rel->nparts;
  int cnt_parts;
  List *grouped_live_children = NIL;
  List *partially_grouped_live_children = NIL;
  PathTarget *target = grouped_rel->reltarget;
  bool partial_grouping_valid = true;

  Assert(patype != PARTITIONWISE_AGGREGATE_NONE);
  Assert(patype != PARTITIONWISE_AGGREGATE_PARTIAL || partially_grouped_rel != NULL);

                                                         
  for (cnt_parts = 0; cnt_parts < nparts; cnt_parts++)
  {
    RelOptInfo *child_input_rel = input_rel->part_rels[cnt_parts];
    PathTarget *child_target = copy_pathtarget(target);
    AppendRelInfo **appinfos;
    int nappinfos;
    GroupPathExtraData child_extra;
    RelOptInfo *child_grouped_rel;
    RelOptInfo *child_partially_grouped_rel;

                                                  
    if (child_input_rel == NULL || IS_DUMMY_REL(child_input_rel))
    {
      continue;
    }

       
                                                                    
                                       
       
    memcpy(&child_extra, extra, sizeof(child_extra));

    appinfos = find_appinfos_by_relids(root, child_input_rel->relids, &nappinfos);

    child_target->exprs = (List *)adjust_appendrel_attrs(root, (Node *)target->exprs, nappinfos, appinfos);

                                              
    child_extra.havingQual = (Node *)adjust_appendrel_attrs(root, extra->havingQual, nappinfos, appinfos);
    child_extra.targetList = (List *)adjust_appendrel_attrs(root, (Node *)extra->targetList, nappinfos, appinfos);

       
                                                                          
                                                                     
                           
       
    child_extra.patype = patype;

       
                                                                         
                                        
       
    child_grouped_rel = make_grouping_rel(root, child_input_rel, child_target, extra->target_parallel_safe, child_extra.havingQual);

                                                        
    create_ordinary_grouping_paths(root, child_input_rel, child_grouped_rel, agg_costs, gd, &child_extra, &child_partially_grouped_rel);

    if (child_partially_grouped_rel)
    {
      partially_grouped_live_children = lappend(partially_grouped_live_children, child_partially_grouped_rel);
    }
    else
    {
      partial_grouping_valid = false;
    }

    if (patype == PARTITIONWISE_AGGREGATE_FULL)
    {
      set_cheapest(child_grouped_rel);
      grouped_live_children = lappend(grouped_live_children, child_grouped_rel);
    }

    pfree(appinfos);
  }

     
                                                                         
                                                                            
                                                                     
                                                                           
     
                                                                           
                                                          
     
  if (partially_grouped_rel && partial_grouping_valid)
  {
    Assert(partially_grouped_live_children != NIL);

    add_paths_to_append_rel(root, partially_grouped_rel, partially_grouped_live_children);

       
                                                                           
                                   
       
    if (partially_grouped_rel->pathlist)
    {
      set_cheapest(partially_grouped_rel);
    }
  }

                                                                    
  if (patype == PARTITIONWISE_AGGREGATE_FULL)
  {
    Assert(grouped_live_children != NIL);

    add_paths_to_append_rel(root, grouped_rel, grouped_live_children);
  }
}

   
                        
   
                                                                             
                                          
   
static bool
group_by_has_partkey(RelOptInfo *input_rel, List *targetList, List *groupClause)
{
  List *groupexprs = get_sortgrouplist_exprs(groupClause, targetList);
  int cnt = 0;
  int partnatts;

                                             
  Assert(input_rel->part_scheme);

                                                               
  if (!input_rel->partexprs)
  {
    return false;
  }

  partnatts = input_rel->part_scheme->partnatts;

  for (cnt = 0; cnt < partnatts; cnt++)
  {
    List *partexprs = input_rel->partexprs[cnt];
    ListCell *lc;
    bool found = false;

    foreach (lc, partexprs)
    {
      Expr *partexpr = lfirst(lc);

      if (list_member(groupexprs, partexpr))
      {
        found = true;
        break;
      }
    }

       
                                                                      
                                          
       
    if (!found)
    {
      return false;
    }
  }

  return true;
}
