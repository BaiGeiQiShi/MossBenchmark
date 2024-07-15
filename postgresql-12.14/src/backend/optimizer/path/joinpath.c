                                                                            
   
              
                                                                       
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"

                                                               
set_join_pathlist_hook_type set_join_pathlist_hook = NULL;

   
                                                                              
                     
   
#define PATH_PARAM_BY_PARENT(path, rel) ((path)->param_info && bms_overlap(PATH_REQ_OUTER(path), (rel)->top_parent_relids))
#define PATH_PARAM_BY_REL_SELF(path, rel) ((path)->param_info && bms_overlap(PATH_REQ_OUTER(path), (rel)->relids))

#define PATH_PARAM_BY_REL(path, rel) (PATH_PARAM_BY_REL_SELF(path, rel) || PATH_PARAM_BY_PARENT(path, rel))

static void
try_partial_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, List *mergeclauses, List *outersortkeys, List *innersortkeys, JoinType jointype, JoinPathExtraData *extra);
static void
sort_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static void
match_unsorted_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static void
consider_parallel_nestloop(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static void
consider_parallel_mergejoin(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra, Path *inner_cheapest_total);
static void
hash_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static List *
select_mergejoin_clauses(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, List *restrictlist, JoinType jointype, bool *mergejoin_allowed);
static void
generate_mergejoin_paths(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *innerrel, Path *outerpath, JoinType jointype, JoinPathExtraData *extra, bool useallclauses, Path *inner_cheapest_total, List *merge_pathkeys, bool is_partial);

   
                        
                                                                             
                                                                          
                                                                             
                                                                          
                                               
   
                                                                       
                       
   
                                                                         
                                                                           
                                              
   
                                                                             
                                                                      
                                                                         
                                                                         
                                                                       
                                                                          
                                                      
   
void
add_paths_to_joinrel(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, SpecialJoinInfo *sjinfo, List *restrictlist)
{
  JoinPathExtraData extra;
  bool mergejoin_allowed = true;
  ListCell *lc;
  Relids joinrelids;

     
                                                                        
                                                                          
                                                                            
                                                                        
                 
     
  if (joinrel->reloptkind == RELOPT_OTHER_JOINREL)
  {
    joinrelids = joinrel->top_parent_relids;
  }
  else
  {
    joinrelids = joinrel->relids;
  }

  extra.restrictlist = restrictlist;
  extra.mergeclause_list = NIL;
  extra.sjinfo = sjinfo;
  extra.param_source_rels = NULL;

     
                                                                      
     
                                                                         
                                                                            
                                                                             
                                                                            
                                                                          
                                                                             
                                                                            
                                                                             
                                                                           
                                                                           
                                            
     
  switch (jointype)
  {
  case JOIN_SEMI:
  case JOIN_ANTI:
    extra.inner_unique = false;                     
    break;
  case JOIN_UNIQUE_INNER:
    extra.inner_unique = bms_is_subset(sjinfo->min_lefthand, outerrel->relids);
    break;
  case JOIN_UNIQUE_OUTER:
    extra.inner_unique = innerrel_is_unique(root, joinrel->relids, outerrel->relids, innerrel, JOIN_INNER, restrictlist, false);
    break;
  default:
    extra.inner_unique = innerrel_is_unique(root, joinrel->relids, outerrel->relids, innerrel, jointype, restrictlist, false);
    break;
  }

     
                                                                       
                                                                          
                                                                            
                       
     
  if (enable_mergejoin || jointype == JOIN_FULL)
  {
    extra.mergeclause_list = select_mergejoin_clauses(root, joinrel, outerrel, innerrel, restrictlist, jointype, &mergejoin_allowed);
  }

     
                                                                          
                                                                 
     
  if (jointype == JOIN_SEMI || jointype == JOIN_ANTI || extra.inner_unique)
  {
    compute_semi_anti_join_factors(root, joinrel, outerrel, innerrel, jointype, sjinfo, restrictlist, &extra.semifactors);
  }

     
                                                                           
                                                                           
                                                                           
                                                                             
                                                                          
                                                                      
                                                                       
                                                                           
                                                                             
                                                 
     
  foreach (lc, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo2 = (SpecialJoinInfo *)lfirst(lc);

       
                                                                   
                                                                           
                                                                           
                                                                       
                                                                    
       
    if (bms_overlap(joinrelids, sjinfo2->min_righthand) && !bms_overlap(joinrelids, sjinfo2->min_lefthand))
    {
      extra.param_source_rels = bms_join(extra.param_source_rels, bms_difference(root->all_baserels, sjinfo2->min_righthand));
    }

                                                       
    if (sjinfo2->jointype == JOIN_FULL && bms_overlap(joinrelids, sjinfo2->min_lefthand) && !bms_overlap(joinrelids, sjinfo2->min_righthand))
    {
      extra.param_source_rels = bms_join(extra.param_source_rels, bms_difference(root->all_baserels, sjinfo2->min_lefthand));
    }
  }

     
                                                                            
                                                                         
                                                                           
                                                                            
                                                                      
     
  extra.param_source_rels = bms_add_members(extra.param_source_rels, joinrel->lateral_relids);

     
                                                                         
                                               
     
  if (mergejoin_allowed)
  {
    sort_inner_and_outer(root, joinrel, outerrel, innerrel, jointype, &extra);
  }

     
                                                                       
                                                                         
                                                                       
                                                                        
                                                                        
     
  if (mergejoin_allowed)
  {
    match_unsorted_outer(root, joinrel, outerrel, innerrel, jointype, &extra);
  }

#ifdef NOT_USED

     
                                                                       
                                                                             
                            
     
                                                                      
                                                                             
                                                                        
                                                                       
                                                         
     
  if (mergejoin_allowed)
  {
    match_unsorted_inner(root, joinrel, outerrel, innerrel, jointype, &extra);
  }
#endif

     
                                                                           
                                                                        
                                                       
     
  if (enable_hashjoin || jointype == JOIN_FULL)
  {
    hash_inner_and_outer(root, joinrel, outerrel, innerrel, jointype, &extra);
  }

     
                                                                             
                                                                      
                                                               
     
  if (joinrel->fdwroutine && joinrel->fdwroutine->GetForeignJoinPaths)
  {
    joinrel->fdwroutine->GetForeignJoinPaths(root, joinrel, outerrel, innerrel, jointype, &extra);
  }

     
                                                                       
     
  if (set_join_pathlist_hook)
  {
    set_join_pathlist_hook(root, joinrel, outerrel, innerrel, jointype, &extra);
  }
}

   
                                                                           
                                                                      
                                                                          
                                                                          
                                                                              
                                                                             
                                                                         
                                                                           
                                                                              
                                                                          
   
                                                                              
                                                             
   
static inline bool
allow_star_schema_join(PlannerInfo *root, Relids outerrelids, Relids inner_paramrels)
{
     
                                                                           
                                       
     
  return (bms_overlap(inner_paramrels, outerrelids) && bms_nonempty_difference(inner_paramrels, outerrelids));
}

   
                     
                                                                       
                                            
   
static void
try_nestloop_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, JoinType jointype, JoinPathExtraData *extra)
{
  Relids required_outer;
  JoinCostWorkspace workspace;
  RelOptInfo *innerrel = inner_path->parent;
  RelOptInfo *outerrel = outer_path->parent;
  Relids innerrelids;
  Relids outerrelids;
  Relids inner_paramrels = PATH_REQ_OUTER(inner_path);
  Relids outer_paramrels = PATH_REQ_OUTER(outer_path);

     
                                                                           
                                 
     
  if (innerrel->top_parent_relids)
  {
    innerrelids = innerrel->top_parent_relids;
  }
  else
  {
    innerrelids = innerrel->relids;
  }

  if (outerrel->top_parent_relids)
  {
    outerrelids = outerrel->top_parent_relids;
  }
  else
  {
    outerrelids = outerrel->relids;
  }

     
                                                                             
                                                                             
                                                                          
                                                                             
                          
     
  required_outer = calc_nestloop_required_outer(outerrelids, outer_paramrels, innerrelids, inner_paramrels);
  if (required_outer && ((!bms_overlap(required_outer, extra->param_source_rels) && !allow_star_schema_join(root, outerrelids, inner_paramrels)) || have_dangerous_phv(root, outerrelids, inner_paramrels)))
  {
                                                    
    bms_free(required_outer);
    return;
  }

     
                                                                      
                                                                   
                                                                             
                                                                             
                                                                            
                                                                      
                             
     
  initial_cost_nestloop(root, &workspace, jointype, outer_path, inner_path, extra);

  if (add_path_precheck(joinrel, workspace.startup_cost, workspace.total_cost, pathkeys, required_outer))
  {
       
                                                                      
                                                                       
             
       
    if (PATH_PARAM_BY_PARENT(inner_path, outer_path->parent))
    {
      inner_path = reparameterize_path_by_child(root, inner_path, outer_path->parent);

         
                                                                       
               
         
      if (!inner_path)
      {
        bms_free(required_outer);
        return;
      }
    }

    add_path(joinrel, (Path *)create_nestloop_path(root, joinrel, jointype, &workspace, extra, outer_path, inner_path, extra->restrictlist, pathkeys, required_outer));
  }
  else
  {
                                                    
    bms_free(required_outer);
  }
}

   
                             
                                                                               
                                                            
   
static void
try_partial_nestloop_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, JoinType jointype, JoinPathExtraData *extra)
{
  JoinCostWorkspace workspace;

     
                                                                            
                                                                            
                                                                     
                                           
     
  Assert(bms_is_empty(joinrel->lateral_relids));
  if (inner_path->param_info != NULL)
  {
    Relids inner_paramrels = inner_path->param_info->ppi_req_outer;
    RelOptInfo *outerrel = outer_path->parent;
    Relids outerrelids;

       
                                                                          
                                                                           
                                       
       
    if (outerrel->top_parent_relids)
    {
      outerrelids = outerrel->top_parent_relids;
    }
    else
    {
      outerrelids = outerrel->relids;
    }

    if (!bms_is_subset(inner_paramrels, outerrelids))
    {
      return;
    }
  }

     
                                                                             
                                                      
     
  initial_cost_nestloop(root, &workspace, jointype, outer_path, inner_path, extra);
  if (!add_partial_path_precheck(joinrel, workspace.total_cost, pathkeys))
  {
    return;
  }

     
                                                                            
                                                                   
     
  if (PATH_PARAM_BY_PARENT(inner_path, outer_path->parent))
  {
    inner_path = reparameterize_path_by_child(root, inner_path, outer_path->parent);

       
                                                                           
       
    if (!inner_path)
    {
      return;
    }
  }

                                                                 
  add_partial_path(joinrel, (Path *)create_nestloop_path(root, joinrel, jointype, &workspace, extra, outer_path, inner_path, extra->restrictlist, pathkeys, NULL));
}

   
                      
                                                                    
                                            
   
static void
try_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, List *mergeclauses, List *outersortkeys, List *innersortkeys, JoinType jointype, JoinPathExtraData *extra, bool is_partial)
{
  Relids required_outer;
  JoinCostWorkspace workspace;

  if (is_partial)
  {
    try_partial_mergejoin_path(root, joinrel, outer_path, inner_path, pathkeys, mergeclauses, outersortkeys, innersortkeys, jointype, extra);
    return;
  }

     
                                                                             
                                            
     
  required_outer = calc_non_nestloop_required_outer(outer_path, inner_path);
  if (required_outer && !bms_overlap(required_outer, extra->param_source_rels))
  {
                                                    
    bms_free(required_outer);
    return;
  }

     
                                                                           
                       
     
  if (outersortkeys && pathkeys_contained_in(outersortkeys, outer_path->pathkeys))
  {
    outersortkeys = NIL;
  }
  if (innersortkeys && pathkeys_contained_in(innersortkeys, inner_path->pathkeys))
  {
    innersortkeys = NIL;
  }

     
                                          
     
  initial_cost_mergejoin(root, &workspace, jointype, mergeclauses, outer_path, inner_path, outersortkeys, innersortkeys, extra);

  if (add_path_precheck(joinrel, workspace.startup_cost, workspace.total_cost, pathkeys, required_outer))
  {
    add_path(joinrel, (Path *)create_mergejoin_path(root, joinrel, jointype, &workspace, extra, outer_path, inner_path, extra->restrictlist, pathkeys, required_outer, mergeclauses, outersortkeys, innersortkeys));
  }
  else
  {
                                                    
    bms_free(required_outer);
  }
}

   
                              
                                                                            
                                                    
   
static void
try_partial_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, List *mergeclauses, List *outersortkeys, List *innersortkeys, JoinType jointype, JoinPathExtraData *extra)
{
  JoinCostWorkspace workspace;

     
                                                  
     
  Assert(bms_is_empty(joinrel->lateral_relids));
  if (inner_path->param_info != NULL)
  {
    Relids inner_paramrels = inner_path->param_info->ppi_req_outer;

    if (!bms_is_empty(inner_paramrels))
    {
      return;
    }
  }

     
                                                                           
                       
     
  if (outersortkeys && pathkeys_contained_in(outersortkeys, outer_path->pathkeys))
  {
    outersortkeys = NIL;
  }
  if (innersortkeys && pathkeys_contained_in(innersortkeys, inner_path->pathkeys))
  {
    innersortkeys = NIL;
  }

     
                                                  
     
  initial_cost_mergejoin(root, &workspace, jointype, mergeclauses, outer_path, inner_path, outersortkeys, innersortkeys, extra);

  if (!add_partial_path_precheck(joinrel, workspace.total_cost, pathkeys))
  {
    return;
  }

                                                                 
  add_partial_path(joinrel, (Path *)create_mergejoin_path(root, joinrel, jointype, &workspace, extra, outer_path, inner_path, extra->restrictlist, pathkeys, NULL, mergeclauses, outersortkeys, innersortkeys));
}

   
                     
                                                                   
                                            
   
static void
try_hashjoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *hashclauses, JoinType jointype, JoinPathExtraData *extra)
{
  Relids required_outer;
  JoinCostWorkspace workspace;

     
                                                                             
                                            
     
  required_outer = calc_non_nestloop_required_outer(outer_path, inner_path);
  if (required_outer && !bms_overlap(required_outer, extra->param_source_rels))
  {
                                                    
    bms_free(required_outer);
    return;
  }

     
                                                                         
                                                                           
     
  initial_cost_hashjoin(root, &workspace, jointype, hashclauses, outer_path, inner_path, extra, false);

  if (add_path_precheck(joinrel, workspace.startup_cost, workspace.total_cost, NIL, required_outer))
  {
    add_path(joinrel, (Path *)create_hashjoin_path(root, joinrel, jointype, &workspace, extra, outer_path, inner_path, false,                    
                          extra->restrictlist, required_outer, hashclauses));
  }
  else
  {
                                                    
    bms_free(required_outer);
  }
}

   
                             
                                                                               
                                                            
                                                                               
                                                                              
                                                                             
                                                                               
   
static void
try_partial_hashjoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *hashclauses, JoinType jointype, JoinPathExtraData *extra, bool parallel_hash)
{
  JoinCostWorkspace workspace;

     
                                                                            
                                                                            
                                                                     
                                           
     
  Assert(bms_is_empty(joinrel->lateral_relids));
  if (inner_path->param_info != NULL)
  {
    Relids inner_paramrels = inner_path->param_info->ppi_req_outer;

    if (!bms_is_empty(inner_paramrels))
    {
      return;
    }
  }

     
                                                                             
                                                      
     
  initial_cost_hashjoin(root, &workspace, jointype, hashclauses, outer_path, inner_path, extra, parallel_hash);
  if (!add_partial_path_precheck(joinrel, workspace.total_cost, NIL))
  {
    return;
  }

                                                                 
  add_partial_path(joinrel, (Path *)create_hashjoin_path(root, joinrel, jointype, &workspace, extra, outer_path, inner_path, parallel_hash, extra->restrictlist, NULL, hashclauses));
}

   
                           
                                                                               
   
                                                                             
                                                                            
                                                                              
                                                                           
                                                                            
   
