                                                                            
   
              
                                                            
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "miscadmin.h"
#include "optimizer/appendinfo.h"
#include "optimizer/joininfo.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "partitioning/partbounds.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static void
make_rels_by_clause_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels);
static void
make_rels_by_clauseless_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels);
static bool
has_join_restriction(PlannerInfo *root, RelOptInfo *rel);
static bool
has_legal_joinclause(PlannerInfo *root, RelOptInfo *rel);
static bool
restriction_is_constant_false(List *restrictlist, RelOptInfo *joinrel, bool only_pushed_down);
static void
populate_joinrel_with_paths(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *sjinfo, List *restrictlist);
static void
try_partitionwise_join(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *parent_sjinfo, List *parent_restrictlist);
static SpecialJoinInfo *
build_child_join_sjinfo(PlannerInfo *root, SpecialJoinInfo *parent_sjinfo, Relids left_relids, Relids right_relids);
static int
match_expr_to_partition_keys(Expr *expr, RelOptInfo *rel, bool strict_op);

   
                         
                                                                        
                                                                          
                                                                          
                                                                         
                                                                  
   
                                                  
                                                                                 
   
                                                          
   
void
join_search_one_level(PlannerInfo *root, int level)
{
  List **joinrels = root->join_rel_level;
  ListCell *r;
  int k;

  Assert(joinrels[level] == NIL);

                                                                        
  root->join_cur_level = level;

     
                                                                        
                                                                            
                                                                           
                                                                          
                                                                 
     
  foreach (r, joinrels[level - 1])
  {
    RelOptInfo *old_rel = (RelOptInfo *)lfirst(r);

    if (old_rel->joininfo != NIL || old_rel->has_eclass_joins || has_join_restriction(root, old_rel))
    {
         
                                                                       
                                                                       
                                                                  
         
                                                                        
                                                                      
                                                                      
                                                                         
                                                                         
                                                                        
                                     
         
      ListCell *other_rels;

      if (level == 2)                                      
      {
        other_rels = lnext(r);
      }
      else                                
      {
        other_rels = list_head(joinrels[1]);
      }

      make_rels_by_clause_joins(root, old_rel, other_rels);
    }
    else
    {
         
                                                                  
                                                                  
                                 
         
                                                                        
                                                                    
                                                                       
                                                                        
                                                                         
                                      
         
      make_rels_by_clauseless_joins(root, old_rel, list_head(joinrels[1]));
    }
  }

     
                                                                          
                                                                         
     
                                                                          
                                                                         
                                           
     
  for (k = 2;; k++)
  {
    int other_level = level - k;

       
                                                                         
                                               
       
    if (k > other_level)
    {
      break;
    }

    foreach (r, joinrels[k])
    {
      RelOptInfo *old_rel = (RelOptInfo *)lfirst(r);
      ListCell *other_rels;
      ListCell *r2;

         
                                                                        
                                                                       
                                     
         
      if (old_rel->joininfo == NIL && !old_rel->has_eclass_joins && !has_join_restriction(root, old_rel))
      {
        continue;
      }

      if (k == other_level)
      {
        other_rels = lnext(r);                                   
      }
      else
      {
        other_rels = list_head(joinrels[other_level]);
      }

      for_each_cell(r2, other_rels)
      {
        RelOptInfo *new_rel = (RelOptInfo *)lfirst(r2);

        if (!bms_overlap(old_rel->relids, new_rel->relids))
        {
             
                                                                 
                                                                    
                                                    
             
          if (have_relevant_joinclause(root, old_rel, new_rel) || have_join_order_restriction(root, old_rel, new_rel))
          {
            (void)make_join_rel(root, old_rel, new_rel);
          }
        }
      }
    }
  }

               
                                                                            
                                                                         
                                                                        
                                                                            
                                                                             
                                                                           
                   
     
                                                        
                                     
     
                                                                         
                                                                          
                                                                            
                                                                            
                                              
               
     
  if (joinrels[level] == NIL)
  {
       
                                                                   
                                        
       
    foreach (r, joinrels[level - 1])
    {
      RelOptInfo *old_rel = (RelOptInfo *)lfirst(r);

      make_rels_by_clauseless_joins(root, old_rel, list_head(joinrels[1]));
    }

                 
                                                                  
                                                                         
       
                                
                                                   
                                               
       
                                                                         
                                                                         
                                                                     
                              
       
                                                                        
                                                                           
                        
                 
       
    if (joinrels[level] == NIL && root->join_info_list == NIL && !root->hasLateralRTEs)
    {
      elog(ERROR, "failed to build any %d-way joins", level);
    }
  }
}

   
                             
                                                                          
                                                                          
                                                          
                                                                         
   
                                                                        
                                                                             
                                                                               
                                                                            
                                                                               
   
                                                                 
                                                                      
                                     
   
                                                                        
                                          
   
