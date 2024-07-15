                                                                            
   
                  
                              
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"

#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/table.h"
#include "access/tsmapi.h"
#include "catalog/catalog.h"
#include "catalog/heap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "parser/parser.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/catcache.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/rel.h"

                                                                    
#define makeDefaultNSItem(rte) makeNamespaceItem(rte, true, true, false, true)

static void
extractRemainingColumns(List *common_colnames, List *src_colnames, List *src_colvars, List **res_colnames, List **res_colvars);
static Node *
transformJoinUsingClause(ParseState *pstate, RangeTblEntry *leftRTE, RangeTblEntry *rightRTE, List *leftVars, List *rightVars);
static Node *
transformJoinOnClause(ParseState *pstate, JoinExpr *j, List *namespace);
static RangeTblEntry *
getRTEForSpecialRelationTypes(ParseState *pstate, RangeVar *rv);
static RangeTblEntry *
transformTableEntry(ParseState *pstate, RangeVar *r);
static RangeTblEntry *
transformRangeSubselect(ParseState *pstate, RangeSubselect *r);
static RangeTblEntry *
transformRangeFunction(ParseState *pstate, RangeFunction *r);
static RangeTblEntry *
transformRangeTableFunc(ParseState *pstate, RangeTableFunc *t);
static TableSampleClause *
transformRangeTableSample(ParseState *pstate, RangeTableSample *rts);
static Node *
transformFromClauseItem(ParseState *pstate, Node *n, RangeTblEntry **top_rte, int *top_rti, List **namespace);
static Node *
buildMergedJoinVar(ParseState *pstate, JoinType jointype, Var *l_colvar, Var *r_colvar);
static ParseNamespaceItem *
makeNamespaceItem(RangeTblEntry *rte, bool rel_visible, bool cols_visible, bool lateral_only, bool lateral_ok);
static void
setNamespaceColumnVisibility(List *namespace, bool cols_visible);
static void
setNamespaceLateralState(List *namespace, bool lateral_only, bool lateral_ok);
static void
checkExprIsVarFree(ParseState *pstate, Node *n, const char *constructName);
static TargetEntry *
findTargetlistEntrySQL92(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind);
static TargetEntry *
findTargetlistEntrySQL99(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind);
static int
get_matching_location(int sortgroupref, List *sortgrouprefs, List *exprs);
static List *
resolve_unique_index_expr(ParseState *pstate, InferClause *infer, Relation heapRel);
static List *
addTargetToGroupList(ParseState *pstate, TargetEntry *tle, List *grouplist, List *targetlist, int location);
static WindowClause *
findWindowClause(List *wclist, const char *name);
static Node *
transformFrameOffset(ParseState *pstate, int frameOptions, Oid rangeopfamily, Oid rangeopcintype, Oid *inRangeFunc, Node *clause);

   
                         
                                                                       
                              
   
                                                                           
                                                              
                                                                            
                                                 
   
void
transformFromClause(ParseState *pstate, List *frmList)
{
  ListCell *fl;

     
                                                                          
                                                                           
                                                                           
                                    
     
                                                                         
                         
     
  foreach (fl, frmList)
  {
    Node *n = lfirst(fl);
    RangeTblEntry *rte;
    int rtindex;
    List *namespace;

    n = transformFromClauseItem(pstate, n, &rte, &rtindex, &namespace);

    checkNameSpaceConflicts(pstate, pstate->p_namespace, namespace);

                                                                 
    setNamespaceLateralState(namespace, true, true);

    pstate->p_joinlist = lappend(pstate->p_joinlist, n);
    pstate->p_namespace = list_concat(pstate->p_namespace, namespace);
  }

     
                                                                   
                                                                           
                                                                            
                                                  
     
  setNamespaceLateralState(pstate->p_namespace, false, true);
}

   
                  
                                                                         
                                                         
   
                                                                      
                                                                           
                                                                           
                                          
   
                                                                       
                                                                       
                                                                      
                                                                     
                                                                       
                                      
   
                                                                          
                       
   
                                                          
   
int
setTargetTable(ParseState *pstate, RangeVar *relation, bool inh, bool alsoSource, AclMode requiredPerms)
{
  RangeTblEntry *rte;
  int rtindex;

     
                                                                            
                                                             
     
  if (relation->schemaname == NULL && scanNameSpaceForENR(pstate, relation->relname))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("relation \"%s\" cannot be the target of a modifying statement", relation->relname)));
  }

                                                                       
  if (pstate->p_target_relation != NULL)
  {
    table_close(pstate->p_target_relation, NoLock);
  }

     
                                                                            
                   
     
                                                                           
                                 
     
  pstate->p_target_relation = parserOpenTable(pstate, relation, RowExclusiveLock);

     
                       
     
  rte = addRangeTableEntryForRelation(pstate, pstate->p_target_relation, RowExclusiveLock, relation->alias, inh, false);
  pstate->p_target_rangetblentry = rte;

                                
  rtindex = list_length(pstate->p_rtable);
  Assert(rte == rt_fetch(rtindex, pstate->p_rtable));

     
                                                                             
                                                                  
                  
     
                                                                    
                                                              
                                           
     
  rte->requiredPerms = requiredPerms;

     
                                                            
     
                                                                           
                                                                             
                                                       
     
  if (alsoSource)
  {
    addRTEtoQuery(pstate, rte, true, true, true);
  }

  return rtindex;
}

   
                                                                         
   
static void
extractRemainingColumns(List *common_colnames, List *src_colnames, List *src_colvars, List **res_colnames, List **res_colvars)
{
  List *new_colnames = NIL;
  List *new_colvars = NIL;
  ListCell *lnames, *lvars;

  Assert(list_length(src_colnames) == list_length(src_colvars));

  forboth(lnames, src_colnames, lvars, src_colvars)
  {
    char *colname = strVal(lfirst(lnames));
    bool match = false;
    ListCell *cnames;

    foreach (cnames, common_colnames)
    {
      char *ccolname = strVal(lfirst(cnames));

      if (strcmp(colname, ccolname) == 0)
      {
        match = true;
        break;
      }
    }

    if (!match)
    {
      new_colnames = lappend(new_colnames, lfirst(lnames));
      new_colvars = lappend(new_colvars, lfirst(lvars));
    }
  }

  *res_colnames = new_colnames;
  *res_colvars = new_colvars;
}

                              
                                                                         
                                                                            
                                                       
   
static Node *
transformJoinUsingClause(ParseState *pstate, RangeTblEntry *leftRTE, RangeTblEntry *rightRTE, List *leftVars, List *rightVars)
{
  Node *result;
  List *andargs = NIL;
  ListCell *lvars, *rvars;

     
                                                                           
                                                                             
                                                                        
                                                                          
                                                                           
                                  
     
  forboth(lvars, leftVars, rvars, rightVars)
  {
    Var *lvar = (Var *)lfirst(lvars);
    Var *rvar = (Var *)lfirst(rvars);
    A_Expr *e;

                                                   
    markVarForSelectPriv(pstate, lvar, leftRTE);
    markVarForSelectPriv(pstate, rvar, rightRTE);

                                                   
    e = makeSimpleA_Expr(AEXPR_OP, "=", (Node *)copyObject(lvar), (Node *)copyObject(rvar), -1);

                                                                         
    andargs = lappend(andargs, e);
  }

                                                             
  if (list_length(andargs) == 1)
  {
    result = (Node *)linitial(andargs);
  }
  else
  {
    result = (Node *)makeBoolExpr(AND_EXPR, andargs, -1);
  }

     
                                                                             
                                                                  
                                                                          
                                    
     
  result = transformExpr(pstate, result, EXPR_KIND_JOIN_USING);

  result = coerce_to_boolean(pstate, result, "JOIN/USING");

  return result;
}

                           
                                                
                                                       
   
