                                                                            
   
               
                                                                         
                                                    
   
                                                                      
                                                                      
                                                                     
                                                                   
                                                                          
                                                                   
                                             
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/tlist.h"
#include "parser/parse_coerce.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

static RelOptInfo *
recurse_set_operations(Node *setOp, PlannerInfo *root, List *colTypes, List *colCollations, bool junkOK, int flag, List *refnames_tlist, List **pTargetList, double *pNumGroups);
static RelOptInfo *
generate_recursion_path(SetOperationStmt *setOp, PlannerInfo *root, List *refnames_tlist, List **pTargetList);
static RelOptInfo *
generate_union_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList);
static RelOptInfo *
generate_nonunion_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList);
static List *
plan_union_children(PlannerInfo *root, SetOperationStmt *top_union, List *refnames_tlist, List **tlist_list);
static Path *
make_union_unique(SetOperationStmt *op, Path *path, List *tlist, PlannerInfo *root);
static void
postprocess_setop_rel(PlannerInfo *root, RelOptInfo *rel);
static bool
choose_hashed_setop(PlannerInfo *root, List *groupClauses, Path *input_path, double dNumGroups, double dNumOutputRows, const char *construct);
static List *
generate_setop_tlist(List *colTypes, List *colCollations, int flag, Index varno, bool hack_constants, List *input_tlist, List *refnames_tlist);
static List *
generate_append_tlist(List *colTypes, List *colCollations, bool flag, List *input_tlists, List *refnames_tlist);
static List *
generate_setop_grouplist(SetOperationStmt *op, List *targetlist);

   
                       
   
                                                                             
   
                                                                           
                                                                               
                                                           
   
                                                                           
                                                                               
                                                                            
   
RelOptInfo *
plan_set_operations(PlannerInfo *root)
{
  Query *parse = root->parse;
  SetOperationStmt *topop = castNode(SetOperationStmt, parse->setOperations);
  Node *node;
  RangeTblEntry *leftmostRTE;
  Query *leftmostQuery;
  RelOptInfo *setop_rel;
  List *top_tlist;

  Assert(topop);

                                   
  Assert(parse->jointree->fromlist == NIL);
  Assert(parse->jointree->quals == NULL);
  Assert(parse->groupClause == NIL);
  Assert(parse->havingQual == NULL);
  Assert(parse->windowClause == NIL);
  Assert(parse->distinctClause == NIL);

     
                                                                            
                                                                           
                      
     
  setup_simple_rel_arrays(root);

     
                                                                       
                             
     
  setup_append_rel_array(root);

     
                                                                             
                                                               
     
  node = topop->larg;
  while (node && IsA(node, SetOperationStmt))
  {
    node = ((SetOperationStmt *)node)->larg;
  }
  Assert(node && IsA(node, RangeTblRef));
  leftmostRTE = root->simple_rte_array[((RangeTblRef *)node)->rtindex];
  leftmostQuery = leftmostRTE->subquery;
  Assert(leftmostQuery != NULL);

     
                                                                            
     
  if (root->hasRecursion)
  {
    setop_rel = generate_recursion_path(topop, root, leftmostQuery->targetList, &top_tlist);
  }
  else
  {
       
                                                                        
                                                                         
                                                                     
                                                                     
       
    setop_rel = recurse_set_operations((Node *)topop, root, topop->colTypes, topop->colCollations, true, -1, leftmostQuery->targetList, &top_tlist, NULL);
  }

                                                               
  root->processed_tlist = top_tlist;

  return setop_rel;
}

   
                          
                                                             
   
                                                          
                                                                
                                                                    
                                                                       
                                                        
   
                                                                             
                                                                             
                                                                       
                                      
   
                                                                            
                                                                             
                                                                        
                                                                   
   
                                                                         
                                                                         
                                                           
   
