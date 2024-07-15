                                                                            
   
                  
                                                                      
   
                                                                
                           
                     
                                   
                       
                             
                                                                       
                       
                               
   
   
                                                                         
                                                                        
   
   
                  
                                               
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/placeholder.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define pull_varnos_of_level(a, b, c) pull_varnos_of_level_new(a, b, c)

typedef struct pullup_replace_vars_context
{
  PlannerInfo *root;
  List *targetlist;                                                 
  RangeTblEntry *target_rte;                      
  Relids relids;                                                          
                                                                           
  bool *outer_hasSubLinks;                                     
  int varno;                                        
  bool need_phvs;                                             
  bool wrap_non_vars;                                               
  Node **rv_cache;                                            
} pullup_replace_vars_context;

typedef struct reduce_outer_joins_state
{
  Relids relids;                                            
  bool contains_outer;                                          
  List *sub_states;                                               
} reduce_outer_joins_state;

static Node *
pull_up_sublinks_jointree_recurse(PlannerInfo *root, Node *jtnode, Relids *relids);
static Node *
pull_up_sublinks_qual_recurse(PlannerInfo *root, Node *node, Node **jtlink1, Relids available_rels1, Node **jtlink2, Relids available_rels2);
static Node *
pull_up_subqueries_recurse(PlannerInfo *root, Node *jtnode, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel);
static Node *
pull_up_simple_subquery(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel);
static Node *
pull_up_simple_union_all(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte);
static void
pull_up_union_leaf_queries(Node *setOp, PlannerInfo *root, int parentRTindex, Query *setOpQuery, int childRToffset);
static void
make_setop_translation_list(Query *query, Index newvarno, List **translated_vars);
static bool
is_simple_subquery(PlannerInfo *root, Query *subquery, RangeTblEntry *rte, JoinExpr *lowest_outer_join);
static Node *
pull_up_simple_values(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte);
static bool
is_simple_values(PlannerInfo *root, RangeTblEntry *rte);
static bool
is_simple_union_all(Query *subquery);
static bool
is_simple_union_all_recurse(Node *setOp, Query *setOpQuery, List *colTypes);
static bool
is_safe_append_member(Query *subquery);
static bool
jointree_contains_lateral_outer_refs(PlannerInfo *root, Node *jtnode, bool restricted, Relids safe_upper_varnos);
static void
replace_vars_in_jointree(Node *jtnode, pullup_replace_vars_context *context, JoinExpr *lowest_nulling_outer_join);
static Node *
pullup_replace_vars(Node *expr, pullup_replace_vars_context *context);
static Node *
pullup_replace_vars_callback(Var *var, replace_rte_variables_context *context);
static Query *
pullup_replace_vars_subquery(Query *query, pullup_replace_vars_context *context);
static reduce_outer_joins_state *
reduce_outer_joins_pass1(Node *jtnode);
static void
reduce_outer_joins_pass2(Node *jtnode, reduce_outer_joins_state *state, PlannerInfo *root, Relids nonnullable_rels, List *nonnullable_vars, List *forced_null_vars);
static Node *
remove_useless_results_recurse(PlannerInfo *root, Node *jtnode);
static int
get_result_relid(PlannerInfo *root, Node *jtnode);
static void
remove_result_refs(PlannerInfo *root, int varno, Node *newjtloc);
static bool
find_dependent_phvs(PlannerInfo *root, int varno);
static bool
find_dependent_phvs_in_jointree(PlannerInfo *root, Node *node, int varno);
static void
substitute_phv_relids(Node *node, int varno, Relids subrelids);
static void
fix_append_rel_relids(List *append_rel_list, int varno, Relids subrelids);
static Node *
find_jointree_node_for_rel(Node *jtnode, int relid);

   
                          
                                                                         
              
   
                                                                             
                                                                         
                                                                             
                                                                         
                                                       
   
                                                                            
                                                                              
   
void
replace_empty_jointree(Query *parse)
{
  RangeTblEntry *rte;
  Index rti;
  RangeTblRef *rtr;

                                                     
  if (parse->jointree->fromlist != NIL)
  {
    return;
  }

                                                                     
  if (parse->setOperations)
  {
    return;
  }

                           
  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RESULT;
  rte->eref = makeAlias("*RESULT*", NIL);

                            
  parse->rtable = lappend(parse->rtable, rte);
  rti = list_length(parse->rtable);

                                             
  rtr = makeNode(RangeTblRef);
  rtr->rtindex = rti;
  parse->jointree->fromlist = list_make1(rtr);
}

   
                    
                                                                
                                 
   
                                                                      
                                                                       
                                                                          
                                                                          
                                                                      
                                                                          
                                                                          
                                                                     
                                                                           
                                                                         
                                                              
   
                                                                          
                                                                          
   
                                                                           
                                     
   
                                                                        
                                                                              
                                                                               
                                                                    
   
void
pull_up_sublinks(PlannerInfo *root)
{
  Node *jtnode;
  Relids relids;

                                            
  jtnode = pull_up_sublinks_jointree_recurse(root, (Node *)root->parse->jointree, &relids);

     
                                                                            
                                                                    
     
  if (IsA(jtnode, FromExpr))
  {
    root->parse->jointree = (FromExpr *)jtnode;
  }
  else
  {
    root->parse->jointree = makeFromExpr(list_make1(jtnode), NULL);
  }
}

   
                                                         
   
                                                                           
                                                    
   