static void
make_rels_by_clause_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels)
{
  ListCell *l;

  for_each_cell(l, other_rels)
  {
    RelOptInfo *other_rel = (RelOptInfo *)lfirst(l);

    if (!bms_overlap(old_rel->relids, other_rel->relids) && (have_relevant_joinclause(root, old_rel, other_rel) || have_join_order_restriction(root, old_rel, other_rel)))
    {
      (void)make_join_rel(root, old_rel, other_rel);
    }
  }
}

   
                                 
                                                              
                                                                     
                                                                      
                                                                         
   
                                                                 
                                                                
                                           
   
                                                                              
                                     
   
static void
make_rels_by_clauseless_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels)
{
  ListCell *l;

  for_each_cell(l, other_rels)
  {
    RelOptInfo *other_rel = (RelOptInfo *)lfirst(l);

    if (!bms_overlap(other_rel->relids, old_rel->relids))
    {
      (void)make_join_rel(root, old_rel, other_rel);
    }
  }
}

   
                 
                                                                   
                                                                     
   
                                                                            
                                                                        
                                                                      
   
                                                                             
                                                                         
                                                                        
                                   
   
static bool
join_is_legal(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, Relids joinrelids, SpecialJoinInfo **sjinfo_p, bool *reversed_p)
{
  SpecialJoinInfo *match_sjinfo;
  bool reversed;
  bool unique_ified;
  bool must_be_leftjoin;
  ListCell *l;

     
                                                                      
                                                                          
     
  *sjinfo_p = NULL;
  *reversed_p = false;

     
                                                                           
                                                                         
                                     
     
  match_sjinfo = NULL;
  reversed = false;
  unique_ified = false;
  must_be_leftjoin = false;

  foreach (l, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(l);

       
                                                                     
                                                                       
                                     
       
    if (!bms_overlap(sjinfo->min_righthand, joinrelids))
    {
      continue;
    }

       
                                                                         
                                              
       
    if (bms_is_subset(joinrelids, sjinfo->min_righthand))
    {
      continue;
    }

       
                                                                     
       
    if (bms_is_subset(sjinfo->min_lefthand, rel1->relids) && bms_is_subset(sjinfo->min_righthand, rel1->relids))
    {
      continue;
    }
    if (bms_is_subset(sjinfo->min_lefthand, rel2->relids) && bms_is_subset(sjinfo->min_righthand, rel2->relids))
    {
      continue;
    }

       
                                                                          
                                                                           
                                                                           
                       
       
    if (sjinfo->jointype == JOIN_SEMI)
    {
      if (bms_is_subset(sjinfo->syn_righthand, rel1->relids) && !bms_equal(sjinfo->syn_righthand, rel1->relids))
      {
        continue;
      }
      if (bms_is_subset(sjinfo->syn_righthand, rel2->relids) && !bms_equal(sjinfo->syn_righthand, rel2->relids))
      {
        continue;
      }
    }

       
                                                                 
                                                               
       
                                                                        
                                                      
       
    if (bms_is_subset(sjinfo->min_lefthand, rel1->relids) && bms_is_subset(sjinfo->min_righthand, rel2->relids))
    {
      if (match_sjinfo)
      {
        return false;                        
      }
      match_sjinfo = sjinfo;
      reversed = false;
    }
    else if (bms_is_subset(sjinfo->min_lefthand, rel2->relids) && bms_is_subset(sjinfo->min_righthand, rel1->relids))
    {
      if (match_sjinfo)
      {
        return false;                        
      }
      match_sjinfo = sjinfo;
      reversed = true;
    }
    else if (sjinfo->jointype == JOIN_SEMI && bms_equal(sjinfo->syn_righthand, rel2->relids) && create_unique_path(root, rel2, rel2->cheapest_total_path, sjinfo) != NULL)
    {
                   
                                                                 
                                                                 
                                                                
                                       
         
                                                                   
                                                                      
                                                                     
                                                                     
                                                                       
                                                                        
                                                                   
                                                                     
                                      
         
                                                                       
                                                                         
                                                                         
                                                 
                   
         
      if (match_sjinfo)
      {
        return false;                        
      }
      match_sjinfo = sjinfo;
      reversed = false;
      unique_ified = true;
    }
    else if (sjinfo->jointype == JOIN_SEMI && bms_equal(sjinfo->syn_righthand, rel1->relids) && create_unique_path(root, rel1, rel1->cheapest_total_path, sjinfo) != NULL)
    {
                                  
      if (match_sjinfo)
      {
        return false;                        
      }
      match_sjinfo = sjinfo;
      reversed = true;
      unique_ified = true;
    }
    else
    {
         
                                                                         
                                                                        
                                                                        
                                                                       
                                                                       
                                                                       
                                                                        
                                                                        
                                                                         
                                                                   
                                                                        
                                                                       
                                                                    
                                                                        
                          
         
      if (bms_overlap(rel1->relids, sjinfo->min_righthand) && bms_overlap(rel2->relids, sjinfo->min_righthand))
      {
        continue;                                             
      }

         
                                                                   
                                                                      
                                                                      
                                                                   
         
      if (sjinfo->jointype != JOIN_LEFT || bms_overlap(joinrelids, sjinfo->min_lefthand))
      {
        return false;                        
      }

         
                                                                       
                                                                         
                                                                     
                                                                   
         
      must_be_leftjoin = true;
    }
  }

     
                                                                      
                                                     
     
                                                                     
                                                                            
                                                                        
                                                                          
     
  if (must_be_leftjoin && (match_sjinfo == NULL || match_sjinfo->jointype != JOIN_LEFT || !match_sjinfo->lhs_strict))
  {
    return false;                        
  }

     
                                                                          
     
  if (root->hasLateralRTEs)
  {
    bool lateral_fwd;
    bool lateral_rev;
    Relids join_lateral_rels;

       
                                                                      
                                                                          
                                                                           
                                                                          
                                                                        
                           
       
                                                                         
                                                                          
                        
       
                                                                         
                                                                     
       
    lateral_fwd = bms_overlap(rel1->relids, rel2->lateral_relids);
    lateral_rev = bms_overlap(rel2->relids, rel1->lateral_relids);
    if (lateral_fwd && lateral_rev)
    {
      return false;                                           
    }
    if (lateral_fwd)
    {
                                                               
      if (match_sjinfo && (reversed || unique_ified || match_sjinfo->jointype == JOIN_FULL))
      {
        return false;                                    
      }
                                                               
      if (!bms_overlap(rel1->relids, rel2->direct_lateral_relids))
      {
        return false;                                    
      }
                                               
      if (have_dangerous_phv(root, rel1->relids, rel2->lateral_relids))
      {
        return false;                                             
      }
    }
    else if (lateral_rev)
    {
                                                               
      if (match_sjinfo && (!reversed || unique_ified || match_sjinfo->jointype == JOIN_FULL))
      {
        return false;                                    
      }
                                                               
      if (!bms_overlap(rel2->relids, rel1->direct_lateral_relids))
      {
        return false;                                    
      }
                                               
      if (have_dangerous_phv(root, rel2->relids, rel1->lateral_relids))
      {
        return false;                                             
      }
    }

       
                                                                          
                                                                           
                                                                           
                                                                       
                                                                           
                                                                     
                                                                         
                                                                         
                                                                        
                                                                          
                                                                     
       
    join_lateral_rels = min_join_parameterization(root, joinrelids, rel1, rel2);
    if (join_lateral_rels)
    {
      Relids join_plus_rhs = bms_copy(joinrelids);
      bool more;

      do
      {
        more = false;
        foreach (l, root->join_info_list)
        {
          SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(l);

                                                                     
          if (sjinfo->jointype == JOIN_FULL)
          {
            continue;
          }

          if (bms_overlap(sjinfo->min_lefthand, join_plus_rhs) && !bms_is_subset(sjinfo->min_righthand, join_plus_rhs))
          {
            join_plus_rhs = bms_add_members(join_plus_rhs, sjinfo->min_righthand);
            more = true;
          }
        }
      } while (more);
      if (bms_overlap(join_plus_rhs, join_lateral_rels))
      {
        return false;                                               
      }
    }
  }

                                    
  *sjinfo_p = match_sjinfo;
  *reversed_p = reversed;
  return true;
}

   
                 
                                                                   
                                                                   
                                                        
                                                                   
                                                               
   
                                                                         
                                                                              
                      
   
