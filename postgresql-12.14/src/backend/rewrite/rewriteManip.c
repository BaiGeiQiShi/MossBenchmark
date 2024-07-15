                                                                            
   
                  
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"

typedef struct
{
  int sublevels_up;
} contain_aggs_of_level_context;

typedef struct
{
  int agg_location;
  int sublevels_up;
} locate_agg_of_level_context;

typedef struct
{
  int win_location;
} locate_windowfunc_context;

static bool
contain_aggs_of_level_walker(Node *node, contain_aggs_of_level_context *context);
static bool
locate_agg_of_level_walker(Node *node, locate_agg_of_level_context *context);
static bool
contain_windowfuncs_walker(Node *node, void *context);
static bool
locate_windowfunc_walker(Node *node, locate_windowfunc_context *context);
static bool
checkExprHasSubLink_walker(Node *node, void *context);
static Relids
offset_relid_set(Relids relids, int offset);
static Relids
adjust_relid_set(Relids relids, int oldrelid, int newrelid);

   
                           
                                                                   
                          
   
                                                                           
                                                                           
                                                                      
                                                                            
                              
   
bool
contain_aggs_of_level(Node *node, int levelsup)
{
  contain_aggs_of_level_context context;

  context.sublevels_up = levelsup;

     
                                                                          
                                                            
     
  return query_or_expression_tree_walker(node, contain_aggs_of_level_walker, (void *)&context, 0);
}

static bool
contain_aggs_of_level_walker(Node *node, contain_aggs_of_level_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
    if (((Aggref *)node)->agglevelsup == context->sublevels_up)
    {
      return true;                                               
    }
                                               
  }
  if (IsA(node, GroupingFunc))
  {
    if (((GroupingFunc *)node)->agglevelsup == context->sublevels_up)
    {
      return true;
    }
                                               
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, contain_aggs_of_level_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, contain_aggs_of_level_walker, (void *)context);
}

   
                         
                                                                            
   
                                                                      
                                                                       
                                                                
   
                                                                    
                                                                         
                                                                      
                                                        
   
int
locate_agg_of_level(Node *node, int levelsup)
{
  locate_agg_of_level_context context;

  context.agg_location = -1;                              
  context.sublevels_up = levelsup;

     
                                                                          
                                                            
     
  (void)query_or_expression_tree_walker(node, locate_agg_of_level_walker, (void *)&context, 0);

  return context.agg_location;
}

static bool
locate_agg_of_level_walker(Node *node, locate_agg_of_level_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
    if (((Aggref *)node)->agglevelsup == context->sublevels_up && ((Aggref *)node)->location >= 0)
    {
      context->agg_location = ((Aggref *)node)->location;
      return true;                                               
    }
                                               
  }
  if (IsA(node, GroupingFunc))
  {
    if (((GroupingFunc *)node)->agglevelsup == context->sublevels_up && ((GroupingFunc *)node)->location >= 0)
    {
      context->agg_location = ((GroupingFunc *)node)->location;
      return true;                                               
    }
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, locate_agg_of_level_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, locate_agg_of_level_walker, (void *)context);
}

   
                         
                                                                 
                        
   
bool
contain_windowfuncs(Node *node)
{
     
                                                                          
                                                            
     
  return query_or_expression_tree_walker(node, contain_windowfuncs_walker, NULL, 0);
}

static bool
contain_windowfuncs_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, WindowFunc))
  {
    return true;                                               
  }
                                       
  return expression_tree_walker(node, contain_windowfuncs_walker, (void *)context);
}

   
                       
                                                                           
   
                                                                             
                                                                       
                                                                
   
                                                                    
                                                                       
                                                                      
                                                        
   
int
locate_windowfunc(Node *node)
{
  locate_windowfunc_context context;

  context.win_location = -1;                              

     
                                                                          
                                                            
     
  (void)query_or_expression_tree_walker(node, locate_windowfunc_walker, (void *)&context, 0);

  return context.win_location;
}

static bool
locate_windowfunc_walker(Node *node, locate_windowfunc_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, WindowFunc))
  {
    if (((WindowFunc *)node)->location >= 0)
    {
      context->win_location = ((WindowFunc *)node)->location;
      return true;                                               
    }
                                               
  }
                                       
  return expression_tree_walker(node, locate_windowfunc_walker, (void *)context);
}

   
                         
                                              
   
