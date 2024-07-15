                                                                            
   
              
                                                                   
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/sysattr.h"
#include "access/tsmapi.h"
#include "catalog/pg_class.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/geqo.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/planner.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "partitioning/partbounds.h"
#include "partitioning/partprune.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"

                                          
typedef struct pushdown_safety_info
{
  bool *unsafeColumns;                                             
  bool unsafeVolatile;                                     
  bool unsafeLeaky;                                     
} pushdown_safety_info;

                                     
bool enable_geqo = false;                                      
int geqo_threshold;
int min_parallel_table_scan_size;
int min_parallel_index_scan_size;

                                                           
set_rel_pathlist_hook_type set_rel_pathlist_hook = NULL;

                                                        
join_search_hook_type join_search_hook = NULL;

static void
set_base_rel_consider_startup(PlannerInfo *root);
static void
set_base_rel_sizes(PlannerInfo *root);
static void
set_base_rel_pathlists(PlannerInfo *root);
static void
set_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_plain_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
create_plain_partial_paths(PlannerInfo *root, RelOptInfo *rel);
static void
set_rel_consider_parallel(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_plain_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_tablesample_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_tablesample_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_foreign_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_foreign_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_append_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_append_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
generate_orderedappend_paths(PlannerInfo *root, RelOptInfo *rel, List *live_childrels, List *all_child_pathkeys, List *partitioned_rels);
static Path *
get_cheapest_parameterized_child_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer);
static void
accumulate_append_subpath(Path *path, List **subpaths, List **special_subpaths);
static Path *
get_singleton_append_subpath(Path *path);
static void
set_dummy_rel_pathlist(RelOptInfo *rel);
static void
set_subquery_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_function_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_values_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_tablefunc_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_cte_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_namedtuplestore_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_result_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_worktable_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static RelOptInfo *
make_rel_from_joinlist(PlannerInfo *root, List *joinlist);
static bool
subquery_is_pushdown_safe(Query *subquery, Query *topquery, pushdown_safety_info *safetyInfo);
static bool
recurse_pushdown_safe(Node *setOp, Query *topquery, pushdown_safety_info *safetyInfo);
static void
check_output_expressions(Query *subquery, pushdown_safety_info *safetyInfo);
static void
compare_tlist_datatypes(List *tlist, List *colTypes, pushdown_safety_info *safetyInfo);
static bool
targetIsInAllPartitionLists(TargetEntry *tle, Query *query);
static bool
qual_is_pushdown_safe(Query *subquery, Index rti, Node *qual, pushdown_safety_info *safetyInfo);
static void
subquery_push_qual(Query *subquery, RangeTblEntry *rte, Index rti, Node *qual);
static void
recurse_push_qual(Node *setOp, Query *topquery, RangeTblEntry *rte, Index rti, Node *qual);
static void
remove_unused_subquery_outputs(Query *subquery, RelOptInfo *rel);

   
                
                                                                        
                                                                        
   
RelOptInfo *
make_one_rel(PlannerInfo *root, List *joinlist)
{
  RelOptInfo *rel;
  Index rti;
  double total_pages;

     
                                            
     
  root->all_baserels = NULL;
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];

                                                                    
    if (brel == NULL)
    {
      continue;
    }

    Assert(brel->relid == rti);                            

                                           
    if (brel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

    root->all_baserels = bms_add_member(root->all_baserels, brel->relid);
  }

                                                                   
  set_base_rel_consider_startup(root);

     
                                                                           
     
  set_base_rel_sizes(root);

     
                                                                          
                                                                         
                                                                          
                                                                     
     
                                                                            
                                                                          
                                  
     
                                                                          
                                                                           
                                                                       
     
  total_pages = 0;
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];

    if (brel == NULL)
    {
      continue;
    }

    Assert(brel->relid == rti);                            

    if (IS_DUMMY_REL(brel))
    {
      continue;
    }

    if (IS_SIMPLE_REL(brel))
    {
      total_pages += (double)brel->pages;
    }
  }
  root->total_table_pages = total_pages;

     
                                              
     
  set_base_rel_pathlists(root);

     
                                                     
     
  rel = make_rel_from_joinlist(root, joinlist);

     
                                                                
     
  Assert(bms_equal(rel->relids, root->all_baserels));

  return rel;
}

   
                                 
                                                                          
   
                                                                              
                                                                               
                                                                              
                                                                           
                              
   
static void
set_base_rel_consider_startup(PlannerInfo *root)
{
     
                                                                            
                                                                        
                                                                           
                                                                            
                                              
     
                                                                        
                                                                            
                                                                         
                                                                           
                                                               
     
  ListCell *lc;

  foreach (lc, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(lc);
    int varno;

    if ((sjinfo->jointype == JOIN_SEMI || sjinfo->jointype == JOIN_ANTI) && bms_get_singleton_member(sjinfo->syn_righthand, &varno))
    {
      RelOptInfo *rel = find_base_rel(root, varno);

      rel->consider_param_startup = true;
    }
  }
}

   
                      
                                                                            
                                                                           
   
                                                                     
                                                                          
                                                                              
                   
   
static void
set_base_rel_sizes(PlannerInfo *root)
{
  Index rti;

  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *rel = root->simple_rel_array[rti];
    RangeTblEntry *rte;

                                                                    
    if (rel == NULL)
    {
      continue;
    }

    Assert(rel->relid == rti);                            

                                           
    if (rel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

    rte = root->simple_rte_array[rti];

       
                                                                          
                                                                      
                                                                        
                                                                           
                                                                          
                                               
       
    if (root->glob->parallelModeOK)
    {
      set_rel_consider_parallel(root, rel, rte);
    }

    set_rel_size(root, rel, rti, rte);
  }
}

   
                          
                                                                      
                                                               
                                                                      
   
static void
set_base_rel_pathlists(PlannerInfo *root)
{
  Index rti;

  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *rel = root->simple_rel_array[rti];

                                                                    
    if (rel == NULL)
    {
      continue;
    }

    Assert(rel->relid == rti);                            

                                           
    if (rel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

    set_rel_pathlist(root, rel, rti, root->simple_rte_array[rti]);
  }
}

   
                
                                            
   
static void
set_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{
  if (rel->reloptkind == RELOPT_BASEREL && relation_excluded_by_constraints(root, rel, rte))
  {
       
                                                                         
                                                                          
                                                                        
                              
       
                                                                           
                                                                          
                                                                       
                                     
       
    set_dummy_rel_pathlist(rel);
  }
  else if (rte->inh)
  {
                                                        
    set_append_rel_size(root, rel, rti, rte);
  }
  else
  {
    switch (rel->rtekind)
    {
    case RTE_RELATION:
      if (rte->relkind == RELKIND_FOREIGN_TABLE)
      {
                           
        set_foreign_size(root, rel, rte);
      }
      else if (rte->relkind == RELKIND_PARTITIONED_TABLE)
      {
           
                                                                  
                                                                 
                                                  
           
        set_dummy_rel_pathlist(rel);
      }
      else if (rte->tablesample != NULL)
      {
                              
        set_tablesample_rel_size(root, rel, rte);
      }
      else
      {
                            
        set_plain_rel_size(root, rel, rte);
      }
      break;
    case RTE_SUBQUERY:

         
                                                          
                                                                   
                                            
         
      set_subquery_pathlist(root, rel, rti, rte);
      break;
    case RTE_FUNCTION:
      set_function_size_estimates(root, rel);
      break;
    case RTE_TABLEFUNC:
      set_tablefunc_size_estimates(root, rel);
      break;
    case RTE_VALUES:
      set_values_size_estimates(root, rel);
      break;
    case RTE_CTE:

         
                                                                  
                                                                     
                            
         
      if (rte->self_reference)
      {
        set_worktable_pathlist(root, rel, rte);
      }
      else
      {
        set_cte_pathlist(root, rel, rte);
      }
      break;
    case RTE_NAMEDTUPLESTORE:
                                                         
      set_namedtuplestore_pathlist(root, rel, rte);
      break;
    case RTE_RESULT:
                                                         
      set_result_pathlist(root, rel, rte);
      break;
    default:
      elog(ERROR, "unexpected rtekind: %d", (int)rel->rtekind);
      break;
    }
  }

     
                                                                         
     
  Assert(rel->rows > 0 || IS_DUMMY_REL(rel));
}

   
                    
                                            
   
static void
set_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{
  if (IS_DUMMY_REL(rel))
  {
                                                                     
  }
  else if (rte->inh)
  {
                                                        
    set_append_rel_pathlist(root, rel, rti, rte);
  }
  else
  {
    switch (rel->rtekind)
    {
    case RTE_RELATION:
      if (rte->relkind == RELKIND_FOREIGN_TABLE)
      {
                           
        set_foreign_pathlist(root, rel, rte);
      }
      else if (rte->tablesample != NULL)
      {
                              
        set_tablesample_rel_pathlist(root, rel, rte);
      }
      else
      {
                            
        set_plain_rel_pathlist(root, rel, rte);
      }
      break;
    case RTE_SUBQUERY:
                                                          
      break;
    case RTE_FUNCTION:
                         
      set_function_pathlist(root, rel, rte);
      break;
    case RTE_TABLEFUNC:
                          
      set_tablefunc_pathlist(root, rel, rte);
      break;
    case RTE_VALUES:
                       
      set_values_pathlist(root, rel, rte);
      break;
    case RTE_CTE:
                                                               
      break;
    case RTE_NAMEDTUPLESTORE:
                                                                      
      break;
    case RTE_RESULT:
                                                               
      break;
    default:
      elog(ERROR, "unexpected rtekind: %d", (int)rel->rtekind);
      break;
    }
  }

     
                                                                      
                                                                        
                                                                         
                                                    
     
  if (set_rel_pathlist_hook)
  {
    (*set_rel_pathlist_hook)(root, rel, rti, rte);
  }

     
                                                                             
                                                                             
                                                                            
           
     
                                                                             
                                                                          
                                                                         
                                     
     
                                                                             
                                                                             
                             
     
  if (rel->reloptkind == RELOPT_BASEREL && bms_membership(root->all_baserels) != BMS_SINGLETON)
  {
    generate_gather_paths(root, rel, false);
  }

                                                       
  set_cheapest(rel);

#ifdef OPTIMIZER_DEBUG
  debug_print_rel(root, rel);
#endif
}

   
                      
                                                                           
   