static inline bool
clause_sides_match_join(RestrictInfo *rinfo, RelOptInfo *outerrel, RelOptInfo *innerrel)
{
  if (bms_is_subset(rinfo->left_relids, outerrel->relids) && bms_is_subset(rinfo->right_relids, innerrel->relids))
  {
                                
    rinfo->outer_is_left = true;
    return true;
  }
  else if (bms_is_subset(rinfo->left_relids, innerrel->relids) && bms_is_subset(rinfo->right_relids, outerrel->relids))
  {
                                 
    rinfo->outer_is_left = false;
    return true;
  }
  return false;                                        
}

   
                        
                                                                          
                                                            
   
                                  
                                         
                                         
                                        
                                            
   
static void
sort_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{
  JoinType save_jointype = jointype;
  Path *outer_path;
  Path *inner_path;
  Path *cheapest_partial_outer = NULL;
  Path *cheapest_safe_inner = NULL;
  List *all_pathkeys;
  ListCell *l;

     
                                                                        
                                                              
                                                                            
           
     
                                                                       
                                                                            
                                                                       
                                                                            
                                                                   
                                   
     
  outer_path = outerrel->cheapest_total_path;
  inner_path = innerrel->cheapest_total_path;

     
                                                                         
                                                                           
                                                                            
             
     
  if (PATH_PARAM_BY_REL(outer_path, innerrel) || PATH_PARAM_BY_REL(inner_path, outerrel))
  {
    return;
  }

     
                                                                        
                 
     
  if (jointype == JOIN_UNIQUE_OUTER)
  {
    outer_path = (Path *)create_unique_path(root, outerrel, outer_path, extra->sjinfo);
    Assert(outer_path);
    jointype = JOIN_INNER;
  }
  else if (jointype == JOIN_UNIQUE_INNER)
  {
    inner_path = (Path *)create_unique_path(root, innerrel, inner_path, extra->sjinfo);
    Assert(inner_path);
    jointype = JOIN_INNER;
  }

     
                                                                           
                                                                          
                                                                            
                                                                     
                                                                           
                                                   
     
  if (joinrel->consider_parallel && save_jointype != JOIN_UNIQUE_OUTER && save_jointype != JOIN_FULL && save_jointype != JOIN_RIGHT && outerrel->partial_pathlist != NIL && bms_is_empty(joinrel->lateral_relids))
  {
    cheapest_partial_outer = (Path *)linitial(outerrel->partial_pathlist);

    if (inner_path->parallel_safe)
    {
      cheapest_safe_inner = inner_path;
    }
    else if (save_jointype != JOIN_UNIQUE_INNER)
    {
      cheapest_safe_inner = get_cheapest_parallel_safe_total_inner(innerrel->pathlist);
    }
  }

     
                                                                             
                                                                             
                                                                          
                                                                      
                                                               
     
                                                                        
                                                                         
                                                                             
                                                                       
                                                                      
     
                                                                            
                                                                         
                                                                            
                                                                            
                                                                       
                                                                             
                                                                            
                                                                           
                                                                           
                                        
     
                                                                         
                                                                         
                                               
     
  all_pathkeys = select_outer_pathkeys_for_merge(root, extra->mergeclause_list, joinrel);

  foreach (l, all_pathkeys)
  {
    PathKey *front_pathkey = (PathKey *)lfirst(l);
    List *cur_mergeclauses;
    List *outerkeys;
    List *innerkeys;
    List *merge_pathkeys;

                                                 
    if (l != list_head(all_pathkeys))
    {
      outerkeys = lcons(front_pathkey, list_delete_ptr(list_copy(all_pathkeys), front_pathkey));
    }
    else
    {
      outerkeys = all_pathkeys;                              
    }

                                                               
    cur_mergeclauses = find_mergeclauses_for_outer_pathkeys(root, outerkeys, extra->mergeclause_list);

                                      
    Assert(list_length(cur_mergeclauses) == list_length(extra->mergeclause_list));

                                                
    innerkeys = make_inner_pathkeys_for_merge(root, cur_mergeclauses, outerkeys);

                                                       
    merge_pathkeys = build_join_pathkeys(root, joinrel, jointype, outerkeys);

       
                                     
       
                                                                          
                                                                           
                                                     
       
    try_mergejoin_path(root, joinrel, outer_path, inner_path, merge_pathkeys, cur_mergeclauses, outerkeys, innerkeys, jointype, extra, false);

       
                                                                      
                               
       
    if (cheapest_partial_outer && cheapest_safe_inner)
    {
      try_partial_mergejoin_path(root, joinrel, cheapest_partial_outer, cheapest_safe_inner, merge_pathkeys, cur_mergeclauses, outerkeys, innerkeys, jointype, extra);
    }
  }
}

   
                            
                                                         
   
                                                                       
                                                                          
                                                                             
                                                                               
                                                                              
                                                                              
                                                                    
                                                                          
                                                                       
   
