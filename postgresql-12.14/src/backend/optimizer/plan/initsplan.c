                                                                            
   
               
                                                                  
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_type.h"
#include "catalog/pg_class.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/inherit.h"
#include "optimizer/joininfo.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "parser/analyze.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

                                     
int from_collapse_limit;
int join_collapse_limit;

                                                                         
typedef struct PostponedQual
{
  Node *qual;                                               
  Relids relids;                                        
} PostponedQual;

static void
extract_lateral_references(PlannerInfo *root, RelOptInfo *brel, Index rtindex);
static List *
deconstruct_recurse(PlannerInfo *root, Node *jtnode, bool below_outer_join, Relids *qualscope, Relids *inner_join_rels, List **postponed_qual_list);
static void
process_security_barrier_quals(PlannerInfo *root, int rti, Relids qualscope, bool below_outer_join);
static SpecialJoinInfo *
make_outerjoininfo(PlannerInfo *root, Relids left_rels, Relids right_rels, Relids inner_join_rels, JoinType jointype, List *clause);
static void
compute_semijoin_info(PlannerInfo *root, SpecialJoinInfo *sjinfo, List *clause);
static void
distribute_qual_to_rels(PlannerInfo *root, Node *clause, bool is_deduced, bool below_outer_join, JoinType jointype, Index security_level, Relids qualscope, Relids ojscope, Relids outerjoin_nonnullable, Relids deduced_nullable_relids, List **postponed_qual_list);
static bool
check_outerjoin_delay(PlannerInfo *root, Relids *relids_p, Relids *nullable_relids_p, bool is_pushed_down);
static bool
check_equivalence_delay(PlannerInfo *root, RestrictInfo *restrictinfo);
static bool
check_redundant_nullability_qual(PlannerInfo *root, Node *clause);
static void
check_mergejoinable(RestrictInfo *restrictinfo);
static void
check_hashjoinable(RestrictInfo *restrictinfo);

                                                                               
   
               
   
                                                                               

   
                          
   
                                                                      
                                                                   
                                
   
                                                                          
                                                                    
   
                                                                          
                                                                       
                                                                      
                                                                 
   
void
add_base_rels_to_query(PlannerInfo *root, Node *jtnode)
{
  if (jtnode == NULL)
  {
    return;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;

    (void)build_simple_rel(root, varno, NULL);
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      add_base_rels_to_query(root, lfirst(l));
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

    add_base_rels_to_query(root, j->larg);
    add_base_rels_to_query(root, j->rarg);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                           
                                                                          
   
                                                                             
                                      
   
void
add_other_rels_to_query(PlannerInfo *root)
{
  int rti;

  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *rel = root->simple_rel_array[rti];
    RangeTblEntry *rte = root->simple_rte_array[rti];

                                                                    
    if (rel == NULL)
    {
      continue;
    }

                                                         
    if (rel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

                                                           
    if (rte->inh)
    {
      expand_inherited_rtentry(root, rel, rte, rti);
    }
  }
}

                                                                               
   
                 
   
                                                                               

   
                         
                                                                           
                                                                    
   
                                                                        
                                             
   
void
build_base_rel_tlists(PlannerInfo *root, List *final_tlist)
{
  List *tlist_vars = pull_var_clause((Node *)final_tlist, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);

  if (tlist_vars != NIL)
  {
    add_vars_to_targetlist(root, tlist_vars, bms_make_singleton(0), true);
    list_free(tlist_vars);
  }

     
                                                                         
                                                          
     
  if (root->parse->havingQual)
  {
    List *having_vars = pull_var_clause(root->parse->havingQual, PVC_RECURSE_AGGREGATES | PVC_INCLUDE_PLACEHOLDERS);

    if (having_vars != NIL)
    {
      add_vars_to_targetlist(root, having_vars, bms_make_singleton(0), true);
      list_free(having_vars);
    }
  }
}

   
                          
                                                                   
                                                                         
                                                                    
                                          
   
                                                                         
                                                                      
                                                                        
                                                                          
                                                                             
                                                                        
   
void
add_vars_to_targetlist(PlannerInfo *root, List *vars, Relids where_needed, bool create_new_ph)
{
  ListCell *temp;

  Assert(!bms_is_empty(where_needed));

  foreach (temp, vars)
  {
    Node *node = (Node *)lfirst(temp);

    if (IsA(node, Var))
    {
      Var *var = (Var *)node;
      RelOptInfo *rel = find_base_rel(root, var->varno);
      int attno = var->varattno;

      if (bms_is_subset(where_needed, rel->relids))
      {
        continue;
      }
      Assert(attno >= rel->min_attr && attno <= rel->max_attr);
      attno -= rel->min_attr;
      if (rel->attr_needed[attno] == NULL)
      {
                                                                    
                                               
        rel->reltarget->exprs = lappend(rel->reltarget->exprs, copyObject(var));
                                                             
      }
      rel->attr_needed[attno] = bms_add_members(rel->attr_needed[attno], where_needed);
    }
    else if (IsA(node, PlaceHolderVar))
    {
      PlaceHolderVar *phv = (PlaceHolderVar *)node;
      PlaceHolderInfo *phinfo = find_placeholder_info(root, phv, create_new_ph);

      phinfo->ph_needed = bms_add_members(phinfo->ph_needed, where_needed);
    }
    else
    {
      elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    }
  }
}

                                                                               
   
                        
   
                                                                               

   
                           
                                                                       
                                                                            
                                                       
   
                                                                             
                                                                          
                                                                           
                                                        
   
                                                                            
                                                                           
                                                                             
                                                                            
                                                                  
   
                                                                         
                                 
   
void
find_lateral_references(PlannerInfo *root)
{
  Index rti;

                                                                
  if (!root->hasLateralRTEs)
  {
    return;
  }

     
                                                                  
     
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

    extract_lateral_references(root, brel, rti);
  }
}

static void
extract_lateral_references(PlannerInfo *root, RelOptInfo *brel, Index rtindex)
{
  RangeTblEntry *rte = root->simple_rte_array[rtindex];
  List *vars;
  List *newvars;
  Relids where_needed;
  ListCell *lc;

                                                            
  if (!rte->lateral)
  {
    return;
  }

                                       
  if (rte->rtekind == RTE_RELATION)
  {
    vars = pull_vars_of_level((Node *)rte->tablesample, 0);
  }
  else if (rte->rtekind == RTE_SUBQUERY)
  {
    vars = pull_vars_of_level((Node *)rte->subquery, 1);
  }
  else if (rte->rtekind == RTE_FUNCTION)
  {
    vars = pull_vars_of_level((Node *)rte->functions, 0);
  }
  else if (rte->rtekind == RTE_TABLEFUNC)
  {
    vars = pull_vars_of_level((Node *)rte->tablefunc, 0);
  }
  else if (rte->rtekind == RTE_VALUES)
  {
    vars = pull_vars_of_level((Node *)rte->values_lists, 0);
  }
  else
  {
    Assert(false);
    return;                          
  }

  if (vars == NIL)
  {
    return;                    
  }

                                                                          
  newvars = NIL;
  foreach (lc, vars)
  {
    Node *node = (Node *)lfirst(lc);

    node = copyObject(node);
    if (IsA(node, Var))
    {
      Var *var = (Var *)node;

                                                       
      var->varlevelsup = 0;
    }
    else if (IsA(node, PlaceHolderVar))
    {
      PlaceHolderVar *phv = (PlaceHolderVar *)node;
      int levelsup = phv->phlevelsup;

                                                                      
      if (levelsup != 0)
      {
        IncrementVarSublevelsUp(node, -levelsup, 0);
      }

         
                                                                    
                                                                        
                                                                  
         
      if (levelsup > 0)
      {
        phv->phexpr = preprocess_phv_expression(root, phv->phexpr);
      }
    }
    else
    {
      Assert(false);
    }
    newvars = lappend(newvars, node);
  }

  list_free(vars);

     
                                                                           
                                                                            
                                                                            
                                                                           
               
     
  where_needed = bms_make_singleton(rtindex);

     
                                                                       
                             
     
  add_vars_to_targetlist(root, newvars, where_needed, true);

                                                                    
  brel->lateral_vars = newvars;
}

   
                            
                                                                         
                                   
   
                                                                           
                                                
   
void
create_lateral_join_info(PlannerInfo *root)
{
  bool found_laterals = false;
  Index rti;
  ListCell *lc;

                                                                
  if (!root->hasLateralRTEs)
  {
    return;
  }

     
                                                                  
     
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];
    Relids lateral_relids;

                                                                    
    if (brel == NULL)
    {
      continue;
    }

    Assert(brel->relid == rti);                            

                                           
    if (brel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

    lateral_relids = NULL;

                                                       
    foreach (lc, brel->lateral_vars)
    {
      Node *node = (Node *)lfirst(lc);

      if (IsA(node, Var))
      {
        Var *var = (Var *)node;

        found_laterals = true;
        lateral_relids = bms_add_member(lateral_relids, var->varno);
      }
      else if (IsA(node, PlaceHolderVar))
      {
        PlaceHolderVar *phv = (PlaceHolderVar *)node;
        PlaceHolderInfo *phinfo = find_placeholder_info(root, phv, false);

        found_laterals = true;
        lateral_relids = bms_add_members(lateral_relids, phinfo->ph_eval_at);
      }
      else
      {
        Assert(false);
      }
    }

                                                               
    brel->direct_lateral_relids = lateral_relids;
    brel->lateral_relids = bms_copy(lateral_relids);
  }

     
                                                                             
                                                                   
     
                                                                            
                                                                         
                                                                       
                                                                             
                                                                             
                                                                           
                                                                             
                                                        
     
  foreach (lc, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc);
    Relids eval_at = phinfo->ph_eval_at;
    int varno;

    if (phinfo->ph_lateral == NULL)
    {
      continue;                                              
    }

    found_laterals = true;

    if (bms_get_singleton_member(eval_at, &varno))
    {
                                        
      RelOptInfo *brel = find_base_rel(root, varno);

      brel->direct_lateral_relids = bms_add_members(brel->direct_lateral_relids, phinfo->ph_lateral);
      brel->lateral_relids = bms_add_members(brel->lateral_relids, phinfo->ph_lateral);
    }
    else
    {
                                     
      varno = -1;
      while ((varno = bms_next_member(eval_at, varno)) >= 0)
      {
        RelOptInfo *brel = find_base_rel(root, varno);

        brel->lateral_relids = bms_add_members(brel->lateral_relids, phinfo->ph_lateral);
      }
    }
  }

     
                                                                         
                                                      
     
  if (!found_laterals)
  {
    root->hasLateralRTEs = false;
    return;
  }

     
                                                                          
                                                                             
                                                                          
                                                                           
                                                     
     
                                                                           
                                                                       
                                                                          
     
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];
    Relids outer_lateral_relids;
    Index rti2;

    if (brel == NULL || brel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

                                                                     
    outer_lateral_relids = brel->lateral_relids;
    if (outer_lateral_relids == NULL)
    {
      continue;
    }

                                
    for (rti2 = 1; rti2 < root->simple_rel_array_size; rti2++)
    {
      RelOptInfo *brel2 = root->simple_rel_array[rti2];

      if (brel2 == NULL || brel2->reloptkind != RELOPT_BASEREL)
      {
        continue;
      }

                                                                   
      if (bms_is_member(rti, brel2->lateral_relids))
      {
        brel2->lateral_relids = bms_add_members(brel2->lateral_relids, outer_lateral_relids);
      }
    }
  }

     
                                                                         
                                                                          
                                                                     
     
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];
    Relids lateral_relids;
    int rti2;

    if (brel == NULL || brel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

                                                    
    lateral_relids = brel->lateral_relids;
    if (lateral_relids == NULL)
    {
      continue;
    }

       
                                                                      
                              
       
    Assert(!bms_is_empty(lateral_relids));

                                                                 
    Assert(!bms_is_member(rti, lateral_relids));

                                     
    rti2 = -1;
    while ((rti2 = bms_next_member(lateral_relids, rti2)) >= 0)
    {
      RelOptInfo *brel2 = root->simple_rel_array[rti2];

      Assert(brel2 != NULL && brel2->reloptkind == RELOPT_BASEREL);
      brel2->lateral_referencers = bms_add_member(brel2->lateral_referencers, rti);
    }
  }
}

                                                                               
   
                          
   
                                                                               

   
                        
                                                                       
                                                                         
                                                                           
                                                                              
                                                                         
                                             
   
                                                                        
                                                                        
                                                                          
                                                                         
                                                                              
                                                                       
                                                                         
   
                                                                               
                                                                            
                                                                             
                                                                             
                                                                               
                                                                           
                                                                              
                                                                          
   
