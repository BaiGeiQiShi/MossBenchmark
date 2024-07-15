                                                                            
   
                  
                                                                 
   
                                                                           
                                                                       
                                                                        
                                                                           
                                                                             
                                                                             
                                                                        
   
                                                                         
                                                                        
   
   
                  
                                               
   
                                                                            
   
#include "postgres.h"

#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/joininfo.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "utils/lsyscache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)

                     
static bool
join_is_removable(PlannerInfo *root, SpecialJoinInfo *sjinfo);
static void
remove_rel_from_query(PlannerInfo *root, int relid, Relids joinrelids);
static List *
remove_rel_from_joinlist(List *joinlist, int relid, int *nremoved);
static bool
rel_supports_distinctness(PlannerInfo *root, RelOptInfo *rel);
static bool
rel_is_distinct_for(PlannerInfo *root, RelOptInfo *rel, List *clause_list);
static Oid
distinct_col_search(int colno, List *colnos, List *opids);
static bool
is_innerrel_unique_for(PlannerInfo *root, Relids joinrelids, Relids outerrelids, RelOptInfo *innerrel, JoinType jointype, List *restrictlist);

   
                        
                                                                      
                                    
   
                                                                          
                                                                      
   
List *
remove_useless_joins(PlannerInfo *root, List *joinlist)
{
  ListCell *lc;

     
                                                                            
                                                  
     
restart:
  foreach (lc, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(lc);
    int innerrelid;
    int nremoved;

                               
    if (!join_is_removable(root, sjinfo))
    {
      continue;
    }

       
                                                                       
                                                                          
                 
       
    innerrelid = bms_singleton_member(sjinfo->min_righthand);

    remove_rel_from_query(root, innerrelid, bms_union(sjinfo->min_lefthand, sjinfo->min_righthand));

                                                                         
    nremoved = 0;
    joinlist = remove_rel_from_joinlist(joinlist, innerrelid, &nremoved);
    if (nremoved != 1)
    {
      elog(ERROR, "failed to find relation %d in joinlist", innerrelid);
    }

       
                                                                           
                           
       
    root->join_info_list = list_delete_ptr(root->join_info_list, sjinfo);

       
                                                                  
                                                                       
                                                                     
                                                                        
                                                                       
                         
       
    goto restart;
  }

  return joinlist;
}

   
                           
                                                                               
   
                                                                             
                                                                            
                                                                              
                                                                           
                                                                            
   
static inline bool
clause_sides_match_join(RestrictInfo *rinfo, Relids outerrelids, Relids innerrelids)
{
  if (bms_is_subset(rinfo->left_relids, outerrelids) && bms_is_subset(rinfo->right_relids, innerrelids))
  {
                                
    rinfo->outer_is_left = true;
    return true;
  }
  else if (bms_is_subset(rinfo->left_relids, innerrelids) && bms_is_subset(rinfo->right_relids, outerrelids))
  {
                                 
    rinfo->outer_is_left = false;
    return true;
  }
  return false;                                        
}

   
                     
                                                                         
                                            
   
                                                                          
                                                                        
                                                                        
                                                                           
                   
   
static bool
join_is_removable(PlannerInfo *root, SpecialJoinInfo *sjinfo)
{
  int innerrelid;
  RelOptInfo *innerrel;
  Relids joinrelids;
  List *clause_list = NIL;
  ListCell *l;
  int attroff;

     
                                                                          
                                              
     
  if (sjinfo->jointype != JOIN_LEFT || sjinfo->delay_upper_joins)
  {
    return false;
  }

  if (!bms_get_singleton_member(sjinfo->min_righthand, &innerrelid))
  {
    return false;
  }

  innerrel = find_base_rel(root, innerrelid);

     
                                                                           
                                                                         
                                                                         
     
  if (!rel_supports_distinctness(root, innerrel))
  {
    return false;
  }

                                                             
  joinrelids = bms_union(sjinfo->min_lefthand, sjinfo->min_righthand);

     
                                                                             
           
     
                                                                            
                                                                             
                                                                          
     
                                                                         
                                                                           
                                                                             
                                
     
  for (attroff = innerrel->max_attr - innerrel->min_attr; attroff >= 0; attroff--)
  {
    if (!bms_is_subset(innerrel->attr_needed[attroff], joinrelids))
    {
      return false;
    }
  }

     
                                                                            
                                                                           
                                                                          
                                                                             
                                                                             
                                                                             
                                                                            
     
  foreach (l, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(l);

    if (bms_overlap(phinfo->ph_lateral, innerrel->relids))
    {
      return false;                                       
    }
    if (bms_is_subset(phinfo->ph_needed, joinrelids))
    {
      continue;                                     
    }
    if (!bms_overlap(phinfo->ph_eval_at, innerrel->relids))
    {
      continue;                                               
    }
    if (bms_is_subset(phinfo->ph_eval_at, innerrel->relids))
    {
      return false;                                              
    }
    if (bms_overlap(pull_varnos(root, (Node *)phinfo->ph_var->phexpr), innerrel->relids))
    {
      return false;                                 
    }
  }

     
                                                                           
                                                                  
                                                                            
                                                                           
                                                                 
     
  foreach (l, innerrel->joininfo)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(l);

       
                                                                       
                                                                         
                                                                           
                                    
       
    if (RINFO_IS_PUSHED_DOWN(restrictinfo, joinrelids))
    {
         
                                                                      
                                                                      
                                                                       
                                                  
         
      if (bms_is_member(innerrelid, restrictinfo->clause_relids))
      {
        return false;
      }
      continue;                                    
    }

                                                   
    if (!restrictinfo->can_join || restrictinfo->mergeopfamilies == NIL)
    {
      continue;                        
    }

       
                                                                          
                                           
       
    if (!clause_sides_match_join(restrictinfo, sjinfo->min_lefthand, innerrel->relids))
    {
      continue;                                        
    }

                         
    clause_list = lappend(clause_list, restrictinfo);
  }

     
                                                                           
                        
     
  if (rel_is_distinct_for(root, innerrel, clause_list))
  {
    return true;
  }

     
                                                                          
                   
     
  return false;
}

   
                                                                      
                                                                
   
                                                                         
                                                                         
                                                                     
                                                                           
                                                                              
   
