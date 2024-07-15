                                                                            
   
               
                                                      
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_aggregate.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

typedef struct
{
  ParseState *pstate;
  int min_varlevel;
  int min_agglevel;
  int sublevels_up;
} check_agg_arguments_context;

typedef struct
{
  ParseState *pstate;
  Query *qry;
  bool hasJoinRTEs;
  List *groupClauses;
  List *groupClauseCommonVars;
  bool have_non_var_grouping;
  List **func_grouped_rels;
  int sublevels_up;
  bool in_agg_direct_args;
} check_ungrouped_columns_context;

static int
check_agg_arguments(ParseState *pstate, List *directargs, List *args, Expr *filter);
static bool
check_agg_arguments_walker(Node *node, check_agg_arguments_context *context);
static void
check_ungrouped_columns(Node *node, ParseState *pstate, Query *qry, List *groupClauses, List *groupClauseVars, bool have_non_var_grouping, List **func_grouped_rels);
static bool
check_ungrouped_columns_walker(Node *node, check_ungrouped_columns_context *context);
static void
finalize_grouping_exprs(Node *node, ParseState *pstate, Query *qry, List *groupClauses, bool hasJoinRTEs, bool have_non_var_grouping);
static bool
finalize_grouping_exprs_walker(Node *node, check_ungrouped_columns_context *context);
static void
check_agglevels_and_constraints(ParseState *pstate, Node *expr);
static List *
expand_groupingset_node(GroupingSet *gs);
static Node *
make_agg_arg(Oid argtype, Oid argcollation);

   
                            
                                                       
   
                                                                            
                                                                         
                                                                            
                                                                             
                                                                           
                       
   
                                                                               
                                                                          
                                                                               
                                                                       
                                                                              
                                                                         
                                                                     
                                                                              
          
   
                                                                               
                                                                             
                 
   
void
transformAggregateCall(ParseState *pstate, Aggref *agg, List *args, List *aggorder, bool agg_distinct)
{
  List *argtypes = NIL;
  List *tlist = NIL;
  List *torder = NIL;
  List *tdistinct = NIL;
  AttrNumber attno = 1;
  int save_next_resno;
  ListCell *lc;

     
                                                                             
                                            
     
  foreach (lc, args)
  {
    Expr *arg = (Expr *)lfirst(lc);

    argtypes = lappend_oid(argtypes, exprType((Node *)arg));
  }
  agg->aggargtypes = argtypes;

  if (AGGKIND_IS_ORDERED_SET(agg->aggkind))
  {
       
                                                                      
                                                  
       
    int numDirectArgs = list_length(args) - list_length(aggorder);
    List *aargs;
    ListCell *lc2;

    Assert(numDirectArgs >= 0);

    aargs = list_copy_tail(args, numDirectArgs);
    agg->aggdirectargs = list_truncate(args, numDirectArgs);

       
                                                                         
                                                                        
                                                                           
                                                                 
       
    forboth(lc, aargs, lc2, aggorder)
    {
      Expr *arg = (Expr *)lfirst(lc);
      SortBy *sortby = (SortBy *)lfirst(lc2);
      TargetEntry *tle;

                                                                 
      tle = makeTargetEntry(arg, attno++, NULL, false);
      tlist = lappend(tlist, tle);

      torder = addTargetToSortList(pstate, tle, torder, tlist, sortby);
    }

                                                  
    Assert(!agg_distinct);
  }
  else
  {
                                                     
    agg->aggdirectargs = NIL;

       
                                                            
       
    foreach (lc, args)
    {
      Expr *arg = (Expr *)lfirst(lc);
      TargetEntry *tle;

                                                                 
      tle = makeTargetEntry(arg, attno++, NULL, false);
      tlist = lappend(tlist, tle);
    }

       
                                                                           
                                                                       
                                                                           
                                               
       
                                                                         
                                   
       
    save_next_resno = pstate->p_next_resno;
    pstate->p_next_resno = attno;

    torder = transformSortClause(pstate, aggorder, &tlist, EXPR_KIND_ORDER_BY, true                        );

       
                                                                      
       
    if (agg_distinct)
    {
      tdistinct = transformDistinctClause(pstate, &tlist, torder, true);

         
                                                                       
                                   
         
      foreach (lc, tdistinct)
      {
        SortGroupClause *sortcl = (SortGroupClause *)lfirst(lc);

        if (!OidIsValid(sortcl->sortop))
        {
          Node *expr = get_sortgroupclause_expr(sortcl, tlist);

          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify an ordering operator for type %s", format_type_be(exprType(expr))), errdetail("Aggregates with DISTINCT must be able to sort their inputs."), parser_errposition(pstate, exprLocation(expr))));
        }
      }
    }

    pstate->p_next_resno = save_next_resno;
  }

                                                         
  agg->args = tlist;
  agg->aggorder = torder;
  agg->aggdistinct = tdistinct;

  check_agglevels_and_constraints(pstate, (Node *)agg);
}

   
                         
                                    
   
                                                                                
                                                                           
   
Node *
transformGroupingFunc(ParseState *pstate, GroupingFunc *p)
{
  ListCell *lc;
  List *args = p->args;
  List *result_list = NIL;
  GroupingFunc *result = makeNode(GroupingFunc);

  if (list_length(args) > 31)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg("GROUPING must have fewer than 32 arguments"), parser_errposition(pstate, p->location)));
  }

  foreach (lc, args)
  {
    Node *current_result;

    current_result = transformExpr(pstate, (Node *)lfirst(lc), pstate->p_expr_kind);

                                                       

    result_list = lappend(result_list, current_result);
  }

  result->args = result_list;
  result->location = p->location;

  check_agglevels_and_constraints(pstate, (Node *)result);

  return (Node *)result;
}

   
                                                                               
                                                                              
                                                                                
                                       
   