bool
checkExprHasSubLink(Node *node)
{
     
                                                                         
                                                         
     
  return query_or_expression_tree_walker(node, checkExprHasSubLink_walker, NULL, QTW_IGNORE_RC_SUBQUERIES);
}

static bool
checkExprHasSubLink_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, SubLink))
  {
    return true;                                               
  }
  return expression_tree_walker(node, checkExprHasSubLink_walker, context);
}

   
                                                    
   
                                                                            
                                
   
static bool
contains_multiexpr_param(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    if (((Param *)node)->paramkind == PARAM_MULTIEXPR)
    {
      return true;                                               
    }
    return false;
  }
  return expression_tree_walker(node, contains_multiexpr_param, context);
}

   
                                                                         
   
                                                                          
                                                                      
                                                                         
                                                                      
   
                                                                         
                                                                      
                                                          
   

typedef struct
{
  int offset;
  int sublevels_up;
} OffsetVarNodes_context;

static bool
OffsetVarNodes_walker(Node *node, OffsetVarNodes_context *context)
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
      var->varno += context->offset;
      var->varnoold += context->offset;
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)node;

    if (context->sublevels_up == 0)
    {
      cexpr->cvarno += context->offset;
    }
    return false;
  }
  if (IsA(node, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)node;

    if (context->sublevels_up == 0)
    {
      rtr->rtindex += context->offset;
    }
                                                   
    return false;
  }
  if (IsA(node, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)node;

    if (j->rtindex && context->sublevels_up == 0)
    {
      j->rtindex += context->offset;
    }
                                          
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    if (phv->phlevelsup == context->sublevels_up)
    {
      phv->phrels = offset_relid_set(phv->phrels, context->offset);
    }
                                          
  }
  if (IsA(node, AppendRelInfo))
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)node;

    if (context->sublevels_up == 0)
    {
      appinfo->parent_relid += context->offset;
      appinfo->child_relid += context->offset;
    }
                                          
  }
                                                                   
  Assert(!IsA(node, PlanRowMark));
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, OffsetVarNodes_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, OffsetVarNodes_walker, (void *)context);
}

void
OffsetVarNodes(Node *node, int offset, int sublevels_up)
{
  OffsetVarNodes_context context;

  context.offset = offset;
  context.sublevels_up = sublevels_up;

     
                                                                          
                                                                      
                                                       
     
  if (node && IsA(node, Query))
  {
    Query *qry = (Query *)node;

       
                                                                        
                                                                       
                                                                        
                                                                         
                                                            
       
    if (sublevels_up == 0)
    {
      ListCell *l;

      if (qry->resultRelation)
      {
        qry->resultRelation += offset;
      }

      if (qry->onConflict && qry->onConflict->exclRelIndex)
      {
        qry->onConflict->exclRelIndex += offset;
      }

      foreach (l, qry->rowMarks)
      {
        RowMarkClause *rc = (RowMarkClause *)lfirst(l);

        rc->rti += offset;
      }
    }
    query_tree_walker(qry, OffsetVarNodes_walker, (void *)&context, 0);
  }
  else
  {
    OffsetVarNodes_walker(node, &context);
  }
}

static Relids
offset_relid_set(Relids relids, int offset)
{
  Relids result = NULL;
  int rtindex;

  rtindex = -1;
  while ((rtindex = bms_next_member(relids, rtindex)) >= 0)
  {
    result = bms_add_member(result, rtindex + offset);
  }
  return result;
}

   
                                                                       
   
                                                                         
                                                                            
                                                                             
                                                                            
   
                                                                         
                                                                      
                                                          
   

typedef struct
{
  int rt_index;
  int new_index;
  int sublevels_up;
} ChangeVarNodes_context;