static void
remove_rel_from_query(PlannerInfo *root, int relid, Relids joinrelids)
{
  RelOptInfo *rel = find_base_rel(root, relid);
  List *joininfos;
  Index rti;
  ListCell *l;
  ListCell *nextl;

     
                                                                           
                                                                      
     
  rel->reloptkind = RELOPT_DEADREL;

     
                                                                           
     
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *otherrel = root->simple_rel_array[rti];
    int attroff;

                                                                    
    if (otherrel == NULL)
    {
      continue;
    }

    Assert(otherrel->relid == rti);                            

                                                  
    if (otherrel == rel)
    {
      continue;
    }

    for (attroff = otherrel->max_attr - otherrel->min_attr; attroff >= 0; attroff--)
    {
      otherrel->attr_needed[attroff] = bms_del_member(otherrel->attr_needed[attroff], relid);
    }
  }

     
                                                                      
     
                                                                             
                                                                             
                                                                         
                                                             
     
  foreach (l, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(l);

    sjinfo->min_lefthand = bms_del_member(sjinfo->min_lefthand, relid);
    sjinfo->min_righthand = bms_del_member(sjinfo->min_righthand, relid);
    sjinfo->syn_lefthand = bms_del_member(sjinfo->syn_lefthand, relid);
    sjinfo->syn_righthand = bms_del_member(sjinfo->syn_righthand, relid);
  }

     
                                                                     
                                                          
     
                                                                          
                                                                            
                                                                             
                                                                             
                                                                  
                                                                          
                                                                          
                                                                       
                                                                           
     
  for (l = list_head(root->placeholder_list); l != NULL; l = nextl)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(l);

    nextl = lnext(l);
    Assert(!bms_is_member(relid, phinfo->ph_lateral));
    if (bms_is_subset(phinfo->ph_needed, joinrelids) && bms_is_member(relid, phinfo->ph_eval_at))
    {
      root->placeholder_list = list_delete_ptr(root->placeholder_list, phinfo);
    }
    else
    {
      phinfo->ph_eval_at = bms_del_member(phinfo->ph_eval_at, relid);
      Assert(!bms_is_empty(phinfo->ph_eval_at));
      phinfo->ph_needed = bms_del_member(phinfo->ph_needed, relid);
    }
  }

     
                                                                       
     
                                                                     
                                                                         
                                                                           
                                                                         
                                                                           
                                                                  
     
                                                                            
                                                                            
                                   
     
  joininfos = list_copy(rel->joininfo);
  foreach (l, joininfos)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);

    remove_join_clause_from_rels(root, rinfo, rinfo->required_relids);

    if (RINFO_IS_PUSHED_DOWN(rinfo, joinrelids))
    {
                                                                       
      Assert(!bms_is_member(relid, rinfo->clause_relids));

         
                                                                        
                                              
         
      rinfo->required_relids = bms_copy(rinfo->required_relids);
      rinfo->required_relids = bms_del_member(rinfo->required_relids, relid);
      distribute_restrictinfo_to_rels(root, rinfo);
    }
  }

     
                                                                       
                                                         
     
}

   
                                                                         
   
                                                                          
                                            
   
                                                                        
                                                       
   
static List *
remove_rel_from_joinlist(List *joinlist, int relid, int *nremoved)
{
  List *result = NIL;
  ListCell *jl;

  foreach (jl, joinlist)
  {
    Node *jlnode = (Node *)lfirst(jl);

    if (IsA(jlnode, RangeTblRef))
    {
      int varno = ((RangeTblRef *)jlnode)->rtindex;

      if (varno == relid)
      {
        (*nremoved)++;
      }
      else
      {
        result = lappend(result, jlnode);
      }
    }
    else if (IsA(jlnode, List))
    {
                                        
      List *sublist;

      sublist = remove_rel_from_joinlist((List *)jlnode, relid, nremoved);
                                                         
      if (sublist)
      {
        result = lappend(result, sublist);
      }
    }
    else
    {
      elog(ERROR, "unrecognized joinlist node type: %d", (int)nodeTag(jlnode));
    }
  }

  return result;
}

   
                           
                                                                    
                                                                        
   
                                                                          
                                     
   
                                                                          
                                                                        
                                                                       
                                         
   
void
reduce_unique_semijoins(PlannerInfo *root)
{
  ListCell *lc;
  ListCell *next;

     
                                                                      
                                             
     
  for (lc = list_head(root->join_info_list); lc != NULL; lc = next)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(lc);
    int innerrelid;
    RelOptInfo *innerrel;
    Relids joinrelids;
    List *restrictlist;

    next = lnext(lc);

       
                                                                           
                                                                    
                                                                      
                             
       
    if (sjinfo->jointype != JOIN_SEMI || sjinfo->delay_upper_joins)
    {
      continue;
    }

    if (!bms_get_singleton_member(sjinfo->min_righthand, &innerrelid))
    {
      continue;
    }

    innerrel = find_base_rel(root, innerrelid);

       
                                                                         
                                                                           
                                         
       
    if (!rel_supports_distinctness(root, innerrel))
    {
      continue;
    }

                                                               
    joinrelids = bms_union(sjinfo->min_lefthand, sjinfo->min_righthand);

       
                                                                          
                                                                          
                                                  
       
    restrictlist = list_concat(generate_join_implied_equalities(root, joinrelids, sjinfo->min_lefthand, innerrel), innerrel->joininfo);

                                                                
    if (!innerrel_is_unique(root, joinrelids, sjinfo->min_lefthand, innerrel, JOIN_SEMI, restrictlist, true))
    {
      continue;
    }

                                                       
    root->join_info_list = list_delete_ptr(root->join_info_list, sjinfo);
  }
}

   
                             
                                                                           
   
                                                                          
                                                                           
                                                                         
                                                                         
                                                                         
            
   
static bool
rel_supports_distinctness(PlannerInfo *root, RelOptInfo *rel)
{
                                       
  if (rel->reloptkind != RELOPT_BASEREL)
  {
    return false;
  }
  if (rel->rtekind == RTE_RELATION)
  {
       
                                                                     
                                                                    
                                                                       
                                                                   
                                                               
       
    ListCell *lc;

    foreach (lc, rel->indexlist)
    {
      IndexOptInfo *ind = (IndexOptInfo *)lfirst(lc);

      if (ind->unique && ind->immediate && (ind->indpred == NIL || ind->predOK))
      {
        return true;
      }
    }
  }
  else if (rel->rtekind == RTE_SUBQUERY)
  {
    Query *subquery = root->simple_rte_array[rel->relid]->subquery;

                                                                           
    if (query_supports_distinctness(subquery))
    {
      return true;
    }
  }
                                                      
  return false;
}

   
                       
                                                                          
   
                                                                            
                                                                         
                                                   
   
                                                                    
                                                                               
                                                                          
                                                                               
                                                                  
   
                                                                            
                                                                              
                                                 
   