static void
check_agglevels_and_constraints(ParseState *pstate, Node *expr)
{
  List *directargs = NIL;
  List *args = NIL;
  Expr *filter = NULL;
  int min_varlevel;
  int location = -1;
  Index *p_levelsup;
  const char *err;
  bool errkind;
  bool isAgg = IsA(expr, Aggref);

  if (isAgg)
  {
    Aggref *agg = (Aggref *)expr;

    directargs = agg->aggdirectargs;
    args = agg->args;
    filter = agg->aggfilter;
    location = agg->location;
    p_levelsup = &agg->agglevelsup;
  }
  else
  {
    GroupingFunc *grp = (GroupingFunc *)expr;

    args = grp->args;
    location = grp->location;
    p_levelsup = &grp->agglevelsup;
  }

     
                                                                     
                       
     
  min_varlevel = check_agg_arguments(pstate, directargs, args, filter);

  *p_levelsup = min_varlevel;

                                                          
  while (min_varlevel-- > 0)
  {
    pstate = pstate->parentParseState;
  }
  pstate->p_hasAggs = true;

     
                                                                          
                            
     
                                                                         
                                                                           
                                                                             
                                                                          
                                                      
     
  err = NULL;
  errkind = false;
  switch (pstate->p_expr_kind)
  {
  case EXPR_KIND_NONE:
    Assert(false);                   
    break;
  case EXPR_KIND_OTHER:

       
                                                                  
              
       
    break;
  case EXPR_KIND_JOIN_ON:
  case EXPR_KIND_JOIN_USING:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in JOIN conditions");
    }
    else
    {
      err = _("grouping operations are not allowed in JOIN conditions");
    }

    break;
  case EXPR_KIND_FROM_SUBSELECT:
                                                       
    Assert(pstate->p_lateral_active);

       
                                                                   
            
       
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in FROM clause of their own query level");
    }
    else
    {
      err = _("grouping operations are not allowed in FROM clause of their own query level");
    }

    break;
  case EXPR_KIND_FROM_FUNCTION:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in functions in FROM");
    }
    else
    {
      err = _("grouping operations are not allowed in functions in FROM");
    }

    break;
  case EXPR_KIND_WHERE:
    errkind = true;
    break;
  case EXPR_KIND_POLICY:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in policy expressions");
    }
    else
    {
      err = _("grouping operations are not allowed in policy expressions");
    }

    break;
  case EXPR_KIND_HAVING:
              
    break;
  case EXPR_KIND_FILTER:
    errkind = true;
    break;
  case EXPR_KIND_WINDOW_PARTITION:
              
    break;
  case EXPR_KIND_WINDOW_ORDER:
              
    break;
  case EXPR_KIND_WINDOW_FRAME_RANGE:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in window RANGE");
    }
    else
    {
      err = _("grouping operations are not allowed in window RANGE");
    }

    break;
  case EXPR_KIND_WINDOW_FRAME_ROWS:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in window ROWS");
    }
    else
    {
      err = _("grouping operations are not allowed in window ROWS");
    }

    break;
  case EXPR_KIND_WINDOW_FRAME_GROUPS:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in window GROUPS");
    }
    else
    {
      err = _("grouping operations are not allowed in window GROUPS");
    }

    break;
  case EXPR_KIND_SELECT_TARGET:
              
    break;
  case EXPR_KIND_INSERT_TARGET:
  case EXPR_KIND_UPDATE_SOURCE:
  case EXPR_KIND_UPDATE_TARGET:
    errkind = true;
    break;
  case EXPR_KIND_GROUP_BY:
    errkind = true;
    break;
  case EXPR_KIND_ORDER_BY:
              
    break;
  case EXPR_KIND_DISTINCT_ON:
              
    break;
  case EXPR_KIND_LIMIT:
  case EXPR_KIND_OFFSET:
    errkind = true;
    break;
  case EXPR_KIND_RETURNING:
    errkind = true;
    break;
  case EXPR_KIND_VALUES:
  case EXPR_KIND_VALUES_SINGLE:
    errkind = true;
    break;
  case EXPR_KIND_CHECK_CONSTRAINT:
  case EXPR_KIND_DOMAIN_CHECK:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in check constraints");
    }
    else
    {
      err = _("grouping operations are not allowed in check constraints");
    }

    break;
  case EXPR_KIND_COLUMN_DEFAULT:
  case EXPR_KIND_FUNCTION_DEFAULT:

    if (isAgg)
    {
      err = _("aggregate functions are not allowed in DEFAULT expressions");
    }
    else
    {
      err = _("grouping operations are not allowed in DEFAULT expressions");
    }

    break;
  case EXPR_KIND_INDEX_EXPRESSION:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in index expressions");
    }
    else
    {
      err = _("grouping operations are not allowed in index expressions");
    }

    break;
  case EXPR_KIND_INDEX_PREDICATE:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in index predicates");
    }
    else
    {
      err = _("grouping operations are not allowed in index predicates");
    }

    break;
  case EXPR_KIND_ALTER_COL_TRANSFORM:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in transform expressions");
    }
    else
    {
      err = _("grouping operations are not allowed in transform expressions");
    }

    break;
  case EXPR_KIND_EXECUTE_PARAMETER:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in EXECUTE parameters");
    }
    else
    {
      err = _("grouping operations are not allowed in EXECUTE parameters");
    }

    break;
  case EXPR_KIND_TRIGGER_WHEN:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in trigger WHEN conditions");
    }
    else
    {
      err = _("grouping operations are not allowed in trigger WHEN conditions");
    }

    break;
  case EXPR_KIND_PARTITION_BOUND:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in partition bound");
    }
    else
    {
      err = _("grouping operations are not allowed in partition bound");
    }

    break;
  case EXPR_KIND_PARTITION_EXPRESSION:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in partition key expressions");
    }
    else
    {
      err = _("grouping operations are not allowed in partition key expressions");
    }

    break;
  case EXPR_KIND_GENERATED_COLUMN:

    if (isAgg)
    {
      err = _("aggregate functions are not allowed in column generation expressions");
    }
    else
    {
      err = _("grouping operations are not allowed in column generation expressions");
    }

    break;

  case EXPR_KIND_CALL_ARGUMENT:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in CALL arguments");
    }
    else
    {
      err = _("grouping operations are not allowed in CALL arguments");
    }

    break;

  case EXPR_KIND_COPY_WHERE:
    if (isAgg)
    {
      err = _("aggregate functions are not allowed in COPY FROM WHERE conditions");
    }
    else
    {
      err = _("grouping operations are not allowed in COPY FROM WHERE conditions");
    }

    break;

       
                                                                 
                                                                
                                                                     
                                                                      
                             
       
  }

  if (err)
  {
    ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg_internal("%s", err), parser_errposition(pstate, location)));
  }

  if (errkind)
  {
    if (isAgg)
    {
                                                                  
      err = _("aggregate functions are not allowed in %s");
    }
    else
    {
                                                                  
      err = _("grouping operations are not allowed in %s");
    }

    ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg_internal(err, ParseExprKindName(pstate->p_expr_kind)), parser_errposition(pstate, location)));
  }
}

   
                       
                                                                  
                                                                     
                              
   
                                                                               
                                                                             
                                                                              
                
   
                                                                           
                                                                     
                                                                          
                                                                           
                                                                           
                                                                              
                                                                       
   
                                                                              
                                                                           
                                                                          
                                                                            
                                                               
   
static int
check_agg_arguments(ParseState *pstate, List *directargs, List *args, Expr *filter)
{
  int agglevel;
  check_agg_arguments_context context;

  context.pstate = pstate;
  context.min_varlevel = -1;                                  
  context.min_agglevel = -1;
  context.sublevels_up = 0;

  (void)check_agg_arguments_walker((Node *)args, &context);
  (void)check_agg_arguments_walker((Node *)filter, &context);

     
                                                                       
                                                          
     
  if (context.min_varlevel < 0)
  {
    if (context.min_agglevel < 0)
    {
      agglevel = 0;
    }
    else
    {
      agglevel = context.min_agglevel;
    }
  }
  else if (context.min_agglevel < 0)
  {
    agglevel = context.min_varlevel;
  }
  else
  {
    agglevel = Min(context.min_varlevel, context.min_agglevel);
  }

     
                                                                         
     
  if (agglevel == context.min_agglevel)
  {
    int aggloc;

    aggloc = locate_agg_of_level((Node *)args, agglevel);
    if (aggloc < 0)
    {
      aggloc = locate_agg_of_level((Node *)filter, agglevel);
    }
    ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("aggregate function calls cannot be nested"), parser_errposition(pstate, aggloc)));
  }

     
                                                                         
                                                                            
                                                                     
                                                                     
                                                                         
                                                                     
                                 
     
  if (directargs)
  {
    context.min_varlevel = -1;
    context.min_agglevel = -1;
    (void)check_agg_arguments_walker((Node *)directargs, &context);
    if (context.min_varlevel >= 0 && context.min_varlevel < agglevel)
    {
      ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("outer-level aggregate cannot contain a lower-level variable in its direct arguments"), parser_errposition(pstate, locate_var_of_level((Node *)directargs, context.min_varlevel))));
    }
    if (context.min_agglevel >= 0 && context.min_agglevel <= agglevel)
    {
      ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("aggregate function calls cannot be nested"), parser_errposition(pstate, locate_agg_of_level((Node *)directargs, context.min_agglevel))));
    }
  }
  return agglevel;
}