static Node *
pull_up_sublinks_jointree_recurse(PlannerInfo *root, Node *jtnode, Relids *relids)
{
                                                                           
  check_stack_depth();

  if (jtnode == NULL)
  {
    *relids = NULL;
  }
  else if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;

    *relids = bms_make_singleton(varno);
                                       
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    List *newfromlist = NIL;
    Relids frelids = NULL;
    FromExpr *newf;
    Node *jtlink;
    ListCell *l;

                                                                     
    foreach (l, f->fromlist)
    {
      Node *newchild;
      Relids childrelids;

      newchild = pull_up_sublinks_jointree_recurse(root, lfirst(l), &childrelids);
      newfromlist = lappend(newfromlist, newchild);
      frelids = bms_join(frelids, childrelids);
    }
                                                      
    newf = makeFromExpr(newfromlist, NULL);
                                                         
    jtlink = (Node *)newf;
                                                                 
    newf->quals = pull_up_sublinks_qual_recurse(root, f->quals, &jtlink, frelids, NULL, NULL);

       
                                                                         
                                                                           
                                                       
       
                                                                          
                                                                         
                       
       
    *relids = frelids;
    jtnode = jtlink;
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j;
    Relids leftrelids;
    Relids rightrelids;
    Node *jtlink;

       
                                                                         
                       
       
    j = (JoinExpr *)palloc(sizeof(JoinExpr));
    memcpy(j, jtnode, sizeof(JoinExpr));
    jtlink = (Node *)j;

                                                              
    j->larg = pull_up_sublinks_jointree_recurse(root, j->larg, &leftrelids);
    j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &rightrelids);

       
                                                                        
                                                                          
                                                                         
                                                                   
                                                                       
                                                                          
                                            
       
                                                                      
                   
       
    switch (j->jointype)
    {
    case JOIN_INNER:
      j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &jtlink, bms_union(leftrelids, rightrelids), NULL, NULL);
      break;
    case JOIN_LEFT:
      j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->rarg, rightrelids, NULL, NULL);
      break;
    case JOIN_FULL:
                                                  
      break;
    case JOIN_RIGHT:
      j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->larg, leftrelids, NULL, NULL);
      break;
    default:
      elog(ERROR, "unrecognized join type: %d", (int)j->jointype);
      break;
    }

       
                                                                          
                                                                         
                                                                           
                                                                       
                                                                          
             
       
    *relids = bms_join(leftrelids, rightrelids);
    if (j->rtindex)
    {
      *relids = bms_add_member(*relids, j->rtindex);
    }
    jtnode = jtlink;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return jtnode;
}

   
                                                               
   
                                                                             
                                                                        
                                                                               
                                                                         
                                                                               
                                                                              
                                                                               
                                                                         
                                                       
   
                                                                             
   
static Node *
pull_up_sublinks_qual_recurse(PlannerInfo *root, Node *node, Node **jtlink1, Relids available_rels1, Node **jtlink2, Relids available_rels2)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, SubLink))
  {
    SubLink *sublink = (SubLink *)node;
    JoinExpr *j;
    Relids child_rels;

                                                   
    if (sublink->subLinkType == ANY_SUBLINK)
    {
      if ((j = convert_ANY_sublink_to_join(root, sublink, available_rels1)) != NULL)
      {
                                                              
        j->larg = *jtlink1;
        *jtlink1 = (Node *)j;
                                                          
        j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &child_rels);

           
                                                                      
                                                                 
                                                   
           
        j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->larg, available_rels1, &j->rarg, child_rels);
                                                    
        return NULL;
      }
      if (available_rels2 != NULL && (j = convert_ANY_sublink_to_join(root, sublink, available_rels2)) != NULL)
      {
                                                              
        j->larg = *jtlink2;
        *jtlink2 = (Node *)j;
                                                          
        j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &child_rels);

           
                                                                      
                                                                 
                                                   
           
        j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->larg, available_rels2, &j->rarg, child_rels);
                                                    
        return NULL;
      }
    }
    else if (sublink->subLinkType == EXISTS_SUBLINK)
    {
      if ((j = convert_EXISTS_sublink_to_join(root, sublink, false, available_rels1)) != NULL)
      {
                                                              
        j->larg = *jtlink1;
        *jtlink1 = (Node *)j;
                                                          
        j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &child_rels);

           
                                                                      
                                                                 
                                                   
           
        j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->larg, available_rels1, &j->rarg, child_rels);
                                                    
        return NULL;
      }
      if (available_rels2 != NULL && (j = convert_EXISTS_sublink_to_join(root, sublink, false, available_rels2)) != NULL)
      {
                                                              
        j->larg = *jtlink2;
        *jtlink2 = (Node *)j;
                                                          
        j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &child_rels);

           
                                                                      
                                                                 
                                                   
           
        j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->larg, available_rels2, &j->rarg, child_rels);
                                                    
        return NULL;
      }
    }
                                   
    return node;
  }
  if (is_notclause(node))
  {
                                                                    
    SubLink *sublink = (SubLink *)get_notclausearg((Expr *)node);
    JoinExpr *j;
    Relids child_rels;

    if (sublink && IsA(sublink, SubLink))
    {
      if (sublink->subLinkType == EXISTS_SUBLINK)
      {
        if ((j = convert_EXISTS_sublink_to_join(root, sublink, true, available_rels1)) != NULL)
        {
                                                                
          j->larg = *jtlink1;
          *jtlink1 = (Node *)j;
                                                            
          j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &child_rels);

             
                                                                   
                                                                     
                                                                   
                                                   
             
          j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->rarg, child_rels, NULL, NULL);
                                                      
          return NULL;
        }
        if (available_rels2 != NULL && (j = convert_EXISTS_sublink_to_join(root, sublink, true, available_rels2)) != NULL)
        {
                                                                
          j->larg = *jtlink2;
          *jtlink2 = (Node *)j;
                                                            
          j->rarg = pull_up_sublinks_jointree_recurse(root, j->rarg, &child_rels);

             
                                                                   
                                                                     
                                                                   
                                                   
             
          j->quals = pull_up_sublinks_qual_recurse(root, j->quals, &j->rarg, child_rels, NULL, NULL);
                                                      
          return NULL;
        }
      }
    }
                                   
    return node;
  }
  if (is_andclause(node))
  {
                                 
    List *newclauses = NIL;
    ListCell *l;

    foreach (l, ((BoolExpr *)node)->args)
    {
      Node *oldclause = (Node *)lfirst(l);
      Node *newclause;

      newclause = pull_up_sublinks_qual_recurse(root, oldclause, jtlink1, available_rels1, jtlink2, available_rels2);
      if (newclause)
      {
        newclauses = lappend(newclauses, newclause);
      }
    }
                                                                   
    if (newclauses == NIL)
    {
      return NULL;
    }
    else if (list_length(newclauses) == 1)
    {
      return (Node *)linitial(newclauses);
    }
    else
    {
      return (Node *)make_andclause(newclauses);
    }
  }
                          
  return node;
}

   
                                  
                                                                    
   
                                                                         
                                                                        
                                                                        
                                                                            
                                                          
   
                                                                        
                                                                      
                                                                    
                                                                   
   
                                                                           
              
   
void
inline_set_returning_functions(PlannerInfo *root)
{
  ListCell *rt;

  foreach (rt, root->parse->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(rt);

    if (rte->rtekind == RTE_FUNCTION)
    {
      Query *funcquery;

                                                             
      funcquery = inline_set_returning_function(root, rte);
      if (funcquery)
      {
                                                                 
        rte->rtekind = RTE_SUBQUERY;
        rte->subquery = funcquery;
        rte->security_barrier = false;
                                                                   
        rte->functions = NIL;
        rte->funcordinality = false;
      }
    }
  }
}

   
                      
                                                                     
                                                                    
                                                                          
                                                                 
                                       
   