static bool
rel_is_distinct_for(PlannerInfo *root, RelOptInfo *rel, List *clause_list)
{
     
                                                                           
                                                                           
               
     
  if (rel->reloptkind != RELOPT_BASEREL)
  {
    return false;
  }
  if (rel->rtekind == RTE_RELATION)
  {
       
                                                                      
                                                                   
                                                                    
       
    if (relation_has_unique_index_for(root, rel, clause_list, NIL, NIL))
    {
      return true;
    }
  }
  else if (rel->rtekind == RTE_SUBQUERY)
  {
    Index relid = rel->relid;
    Query *subquery = root->simple_rte_array[relid]->subquery;
    List *colnos = NIL;
    List *opids = NIL;
    ListCell *l;

       
                                                                     
                                                                           
                                                                       
                              
       
                                                                       
                                       
       
    foreach (l, clause_list)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);
      Oid op;
      Var *var;

         
                                                                    
                                                                       
                                                                     
                                                                   
                                                                  
                  
         
      op = castNode(OpExpr, rinfo->clause)->opno;

                                                   
      if (rinfo->outer_is_left)
      {
        var = (Var *)get_rightop(rinfo->clause);
      }
      else
      {
        var = (Var *)get_leftop(rinfo->clause);
      }

         
                                                                       
                                                                         
                           
         
      if (var && IsA(var, RelabelType))
      {
        var = (Var *)((RelabelType *)var)->arg;
      }

         
                                                                         
                                      
         
      if (!var || !IsA(var, Var) || var->varno != relid || var->varlevelsup != 0)
      {
        continue;
      }

      colnos = lappend_int(colnos, var->varattno);
      opids = lappend_oid(opids, op);
    }

    if (query_is_distinct_for(subquery, colnos, opids))
    {
      return true;
    }
  }
  return false;
}

   
                                                                             
                                   
   
                                                                            
                                                                             
                                                                           
                                                                         
                                                                           
            
   
bool
query_supports_distinctness(Query *query)
{
                                                               
  if (query->hasTargetSRFs && query->distinctClause == NIL)
  {
    return false;
  }

                                                         
  if (query->distinctClause != NIL || query->groupClause != NIL || query->groupingSets != NIL || query->hasAggs || query->havingQual || query->setOperations)
  {
    return true;
  }

  return false;
}

   
                                                                     
                       
   
                                                                           
                                                            
   
                                                                         
                                                                           
                                                                       
                                                                            
                                                                          
                                                                            
                                                                    
                                                                           
                                                                        
                       
   