RelOptInfo *
make_join_rel(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{
  Relids joinrelids;
  SpecialJoinInfo *sjinfo;
  bool reversed;
  SpecialJoinInfo sjinfo_data;
  RelOptInfo *joinrel;
  List *restrictlist;

                                                                 
  Assert(!bms_overlap(rel1->relids, rel2->relids));

                                                         
  joinrelids = bms_union(rel1->relids, rel2->relids);

                                               
  if (!join_is_legal(root, rel1, rel2, joinrelids, &sjinfo, &reversed))
  {
                           
    bms_free(joinrelids);
    return NULL;
  }

                                                   
  if (reversed)
  {
    RelOptInfo *trel = rel1;

    rel1 = rel2;
    rel2 = trel;
  }

     
                                                                      
                                                                    
                                                         
     
  if (sjinfo == NULL)
  {
    sjinfo = &sjinfo_data;
    sjinfo->type = T_SpecialJoinInfo;
    sjinfo->min_lefthand = rel1->relids;
    sjinfo->min_righthand = rel2->relids;
    sjinfo->syn_lefthand = rel1->relids;
    sjinfo->syn_righthand = rel2->relids;
    sjinfo->jointype = JOIN_INNER;
                                                                   
    sjinfo->lhs_strict = false;
    sjinfo->delay_upper_joins = false;
    sjinfo->semi_can_btree = false;
    sjinfo->semi_can_hash = false;
    sjinfo->semi_operators = NIL;
    sjinfo->semi_rhs_exprs = NIL;
  }

     
                                                                          
                                        
     
  joinrel = build_join_rel(root, joinrelids, rel1, rel2, sjinfo, &restrictlist);

     
                                                                         
                        
     
  if (is_dummy_rel(joinrel))
  {
    bms_free(joinrelids);
    return joinrel;
  }

                                       
  populate_joinrel_with_paths(root, rel1, rel2, joinrel, sjinfo, restrictlist);

  bms_free(joinrelids);

  return joinrel;
}

   
                               
                                                                             
                                                                          
                                                                               
                               
   
static void
populate_joinrel_with_paths(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *sjinfo, List *restrictlist)
{
     
                                                                          
                                                                            
                                                                             
                                                                      
                                                                       
                                                 
     
                                                                           
                                                                          
                                                               
                                                                          
                                                                         
                                                       
     
                                                                             
                 
     
  switch (sjinfo->jointype)
  {
  case JOIN_INNER:
    if (is_dummy_rel(rel1) || is_dummy_rel(rel2) || restriction_is_constant_false(restrictlist, joinrel, false))
    {
      mark_dummy_rel(joinrel);
      break;
    }
    add_paths_to_joinrel(root, joinrel, rel1, rel2, JOIN_INNER, sjinfo, restrictlist);
    add_paths_to_joinrel(root, joinrel, rel2, rel1, JOIN_INNER, sjinfo, restrictlist);
    break;
  case JOIN_LEFT:
    if (is_dummy_rel(rel1) || restriction_is_constant_false(restrictlist, joinrel, true))
    {
      mark_dummy_rel(joinrel);
      break;
    }
    if (restriction_is_constant_false(restrictlist, joinrel, false) && bms_is_subset(rel2->relids, sjinfo->syn_righthand))
    {
      mark_dummy_rel(rel2);
    }
    add_paths_to_joinrel(root, joinrel, rel1, rel2, JOIN_LEFT, sjinfo, restrictlist);
    add_paths_to_joinrel(root, joinrel, rel2, rel1, JOIN_RIGHT, sjinfo, restrictlist);
    break;
  case JOIN_FULL:
    if ((is_dummy_rel(rel1) && is_dummy_rel(rel2)) || restriction_is_constant_false(restrictlist, joinrel, true))
    {
      mark_dummy_rel(joinrel);
      break;
    }
    add_paths_to_joinrel(root, joinrel, rel1, rel2, JOIN_FULL, sjinfo, restrictlist);
    add_paths_to_joinrel(root, joinrel, rel2, rel1, JOIN_FULL, sjinfo, restrictlist);

       
                                                                     
                                                                       
                                                                       
                                                                     
                                                          
       
    if (joinrel->pathlist == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("FULL JOIN is only supported with merge-joinable or hash-joinable join conditions")));
    }
    break;
  case JOIN_SEMI:

       
                                                                      
                                                                     
                                                                     
                                                     
       
    if (bms_is_subset(sjinfo->min_lefthand, rel1->relids) && bms_is_subset(sjinfo->min_righthand, rel2->relids))
    {
      if (is_dummy_rel(rel1) || is_dummy_rel(rel2) || restriction_is_constant_false(restrictlist, joinrel, false))
      {
        mark_dummy_rel(joinrel);
        break;
      }
      add_paths_to_joinrel(root, joinrel, rel1, rel2, JOIN_SEMI, sjinfo, restrictlist);
    }

       
                                                                 
                                                                      
                                                                  
                                                                     
                                                                  
                           
       
    if (bms_equal(sjinfo->syn_righthand, rel2->relids) && create_unique_path(root, rel2, rel2->cheapest_total_path, sjinfo) != NULL)
    {
      if (is_dummy_rel(rel1) || is_dummy_rel(rel2) || restriction_is_constant_false(restrictlist, joinrel, false))
      {
        mark_dummy_rel(joinrel);
        break;
      }
      add_paths_to_joinrel(root, joinrel, rel1, rel2, JOIN_UNIQUE_INNER, sjinfo, restrictlist);
      add_paths_to_joinrel(root, joinrel, rel2, rel1, JOIN_UNIQUE_OUTER, sjinfo, restrictlist);
    }
    break;
  case JOIN_ANTI:
    if (is_dummy_rel(rel1) || restriction_is_constant_false(restrictlist, joinrel, true))
    {
      mark_dummy_rel(joinrel);
      break;
    }
    if (restriction_is_constant_false(restrictlist, joinrel, false) && bms_is_subset(rel2->relids, sjinfo->syn_righthand))
    {
      mark_dummy_rel(rel2);
    }
    add_paths_to_joinrel(root, joinrel, rel1, rel2, JOIN_ANTI, sjinfo, restrictlist);
    break;
  default:
                                        
    elog(ERROR, "unrecognized join type: %d", (int)sjinfo->jointype);
    break;
  }

                                                        
  try_partitionwise_join(root, rel1, rel2, joinrel, sjinfo, restrictlist);
}

   
                               
                                                                 
                                                                    
   
                                                                           
                                                                            
                                                                             
                                                                           
                                                                             
                                                                               
                  
   
                                                                       
                                                                      
                                                                             
                                                                         
                                                                            
                   
   