static bool
ChangeVarNodes_walker(Node *node, ChangeVarNodes_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == context->sublevels_up && var->varno == context->rt_index)
    {
      var->varno = context->new_index;
      var->varnoold = context->new_index;
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)node;

    if (context->sublevels_up == 0 && cexpr->cvarno == context->rt_index)
    {
      cexpr->cvarno = context->new_index;
    }
    return false;
  }
  if (IsA(node, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)node;

    if (context->sublevels_up == 0 && rtr->rtindex == context->rt_index)
    {
      rtr->rtindex = context->new_index;
    }
                                                   
    return false;
  }
  if (IsA(node, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)node;

    if (context->sublevels_up == 0 && j->rtindex == context->rt_index)
    {
      j->rtindex = context->new_index;
    }
                                          
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    if (phv->phlevelsup == context->sublevels_up)
    {
      phv->phrels = adjust_relid_set(phv->phrels, context->rt_index, context->new_index);
    }
                                          
  }
  if (IsA(node, PlanRowMark))
  {
    PlanRowMark *rowmark = (PlanRowMark *)node;

    if (context->sublevels_up == 0)
    {
      if (rowmark->rti == context->rt_index)
      {
        rowmark->rti = context->new_index;
      }
      if (rowmark->prti == context->rt_index)
      {
        rowmark->prti = context->new_index;
      }
    }
    return false;
  }
  if (IsA(node, AppendRelInfo))
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)node;

    if (context->sublevels_up == 0)
    {
      if (appinfo->parent_relid == context->rt_index)
      {
        appinfo->parent_relid = context->new_index;
      }
      if (appinfo->child_relid == context->rt_index)
      {
        appinfo->child_relid = context->new_index;
      }
    }
                                          
  }
                                                                   
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, ChangeVarNodes_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, ChangeVarNodes_walker, (void *)context);
}

void
ChangeVarNodes(Node *node, int rt_index, int new_index, int sublevels_up)
{
  ChangeVarNodes_context context;

  context.rt_index = rt_index;
  context.new_index = new_index;
  context.sublevels_up = sublevels_up;

     
                                                                          
                                                                      
                                                       
     
  if (node && IsA(node, Query))
  {
    Query *qry = (Query *)node;

       
                                                                        
                                                                       
                                                                         
                                                                           
                                           
       
    if (sublevels_up == 0)
    {
      ListCell *l;

      if (qry->resultRelation == rt_index)
      {
        qry->resultRelation = new_index;
      }

                                                     
      if (qry->onConflict && qry->onConflict->exclRelIndex == rt_index)
      {
        qry->onConflict->exclRelIndex = new_index;
      }

      foreach (l, qry->rowMarks)
      {
        RowMarkClause *rc = (RowMarkClause *)lfirst(l);

        if (rc->rti == rt_index)
        {
          rc->rti = new_index;
        }
      }
    }
    query_tree_walker(qry, ChangeVarNodes_walker, (void *)&context, 0);
  }
  else
  {
    ChangeVarNodes_walker(node, &context);
  }
}

   
                                                   
   
static Relids
adjust_relid_set(Relids relids, int oldrelid, int newrelid)
{
  if (bms_is_member(oldrelid, relids))
  {
                                          
    relids = bms_copy(relids);
                             
    relids = bms_del_member(relids, oldrelid);
    relids = bms_add_member(relids, newrelid);
  }
  return relids;
}

   
                                                                             
   
                                                                                
                                                                               
                                                                          
                                                                            
                                                                        
                                                                           
                                                                            
                                                                
   
                                                                        
   
                                                                         
                                                                          
                                                          
   

typedef struct
{
  int delta_sublevels_up;
  int min_sublevels_up;
} IncrementVarSublevelsUp_context;

static bool
IncrementVarSublevelsUp_walker(Node *node, IncrementVarSublevelsUp_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup >= context->min_sublevels_up)
    {
      var->varlevelsup += context->delta_sublevels_up;
    }
    return false;                
  }
  if (IsA(node, CurrentOfExpr))
  {
                                
    if (context->min_sublevels_up == 0)
    {
      elog(ERROR, "cannot push down CurrentOfExpr");
    }
    return false;
  }
  if (IsA(node, Aggref))
  {
    Aggref *agg = (Aggref *)node;

    if (agg->agglevelsup >= context->min_sublevels_up)
    {
      agg->agglevelsup += context->delta_sublevels_up;
    }
                                               
  }
  if (IsA(node, GroupingFunc))
  {
    GroupingFunc *grp = (GroupingFunc *)node;

    if (grp->agglevelsup >= context->min_sublevels_up)
    {
      grp->agglevelsup += context->delta_sublevels_up;
    }
                                               
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    if (phv->phlevelsup >= context->min_sublevels_up)
    {
      phv->phlevelsup += context->delta_sublevels_up;
    }
                                               
  }
  if (IsA(node, RangeTblEntry))
  {
    RangeTblEntry *rte = (RangeTblEntry *)node;

    if (rte->rtekind == RTE_CTE)
    {
      if (rte->ctelevelsup >= context->min_sublevels_up)
      {
        rte->ctelevelsup += context->delta_sublevels_up;
      }
    }
    return false;                                           
  }
  if (IsA(node, Query))
  {
                                 
    bool result;

    context->min_sublevels_up++;
    result = query_tree_walker((Query *)node, IncrementVarSublevelsUp_walker, (void *)context, QTW_EXAMINE_RTES_BEFORE);
    context->min_sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, IncrementVarSublevelsUp_walker, (void *)context);
}

