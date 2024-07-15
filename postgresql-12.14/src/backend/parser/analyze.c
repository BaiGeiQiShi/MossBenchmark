                                                                            
   
             
                                                    
   
                                                                           
                                                                       
                                                                           
                                                                      
                                                                            
                                                                           
                                                                              
                                                                            
                                                              
   
   
                                                                         
                                                                        
   
                                
   
                                                                            
   

#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_cte.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_param.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/rel.h"

                                                              
post_parse_analyze_hook_type post_parse_analyze_hook = NULL;

static Query *
transformOptionalSelectInto(ParseState *pstate, Node *parseTree);
static Query *
transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
static Query *
transformInsertStmt(ParseState *pstate, InsertStmt *stmt);
static List *
transformInsertRow(ParseState *pstate, List *exprlist, List *stmtcols, List *icolumns, List *attrnos, bool strip_indirection);
static OnConflictExpr *
transformOnConflictClause(ParseState *pstate, OnConflictClause *onConflictClause);
static int
count_rowexpr_columns(ParseState *pstate, Node *expr);
static Query *
transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
static Query *
transformValuesClause(ParseState *pstate, SelectStmt *stmt);
static Query *
transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt);
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt, bool isTopLevel, List **targetlist);
static void
determineRecursiveColTypes(ParseState *pstate, Node *larg, List *nrtargetlist);
static Query *
transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
static List *
transformReturningList(ParseState *pstate, List *returningList);
static List *
transformUpdateTargetList(ParseState *pstate, List *targetList);
static Query *
transformDeclareCursorStmt(ParseState *pstate, DeclareCursorStmt *stmt);
static Query *
transformExplainStmt(ParseState *pstate, ExplainStmt *stmt);
static Query *
transformCreateTableAsStmt(ParseState *pstate, CreateTableAsStmt *stmt);
static Query *
transformCallStmt(ParseState *pstate, CallStmt *stmt);
static void
transformLockingClause(ParseState *pstate, Query *qry, LockingClause *lc, bool pushedDown);
#ifdef RAW_EXPRESSION_COVERAGE_TEST
static bool
test_raw_expression_coverage(Node *node, void *context);
#endif

   
                 
                                                             
   
                                                                     
                                                                        
   
                                                                            
                                                                     
                                   
   
Query *
parse_analyze(RawStmt *parseTree, const char *sourceText, Oid *paramTypes, int numParams, QueryEnvironment *queryEnv)
{
  ParseState *pstate = make_parsestate(NULL);
  Query *query;

  Assert(sourceText != NULL);                         

  pstate->p_sourcetext = sourceText;

  if (numParams > 0)
  {
    parse_fixed_parameters(pstate, paramTypes, numParams);
  }

  pstate->p_queryEnv = queryEnv;

  query = transformTopLevelStmt(pstate, parseTree);

  if (post_parse_analyze_hook)
  {
    (*post_parse_analyze_hook)(pstate, query);
  }

  free_parsestate(pstate);

  return query;
}

   
                           
   
                                                                      
                                                                        
                                           
   
Query *
parse_analyze_varparams(RawStmt *parseTree, const char *sourceText, Oid **paramTypes, int *numParams)
{
  ParseState *pstate = make_parsestate(NULL);
  Query *query;

  Assert(sourceText != NULL);                         

  pstate->p_sourcetext = sourceText;

  parse_variable_parameters(pstate, paramTypes, numParams);

  query = transformTopLevelStmt(pstate, parseTree);

                                                  
  check_variable_parameters(pstate, query);

  if (post_parse_analyze_hook)
  {
    (*post_parse_analyze_hook)(pstate, query);
  }

  free_parsestate(pstate);

  return query;
}

   
                     
                                                           
   
Query *
parse_sub_analyze(Node *parseTree, ParseState *parentParseState, CommonTableExpr *parentCTE, bool locked_from_parent, bool resolve_unknowns)
{
  ParseState *pstate = make_parsestate(parentParseState);
  Query *query;

  pstate->p_parent_cte = parentCTE;
  pstate->p_locked_from_parent = locked_from_parent;
  pstate->p_resolve_unknowns = resolve_unknowns;

  query = transformStmt(pstate, parseTree);

  free_parsestate(pstate);

  return query;
}

   
                           
                                               
   
                                                                              
                                             
   
Query *
transformTopLevelStmt(ParseState *pstate, RawStmt *parseTree)
{
  Query *result;

                                                
  result = transformOptionalSelectInto(pstate, parseTree->stmt);

  result->stmt_location = parseTree->stmt_location;
  result->stmt_len = parseTree->stmt_len;

  return result;
}

   
                                 
                                                        
   
                                                                       
                                                                           
                                                                            
                                                                          
                               
   
static Query *
transformOptionalSelectInto(ParseState *pstate, Node *parseTree)
{
  if (IsA(parseTree, SelectStmt))
  {
    SelectStmt *stmt = (SelectStmt *)parseTree;

                                                                         
    while (stmt && stmt->op != SETOP_NONE)
    {
      stmt = stmt->larg;
    }
    Assert(stmt && IsA(stmt, SelectStmt) && stmt->larg == NULL);

    if (stmt->intoClause)
    {
      CreateTableAsStmt *ctas = makeNode(CreateTableAsStmt);

      ctas->query = parseTree;
      ctas->into = stmt->intoClause;
      ctas->relkind = OBJECT_TABLE;
      ctas->is_select_into = true;

         
                                                                        
                                                                        
                                                                  
         
      stmt->intoClause = NULL;

      parseTree = (Node *)ctas;
    }
  }

  return transformStmt(pstate, parseTree);
}

   
                   
                                                           
   
Query *
transformStmt(ParseState *pstate, Node *parseTree)
{
  Query *result;

     
                                                                            
                                                                             
                                                 
     
#ifdef RAW_EXPRESSION_COVERAGE_TEST
  switch (nodeTag(parseTree))
  {
  case T_SelectStmt:
  case T_InsertStmt:
  case T_UpdateStmt:
  case T_DeleteStmt:
    (void)test_raw_expression_coverage(parseTree, NULL);
    break;
  default:
    break;
  }
#endif                                   

  switch (nodeTag(parseTree))
  {
       
                              
       
  case T_InsertStmt:
    result = transformInsertStmt(pstate, (InsertStmt *)parseTree);
    break;

  case T_DeleteStmt:
    result = transformDeleteStmt(pstate, (DeleteStmt *)parseTree);
    break;

  case T_UpdateStmt:
    result = transformUpdateStmt(pstate, (UpdateStmt *)parseTree);
    break;

  case T_SelectStmt:
  {
    SelectStmt *n = (SelectStmt *)parseTree;

    if (n->valuesLists)
    {
      result = transformValuesClause(pstate, n);
    }
    else if (n->op == SETOP_NONE)
    {
      result = transformSelectStmt(pstate, n);
    }
    else
    {
      result = transformSetOperationStmt(pstate, n);
    }
  }
  break;

       
                     
       
  case T_DeclareCursorStmt:
    result = transformDeclareCursorStmt(pstate, (DeclareCursorStmt *)parseTree);
    break;

  case T_ExplainStmt:
    result = transformExplainStmt(pstate, (ExplainStmt *)parseTree);
    break;

  case T_CreateTableAsStmt:
    result = transformCreateTableAsStmt(pstate, (CreateTableAsStmt *)parseTree);
    break;

  case T_CallStmt:
    result = transformCallStmt(pstate, (CallStmt *)parseTree);
    break;

  default:

       
                                                                      
                                                                  
       
    result = makeNode(Query);
    result->commandType = CMD_UTILITY;
    result->utilityStmt = (Node *)parseTree;
    break;
  }

                                                         
  result->querySource = QSRC_ORIGINAL;
  result->canSetTag = true;

  return result;
}

   
                             
                                                                       
                                 
   
                                                     
   
bool
analyze_requires_snapshot(RawStmt *parseTree)
{
  bool result;

  switch (nodeTag(parseTree->stmt))
  {
       
                              
       
  case T_InsertStmt:
  case T_DeleteStmt:
  case T_UpdateStmt:
  case T_SelectStmt:
    result = true;
    break;

       
                     
       
  case T_DeclareCursorStmt:
  case T_ExplainStmt:
  case T_CreateTableAsStmt:
                                                              
    result = true;
    break;

  default:
                                                                     
    result = false;
    break;
  }

  return result;
}

   
                         
                                   
   