static void
generate_mergejoin_paths(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *innerrel, Path *outerpath, JoinType jointype, JoinPathExtraData *extra, bool useallclauses, Path *inner_cheapest_total, List *merge_pathkeys, bool is_partial)
{
  List *mergeclauses;
  List *innersortkeys;
  List *trialsortkeys;
  Path *cheapest_startup_inner;
  Path *cheapest_total_inner;
  JoinType save_jointype = jointype;
  int num_sortkeys;
  int sortkeycnt;

  if (jointype == JOIN_UNIQUE_OUTER || jointype == JOIN_UNIQUE_INNER)
  {
    jointype = JOIN_INNER;
  }

                                             
  mergeclauses = find_mergeclauses_for_outer_pathkeys(root, outerpath->pathkeys, extra->mergeclause_list);

     
                                                             
     
                                                                             
                                                                           
                                                                       
                                                                       
                             
     
  if (mergeclauses == NIL)
  {
    if (jointype == JOIN_FULL)
                                     ;
    else
    {
      return;
    }
  }
  if (useallclauses && list_length(mergeclauses) != list_length(extra->mergeclause_list))
  {
    return;
  }

                                                       
  innersortkeys = make_inner_pathkeys_for_merge(root, mergeclauses, outerpath->pathkeys);

     
                                                                            
                                                                   
                                                                           
                                
     
  try_mergejoin_path(root, joinrel, outerpath, inner_cheapest_total, merge_pathkeys, mergeclauses, NIL, innersortkeys, jointype, extra, is_partial);

                                                                 
  if (save_jointype == JOIN_UNIQUE_INNER)
  {
    return;
  }

     
                                                                           
                                                                             
                                                                          
                                
     
                                                                       
                                                                       
                                                           
                                   
     
                                                                            
                                                                           
                                                                           
                                                                     
                                                                         
                                                                         
                                                        
     
                                                                          
                                                                          
                                                                           
                                                                      
                                                           
                                                                    
                                                                        
                                                                        
                                           
     
  if (pathkeys_contained_in(innersortkeys, inner_cheapest_total->pathkeys))
  {
                                                    
    cheapest_startup_inner = inner_cheapest_total;
    cheapest_total_inner = inner_cheapest_total;
  }
  else
  {
                                                                  
    cheapest_startup_inner = NULL;
    cheapest_total_inner = NULL;
  }
  num_sortkeys = list_length(innersortkeys);
  if (num_sortkeys > 1 && !useallclauses)
  {
    trialsortkeys = list_copy(innersortkeys);                           
  }
  else
  {
    trialsortkeys = innersortkeys;                            
  }

  for (sortkeycnt = num_sortkeys; sortkeycnt > 0; sortkeycnt--)
  {
    Path *innerpath;
    List *newclauses = NIL;

       
                                                                
                                                                       
                                                     
       
    trialsortkeys = list_truncate(trialsortkeys, sortkeycnt);
    innerpath = get_cheapest_path_for_pathkeys(innerrel->pathlist, trialsortkeys, NULL, TOTAL_COST, is_partial);
    if (innerpath != NULL && (cheapest_total_inner == NULL || compare_path_costs(innerpath, cheapest_total_inner, TOTAL_COST) < 0))
    {
                                                       
                                                               
      if (sortkeycnt < num_sortkeys)
      {
        newclauses = trim_mergeclauses_for_inner_pathkeys(root, mergeclauses, trialsortkeys);
        Assert(newclauses != NIL);
      }
      else
      {
        newclauses = mergeclauses;
      }
      try_mergejoin_path(root, joinrel, outerpath, innerpath, merge_pathkeys, newclauses, NIL, NIL, jointype, extra, is_partial);
      cheapest_total_inner = innerpath;
    }
                                                        
    innerpath = get_cheapest_path_for_pathkeys(innerrel->pathlist, trialsortkeys, NULL, STARTUP_COST, is_partial);
    if (innerpath != NULL && (cheapest_startup_inner == NULL || compare_path_costs(innerpath, cheapest_startup_inner, STARTUP_COST) < 0))
    {
                                                       
      if (innerpath != cheapest_total_inner)
      {
           
                                                                      
                                       
           
        if (newclauses == NIL)
        {
          if (sortkeycnt < num_sortkeys)
          {
            newclauses = trim_mergeclauses_for_inner_pathkeys(root, mergeclauses, trialsortkeys);
            Assert(newclauses != NIL);
          }
          else
          {
            newclauses = mergeclauses;
          }
        }
        try_mergejoin_path(root, joinrel, outerpath, innerpath, merge_pathkeys, newclauses, NIL, NIL, jointype, extra, is_partial);
      }
      cheapest_startup_inner = innerpath;
    }

       
                                                                 
       
    if (useallclauses)
    {
      break;
    }
  }
}

   
                        
                                                                       
                                                             
                                                                   
                                                                         
   
                                                                     
                                                                           
                                                                
                                                               
                                                                
                                                         
   
                                                                        
                                                  
   
                                  
                                         
                                         
                                        
                                            
   
static void
match_unsorted_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{
  JoinType save_jointype = jointype;
  bool nestjoinOK;
  bool useallclauses;
  Path *inner_cheapest_total = innerrel->cheapest_total_path;
  Path *matpath = NULL;
  ListCell *lc1;

     
                                                                            
                                                                             
                                                                           
                                                                          
                               
     
  switch (jointype)
  {
  case JOIN_INNER:
  case JOIN_LEFT:
  case JOIN_SEMI:
  case JOIN_ANTI:
    nestjoinOK = true;
    useallclauses = false;
    break;
  case JOIN_RIGHT:
  case JOIN_FULL:
    nestjoinOK = false;
    useallclauses = true;
    break;
  case JOIN_UNIQUE_OUTER:
  case JOIN_UNIQUE_INNER:
    jointype = JOIN_INNER;
    nestjoinOK = true;
    useallclauses = false;
    break;
  default:
    elog(ERROR, "unrecognized join type: %d", (int)jointype);
    nestjoinOK = false;                          
    useallclauses = false;
    break;
  }

     
                                                                           
                                                                            
                                                                           
     
  if (PATH_PARAM_BY_REL(inner_cheapest_total, outerrel))
  {
    inner_cheapest_total = NULL;
  }

     
                                                                        
                           
     
  if (save_jointype == JOIN_UNIQUE_INNER)
  {
                                                                         
    if (inner_cheapest_total == NULL)
    {
      return;
    }
    inner_cheapest_total = (Path *)create_unique_path(root, innerrel, inner_cheapest_total, extra->sjinfo);
    Assert(inner_cheapest_total);
  }
  else if (nestjoinOK)
  {
       
                                                              
                                                                       
                      
       
    if (enable_material && inner_cheapest_total != NULL && !ExecMaterializesOutput(inner_cheapest_total->pathtype))
    {
      matpath = (Path *)create_material_path(innerrel, inner_cheapest_total);
    }
  }

  foreach (lc1, outerrel->pathlist)
  {
    Path *outerpath = (Path *)lfirst(lc1);
    List *merge_pathkeys;

       
                                                                           
       
    if (PATH_PARAM_BY_REL(outerpath, innerrel))
    {
      continue;
    }

       
                                                                           
                                                                         
                                                                
       
    if (save_jointype == JOIN_UNIQUE_OUTER)
    {
      if (outerpath != outerrel->cheapest_total_path)
      {
        continue;
      }
      outerpath = (Path *)create_unique_path(root, outerrel, outerpath, extra->sjinfo);
      Assert(outerpath);
    }

       
                                                                          
                                                                           
                                                  
       
    merge_pathkeys = build_join_pathkeys(root, joinrel, jointype, outerpath->pathkeys);

    if (save_jointype == JOIN_UNIQUE_INNER)
    {
         
                                                                         
                    
         
      try_nestloop_path(root, joinrel, outerpath, inner_cheapest_total, merge_pathkeys, jointype, extra);
    }
    else if (nestjoinOK)
    {
         
                                                                   
                                                                  
                                                                         
                                                             
         
      ListCell *lc2;

      foreach (lc2, innerrel->cheapest_parameterized_paths)
      {
        Path *innerpath = (Path *)lfirst(lc2);

        try_nestloop_path(root, joinrel, outerpath, innerpath, merge_pathkeys, jointype, extra);
      }

                                                                      
      if (matpath != NULL)
      {
        try_nestloop_path(root, joinrel, outerpath, matpath, merge_pathkeys, jointype, extra);
      }
    }

                                                                   
    if (save_jointype == JOIN_UNIQUE_OUTER)
    {
      continue;
    }

                                                                       
    if (inner_cheapest_total == NULL)
    {
      continue;
    }

                                   
    generate_mergejoin_paths(root, joinrel, innerrel, outerpath, save_jointype, extra, useallclauses, inner_cheapest_total, merge_pathkeys, false);
  }

     
                                                                      
                                                                       
                                                                           
                                                                           
                                                                   
                                                                         
                                                        
     
  if (joinrel->consider_parallel && save_jointype != JOIN_UNIQUE_OUTER && save_jointype != JOIN_FULL && save_jointype != JOIN_RIGHT && outerrel->partial_pathlist != NIL && bms_is_empty(joinrel->lateral_relids))
  {
    if (nestjoinOK)
    {
      consider_parallel_nestloop(root, joinrel, outerrel, innerrel, save_jointype, extra);
    }

       
                                                                          
                                                                          
                                             
       
    if (inner_cheapest_total == NULL || !inner_cheapest_total->parallel_safe)
    {
      if (save_jointype == JOIN_UNIQUE_INNER)
      {
        return;
      }

      inner_cheapest_total = get_cheapest_parallel_safe_total_inner(innerrel->pathlist);
    }

    if (inner_cheapest_total)
    {
      consider_parallel_mergejoin(root, joinrel, outerrel, innerrel, save_jointype, extra, inner_cheapest_total);
    }
  }
}

   
                               
                                                                        
                                                                       
   
                                  
                                         
                                         
                                        
                                            
                                                           
   
