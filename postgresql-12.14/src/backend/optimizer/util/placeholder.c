                                                                            
   
                 
                                                              
   
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   
#include "postgres.h"

#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"
#include "utils/lsyscache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)

                     
static void
find_placeholders_recurse(PlannerInfo *root, Node *jtnode);
static void
find_placeholders_in_expr(PlannerInfo *root, Node *expr);

   
                         
                                                    
   
                                                                        
                      
   
PlaceHolderVar *
make_placeholder_expr(PlannerInfo *root, Expr *expr, Relids phrels)
{
  PlaceHolderVar *phv = makeNode(PlaceHolderVar);

  phv->phexpr = expr;
  phv->phrels = phrels;
  phv->phid = ++(root->glob->lastPHId);
  phv->phlevelsup = 0;

  return phv;
}

   
                         
                                                
   
                                                                           
                              
   
                                                                           
                                                                            
                                                                               
                                                                         
                                               
   
                                                                              
                                                                             
                                                                           
   
PlaceHolderInfo *
find_placeholder_info(PlannerInfo *root, PlaceHolderVar *phv, bool create_new_ph)
{
  PlaceHolderInfo *phinfo;
  Relids rels_used;
  ListCell *lc;

                                                                             
  Assert(phv->phlevelsup == 0);

  foreach (lc, root->placeholder_list)
  {
    phinfo = (PlaceHolderInfo *)lfirst(lc);
    if (phinfo->phid == phv->phid)
    {
      return phinfo;
    }
  }

                               
  if (!create_new_ph)
  {
    elog(ERROR, "too late to create a new PlaceHolderInfo");
  }

  phinfo = makeNode(PlaceHolderInfo);

  phinfo->phid = phv->phid;
  phinfo->ph_var = copyObject(phv);

     
                                                                        
                                                                           
                                                                        
                                                 
     
  rels_used = pull_varnos(root, (Node *)phv->phexpr);
  phinfo->ph_lateral = bms_difference(rels_used, phv->phrels);
  if (bms_is_empty(phinfo->ph_lateral))
  {
    phinfo->ph_lateral = NULL;                                    
  }
  phinfo->ph_eval_at = bms_int_members(rels_used, phv->phrels);
                                                                    
  if (bms_is_empty(phinfo->ph_eval_at))
  {
    phinfo->ph_eval_at = bms_copy(phv->phrels);
    Assert(!bms_is_empty(phinfo->ph_eval_at));
  }
                                                                       
  phinfo->ph_needed = NULL;                            
                                                                   
  phinfo->ph_width = get_typavgwidth(exprType((Node *)phv->phexpr), exprTypmod((Node *)phv->phexpr));

  root->placeholder_list = lappend(root->placeholder_list, phinfo);

     
                                                                             
                                                                             
                                      
     
  find_placeholders_in_expr(root, (Node *)phinfo->ph_var->phexpr);

  return phinfo;
}

   
                                 
                                                                        
   
                                                                           
                                                             
   
                                                                       
                                                                    
                                                          
                                                                    
                                    
   
void
find_placeholders_in_jointree(PlannerInfo *root)
{
                                                                   
  if (root->glob->lastPHId != 0)
  {
                                            
    Assert(root->parse->jointree != NULL && IsA(root->parse->jointree, FromExpr));
    find_placeholders_recurse(root, (Node *)root->parse->jointree);
  }
}

   
                             
                                                           
   
                                                   
   
static void
find_placeholders_recurse(PlannerInfo *root, Node *jtnode)
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
      find_placeholders_recurse(root, lfirst(l));
    }

       
                                        
       
    find_placeholders_in_expr(root, f->quals);
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

       
                                             
       
    find_placeholders_recurse(root, j->larg);
    find_placeholders_recurse(root, j->rarg);

                                  
    find_placeholders_in_expr(root, j->quals);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                             
                                                                 
                                      
   
static void
find_placeholders_in_expr(PlannerInfo *root, Node *expr)
{
  List *vars;
  ListCell *vl;

     
                                                                        
                        
     
  vars = pull_var_clause(expr, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);
  foreach (vl, vars)
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)lfirst(vl);

                               
    if (!IsA(phv, PlaceHolderVar))
    {
      continue;
    }

                                                                   
    (void)find_placeholder_info(root, phv, true);
  }
  list_free(vars);
}

   
                                  
                                                         
   
                                                                         
                                                                           
                                                                        
                                                                          
                                                  
   
                                                                   
                                                                         
                                                                             
                                                                         
                                                                            
                
   
                                                                       
                                                                         
                                                                          
                                                                          
                                                                          
                                                                              
   