static Query *
transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt)
{
  Query *qry = makeNode(Query);
  ParseNamespaceItem *nsitem;
  Node *qual;

  qry->commandType = CMD_DELETE;

                                                         
  if (stmt->withClause)
  {
    qry->hasRecursive = stmt->withClause->recursive;
    qry->cteList = transformWithClause(pstate, stmt->withClause);
    qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
  }

                                                   
  qry->resultRelation = setTargetTable(pstate, stmt->relation, stmt->relation->inh, true, ACL_DELETE);

                                                      
  nsitem = (ParseNamespaceItem *)llast(pstate->p_namespace);

                                     
  qry->distinctClause = NIL;

                                                             
  nsitem->p_lateral_only = true;
  nsitem->p_lateral_ok = false;

     
                                                                       
                                                                          
                                                                      
                                   
     
  transformFromClause(pstate, stmt->usingClause);

                                                                    
  nsitem->p_lateral_only = false;
  nsitem->p_lateral_ok = true;

  qual = transformWhereClause(pstate, stmt->whereClause, EXPR_KIND_WHERE, "WHERE");

  qry->returningList = transformReturningList(pstate, stmt->returningList);

                                                  
  qry->rtable = pstate->p_rtable;
  qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

  qry->hasSubLinks = pstate->p_hasSubLinks;
  qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
  qry->hasTargetSRFs = pstate->p_hasTargetSRFs;
  qry->hasAggs = pstate->p_hasAggs;

  assign_query_collations(pstate, qry);

                                                                            
  if (pstate->p_hasAggs)
  {
    parseCheckAggregates(pstate, qry);
  }

  return qry;
}

   
                         
                                   
   
static Query *
transformInsertStmt(ParseState *pstate, InsertStmt *stmt)
{
  Query *qry = makeNode(Query);
  SelectStmt *selectStmt = (SelectStmt *)stmt->selectStmt;
  List *exprList = NIL;
  bool isGeneralSelect;
  List *sub_rtable;
  List *sub_namespace;
  List *icolumns;
  List *attrnos;
  RangeTblEntry *rte;
  RangeTblRef *rtr;
  ListCell *icols;
  ListCell *attnos;
  ListCell *lc;
  bool isOnConflictUpdate;
  AclMode targetPerms;

                                                    
  Assert(pstate->p_ctenamespace == NIL);

  qry->commandType = CMD_INSERT;
  pstate->p_is_insert = true;

                                                         
  if (stmt->withClause)
  {
    qry->hasRecursive = stmt->withClause->recursive;
    qry->cteList = transformWithClause(pstate, stmt->withClause);
    qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
  }

  qry->override = stmt->override;

  isOnConflictUpdate = (stmt->onConflictClause && stmt->onConflictClause->action == ONCONFLICT_UPDATE);

     
                                                                            
                                                                             
                                                             
     
                                                                            
                                                                            
                                                                           
     
  isGeneralSelect = (selectStmt && (selectStmt->valuesLists == NIL || selectStmt->sortClause != NIL || selectStmt->limitOffset != NULL || selectStmt->limitCount != NULL || selectStmt->lockingClause != NIL || selectStmt->withClause != NULL));

     
                                                                       
                                                                         
                                                                          
                                                                          
                                                                            
                                                                            
                                                     
     
  if (isGeneralSelect)
  {
    sub_rtable = pstate->p_rtable;
    pstate->p_rtable = NIL;
    sub_namespace = pstate->p_namespace;
    pstate->p_namespace = NIL;
  }
  else
  {
    sub_rtable = NIL;                                        
    sub_namespace = NIL;
  }

     
                                                                             
                                                                             
                                                                            
                                   
     
  targetPerms = ACL_INSERT;
  if (isOnConflictUpdate)
  {
    targetPerms |= ACL_UPDATE;
  }
  qry->resultRelation = setTargetTable(pstate, stmt->relation, false, false, targetPerms);

                                                                        
  icolumns = checkInsertTargets(pstate, stmt->cols, &attrnos);
  Assert(list_length(icolumns) == list_length(attrnos));

     
                                                
     
  if (selectStmt == NULL)
  {
       
                                                                      
                                                                           
                                           
       
    exprList = NIL;
  }
  else if (isGeneralSelect)
  {
       
                                                                         
                                                                       
                                                                       
                                                                          
            
       
    ParseState *sub_pstate = make_parsestate(pstate);
    Query *selectQuery;

       
                                  
       
                                                                           
                                                                         
                                                                       
                                    
       
                                                                    
                                                                         
                                                                        
                                                                          
                                                        
       
    sub_pstate->p_rtable = sub_rtable;
    sub_pstate->p_joinexprs = NIL;                              
    sub_pstate->p_namespace = sub_namespace;
    sub_pstate->p_resolve_unknowns = false;

    selectQuery = transformStmt(sub_pstate, stmt->selectStmt);

    free_parsestate(sub_pstate);

                                                   
    if (!IsA(selectQuery, Query) || selectQuery->commandType != CMD_SELECT)
    {
      elog(ERROR, "unexpected non-SELECT command in INSERT ... SELECT");
    }

       
                                                                         
                                    
       
    rte = addRangeTableEntryForSubquery(pstate, selectQuery, makeAlias("*SELECT*", NIL), false, false);
    rtr = makeNode(RangeTblRef);
                                  
    rtr->rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
    pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);

                 
                                                                       
                                                                       
                                                                      
                                        
       
                                                                          
                                                                    
                                                                      
                                                                  
                                                              
                                                   
                 
       
    exprList = NIL;
    foreach (lc, selectQuery->targetList)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(lc);
      Expr *expr;

      if (tle->resjunk)
      {
        continue;
      }
      if (tle->expr && (IsA(tle->expr, Const) || IsA(tle->expr, Param)) && exprType((Node *)tle->expr) == UNKNOWNOID)
      {
        expr = tle->expr;
      }
      else
      {
        Var *var = makeVarFromTargetEntry(rtr->rtindex, tle);

        var->location = exprLocation((Node *)tle->expr);
        expr = (Expr *)var;
      }
      exprList = lappend(exprList, expr);
    }

                                                    
    exprList = transformInsertRow(pstate, exprList, stmt->cols, icolumns, attrnos, false);
  }
  else if (list_length(selectStmt->valuesLists) > 1)
  {
       
                                                                   
                                                                           
                                                                       
            
       
    List *exprsLists = NIL;
    List *coltypes = NIL;
    List *coltypmods = NIL;
    List *colcollations = NIL;
    int sublist_length = -1;
    bool lateral = false;

    Assert(selectStmt->intoClause == NULL);

    foreach (lc, selectStmt->valuesLists)
    {
      List *sublist = (List *)lfirst(lc);

         
                                                                       
                                          
         
      sublist = transformExpressionList(pstate, sublist, EXPR_KIND_VALUES, true);

         
                                                           
                                                                      
                                                         
         
      if (sublist_length < 0)
      {
                                                                  
        sublist_length = list_length(sublist);
      }
      else if (sublist_length != list_length(sublist))
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("VALUES lists must all be the same length"), parser_errposition(pstate, exprLocation((Node *)sublist))));
      }

         
                                                                     
                                                                        
                                                                        
                                                                      
                                                              
                                                                    
                                                     
                                          
         
      sublist = transformInsertRow(pstate, sublist, stmt->cols, icolumns, attrnos, true);

         
                                                                       
                                                                     
                                                                     
                                                                         
                                                                         
                                                                         
                                                                   
                                                                         
                                                                
                                                       
         
      assign_list_collations(pstate, sublist);

      exprsLists = lappend(exprsLists, sublist);
    }

       
                                                                        
                                                                           
                                                                          
                                                                           
                                                                          
       
    foreach (lc, (List *)linitial(exprsLists))
    {
      Node *val = (Node *)lfirst(lc);

      coltypes = lappend_oid(coltypes, exprType(val));
      coltypmods = lappend_int(coltypmods, exprTypmod(val));
      colcollations = lappend_oid(colcollations, InvalidOid);
    }

       
                                                                          
                                                                      
                                                                           
                                               
       
    if (list_length(pstate->p_rtable) != 1 && contain_vars_of_level((Node *)exprsLists, 0))
    {
      lateral = true;
    }

       
                               
       
    rte = addRangeTableEntryForValues(pstate, exprsLists, coltypes, coltypmods, colcollations, NULL, lateral, true);
    rtr = makeNode(RangeTblRef);
                                  
    rtr->rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
    pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);

       
                                                 
       
    expandRTE(rte, rtr->rtindex, 0, -1, false, NULL, &exprList);

       
                                                                       
       
    exprList = transformInsertRow(pstate, exprList, stmt->cols, icolumns, attrnos, false);
  }
  else
  {
       
                                                                         
                                                                          
                                                                      
                                                  
       
    List *valuesLists = selectStmt->valuesLists;

    Assert(list_length(valuesLists) == 1);
    Assert(selectStmt->intoClause == NULL);

       
                                                                           
                                  
       
    exprList = transformExpressionList(pstate, (List *)linitial(valuesLists), EXPR_KIND_VALUES_SINGLE, true);

                                                    
    exprList = transformInsertRow(pstate, exprList, stmt->cols, icolumns, attrnos, false);
  }

     
                                                                          
                                                                      
     
  rte = pstate->p_target_rangetblentry;
  qry->targetList = NIL;
  Assert(list_length(exprList) <= list_length(icolumns));
  forthree(lc, exprList, icols, icolumns, attnos, attrnos)
  {
    Expr *expr = (Expr *)lfirst(lc);
    ResTarget *col = lfirst_node(ResTarget, icols);
    AttrNumber attr_num = (AttrNumber)lfirst_int(attnos);
    TargetEntry *tle;

    tle = makeTargetEntry(expr, attr_num, col->name, false);
    qry->targetList = lappend(qry->targetList, tle);

    rte->insertedCols = bms_add_member(rte->insertedCols, attr_num - FirstLowInvalidHeapAttributeNumber);
  }

                                    
  if (stmt->onConflictClause)
  {
    qry->onConflict = transformOnConflictClause(pstate, stmt->onConflictClause);
  }

     
                                                                          
                                                                         
                                                                         
                                
     
  if (stmt->returningList)
  {
    pstate->p_namespace = NIL;
    addRTEtoQuery(pstate, pstate->p_target_rangetblentry, false, true, true);
    qry->returningList = transformReturningList(pstate, stmt->returningList);
  }

                                                  
  qry->rtable = pstate->p_rtable;
  qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

  qry->hasTargetSRFs = pstate->p_hasTargetSRFs;
  qry->hasSubLinks = pstate->p_hasSubLinks;

  assign_query_collations(pstate, qry);

  return qry;
}

   
                                                             
   
                                                                              
                                                                           
                                                                            
                                                               
                                                                     
                                                                       
   