bool
have_join_order_restriction(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{
  bool result = false;
  ListCell *l;

     
                                                                             
                                                   
     
  if (bms_overlap(rel1->relids, rel2->direct_lateral_relids) || bms_overlap(rel2->relids, rel1->direct_lateral_relids))
  {
    return true;
  }

     
                                                                       
                                                                             
                                                                             
                                                                            
                                                   
     
  foreach (l, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(l);

    if (bms_is_subset(rel1->relids, phinfo->ph_eval_at) && bms_is_subset(rel2->relids, phinfo->ph_eval_at))
    {
      return true;
    }
  }

     
                                                                             
                                                                           
                                                                         
     
                                                                         
                                                            
     
  foreach (l, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(l);

                                                            
    if (sjinfo->jointype == JOIN_FULL)
    {
      continue;
    }

                                                
    if (bms_is_subset(sjinfo->min_lefthand, rel1->relids) && bms_is_subset(sjinfo->min_righthand, rel2->relids))
    {
      result = true;
      break;
    }
    if (bms_is_subset(sjinfo->min_lefthand, rel2->relids) && bms_is_subset(sjinfo->min_righthand, rel1->relids))
    {
      result = true;
      break;
    }

       
                                                                         
                                                                          
                                                 
       
    if (bms_overlap(sjinfo->min_righthand, rel1->relids) && bms_overlap(sjinfo->min_righthand, rel2->relids))
    {
      result = true;
      break;
    }

                               
    if (bms_overlap(sjinfo->min_lefthand, rel1->relids) && bms_overlap(sjinfo->min_lefthand, rel2->relids))
    {
      result = true;
      break;
    }
  }

     
                                                                          
                                                                             
                                                                           
                                                                          
                                                                         
                                                                            
                     
     
  if (result)
  {
    if (has_legal_joinclause(root, rel1) || has_legal_joinclause(root, rel2))
    {
      result = false;
    }
  }

  return result;
}

   
                        
                                                                       
                                                             
                                                                  
   
                                                                       
                                                                      
                                                                            
                                         
   
static bool
has_join_restriction(PlannerInfo *root, RelOptInfo *rel)
{
  ListCell *l;

  if (rel->lateral_relids != NULL || rel->lateral_referencers != NULL)
  {
    return true;
  }

  foreach (l, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(l);

    if (bms_is_subset(rel->relids, phinfo->ph_eval_at) && !bms_equal(rel->relids, phinfo->ph_eval_at))
    {
      return true;
    }
  }

  foreach (l, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(l);

                                                                        
    if (sjinfo->jointype == JOIN_FULL)
    {
      continue;
    }

                                                  
    if (bms_is_subset(sjinfo->min_lefthand, rel->relids) && bms_is_subset(sjinfo->min_righthand, rel->relids))
    {
      continue;
    }

                                                                      
    if (bms_overlap(sjinfo->min_lefthand, rel->relids) || bms_overlap(sjinfo->min_righthand, rel->relids))
    {
      return true;
    }
  }

  return false;
}

   
                        
                                                                
                                          
   
                                                                   
                                                                              
                                                                          
                                                                               
                                                                            
                                                                         
                                                                         
                                                                          
                                                                          
                                                                
   
static bool
has_legal_joinclause(PlannerInfo *root, RelOptInfo *rel)
{
  ListCell *lc;

  foreach (lc, root->initial_rels)
  {
    RelOptInfo *rel2 = (RelOptInfo *)lfirst(lc);

                                               
    if (bms_overlap(rel->relids, rel2->relids))
    {
      continue;
    }

    if (have_relevant_joinclause(root, rel, rel2))
    {
      Relids joinrelids;
      SpecialJoinInfo *sjinfo;
      bool reversed;

                                                   
      joinrelids = bms_union(rel->relids, rel2->relids);

      if (join_is_legal(root, rel, rel2, joinrelids, &sjinfo, &reversed))
      {
                                 
        bms_free(joinrelids);
        return true;
      }

      bms_free(joinrelids);
    }
  }

  return false;
}

   
                                                                             
                                                                            
                                                                          
                                                                              
                                                                               
                                                                               
                                                                               
                                                                          
                                                                           
                                                                             
                                                                          
                            
   
                                                                            
                                                                               
                                                                              
                                  
   
                                                                       
                                                                               
                                                                               
                                                                               
                                                                             
                                                               
   
bool
have_dangerous_phv(PlannerInfo *root, Relids outer_relids, Relids inner_params)
{
  ListCell *lc;

  foreach (lc, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc);

    if (!bms_is_subset(phinfo->ph_eval_at, inner_params))
    {
      continue;                                            
    }
    if (!bms_overlap(phinfo->ph_eval_at, outer_relids))
    {
      continue;                                        
    }
    if (bms_is_subset(phinfo->ph_eval_at, outer_relids))
    {
      continue;                                             
    }
                                                                
    return true;
  }

                              
  return false;
}

   
                                                    
   
bool
is_dummy_rel(RelOptInfo *rel)
{
  Path *path;

     
                                                                           
                                                                          
                                                                       
     
  if (rel->pathlist == NIL)
  {
    return false;
  }
  path = (Path *)linitial(rel->pathlist);

     
                                                                            
                                                                           
                                                                             
                                                                          
     
  for (;;)
  {
    if (IsA(path, ProjectionPath))
    {
      path = ((ProjectionPath *)path)->subpath;
    }
    else if (IsA(path, ProjectSetPath))
    {
      path = ((ProjectSetPath *)path)->subpath;
    }
    else
    {
      break;
    }
  }
  if (IS_DUMMY_APPEND(path))
  {
    return true;
  }
  return false;
}

   
                                    
   
                                                                         
                                                                              
          
   
                                                                        
                                                                        
                                                                           
                                                                               
                                                                              
                                                                              
                                       
   
void
mark_dummy_rel(RelOptInfo *rel)
{
  MemoryContext oldcontext;

                       
  if (is_dummy_rel(rel))
  {
    return;
  }

                                                               
  oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));

                               
  rel->rows = 0;

                                         
  rel->pathlist = NIL;
  rel->partial_pathlist = NIL;

                             
  add_path(rel, (Path *)create_append_path(NULL, rel, NIL, NIL, NIL, rel->lateral_relids, 0, false, NIL, -1));

                                                            
  set_cheapest(rel);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                   
   
                                                                            
                                                                            
                                                                           
                                                                          
                               
   
                                                                              
                                          
   