List *
deconstruct_jointree(PlannerInfo *root)
{
  List *result;
  Relids qualscope;
  Relids inner_join_rels;
  List *postponed_qual_list = NIL;

                                          
  Assert(root->parse->jointree != NULL && IsA(root->parse->jointree, FromExpr));

                                              
  root->nullable_baserels = NULL;

  result = deconstruct_recurse(root, (Node *)root->parse->jointree, false, &qualscope, &inner_join_rels, &postponed_qual_list);

                                       
  Assert(postponed_qual_list == NIL);

  return result;
}

   
                       
                                                             
   
           
                                          
                                                                          
                            
            
                                                                         
                                                                         
                                                  
                                                                          
                                                                        
                          
                                                                         
                                                                   
                                                                   
   
                                                                               
   
static List *
deconstruct_recurse(PlannerInfo *root, Node *jtnode, bool below_outer_join, Relids *qualscope, Relids *inner_join_rels, List **postponed_qual_list)
{
  List *joinlist;

  if (jtnode == NULL)
  {
    *qualscope = NULL;
    *inner_join_rels = NULL;
    return NIL;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;

                                       
    *qualscope = bms_make_singleton(varno);
                                                         
    if (root->qual_security_level > 0)
    {
      process_security_barrier_quals(root, varno, *qualscope, below_outer_join);
    }
                                                        
    *inner_join_rels = NULL;
    joinlist = list_make1(jtnode);
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    List *child_postponed_quals = NIL;
    int remaining;
    ListCell *l;

       
                                                                           
                                                                         
                                                                       
                                                                   
       
    *qualscope = NULL;
    *inner_join_rels = NULL;
    joinlist = NIL;
    remaining = list_length(f->fromlist);
    foreach (l, f->fromlist)
    {
      Relids sub_qualscope;
      List *sub_joinlist;
      int sub_members;

      sub_joinlist = deconstruct_recurse(root, lfirst(l), below_outer_join, &sub_qualscope, inner_join_rels, &child_postponed_quals);
      *qualscope = bms_add_members(*qualscope, sub_qualscope);
      sub_members = list_length(sub_joinlist);
      remaining--;
      if (sub_members <= 1 || list_length(joinlist) + sub_members + remaining <= from_collapse_limit)
      {
        joinlist = list_concat(joinlist, sub_joinlist);
      }
      else
      {
        joinlist = lappend(joinlist, sub_joinlist);
      }
    }

       
                                                                         
                                                                         
                                                                         
                                                                         
                                                                          
       
    if (list_length(f->fromlist) > 1)
    {
      *inner_join_rels = *qualscope;
    }

       
                                                                     
                                                                        
       
    foreach (l, child_postponed_quals)
    {
      PostponedQual *pq = (PostponedQual *)lfirst(l);

      if (bms_is_subset(pq->relids, *qualscope))
      {
        distribute_qual_to_rels(root, pq->qual, false, below_outer_join, JOIN_INNER, root->qual_security_level, *qualscope, NULL, NULL, NULL, NULL);
      }
      else
      {
        *postponed_qual_list = lappend(*postponed_qual_list, pq);
      }
    }

       
                                        
       
    foreach (l, (List *)f->quals)
    {
      Node *qual = (Node *)lfirst(l);

      distribute_qual_to_rels(root, qual, false, below_outer_join, JOIN_INNER, root->qual_security_level, *qualscope, NULL, NULL, NULL, postponed_qual_list);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    List *child_postponed_quals = NIL;
    Relids leftids, rightids, left_inners, right_inners, nonnullable_rels, nullable_rels, ojscope;
    List *leftjoinlist, *rightjoinlist;
    List *my_quals;
    SpecialJoinInfo *sjinfo;
    ListCell *l;

       
                                                                          
                                                                     
                                                                         
                                                                       
                                                                           
                                                                           
                                                                        
                                                                        
                                                                      
                                                            
       
    switch (j->jointype)
    {
    case JOIN_INNER:
      leftjoinlist = deconstruct_recurse(root, j->larg, below_outer_join, &leftids, &left_inners, &child_postponed_quals);
      rightjoinlist = deconstruct_recurse(root, j->rarg, below_outer_join, &rightids, &right_inners, &child_postponed_quals);
      *qualscope = bms_union(leftids, rightids);
      *inner_join_rels = *qualscope;
                                                     
      nonnullable_rels = NULL;
                                                         
      nullable_rels = NULL;
      break;
    case JOIN_LEFT:
    case JOIN_ANTI:
      leftjoinlist = deconstruct_recurse(root, j->larg, below_outer_join, &leftids, &left_inners, &child_postponed_quals);
      rightjoinlist = deconstruct_recurse(root, j->rarg, true, &rightids, &right_inners, &child_postponed_quals);
      *qualscope = bms_union(leftids, rightids);
      *inner_join_rels = bms_union(left_inners, right_inners);
      nonnullable_rels = leftids;
      nullable_rels = rightids;
      break;
    case JOIN_SEMI:
      leftjoinlist = deconstruct_recurse(root, j->larg, below_outer_join, &leftids, &left_inners, &child_postponed_quals);
      rightjoinlist = deconstruct_recurse(root, j->rarg, below_outer_join, &rightids, &right_inners, &child_postponed_quals);
      *qualscope = bms_union(leftids, rightids);
      *inner_join_rels = bms_union(left_inners, right_inners);
                                                    
      nonnullable_rels = NULL;

         
                                                                     
                                                                  
                                        
         
      nullable_rels = NULL;
      break;
    case JOIN_FULL:
      leftjoinlist = deconstruct_recurse(root, j->larg, true, &leftids, &left_inners, &child_postponed_quals);
      rightjoinlist = deconstruct_recurse(root, j->rarg, true, &rightids, &right_inners, &child_postponed_quals);
      *qualscope = bms_union(leftids, rightids);
      *inner_join_rels = bms_union(left_inners, right_inners);
                                             
      nonnullable_rels = *qualscope;
      nullable_rels = *qualscope;
      break;
    default:
                                                                 
      elog(ERROR, "unrecognized join type: %d", (int)j->jointype);
      nonnullable_rels = NULL;                          
      nullable_rels = NULL;
      leftjoinlist = rightjoinlist = NIL;
      break;
    }

                                                                      
    root->nullable_baserels = bms_add_members(root->nullable_baserels, nullable_rels);

       
                                                                     
                                                                        
                                                                        
                                                               
       
    my_quals = NIL;
    foreach (l, child_postponed_quals)
    {
      PostponedQual *pq = (PostponedQual *)lfirst(l);

      if (bms_is_subset(pq->relids, *qualscope))
      {
        my_quals = lappend(my_quals, pq->qual);
      }
      else
      {
           
                                                                     
                                                                 
           
        Assert(j->jointype == JOIN_INNER);
        *postponed_qual_list = lappend(*postponed_qual_list, pq);
      }
    }
                                                              
    my_quals = list_concat(my_quals, (List *)j->quals);

       
                                                                         
                                                                         
                                                                           
                                                                      
       
                                                                           
                                                        
       
    if (j->jointype != JOIN_INNER)
    {
      sjinfo = make_outerjoininfo(root, leftids, rightids, *inner_join_rels, j->jointype, my_quals);
      if (j->jointype == JOIN_SEMI)
      {
        ojscope = NULL;
      }
      else
      {
        ojscope = bms_union(sjinfo->min_lefthand, sjinfo->min_righthand);
      }
    }
    else
    {
      sjinfo = NULL;
      ojscope = NULL;
    }

                                         
    foreach (l, my_quals)
    {
      Node *qual = (Node *)lfirst(l);

      distribute_qual_to_rels(root, qual, false, below_outer_join, j->jointype, root->qual_security_level, *qualscope, ojscope, nonnullable_rels, NULL, postponed_qual_list);
    }

                                                              
    if (sjinfo)
    {
      root->join_info_list = lappend(root->join_info_list, sjinfo);
                                                                 
      update_placeholder_eval_levels(root, sjinfo);
    }

       
                                                                           
                                                                   
                 
       
    if (j->jointype == JOIN_FULL)
    {
                                                     
      joinlist = list_make1(list_make2(leftjoinlist, rightjoinlist));
    }
    else if (list_length(leftjoinlist) + list_length(rightjoinlist) <= join_collapse_limit)
    {
                                     
      joinlist = list_concat(leftjoinlist, rightjoinlist);
    }
    else
    {
                                                                  
      Node *leftpart, *rightpart;

                                                     
      if (list_length(leftjoinlist) == 1)
      {
        leftpart = (Node *)linitial(leftjoinlist);
      }
      else
      {
        leftpart = (Node *)leftjoinlist;
      }
      if (list_length(rightjoinlist) == 1)
      {
        rightpart = (Node *)linitial(rightjoinlist);
      }
      else
      {
        rightpart = (Node *)rightjoinlist;
      }
      joinlist = list_make2(leftpart, rightpart);
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
    joinlist = NIL;                          
  }
  return joinlist;
}

   
                                  
                                                                            
   
                                                                            
                                                                      
                     
   
                                                                           
                                                                           
                                                                         
                                                                  
   
static void
process_security_barrier_quals(PlannerInfo *root, int rti, Relids qualscope, bool below_outer_join)
{
  RangeTblEntry *rte = root->simple_rte_array[rti];
  Index security_level = 0;
  ListCell *lc;

     
                                                                          
                                                                           
                                                                            
             
     
  foreach (lc, rte->securityQuals)
  {
    List *qualset = (List *)lfirst(lc);
    ListCell *lc2;

    foreach (lc2, qualset)
    {
      Node *qual = (Node *)lfirst(lc2);

         
                                                                      
                                                                        
                                                                       
                                                                   
         
      distribute_qual_to_rels(root, qual, false, below_outer_join, JOIN_INNER, security_level, qualscope, qualscope, NULL, NULL, NULL);
    }
    security_level++;
  }

                                                                            
  Assert(security_level <= root->qual_security_level);
}

   
                      
                                                        
   
           
                                                                  
                                                                   
                                                                            
                                                                     
                                                                    
   
                                                                          
                        
   
                                                                    
                                                                              
                                 
   
static SpecialJoinInfo *
make_outerjoininfo(PlannerInfo *root, Relids left_rels, Relids right_rels, Relids inner_join_rels, JoinType jointype, List *clause)
{
  SpecialJoinInfo *sjinfo = makeNode(SpecialJoinInfo);
  Relids clause_relids;
  Relids strict_relids;
  Relids min_lefthand;
  Relids min_righthand;
  ListCell *l;

     
                                                                        
             
     
  Assert(jointype != JOIN_INNER);
  Assert(jointype != JOIN_RIGHT);

     
                                                                             
                                                                          
                                                                             
                                                                     
                                                             
     
                                                                         
                                                                          
                                                                           
                                                      
     
                                                                             
                      
     
  foreach (l, root->parse->rowMarks)
  {
    RowMarkClause *rc = (RowMarkClause *)lfirst(l);

    if (bms_is_member(rc->rti, right_rels) || (jointype == JOIN_FULL && bms_is_member(rc->rti, left_rels)))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                  
                                                                                          
                         errmsg("%s cannot be applied to the nullable side of an outer join", LCS_asString(rc->strength))));
    }
  }

  sjinfo->syn_lefthand = left_rels;
  sjinfo->syn_righthand = right_rels;
  sjinfo->jointype = jointype;
                                    
  sjinfo->delay_upper_joins = false;

  compute_semijoin_info(root, sjinfo, clause);

                                                     
  if (jointype == JOIN_FULL)
  {
    sjinfo->min_lefthand = bms_copy(left_rels);
    sjinfo->min_righthand = bms_copy(right_rels);
    sjinfo->lhs_strict = false;                            
    return sjinfo;
  }

     
                                                           
     
  clause_relids = pull_varnos(root, (Node *)clause);

     
                                                                         
                                 
     
  strict_relids = find_nonnullable_rels((Node *)clause);

                                                                   
  sjinfo->lhs_strict = bms_overlap(strict_relids, left_rels);

     
                                                                           
                                                                      
     
  min_lefthand = bms_intersect(clause_relids, left_rels);

     
                                                                           
                                                                      
     
  min_righthand = bms_int_members(bms_union(clause_relids, inner_join_rels), right_rels);

     
                                                               
     
  foreach (l, root->join_info_list)
  {
    SpecialJoinInfo *otherinfo = (SpecialJoinInfo *)lfirst(l);

       
                                                                          
                                                                          
                                                                        
       
    if (otherinfo->jointype == JOIN_FULL)
    {
      if (bms_overlap(left_rels, otherinfo->syn_lefthand) || bms_overlap(left_rels, otherinfo->syn_righthand))
      {
        min_lefthand = bms_add_members(min_lefthand, otherinfo->syn_lefthand);
        min_lefthand = bms_add_members(min_lefthand, otherinfo->syn_righthand);
      }
      if (bms_overlap(right_rels, otherinfo->syn_lefthand) || bms_overlap(right_rels, otherinfo->syn_righthand))
      {
        min_righthand = bms_add_members(min_righthand, otherinfo->syn_lefthand);
        min_righthand = bms_add_members(min_righthand, otherinfo->syn_righthand);
      }
                                                       
      continue;
    }

       
                                                                       
                                                                       
                                                                           
                                                                           
                                                                           
                                                                        
                                                                           
                                                                 
                                 
       
                                                                          
                                                                         
       
    if (bms_overlap(left_rels, otherinfo->syn_righthand))
    {
      if (bms_overlap(clause_relids, otherinfo->syn_righthand) && (jointype == JOIN_SEMI || jointype == JOIN_ANTI || !bms_overlap(strict_relids, otherinfo->min_righthand)))
      {
        min_lefthand = bms_add_members(min_lefthand, otherinfo->syn_lefthand);
        min_lefthand = bms_add_members(min_lefthand, otherinfo->syn_righthand);
      }
    }

       
                                                                         
                                                                        
                                                                          
                                                              
       
                                                                     
                                                                         
                                                                         
                                                                           
       
                                                                         
                                                            
       
                                                                        
                                                                        
                                                                           
                                                                       
                                                                     
                                                                     
                                                                          
                                                                   
                                                                        
                                              
       
    if (bms_overlap(right_rels, otherinfo->syn_righthand))
    {
      if (bms_overlap(clause_relids, otherinfo->syn_righthand) || !bms_overlap(clause_relids, otherinfo->min_lefthand) || jointype == JOIN_SEMI || jointype == JOIN_ANTI || otherinfo->jointype == JOIN_SEMI || otherinfo->jointype == JOIN_ANTI || !otherinfo->lhs_strict || otherinfo->delay_upper_joins)
      {
        min_righthand = bms_add_members(min_righthand, otherinfo->syn_lefthand);
        min_righthand = bms_add_members(min_righthand, otherinfo->syn_righthand);
      }
    }
  }

     
                                                                           
                                                                            
                                                                             
                                                                            
                                                                 
                                     
     
  foreach (l, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(l);
    Relids ph_syn_level = phinfo->ph_var->phrels;

                                                                     
    if (!bms_is_subset(ph_syn_level, right_rels))
    {
      continue;
    }

                                                                     
    min_righthand = bms_add_members(min_righthand, phinfo->ph_eval_at);
  }

     
                                                                           
                                                                         
                                                                            
                                 
     
  if (bms_is_empty(min_lefthand))
  {
    min_lefthand = bms_copy(left_rels);
  }
  if (bms_is_empty(min_righthand))
  {
    min_righthand = bms_copy(right_rels);
  }

                                     
  Assert(!bms_is_empty(min_lefthand));
  Assert(!bms_is_empty(min_righthand));
                                
  Assert(!bms_overlap(min_lefthand, min_righthand));

  sjinfo->min_lefthand = min_lefthand;
  sjinfo->min_righthand = min_righthand;

  return sjinfo;
}

   
                         
                                                           
   
                                                                          
                                                 
   
static void
compute_semijoin_info(PlannerInfo *root, SpecialJoinInfo *sjinfo, List *clause)
{
  List *semi_operators;
  List *semi_rhs_exprs;
  bool all_btree;
  bool all_hash;
  ListCell *lc;

                                                                      
  sjinfo->semi_can_btree = false;
  sjinfo->semi_can_hash = false;
  sjinfo->semi_operators = NIL;
  sjinfo->semi_rhs_exprs = NIL;

                                                 
  if (sjinfo->jointype != JOIN_SEMI)
  {
    return;
  }

     
                                                                     
                                                                            
                                                                           
     
                                                                   
                                                                           
                                                                          
                                                                           
                                                                            
                                                                         
                                                                        
                                                                           
                                                                          
                                                                            
                                                                             
                                                                          
                                                                          
     
                                                                          
                                                                            
                                                                     
                                                                          
                                                                             
                                                                             
     
  semi_operators = NIL;
  semi_rhs_exprs = NIL;
  all_btree = true;
  all_hash = enable_hashagg;                                         
  foreach (lc, clause)
  {
    OpExpr *op = (OpExpr *)lfirst(lc);
    Oid opno;
    Node *left_expr;
    Node *right_expr;
    Relids left_varnos;
    Relids right_varnos;
    Relids all_varnos;
    Oid opinputtype;

                                  
    if (!IsA(op, OpExpr) || list_length(op->args) != 2)
    {
                                                 
      all_varnos = pull_varnos(root, (Node *)op);
      if (!bms_overlap(all_varnos, sjinfo->syn_righthand) || bms_is_subset(all_varnos, sjinfo->syn_righthand))
      {
           
                                                                     
                                                                  
                 
           
        if (contain_volatile_functions((Node *)op))
        {
          return;
        }
        continue;
      }
                                                                 
      return;
    }

                                           
    opno = op->opno;
    left_expr = linitial(op->args);
    right_expr = lsecond(op->args);
    left_varnos = pull_varnos(root, left_expr);
    right_varnos = pull_varnos(root, right_expr);
    all_varnos = bms_union(left_varnos, right_varnos);
    opinputtype = exprType(left_expr);

                                       
    if (!bms_overlap(all_varnos, sjinfo->syn_righthand) || bms_is_subset(all_varnos, sjinfo->syn_righthand))
    {
         
                                                                   
                                                                      
         
      if (contain_volatile_functions((Node *)op))
      {
        return;
      }
      continue;
    }

                                           
    if (!bms_is_empty(right_varnos) && bms_is_subset(right_varnos, sjinfo->syn_righthand) && !bms_overlap(left_varnos, sjinfo->syn_righthand))
    {
                                                    
    }
    else if (!bms_is_empty(left_varnos) && bms_is_subset(left_varnos, sjinfo->syn_righthand) && !bms_overlap(right_varnos, sjinfo->syn_righthand))
    {
                                                   
      opno = get_commutator(opno);
      if (!OidIsValid(opno))
      {
        return;
      }
      right_expr = left_expr;
    }
    else
    {
                                          
      return;
    }

                                                               
    if (all_btree)
    {
                                               
      if (!op_mergejoinable(opno, opinputtype) || get_mergejoin_opfamilies(opno) == NIL)
      {
        all_btree = false;
      }
    }
    if (all_hash)
    {
                                                    
      if (!op_hashjoinable(opno, opinputtype))
      {
        all_hash = false;
      }
    }
    if (!(all_btree || all_hash))
    {
      return;
    }

                                             
    semi_operators = lappend_oid(semi_operators, opno);
    semi_rhs_exprs = lappend(semi_rhs_exprs, copyObject(right_expr));
  }

                                                                
  if (semi_rhs_exprs == NIL)
  {
    return;
  }

     
                                                                  
     
  if (contain_volatile_functions((Node *)semi_rhs_exprs))
  {
    return;
  }

     
                                                                             
                                                                         
     
  sjinfo->semi_can_btree = all_btree;
  sjinfo->semi_can_hash = all_hash;
  sjinfo->semi_operators = semi_operators;
  sjinfo->semi_rhs_exprs = semi_rhs_exprs;
}

                                                                               
   
                    
   
                                                                               

   
                           
                                                                            
                                                                       
                                                                           
                                                                             
                                                                          
                                                                   
                                                                           
                                                                              
   
                                               
                                                                       
                                                                            
                                               
                                                                             
                                                          
                                                                  
                                                                               
                             
                                                                            
                                                                   
                                                                          
                     
                                                                            
                                         
                                                                          
                                                                   
                                                            
   
                                                                               
                                                                           
                                                           
   
                                                                         
                                                                            
                                                                               
                                                                               
                                                                             
                   
   
static void
distribute_qual_to_rels(PlannerInfo *root, Node *clause, bool is_deduced, bool below_outer_join, JoinType jointype, Index security_level, Relids qualscope, Relids ojscope, Relids outerjoin_nonnullable, Relids deduced_nullable_relids, List **postponed_qual_list)
{
  Relids relids;
  bool is_pushed_down;
  bool outerjoin_delayed;
  bool pseudoconstant = false;
  bool maybe_equivalence;
  bool maybe_outer_join;
  Relids nullable_relids;
  RestrictInfo *restrictinfo;

     
                                                      
     
  relids = pull_varnos(root, clause);

     
                                                                         
                                                                        
                                                                            
                                                                           
                                                                           
                                                                         
                                                                         
                                                                             
                                                                           
     
  if (!bms_is_subset(relids, qualscope))
  {
    PostponedQual *pq = (PostponedQual *)palloc(sizeof(PostponedQual));

    Assert(root->hasLateralRTEs);                                   
    Assert(jointype == JOIN_INNER);                                       
    Assert(!is_deduced);                                              
    pq->qual = clause;
    pq->relids = relids;
    *postponed_qual_list = lappend(*postponed_qual_list, pq);
    return;
  }

     
                                                                         
                                                                           
     
  if (ojscope && !bms_is_subset(relids, ojscope))
  {
    elog(ERROR, "JOIN qualification cannot refer to other relations");
  }

     
                                                                         
                                                                           
     
                                                                         
                                           
     
                                                                            
                                                                       
                         
     
                                                                             
                                                                     
                                                                          
                                                                     
                                                                         
                                                                          
                                                                            
                                                                          
                                                                           
                                                                            
                                                                            
                                                                      
             
     
  if (bms_is_empty(relids))
  {
    if (ojscope)
    {
                                                           
      relids = bms_copy(ojscope);
                                                                    
    }
    else
    {
                                            
      relids = bms_copy(qualscope);
      if (!contain_volatile_functions(clause))
      {
                                 
        pseudoconstant = true;
                                                         
        root->hasPseudoConstantQuals = true;
                                                             
        if (!below_outer_join)
        {
          relids = get_relids_in_jointree((Node *)root->parse->jointree, false);
          qualscope = bms_copy(relids);
        }
      }
    }
  }

               
                                                                      
                     
     
                                                                       
                                                                           
                                                                            
                                                                       
                                                               
                                                               
                                                          
                                                                    
                                                                            
                                                                       
                                                                  
     
                                                                         
                                                                         
                                                                           
                                                                        
                                                                          
                                                                           
                                                                        
                                                                         
                                                      
     
                                                                        
                                                                            
                                                                         
                                                                        
               
     
  if (is_deduced)
  {
       
                                                                          
                                                                         
                                                                           
                                                                           
                                    
       
    Assert(!ojscope);
    is_pushed_down = true;
    outerjoin_delayed = false;
    nullable_relids = deduced_nullable_relids;
                                                
    maybe_equivalence = false;
    maybe_outer_join = false;
  }
  else if (bms_overlap(relids, outerjoin_nonnullable))
  {
       
                                                                        
                                                             
       
                                                                      
                                                                           
                                                                         
                                                                         
                                              
       
    is_pushed_down = false;
    maybe_equivalence = false;
    maybe_outer_join = true;

                                                             
    outerjoin_delayed = check_outerjoin_delay(root, &relids, &nullable_relids, false);

       
                                                                          
                                                                          
                                                                          
                                                                  
                                               
       
                                                                       
                        
       
    Assert(ojscope);
    relids = ojscope;
    Assert(!pseudoconstant);
  }
  else
  {
       
                                                                           
                                   
       
    is_pushed_down = true;

                                                             
    outerjoin_delayed = check_outerjoin_delay(root, &relids, &nullable_relids, true);

    if (outerjoin_delayed)
    {
                                                         
      Assert(root->hasLateralRTEs || bms_is_subset(relids, qualscope));
      Assert(ojscope == NULL || bms_is_subset(relids, ojscope));

         
                                                                        
                                                          
         
      maybe_equivalence = false;

         
                                                                       
                                                                       
                                                                       
                                                    
         
      if (check_redundant_nullability_qual(root, clause))
      {
        return;
      }
    }
    else
    {
         
                                                                        
                                                                        
                                                                        
                                                                      
                                                                   
         
      maybe_equivalence = true;
      if (outerjoin_nonnullable != NULL)
      {
        below_outer_join = true;
      }
    }

       
                                                                        
                                                   
       
    maybe_outer_join = false;
  }

     
                                         
     
  restrictinfo = make_restrictinfo(root, (Expr *)clause, is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, relids, outerjoin_nonnullable, nullable_relids);

     
                                                                    
                                                                            
                                                                         
                                                                       
     
                                                                          
                                                                        
                                                                           
                            
     
  if (bms_membership(relids) == BMS_MULTIPLE)
  {
    List *vars = pull_var_clause(clause, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);

    add_vars_to_targetlist(root, vars, relids, false);
    list_free(vars);
  }

     
                                                                         
                                                                         
                                           
     
  check_mergejoinable(restrictinfo);

     
                                                                         
                                                                           
                                                                            
     
                                                           
                                                                             
                                                                             
                                                                          
     
                                                                            
                                                                           
                                                        
                                                                      
                          
     
                                                            
                                                                       
                                                                         
                                                                           
                                                                        
                                                                 
     
                                               
                                        
     
                                                                         
                                                                          
                                                                 
                                                                   
                                                                          
     
  if (restrictinfo->mergeopfamilies)
  {
    if (maybe_equivalence)
    {
      if (check_equivalence_delay(root, restrictinfo) && process_equivalence(root, &restrictinfo, below_outer_join))
      {
        return;
      }
                                                                    
      if (restrictinfo->mergeopfamilies)                                 
      {
        initialize_mergeclause_eclasses(root, restrictinfo);
      }
                                                                   
    }
    else if (maybe_outer_join && restrictinfo->can_join)
    {
                                                           
      initialize_mergeclause_eclasses(root, restrictinfo);
                                                           
      if (bms_is_subset(restrictinfo->left_relids, outerjoin_nonnullable) && !bms_overlap(restrictinfo->right_relids, outerjoin_nonnullable))
      {
                                         
        root->left_join_clauses = lappend(root->left_join_clauses, restrictinfo);
        return;
      }
      if (bms_is_subset(restrictinfo->right_relids, outerjoin_nonnullable) && !bms_overlap(restrictinfo->left_relids, outerjoin_nonnullable))
      {
                                         
        root->right_join_clauses = lappend(root->right_join_clauses, restrictinfo);
        return;
      }
      if (jointype == JOIN_FULL)
      {
                                                               
        root->full_join_clauses = lappend(root->full_join_clauses, restrictinfo);
        return;
      }
                                                                    
    }
    else
    {
                                                    
      initialize_mergeclause_eclasses(root, restrictinfo);
    }
  }

                                                                    
  distribute_restrictinfo_to_rels(root, restrictinfo);
}

   
                         
                                                                       
                                                                     
                                                       
   
                                                                              
                                                                             
                                                                           
                                                                         
                                                                            
                                                 
   
                                                                               
                                                                             
                                                                          
                                                                             
                                                                             
                                                                           
                                                                               
                                                                              
                                  
   
                                                                                
                                                                          
                                                                             
                                                                            
                                                                         
                                                                               
                                                                              
                                                                            
                                                            
   
                                                                                
                                                                               
                                          
   
                                                                               
                                                                            
                                                                           
                                                                     
                                                                         
                                                                          
                                                                         
                                                                           
                                                                          
                                                                          
                                                                         
                        
   