static List *
transformInsertRow(ParseState *pstate, List *exprlist, List *stmtcols, List *icolumns, List *attrnos, bool strip_indirection)
{
  List *result;
  ListCell *lc;
  ListCell *icols;
  ListCell *attnos;

     
                                                                        
                                                                        
                                                                  
                                                                         
                                                
     
  if (list_length(exprlist) > list_length(icolumns))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("INSERT has more expressions than target columns"), parser_errposition(pstate, exprLocation(list_nth(exprlist, list_length(icolumns))))));
  }
  if (stmtcols != NIL && list_length(exprlist) < list_length(icolumns))
  {
       
                                                                         
                                                                         
                                                                      
                                                                         
                                                                     
                                                                           
                           
       
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("INSERT has more target columns than expressions"), ((list_length(exprlist) == 1 && count_rowexpr_columns(pstate, linitial(exprlist)) == list_length(icolumns)) ? errhint("The insertion source is a row expression containing the same number of columns expected by the INSERT. Did you accidentally use extra parentheses?") : 0), parser_errposition(pstate, exprLocation(list_nth(icolumns, list_length(exprlist))))));
  }

     
                                                     
     
  result = NIL;
  forthree(lc, exprlist, icols, icolumns, attnos, attrnos)
  {
    Expr *expr = (Expr *)lfirst(lc);
    ResTarget *col = lfirst_node(ResTarget, icols);
    int attno = lfirst_int(attnos);

    expr = transformAssignedExpr(pstate, expr, EXPR_KIND_INSERT_TARGET, col->name, attno, col->indirection, col->location);

    if (strip_indirection)
    {
      while (expr)
      {
        if (IsA(expr, FieldStore))
        {
          FieldStore *fstore = (FieldStore *)expr;

          expr = (Expr *)linitial(fstore->newvals);
        }
        else if (IsA(expr, SubscriptingRef))
        {
          SubscriptingRef *sbsref = (SubscriptingRef *)expr;

          if (sbsref->refassgnexpr == NULL)
          {
            break;
          }

          expr = sbsref->refassgnexpr;
        }
        else
        {
          break;
        }
      }
    }

    result = lappend(result, expr);
  }

  return result;
}

   
                               
                                                 
   