void
pull_up_subqueries(PlannerInfo *root)
{
                                                       
  Assert(IsA(root->parse->jointree, FromExpr));
                                                              
  root->parse->jointree = (FromExpr *)pull_up_subqueries_recurse(root, (Node *)root->parse->jointree, NULL, NULL, NULL);
                                       
  Assert(IsA(root->parse->jointree, FromExpr));
}

   
                              
                                          
   
                                                                            
   
                                                                      
                                                                         
                                                                            
   
                                                                            
                                                                       
                                                                              
                                                                              
              
   
                                                                 
                                                                  
                                                                              
                                                                          
   
                                                                         
                                                                        
                                                                          
                                                                             
                                                                               
                                                                           
                                                                        
   
                                                                         
                                                                        
                                                                         
                                                                         
                                                                         
                                                                           
                                                                       
   
static Node *
pull_up_subqueries_recurse(PlannerInfo *root, Node *jtnode, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel)
{
                                                                           
  check_stack_depth();
                                                                       
  CHECK_FOR_INTERRUPTS();

  Assert(jtnode != NULL);
  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;
    RangeTblEntry *rte = rt_fetch(varno, root->parse->rtable);

       
                                                                           
                
       
                                                                           
                                             
       
    if (rte->rtekind == RTE_SUBQUERY && is_simple_subquery(root, rte->subquery, rte, lowest_outer_join) && (containing_appendrel == NULL || is_safe_append_member(rte->subquery)))
    {
      return pull_up_simple_subquery(root, jtnode, rte, lowest_outer_join, lowest_nulling_outer_join, containing_appendrel);
    }

       
                                                                         
                                  
       
                                                                          
                                                                           
                                                                          
                                              
       
    if (rte->rtekind == RTE_SUBQUERY && is_simple_union_all(rte->subquery))
    {
      return pull_up_simple_union_all(root, jtnode, rte);
    }

       
                                            
       
                                                                    
                                                                   
       
    if (rte->rtekind == RTE_VALUES && lowest_outer_join == NULL && containing_appendrel == NULL && is_simple_values(root, rte))
    {
      return pull_up_simple_values(root, jtnode, rte);
    }

                                             
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    Assert(containing_appendrel == NULL);
                                                   
    foreach (l, f->fromlist)
    {
      lfirst(l) = pull_up_subqueries_recurse(root, lfirst(l), lowest_outer_join, lowest_nulling_outer_join, NULL);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

    Assert(containing_appendrel == NULL);
                                                                      
    switch (j->jointype)
    {
    case JOIN_INNER:
      j->larg = pull_up_subqueries_recurse(root, j->larg, lowest_outer_join, lowest_nulling_outer_join, NULL);
      j->rarg = pull_up_subqueries_recurse(root, j->rarg, lowest_outer_join, lowest_nulling_outer_join, NULL);
      break;
    case JOIN_LEFT:
    case JOIN_SEMI:
    case JOIN_ANTI:
      j->larg = pull_up_subqueries_recurse(root, j->larg, j, lowest_nulling_outer_join, NULL);
      j->rarg = pull_up_subqueries_recurse(root, j->rarg, j, j, NULL);
      break;
    case JOIN_FULL:
      j->larg = pull_up_subqueries_recurse(root, j->larg, j, j, NULL);
      j->rarg = pull_up_subqueries_recurse(root, j->rarg, j, j, NULL);
      break;
    case JOIN_RIGHT:
      j->larg = pull_up_subqueries_recurse(root, j->larg, j, j, NULL);
      j->rarg = pull_up_subqueries_recurse(root, j->rarg, j, lowest_nulling_outer_join, NULL);
      break;
    default:
      elog(ERROR, "unrecognized join type: %d", (int)j->jointype);
      break;
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return jtnode;
}

   
                           
                                                 
   
                                                                            
                                                                             
                                                                         
              
   
                                                                            
                                      
   
static Node *
pull_up_simple_subquery(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel)
{
  Query *parse = root->parse;
  int varno = ((RangeTblRef *)jtnode)->rtindex;
  Query *subquery;
  PlannerInfo *subroot;
  int rtoffset;
  pullup_replace_vars_context rvcontext;
  ListCell *lc;

     
                                                                           
                                                                     
                                                                        
                                                                          
     
  subquery = copyObject(rte->subquery);

     
                                                            
     
                                                                   
                                                                        
                                         
     
  subroot = makeNode(PlannerInfo);
  subroot->parse = subquery;
  subroot->glob = root->glob;
  subroot->query_level = root->query_level;
  subroot->parent_root = root->parent_root;
  subroot->plan_params = NIL;
  subroot->outer_params = NULL;
  subroot->planner_cxt = CurrentMemoryContext;
  subroot->init_plans = NIL;
  subroot->cte_plan_ids = NIL;
  subroot->multiexpr_params = NIL;
  subroot->eq_classes = NIL;
  subroot->append_rel_list = NIL;
  subroot->rowMarks = NIL;
  memset(subroot->upper_rels, 0, sizeof(subroot->upper_rels));
  memset(subroot->upper_targets, 0, sizeof(subroot->upper_targets));
  subroot->processed_tlist = NIL;
  subroot->grouping_map = NULL;
  subroot->minmax_aggs = NIL;
  subroot->qual_security_level = 0;
  subroot->inhTargetKind = INHKIND_NONE;
  subroot->hasRecursion = false;
  subroot->wt_param_id = -1;
  subroot->non_recursive_path = NULL;

                              
  Assert(subquery->cteList == NIL);

     
                                                                             
                                                                           
     
  replace_empty_jointree(subquery);

     
                                                                        
                                        
     
  if (subquery->hasSubLinks)
  {
    pull_up_sublinks(subroot);
  }

     
                                                                      
     
  inline_set_returning_functions(subroot);

     
                                                            
                                                                     
                 
     
                                                                        
                                                                            
                                                                     
                                                                 
     
  pull_up_subqueries(subroot);

     
                                                                             
                                         
     
                                                                           
                                                                  
                                 
     
  if (is_simple_subquery(root, subquery, rte, lowest_outer_join) && (containing_appendrel == NULL || is_safe_append_member(subquery)))
  {
                    
  }
  else
  {
       
                                               
       
                                                                        
                                                                       
                                                                          
               
       
    return jtnode;
  }

     
                                                                       
                                                                           
                                                               
                                                                          
                                                                 
                                                                            
                                                                             
     
  subquery->targetList = (List *)flatten_join_alias_vars(subroot->parse, (Node *)subquery->targetList);

     
                                                                            
                                                                         
           
     
  rtoffset = list_length(parse->rtable);
  OffsetVarNodes((Node *)subquery, rtoffset, 0);
  OffsetVarNodes((Node *)subroot->append_rel_list, rtoffset, 0);

     
                                                                           
                  
     
  IncrementVarSublevelsUp((Node *)subquery, -1, 1);
  IncrementVarSublevelsUp((Node *)subroot->append_rel_list, -1, 1);

     
                                                                        
                                                                        
                                                                             
     
  rvcontext.root = root;
  rvcontext.targetlist = subquery->targetList;
  rvcontext.target_rte = rte;
  if (rte->lateral)
  {
    rvcontext.relids = get_relids_in_jointree((Node *)subquery->jointree, true);
  }
  else                        
  {
    rvcontext.relids = NULL;
  }
  rvcontext.outer_hasSubLinks = &parse->hasSubLinks;
  rvcontext.varno = varno;
                                                
  rvcontext.need_phvs = false;
  rvcontext.wrap_non_vars = false;
                                                              
  rvcontext.rv_cache = palloc0((list_length(subquery->targetList) + 1) * sizeof(Node *));

     
                                                                       
                                                            
     
  if (lowest_nulling_outer_join != NULL)
  {
    rvcontext.need_phvs = true;
  }

     
                                                                           
                                                                          
                                                                       
                                                                   
                                                       
     
  if (containing_appendrel != NULL)
  {
    rvcontext.need_phvs = true;
    rvcontext.wrap_non_vars = true;
  }

     
                                                                          
                                                                             
                                                                         
                                                                           
                                                                            
                                                                          
                                                     
     
  if (parse->groupingSets)
  {
    rvcontext.need_phvs = true;
    rvcontext.wrap_non_vars = true;
  }

     
                                                                         
                                                                      
                                                                           
                                                                            
                                                                        
                                                                      
                                                  
     
  parse->targetList = (List *)pullup_replace_vars((Node *)parse->targetList, &rvcontext);
  parse->returningList = (List *)pullup_replace_vars((Node *)parse->returningList, &rvcontext);
  if (parse->onConflict)
  {
    parse->onConflict->onConflictSet = (List *)pullup_replace_vars((Node *)parse->onConflict->onConflictSet, &rvcontext);
    parse->onConflict->onConflictWhere = pullup_replace_vars(parse->onConflict->onConflictWhere, &rvcontext);

       
                                                                        
                                                  
       
  }
  replace_vars_in_jointree((Node *)parse->jointree, &rvcontext, lowest_nulling_outer_join);
  Assert(parse->setOperations == NULL);
  parse->havingQual = pullup_replace_vars(parse->havingQual, &rvcontext);

     
                                                                         
                                                                            
                                                                             
                                                                         
                                         
     
  foreach (lc, root->append_rel_list)
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)lfirst(lc);
    bool save_need_phvs = rvcontext.need_phvs;

    if (appinfo == containing_appendrel)
    {
      rvcontext.need_phvs = false;
    }
    appinfo->translated_vars = (List *)pullup_replace_vars((Node *)appinfo->translated_vars, &rvcontext);
    rvcontext.need_phvs = save_need_phvs;
  }

     
                                                                 
     
                                                                            
                                                                        
                                                                            
                                                                         
                                                         
     
  foreach (lc, parse->rtable)
  {
    RangeTblEntry *otherrte = (RangeTblEntry *)lfirst(lc);

    if (otherrte->rtekind == RTE_JOIN)
    {
      otherrte->joinaliasvars = (List *)pullup_replace_vars((Node *)otherrte->joinaliasvars, &rvcontext);
    }
  }

     
                                                                        
                                                                          
                                                                
                                                                            
                                               
     
  if (rte->lateral)
  {
    foreach (lc, subquery->rtable)
    {
      RangeTblEntry *child_rte = (RangeTblEntry *)lfirst(lc);

      switch (child_rte->rtekind)
      {
      case RTE_RELATION:
        if (child_rte->tablesample)
        {
          child_rte->lateral = true;
        }
        break;
      case RTE_SUBQUERY:
      case RTE_FUNCTION:
      case RTE_VALUES:
      case RTE_TABLEFUNC:
        child_rte->lateral = true;
        break;
      case RTE_JOIN:
      case RTE_CTE:
      case RTE_NAMEDTUPLESTORE:
      case RTE_RESULT:
                                                        
        break;
      }
    }
  }

     
                                                                         
                                                                           
                                     
     
  parse->rtable = list_concat(parse->rtable, subquery->rtable);

     
                                                                         
                                                               
     
  parse->rowMarks = list_concat(parse->rowMarks, subquery->rowMarks);

     
                                                                           
                                                                          
                                                                           
                                                                        
                                                                            
     
                                                                            
                                                                            
                                                 
     
  if (parse->hasSubLinks || root->glob->lastPHId != 0 || root->append_rel_list)
  {
    Relids subrelids;

    subrelids = get_relids_in_jointree((Node *)subquery->jointree, false);
    substitute_phv_relids((Node *)parse, varno, subrelids);
    fix_append_rel_relids(root->append_rel_list, varno, subrelids);
  }

     
                                                        
     
  root->append_rel_list = list_concat(root->append_rel_list, subroot->append_rel_list);

     
                                                                         
                                                                      
     
  Assert(root->join_info_list == NIL);
  Assert(subroot->join_info_list == NIL);
  Assert(root->placeholder_list == NIL);
  Assert(subroot->placeholder_list == NIL);

     
                                 
     
                                                                            
                                                                          
                                                                           
                                                                             
                                                                          
     
  parse->hasSubLinks |= subquery->hasSubLinks;

                                                                   
  parse->hasRowSecurity |= subquery->hasRowSecurity;

     
                                                                   
                                                     
     

     
                                                                            
                                                                          
                        
     
  Assert(IsA(subquery->jointree, FromExpr));
  Assert(subquery->jointree->fromlist != NIL);
  if (subquery->jointree->quals == NULL && list_length(subquery->jointree->fromlist) == 1)
  {
    return (Node *)linitial(subquery->jointree->fromlist);
  }

  return (Node *)subquery->jointree;
}

   
                            
                                                
   
                                                                          
                                                                       
                                                                           
                                                                      
   
static Node *
pull_up_simple_union_all(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte)
{
  int varno = ((RangeTblRef *)jtnode)->rtindex;
  Query *subquery = rte->subquery;
  int rtoffset = list_length(root->parse->rtable);
  List *rtable;

     
                                                                       
                                                                          
                                                             
     
  rtable = copyObject(subquery->rtable);

     
                                                                           
                                                                           
                                                                      
     
  IncrementVarSublevelsUp_rtable(rtable, -1, 1);

     
                                                                           
                                                                           
                                                                        
                                                                  
     
  if (rte->lateral)
  {
    ListCell *rt;

    foreach (rt, rtable)
    {
      RangeTblEntry *child_rte = (RangeTblEntry *)lfirst(rt);

      Assert(child_rte->rtekind == RTE_SUBQUERY);
      child_rte->lateral = true;
    }
  }

     
                                         
     
  root->parse->rtable = list_concat(root->parse->rtable, rtable);

     
                                                                
                                                             
                                                                             
     
  Assert(subquery->setOperations);
  pull_up_union_leaf_queries(subquery->setOperations, root, varno, subquery, rtoffset);

     
                                            
     
  rte->inh = true;

  return jtnode;
}

   
                                                                            
   
                                                                          
                                               
   
                                                                            
                                                                          
                                                                             
                                            
   
                                                                         
   
                                                                         
                                                                             
                                                                          
                                                                             
   
