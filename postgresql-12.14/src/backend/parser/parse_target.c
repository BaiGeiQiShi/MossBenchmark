                                                                            
   
                  
                         
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/typcache.h"

static void
markTargetListOrigin(ParseState *pstate, TargetEntry *tle, Var *var, int levelsup);
static Node *
transformAssignmentIndirection(ParseState *pstate, Node *basenode, const char *targetName, bool targetIsSubscripting, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, ListCell *indirection, Node *rhs, int location);
static Node *
transformAssignmentSubscripts(ParseState *pstate, Node *basenode, const char *targetName, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, List *subscripts, bool isSlice, ListCell *next_indirection, Node *rhs, int location);
static List *
ExpandColumnRefStar(ParseState *pstate, ColumnRef *cref, bool make_target_entry);
static List *
ExpandAllTables(ParseState *pstate, int location);
static List *
ExpandIndirectionStar(ParseState *pstate, A_Indirection *ind, bool make_target_entry, ParseExprKind exprKind);
static List *
ExpandSingleTable(ParseState *pstate, RangeTblEntry *rte, int location, bool make_target_entry);
static List *
ExpandRowReference(ParseState *pstate, Node *expr, bool make_target_entry);
static int
FigureColnameInternal(Node *node, char **name);

   
                          
                                                                          
                                                                           
                                                                    
   
                                                                  
                                                                         
                                                           
                                                                    
                                                                      
                                          
   
TargetEntry *
transformTargetEntry(ParseState *pstate, Node *node, Node *expr, ParseExprKind exprKind, char *colname, bool resjunk)
{
                                                         
  if (expr == NULL)
  {
       
                                                                     
                                                                      
                                       
       
    if (exprKind == EXPR_KIND_UPDATE_SOURCE && IsA(node, SetToDefault))
    {
      expr = node;
    }
    else
    {
      expr = transformExpr(pstate, node, exprKind);
    }
  }

  if (colname == NULL && !resjunk)
  {
       
                                                                         
                               
       
    colname = FigureColname(node);
  }

  return makeTargetEntry((Expr *)expr, (AttrNumber)pstate->p_next_resno++, colname, resjunk);
}

   
                         
                                                             
   
                                                                          
                                                                            
                                                                    
   
List *
transformTargetList(ParseState *pstate, List *targetlist, ParseExprKind exprKind)
{
  List *p_target = NIL;
  bool expand_star;
  ListCell *o_target;

                                                              
  Assert(pstate->p_multiassign_exprs == NIL);

                                                                    
  expand_star = (exprKind != EXPR_KIND_UPDATE_SOURCE);

  foreach (o_target, targetlist)
  {
    ResTarget *res = (ResTarget *)lfirst(o_target);

       
                                                                    
                                                                          
                                                         
       
    if (expand_star)
    {
      if (IsA(res->val, ColumnRef))
      {
        ColumnRef *cref = (ColumnRef *)res->val;

        if (IsA(llast(cref->fields), A_Star))
        {
                                                             
          p_target = list_concat(p_target, ExpandColumnRefStar(pstate, cref, true));
          continue;
        }
      }
      else if (IsA(res->val, A_Indirection))
      {
        A_Indirection *ind = (A_Indirection *)res->val;

        if (IsA(llast(ind->indirection), A_Star))
        {
                                                             
          p_target = list_concat(p_target, ExpandIndirectionStar(pstate, ind, true, exprKind));
          continue;
        }
      }
    }

       
                                                                        
                                                     
       
    p_target = lappend(p_target, transformTargetEntry(pstate, res->val, NULL, exprKind, res->name, false));
  }

     
                                                                           
                                                                         
                                                                             
                            
     
  if (pstate->p_multiassign_exprs)
  {
    Assert(exprKind == EXPR_KIND_UPDATE_SOURCE);
    p_target = list_concat(p_target, pstate->p_multiassign_exprs);
    pstate->p_multiassign_exprs = NIL;
  }

  return p_target;
}

   
                             
   
                                                                            
                                                                              
                                                                             
                                                                            
                                                                       
                        
   
                                                                       
                                          
   
List *
transformExpressionList(ParseState *pstate, List *exprlist, ParseExprKind exprKind, bool allowDefault)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, exprlist)
  {
    Node *e = (Node *)lfirst(lc);

       
                                                                    
                                                                          
                                                         
       
    if (IsA(e, ColumnRef))
    {
      ColumnRef *cref = (ColumnRef *)e;

      if (IsA(llast(cref->fields), A_Star))
      {
                                                           
        result = list_concat(result, ExpandColumnRefStar(pstate, cref, false));
        continue;
      }
    }
    else if (IsA(e, A_Indirection))
    {
      A_Indirection *ind = (A_Indirection *)e;

      if (IsA(llast(ind->indirection), A_Star))
      {
                                                           
        result = list_concat(result, ExpandIndirectionStar(pstate, ind, false, exprKind));
        continue;
      }
    }

       
                                                                          
                                                                   
                                                                       
                              
       
    if (allowDefault && IsA(e, SetToDefault))
                      ;
    else
    {
      e = transformExpr(pstate, e, exprKind);
    }

    result = lappend(result, e);
  }

  return result;
}

   
                               
                                                              
   
                                                                             
                            
   
void
resolveTargetListUnknowns(ParseState *pstate, List *targetlist)
{
  ListCell *l;

  foreach (l, targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);
    Oid restype = exprType((Node *)tle->expr);

    if (restype == UNKNOWNOID)
    {
      tle->expr = (Expr *)coerce_type(pstate, (Node *)tle->expr, restype, TEXTOID, -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
    }
  }
}

   
                           
                                                                 
                                   
   
                                                                            
                                                                           
   