static Node *
transformJoinOnClause(ParseState *pstate, JoinExpr *j, List *namespace)
{
  Node *result;
  List *save_namespace;

     
                                                                       
                                                                      
                                                                             
                                                                        
                                                                         
                    
     
  setNamespaceLateralState(namespace, false, true);

  save_namespace = pstate->p_namespace;
  pstate->p_namespace = namespace;

  result = transformWhereClause(pstate, j->quals, EXPR_KIND_JOIN_ON, "JOIN/ON");

  pstate->p_namespace = save_namespace;

  return result;
}

   
                                                                            
   
static RangeTblEntry *
transformTableEntry(ParseState *pstate, RangeVar *r)
{
  RangeTblEntry *rte;

                                              
  rte = addRangeTableEntry(pstate, r, r->alias, r->inh, true);

  return rte;
}

   
                                                                        
   
static RangeTblEntry *
transformRangeSubselect(ParseState *pstate, RangeSubselect *r)
{
  Query *query;
  RangeTblEntry *rte;

     
                                                                             
                                                                    
                                                                        
                                                                          
                                                                           
     
  if (r->alias == NULL)
  {
    elog(ERROR, "subquery in FROM must have an alias");
  }

     
                                                                           
                                                                          
                 
     
  Assert(pstate->p_expr_kind == EXPR_KIND_NONE);
  pstate->p_expr_kind = EXPR_KIND_FROM_SUBSELECT;

     
                                                                        
                                                                             
                                          
     
  Assert(!pstate->p_lateral_active);
  pstate->p_lateral_active = r->lateral;

     
                                         
     
  query = parse_sub_analyze(r->subquery, pstate, NULL, isLockedRefname(pstate, r->alias->aliasname), true);

                     
  pstate->p_lateral_active = false;
  pstate->p_expr_kind = EXPR_KIND_NONE;

     
                                                                           
                                                    
     
  if (!IsA(query, Query) || query->commandType != CMD_SELECT)
  {
    elog(ERROR, "unexpected non-SELECT command in subquery in FROM");
  }

     
                                        
     
  rte = addRangeTableEntryForSubquery(pstate, query, r->alias, r->lateral, true);

  return rte;
}

   
                                                                          
   
static RangeTblEntry *
transformRangeFunction(ParseState *pstate, RangeFunction *r)
{
  List *funcexprs = NIL;
  List *funcnames = NIL;
  List *coldeflists = NIL;
  bool is_lateral;
  RangeTblEntry *rte;
  ListCell *lc;

     
                                                                          
                                                                         
                                                                  
                                                    
     
                                                                        
                               
     
  Assert(!pstate->p_lateral_active);
  pstate->p_lateral_active = true;

     
                                    
     
                                                                            
                                                                             
                                                                     
                                                                          
                 
     
                                                                           
                                                                            
                                    
     
                                                                       
                                                                     
     
  foreach (lc, r->functions)
  {
    List *pair = (List *)lfirst(lc);
    Node *fexpr;
    List *coldeflist;
    Node *newfexpr;
    Node *last_srf;

                                                             
    Assert(list_length(pair) == 2);
    fexpr = (Node *)linitial(pair);
    coldeflist = (List *)lsecond(pair);

       
                                                                           
                                                                           
                                                                           
                                                                    
       
                                                                     
                                                                           
                                                                           
                                                                       
                                                                        
                                                     
       
                                                                        
                                                                      
                                                                        
                                                                  
       
    if (IsA(fexpr, FuncCall))
    {
      FuncCall *fc = (FuncCall *)fexpr;

      if (list_length(fc->funcname) == 1 && strcmp(strVal(linitial(fc->funcname)), "unnest") == 0 && list_length(fc->args) > 1 && fc->agg_order == NIL && fc->agg_filter == NULL && !fc->agg_star && !fc->agg_distinct && !fc->func_variadic && fc->over == NULL && coldeflist == NIL)
      {
        ListCell *lc;

        foreach (lc, fc->args)
        {
          Node *arg = (Node *)lfirst(lc);
          FuncCall *newfc;

          last_srf = pstate->p_last_srf;

          newfc = makeFuncCall(SystemFuncName("unnest"), list_make1(arg), fc->location);

          newfexpr = transformExpr(pstate, (Node *)newfc, EXPR_KIND_FROM_FUNCTION);

                                                                   
          if (pstate->p_last_srf != last_srf && pstate->p_last_srf != newfexpr)
          {
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-returning functions must appear at top level of FROM"), parser_errposition(pstate, exprLocation(pstate->p_last_srf))));
          }

          funcexprs = lappend(funcexprs, newfexpr);

          funcnames = lappend(funcnames, FigureColname((Node *)newfc));

                                                            

          coldeflists = lappend(coldeflists, coldeflist);
        }
        continue;                                   
      }
    }

                         
    last_srf = pstate->p_last_srf;

    newfexpr = transformExpr(pstate, fexpr, EXPR_KIND_FROM_FUNCTION);

                                                             
    if (pstate->p_last_srf != last_srf && pstate->p_last_srf != newfexpr)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-returning functions must appear at top level of FROM"), parser_errposition(pstate, exprLocation(pstate->p_last_srf))));
    }

    funcexprs = lappend(funcexprs, newfexpr);

    funcnames = lappend(funcnames, FigureColname(fexpr));

    if (coldeflist && r->coldeflist)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("multiple column definition lists are not allowed for the same function"), parser_errposition(pstate, exprLocation((Node *)r->coldeflist))));
    }

    coldeflists = lappend(coldeflists, coldeflist);
  }

  pstate->p_lateral_active = false;

     
                                                                             
                                    
     
  assign_list_collations(pstate, funcexprs);

     
                                                                           
                                                             
     
                                                                          
                                                                   
                                                                           
                                                                            
                                                                        
                                          
     
  if (r->coldeflist)
  {
    if (list_length(funcexprs) != 1)
    {
      if (r->is_rowsfrom)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("ROWS FROM() with multiple functions cannot have a column definition list"), errhint("Put a separate column definition list for each function inside ROWS FROM()."), parser_errposition(pstate, exprLocation((Node *)r->coldeflist))));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("UNNEST() with multiple arguments cannot have a column definition list"), errhint("Use separate UNNEST() calls inside ROWS FROM(), and attach a column definition list to each one."), parser_errposition(pstate, exprLocation((Node *)r->coldeflist))));
      }
    }
    if (r->ordinality)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("WITH ORDINALITY cannot be used with a column definition list"), errhint("Put the column definition list inside ROWS FROM()."), parser_errposition(pstate, exprLocation((Node *)r->coldeflist))));
    }

    coldeflists = list_make1(r->coldeflist);
  }

     
                                                                        
                                                   
     
  is_lateral = r->lateral || contain_vars_of_level((Node *)funcexprs, 0);

     
                                        
     
  rte = addRangeTableEntryForFunction(pstate, funcnames, funcexprs, coldeflists, r, is_lateral, true);

  return rte;
}

   
                             
                                                    
   
                                                                            
                                                                         
                              
   
static RangeTblEntry *
transformRangeTableFunc(ParseState *pstate, RangeTableFunc *rtf)
{
  TableFunc *tf = makeNode(TableFunc);
  const char *constructName;
  Oid docType;
  RangeTblEntry *rte;
  bool is_lateral;
  ListCell *col;
  char **names;
  int colno;

                                            
  constructName = "XMLTABLE";
  docType = XMLOID;

     
                                                                          
                                                                          
                                                                     
                        
     
                                                                        
                               
     
  Assert(!pstate->p_lateral_active);
  pstate->p_lateral_active = true;

                                                                         
  Assert(rtf->rowexpr != NULL);
  tf->rowexpr = coerce_to_specific_type(pstate, transformExpr(pstate, rtf->rowexpr, EXPR_KIND_FROM_FUNCTION), TEXTOID, constructName);
  assign_expr_collations(pstate, tf->rowexpr);

                                      
  Assert(rtf->docexpr != NULL);
  tf->docexpr = coerce_to_specific_type(pstate, transformExpr(pstate, rtf->docexpr, EXPR_KIND_FROM_FUNCTION), docType, constructName);
  assign_expr_collations(pstate, tf->docexpr);

                                      
  tf->ordinalitycol = -1;

                            
  names = palloc(sizeof(char *) * list_length(rtf->columns));

  colno = 0;
  foreach (col, rtf->columns)
  {
    RangeTableFuncCol *rawc = (RangeTableFuncCol *)lfirst(col);
    Oid typid;
    int32 typmod;
    Node *colexpr;
    Node *coldefexpr;
    int j;

    tf->colnames = lappend(tf->colnames, makeString(pstrdup(rawc->colname)));

       
                                                                        
                                                                    
       
    if (rawc->for_ordinality)
    {
      if (tf->ordinalitycol != -1)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("only one FOR ORDINALITY column is allowed"), parser_errposition(pstate, rawc->location)));
      }

      typid = INT4OID;
      typmod = -1;
      tf->ordinalitycol = colno;
    }
    else
    {
      if (rawc->typeName->setof)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" cannot be declared SETOF", rawc->colname), parser_errposition(pstate, rawc->location)));
      }

      typenameTypeIdAndMod(pstate, rawc->typeName, &typid, &typmod);
    }

    tf->coltypes = lappend_oid(tf->coltypes, typid);
    tf->coltypmods = lappend_int(tf->coltypmods, typmod);
    tf->colcollations = lappend_oid(tf->colcollations, get_typcollation(typid));

                                                    
    if (rawc->colexpr)
    {
      colexpr = coerce_to_specific_type(pstate, transformExpr(pstate, rawc->colexpr, EXPR_KIND_FROM_FUNCTION), TEXTOID, constructName);
      assign_expr_collations(pstate, colexpr);
    }
    else
    {
      colexpr = NULL;
    }

    if (rawc->coldefexpr)
    {
      coldefexpr = coerce_to_specific_type_typmod(pstate, transformExpr(pstate, rawc->coldefexpr, EXPR_KIND_FROM_FUNCTION), typid, typmod, constructName);
      assign_expr_collations(pstate, coldefexpr);
    }
    else
    {
      coldefexpr = NULL;
    }

    tf->colexprs = lappend(tf->colexprs, colexpr);
    tf->coldefexprs = lappend(tf->coldefexprs, coldefexpr);

    if (rawc->is_not_null)
    {
      tf->notnulls = bms_add_member(tf->notnulls, colno);
    }

                                           
    for (j = 0; j < colno; j++)
    {
      if (strcmp(names[j], rawc->colname) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("column name \"%s\" is not unique", rawc->colname), parser_errposition(pstate, rawc->location)));
      }
    }
    names[colno] = rawc->colname;

    colno++;
  }
  pfree(names);

                                                       
  if (rtf->namespaces != NIL)
  {
    ListCell *ns;
    ListCell *lc2;
    List *ns_uris = NIL;
    List *ns_names = NIL;
    bool default_ns_seen = false;

    foreach (ns, rtf->namespaces)
    {
      ResTarget *r = (ResTarget *)lfirst(ns);
      Node *ns_uri;

      Assert(IsA(r, ResTarget));
      ns_uri = transformExpr(pstate, r->val, EXPR_KIND_FROM_FUNCTION);
      ns_uri = coerce_to_specific_type(pstate, ns_uri, TEXTOID, constructName);
      assign_expr_collations(pstate, ns_uri);
      ns_uris = lappend(ns_uris, ns_uri);

                                                                       
      if (r->name != NULL)
      {
        foreach (lc2, ns_names)
        {
          Value *ns_node = (Value *)lfirst(lc2);

          if (ns_node == NULL)
          {
            continue;
          }
          if (strcmp(strVal(ns_node), r->name) == 0)
          {
            ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("namespace name \"%s\" is not unique", r->name), parser_errposition(pstate, r->location)));
          }
        }
      }
      else
      {
        if (default_ns_seen)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("only one default namespace is allowed"), parser_errposition(pstate, r->location)));
        }
        default_ns_seen = true;
      }

                                                  
      ns_names = lappend(ns_names, r->name ? makeString(r->name) : NULL);
    }

    tf->ns_uris = ns_uris;
    tf->ns_names = ns_names;
  }

  tf->location = rtf->location;

  pstate->p_lateral_active = false;

     
                                                                        
                                                   
     
  is_lateral = rtf->lateral || contain_vars_of_level((Node *)tf, 0);

  rte = addRangeTableEntryForTableFunc(pstate, tf, rtf->alias, is_lateral, true);

  return rte;
}

   
                                                                
   
                                                                          
                                                             
   