static void
set_plain_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
     
                                                                         
                                                                   
     
  check_index_predicates(root, rel);

                                                       
  set_baserel_size_estimates(root, rel);
}

   
                                                                             
                               
   
static void
set_rel_consider_parallel(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
     
                                                                       
                                                             
     
  Assert(!rel->consider_parallel);

                                                                          
  Assert(root->glob->parallelModeOK);

                                                                       
  Assert(IS_SIMPLE_REL(rel));

                                         
  switch (rte->rtekind)
  {
  case RTE_RELATION:

       
                                                                       
                                                                    
                                                                   
                                                                     
                                                                   
                                                                       
                                                                      
                                                      
       
    if (get_rel_persistence(rte->relid) == RELPERSISTENCE_TEMP)
    {
      return;
    }

       
                                                                  
                                            
       
    if (rte->tablesample != NULL)
    {
      char proparallel = func_parallel(rte->tablesample->tsmhandler);

      if (proparallel != PROPARALLEL_SAFE)
      {
        return;
      }
      if (!is_parallel_safe(root, (Node *)rte->tablesample->args))
      {
        return;
      }
    }

       
                                                                  
                                                                 
                                                                     
                                                                       
                                                                      
                                                                    
       
    if (rte->relkind == RELKIND_FOREIGN_TABLE)
    {
      Assert(rel->fdwroutine);
      if (!rel->fdwroutine->IsForeignScanParallelSafe)
      {
        return;
      }
      if (!rel->fdwroutine->IsForeignScanParallelSafe(root, rel, rte))
      {
        return;
      }
    }

       
                                                                       
                                                                     
                                                                  
                             
       
    break;

  case RTE_SUBQUERY:

       
                                                                     
                                                                      
                                                                       
                                                                    
                                                           
                                                                      
                                                                     
                                                                    
                                                              
                                                                   
                                                                    
       
                                                                      
                                                              
                                                             
                                                                     
                                                                       
                                                                      
       
    {
      Query *subquery = castNode(Query, rte->subquery);

      if (limit_needed(subquery))
      {
        return;
      }
    }
    break;

  case RTE_JOIN:
                                                                 
    Assert(false);
    return;

  case RTE_FUNCTION:
                                                  
    if (!is_parallel_safe(root, (Node *)rte->functions))
    {
      return;
    }
    break;

  case RTE_TABLEFUNC:
                           
    return;

  case RTE_VALUES:
                                                  
    if (!is_parallel_safe(root, (Node *)rte->values_lists))
    {
      return;
    }
    break;

  case RTE_CTE:

       
                                                                   
                                                                      
                                                                      
                                                                 
                           
       
    return;

  case RTE_NAMEDTUPLESTORE:

       
                                                          
                                       
       
    return;

  case RTE_RESULT:
                                                     
    break;
  }

     
                                                                            
                                                                          
                                                                        
                                                                         
                                                                            
                                                                           
                   
     
  if (!is_parallel_safe(root, (Node *)rel->baserestrictinfo))
  {
    return;
  }

     
                                                                         
                                                              
     
  if (!is_parallel_safe(root, (Node *)rel->reltarget->exprs))
  {
    return;
  }

                         
  rel->consider_parallel = true;
}

   
                          
                                                                           
   
static void
set_plain_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;

     
                                                                            
                                                                          
                
     
  required_outer = rel->lateral_relids;

                                
  add_path(rel, create_seqscan_path(root, rel, required_outer, 0));

                                                         
  if (rel->consider_parallel && required_outer == NULL)
  {
    create_plain_partial_paths(root, rel);
  }

                            
  create_index_paths(root, rel);

                          
  create_tidscan_paths(root, rel);
}

   
                              
                                                                      
   
static void
create_plain_partial_paths(PlannerInfo *root, RelOptInfo *rel)
{
  int parallel_workers;

  parallel_workers = compute_parallel_worker(rel, rel->pages, -1, max_parallel_workers_per_gather);

                                                                            
  if (parallel_workers <= 0)
  {
    return;
  }

                                                                          
  add_partial_path(rel, create_seqscan_path(root, rel, NULL, parallel_workers));
}

   
                            
                                               
   
static void
set_tablesample_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  TableSampleClause *tsc = rte->tablesample;
  TsmRoutine *tsm;
  BlockNumber pages;
  double tuples;

     
                                                                         
                                                                   
     
  check_index_predicates(root, rel);

     
                                                                           
                                                                            
                                                  
     
  tsm = GetTsmRoutine(tsc->tsmhandler);
  tsm->SampleScanGetSampleSize(root, rel, tsc->args, &pages, &tuples);

     
                                                                             
                                                                             
                                                                          
                                         
     
  rel->pages = pages;
  rel->tuples = tuples;

                                                       
  set_baserel_size_estimates(root, rel);
}

   
                                
                                               
   
static void
set_tablesample_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;
  Path *path;

     
                                                                           
                                                                           
                                            
     
  required_outer = rel->lateral_relids;

                             
  path = create_samplescan_path(root, rel, required_outer);

     
                                                                             
                                                                         
                                                                             
                                                                            
                                                                             
                                                                          
                                                                           
                                                                       
                                                                            
                                                                             
                                  
     
                                                                             
                                                                          
     
  if ((root->query_level > 1 || bms_membership(root->all_baserels) != BMS_SINGLETON) && !(GetTsmRoutine(rte->tablesample->tsmhandler)->repeatable_across_scans))
  {
    path = (Path *)create_material_path(rel, path);
  }

  add_path(rel, path);

                                                                      
}

   
                    
                                               
   
static void
set_foreign_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
                                                       
  set_foreign_size_estimates(root, rel);

                                                    
  rel->fdwroutine->GetForeignRelSize(root, rel, rte->relid);

                                                           
  rel->rows = clamp_row_est(rel->rows);

                                                                       
  rel->tuples = Max(rel->tuples, rel->rows);
}

   
                        
                                               
   
static void
set_foreign_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
                                                                   
  rel->fdwroutine->GetForeignPaths(root, rel, rte->relid);
}

   
                       
                                                       
   
                                                                        
                                                                            
                                                                              
                                                                              
                                                                             
                                                             
   
static void
set_append_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{
  int parentRTindex = rti;
  bool has_live_children;
  double parent_rows;
  double parent_size;
  double *parent_attrsizes;
  int nattrs;
  ListCell *l;

                                                                         
  check_stack_depth();

  Assert(IS_SIMPLE_REL(rel));

     
                                                                 
     
                                                                             
                                                                           
                                                                  
                                                             
     
  if (rte->relkind == RELKIND_PARTITIONED_TABLE)
  {
    rel->partitioned_child_rels = list_make1_int(rti);
  }

     
                                                                           
                                                                            
                                                        
     
  if (enable_partitionwise_join && rel->reloptkind == RELOPT_BASEREL && rte->relkind == RELKIND_PARTITIONED_TABLE && rel->attr_needed[InvalidAttrNumber - rel->min_attr] == NULL)
  {
    rel->consider_partitionwise_join = true;
  }

     
                                                                     
     
                                                                          
                                                                            
                                                                        
                                                                        
                                                                           
                                                                         
                               
     
                                                                             
                                                                        
     
  has_live_children = false;
  parent_rows = 0;
  parent_size = 0;
  nattrs = rel->max_attr - rel->min_attr + 1;
  parent_attrsizes = (double *)palloc0(nattrs * sizeof(double));

  foreach (l, root->append_rel_list)
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)lfirst(l);
    int childRTindex;
    RangeTblEntry *childRTE;
    RelOptInfo *childrel;
    ListCell *parentvars;
    ListCell *childvars;

                                                                 
    if (appinfo->parent_relid != parentRTindex)
    {
      continue;
    }

    childRTindex = appinfo->child_relid;
    childRTE = root->simple_rte_array[childRTindex];

       
                                                             
                                
       
    childrel = find_base_rel(root, childRTindex);
    Assert(childrel->reloptkind == RELOPT_OTHER_MEMBER_REL);

                                                           
    if (IS_DUMMY_REL(childrel))
    {
      continue;
    }

       
                                                                       
                                                                 
                                                                       
                                                                          
                                             
       
    if (relation_excluded_by_constraints(root, childrel, childRTE))
    {
         
                                                                    
                    
         
      set_dummy_rel_pathlist(childrel);
      continue;
    }

       
                                                                        
                                                                         
       
                                                                          
                                                                           
                                                                       
                                                                        
                                                                           
                                                                         
       
    childrel->joininfo = (List *)adjust_appendrel_attrs(root, (Node *)rel->joininfo, 1, &appinfo);
    childrel->reltarget->exprs = (List *)adjust_appendrel_attrs(root, (Node *)rel->reltarget->exprs, 1, &appinfo);

       
                                                                  
                                                                
                                                                           
                                                                          
                                                                       
                                                 
       
    if (rel->has_eclass_joins || has_useful_pathkeys(root, rel))
    {
      add_child_rel_equivalences(root, appinfo, rel, childrel);
    }
    childrel->has_eclass_joins = rel->has_eclass_joins;

       
                                                                           
                                                                       
                                                                    
                                                                   
                                                                   
       

       
                                                                           
                                   
       
                                                                           
                                                                           
                                                                        
                                                                        
                                                                    
                                                     
       
    if (rel->consider_partitionwise_join)
    {
      childrel->consider_partitionwise_join = true;
    }

       
                                                                          
                                                                     
                                                                      
                                                                        
                                                                         
       
    if (root->glob->parallelModeOK && rel->consider_parallel)
    {
      set_rel_consider_parallel(root, childrel, childRTE);
    }

       
                                 
       
    set_rel_size(root, childrel, childRTindex, childRTE);

       
                                                                         
                                                                          
                                   
       
    if (IS_DUMMY_REL(childrel))
    {
      continue;
    }

                                          
    has_live_children = true;

       
                                                                         
                                                                           
                                                                         
                                                                      
                                                                     
                                                               
                                   
       
    if (!childrel->consider_parallel)
    {
      rel->consider_parallel = false;
    }

       
                                                         
       
    Assert(childrel->rows > 0);

    parent_rows += childrel->rows;
    parent_size += childrel->reltarget->width * childrel->rows;

       
                                                                         
                                                                        
                                                                         
                                          
       
                                                                    
       
    forboth(parentvars, rel->reltarget->exprs, childvars, childrel->reltarget->exprs)
    {
      Var *parentvar = (Var *)lfirst(parentvars);
      Node *childvar = (Node *)lfirst(childvars);

      if (IsA(parentvar, Var))
      {
        int pndx = parentvar->varattno - rel->min_attr;
        int32 child_width = 0;

        if (IsA(childvar, Var) && ((Var *)childvar)->varno == childrel->relid)
        {
          int cndx = ((Var *)childvar)->varattno - childrel->min_attr;

          child_width = childrel->attr_widths[cndx];
        }
        if (child_width <= 0)
        {
          child_width = get_typavgwidth(exprType(childvar), exprTypmod(childvar));
        }
        Assert(child_width > 0);
        parent_attrsizes[pndx] += child_width * childrel->rows;
      }
    }
  }

  if (has_live_children)
  {
       
                                         
       
    int i;

    Assert(parent_rows > 0);
    rel->rows = parent_rows;
    rel->reltarget->width = rint(parent_size / parent_rows);
    for (i = 0; i < nattrs; i++)
    {
      rel->attr_widths[i] = rint(parent_attrsizes[i] / parent_rows);
    }

       
                                                                        
                                                                        
       
    rel->tuples = parent_rows;

       
                                                                         
                                                                
       
  }
  else
  {
       
                                                                    
                                                                         
                                                                    
       
    set_dummy_rel_pathlist(rel);
  }

  pfree(parent_attrsizes);
}

   
                           
                                                 
   