void
markTargetListOrigins(ParseState *pstate, List *targetlist)
{
  ListCell *l;

  foreach (l, targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    markTargetListOrigin(pstate, tle, (Var *)tle->expr, 0);
  }
}

   
                          
                                                                      
   
                                                                             
   
                                                                          
                                                                          
   
static void
markTargetListOrigin(ParseState *pstate, TargetEntry *tle, Var *var, int levelsup)
{
  int netlevelsup;
  RangeTblEntry *rte;
  AttrNumber attnum;

  if (var == NULL || !IsA(var, Var))
  {
    return;
  }
  netlevelsup = var->varlevelsup + levelsup;
  rte = GetRTEByRangeTablePosn(pstate, var->varno, netlevelsup);
  attnum = var->varattno;

  switch (rte->rtekind)
  {
  case RTE_RELATION:
                                         
    tle->resorigtbl = rte->relid;
    tle->resorigcol = attnum;
    break;
  case RTE_SUBQUERY:
                                                       
    if (attnum != InvalidAttrNumber)
    {
      TargetEntry *ste = get_tle_by_resno(rte->subquery->targetList, attnum);

      if (ste == NULL || ste->resjunk)
      {
        elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
      }
      tle->resorigtbl = ste->resorigtbl;
      tle->resorigcol = ste->resorigcol;
    }
    break;
  case RTE_JOIN:
                                                             
    if (attnum != InvalidAttrNumber)
    {
      Var *aliasvar;

      Assert(attnum > 0 && attnum <= list_length(rte->joinaliasvars));
      aliasvar = (Var *)list_nth(rte->joinaliasvars, attnum - 1);
                                                                
      markTargetListOrigin(pstate, tle, aliasvar, netlevelsup);
    }
    break;
  case RTE_FUNCTION:
  case RTE_VALUES:
  case RTE_TABLEFUNC:
  case RTE_NAMEDTUPLESTORE:
  case RTE_RESULT:
                                                  
    break;
  case RTE_CTE:

       
                                                                     
                                                                   
                                                                      
                                                                       
                                                                    
                                                   
       
    if (attnum != InvalidAttrNumber && !rte->self_reference)
    {
      CommonTableExpr *cte = GetCTEForRTE(pstate, rte, netlevelsup);
      TargetEntry *ste;

      ste = get_tle_by_resno(GetCTETargetList(cte), attnum);
      if (ste == NULL || ste->resjunk)
      {
        elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
      }
      tle->resorigtbl = ste->resorigtbl;
      tle->resorigcol = ste->resorigcol;
    }
    break;
  }
}

   
                           
                                                                      
                                                              
                                                                      
                                                                     
                                                                   
                                         
   
                       
                                    
                                                                  
                                                                         
                                   
                                                                
                                                                
   
                                    
   
                                                                         
                                                                       
                                                                   
                                                                      
   