static OnConflictExpr *
transformOnConflictClause(ParseState *pstate, OnConflictClause *onConflictClause)
{
  List *arbiterElems;
  Node *arbiterWhere;
  Oid arbiterConstraint;
  List *onConflictSet = NIL;
  Node *onConflictWhere = NULL;
  RangeTblEntry *exclRte = NULL;
  int exclRelIndex = 0;
  List *exclRelTlist = NIL;
  OnConflictExpr *result;

                                                        
  transformOnConflictArbiter(pstate, onConflictClause, &arbiterElems, &arbiterWhere, &arbiterConstraint);

                         
  if (onConflictClause->action == ONCONFLICT_UPDATE)
  {
    Relation targetrel = pstate->p_target_relation;

       
                                                                          
                                                                         
       
    pstate->p_is_insert = false;

       
                                                                           
                                                                        
                                                                      
                                                   
       
    exclRte = addRangeTableEntryForRelation(pstate, targetrel, RowExclusiveLock, makeAlias("excluded", NIL), false, false);
    exclRte->relkind = RELKIND_COMPOSITE_TYPE;
    exclRte->requiredPerms = 0;
                                                               

    exclRelIndex = list_length(pstate->p_rtable);

                                                             
    exclRelTlist = BuildOnConflictExcludedTargetlist(targetrel, exclRelIndex);

       
                                                                          
                                             
       
    addRTEtoQuery(pstate, exclRte, false, true, true);
    addRTEtoQuery(pstate, pstate->p_target_rangetblentry, false, true, true);

       
                                                
       
    onConflictSet = transformUpdateTargetList(pstate, onConflictClause->targetList);

    onConflictWhere = transformWhereClause(pstate, onConflictClause->whereClause, EXPR_KIND_WHERE, "WHERE");
  }

                                                                   
  result = makeNode(OnConflictExpr);

  result->action = onConflictClause->action;
  result->arbiterElems = arbiterElems;
  result->arbiterWhere = arbiterWhere;
  result->constraint = arbiterConstraint;
  result->onConflictSet = onConflictSet;
  result->onConflictWhere = onConflictWhere;
  result->exclRelIndex = exclRelIndex;
  result->exclRelTlist = exclRelTlist;

  return result;
}

   
                                     
                                                                        
                                                                   
   
                                           
   
List *
BuildOnConflictExcludedTargetlist(Relation targetrel, Index exclRelIndex)
{
  List *result = NIL;
  int attno;
  Var *var;
  TargetEntry *te;

     
                                                                    
                                                                         
     
  for (attno = 0; attno < RelationGetNumberOfAttributes(targetrel); attno++)
  {
    Form_pg_attribute attr = TupleDescAttr(targetrel->rd_att, attno);
    char *name;

    if (attr->attisdropped)
    {
         
                                                                         
                                 
         
      var = (Var *)makeNullConst(INT4OID, -1, InvalidOid);
      name = NULL;
    }
    else
    {
      var = makeVar(exclRelIndex, attno + 1, attr->atttypid, attr->atttypmod, attr->attcollation, 0);
      name = pstrdup(NameStr(attr->attname));
    }

    te = makeTargetEntry((Expr *)var, attno + 1, name, false);

    result = lappend(result, te);
  }

     
                                                                            
                                                                             
                                                                          
                                                                         
                                                        
     
  var = makeVar(exclRelIndex, InvalidAttrNumber, targetrel->rd_rel->reltype, -1, InvalidOid, 0);
  te = makeTargetEntry((Expr *)var, InvalidAttrNumber, NULL, true);
  result = lappend(result, te);

  return result;
}

   
                           
                                                            
                                                                       
   
                                                                        
                                                                            
                                                                      
   
static int
count_rowexpr_columns(ParseState *pstate, Node *expr)
{
  if (expr == NULL)
  {
    return -1;
  }
  if (IsA(expr, RowExpr))
  {
    return list_length(((RowExpr *)expr)->args);
  }
  if (IsA(expr, Var))
  {
    Var *var = (Var *)expr;
    AttrNumber attnum = var->varattno;

    if (attnum > 0 && var->vartype == RECORDOID)
    {
      RangeTblEntry *rte;

      rte = GetRTEByRangeTablePosn(pstate, var->varno, var->varlevelsup);
      if (rte->rtekind == RTE_SUBQUERY)
      {
                                                                 
        TargetEntry *ste = get_tle_by_resno(rte->subquery->targetList, attnum);

        if (ste == NULL || ste->resjunk)
        {
          return -1;
        }
        expr = (Node *)ste->expr;
        if (IsA(expr, RowExpr))
        {
          return list_length(((RowExpr *)expr)->args);
        }
      }
    }
  }
  return -1;
}

   
                         
                                   
   
                                                                            
                                  
   
static Query *
transformSelectStmt(ParseState *pstate, SelectStmt *stmt)
{
  Query *qry = makeNode(Query);
  Node *qual;
  ListCell *l;

  qry->commandType = CMD_SELECT;

                                                         
  if (stmt->withClause)
  {
    qry->hasRecursive = stmt->withClause->recursive;
    qry->cteList = transformWithClause(pstate, stmt->withClause);
    qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
  }

                                                                          
  if (stmt->intoClause)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("SELECT ... INTO is not allowed here"), parser_errposition(pstate, exprLocation((Node *)stmt->intoClause))));
  }

                                                                      
  pstate->p_locking_clause = stmt->lockingClause;

                                                            
  pstate->p_windowdefs = stmt->windowClause;

                               
  transformFromClause(pstate, stmt->fromClause);

                            
  qry->targetList = transformTargetList(pstate, stmt->targetList, EXPR_KIND_SELECT_TARGET);

                           
  markTargetListOrigins(pstate, qry->targetList);

                       
  qual = transformWhereClause(pstate, stmt->whereClause, EXPR_KIND_WHERE, "WHERE");

                                                                     
  qry->havingQual = transformWhereClause(pstate, stmt->havingClause, EXPR_KIND_HAVING, "HAVING");

     
                                                                       
                                                                             
                                                                            
                        
     
  qry->sortClause = transformSortClause(pstate, stmt->sortClause, &qry->targetList, EXPR_KIND_ORDER_BY, false                        );

  qry->groupClause = transformGroupClause(pstate, stmt->groupClause, &qry->groupingSets, &qry->targetList, qry->sortClause, EXPR_KIND_GROUP_BY, false                        );

  if (stmt->distinctClause == NIL)
  {
    qry->distinctClause = NIL;
    qry->hasDistinctOn = false;
  }
  else if (linitial(stmt->distinctClause) == NULL)
  {
                                
    qry->distinctClause = transformDistinctClause(pstate, &qry->targetList, qry->sortClause, false);
    qry->hasDistinctOn = false;
  }
  else
  {
                                   
    qry->distinctClause = transformDistinctOnClause(pstate, stmt->distinctClause, &qry->targetList, qry->sortClause);
    qry->hasDistinctOn = true;
  }

                       
  qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset, EXPR_KIND_OFFSET, "OFFSET");
  qry->limitCount = transformLimitClause(pstate, stmt->limitCount, EXPR_KIND_LIMIT, "LIMIT");

                                                                        
  qry->windowClause = transformWindowDefinitions(pstate, pstate->p_windowdefs, &qry->targetList);

                                                                      
  if (pstate->p_resolve_unknowns)
  {
    resolveTargetListUnknowns(pstate, qry->targetList);
  }

  qry->rtable = pstate->p_rtable;
  qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

  qry->hasSubLinks = pstate->p_hasSubLinks;
  qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
  qry->hasTargetSRFs = pstate->p_hasTargetSRFs;
  qry->hasAggs = pstate->p_hasAggs;

  foreach (l, stmt->lockingClause)
  {
    transformLockingClause(pstate, qry, (LockingClause *)lfirst(l), false);
  }

  assign_query_collations(pstate, qry);

                                                                            
  if (pstate->p_hasAggs || qry->groupClause || qry->groupingSets || qry->havingQual)
  {
    parseCheckAggregates(pstate, qry);
  }

  return qry;
}

   
                           
                                                                         
   
                                                                          
                                              
   
static Query *
transformValuesClause(ParseState *pstate, SelectStmt *stmt)
{
  Query *qry = makeNode(Query);
  List *exprsLists = NIL;
  List *coltypes = NIL;
  List *coltypmods = NIL;
  List *colcollations = NIL;
  List **colexprs = NULL;
  int sublist_length = -1;
  bool lateral = false;
  RangeTblEntry *rte;
  int rtindex;
  ListCell *lc;
  ListCell *lc2;
  int i;

  qry->commandType = CMD_SELECT;

                                                          
  Assert(stmt->distinctClause == NIL);
  Assert(stmt->intoClause == NULL);
  Assert(stmt->targetList == NIL);
  Assert(stmt->fromClause == NIL);
  Assert(stmt->whereClause == NULL);
  Assert(stmt->groupClause == NIL);
  Assert(stmt->havingClause == NULL);
  Assert(stmt->windowClause == NIL);
  Assert(stmt->op == SETOP_NONE);

                                                         
  if (stmt->withClause)
  {
    qry->hasRecursive = stmt->withClause->recursive;
    qry->cteList = transformWithClause(pstate, stmt->withClause);
    qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
  }

     
                                                            
     
                                                                            
                                                                           
            
     
  foreach (lc, stmt->valuesLists)
  {
    List *sublist = (List *)lfirst(lc);

       
                                                                          
                                 
       
    sublist = transformExpressionList(pstate, sublist, EXPR_KIND_VALUES, false);

       
                                                                        
                                                                           
                                  
       
    if (sublist_length < 0)
    {
                                                                
      sublist_length = list_length(sublist);
                                                   
      colexprs = (List **)palloc0(sublist_length * sizeof(List *));
    }
    else if (sublist_length != list_length(sublist))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("VALUES lists must all be the same length"), parser_errposition(pstate, exprLocation((Node *)sublist))));
    }

                                           
    i = 0;
    foreach (lc2, sublist)
    {
      Node *col = (Node *)lfirst(lc2);

      colexprs[i] = lappend(colexprs[i], col);
      i++;
    }

                                                 
    list_free(sublist);

                                                    
    exprsLists = lappend(exprsLists, NIL);
  }

     
                                                                           
                                                                            
                          
     
                                                                             
                                                                             
                                                                      
                                                                           
                                                                             
                                                                      
     
                                                              
     
  for (i = 0; i < sublist_length; i++)
  {
    Oid coltype;
    int32 coltypmod = -1;
    Oid colcoll;
    bool first = true;

    coltype = select_common_type(pstate, colexprs[i], "VALUES", NULL);

    foreach (lc, colexprs[i])
    {
      Node *col = (Node *)lfirst(lc);

      col = coerce_to_common_type(pstate, col, coltype, "VALUES");
      lfirst(lc) = (void *)col;
      if (first)
      {
        coltypmod = exprTypmod(col);
        first = false;
      }
      else
      {
                                                                      
        if (coltypmod >= 0 && coltypmod != exprTypmod(col))
        {
          coltypmod = -1;
        }
      }
    }

    colcoll = select_common_collation(pstate, colexprs[i], true);

    coltypes = lappend_oid(coltypes, coltype);
    coltypmods = lappend_int(coltypmods, coltypmod);
    colcollations = lappend_oid(colcollations, colcoll);
  }

     
                                                                          
     
  for (i = 0; i < sublist_length; i++)
  {
    forboth(lc, colexprs[i], lc2, exprsLists)
    {
      Node *col = (Node *)lfirst(lc);
      List *sublist = lfirst(lc2);

      sublist = lappend(sublist, col);
      lfirst(lc2) = sublist;
    }
    list_free(colexprs[i]);
  }

     
                                                                        
                                                                           
                                                                          
                                     
     
  if (pstate->p_rtable != NIL && contain_vars_of_level((Node *)exprsLists, 0))
  {
    lateral = true;
  }

     
                             
     
  rte = addRangeTableEntryForValues(pstate, exprsLists, coltypes, coltypmods, colcollations, NULL, lateral, true);
  addRTEtoQuery(pstate, rte, true, true, true);

                                
  rtindex = list_length(pstate->p_rtable);
  Assert(rte == rt_fetch(rtindex, pstate->p_rtable));

     
                                                   
     
  Assert(pstate->p_next_resno == 1);
  qry->targetList = expandRelAttrs(pstate, rte, rtindex, 0, -1);

     
                                                                       
                      
     
  qry->sortClause = transformSortClause(pstate, stmt->sortClause, &qry->targetList, EXPR_KIND_ORDER_BY, false                        );

  qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset, EXPR_KIND_OFFSET, "OFFSET");
  qry->limitCount = transformLimitClause(pstate, stmt->limitCount, EXPR_KIND_LIMIT, "LIMIT");

  if (stmt->lockingClause)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s cannot be applied to VALUES", LCS_asString(((LockingClause *)linitial(stmt->lockingClause))->strength))));
  }

  qry->rtable = pstate->p_rtable;
  qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

  qry->hasSubLinks = pstate->p_hasSubLinks;

  assign_query_collations(pstate, qry);

  return qry;
}

   
                               
                                      
   
                                                                          
                                                                            
                                                                               
                                                                           
                        
   