static void
set_append_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{
  int parentRTindex = rti;
  List *live_childrels = NIL;
  ListCell *l;

     
                                                                      
                         
     
  foreach (l, root->append_rel_list)
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)lfirst(l);
    int childRTindex;
    RangeTblEntry *childRTE;
    RelOptInfo *childrel;

                                                                 
    if (appinfo->parent_relid != parentRTindex)
    {
      continue;
    }

                                                
    childRTindex = appinfo->child_relid;
    childRTE = root->simple_rte_array[childRTindex];
    childrel = root->simple_rel_array[childRTindex];

       
                                                                 
                                                                       
                                                                         
                                                       
       
    if (!rel->consider_parallel)
    {
      childrel->consider_parallel = false;
    }

       
                                         
       
    set_rel_pathlist(root, childrel, childRTindex, childRTE);

       
                                     
       
    if (IS_DUMMY_REL(childrel))
    {
      continue;
    }

                                                    
    if (rel->part_scheme)
    {
      rel->partitioned_child_rels = list_concat(rel->partitioned_child_rels, list_copy(childrel->partitioned_child_rels));
    }

       
                                                                          
       
    live_childrels = lappend(live_childrels, childrel);
  }

                                         
  add_paths_to_append_rel(root, rel, live_childrels);
}

   
                           
                                                                            
                
   
                                                                              
                                                                               
                                                                           
                                                                          
                                                      
   
void
add_paths_to_append_rel(PlannerInfo *root, RelOptInfo *rel, List *live_childrels)
{
  List *subpaths = NIL;
  bool subpaths_valid = true;
  List *partial_subpaths = NIL;
  List *pa_partial_subpaths = NIL;
  List *pa_nonpartial_subpaths = NIL;
  bool partial_subpaths_valid = true;
  bool pa_subpaths_valid;
  List *all_child_pathkeys = NIL;
  List *all_child_outers = NIL;
  ListCell *l;
  List *partitioned_rels = NIL;
  double partial_rows = -1;

                                                
  pa_subpaths_valid = enable_parallel_append && rel->consider_parallel;

     
                                                                            
                                                                        
                 
     
                                                                             
                                                                          
                                                                        
                                                                        
                                                                             
                                                                         
                                                                            
                                                                           
                                                                       
                                 
     
  if (rel->part_scheme != NULL)
  {
    if (IS_SIMPLE_REL(rel))
    {
      partitioned_rels = list_make1(rel->partitioned_child_rels);
    }
    else if (IS_JOIN_REL(rel))
    {
      int relid = -1;
      List *partrels = NIL;

         
                                                                    
                                       
         
      while ((relid = bms_next_member(rel->relids, relid)) >= 0)
      {
        RelOptInfo *component;

        Assert(relid >= 1 && relid < root->simple_rel_array_size);
        component = root->simple_rel_array[relid];
        Assert(component->part_scheme != NULL);
        Assert(list_length(component->partitioned_child_rels) >= 1);
        partrels = list_concat(partrels, list_copy(component->partitioned_child_rels));
      }

      partitioned_rels = list_make1(partrels);
    }

    Assert(list_length(partitioned_rels) >= 1);
  }

     
                                                                            
                                                                          
                                                   
     
  foreach (l, live_childrels)
  {
    RelOptInfo *childrel = lfirst(l);
    ListCell *lcp;
    Path *cheapest_partial_path = NULL;

       
                                                                        
                                     
       
    if (rel->rtekind == RTE_SUBQUERY && childrel->partitioned_child_rels != NIL)
    {
      partitioned_rels = lappend(partitioned_rels, childrel->partitioned_child_rels);
    }

       
                                                                        
                                                                           
                                                         
       
                                                                      
                                                       
       
    if (childrel->pathlist != NIL && childrel->cheapest_total_path->param_info == NULL)
    {
      accumulate_append_subpath(childrel->cheapest_total_path, &subpaths, NULL);
    }
    else
    {
      subpaths_valid = false;
    }

                                            
    if (childrel->partial_pathlist != NIL)
    {
      cheapest_partial_path = linitial(childrel->partial_pathlist);
      accumulate_append_subpath(cheapest_partial_path, &partial_subpaths, NULL);
    }
    else
    {
      partial_subpaths_valid = false;
    }

       
                                                                           
              
       
    if (pa_subpaths_valid)
    {
      Path *nppath = NULL;

      nppath = get_cheapest_parallel_safe_total_inner(childrel->pathlist);

      if (cheapest_partial_path == NULL && nppath == NULL)
      {
                                                                     
        pa_subpaths_valid = false;
      }
      else if (nppath == NULL || (cheapest_partial_path != NULL && cheapest_partial_path->total_cost < nppath->total_cost))
      {
                                                         
        Assert(cheapest_partial_path != NULL);
        accumulate_append_subpath(cheapest_partial_path, &pa_partial_subpaths, &pa_nonpartial_subpaths);
      }
      else
      {
           
                                                                      
                                                                  
                                                                      
                                          
           
                                                                    
                                                                     
                                                                
                                                                     
                                                                    
                            
           
        accumulate_append_subpath(nppath, &pa_nonpartial_subpaths, NULL);
      }
    }

       
                                                             
                                                                  
                                                                           
                                                      
       
    foreach (lcp, childrel->pathlist)
    {
      Path *childpath = (Path *)lfirst(lcp);
      List *childkeys = childpath->pathkeys;
      Relids childouter = PATH_REQ_OUTER(childpath);

                                                           
      if (childkeys != NIL)
      {
        ListCell *lpk;
        bool found = false;

                                                 
        foreach (lpk, all_child_pathkeys)
        {
          List *existing_pathkeys = (List *)lfirst(lpk);

          if (compare_pathkeys(existing_pathkeys, childkeys) == PATHKEYS_EQUAL)
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
                                                   
          all_child_pathkeys = lappend(all_child_pathkeys, childkeys);
        }
      }

                                                                    
      if (childouter)
      {
        ListCell *lco;
        bool found = false;

                                                  
        foreach (lco, all_child_outers)
        {
          Relids existing_outers = (Relids)lfirst(lco);

          if (bms_equal(existing_outers, childouter))
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
                                                 
          all_child_outers = lappend(all_child_outers, childouter);
        }
      }
    }
  }

     
                                                                             
                                                                           
                                                                       
     
  if (subpaths_valid)
  {
    add_path(rel, (Path *)create_append_path(root, rel, subpaths, NIL, NIL, NULL, 0, false, partitioned_rels, -1));
  }

     
                                                                           
                                    
     
  if (partial_subpaths_valid && partial_subpaths != NIL)
  {
    AppendPath *appendpath;
    ListCell *lc;
    int parallel_workers = 0;

                                                                       
    foreach (lc, partial_subpaths)
    {
      Path *path = lfirst(lc);

      parallel_workers = Max(parallel_workers, path->parallel_workers);
    }
    Assert(parallel_workers > 0);

       
                                                                           
                                                                        
                                                                         
                                                                        
                                                                           
                                                                        
                                                                      
       
    if (enable_parallel_append)
    {
      parallel_workers = Max(parallel_workers, fls(list_length(live_childrels)));
      parallel_workers = Min(parallel_workers, max_parallel_workers_per_gather);
    }
    Assert(parallel_workers > 0);

                                         
    appendpath = create_append_path(root, rel, NIL, partial_subpaths, NIL, NULL, parallel_workers, enable_parallel_append, partitioned_rels, -1);

       
                                                                     
                 
       
    partial_rows = appendpath->path.rows;

                       
    add_partial_path(rel, (Path *)appendpath);
  }

     
                                                                             
                                                                            
                                                                             
                                                                           
     
  if (pa_subpaths_valid && pa_nonpartial_subpaths != NIL)
  {
    AppendPath *appendpath;
    ListCell *lc;
    int parallel_workers = 0;

       
                                                                    
                
       
    foreach (lc, pa_partial_subpaths)
    {
      Path *path = lfirst(lc);

      parallel_workers = Max(parallel_workers, path->parallel_workers);
    }

       
                                                                     
                                                                           
                                               
       
    parallel_workers = Max(parallel_workers, fls(list_length(live_childrels)));
    parallel_workers = Min(parallel_workers, max_parallel_workers_per_gather);
    Assert(parallel_workers > 0);

    appendpath = create_append_path(root, rel, pa_nonpartial_subpaths, pa_partial_subpaths, NIL, NULL, parallel_workers, true, partitioned_rels, partial_rows);
    add_partial_path(rel, (Path *)appendpath);
  }

     
                                                                            
                             
     
  if (subpaths_valid)
  {
    generate_orderedappend_paths(root, rel, live_childrels, all_child_pathkeys, partitioned_rels);
  }

     
                                                                             
                                                                     
                                                                             
                                                                
     
                                                                            
                                                                          
                                                                        
                                                                       
                                                                          
                                                     
     
  foreach (l, all_child_outers)
  {
    Relids required_outer = (Relids)lfirst(l);
    ListCell *lcr;

                                                                         
    subpaths = NIL;
    subpaths_valid = true;
    foreach (lcr, live_childrels)
    {
      RelOptInfo *childrel = (RelOptInfo *)lfirst(lcr);
      Path *subpath;

      if (childrel->pathlist == NIL)
      {
                                                           
        subpaths_valid = false;
        break;
      }

      subpath = get_cheapest_parameterized_child_path(root, childrel, required_outer);
      if (subpath == NULL)
      {
                                                           
        subpaths_valid = false;
        break;
      }
      accumulate_append_subpath(subpath, &subpaths, NULL);
    }

    if (subpaths_valid)
    {
      add_path(rel, (Path *)create_append_path(root, rel, subpaths, NIL, NIL, required_outer, 0, false, partitioned_rels, -1));
    }
  }

     
                                                                             
                                                                             
                                                                            
                                                                      
                                       
     
  if (list_length(live_childrels) == 1)
  {
    RelOptInfo *childrel = (RelOptInfo *)linitial(live_childrels);

    foreach (l, childrel->partial_pathlist)
    {
      Path *path = (Path *)lfirst(l);
      AppendPath *appendpath;

         
                                                                      
                                                 
         
      if (path->pathkeys == NIL || path == linitial(childrel->partial_pathlist))
      {
        continue;
      }

      appendpath = create_append_path(root, rel, NIL, list_make1(path), NIL, NULL, path->parallel_workers, true, partitioned_rels, partial_rows);
      add_partial_path(rel, (Path *)appendpath);
    }
  }
}

   
                                
                                                         
   
                                                                          
                                                                         
                                                     
   
                                                                    
                       
   
                                                                            
                                                                               
                                                                  
   
                                                                            
                                                                             
                                                                          
                                                                              
                                                                            
                                                                              
                                                                          
                                                                              
                                                                       
                                                               
                                                                 
   
static void
generate_orderedappend_paths(PlannerInfo *root, RelOptInfo *rel, List *live_childrels, List *all_child_pathkeys, List *partitioned_rels)
{
  ListCell *lcp;
  List *partition_pathkeys = NIL;
  List *partition_pathkeys_desc = NIL;
  bool partition_pathkeys_partial = true;
  bool partition_pathkeys_desc_partial = true;

     
                                                                      
                                                                        
                                                                             
                                                                           
                                                                             
                                         
     
  if (rel->part_scheme != NULL && IS_SIMPLE_REL(rel) && partitions_are_ordered(rel->boundinfo, rel->nparts))
  {
    partition_pathkeys = build_partition_pathkeys(root, rel, ForwardScanDirection, &partition_pathkeys_partial);

    partition_pathkeys_desc = build_partition_pathkeys(root, rel, BackwardScanDirection, &partition_pathkeys_desc_partial);

       
                                                                     
                                                                          
                                                                          
                                                                          
                                                                           
                                                                    
                           
       
  }

                                                   
  foreach (lcp, all_child_pathkeys)
  {
    List *pathkeys = (List *)lfirst(lcp);
    List *startup_subpaths = NIL;
    List *total_subpaths = NIL;
    bool startup_neq_total = false;
    ListCell *lcr;
    bool match_partition_order;
    bool match_partition_order_desc;

       
                                                                         
                                                                        
                                                                           
                                                                        
                                                                           
                                                                    
                                                      
       
    match_partition_order = pathkeys_contained_in(pathkeys, partition_pathkeys) || (!partition_pathkeys_partial && pathkeys_contained_in(partition_pathkeys, pathkeys));

    match_partition_order_desc = !match_partition_order && (pathkeys_contained_in(pathkeys, partition_pathkeys_desc) || (!partition_pathkeys_desc_partial && pathkeys_contained_in(partition_pathkeys_desc, pathkeys)));

                                                     
    foreach (lcr, live_childrels)
    {
      RelOptInfo *childrel = (RelOptInfo *)lfirst(lcr);
      Path *cheapest_startup, *cheapest_total;

                                                          
      cheapest_startup = get_cheapest_path_for_pathkeys(childrel->pathlist, pathkeys, NULL, STARTUP_COST, false);
      cheapest_total = get_cheapest_path_for_pathkeys(childrel->pathlist, pathkeys, NULL, TOTAL_COST, false);

         
                                                                      
                                                           
         
      if (cheapest_startup == NULL || cheapest_total == NULL)
      {
        cheapest_startup = cheapest_total = childrel->cheapest_total_path;
                                                                      
        Assert(cheapest_total->param_info == NULL);
      }

         
                                                                 
                                                                         
                                                  
         
      if (cheapest_startup != cheapest_total)
      {
        startup_neq_total = true;
      }

         
                                                                         
                                               
         
      if (match_partition_order)
      {
           
                                                                   
                                                                      
                                                                      
                                                                  
                    
           
        cheapest_startup = get_singleton_append_subpath(cheapest_startup);
        cheapest_total = get_singleton_append_subpath(cheapest_total);

        startup_subpaths = lappend(startup_subpaths, cheapest_startup);
        total_subpaths = lappend(total_subpaths, cheapest_total);
      }
      else if (match_partition_order_desc)
      {
           
                                                                       
                                                                    
                                                                       
           
        cheapest_startup = get_singleton_append_subpath(cheapest_startup);
        cheapest_total = get_singleton_append_subpath(cheapest_total);

        startup_subpaths = lcons(cheapest_startup, startup_subpaths);
        total_subpaths = lcons(cheapest_total, total_subpaths);
      }
      else
      {
           
                                                                       
                                            
           
        accumulate_append_subpath(cheapest_startup, &startup_subpaths, NULL);
        accumulate_append_subpath(cheapest_total, &total_subpaths, NULL);
      }
    }

                                                       
    if (match_partition_order || match_partition_order_desc)
    {
                               
      add_path(rel, (Path *)create_append_path(root, rel, startup_subpaths, NIL, pathkeys, NULL, 0, false, partitioned_rels, -1));
      if (startup_neq_total)
      {
        add_path(rel, (Path *)create_append_path(root, rel, total_subpaths, NIL, pathkeys, NULL, 0, false, partitioned_rels, -1));
      }
    }
    else
    {
                               
      add_path(rel, (Path *)create_merge_append_path(root, rel, startup_subpaths, pathkeys, NULL, partitioned_rels));
      if (startup_neq_total)
      {
        add_path(rel, (Path *)create_merge_append_path(root, rel, total_subpaths, pathkeys, NULL, partitioned_rels));
      }
    }
  }
}

   
                                         
                                                                       
                      
   
                                                 
   
static Path *
get_cheapest_parameterized_child_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *cheapest;
  ListCell *lc;

     
                                                                     
                                                                             
           
     
  cheapest = get_cheapest_path_for_pathkeys(rel->pathlist, NIL, required_outer, TOTAL_COST, false);
  Assert(cheapest != NULL);
  if (bms_equal(PATH_REQ_OUTER(cheapest), required_outer))
  {
    return cheapest;
  }

     
                                                                            
                                                                       
                                                                             
                                                                           
                                                                          
                                                                       
     
  cheapest = NULL;
  foreach (lc, rel->pathlist)
  {
    Path *path = (Path *)lfirst(lc);

                                                                       
    if (!bms_is_subset(PATH_REQ_OUTER(path), required_outer))
    {
      continue;
    }

       
                                                                        
                                                                    
       
    if (cheapest != NULL && compare_path_costs(cheapest, path, TOTAL_COST) <= 0)
    {
      continue;
    }

                                                     
    if (!bms_equal(PATH_REQ_OUTER(path), required_outer))
    {
      path = reparameterize_path(root, path, required_outer, 1.0);
      if (path == NULL)
      {
        continue;                                        
      }
      Assert(bms_equal(PATH_REQ_OUTER(path), required_outer));

      if (cheapest != NULL && compare_path_costs(cheapest, path, TOTAL_COST) <= 0)
      {
        continue;
      }
    }

                                 
    cheapest = path;
  }

                                                                       
  return cheapest;
}

   
                             
                                                                        
   
                                                                            
                                                                             
                                                                            
                                                
   
                                                                            
                                                                             
                                                                       
                                                                
   
                                                                             
                                                                              
                                                                             
                                                                            
                                                                           
           
   
static void
accumulate_append_subpath(Path *path, List **subpaths, List **special_subpaths)
{
  if (IsA(path, AppendPath))
  {
    AppendPath *apath = (AppendPath *)path;

    if (!apath->path.parallel_aware || apath->first_partial_path == 0)
    {
                                                                          
      *subpaths = list_concat(*subpaths, list_copy(apath->subpaths));
      return;
    }
    else if (special_subpaths != NULL)
    {
      List *new_special_subpaths;

                                                                       
      *subpaths = list_concat(*subpaths, list_copy_tail(apath->subpaths, apath->first_partial_path));
      new_special_subpaths = list_truncate(list_copy(apath->subpaths), apath->first_partial_path);
      *special_subpaths = list_concat(*special_subpaths, new_special_subpaths);
      return;
    }
  }
  else if (IsA(path, MergeAppendPath))
  {
    MergeAppendPath *mpath = (MergeAppendPath *)path;

                                                                        
    *subpaths = list_concat(*subpaths, list_copy(mpath->subpaths));
    return;
  }

  *subpaths = lappend(*subpaths, path);
}

   
                                
                                                                 
                                                                    
   
                                                   
   
static Path *
get_singleton_append_subpath(Path *path)
{
  Assert(!path->parallel_aware);

  if (IsA(path, AppendPath))
  {
    AppendPath *apath = (AppendPath *)path;

    if (list_length(apath->subpaths) == 1)
    {
      return (Path *)linitial(apath->subpaths);
    }
  }
  else if (IsA(path, MergeAppendPath))
  {
    MergeAppendPath *mpath = (MergeAppendPath *)path;

    if (list_length(mpath->subpaths) == 1)
    {
      return (Path *)linitial(mpath->subpaths);
    }
  }

  return path;
}

   
                          
                                                                           
   
                                                                              
                                                                              
   
                                                                         
                                                                         
                  
   
static void
set_dummy_rel_pathlist(RelOptInfo *rel)
{
                                                                     
  rel->rows = 0;
  rel->reltarget->width = 0;

                                                                
  rel->pathlist = NIL;
  rel->partial_pathlist = NIL;

                             
  add_path(rel, (Path *)create_append_path(NULL, rel, NIL, NIL, NIL, rel->lateral_relids, 0, false, NIL, -1));

     
                                                                         
                                                                           
                                                                           
                               
     
  set_cheapest(rel);
}

                                                          
static bool
has_multiple_baserels(PlannerInfo *root)
{
  int num_base_rels = 0;
  Index rti;

  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];

    if (brel == NULL)
    {
      continue;
    }

                                           
    if (brel->reloptkind == RELOPT_BASEREL)
    {
      if (++num_base_rels > 1)
      {
        return true;
      }
    }
  }
  return false;
}

   
                         
                                                          
   
                                                                            
                                                                             
                                                                   
                                                                     
                                                                         
                                                                               
                                                                           
   