static bool
check_outerjoin_delay(PlannerInfo *root, Relids *relids_p,                       
    Relids *nullable_relids_p,                                                   
    bool is_pushed_down)
{
  Relids relids;
  Relids nullable_relids;
  bool outerjoin_delayed;
  bool found_some;

                                     
  if (root->join_info_list == NIL)
  {
    *nullable_relids_p = NULL;
    return false;
  }

                                                                      
  relids = bms_copy(*relids_p);
  nullable_relids = NULL;
  outerjoin_delayed = false;
  do
  {
    ListCell *l;

    found_some = false;
    foreach (l, root->join_info_list)
    {
      SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(l);

                                                         
      if (bms_overlap(relids, sjinfo->min_righthand) || (sjinfo->jointype == JOIN_FULL && bms_overlap(relids, sjinfo->min_lefthand)))
      {
                                                           
        if (!bms_is_subset(sjinfo->min_lefthand, relids) || !bms_is_subset(sjinfo->min_righthand, relids))
        {
                                  
          relids = bms_add_members(relids, sjinfo->min_lefthand);
          relids = bms_add_members(relids, sjinfo->min_righthand);
          outerjoin_delayed = true;
                                            
          found_some = true;
        }
                                                         
        nullable_relids = bms_add_members(nullable_relids, sjinfo->min_righthand);
        if (sjinfo->jointype == JOIN_FULL)
        {
          nullable_relids = bms_add_members(nullable_relids, sjinfo->min_lefthand);
        }
                                             
        if (is_pushed_down && sjinfo->jointype != JOIN_FULL && bms_overlap(relids, sjinfo->min_lefthand))
        {
          sjinfo->delay_upper_joins = true;
        }
      }
    }
  } while (found_some);

                                                           
  nullable_relids = bms_int_members(nullable_relids, *relids_p);

                                                     
  bms_free(*relids_p);
  *relids_p = relids;
  *nullable_relids_p = nullable_relids;
  return outerjoin_delayed;
}

   
                           
                                                                     
                                                                   
   
                                                                              
                                                                               
                                                                               
                                                                        
                                                                                
                                                                           
              
   
static bool
check_equivalence_delay(PlannerInfo *root, RestrictInfo *restrictinfo)
{
  Relids relids;
  Relids nullable_relids;

                                     
  if (root->join_info_list == NIL)
  {
    return true;
  }

                                                            
  relids = bms_copy(restrictinfo->left_relids);
                                           
  if (check_outerjoin_delay(root, &relids, &nullable_relids, true))
  {
    return false;
  }

                                        
  relids = bms_copy(restrictinfo->right_relids);
  if (check_outerjoin_delay(root, &relids, &nullable_relids, true))
  {
    return false;
  }

  return true;
}

   
                                    
                                                                        
                             
   
                                                                           
                                                                       
                                                                           
             
   