void
IncrementVarSublevelsUp(Node *node, int delta_sublevels_up, int min_sublevels_up)
{
  IncrementVarSublevelsUp_context context;

  context.delta_sublevels_up = delta_sublevels_up;
  context.min_sublevels_up = min_sublevels_up;

     
                                                                          
                                                            
     
  query_or_expression_tree_walker(node, IncrementVarSublevelsUp_walker, (void *)&context, QTW_EXAMINE_RTES_BEFORE);
}

   
                                    
                                                                        
   
void
IncrementVarSublevelsUp_rtable(List *rtable, int delta_sublevels_up, int min_sublevels_up)
{
  IncrementVarSublevelsUp_context context;

  context.delta_sublevels_up = delta_sublevels_up;
  context.min_sublevels_up = min_sublevels_up;

  range_table_walker(rtable, IncrementVarSublevelsUp_walker, (void *)&context, QTW_EXAMINE_RTES_BEFORE);
}

   
                                                                        
                                                                 
   

typedef struct
{
  int rt_index;
  int sublevels_up;
} rangeTableEntry_used_context;

static bool
rangeTableEntry_used_walker(Node *node, rangeTableEntry_used_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varlevelsup == context->sublevels_up && var->varno == context->rt_index)
    {
      return true;
    }
    return false;
  }
  if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)node;

    if (context->sublevels_up == 0 && cexpr->cvarno == context->rt_index)
    {
      return true;
    }
    return false;
  }
  if (IsA(node, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)node;

    if (rtr->rtindex == context->rt_index && context->sublevels_up == 0)
    {
      return true;
    }
                                                   
    return false;
  }
  if (IsA(node, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)node;

    if (j->rtindex == context->rt_index && context->sublevels_up == 0)
    {
      return true;
    }
                                          
  }
                                                             
  Assert(!IsA(node, PlaceHolderVar));
  Assert(!IsA(node, PlanRowMark));
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, AppendRelInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

  if (IsA(node, Query))
  {
                                 
    bool result;

    context->sublevels_up++;
    result = query_tree_walker((Query *)node, rangeTableEntry_used_walker, (void *)context, 0);
    context->sublevels_up--;
    return result;
  }
  return expression_tree_walker(node, rangeTableEntry_used_walker, (void *)context);
}