static TableSampleClause *
transformRangeTableSample(ParseState *pstate, RangeTableSample *rts)
{
  TableSampleClause *tablesample;
  Oid handlerOid;
  Oid funcargtypes[1];
  TsmRoutine *tsm;
  List *fargs;
  ListCell *larg, *ltyp;

     
                                                                             
                                                                          
                                                                            
                                                                            
                                                               
     
  funcargtypes[0] = INTERNALOID;

  handlerOid = LookupFuncName(rts->method, 1, funcargtypes, true);

                                                                            
  if (!OidIsValid(handlerOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablesample method %s does not exist", NameListToString(rts->method)), parser_errposition(pstate, rts->location)));
  }

                                                  
  if (get_func_rettype(handlerOid) != TSM_HANDLEROID)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("function %s must return type %s", NameListToString(rts->method), "tsm_handler"), parser_errposition(pstate, rts->location)));
  }

                                                                     
  tsm = GetTsmRoutine(handlerOid);

  tablesample = makeNode(TableSampleClause);
  tablesample->tsmhandler = handlerOid;

                                                            
  if (list_length(rts->args) != list_length(tsm->parameterTypes))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLESAMPLE_ARGUMENT), errmsg_plural("tablesample method %s requires %d argument, not %d", "tablesample method %s requires %d arguments, not %d", list_length(tsm->parameterTypes), NameListToString(rts->method), list_length(tsm->parameterTypes), list_length(rts->args)), parser_errposition(pstate, rts->location)));
  }

     
                                                                             
                                                                      
                                       
     
  fargs = NIL;
  forboth(larg, rts->args, ltyp, tsm->parameterTypes)
  {
    Node *arg = (Node *)lfirst(larg);
    Oid argtype = lfirst_oid(ltyp);

    arg = transformExpr(pstate, arg, EXPR_KIND_FROM_FUNCTION);
    arg = coerce_to_specific_type(pstate, arg, argtype, "TABLESAMPLE");
    assign_expr_collations(pstate, arg);
    fargs = lappend(fargs, arg);
  }
  tablesample->args = fargs;

                                 
  if (rts->repeatable != NULL)
  {
    Node *arg;

    if (!tsm->repeatable_across_queries)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tablesample method %s does not support REPEATABLE", NameListToString(rts->method)), parser_errposition(pstate, rts->location)));
    }

    arg = transformExpr(pstate, rts->repeatable, EXPR_KIND_FROM_FUNCTION);
    arg = coerce_to_specific_type(pstate, arg, FLOAT8OID, "REPEATABLE");
    assign_expr_collations(pstate, arg);
    tablesample->repeatable = (Expr *)arg;
  }
  else
  {
    tablesample->repeatable = NULL;
  }

  return tablesample;
}

   
                                 
   
                                                                   
                                                              
   
static RangeTblEntry *
getRTEForSpecialRelationTypes(ParseState *pstate, RangeVar *rv)
{
  CommonTableExpr *cte;
  Index levelsup;
  RangeTblEntry *rte;

     
                                                                          
     
  if (rv->schemaname)
  {
    return NULL;
  }

  cte = scanNameSpaceForCTE(pstate, rv->relname, &levelsup);
  if (cte)
  {
    rte = addRangeTableEntryForCTE(pstate, cte, levelsup, rv, true);
  }
  else if (scanNameSpaceForENR(pstate, rv->relname))
  {
    rte = addRangeTableEntryForENR(pstate, rv, true);
  }
  else
  {
    rte = NULL;
  }

  return rte;
}

   
                             
                                                                      
                                                                    
                                                                      
                                                                        
                                                                
   
                                                                   
                                                                
   
                                                                  
                                                                             
                                
   
                                                                 
   
                                                                           
                                                                               
                                                                             
   
static Node *
transformFromClauseItem(ParseState *pstate, Node *n, RangeTblEntry **top_rte, int *top_rti, List **namespace)
{
                                                               
  check_stack_depth();

  if (IsA(n, RangeVar))
  {
                                                              
    RangeVar *rv = (RangeVar *)n;
    RangeTblRef *rtr;
    RangeTblEntry *rte;
    int rtindex;

                                                     
    rte = getRTEForSpecialRelationTypes(pstate, rv);

                                                       
    if (!rte)
    {
      rte = transformTableEntry(pstate, rv);
    }

                                  
    rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
    *top_rte = rte;
    *top_rti = rtindex;
    *namespace = list_make1(makeDefaultNSItem(rte));
    rtr = makeNode(RangeTblRef);
    rtr->rtindex = rtindex;
    return (Node *)rtr;
  }
  else if (IsA(n, RangeSubselect))
  {
                                             
    RangeTblRef *rtr;
    RangeTblEntry *rte;
    int rtindex;

    rte = transformRangeSubselect(pstate, (RangeSubselect *)n);
                                  
    rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
    *top_rte = rte;
    *top_rti = rtindex;
    *namespace = list_make1(makeDefaultNSItem(rte));
    rtr = makeNode(RangeTblRef);
    rtr->rtindex = rtindex;
    return (Node *)rtr;
  }
  else if (IsA(n, RangeFunction))
  {
                                           
    RangeTblRef *rtr;
    RangeTblEntry *rte;
    int rtindex;

    rte = transformRangeFunction(pstate, (RangeFunction *)n);
                                  
    rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
    *top_rte = rte;
    *top_rti = rtindex;
    *namespace = list_make1(makeDefaultNSItem(rte));
    rtr = makeNode(RangeTblRef);
    rtr->rtindex = rtindex;
    return (Node *)rtr;
  }
  else if (IsA(n, RangeTableFunc))
  {
                                                 
    RangeTblRef *rtr;
    RangeTblEntry *rte;
    int rtindex;

    rte = transformRangeTableFunc(pstate, (RangeTableFunc *)n);
                                  
    rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
    *top_rte = rte;
    *top_rti = rtindex;
    *namespace = list_make1(makeDefaultNSItem(rte));
    rtr = makeNode(RangeTblRef);
    rtr->rtindex = rtindex;
    return (Node *)rtr;
  }
  else if (IsA(n, RangeTableSample))
  {
                                                                  
    RangeTableSample *rts = (RangeTableSample *)n;
    Node *rel;
    RangeTblRef *rtr;
    RangeTblEntry *rte;

                                                      
    rel = transformFromClauseItem(pstate, rts->relation, top_rte, top_rti, namespace);
                                                                          
    rtr = castNode(RangeTblRef, rel);
    rte = rt_fetch(rtr->rtindex, pstate->p_rtable);
                                                              
    if (rte->relkind != RELKIND_RELATION && rte->relkind != RELKIND_MATVIEW && rte->relkind != RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("TABLESAMPLE clause can only be applied to tables and materialized views"), parser_errposition(pstate, exprLocation(rts->relation))));
    }

                                                             
    rte->tablesample = transformRangeTableSample(pstate, rts);
    return (Node *)rtr;
  }
  else if (IsA(n, JoinExpr))
  {
                                      
    JoinExpr *j = (JoinExpr *)n;
    RangeTblEntry *l_rte;
    RangeTblEntry *r_rte;
    int l_rtindex;
    int r_rtindex;
    List *l_namespace, *r_namespace, *my_namespace, *l_colnames, *r_colnames, *res_colnames, *l_colvars, *r_colvars, *res_colvars;
    bool lateral_ok;
    int sv_namespace_length;
    RangeTblEntry *rte;
    int k;

       
                                                                         
                                                                      
       
    j->larg = transformFromClauseItem(pstate, j->larg, &l_rte, &l_rtindex, &l_namespace);

       
                                                                       
                                                                        
                                                                           
                                                                   
                                                                          
                                                                    
                                            
       
                                                                    
                                                                       
       
                                                                  
                                           
       
    lateral_ok = (j->jointype == JOIN_INNER || j->jointype == JOIN_LEFT);
    setNamespaceLateralState(l_namespace, true, lateral_ok);

    sv_namespace_length = list_length(pstate->p_namespace);
    pstate->p_namespace = list_concat(pstate->p_namespace, l_namespace);

                                        
    j->rarg = transformFromClauseItem(pstate, j->rarg, &r_rte, &r_rtindex, &r_namespace);

                                                                 
    pstate->p_namespace = list_truncate(pstate->p_namespace, sv_namespace_length);

       
                                                                          
                                                                  
                                  
       
    checkNameSpaceConflicts(pstate, l_namespace, r_namespace);

       
                                                                
       
    my_namespace = list_concat(l_namespace, r_namespace);

       
                                                            
       
                                                                
       
    expandRTE(l_rte, l_rtindex, 0, -1, false, &l_colnames, &l_colvars);
    expandRTE(r_rte, r_rtindex, 0, -1, false, &r_colnames, &r_colvars);

       
                                                                       
                                                                          
                                                                         
                                                                      
                                                                          
                                                                           
                   
       
    if (j->isNatural)
    {
      List *rlist = NIL;
      ListCell *lx, *rx;

      Assert(j->usingClause == NIL);                                 

      foreach (lx, l_colnames)
      {
        char *l_colname = strVal(lfirst(lx));
        Value *m_name = NULL;

        foreach (rx, r_colnames)
        {
          char *r_colname = strVal(lfirst(rx));

          if (strcmp(l_colname, r_colname) == 0)
          {
            m_name = makeString(l_colname);
            break;
          }
        }

                                                                 
        if (m_name != NULL)
        {
          rlist = lappend(rlist, m_name);
        }
      }

      j->usingClause = rlist;
    }

       
                                                      
       
    res_colnames = NIL;
    res_colvars = NIL;

    if (j->usingClause)
    {
         
                                                                       
                                                                        
                                
         
      List *ucols = j->usingClause;
      List *l_usingvars = NIL;
      List *r_usingvars = NIL;
      ListCell *ucol;

      Assert(j->quals == NULL);                              

      foreach (ucol, ucols)
      {
        char *u_colname = strVal(lfirst(ucol));
        ListCell *col;
        int ndx;
        int l_index = -1;
        int r_index = -1;
        Var *l_colvar, *r_colvar;

                                      
        foreach (col, res_colnames)
        {
          char *res_colname = strVal(lfirst(col));

          if (strcmp(res_colname, u_colname) == 0)
          {
            ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column name \"%s\" appears more than once in USING clause", u_colname)));
          }
        }

                                   
        ndx = 0;
        foreach (col, l_colnames)
        {
          char *l_colname = strVal(lfirst(col));

          if (strcmp(l_colname, u_colname) == 0)
          {
            if (l_index >= 0)
            {
              ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("common column name \"%s\" appears more than once in left table", u_colname)));
            }
            l_index = ndx;
          }
          ndx++;
        }
        if (l_index < 0)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" specified in USING clause does not exist in left table", u_colname)));
        }

                                    
        ndx = 0;
        foreach (col, r_colnames)
        {
          char *r_colname = strVal(lfirst(col));

          if (strcmp(r_colname, u_colname) == 0)
          {
            if (r_index >= 0)
            {
              ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("common column name \"%s\" appears more than once in right table", u_colname)));
            }
            r_index = ndx;
          }
          ndx++;
        }
        if (r_index < 0)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" specified in USING clause does not exist in right table", u_colname)));
        }

        l_colvar = list_nth(l_colvars, l_index);
        l_usingvars = lappend(l_usingvars, l_colvar);
        r_colvar = list_nth(r_colvars, r_index);
        r_usingvars = lappend(r_usingvars, r_colvar);

        res_colnames = lappend(res_colnames, lfirst(ucol));
        res_colvars = lappend(res_colvars, buildMergedJoinVar(pstate, j->jointype, l_colvar, r_colvar));
      }

      j->quals = transformJoinUsingClause(pstate, l_rte, r_rte, l_usingvars, r_usingvars);
    }
    else if (j->quals)
    {
                                                   
      j->quals = transformJoinOnClause(pstate, j, my_namespace);
    }
    else
    {
                                
    }

                                                                    
    extractRemainingColumns(res_colnames, l_colnames, l_colvars, &l_colnames, &l_colvars);
    extractRemainingColumns(res_colnames, r_colnames, r_colvars, &r_colnames, &r_colvars);
    res_colnames = list_concat(res_colnames, l_colnames);
    res_colvars = list_concat(res_colvars, l_colvars);
    res_colnames = list_concat(res_colnames, r_colnames);
    res_colvars = list_concat(res_colvars, r_colvars);

       
                                                   
       
    rte = addRangeTableEntryForJoin(pstate, res_colnames, j->jointype, res_colvars, j->alias, true);

                                  
    j->rtindex = list_length(pstate->p_rtable);
    Assert(rte == rt_fetch(j->rtindex, pstate->p_rtable));

    *top_rte = rte;
    *top_rti = j->rtindex;

                                                            
    for (k = list_length(pstate->p_joinexprs) + 1; k < j->rtindex; k++)
    {
      pstate->p_joinexprs = lappend(pstate->p_joinexprs, NULL);
    }
    pstate->p_joinexprs = lappend(pstate->p_joinexprs, j);
    Assert(list_length(pstate->p_joinexprs) == j->rtindex);

       
                                                                          
                                                                          
                                                                 
                                       
       
                                                                        
                                                                        
                                                                          
                                                          
       
    if (j->alias != NULL)
    {
      my_namespace = NIL;
    }
    else
    {
      setNamespaceColumnVisibility(my_namespace, false);
    }

       
                                                                         
                                                                        
       
    *namespace = lappend(my_namespace, makeNamespaceItem(rte, (j->alias != NULL), true, false, true));

    return (Node *)j;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(n));
  }
  return NULL;                                          
}

   
                        
                                                                         
   