static void
consider_parallel_mergejoin(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra, Path *inner_cheapest_total)
{
  ListCell *lc1;

                                                            
  foreach (lc1, outerrel->partial_pathlist)
  {
    Path *outerpath = (Path *)lfirst(lc1);
    List *merge_pathkeys;

       
                                                                      
       
    merge_pathkeys = build_join_pathkeys(root, joinrel, jointype, outerpath->pathkeys);

    generate_mergejoin_paths(root, joinrel, innerrel, outerpath, jointype, extra, false, inner_cheapest_total, merge_pathkeys, true);
  }
}

   
                              
                                                                                
                                                               
   
                                  
                                         
                                         
                                        
                                            
   
static void
consider_parallel_nestloop(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{
  JoinType save_jointype = jointype;
  ListCell *lc1;

  if (jointype == JOIN_UNIQUE_INNER)
  {
    jointype = JOIN_INNER;
  }

  foreach (lc1, outerrel->partial_pathlist)
  {
    Path *outerpath = (Path *)lfirst(lc1);
    List *pathkeys;
    ListCell *lc2;

                                                                        
    pathkeys = build_join_pathkeys(root, joinrel, jointype, outerpath->pathkeys);

       
                                                                           
                                                                         
                                                                        
                          
       
    foreach (lc2, innerrel->cheapest_parameterized_paths)
    {
      Path *innerpath = (Path *)lfirst(lc2);

                                                                 
      if (!innerpath->parallel_safe)
      {
        continue;
      }

         
                                                                       
                                                                       
                                                                    
                                                                       
                     
         
      if (save_jointype == JOIN_UNIQUE_INNER)
      {
        if (innerpath != innerrel->cheapest_total_path)
        {
          continue;
        }
        innerpath = (Path *)create_unique_path(root, innerrel, innerpath, extra->sjinfo);
        Assert(innerpath);
      }

      try_partial_nestloop_path(root, joinrel, outerpath, innerpath, pathkeys, jointype, extra);
    }
  }
}

   
                        
                                                                         
                                               
   
                                  
                                         
                                         
                                        
                                            
   
static void
hash_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{
  JoinType save_jointype = jointype;
  bool isouterjoin = IS_OUTER_JOIN(jointype);
  List *hashclauses;
  ListCell *l;

     
                                                                            
                                                                            
     
                                                                             
                                             
     
  hashclauses = NIL;
  foreach (l, extra->restrictlist)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(l);

       
                                                                      
                                                          
       
    if (isouterjoin && RINFO_IS_PUSHED_DOWN(restrictinfo, joinrel->relids))
    {
      continue;
    }

    if (!restrictinfo->can_join || restrictinfo->hashjoinoperator == InvalidOid)
    {
      continue;                       
    }

       
                                                                          
       
    if (!clause_sides_match_join(restrictinfo, outerrel, innerrel))
    {
      continue;                                        
    }

    hashclauses = lappend(hashclauses, restrictinfo);
  }

                                                      
  if (hashclauses)
  {
       
                                                                          
                                                             
                                                
       
    Path *cheapest_startup_outer = outerrel->cheapest_startup_path;
    Path *cheapest_total_outer = outerrel->cheapest_total_path;
    Path *cheapest_total_inner = innerrel->cheapest_total_path;

       
                                                                           
                                                                      
                                                                          
                         
       
    if (PATH_PARAM_BY_REL(cheapest_total_outer, innerrel) || PATH_PARAM_BY_REL(cheapest_total_inner, outerrel))
    {
      return;
    }

                                                                      
    if (jointype == JOIN_UNIQUE_OUTER)
    {
      cheapest_total_outer = (Path *)create_unique_path(root, outerrel, cheapest_total_outer, extra->sjinfo);
      Assert(cheapest_total_outer);
      jointype = JOIN_INNER;
      try_hashjoin_path(root, joinrel, cheapest_total_outer, cheapest_total_inner, hashclauses, jointype, extra);
                                                
    }
    else if (jointype == JOIN_UNIQUE_INNER)
    {
      cheapest_total_inner = (Path *)create_unique_path(root, innerrel, cheapest_total_inner, extra->sjinfo);
      Assert(cheapest_total_inner);
      jointype = JOIN_INNER;
      try_hashjoin_path(root, joinrel, cheapest_total_outer, cheapest_total_inner, hashclauses, jointype, extra);
      if (cheapest_startup_outer != NULL && cheapest_startup_outer != cheapest_total_outer)
      {
        try_hashjoin_path(root, joinrel, cheapest_startup_outer, cheapest_total_inner, hashclauses, jointype, extra);
      }
    }
    else
    {
         
                                                                     
                                                                   
                                                                        
                                                                        
                                                                
         
      ListCell *lc1;
      ListCell *lc2;

      if (cheapest_startup_outer != NULL)
      {
        try_hashjoin_path(root, joinrel, cheapest_startup_outer, cheapest_total_inner, hashclauses, jointype, extra);
      }

      foreach (lc1, outerrel->cheapest_parameterized_paths)
      {
        Path *outerpath = (Path *)lfirst(lc1);

           
                                                                    
                      
           
        if (PATH_PARAM_BY_REL(outerpath, innerrel))
        {
          continue;
        }

        foreach (lc2, innerrel->cheapest_parameterized_paths)
        {
          Path *innerpath = (Path *)lfirst(lc2);

             
                                                                  
                                    
             
          if (PATH_PARAM_BY_REL(innerpath, outerrel))
          {
            continue;
          }

          if (outerpath == cheapest_startup_outer && innerpath == cheapest_total_inner)
          {
            continue;                       
          }

          try_hashjoin_path(root, joinrel, outerpath, innerpath, hashclauses, jointype, extra);
        }
      }
    }

       
                                                                     
                                                                       
                                                                         
                                                                          
                                                                     
                                                                           
                                                                         
                                                                         
                                                                      
                                                                         
       
    if (joinrel->consider_parallel && save_jointype != JOIN_UNIQUE_OUTER && save_jointype != JOIN_FULL && save_jointype != JOIN_RIGHT && outerrel->partial_pathlist != NIL && bms_is_empty(joinrel->lateral_relids))
    {
      Path *cheapest_partial_outer;
      Path *cheapest_partial_inner = NULL;
      Path *cheapest_safe_inner = NULL;

      cheapest_partial_outer = (Path *)linitial(outerrel->partial_pathlist);

         
                                                                     
                                                         
                                                                  
         
      if (innerrel->partial_pathlist != NIL && save_jointype != JOIN_UNIQUE_INNER && enable_parallel_hash)
      {
        cheapest_partial_inner = (Path *)linitial(innerrel->partial_pathlist);
        try_partial_hashjoin_path(root, joinrel, cheapest_partial_outer, cheapest_partial_inner, hashclauses, jointype, extra, true                    );
      }

         
                                                                         
                                                                        
                                                                     
                                                                         
                     
         
      if (cheapest_total_inner->parallel_safe)
      {
        cheapest_safe_inner = cheapest_total_inner;
      }
      else if (save_jointype != JOIN_UNIQUE_INNER)
      {
        cheapest_safe_inner = get_cheapest_parallel_safe_total_inner(innerrel->pathlist);
      }

      if (cheapest_safe_inner != NULL)
      {
        try_partial_hashjoin_path(root, joinrel, cheapest_partial_outer, cheapest_safe_inner, hashclauses, jointype, extra, false                    );
      }
    }
  }
}

   
                            
                                                                     
                                                             
   
                                                                         
                                                                          
                                                                           
                                                                        
                                                                           
                                                                          
                                                                         
                  
   
                                                                           
                                                                          
                                                                     
   
                                                                 
                                                                       
                          
   