bool
rangeTableEntry_used(Node *node, int rt_index, int sublevels_up)
{
  rangeTableEntry_used_context context;

  context.rt_index = rt_index;
  context.sublevels_up = sublevels_up;

     
                                                                          
                                                            
     
  return query_or_expression_tree_walker(node, rangeTableEntry_used_walker, (void *)&context, 0);
}

   
                                                                     
                                                                         
                           
   
                                                                          
                                                                          
                      
   
                                                                            
                                                                             
                                                                            
   
Query *
getInsertSelectQuery(Query *parsetree, Query ***subquery_ptr)
{
  Query *selectquery;
  RangeTblEntry *selectrte;
  RangeTblRef *rtr;

  if (subquery_ptr)
  {
    *subquery_ptr = NULL;
  }

  if (parsetree == NULL)
  {
    return parsetree;
  }
  if (parsetree->commandType != CMD_INSERT)
  {
    return parsetree;
  }

     
                                                                       
                                                                            
                                                                             
                                
     
  if (list_length(parsetree->rtable) >= 2 && strcmp(rt_fetch(PRS2_OLD_VARNO, parsetree->rtable)->eref->aliasname, "old") == 0 && strcmp(rt_fetch(PRS2_NEW_VARNO, parsetree->rtable)->eref->aliasname, "new") == 0)
  {
    return parsetree;
  }
  Assert(parsetree->jointree && IsA(parsetree->jointree, FromExpr));
  if (list_length(parsetree->jointree->fromlist) != 1)
  {
    elog(ERROR, "expected to find SELECT subquery");
  }
  rtr = (RangeTblRef *)linitial(parsetree->jointree->fromlist);
  Assert(IsA(rtr, RangeTblRef));
  selectrte = rt_fetch(rtr->rtindex, parsetree->rtable);
  selectquery = selectrte->subquery;
  if (!(selectquery && IsA(selectquery, Query) && selectquery->commandType == CMD_SELECT))
  {
    elog(ERROR, "expected to find SELECT subquery");
  }
  if (list_length(selectquery->rtable) >= 2 && strcmp(rt_fetch(PRS2_OLD_VARNO, selectquery->rtable)->eref->aliasname, "old") == 0 && strcmp(rt_fetch(PRS2_NEW_VARNO, selectquery->rtable)->eref->aliasname, "new") == 0)
  {
    if (subquery_ptr)
    {
      *subquery_ptr = &(selectrte->subquery);
    }
    return selectquery;
  }
  elog(ERROR, "could not find rule placeholders");
  return NULL;                  
}

   
                                                                 
   
void
AddQual(Query *parsetree, Node *qual)
{
  Node *copy;

  if (qual == NULL)
  {
    return;
  }

  if (parsetree->commandType == CMD_UTILITY)
  {
       
                                                               
       
                                                                       
                                                                          
                                                                      
                                                                         
                               
       
                                                                         
                                                                        
                                                                
       
    if (parsetree->utilityStmt && IsA(parsetree->utilityStmt, NotifyStmt))
    {
      return;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("conditional utility statements are not implemented")));
    }
  }

  if (parsetree->setOperations != NULL)
  {
       
                                                                           
                                                                         
                                    
       
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("conditional UNION/INTERSECT/EXCEPT statements are not implemented")));
  }

                                                               
  copy = copyObject(qual);

  parsetree->jointree->quals = make_and_qual(parsetree->jointree->quals, copy);

     
                                                                      
     
  Assert(!contain_aggs_of_level(copy, 0));

     
                                                                          
                                                   
     
  if (!parsetree->hasSubLinks)
  {
    parsetree->hasSubLinks = checkExprHasSubLink(copy);
  }
}

   
                                                                         
                                                                        
                                                             
   
void
AddInvertedQual(Query *parsetree, Node *qual)
{
  BooleanTest *invqual;

  if (qual == NULL)
  {
    return;
  }

                                                         
  invqual = makeNode(BooleanTest);
  invqual->arg = (Expr *)qual;
  invqual->booltesttype = IS_NOT_TRUE;
  invqual->location = -1;

  AddQual(parsetree, (Node *)invqual);
}

   
                                                                
                                                                      
                                                                  
   
                                                                         
                                                                             
                                                                             
                             
   
                                                                            
                                                                         
                                                                       
                                                                            
                                                                            
                                
   
                                                                         
                                                                       
                                                                       
                          
   
Node *
replace_rte_variables(Node *node, int target_varno, int sublevels_up, replace_rte_variables_callback callback, void *callback_arg, bool *outer_hasSubLinks)
{
  Node *result;
  replace_rte_variables_context context;

  context.callback = callback;
  context.callback_arg = callback_arg;
  context.target_varno = target_varno;
  context.sublevels_up = sublevels_up;

     
                                                                          
                                                             
     
  if (node && IsA(node, Query))
  {
    context.inserted_sublink = ((Query *)node)->hasSubLinks;
  }
  else if (outer_hasSubLinks)
  {
    context.inserted_sublink = *outer_hasSubLinks;
  }
  else
  {
    context.inserted_sublink = false;
  }

     
                                                                          
                                                            
     
  result = query_or_expression_tree_mutator(node, replace_rte_variables_mutator, (void *)&context, 0);

  if (context.inserted_sublink)
  {
    if (result && IsA(result, Query))
    {
      ((Query *)result)->hasSubLinks = true;
    }
    else if (outer_hasSubLinks)
    {
      *outer_hasSubLinks = true;
    }
    else
    {
      elog(ERROR, "replace_rte_variables inserted a SubLink, but has noplace to record it");
    }
  }

  return result;
}