static bool
restriction_is_constant_false(List *restrictlist, RelOptInfo *joinrel, bool only_pushed_down)
{
  ListCell *lc;

     
                                                                       
                                                                         
                                                                         
                              
     
  foreach (lc, restrictlist)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc);

    if (only_pushed_down && !RINFO_IS_PUSHED_DOWN(rinfo, joinrel->relids))
    {
      continue;
    }

    if (rinfo->clause && IsA(rinfo->clause, Const))
    {
      Const *con = (Const *)rinfo->clause;

                                                                       
      if (con->constisnull)
      {
        return true;
      }
      if (!DatumGetBool(con->constvalue))
      {
        return true;
      }
    }
  }
  return false;
}

   
                                                                             
                                                                   
                        
   
                                                                      
                                                                               
                         
   
                                                                         
   
                                                                       
                                      
   
                                                                           
                                                                            
   
                                                                            
                                                                  
   
static void
try_partitionwise_join(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *parent_sjinfo, List *parent_restrictlist)
{
  bool rel1_is_simple = IS_SIMPLE_REL(rel1);
  bool rel2_is_simple = IS_SIMPLE_REL(rel2);
  int nparts;
  int cnt_parts;

                                                                            
  check_stack_depth();

                                                               
  if (!IS_PARTITIONED_REL(joinrel))
  {
    return;
  }

                                                                      
  Assert(joinrel->consider_partitionwise_join);

     
                                                                     
                                                                       
                                  
     
  Assert(IS_PARTITIONED_REL(rel1) && IS_PARTITIONED_REL(rel2));
  Assert(REL_HAS_ALL_PART_PROPS(rel1) && REL_HAS_ALL_PART_PROPS(rel2));

                                                                          
  Assert(rel1->consider_partitionwise_join && rel2->consider_partitionwise_join);

     
                                                                        
                        
     
  Assert(joinrel->part_scheme == rel1->part_scheme && joinrel->part_scheme == rel2->part_scheme);

     
                                                                             
                                                                       
                                                  
     
  Assert(partition_bounds_equal(joinrel->part_scheme->partnatts, joinrel->part_scheme->parttyplen, joinrel->part_scheme->parttypbyval, joinrel->boundinfo, rel1->boundinfo));
  Assert(partition_bounds_equal(joinrel->part_scheme->partnatts, joinrel->part_scheme->parttyplen, joinrel->part_scheme->parttypbyval, joinrel->boundinfo, rel2->boundinfo));

  nparts = joinrel->nparts;

     
                                                                           
                                                                   
                                                          
     
  for (cnt_parts = 0; cnt_parts < nparts; cnt_parts++)
  {
    RelOptInfo *child_rel1 = rel1->part_rels[cnt_parts];
    RelOptInfo *child_rel2 = rel2->part_rels[cnt_parts];
    bool rel1_empty = (child_rel1 == NULL || IS_DUMMY_REL(child_rel1));
    bool rel2_empty = (child_rel2 == NULL || IS_DUMMY_REL(child_rel2));
    SpecialJoinInfo *child_sjinfo;
    List *child_restrictlist;
    RelOptInfo *child_joinrel;
    Relids child_joinrelids;
    AppendRelInfo **appinfos;
    int nappinfos;

       
                                                                        
                                                                         
                                                                           
                                                                         
                                  
       
    switch (parent_sjinfo->jointype)
    {
    case JOIN_INNER:
    case JOIN_SEMI:
      if (rel1_empty || rel2_empty)
      {
        continue;                               
      }
      break;
    case JOIN_LEFT:
    case JOIN_ANTI:
      if (rel1_empty)
      {
        continue;                               
      }
      break;
    case JOIN_FULL:
      if (rel1_empty && rel2_empty)
      {
        continue;                               
      }
      break;
    default:
                                          
      elog(ERROR, "unrecognized join type: %d", (int)parent_sjinfo->jointype);
      break;
    }

       
                                                                        
                                                                         
                                               
       
    if (child_rel1 == NULL || child_rel2 == NULL)
    {
         
                                                                         
                       
         
      joinrel->nparts = 0;
      return;
    }

       
                                                                          
                                                                        
                                                                      
                                   
       
    if (rel1_is_simple && !child_rel1->consider_partitionwise_join)
    {
      Assert(child_rel1->reloptkind == RELOPT_OTHER_MEMBER_REL);
      Assert(IS_DUMMY_REL(child_rel1));
      joinrel->nparts = 0;
      return;
    }
    if (rel2_is_simple && !child_rel2->consider_partitionwise_join)
    {
      Assert(child_rel2->reloptkind == RELOPT_OTHER_MEMBER_REL);
      Assert(IS_DUMMY_REL(child_rel2));
      joinrel->nparts = 0;
      return;
    }

                                                                   
    Assert(!bms_overlap(child_rel1->relids, child_rel2->relids));
    child_joinrelids = bms_union(child_rel1->relids, child_rel2->relids);
    appinfos = find_appinfos_by_relids(root, child_joinrelids, &nappinfos);

       
                                                              
                        
       
    child_sjinfo = build_child_join_sjinfo(root, parent_sjinfo, child_rel1->relids, child_rel2->relids);

       
                                                                      
                                      
       
    child_restrictlist = (List *)adjust_appendrel_attrs(root, (Node *)parent_restrictlist, nappinfos, appinfos);
    pfree(appinfos);

    child_joinrel = joinrel->part_rels[cnt_parts];
    if (!child_joinrel)
    {
      child_joinrel = build_child_join_rel(root, child_rel1, child_rel2, joinrel, child_restrictlist, child_sjinfo, child_sjinfo->jointype);
      joinrel->part_rels[cnt_parts] = child_joinrel;
    }

    Assert(bms_equal(child_joinrel->relids, child_joinrelids));

    populate_joinrel_with_paths(root, child_rel1, child_rel2, child_joinrel, child_sjinfo, child_restrictlist);
  }
}

   
                                                                 
                                                                              
                                                                   
   
static SpecialJoinInfo *
build_child_join_sjinfo(PlannerInfo *root, SpecialJoinInfo *parent_sjinfo, Relids left_relids, Relids right_relids)
{
  SpecialJoinInfo *sjinfo = makeNode(SpecialJoinInfo);
  AppendRelInfo **left_appinfos;
  int left_nappinfos;
  AppendRelInfo **right_appinfos;
  int right_nappinfos;

  memcpy(sjinfo, parent_sjinfo, sizeof(SpecialJoinInfo));
  left_appinfos = find_appinfos_by_relids(root, left_relids, &left_nappinfos);
  right_appinfos = find_appinfos_by_relids(root, right_relids, &right_nappinfos);

  sjinfo->min_lefthand = adjust_child_relids(sjinfo->min_lefthand, left_nappinfos, left_appinfos);
  sjinfo->min_righthand = adjust_child_relids(sjinfo->min_righthand, right_nappinfos, right_appinfos);
  sjinfo->syn_lefthand = adjust_child_relids(sjinfo->syn_lefthand, left_nappinfos, left_appinfos);
  sjinfo->syn_righthand = adjust_child_relids(sjinfo->syn_righthand, right_nappinfos, right_appinfos);
  sjinfo->semi_rhs_exprs = (List *)adjust_appendrel_attrs(root, (Node *)sjinfo->semi_rhs_exprs, right_nappinfos, right_appinfos);

  pfree(left_appinfos);
  pfree(right_appinfos);

  return sjinfo;
}

   
                                                                        
                                                     
   