static void
set_subquery_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{
  Query *parse = root->parse;
  Query *subquery = rte->subquery;
  Relids required_outer;
  pushdown_safety_info safetyInfo;
  double tuple_fraction;
  RelOptInfo *sub_final_rel;
  ListCell *lc;

     
                                                                           
                                                                          
                                                                        
     
  subquery = copyObject(subquery);

     
                                                                           
                                                                           
                                                               
     
  required_outer = rel->lateral_relids;

     
                                                                            
                                                                          
                                                                         
                                                                         
                                          
     
  memset(&safetyInfo, 0, sizeof(safetyInfo));
  safetyInfo.unsafeColumns = (bool *)palloc0((list_length(subquery->targetList) + 1) * sizeof(bool));

     
                                                                            
                                                                           
                                                                           
                                                                           
                                                                   
     
  safetyInfo.unsafeLeaky = rte->security_barrier;

     
                                                                         
                                                                             
                                                                             
                                                                             
                                                           
     
                                                                             
                                                                        
                                                       
                                                                
                                                                      
               
     
                                                                  
                        
     
                                                                            
                                                                     
     
  if (rel->baserestrictinfo != NIL && subquery_is_pushdown_safe(subquery, subquery, &safetyInfo))
  {
                                                      
    List *upperrestrictlist = NIL;
    ListCell *l;

    foreach (l, rel->baserestrictinfo)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);
      Node *clause = (Node *)rinfo->clause;

      if (!rinfo->pseudoconstant && qual_is_pushdown_safe(subquery, rti, clause, &safetyInfo))
      {
                          
        subquery_push_qual(subquery, rte, rti, clause);
      }
      else
      {
                                        
        upperrestrictlist = lappend(upperrestrictlist, rinfo);
      }
    }
    rel->baserestrictinfo = upperrestrictlist;
                                                               
  }

  pfree(safetyInfo.unsafeColumns);

     
                                                                         
                           
     
  remove_unused_subquery_outputs(subquery, rel);

     
                                                                             
                                                                          
                                                                         
                                                  
     
  if (parse->hasAggs || parse->groupClause || parse->groupingSets || parse->havingQual || parse->distinctClause || parse->sortClause || has_multiple_baserels(root))
  {
    tuple_fraction = 0.0;                   
  }
  else
  {
    tuple_fraction = root->tuple_fraction;
  }

                                                               
  Assert(root->plan_params == NIL);

                                                     
  rel->subroot = subquery_planner(root->glob, subquery, root, false, tuple_fraction);

                                                          
  rel->subplan_params = root->plan_params;
  root->plan_params = NIL;

     
                                                                           
                                                                           
                                                              
     
  sub_final_rel = fetch_upper_rel(rel->subroot, UPPERREL_FINAL, NULL);

  if (IS_DUMMY_REL(sub_final_rel))
  {
    set_dummy_rel_pathlist(rel);
    return;
  }

     
                                                                            
                                                                            
                
     
  set_subquery_size_estimates(root, rel);

     
                                                                           
                         
     
  foreach (lc, sub_final_rel->pathlist)
  {
    Path *subpath = (Path *)lfirst(lc);
    List *pathkeys;

                                                            
    pathkeys = convert_subquery_pathkeys(root, rel, subpath->pathkeys, make_tlist_from_pathtarget(subpath->pathtarget));

                                                
    add_path(rel, (Path *)create_subqueryscan_path(root, rel, subpath, pathkeys, required_outer));
  }

                                                                   
  if (rel->consider_parallel && bms_is_empty(required_outer))
  {
                                                                          
    Assert(sub_final_rel->consider_parallel || sub_final_rel->partial_pathlist == NIL);

                                 
    foreach (lc, sub_final_rel->partial_pathlist)
    {
      Path *subpath = (Path *)lfirst(lc);
      List *pathkeys;

                                                              
      pathkeys = convert_subquery_pathkeys(root, rel, subpath->pathkeys, make_tlist_from_pathtarget(subpath->pathtarget));

                                                  
      add_partial_path(rel, (Path *)create_subqueryscan_path(root, rel, subpath, pathkeys, required_outer));
    }
  }
}

   
                         
                                                      
   