Expr *
transformAssignedExpr(ParseState *pstate, Expr *expr, ParseExprKind exprKind, const char *colname, int attrno, List *indirection, int location)
{
  Relation rd = pstate->p_target_relation;
  Oid type_id;                              
  Oid attrtype;                            
  int32 attrtypmod;
  Oid attrcollation;                                 
  ParseExprKind sv_expr_kind;

     
                                                                          
                                                                        
                              
     
  Assert(exprKind != EXPR_KIND_NONE);
  sv_expr_kind = pstate->p_expr_kind;
  pstate->p_expr_kind = exprKind;

  Assert(rd != NULL);
  if (attrno <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot assign to system column \"%s\"", colname), parser_errposition(pstate, location)));
  }
  attrtype = attnumTypeId(rd, attrno);
  attrtypmod = TupleDescAttr(rd->rd_att, attrno - 1)->atttypmod;
  attrcollation = TupleDescAttr(rd->rd_att, attrno - 1)->attcollation;

     
                                                                        
                                                                        
                                                                       
                                                                       
                                                                       
                                                                             
                                                          
     
  if (expr && IsA(expr, SetToDefault))
  {
    SetToDefault *def = (SetToDefault *)expr;

    def->typeId = attrtype;
    def->typeMod = attrtypmod;
    def->collation = attrcollation;
    if (indirection)
    {
      if (IsA(linitial(indirection), A_Indices))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot set an array element to DEFAULT"), parser_errposition(pstate, location)));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot set a subfield to DEFAULT"), parser_errposition(pstate, location)));
      }
    }
  }

                                         
  type_id = exprType((Node *)expr);

     
                                                                       
                                                                            
                                                                            
                                                       
     
  if (indirection)
  {
    Node *colVar;

    if (pstate->p_is_insert)
    {
         
                                                                       
                                                                  
                                       
         
      colVar = (Node *)makeNullConst(attrtype, attrtypmod, attrcollation);
    }
    else
    {
         
                                                   
         
      colVar = (Node *)make_var(pstate, pstate->p_target_rangetblentry, attrno, location);
    }

    expr = (Expr *)transformAssignmentIndirection(pstate, colVar, colname, false, attrtype, attrtypmod, attrcollation, list_head(indirection), (Node *)expr, location);
  }
  else
  {
       
                                                                    
                 
       
    Node *orig_expr = (Node *)expr;

    expr = (Expr *)coerce_to_target_type(pstate, orig_expr, type_id, attrtype, attrtypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (expr == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                         errmsg("column \"%s\" is of type %s"
                                " but expression is of type %s",
                             colname, format_type_be(attrtype), format_type_be(type_id)),
                         errhint("You will need to rewrite or cast the expression."), parser_errposition(pstate, exprLocation(orig_expr))));
    }
  }

  pstate->p_expr_kind = sv_expr_kind;

  return expr;
}

   
                           
                                                                 
                                                                
                                                                 
                                                                      
                                                                  
           
   
                       
                                          
                                                                         
                                   
                                                                
                                                                        
   