Node *
replace_rte_variables_mutator(Node *node, replace_rte_variables_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varno == context->target_varno && var->varlevelsup == context->sublevels_up)
    {
                                                            
      Node *newnode;

      newnode = context->callback(var, context);
                                                      
      if (!context->inserted_sublink)
      {
        context->inserted_sublink = checkExprHasSubLink(newnode);
      }
      return newnode;
    }
                                                         
  }
  else if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)node;

    if (cexpr->cvarno == context->target_varno && context->sublevels_up == 0)
    {
         
                                                                         
                                                               
                                                                     
                                         
         
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("WHERE CURRENT OF on a view is not implemented")));
    }
                                                          
  }
  else if (IsA(node, Query))
  {
                                                                       
    Query *newnode;
    bool save_inserted_sublink;

    context->sublevels_up++;
    save_inserted_sublink = context->inserted_sublink;
    context->inserted_sublink = ((Query *)node)->hasSubLinks;
    newnode = query_tree_mutator((Query *)node, replace_rte_variables_mutator, (void *)context, 0);
    newnode->hasSubLinks |= context->inserted_sublink;
    context->inserted_sublink = save_inserted_sublink;
    context->sublevels_up--;
    return (Node *)newnode;
  }
  return expression_tree_mutator(node, replace_rte_variables_mutator, (void *)context);
}

   
                                                                          
                                                                          
                                                                          
                                             
   
                                                                             
                             
   
                                                                       
                                                                   
                                                                            
                                                                               
                                                                              
                                                                             
                                                                            
                                                                          
                                                                  
   
                                                                            
                                                                             
                       
   

typedef struct
{
  int target_varno;                                         
  int sublevels_up;                                         
  const AttrNumber *attno_map;                                
  int map_length;                                                    
  Oid to_rowtype;                                                      
  bool *found_whole_row;                        
} map_variable_attnos_context;

static Node *
map_variable_attnos_mutator(Node *node, map_variable_attnos_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    if (var->varno == context->target_varno && var->varlevelsup == context->sublevels_up)
    {
                                                            
      Var *newvar = (Var *)palloc(sizeof(Var));
      int attno = var->varattno;

      *newvar = *var;                                           

      if (attno > 0)
      {
                                                
        if (attno > context->map_length || context->attno_map[attno - 1] == 0)
        {
          elog(ERROR, "unexpected varattno %d in expression to be mapped", attno);
        }
        newvar->varattno = newvar->varoattno = context->attno_map[attno - 1];
      }
      else if (attno == 0)
      {
                                             
        *(context->found_whole_row) = true;

                                                                 
        if (OidIsValid(context->to_rowtype) && context->to_rowtype != var->vartype)
        {
          ConvertRowtypeExpr *r;

                                                                
          Assert(var->vartype != RECORDOID);

                                                            
          newvar->vartype = context->to_rowtype;

             
                                                                 
                                                       
             
          r = makeNode(ConvertRowtypeExpr);
          r->arg = (Expr *)newvar;
          r->resulttype = var->vartype;
          r->convertformat = COERCE_IMPLICIT_CAST;
          r->location = -1;

          return (Node *)r;
        }
      }
      return (Node *)newvar;
    }
                                                         
  }
  else if (IsA(node, ConvertRowtypeExpr))
  {
    ConvertRowtypeExpr *r = (ConvertRowtypeExpr *)node;
    Var *var = (Var *)r->arg;

       
                                                                         
                                                                        
                                                                           
                                                                          
                                            
       
    if (IsA(var, Var) && var->varno == context->target_varno && var->varlevelsup == context->sublevels_up && var->varattno == 0 && OidIsValid(context->to_rowtype) && context->to_rowtype != var->vartype)
    {
      ConvertRowtypeExpr *newnode;
      Var *newvar = (Var *)palloc(sizeof(Var));

                                           
      *(context->found_whole_row) = true;

      *newvar = *var;                                           

                                                            
      Assert(var->vartype != RECORDOID);

                                                        
      newvar->vartype = context->to_rowtype;

      newnode = (ConvertRowtypeExpr *)palloc(sizeof(ConvertRowtypeExpr));
      *newnode = *r;                                           
      newnode->arg = (Expr *)newvar;

      return (Node *)newnode;
    }
                                                                   
  }
  else if (IsA(node, Query))
  {
                                                                       
    Query *newnode;

    context->sublevels_up++;
    newnode = query_tree_mutator((Query *)node, map_variable_attnos_mutator, (void *)context, 0);
    context->sublevels_up--;
    return (Node *)newnode;
  }
  return expression_tree_mutator(node, map_variable_attnos_mutator, (void *)context);
}