static Node *
buildMergedJoinVar(ParseState *pstate, JoinType jointype, Var *l_colvar, Var *r_colvar)
{
  Oid outcoltype;
  int32 outcoltypmod;
  Node *l_node, *r_node, *res_node;

     
                                                       
     
  outcoltype = l_colvar->vartype;
  outcoltypmod = l_colvar->vartypmod;
  if (outcoltype != r_colvar->vartype)
  {
    outcoltype = select_common_type(pstate, list_make2(l_colvar, r_colvar), "JOIN/USING", NULL);
    outcoltypmod = -1;                  
  }
  else if (outcoltypmod != r_colvar->vartypmod)
  {
                                        
    outcoltypmod = -1;                  
  }

     
                                                                            
                                                                         
                                                                          
                                                           
     
  if (l_colvar->vartype != outcoltype)
  {
    l_node = coerce_type(pstate, (Node *)l_colvar, l_colvar->vartype, outcoltype, outcoltypmod, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
  }
  else if (l_colvar->vartypmod != outcoltypmod)
  {
    l_node = (Node *)makeRelabelType((Expr *)l_colvar, outcoltype, outcoltypmod, InvalidOid,                  
        COERCE_IMPLICIT_CAST);
  }
  else
  {
    l_node = (Node *)l_colvar;
  }

  if (r_colvar->vartype != outcoltype)
  {
    r_node = coerce_type(pstate, (Node *)r_colvar, r_colvar->vartype, outcoltype, outcoltypmod, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
  }
  else if (r_colvar->vartypmod != outcoltypmod)
  {
    r_node = (Node *)makeRelabelType((Expr *)r_colvar, outcoltype, outcoltypmod, InvalidOid,                  
        COERCE_IMPLICIT_CAST);
  }
  else
  {
    r_node = (Node *)r_colvar;
  }

     
                         
     
  switch (jointype)
  {
  case JOIN_INNER:

       
                                                                   
       
    if (IsA(l_node, Var))
    {
      res_node = l_node;
    }
    else if (IsA(r_node, Var))
    {
      res_node = r_node;
    }
    else
    {
      res_node = l_node;
    }
    break;
  case JOIN_LEFT:
                             
    res_node = l_node;
    break;
  case JOIN_RIGHT:
                              
    res_node = r_node;
    break;
  case JOIN_FULL:
  {
       
                                                                   
                                                   
       
    CoalesceExpr *c = makeNode(CoalesceExpr);

    c->coalescetype = outcoltype;
                                           
    c->args = list_make2(l_node, r_node);
    c->location = -1;
    res_node = (Node *)c;
    break;
  }
  default:
    elog(ERROR, "unrecognized join type: %d", (int)jointype);
    res_node = NULL;                          
    break;
  }

     
                                                                      
                                                                             
                                                                     
     
  assign_expr_collations(pstate, res_node);

  return res_node;
}

   
                       
                                                               
   
static ParseNamespaceItem *
makeNamespaceItem(RangeTblEntry *rte, bool rel_visible, bool cols_visible, bool lateral_only, bool lateral_ok)
{
  ParseNamespaceItem *nsitem;

  nsitem = (ParseNamespaceItem *)palloc(sizeof(ParseNamespaceItem));
  nsitem->p_rte = rte;
  nsitem->p_rel_visible = rel_visible;
  nsitem->p_cols_visible = cols_visible;
  nsitem->p_lateral_only = lateral_only;
  nsitem->p_lateral_ok = lateral_ok;
  return nsitem;
}

   
                                  
                                                                              
   
static void
setNamespaceColumnVisibility(List *namespace, bool cols_visible)
{
  ListCell *lc;

  foreach (lc, namespace)
  {
    ParseNamespaceItem *nsitem = (ParseNamespaceItem *)lfirst(lc);

    nsitem->p_cols_visible = cols_visible;
  }
}

   
                              
                                                                         
   
static void
setNamespaceLateralState(List *namespace, bool lateral_only, bool lateral_ok)
{
  ListCell *lc;

  foreach (lc, namespace)
  {
    ParseNamespaceItem *nsitem = (ParseNamespaceItem *)lfirst(lc);

    nsitem->p_lateral_only = lateral_only;
    nsitem->p_lateral_ok = lateral_ok;
  }
}

   
                          
                                                                      
                                        
   
                                                                              
   
Node *
transformWhereClause(ParseState *pstate, Node *clause, ParseExprKind exprKind, const char *constructName)
{
  Node *qual;

  if (clause == NULL)
  {
    return NULL;
  }

  qual = transformExpr(pstate, clause, exprKind);

  qual = coerce_to_boolean(pstate, qual, constructName);

  return qual;
}

   
                          
                                                                  
                                        
   
                                                                           
                               
   
                                                                              
   
Node *
transformLimitClause(ParseState *pstate, Node *clause, ParseExprKind exprKind, const char *constructName)
{
  Node *qual;

  if (clause == NULL)
  {
    return NULL;
  }

  qual = transformExpr(pstate, clause, exprKind);

  qual = coerce_to_specific_type(pstate, qual, INT8OID, constructName);

                                                               
  checkExprIsVarFree(pstate, qual, constructName);

  return qual;
}

   
                      
                                                                 
                                                                         
   
                                                                          
                                                                             
                                                                        
                                                      
   
                                                                              
   
static void
checkExprIsVarFree(ParseState *pstate, Node *n, const char *constructName)
{
  if (contain_vars_of_level(n, 0))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
                                                                                
                       errmsg("argument of %s must not contain variables", constructName), parser_errposition(pstate, locate_var_of_level(n, 0))));
  }
}

   
                               
                                                                   
   
                                                                        
                                                                         
                                                                            
                    
   
static void
checkTargetlistEntrySQL92(ParseState *pstate, TargetEntry *tle, ParseExprKind exprKind)
{
  switch (exprKind)
  {
  case EXPR_KIND_GROUP_BY:
                                                
    if (pstate->p_hasAggs && contain_aggs_of_level((Node *)tle->expr, 0))
    {
      ereport(ERROR, (errcode(ERRCODE_GROUPING_ERROR),
                                                                                     
                         errmsg("aggregate functions are not allowed in %s", ParseExprKindName(exprKind)), parser_errposition(pstate, locate_agg_of_level((Node *)tle->expr, 0))));
    }
    if (pstate->p_hasWindowFuncs && contain_windowfuncs((Node *)tle->expr))
    {
      ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR),
                                                                                     
                         errmsg("window functions are not allowed in %s", ParseExprKindName(exprKind)), parser_errposition(pstate, locate_windowfunc((Node *)tle->expr))));
    }
    break;
  case EXPR_KIND_ORDER_BY:
                                
    break;
  case EXPR_KIND_DISTINCT_ON:
                                
    break;
  default:
    elog(ERROR, "unexpected exprKind in checkTargetlistEntrySQL92");
    break;
  }
}

   
                              
                                                                           
                                                                            
                               
   
                                                                           
                                                                        
                                                                           
                                                                          
                                                                           
                                                       
   
                                                                         
                                                                      
                                                   
   
static TargetEntry *
findTargetlistEntrySQL92(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind)
{
  ListCell *tl;

               
                                                             
     
                                                     
                                                                   
                                                                   
                                                           
                                                  
                                                      
                                                                      
                         
                                                                        
                                                                         
                                                                        
                                                                      
                                                                       
                                                                       
                                                                      
                                                                    
                                                                           
                                                                     
                                                                   
     
                        
                                                                    
                                                                      
                                                                       
                                                                 
                                                                   
                                                    
     
                                                                             
                                                          
     
                                                                        
                              
               
     
  if (IsA(node, ColumnRef) && list_length(((ColumnRef *)node)->fields) == 1 && IsA(linitial(((ColumnRef *)node)->fields), String))
  {
    char *name = strVal(linitial(((ColumnRef *)node)->fields));
    int location = ((ColumnRef *)node)->location;

    if (exprKind == EXPR_KIND_GROUP_BY)
    {
         
                                                                   
                                                                        
                                                                     
                                                                       
                                                                        
                                 
         
                                                                      
                                                                         
                                                                       
                                                                      
                                                                    
                                                                      
                                   
         
      if (colNameToVar(pstate, name, true, location) != NULL)
      {
        name = NULL;
      }
    }

    if (name != NULL)
    {
      TargetEntry *target_result = NULL;

      foreach (tl, *tlist)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(tl);

        if (!tle->resjunk && strcmp(tle->resname, name) == 0)
        {
          if (target_result != NULL)
          {
            if (!equal(target_result->expr, tle->expr))
            {
              ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN),

                                          
                                                                                                  
                                 errmsg("%s \"%s\" is ambiguous", ParseExprKindName(exprKind), name), parser_errposition(pstate, location)));
            }
          }
          else
          {
            target_result = tle;
          }
                                                   
        }
      }
      if (target_result != NULL)
      {
                                                               
        checkTargetlistEntrySQL92(pstate, target_result, exprKind);
        return target_result;
      }
    }
  }
  if (IsA(node, A_Const))
  {
    Value *val = &((A_Const *)node)->val;
    int location = ((A_Const *)node)->location;
    int targetlist_pos = 0;
    int target_pos;

    if (!IsA(val, Integer))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                                     
                         errmsg("non-integer constant in %s", ParseExprKindName(exprKind)), parser_errposition(pstate, location)));
    }

    target_pos = intVal(val);
    foreach (tl, *tlist)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(tl);

      if (!tle->resjunk)
      {
        if (++targetlist_pos == target_pos)
        {
                                                                  
          checkTargetlistEntrySQL92(pstate, tle, exprKind);
          return tle;
        }
      }
    }
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
                                                                                   
                       errmsg("%s position %d is not in select list", ParseExprKindName(exprKind), target_pos), parser_errposition(pstate, location)));
  }

     
                                                                      
     
  return findTargetlistEntrySQL99(pstate, node, tlist, exprKind);
}

   
                              
                                                                           
                                                                            
                               
   
                                                                           
                                                                  
   
                                                              
                                                                      
                                                   
   
static TargetEntry *
findTargetlistEntrySQL99(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind)
{
  TargetEntry *target_result;
  ListCell *tl;
  Node *expr;

     
                                                                            
                                                                             
                                                                        
                                                                           
              
     
  expr = transformExpr(pstate, node, exprKind);

  foreach (tl, *tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tl);
    Node *texpr;

       
                                                                  
       
                                                                          
                                                                           
                                                                           
                                                                        
                           
       
    texpr = strip_implicit_coercions((Node *)tle->expr);

    if (equal(expr, texpr))
    {
      return tle;
    }
  }

     
                                                                          
                                                                             
                                                 
     
  target_result = transformTargetEntry(pstate, node, expr, exprKind, NULL, true);

  *tlist = lappend(*tlist, target_result);

  return target_result;
}

                                                                            
                                                                        
                            
   
                                                                         
                                                                         
                                                                          
                                       
   
                                                                            
                                                                             
                                                           
                                                               
           
                                                  
   
                                                                              
                                                                       
                                                                           
                                                                        
                                          
   
                                                                          
             
                    
                                                                
                                        
                       
                          
                                                    
                                                                            
                      
   
                                                                      
                                                                            
   
