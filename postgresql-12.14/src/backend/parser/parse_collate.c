                                                                            
   
                   
                                                  
   
                                                                         
                                                                        
                                                                       
                                                                        
                                                                         
                                                              
   
                                                            
                                                                        
                                                                          
                                                                 
                                                                             
                                                                            
                                                                           
              
   
                                                                         
                                                                          
                                                                            
                                                                             
           
   
                                                                            
                                                                     
                                                                   
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_aggregate.h"
#include "catalog/pg_collation.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_collate.h"
#include "utils/lsyscache.h"

   
                                                                            
                                                                             
                                                  
   
typedef enum
{
  COLLATE_NONE,                                                    
  COLLATE_IMPLICIT,                                       
  COLLATE_CONFLICT,                                               
  COLLATE_EXPLICIT                                        
} CollateStrength;

typedef struct
{
  ParseState *pstate;                                              
  Oid collation;                                                  
  CollateStrength strength;                                           
  int location;                                                      
                                                                         
  Oid collation2;                                   
  int location2;                                            
} assign_collations_context;

static bool
assign_query_collations_walker(Node *node, ParseState *pstate);
static bool
assign_collations_walker(Node *node, assign_collations_context *context);
static void
merge_collation_state(Oid collation, CollateStrength strength, int location, Oid collation2, int location2, assign_collations_context *context);
static void
assign_aggregate_collations(Aggref *aggref, assign_collations_context *loccontext);
static void
assign_ordered_set_collations(Aggref *aggref, assign_collations_context *loccontext);
static void
assign_hypothetical_collations(Aggref *aggref, assign_collations_context *loccontext);

   
                             
                                                                        
   
                                                                           
                                                                         
                                                
   
void
assign_query_collations(ParseState *pstate, Query *query)
{
     
                                                                             
                                                                           
                                                                            
                                                              
     
  (void)query_tree_walker(query, assign_query_collations_walker, (void *)pstate, QTW_IGNORE_RANGE_TABLE | QTW_IGNORE_CTE_SUBQUERIES);
}

   
                                      
   
                                                                          
                                                                     
                                                                  
                                                                           
                              
   
static bool
assign_query_collations_walker(Node *node, ParseState *pstate)
{
                                                
  if (node == NULL)
  {
    return false;
  }

     
                                                                            
                                                   
     
  if (IsA(node, SetOperationStmt))
  {
    return false;
  }

  if (IsA(node, List))
  {
    assign_list_collations(pstate, (List *)node);
  }
  else
  {
    assign_expr_collations(pstate, node);
  }

  return false;
}

   
                            
                                                                          
   
                                                                             
                                
   
void
assign_list_collations(ParseState *pstate, List *exprs)
{
  ListCell *lc;

  foreach (lc, exprs)
  {
    Node *node = (Node *)lfirst(lc);

    assign_expr_collations(pstate, node);
  }
}

   
                            
                                                                            
   
                                                                             
                                                                              
                                                                            
                        
   
void
assign_expr_collations(ParseState *pstate, Node *expr)
{
  assign_collations_context context;

                                        
  context.pstate = pstate;
  context.collation = InvalidOid;
  context.strength = COLLATE_NONE;
  context.location = -1;

                      
  (void)assign_collations_walker(expr, &context);
}

   
                             
                                                           
   
                                                                         
                        
   
                                                                            
                                                                           
                                                                      
   
                                                                               
                                                                            
                                                                  
                 
   