static RelOptInfo *
recurse_set_operations(Node *setOp, PlannerInfo *root, List *colTypes, List *colCollations, bool junkOK, int flag, List *refnames_tlist, List **pTargetList, double *pNumGroups)
{
  RelOptInfo *rel = NULL;                          

                                                                      
  check_stack_depth();

  if (IsA(setOp, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)setOp;
    RangeTblEntry *rte = root->simple_rte_array[rtr->rtindex];
    Query *subquery = rte->subquery;
    PlannerInfo *subroot;
    RelOptInfo *final_rel;
    Path *subpath;
    Path *path;
    List *tlist;

    Assert(subquery != NULL);

                                                    
    rel = build_simple_rel(root, rtr->rtindex, NULL);

                                                                 
    Assert(root->plan_params == NIL);

                                                       
    subroot = rel->subroot = subquery_planner(root->glob, subquery, root, false, root->tuple_fraction);

       
                                                                        
                                                                      
       
    if (root->plan_params)
    {
      elog(ERROR, "unexpected outer reference in set operation subquery");
    }

                                                                   
    tlist = generate_setop_tlist(colTypes, colCollations, flag, rtr->rtindex, true, subroot->processed_tlist, refnames_tlist);
    rel->reltarget = create_pathtarget(root, tlist);

                                                       
    *pTargetList = tlist;

       
                                                                           
                                                            
                                       
       
    set_subquery_size_estimates(root, rel);

       
                                                                         
                                                 
       
    final_rel = fetch_upper_rel(subroot, UPPERREL_FINAL, NULL);
    rel->consider_parallel = final_rel->consider_parallel;

       
                                                                        
                                                       
                               
       
    subpath = get_cheapest_fractional_path(final_rel, root->tuple_fraction);

       
                                           
       
                                                                         
                                                                        
                                                                        
                          
       
    path = (Path *)create_subqueryscan_path(root, rel, subpath, NIL, NULL);

    add_path(rel, path);

       
                                                                         
                                                                           
                                              
       
    if (rel->consider_parallel && bms_is_empty(rel->lateral_relids) && final_rel->partial_pathlist != NIL)
    {
      Path *partial_subpath;
      Path *partial_path;

      partial_subpath = linitial(final_rel->partial_pathlist);
      partial_path = (Path *)create_subqueryscan_path(root, rel, partial_subpath, NIL, NULL);
      add_partial_path(rel, partial_path);
    }

       
                                                                           
                                                                     
                                                    
       
                                                                          
                                                                     
                                                                        
                                                                         
                                                              
                                                                          
                                                                      
                                                                         
                                                                      
       
    if (pNumGroups)
    {
      if (subquery->groupClause || subquery->groupingSets || subquery->distinctClause || subroot->hasHavingQual || subquery->hasAggs)
      {
        *pNumGroups = subpath->rows;
      }
      else
      {
        *pNumGroups = estimate_num_groups(subroot, get_tlist_exprs(subquery->targetList, false), subpath->rows, NULL);
      }
    }
  }
  else if (IsA(setOp, SetOperationStmt))
  {
    SetOperationStmt *op = (SetOperationStmt *)setOp;

                                                         
    if (op->op == SETOP_UNION)
    {
      rel = generate_union_paths(op, root, refnames_tlist, pTargetList);
    }
    else
    {
      rel = generate_nonunion_paths(op, root, refnames_tlist, pTargetList);
    }
    if (pNumGroups)
    {
      *pNumGroups = rel->rows;
    }

       
                                                                       
                       
       
                                                                          
                                                                           
                                                                         
                                                                         
                                                             
                                                                      
                                                              
                                              
       
    if (flag >= 0 || !tlist_same_datatypes(*pTargetList, colTypes, junkOK) || !tlist_same_collations(*pTargetList, colCollations, junkOK))
    {
      PathTarget *target;
      ListCell *lc;

      *pTargetList = generate_setop_tlist(colTypes, colCollations, flag, 0, false, *pTargetList, refnames_tlist);
      target = create_pathtarget(root, *pTargetList);

                                         
      foreach (lc, rel->pathlist)
      {
        Path *subpath = (Path *)lfirst(lc);
        Path *path;

        Assert(subpath->param_info == NULL);
        path = apply_projection_to_path(root, subpath->parent, subpath, target);
                                                                       
        if (path != subpath)
        {
          lfirst(lc) = path;
        }
      }

                                                 
      foreach (lc, rel->partial_pathlist)
      {
        Path *subpath = (Path *)lfirst(lc);
        Path *path;

        Assert(subpath->param_info == NULL);

                                                                      
        path = (Path *)create_projection_path(root, subpath->parent, subpath, target);
        lfirst(lc) = path;
      }
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(setOp));
    *pTargetList = NIL;
  }

  postprocess_setop_rel(root, rel);

  return rel;
}

   
                                             
   
static RelOptInfo *
generate_recursion_path(SetOperationStmt *setOp, PlannerInfo *root, List *refnames_tlist, List **pTargetList)
{
  RelOptInfo *result_rel;
  Path *path;
  RelOptInfo *lrel, *rrel;
  Path *lpath;
  Path *rpath;
  List *lpath_tlist;
  List *rpath_tlist;
  List *tlist;
  List *groupList;
  double dNumGroups;

                                               
  if (setOp->op != SETOP_UNION)
  {
    elog(ERROR, "only UNION queries can be recursive");
  }
                                       
  Assert(root->wt_param_id >= 0);

     
                                                                    
                                                                         
     
  lrel = recurse_set_operations(setOp->larg, root, setOp->colTypes, setOp->colCollations, false, -1, refnames_tlist, &lpath_tlist, NULL);
  lpath = lrel->cheapest_total_path;
                                                            
  root->non_recursive_path = lpath;
  rrel = recurse_set_operations(setOp->rarg, root, setOp->colTypes, setOp->colCollations, false, -1, refnames_tlist, &rpath_tlist, NULL);
  rpath = rrel->cheapest_total_path;
  root->non_recursive_path = NULL;

     
                                                                             
     
  tlist = generate_append_tlist(setOp->colTypes, setOp->colCollations, false, list_make2(lpath_tlist, rpath_tlist), refnames_tlist);

  *pTargetList = tlist;

                              
  result_rel = fetch_upper_rel(root, UPPERREL_SETOP, bms_union(lrel->relids, rrel->relids));
  result_rel->reltarget = create_pathtarget(root, tlist);

     
                                               
     
  if (setOp->all)
  {
    groupList = NIL;
    dNumGroups = 0;
  }
  else
  {
                                         
    groupList = generate_setop_grouplist(setOp, tlist);

                                      
    if (!grouping_is_hashable(groupList))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not implement recursive UNION"), errdetail("All column datatypes must be hashable.")));
    }

       
                                                                          
                                             
       
    dNumGroups = lpath->rows + rpath->rows * 10;
  }

     
                             
     
  path = (Path *)create_recursiveunion_path(root, result_rel, lpath, rpath, result_rel->reltarget, groupList, root->wt_param_id, dNumGroups);

  add_path(result_rel, path);
  postprocess_setop_rel(root, result_rel);
  return result_rel;
}

   
                                                
   
static RelOptInfo *
generate_union_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList)
{
  Relids relids = NULL;
  RelOptInfo *result_rel;
  double save_fraction = root->tuple_fraction;
  ListCell *lc;
  List *pathlist = NIL;
  List *partial_pathlist = NIL;
  bool partial_paths_valid = true;
  bool consider_parallel = true;
  List *rellist;
  List *tlist_list;
  List *tlist;
  Path *path;

     
                                                        
     
                                                                            
                                                                        
                                                                            
                                                                           
                                                                           
                                                                           
                                     
     
  if (!op->all)
  {
    root->tuple_fraction = 0.0;
  }

     
                                                                             
                                                                          
                                                                             
                                               
     
  rellist = plan_union_children(root, op, refnames_tlist, &tlist_list);

     
                                          
     
                                                                          
                                                                            
                         
     
  tlist = generate_append_tlist(op->colTypes, op->colCollations, false, tlist_list, refnames_tlist);

  *pTargetList = tlist;

                                       
  foreach (lc, rellist)
  {
    RelOptInfo *rel = lfirst(lc);

    pathlist = lappend(pathlist, rel->cheapest_total_path);

    if (consider_parallel)
    {
      if (!rel->consider_parallel)
      {
        consider_parallel = false;
        partial_paths_valid = false;
      }
      else if (rel->partial_pathlist == NIL)
      {
        partial_paths_valid = false;
      }
      else
      {
        partial_pathlist = lappend(partial_pathlist, linitial(rel->partial_pathlist));
      }
    }

    relids = bms_union(relids, rel->relids);
  }

                              
  result_rel = fetch_upper_rel(root, UPPERREL_SETOP, relids);
  result_rel->reltarget = create_pathtarget(root, tlist);
  result_rel->consider_parallel = consider_parallel;

     
                                        
     
  path = (Path *)create_append_path(root, result_rel, pathlist, NIL, NIL, NULL, 0, false, NIL, -1);

     
                                                                          
                                   
     
  if (!op->all)
  {
    path = make_union_unique(op, path, tlist, root);
  }

  add_path(result_rel, path);

     
                                                                             
                                                                           
                       
     
  result_rel->rows = path->rows;

     
                                                                           
                  
     
  if (partial_paths_valid)
  {
    Path *ppath;
    ListCell *lc;
    int parallel_workers = 0;

                                                                       
    foreach (lc, partial_pathlist)
    {
      Path *path = lfirst(lc);

      parallel_workers = Max(parallel_workers, path->parallel_workers);
    }
    Assert(parallel_workers > 0);

       
                                                                           
                                                                      
                                                                         
                                                               
                                
       
    if (enable_parallel_append)
    {
      parallel_workers = Max(parallel_workers, fls(list_length(partial_pathlist)));
      parallel_workers = Min(parallel_workers, max_parallel_workers_per_gather);
    }
    Assert(parallel_workers > 0);

    ppath = (Path *)create_append_path(root, result_rel, NIL, partial_pathlist, NIL, NULL, parallel_workers, enable_parallel_append, NIL, -1);
    ppath = (Path *)create_gather_path(root, result_rel, ppath, result_rel->reltarget, NULL, NULL);
    if (!op->all)
    {
      ppath = make_union_unique(op, ppath, tlist, root);
    }
    add_path(result_rel, ppath);
  }

                                                            
  root->tuple_fraction = save_fraction;

  return result_rel;
}

   
                                                                              
   
static RelOptInfo *
generate_nonunion_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList)
{
  RelOptInfo *result_rel;
  RelOptInfo *lrel, *rrel;
  double save_fraction = root->tuple_fraction;
  Path *lpath, *rpath, *path;
  List *lpath_tlist, *rpath_tlist, *tlist_list, *tlist, *groupList, *pathlist;
  double dLeftGroups, dRightGroups, dNumGroups, dNumOutputRows;
  bool use_hash;
  SetOpCmd cmd;
  int firstFlag;

     
                                        
     
  root->tuple_fraction = 0.0;

                                                              
  lrel = recurse_set_operations(op->larg, root, op->colTypes, op->colCollations, false, 0, refnames_tlist, &lpath_tlist, &dLeftGroups);
  lpath = lrel->cheapest_total_path;
  rrel = recurse_set_operations(op->rarg, root, op->colTypes, op->colCollations, false, 1, refnames_tlist, &rpath_tlist, &dRightGroups);
  rpath = rrel->cheapest_total_path;

                                                   
  root->tuple_fraction = save_fraction;

     
                                                                          
                                                                          
                                                                        
                                                                   
     
  if (op->op == SETOP_EXCEPT || dLeftGroups <= dRightGroups)
  {
    pathlist = list_make2(lpath, rpath);
    tlist_list = list_make2(lpath_tlist, rpath_tlist);
    firstFlag = 0;
  }
  else
  {
    pathlist = list_make2(rpath, lpath);
    tlist_list = list_make2(rpath_tlist, lpath_tlist);
    firstFlag = 1;
  }

     
                                          
     
                                                                          
                                                                            
                                                                          
                                                                           
               
     
  tlist = generate_append_tlist(op->colTypes, op->colCollations, true, tlist_list, refnames_tlist);

  *pTargetList = tlist;

                              
  result_rel = fetch_upper_rel(root, UPPERREL_SETOP, bms_union(lrel->relids, rrel->relids));
  result_rel->reltarget = create_pathtarget(root, tlist);

     
                                        
     
  path = (Path *)create_append_path(root, result_rel, pathlist, NIL, NIL, NULL, 0, false, NIL, -1);

                                       
  groupList = generate_setop_grouplist(op, tlist);

     
                                                                          
                                                                             
                                                                             
                                                                          
                                                                     
                                                           
     
  if (op->op == SETOP_EXCEPT)
  {
    dNumGroups = dLeftGroups;
    dNumOutputRows = op->all ? lpath->rows : dNumGroups;
  }
  else
  {
    dNumGroups = Min(dLeftGroups, dRightGroups);
    dNumOutputRows = op->all ? Min(lpath->rows, rpath->rows) : dNumGroups;
  }

     
                                                                    
     
  use_hash = choose_hashed_setop(root, groupList, path, dNumGroups, dNumOutputRows, (op->op == SETOP_INTERSECT) ? "INTERSECT" : "EXCEPT");

  if (groupList && !use_hash)
  {
    path = (Path *)create_sort_path(root, result_rel, path, make_pathkeys_for_sortclauses(root, groupList, tlist), -1.0);
  }

     
                                                                    
     
  switch (op->op)
  {
  case SETOP_INTERSECT:
    cmd = op->all ? SETOPCMD_INTERSECT_ALL : SETOPCMD_INTERSECT;
    break;
  case SETOP_EXCEPT:
    cmd = op->all ? SETOPCMD_EXCEPT_ALL : SETOPCMD_EXCEPT;
    break;
  default:
    elog(ERROR, "unrecognized set op: %d", (int)op->op);
    cmd = SETOPCMD_INTERSECT;                          
    break;
  }
  path = (Path *)create_setop_path(root, result_rel, path, cmd, use_hash ? SETOP_HASHED : SETOP_SORTED, groupList, list_length(op->colTypes) + 1, use_hash ? firstFlag : -1, dNumGroups, dNumOutputRows);

  result_rel->rows = path->rows;
  add_path(result_rel, path);
  return result_rel;
}

   
                                                                            
   
                                                                          
                                    
   
                                                                          
                                                                        
                                                                     
                                                                         
                                                                       
                                                                             
   
static List *
plan_union_children(PlannerInfo *root, SetOperationStmt *top_union, List *refnames_tlist, List **tlist_list)
{
  List *pending_rels = list_make1(top_union);
  List *result = NIL;
  List *child_tlist;

  *tlist_list = NIL;

  while (pending_rels != NIL)
  {
    Node *setOp = linitial(pending_rels);

    pending_rels = list_delete_first(pending_rels);

    if (IsA(setOp, SetOperationStmt))
    {
      SetOperationStmt *op = (SetOperationStmt *)setOp;

      if (op->op == top_union->op && (op->all == top_union->all || op->all) && equal(op->colTypes, top_union->colTypes))
      {
                                                      
        pending_rels = lcons(op->rarg, pending_rels);
        pending_rels = lcons(op->larg, pending_rels);
        continue;
      }
    }

       
                                                
       
                                                                       
                                                                          
                                                                         
                                                                           
                                                                    
                       
       
    result = lappend(result, recurse_set_operations(setOp, root, top_union->colTypes, top_union->colCollations, false, -1, refnames_tlist, &child_tlist, NULL));
    *tlist_list = lappend(*tlist_list, child_tlist);
  }

  return result;
}

   
                                                                         
   
static Path *
make_union_unique(SetOperationStmt *op, Path *path, List *tlist, PlannerInfo *root)
{
  RelOptInfo *result_rel = fetch_upper_rel(root, UPPERREL_SETOP, NULL);
  List *groupList;
  double dNumGroups;

                                       
  groupList = generate_setop_grouplist(op, tlist);

     
                                                                            
                                                                             
                                                                            
                                                                           
                                                                            
                                                   
     
  dNumGroups = path->rows;

                                      
  if (choose_hashed_setop(root, groupList, path, dNumGroups, dNumGroups, "UNION"))
  {
                                                  
    path = (Path *)create_agg_path(root, result_rel, path, create_pathtarget(root, tlist), AGG_HASHED, AGGSPLIT_SIMPLE, groupList, NIL, NULL, dNumGroups);
  }
  else
  {
                         
    if (groupList)
    {
      path = (Path *)create_sort_path(root, result_rel, path, make_pathkeys_for_sortclauses(root, groupList, tlist), -1.0);
    }
    path = (Path *)create_upper_unique_path(root, result_rel, path, list_length(path->pathkeys), dNumGroups);
  }

  return path;
}

   
                                                                     
   
static void
postprocess_setop_rel(PlannerInfo *root, RelOptInfo *rel)
{
     
                                                                         
                                                  
     
  if (create_upper_paths_hook)
  {
    (*create_upper_paths_hook)(root, UPPERREL_SETOP, NULL, rel, NULL);
  }

                            
  set_cheapest(rel);
}

   
                                                                    
   
static bool
choose_hashed_setop(PlannerInfo *root, List *groupClauses, Path *input_path, double dNumGroups, double dNumOutputRows, const char *construct)
{
  int numGroupCols = list_length(groupClauses);
  bool can_sort;
  bool can_hash;
  Size hashentrysize;
  Path hashed_p;
  Path sorted_p;
  double tuple_fraction;

                                                              
  can_sort = grouping_is_sortable(groupClauses);
  can_hash = grouping_is_hashable(groupClauses);
  if (can_hash && can_sort)
  {
                                                           
  }
  else if (can_hash)
  {
    return true;
  }
  else if (can_sort)
  {
    return false;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                          
                       errmsg("could not implement %s", construct), errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
  }

                                                 
  if (!enable_hashagg)
  {
    return false;
  }

     
                                                                     
               
     
  hashentrysize = MAXALIGN(input_path->pathtarget->width) + MAXALIGN(SizeofMinimalTupleHeader);

  if (hashentrysize * dNumGroups > work_mem * 1024L)
  {
    return false;
  }

     
                                                                       
     
                                                                         
                                                                       
                                                                             
                                                   
     
                                                                           
                                        
     
  cost_agg(&hashed_p, root, AGG_HASHED, NULL, numGroupCols, dNumGroups, NIL, input_path->startup_cost, input_path->total_cost, input_path->rows);

     
                                                                         
                                                                      
     
  sorted_p.startup_cost = input_path->startup_cost;
  sorted_p.total_cost = input_path->total_cost;
                                                                         
  cost_sort(&sorted_p, root, NIL, sorted_p.total_cost, input_path->rows, input_path->pathtarget->width, 0.0, work_mem, -1.0);
  cost_group(&sorted_p, root, numGroupCols, dNumGroups, NIL, sorted_p.startup_cost, sorted_p.total_cost, input_path->rows);

     
                                                                         
                                                                     
     
  tuple_fraction = root->tuple_fraction;
  if (tuple_fraction >= 1.0)
  {
    tuple_fraction /= dNumOutputRows;
  }

  if (compare_fractional_path_costs(&hashed_p, &sorted_p, tuple_fraction) < 0)
  {
                                      
    return true;
  }
  return false;
}

   
                                                     
   
                                                          
                                                                
                                                                           
                                         
                                                                    
                                                     
                                                        
   
static List *
generate_setop_tlist(List *colTypes, List *colCollations, int flag, Index varno, bool hack_constants, List *input_tlist, List *refnames_tlist)
{
  List *tlist = NIL;
  int resno = 1;
  ListCell *ctlc, *cclc, *itlc, *rtlc;
  TargetEntry *tle;
  Node *expr;

  forfour(ctlc, colTypes, cclc, colCollations, itlc, input_tlist, rtlc, refnames_tlist)
  {
    Oid colType = lfirst_oid(ctlc);
    Oid colColl = lfirst_oid(cclc);
    TargetEntry *inputtle = (TargetEntry *)lfirst(itlc);
    TargetEntry *reftle = (TargetEntry *)lfirst(rtlc);

    Assert(inputtle->resno == resno);
    Assert(reftle->resno == resno);
    Assert(!inputtle->resjunk);
    Assert(!reftle->resjunk);

       
                                                                         
                                                                     
                  
       
                                                                     
                                                                         
                                                                        
                                                                        
                                                                          
                                                                     
              
       
    if (hack_constants && inputtle->expr && IsA(inputtle->expr, Const))
    {
      expr = (Node *)inputtle->expr;
    }
    else
    {
      expr = (Node *)makeVar(varno, inputtle->resno, exprType((Node *)inputtle->expr), exprTypmod((Node *)inputtle->expr), exprCollation((Node *)inputtle->expr), 0);
    }

    if (exprType(expr) != colType)
    {
         
                                                                         
                                                                      
                                                                       
                                                                       
                                                                         
                                                              
         
      expr = coerce_to_common_type(NULL,                       
          expr, colType, "UNION/INTERSECT/EXCEPT");
    }

       
                                                                           
                                                                     
                                                                           
                                                                         
                                                                  
                             
       
                                                                       
                                                               
       
    if (exprCollation(expr) != colColl)
    {
      expr = applyRelabelType(expr, exprType(expr), exprTypmod(expr), colColl, COERCE_IMPLICIT_CAST, -1, false);
    }

    tle = makeTargetEntry((Expr *)expr, (AttrNumber)resno++, pstrdup(reftle->resname), false);

       
                                                                   
                                                                          
                                                                         
       
    tle->ressortgroupref = tle->resno;

    tlist = lappend(tlist, tle);
  }

  if (flag >= 0)
  {
                                   
                                          
    expr = (Node *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), Int32GetDatum(flag), false, true);
    tle = makeTargetEntry((Expr *)expr, (AttrNumber)resno++, pstrdup("flag"), true);
    tlist = lappend(tlist, tle);
  }

  return tlist;
}

   
                                                       
   
                                                          
                                                                
                                                              
                                                            
                                                        
   
                                                                        
                                                                               
                                               
   
                                                                                
                                                                           
                                                                         
   
static List *
generate_append_tlist(List *colTypes, List *colCollations, bool flag, List *input_tlists, List *refnames_tlist)
{
  List *tlist = NIL;
  int resno = 1;
  ListCell *curColType;
  ListCell *curColCollation;
  ListCell *ref_tl_item;
  int colindex;
  TargetEntry *tle;
  Node *expr;
  ListCell *tlistl;
  int32 *colTypmods;

     
                                   
     
                                                                            
                               
     
  colTypmods = (int32 *)palloc(list_length(colTypes) * sizeof(int32));

  foreach (tlistl, input_tlists)
  {
    List *subtlist = (List *)lfirst(tlistl);
    ListCell *subtlistl;

    curColType = list_head(colTypes);
    colindex = 0;
    foreach (subtlistl, subtlist)
    {
      TargetEntry *subtle = (TargetEntry *)lfirst(subtlistl);

      if (subtle->resjunk)
      {
        continue;
      }
      Assert(curColType != NULL);
      if (exprType((Node *)subtle->expr) == lfirst_oid(curColType))
      {
                                                             
        int32 subtypmod = exprTypmod((Node *)subtle->expr);

        if (tlistl == list_head(input_tlists))
        {
          colTypmods[colindex] = subtypmod;
        }
        else if (subtypmod != colTypmods[colindex])
        {
          colTypmods[colindex] = -1;
        }
      }
      else
      {
                                                   
        colTypmods[colindex] = -1;
      }
      curColType = lnext(curColType);
      colindex++;
    }
    Assert(curColType == NULL);
  }

     
                                                
     
  colindex = 0;
  forthree(curColType, colTypes, curColCollation, colCollations, ref_tl_item, refnames_tlist)
  {
    Oid colType = lfirst_oid(curColType);
    int32 colTypmod = colTypmods[colindex++];
    Oid colColl = lfirst_oid(curColCollation);
    TargetEntry *reftle = (TargetEntry *)lfirst(ref_tl_item);

    Assert(reftle->resno == resno);
    Assert(!reftle->resjunk);
    expr = (Node *)makeVar(0, resno, colType, colTypmod, colColl, 0);
    tle = makeTargetEntry((Expr *)expr, (AttrNumber)resno++, pstrdup(reftle->resname), false);

       
                                                                   
                                                                          
                                                                         
       
    tle->ressortgroupref = tle->resno;

    tlist = lappend(tlist, tle);
  }

  if (flag)
  {
                                   
                                                       
    expr = (Node *)makeVar(0, resno, INT4OID, -1, InvalidOid, 0);
    tle = makeTargetEntry((Expr *)expr, (AttrNumber)resno++, pstrdup("flag"), true);
    tlist = lappend(tlist, tle);
  }

  pfree(colTypmods);

  return tlist;
}

   
                            
                                                                       
                                   
   
                                                                         
                                                                       
                                                                     
                                                                    
                                                                     
   
static List *
generate_setop_grouplist(SetOperationStmt *op, List *targetlist)
{
  List *grouplist = copyObject(op->groupClauses);
  ListCell *lg;
  ListCell *lt;

  lg = list_head(grouplist);
  foreach (lt, targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lt);
    SortGroupClause *sgc;

    if (tle->resjunk)
    {
                                                         
      Assert(tle->ressortgroupref == 0);
      continue;                             
    }

                                                              
    Assert(tle->ressortgroupref == tle->resno);

                                                          
    Assert(lg != NULL);
    sgc = (SortGroupClause *)lfirst(lg);
    lg = lnext(lg);
    Assert(sgc->tleSortGroupRef == 0);

    sgc->tleSortGroupRef = tle->ressortgroupref;
  }
  Assert(lg == NULL);
  return grouplist;
}