static void
pull_up_union_leaf_queries(Node *setOp, PlannerInfo *root, int parentRTindex, Query *setOpQuery, int childRToffset)
{
  if (IsA(setOp, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)setOp;
    int childRTindex;
    AppendRelInfo *appinfo;

       
                                                       
       
    childRTindex = childRToffset + rtr->rtindex;

       
                                                                    
       
    appinfo = makeNode(AppendRelInfo);
    appinfo->parent_relid = parentRTindex;
    appinfo->child_relid = childRTindex;
    appinfo->parent_reltype = InvalidOid;
    appinfo->child_reltype = InvalidOid;
    make_setop_translation_list(setOpQuery, childRTindex, &appinfo->translated_vars);
    appinfo->parent_reloid = InvalidOid;
    root->append_rel_list = lappend(root->append_rel_list, appinfo);

       
                                                                       
                                                                         
                                                                         
                                                                     
                                                                      
                                                                         
                                                                           
                                                                      
       
    rtr = makeNode(RangeTblRef);
    rtr->rtindex = childRTindex;
    (void)pull_up_subqueries_recurse(root, (Node *)rtr, NULL, NULL, appinfo);
  }
  else if (IsA(setOp, SetOperationStmt))
  {
    SetOperationStmt *op = (SetOperationStmt *)setOp;

                                       
    pull_up_union_leaf_queries(op->larg, root, parentRTindex, setOpQuery, childRToffset);
    pull_up_union_leaf_queries(op->rarg, root, parentRTindex, setOpQuery, childRToffset);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(setOp));
  }
}

   
                               
                                                                       
                                                                    
                                                                  
                                                                     
   
static void
make_setop_translation_list(Query *query, Index newvarno, List **translated_vars)
{
  List *vars = NIL;
  ListCell *l;

  foreach (l, query->targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resjunk)
    {
      continue;
    }

    vars = lappend(vars, makeVarFromTargetEntry(newvarno, tle));
  }

  *translated_vars = vars;
}

   
                      
                                                                      
                                       
   
                                                                      
                                                                           
                            
                                                                           
   
static bool
is_simple_subquery(PlannerInfo *root, Query *subquery, RangeTblEntry *rte, JoinExpr *lowest_outer_join)
{
     
                                                     
     
  if (!IsA(subquery, Query) || subquery->commandType != CMD_SELECT)
  {
    elog(ERROR, "subquery is bogus");
  }

     
                                                                           
                                                                            
                 
     
  if (subquery->setOperations)
  {
    return false;
  }

     
                                                                     
                                                                             
     
                                                                         
                                                                           
                                                                          
                                                                      
             
     
  if (subquery->hasAggs || subquery->hasWindowFuncs || subquery->hasTargetSRFs || subquery->groupClause || subquery->groupingSets || subquery->havingQual || subquery->sortClause || subquery->distinctClause || subquery->limitOffset || subquery->limitCount || subquery->hasForUpdate || subquery->cteList)
  {
    return false;
  }

     
                                                                     
                                                                            
                               
     
  if (rte->security_barrier)
  {
    return false;
  }

     
                                                                          
     
  if (rte->lateral)
  {
    bool restricted;
    Relids safe_upper_varnos;

       
                                                                          
                                                                          
                                                                       
                                                                     
                                                                     
                                                                        
                                                    
       
    if (lowest_outer_join != NULL)
    {
      restricted = true;
      safe_upper_varnos = get_relids_in_jointree((Node *)lowest_outer_join, true);
    }
    else
    {
      restricted = false;
      safe_upper_varnos = NULL;                     
    }

    if (jointree_contains_lateral_outer_refs(root, (Node *)subquery->jointree, restricted, safe_upper_varnos))
    {
      return false;
    }

       
                                                                          
                                                                      
                                                                       
                                                                         
                                                                          
                                                                           
                                                                      
                                                                         
                                                                           
                                                                     
                                           
       
    if (lowest_outer_join != NULL)
    {
      Relids lvarnos = pull_varnos_of_level(root, (Node *)subquery->targetList, 1);

      if (!bms_is_subset(lvarnos, safe_upper_varnos))
      {
        return false;
      }
    }
  }

     
                                                                     
                                                                             
                                                                          
                                                                         
                                                                             
                                                      
     
  if (contain_volatile_functions((Node *)subquery->targetList))
  {
    return false;
  }

  return true;
}

   
                         
                                        
   
                                                                           
                                                                         
                                                                            
                                                                          
                                                                    
   
                                                                          
                                                                          
                                  
   
static Node *
pull_up_simple_values(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte)
{
  Query *parse = root->parse;
  int varno = ((RangeTblRef *)jtnode)->rtindex;
  List *values_list;
  List *tlist;
  AttrNumber attrno;
  pullup_replace_vars_context rvcontext;
  ListCell *lc;

  Assert(rte->rtekind == RTE_VALUES);
  Assert(list_length(rte->values_lists) == 1);

     
                                                                             
                          
     
  values_list = copyObject(linitial(rte->values_lists));

     
                                                                             
                                                              
     
  Assert(!contain_vars_of_level((Node *)values_list, 0));

     
                                                                           
                                                                      
     
  tlist = NIL;
  attrno = 1;
  foreach (lc, values_list)
  {
    tlist = lappend(tlist, makeTargetEntry((Expr *)lfirst(lc), attrno, NULL, false));
    attrno++;
  }
  rvcontext.root = root;
  rvcontext.targetlist = tlist;
  rvcontext.target_rte = rte;
  rvcontext.relids = NULL;
  rvcontext.outer_hasSubLinks = &parse->hasSubLinks;
  rvcontext.varno = varno;
  rvcontext.need_phvs = false;
  rvcontext.wrap_non_vars = false;
                                                              
  rvcontext.rv_cache = palloc0((list_length(tlist) + 1) * sizeof(Node *));

     
                                                                         
                                                                             
                                                                             
                                                                            
                                                                       
               
     
  parse->targetList = (List *)pullup_replace_vars((Node *)parse->targetList, &rvcontext);
  parse->returningList = (List *)pullup_replace_vars((Node *)parse->returningList, &rvcontext);
  if (parse->onConflict)
  {
    parse->onConflict->onConflictSet = (List *)pullup_replace_vars((Node *)parse->onConflict->onConflictSet, &rvcontext);
    parse->onConflict->onConflictWhere = pullup_replace_vars(parse->onConflict->onConflictWhere, &rvcontext);

       
                                                                        
                                                  
       
  }
  replace_vars_in_jointree((Node *)parse->jointree, &rvcontext, NULL);
  Assert(parse->setOperations == NULL);
  parse->havingQual = pullup_replace_vars(parse->havingQual, &rvcontext);

     
                                                                            
                                               
     
  Assert(root->append_rel_list == NIL);
  Assert(list_length(parse->rtable) == 1);
  Assert(root->join_info_list == NIL);
  Assert(root->placeholder_list == NIL);

     
                                                                           
                                                               
     
  Assert(list_length(parse->rtable) == 1);

                           
  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RESULT;
  rte->eref = makeAlias("*RESULT*", NIL);

                          
  parse->rtable = list_make1(rte);

                                                                           
  Assert(varno == 1);

  return jtnode;
}

   
                    
                                                                        
                                       
   
                                                 
   
static bool
is_simple_values(PlannerInfo *root, RangeTblEntry *rte)
{
  Assert(rte->rtekind == RTE_VALUES);

     
                                                                       
                                                                            
                                                                      
     
  if (list_length(rte->values_lists) != 1)
  {
    return false;
  }

     
                                                                            
                                                                         
                                                     
     

     
                                                                        
                                                                        
                                                        
     
  if (expression_returns_set((Node *)rte->values_lists) || contain_volatile_functions((Node *)rte->values_lists))
  {
    return false;
  }

     
                                                                          
                                                                         
                                                          
                              
     
  if (list_length(root->parse->rtable) != 1 || rte != (RangeTblEntry *)linitial(root->parse->rtable))
  {
    return false;
  }

  return true;
}

   
                       
                                                         
   
                                                                            
                                                                           
                   
   
