                                                                            
   
               
                                                      
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "parser/analyze.h"
#include "parser/parse_cte.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

                                                                     
typedef enum
{
  RECURSION_OK,
  RECURSION_NONRECURSIVETERM,                                
  RECURSION_SUBLINK,                                
  RECURSION_OUTERJOIN,                                                   
  RECURSION_INTERSECT,                                        
  RECURSION_EXCEPT                                         
} RecursionContext;

                                                                      
static const char *const recursion_errormsgs[] = {
                      
    NULL,
                                    
    gettext_noop("recursive reference to query \"%s\" must not appear within its non-recursive term"),
                           
    gettext_noop("recursive reference to query \"%s\" must not appear within a subquery"),
                             
    gettext_noop("recursive reference to query \"%s\" must not appear within an outer join"),
                             
    gettext_noop("recursive reference to query \"%s\" must not appear within INTERSECT"),
                          
    gettext_noop("recursive reference to query \"%s\" must not appear within EXCEPT")};

   
                                                                         
                                                                         
                                                                        
                                                      
   
typedef struct CteItem
{
  CommonTableExpr *cte;                          
  int id;                                                    
  Bitmapset *depends_on;                                            
} CteItem;

                                                                 
typedef struct CteState
{
                     
  ParseState *pstate;                         
  CteItem *items;                                       
  int numitems;                           
                                         
  int curitem;                                                  
  List *innerwiths;                                       
                                                             
  int selfrefcount;                                                 
  RecursionContext context;                                            
} CteState;