static Node *
flatten_grouping_sets(Node *expr, bool toplevel, bool *hasGroupingSets)
{
                                          
  check_stack_depth();

  if (expr == (Node *)NIL)
  {
    return (Node *)NIL;
  }

  switch (expr->type)
  {
  case T_RowExpr:
  {
    RowExpr *r = (RowExpr *)expr;

    if (r->row_format == COERCE_IMPLICIT_CAST)
    {
      return flatten_grouping_sets((Node *)r->args, false, NULL);
    }
  }
  break;
  case T_GroupingSet:
  {
    GroupingSet *gset = (GroupingSet *)expr;
    ListCell *l2;
    List *result_set = NIL;

    if (hasGroupingSets)
    {
      *hasGroupingSets = true;
    }

       
                                                                   
                                                                 
             
       

    if (toplevel && gset->kind == GROUPING_SET_EMPTY)
    {
      return (Node *)NIL;
    }

    foreach (l2, gset->content)
    {
      Node *n1 = lfirst(l2);
      Node *n2 = flatten_grouping_sets(n1, false, NULL);

      if (IsA(n1, GroupingSet) && ((GroupingSet *)n1)->kind == GROUPING_SET_SETS)
      {
        result_set = list_concat(result_set, (List *)n2);
      }
      else
      {
        result_set = lappend(result_set, n2);
      }
    }

       
                                                                   
                                                                 
                                                         
       

    if (toplevel || (gset->kind != GROUPING_SET_SETS))
    {
      return (Node *)makeGroupingSet(gset->kind, result_set, gset->location);
    }
    else
    {
      return (Node *)result_set;
    }
  }
  case T_List:
  {
    List *result = NIL;
    ListCell *l;

    foreach (l, (List *)expr)
    {
      Node *n = flatten_grouping_sets(lfirst(l), toplevel, hasGroupingSets);

      if (n != (Node *)NIL)
      {
        if (IsA(n, List))
        {
          result = list_concat(result, (List *)n);
        }
        else
        {
          result = lappend(result, n);
        }
      }
    }

    return (Node *)result;
  }
  default:
    break;
  }

  return expr;
}

   
                                                                           
   
                                                                                
                                                                              
                                                                          
   
                                                  
   
                                                              
                                                                         
                      
                            
                                            
                                                      
                             
                                            
                                              
   
static Index
transformGroupClauseExpr(List **flatresult, Bitmapset *seen_local, ParseState *pstate, Node *gexpr, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99, bool toplevel)
{
  TargetEntry *tle;
  bool found = false;

  if (useSQL99)
  {
    tle = findTargetlistEntrySQL99(pstate, gexpr, targetlist, exprKind);
  }
  else
  {
    tle = findTargetlistEntrySQL92(pstate, gexpr, targetlist, exprKind);
  }

  if (tle->ressortgroupref > 0)
  {
    ListCell *sl;

       
                                                                     
                                                                      
                                                    
       
                                                                          
                                                   
       
    if (bms_is_member(tle->ressortgroupref, seen_local))
    {
      return 0;
    }

       
                                                                           
                               
       
    found = targetIsInSortList(tle, InvalidOid, *flatresult);
    if (found)
    {
      return tle->ressortgroupref;
    }

       
                                                                           
                                                                         
                                                                           
                                                                           
                                                                           
                                                                           
                                                                          
                                                                        
                                        
       
                                                                           
                                                                           
                                                                       
                                                                        
                                                                   
                          
       

    foreach (sl, sortClause)
    {
      SortGroupClause *sc = (SortGroupClause *)lfirst(sl);

      if (sc->tleSortGroupRef == tle->ressortgroupref)
      {
        SortGroupClause *grpc = copyObject(sc);

        if (!toplevel)
        {
          grpc->nulls_first = false;
        }
        *flatresult = lappend(*flatresult, grpc);
        found = true;
        break;
      }
    }
  }

     
                                                                      
                           
     
  if (!found)
  {
    *flatresult = addTargetToGroupList(pstate, tle, *flatresult, *targetlist, exprLocation(gexpr));
  }

     
                                                                
     

  return tle->ressortgroupref;
}

   
                                                                             
   
                                                                              
                             
   
                                                      
   
                                                              
                      
                             
                                            
                                                      
                             
                                            
                                              
   
static List *
transformGroupClauseList(List **flatresult, ParseState *pstate, List *list, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99, bool toplevel)
{
  Bitmapset *seen_local = NULL;
  List *result = NIL;
  ListCell *gl;

  foreach (gl, list)
  {
    Node *gexpr = (Node *)lfirst(gl);

    Index ref = transformGroupClauseExpr(flatresult, seen_local, pstate, gexpr, targetlist, sortClause, exprKind, useSQL99, toplevel);

    if (ref > 0)
    {
      seen_local = bms_add_member(seen_local, ref);
      result = lappend_int(result, ref);
    }
  }

  return result;
}

   
                                                           
   
                                                                           
                                                                          
                  
   
                                                                            
                                                
   
                                                              
                      
                                    
                                            
                                                      
                             
                                            
                                              
   
static Node *
transformGroupingSet(List **flatresult, ParseState *pstate, GroupingSet *gset, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99, bool toplevel)
{
  ListCell *gl;
  List *content = NIL;

  Assert(toplevel || gset->kind != GROUPING_SET_SETS);

  foreach (gl, gset->content)
  {
    Node *n = lfirst(gl);

    if (IsA(n, List))
    {
      List *l = transformGroupClauseList(flatresult, pstate, (List *)n, targetlist, sortClause, exprKind, useSQL99, false);

      content = lappend(content, makeGroupingSet(GROUPING_SET_SIMPLE, l, exprLocation(n)));
    }
    else if (IsA(n, GroupingSet))
    {
      GroupingSet *gset2 = (GroupingSet *)lfirst(gl);

      content = lappend(content, transformGroupingSet(flatresult, pstate, gset2, targetlist, sortClause, exprKind, useSQL99, false));
    }
    else
    {
      Index ref = transformGroupClauseExpr(flatresult, NULL, pstate, n, targetlist, sortClause, exprKind, useSQL99, false);

      content = lappend(content, makeGroupingSet(GROUPING_SET_SIMPLE, list_make1_int(ref), exprLocation(n)));
    }
  }

                                                                      
  if (gset->kind == GROUPING_SET_CUBE)
  {
    if (list_length(content) > 12)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("CUBE is limited to 12 elements"), parser_errposition(pstate, gset->location)));
    }
  }

  return (Node *)makeGroupingSet(gset->kind, content, gset->location);
}

   
                          
                                 
   
                                                                       
                                                                          
   
                                                                           
                                                      
   
                                                                            
                                                                          
                                                                              
                                                                           
                                                                                
                                                                                
                                                                            
                                                              
   
                                                                
   
                                                                        
                                                                           
                                                                            
                                                                             
                                            
   
                                               
   
                      
                                 
                                                                   
                                            
                                                      
                             
                                            
   
List *
transformGroupClause(ParseState *pstate, List *grouplist, List **groupingSets, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99)
{
  List *result = NIL;
  List *flat_grouplist;
  List *gsets = NIL;
  ListCell *gl;
  bool hasGroupingSets = false;
  Bitmapset *seen_local = NULL;

     
                                                                             
                                                                        
              
     
  flat_grouplist = (List *)flatten_grouping_sets((Node *)grouplist, true, &hasGroupingSets);

     
                                                                            
                                                                           
                                                
     

  if (flat_grouplist == NIL && hasGroupingSets)
  {
    flat_grouplist = list_make1(makeGroupingSet(GROUPING_SET_EMPTY, NIL, exprLocation((Node *)grouplist)));
  }

  foreach (gl, flat_grouplist)
  {
    Node *gexpr = (Node *)lfirst(gl);

    if (IsA(gexpr, GroupingSet))
    {
      GroupingSet *gset = (GroupingSet *)gexpr;

      switch (gset->kind)
      {
      case GROUPING_SET_EMPTY:
        gsets = lappend(gsets, gset);
        break;
      case GROUPING_SET_SIMPLE:
                          
        Assert(false);
        break;
      case GROUPING_SET_SETS:
      case GROUPING_SET_CUBE:
      case GROUPING_SET_ROLLUP:
        gsets = lappend(gsets, transformGroupingSet(&result, pstate, gset, targetlist, sortClause, exprKind, useSQL99, true));
        break;
      }
    }
    else
    {
      Index ref = transformGroupClauseExpr(&result, seen_local, pstate, gexpr, targetlist, sortClause, exprKind, useSQL99, true);

      if (ref > 0)
      {
        seen_local = bms_add_member(seen_local, ref);
        if (hasGroupingSets)
        {
          gsets = lappend(gsets, makeGroupingSet(GROUPING_SET_SIMPLE, list_make1_int(ref), exprLocation(gexpr)));
        }
      }
    }
  }

                                  
  Assert(gsets == NIL || groupingSets != NULL);

  if (groupingSets)
  {
    *groupingSets = gsets;
  }

  return result;
}

   
                         
                                  
   
                                                                       
                                                                          
   
                                                                          
                                                                 
   