static void
set_function_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;
  List *pathkeys = NIL;

     
                                                                        
                                                                            
                                      
     
  required_outer = rel->lateral_relids;

     
                                                                             
                                                                         
                                                                    
     
  if (rte->funcordinality)
  {
    AttrNumber ordattno = rel->max_attr;
    Var *var = NULL;
    ListCell *lc;

       
                                                                         
                                                                       
                                              
       
    foreach (lc, rel->reltarget->exprs)
    {
      Var *node = (Var *)lfirst(lc);

                                                       
      if (IsA(node, Var) && node->varattno == ordattno && node->varno == rel->relid && node->varlevelsup == 0)
      {
        var = node;
        break;
      }
    }

       
                                                                      
                                                                           
                                                                         
                                 
       
    if (var)
    {
      pathkeys = build_expression_pathkey(root, (Expr *)var, NULL,                        
          Int8LessOperator, rel->relids, false);
    }
  }

                                 
  add_path(rel, create_functionscan_path(root, rel, pathkeys, required_outer));
}

   
                       
                                                    
   
static void
set_values_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;

     
                                                                            
                                                                           
                                
     
  required_outer = rel->lateral_relids;

                                 
  add_path(rel, create_valuesscan_path(root, rel, required_outer));
}

   
                          
                                                        
   
static void
set_tablefunc_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;

     
                                                                         
                                                                            
                                      
     
  required_outer = rel->lateral_relids;

                                 
  add_path(rel, create_tablefuncscan_path(root, rel, required_outer));
}

   
                    
                                                                    
   
                                                                     
                                                   
   
static void
set_cte_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Plan *cteplan;
  PlannerInfo *cteroot;
  Index levelsup;
  int ndx;
  ListCell *lc;
  int plan_id;
  Relids required_outer;

     
                                                                          
     
  levelsup = rte->ctelevelsup;
  cteroot = root;
  while (levelsup-- > 0)
  {
    cteroot = cteroot->parent_root;
    if (!cteroot)                       
    {
      elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
    }
  }

     
                                                                             
                                                                           
                                     
     
  ndx = 0;
  foreach (lc, cteroot->parse->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

    if (strcmp(cte->ctename, rte->ctename) == 0)
    {
      break;
    }
    ndx++;
  }
  if (lc == NULL)                       
  {
    elog(ERROR, "could not find CTE \"%s\"", rte->ctename);
  }
  if (ndx >= list_length(cteroot->cte_plan_ids))
  {
    elog(ERROR, "could not find plan for CTE \"%s\"", rte->ctename);
  }
  plan_id = list_nth_int(cteroot->cte_plan_ids, ndx);
  if (plan_id <= 0)
  {
    elog(ERROR, "no plan was made for CTE \"%s\"", rte->ctename);
  }
  cteplan = (Plan *)list_nth(root->glob->subplans, plan_id - 1);

                                                       
  set_cte_size_estimates(root, rel, cteplan->plan_rows);

     
                                                                             
                                                                          
                
     
  required_outer = rel->lateral_relids;

                                 
  add_path(rel, create_ctescan_path(root, rel, required_outer));
}

   
                                
                                                              
   
                                                                           
                                                                
   
static void
set_namedtuplestore_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;

                                                       
  set_namedtuplestore_size_estimates(root, rel);

     
                                                                          
                                                                            
                        
     
  required_outer = rel->lateral_relids;

                                 
  add_path(rel, create_namedtuplestorescan_path(root, rel, required_outer));

                                                          
  set_cheapest(rel);
}

   
                       
                                                         
   
                                                                  
                                                               
   
static void
set_result_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Relids required_outer;

                                                       
  set_result_size_estimates(root, rel);

     
                                                                            
                                                                           
                   
     
  required_outer = rel->lateral_relids;

                                 
  add_path(rel, create_resultscan_path(root, rel, required_outer));

                                                          
  set_cheapest(rel);
}

   
                          
                                                                
   
                                                                           
                                                   
   
static void
set_worktable_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  Path *ctepath;
  PlannerInfo *cteroot;
  Index levelsup;
  Relids required_outer;

     
                                                                         
                                                                             
                               
     
  levelsup = rte->ctelevelsup;
  if (levelsup == 0)                       
  {
    elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
  }
  levelsup--;
  cteroot = root;
  while (levelsup-- > 0)
  {
    cteroot = cteroot->parent_root;
    if (!cteroot)                       
    {
      elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
    }
  }
  ctepath = cteroot->non_recursive_path;
  if (!ctepath)                       
  {
    elog(ERROR, "could not find path for CTE \"%s\"", rte->ctename);
  }

                                                       
  set_cte_size_estimates(root, rel, ctepath->rows);

     
                                                                         
                                                                            
                                                                           
                                                                             
     
  required_outer = rel->lateral_relids;

                                 
  add_path(rel, create_worktablescan_path(root, rel, required_outer));
}

   
                         
                                                                         
                                           
   
                                                                             
                                                                            
                                                                     
   
                                                                             
                                                                          
                                                                          
                                                                           
                                                                          
                          
   
