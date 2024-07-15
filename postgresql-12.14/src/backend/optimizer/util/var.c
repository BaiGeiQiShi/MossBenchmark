                                                                            
   
         
                                    
   
                                                                    
                                                                           
                                                                         
                       
   
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/sysattr.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/placeholder.h"
#include "optimizer/prep.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"

typedef struct
{
  Relids varnos;
  PlannerInfo *root;
  int sublevels_up;
} pull_varnos_context;

typedef struct
{
  Bitmapset *varattnos;
  Index varno;
} pull_varattnos_context;

typedef struct
{
  List *vars;
  int sublevels_up;
} pull_vars_context;

typedef struct
{
  int var_location;
  int sublevels_up;
} locate_var_of_level_context;

typedef struct
{
  List *varlist;
  int flags;
} pull_var_clause_context;

typedef struct
{
  Query *query;                  
  int sublevels_up;
  bool possible_sublink;                                       
  bool inserted_sublink;                                  
} flatten_join_alias_vars_context;

static bool
pull_varnos_walker(Node *node, pull_varnos_context *context);
static bool
pull_varattnos_walker(Node *node, pull_varattnos_context *context);
static bool
pull_vars_walker(Node *node, pull_vars_context *context);
static bool
contain_var_clause_walker(Node *node, void *context);
static bool
contain_vars_of_level_walker(Node *node, int *sublevels_up);
static bool
locate_var_of_level_walker(Node *node, locate_var_of_level_context *context);
static bool
pull_var_clause_walker(Node *node, pull_var_clause_context *context);
static Node *
flatten_join_alias_vars_mutator(Node *node, flatten_join_alias_vars_context *context);
static Relids
alias_relid_set(Query *query, Relids relids);

   
               
                                                                    
                                                                         
   
                                                                             
                                                                              
                                                                        
                                                                          
   
Relids
pull_varnos(Node *node)
{
  return pull_varnos_new(NULL, node);
}

Relids
pull_varnos_new(PlannerInfo *root, Node *node)
{
  pull_varnos_context context;

  context.varnos = NULL;
  context.root = root;
  context.sublevels_up = 0;

     
                                                                          
                                                            
     
  query_or_expression_tree_walker(node, pull_varnos_walker, (void *)&context, 0);

  return context.varnos;
}

   
                        
                                                                    
                                                     
   
Relids
pull_varnos_of_level(Node *node, int levelsup)
{
  return pull_varnos_of_level_new(NULL, node, levelsup);
}

Relids
pull_varnos_of_level_new(PlannerInfo *root, Node *node, int levelsup)
{
  pull_varnos_context context;

  context.varnos = NULL;
  context.root = root;
  context.sublevels_up = levelsup;

     
                                                                          
                                                            
     
  query_or_expression_tree_walker(node, pull_varnos_walker, (void *)&context, 0);

  return context.varnos;
}

static bool
pull_varnos_walker(Node *node, pull_varnos_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == context->sublevels_up)
    {
      context->varnos = bms_add_member(context->varnos, var->varno);
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)node;

    if (context->sublevels_up == 0)
    {
      context->varnos = bms_add_member(context->varnos, cexpr->cvarno);
    }
    return false;
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

       
                                                                        
                                                                       
                                          
       
    if (phv->phlevelsup == context->sublevels_up)
    {
         
                                                                   
                                                                   
                                                                     
                                                                       
                                                         
         
                                                                     
                                                                      
                                                                       
                                                                       
                                                                   
                                                                         
                                                                     
                                                                      
                                                                  
         
                                                                         
                                                                        
                                                                        
                                                                       
                                                                      
                                                                   
                                                                     
                                                                        
                                                            
         
      PlaceHolderInfo *phinfo = NULL;

      if (phv->phlevelsup == 0 && context->root)
      {
        ListCell *lc;

        foreach (lc, context->root->placeholder_list)
        {
          phinfo = (PlaceHolderInfo *)lfirst(lc);
          if (phinfo->phid == phv->phid)
          {
            break;
          }
          phinfo = NULL;
        }
      }
      if (phinfo == NULL)
      {
                                                
        context->varnos = bms_add_members(context->varnos, phv->phrels);
      }
      else if (bms_equal(phv->phrels, phinfo->ph_var->phrels))
      {
                                         
        context->varnos = bms_add_members(context->varnos, phinfo->ph_eval_at);
      }
      else
      {
                                                                      
        Relids newevalat, delta;

                                                          
        delta = bms_difference(phinfo->ph_var->phrels, phv->phrels);
        newevalat = bms_difference(phinfo->ph_eval_at, delta);
                                                                 
        if (!bms_equal(newevalat, phinfo->ph_eval_at))
        {
                                      
          delta = bms_difference(phv->phrels, phinfo->ph_var->phrels);
          newevalat = bms_join(newevalat, delta);
        }
        context->varnos = bms_join(context->varnos, newevalat);
      }
      return false;                                    
    }
  }
  else if (IsA(node, Query))
  {
                                                                       
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, pull_varnos_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, pull_varnos_walker, (void *)context);
}

   
                  
                                                                           
                                                        
                                                                       
   
                                                                              
                                                                              
   
                                                                             
                                                                            
                                                                              
                                                               
   