List *
transformSortClause(ParseState *pstate, List *orderlist, List **targetlist, ParseExprKind exprKind, bool useSQL99)
{
  List *sortlist = NIL;
  ListCell *olitem;

  foreach (olitem, orderlist)
  {
    SortBy *sortby = (SortBy *)lfirst(olitem);
    TargetEntry *tle;

    if (useSQL99)
    {
      tle = findTargetlistEntrySQL99(pstate, sortby->node, targetlist, exprKind);
    }
    else
    {
      tle = findTargetlistEntrySQL92(pstate, sortby->node, targetlist, exprKind);
    }

    sortlist = addTargetToSortList(pstate, tle, sortlist, *targetlist, sortby);
  }

  return sortlist;
}

   
                                
                                                             
   
List *
transformWindowDefinitions(ParseState *pstate, List *windowdefs, List **targetlist)
{
  List *result = NIL;
  Index winref = 0;
  ListCell *lc;

  foreach (lc, windowdefs)
  {
    WindowDef *windef = (WindowDef *)lfirst(lc);
    WindowClause *refwc = NULL;
    List *partitionClause;
    List *orderClause;
    Oid rangeopfamily = InvalidOid;
    Oid rangeopcintype = InvalidOid;
    WindowClause *wc;

    winref++;

       
                                         
       
    if (windef->name && findWindowClause(result, windef->name) != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("window \"%s\" is already defined", windef->name), parser_errposition(pstate, windef->location)));
    }

       
                                                         
       
    if (windef->refname)
    {
      refwc = findWindowClause(result, windef->refname);
      if (refwc == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("window \"%s\" does not exist", windef->refname), parser_errposition(pstate, windef->location)));
      }
    }

       
                                                                       
                                                                    
                                                                        
       
    orderClause = transformSortClause(pstate, windef->orderClause, targetlist, EXPR_KIND_WINDOW_ORDER, true                        );
    partitionClause = transformGroupClause(pstate, windef->partitionClause, NULL, targetlist, orderClause, EXPR_KIND_WINDOW_PARTITION, true                        );

       
                                         
       
    wc = makeNode(WindowClause);
    wc->name = windef->name;
    wc->refname = windef->refname;

       
                                                                       
                                                                        
                                                                         
                                                                         
                                                                          
                                                                     
                                                                     
                                                                     
                                                                        
                                                                      
                                                                       
                               
       
    if (refwc)
    {
      if (partitionClause)
      {
        ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("cannot override PARTITION BY clause of window \"%s\"", windef->refname), parser_errposition(pstate, windef->location)));
      }
      wc->partitionClause = copyObject(refwc->partitionClause);
    }
    else
    {
      wc->partitionClause = partitionClause;
    }
    if (refwc)
    {
      if (orderClause && refwc->orderClause)
      {
        ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("cannot override ORDER BY clause of window \"%s\"", windef->refname), parser_errposition(pstate, windef->location)));
      }
      if (orderClause)
      {
        wc->orderClause = orderClause;
        wc->copiedOrder = false;
      }
      else
      {
        wc->orderClause = copyObject(refwc->orderClause);
        wc->copiedOrder = true;
      }
    }
    else
    {
      wc->orderClause = orderClause;
      wc->copiedOrder = false;
    }
    if (refwc && refwc->frameOptions != FRAMEOPTION_DEFAULTS)
    {
         
                                                                         
                                                                        
                                                                 
         
      if (windef->name || orderClause || windef->frameOptions != FRAMEOPTION_DEFAULTS)
      {
        ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("cannot copy window \"%s\" because it has a frame clause", windef->refname), parser_errposition(pstate, windef->location)));
      }
                                                             
      ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("cannot copy window \"%s\" because it has a frame clause", windef->refname), errhint("Omit the parentheses in this OVER clause."), parser_errposition(pstate, windef->location)));
    }
    wc->frameOptions = windef->frameOptions;

       
                                                                      
                                                          
       
    if ((wc->frameOptions & FRAMEOPTION_RANGE) && (wc->frameOptions & (FRAMEOPTION_START_OFFSET | FRAMEOPTION_END_OFFSET)))
    {
      SortGroupClause *sortcl;
      Node *sortkey;
      int16 rangestrategy;

      if (list_length(wc->orderClause) != 1)
      {
        ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("RANGE with offset PRECEDING/FOLLOWING requires exactly one ORDER BY column"), parser_errposition(pstate, windef->location)));
      }
      sortcl = castNode(SortGroupClause, linitial(wc->orderClause));
      sortkey = get_sortgroupclause_expr(sortcl, *targetlist);
                                             
      if (!get_ordering_op_properties(sortcl->sortop, &rangeopfamily, &rangeopcintype, &rangestrategy))
      {
        elog(ERROR, "operator %u is not a valid ordering operator", sortcl->sortop);
      }
                                              
      wc->inRangeColl = exprCollation(sortkey);
      wc->inRangeAsc = (rangestrategy == BTLessStrategyNumber);
      wc->inRangeNullsFirst = sortcl->nulls_first;
    }

                                                           
    if (wc->frameOptions & FRAMEOPTION_GROUPS)
    {
      if (wc->orderClause == NIL)
      {
        ereport(ERROR, (errcode(ERRCODE_WINDOWING_ERROR), errmsg("GROUPS mode requires an ORDER BY clause"), parser_errposition(pstate, windef->location)));
      }
    }

                                          
    wc->startOffset = transformFrameOffset(pstate, wc->frameOptions, rangeopfamily, rangeopcintype, &wc->startInRangeFunc, windef->startOffset);
    wc->endOffset = transformFrameOffset(pstate, wc->frameOptions, rangeopfamily, rangeopcintype, &wc->endInRangeFunc, windef->endOffset);
    wc->winref = winref;

    result = lappend(result, wc);
  }

  return result;
}

   
                             
                                 
   
                                                                       
                           
   
                                                                            
                                                                              
                                                                          
                                                                         
             
   
                                                                    
                                                                        
                      
   
List *
transformDistinctClause(ParseState *pstate, List **targetlist, List *sortClause, bool is_agg)
{
  List *result = NIL;
  ListCell *slitem;
  ListCell *tlitem;

     
                                                                             
                                                                        
                                                                             
                                                                      
                                                                             
                                                                           
                                           
     
                                                                             
                                                                 
                                                                          
                                                                             
              
     
  foreach (slitem, sortClause)
  {
    SortGroupClause *scl = (SortGroupClause *)lfirst(slitem);
    TargetEntry *tle = get_sortgroupclause_tle(scl, *targetlist);

    if (tle->resjunk)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), is_agg ? errmsg("in an aggregate with DISTINCT, ORDER BY expressions must appear in argument list") : errmsg("for SELECT DISTINCT, ORDER BY expressions must appear in select list"), parser_errposition(pstate, exprLocation((Node *)tle->expr))));
    }
    result = lappend(result, copyObject(scl));
  }

     
                                                                             
                                     
     
  foreach (tlitem, *targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tlitem);

    if (tle->resjunk)
    {
      continue;                  
    }
    result = addTargetToGroupList(pstate, tle, result, *targetlist, exprLocation((Node *)tle->expr));
  }

     
                                                                             
                                                                             
                                                                        
                                                                          
                          
     
  if (result == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), is_agg ? errmsg("an aggregate with DISTINCT must have at least one argument") : errmsg("SELECT DISTINCT must have at least one column")));
  }

  return result;
}

   
                               
                                    
   
                                                                       
                           
   
                                                                            
                                                                              
                                                                          
                                                                         
             
   
List *
transformDistinctOnClause(ParseState *pstate, List *distinctlist, List **targetlist, List *sortClause)
{
  List *result = NIL;
  List *sortgrouprefs = NIL;
  bool skipped_sortitem;
  ListCell *lc;
  ListCell *lc2;

     
                                                                      
                                                                             
                                                                           
                                                                          
                                                                          
                                                
     
  foreach (lc, distinctlist)
  {
    Node *dexpr = (Node *)lfirst(lc);
    int sortgroupref;
    TargetEntry *tle;

    tle = findTargetlistEntrySQL92(pstate, dexpr, targetlist, EXPR_KIND_DISTINCT_ON);
    sortgroupref = assignSortGroupRef(tle, *targetlist);
    sortgrouprefs = lappend_int(sortgrouprefs, sortgroupref);
  }

     
                                                                         
                                                                          
                                                                           
                                                                          
                                                                           
                     
     
  skipped_sortitem = false;
  foreach (lc, sortClause)
  {
    SortGroupClause *scl = (SortGroupClause *)lfirst(lc);

    if (list_member_int(sortgrouprefs, scl->tleSortGroupRef))
    {
      if (skipped_sortitem)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("SELECT DISTINCT ON expressions must match initial ORDER BY expressions"), parser_errposition(pstate, get_matching_location(scl->tleSortGroupRef, sortgrouprefs, distinctlist))));
      }
      else
      {
        result = lappend(result, copyObject(scl));
      }
    }
    else
    {
      skipped_sortitem = true;
    }
  }

     
                                                                       
                                                                             
                                                                          
                                                                          
                                                                           
                                                                       
                                    
     
  forboth(lc, distinctlist, lc2, sortgrouprefs)
  {
    Node *dexpr = (Node *)lfirst(lc);
    int sortgroupref = lfirst_int(lc2);
    TargetEntry *tle = get_sortgroupref_tle(sortgroupref, *targetlist);

    if (targetIsInSortList(tle, InvalidOid, result))
    {
      continue;                                            
    }
    if (skipped_sortitem)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("SELECT DISTINCT ON expressions must match initial ORDER BY expressions"), parser_errposition(pstate, exprLocation(dexpr))));
    }
    result = addTargetToGroupList(pstate, tle, result, *targetlist, exprLocation(dexpr));
  }

     
                                                                
                   
     
  Assert(result != NIL);

  return result;
}

   
                         
                                                                  
                                                              
   
                                                                         
                                                                         
                                                                     
                                                                     
                      
   
static int
get_matching_location(int sortgroupref, List *sortgrouprefs, List *exprs)
{
  ListCell *lcs;
  ListCell *lce;

  forboth(lcs, sortgrouprefs, lce, exprs)
  {
    if (lfirst_int(lcs) == sortgroupref)
    {
      return exprLocation((Node *)lfirst(lce));
    }
  }
                                   
  elog(ERROR, "get_matching_location: no matching sortgroupref");
  return -1;                          
}

   
                             
                                                           
                    
   
                                                                         
                                                                               
                                       
   
static List *
resolve_unique_index_expr(ParseState *pstate, InferClause *infer, Relation heapRel)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, infer->indexElems)
  {
    IndexElem *ielem = (IndexElem *)lfirst(l);
    InferenceElem *pInfer = makeNode(InferenceElem);
    Node *parse;

       
                                                                        
                                                                         
       
                                                                          
                                                                    
                                                                          
                                                                          
                     
       
    if (ielem->ordering != SORTBY_DEFAULT)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("ASC/DESC is not allowed in ON CONFLICT clause"), parser_errposition(pstate, exprLocation((Node *)infer))));
    }
    if (ielem->nulls_ordering != SORTBY_NULLS_DEFAULT)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("NULLS FIRST/LAST is not allowed in ON CONFLICT clause"), parser_errposition(pstate, exprLocation((Node *)infer))));
    }

    if (!ielem->expr)
    {
                                  
      ColumnRef *n;

         
                                                                    
                                                                   
                                                                        
                                                                    
                   
         
      n = makeNode(ColumnRef);
      n->fields = list_make1(makeString(ielem->name));
                                                                     
      n->location = infer->location;
      parse = (Node *)n;
    }
    else
    {
                                                         
      parse = (Node *)ielem->expr;
    }

       
                                                                  
                                                  
                                                                    
                                                                 
                          
       
    pInfer->expr = transformExpr(pstate, parse, EXPR_KIND_INDEX_EXPRESSION);

                                                                    
    if (!ielem->collation)
    {
      pInfer->infercollid = InvalidOid;
    }
    else
    {
      pInfer->infercollid = LookupCollation(pstate, ielem->collation, exprLocation(pInfer->expr));
    }

    if (!ielem->opclass)
    {
      pInfer->inferopclass = InvalidOid;
    }
    else
    {
      pInfer->inferopclass = get_opclass_oid(BTREE_AM_OID, ielem->opclass, false);
    }

    result = lappend(result, pInfer);
  }

  return result;
}

   
                                
                                                            
   
                                                                               
                                                                               
                                               
   