static bool
check_agg_arguments_walker(Node *node, check_agg_arguments_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    int varlevelsup = ((Var *)node)->varlevelsup;

                                                                  
    varlevelsup -= context->sublevels_up;
                                         
    if (varlevelsup >= 0)
    {
      if (context->min_varlevel < 0 || context->min_varlevel > varlevelsup)
      {
        context->min_varlevel = varlevelsup;
      }
    }
    return false;
  }
  if (IsA(node, Aggref))
  {
    int agglevelsup = ((Aggref *)node)->agglevelsup;

                                                                  
    agglevelsup -= context->sublevels_up;
                                         
    if (agglevelsup >= 0)
    {
      if (context->min_agglevel < 0 || context->min_agglevel > agglevelsup)
      {
        context->min_agglevel = agglevelsup;
      }
    }
                                                        
    return false;
  }
  if (IsA(node, GroupingFunc))
  {
    int agglevelsup = ((GroupingFunc *)node)->agglevelsup;

                                                                  
    agglevelsup -= context->sublevels_up;
                                         
    if (agglevelsup >= 0)
    {
      if (context->min_agglevel < 0 || context->min_agglevel > agglevelsup)
      {
        context->min_agglevel = agglevelsup;
      }
    }
                                           
  }

     
                                                                          
                                                                        
                 
     
  if (context->sublevels_up == 0)
  {
    if ((IsA(node, FuncExpr) && ((FuncExpr *)node)->funcretset) || (IsA(node, OpExpr) && ((OpExpr *)node)->opretset))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("aggregate function calls cannot contain set-returning function calls"), errhint("You might be able to move the set-returning function into a LATERAL FROM item."), parser_errposition(context->pstate, exprLocation(node))));
    }
    if (IsA(node, WindowFunc))
    {
      ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("aggregate function calls cannot contain window function calls"), parser_errposition(context->pstate, ((WindowFunc *)node)->location)));
    }
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, check_agg_arguments_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }

  return expression_tree_walker(node, check_agg_arguments_walker, (void *)context);
}

   
                             
                                                            
   
                                                                              
                                                                            
                                                                               
                                                                               
                                                                        
                                                                      
   
void
transformWindowFuncCall(ParseState *pstate, WindowFunc *wfunc, WindowDef *windef)
{
  const char *err;
  bool errkind;

     
                                                                             
                                                                 
     
                                                                          
                                                                        
                                                                  
     
  if (pstate->p_hasWindowFuncs && contain_windowfuncs((Node *)wfunc->args))
  {
    ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("window function calls cannot be nested"), parser_errposition(pstate, locate_windowfunc((Node *)wfunc->args))));
  }

     
                                                                           
            
     
                                                                         
                                                                           
                                                                             
                                                                          
                                                      
     
  err = NULL;
  errkind = false;
  switch (pstate->p_expr_kind)
  {
  case EXPR_KIND_NONE:
    Assert(false);                   
    break;
  case EXPR_KIND_OTHER:
                                                                    
    break;
  case EXPR_KIND_JOIN_ON:
  case EXPR_KIND_JOIN_USING:
    err = _("window functions are not allowed in JOIN conditions");
    break;
  case EXPR_KIND_FROM_SUBSELECT:
                                                          
    errkind = true;
    break;
  case EXPR_KIND_FROM_FUNCTION:
    err = _("window functions are not allowed in functions in FROM");
    break;
  case EXPR_KIND_WHERE:
    errkind = true;
    break;
  case EXPR_KIND_POLICY:
    err = _("window functions are not allowed in policy expressions");
    break;
  case EXPR_KIND_HAVING:
    errkind = true;
    break;
  case EXPR_KIND_FILTER:
    errkind = true;
    break;
  case EXPR_KIND_WINDOW_PARTITION:
  case EXPR_KIND_WINDOW_ORDER:
  case EXPR_KIND_WINDOW_FRAME_RANGE:
  case EXPR_KIND_WINDOW_FRAME_ROWS:
  case EXPR_KIND_WINDOW_FRAME_GROUPS:
    err = _("window functions are not allowed in window definitions");
    break;
  case EXPR_KIND_SELECT_TARGET:
              
    break;
  case EXPR_KIND_INSERT_TARGET:
  case EXPR_KIND_UPDATE_SOURCE:
  case EXPR_KIND_UPDATE_TARGET:
    errkind = true;
    break;
  case EXPR_KIND_GROUP_BY:
    errkind = true;
    break;
  case EXPR_KIND_ORDER_BY:
              
    break;
  case EXPR_KIND_DISTINCT_ON:
              
    break;
  case EXPR_KIND_LIMIT:
  case EXPR_KIND_OFFSET:
    errkind = true;
    break;
  case EXPR_KIND_RETURNING:
    errkind = true;
    break;
  case EXPR_KIND_VALUES:
  case EXPR_KIND_VALUES_SINGLE:
    errkind = true;
    break;
  case EXPR_KIND_CHECK_CONSTRAINT:
  case EXPR_KIND_DOMAIN_CHECK:
    err = _("window functions are not allowed in check constraints");
    break;
  case EXPR_KIND_COLUMN_DEFAULT:
  case EXPR_KIND_FUNCTION_DEFAULT:
    err = _("window functions are not allowed in DEFAULT expressions");
    break;
  case EXPR_KIND_INDEX_EXPRESSION:
    err = _("window functions are not allowed in index expressions");
    break;
  case EXPR_KIND_INDEX_PREDICATE:
    err = _("window functions are not allowed in index predicates");
    break;
  case EXPR_KIND_ALTER_COL_TRANSFORM:
    err = _("window functions are not allowed in transform expressions");
    break;
  case EXPR_KIND_EXECUTE_PARAMETER:
    err = _("window functions are not allowed in EXECUTE parameters");
    break;
  case EXPR_KIND_TRIGGER_WHEN:
    err = _("window functions are not allowed in trigger WHEN conditions");
    break;
  case EXPR_KIND_PARTITION_BOUND:
    err = _("window functions are not allowed in partition bound");
    break;
  case EXPR_KIND_PARTITION_EXPRESSION:
    err = _("window functions are not allowed in partition key expressions");
    break;
  case EXPR_KIND_CALL_ARGUMENT:
    err = _("window functions are not allowed in CALL arguments");
    break;
  case EXPR_KIND_COPY_WHERE:
    err = _("window functions are not allowed in COPY FROM WHERE conditions");
    break;
  case EXPR_KIND_GENERATED_COLUMN:
    err = _("window functions are not allowed in column generation expressions");
    break;

       
                                                                 
                                                                
                                                                     
                                                                      
                             
       
  }
  if (err)
  {
    ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg_internal("%s", err), parser_errposition(pstate, wfunc->location)));
  }
  if (errkind)
  {
    ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR),
                                                                                   
                       errmsg("window functions are not allowed in %s", ParseExprKindName(pstate->p_expr_kind)), parser_errposition(pstate, wfunc->location)));
  }

     
                                                                       
                                                                            
                                                                             
                      
     
  if (windef->name)
  {
    Index winref = 0;
    ListCell *lc;

    Assert(windef->refname == NULL && windef->partitionClause == NIL && windef->orderClause == NIL && windef->frameOptions == FRAMEOPTION_DEFAULTS);

    foreach (lc, pstate->p_windowdefs)
    {
      WindowDef *refwin = (WindowDef *)lfirst(lc);

      winref++;
      if (refwin->name && strcmp(refwin->name, windef->name) == 0)
      {
        wfunc->winref = winref;
        break;
      }
    }
    if (lc == NULL)                      
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("window \"%s\" does not exist", windef->name), parser_errposition(pstate, windef->location)));
    }
  }
  else
  {
    Index winref = 0;
    ListCell *lc;

    foreach (lc, pstate->p_windowdefs)
    {
      WindowDef *refwin = (WindowDef *)lfirst(lc);

      winref++;
      if (refwin->refname && windef->refname && strcmp(refwin->refname, windef->refname) == 0)
                                ;
      else if (!refwin->refname && !windef->refname)
                                 ;
      else
      {
        continue;
      }
      if (equal(refwin->partitionClause, windef->partitionClause) && equal(refwin->orderClause, windef->orderClause) && refwin->frameOptions == windef->frameOptions && equal(refwin->startOffset, windef->startOffset) && equal(refwin->endOffset, windef->endOffset))
      {
                                                    
        wfunc->winref = winref;
        break;
      }
    }
    if (lc == NULL)                      
    {
      pstate->p_windowdefs = lappend(pstate->p_windowdefs, windef);
      wfunc->winref = list_length(pstate->p_windowdefs);
    }
  }

  pstate->p_hasWindowFuncs = true;
}

   
                        
                                                                       
                                                                           
                  
   
                                                                           
                                                                         
                                                                        
                                                                          
                   
   