Oid
select_common_collation(ParseState *pstate, List *exprs, bool none_ok)
{
  assign_collations_context context;

                                        
  context.pstate = pstate;
  context.collation = InvalidOid;
  context.strength = COLLATE_NONE;
  context.location = -1;

                      
  (void)assign_collations_walker((Node *)exprs, &context);

                                    
  if (context.strength == COLLATE_CONFLICT)
  {
    if (none_ok)
    {
      return InvalidOid;
    }
    ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("collation mismatch between implicit collations \"%s\" and \"%s\"", get_collation_name(context.collation), get_collation_name(context.collation2)), errhint("You can choose the collation by applying the COLLATE clause to one or both expressions."), parser_errposition(context.pstate, context.location2)));
  }

     
                                                                           
                                                                       
                           
     
  return context.collation;
}

   
                              
                                            
   
                                                                           
                                                       
   
                                                                           
                                                                            
                                                                             
   
static bool
assign_collations_walker(Node *node, assign_collations_context *context)
{
  assign_collations_context loccontext;
  Oid collation;
  CollateStrength strength;
  int location;

                                                
  if (node == NULL)
  {
    return false;
  }

     
                                                                            
                                                                            
                                            
     
  loccontext.pstate = context->pstate;
  loccontext.collation = InvalidOid;
  loccontext.strength = COLLATE_NONE;
  loccontext.location = -1;
                                                                       
  loccontext.collation2 = InvalidOid;
  loccontext.location2 = -1;

     
                                                                         
     
                                                                            
                    
     
  switch (nodeTag(node))
  {
  case T_CollateExpr:
  {
       
                                                                   
                                                               
                                  
       
    CollateExpr *expr = (CollateExpr *)node;

    (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);

    collation = expr->collOid;
    Assert(OidIsValid(collation));
    strength = COLLATE_EXPLICIT;
    location = expr->location;
  }
  break;
  case T_FieldSelect:
  {
       
                                                            
                                                                   
                                                              
                                                               
                                        
       
    FieldSelect *expr = (FieldSelect *)node;

                                
    (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);

    if (OidIsValid(expr->resultcollid))
    {
                                             
                                                            
      collation = expr->resultcollid;
      strength = COLLATE_IMPLICIT;
      location = exprLocation(node);
    }
    else
    {
                                                
      collation = InvalidOid;
      strength = COLLATE_NONE;
      location = -1;                    
    }
  }
  break;
  case T_RowExpr:
  {
       
                                                                
                                                                   
                                         
       
    RowExpr *expr = (RowExpr *)node;

    assign_list_collations(context->pstate, expr->args);

       
                                                                
                                                                
                                              
       
    return false;           
  }
  case T_RowCompareExpr:
  {
       
                                                               
                                                                 
                                                                
                                                             
       
    RowCompareExpr *expr = (RowCompareExpr *)node;
    List *colls = NIL;
    ListCell *l;
    ListCell *r;

    forboth(l, expr->largs, r, expr->rargs)
    {
      Node *le = (Node *)lfirst(l);
      Node *re = (Node *)lfirst(r);
      Oid coll;

      coll = select_common_collation(context->pstate, list_make2(le, re), true);
      colls = lappend_oid(colls, coll);
    }
    expr->inputcollids = colls;

       
                                                                  
                                                                   
                                       
       
    return false;           
  }
  case T_CoerceToDomain:
  {
       
                                                                
                                                                
                                                             
                                                                   
                                                           
                                       
       
    CoerceToDomain *expr = (CoerceToDomain *)node;
    Oid typcollation = get_typcollation(expr->resulttype);

                                
    (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);

    if (OidIsValid(typcollation))
    {
                                             
      if (typcollation == DEFAULT_COLLATION_OID)
      {
                                                    
        collation = loccontext.collation;
        strength = loccontext.strength;
        location = loccontext.location;
      }
      else
      {
                                                           
        collation = typcollation;
        strength = COLLATE_IMPLICIT;
        location = exprLocation(node);
      }
    }
    else
    {
                                                
      collation = InvalidOid;
      strength = COLLATE_NONE;
      location = -1;                    
    }

       
                                                            
                                           
       
    if (strength == COLLATE_CONFLICT)
    {
      exprSetCollation(node, InvalidOid);
    }
    else
    {
      exprSetCollation(node, collation);
    }
  }
  break;
  case T_TargetEntry:
    (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);

       
                                                                   
                                                                   
                                                                     
       
    collation = loccontext.collation;
    strength = loccontext.strength;
    location = loccontext.location;

       
                                                                       
                                                                       
                                                                       
                                                                      
                                                                    
                                                                      
                                                                   
                                                                       
                                                                       
                                                                
                         
       
    if (strength == COLLATE_CONFLICT && ((TargetEntry *)node)->ressortgroupref != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("collation mismatch between implicit collations \"%s\" and \"%s\"", get_collation_name(loccontext.collation), get_collation_name(loccontext.collation2)), errhint("You can choose the collation by applying the COLLATE clause to one or both expressions."), parser_errposition(context->pstate, loccontext.location2)));
    }
    break;
  case T_InferenceElem:
  case T_RangeTblRef:
  case T_JoinExpr:
  case T_FromExpr:
  case T_OnConflictExpr:
  case T_SortGroupClause:
    (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);

       
                                                                     
                                                                       
                                                                     
                                                             
       
    return false;
  case T_Query:
  {
       
                                                                  
                                                                  
                                                                 
                                                            
                                                                  
                                                                   
                           
       
                                                                 
       
    Query *qtree = (Query *)node;
    TargetEntry *tent;

    if (qtree->targetList == NIL)
    {
      return false;
    }
    tent = linitial_node(TargetEntry, qtree->targetList);
    if (tent->resjunk)
    {
      return false;
    }

    collation = exprCollation((Node *)tent->expr);
                                                             
    strength = COLLATE_IMPLICIT;
    location = exprLocation((Node *)tent->expr);
  }
  break;
  case T_List:
    (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);

       
                                                                    
                          
       
    collation = loccontext.collation;
    strength = loccontext.strength;
    location = loccontext.location;
    break;

  case T_Var:
  case T_Const:
  case T_Param:
  case T_CoerceToDomainValue:
  case T_CaseTestExpr:
  case T_SetToDefault:
  case T_CurrentOfExpr:

       
                                                                  
                                                                    
                                                              
                    
       
    collation = exprCollation(node);

       
                                                                
                                                                     
                                                                       
                                                                    
                                                                       
                
       

    if (OidIsValid(collation))
    {
      strength = COLLATE_IMPLICIT;
    }
    else
    {
      strength = COLLATE_NONE;
    }
    location = exprLocation(node);
    break;

  default:
  {
       
                                                                   
                                                             
       
    Oid typcollation;

       
                                                           
                                                                
                          
       
    switch (nodeTag(node))
    {
    case T_Aggref:
    {
         
                                                        
                                                      
                                                  
                                                    
                     
         
      Aggref *aggref = (Aggref *)node;

      switch (aggref->aggkind)
      {
      case AGGKIND_NORMAL:
        assign_aggregate_collations(aggref, &loccontext);
        break;
      case AGGKIND_ORDERED_SET:
        assign_ordered_set_collations(aggref, &loccontext);
        break;
      case AGGKIND_HYPOTHETICAL:
        assign_hypothetical_collations(aggref, &loccontext);
        break;
      default:
        elog(ERROR, "unrecognized aggkind: %d", (int)aggref->aggkind);
      }

      assign_expr_collations(context->pstate, (Node *)aggref->aggfilter);
    }
    break;
    case T_WindowFunc:
    {
         
                                                         
                                                  
         
      WindowFunc *wfunc = (WindowFunc *)node;

      (void)assign_collations_walker((Node *)wfunc->args, &loccontext);

      assign_expr_collations(context->pstate, (Node *)wfunc->aggfilter);
    }
    break;
    case T_CaseExpr:
    {
         
                                                      
                                                      
                                                      
                                                       
                                                        
                                                   
         
      CaseExpr *expr = (CaseExpr *)node;
      ListCell *lc;

      foreach (lc, expr->args)
      {
        CaseWhen *when = lfirst_node(CaseWhen, lc);

           
                                                    
                                                   
                                                       
                                                     
                                    
           
        (void)assign_collations_walker((Node *)when->expr, &loccontext);
        (void)assign_collations_walker((Node *)when->result, &loccontext);
      }
      (void)assign_collations_walker((Node *)expr->defresult, &loccontext);
    }
    break;
    default:

         
                                                       
                                
         
      (void)expression_tree_walker(node, assign_collations_walker, (void *)&loccontext);
      break;
    }

       
                                                             
       
    typcollation = get_typcollation(exprType(node));
    if (OidIsValid(typcollation))
    {
                                                              
      if (loccontext.strength > COLLATE_NONE)
      {
                                                       
        collation = loccontext.collation;
        strength = loccontext.strength;
        location = loccontext.location;
      }
      else
      {
           
                                                             
                                                              
                                                               
                    
           
        collation = typcollation;
        strength = COLLATE_IMPLICIT;
        location = exprLocation(node);
      }
    }
    else
    {
                                                
      collation = InvalidOid;
      strength = COLLATE_NONE;
      location = -1;                    
    }

       
                                                                  
                                                             
                                                              
       
    if (strength == COLLATE_CONFLICT)
    {
      exprSetCollation(node, InvalidOid);
    }
    else
    {
      exprSetCollation(node, collation);
    }

       
                                                                
                                                    
       
    if (loccontext.strength == COLLATE_CONFLICT)
    {
      exprSetInputCollation(node, InvalidOid);
    }
    else
    {
      exprSetInputCollation(node, loccontext.collation);
    }
  }
  break;
  }

     
                                                       
     
  merge_collation_state(collation, strength, location, loccontext.collation2, loccontext.location2, context);

  return false;
}

   
                                                                             
   
static void
merge_collation_state(Oid collation, CollateStrength strength, int location, Oid collation2, int location2, assign_collations_context *context)
{
     
                                                                      
                                                                             
                       
     
  if (strength > context->strength)
  {
                                        
    context->collation = collation;
    context->strength = strength;
    context->location = location;
                                            
    if (strength == COLLATE_CONFLICT)
    {
      context->collation2 = collation2;
      context->location2 = location2;
    }
  }
  else if (strength == context->strength)
  {
                                                                
    switch (strength)
    {
    case COLLATE_NONE:
                                              
      break;
    case COLLATE_IMPLICIT:
      if (collation != context->collation)
      {
           
                                                                
           
        if (context->collation == DEFAULT_COLLATION_OID)
        {
                                              
          context->collation = collation;
          context->strength = strength;
          context->location = location;
        }
        else if (collation != DEFAULT_COLLATION_OID)
        {
             
                                                              
                                                             
                                                                
                                                                 
                                               
             
          context->strength = COLLATE_CONFLICT;
          context->collation2 = collation;
          context->location2 = location;
        }
      }
      break;
    case COLLATE_CONFLICT:
                                      
      break;
    case COLLATE_EXPLICIT:
      if (collation != context->collation)
      {
           
                                                                 
                                                                   
                                                                   
                              
           
        ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("collation mismatch between explicit collations \"%s\" and \"%s\"", get_collation_name(context->collation), get_collation_name(collation)), parser_errposition(context->pstate, location)));
      }
      break;
    }
  }
}

   
                                                                       
                                                                        
                                                                    
                                                                           
                                
   
                                                                           
                                                                            
                                                                             
                            
   
                                                                             
                                                                 
   
static void
assign_aggregate_collations(Aggref *aggref, assign_collations_context *loccontext)
{
  ListCell *lc;

                                            
  Assert(aggref->aggdirectargs == NIL);

                                                                     
  foreach (lc, aggref->args)
  {
    TargetEntry *tle = lfirst_node(TargetEntry, lc);

    if (tle->resjunk)
    {
      assign_expr_collations(loccontext->pstate, (Node *)tle);
    }
    else
    {
      (void)assign_collations_walker((Node *)tle, loccontext);
    }
  }
}

   
                                                                          
                                                                             
                                                                             
                                                                              
                                                                         
                                                                            
                                                                              
                                                                           
                                                                           
                                                                
                                                                      
                                                                             
                                                                         
   
                                                         
   
static void
assign_ordered_set_collations(Aggref *aggref, assign_collations_context *loccontext)
{
  bool merge_sort_collations;
  ListCell *lc;

                                                                     
  merge_sort_collations = (list_length(aggref->args) == 1 && get_func_variadictype(aggref->aggfnoid) == InvalidOid);

                                                                   
  (void)assign_collations_walker((Node *)aggref->aggdirectargs, loccontext);

                                             
  foreach (lc, aggref->args)
  {
    TargetEntry *tle = lfirst_node(TargetEntry, lc);

    if (merge_sort_collations)
    {
      (void)assign_collations_walker((Node *)tle, loccontext);
    }
    else
    {
      assign_expr_collations(loccontext->pstate, (Node *)tle);
    }
  }
}

   
                                                                           
                                                                          
                                                                          
                                                                          
                                                                         
                                                                       
                                                    
   
static void
assign_hypothetical_collations(Aggref *aggref, assign_collations_context *loccontext)
{
  ListCell *h_cell = list_head(aggref->aggdirectargs);
  ListCell *s_cell = list_head(aggref->args);
  bool merge_sort_collations;
  int extra_args;

                                                                     
  merge_sort_collations = (list_length(aggref->args) == 1 && get_func_variadictype(aggref->aggfnoid) == InvalidOid);

                                                
  extra_args = list_length(aggref->aggdirectargs) - list_length(aggref->args);
  Assert(extra_args >= 0);
  while (extra_args-- > 0)
  {
    (void)assign_collations_walker((Node *)lfirst(h_cell), loccontext);
    h_cell = lnext(h_cell);
  }

                                                              
  while (h_cell && s_cell)
  {
    Node *h_arg = (Node *)lfirst(h_cell);
    TargetEntry *s_tle = (TargetEntry *)lfirst(s_cell);
    assign_collations_context paircontext;

       
                                                                      
                                                              
                                                                       
                                                                     
                                                       
       
    paircontext.pstate = loccontext->pstate;
    paircontext.collation = InvalidOid;
    paircontext.strength = COLLATE_NONE;
    paircontext.location = -1;
                                                                         
    paircontext.collation2 = InvalidOid;
    paircontext.location2 = -1;

    (void)assign_collations_walker(h_arg, &paircontext);
    (void)assign_collations_walker((Node *)s_tle->expr, &paircontext);

                                      
    if (paircontext.strength == COLLATE_CONFLICT)
    {
      ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("collation mismatch between implicit collations \"%s\" and \"%s\"", get_collation_name(paircontext.collation), get_collation_name(paircontext.collation2)), errhint("You can choose the collation by applying the COLLATE clause to one or both expressions."), parser_errposition(paircontext.pstate, paircontext.location2)));
    }

       
                                                                         
                                                                           
                                                                           
                                                  
       
                                                          
                                                                  
                                                                  
                                                                         
                                                                         
                                                                         
                                                                  
                                                                         
                                                                  
                                                                      
                                                                    
                          
       
    if (OidIsValid(paircontext.collation) && paircontext.collation != exprCollation((Node *)s_tle->expr))
    {
      s_tle->expr = (Expr *)makeRelabelType(s_tle->expr, exprType((Node *)s_tle->expr), exprTypmod((Node *)s_tle->expr), paircontext.collation, COERCE_IMPLICIT_CAST);
    }

       
                                                                     
                           
       
    if (merge_sort_collations)
    {
      merge_collation_state(paircontext.collation, paircontext.strength, paircontext.location, paircontext.collation2, paircontext.location2, loccontext);
    }

    h_cell = lnext(h_cell);
    s_cell = lnext(s_cell);
  }
  Assert(h_cell == NULL && s_cell == NULL);
}