void
updateTargetListEntry(ParseState *pstate, TargetEntry *tle, char *colname, int attrno, List *indirection, int location)
{
                                   
  tle->expr = transformAssignedExpr(pstate, tle->expr, EXPR_KIND_UPDATE_TARGET, colname, attrno, indirection, location);

     
                                                                      
                                                                             
                                                                       
                                                                            
     
  tle->resno = (AttrNumber)attrno;
  tle->resname = colname;
}

   
                                                                       
                                                                       
                                                                        
                                                                           
              
   
                                                                           
                                                                        
                                                                              
           
   
                                                                           
                                                                              
                    
   
                                                                         
                                                                           
                          
   
                                                                           
                                                          
   
                                                                             
                                   
   
                                                                             
                                                                           
                                                                          
                                                                   
   
static Node *
transformAssignmentIndirection(ParseState *pstate, Node *basenode, const char *targetName, bool targetIsSubscripting, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, ListCell *indirection, Node *rhs, int location)
{
  Node *result;
  List *subscripts = NIL;
  bool isSlice = false;
  ListCell *i;

  if (indirection && !basenode)
  {
       
                                                                          
                                                                           
                                                                         
                                                                      
                                                       
       
    CaseTestExpr *ctest = makeNode(CaseTestExpr);

    ctest->typeId = targetTypeId;
    ctest->typeMod = targetTypMod;
    ctest->collation = targetCollation;
    basenode = (Node *)ctest;
  }

     
                                                                
                                                                            
                                           
     
  for_each_cell(i, indirection)
  {
    Node *n = lfirst(i);

    if (IsA(n, A_Indices))
    {
      subscripts = lappend(subscripts, n);
      if (((A_Indices *)n)->is_slice)
      {
        isSlice = true;
      }
    }
    else if (IsA(n, A_Star))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("row expansion via \"*\" is not supported here"), parser_errposition(pstate, location)));
    }
    else
    {
      FieldStore *fstore;
      Oid baseTypeId;
      int32 baseTypeMod;
      Oid typrelid;
      AttrNumber attnum;
      Oid fieldTypeId;
      int32 fieldTypMod;
      Oid fieldCollation;

      Assert(IsA(n, String));

                                                          
      if (subscripts)
      {
                                                         
        return transformAssignmentSubscripts(pstate, basenode, targetName, targetTypeId, targetTypMod, targetCollation, subscripts, isSlice, i, rhs, location);
      }

                                                              

         
                                                                     
                                                       
         
      baseTypeMod = targetTypMod;
      baseTypeId = getBaseTypeAndTypmod(targetTypeId, &baseTypeMod);

      typrelid = typeidTypeRelid(baseTypeId);
      if (!typrelid)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot assign to field \"%s\" of column \"%s\" because its type %s is not a composite type", strVal(n), targetName, format_type_be(targetTypeId)), parser_errposition(pstate, location)));
      }

      attnum = get_attnum(typrelid, strVal(n));
      if (attnum == InvalidAttrNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("cannot assign to field \"%s\" of column \"%s\" because there is no such column in data type %s", strVal(n), targetName, format_type_be(targetTypeId)), parser_errposition(pstate, location)));
      }
      if (attnum < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("cannot assign to system column \"%s\"", strVal(n)), parser_errposition(pstate, location)));
      }

      get_atttypetypmodcoll(typrelid, attnum, &fieldTypeId, &fieldTypMod, &fieldCollation);

                                                              
      rhs = transformAssignmentIndirection(pstate, NULL, strVal(n), false, fieldTypeId, fieldTypMod, fieldCollation, lnext(i), rhs, location);

                                       
      fstore = makeNode(FieldStore);
      fstore->arg = (Expr *)basenode;
      fstore->newvals = list_make1(rhs);
      fstore->fieldnums = list_make1_int(attnum);
      fstore->resulttype = baseTypeId;

                                                    
      if (baseTypeId != targetTypeId)
      {
        return coerce_to_domain((Node *)fstore, baseTypeId, baseTypeMod, targetTypeId, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, location, false);
      }

      return (Node *)fstore;
    }
  }

                                           
  if (subscripts)
  {
                                                     
    return transformAssignmentSubscripts(pstate, basenode, targetName, targetTypeId, targetTypMod, targetCollation, subscripts, isSlice, NULL, rhs, location);
  }

                                                          

  result = coerce_to_target_type(pstate, rhs, exprType(rhs), targetTypeId, targetTypMod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
  if (result == NULL)
  {
    if (targetIsSubscripting)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                         errmsg("array assignment to \"%s\" requires type %s"
                                " but expression is of type %s",
                             targetName, format_type_be(targetTypeId), format_type_be(exprType(rhs))),
                         errhint("You will need to rewrite or cast the expression."), parser_errposition(pstate, location)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                         errmsg("subfield \"%s\" is of type %s"
                                " but expression is of type %s",
                             targetName, format_type_be(targetTypeId), format_type_be(exprType(rhs))),
                         errhint("You will need to rewrite or cast the expression."), parser_errposition(pstate, location)));
    }
  }

  return result;
}

   
                                                                           
   
static Node *
transformAssignmentSubscripts(ParseState *pstate, Node *basenode, const char *targetName, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, List *subscripts, bool isSlice, ListCell *next_indirection, Node *rhs, int location)
{
  Node *result;
  Oid containerType;
  int32 containerTypMod;
  Oid elementTypeId;
  Oid typeNeeded;
  Oid collationNeeded;

  Assert(subscripts != NIL);

                                                                
  containerType = targetTypeId;
  containerTypMod = targetTypMod;
  elementTypeId = transformContainerType(&containerType, &containerTypMod);

                                           
  typeNeeded = isSlice ? containerType : elementTypeId;

     
                                                                       
                                                                            
                                               
     
  if (containerType == targetTypeId)
  {
    collationNeeded = targetCollation;
  }
  else
  {
    collationNeeded = get_typcollation(containerType);
  }

                                                              
  rhs = transformAssignmentIndirection(pstate, NULL, targetName, true, typeNeeded, containerTypMod, collationNeeded, next_indirection, rhs, location);

                          
  result = (Node *)transformContainerSubscripts(pstate, basenode, containerType, elementTypeId, containerTypMod, subscripts, rhs);

                                                                              
  if (containerType != targetTypeId)
  {
    Oid resulttype = exprType(result);

    result = coerce_to_target_type(pstate, result, resulttype, targetTypeId, targetTypMod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
                                                                           
    if (result == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(resulttype), format_type_be(targetTypeId)), parser_errposition(pstate, location)));
    }
  }

  return result;
}

   
                        
                                                                  
                                                                       
                                                                    
   