static Query *
transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt)
{
  Query *qry = makeNode(Query);
  SelectStmt *leftmostSelect;
  int leftmostRTI;
  Query *leftmostQuery;
  SetOperationStmt *sostmt;
  List *sortClause;
  Node *limitOffset;
  Node *limitCount;
  List *lockingClause;
  WithClause *withClause;
  Node *node;
  ListCell *left_tlist, *lct, *lcm, *lcc, *l;
  List *targetvars, *targetnames, *sv_namespace;
  int sv_rtable_length;
  RangeTblEntry *jrte;
  int tllen;

  qry->commandType = CMD_SELECT;

     
                                                                          
                                                                         
                                                                        
                                                                        
                                                                         
                                                                   
     
  leftmostSelect = stmt->larg;
  while (leftmostSelect && leftmostSelect->op != SETOP_NONE)
  {
    leftmostSelect = leftmostSelect->larg;
  }
  Assert(leftmostSelect && IsA(leftmostSelect, SelectStmt) && leftmostSelect->larg == NULL);
  if (leftmostSelect->intoClause)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("SELECT ... INTO is not allowed here"), parser_errposition(pstate, exprLocation((Node *)leftmostSelect->intoClause))));
  }

     
                                                                          
                                                                          
                      
     
  sortClause = stmt->sortClause;
  limitOffset = stmt->limitOffset;
  limitCount = stmt->limitCount;
  lockingClause = stmt->lockingClause;
  withClause = stmt->withClause;

  stmt->sortClause = NIL;
  stmt->limitOffset = NULL;
  stmt->limitCount = NULL;
  stmt->lockingClause = NIL;
  stmt->withClause = NULL;

                                                                     
  if (lockingClause)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT", LCS_asString(((LockingClause *)linitial(lockingClause))->strength))));
  }

                                                         
  if (withClause)
  {
    qry->hasRecursive = withClause->recursive;
    qry->cteList = transformWithClause(pstate, withClause);
    qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
  }

     
                                                       
     
  sostmt = castNode(SetOperationStmt, transformSetOperationTree(pstate, stmt, true, NULL));
  Assert(sostmt);
  qry->setOperations = (Node *)sostmt;

     
                                                                  
     
  node = sostmt->larg;
  while (node && IsA(node, SetOperationStmt))
  {
    node = ((SetOperationStmt *)node)->larg;
  }
  Assert(node && IsA(node, RangeTblRef));
  leftmostRTI = ((RangeTblRef *)node)->rtindex;
  leftmostQuery = rt_fetch(leftmostRTI, pstate->p_rtable)->subquery;
  Assert(leftmostQuery != NULL);

     
                                                                     
                                                                    
                                                                           
                          
     
                                                                      
                                                                         
                                                                         
                                                       
     
  qry->targetList = NIL;
  targetvars = NIL;
  targetnames = NIL;

  forfour(lct, sostmt->colTypes, lcm, sostmt->colTypmods, lcc, sostmt->colCollations, left_tlist, leftmostQuery->targetList)
  {
    Oid colType = lfirst_oid(lct);
    int32 colTypmod = lfirst_int(lcm);
    Oid colCollation = lfirst_oid(lcc);
    TargetEntry *lefttle = (TargetEntry *)lfirst(left_tlist);
    char *colName;
    TargetEntry *tle;
    Var *var;

    Assert(!lefttle->resjunk);
    colName = pstrdup(lefttle->resname);
    var = makeVar(leftmostRTI, lefttle->resno, colType, colTypmod, colCollation, 0);
    var->location = exprLocation((Node *)lefttle->expr);
    tle = makeTargetEntry((Expr *)var, (AttrNumber)pstate->p_next_resno++, colName, false);
    qry->targetList = lappend(qry->targetList, tle);
    targetvars = lappend(targetvars, var);
    targetnames = lappend(targetnames, makeString(colName));
  }

     
                                                                          
                                                                         
                                                                          
                                                         
     
                                                                         
                                                                         
                      
     
  sv_rtable_length = list_length(pstate->p_rtable);

  jrte = addRangeTableEntryForJoin(pstate, targetnames, JOIN_INNER, targetvars, NULL, false);

  sv_namespace = pstate->p_namespace;
  pstate->p_namespace = NIL;

                                         
  addRTEtoQuery(pstate, jrte, false, false, true);

     
                                                                       
                                                                      
                                                                             
                                                         
     
  tllen = list_length(qry->targetList);

  qry->sortClause = transformSortClause(pstate, sortClause, &qry->targetList, EXPR_KIND_ORDER_BY, false                        );

                                                  
  pstate->p_namespace = sv_namespace;
  pstate->p_rtable = list_truncate(pstate->p_rtable, sv_rtable_length);

  if (tllen != list_length(qry->targetList))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("invalid UNION/INTERSECT/EXCEPT ORDER BY clause"), errdetail("Only result column names can be used, not expressions or functions."), errhint("Add the expression/function to every SELECT, or move the UNION into a FROM clause."), parser_errposition(pstate, exprLocation(list_nth(qry->targetList, tllen)))));
  }

  qry->limitOffset = transformLimitClause(pstate, limitOffset, EXPR_KIND_OFFSET, "OFFSET");
  qry->limitCount = transformLimitClause(pstate, limitCount, EXPR_KIND_LIMIT, "LIMIT");

  qry->rtable = pstate->p_rtable;
  qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

  qry->hasSubLinks = pstate->p_hasSubLinks;
  qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
  qry->hasTargetSRFs = pstate->p_hasTargetSRFs;
  qry->hasAggs = pstate->p_hasAggs;

  foreach (l, lockingClause)
  {
    transformLockingClause(pstate, qry, (LockingClause *)lfirst(l), false);
  }

  assign_query_collations(pstate, qry);

                                                                            
  if (pstate->p_hasAggs || qry->groupClause || qry->groupingSets || qry->havingQual)
  {
    parseCheckAggregates(pstate, qry);
  }

  return qry;
}

   
                             
                                                                     
   
                                                                           
                                                                           
                                                                           
                                                                            
                                                                             
                                                                             
                                                                       
                                                                        
                                                           
   
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt, bool isTopLevel, List **targetlist)
{
  bool isLeaf;

  Assert(stmt && IsA(stmt, SelectStmt));

                                                                          
  check_stack_depth();

     
                                                                       
     
  if (stmt->intoClause)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("INTO is only allowed on first SELECT of UNION/INTERSECT/EXCEPT"), parser_errposition(pstate, exprLocation((Node *)stmt->intoClause))));
  }

                                                                     
  if (stmt->lockingClause)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT", LCS_asString(((LockingClause *)linitial(stmt->lockingClause))->strength))));
  }

     
                                                                           
                                                                       
                                                                   
                                                                         
     
  if (stmt->op == SETOP_NONE)
  {
    Assert(stmt->larg == NULL && stmt->rarg == NULL);
    isLeaf = true;
  }
  else
  {
    Assert(stmt->larg != NULL && stmt->rarg != NULL);
    if (stmt->sortClause || stmt->limitOffset || stmt->limitCount || stmt->lockingClause || stmt->withClause)
    {
      isLeaf = true;
    }
    else
    {
      isLeaf = false;
    }
  }

  if (isLeaf)
  {
                             
    Query *selectQuery;
    char selectName[32];
    RangeTblEntry *rte PG_USED_FOR_ASSERTS_ONLY;
    RangeTblRef *rtr;
    ListCell *tl;

       
                                          
       
                                                                           
                                                                          
                                                                    
                                                                           
                                                              
                                    
       
                                                                         
                                                                        
                       
       
    selectQuery = parse_sub_analyze((Node *)stmt, pstate, NULL, false, false);

       
                                                                          
                                                                    
                                                                          
                      
       
    if (pstate->p_namespace)
    {
      if (contain_vars_of_level((Node *)selectQuery, 1))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("UNION/INTERSECT/EXCEPT member statement cannot refer to other relations of same query level"), parser_errposition(pstate, locate_var_of_level((Node *)selectQuery, 1))));
      }
    }

       
                                                                       
       
    if (targetlist)
    {
      *targetlist = NIL;
      foreach (tl, selectQuery->targetList)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(tl);

        if (!tle->resjunk)
        {
          *targetlist = lappend(*targetlist, tle);
        }
      }
    }

       
                                                                      
       
    snprintf(selectName, sizeof(selectName), "*SELECT* %d", list_length(pstate->p_rtable) + 1);
    rte = addRangeTableEntryForSubquery(pstate, selectQuery, makeAlias(selectName, NIL), false, false);

       
                                                                          
       
    rtr = makeNode(RangeTblRef);
                                  
    rtr->rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
    return (Node *)rtr;
  }
  else
  {
                                                       
    SetOperationStmt *op = makeNode(SetOperationStmt);
    List *ltargetlist;
    List *rtargetlist;
    ListCell *ltl;
    ListCell *rtl;
    const char *context;

    context = (stmt->op == SETOP_UNION ? "UNION" : (stmt->op == SETOP_INTERSECT ? "INTERSECT" : "EXCEPT"));

    op->op = stmt->op;
    op->all = stmt->all;

       
                                                  
       
    op->larg = transformSetOperationTree(pstate, stmt->larg, false, &ltargetlist);

       
                                                                        
                                                                    
                                                                         
                                                        
       
    if (isTopLevel && pstate->p_parent_cte && pstate->p_parent_cte->cterecursive)
    {
      determineRecursiveColTypes(pstate, op->larg, ltargetlist);
    }

       
                                                   
       
    op->rarg = transformSetOperationTree(pstate, stmt->rarg, false, &rtargetlist);

       
                                                                     
                                                                      
       
    if (list_length(ltargetlist) != list_length(rtargetlist))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("each %s query must have the same number of columns", context), parser_errposition(pstate, exprLocation((Node *)rtargetlist))));
    }

    if (targetlist)
    {
      *targetlist = NIL;
    }
    op->colTypes = NIL;
    op->colTypmods = NIL;
    op->colCollations = NIL;
    op->groupClauses = NIL;
    forboth(ltl, ltargetlist, rtl, rtargetlist)
    {
      TargetEntry *ltle = (TargetEntry *)lfirst(ltl);
      TargetEntry *rtle = (TargetEntry *)lfirst(rtl);
      Node *lcolnode = (Node *)ltle->expr;
      Node *rcolnode = (Node *)rtle->expr;
      Oid lcoltype = exprType(lcolnode);
      Oid rcoltype = exprType(rcolnode);
      int32 lcoltypmod = exprTypmod(lcolnode);
      int32 rcoltypmod = exprTypmod(rcolnode);
      Node *bestexpr;
      int bestlocation;
      Oid rescoltype;
      int32 rescoltypmod;
      Oid rescolcoll;

                                                  
      rescoltype = select_common_type(pstate, list_make2(lcolnode, rcolnode), context, &bestexpr);
      bestlocation = exprLocation(bestexpr);
                                                                  
      if (lcoltype == rcoltype && lcoltypmod == rcoltypmod)
      {
        rescoltypmod = lcoltypmod;
      }
      else
      {
        rescoltypmod = -1;
      }

         
                                                                        
                                                                        
                                                      
         
                                                                       
                                                                       
                                  
         
                                                                     
                                                                        
                                                                    
                                                                       
                                                                       
                                                                    
                                                                     
                                                                      
                                                                        
                                                                      
                                                                       
         
                                                                       
                                                                       
                                                                       
                                                                    
                                
         
      if (lcoltype != UNKNOWNOID)
      {
        lcolnode = coerce_to_common_type(pstate, lcolnode, rescoltype, context);
      }
      else if (IsA(lcolnode, Const) || IsA(lcolnode, Param))
      {
        lcolnode = coerce_to_common_type(pstate, lcolnode, rescoltype, context);
        ltle->expr = (Expr *)lcolnode;
      }

      if (rcoltype != UNKNOWNOID)
      {
        rcolnode = coerce_to_common_type(pstate, rcolnode, rescoltype, context);
      }
      else if (IsA(rcolnode, Const) || IsA(rcolnode, Param))
      {
        rcolnode = coerce_to_common_type(pstate, rcolnode, rescoltype, context);
        rtle->expr = (Expr *)rcolnode;
      }

         
                                                                      
                                                                      
                                                                        
                                                                     
                                                                        
                                                                        
                     
         
      rescolcoll = select_common_collation(pstate, list_make2(lcolnode, rcolnode), (op->op == SETOP_UNION && op->all));

                        
      op->colTypes = lappend_oid(op->colTypes, rescoltype);
      op->colTypmods = lappend_int(op->colTypmods, rescoltypmod);
      op->colCollations = lappend_oid(op->colCollations, rescolcoll);

         
                                                                         
                                                                     
                               
         
      if (op->op != SETOP_UNION || !op->all)
      {
        SortGroupClause *grpcl = makeNode(SortGroupClause);
        Oid sortop;
        Oid eqop;
        bool hashable;
        ParseCallbackState pcbstate;

        setup_parser_errposition_callback(&pcbstate, pstate, bestlocation);

                                                    
        get_sort_group_operators(rescoltype, false, true, false, &sortop, &eqop, NULL, &hashable);

        cancel_parser_errposition_callback(&pcbstate);

                                                                      
        grpcl->tleSortGroupRef = 0;
        grpcl->eqop = eqop;
        grpcl->sortop = sortop;
        grpcl->nulls_first = false;                                
        grpcl->hashable = hashable;

        op->groupClauses = lappend(op->groupClauses, grpcl);
      }

         
                                                                         
                                                                      
                                                                      
         
      if (targetlist)
      {
        SetToDefault *rescolnode = makeNode(SetToDefault);
        TargetEntry *restle;

        rescolnode->typeId = rescoltype;
        rescolnode->typeMod = rescoltypmod;
        rescolnode->collation = rescolcoll;
        rescolnode->location = bestlocation;
        restle = makeTargetEntry((Expr *)rescolnode, 0,                           
            NULL, false);
        *targetlist = lappend(*targetlist, restle);
      }
    }

    return (Node *)op;
  }
}

   
                                                                      
                                      
   