void
pull_varattnos(Node *node, Index varno, Bitmapset **varattnos)
{
  pull_varattnos_context context;

  context.varattnos = *varattnos;
  context.varno = varno;

  (void)pull_varattnos_walker(node, &context);

  *varattnos = context.varattnos;
}

static bool
pull_varattnos_walker(Node *node, pull_varattnos_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varno == context->varno && var->varlevelsup == 0)
    {
      context->varattnos = bms_add_member(context->varattnos, var->varattno - FirstLowInvalidHeapAttributeNumber);
    }
    return false;
  }

                                             
  Assert(!IsA(node, Query));

  return expression_tree_walker(node, pull_varattnos_walker, (void *)context);
}

   
                      
                                                                    
                                                  
   
                                                                
   
List *
pull_vars_of_level(Node *node, int levelsup)
{
  pull_vars_context context;

  context.vars = NIL;
  context.sublevels_up = levelsup;

     
                                                                          
                                                            
     
  query_or_expression_tree_walker(node, pull_vars_walker, (void *)&context, 0);

  return context.vars;
}

static bool
pull_vars_walker(Node *node, pull_vars_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == context->sublevels_up)
    {
      context->vars = lappend(context->vars, var);
    }
    return false;
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    if (phv->phlevelsup == context->sublevels_up)
    {
      context->vars = lappend(context->vars, phv);
    }
                                                             
    return false;
  }
  if (IsA(node, Query))
  {
                                                                       
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, pull_vars_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, pull_vars_walker, (void *)context);
}

   
                      
                                                                             
                                   
   
                                        
   
                                                                            
                            
   
bool
contain_var_clause(Node *node)
{
  return contain_var_clause_walker(node, NULL);
}

static bool
contain_var_clause_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    if (((Var *)node)->varlevelsup == 0)
    {
      return true;                                               
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
    return true;
  }
  if (IsA(node, PlaceHolderVar))
  {
    if (((PlaceHolderVar *)node)->phlevelsup == 0)
    {
      return true;                                               
    }
                                                       
  }
  return expression_tree_walker(node, contain_var_clause_walker, context);
}

   
                         
                                                                             
                                   
   
                                         
   
                                                                          
   
bool
contain_vars_of_level(Node *node, int levelsup)
{
  int sublevels_up = levelsup;

  return query_or_expression_tree_walker(node, contain_vars_of_level_walker, (void *)&sublevels_up, 0);
}

static bool
contain_vars_of_level_walker(Node *node, int *sublevels_up)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    if (((Var *)node)->varlevelsup == *sublevels_up)
    {
      return true;                                           
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
    if (*sublevels_up == 0)
    {
      return true;
    }
    return false;
  }
  if (IsA(node, PlaceHolderVar))
  {
    if (((PlaceHolderVar *)node)->phlevelsup == *sublevels_up)
    {
      return true;                                               
    }
                                                       
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    (*sublevels_up)++;
    result = query_tree_walker((Query *)node, contain_vars_of_level_walker, (void *)sublevels_up, 0);
    (*sublevels_up)--;
    return result;
  }
  return expression_tree_walker(node, contain_vars_of_level_walker, (void *)sublevels_up);
}

   
                       
                                                                      
   
                                                                      
                                                                       
                                                                
   
                                                                          
   
                                                                    
                                                                         
                                                                      
                                                        
   
int
locate_var_of_level(Node *node, int levelsup)
{
  locate_var_of_level_context context;

  context.var_location = -1;                              
  context.sublevels_up = levelsup;

  (void)query_or_expression_tree_walker(node, locate_var_of_level_walker, (void *)&context, 0);

  return context.var_location;
}