static bool
check_redundant_nullability_qual(PlannerInfo *root, Node *clause)
{
  Var *forced_null_var;
  Index forced_null_rel;
  ListCell *lc;

                                                              
  forced_null_var = find_forced_null_var(clause);
  if (forced_null_var == NULL)
  {
    return false;
  }
  forced_null_rel = forced_null_var->varno;

     
                                                                         
                                         
     
  foreach (lc, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(lc);

    if (sjinfo->jointype == JOIN_ANTI && bms_is_member(forced_null_rel, sjinfo->syn_righthand))
    {
      return true;
    }
  }

  return false;
}

   
                                   
                                                                       
                     
   
                                                                        
                                                                           
                                                                           
   
void
distribute_restrictinfo_to_rels(PlannerInfo *root, RestrictInfo *restrictinfo)
{
  Relids relids = restrictinfo->required_relids;
  RelOptInfo *rel;

  switch (bms_membership(relids))
  {
  case BMS_SINGLETON:

       
                                                                     
                                                  
       
    rel = find_base_rel(root, bms_singleton_member(relids));

                                              
    rel->baserestrictinfo = lappend(rel->baserestrictinfo, restrictinfo);
                                    
    rel->baserestrict_min_security = Min(rel->baserestrict_min_security, restrictinfo->security_level);
    break;
  case BMS_MULTIPLE:

       
                                                                     
                         
       

       
                                                                       
                                                   
       
    check_hashjoinable(restrictinfo);

       
                                                                   
       
    add_join_clause_to_rels(root, restrictinfo, relids);
    break;
  default:

       
                                                                    
                                                                       
       
    elog(ERROR, "cannot cope with variable-free clause");
    break;
  }
}

   
                            
                                                                        
                                                                      
                         
   
                                                                             
                                                                           
                                                                      
                                                                          
                                    
   
                                                                           
                                                                            
                                                                          
                                                              
   
                                                                             
   
                                                                        
                                                                         
                                                                      
                                                                         
   
                                                                     
                                                                           
                               
   
                                                                    
                                                                   
   
void
process_implied_equality(PlannerInfo *root, Oid opno, Oid collation, Expr *item1, Expr *item2, Relids qualscope, Relids nullable_relids, Index security_level, bool below_outer_join, bool both_const)
{
  Expr *clause;

     
                                                                          
                                                                           
     
  clause = make_opclause(opno, BOOLOID,                   
      false,                                          
      copyObject(item1), copyObject(item2), InvalidOid, collation);

                                                              
  if (both_const)
  {
    clause = (Expr *)eval_const_expressions(root, (Node *)clause);

                                                         
    if (clause && IsA(clause, Const))
    {
      Const *cclause = (Const *)clause;

      Assert(cclause->consttype == BOOLOID);
      if (!cclause->constisnull && DatumGetBool(cclause->constvalue))
      {
        return;
      }
    }
  }

     
                                                                      
     
  distribute_qual_to_rels(root, (Node *)clause, true, below_outer_join, JOIN_INNER, security_level, qualscope, NULL, NULL, nullable_relids, NULL);
}

   
                                                                               
   
                                                                         
                                                                     
   
                                                                     
                                                                           
                               
   
                                                                     
                                                                      
   
RestrictInfo *
build_implied_join_equality(PlannerInfo *root, Oid opno, Oid collation, Expr *item1, Expr *item2, Relids qualscope, Relids nullable_relids, Index security_level)
{
  RestrictInfo *restrictinfo;
  Expr *clause;

     
                                                                          
                                                                           
     
  clause = make_opclause(opno, BOOLOID,                   
      false,                                          
      copyObject(item1), copyObject(item2), InvalidOid, collation);

     
                                         
     
  restrictinfo = make_restrictinfo(root, clause, true,                     
      false,                                                                  
      false,                                                               
      security_level,                                                      
      qualscope,                                                            
      NULL,                                                              
      nullable_relids);                                                     

                                                  
  check_mergejoinable(restrictinfo);
  check_hashjoinable(restrictinfo);

  return restrictinfo;
}

   
                               
                                                                        
   
                                                                      
                                                                         
                                                                           
                                                                            
                                                                             
   
                                                                          
                                                                        
                                                                     
   