void
update_placeholder_eval_levels(PlannerInfo *root, SpecialJoinInfo *new_sjinfo)
{
  ListCell *lc1;

  foreach (lc1, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc1);
    Relids syn_level = phinfo->ph_var->phrels;
    Relids eval_at;
    bool found_some;
    ListCell *lc2;

       
                                                                   
                                                                     
       
    if (!bms_is_subset(new_sjinfo->syn_lefthand, syn_level) || !bms_is_subset(new_sjinfo->syn_righthand, syn_level))
    {
      continue;
    }

       
                                                                          
                                                                        
                                                                      
                                                                     
                              
       
    eval_at = phinfo->ph_eval_at;

    do
    {
      found_some = false;
      foreach (lc2, root->join_info_list)
      {
        SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(lc2);

                                                             
        if (!bms_is_subset(sjinfo->syn_lefthand, syn_level) || !bms_is_subset(sjinfo->syn_righthand, syn_level))
        {
          continue;
        }

                                                           
        if (bms_overlap(eval_at, sjinfo->min_righthand) || (sjinfo->jointype == JOIN_FULL && bms_overlap(eval_at, sjinfo->min_lefthand)))
        {
                                                              
          if (!bms_is_subset(sjinfo->min_lefthand, eval_at) || !bms_is_subset(sjinfo->min_righthand, eval_at))
          {
                                    
            eval_at = bms_add_members(eval_at, sjinfo->min_lefthand);
            eval_at = bms_add_members(eval_at, sjinfo->min_righthand);
                                              
            found_some = true;
          }
        }
      }
    } while (found_some);

                                                                         
    Assert(bms_is_subset(eval_at, syn_level));

    phinfo->ph_eval_at = eval_at;
  }
}

   
                                       
                                                         
   
                                                                          
                                                                          
                                                                             
                                                                            
                                                                          
                                                                          
                                                                         
                                                                            
                                                                             
                                                
   
void
fix_placeholder_input_needed_levels(PlannerInfo *root)
{
  ListCell *lc;

  foreach (lc, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc);
    List *vars = pull_var_clause((Node *)phinfo->ph_var->phexpr, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);

    add_vars_to_targetlist(root, vars, phinfo->ph_eval_at, false);
    list_free(vars);
  }
}

   
                                 
                                                                
   
                                                                            
                                                                             
                                                                             
                                                                               
                                                                              
                                           
   
void
add_placeholders_to_base_rels(PlannerInfo *root)
{
  ListCell *lc;

  foreach (lc, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc);
    Relids eval_at = phinfo->ph_eval_at;
    int varno;

    if (bms_get_singleton_member(eval_at, &varno) && bms_nonempty_difference(phinfo->ph_needed, eval_at))
    {
      RelOptInfo *rel = find_base_rel(root, varno);

      rel->reltarget->exprs = lappend(rel->reltarget->exprs, copyObject(phinfo->ph_var));
                                                                   
    }
  }
}

   
                               
                                                                 
                                                                        
                                     
   
                                                                          
                                                                           
                                                                         
                       
   
void
add_placeholders_to_joinrel(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
  Relids relids = joinrel->relids;
  ListCell *lc;

  foreach (lc, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc);

                                
    if (bms_is_subset(phinfo->ph_eval_at, relids))
    {
                                                  
      if (bms_nonempty_difference(phinfo->ph_needed, relids))
      {
                                       
        joinrel->reltarget->exprs = lappend(joinrel->reltarget->exprs, phinfo->ph_var);
        joinrel->reltarget->width += phinfo->ph_width;

           
                                                                     
                                                                       
                                                                    
                                                                     
                                                                       
                                                                      
                                                                       
                                                               
                                                                   
           
        if (!bms_is_subset(phinfo->ph_eval_at, outer_rel->relids) && !bms_is_subset(phinfo->ph_eval_at, inner_rel->relids))
        {
          QualCost cost;

          cost_qual_eval_node(&cost, (Node *)phinfo->ph_var->phexpr, root);
          joinrel->reltarget->cost.startup += cost.startup;
          joinrel->reltarget->cost.per_tuple += cost.per_tuple;
        }
      }

         
                                                                    
                                                                 
                                                                        
                                                                    
                                                                     
                                                                    
                                                                  
         
                                                                     
                                                          
                                                                      
                                                                  
                                
         
      joinrel->direct_lateral_relids = bms_add_members(joinrel->direct_lateral_relids, phinfo->ph_lateral);
    }
  }
}