static bool
locate_var_of_level_walker(Node *node, locate_var_of_level_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == context->sublevels_up && var->location >= 0)
    {
      context->var_location = var->location;
      return true;                                           
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
                                                                       
    return false;
  }
                                                                            
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, locate_var_of_level_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, locate_var_of_level_walker, (void *)context);
}

   
                   
                                                                
   
                                                             
                                                           
                                                          
                                                
                                                                        
                                               
   
                                                                 
                                                                
                                                               
                                                    
                                                                           
                                                
   
                                                                     
                                                                    
                                                                   
                                                        
                                                                    
                                                 
   
                                                                        
                          
   
                                                   
   
                                                                      
                                                           
   
                                                                     
                              
   
                                                                            
                            
   
List *
pull_var_clause(Node *node, int flags)
{
  pull_var_clause_context context;

                                                               
  Assert((flags & (PVC_INCLUDE_AGGREGATES | PVC_RECURSE_AGGREGATES)) != (PVC_INCLUDE_AGGREGATES | PVC_RECURSE_AGGREGATES));
  Assert((flags & (PVC_INCLUDE_WINDOWFUNCS | PVC_RECURSE_WINDOWFUNCS)) != (PVC_INCLUDE_WINDOWFUNCS | PVC_RECURSE_WINDOWFUNCS));
  Assert((flags & (PVC_INCLUDE_PLACEHOLDERS | PVC_RECURSE_PLACEHOLDERS)) != (PVC_INCLUDE_PLACEHOLDERS | PVC_RECURSE_PLACEHOLDERS));

  context.varlist = NIL;
  context.flags = flags;

  pull_var_clause_walker(node, &context);
  return context.varlist;
}

static bool
pull_var_clause_walker(Node *node, pull_var_clause_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    if (((Var *)node)->varlevelsup != 0)
    {
      elog(ERROR, "Upper-level Var found where not expected");
    }
    context->varlist = lappend(context->varlist, node);
    return false;
  }
  else if (IsA(node, Aggref))
  {
    if (((Aggref *)node)->agglevelsup != 0)
    {
      elog(ERROR, "Upper-level Aggref found where not expected");
    }
    if (context->flags & PVC_INCLUDE_AGGREGATES)
    {
      context->varlist = lappend(context->varlist, node);
                                                           
      return false;
    }
    else if (context->flags & PVC_RECURSE_AGGREGATES)
    {
                                                                  
    }
    else
    {
      elog(ERROR, "Aggref found where not expected");
    }
  }
  else if (IsA(node, GroupingFunc))
  {
    if (((GroupingFunc *)node)->agglevelsup != 0)
    {
      elog(ERROR, "Upper-level GROUPING found where not expected");
    }
    if (context->flags & PVC_INCLUDE_AGGREGATES)
    {
      context->varlist = lappend(context->varlist, node);
                                                           
      return false;
    }
    else if (context->flags & PVC_RECURSE_AGGREGATES)
    {
                                                                     
    }
    else
    {
      elog(ERROR, "GROUPING found where not expected");
    }
  }
  else if (IsA(node, WindowFunc))
  {
                                                         
    if (context->flags & PVC_INCLUDE_WINDOWFUNCS)
    {
      context->varlist = lappend(context->varlist, node);
                                                            
      return false;
    }
    else if (context->flags & PVC_RECURSE_WINDOWFUNCS)
    {
                                                                   
    }
    else
    {
      elog(ERROR, "WindowFunc found where not expected");
    }
  }
  else if (IsA(node, PlaceHolderVar))
  {
    if (((PlaceHolderVar *)node)->phlevelsup != 0)
    {
      elog(ERROR, "Upper-level PlaceHolderVar found where not expected");
    }
    if (context->flags & PVC_INCLUDE_PLACEHOLDERS)
    {
      context->varlist = lappend(context->varlist, node);
                                                           
      return false;
    }
    else if (context->flags & PVC_RECURSE_PLACEHOLDERS)
    {
                                                                     
    }
    else
    {
      elog(ERROR, "PlaceHolderVar found where not expected");
    }
  }
  return expression_tree_walker(node, pull_var_clause_walker, (void *)context);
}

   
                           
                                                                              
                                                                              
                                                                             
                                                                         
                                                                            
                                                                           
   
                                                                       
                                                          
   
                                                                           
                                                                            
                                                                         
                                                                         
                                                                     
                                                                         
                                                                            
   
                                                                           
                                                                            
                                                                       
                
   
Node *
flatten_join_alias_vars(Query *query, Node *node)
{
  flatten_join_alias_vars_context context;

  context.query = query;
  context.sublevels_up = 0;
                                                                 
  context.possible_sublink = query->hasSubLinks;
                                                            
  context.inserted_sublink = query->hasSubLinks;

  return flatten_join_alias_vars_mutator(node, &context);
}