void
transformOnConflictArbiter(ParseState *pstate, OnConflictClause *onConflictClause, List **arbiterExpr, Node **arbiterWhere, Oid *constraint)
{
  InferClause *infer = onConflictClause->infer;

  *arbiterExpr = NIL;
  *arbiterWhere = NULL;
  *constraint = InvalidOid;

  if (onConflictClause->action == ONCONFLICT_UPDATE && !infer)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("ON CONFLICT DO UPDATE requires inference specification or constraint name"), errhint("For example, ON CONFLICT (column_name)."), parser_errposition(pstate, exprLocation((Node *)onConflictClause))));
  }

     
                                                                           
                                   
     
  if (IsCatalogRelation(pstate->p_target_relation))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ON CONFLICT is not supported with system catalog tables"), parser_errposition(pstate, exprLocation((Node *)onConflictClause))));
  }

                                                                       
  if (RelationIsUsedAsCatalogTable(pstate->p_target_relation))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ON CONFLICT is not supported on table \"%s\" used as a catalog table", RelationGetRelationName(pstate->p_target_relation)), parser_errposition(pstate, exprLocation((Node *)onConflictClause))));
  }

                                                                   
  if (infer)
  {
    List *save_namespace;

       
                                                                           
                                                                 
       
    save_namespace = pstate->p_namespace;
    pstate->p_namespace = NIL;
    addRTEtoQuery(pstate, pstate->p_target_rangetblentry, false, false, true);

    if (infer->indexElems)
    {
      *arbiterExpr = resolve_unique_index_expr(pstate, infer, pstate->p_target_relation);
    }

       
                                                                 
                  
       
    if (infer->whereClause)
    {
      *arbiterWhere = transformExpr(pstate, infer->whereClause, EXPR_KIND_INDEX_PREDICATE);
    }

    pstate->p_namespace = save_namespace;

       
                                                                          
                                                                           
                                                                      
                                                                          
       
    if (infer->conname)
    {
      Oid relid = RelationGetRelid(pstate->p_target_relation);
      RangeTblEntry *rte = pstate->p_target_rangetblentry;
      Bitmapset *conattnos;

      conattnos = get_relation_constraint_attnos(relid, infer->conname, false, constraint);

                                                                    
      rte->requiredPerms |= ACL_SELECT;
                                                                   
      rte->selectedCols = bms_add_members(rte->selectedCols, conattnos);
    }
  }

     
                                                                
                                                                          
                                                                           
                                                                    
                                                                             
                                                                     
     
}

   
                       
                                                                       
                                                                       
          
   
                                             
   