List *
checkInsertTargets(ParseState *pstate, List *cols, List **attrnos)
{
  *attrnos = NIL;

  if (cols == NIL)
  {
       
                                                
       
    int numcol = RelationGetNumberOfAttributes(pstate->p_target_relation);

    int i;

    for (i = 0; i < numcol; i++)
    {
      ResTarget *col;
      Form_pg_attribute attr;

      attr = TupleDescAttr(pstate->p_target_relation->rd_att, i);

      if (attr->attisdropped)
      {
        continue;
      }

      col = makeNode(ResTarget);
      col->name = pstrdup(NameStr(attr->attname));
      col->indirection = NIL;
      col->val = NULL;
      col->location = -1;
      cols = lappend(cols, col);
      *attrnos = lappend_int(*attrnos, i + 1);
    }
  }
  else
  {
       
                                                                  
       
    Bitmapset *wholecols = NULL;
    Bitmapset *partialcols = NULL;
    ListCell *tl;

    foreach (tl, cols)
    {
      ResTarget *col = (ResTarget *)lfirst(tl);
      char *name = col->name;
      int attrno;

                                                  
      attrno = attnameAttNum(pstate->p_target_relation, name, false);
      if (attrno == InvalidAttrNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", name, RelationGetRelationName(pstate->p_target_relation)), parser_errposition(pstate, col->location)));
      }

         
                                                                      
                                                    
         
      if (col->indirection == NIL)
      {
                                                              
        if (bms_is_member(attrno, wholecols) || bms_is_member(attrno, partialcols))
        {
          ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" specified more than once", name), parser_errposition(pstate, col->location)));
        }
        wholecols = bms_add_member(wholecols, attrno);
      }
      else
      {
                                                                
        if (bms_is_member(attrno, wholecols))
        {
          ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" specified more than once", name), parser_errposition(pstate, col->location)));
        }
        partialcols = bms_add_member(partialcols, attrno);
      }

      *attrnos = lappend_int(*attrnos, attrno);
    }
  }

  return cols;
}

   
                         
                                                                       
   
                                                                         
                                                                             
                                                                           
                                                                       
                 
   
                                                                 
   