void
parseCheckAggregates(ParseState *pstate, Query *qry)
{
  List *gset_common = NIL;
  List *groupClauses = NIL;
  List *groupClauseCommonVars = NIL;
  bool have_non_var_grouping;
  List *func_grouped_rels = NIL;
  ListCell *l;
  bool hasJoinRTEs;
  bool hasSelfRefRTEs;
  Node *clause;

                                                                     
  Assert(pstate->p_hasAggs || qry->groupClause || qry->havingQual || qry->groupingSets);

     
                                                                            
           
     
  if (qry->groupingSets)
  {
       
                                                                          
                                            
       
    List *gsets = expand_grouping_sets(qry->groupingSets, 4096);

    if (!gsets)
    {
      ereport(ERROR, (errcode(ERRCODE_STATEMENT_TOO_COMPLEX), errmsg("too many grouping sets present (maximum 4096)"), parser_errposition(pstate, qry->groupClause ? exprLocation((Node *)qry->groupClause) : exprLocation((Node *)qry->groupingSets))));
    }

       
                                                                     
                                                    
       
    gset_common = linitial(gsets);

    if (gset_common)
    {
      for_each_cell(l, lnext(list_head(gsets)))
      {
        gset_common = list_intersection_int(gset_common, lfirst(l));
        if (!gset_common)
        {
          break;
        }
      }
    }

       
                                                                       
                                                                      
                                                                        
                                   
       
    if (list_length(gsets) == 1 && qry->groupClause)
    {
      qry->groupingSets = NIL;
    }
  }

     
                                                                         
                                           
     
  hasJoinRTEs = hasSelfRefRTEs = false;
  foreach (l, pstate->p_rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);

    if (rte->rtekind == RTE_JOIN)
    {
      hasJoinRTEs = true;
    }
    else if (rte->rtekind == RTE_CTE && rte->self_reference)
    {
      hasSelfRefRTEs = true;
    }
  }

     
                                                                    
                                
     
                                                                           
                   
     
  foreach (l, qry->groupClause)
  {
    SortGroupClause *grpcl = (SortGroupClause *)lfirst(l);
    TargetEntry *expr;

    expr = get_sortgroupclause_tle(grpcl, qry->targetList);
    if (expr == NULL)
    {
      continue;                             
    }

    groupClauses = lcons(expr, groupClauses);
  }

     
                                                                           
                                                                           
                                                                             
                                
     
  if (hasJoinRTEs)
  {
    groupClauses = (List *)flatten_join_alias_vars(qry, (Node *)groupClauses);
  }

     
                                                                           
                                                                          
                                                            
     
                                                                     
                                                                        
                                        
     
  have_non_var_grouping = false;
  foreach (l, groupClauses)
  {
    TargetEntry *tle = lfirst(l);

    if (!IsA(tle->expr, Var))
    {
      have_non_var_grouping = true;
    }
    else if (!qry->groupingSets || list_member_int(gset_common, tle->ressortgroupref))
    {
      groupClauseCommonVars = lappend(groupClauseCommonVars, tle->expr);
    }
  }

     
                                                                     
     
                                                                            
                                                                         
                                                                      
                                                                           
     
                                                                             
                                                                 
     
  clause = (Node *)qry->targetList;
  finalize_grouping_exprs(clause, pstate, qry, groupClauses, hasJoinRTEs, have_non_var_grouping);
  if (hasJoinRTEs)
  {
    clause = flatten_join_alias_vars(qry, clause);
  }
  check_ungrouped_columns(clause, pstate, qry, groupClauses, groupClauseCommonVars, have_non_var_grouping, &func_grouped_rels);

  clause = (Node *)qry->havingQual;
  finalize_grouping_exprs(clause, pstate, qry, groupClauses, hasJoinRTEs, have_non_var_grouping);
  if (hasJoinRTEs)
  {
    clause = flatten_join_alias_vars(qry, clause);
  }
  check_ungrouped_columns(clause, pstate, qry, groupClauses, groupClauseCommonVars, have_non_var_grouping, &func_grouped_rels);

     
                                                            
     
  if (pstate->p_hasAggs && hasSelfRefRTEs)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_RECURSION), errmsg("aggregate functions are not allowed in a recursive query's recursive term"), parser_errposition(pstate, locate_agg_of_level((Node *)qry, 0))));
  }
}

   
                             
                                                                       
                                                                     
                                                                           
                       
   
                                                                           
                                                                 
   
                                                                       
                                                                     
                                 
           
                                                   
             
                    
                                                                     
                                                                       
                                               
   
static void
check_ungrouped_columns(Node *node, ParseState *pstate, Query *qry, List *groupClauses, List *groupClauseCommonVars, bool have_non_var_grouping, List **func_grouped_rels)
{
  check_ungrouped_columns_context context;

  context.pstate = pstate;
  context.qry = qry;
  context.hasJoinRTEs = false;                                        
  context.groupClauses = groupClauses;
  context.groupClauseCommonVars = groupClauseCommonVars;
  context.have_non_var_grouping = have_non_var_grouping;
  context.func_grouped_rels = func_grouped_rels;
  context.sublevels_up = 0;
  context.in_agg_direct_args = false;
  check_ungrouped_columns_walker(node, &context);
}