void
generate_gather_paths(PlannerInfo *root, RelOptInfo *rel, bool override_rows)
{
  Path *cheapest_partial_path;
  Path *simple_gather_path;
  ListCell *lc;
  double rows;
  double *rowsp = NULL;

                                                                  
  if (rel->partial_pathlist == NIL)
  {
    return;
  }

                                                       
  if (override_rows)
  {
    rowsp = &rows;
  }

     
                                                                          
                                                                            
                                                                    
     
  cheapest_partial_path = linitial(rel->partial_pathlist);
  rows = cheapest_partial_path->rows * cheapest_partial_path->parallel_workers;
  simple_gather_path = (Path *)create_gather_path(root, rel, cheapest_partial_path, rel->reltarget, NULL, rowsp);
  add_path(rel, simple_gather_path);

     
                                                                          
            
     
  foreach (lc, rel->partial_pathlist)
  {
    Path *subpath = (Path *)lfirst(lc);
    GatherMergePath *path;

    if (subpath->pathkeys == NIL)
    {
      continue;
    }

    rows = subpath->rows * subpath->parallel_workers;
    path = create_gather_merge_path(root, rel, subpath, rel->reltarget, subpath->pathkeys, NULL, rowsp);
    add_path(rel, &path->path);
  }
}

   
                          
                                                                          
   
                                                                          
                   
   
static RelOptInfo *
make_rel_from_joinlist(PlannerInfo *root, List *joinlist)
{
  int levels_needed;
  List *initial_rels;
  ListCell *jl;

     
                                                                         
                                                                          
                              
     
  levels_needed = list_length(joinlist);

  if (levels_needed <= 0)
  {
    return NULL;                     
  }

     
                                                                         
                                                                       
                    
     
  initial_rels = NIL;
  foreach (jl, joinlist)
  {
    Node *jlnode = (Node *)lfirst(jl);
    RelOptInfo *thisrel;

    if (IsA(jlnode, RangeTblRef))
    {
      int varno = ((RangeTblRef *)jlnode)->rtindex;

      thisrel = find_base_rel(root, varno);
    }
    else if (IsA(jlnode, List))
    {
                                        
      thisrel = make_rel_from_joinlist(root, (List *)jlnode);
    }
    else
    {
      elog(ERROR, "unrecognized joinlist node type: %d", (int)nodeTag(jlnode));
      thisrel = NULL;                          
    }

    initial_rels = lappend(initial_rels, thisrel);
  }

  if (levels_needed == 1)
  {
       
                                            
       
    return (RelOptInfo *)linitial(initial_rels);
  }
  else
  {
       
                                                                      
                                                              
       
                                                                     
                                                              
       
    root->initial_rels = initial_rels;

    if (join_search_hook)
    {
      return (*join_search_hook)(root, levels_needed, initial_rels);
    }
    else if (enable_geqo && levels_needed >= geqo_threshold)
    {
      return geqo(root, levels_needed, initial_rels);
    }
    else
    {
      return standard_join_search(root, levels_needed, initial_rels);
    }
  }
}

   
                        
                                                                      
                                                      
   
                                                                         
                                                           
   
                                                                     
                                                                    
                                                          
   
                                                                         
                                                              
                                                                           
                               
   
                                                                            
                                                                           
                                                                            
                                                                       
                                                                             
                                                       
   
                                                                               
                                                                               
                                                                            
                                                                              
   
RelOptInfo *
standard_join_search(PlannerInfo *root, int levels_needed, List *initial_rels)
{
  int lev;
  RelOptInfo *rel;

     
                                                                         
                                                           
     
  Assert(root->join_rel_level == NULL);

     
                                                                           
                                                                             
                                                                           
                                                                        
                         
     
                                                                             
                                                                           
                
     
  root->join_rel_level = (List **)palloc0((levels_needed + 1) * sizeof(List *));

  root->join_rel_level[1] = initial_rels;

  for (lev = 2; lev <= levels_needed; lev++)
  {
    ListCell *lc;

       
                                                                      
                                                                       
                                      
       
    join_search_one_level(root, lev);

       
                                                                           
                                                                      
                                                                 
                                                                          
       
                                                                     
                       
       
    foreach (lc, root->join_rel_level[lev])
    {
      rel = (RelOptInfo *)lfirst(lc);

                                                 
      generate_partitionwise_join_paths(root, rel);

         
                                                                  
                                                                         
                                                                   
         
      if (lev < levels_needed)
      {
        generate_gather_paths(root, rel, false);
      }

                                                         
      set_cheapest(rel);

#ifdef OPTIMIZER_DEBUG
      debug_print_rel(root, rel);
#endif
    }
  }

     
                                                     
     
  if (root->join_rel_level[levels_needed] == NIL)
  {
    elog(ERROR, "failed to build any %d-way joins", levels_needed);
  }
  Assert(list_length(root->join_rel_level[levels_needed]) == 1);

  rel = (RelOptInfo *)linitial(root->join_rel_level[levels_needed]);

  root->join_rel_level = NULL;

  return rel;
}

                                                                               
                                        
                                                                               

   
                                                                          
   
                                                                       
                                                                       
                        
   
                            
   
                                                                           
                                                     
   
                                                                           
                                                         
   
                                                                            
                                                                           
                                                                        
                                                                          
                                                                          
                                                                           
                                         
   
                                                                               
                                                                              
                                                                          
                                                                          
                                 
   
                                                                              
                                                                               
                                                                       
                                                                             
                                                                        
   
                                                                          
                                                                             
                                                                               
                                                                              
                                                                        
                                                                          
                                                                         
                                                                               
                                                                              
                
   
                                                                               
                                                                             
                                                                           
                                                                              
                                                                       
                                                                              
                                                                             
                                                                    
                                          
   
                                                                          
                                                                            
                                                                      
                                                                          
                                                                              
                                                                               
                                                                            
                                                                              
                                                                           
                                                                               
                                                                     
   
                                                                            
                                                                             
                                                                            
                                                                           
                                                                             
                                                                            
                                                                              
                                                                          
                                                                            
                                                                               
                           
   
static bool
subquery_is_pushdown_safe(Query *subquery, Query *topquery, pushdown_safety_info *safetyInfo)
{
  SetOperationStmt *topop;

                     
  if (subquery->limitOffset != NULL || subquery->limitCount != NULL)
  {
    return false;
  }

                     
  if (subquery->groupClause && subquery->groupingSets)
  {
    return false;
  }

                                
  if (subquery->distinctClause || subquery->hasWindowFuncs || subquery->hasTargetSRFs)
  {
    safetyInfo->unsafeVolatile = true;
  }

     
                                                                          
                                                                            
                                                                            
            
     
  if (subquery->setOperations == NULL)
  {
    check_output_expressions(subquery, safetyInfo);
  }

                                                             
  if (subquery == topquery)
  {
                                                   
    if (subquery->setOperations != NULL)
    {
      if (!recurse_pushdown_safe(subquery->setOperations, topquery, safetyInfo))
      {
        return false;
      }
    }
  }
  else
  {
                                                                   
    if (subquery->setOperations != NULL)
    {
      return false;
    }
                                                                    
    topop = castNode(SetOperationStmt, topquery->setOperations);
    Assert(topop);
    compare_tlist_datatypes(subquery->targetList, topop->colTypes, safetyInfo);
  }
  return true;
}

   
                                                        
   
static bool
recurse_pushdown_safe(Node *setOp, Query *topquery, pushdown_safety_info *safetyInfo)
{
  if (IsA(setOp, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)setOp;
    RangeTblEntry *rte = rt_fetch(rtr->rtindex, topquery->rtable);
    Query *subquery = rte->subquery;

    Assert(subquery != NULL);
    return subquery_is_pushdown_safe(subquery, topquery, safetyInfo);
  }
  else if (IsA(setOp, SetOperationStmt))
  {
    SetOperationStmt *op = (SetOperationStmt *)setOp;

                                                                   
    if (op->op == SETOP_EXCEPT)
    {
      return false;
    }
                      
    if (!recurse_pushdown_safe(op->larg, topquery, safetyInfo))
    {
      return false;
    }
    if (!recurse_pushdown_safe(op->rarg, topquery, safetyInfo))
    {
      return false;
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(setOp));
  }
  return true;
}

   
                                                                             
   
                                                                            
                                                                             
                                                                          
                                                                              
                     
   
                                                                           
                                                                      
                                  
   
                                                                           
                                                                           
                                                  
   
                                                                             
                                                                           
                                                                            
                                                                             
                                                                         
                                              
   
                                                                            
                                                                               
                                                                      
                                                                            
                                                                     
                                                                        
                                                                  
                                            
   
static void
check_output_expressions(Query *subquery, pushdown_safety_info *safetyInfo)
{
  ListCell *lc;

  foreach (lc, subquery->targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);

    if (tle->resjunk)
    {
      continue;                             
    }

                                                                         
    if (safetyInfo->unsafeColumns[tle->resno])
    {
      continue;
    }

                                                       
    if (subquery->hasTargetSRFs && expression_returns_set((Node *)tle->expr))
    {
      safetyInfo->unsafeColumns[tle->resno] = true;
      continue;
    }

                                                 
    if (contain_volatile_functions((Node *)tle->expr))
    {
      safetyInfo->unsafeColumns[tle->resno] = true;
      continue;
    }

                                                     
    if (subquery->hasDistinctOn && !targetIsInSortList(tle, InvalidOid, subquery->distinctClause))
    {
                                                  
      safetyInfo->unsafeColumns[tle->resno] = true;
      continue;
    }

                                                          
    if (subquery->hasWindowFuncs && !targetIsInAllPartitionLists(tle, subquery))
    {
                                                                      
      safetyInfo->unsafeColumns[tle->resno] = true;
      continue;
    }
  }
}

   
                                                                        
                                                                          
                                                                        
                                                                    
                                                                            
                                                                          
                                        
   
                                                                         
                                                                         
                                                           
   
                              
                                                                         
                                                    
   
static void
compare_tlist_datatypes(List *tlist, List *colTypes, pushdown_safety_info *safetyInfo)
{
  ListCell *l;
  ListCell *colType = list_head(colTypes);

  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resjunk)
    {
      continue;                             
    }
    if (colType == NULL)
    {
      elog(ERROR, "wrong number of tlist entries");
    }
    if (exprType((Node *)tle->expr) != lfirst_oid(colType))
    {
      safetyInfo->unsafeColumns[tle->resno] = true;
    }
    colType = lnext(colType);
  }
  if (colType != NULL)
  {
    elog(ERROR, "wrong number of tlist entries");
  }
}

   
                               
                                                                 
                                          
   
                                                                      
                                                                        
                                                                     
                                                                        
   