List *
addTargetToSortList(ParseState *pstate, TargetEntry *tle, List *sortlist, List *targetlist, SortBy *sortby)
{
  Oid restype = exprType((Node *)tle->expr);
  Oid sortop;
  Oid eqop;
  bool hashable;
  bool reverse;
  int location;
  ParseCallbackState pcbstate;

                                                              
  if (restype == UNKNOWNOID)
  {
    tle->expr = (Expr *)coerce_type(pstate, (Node *)tle->expr, restype, TEXTOID, -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
    restype = TEXTOID;
  }

     
                                                                           
                                                                         
                                                                             
                                                                             
                                                                            
                                                            
     
  location = sortby->location;
  if (location < 0)
  {
    location = exprLocation(sortby->node);
  }
  setup_parser_errposition_callback(&pcbstate, pstate, location);

                                                      
  switch (sortby->sortby_dir)
  {
  case SORTBY_DEFAULT:
  case SORTBY_ASC:
    get_sort_group_operators(restype, true, true, false, &sortop, &eqop, NULL, &hashable);
    reverse = false;
    break;
  case SORTBY_DESC:
    get_sort_group_operators(restype, false, true, true, NULL, &eqop, &sortop, &hashable);
    reverse = true;
    break;
  case SORTBY_USING:
    Assert(sortby->useOp != NIL);
    sortop = compatible_oper_opid(sortby->useOp, restype, restype, false);

       
                                                                      
                                                                    
                    
       
    eqop = get_equality_op_for_ordering_op(sortop, &reverse);
    if (!OidIsValid(eqop))
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("operator %s is not a valid ordering operator", strVal(llast(sortby->useOp))), errhint("Ordering operators must be \"<\" or \">\" members of btree operator families.")));
    }

       
                                                      
       
    hashable = op_hashjoinable(eqop, restype);
    break;
  default:
    elog(ERROR, "unrecognized sortby_dir: %d", sortby->sortby_dir);
    sortop = InvalidOid;                          
    eqop = InvalidOid;
    hashable = false;
    reverse = false;
    break;
  }

  cancel_parser_errposition_callback(&pcbstate);

                                               
  if (!targetIsInSortList(tle, sortop, sortlist))
  {
    SortGroupClause *sortcl = makeNode(SortGroupClause);

    sortcl->tleSortGroupRef = assignSortGroupRef(tle, targetlist);

    sortcl->eqop = eqop;
    sortcl->sortop = sortop;
    sortcl->hashable = hashable;

    switch (sortby->sortby_nulls)
    {
    case SORTBY_NULLS_DEFAULT:
                                                              
      sortcl->nulls_first = reverse;
      break;
    case SORTBY_NULLS_FIRST:
      sortcl->nulls_first = true;
      break;
    case SORTBY_NULLS_LAST:
      sortcl->nulls_first = false;
      break;
    default:
      elog(ERROR, "unrecognized sortby_nulls: %d", sortby->sortby_nulls);
      break;
    }

    sortlist = lappend(sortlist, sortcl);
  }

  return sortlist;
}

   
                        
                                                                       
                                                                  
               
   
                                                                         
                                                                         
                                                                            
                      
   
                                                                            
                                                                           
                                                                             
                              
   
                                             
   
static List *
addTargetToGroupList(ParseState *pstate, TargetEntry *tle, List *grouplist, List *targetlist, int location)
{
  Oid restype = exprType((Node *)tle->expr);

                                                              
  if (restype == UNKNOWNOID)
  {
    tle->expr = (Expr *)coerce_type(pstate, (Node *)tle->expr, restype, TEXTOID, -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
    restype = TEXTOID;
  }

                                                
  if (!targetIsInSortList(tle, InvalidOid, grouplist))
  {
    SortGroupClause *grpcl = makeNode(SortGroupClause);
    Oid sortop;
    Oid eqop;
    bool hashable;
    ParseCallbackState pcbstate;

    setup_parser_errposition_callback(&pcbstate, pstate, location);

                                                
    get_sort_group_operators(restype, false, true, false, &sortop, &eqop, NULL, &hashable);

    cancel_parser_errposition_callback(&pcbstate);

    grpcl->tleSortGroupRef = assignSortGroupRef(tle, targetlist);
    grpcl->eqop = eqop;
    grpcl->sortop = sortop;
    grpcl->nulls_first = false;                                
    grpcl->hashable = hashable;

    grouplist = lappend(grouplist, grpcl);
  }

  return grouplist;
}

   
                      
                                                                     
                                                                       
   
                                                                               
   
Index
assignSortGroupRef(TargetEntry *tle, List *tlist)
{
  Index maxRef;
  ListCell *l;

  if (tle->ressortgroupref)                       
  {
    return tle->ressortgroupref;
  }

                                                             
  maxRef = 0;
  foreach (l, tlist)
  {
    Index ref = ((TargetEntry *)lfirst(l))->ressortgroupref;

    if (ref > maxRef)
    {
      maxRef = ref;
    }
  }
  tle->ressortgroupref = maxRef + 1;
  return tle->ressortgroupref;
}

   
                      
                                                      
                                                                      
   
                                                                           
                                                                          
                                                                   
                                                                      
                                                                 
                                                                  
                                                                 
                                                                          
   
                                                                        
                                                                             
                                                                               
                                                    
   
bool
targetIsInSortList(TargetEntry *tle, Oid sortop, List *sortList)
{
  Index ref = tle->ressortgroupref;
  ListCell *l;

                                                 
  if (ref == 0)
  {
    return false;
  }

  foreach (l, sortList)
  {
    SortGroupClause *scl = (SortGroupClause *)lfirst(l);

    if (scl->tleSortGroupRef == ref && (sortop == InvalidOid || sortop == scl->sortop || sortop == get_commutator(scl->sortop)))
    {
      return true;
    }
  }
  return false;
}

   
                    
                                                                         
   
static WindowClause *
findWindowClause(List *wclist, const char *name)
{
  ListCell *l;

  foreach (l, wclist)
  {
    WindowClause *wc = (WindowClause *)lfirst(l);

    if (wc->name && strcmp(wc->name, name) == 0)
    {
      return wc;
    }
  }

  return NULL;
}

   
                        
                                             
   
                                                                            
                                                                          
                                                                           
                                                                            
                                                                          
                                                                  
   
static Node *
transformFrameOffset(ParseState *pstate, int frameOptions, Oid rangeopfamily, Oid rangeopcintype, Oid *inRangeFunc, Node *clause)
{
  const char *constructName = NULL;
  Node *node;

  *inRangeFunc = InvalidOid;                     

                                          
  if (clause == NULL)
  {
    return NULL;
  }

  if (frameOptions & FRAMEOPTION_ROWS)
  {
                                           
    node = transformExpr(pstate, clause, EXPR_KIND_WINDOW_FRAME_ROWS);

       
                                                
       
    constructName = "ROWS";
    node = coerce_to_specific_type(pstate, node, INT8OID, constructName);
  }
  else if (frameOptions & FRAMEOPTION_RANGE)
  {
       
                                                                        
                                                                          
                                   
       
    Oid nodeType;
    Oid preferredType;
    int nfuncs = 0;
    int nmatches = 0;
    Oid selectedType = InvalidOid;
    Oid selectedFunc = InvalidOid;
    CatCList *proclist;
    int i;

                                           
    node = transformExpr(pstate, clause, EXPR_KIND_WINDOW_FRAME_RANGE);
    nodeType = exprType(node);

       
                                                                           
                                                                          
                                                                       
                                                           
       
    preferredType = (nodeType != UNKNOWNOID) ? nodeType : rangeopcintype;

                                                                     
    proclist = SearchSysCacheList2(AMPROCNUM, ObjectIdGetDatum(rangeopfamily), ObjectIdGetDatum(rangeopcintype));
    for (i = 0; i < proclist->n_members; i++)
    {
      HeapTuple proctup = &proclist->members[i]->tuple;
      Form_pg_amproc procform = (Form_pg_amproc)GETSTRUCT(proctup);

                                                                      
      if (procform->amprocnum != BTINRANGE_PROC)
      {
        continue;
      }
      nfuncs++;

                                                                        
      if (!can_coerce_type(1, &nodeType, &procform->amprocrighttype, COERCION_IMPLICIT))
      {
        continue;
      }
      nmatches++;

                                                                      
      if (selectedType != preferredType)
      {
        selectedType = procform->amprocrighttype;
        selectedFunc = procform->amproc;
      }
    }
    ReleaseCatCacheList(proclist);

       
                                                                    
                                                                  
                               
       
    if (nfuncs == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("RANGE with offset PRECEDING/FOLLOWING is not supported for column type %s", format_type_be(rangeopcintype)), parser_errposition(pstate, exprLocation(node))));
    }
    if (nmatches == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("RANGE with offset PRECEDING/FOLLOWING is not supported for column type %s and offset type %s", format_type_be(rangeopcintype), format_type_be(nodeType)), errhint("Cast the offset value to an appropriate type."), parser_errposition(pstate, exprLocation(node))));
    }
    if (nmatches != 1 && selectedType != preferredType)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("RANGE with offset PRECEDING/FOLLOWING has multiple interpretations for column type %s and offset type %s", format_type_be(rangeopcintype), format_type_be(nodeType)), errhint("Cast the offset value to the exact intended type."), parser_errposition(pstate, exprLocation(node))));
    }

                                                 
    constructName = "RANGE";
    node = coerce_to_specific_type(pstate, node, selectedType, constructName);
    *inRangeFunc = selectedFunc;
  }
  else if (frameOptions & FRAMEOPTION_GROUPS)
  {
                                           
    node = transformExpr(pstate, clause, EXPR_KIND_WINDOW_FRAME_GROUPS);

       
                                                
       
    constructName = "GROUPS";
    node = coerce_to_specific_type(pstate, node, INT8OID, constructName);
  }
  else
  {
    Assert(false);
    node = NULL;
  }

                                           
  checkExprIsVarFree(pstate, node, constructName);

  return node;
}