static bool
check_ungrouped_columns_walker(Node *node, check_ungrouped_columns_context *context)
{
  ListCell *gl;

  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Const) || IsA(node, Param))
  {
    return false;                                      
  }

  if (IsA(node, Aggref))
  {
    Aggref *agg = (Aggref *)node;

    if ((int)agg->agglevelsup == context->sublevels_up)
    {
         
                                                                    
                                                                   
                                                                       
                                                                        
                                                                       
                                                               
         
      bool result;

      Assert(!context->in_agg_direct_args);
      context->in_agg_direct_args = true;
      result = check_ungrouped_columns_walker((Node *)agg->aggdirectargs, context);
      context->in_agg_direct_args = false;
      return result;
    }

       
                                                                          
                                                                        
                                                                           
                        
       
    if ((int)agg->agglevelsup > context->sublevels_up)
    {
      return false;
    }
  }

  if (IsA(node, GroupingFunc))
  {
    GroupingFunc *grp = (GroupingFunc *)node;

                                                                           

    if ((int)grp->agglevelsup >= context->sublevels_up)
    {
      return false;
    }
  }

     
                                                                             
                                                                            
                                                                          
                                                                             
                                  
     
  if (context->have_non_var_grouping && context->sublevels_up == 0)
  {
    foreach (gl, context->groupClauses)
    {
      TargetEntry *tle = lfirst(gl);

      if (equal(node, tle->expr))
      {
        return false;                                      
      }
    }
  }

     
                                                                        
                                                                          
                                                                            
                                                                           
     
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;
    RangeTblEntry *rte;
    char *attname;

    if (var->varlevelsup != context->sublevels_up)
    {
      return false;                                         
    }

       
                                                    
       
    if (!context->have_non_var_grouping || context->sublevels_up != 0)
    {
      foreach (gl, context->groupClauses)
      {
        Var *gvar = (Var *)((TargetEntry *)lfirst(gl))->expr;

        if (IsA(gvar, Var) && gvar->varno == var->varno && gvar->varattno == var->varattno && gvar->varlevelsup == 0)
        {
          return false;                             
        }
      }
    }

       
                                                                          
                                                                        
                                                                           
                                                                          
                                                                          
                                                                        
                                                                           
                                                                
       
                                                                        
                                                                        
                                                                         
                                                                          
                                
       
    if (list_member_int(*context->func_grouped_rels, var->varno))
    {
      return false;                                   
    }

    Assert(var->varno > 0 && (int)var->varno <= list_length(context->pstate->p_rtable));
    rte = rt_fetch(var->varno, context->pstate->p_rtable);
    if (rte->rtekind == RTE_RELATION)
    {
      if (check_functional_grouping(rte->relid, var->varno, 0, context->groupClauseCommonVars, &context->qry->constraintDeps))
      {
        *context->func_grouped_rels = lappend_int(*context->func_grouped_rels, var->varno);
        return false;                 
      }
    }

                                                                   
    attname = get_rte_attribute_name(rte, var->varattno);
    if (context->sublevels_up == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("column \"%s.%s\" must appear in the GROUP BY clause or be used in an aggregate function", rte->eref->aliasname, attname), context->in_agg_direct_args ? errdetail("Direct arguments of an ordered-set aggregate must use only grouped columns.") : 0, parser_errposition(context->pstate, var->location)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("subquery uses ungrouped column \"%s.%s\" from outer query", rte->eref->aliasname, attname), parser_errposition(context->pstate, var->location)));
    }
  }

  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, check_ungrouped_columns_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, check_ungrouped_columns_walker, (void *)context);
}

   
                             
                                                                      
                                               
   
                                                                         
                                                                         
                                                                         
                                                                      
                                                       
   
static void
finalize_grouping_exprs(Node *node, ParseState *pstate, Query *qry, List *groupClauses, bool hasJoinRTEs, bool have_non_var_grouping)
{
  check_ungrouped_columns_context context;

  context.pstate = pstate;
  context.qry = qry;
  context.hasJoinRTEs = hasJoinRTEs;
  context.groupClauses = groupClauses;
  context.groupClauseCommonVars = NIL;
  context.have_non_var_grouping = have_non_var_grouping;
  context.func_grouped_rels = NULL;
  context.sublevels_up = 0;
  context.in_agg_direct_args = false;
  finalize_grouping_exprs_walker(node, &context);
}