Node *
map_variable_attnos(Node *node, int target_varno, int sublevels_up, const AttrNumber *attno_map, int map_length, Oid to_rowtype, bool *found_whole_row)
{
  map_variable_attnos_context context;

  context.target_varno = target_varno;
  context.sublevels_up = sublevels_up;
  context.attno_map = attno_map;
  context.map_length = map_length;
  context.to_rowtype = to_rowtype;
  context.found_whole_row = found_whole_row;

  *found_whole_row = false;

     
                                                                          
                                                            
     
  return query_or_expression_tree_mutator(node, map_variable_attnos_mutator, (void *)&context, 0);
}

   
                                                                         
   
                                                                   
                                                               
   
                                                                           
                   
                                            
                                                                 
                                                                           
   
                                                                          
                                                                              
                                                
   
                                                                    
   

typedef struct
{
  RangeTblEntry *target_rte;
  List *targetlist;
  ReplaceVarsNoMatchOption nomatch_option;
  int nomatch_varno;
} ReplaceVarsFromTargetList_context;

static Node *
ReplaceVarsFromTargetList_callback(Var *var, replace_rte_variables_context *context)
{
  ReplaceVarsFromTargetList_context *rcon = (ReplaceVarsFromTargetList_context *)context->callback_arg;
  TargetEntry *tle;

  if (var->varattno == InvalidAttrNumber)
  {
                                                        
    RowExpr *rowexpr;
    List *colnames;
    List *fields;

       
                                                                         
                                                                      
                                                                         
                                                                     
                                       
       
    expandRTE(rcon->target_rte, var->varno, var->varlevelsup, var->location, (var->vartype != RECORDOID), &colnames, &fields);
                                                
    fields = (List *)replace_rte_variables_mutator((Node *)fields, context);
    rowexpr = makeNode(RowExpr);
    rowexpr->args = fields;
    rowexpr->row_typeid = var->vartype;
    rowexpr->row_format = COERCE_IMPLICIT_CAST;
    rowexpr->colnames = colnames;
    rowexpr->location = var->location;

    return (Node *)rowexpr;
  }

                                                      
  tle = get_tle_by_resno(rcon->targetlist, var->varattno);

  if (tle == NULL || tle->resjunk)
  {
                                             
    switch (rcon->nomatch_option)
    {
    case REPLACEVARS_REPORT_ERROR:
                                           
      break;

    case REPLACEVARS_CHANGE_VARNO:
      var = (Var *)copyObject(var);
      var->varno = rcon->nomatch_varno;
      var->varnoold = rcon->nomatch_varno;
      return (Node *)var;

    case REPLACEVARS_SUBSTITUTE_NULL:

         
                                                                  
                                                              
         
      return coerce_to_domain((Node *)makeNullConst(var->vartype, var->vartypmod, var->varcollid), InvalidOid, -1, var->vartype, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1, false);
    }
    elog(ERROR, "could not find replacement targetlist entry for attno %d", var->varattno);
    return NULL;                          
  }
  else
  {
                                                 
    Expr *newnode = copyObject(tle->expr);

                                                                    
    if (var->varlevelsup > 0)
    {
      IncrementVarSublevelsUp((Node *)newnode, var->varlevelsup, 0);
    }

       
                                                                        
                                                                          
                                                                         
                                                                           
                                                                    
                                                                       
                                                                          
                                                                   
       
    if (contains_multiexpr_param((Node *)newnode, NULL))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("NEW variables in ON UPDATE rules cannot reference columns that are part of a multiple assignment in the subject UPDATE command")));
    }

    return (Node *)newnode;
  }
}

Node *
ReplaceVarsFromTargetList(Node *node, int target_varno, int sublevels_up, RangeTblEntry *target_rte, List *targetlist, ReplaceVarsNoMatchOption nomatch_option, int nomatch_varno, bool *outer_hasSubLinks)
{
  ReplaceVarsFromTargetList_context context;

  context.target_rte = target_rte;
  context.targetlist = targetlist;
  context.nomatch_option = nomatch_option;
  context.nomatch_varno = nomatch_varno;

  return replace_rte_variables(node, target_varno, sublevels_up, ReplaceVarsFromTargetList_callback, (void *)&context, outer_hasSubLinks);
}