bool
have_partkey_equi_join(RelOptInfo *joinrel, RelOptInfo *rel1, RelOptInfo *rel2, JoinType jointype, List *restrictlist)
{
  PartitionScheme part_scheme = rel1->part_scheme;
  ListCell *lc;
  int cnt_pks;
  bool pk_has_clause[PARTITION_MAX_KEYS];
  bool strict_op;

     
                                                                         
                          
     
  Assert(rel1->part_scheme == rel2->part_scheme);
  Assert(part_scheme);

  memset(pk_has_clause, 0, sizeof(pk_has_clause));
  foreach (lc, restrictlist)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc);
    OpExpr *opexpr;
    Expr *expr1;
    Expr *expr2;
    int ipk1;
    int ipk2;

                                                                     
    if (IS_OUTER_JOIN(jointype) && RINFO_IS_PUSHED_DOWN(rinfo, joinrel->relids))
    {
      continue;
    }

                                                        
    if (!rinfo->can_join)
    {
      continue;
    }

                                                         
    if (!rinfo->mergeopfamilies && !OidIsValid(rinfo->hashjoinoperator))
    {
      continue;
    }

    opexpr = castNode(OpExpr, rinfo->clause);

       
                                                                           
                                                                  
                                                             
                        
       
    strict_op = op_strict(opexpr->opno);

                                             
    if (bms_is_subset(rinfo->left_relids, rel1->relids) && bms_is_subset(rinfo->right_relids, rel2->relids))
    {
      expr1 = linitial(opexpr->args);
      expr2 = lsecond(opexpr->args);
    }
    else if (bms_is_subset(rinfo->left_relids, rel2->relids) && bms_is_subset(rinfo->right_relids, rel1->relids))
    {
      expr1 = lsecond(opexpr->args);
      expr2 = linitial(opexpr->args);
    }
    else
    {
      continue;
    }

       
                                                                  
                           
       
    ipk1 = match_expr_to_partition_keys(expr1, rel1, strict_op);
    if (ipk1 < 0)
    {
      continue;
    }
    ipk2 = match_expr_to_partition_keys(expr2, rel2, strict_op);
    if (ipk2 < 0)
    {
      continue;
    }

       
                                                                           
                                           
       
    if (ipk1 != ipk2)
    {
      continue;
    }

       
                                                                     
                                                               
       
    if (rel1->part_scheme->strategy == PARTITION_STRATEGY_HASH)
    {
      if (!op_in_opfamily(rinfo->hashjoinoperator, part_scheme->partopfamily[ipk1]))
      {
        continue;
      }
    }
    else if (!list_member_oid(rinfo->mergeopfamilies, part_scheme->partopfamily[ipk1]))
    {
      continue;
    }

                                                               
    pk_has_clause[ipk1] = true;
  }

                                                                     
  for (cnt_pks = 0; cnt_pks < part_scheme->partnatts; cnt_pks++)
  {
    if (!pk_has_clause[cnt_pks])
    {
      return false;
    }
  }

  return true;
}

   
                                                                     
                                                                                
   
static int
match_expr_to_partition_keys(Expr *expr, RelOptInfo *rel, bool strict_op)
{
  int cnt;

                                                                      
  Assert(rel->part_scheme);

                                       
  while (IsA(expr, RelabelType))
  {
    expr = (Expr *)(castNode(RelabelType, expr))->arg;
  }

  for (cnt = 0; cnt < rel->part_scheme->partnatts; cnt++)
  {
    ListCell *lc;

    Assert(rel->partexprs);
    foreach (lc, rel->partexprs[cnt])
    {
      if (equal(lfirst(lc), expr))
      {
        return cnt;
      }
    }

    if (!strict_op)
    {
      continue;
    }

       
                                                                        
                                                                           
                                                                          
                                                                       
                                        
       
    Assert(rel->nullable_partexprs);
    foreach (lc, rel->nullable_partexprs[cnt])
    {
      if (equal(lfirst(lc), expr))
      {
        return cnt;
      }
    }
  }

  return -1;
}