static bool
finalize_grouping_exprs_walker(Node *node, check_ungrouped_columns_context *context)
{
  ListCell *gl;

  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Const) || IsA(node, Param))
  {
    return false;                                      
  }

  if (IsA(node, Aggref))
  {
    Aggref *agg = (Aggref *)node;

    if ((int)agg->agglevelsup == context->sublevels_up)
    {
         
                                                                    
                                                                   
                                                                         
                                                                        
         
      bool result;

      Assert(!context->in_agg_direct_args);
      context->in_agg_direct_args = true;
      result = finalize_grouping_exprs_walker((Node *)agg->aggdirectargs, context);
      context->in_agg_direct_args = false;
      return result;
    }

       
                                                                          
                                                                         
                                                                           
                        
       
    if ((int)agg->agglevelsup > context->sublevels_up)
    {
      return false;
    }
  }

  if (IsA(node, GroupingFunc))
  {
    GroupingFunc *grp = (GroupingFunc *)node;

       
                                                                      
                                                                     
       

    if ((int)grp->agglevelsup == context->sublevels_up)
    {
      ListCell *lc;
      List *ref_list = NIL;

      foreach (lc, grp->args)
      {
        Node *expr = lfirst(lc);
        Index ref = 0;

        if (context->hasJoinRTEs)
        {
          expr = flatten_join_alias_vars(context->qry, expr);
        }

           
                                                                      
                                                                     
                                                              
           

        if (IsA(expr, Var))
        {
          Var *var = (Var *)expr;

          if (var->varlevelsup == context->sublevels_up)
          {
            foreach (gl, context->groupClauses)
            {
              TargetEntry *tle = lfirst(gl);
              Var *gvar = (Var *)tle->expr;

              if (IsA(gvar, Var) && gvar->varno == var->varno && gvar->varattno == var->varattno && gvar->varlevelsup == 0)
              {
                ref = tle->ressortgroupref;
                break;
              }
            }
          }
        }
        else if (context->have_non_var_grouping && context->sublevels_up == 0)
        {
          foreach (gl, context->groupClauses)
          {
            TargetEntry *tle = lfirst(gl);

            if (equal(expr, tle->expr))
            {
              ref = tle->ressortgroupref;
              break;
            }
          }
        }

        if (ref == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR), errmsg("arguments to GROUPING must be grouping expressions of the associated query level"), parser_errposition(context->pstate, exprLocation(expr))));
        }

        ref_list = lappend_int(ref_list, ref);
      }

      grp->refs = ref_list;
    }

    if ((int)grp->agglevelsup > context->sublevels_up)
    {
      return false;
    }
  }

  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, finalize_grouping_exprs_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, finalize_grouping_exprs_walker, (void *)context);
}

   
                                                                   
   
                                                     
   
                                                                           
   
                                                               
   
                                                                
   
static List *
expand_groupingset_node(GroupingSet *gs)
{
  List *result = NIL;

  switch (gs->kind)
  {
  case GROUPING_SET_EMPTY:
    result = list_make1(NIL);
    break;

  case GROUPING_SET_SIMPLE:
    result = list_make1(gs->content);
    break;

  case GROUPING_SET_ROLLUP:
  {
    List *rollup_val = gs->content;
    ListCell *lc;
    int curgroup_size = list_length(gs->content);

    while (curgroup_size > 0)
    {
      List *current_result = NIL;
      int i = curgroup_size;

      foreach (lc, rollup_val)
      {
        GroupingSet *gs_current = (GroupingSet *)lfirst(lc);

        Assert(gs_current->kind == GROUPING_SET_SIMPLE);

        current_result = list_concat(current_result, list_copy(gs_current->content));

                                                                 
        if (--i == 0)
        {
          break;
        }
      }

      result = lappend(result, current_result);
      --curgroup_size;
    }

    result = lappend(result, NIL);
  }
  break;

  case GROUPING_SET_CUBE:
  {
    List *cube_list = gs->content;
    int number_bits = list_length(cube_list);
    uint32 num_sets;
    uint32 i;

                                           
    Assert(number_bits < 31);

    num_sets = (1U << number_bits);

    for (i = 0; i < num_sets; i++)
    {
      List *current_result = NIL;
      ListCell *lc;
      uint32 mask = 1U;

      foreach (lc, cube_list)
      {
        GroupingSet *gs_current = (GroupingSet *)lfirst(lc);

        Assert(gs_current->kind == GROUPING_SET_SIMPLE);

        if (mask & i)
        {
          current_result = list_concat(current_result, list_copy(gs_current->content));
        }

        mask <<= 1;
      }

      result = lappend(result, current_result);
    }
  }
  break;

  case GROUPING_SET_SETS:
  {
    ListCell *lc;

    foreach (lc, gs->content)
    {
      List *current_result = expand_groupingset_node(lfirst(lc));

      result = list_concat(result, current_result);
    }
  }
  break;
  }

  return result;
}