void
match_foreign_keys_to_quals(PlannerInfo *root)
{
  List *newlist = NIL;
  ListCell *lc;

  foreach (lc, root->fkey_list)
  {
    ForeignKeyOptInfo *fkinfo = (ForeignKeyOptInfo *)lfirst(lc);
    RelOptInfo *con_rel;
    RelOptInfo *ref_rel;
    int colno;

       
                                                                           
                                                                           
                                                                
       
    if (fkinfo->con_relid >= root->simple_rel_array_size || fkinfo->ref_relid >= root->simple_rel_array_size)
    {
      continue;                    
    }
    con_rel = root->simple_rel_array[fkinfo->con_relid];
    if (con_rel == NULL)
    {
      continue;
    }
    ref_rel = root->simple_rel_array[fkinfo->ref_relid];
    if (ref_rel == NULL)
    {
      continue;
    }

       
                                                                           
                                                                         
                                                 
       
    if (con_rel->reloptkind != RELOPT_BASEREL || ref_rel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

       
                                                                     
       
                                                                       
                                                                       
                                                                        
                                                                        
                              
       
    for (colno = 0; colno < fkinfo->nkeys; colno++)
    {
      AttrNumber con_attno, ref_attno;
      Oid fpeqop;
      ListCell *lc2;

      fkinfo->eclass[colno] = match_eclasses_to_foreign_key_col(root, fkinfo, colno);
                                                                      
      if (fkinfo->eclass[colno] != NULL)
      {
        fkinfo->nmatched_ec++;
        continue;
      }

         
                                                                         
                                                       
         
      con_attno = fkinfo->conkey[colno];
      ref_attno = fkinfo->confkey[colno];
      fpeqop = InvalidOid;                                        

      foreach (lc2, con_rel->joininfo)
      {
        RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc2);
        OpExpr *clause = (OpExpr *)rinfo->clause;
        Var *leftvar;
        Var *rightvar;

                                              
        if (rinfo->outerjoin_delayed)
        {
          continue;
        }

                                                              
        if (!IsA(clause, OpExpr) || list_length(clause->args) != 2)
        {
          continue;
        }
        leftvar = (Var *)get_leftop((Expr *)clause);
        rightvar = (Var *)get_rightop((Expr *)clause);

                                                              
        while (leftvar && IsA(leftvar, RelabelType))
        {
          leftvar = (Var *)((RelabelType *)leftvar)->arg;
        }
        if (!(leftvar && IsA(leftvar, Var)))
        {
          continue;
        }
        while (rightvar && IsA(rightvar, RelabelType))
        {
          rightvar = (Var *)((RelabelType *)rightvar)->arg;
        }
        if (!(rightvar && IsA(rightvar, Var)))
        {
          continue;
        }

                                                                       
        if (fkinfo->ref_relid == leftvar->varno && ref_attno == leftvar->varattno && fkinfo->con_relid == rightvar->varno && con_attno == rightvar->varattno)
        {
                                                         
          if (clause->opno == fkinfo->conpfeqop[colno])
          {
            fkinfo->rinfos[colno] = lappend(fkinfo->rinfos[colno], rinfo);
            fkinfo->nmatched_ri++;
          }
        }
        else if (fkinfo->ref_relid == rightvar->varno && ref_attno == rightvar->varattno && fkinfo->con_relid == leftvar->varno && con_attno == leftvar->varattno)
        {
             
                                                                     
                                                                   
                                                                    
                                                            
                        
             
          if (!OidIsValid(fpeqop))
          {
            fpeqop = get_commutator(fkinfo->conpfeqop[colno]);
          }
          if (clause->opno == fpeqop)
          {
            fkinfo->rinfos[colno] = lappend(fkinfo->rinfos[colno], rinfo);
            fkinfo->nmatched_ri++;
          }
        }
      }
                                                                      
      if (fkinfo->rinfos[colno])
      {
        fkinfo->nmatched_rcols++;
      }
    }

       
                                                                           
                                                                    
                                                                         
                                                                  
       
    if ((fkinfo->nmatched_ec + fkinfo->nmatched_rcols) == fkinfo->nkeys)
    {
      newlist = lappend(newlist, fkinfo);
    }
  }
                                                                 
  root->fkey_list = newlist;
}

                                                                               
   
                                                      
   
                                                                               

   
                       
                                                                      
                                      
   
                                                                
                                                                     
                                                                      
   
static void
check_mergejoinable(RestrictInfo *restrictinfo)
{
  Expr *clause = restrictinfo->clause;
  Oid opno;
  Node *leftarg;

  if (restrictinfo->pseudoconstant)
  {
    return;
  }
  if (!is_opclause(clause))
  {
    return;
  }
  if (list_length(((OpExpr *)clause)->args) != 2)
  {
    return;
  }

  opno = ((OpExpr *)clause)->opno;
  leftarg = linitial(((OpExpr *)clause)->args);

  if (op_mergejoinable(opno, exprType(leftarg)) && !contain_volatile_functions((Node *)clause))
  {
    restrictinfo->mergeopfamilies = get_mergejoin_opfamilies(opno);
  }

     
                                                                            
                                                                            
                                      
     
}

   
                      
                                                                    
                                      
   
                                                               
                                                                    
                                                                      
   
static void
check_hashjoinable(RestrictInfo *restrictinfo)
{
  Expr *clause = restrictinfo->clause;
  Oid opno;
  Node *leftarg;

  if (restrictinfo->pseudoconstant)
  {
    return;
  }
  if (!is_opclause(clause))
  {
    return;
  }
  if (list_length(((OpExpr *)clause)->args) != 2)
  {
    return;
  }

  opno = ((OpExpr *)clause)->opno;
  leftarg = linitial(((OpExpr *)clause)->args);

  if (op_hashjoinable(opno, exprType(leftarg)) && !contain_volatile_functions((Node *)clause))
  {
    restrictinfo->hashjoinoperator = opno;
  }
}