static void
analyzeCTE(ParseState *pstate, CommonTableExpr *cte);

                                     
static void
makeDependencyGraph(CteState *cstate);
static bool
makeDependencyGraphWalker(Node *node, CteState *cstate);
static void
TopologicalSort(ParseState *pstate, CteItem *items, int numitems);

                                          
static void
checkWellFormedRecursion(CteState *cstate);
static bool
checkWellFormedRecursionWalker(Node *node, CteState *cstate);
static void
checkWellFormedSelectStmt(SelectStmt *stmt, CteState *cstate);

   
                         
                                                                       
                  
   
                                                                        
                                                                            
                                                                   
   
List *
transformWithClause(ParseState *pstate, WithClause *withClause)
{
  ListCell *lc;

                                            
  Assert(pstate->p_ctenamespace == NIL);
  Assert(pstate->p_future_ctes == NIL);

     
                                                                           
                                                             
     
                                                                          
                                                                           
     
  foreach (lc, withClause->ctes)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);
    ListCell *rest;

    for_each_cell(rest, lnext(lc))
    {
      CommonTableExpr *cte2 = (CommonTableExpr *)lfirst(rest);

      if (strcmp(cte->ctename, cte2->ctename) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_ALIAS), errmsg("WITH query name \"%s\" specified more than once", cte2->ctename), parser_errposition(pstate, cte2->location)));
      }
    }

    cte->cterecursive = false;
    cte->cterefcount = 0;

    if (!IsA(cte->ctequery, SelectStmt))
    {
                                              
      Assert(IsA(cte->ctequery, InsertStmt) || IsA(cte->ctequery, UpdateStmt) || IsA(cte->ctequery, DeleteStmt));

      pstate->p_hasModifyingCTE = true;
    }
  }

  if (withClause->recursive)
  {
       
                                                                       
                                                                           
                                                      
       
    CteState cstate;
    int i;

    cstate.pstate = pstate;
    cstate.numitems = list_length(withClause->ctes);
    cstate.items = (CteItem *)palloc0(cstate.numitems * sizeof(CteItem));
    i = 0;
    foreach (lc, withClause->ctes)
    {
      cstate.items[i].cte = (CommonTableExpr *)lfirst(lc);
      cstate.items[i].id = i;
      i++;
    }

       
                                                                   
                                                                        
       
    makeDependencyGraph(&cstate);

       
                                                     
       
    checkWellFormedRecursion(&cstate);

       
                                                                           
                                                                          
                                                                         
                                                    
       
    for (i = 0; i < cstate.numitems; i++)
    {
      CommonTableExpr *cte = cstate.items[i].cte;

      pstate->p_ctenamespace = lappend(pstate->p_ctenamespace, cte);
    }

       
                                                                          
       
    for (i = 0; i < cstate.numitems; i++)
    {
      CommonTableExpr *cte = cstate.items[i].cte;

      analyzeCTE(pstate, cte);
    }
  }
  else
  {
       
                                                                          
                                                                   
                                                                           
                                                                         
                                                                        
       
    pstate->p_future_ctes = list_copy(withClause->ctes);

    foreach (lc, withClause->ctes)
    {
      CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

      analyzeCTE(pstate, cte);
      pstate->p_ctenamespace = lappend(pstate->p_ctenamespace, cte);
      pstate->p_future_ctes = list_delete_first(pstate->p_future_ctes);
    }
  }

  return pstate->p_ctenamespace;
}

   
                                                                     
                                                                            
                                                                    
   
static void
analyzeCTE(ParseState *pstate, CommonTableExpr *cte)
{
  Query *query;

                                 
  Assert(!IsA(cte->ctequery, Query));

  query = parse_sub_analyze(cte->ctequery, pstate, cte, false, true);
  cte->ctequery = (Node *)query;

     
                                                                           
                                  
     
  if (!IsA(query, Query))
  {
    elog(ERROR, "unexpected non-Query statement in WITH");
  }
  if (query->utilityStmt != NULL)
  {
    elog(ERROR, "unexpected utility statement in WITH");
  }

     
                                                                         
                                                                         
     
  if (query->commandType != CMD_SELECT && pstate->parentParseState != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("WITH clause containing a data-modifying statement must be at the top level"), parser_errposition(pstate, cte->location)));
  }

     
                                                                        
                                                                       
                                               
     
  query->canSetTag = false;

  if (!cte->cterecursive)
  {
                                                               
    analyzeCTETargetList(pstate, cte, GetCTETargetList(cte));
  }
  else
  {
       
                                                                     
                                                                          
                                                                 
                                                                       
       
    ListCell *lctlist, *lctyp, *lctypmod, *lccoll;
    int varattno;

    lctyp = list_head(cte->ctecoltypes);
    lctypmod = list_head(cte->ctecoltypmods);
    lccoll = list_head(cte->ctecolcollations);
    varattno = 0;
    foreach (lctlist, GetCTETargetList(cte))
    {
      TargetEntry *te = (TargetEntry *)lfirst(lctlist);
      Node *texpr;

      if (te->resjunk)
      {
        continue;
      }
      varattno++;
      Assert(varattno == te->resno);
      if (lctyp == NULL || lctypmod == NULL || lccoll == NULL)                       
      {
        elog(ERROR, "wrong number of output columns in WITH");
      }
      texpr = (Node *)te->expr;
      if (exprType(texpr) != lfirst_oid(lctyp) || exprTypmod(texpr) != lfirst_int(lctypmod))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("recursive query \"%s\" column %d has type %s in non-recursive term but type %s overall", cte->ctename, varattno, format_type_with_typemod(lfirst_oid(lctyp), lfirst_int(lctypmod)), format_type_with_typemod(exprType(texpr), exprTypmod(texpr))), errhint("Cast the output of the non-recursive term to the correct type."), parser_errposition(pstate, exprLocation(texpr))));
      }
      if (exprCollation(texpr) != lfirst_oid(lccoll))
      {
        ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("recursive query \"%s\" column %d has collation \"%s\" in non-recursive term but collation \"%s\" overall", cte->ctename, varattno, get_collation_name(lfirst_oid(lccoll)), get_collation_name(exprCollation(texpr))), errhint("Use the COLLATE clause to set the collation of the non-recursive term."), parser_errposition(pstate, exprLocation(texpr))));
      }
      lctyp = lnext(lctyp);
      lctypmod = lnext(lctypmod);
      lccoll = lnext(lccoll);
    }
    if (lctyp != NULL || lctypmod != NULL || lccoll != NULL)                       
    {
      elog(ERROR, "wrong number of output columns in WITH");
    }
  }
}

   
                                                                            
   
                                                                              
                                                                              
                                                                   
   
                                                                            
                                                                          
                                                                        
                                 
   