static int
cmp_list_len_asc(const void *a, const void *b)
{
  int la = list_length(*(List *const *)a);
  int lb = list_length(*(List *const *)b);

  return (la > lb) ? 1 : (la == lb) ? 0 : -1;
}

   
                                                                 
                                                               
   
                                                                
                            
   
List *
expand_grouping_sets(List *groupingSets, int limit)
{
  List *expanded_groups = NIL;
  List *result = NIL;
  double numsets = 1;
  ListCell *lc;

  if (groupingSets == NIL)
  {
    return NIL;
  }

  foreach (lc, groupingSets)
  {
    List *current_result = NIL;
    GroupingSet *gs = lfirst(lc);

    current_result = expand_groupingset_node(gs);

    Assert(current_result != NIL);

    numsets *= list_length(current_result);

    if (limit >= 0 && numsets > limit)
    {
      return NIL;
    }

    expanded_groups = lappend(expanded_groups, current_result);
  }

     
                                                                            
                                                                          
                                           
     

  foreach (lc, (List *)linitial(expanded_groups))
  {
    result = lappend(result, list_union_int(NIL, (List *)lfirst(lc)));
  }

  for_each_cell(lc, lnext(list_head(expanded_groups)))
  {
    List *p = lfirst(lc);
    List *new_result = NIL;
    ListCell *lc2;

    foreach (lc2, result)
    {
      List *q = lfirst(lc2);
      ListCell *lc3;

      foreach (lc3, p)
      {
        new_result = lappend(new_result, list_union_int(q, (List *)lfirst(lc3)));
      }
    }
    result = new_result;
  }

  if (list_length(result) > 1)
  {
    int result_len = list_length(result);
    List **buf = palloc(sizeof(List *) * result_len);
    List **ptr = buf;

    foreach (lc, result)
    {
      *ptr++ = lfirst(lc);
    }

    qsort(buf, result_len, sizeof(List *), cmp_list_len_asc);

    result = NIL;
    ptr = buf;

    while (result_len-- > 0)
    {
      result = lappend(result, *ptr++);
    }

    pfree(buf);
  }

  return result;
}

   
                          
                                                                
   
                                                                         
                                                                      
                                                                         
                                                                           
                             
   
                                                                           
                            
   
                                                          
   
int
get_aggregate_argtypes(Aggref *aggref, Oid *inputTypes)
{
  int numArguments = 0;
  ListCell *lc;

  Assert(list_length(aggref->aggargtypes) <= FUNC_MAX_ARGS);

  foreach (lc, aggref->aggargtypes)
  {
    inputTypes[numArguments++] = lfirst_oid(lc);
  }

  return numArguments;
}

   
                               
                                                                         
   
                                                                    
                                                                          
                                                                             
                                                                          
                                                                     
   