static void
determineRecursiveColTypes(ParseState *pstate, Node *larg, List *nrtargetlist)
{
  Node *node;
  int leftmostRTI;
  Query *leftmostQuery;
  List *targetList;
  ListCell *left_tlist;
  ListCell *nrtl;
  int next_resno;

     
                               
     
  node = larg;
  while (node && IsA(node, SetOperationStmt))
  {
    node = ((SetOperationStmt *)node)->larg;
  }
  Assert(node && IsA(node, RangeTblRef));
  leftmostRTI = ((RangeTblRef *)node)->rtindex;
  leftmostQuery = rt_fetch(leftmostRTI, pstate->p_rtable)->subquery;
  Assert(leftmostQuery != NULL);

     
                                                                         
                                                         
     
  targetList = NIL;
  next_resno = 1;

  forboth(nrtl, nrtargetlist, left_tlist, leftmostQuery->targetList)
  {
    TargetEntry *nrtle = (TargetEntry *)lfirst(nrtl);
    TargetEntry *lefttle = (TargetEntry *)lfirst(left_tlist);
    char *colName;
    TargetEntry *tle;

    Assert(!lefttle->resjunk);
    colName = pstrdup(lefttle->resname);
    tle = makeTargetEntry(nrtle->expr, next_resno++, colName, false);
    targetList = lappend(targetList, tle);
  }

                                                                 
  analyzeCTETargetList(pstate, pstate->p_parent_cte, targetList);
}

   
                         
                                    
   