static List *
ExpandColumnRefStar(ParseState *pstate, ColumnRef *cref, bool make_target_entry)
{
  List *fields = cref->fields;
  int numnames = list_length(fields);

  if (numnames == 1)
  {
       
                                                    
       
                                       
       
                                                                          
                                                               
       
    Assert(make_target_entry);
    return ExpandAllTables(pstate, cref->location);
  }
  else
  {
       
                                                    
       
                                                  
       
                                                                          
                                                                           
                                                                    
                                                                   
                                                                     
                                                                  
                                                                          
                                                           
       
    char *nspname = NULL;
    char *relname = NULL;
    RangeTblEntry *rte = NULL;
    int levels_up;
    enum
    {
      CRSERR_NO_RTE,
      CRSERR_WRONG_DB,
      CRSERR_TOO_MANY
    } crserr = CRSERR_NO_RTE;

       
                                                                          
                                                    
       
    if (pstate->p_pre_columnref_hook != NULL)
    {
      Node *node;

      node = pstate->p_pre_columnref_hook(pstate, cref);
      if (node != NULL)
      {
        return ExpandRowReference(pstate, node, make_target_entry);
      }
    }

    switch (numnames)
    {
    case 2:
      relname = strVal(linitial(fields));
      rte = refnameRangeTblEntry(pstate, nspname, relname, cref->location, &levels_up);
      break;
    case 3:
      nspname = strVal(linitial(fields));
      relname = strVal(lsecond(fields));
      rte = refnameRangeTblEntry(pstate, nspname, relname, cref->location, &levels_up);
      break;
    case 4:
    {
      char *catname = strVal(linitial(fields));

         
                                                       
         
      if (strcmp(catname, get_database_name(MyDatabaseId)) != 0)
      {
        crserr = CRSERR_WRONG_DB;
        break;
      }
      nspname = strVal(lsecond(fields));
      relname = strVal(lthird(fields));
      rte = refnameRangeTblEntry(pstate, nspname, relname, cref->location, &levels_up);
      break;
    }
    default:
      crserr = CRSERR_TOO_MANY;
      break;
    }

       
                                                                         
                                                                   
                                                                        
                                                                     
                   
       
    if (pstate->p_post_columnref_hook != NULL)
    {
      Node *node;

      node = pstate->p_post_columnref_hook(pstate, cref, (Node *)rte);
      if (node != NULL)
      {
        if (rte != NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("column reference \"%s\" is ambiguous", NameListToString(cref->fields)), parser_errposition(pstate, cref->location)));
        }
        return ExpandRowReference(pstate, node, make_target_entry);
      }
    }

       
                                            
       
    if (rte == NULL)
    {
      switch (crserr)
      {
      case CRSERR_NO_RTE:
        errorMissingRTE(pstate, makeRangeVar(nspname, relname, cref->location));
        break;
      case CRSERR_WRONG_DB:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cross-database references are not implemented: %s", NameListToString(cref->fields)), parser_errposition(pstate, cref->location)));
        break;
      case CRSERR_TOO_MANY:
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("improper qualified name (too many dotted names): %s", NameListToString(cref->fields)), parser_errposition(pstate, cref->location)));
        break;
      }
    }

       
                                       
       
    return ExpandSingleTable(pstate, rte, cref->location, make_target_entry);
  }
}

   
                     
                                                                           
   
                                                                         
                                                                               
                                                                               
        
   
                                                                           
   
static List *
ExpandAllTables(ParseState *pstate, int location)
{
  List *target = NIL;
  bool found_table = false;
  ListCell *l;

  foreach (l, pstate->p_namespace)
  {
    ParseNamespaceItem *nsitem = (ParseNamespaceItem *)lfirst(l);
    RangeTblEntry *rte = nsitem->p_rte;

                                 
    if (!nsitem->p_cols_visible)
    {
      continue;
    }
                                                                        
    Assert(!nsitem->p_lateral_only);
                                                 
    found_table = true;

    target = list_concat(target, expandRelAttrs(pstate, rte, RTERangeTablePosn(pstate, rte, NULL), 0, location));
  }

     
                                                                         
                                                                         
            
     
  if (!found_table)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("SELECT * with no tables specified is not valid"), parser_errposition(pstate, location)));
  }

  return target;
}

   
                           
                                                                       
   
                                                                              
                                                                             
                                                                            
                                                                        
                                                                         
                                         
   
static List *
ExpandIndirectionStar(ParseState *pstate, A_Indirection *ind, bool make_target_entry, ParseExprKind exprKind)
{
  Node *expr;

                                                                     
  ind = copyObject(ind);
  ind->indirection = list_truncate(ind->indirection, list_length(ind->indirection) - 1);

                          
  expr = transformExpr(pstate, (Node *)ind, exprKind);

                                                            
  return ExpandRowReference(pstate, expr, make_target_entry);
}

   
                       
                                                                       
   
                                                                      
                                                                          
   
                                                                 
   
static List *
ExpandSingleTable(ParseState *pstate, RangeTblEntry *rte, int location, bool make_target_entry)
{
  int sublevels_up;
  int rtindex;

  rtindex = RTERangeTablePosn(pstate, rte, &sublevels_up);

  if (make_target_entry)
  {
                                                    
    return expandRelAttrs(pstate, rte, rtindex, sublevels_up, location);
  }
  else
  {
    List *vars;
    ListCell *l;

    expandRTE(rte, rtindex, sublevels_up, location, false, NULL, &vars);

       
                                                                          
                                                                           
                
       
    rte->requiredPerms |= ACL_SELECT;

                                            
    foreach (l, vars)
    {
      Var *var = (Var *)lfirst(l);

      markVarForSelectPriv(pstate, var, rte);
    }

    return vars;
  }
}

   
                        
                                                                       
   
                                                                           
         
   
static List *
ExpandRowReference(ParseState *pstate, Node *expr, bool make_target_entry)
{
  List *result = NIL;
  TupleDesc tupleDesc;
  int numAttrs;
  int i;

     
                                                                            
                                                                          
                                                                         
                                                                            
                                                                           
                                                                         
                            
     
  if (IsA(expr, Var) && ((Var *)expr)->varattno == InvalidAttrNumber)
  {
    Var *var = (Var *)expr;
    RangeTblEntry *rte;

    rte = GetRTEByRangeTablePosn(pstate, var->varno, var->varlevelsup);
    return ExpandSingleTable(pstate, rte, var->location, make_target_entry);
  }

     
                                                                             
                                                                        
                                                                           
                       
     
                                                        
                                                          
     
                                                                           
                                                                            
                                                     
     
  if (IsA(expr, Var) && ((Var *)expr)->vartype == RECORDOID)
  {
    tupleDesc = expandRecordVariable(pstate, (Var *)expr, 0);
  }
  else
  {
    tupleDesc = get_expr_result_tupdesc(expr, false);
  }
  Assert(tupleDesc);

                                                              
  numAttrs = tupleDesc->natts;
  for (i = 0; i < numAttrs; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupleDesc, i);
    FieldSelect *fselect;

    if (att->attisdropped)
    {
      continue;
    }

    fselect = makeNode(FieldSelect);
    fselect->arg = (Expr *)copyObject(expr);
    fselect->fieldnum = i + 1;
    fselect->resulttype = att->atttypid;
    fselect->resulttypmod = att->atttypmod;
                                                        
    fselect->resultcollid = att->attcollation;

    if (make_target_entry)
    {
                                      
      TargetEntry *te;

      te = makeTargetEntry((Expr *)fselect, (AttrNumber)pstate->p_next_resno++, pstrdup(NameStr(att->attname)), false);
      result = lappend(result, te);
    }
    else
    {
      result = lappend(result, fselect);
    }
  }

  return result;
}

   
                        
                                                                    
   
                                                                             
                                                                           
                                                                            
                                                                       
   
                                                                             
   
TupleDesc
expandRecordVariable(ParseState *pstate, Var *var, int levelsup)
{
  TupleDesc tupleDesc;
  int netlevelsup;
  RangeTblEntry *rte;
  AttrNumber attnum;
  Node *expr;

                                      
  Assert(IsA(var, Var));
  Assert(var->vartype == RECORDOID);

  netlevelsup = var->varlevelsup + levelsup;
  rte = GetRTEByRangeTablePosn(pstate, var->varno, netlevelsup);
  attnum = var->varattno;

  if (attnum == InvalidAttrNumber)
  {
                                                                   
    List *names, *vars;
    ListCell *lname, *lvar;
    int i;

    expandRTE(rte, var->varno, 0, var->location, false, &names, &vars);

    tupleDesc = CreateTemplateTupleDesc(list_length(vars));
    i = 1;
    forboth(lname, names, lvar, vars)
    {
      char *label = strVal(lfirst(lname));
      Node *varnode = (Node *)lfirst(lvar);

      TupleDescInitEntry(tupleDesc, i, label, exprType(varnode), exprTypmod(varnode), 0);
      TupleDescInitEntryCollation(tupleDesc, i, exprCollation(varnode));
      i++;
    }
    Assert(lname == NULL && lvar == NULL);                         

    return tupleDesc;
  }

  expr = (Node *)var;                                     

  switch (rte->rtekind)
  {
  case RTE_RELATION:
  case RTE_VALUES:
  case RTE_NAMEDTUPLESTORE:
  case RTE_RESULT:

       
                                                                     
                                                                       
                              
       
    break;
  case RTE_SUBQUERY:
  {
                                                             
    TargetEntry *ste = get_tle_by_resno(rte->subquery->targetList, attnum);

    if (ste == NULL || ste->resjunk)
    {
      elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
    }
    expr = (Node *)ste->expr;
    if (IsA(expr, Var))
    {
         
                                                                
                                                                 
                                                            
         
      ParseState mypstate;

      MemSet(&mypstate, 0, sizeof(mypstate));
      mypstate.parentParseState = pstate;
      mypstate.p_rtable = rte->subquery->rtable;
                                                            

      return expandRecordVariable(&mypstate, (Var *)expr, 0);
    }
                                                     
  }
  break;
  case RTE_JOIN:
                                                             
    Assert(attnum > 0 && attnum <= list_length(rte->joinaliasvars));
    expr = (Node *)list_nth(rte->joinaliasvars, attnum - 1);
    Assert(expr != NULL);
                                                              
    if (IsA(expr, Var))
    {
      return expandRecordVariable(pstate, (Var *)expr, netlevelsup);
    }
                                                     
    break;
  case RTE_FUNCTION:

       
                                                                      
                                                           
       
    break;
  case RTE_TABLEFUNC:

       
                                                            
       
    break;
  case RTE_CTE:
                                                       
    if (!rte->self_reference)
    {
      CommonTableExpr *cte = GetCTEForRTE(pstate, rte, netlevelsup);
      TargetEntry *ste;

      ste = get_tle_by_resno(GetCTETargetList(cte), attnum);
      if (ste == NULL || ste->resjunk)
      {
        elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
      }
      expr = (Node *)ste->expr;
      if (IsA(expr, Var))
      {
           
                                                                  
                                                                   
                                                               
                                  
           
        ParseState mypstate;
        Index levelsup;

        MemSet(&mypstate, 0, sizeof(mypstate));
                                                         
        for (levelsup = 0; levelsup < rte->ctelevelsup + netlevelsup; levelsup++)
        {
          pstate = pstate->parentParseState;
        }
        mypstate.parentParseState = pstate;
        mypstate.p_rtable = ((Query *)cte->ctequery)->rtable;
                                                              

        return expandRecordVariable(&mypstate, (Var *)expr, 0);
      }
                                                       
    }
    break;
  }

     
                                                                   
                                                        
     
  return get_expr_result_tupdesc(expr, false);
}

   
                   
                                                                        
                                                                         
                               
   
                                                                           
                                                                         
   
char *
FigureColname(Node *node)
{
  char *name = NULL;

  (void)FigureColnameInternal(node, &name);
  if (name != NULL)
  {
    return name;
  }
                                                 
  return "?column?";
}

   
                        
                                                          
   
                                                                      
                              
   
char *
FigureIndexColname(Node *node)
{
  char *name = NULL;

  (void)FigureColnameInternal(node, &name);
  return name;
}

   
                           
                                          
   
                                                            
                       
                                
                         
                                                      
                                                              
   
static int
FigureColnameInternal(Node *node, char **name)
{
  int strength = 0;

  if (node == NULL)
  {
    return strength;
  }

  switch (nodeTag(node))
  {
  case T_ColumnRef:
  {
    char *fname = NULL;
    ListCell *l;

                                                    
    foreach (l, ((ColumnRef *)node)->fields)
    {
      Node *i = lfirst(l);

      if (IsA(i, String))
      {
        fname = strVal(i);
      }
    }
    if (fname)
    {
      *name = fname;
      return 2;
    }
  }
  break;
  case T_A_Indirection:
  {
    A_Indirection *ind = (A_Indirection *)node;
    char *fname = NULL;
    ListCell *l;

                                                                   
    foreach (l, ind->indirection)
    {
      Node *i = lfirst(l);

      if (IsA(i, String))
      {
        fname = strVal(i);
      }
    }
    if (fname)
    {
      *name = fname;
      return 2;
    }
    return FigureColnameInternal(ind->arg, name);
  }
  break;
  case T_FuncCall:
    *name = strVal(llast(((FuncCall *)node)->funcname));
    return 2;
  case T_A_Expr:
    if (((A_Expr *)node)->kind == AEXPR_NULLIF)
    {
                                                     
      *name = "nullif";
      return 2;
    }
    if (((A_Expr *)node)->kind == AEXPR_PAREN)
    {
                                               
      return FigureColnameInternal(((A_Expr *)node)->lexpr, name);
    }
    break;
  case T_TypeCast:
    strength = FigureColnameInternal(((TypeCast *)node)->arg, name);
    if (strength <= 1)
    {
      if (((TypeCast *)node)->typeName != NULL)
      {
        *name = strVal(llast(((TypeCast *)node)->typeName->names));
        return 1;
      }
    }
    break;
  case T_CollateClause:
    return FigureColnameInternal(((CollateClause *)node)->arg, name);
  case T_GroupingFunc:
                                                     
    *name = "grouping";
    return 2;
  case T_SubLink:
    switch (((SubLink *)node)->subLinkType)
    {
    case EXISTS_SUBLINK:
      *name = "exists";
      return 2;
    case ARRAY_SUBLINK:
      *name = "array";
      return 2;
    case EXPR_SUBLINK:
    {
                                                           
      SubLink *sublink = (SubLink *)node;
      Query *query = (Query *)sublink->subselect;

         
                                                             
                                                           
                                                        
                                                           
                         
         
      if (IsA(query, Query))
      {
        TargetEntry *te = (TargetEntry *)linitial(query->targetList);

        if (te->resname)
        {
          *name = te->resname;
          return 2;
        }
      }
    }
    break;
                                                                  
    case MULTIEXPR_SUBLINK:
    case ALL_SUBLINK:
    case ANY_SUBLINK:
    case ROWCOMPARE_SUBLINK:
    case CTE_SUBLINK:
      break;
    }
    break;
  case T_CaseExpr:
    strength = FigureColnameInternal((Node *)((CaseExpr *)node)->defresult, name);
    if (strength <= 1)
    {
      *name = "case";
      return 1;
    }
    break;
  case T_A_ArrayExpr:
                                          
    *name = "array";
    return 2;
  case T_RowExpr:
                                        
    *name = "row";
    return 2;
  case T_CoalesceExpr:
                                                     
    *name = "coalesce";
    return 2;
  case T_MinMaxExpr:
                                                         
    switch (((MinMaxExpr *)node)->op)
    {
    case IS_GREATEST:
      *name = "greatest";
      return 2;
    case IS_LEAST:
      *name = "least";
      return 2;
    }
    break;
  case T_SQLValueFunction:
                                                    
    switch (((SQLValueFunction *)node)->op)
    {
    case SVFOP_CURRENT_DATE:
      *name = "current_date";
      return 2;
    case SVFOP_CURRENT_TIME:
    case SVFOP_CURRENT_TIME_N:
      *name = "current_time";
      return 2;
    case SVFOP_CURRENT_TIMESTAMP:
    case SVFOP_CURRENT_TIMESTAMP_N:
      *name = "current_timestamp";
      return 2;
    case SVFOP_LOCALTIME:
    case SVFOP_LOCALTIME_N:
      *name = "localtime";
      return 2;
    case SVFOP_LOCALTIMESTAMP:
    case SVFOP_LOCALTIMESTAMP_N:
      *name = "localtimestamp";
      return 2;
    case SVFOP_CURRENT_ROLE:
      *name = "current_role";
      return 2;
    case SVFOP_CURRENT_USER:
      *name = "current_user";
      return 2;
    case SVFOP_USER:
      *name = "user";
      return 2;
    case SVFOP_SESSION_USER:
      *name = "session_user";
      return 2;
    case SVFOP_CURRENT_CATALOG:
      *name = "current_catalog";
      return 2;
    case SVFOP_CURRENT_SCHEMA:
      *name = "current_schema";
      return 2;
    }
    break;
  case T_XmlExpr:
                                                            
    switch (((XmlExpr *)node)->op)
    {
    case IS_XMLCONCAT:
      *name = "xmlconcat";
      return 2;
    case IS_XMLELEMENT:
      *name = "xmlelement";
      return 2;
    case IS_XMLFOREST:
      *name = "xmlforest";
      return 2;
    case IS_XMLPARSE:
      *name = "xmlparse";
      return 2;
    case IS_XMLPI:
      *name = "xmlpi";
      return 2;
    case IS_XMLROOT:
      *name = "xmlroot";
      return 2;
    case IS_XMLSERIALIZE:
      *name = "xmlserialize";
      return 2;
    case IS_DOCUMENT:
                   
      break;
    }
    break;
  case T_XmlSerialize:
    *name = "xmlserialize";
    return 2;
  default:
    break;
  }

  return strength;
}