static List *
select_mergejoin_clauses(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, List *restrictlist, JoinType jointype, bool *mergejoin_allowed)
{
  List *result_list = NIL;
  bool isouterjoin = IS_OUTER_JOIN(jointype);
  bool have_nonmergeable_joinclause = false;
  ListCell *l;

  foreach (l, restrictlist)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(l);

       
                                                                         
                                                                          
                                                                          
                                                      
       
    if (isouterjoin && RINFO_IS_PUSHED_DOWN(restrictinfo, joinrel->relids))
    {
      continue;
    }

                                                          
    if (!restrictinfo->can_join || restrictinfo->mergeopfamilies == NIL)
    {
         
                                                                         
                                                                    
                                                                  
                 
         
      if (!restrictinfo->clause || !IsA(restrictinfo->clause, Const))
      {
        have_nonmergeable_joinclause = true;
      }
      continue;                        
    }

       
                                                                          
       
    if (!clause_sides_match_join(restrictinfo, outerrel, innerrel))
    {
      have_nonmergeable_joinclause = true;
      continue;                                        
    }

       
                                                                
                                                                        
                                                                        
                                                                      
                                                                        
                                                   
       
                                                                       
                                                                  
                                                                    
                                                                        
                                                                     
       
                                                                          
                                                                           
                                                                         
                                                                          
                                         
       
    update_mergeclause_eclasses(root, restrictinfo);

    if (EC_MUST_BE_REDUNDANT(restrictinfo->left_ec) || EC_MUST_BE_REDUNDANT(restrictinfo->right_ec))
    {
      have_nonmergeable_joinclause = true;
      continue;                                      
    }

    result_list = lappend(result_list, restrictinfo);
  }

     
                                                                           
     
  switch (jointype)
  {
  case JOIN_RIGHT:
  case JOIN_FULL:
    *mergejoin_allowed = !have_nonmergeable_joinclause;
    break;
  default:
    *mergejoin_allowed = true;
    break;
  }

  return result_list;
}