static bool
is_simple_union_all(Query *subquery)
{
  SetOperationStmt *topop;

                                                       
  if (!IsA(subquery, Query) || subquery->commandType != CMD_SELECT)
  {
    elog(ERROR, "subquery is bogus");
  }

                                           
  topop = castNode(SetOperationStmt, subquery->setOperations);
  if (!topop)
  {
    return false;
  }

                                                             
  if (subquery->sortClause || subquery->limitOffset || subquery->limitCount || subquery->rowMarks || subquery->cteList)
  {
    return false;
  }

                                                    
  return is_simple_union_all_recurse((Node *)topop, subquery, topop->colTypes);
}

static bool
is_simple_union_all_recurse(Node *setOp, Query *setOpQuery, List *colTypes)
{
                                                                           
  check_stack_depth();

  if (IsA(setOp, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)setOp;
    RangeTblEntry *rte = rt_fetch(rtr->rtindex, setOpQuery->rtable);
    Query *subquery = rte->subquery;

    Assert(subquery != NULL);

                                                                   
                                                             
    return tlist_same_datatypes(subquery->targetList, colTypes, true);
  }
  else if (IsA(setOp, SetOperationStmt))
  {
    SetOperationStmt *op = (SetOperationStmt *)setOp;

                           
    if (op->op != SETOP_UNION || !op->all)
    {
      return false;
    }

                                 
    return is_simple_union_all_recurse(op->larg, setOpQuery, colTypes) && is_simple_union_all_recurse(op->rarg, setOpQuery, colTypes);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(setOp));
    return false;                          
  }
}

   
                         
                                                                             
                      
   
static bool
is_safe_append_member(Query *subquery)
{
  FromExpr *jtnode;

     
                                                                          
                                                                             
                                                                           
                                                                  
                                                                          
     
                                                                            
                                                                         
                                                                         
                                                                      
                              
     
  jtnode = subquery->jointree;
  Assert(IsA(jtnode, FromExpr));
                                       
  if (jtnode->fromlist == NIL && jtnode->quals == NULL)
  {
    return true;
  }
                                   
  while (IsA(jtnode, FromExpr))
  {
    if (jtnode->quals != NULL)
    {
      return false;
    }
    if (list_length(jtnode->fromlist) != 1)
    {
      return false;
    }
    jtnode = linitial(jtnode->fromlist);
  }
  if (!IsA(jtnode, RangeTblRef))
  {
    return false;
  }

  return true;
}

   
                                        
                                                                  
   
                                                                           
                                                                             
                                                                            
                                                                             
                         
   