void
analyzeCTETargetList(ParseState *pstate, CommonTableExpr *cte, List *tlist)
{
  int numaliases;
  int varattno;
  ListCell *tlistitem;

                            
  Assert(cte->ctecolnames == NIL);

     
                                                                          
                                                                          
                                                                            
                                                                             
                           
     
  cte->ctecolnames = copyObject(cte->aliascolnames);
  cte->ctecoltypes = cte->ctecoltypmods = cte->ctecolcollations = NIL;
  numaliases = list_length(cte->aliascolnames);
  varattno = 0;
  foreach (tlistitem, tlist)
  {
    TargetEntry *te = (TargetEntry *)lfirst(tlistitem);
    Oid coltype;
    int32 coltypmod;
    Oid colcoll;

    if (te->resjunk)
    {
      continue;
    }
    varattno++;
    Assert(varattno == te->resno);
    if (varattno > numaliases)
    {
      char *attrname;

      attrname = pstrdup(te->resname);
      cte->ctecolnames = lappend(cte->ctecolnames, makeString(attrname));
    }
    coltype = exprType((Node *)te->expr);
    coltypmod = exprTypmod((Node *)te->expr);
    colcoll = exprCollation((Node *)te->expr);

       
                                                                     
                                                                        
                                                                      
                                                                       
                                                                      
       
                                                                       
                                      
       
    if (cte->cterecursive && coltype == UNKNOWNOID)
    {
      coltype = TEXTOID;
      coltypmod = -1;                                        
      if (!OidIsValid(colcoll))
      {
        colcoll = DEFAULT_COLLATION_OID;
      }
    }
    cte->ctecoltypes = lappend_oid(cte->ctecoltypes, coltype);
    cte->ctecoltypmods = lappend_int(cte->ctecoltypmods, coltypmod);
    cte->ctecolcollations = lappend_oid(cte->ctecolcollations, colcoll);
  }
  if (varattno < numaliases)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("WITH query \"%s\" has %d columns available but %d columns specified", cte->ctename, varattno, numaliases), parser_errposition(pstate, cte->location)));
  }
}

   
                                                                    
                                                          
   
static void
makeDependencyGraph(CteState *cstate)
{
  int i;

  for (i = 0; i < cstate->numitems; i++)
  {
    CommonTableExpr *cte = cstate->items[i].cte;

    cstate->curitem = i;
    cstate->innerwiths = NIL;
    makeDependencyGraphWalker((Node *)cte->ctequery, cstate);
    Assert(cstate->innerwiths == NIL);
  }

  TopologicalSort(cstate->pstate, cstate->items, cstate->numitems);
}

   
                                                                              
                                  
   