static bool
targetIsInAllPartitionLists(TargetEntry *tle, Query *query)
{
  ListCell *lc;

  foreach (lc, query->windowClause)
  {
    WindowClause *wc = (WindowClause *)lfirst(lc);

    if (!targetIsInSortList(tle, InvalidOid, wc->partitionClause))
    {
      return false;
    }
  }
  return true;
}

   
                                                                   
   
                                                                          
                                       
   
                            
   
                                                                          
                                                                            
                                                                            
                                                                           
                                                                          
   
                                                                       
              
   
                                                                           
                                                                         
                             
   
                                                                      
                                                                         
   
                                                                       
                                                                   
   
static bool
qual_is_pushdown_safe(Query *subquery, Index rti, Node *qual, pushdown_safety_info *safetyInfo)
{
  bool safe = true;
  List *vars;
  ListCell *vl;

                                   
  if (contain_subplans(qual))
  {
    return false;
  }

                                                                    
  if (safetyInfo->unsafeVolatile && contain_volatile_functions(qual))
  {
    return false;
  }

                                               
  if (safetyInfo->unsafeLeaky && contain_leaked_vars(qual))
  {
    return false;
  }

     
                                                                             
                                                                            
                                                                  
     
  Assert(!contain_window_function(qual));

     
                                                                            
                                                                         
                                                                      
                 
     
  vars = pull_var_clause(qual, PVC_INCLUDE_PLACEHOLDERS);
  foreach (vl, vars)
  {
    Var *var = (Var *)lfirst(vl);

       
                                                                          
                                                                          
                                                                       
                                                                          
             
       
    if (!IsA(var, Var))
    {
      safe = false;
      break;
    }

       
                                                                         
                                                                        
                                                                          
                                                                    
       
    if (var->varno != rti)
    {
      safe = false;
      break;
    }

                                           
    Assert(var->varattno >= 0);

                       
    if (var->varattno == 0)
    {
      safe = false;
      break;
    }

                       
    if (safetyInfo->unsafeColumns[var->varattno])
    {
      safe = false;
      break;
    }
  }

  list_free(vars);

  return safe;
}

   
                                                                         
   
static void
subquery_push_qual(Query *subquery, RangeTblEntry *rte, Index rti, Node *qual)
{
  if (subquery->setOperations != NULL)
  {
                                                               
    recurse_push_qual(subquery->setOperations, subquery, rte, rti, qual);
  }
  else
  {
       
                                                                           
                                                                           
                                                                         
                                                        
       
                                                                          
                                                           
       
    qual = ReplaceVarsFromTargetList(qual, rti, 0, rte, subquery->targetList, REPLACEVARS_REPORT_ERROR, 0, &subquery->hasSubLinks);

       
                                                                           
                                                                          
                                                     
       
    if (subquery->hasAggs || subquery->groupClause || subquery->groupingSets || subquery->havingQual)
    {
      subquery->havingQual = make_and_qual(subquery->havingQual, qual);
    }
    else
    {
      subquery->jointree->quals = make_and_qual(subquery->jointree->quals, qual);
    }

       
                                                                       
                                                                        
                                                         
       
  }
}

   
                                                        
   
static void
recurse_push_qual(Node *setOp, Query *topquery, RangeTblEntry *rte, Index rti, Node *qual)
{
  if (IsA(setOp, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)setOp;
    RangeTblEntry *subrte = rt_fetch(rtr->rtindex, topquery->rtable);
    Query *subquery = subrte->subquery;

    Assert(subquery != NULL);
    subquery_push_qual(subquery, rte, rti, qual);
  }
  else if (IsA(setOp, SetOperationStmt))
  {
    SetOperationStmt *op = (SetOperationStmt *)setOp;

    recurse_push_qual(op->larg, topquery, rte, rti, qual);
    recurse_push_qual(op->rarg, topquery, rte, rti, qual);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(setOp));
  }
}

                                                                               
                                      
                                                                               

   
                                  
                                                   
   
                                                                          
                                                                            
                                                                              
                                                                           
                                                                            
                                                                           
                                                                    
   
                                                                              
                                                                               
                                                                      
   
static void
remove_unused_subquery_outputs(Query *subquery, RelOptInfo *rel)
{
  Bitmapset *attrs_used = NULL;
  ListCell *lc;

     
                                                                        
                                                                            
                        
     
  if (subquery->setOperations)
  {
    return;
  }

     
                                                                           
                                                                      
     
  if (subquery->distinctClause && !subquery->hasDistinctOn)
  {
    return;
  }

     
                                                                         
            
     
                                                                             
                                                                             
                                                                          
                                                  
     
  pull_varattnos((Node *)rel->reltarget->exprs, rel->relid, &attrs_used);

                                                                          
  foreach (lc, rel->baserestrictinfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

    pull_varattnos((Node *)rinfo->clause, rel->relid, &attrs_used);
  }

     
                                                                       
               
     
  if (bms_is_member(0 - FirstLowInvalidHeapAttributeNumber, attrs_used))
  {
    return;
  }

     
                                                                        
                                                                          
                           
     
  foreach (lc, subquery->targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);
    Node *texpr = (Node *)tle->expr;

       
                                                                     
                                                                    
                                                                       
                                                                      
                                                                         
                                                                        
       
    if (tle->ressortgroupref || tle->resjunk)
    {
      continue;
    }

       
                                                            
       
    if (bms_is_member(tle->resno - FirstLowInvalidHeapAttributeNumber, attrs_used))
    {
      continue;
    }

       
                                                                         
                                                                      
       
    if (subquery->hasTargetSRFs && expression_returns_set(texpr))
    {
      continue;
    }

       
                                                                        
                                                                
       
    if (contain_volatile_functions(texpr))
    {
      continue;
    }

       
                                                                           
                                                                      
                                                      
       
    tle->expr = (Expr *)makeNullConst(exprType(texpr), exprTypmod(texpr), exprCollation(texpr));
  }
}

   
                               
                                                     
   
void
create_partial_bitmap_paths(PlannerInfo *root, RelOptInfo *rel, Path *bitmapqual)
{
  int parallel_workers;
  double pages_fetched;

                                               
  pages_fetched = compute_bitmap_pages(root, rel, bitmapqual, 1.0, NULL, NULL);

  parallel_workers = compute_parallel_worker(rel, pages_fetched, -1, max_parallel_workers_per_gather);

  if (parallel_workers <= 0)
  {
    return;
  }

  add_partial_path(rel, (Path *)create_bitmap_heap_path(root, rel, bitmapqual, rel->lateral_relids, 1.0, parallel_workers));
}

   
                                                                        
                                                                               
                                                                             
             
   
                                                                                 
                                      
   
                                                                                  
                                      
   
                                                                             
                     
   
int
compute_parallel_worker(RelOptInfo *rel, double heap_pages, double index_pages, int max_workers)
{
  int parallel_workers = 0;

     
                                                                             
                                         
     
  if (rel->rel_parallel_workers != -1)
  {
    parallel_workers = rel->rel_parallel_workers;
  }
  else
  {
       
                                                                         
                                                                      
                                                                     
                                                                       
                                                                          
            
       
    if (rel->reloptkind == RELOPT_BASEREL && ((heap_pages >= 0 && heap_pages < min_parallel_table_scan_size) || (index_pages >= 0 && index_pages < min_parallel_index_scan_size)))
    {
      return 0;
    }

    if (heap_pages >= 0)
    {
      int heap_parallel_threshold;
      int heap_parallel_workers = 1;

         
                                                                      
                                                                   
                                                                       
                                                                    
                                          
         
      heap_parallel_threshold = Max(min_parallel_table_scan_size, 1);
      while (heap_pages >= (BlockNumber)(heap_parallel_threshold * 3))
      {
        heap_parallel_workers++;
        heap_parallel_threshold *= 3;
        if (heap_parallel_threshold > INT_MAX / 3)
        {
          break;                     
        }
      }

      parallel_workers = heap_parallel_workers;
    }

    if (index_pages >= 0)
    {
      int index_parallel_workers = 1;
      int index_parallel_threshold;

                                                    
      index_parallel_threshold = Max(min_parallel_index_scan_size, 1);
      while (index_pages >= (BlockNumber)(index_parallel_threshold * 3))
      {
        index_parallel_workers++;
        index_parallel_threshold *= 3;
        if (index_parallel_threshold > INT_MAX / 3)
        {
          break;                     
        }
      }

      if (parallel_workers > 0)
      {
        parallel_workers = Min(parallel_workers, index_parallel_workers);
      }
      else
      {
        parallel_workers = index_parallel_workers;
      }
    }
  }

                                                                          
  parallel_workers = Min(parallel_workers, max_workers);

  return parallel_workers;
}

   
                                     
                                                                        
                    
   
                                                                        
                                                                           
                                   
   
void
generate_partitionwise_join_paths(PlannerInfo *root, RelOptInfo *rel)
{
  List *live_children = NIL;
  int cnt_parts;
  int num_parts;
  RelOptInfo **part_rels;

                                        
  if (!IS_JOIN_REL(rel))
  {
    return;
  }

                                                               
  if (!IS_PARTITIONED_REL(rel))
  {
    return;
  }

                                                                 
  Assert(rel->consider_partitionwise_join);

                                                                            
  check_stack_depth();

  num_parts = rel->nparts;
  part_rels = rel->part_rels;

                                      
  for (cnt_parts = 0; cnt_parts < num_parts; cnt_parts++)
  {
    RelOptInfo *child_rel = part_rels[cnt_parts];

                                                             
    if (child_rel == NULL)
    {
      continue;
    }

                                                                        
    generate_partitionwise_join_paths(root, child_rel);

                                                                        
    if (child_rel->pathlist == NIL)
    {
         
                                                                
                                       
         
      rel->nparts = 0;
      return;
    }

                                                  
    set_cheapest(child_rel);

                                                              
    if (IS_DUMMY_REL(child_rel))
    {
      continue;
    }

#ifdef OPTIMIZER_DEBUG
    debug_print_rel(root, child_rel);
#endif

    live_children = lappend(live_children, child_rel);
  }

                                                                
  if (!live_children)
  {
    mark_dummy_rel(rel);
    return;
  }

                                                                  
  add_paths_to_append_rel(root, rel, live_children);
  list_free(live_children);
}

                                                                               
                   
                                                                               