static Node *
flatten_join_alias_vars_mutator(Node *node, flatten_join_alias_vars_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;
    RangeTblEntry *rte;
    Node *newvar;

                                                                    
    if (var->varlevelsup != context->sublevels_up)
    {
      return node;                              
    }
    rte = rt_fetch(var->varno, context->query->rtable);
    if (rte->rtekind != RTE_JOIN)
    {
      return node;
    }
    if (var->varattno == InvalidAttrNumber)
    {
                                           
      RowExpr *rowexpr;
      List *fields = NIL;
      List *colnames = NIL;
      ListCell *lv;
      ListCell *ln;

      Assert(list_length(rte->joinaliasvars) == list_length(rte->eref->colnames));
      forboth(lv, rte->joinaliasvars, ln, rte->eref->colnames)
      {
        newvar = (Node *)lfirst(lv);
                                    
        if (newvar == NULL)
        {
          continue;
        }
        newvar = copyObject(newvar);

           
                                                                   
                                                      
           
        if (context->sublevels_up != 0)
        {
          IncrementVarSublevelsUp(newvar, context->sublevels_up, 0);
        }
                                                           
        if (IsA(newvar, Var))
        {
          ((Var *)newvar)->location = var->location;
        }
                                                         
                                                                     
        newvar = flatten_join_alias_vars_mutator(newvar, context);
        fields = lappend(fields, newvar);
                                                           
        colnames = lappend(colnames, copyObject((Node *)lfirst(ln)));
      }
      rowexpr = makeNode(RowExpr);
      rowexpr->args = fields;
      rowexpr->row_typeid = var->vartype;
      rowexpr->row_format = COERCE_IMPLICIT_CAST;
      rowexpr->colnames = colnames;
      rowexpr->location = var->location;

      return (Node *)rowexpr;
    }

                                     
    Assert(var->varattno > 0);
    newvar = (Node *)list_nth(rte->joinaliasvars, var->varattno - 1);
    Assert(newvar != NULL);
    newvar = copyObject(newvar);

       
                                                                           
                                      
       
    if (context->sublevels_up != 0)
    {
      IncrementVarSublevelsUp(newvar, context->sublevels_up, 0);
    }

                                                       
    if (IsA(newvar, Var))
    {
      ((Var *)newvar)->location = var->location;
    }

                                                     
    newvar = flatten_join_alias_vars_mutator(newvar, context);

                                                    
    if (context->possible_sublink && !context->inserted_sublink)
    {
      context->inserted_sublink = checkExprHasSubLink(newvar);
    }

    return newvar;
  }
  if (IsA(node, PlaceHolderVar))
  {
                                                                        
    PlaceHolderVar *phv;

    phv = (PlaceHolderVar *)expression_tree_mutator(node, flatten_join_alias_vars_mutator, (void *)context);
                                             
    if (phv->phlevelsup == context->sublevels_up)
    {
      phv->phrels = alias_relid_set(context->query, phv->phrels);
    }
    return (Node *)phv;
  }

  if (IsA(node, Query))
  {
                                                                       
    Query *newnode;
    bool save_inserted_sublink;

    context->sublevels_up++;
    save_inserted_sublink = context->inserted_sublink;
    context->inserted_sublink = ((Query *)node)->hasSubLinks;
    newnode = query_tree_mutator((Query *)node, flatten_join_alias_vars_mutator, (void *)context, QTW_IGNORE_JOINALIASES);
    newnode->hasSubLinks |= context->inserted_sublink;
    context->inserted_sublink = save_inserted_sublink;
    context->sublevels_up--;
    return (Node *)newnode;
  }
                                          
  Assert(!IsA(node, SubPlan));
                                                                   
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

  return expression_tree_mutator(node, flatten_join_alias_vars_mutator, (void *)context);
}

   
                                                                   
                          
   
static Relids
alias_relid_set(Query *query, Relids relids)
{
  Relids result = NULL;
  int rtindex;

  rtindex = -1;
  while ((rtindex = bms_next_member(relids, rtindex)) >= 0)
  {
    RangeTblEntry *rte = rt_fetch(rtindex, query->rtable);

    if (rte->rtekind == RTE_JOIN)
    {
      result = bms_join(result, get_relids_for_join(query, rtindex));
    }
    else
    {
      result = bms_add_member(result, rtindex);
    }
  }
  return result;
}