static bool
makeDependencyGraphWalker(Node *node, CteState *cstate)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, RangeVar))
  {
    RangeVar *rv = (RangeVar *)node;

                                                       
    if (!rv->schemaname)
    {
      ListCell *lc;
      int i;

                                                               
      foreach (lc, cstate->innerwiths)
      {
        List *withlist = (List *)lfirst(lc);
        ListCell *lc2;

        foreach (lc2, withlist)
        {
          CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc2);

          if (strcmp(rv->relname, cte->ctename) == 0)
          {
            return false;                       
          }
        }
      }

                                                                         
      for (i = 0; i < cstate->numitems; i++)
      {
        CommonTableExpr *cte = cstate->items[i].cte;

        if (strcmp(rv->relname, cte->ctename) == 0)
        {
          int myindex = cstate->curitem;

          if (i != myindex)
          {
                                           
            cstate->items[myindex].depends_on = bms_add_member(cstate->items[myindex].depends_on, cstate->items[i].id);
          }
          else
          {
                                                        
            cte->cterecursive = true;
          }
          break;
        }
      }
    }
    return false;
  }
  if (IsA(node, SelectStmt))
  {
    SelectStmt *stmt = (SelectStmt *)node;
    ListCell *lc;

    if (stmt->withClause)
    {
      if (stmt->withClause->recursive)
      {
           
                                                                  
                                                                   
                                                        
           
        cstate->innerwiths = lcons(stmt->withClause->ctes, cstate->innerwiths);
        foreach (lc, stmt->withClause->ctes)
        {
          CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

          (void)makeDependencyGraphWalker(cte->ctequery, cstate);
        }
        (void)raw_expression_tree_walker(node, makeDependencyGraphWalker, (void *)cstate);
        cstate->innerwiths = list_delete_first(cstate->innerwiths);
      }
      else
      {
           
                                                                     
                                                        
           
        ListCell *cell1;

        cstate->innerwiths = lcons(NIL, cstate->innerwiths);
        cell1 = list_head(cstate->innerwiths);
        foreach (lc, stmt->withClause->ctes)
        {
          CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

          (void)makeDependencyGraphWalker(cte->ctequery, cstate);
          lfirst(cell1) = lappend((List *)lfirst(cell1), cte);
        }
        (void)raw_expression_tree_walker(node, makeDependencyGraphWalker, (void *)cstate);
        cstate->innerwiths = list_delete_first(cstate->innerwiths);
      }
                                               
      return false;
    }
                                                                    
  }
  if (IsA(node, WithClause))
  {
       
                                                                         
                                                                          
                   
       
    return false;
  }
  return raw_expression_tree_walker(node, makeDependencyGraphWalker, (void *)cstate);
}

   
                                                                     
   
static void
TopologicalSort(ParseState *pstate, CteItem *items, int numitems)
{
  int i, j;

                                         
  for (i = 0; i < numitems; i++)
  {
                                                                           
    for (j = i; j < numitems; j++)
    {
      if (bms_is_empty(items[j].depends_on))
      {
        break;
      }
    }

                                                                 
    if (j >= numitems)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("mutual recursion between WITH items is not implemented"), parser_errposition(pstate, items[i].cte->location)));
    }

       
                                                                          
                     
       
    if (i != j)
    {
      CteItem tmp;

      tmp = items[i];
      items[i] = items[j];
      items[j] = tmp;
    }

       
                                                                        
                                   
       
    for (j = i + 1; j < numitems; j++)
    {
      items[j].depends_on = bms_del_member(items[j].depends_on, items[i].id);
    }
  }
}

   
                                                 
   
static void
checkWellFormedRecursion(CteState *cstate)
{
  int i;

  for (i = 0; i < cstate->numitems; i++)
  {
    CommonTableExpr *cte = cstate->items[i].cte;
    SelectStmt *stmt = (SelectStmt *)cte->ctequery;

    Assert(!IsA(stmt, Query));                       

                                                         
    if (!cte->cterecursive)
    {
      continue;
    }

                                    
    if (!IsA(stmt, SelectStmt))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_RECURSION), errmsg("recursive query \"%s\" must not contain data-modifying statements", cte->ctename), parser_errposition(cstate->pstate, cte->location)));
    }

                                   
    if (stmt->op != SETOP_UNION)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_RECURSION), errmsg("recursive query \"%s\" does not have the form non-recursive-term UNION [ALL] recursive-term", cte->ctename), parser_errposition(cstate->pstate, cte->location)));
    }

                                                                     
    cstate->curitem = i;
    cstate->innerwiths = NIL;
    cstate->selfrefcount = 0;
    cstate->context = RECURSION_NONRECURSIVETERM;
    checkWellFormedRecursionWalker((Node *)stmt->larg, cstate);
    Assert(cstate->innerwiths == NIL);

                                                                          
    cstate->curitem = i;
    cstate->innerwiths = NIL;
    cstate->selfrefcount = 0;
    cstate->context = RECURSION_OK;
    checkWellFormedRecursionWalker((Node *)stmt->rarg, cstate);
    Assert(cstate->innerwiths == NIL);
    if (cstate->selfrefcount != 1)                       
    {
      elog(ERROR, "missing recursive reference");
    }

                                                     
    if (stmt->withClause)
    {
      cstate->curitem = i;
      cstate->innerwiths = NIL;
      cstate->selfrefcount = 0;
      cstate->context = RECURSION_SUBLINK;
      checkWellFormedRecursionWalker((Node *)stmt->withClause->ctes, cstate);
      Assert(cstate->innerwiths == NIL);
    }

       
                                                                      
                                                                        
                                                                         
                                                                      
                               
       
    if (stmt->sortClause)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ORDER BY in a recursive query is not implemented"), parser_errposition(cstate->pstate, exprLocation((Node *)stmt->sortClause))));
    }
    if (stmt->limitOffset)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("OFFSET in a recursive query is not implemented"), parser_errposition(cstate->pstate, exprLocation(stmt->limitOffset))));
    }
    if (stmt->limitCount)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("LIMIT in a recursive query is not implemented"), parser_errposition(cstate->pstate, exprLocation(stmt->limitCount))));
    }
    if (stmt->lockingClause)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("FOR UPDATE/SHARE in a recursive query is not implemented"), parser_errposition(cstate->pstate, exprLocation((Node *)stmt->lockingClause))));
    }
  }
}

   
                                                                                
   
static bool
checkWellFormedRecursionWalker(Node *node, CteState *cstate)
{
  RecursionContext save_context = cstate->context;

  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, RangeVar))
  {
    RangeVar *rv = (RangeVar *)node;

                                                       
    if (!rv->schemaname)
    {
      ListCell *lc;
      CommonTableExpr *mycte;

                                                               
      foreach (lc, cstate->innerwiths)
      {
        List *withlist = (List *)lfirst(lc);
        ListCell *lc2;

        foreach (lc2, withlist)
        {
          CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc2);

          if (strcmp(rv->relname, cte->ctename) == 0)
          {
            return false;                       
          }
        }
      }

                                                                         
      mycte = cstate->items[cstate->curitem].cte;
      if (strcmp(rv->relname, mycte->ctename) == 0)
      {
                                                             
        if (cstate->context != RECURSION_OK)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_RECURSION), errmsg(recursion_errormsgs[cstate->context], mycte->ctename), parser_errposition(cstate->pstate, rv->location)));
        }
                              
        if (++(cstate->selfrefcount) > 1)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_RECURSION), errmsg("recursive reference to query \"%s\" must not appear more than once", mycte->ctename), parser_errposition(cstate->pstate, rv->location)));
        }
      }
    }
    return false;
  }
  if (IsA(node, SelectStmt))
  {
    SelectStmt *stmt = (SelectStmt *)node;
    ListCell *lc;

    if (stmt->withClause)
    {
      if (stmt->withClause->recursive)
      {
           
                                                                  
                                                                   
                                                        
           
        cstate->innerwiths = lcons(stmt->withClause->ctes, cstate->innerwiths);
        foreach (lc, stmt->withClause->ctes)
        {
          CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

          (void)checkWellFormedRecursionWalker(cte->ctequery, cstate);
        }
        checkWellFormedSelectStmt(stmt, cstate);
        cstate->innerwiths = list_delete_first(cstate->innerwiths);
      }
      else
      {
           
                                                                     
                                                        
           
        ListCell *cell1;

        cstate->innerwiths = lcons(NIL, cstate->innerwiths);
        cell1 = list_head(cstate->innerwiths);
        foreach (lc, stmt->withClause->ctes)
        {
          CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

          (void)checkWellFormedRecursionWalker(cte->ctequery, cstate);
          lfirst(cell1) = lappend((List *)lfirst(cell1), cte);
        }
        checkWellFormedSelectStmt(stmt, cstate);
        cstate->innerwiths = list_delete_first(cstate->innerwiths);
      }
    }
    else
    {
      checkWellFormedSelectStmt(stmt, cstate);
    }
                                             
    return false;
  }
  if (IsA(node, WithClause))
  {
       
                                                                         
                                                                          
                   
       
    return false;
  }
  if (IsA(node, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)node;

    switch (j->jointype)
    {
    case JOIN_INNER:
      checkWellFormedRecursionWalker(j->larg, cstate);
      checkWellFormedRecursionWalker(j->rarg, cstate);
      checkWellFormedRecursionWalker(j->quals, cstate);
      break;
    case JOIN_LEFT:
      checkWellFormedRecursionWalker(j->larg, cstate);
      if (save_context == RECURSION_OK)
      {
        cstate->context = RECURSION_OUTERJOIN;
      }
      checkWellFormedRecursionWalker(j->rarg, cstate);
      cstate->context = save_context;
      checkWellFormedRecursionWalker(j->quals, cstate);
      break;
    case JOIN_FULL:
      if (save_context == RECURSION_OK)
      {
        cstate->context = RECURSION_OUTERJOIN;
      }
      checkWellFormedRecursionWalker(j->larg, cstate);
      checkWellFormedRecursionWalker(j->rarg, cstate);
      cstate->context = save_context;
      checkWellFormedRecursionWalker(j->quals, cstate);
      break;
    case JOIN_RIGHT:
      if (save_context == RECURSION_OK)
      {
        cstate->context = RECURSION_OUTERJOIN;
      }
      checkWellFormedRecursionWalker(j->larg, cstate);
      cstate->context = save_context;
      checkWellFormedRecursionWalker(j->rarg, cstate);
      checkWellFormedRecursionWalker(j->quals, cstate);
      break;
    default:
      elog(ERROR, "unrecognized join type: %d", (int)j->jointype);
    }
    return false;
  }
  if (IsA(node, SubLink))
  {
    SubLink *sl = (SubLink *)node;

       
                                                                  
                   
       
    cstate->context = RECURSION_SUBLINK;
    checkWellFormedRecursionWalker(sl->subselect, cstate);
    cstate->context = save_context;
    checkWellFormedRecursionWalker(sl->testexpr, cstate);
    return false;
  }
  return raw_expression_tree_walker(node, checkWellFormedRecursionWalker, (void *)cstate);
}

   
                                                                       
                                          
   