static Query *
transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt)
{
  Query *qry = makeNode(Query);
  ParseNamespaceItem *nsitem;
  Node *qual;

  qry->commandType = CMD_UPDATE;
  pstate->p_is_insert = false;

                                                         
  if (stmt->withClause)
  {
    qry->hasRecursive = stmt->withClause->recursive;
    qry->cteList = transformWithClause(pstate, stmt->withClause);
    qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
  }

  qry->resultRelation = setTargetTable(pstate, stmt->relation, stmt->relation->inh, true, ACL_UPDATE);

                                                      
  nsitem = (ParseNamespaceItem *)llast(pstate->p_namespace);

                                                            
  nsitem->p_lateral_only = true;
  nsitem->p_lateral_ok = false;

     
                                                                          
                                                           
     
  transformFromClause(pstate, stmt->fromClause);

                                                                    
  nsitem->p_lateral_only = false;
  nsitem->p_lateral_ok = true;

  qual = transformWhereClause(pstate, stmt->whereClause, EXPR_KIND_WHERE, "WHERE");

  qry->returningList = transformReturningList(pstate, stmt->returningList);

     
                                                                      
                                                                      
     
  qry->targetList = transformUpdateTargetList(pstate, stmt->targetList);

  qry->rtable = pstate->p_rtable;
  qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

  qry->hasTargetSRFs = pstate->p_hasTargetSRFs;
  qry->hasSubLinks = pstate->p_hasSubLinks;

  assign_query_collations(pstate, qry);

  return qry;
}

   
                               
                                                             
   
static List *
transformUpdateTargetList(ParseState *pstate, List *origTlist)
{
  List *tlist = NIL;
  RangeTblEntry *target_rte;
  ListCell *orig_tl;
  ListCell *tl;

  tlist = transformTargetList(pstate, origTlist, EXPR_KIND_UPDATE_SOURCE);

                                                                      
  if (pstate->p_next_resno <= RelationGetNumberOfAttributes(pstate->p_target_relation))
  {
    pstate->p_next_resno = RelationGetNumberOfAttributes(pstate->p_target_relation) + 1;
  }

                                                               
  target_rte = pstate->p_target_rangetblentry;
  orig_tl = list_head(origTlist);

  foreach (tl, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tl);
    ResTarget *origTarget;
    int attrno;

    if (tle->resjunk)
    {
         
                                                                       
                                                                         
                                                                   
                 
         
      tle->resno = (AttrNumber)pstate->p_next_resno++;
      tle->resname = NULL;
      continue;
    }
    if (orig_tl == NULL)
    {
      elog(ERROR, "UPDATE target count mismatch --- internal error");
    }
    origTarget = lfirst_node(ResTarget, orig_tl);

    attrno = attnameAttNum(pstate->p_target_relation, origTarget->name, true);
    if (attrno == InvalidAttrNumber)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", origTarget->name, RelationGetRelationName(pstate->p_target_relation)), parser_errposition(pstate, origTarget->location)));
    }

    updateTargetListEntry(pstate, tle, origTarget->name, attrno, origTarget->indirection, origTarget->location);

                                                                
    target_rte->updatedCols = bms_add_member(target_rte->updatedCols, attrno - FirstLowInvalidHeapAttributeNumber);

    orig_tl = lnext(orig_tl);
  }
  if (orig_tl != NULL)
  {
    elog(ERROR, "UPDATE target count mismatch --- internal error");
  }

  return tlist;
}

   
                            
                                                     
   
static List *
transformReturningList(ParseState *pstate, List *returningList)
{
  List *rlist;
  int save_next_resno;

  if (returningList == NIL)
  {
    return NIL;                    
  }

     
                                                                          
                                                                      
                                                        
     
  save_next_resno = pstate->p_next_resno;
  pstate->p_next_resno = 1;

                                                              
  rlist = transformTargetList(pstate, returningList, EXPR_KIND_RETURNING);

     
                                                                           
                                                                          
                                                                           
                                                         
     
  if (rlist == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("RETURNING must have at least one column"), parser_errposition(pstate, exprLocation(linitial(returningList)))));
  }

                           
  markTargetListOrigins(pstate, rlist);

                                                                      
  if (pstate->p_resolve_unknowns)
  {
    resolveTargetListUnknowns(pstate, rlist);
  }

                     
  pstate->p_next_resno = save_next_resno;

  return rlist;
}

   
                                
                                        
   
                                                                           
                                                                          
                                                                               
                                                                               
                                                
   
static Query *
transformDeclareCursorStmt(ParseState *pstate, DeclareCursorStmt *stmt)
{
  Query *result;
  Query *query;

     
                                                           
     
  if ((stmt->options & CURSOR_OPT_SCROLL) && (stmt->options & CURSOR_OPT_NO_SCROLL))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_DEFINITION), errmsg("cannot specify both SCROLL and NO SCROLL")));
  }

                                                           
  query = transformStmt(pstate, stmt->query);
  stmt->query = (Node *)query;

                                                           
  if (!IsA(query, Query) || query->commandType != CMD_SELECT)
  {
    elog(ERROR, "unexpected non-SELECT command in DECLARE CURSOR");
  }

     
                                                                       
                                                                   
                  
     
  if (query->hasModifyingCTE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DECLARE CURSOR must not contain data-modifying statements in WITH")));
  }

                                                   
  if (query->rowMarks != NIL && (stmt->options & CURSOR_OPT_HOLD))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("DECLARE CURSOR WITH HOLD ... %s is not supported", LCS_asString(((RowMarkClause *)linitial(query->rowMarks))->strength)), errdetail("Holdable cursors must be READ ONLY.")));
  }

                                                
  if (query->rowMarks != NIL && (stmt->options & CURSOR_OPT_SCROLL))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("DECLARE SCROLL CURSOR ... %s is not supported", LCS_asString(((RowMarkClause *)linitial(query->rowMarks))->strength)), errdetail("Scrollable cursors must be READ ONLY.")));
  }

                                                     
  if (query->rowMarks != NIL && (stmt->options & CURSOR_OPT_INSENSITIVE))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("DECLARE INSENSITIVE CURSOR ... %s is not supported", LCS_asString(((RowMarkClause *)linitial(query->rowMarks))->strength)), errdetail("Insensitive cursors must be READ ONLY.")));
  }

                                                
  result = makeNode(Query);
  result->commandType = CMD_UTILITY;
  result->utilityStmt = (Node *)stmt;

  return result;
}

   
                          
                                  
   
                                                                    
                                                                          
                                                                               
                                                                               
                                                
   