static bool
jointree_contains_lateral_outer_refs(PlannerInfo *root, Node *jtnode, bool restricted, Relids safe_upper_varnos)
{
  if (jtnode == NULL)
  {
    return false;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    return false;
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

                                             
    foreach (l, f->fromlist)
    {
      if (jointree_contains_lateral_outer_refs(root, lfirst(l), restricted, safe_upper_varnos))
      {
        return true;
      }
    }

                                        
    if (restricted && !bms_is_subset(pull_varnos_of_level(root, f->quals, 1), safe_upper_varnos))
    {
      return true;
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

       
                                                                    
                                  
       
    if (j->jointype != JOIN_INNER)
    {
      restricted = true;
      safe_upper_varnos = NULL;
    }

                               
    if (jointree_contains_lateral_outer_refs(root, j->larg, restricted, safe_upper_varnos))
    {
      return true;
    }
    if (jointree_contains_lateral_outer_refs(root, j->rarg, restricted, safe_upper_varnos))
    {
      return true;
    }

                                       
    if (restricted && !bms_is_subset(pull_varnos_of_level(root, j->quals, 1), safe_upper_varnos))
    {
      return true;
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return false;
}

   
                                                                          
                                                                               
                                     
   
                                                                           
                                                               
   
static void
replace_vars_in_jointree(Node *jtnode, pullup_replace_vars_context *context, JoinExpr *lowest_nulling_outer_join)
{
  if (jtnode == NULL)
  {
    return;
  }
  if (IsA(jtnode, RangeTblRef))
  {
       
                                                                       
                                                                           
                                                                       
                                                                        
                                                                          
                                                                         
                                                             
       
    int varno = ((RangeTblRef *)jtnode)->rtindex;

    if (varno != context->varno)                                    
    {
      RangeTblEntry *rte = rt_fetch(varno, context->root->parse->rtable);

      Assert(rte != context->target_rte);
      if (rte->lateral)
      {
        switch (rte->rtekind)
        {
        case RTE_RELATION:
                                                              
          Assert(rte->tablesample);
          rte->tablesample = (TableSampleClause *)pullup_replace_vars((Node *)rte->tablesample, context);
          break;
        case RTE_SUBQUERY:
          rte->subquery = pullup_replace_vars_subquery(rte->subquery, context);
          break;
        case RTE_FUNCTION:
          rte->functions = (List *)pullup_replace_vars((Node *)rte->functions, context);
          break;
        case RTE_TABLEFUNC:
          rte->tablefunc = (TableFunc *)pullup_replace_vars((Node *)rte->tablefunc, context);
          break;
        case RTE_VALUES:
          rte->values_lists = (List *)pullup_replace_vars((Node *)rte->values_lists, context);
          break;
        case RTE_JOIN:
        case RTE_CTE:
        case RTE_NAMEDTUPLESTORE:
        case RTE_RESULT:
                                                 
          Assert(false);
          break;
        }
      }
    }
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      replace_vars_in_jointree(lfirst(l), context, lowest_nulling_outer_join);
    }
    f->quals = pullup_replace_vars(f->quals, context);
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    bool save_need_phvs = context->need_phvs;

    if (j == lowest_nulling_outer_join)
    {
                                              
      context->need_phvs = false;
      lowest_nulling_outer_join = NULL;
    }
    replace_vars_in_jointree(j->larg, context, lowest_nulling_outer_join);
    replace_vars_in_jointree(j->rarg, context, lowest_nulling_outer_join);

       
                                                                         
                                                                       
                                                                         
                                                                           
                                                                           
                                                                       
       
    if (j->jointype == JOIN_FULL)
    {
      context->need_phvs = true;
    }

    j->quals = pullup_replace_vars(j->quals, context);

       
                                                                          
                 
       
    context->need_phvs = save_need_phvs;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                                                                   
   
                                                                       
                                    
   
static Node *
pullup_replace_vars(Node *expr, pullup_replace_vars_context *context)
{
  return replace_rte_variables(expr, context->varno, 0, pullup_replace_vars_callback, (void *)context, context->outer_hasSubLinks);
}

static Node *
pullup_replace_vars_callback(Var *var, replace_rte_variables_context *context)
{
  pullup_replace_vars_context *rcon = (pullup_replace_vars_context *)context->callback_arg;
  int varattno = var->varattno;
  Node *newnode;

     
                                                                         
                                                                        
                                                                       
                                                                            
                                                                           
                                                                             
                                        
     
  if (rcon->need_phvs && varattno >= InvalidAttrNumber && varattno <= list_length(rcon->targetlist) && rcon->rv_cache[varattno] != NULL)
  {
                                                                        
    newnode = copyObject(rcon->rv_cache[varattno]);
  }
  else if (varattno == InvalidAttrNumber)
  {
                                                        
    RowExpr *rowexpr;
    List *colnames;
    List *fields;
    bool save_need_phvs = rcon->need_phvs;
    int save_sublevelsup = context->sublevels_up;

       
                                                                         
                                                                      
                                                                         
                                                                    
                                       
       
                                                                        
                                                                  
       
    expandRTE(rcon->target_rte, var->varno, 0                      , var->location, (var->vartype != RECORDOID), &colnames, &fields);
                                                                    
    rcon->need_phvs = false;
    context->sublevels_up = 0;                                    
    fields = (List *)replace_rte_variables_mutator((Node *)fields, context);
    rcon->need_phvs = save_need_phvs;
    context->sublevels_up = save_sublevelsup;

    rowexpr = makeNode(RowExpr);
    rowexpr->args = fields;
    rowexpr->row_typeid = var->vartype;
    rowexpr->row_format = COERCE_IMPLICIT_CAST;
    rowexpr->colnames = colnames;
    rowexpr->location = var->location;
    newnode = (Node *)rowexpr;

       
                                                                         
                                                                        
                                                                    
                                                                          
                                 
       
    if (rcon->need_phvs)
    {
                                                               
      newnode = (Node *)make_placeholder_expr(rcon->root, (Expr *)newnode, bms_make_singleton(rcon->varno));
                                                                  
      rcon->rv_cache[InvalidAttrNumber] = copyObject(newnode);
    }
  }
  else
  {
                                                        
    TargetEntry *tle = get_tle_by_resno(rcon->targetlist, varattno);

    if (tle == NULL)                       
    {
      elog(ERROR, "could not find attribute %d in subquery targetlist", varattno);
    }

                                                 
    newnode = (Node *)copyObject(tle->expr);

                                         
    if (rcon->need_phvs)
    {
      bool wrap;

      if (newnode && IsA(newnode, Var) && ((Var *)newnode)->varlevelsup == 0)
      {
           
                                                                    
                                                                      
                                                                       
                                                                       
                                                             
           
        if (rcon->target_rte->lateral && !bms_is_member(((Var *)newnode)->varno, rcon->relids))
        {
          wrap = true;
        }
        else
        {
          wrap = false;
        }
      }
      else if (newnode && IsA(newnode, PlaceHolderVar) && ((PlaceHolderVar *)newnode)->phlevelsup == 0)
      {
                                                                       
        wrap = false;
      }
      else if (rcon->wrap_non_vars)
      {
                                                   
        wrap = true;
      }
      else
      {
           
                                                                     
                                                                 
                                                           
                           
           
                                                                       
                                                                       
                                                                   
                                            
           
                                                                   
                                                               
           
                                                                   
                                                                    
                                                       
           
        if ((rcon->target_rte->lateral ? bms_overlap(pull_varnos(rcon->root, (Node *)newnode), rcon->relids) : contain_vars_of_level((Node *)newnode, 0)) && !contain_nonstrict_functions((Node *)newnode))
        {
                              
          wrap = false;
        }
        else
        {
                                                
          wrap = true;
        }
      }

      if (wrap)
      {
        newnode = (Node *)make_placeholder_expr(rcon->root, (Expr *)newnode, bms_make_singleton(rcon->varno));
      }

         
                                                                      
                                                                        
                                                                 
                                                      
         
      if (varattno > InvalidAttrNumber && varattno <= list_length(rcon->targetlist))
      {
        rcon->rv_cache[varattno] = copyObject(newnode);
      }
    }
  }

                                                                  
  if (var->varlevelsup > 0)
  {
    IncrementVarSublevelsUp(newnode, var->varlevelsup, 0);
  }

  return newnode;
}

   
                                                   
   
                                                                 
                                                                             
                                                                            
   
static Query *
pullup_replace_vars_subquery(Query *query, pullup_replace_vars_context *context)
{
  Assert(IsA(query, Query));
  return (Query *)replace_rte_variables((Node *)query, context->varno, 1, pullup_replace_vars_callback, (void *)context, NULL);
}

   
                            
                                                                    
   
                                                                         
                                                                             
                                                                       
   
                                                                             
                                                                               
                                                                             
                                                                              
                                             
   
void
flatten_simple_union_all(PlannerInfo *root)
{
  Query *parse = root->parse;
  SetOperationStmt *topop;
  Node *leftmostjtnode;
  int leftmostRTI;
  RangeTblEntry *leftmostRTE;
  int childRTI;
  RangeTblEntry *childRTE;
  RangeTblRef *rtr;

                                                   
  topop = castNode(SetOperationStmt, parse->setOperations);
  Assert(topop);

                                             
  if (root->hasRecursion)
  {
    return;
  }

     
                                                                         
                                        
     
  if (!is_simple_union_all_recurse((Node *)topop, parse, topop->colTypes))
  {
    return;
  }

     
                                                                           
                                                                 
     
  leftmostjtnode = topop->larg;
  while (leftmostjtnode && IsA(leftmostjtnode, SetOperationStmt))
  {
    leftmostjtnode = ((SetOperationStmt *)leftmostjtnode)->larg;
  }
  Assert(leftmostjtnode && IsA(leftmostjtnode, RangeTblRef));
  leftmostRTI = ((RangeTblRef *)leftmostjtnode)->rtindex;
  leftmostRTE = rt_fetch(leftmostRTI, parse->rtable);
  Assert(leftmostRTE->rtekind == RTE_SUBQUERY);

     
                                                                          
                                                                           
                                                                           
                                                                           
                                                
     
  childRTE = copyObject(leftmostRTE);
  parse->rtable = lappend(parse->rtable, childRTE);
  childRTI = list_length(parse->rtable);

                                                          
  ((RangeTblRef *)leftmostjtnode)->rtindex = childRTI;

                                                                          
  leftmostRTE->inh = true;

     
                                                                             
                                                                           
     
  rtr = makeNode(RangeTblRef);
  rtr->rtindex = leftmostRTI;
  Assert(parse->jointree->fromlist == NIL);
  parse->jointree->fromlist = list_make1(rtr);

     
                                                                            
                                                                       
     
  parse->setOperations = NULL;

     
                                                                          
                                                                       
                                                                          
                                                 
     
  pull_up_union_leaf_queries((Node *)topop, root, leftmostRTI, parse, 0);
}

   
                      
                                                        
   
                                            
                                                           
                                                                            
                                                                              
                                                                         
                                                                              
                                                                              
                                                                             
                                                                       
   
                                                                          
                                                                        
                                                                           
                             
   
                                                                   
                                                                    
                                                                            
                                                                           
                                                                           
                                                                         
                                                                         
                                                       
   
                                                                          
                                                                             
                                                                             
              
   
                                                                             
                                                                            
                         
   
void
reduce_outer_joins(PlannerInfo *root)
{
  reduce_outer_joins_state *state;

     
                                                                            
                                                                         
                                                                             
                                                                           
                                                                      
                                                                           
                                                                  
     
  state = reduce_outer_joins_pass1((Node *)root->parse->jointree);

                                                            
  if (state == NULL || !state->contains_outer)
  {
    elog(ERROR, "so where are the outer joins?");
  }

  reduce_outer_joins_pass2((Node *)root->parse->jointree, state, root, NULL, NIL, NIL);
}

   
                                                      
   
                                                            
   
static reduce_outer_joins_state *
reduce_outer_joins_pass1(Node *jtnode)
{
  reduce_outer_joins_state *result;

  result = (reduce_outer_joins_state *)palloc(sizeof(reduce_outer_joins_state));
  result->relids = NULL;
  result->contains_outer = false;
  result->sub_states = NIL;

  if (jtnode == NULL)
  {
    return result;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;

    result->relids = bms_make_singleton(varno);
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      reduce_outer_joins_state *sub_state;

      sub_state = reduce_outer_joins_pass1(lfirst(l));
      result->relids = bms_add_members(result->relids, sub_state->relids);
      result->contains_outer |= sub_state->contains_outer;
      result->sub_states = lappend(result->sub_states, sub_state);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    reduce_outer_joins_state *sub_state;

                                                             
    if (IS_OUTER_JOIN(j->jointype))
    {
      result->contains_outer = true;
    }

    sub_state = reduce_outer_joins_pass1(j->larg);
    result->relids = bms_add_members(result->relids, sub_state->relids);
    result->contains_outer |= sub_state->contains_outer;
    result->sub_states = lappend(result->sub_states, sub_state);

    sub_state = reduce_outer_joins_pass1(j->rarg);
    result->relids = bms_add_members(result->relids, sub_state->relids);
    result->contains_outer |= sub_state->contains_outer;
    result->sub_states = lappend(result->sub_states, sub_state);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return result;
}

   
                                                 
   
                                 
                                                        
                                
                                                                       
                                                                 
                                                             
   
static void
reduce_outer_joins_pass2(Node *jtnode, reduce_outer_joins_state *state, PlannerInfo *root, Relids nonnullable_rels, List *nonnullable_vars, List *forced_null_vars)
{
     
                                                                         
                                                                    
     
  if (jtnode == NULL)
  {
    elog(ERROR, "reached empty jointree");
  }
  if (IsA(jtnode, RangeTblRef))
  {
    elog(ERROR, "reached base rel");
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;
    ListCell *s;
    Relids pass_nonnullable_rels;
    List *pass_nonnullable_vars;
    List *pass_forced_null_vars;

                                                         
    pass_nonnullable_rels = find_nonnullable_rels(f->quals);
    pass_nonnullable_rels = bms_add_members(pass_nonnullable_rels, nonnullable_rels);
                                                                      
    pass_nonnullable_vars = find_nonnullable_vars(f->quals);
    pass_nonnullable_vars = list_concat(pass_nonnullable_vars, nonnullable_vars);
    pass_forced_null_vars = find_forced_null_vars(f->quals);
    pass_forced_null_vars = list_concat(pass_forced_null_vars, forced_null_vars);
                                                            
    Assert(list_length(f->fromlist) == list_length(state->sub_states));
    forboth(l, f->fromlist, s, state->sub_states)
    {
      reduce_outer_joins_state *sub_state = lfirst(s);

      if (sub_state->contains_outer)
      {
        reduce_outer_joins_pass2(lfirst(l), sub_state, root, pass_nonnullable_rels, pass_nonnullable_vars, pass_forced_null_vars);
      }
    }
    bms_free(pass_nonnullable_rels);
                                                           
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    int rtindex = j->rtindex;
    JoinType jointype = j->jointype;
    reduce_outer_joins_state *left_state = linitial(state->sub_states);
    reduce_outer_joins_state *right_state = lsecond(state->sub_states);
    List *local_nonnullable_vars = NIL;
    bool computed_local_nonnullable_vars = false;

                                    
    switch (jointype)
    {
    case JOIN_INNER:
      break;
    case JOIN_LEFT:
      if (bms_overlap(nonnullable_rels, right_state->relids))
      {
        jointype = JOIN_INNER;
      }
      break;
    case JOIN_RIGHT:
      if (bms_overlap(nonnullable_rels, left_state->relids))
      {
        jointype = JOIN_INNER;
      }
      break;
    case JOIN_FULL:
      if (bms_overlap(nonnullable_rels, left_state->relids))
      {
        if (bms_overlap(nonnullable_rels, right_state->relids))
        {
          jointype = JOIN_INNER;
        }
        else
        {
          jointype = JOIN_LEFT;
        }
      }
      else
      {
        if (bms_overlap(nonnullable_rels, right_state->relids))
        {
          jointype = JOIN_RIGHT;
        }
      }
      break;
    case JOIN_SEMI:
    case JOIN_ANTI:

         
                                                                    
                                                                 
                                                    
         
      break;
    default:
      elog(ERROR, "unrecognized join type: %d", (int)jointype);
      break;
    }

       
                                                                        
                                                                       
                                                                           
                                                                     
                                      
       
    if (jointype == JOIN_RIGHT)
    {
      Node *tmparg;

      tmparg = j->larg;
      j->larg = j->rarg;
      j->rarg = tmparg;
      jointype = JOIN_LEFT;
      right_state = linitial(state->sub_states);
      left_state = lsecond(state->sub_states);
    }

       
                                                                         
                                                                           
                                                                     
                                                                           
                                                                          
                                                                       
                                                                        
                         
       
    if (jointype == JOIN_LEFT)
    {
      List *overlap;

      local_nonnullable_vars = find_nonnullable_vars(j->quals);
      computed_local_nonnullable_vars = true;

         
                                                                         
                                                                  
                                     
         
      overlap = list_intersection(local_nonnullable_vars, forced_null_vars);
      if (overlap != NIL && bms_overlap(pull_varnos(root, (Node *)overlap), right_state->relids))
      {
        jointype = JOIN_ANTI;
      }
    }

                                                                          
    if (rtindex && jointype != j->jointype)
    {
      RangeTblEntry *rte = rt_fetch(rtindex, root->parse->rtable);

      Assert(rte->rtekind == RTE_JOIN);
      Assert(rte->jointype == j->jointype);
      rte->jointype = jointype;
    }
    j->jointype = jointype;

                                                       
    if (left_state->contains_outer || right_state->contains_outer)
    {
      Relids local_nonnullable_rels;
      List *local_forced_null_vars;
      Relids pass_nonnullable_rels;
      List *pass_nonnullable_vars;
      List *pass_forced_null_vars;

         
                                                                     
                                                                        
                                                                       
                                                                        
                                                                      
                                                                     
                                                                        
                                                                   
                                                                         
                                                                    
                                                       
         
                                                                        
                                                                         
                                                                      
                                                                   
         
                                                                     
                                 
         
      if (jointype != JOIN_FULL)
      {
        local_nonnullable_rels = find_nonnullable_rels(j->quals);
        if (!computed_local_nonnullable_vars)
        {
          local_nonnullable_vars = find_nonnullable_vars(j->quals);
        }
        local_forced_null_vars = find_forced_null_vars(j->quals);
        if (jointype == JOIN_INNER || jointype == JOIN_SEMI)
        {
                                                       
          local_nonnullable_rels = bms_add_members(local_nonnullable_rels, nonnullable_rels);
          local_nonnullable_vars = list_concat(local_nonnullable_vars, nonnullable_vars);
          local_forced_null_vars = list_concat(local_forced_null_vars, forced_null_vars);
        }
      }
      else
      {
                                         
        local_nonnullable_rels = NULL;
        local_forced_null_vars = NIL;
      }

      if (left_state->contains_outer)
      {
        if (jointype == JOIN_INNER || jointype == JOIN_SEMI)
        {
                                                         
          pass_nonnullable_rels = local_nonnullable_rels;
          pass_nonnullable_vars = local_nonnullable_vars;
          pass_forced_null_vars = local_forced_null_vars;
        }
        else if (jointype != JOIN_FULL)                       
        {
                                                                 
          pass_nonnullable_rels = nonnullable_rels;
          pass_nonnullable_vars = nonnullable_vars;
          pass_forced_null_vars = forced_null_vars;
        }
        else
        {
                                                     
          pass_nonnullable_rels = NULL;
          pass_nonnullable_vars = NIL;
          pass_forced_null_vars = NIL;
        }
        reduce_outer_joins_pass2(j->larg, left_state, root, pass_nonnullable_rels, pass_nonnullable_vars, pass_forced_null_vars);
      }

      if (right_state->contains_outer)
      {
        if (jointype != JOIN_FULL)                               
        {
                                                               
          pass_nonnullable_rels = local_nonnullable_rels;
          pass_nonnullable_vars = local_nonnullable_vars;
          pass_forced_null_vars = local_forced_null_vars;
        }
        else
        {
                                                     
          pass_nonnullable_rels = NULL;
          pass_nonnullable_vars = NIL;
          pass_forced_null_vars = NIL;
        }
        reduce_outer_joins_pass2(j->rarg, right_state, root, pass_nonnullable_rels, pass_nonnullable_vars, pass_forced_null_vars);
      }
      bms_free(local_nonnullable_rels);
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                              
                                                          
   
                                                                           
                                                                              
                                                                             
                                                                   
   
                                                                           
                                                                          
                                                                          
                                                                           
                                                                               
                                                                        
                                                                           
                                                                         
                                                                           
   
                                                                            
                                                                             
                                                                          
                                                                             
                                                                        
                                                                              
                                                                            
                                                                             
                                                                             
                                                                             
                                                                            
                                                                            
                            
   
                                                                            
                                                                           
                                        
   
void
remove_useless_result_rtes(PlannerInfo *root)
{
  ListCell *cell;
  ListCell *prev;
  ListCell *next;

                                                       
  Assert(IsA(root->parse->jointree, FromExpr));
                   
  root->parse->jointree = (FromExpr *)remove_useless_results_recurse(root, (Node *)root->parse->jointree);
                                       
  Assert(IsA(root->parse->jointree, FromExpr));

     
                                                                         
                                                                          
                                                                         
                                                                        
                       
     
                                                                             
                                                                      
                                                        
     
  prev = NULL;
  for (cell = list_head(root->rowMarks); cell; cell = next)
  {
    PlanRowMark *rc = (PlanRowMark *)lfirst(cell);

    next = lnext(cell);
    if (rt_fetch(rc->rti, root->parse->rtable)->rtekind == RTE_RESULT)
    {
      root->rowMarks = list_delete_cell(root->rowMarks, cell, prev);
    }
    else
    {
      prev = cell;
    }
  }
}

   
                                  
                                                  
   
                                                                            
   
static Node *
remove_useless_results_recurse(PlannerInfo *root, Node *jtnode)
{
  Assert(jtnode != NULL);
  if (IsA(jtnode, RangeTblRef))
  {
                                                          
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    Relids result_relids = NULL;
    ListCell *cell;
    ListCell *prev;
    ListCell *next;

       
                                                                         
                                                                   
                                                                           
                                                                           
                                                                           
                                                                         
                                                         
       
    prev = NULL;
    for (cell = list_head(f->fromlist); cell; cell = next)
    {
      Node *child = (Node *)lfirst(cell);
      int varno;

                                           
      child = remove_useless_results_recurse(root, child);
                                               
      lfirst(cell) = child;
      next = lnext(cell);

         
                                                                         
                                                                       
                                                                    
                                                   
         
      if (list_length(f->fromlist) > 1 && (varno = get_result_relid(root, child)) != 0 && !find_dependent_phvs_in_jointree(root, (Node *)f, varno))
      {
        f->fromlist = list_delete_cell(f->fromlist, cell, prev);
        result_relids = bms_add_member(result_relids, varno);
      }
      else
      {
        prev = cell;
      }
    }

       
                                                                  
                                                                    
                                                            
       
    if (result_relids)
    {
      int varno = -1;

      while ((varno = bms_next_member(result_relids, varno)) >= 0)
      {
        remove_result_refs(root, varno, (Node *)f);
      }
    }

       
                                                                         
                                                                        
                                                                        
                  
       
    if (f != root->parse->jointree && f->quals == NULL && list_length(f->fromlist) == 1)
    {
      return (Node *)linitial(f->fromlist);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    int varno;

                        
    j->larg = remove_useless_results_recurse(root, j->larg);
    j->rarg = remove_useless_results_recurse(root, j->rarg);

                                                     
    switch (j->jointype)
    {
    case JOIN_INNER:

         
                                                                 
                                                                  
                                                                   
                                                               
                           
         
                                                                
                                                                   
                                                              
                                                                   
                                                                   
                                                            
                                                                 
                                    
         
      if ((varno = get_result_relid(root, j->larg)) != 0 && !find_dependent_phvs_in_jointree(root, j->rarg, varno))
      {
        remove_result_refs(root, varno, j->rarg);
        if (j->quals)
        {
          jtnode = (Node *)makeFromExpr(list_make1(j->rarg), j->quals);
        }
        else
        {
          jtnode = j->rarg;
        }
      }
      else if ((varno = get_result_relid(root, j->rarg)) != 0)
      {
        remove_result_refs(root, varno, j->larg);
        if (j->quals)
        {
          jtnode = (Node *)makeFromExpr(list_make1(j->larg), j->quals);
        }
        else
        {
          jtnode = j->larg;
        }
      }
      break;
    case JOIN_LEFT:

         
                                                                     
                                      
         
                                                                   
                                                                    
                                                                     
                                                                
                                                                  
                   
         
                                                                
                                                                     
                                                                     
                                                                   
                                                                     
               
         
      if ((varno = get_result_relid(root, j->rarg)) != 0 && (j->quals == NULL || !find_dependent_phvs(root, varno)))
      {
        remove_result_refs(root, varno, j->larg);
        jtnode = j->larg;
      }
      break;
    case JOIN_SEMI:

         
                                                                    
                                                                  
                                                                     
                                                                   
         
                                                                  
                                                                   
                                                             
                                                                  
                                                                   
                                                                    
                                                                
                                                                    
         
      if ((varno = get_result_relid(root, j->rarg)) != 0)
      {
        remove_result_refs(root, varno, j->larg);
        if (j->quals)
        {
          jtnode = (Node *)makeFromExpr(list_make1(j->larg), j->quals);
        }
        else
        {
          jtnode = j->larg;
        }
      }
      break;
    case JOIN_FULL:
    case JOIN_ANTI:
                                                     
      break;
    default:
                                                         
      elog(ERROR, "unrecognized join type: %d", (int)j->jointype);
      break;
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return jtnode;
}

   
                    
                                                                        
                        
   
static int
get_result_relid(PlannerInfo *root, Node *jtnode)
{
  int varno;

  if (!IsA(jtnode, RangeTblRef))
  {
    return 0;
  }
  varno = ((RangeTblRef *)jtnode)->rtindex;
  if (rt_fetch(varno, root->parse->rtable)->rtekind != RTE_RESULT)
  {
    return 0;
  }
  return varno;
}

   
                      
                                                            
   
                                                                            
                                                                           
                                                                             
                                                                          
                                              
   
                                                                      
                                                                   
   
                                    
                                                                       
                                           
   
static void
remove_result_refs(PlannerInfo *root, int varno, Node *newjtloc)
{
                                        
                                                           
  if (root->glob->lastPHId != 0)
  {
    Relids subrelids;

    subrelids = get_relids_in_jointree(newjtloc, false);
    Assert(!bms_is_empty(subrelids));
    substitute_phv_relids((Node *)root->parse, varno, subrelids);
    fix_append_rel_relids(root->append_rel_list, varno, subrelids);
  }

     
                                                                        
                                                                       
     
}

   
                                                                        
                            
   
                                                                       
                                                                       
                                                                        
                                                                 
                                                                    
   

typedef struct
{
  Relids relids;
  int sublevels_up;
} find_dependent_phvs_context;

static bool
find_dependent_phvs_walker(Node *node, find_dependent_phvs_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    if (phv->phlevelsup == context->sublevels_up && bms_equal(context->relids, phv->phrels))
    {
      return true;
    }
                                          
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, find_dependent_phvs_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
                                                             
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, AppendRelInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

  return expression_tree_walker(node, find_dependent_phvs_walker, (void *)context);
}

static bool
find_dependent_phvs(PlannerInfo *root, int varno)
{
  find_dependent_phvs_context context;

                                                           
  if (root->glob->lastPHId == 0)
  {
    return false;
  }

  context.relids = bms_make_singleton(varno);
  context.sublevels_up = 0;

  return query_tree_walker(root->parse, find_dependent_phvs_walker, (void *)&context, 0);
}

static bool
find_dependent_phvs_in_jointree(PlannerInfo *root, Node *node, int varno)
{
  find_dependent_phvs_context context;
  Relids subrelids;
  int relid;

                                                           
  if (root->glob->lastPHId == 0)
  {
    return false;
  }

  context.relids = bms_make_singleton(varno);
  context.sublevels_up = 0;

     
                                                                             
     
  if (find_dependent_phvs_walker(node, &context))
  {
    return true;
  }

     
                                                                          
                                                                          
                                                                            
                                                                          
                                         
     
  subrelids = get_relids_in_jointree(node, false);
  relid = -1;
  while ((relid = bms_next_member(subrelids, relid)) >= 0)
  {
    RangeTblEntry *rte = rt_fetch(relid, root->parse->rtable);

    if (rte->lateral && range_table_entry_walker(rte, find_dependent_phvs_walker, (void *)&context, 0))
    {
      return true;
    }
  }

  return false;
}

   
                                                                             
                                                      
   
                                                                      
                                                                           
   
                                                                         
                                                                   
                                                                            
                                                                               
   

typedef struct
{
  int varno;
  int sublevels_up;
  Relids subrelids;
} substitute_phv_relids_context;

static bool
substitute_phv_relids_walker(Node *node, substitute_phv_relids_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    if (phv->phlevelsup == context->sublevels_up && bms_is_member(context->varno, phv->phrels))
    {
      phv->phrels = bms_union(phv->phrels, context->subrelids);
      phv->phrels = bms_del_member(phv->phrels, context->varno);
                                            
      Assert(!bms_is_empty(phv->phrels));
    }
                                          
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, substitute_phv_relids_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
                                                             
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, AppendRelInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

  return expression_tree_walker(node, substitute_phv_relids_walker, (void *)context);
}

static void
substitute_phv_relids(Node *node, int varno, Relids subrelids)
{
  substitute_phv_relids_context context;

  context.varno = varno;
  context.sublevels_up = 0;
  context.subrelids = subrelids;

     
                                                                       
     
  query_or_expression_tree_walker(node, substitute_phv_relids_walker, (void *)&context, 0);
}

   
                                                                        
   
                                                                              
                                                                               
                                                                       
                                                                     
   
                                                             
   
static void
fix_append_rel_relids(List *append_rel_list, int varno, Relids subrelids)
{
  ListCell *l;
  int subvarno = -1;

     
                                                                        
                                                                             
                                                                             
                                                                 
     
  foreach (l, append_rel_list)
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)lfirst(l);

                                                            
    Assert(appinfo->parent_relid != varno);

    if (appinfo->child_relid == varno)
    {
      if (subvarno < 0)
      {
        subvarno = bms_singleton_member(subrelids);
      }
      appinfo->child_relid = subvarno;
    }

                                                     
    substitute_phv_relids((Node *)appinfo->translated_vars, varno, subrelids);
  }
}

   
                                                                       
   
                                                                     
                                
   
Relids
get_relids_in_jointree(Node *jtnode, bool include_joins)
{
  Relids result = NULL;

  if (jtnode == NULL)
  {
    return result;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;

    result = bms_make_singleton(varno);
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      result = bms_join(result, get_relids_in_jointree(lfirst(l), include_joins));
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

    result = get_relids_in_jointree(j->larg, include_joins);
    result = bms_join(result, get_relids_in_jointree(j->rarg, include_joins));
    if (include_joins && j->rtindex)
    {
      result = bms_add_member(result, j->rtindex);
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return result;
}

   
                                                                    
   
Relids
get_relids_for_join(Query *query, int joinrelid)
{
  Node *jtnode;

  jtnode = find_jointree_node_for_rel((Node *)query->jointree, joinrelid);
  if (!jtnode)
  {
    elog(ERROR, "could not find join node %d", joinrelid);
  }
  return get_relids_in_jointree(jtnode, false);
}

   
                                                                                
   
                             
   
static Node *
find_jointree_node_for_rel(Node *jtnode, int relid)
{
  if (jtnode == NULL)
  {
    return NULL;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;

    if (relid == varno)
    {
      return jtnode;
    }
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      jtnode = find_jointree_node_for_rel(lfirst(l), relid);
      if (jtnode)
      {
        return jtnode;
      }
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

    if (relid == j->rtindex)
    {
      return jtnode;
    }
    jtnode = find_jointree_node_for_rel(j->larg, relid);
    if (jtnode)
    {
      return jtnode;
    }
    jtnode = find_jointree_node_for_rel(j->rarg, relid);
    if (jtnode)
    {
      return jtnode;
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return NULL;
}