Oid
resolve_aggregate_transtype(Oid aggfuncid, Oid aggtranstype, Oid *inputTypes, int numArguments)
{
                                                               
  if (IsPolymorphicType(aggtranstype))
  {
                                                         
    Oid *declaredArgTypes;
    int agg_nargs;

    (void)get_func_signature(aggfuncid, &declaredArgTypes, &agg_nargs);

       
                                                                        
                                                                 
       
    Assert(agg_nargs <= numArguments);

    aggtranstype = enforce_generic_type_consistency(inputTypes, declaredArgTypes, agg_nargs, aggtranstype, false);
    pfree(declaredArgTypes);
  }
  return aggtranstype;
}

   
                                                                          
                                                                      
                                                                            
                                                                           
                                                                
   
                                                                        
                                                                         
                            
                                                                    
   
                                                                         
                                                              
   
                                                                       
                                                                   
                                 
   
                                                                     
                                                                             
                                                                             
                   
   
void
build_aggregate_transfn_expr(Oid *agg_input_types, int agg_num_inputs, int agg_num_direct_inputs, bool agg_variadic, Oid agg_state_type, Oid agg_input_collation, Oid transfn_oid, Oid invtransfn_oid, Expr **transfnexpr, Expr **invtransfnexpr)
{
  List *args;
  FuncExpr *fexpr;
  int i;

     
                                                         
     
  args = list_make1(make_agg_arg(agg_state_type, agg_input_collation));

  for (i = agg_num_direct_inputs; i < agg_num_inputs; i++)
  {
    args = lappend(args, make_agg_arg(agg_input_types[i], agg_input_collation));
  }

  fexpr = makeFuncExpr(transfn_oid, agg_state_type, args, InvalidOid, agg_input_collation, COERCE_EXPLICIT_CALL);
  fexpr->funcvariadic = agg_variadic;
  *transfnexpr = (Expr *)fexpr;

     
                                                                         
     
  if (invtransfnexpr != NULL)
  {
    if (OidIsValid(invtransfn_oid))
    {
      fexpr = makeFuncExpr(invtransfn_oid, agg_state_type, args, InvalidOid, agg_input_collation, COERCE_EXPLICIT_CALL);
      fexpr->funcvariadic = agg_variadic;
      *invtransfnexpr = (Expr *)fexpr;
    }
    else
    {
      *invtransfnexpr = NULL;
    }
  }
}

   
                                                                             
                                                                          
   
void
build_aggregate_combinefn_expr(Oid agg_state_type, Oid agg_input_collation, Oid combinefn_oid, Expr **combinefnexpr)
{
  Node *argp;
  List *args;
  FuncExpr *fexpr;

                                                                 
  argp = make_agg_arg(agg_state_type, agg_input_collation);

  args = list_make2(argp, argp);

  fexpr = makeFuncExpr(combinefn_oid, agg_state_type, args, InvalidOid, agg_input_collation, COERCE_EXPLICIT_CALL);
                                                        
  *combinefnexpr = (Expr *)fexpr;
}

   
                                                                             
                                           
   
void
build_aggregate_serialfn_expr(Oid serialfn_oid, Expr **serialfnexpr)
{
  List *args;
  FuncExpr *fexpr;

                                                        
  args = list_make1(make_agg_arg(INTERNALOID, InvalidOid));

  fexpr = makeFuncExpr(serialfn_oid, BYTEAOID, args, InvalidOid, InvalidOid, COERCE_EXPLICIT_CALL);
  *serialfnexpr = (Expr *)fexpr;
}

   
                                                                             
                                             
   
void
build_aggregate_deserialfn_expr(Oid deserialfn_oid, Expr **deserialfnexpr)
{
  List *args;
  FuncExpr *fexpr;

                                                                    
  args = list_make2(make_agg_arg(BYTEAOID, InvalidOid), make_agg_arg(INTERNALOID, InvalidOid));

  fexpr = makeFuncExpr(deserialfn_oid, INTERNALOID, args, InvalidOid, InvalidOid, COERCE_EXPLICIT_CALL);
  *deserialfnexpr = (Expr *)fexpr;
}

   
                                                                             
                                                                        
   
void
build_aggregate_finalfn_expr(Oid *agg_input_types, int num_finalfn_inputs, Oid agg_state_type, Oid agg_result_type, Oid agg_input_collation, Oid finalfn_oid, Expr **finalfnexpr)
{
  List *args;
  int i;

     
                                        
     
  args = list_make1(make_agg_arg(agg_state_type, agg_input_collation));

                                                                       
  for (i = 0; i < num_finalfn_inputs - 1; i++)
  {
    args = lappend(args, make_agg_arg(agg_input_types[i], agg_input_collation));
  }

  *finalfnexpr = (Expr *)makeFuncExpr(finalfn_oid, agg_result_type, args, InvalidOid, agg_input_collation, COERCE_EXPLICIT_CALL);
                                                      
}

   
                                                                            
   
                                                                           
                                                                              
                                                               
   
static Node *
make_agg_arg(Oid argtype, Oid argcollation)
{
  Param *argp = makeNode(Param);

  argp->paramkind = PARAM_EXEC;
  argp->paramid = -1;
  argp->paramtype = argtype;
  argp->paramtypmod = -1;
  argp->paramcollid = argcollation;
  argp->location = -1;
  return (Node *)argp;
}