#ifdef OPTIMIZER_DEBUG

static void
print_relids(PlannerInfo *root, Relids relids)
{
  int x;
  bool first = true;

  x = -1;
  while ((x = bms_next_member(relids, x)) >= 0)
  {
    if (!first)
    {
      printf(" ");
    }
    if (x < root->simple_rel_array_size && root->simple_rte_array[x])
    {
      printf("%s", root->simple_rte_array[x]->eref->aliasname);
    }
    else
    {
      printf("%d", x);
    }
    first = false;
  }
}

static void
print_restrictclauses(PlannerInfo *root, List *clauses)
{
  ListCell *l;

  foreach (l, clauses)
  {
    RestrictInfo *c = lfirst(l);

    print_expr((Node *)c->clause, root->parse->rtable);
    if (lnext(l))
    {
      printf(", ");
    }
  }
}

static void
print_path(PlannerInfo *root, Path *path, int indent)
{
  const char *ptype;
  bool join = false;
  Path *subpath = NULL;
  int i;

  switch (nodeTag(path))
  {
  case T_Path:
    switch (path->pathtype)
    {
    case T_SeqScan:
      ptype = "SeqScan";
      break;
    case T_SampleScan:
      ptype = "SampleScan";
      break;
    case T_FunctionScan:
      ptype = "FunctionScan";
      break;
    case T_TableFuncScan:
      ptype = "TableFuncScan";
      break;
    case T_ValuesScan:
      ptype = "ValuesScan";
      break;
    case T_CteScan:
      ptype = "CteScan";
      break;
    case T_NamedTuplestoreScan:
      ptype = "NamedTuplestoreScan";
      break;
    case T_Result:
      ptype = "Result";
      break;
    case T_WorkTableScan:
      ptype = "WorkTableScan";
      break;
    default:
      ptype = "???Path";
      break;
    }
    break;
  case T_IndexPath:
    ptype = "IdxScan";
    break;
  case T_BitmapHeapPath:
    ptype = "BitmapHeapScan";
    break;
  case T_BitmapAndPath:
    ptype = "BitmapAndPath";
    break;
  case T_BitmapOrPath:
    ptype = "BitmapOrPath";
    break;
  case T_TidPath:
    ptype = "TidScan";
    break;
  case T_SubqueryScanPath:
    ptype = "SubqueryScan";
    break;
  case T_ForeignPath:
    ptype = "ForeignScan";
    break;
  case T_CustomPath:
    ptype = "CustomScan";
    break;
  case T_NestPath:
    ptype = "NestLoop";
    join = true;
    break;
  case T_MergePath:
    ptype = "MergeJoin";
    join = true;
    break;
  case T_HashPath:
    ptype = "HashJoin";
    join = true;
    break;
  case T_AppendPath:
    ptype = "Append";
    break;
  case T_MergeAppendPath:
    ptype = "MergeAppend";
    break;
  case T_GroupResultPath:
    ptype = "GroupResult";
    break;
  case T_MaterialPath:
    ptype = "Material";
    subpath = ((MaterialPath *)path)->subpath;
    break;
  case T_UniquePath:
    ptype = "Unique";
    subpath = ((UniquePath *)path)->subpath;
    break;
  case T_GatherPath:
    ptype = "Gather";
    subpath = ((GatherPath *)path)->subpath;
    break;
  case T_GatherMergePath:
    ptype = "GatherMerge";
    subpath = ((GatherMergePath *)path)->subpath;
    break;
  case T_ProjectionPath:
    ptype = "Projection";
    subpath = ((ProjectionPath *)path)->subpath;
    break;
  case T_ProjectSetPath:
    ptype = "ProjectSet";
    subpath = ((ProjectSetPath *)path)->subpath;
    break;
  case T_SortPath:
    ptype = "Sort";
    subpath = ((SortPath *)path)->subpath;
    break;
  case T_GroupPath:
    ptype = "Group";
    subpath = ((GroupPath *)path)->subpath;
    break;
  case T_UpperUniquePath:
    ptype = "UpperUnique";
    subpath = ((UpperUniquePath *)path)->subpath;
    break;
  case T_AggPath:
    ptype = "Agg";
    subpath = ((AggPath *)path)->subpath;
    break;
  case T_GroupingSetsPath:
    ptype = "GroupingSets";
    subpath = ((GroupingSetsPath *)path)->subpath;
    break;
  case T_MinMaxAggPath:
    ptype = "MinMaxAgg";
    break;
  case T_WindowAggPath:
    ptype = "WindowAgg";
    subpath = ((WindowAggPath *)path)->subpath;
    break;
  case T_SetOpPath:
    ptype = "SetOp";
    subpath = ((SetOpPath *)path)->subpath;
    break;
  case T_RecursiveUnionPath:
    ptype = "RecursiveUnion";
    break;
  case T_LockRowsPath:
    ptype = "LockRows";
    subpath = ((LockRowsPath *)path)->subpath;
    break;
  case T_ModifyTablePath:
    ptype = "ModifyTable";
    break;
  case T_LimitPath:
    ptype = "Limit";
    subpath = ((LimitPath *)path)->subpath;
    break;
  default:
    ptype = "???Path";
    break;
  }

  for (i = 0; i < indent; i++)
  {
    printf("\t");
  }
  printf("%s", ptype);

  if (path->parent)
  {
    printf("(");
    print_relids(root, path->parent->relids);
    printf(")");
  }
  if (path->param_info)
  {
    printf(" required_outer (");
    print_relids(root, path->param_info->ppi_req_outer);
    printf(")");
  }
  printf(" rows=%.0f cost=%.2f..%.2f\n", path->rows, path->startup_cost, path->total_cost);

  if (path->pathkeys)
  {
    for (i = 0; i < indent; i++)
    {
      printf("\t");
    }
    printf("  pathkeys: ");
    print_pathkeys(path->pathkeys, root->parse->rtable);
  }

  if (join)
  {
    JoinPath *jp = (JoinPath *)path;

    for (i = 0; i < indent; i++)
    {
      printf("\t");
    }
    printf("  clauses: ");
    print_restrictclauses(root, jp->joinrestrictinfo);
    printf("\n");

    if (IsA(path, MergePath))
    {
      MergePath *mp = (MergePath *)path;

      for (i = 0; i < indent; i++)
      {
        printf("\t");
      }
      printf("  sortouter=%d sortinner=%d materializeinner=%d\n", ((mp->outersortkeys) ? 1 : 0), ((mp->innersortkeys) ? 1 : 0), ((mp->materialize_inner) ? 1 : 0));
    }

    print_path(root, jp->outerjoinpath, indent + 1);
    print_path(root, jp->innerjoinpath, indent + 1);
  }

  if (subpath)
  {
    print_path(root, subpath, indent + 1);
  }
}

void
debug_print_rel(PlannerInfo *root, RelOptInfo *rel)
{
  ListCell *l;

  printf("RELOPTINFO (");
  print_relids(root, rel->relids);
  printf("): rows=%.0f width=%d\n", rel->rows, rel->reltarget->width);

  if (rel->baserestrictinfo)
  {
    printf("\tbaserestrictinfo: ");
    print_restrictclauses(root, rel->baserestrictinfo);
    printf("\n");
  }

  if (rel->joininfo)
  {
    printf("\tjoininfo: ");
    print_restrictclauses(root, rel->joininfo);
    printf("\n");
  }

  printf("\tpath list:\n");
  foreach (l, rel->pathlist)
  {
    print_path(root, lfirst(l), 1);
  }
  if (rel->cheapest_parameterized_paths)
  {
    printf("\n\tcheapest parameterized paths:\n");
    foreach (l, rel->cheapest_parameterized_paths)
    {
      print_path(root, lfirst(l), 1);
    }
  }
  if (rel->cheapest_startup_path)
  {
    printf("\n\tcheapest startup path:\n");
    print_path(root, rel->cheapest_startup_path, 1);
  }
  if (rel->cheapest_total_path)
  {
    printf("\n\tcheapest total path:\n");
    print_path(root, rel->cheapest_total_path, 1);
  }
  printf("\n");
  fflush(stdout);
}

#endif                      