static Query *
transformExplainStmt(ParseState *pstate, ExplainStmt *stmt)
{
  Query *result;

                                                       
  stmt->query = (Node *)transformOptionalSelectInto(pstate, stmt->query);

                                                
  result = makeNode(Query);
  result->commandType = CMD_UTILITY;
  result->utilityStmt = (Node *)stmt;

  return result;
}

   
                                
                                                                             
             
   
                                                                              
   
static Query *
transformCreateTableAsStmt(ParseState *pstate, CreateTableAsStmt *stmt)
{
  Query *result;
  Query *query;

                                                           
  query = transformStmt(pstate, stmt->query);
  stmt->query = (Node *)query;

                                                           
  if (stmt->relkind == OBJECT_MATVIEW)
  {
       
                                                                   
                                                                          
                                                                          
       
    if (query->hasModifyingCTE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialized views must not use data-modifying statements in WITH")));
    }

       
                                                                    
                                                                         
                                            
       
    if (isQueryUsingTempRelation(query))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialized views must not use temporary tables or views")));
    }

       
                                                                           
                                                                           
                                  
       
    if (query_contains_extern_params(query))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialized views may not be defined using bound parameters")));
    }

       
                                                                          
                                                                          
                                                                      
                                                                     
                
       
    if (stmt->into->rel->relpersistence == RELPERSISTENCE_UNLOGGED)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialized views cannot be unlogged")));
    }

       
                                                                           
                                                                          
                                                                    
                                 
       
    stmt->into->viewQuery = (Node *)copyObject(query);
  }

                                                
  result = makeNode(Query);
  result->commandType = CMD_UTILITY;
  result->utilityStmt = (Node *)stmt;

  return result;
}

   
                        
   
                                                                         
   
static Query *
transformCallStmt(ParseState *pstate, CallStmt *stmt)
{
  List *targs;
  ListCell *lc;
  Node *node;
  Query *result;

  targs = NIL;
  foreach (lc, stmt->funccall->args)
  {
    targs = lappend(targs, transformExpr(pstate, (Node *)lfirst(lc), EXPR_KIND_CALL_ARGUMENT));
  }

  node = ParseFuncOrColumn(pstate, stmt->funccall->funcname, targs, pstate->p_last_srf, stmt->funccall, true, stmt->funccall->location);

  assign_expr_collations(pstate, node);

  stmt->funcexpr = castNode(FuncExpr, node);

  result = makeNode(Query);
  result->commandType = CMD_UTILITY;
  result->utilityStmt = (Node *)stmt;

  return result;
}

   
                                                                  
                                                               
   
const char *
LCS_asString(LockClauseStrength strength)
{
  switch (strength)
  {
  case LCS_NONE:
    Assert(false);
    break;
  case LCS_FORKEYSHARE:
    return "FOR KEY SHARE";
  case LCS_FORSHARE:
    return "FOR SHARE";
  case LCS_FORNOKEYUPDATE:
    return "FOR NO KEY UPDATE";
  case LCS_FORUPDATE:
    return "FOR UPDATE";
  }
  return "FOR some";                       
}

   
                                                                          
   
                                                                          
   
void
CheckSelectLocking(Query *qry, LockClauseStrength strength)
{
  Assert(strength != LCS_NONE);                        

  if (qry->setOperations)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT", LCS_asString(strength))));
  }
  if (qry->distinctClause != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with DISTINCT clause", LCS_asString(strength))));
  }
  if (qry->groupClause != NIL || qry->groupingSets != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with GROUP BY clause", LCS_asString(strength))));
  }
  if (qry->havingQual != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with HAVING clause", LCS_asString(strength))));
  }
  if (qry->hasAggs)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with aggregate functions", LCS_asString(strength))));
  }
  if (qry->hasWindowFuncs)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with window functions", LCS_asString(strength))));
  }
  if (qry->hasTargetSRFs)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                
                                                                                         
                       errmsg("%s is not allowed with set-returning functions in the target list", LCS_asString(strength))));
  }
}

   
                                             
   
                                                              
   
                                                                  
                                                                   
   
static void
transformLockingClause(ParseState *pstate, Query *qry, LockingClause *lc, bool pushedDown)
{
  List *lockedRels = lc->lockedRels;
  ListCell *l;
  ListCell *rt;
  Index i;
  LockingClause *allrels;

  CheckSelectLocking(qry, lc->strength);

                                                                       
  allrels = makeNode(LockingClause);
  allrels->lockedRels = NIL;                         
  allrels->strength = lc->strength;
  allrels->waitPolicy = lc->waitPolicy;

  if (lockedRels == NIL)
  {
       
                                                                     
                                                                         
                                                                           
                                                                           
                                                                    
                                                                  
       
    i = 0;
    foreach (rt, qry->rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(rt);

      ++i;
      if (!rte->inFromCl)
      {
        continue;
      }
      switch (rte->rtekind)
      {
      case RTE_RELATION:
        applyLockingClause(qry, i, lc->strength, lc->waitPolicy, pushedDown);
        rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
        break;
      case RTE_SUBQUERY:
        applyLockingClause(qry, i, lc->strength, lc->waitPolicy, pushedDown);

           
                                                                
                                                                   
                                                                 
                                                                   
                                                     
           
        transformLockingClause(pstate, rte->subquery, allrels, true);
        break;
      default:
                                                              
        break;
      }
    }
  }
  else
  {
       
                                                                        
                                                                    
                                            
       
    foreach (l, lockedRels)
    {
      RangeVar *thisrel = (RangeVar *)lfirst(l);

                                                                    
      if (thisrel->catalogname || thisrel->schemaname)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                    
                                                                                             
                           errmsg("%s must specify unqualified relation names", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
      }

      i = 0;
      foreach (rt, qry->rtable)
      {
        RangeTblEntry *rte = (RangeTblEntry *)lfirst(rt);

        ++i;
        if (!rte->inFromCl)
        {
          continue;
        }

           
                                                                    
                                                                   
                                              
           
        if (rte->rtekind == RTE_JOIN && rte->alias == NULL)
        {
          continue;
        }

        if (strcmp(rte->eref->aliasname, thisrel->relname) == 0)
        {
          switch (rte->rtekind)
          {
          case RTE_RELATION:
            applyLockingClause(qry, i, lc->strength, lc->waitPolicy, pushedDown);
            rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
            break;
          case RTE_SUBQUERY:
            applyLockingClause(qry, i, lc->strength, lc->waitPolicy, pushedDown);
                                   
            transformLockingClause(pstate, rte->subquery, allrels, true);
            break;
          case RTE_JOIN:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                        
                                                                                                 
                               errmsg("%s cannot be applied to a join", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
            break;
          case RTE_FUNCTION:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                        
                                                                                                 
                               errmsg("%s cannot be applied to a function", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
            break;
          case RTE_TABLEFUNC:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                        
                                                                                                 
                               errmsg("%s cannot be applied to a table function", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
            break;
          case RTE_VALUES:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                        
                                                                                                 
                               errmsg("%s cannot be applied to VALUES", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
            break;
          case RTE_CTE:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                        
                                                                                                 
                               errmsg("%s cannot be applied to a WITH query", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
            break;
          case RTE_NAMEDTUPLESTORE:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                        
                                                                                                 
                               errmsg("%s cannot be applied to a named tuplestore", LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
            break;

                                                              

          default:
            elog(ERROR, "unrecognized RTE type: %d", (int)rte->rtekind);
            break;
          }
          break;                          
        }
      }
      if (rt == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE),
                                    
                                                                                             
                           errmsg("relation \"%s\" in %s clause not found in FROM clause", thisrel->relname, LCS_asString(lc->strength)), parser_errposition(pstate, thisrel->location)));
      }
    }
  }
}

   
                                                    
   
void
applyLockingClause(Query *qry, Index rtindex, LockClauseStrength strength, LockWaitPolicy waitPolicy, bool pushedDown)
{
  RowMarkClause *rc;

  Assert(strength != LCS_NONE);                        

                                                                   
  if (!pushedDown)
  {
    qry->hasForUpdate = true;
  }

                                                     
  if ((rc = get_parse_rowmark(qry, rtindex)) != NULL)
  {
       
                                                                         
                                                                           
                                                                         
                
       
                                                                       
                                                                         
                                                                         
                                                                           
                                                                         
                                                                       
                                                                        
                                                                      
                                                                      
                
       
                                                                         
       
    rc->strength = Max(rc->strength, strength);
    rc->waitPolicy = Max(rc->waitPolicy, waitPolicy);
    rc->pushedDown &= pushedDown;
    return;
  }

                                
  rc = makeNode(RowMarkClause);
  rc->rti = rtindex;
  rc->strength = strength;
  rc->waitPolicy = waitPolicy;
  rc->pushedDown = pushedDown;
  qry->rowMarks = lappend(qry->rowMarks, rc);
}

   
                                                      
   
                                                                              
                                                                               
                                                                             
                                                       
   
#ifdef RAW_EXPRESSION_COVERAGE_TEST

static bool
test_raw_expression_coverage(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  return raw_expression_tree_walker(node, test_raw_expression_coverage, context);
}

#endif                                   