static void
checkWellFormedSelectStmt(SelectStmt *stmt, CteState *cstate)
{
  RecursionContext save_context = cstate->context;

  if (save_context != RECURSION_OK)
  {
                                             
    raw_expression_tree_walker((Node *)stmt, checkWellFormedRecursionWalker, (void *)cstate);
  }
  else
  {
    switch (stmt->op)
    {
    case SETOP_NONE:
    case SETOP_UNION:
      raw_expression_tree_walker((Node *)stmt, checkWellFormedRecursionWalker, (void *)cstate);
      break;
    case SETOP_INTERSECT:
      if (stmt->all)
      {
        cstate->context = RECURSION_INTERSECT;
      }
      checkWellFormedRecursionWalker((Node *)stmt->larg, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->rarg, cstate);
      cstate->context = save_context;
      checkWellFormedRecursionWalker((Node *)stmt->sortClause, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->limitOffset, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->limitCount, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->lockingClause, cstate);
                                                          
      break;
    case SETOP_EXCEPT:
      if (stmt->all)
      {
        cstate->context = RECURSION_EXCEPT;
      }
      checkWellFormedRecursionWalker((Node *)stmt->larg, cstate);
      cstate->context = RECURSION_EXCEPT;
      checkWellFormedRecursionWalker((Node *)stmt->rarg, cstate);
      cstate->context = save_context;
      checkWellFormedRecursionWalker((Node *)stmt->sortClause, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->limitOffset, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->limitCount, cstate);
      checkWellFormedRecursionWalker((Node *)stmt->lockingClause, cstate);
                                                          
      break;
    default:
      elog(ERROR, "unrecognized set op: %d", (int)stmt->op);
    }
  }
}