bool
query_is_distinct_for(Query *query, List *colnos, List *opids)
{
  ListCell *l;
  Oid opid;

  Assert(list_length(colnos) == list_length(opids));

     
                                                                       
                                                                            
                                                                            
                             
     
  if (query->distinctClause)
  {
    foreach (l, query->distinctClause)
    {
      SortGroupClause *sgc = (SortGroupClause *)lfirst(l);
      TargetEntry *tle = get_sortgroupclause_tle(sgc, query->targetList);

      opid = distinct_col_search(tle->resno, colnos, opids);
      if (!OidIsValid(opid) || !equality_ops_are_compatible(opid, sgc->eqop))
      {
        break;                             
      }
    }
    if (l == NULL)                           
    {
      return true;
    }
  }

     
                                                                       
                                                                         
                                                                            
                                                                           
                                                                        
     
  if (query->hasTargetSRFs)
  {
    return false;
  }

     
                                                                            
                                                                        
     
  if (query->groupClause && !query->groupingSets)
  {
    foreach (l, query->groupClause)
    {
      SortGroupClause *sgc = (SortGroupClause *)lfirst(l);
      TargetEntry *tle = get_sortgroupclause_tle(sgc, query->targetList);

      opid = distinct_col_search(tle->resno, colnos, opids);
      if (!OidIsValid(opid) || !equality_ops_are_compatible(opid, sgc->eqop))
      {
        break;                             
      }
    }
    if (l == NULL)                           
    {
      return true;
    }
  }
  else if (query->groupingSets)
  {
       
                                                                         
                                                    
       
    if (query->groupClause)
    {
      return false;
    }

       
                                                                         
                                                                        
                                                                       
                                                      
       
    if (list_length(query->groupingSets) == 1 && ((GroupingSet *)linitial(query->groupingSets))->kind == GROUPING_SET_EMPTY)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
       
                                                                          
                                                                           
       
    if (query->hasAggs || query->havingQual)
    {
      return true;
    }
  }

     
                                                                            
                      
     
  if (query->setOperations)
  {
    SetOperationStmt *topop = castNode(SetOperationStmt, query->setOperations);

    Assert(topop->op != SETOP_NONE);

    if (!topop->all)
    {
      ListCell *lg;

                                                                      
      lg = list_head(topop->groupClauses);
      foreach (l, query->targetList)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(l);
        SortGroupClause *sgc;

        if (tle->resjunk)
        {
          continue;                             
        }

                                                              
        Assert(lg != NULL);
        sgc = (SortGroupClause *)lfirst(lg);
        lg = lnext(lg);

        opid = distinct_col_search(tle->resno, colnos, opids);
        if (!OidIsValid(opid) || !equality_ops_are_compatible(opid, sgc->eqop))
        {
          break;                             
        }
      }
      if (l == NULL)                           
      {
        return true;
      }
    }
  }

     
                                                                         
                       
     
                                                                   
                                             
     

  return false;
}

   
                                                              
   
                                                                     
                                                                             
                                                           
   
static Oid
distinct_col_search(int colno, List *colnos, List *opids)
{
  ListCell *lc1, *lc2;

  forboth(lc1, colnos, lc2, opids)
  {
    if (colno == lfirst_int(lc1))
    {
      return lfirst_oid(lc2);
    }
  }
  return InvalidOid;
}

   
                      
                                                                            
                                                                           
   
                                                                         
                                                                             
                                                                         
                                                                 
   
                                                                         
                                                                        
                                                                           
                                                                             
                                                                         
                                                                          
                                                                           
   
                                                                             
                                                                    
                                                                          
                                                                          
                                                                             
             
   
bool
innerrel_is_unique(PlannerInfo *root, Relids joinrelids, Relids outerrelids, RelOptInfo *innerrel, JoinType jointype, List *restrictlist, bool force_cache)
{
  MemoryContext old_context;
  ListCell *lc;

                                                                      
  if (restrictlist == NIL)
  {
    return false;
  }

     
                                                                             
                                          
     
  if (!rel_supports_distinctness(root, innerrel))
  {
    return false;
  }

     
                                                                       
                                                                            
                                                                         
                                                                            
                                                              
     
  foreach (lc, innerrel->unique_for_rels)
  {
    Relids unique_for_rels = (Relids)lfirst(lc);

    if (bms_is_subset(unique_for_rels, outerrelids))
    {
      return true;               
    }
  }

     
                                                                            
                                                                
     
  foreach (lc, innerrel->non_unique_for_rels)
  {
    Relids unique_for_rels = (Relids)lfirst(lc);

    if (bms_is_subset(outerrelids, unique_for_rels))
    {
      return false;
    }
  }

                                                        
  if (is_innerrel_unique_for(root, joinrelids, outerrelids, innerrel, jointype, restrictlist))
  {
       
                                                                          
                                                          
       
                                                                        
                                                                          
                                                                         
                                                                      
                                 
       
    old_context = MemoryContextSwitchTo(root->planner_cxt);
    innerrel->unique_for_rels = lappend(innerrel->unique_for_rels, bms_copy(outerrelids));
    MemoryContextSwitchTo(old_context);

    return true;               
  }
  else
  {
       
                                                                           
                                                                        
               
       
                                                                           
                                                                          
                                                                     
                                                                         
                                                                           
                                                                          
                                 
       
                                                                         
                                                                          
                                      
       
    if (force_cache || root->join_search_private)
    {
      old_context = MemoryContextSwitchTo(root->planner_cxt);
      innerrel->non_unique_for_rels = lappend(innerrel->non_unique_for_rels, bms_copy(outerrelids));
      MemoryContextSwitchTo(old_context);
    }

    return false;
  }
}

   
                          
                                                                            
                                                                           
   
static bool
is_innerrel_unique_for(PlannerInfo *root, Relids joinrelids, Relids outerrelids, RelOptInfo *innerrel, JoinType jointype, List *restrictlist)
{
  List *clause_list = NIL;
  ListCell *lc;

     
                                                                           
                                                                          
                                                                 
                                                                       
                                             
     
  foreach (lc, restrictlist)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(lc);

       
                                                                          
                              
       
    if (IS_OUTER_JOIN(jointype) && RINFO_IS_PUSHED_DOWN(restrictinfo, joinrelids))
    {
      continue;
    }

                                                   
    if (!restrictinfo->can_join || restrictinfo->mergeopfamilies == NIL)
    {
      continue;                        
    }

       
                                                                          
                                           
       
    if (!clause_sides_match_join(restrictinfo, outerrelids, innerrel->relids))
    {
      continue;                                        
    }

                         
    clause_list = lappend(clause_list, restrictinfo);
  }

                                                  
  return rel_is_distinct_for(root, innerrel, clause_list);
}
