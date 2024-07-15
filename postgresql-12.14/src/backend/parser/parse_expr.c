                                                                            
   
                
                                  
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_agg.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"
#include "utils/xml.h"

                    
bool operator_precedence_warning = false;
bool Transform_null_equals = false;

   
                                                     
                                                       
   
#define PREC_GROUP_POSTFIX_IS 1                                         
#define PREC_GROUP_INFIX_IS 2                                           
#define PREC_GROUP_LESS 3                  
#define PREC_GROUP_EQUAL 4               
#define PREC_GROUP_LESS_EQUAL 5                 
#define PREC_GROUP_LIKE 6                                 
#define PREC_GROUP_BETWEEN 7                   
#define PREC_GROUP_IN 8                   
#define PREC_GROUP_NOT_LIKE 9                                 
#define PREC_GROUP_NOT_BETWEEN 10                  
#define PREC_GROUP_NOT_IN 11                  
#define PREC_GROUP_POSTFIX_OP 12                                 
#define PREC_GROUP_INFIX_OP 13                                 
#define PREC_GROUP_PREFIX_OP 14                                 

   
                                                       
   
                         
          
        
          
                         
              
         
                         
                                     
                        
                                             
   
                                                                            
                                                                          
                                                                
   
static const int oldprecedence_l[] = {0, 10, 10, 3, 2, 8, 4, 5, 6, 4, 5, 6, 7, 8, 9};
static const int oldprecedence_r[] = {0, 10, 10, 3, 2, 8, 4, 5, 6, 1, 1, 1, 7, 8, 9};

static Node *
transformExprRecurse(ParseState *pstate, Node *expr);
static Node *
transformParamRef(ParseState *pstate, ParamRef *pref);
static Node *
transformAExprOp(ParseState *pstate, A_Expr *a);
static Node *
transformAExprOpAny(ParseState *pstate, A_Expr *a);
static Node *
transformAExprOpAll(ParseState *pstate, A_Expr *a);
static Node *
transformAExprDistinct(ParseState *pstate, A_Expr *a);
static Node *
transformAExprNullIf(ParseState *pstate, A_Expr *a);
static Node *
transformAExprOf(ParseState *pstate, A_Expr *a);
static Node *
transformAExprIn(ParseState *pstate, A_Expr *a);
static Node *
transformAExprBetween(ParseState *pstate, A_Expr *a);
static Node *
transformBoolExpr(ParseState *pstate, BoolExpr *a);
static Node *
transformFuncCall(ParseState *pstate, FuncCall *fn);
static Node *
transformMultiAssignRef(ParseState *pstate, MultiAssignRef *maref);
static Node *
transformCaseExpr(ParseState *pstate, CaseExpr *c);
static Node *
transformSubLink(ParseState *pstate, SubLink *sublink);
static Node *
transformArrayExpr(ParseState *pstate, A_ArrayExpr *a, Oid array_type, Oid element_type, int32 typmod);
static Node *
transformRowExpr(ParseState *pstate, RowExpr *r, bool allowDefault);
static Node *
transformCoalesceExpr(ParseState *pstate, CoalesceExpr *c);
static Node *
transformMinMaxExpr(ParseState *pstate, MinMaxExpr *m);
static Node *
transformSQLValueFunction(ParseState *pstate, SQLValueFunction *svf);
static Node *
transformXmlExpr(ParseState *pstate, XmlExpr *x);
static Node *
transformXmlSerialize(ParseState *pstate, XmlSerialize *xs);
static Node *
transformBooleanTest(ParseState *pstate, BooleanTest *b);
static Node *
transformCurrentOfExpr(ParseState *pstate, CurrentOfExpr *cexpr);
static Node *
transformColumnRef(ParseState *pstate, ColumnRef *cref);
static Node *
transformWholeRowRef(ParseState *pstate, RangeTblEntry *rte, int location);
static Node *
transformIndirection(ParseState *pstate, A_Indirection *ind);
static Node *
transformTypeCast(ParseState *pstate, TypeCast *tc);
static Node *
transformCollateClause(ParseState *pstate, CollateClause *c);
static Node *
make_row_comparison_op(ParseState *pstate, List *opname, List *largs, List *rargs, int location);
static Node *
make_row_distinct_op(ParseState *pstate, List *opname, RowExpr *lrow, RowExpr *rrow, int location);
static Expr *
make_distinct_op(ParseState *pstate, List *opname, Node *ltree, Node *rtree, int location);
static Node *
make_nulltest_from_distinct(ParseState *pstate, A_Expr *distincta, Node *arg);
static int
operator_precedence_group(Node *node, const char **nodename);
static void
emit_precedence_warnings(ParseState *pstate, int opgroup, const char *opname, Node *lchild, Node *rchild, int location);

   
                   
                                                                          
                                                                      
                                                       
   
Node *
transformExpr(ParseState *pstate, Node *expr, ParseExprKind exprKind)
{
  Node *result;
  ParseExprKind sv_expr_kind;

                                                                  
  Assert(exprKind != EXPR_KIND_NONE);
  sv_expr_kind = pstate->p_expr_kind;
  pstate->p_expr_kind = exprKind;

  result = transformExprRecurse(pstate, expr);

  pstate->p_expr_kind = sv_expr_kind;

  return result;
}

static Node *
transformExprRecurse(ParseState *pstate, Node *expr)
{
  Node *result;

  if (expr == NULL)
  {
    return NULL;
  }

                                                                      
  check_stack_depth();

  switch (nodeTag(expr))
  {
  case T_ColumnRef:
    result = transformColumnRef(pstate, (ColumnRef *)expr);
    break;

  case T_ParamRef:
    result = transformParamRef(pstate, (ParamRef *)expr);
    break;

  case T_A_Const:
  {
    A_Const *con = (A_Const *)expr;
    Value *val = &con->val;

    result = (Node *)make_const(pstate, val, con->location);
    break;
  }

  case T_A_Indirection:
    result = transformIndirection(pstate, (A_Indirection *)expr);
    break;

  case T_A_ArrayExpr:
    result = transformArrayExpr(pstate, (A_ArrayExpr *)expr, InvalidOid, InvalidOid, -1);
    break;

  case T_TypeCast:
    result = transformTypeCast(pstate, (TypeCast *)expr);
    break;

  case T_CollateClause:
    result = transformCollateClause(pstate, (CollateClause *)expr);
    break;

  case T_A_Expr:
  {
    A_Expr *a = (A_Expr *)expr;

    switch (a->kind)
    {
    case AEXPR_OP:
      result = transformAExprOp(pstate, a);
      break;
    case AEXPR_OP_ANY:
      result = transformAExprOpAny(pstate, a);
      break;
    case AEXPR_OP_ALL:
      result = transformAExprOpAll(pstate, a);
      break;
    case AEXPR_DISTINCT:
    case AEXPR_NOT_DISTINCT:
      result = transformAExprDistinct(pstate, a);
      break;
    case AEXPR_NULLIF:
      result = transformAExprNullIf(pstate, a);
      break;
    case AEXPR_OF:
      result = transformAExprOf(pstate, a);
      break;
    case AEXPR_IN:
      result = transformAExprIn(pstate, a);
      break;
    case AEXPR_LIKE:
    case AEXPR_ILIKE:
    case AEXPR_SIMILAR:
                                                     
      result = transformAExprOp(pstate, a);
      break;
    case AEXPR_BETWEEN:
    case AEXPR_NOT_BETWEEN:
    case AEXPR_BETWEEN_SYM:
    case AEXPR_NOT_BETWEEN_SYM:
      result = transformAExprBetween(pstate, a);
      break;
    case AEXPR_PAREN:
      result = transformExprRecurse(pstate, a->lexpr);
      break;
    default:
      elog(ERROR, "unrecognized A_Expr kind: %d", a->kind);
      result = NULL;                          
      break;
    }
    break;
  }

  case T_BoolExpr:
    result = transformBoolExpr(pstate, (BoolExpr *)expr);
    break;

  case T_FuncCall:
    result = transformFuncCall(pstate, (FuncCall *)expr);
    break;

  case T_MultiAssignRef:
    result = transformMultiAssignRef(pstate, (MultiAssignRef *)expr);
    break;

  case T_GroupingFunc:
    result = transformGroupingFunc(pstate, (GroupingFunc *)expr);
    break;

  case T_NamedArgExpr:
  {
    NamedArgExpr *na = (NamedArgExpr *)expr;

    na->arg = (Expr *)transformExprRecurse(pstate, (Node *)na->arg);
    result = expr;
    break;
  }

  case T_SubLink:
    result = transformSubLink(pstate, (SubLink *)expr);
    break;

  case T_CaseExpr:
    result = transformCaseExpr(pstate, (CaseExpr *)expr);
    break;

  case T_RowExpr:
    result = transformRowExpr(pstate, (RowExpr *)expr, false);
    break;

  case T_CoalesceExpr:
    result = transformCoalesceExpr(pstate, (CoalesceExpr *)expr);
    break;

  case T_MinMaxExpr:
    result = transformMinMaxExpr(pstate, (MinMaxExpr *)expr);
    break;

  case T_SQLValueFunction:
    result = transformSQLValueFunction(pstate, (SQLValueFunction *)expr);
    break;

  case T_XmlExpr:
    result = transformXmlExpr(pstate, (XmlExpr *)expr);
    break;

  case T_XmlSerialize:
    result = transformXmlSerialize(pstate, (XmlSerialize *)expr);
    break;

  case T_NullTest:
  {
    NullTest *n = (NullTest *)expr;

    if (operator_precedence_warning)
    {
      emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_IS, "IS", (Node *)n->arg, NULL, n->location);
    }

    n->arg = (Expr *)transformExprRecurse(pstate, (Node *)n->arg);
                                                          
    n->argisrow = type_is_rowtype(exprType((Node *)n->arg));
    result = expr;
    break;
  }

  case T_BooleanTest:
    result = transformBooleanTest(pstate, (BooleanTest *)expr);
    break;

  case T_CurrentOfExpr:
    result = transformCurrentOfExpr(pstate, (CurrentOfExpr *)expr);
    break;

       
                                                                    
                                                               
       
  case T_SetToDefault:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("DEFAULT is not allowed in this context"), parser_errposition(pstate, ((SetToDefault *)expr)->location)));
    break;

       
                                                               
                                                          
       
                                                                     
                                                                       
                                                            
                                                                  
                                                                    
       
  case T_CaseTestExpr:
  case T_Var:
  {
    result = (Node *)expr;
    break;
  }

  default:
                               
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(expr));
    result = NULL;                          
    break;
  }

  return result;
}

   
                                                                       
   
                                                                           
                                               
   
static void
unknown_attribute(ParseState *pstate, Node *relref, const char *attname, int location)
{
  RangeTblEntry *rte;

  if (IsA(relref, Var) && ((Var *)relref)->varattno == InvalidAttrNumber)
  {
                                                             
    rte = GetRTEByRangeTablePosn(pstate, ((Var *)relref)->varno, ((Var *)relref)->varlevelsup);
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column %s.%s does not exist", rte->eref->aliasname, attname), parser_errposition(pstate, location)));
  }
  else
  {
                                                                  
    Oid relTypeId = exprType(relref);

    if (ISCOMPLEX(relTypeId))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" not found in data type %s", attname, format_type_be(relTypeId)), parser_errposition(pstate, location)));
    }
    else if (relTypeId == RECORDOID)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("could not identify column \"%s\" in record data type", attname), parser_errposition(pstate, location)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE),
                         errmsg("column notation .%s applied to type %s, "
                                "which is not a composite type",
                             attname, format_type_be(relTypeId)),
                         parser_errposition(pstate, location)));
    }
  }
}

static Node *
transformIndirection(ParseState *pstate, A_Indirection *ind)
{
  Node *last_srf = pstate->p_last_srf;
  Node *result = transformExprRecurse(pstate, ind->arg);
  List *subscripts = NIL;
  int location = exprLocation(result);
  ListCell *i;

     
                                                                
                                                                            
                                           
     
  foreach (i, ind->indirection)
  {
    Node *n = lfirst(i);

    if (IsA(n, A_Indices))
    {
      subscripts = lappend(subscripts, n);
    }
    else if (IsA(n, A_Star))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("row expansion via \"*\" is not supported here"), parser_errposition(pstate, location)));
    }
    else
    {
      Node *newresult;

      Assert(IsA(n, String));

                                                          
      if (subscripts)
      {
        result = (Node *)transformContainerSubscripts(pstate, result, exprType(result), InvalidOid, exprTypmod(result), subscripts, NULL);
      }
      subscripts = NIL;

      newresult = ParseFuncOrColumn(pstate, list_make1(n), list_make1(result), last_srf, NULL, false, location);
      if (newresult == NULL)
      {
        unknown_attribute(pstate, result, strVal(n), location);
      }
      result = newresult;
    }
  }
                                           
  if (subscripts)
  {
    result = (Node *)transformContainerSubscripts(pstate, result, exprType(result), InvalidOid, exprTypmod(result), subscripts, NULL);
  }

  return result;
}

   
                          
   
                                                                          
   
static Node *
transformColumnRef(ParseState *pstate, ColumnRef *cref)
{
  Node *node = NULL;
  char *nspname = NULL;
  char *relname = NULL;
  char *colname = NULL;
  RangeTblEntry *rte;
  int levels_up;
  enum
  {
    CRERR_NO_COLUMN,
    CRERR_NO_RTE,
    CRERR_WRONG_DB,
    CRERR_TOO_MANY
  } crerr = CRERR_NO_COLUMN;
  const char *err;

     
                                                                            
                                                                          
                                                  
     
  err = NULL;
  switch (pstate->p_expr_kind)
  {
  case EXPR_KIND_NONE:
    Assert(false);                   
    break;
  case EXPR_KIND_OTHER:
  case EXPR_KIND_JOIN_ON:
  case EXPR_KIND_JOIN_USING:
  case EXPR_KIND_FROM_SUBSELECT:
  case EXPR_KIND_FROM_FUNCTION:
  case EXPR_KIND_WHERE:
  case EXPR_KIND_POLICY:
  case EXPR_KIND_HAVING:
  case EXPR_KIND_FILTER:
  case EXPR_KIND_WINDOW_PARTITION:
  case EXPR_KIND_WINDOW_ORDER:
  case EXPR_KIND_WINDOW_FRAME_RANGE:
  case EXPR_KIND_WINDOW_FRAME_ROWS:
  case EXPR_KIND_WINDOW_FRAME_GROUPS:
  case EXPR_KIND_SELECT_TARGET:
  case EXPR_KIND_INSERT_TARGET:
  case EXPR_KIND_UPDATE_SOURCE:
  case EXPR_KIND_UPDATE_TARGET:
  case EXPR_KIND_GROUP_BY:
  case EXPR_KIND_ORDER_BY:
  case EXPR_KIND_DISTINCT_ON:
  case EXPR_KIND_LIMIT:
  case EXPR_KIND_OFFSET:
  case EXPR_KIND_RETURNING:
  case EXPR_KIND_VALUES:
  case EXPR_KIND_VALUES_SINGLE:
  case EXPR_KIND_CHECK_CONSTRAINT:
  case EXPR_KIND_DOMAIN_CHECK:
  case EXPR_KIND_FUNCTION_DEFAULT:
  case EXPR_KIND_INDEX_EXPRESSION:
  case EXPR_KIND_INDEX_PREDICATE:
  case EXPR_KIND_ALTER_COL_TRANSFORM:
  case EXPR_KIND_EXECUTE_PARAMETER:
  case EXPR_KIND_TRIGGER_WHEN:
  case EXPR_KIND_PARTITION_EXPRESSION:
  case EXPR_KIND_CALL_ARGUMENT:
  case EXPR_KIND_COPY_WHERE:
  case EXPR_KIND_GENERATED_COLUMN:
              
    break;

  case EXPR_KIND_COLUMN_DEFAULT:
    err = _("cannot use column reference in DEFAULT expression");
    break;
  case EXPR_KIND_PARTITION_BOUND:
    err = _("cannot use column reference in partition bound expression");
    break;

       
                                                                 
                                                                
                                                                     
                                                                      
                             
       
  }
  if (err)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg_internal("%s", err), parser_errposition(pstate, cref->location)));
  }

     
                                                                        
                                      
     
  if (pstate->p_pre_columnref_hook != NULL)
  {
    node = pstate->p_pre_columnref_hook(pstate, cref);
    if (node != NULL)
    {
      return node;
    }
  }

               
                               
     
                                                         
                                                                   
                                                        
                                                           
                                                  
                                                          
                                                                 
                                                   
                                                                  
     
                                                                         
                                                                            
                                                                          
                                                                            
                                                                           
     
                                                                          
                                                          
               
     
  switch (list_length(cref->fields))
  {
  case 1:
  {
    Node *field1 = (Node *)linitial(cref->fields);

    Assert(IsA(field1, String));
    colname = strVal(field1);

                                                  
    node = colNameToVar(pstate, colname, false, cref->location);

    if (node == NULL)
    {
         
                                                         
         
                                                             
                                                               
                     
         
                                                         
                                                              
                  
         
      rte = refnameRangeTblEntry(pstate, NULL, colname, cref->location, &levels_up);
      if (rte)
      {
        node = transformWholeRowRef(pstate, rte, cref->location);
      }
    }
    break;
  }
  case 2:
  {
    Node *field1 = (Node *)linitial(cref->fields);
    Node *field2 = (Node *)lsecond(cref->fields);

    Assert(IsA(field1, String));
    relname = strVal(field1);

                                   
    rte = refnameRangeTblEntry(pstate, nspname, relname, cref->location, &levels_up);
    if (rte == NULL)
    {
      crerr = CRERR_NO_RTE;
      break;
    }

                              
    if (IsA(field2, A_Star))
    {
      node = transformWholeRowRef(pstate, rte, cref->location);
      break;
    }

    Assert(IsA(field2, String));
    colname = strVal(field2);

                                                
    node = scanRTEForColumn(pstate, rte, colname, cref->location, 0, NULL);
    if (node == NULL)
    {
                                                      
      node = transformWholeRowRef(pstate, rte, cref->location);
      node = ParseFuncOrColumn(pstate, list_make1(makeString(colname)), list_make1(node), pstate->p_last_srf, NULL, false, cref->location);
    }
    break;
  }
  case 3:
  {
    Node *field1 = (Node *)linitial(cref->fields);
    Node *field2 = (Node *)lsecond(cref->fields);
    Node *field3 = (Node *)lthird(cref->fields);

    Assert(IsA(field1, String));
    nspname = strVal(field1);
    Assert(IsA(field2, String));
    relname = strVal(field2);

                                   
    rte = refnameRangeTblEntry(pstate, nspname, relname, cref->location, &levels_up);
    if (rte == NULL)
    {
      crerr = CRERR_NO_RTE;
      break;
    }

                              
    if (IsA(field3, A_Star))
    {
      node = transformWholeRowRef(pstate, rte, cref->location);
      break;
    }

    Assert(IsA(field3, String));
    colname = strVal(field3);

                                                
    node = scanRTEForColumn(pstate, rte, colname, cref->location, 0, NULL);
    if (node == NULL)
    {
                                                      
      node = transformWholeRowRef(pstate, rte, cref->location);
      node = ParseFuncOrColumn(pstate, list_make1(makeString(colname)), list_make1(node), pstate->p_last_srf, NULL, false, cref->location);
    }
    break;
  }
  case 4:
  {
    Node *field1 = (Node *)linitial(cref->fields);
    Node *field2 = (Node *)lsecond(cref->fields);
    Node *field3 = (Node *)lthird(cref->fields);
    Node *field4 = (Node *)lfourth(cref->fields);
    char *catname;

    Assert(IsA(field1, String));
    catname = strVal(field1);
    Assert(IsA(field2, String));
    nspname = strVal(field2);
    Assert(IsA(field3, String));
    relname = strVal(field3);

       
                                                     
       
    if (strcmp(catname, get_database_name(MyDatabaseId)) != 0)
    {
      crerr = CRERR_WRONG_DB;
      break;
    }

                                   
    rte = refnameRangeTblEntry(pstate, nspname, relname, cref->location, &levels_up);
    if (rte == NULL)
    {
      crerr = CRERR_NO_RTE;
      break;
    }

                              
    if (IsA(field4, A_Star))
    {
      node = transformWholeRowRef(pstate, rte, cref->location);
      break;
    }

    Assert(IsA(field4, String));
    colname = strVal(field4);

                                                
    node = scanRTEForColumn(pstate, rte, colname, cref->location, 0, NULL);
    if (node == NULL)
    {
                                                      
      node = transformWholeRowRef(pstate, rte, cref->location);
      node = ParseFuncOrColumn(pstate, list_make1(makeString(colname)), list_make1(node), pstate->p_last_srf, NULL, false, cref->location);
    }
    break;
  }
  default:
    crerr = CRERR_TOO_MANY;                            
    break;
  }

     
                                                                         
                                                                          
                                                                            
                                                                         
                                                                             
                                                                           
                       
     
  if (pstate->p_post_columnref_hook != NULL)
  {
    Node *hookresult;

    hookresult = pstate->p_post_columnref_hook(pstate, cref, node);
    if (node == NULL)
    {
      node = hookresult;
    }
    else if (hookresult != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("column reference \"%s\" is ambiguous", NameListToString(cref->fields)), parser_errposition(pstate, cref->location)));
    }
  }

     
                                          
     
  if (node == NULL)
  {
    switch (crerr)
    {
    case CRERR_NO_COLUMN:
      errorMissingColumn(pstate, relname, colname, cref->location);
      break;
    case CRERR_NO_RTE:
      errorMissingRTE(pstate, makeRangeVar(nspname, relname, cref->location));
      break;
    case CRERR_WRONG_DB:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cross-database references are not implemented: %s", NameListToString(cref->fields)), parser_errposition(pstate, cref->location)));
      break;
    case CRERR_TOO_MANY:
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("improper qualified name (too many dotted names): %s", NameListToString(cref->fields)), parser_errposition(pstate, cref->location)));
      break;
    }
  }

  return node;
}

static Node *
transformParamRef(ParseState *pstate, ParamRef *pref)
{
  Node *result;

     
                                                                         
                                                                           
     
  if (pstate->p_paramref_hook != NULL)
  {
    result = pstate->p_paramref_hook(pstate, pref);
  }
  else
  {
    result = NULL;
  }

  if (result == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("there is no parameter $%d", pref->number), parser_errposition(pstate, pref->location)));
  }

  return result;
}

                                                            
static bool
exprIsNullConstant(Node *arg)
{
  if (arg && IsA(arg, A_Const))
  {
    A_Const *con = (A_Const *)arg;

    if (con->val.type == T_Null)
    {
      return true;
    }
  }
  return false;
}

static Node *
transformAExprOp(ParseState *pstate, A_Expr *a)
{
  Node *lexpr = a->lexpr;
  Node *rexpr = a->rexpr;
  Node *result;

  if (operator_precedence_warning)
  {
    int opgroup;
    const char *opname;

    opgroup = operator_precedence_group((Node *)a, &opname);
    if (opgroup > 0)
    {
      emit_precedence_warnings(pstate, opgroup, opname, lexpr, rexpr, a->location);
    }

                                                                         
    while (lexpr && IsA(lexpr, A_Expr) && ((A_Expr *)lexpr)->kind == AEXPR_PAREN)
    {
      lexpr = ((A_Expr *)lexpr)->lexpr;
    }
    while (rexpr && IsA(rexpr, A_Expr) && ((A_Expr *)rexpr)->kind == AEXPR_PAREN)
    {
      rexpr = ((A_Expr *)rexpr)->lexpr;
    }
  }

     
                                                                       
                                                                            
                                                                       
                                                           
                                            
     
  if (Transform_null_equals && list_length(a->name) == 1 && strcmp(strVal(linitial(a->name)), "=") == 0 && (exprIsNullConstant(lexpr) || exprIsNullConstant(rexpr)) && (!IsA(lexpr, CaseTestExpr) && !IsA(rexpr, CaseTestExpr)))
  {
    NullTest *n = makeNode(NullTest);

    n->nulltesttype = IS_NULL;
    n->location = a->location;

    if (exprIsNullConstant(lexpr))
    {
      n->arg = (Expr *)rexpr;
    }
    else
    {
      n->arg = (Expr *)lexpr;
    }

    result = transformExprRecurse(pstate, (Node *)n);
  }
  else if (lexpr && IsA(lexpr, RowExpr) && rexpr && IsA(rexpr, SubLink) && ((SubLink *)rexpr)->subLinkType == EXPR_SUBLINK)
  {
       
                                                                          
                                                                          
                                                  
       
    SubLink *s = (SubLink *)rexpr;

    s->subLinkType = ROWCOMPARE_SUBLINK;
    s->testexpr = lexpr;
    s->operName = a->name;
    s->location = a->location;
    result = transformExprRecurse(pstate, (Node *)s);
  }
  else if (lexpr && IsA(lexpr, RowExpr) && rexpr && IsA(rexpr, RowExpr))
  {
                                             
    lexpr = transformExprRecurse(pstate, lexpr);
    rexpr = transformExprRecurse(pstate, rexpr);

    result = make_row_comparison_op(pstate, a->name, castNode(RowExpr, lexpr)->args, castNode(RowExpr, rexpr)->args, a->location);
  }
  else
  {
                                  
    Node *last_srf = pstate->p_last_srf;

    lexpr = transformExprRecurse(pstate, lexpr);
    rexpr = transformExprRecurse(pstate, rexpr);

    result = (Node *)make_op(pstate, a->name, lexpr, rexpr, last_srf, a->location);
  }

  return result;
}

static Node *
transformAExprOpAny(ParseState *pstate, A_Expr *a)
{
  Node *lexpr = a->lexpr;
  Node *rexpr = a->rexpr;

  if (operator_precedence_warning)
  {
    emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_OP, strVal(llast(a->name)), lexpr, NULL, a->location);
  }

  lexpr = transformExprRecurse(pstate, lexpr);
  rexpr = transformExprRecurse(pstate, rexpr);

  return (Node *)make_scalar_array_op(pstate, a->name, true, lexpr, rexpr, a->location);
}

static Node *
transformAExprOpAll(ParseState *pstate, A_Expr *a)
{
  Node *lexpr = a->lexpr;
  Node *rexpr = a->rexpr;

  if (operator_precedence_warning)
  {
    emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_OP, strVal(llast(a->name)), lexpr, NULL, a->location);
  }

  lexpr = transformExprRecurse(pstate, lexpr);
  rexpr = transformExprRecurse(pstate, rexpr);

  return (Node *)make_scalar_array_op(pstate, a->name, false, lexpr, rexpr, a->location);
}

static Node *
transformAExprDistinct(ParseState *pstate, A_Expr *a)
{
  Node *lexpr = a->lexpr;
  Node *rexpr = a->rexpr;
  Node *result;

  if (operator_precedence_warning)
  {
    emit_precedence_warnings(pstate, PREC_GROUP_INFIX_IS, "IS", lexpr, rexpr, a->location);
  }

     
                                                                             
                                                                             
                                                                            
     
  if (exprIsNullConstant(rexpr))
  {
    return make_nulltest_from_distinct(pstate, a, lexpr);
  }
  if (exprIsNullConstant(lexpr))
  {
    return make_nulltest_from_distinct(pstate, a, rexpr);
  }

  lexpr = transformExprRecurse(pstate, lexpr);
  rexpr = transformExprRecurse(pstate, rexpr);

  if (lexpr && IsA(lexpr, RowExpr) && rexpr && IsA(rexpr, RowExpr))
  {
                                             
    result = make_row_distinct_op(pstate, a->name, (RowExpr *)lexpr, (RowExpr *)rexpr, a->location);
  }
  else
  {
                                  
    result = (Node *)make_distinct_op(pstate, a->name, lexpr, rexpr, a->location);
  }

     
                                                                          
                 
     
  if (a->kind == AEXPR_NOT_DISTINCT)
  {
    result = (Node *)makeBoolExpr(NOT_EXPR, list_make1(result), a->location);
  }

  return result;
}

static Node *
transformAExprNullIf(ParseState *pstate, A_Expr *a)
{
  Node *lexpr = transformExprRecurse(pstate, a->lexpr);
  Node *rexpr = transformExprRecurse(pstate, a->rexpr);
  OpExpr *result;

  result = (OpExpr *)make_op(pstate, a->name, lexpr, rexpr, pstate->p_last_srf, a->location);

     
                                                             
     
  if (result->opresulttype != BOOLOID)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("NULLIF requires = operator to yield boolean"), parser_errposition(pstate, a->location)));
  }
  if (result->opretset)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                 
                       errmsg("%s must not return a set", "NULLIF"), parser_errposition(pstate, a->location)));
  }

     
                                                                 
     
  result->opresulttype = exprType((Node *)linitial(result->args));

     
                                                            
     
  NodeSetTag(result, T_NullIfExpr);

  return (Node *)result;
}

   
                                                                         
                               
   
static Node *
transformAExprOf(ParseState *pstate, A_Expr *a)
{
  Node *lexpr = a->lexpr;
  Const *result;
  ListCell *telem;
  Oid ltype, rtype;
  bool matched = false;

  if (operator_precedence_warning)
  {
    emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_IS, "IS", lexpr, NULL, a->location);
  }

  lexpr = transformExprRecurse(pstate, lexpr);

  ltype = exprType(lexpr);
  foreach (telem, (List *)a->rexpr)
  {
    rtype = typenameTypeId(pstate, lfirst(telem));
    matched = (rtype == ltype);
    if (matched)
    {
      break;
    }
  }

     
                                                                           
                     
     
  if (strcmp(strVal(linitial(a->name)), "<>") == 0)
  {
    matched = (!matched);
  }

  result = (Const *)makeBoolConst(matched, false);

                                                                
  result->location = exprLocation((Node *)a);

  return (Node *)result;
}

static Node *
transformAExprIn(ParseState *pstate, A_Expr *a)
{
  Node *result = NULL;
  Node *lexpr;
  List *rexprs;
  List *rvars;
  List *rnonvars;
  bool useOr;
  ListCell *l;

     
                                                     
     
  if (strcmp(strVal(linitial(a->name)), "<>") == 0)
  {
    useOr = false;
  }
  else
  {
    useOr = true;
  }

  if (operator_precedence_warning)
  {
    emit_precedence_warnings(pstate, useOr ? PREC_GROUP_IN : PREC_GROUP_NOT_IN, "IN", a->lexpr, NULL, a->location);
  }

     
                                                                             
                                                                            
                                                                           
                                                                           
                                                                            
                                       
     
                                                                          
           
     
  lexpr = transformExprRecurse(pstate, a->lexpr);
  rexprs = rvars = rnonvars = NIL;
  foreach (l, (List *)a->rexpr)
  {
    Node *rexpr = transformExprRecurse(pstate, lfirst(l));

    rexprs = lappend(rexprs, rexpr);
    if (contain_vars_of_level(rexpr, 0))
    {
      rvars = lappend(rvars, rexpr);
    }
    else
    {
      rnonvars = lappend(rnonvars, rexpr);
    }
  }

     
                                                                           
                             
     
  if (list_length(rnonvars) > 1)
  {
    List *allexprs;
    Oid scalar_type;
    Oid array_type;

       
                                                                      
                                                                           
                                                                         
       
                                                                         
       
    allexprs = list_concat(list_make1(lexpr), rnonvars);
    scalar_type = select_common_type(pstate, allexprs, NULL, NULL);

       
                                                                         
                                                                        
                                                                           
                                                   
       
    if (OidIsValid(scalar_type) && scalar_type != RECORDOID)
    {
      array_type = get_array_type(scalar_type);
    }
    else
    {
      array_type = InvalidOid;
    }
    if (array_type != InvalidOid)
    {
         
                                                                         
                                          
         
      List *aexprs;
      ArrayExpr *newa;

      aexprs = NIL;
      foreach (l, rnonvars)
      {
        Node *rexpr = (Node *)lfirst(l);

        rexpr = coerce_to_common_type(pstate, rexpr, scalar_type, "IN");
        aexprs = lappend(aexprs, rexpr);
      }
      newa = makeNode(ArrayExpr);
      newa->array_typeid = array_type;
                                                       
      newa->element_typeid = scalar_type;
      newa->elements = aexprs;
      newa->multidims = false;
      newa->location = -1;

      result = (Node *)make_scalar_array_op(pstate, a->name, useOr, lexpr, (Node *)newa, a->location);

                                                             
      rexprs = rvars;
    }
  }

     
                                                                  
     
  foreach (l, rexprs)
  {
    Node *rexpr = (Node *)lfirst(l);
    Node *cmp;

    if (IsA(lexpr, RowExpr) && IsA(rexpr, RowExpr))
    {
                                               
      cmp = make_row_comparison_op(pstate, a->name, copyObject(((RowExpr *)lexpr)->args), ((RowExpr *)rexpr)->args, a->location);
    }
    else
    {
                                    
      cmp = (Node *)make_op(pstate, a->name, copyObject(lexpr), rexpr, pstate->p_last_srf, a->location);
    }

    cmp = coerce_to_boolean(pstate, cmp, "IN");
    if (result == NULL)
    {
      result = cmp;
    }
    else
    {
      result = (Node *)makeBoolExpr(useOr ? OR_EXPR : AND_EXPR, list_make2(result, cmp), a->location);
    }
  }

  return result;
}

static Node *
transformAExprBetween(ParseState *pstate, A_Expr *a)
{
  Node *aexpr;
  Node *bexpr;
  Node *cexpr;
  Node *result;
  Node *sub1;
  Node *sub2;
  List *args;

                                              
  aexpr = a->lexpr;
  args = castNode(List, a->rexpr);
  Assert(list_length(args) == 2);
  bexpr = (Node *)linitial(args);
  cexpr = (Node *)lsecond(args);

  if (operator_precedence_warning)
  {
    int opgroup;
    const char *opname;

    opgroup = operator_precedence_group((Node *)a, &opname);
    emit_precedence_warnings(pstate, opgroup, opname, aexpr, cexpr, a->location);
                                                              
                                                       
    aexpr = (Node *)makeA_Expr(AEXPR_PAREN, NIL, aexpr, NULL, -1);
    bexpr = (Node *)makeA_Expr(AEXPR_PAREN, NIL, bexpr, NULL, -1);
    cexpr = (Node *)makeA_Expr(AEXPR_PAREN, NIL, cexpr, NULL, -1);
  }

     
                                                                 
                                                                         
                                                                           
                               
     
                                                                        
                                                                      
                
                                                                       
     
  switch (a->kind)
  {
  case AEXPR_BETWEEN:
    args = list_make2(makeSimpleA_Expr(AEXPR_OP, ">=", aexpr, bexpr, a->location), makeSimpleA_Expr(AEXPR_OP, "<=", copyObject(aexpr), cexpr, a->location));
    result = (Node *)makeBoolExpr(AND_EXPR, args, a->location);
    break;
  case AEXPR_NOT_BETWEEN:
    args = list_make2(makeSimpleA_Expr(AEXPR_OP, "<", aexpr, bexpr, a->location), makeSimpleA_Expr(AEXPR_OP, ">", copyObject(aexpr), cexpr, a->location));
    result = (Node *)makeBoolExpr(OR_EXPR, args, a->location);
    break;
  case AEXPR_BETWEEN_SYM:
    args = list_make2(makeSimpleA_Expr(AEXPR_OP, ">=", aexpr, bexpr, a->location), makeSimpleA_Expr(AEXPR_OP, "<=", copyObject(aexpr), cexpr, a->location));
    sub1 = (Node *)makeBoolExpr(AND_EXPR, args, a->location);
    args = list_make2(makeSimpleA_Expr(AEXPR_OP, ">=", copyObject(aexpr), copyObject(cexpr), a->location), makeSimpleA_Expr(AEXPR_OP, "<=", copyObject(aexpr), copyObject(bexpr), a->location));
    sub2 = (Node *)makeBoolExpr(AND_EXPR, args, a->location);
    args = list_make2(sub1, sub2);
    result = (Node *)makeBoolExpr(OR_EXPR, args, a->location);
    break;
  case AEXPR_NOT_BETWEEN_SYM:
    args = list_make2(makeSimpleA_Expr(AEXPR_OP, "<", aexpr, bexpr, a->location), makeSimpleA_Expr(AEXPR_OP, ">", copyObject(aexpr), cexpr, a->location));
    sub1 = (Node *)makeBoolExpr(OR_EXPR, args, a->location);
    args = list_make2(makeSimpleA_Expr(AEXPR_OP, "<", copyObject(aexpr), copyObject(cexpr), a->location), makeSimpleA_Expr(AEXPR_OP, ">", copyObject(aexpr), copyObject(bexpr), a->location));
    sub2 = (Node *)makeBoolExpr(OR_EXPR, args, a->location);
    args = list_make2(sub1, sub2);
    result = (Node *)makeBoolExpr(AND_EXPR, args, a->location);
    break;
  default:
    elog(ERROR, "unrecognized A_Expr kind: %d", a->kind);
    result = NULL;                          
    break;
  }

  return transformExprRecurse(pstate, result);
}

static Node *
transformBoolExpr(ParseState *pstate, BoolExpr *a)
{
  List *args = NIL;
  const char *opname;
  ListCell *lc;

  switch (a->boolop)
  {
  case AND_EXPR:
    opname = "AND";
    break;
  case OR_EXPR:
    opname = "OR";
    break;
  case NOT_EXPR:
    opname = "NOT";
    break;
  default:
    elog(ERROR, "unrecognized boolop: %d", (int)a->boolop);
    opname = NULL;                          
    break;
  }

  foreach (lc, a->args)
  {
    Node *arg = (Node *)lfirst(lc);

    arg = transformExprRecurse(pstate, arg);
    arg = coerce_to_boolean(pstate, arg, opname);
    args = lappend(args, arg);
  }

  return (Node *)makeBoolExpr(a->boolop, args, a->location);
}

static Node *
transformFuncCall(ParseState *pstate, FuncCall *fn)
{
  Node *last_srf = pstate->p_last_srf;
  List *targs;
  ListCell *args;

                                           
  targs = NIL;
  foreach (args, fn->args)
  {
    targs = lappend(targs, transformExprRecurse(pstate, (Node *)lfirst(args)));
  }

     
                                                                     
                                                                           
                                                                             
                                                                           
                                                                           
                                 
     
  if (fn->agg_within_group)
  {
    Assert(fn->agg_order != NIL);
    foreach (args, fn->agg_order)
    {
      SortBy *arg = (SortBy *)lfirst(args);

      targs = lappend(targs, transformExpr(pstate, arg->node, EXPR_KIND_ORDER_BY));
    }
  }

                                             
  return ParseFuncOrColumn(pstate, fn->funcname, targs, last_srf, fn, false, fn->location);
}

static Node *
transformMultiAssignRef(ParseState *pstate, MultiAssignRef *maref)
{
  SubLink *sublink;
  RowExpr *rexpr;
  Query *qtree;
  TargetEntry *tle;

                                                                          
  Assert(pstate->p_expr_kind == EXPR_KIND_UPDATE_SOURCE);

                                                                        
  if (maref->colno == 1)
  {
       
                                                                          
                                                                           
                                                                         
                                                                   
       
    if (IsA(maref->source, SubLink) && ((SubLink *)maref->source)->subLinkType == EXPR_SUBLINK)
    {
                                             
      sublink = (SubLink *)maref->source;
      sublink->subLinkType = MULTIEXPR_SUBLINK;
                            
      sublink = (SubLink *)transformExprRecurse(pstate, (Node *)sublink);

      qtree = castNode(Query, sublink->subselect);

                                                             
      if (count_nonjunk_tlist_entries(qtree->targetList) != maref->ncolumns)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("number of columns does not match number of values"), parser_errposition(pstate, sublink->location)));
      }

         
                                                                      
                                                                         
                                                                     
                                                                     
               
         
      tle = makeTargetEntry((Expr *)sublink, 0, NULL, true);
      pstate->p_multiassign_exprs = lappend(pstate->p_multiassign_exprs, tle);

         
                                                                    
                                                       
                                   
         
      sublink->subLinkId = list_length(pstate->p_multiassign_exprs);
    }
    else if (IsA(maref->source, RowExpr))
    {
                                                              
      rexpr = (RowExpr *)transformRowExpr(pstate, (RowExpr *)maref->source, true);

                                                       
      if (list_length(rexpr->args) != maref->ncolumns)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("number of columns does not match number of values"), parser_errposition(pstate, rexpr->location)));
      }

         
                                                                        
                                                             
         
      tle = makeTargetEntry((Expr *)rexpr, 0, NULL, true);
      pstate->p_multiassign_exprs = lappend(pstate->p_multiassign_exprs, tle);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("source for a multiple-column UPDATE item must be a sub-SELECT or ROW() expression"), parser_errposition(pstate, exprLocation(maref->source))));
    }
  }
  else
  {
       
                                                                  
                                                                         
                                     
       
    Assert(pstate->p_multiassign_exprs != NIL);
    tle = (TargetEntry *)llast(pstate->p_multiassign_exprs);
  }

     
                                                                   
     
  if (IsA(tle->expr, SubLink))
  {
    Param *param;

    sublink = (SubLink *)tle->expr;
    Assert(sublink->subLinkType == MULTIEXPR_SUBLINK);
    qtree = castNode(Query, sublink->subselect);

                                                                       
    tle = (TargetEntry *)list_nth(qtree->targetList, maref->colno - 1);
    Assert(!tle->resjunk);

    param = makeNode(Param);
    param->paramkind = PARAM_MULTIEXPR;
    param->paramid = (sublink->subLinkId << 16) | maref->colno;
    param->paramtype = exprType((Node *)tle->expr);
    param->paramtypmod = exprTypmod((Node *)tle->expr);
    param->paramcollid = exprCollation((Node *)tle->expr);
    param->location = exprLocation((Node *)tle->expr);

    return (Node *)param;
  }

  if (IsA(tle->expr, RowExpr))
  {
    Node *result;

    rexpr = (RowExpr *)tle->expr;

                                                                 
    result = (Node *)list_nth(rexpr->args, maref->colno - 1);

       
                                                            
                                                                           
                                  
       
    if (maref->colno == maref->ncolumns)
    {
      pstate->p_multiassign_exprs = list_delete_ptr(pstate->p_multiassign_exprs, tle);
    }

    return result;
  }

  elog(ERROR, "unexpected expr type in multiassign list");
  return NULL;                          
}

static Node *
transformCaseExpr(ParseState *pstate, CaseExpr *c)
{
  CaseExpr *newc = makeNode(CaseExpr);
  Node *last_srf = pstate->p_last_srf;
  Node *arg;
  CaseTestExpr *placeholder;
  List *newargs;
  List *resultexprs;
  ListCell *l;
  Node *defresult;
  Oid ptype;

                                             
  arg = transformExprRecurse(pstate, (Node *)c->arg);

                                                
  if (arg)
  {
       
                                                                           
                                                                           
                                                                         
                                                                           
                      
       
    if (exprType(arg) == UNKNOWNOID)
    {
      arg = coerce_to_common_type(pstate, arg, TEXTOID, "CASE");
    }

       
                                                                       
                                                                           
                                                                         
                                                                      
       
    assign_expr_collations(pstate, arg);

    placeholder = makeNode(CaseTestExpr);
    placeholder->typeId = exprType(arg);
    placeholder->typeMod = exprTypmod(arg);
    placeholder->collation = exprCollation(arg);
  }
  else
  {
    placeholder = NULL;
  }

  newc->arg = (Expr *)arg;

                                       
  newargs = NIL;
  resultexprs = NIL;
  foreach (l, c->args)
  {
    CaseWhen *w = lfirst_node(CaseWhen, l);
    CaseWhen *neww = makeNode(CaseWhen);
    Node *warg;

    warg = (Node *)w->expr;
    if (placeholder)
    {
                                                      
      warg = (Node *)makeSimpleA_Expr(AEXPR_OP, "=", (Node *)placeholder, warg, w->location);
    }
    neww->expr = (Expr *)transformExprRecurse(pstate, warg);

    neww->expr = (Expr *)coerce_to_boolean(pstate, (Node *)neww->expr, "CASE/WHEN");

    warg = (Node *)w->result;
    neww->result = (Expr *)transformExprRecurse(pstate, warg);
    neww->location = w->location;

    newargs = lappend(newargs, neww);
    resultexprs = lappend(resultexprs, neww->result);
  }

  newc->args = newargs;

                                    
  defresult = (Node *)c->defresult;
  if (defresult == NULL)
  {
    A_Const *n = makeNode(A_Const);

    n->val.type = T_Null;
    n->location = -1;
    defresult = (Node *)n;
  }
  newc->defresult = (Expr *)transformExprRecurse(pstate, defresult);

     
                                                                     
                                                                            
                                        
     
  resultexprs = lcons(newc->defresult, resultexprs);

  ptype = select_common_type(pstate, resultexprs, "CASE", NULL);
  Assert(OidIsValid(ptype));
  newc->casetype = ptype;
                                                 

                                                   
  newc->defresult = (Expr *)coerce_to_common_type(pstate, (Node *)newc->defresult, ptype, "CASE/ELSE");

                                                 
  foreach (l, newc->args)
  {
    CaseWhen *w = (CaseWhen *)lfirst(l);

    w->result = (Expr *)coerce_to_common_type(pstate, (Node *)w->result, ptype, "CASE/WHEN");
  }

                                                      
  if (pstate->p_last_srf != last_srf)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                                   
                       errmsg("set-returning functions are not allowed in %s", "CASE"), errhint("You might be able to move the set-returning function into a LATERAL FROM item."), parser_errposition(pstate, exprLocation(pstate->p_last_srf))));
  }

  newc->location = c->location;

  return (Node *)newc;
}

static Node *
transformSubLink(ParseState *pstate, SubLink *sublink)
{
  Node *result = (Node *)sublink;
  Query *qtree;
  const char *err;

     
                                                                             
                                                                             
                                
     
  err = NULL;
  switch (pstate->p_expr_kind)
  {
  case EXPR_KIND_NONE:
    Assert(false);                   
    break;
  case EXPR_KIND_OTHER:
                                                                
    break;
  case EXPR_KIND_JOIN_ON:
  case EXPR_KIND_JOIN_USING:
  case EXPR_KIND_FROM_SUBSELECT:
  case EXPR_KIND_FROM_FUNCTION:
  case EXPR_KIND_WHERE:
  case EXPR_KIND_POLICY:
  case EXPR_KIND_HAVING:
  case EXPR_KIND_FILTER:
  case EXPR_KIND_WINDOW_PARTITION:
  case EXPR_KIND_WINDOW_ORDER:
  case EXPR_KIND_WINDOW_FRAME_RANGE:
  case EXPR_KIND_WINDOW_FRAME_ROWS:
  case EXPR_KIND_WINDOW_FRAME_GROUPS:
  case EXPR_KIND_SELECT_TARGET:
  case EXPR_KIND_INSERT_TARGET:
  case EXPR_KIND_UPDATE_SOURCE:
  case EXPR_KIND_UPDATE_TARGET:
  case EXPR_KIND_GROUP_BY:
  case EXPR_KIND_ORDER_BY:
  case EXPR_KIND_DISTINCT_ON:
  case EXPR_KIND_LIMIT:
  case EXPR_KIND_OFFSET:
  case EXPR_KIND_RETURNING:
  case EXPR_KIND_VALUES:
  case EXPR_KIND_VALUES_SINGLE:
              
    break;
  case EXPR_KIND_CHECK_CONSTRAINT:
  case EXPR_KIND_DOMAIN_CHECK:
    err = _("cannot use subquery in check constraint");
    break;
  case EXPR_KIND_COLUMN_DEFAULT:
  case EXPR_KIND_FUNCTION_DEFAULT:
    err = _("cannot use subquery in DEFAULT expression");
    break;
  case EXPR_KIND_INDEX_EXPRESSION:
    err = _("cannot use subquery in index expression");
    break;
  case EXPR_KIND_INDEX_PREDICATE:
    err = _("cannot use subquery in index predicate");
    break;
  case EXPR_KIND_ALTER_COL_TRANSFORM:
    err = _("cannot use subquery in transform expression");
    break;
  case EXPR_KIND_EXECUTE_PARAMETER:
    err = _("cannot use subquery in EXECUTE parameter");
    break;
  case EXPR_KIND_TRIGGER_WHEN:
    err = _("cannot use subquery in trigger WHEN condition");
    break;
  case EXPR_KIND_PARTITION_BOUND:
    err = _("cannot use subquery in partition bound");
    break;
  case EXPR_KIND_PARTITION_EXPRESSION:
    err = _("cannot use subquery in partition key expression");
    break;
  case EXPR_KIND_CALL_ARGUMENT:
    err = _("cannot use subquery in CALL argument");
    break;
  case EXPR_KIND_COPY_WHERE:
    err = _("cannot use subquery in COPY FROM WHERE condition");
    break;
  case EXPR_KIND_GENERATED_COLUMN:
    err = _("cannot use subquery in column generation expression");
    break;

       
                                                                 
                                                                
                                                                     
                                                                      
                             
       
  }
  if (err)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg_internal("%s", err), parser_errposition(pstate, sublink->location)));
  }

  pstate->p_hasSubLinks = true;

     
                                         
     
  qtree = parse_sub_analyze(sublink->subselect, pstate, NULL, false, true);

     
                                                                           
                                                    
     
  if (!IsA(qtree, Query) || qtree->commandType != CMD_SELECT)
  {
    elog(ERROR, "unexpected non-SELECT command in SubLink");
  }

  sublink->subselect = (Node *)qtree;

  if (sublink->subLinkType == EXISTS_SUBLINK)
  {
       
                                                                           
                                              
       
    sublink->testexpr = NULL;
    sublink->operName = NIL;
  }
  else if (sublink->subLinkType == EXPR_SUBLINK || sublink->subLinkType == ARRAY_SUBLINK)
  {
       
                                                                          
                 
       
    if (count_nonjunk_tlist_entries(qtree->targetList) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("subquery must return only one column"), parser_errposition(pstate, sublink->location)));
    }

       
                                                                           
                                                     
       
    sublink->testexpr = NULL;
    sublink->operName = NIL;
  }
  else if (sublink->subLinkType == MULTIEXPR_SUBLINK)
  {
                                                                       
    sublink->testexpr = NULL;
    sublink->operName = NIL;
  }
  else
  {
                                                                    
    Node *lefthand;
    List *left_list;
    List *right_list;
    ListCell *l;

    if (operator_precedence_warning)
    {
      if (sublink->operName == NIL)
      {
        emit_precedence_warnings(pstate, PREC_GROUP_IN, "IN", sublink->testexpr, NULL, sublink->location);
      }
      else
      {
        emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_OP, strVal(llast(sublink->operName)), sublink->testexpr, NULL, sublink->location);
      }
    }

       
                                                                         
       
    if (sublink->operName == NIL)
    {
      sublink->operName = list_make1(makeString("="));
    }

       
                                                            
       
    lefthand = transformExprRecurse(pstate, sublink->testexpr);
    if (lefthand && IsA(lefthand, RowExpr))
    {
      left_list = ((RowExpr *)lefthand)->args;
    }
    else
    {
      left_list = list_make1(lefthand);
    }

       
                                                                           
                        
       
    right_list = NIL;
    foreach (l, qtree->targetList)
    {
      TargetEntry *tent = (TargetEntry *)lfirst(l);
      Param *param;

      if (tent->resjunk)
      {
        continue;
      }

      param = makeNode(Param);
      param->paramkind = PARAM_SUBLINK;
      param->paramid = tent->resno;
      param->paramtype = exprType((Node *)tent->expr);
      param->paramtypmod = exprTypmod((Node *)tent->expr);
      param->paramcollid = exprCollation((Node *)tent->expr);
      param->location = -1;

      right_list = lappend(right_list, param);
    }

       
                                                                       
                                                                       
                
       
    if (list_length(left_list) < list_length(right_list))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("subquery has too many columns"), parser_errposition(pstate, sublink->location)));
    }
    if (list_length(left_list) > list_length(right_list))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("subquery has too few columns"), parser_errposition(pstate, sublink->location)));
    }

       
                                                                  
                                  
       
    sublink->testexpr = make_row_comparison_op(pstate, sublink->operName, left_list, right_list, sublink->location);
  }

  return result;
}

   
                      
   
                                                                     
                                                                     
                                                
   
static Node *
transformArrayExpr(ParseState *pstate, A_ArrayExpr *a, Oid array_type, Oid element_type, int32 typmod)
{
  ArrayExpr *newa = makeNode(ArrayExpr);
  List *newelems = NIL;
  List *newcoercedelems = NIL;
  ListCell *element;
  Oid coerce_type;
  bool coerce_hard;

     
                                       
     
                                                                           
                         
     
  newa->multidims = false;
  foreach (element, a->elements)
  {
    Node *e = (Node *)lfirst(element);
    Node *newe;

                                                                        
    while (e && IsA(e, A_Expr) && ((A_Expr *)e)->kind == AEXPR_PAREN)
    {
      e = ((A_Expr *)e)->lexpr;
    }

       
                                                                           
                                                    
       
    if (IsA(e, A_ArrayExpr))
    {
      newe = transformArrayExpr(pstate, (A_ArrayExpr *)e, array_type, element_type, typmod);
                                           
      Assert(array_type == InvalidOid || array_type == exprType(newe));
      newa->multidims = true;
    }
    else
    {
      newe = transformExprRecurse(pstate, e);

         
                                                                      
              
         
      if (!newa->multidims && type_is_array(exprType(newe)))
      {
        newa->multidims = true;
      }
    }

    newelems = lappend(newelems, newe);
  }

     
                                            
     
                                                                           
                                                                        
     
  if (OidIsValid(array_type))
  {
                                                            
    Assert(OidIsValid(element_type));
    coerce_type = (newa->multidims ? array_type : element_type);
    coerce_hard = true;
  }
  else
  {
                                                           
    if (newelems == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_DATATYPE), errmsg("cannot determine type of empty array"),
                         errhint("Explicitly cast to the desired type, "
                                 "for example ARRAY[]::integer[]."),
                         parser_errposition(pstate, a->location)));
    }

                                               
    coerce_type = select_common_type(pstate, newelems, "ARRAY", NULL);

    if (newa->multidims)
    {
      array_type = coerce_type;
      element_type = get_element_type(array_type);
      if (!OidIsValid(element_type))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find element type for data type %s", format_type_be(array_type)), parser_errposition(pstate, a->location)));
      }
    }
    else
    {
      element_type = coerce_type;
      array_type = get_array_type(element_type);
      if (!OidIsValid(array_type))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(element_type)), parser_errposition(pstate, a->location)));
      }
    }
    coerce_hard = false;
  }

     
                                    
     
                                                                          
                         
     
                                                                        
                                                                            
                                                                 
     
  foreach (element, newelems)
  {
    Node *e = (Node *)lfirst(element);
    Node *newe;

    if (coerce_hard)
    {
      newe = coerce_to_target_type(pstate, e, exprType(e), coerce_type, typmod, COERCION_EXPLICIT, COERCE_EXPLICIT_CAST, -1);
      if (newe == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(exprType(e)), format_type_be(coerce_type)), parser_errposition(pstate, exprLocation(e))));
      }
    }
    else
    {
      newe = coerce_to_common_type(pstate, e, coerce_type, "ARRAY");
    }
    newcoercedelems = lappend(newcoercedelems, newe);
  }

  newa->array_typeid = array_type;
                                                   
  newa->element_typeid = element_type;
  newa->elements = newcoercedelems;
  newa->location = a->location;

  return (Node *)newa;
}

static Node *
transformRowExpr(ParseState *pstate, RowExpr *r, bool allowDefault)
{
  RowExpr *newr;
  char fname[16];
  int fnum;
  ListCell *lc;

  newr = makeNode(RowExpr);

                                       
  newr->args = transformExpressionList(pstate, r->args, pstate->p_expr_kind, allowDefault);

                                                      
  if (list_length(newr->args) > MaxTupleAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("ROW expressions can have at most %d entries", MaxTupleAttributeNumber), parser_errposition(pstate, r->location)));
  }

                                                          
  newr->row_typeid = RECORDOID;
  newr->row_format = COERCE_IMPLICIT_CAST;

                                                               
  newr->colnames = NIL;
  fnum = 1;
  foreach (lc, newr->args)
  {
    snprintf(fname, sizeof(fname), "f%d", fnum++);
    newr->colnames = lappend(newr->colnames, makeString(pstrdup(fname)));
  }

  newr->location = r->location;

  return (Node *)newr;
}

static Node *
transformCoalesceExpr(ParseState *pstate, CoalesceExpr *c)
{
  CoalesceExpr *newc = makeNode(CoalesceExpr);
  Node *last_srf = pstate->p_last_srf;
  List *newargs = NIL;
  List *newcoercedargs = NIL;
  ListCell *args;

  foreach (args, c->args)
  {
    Node *e = (Node *)lfirst(args);
    Node *newe;

    newe = transformExprRecurse(pstate, e);
    newargs = lappend(newargs, newe);
  }

  newc->coalescetype = select_common_type(pstate, newargs, "COALESCE", NULL);
                                                     

                                      
  foreach (args, newargs)
  {
    Node *e = (Node *)lfirst(args);
    Node *newe;

    newe = coerce_to_common_type(pstate, e, newc->coalescetype, "COALESCE");
    newcoercedargs = lappend(newcoercedargs, newe);
  }

                                                      
  if (pstate->p_last_srf != last_srf)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                                   
                       errmsg("set-returning functions are not allowed in %s", "COALESCE"), errhint("You might be able to move the set-returning function into a LATERAL FROM item."), parser_errposition(pstate, exprLocation(pstate->p_last_srf))));
  }

  newc->args = newcoercedargs;
  newc->location = c->location;
  return (Node *)newc;
}

static Node *
transformMinMaxExpr(ParseState *pstate, MinMaxExpr *m)
{
  MinMaxExpr *newm = makeNode(MinMaxExpr);
  List *newargs = NIL;
  List *newcoercedargs = NIL;
  const char *funcname = (m->op == IS_GREATEST) ? "GREATEST" : "LEAST";
  ListCell *args;

  newm->op = m->op;
  foreach (args, m->args)
  {
    Node *e = (Node *)lfirst(args);
    Node *newe;

    newe = transformExprRecurse(pstate, e);
    newargs = lappend(newargs, newe);
  }

  newm->minmaxtype = select_common_type(pstate, newargs, funcname, NULL);
                                                                   

                                      
  foreach (args, newargs)
  {
    Node *e = (Node *)lfirst(args);
    Node *newe;

    newe = coerce_to_common_type(pstate, e, newm->minmaxtype, funcname);
    newcoercedargs = lappend(newcoercedargs, newe);
  }

  newm->args = newcoercedargs;
  newm->location = m->location;
  return (Node *)newm;
}

static Node *
transformSQLValueFunction(ParseState *pstate, SQLValueFunction *svf)
{
     
                                                                            
                                                               
     
  switch (svf->op)
  {
  case SVFOP_CURRENT_DATE:
    svf->type = DATEOID;
    break;
  case SVFOP_CURRENT_TIME:
    svf->type = TIMETZOID;
    break;
  case SVFOP_CURRENT_TIME_N:
    svf->type = TIMETZOID;
    svf->typmod = anytime_typmod_check(true, svf->typmod);
    break;
  case SVFOP_CURRENT_TIMESTAMP:
    svf->type = TIMESTAMPTZOID;
    break;
  case SVFOP_CURRENT_TIMESTAMP_N:
    svf->type = TIMESTAMPTZOID;
    svf->typmod = anytimestamp_typmod_check(true, svf->typmod);
    break;
  case SVFOP_LOCALTIME:
    svf->type = TIMEOID;
    break;
  case SVFOP_LOCALTIME_N:
    svf->type = TIMEOID;
    svf->typmod = anytime_typmod_check(false, svf->typmod);
    break;
  case SVFOP_LOCALTIMESTAMP:
    svf->type = TIMESTAMPOID;
    break;
  case SVFOP_LOCALTIMESTAMP_N:
    svf->type = TIMESTAMPOID;
    svf->typmod = anytimestamp_typmod_check(false, svf->typmod);
    break;
  case SVFOP_CURRENT_ROLE:
  case SVFOP_CURRENT_USER:
  case SVFOP_USER:
  case SVFOP_SESSION_USER:
  case SVFOP_CURRENT_CATALOG:
  case SVFOP_CURRENT_SCHEMA:
    svf->type = NAMEOID;
    break;
  }

  return (Node *)svf;
}

static Node *
transformXmlExpr(ParseState *pstate, XmlExpr *x)
{
  XmlExpr *newx;
  ListCell *lc;
  int i;

  if (operator_precedence_warning && x->op == IS_DOCUMENT)
  {
    emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_IS, "IS", (Node *)linitial(x->args), NULL, x->location);
  }

  newx = makeNode(XmlExpr);
  newx->op = x->op;
  if (x->name)
  {
    newx->name = map_sql_identifier_to_xml_name(x->name, false, false);
  }
  else
  {
    newx->name = NULL;
  }
  newx->xmloption = x->xmloption;
  newx->type = XMLOID;                                              
  newx->typmod = -1;
  newx->location = x->location;

     
                                                                          
                                                 
     
  newx->named_args = NIL;
  newx->arg_names = NIL;

  foreach (lc, x->named_args)
  {
    ResTarget *r = lfirst_node(ResTarget, lc);
    Node *expr;
    char *argname;

    expr = transformExprRecurse(pstate, r->val);

    if (r->name)
    {
      argname = map_sql_identifier_to_xml_name(r->name, false, false);
    }
    else if (IsA(r->val, ColumnRef))
    {
      argname = map_sql_identifier_to_xml_name(FigureColname(r->val), true, false);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), x->op == IS_XMLELEMENT ? errmsg("unnamed XML attribute value must be a column reference") : errmsg("unnamed XML element value must be a column reference"), parser_errposition(pstate, r->location)));
      argname = NULL;                          
    }

                                                      
    if (x->op == IS_XMLELEMENT)
    {
      ListCell *lc2;

      foreach (lc2, newx->arg_names)
      {
        if (strcmp(argname, strVal(lfirst(lc2))) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("XML attribute name \"%s\" appears more than once", argname), parser_errposition(pstate, r->location)));
        }
      }
    }

    newx->named_args = lappend(newx->named_args, expr);
    newx->arg_names = lappend(newx->arg_names, makeString(argname));
  }

                                                                          
  newx->args = NIL;
  i = 0;
  foreach (lc, x->args)
  {
    Node *e = (Node *)lfirst(lc);
    Node *newe;

    newe = transformExprRecurse(pstate, e);
    switch (x->op)
    {
    case IS_XMLCONCAT:
      newe = coerce_to_specific_type(pstate, newe, XMLOID, "XMLCONCAT");
      break;
    case IS_XMLELEMENT:
                                 
      break;
    case IS_XMLFOREST:
      newe = coerce_to_specific_type(pstate, newe, XMLOID, "XMLFOREST");
      break;
    case IS_XMLPARSE:
      if (i == 0)
      {
        newe = coerce_to_specific_type(pstate, newe, TEXTOID, "XMLPARSE");
      }
      else
      {
        newe = coerce_to_boolean(pstate, newe, "XMLPARSE");
      }
      break;
    case IS_XMLPI:
      newe = coerce_to_specific_type(pstate, newe, TEXTOID, "XMLPI");
      break;
    case IS_XMLROOT:
      if (i == 0)
      {
        newe = coerce_to_specific_type(pstate, newe, XMLOID, "XMLROOT");
      }
      else if (i == 1)
      {
        newe = coerce_to_specific_type(pstate, newe, TEXTOID, "XMLROOT");
      }
      else
      {
        newe = coerce_to_specific_type(pstate, newe, INT4OID, "XMLROOT");
      }
      break;
    case IS_XMLSERIALIZE:
                            
      Assert(false);
      break;
    case IS_DOCUMENT:
      newe = coerce_to_specific_type(pstate, newe, XMLOID, "IS DOCUMENT");
      break;
    }
    newx->args = lappend(newx->args, newe);
    i++;
  }

  return (Node *)newx;
}

static Node *
transformXmlSerialize(ParseState *pstate, XmlSerialize *xs)
{
  Node *result;
  XmlExpr *xexpr;
  Oid targetType;
  int32 targetTypmod;

  xexpr = makeNode(XmlExpr);
  xexpr->op = IS_XMLSERIALIZE;
  xexpr->args = list_make1(coerce_to_specific_type(pstate, transformExprRecurse(pstate, xs->expr), XMLOID, "XMLSERIALIZE"));

  typenameTypeIdAndMod(pstate, xs->typeName, &targetType, &targetTypmod);

  xexpr->xmloption = xs->xmloption;
  xexpr->location = xs->location;
                                                                            
  xexpr->type = targetType;
  xexpr->typmod = targetTypmod;

     
                                                                         
                                                                             
                                                                           
             
     
  result = coerce_to_target_type(pstate, (Node *)xexpr, TEXTOID, targetType, targetTypmod, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
  if (result == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast XMLSERIALIZE result to %s", format_type_be(targetType)), parser_errposition(pstate, xexpr->location)));
  }
  return result;
}

static Node *
transformBooleanTest(ParseState *pstate, BooleanTest *b)
{
  const char *clausename;

  if (operator_precedence_warning)
  {
    emit_precedence_warnings(pstate, PREC_GROUP_POSTFIX_IS, "IS", (Node *)b->arg, NULL, b->location);
  }

  switch (b->booltesttype)
  {
  case IS_TRUE:
    clausename = "IS TRUE";
    break;
  case IS_NOT_TRUE:
    clausename = "IS NOT TRUE";
    break;
  case IS_FALSE:
    clausename = "IS FALSE";
    break;
  case IS_NOT_FALSE:
    clausename = "IS NOT FALSE";
    break;
  case IS_UNKNOWN:
    clausename = "IS UNKNOWN";
    break;
  case IS_NOT_UNKNOWN:
    clausename = "IS NOT UNKNOWN";
    break;
  default:
    elog(ERROR, "unrecognized booltesttype: %d", (int)b->booltesttype);
    clausename = NULL;                          
  }

  b->arg = (Expr *)transformExprRecurse(pstate, (Node *)b->arg);

  b->arg = (Expr *)coerce_to_boolean(pstate, (Node *)b->arg, clausename);

  return (Node *)b;
}

static Node *
transformCurrentOfExpr(ParseState *pstate, CurrentOfExpr *cexpr)
{
  int sublevels_up;

                                                                
  Assert(pstate->p_target_rangetblentry != NULL);
  cexpr->cvarno = RTERangeTablePosn(pstate, pstate->p_target_rangetblentry, &sublevels_up);
  Assert(sublevels_up == 0);

     
                                                                            
                                                                             
                                                
     
  if (cexpr->cursor_name != NULL)                                  
  {
    ColumnRef *cref = makeNode(ColumnRef);
    Node *node = NULL;

                                                            
    cref->fields = list_make1(makeString(cexpr->cursor_name));
    cref->location = -1;

                                                                    
    if (pstate->p_pre_columnref_hook != NULL)
    {
      node = pstate->p_pre_columnref_hook(pstate, cref);
    }
    if (node == NULL && pstate->p_post_columnref_hook != NULL)
    {
      node = pstate->p_post_columnref_hook(pstate, cref, NULL);
    }

       
                                                                         
                                                                        
                
       
    if (node != NULL && IsA(node, Param))
    {
      Param *p = (Param *)node;

      if (p->paramkind == PARAM_EXTERN && p->paramtype == REFCURSOROID)
      {
                                                                 
        cexpr->cursor_name = NULL;
        cexpr->cursor_param = p->paramid;
      }
    }
  }

  return (Node *)cexpr;
}

   
                                                                           
   
static Node *
transformWholeRowRef(ParseState *pstate, RangeTblEntry *rte, int location)
{
  Var *result;
  int vnum;
  int sublevels_up;

                                          
  vnum = RTERangeTablePosn(pstate, rte, &sublevels_up);

     
                                                                        
                                                                        
                                                                          
                                                                  
                                                                           
                                                                            
     
  result = makeWholeRowVar(rte, vnum, sublevels_up, true);

                                                    
  result->location = location;

                                                          
  markVarForSelectPriv(pstate, result, rte);

  return (Node *)result;
}

   
                                      
   
                                                                          
                         
   
static Node *
transformTypeCast(ParseState *pstate, TypeCast *tc)
{
  Node *result;
  Node *arg = tc->arg;
  Node *expr;
  Oid inputType;
  Oid targetType;
  int32 targetTypmod;
  int location;

                                   
  typenameTypeIdAndMod(pstate, tc->typeName, &targetType, &targetTypmod);

     
                                                                           
                                                                        
                                        
     
  while (arg && IsA(arg, A_Expr) && ((A_Expr *)arg)->kind == AEXPR_PAREN)
  {
    arg = ((A_Expr *)arg)->lexpr;
  }

     
                                                                           
                                                                            
                                                                          
                                                                             
                                      
     
  if (IsA(arg, A_ArrayExpr))
  {
    Oid targetBaseType;
    int32 targetBaseTypmod;
    Oid elementType;

       
                                                                       
                                                                      
                                                                       
                        
       
    targetBaseTypmod = targetTypmod;
    targetBaseType = getBaseTypeAndTypmod(targetType, &targetBaseTypmod);
    elementType = get_element_type(targetBaseType);
    if (OidIsValid(elementType))
    {
      expr = transformArrayExpr(pstate, (A_ArrayExpr *)arg, targetBaseType, elementType, targetBaseTypmod);
    }
    else
    {
      expr = transformExprRecurse(pstate, arg);
    }
  }
  else
  {
    expr = transformExprRecurse(pstate, arg);
  }

  inputType = exprType(expr);
  if (inputType == InvalidOid)
  {
    return expr;                               
  }

     
                                                                          
                                                                         
                                                                       
     
  location = tc->location;
  if (location < 0)
  {
    location = tc->typeName->location;
  }

  result = coerce_to_target_type(pstate, expr, inputType, targetType, targetTypmod, COERCION_EXPLICIT, COERCE_EXPLICIT_CAST, location);
  if (result == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(inputType), format_type_be(targetType)), parser_coercion_errposition(pstate, location, expr)));
  }

  return result;
}

   
                                      
   
                                                           
   
static Node *
transformCollateClause(ParseState *pstate, CollateClause *c)
{
  CollateExpr *newc;
  Oid argtype;

  newc = makeNode(CollateExpr);
  newc->arg = (Expr *)transformExprRecurse(pstate, c->arg);

  argtype = exprType((Node *)newc->arg);

     
                                                                            
                                          
     
  if (!type_is_collatable(argtype) && argtype != UNKNOWNOID)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be(argtype)), parser_errposition(pstate, c->location)));
  }

  newc->collOid = LookupCollation(pstate, c->collname, c->location);
  newc->location = c->location;

  return (Node *)newc;
}

   
                                              
   
                                                            
                                                                       
                         
   
                                                                           
                                                                          
                                                                        
                                                                       
   
static Node *
make_row_comparison_op(ParseState *pstate, List *opname, List *largs, List *rargs, int location)
{
  RowCompareExpr *rcexpr;
  RowCompareType rctype;
  List *opexprs;
  List *opnos;
  List *opfamilies;
  ListCell *l, *r;
  List **opinfo_lists;
  Bitmapset *strats;
  int nopers;
  int i;

  nopers = list_length(largs);
  if (nopers != list_length(rargs))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unequal number of entries in row expressions"), parser_errposition(pstate, location)));
  }

     
                                                                            
                                            
     
  if (nopers == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot compare rows of zero length"), parser_errposition(pstate, location)));
  }

     
                                                                            
                                            
     
  opexprs = NIL;
  forboth(l, largs, r, rargs)
  {
    Node *larg = (Node *)lfirst(l);
    Node *rarg = (Node *)lfirst(r);
    OpExpr *cmp;

    cmp = castNode(OpExpr, make_op(pstate, opname, larg, rarg, pstate->p_last_srf, location));

       
                                                                    
                                                                    
                                                                 
       
    if (cmp->opresulttype != BOOLOID)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                         errmsg("row comparison operator must yield type boolean, "
                                "not type %s",
                             format_type_be(cmp->opresulttype)),
                         parser_errposition(pstate, location)));
    }
    if (expression_returns_set((Node *)cmp))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("row comparison operator must not return a set"), parser_errposition(pstate, location)));
    }
    opexprs = lappend(opexprs, cmp);
  }

     
                                                                             
                                                                          
                                          
     
  if (nopers == 1)
  {
    return (Node *)linitial(opexprs);
  }

     
                                                                           
                                                                   
                                                                       
                                       
     
  opinfo_lists = (List **)palloc(nopers * sizeof(List *));
  strats = NULL;
  i = 0;
  foreach (l, opexprs)
  {
    Oid opno = ((OpExpr *)lfirst(l))->opno;
    Bitmapset *this_strats;
    ListCell *j;

    opinfo_lists[i] = get_op_btree_interpretation(opno);

       
                                                                          
                         
       
    this_strats = NULL;
    foreach (j, opinfo_lists[i])
    {
      OpBtreeInterpretation *opinfo = lfirst(j);

      this_strats = bms_add_member(this_strats, opinfo->strategy);
    }
    if (i == 0)
    {
      strats = this_strats;
    }
    else
    {
      strats = bms_int_members(strats, this_strats);
    }
    i++;
  }

     
                                                                         
                                                                      
             
     
  i = bms_first_member(strats);
  if (i < 0)
  {
                                           
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not determine interpretation of row comparison operator %s", strVal(llast(opname))), errhint("Row comparison operators must be associated with btree operator families."), parser_errposition(pstate, location)));
  }
  rctype = (RowCompareType)i;

     
                                                                            
                      
     
  if (rctype == ROWCOMPARE_EQ)
  {
    return (Node *)makeBoolExpr(AND_EXPR, opexprs, location);
  }
  if (rctype == ROWCOMPARE_NE)
  {
    return (Node *)makeBoolExpr(OR_EXPR, opexprs, location);
  }

     
                                                                          
                    
     
  opfamilies = NIL;
  for (i = 0; i < nopers; i++)
  {
    Oid opfamily = InvalidOid;
    ListCell *j;

    foreach (j, opinfo_lists[i])
    {
      OpBtreeInterpretation *opinfo = lfirst(j);

      if (opinfo->strategy == rctype)
      {
        opfamily = opinfo->opfamily_id;
        break;
      }
    }
    if (OidIsValid(opfamily))
    {
      opfamilies = lappend_oid(opfamilies, opfamily);
    }
    else                        
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not determine interpretation of row comparison operator %s", strVal(llast(opname))), errdetail("There are multiple equally-plausible candidates."), parser_errposition(pstate, location)));
    }
  }

     
                                                              
     
                                                                     
                                                            
     
  opnos = NIL;
  largs = NIL;
  rargs = NIL;
  foreach (l, opexprs)
  {
    OpExpr *cmp = (OpExpr *)lfirst(l);

    opnos = lappend_oid(opnos, cmp->opno);
    largs = lappend(largs, linitial(cmp->args));
    rargs = lappend(rargs, lsecond(cmp->args));
  }

  rcexpr = makeNode(RowCompareExpr);
  rcexpr->rctype = rctype;
  rcexpr->opnos = opnos;
  rcexpr->opfamilies = opfamilies;
  rcexpr->inputcollids = NIL;                                           
  rcexpr->largs = largs;
  rcexpr->rargs = rargs;

  return (Node *)rcexpr;
}

   
                                                    
   
                                              
   
static Node *
make_row_distinct_op(ParseState *pstate, List *opname, RowExpr *lrow, RowExpr *rrow, int location)
{
  Node *result = NULL;
  List *largs = lrow->args;
  List *rargs = rrow->args;
  ListCell *l, *r;

  if (list_length(largs) != list_length(rargs))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unequal number of entries in row expressions"), parser_errposition(pstate, location)));
  }

  forboth(l, largs, r, rargs)
  {
    Node *larg = (Node *)lfirst(l);
    Node *rarg = (Node *)lfirst(r);
    Node *cmp;

    cmp = (Node *)make_distinct_op(pstate, opname, larg, rarg, location);
    if (result == NULL)
    {
      result = cmp;
    }
    else
    {
      result = (Node *)makeBoolExpr(OR_EXPR, list_make2(result, cmp), location);
    }
  }

  if (result == NULL)
  {
                                                    
    result = makeBoolConst(false, false);
  }

  return result;
}

   
                                                  
   
static Expr *
make_distinct_op(ParseState *pstate, List *opname, Node *ltree, Node *rtree, int location)
{
  Expr *result;

  result = make_op(pstate, opname, ltree, rtree, pstate->p_last_srf, location);
  if (((OpExpr *)result)->opresulttype != BOOLOID)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("IS DISTINCT FROM requires = operator to yield boolean"), parser_errposition(pstate, location)));
  }
  if (((OpExpr *)result)->opretset)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                 
                       errmsg("%s must not return a set", "IS DISTINCT FROM"), parser_errposition(pstate, location)));
  }

     
                                                          
     
  NodeSetTag(result, T_DistinctExpr);

  return result;
}

   
                                                                         
   
                                             
   
static Node *
make_nulltest_from_distinct(ParseState *pstate, A_Expr *distincta, Node *arg)
{
  NullTest *nt = makeNode(NullTest);

  nt->arg = (Expr *)transformExprRecurse(pstate, arg);
                                                        
  if (distincta->kind == AEXPR_NOT_DISTINCT)
  {
    nt->nulltesttype = IS_NULL;
  }
  else
  {
    nt->nulltesttype = IS_NOT_NULL;
  }
                                                                   
  nt->argisrow = false;
  nt->location = distincta->location;
  return (Node *)nt;
}

   
                                                          
   
                                                                                
   
                                                                          
                                                                         
                          
   
static int
operator_precedence_group(Node *node, const char **nodename)
{
  int group = 0;

  *nodename = NULL;
  if (node == NULL)
  {
    return 0;
  }

  if (IsA(node, A_Expr))
  {
    A_Expr *aexpr = (A_Expr *)node;

    if (aexpr->kind == AEXPR_OP && aexpr->lexpr != NULL && aexpr->rexpr != NULL)
    {
                           
      if (list_length(aexpr->name) == 1)
      {
        *nodename = strVal(linitial(aexpr->name));
                                                                   
        if (strcmp(*nodename, "+") == 0 || strcmp(*nodename, "-") == 0 || strcmp(*nodename, "*") == 0 || strcmp(*nodename, "/") == 0 || strcmp(*nodename, "%") == 0 || strcmp(*nodename, "^") == 0)
        {
          group = 0;
        }
        else if (strcmp(*nodename, "<") == 0 || strcmp(*nodename, ">") == 0)
        {
          group = PREC_GROUP_LESS;
        }
        else if (strcmp(*nodename, "=") == 0)
        {
          group = PREC_GROUP_EQUAL;
        }
        else if (strcmp(*nodename, "<=") == 0 || strcmp(*nodename, ">=") == 0 || strcmp(*nodename, "<>") == 0)
        {
          group = PREC_GROUP_LESS_EQUAL;
        }
        else
        {
          group = PREC_GROUP_INFIX_OP;
        }
      }
      else
      {
                                              
        *nodename = "OPERATOR()";
        group = PREC_GROUP_INFIX_OP;
      }
    }
    else if (aexpr->kind == AEXPR_OP && aexpr->lexpr == NULL && aexpr->rexpr != NULL)
    {
                           
      if (list_length(aexpr->name) == 1)
      {
        *nodename = strVal(linitial(aexpr->name));
                                                                   
        if (strcmp(*nodename, "+") == 0 || strcmp(*nodename, "-") == 0)
        {
          group = 0;
        }
        else
        {
          group = PREC_GROUP_PREFIX_OP;
        }
      }
      else
      {
                                              
        *nodename = "OPERATOR()";
        group = PREC_GROUP_PREFIX_OP;
      }
    }
    else if (aexpr->kind == AEXPR_OP && aexpr->lexpr != NULL && aexpr->rexpr == NULL)
    {
                            
      if (list_length(aexpr->name) == 1)
      {
        *nodename = strVal(linitial(aexpr->name));
        group = PREC_GROUP_POSTFIX_OP;
      }
      else
      {
                                              
        *nodename = "OPERATOR()";
        group = PREC_GROUP_POSTFIX_OP;
      }
    }
    else if (aexpr->kind == AEXPR_OP_ANY || aexpr->kind == AEXPR_OP_ALL)
    {
      *nodename = strVal(llast(aexpr->name));
      group = PREC_GROUP_POSTFIX_OP;
    }
    else if (aexpr->kind == AEXPR_DISTINCT || aexpr->kind == AEXPR_NOT_DISTINCT)
    {
      *nodename = "IS";
      group = PREC_GROUP_INFIX_IS;
    }
    else if (aexpr->kind == AEXPR_OF)
    {
      *nodename = "IS";
      group = PREC_GROUP_POSTFIX_IS;
    }
    else if (aexpr->kind == AEXPR_IN)
    {
      *nodename = "IN";
      if (strcmp(strVal(linitial(aexpr->name)), "=") == 0)
      {
        group = PREC_GROUP_IN;
      }
      else
      {
        group = PREC_GROUP_NOT_IN;
      }
    }
    else if (aexpr->kind == AEXPR_LIKE)
    {
      *nodename = "LIKE";
      if (strcmp(strVal(linitial(aexpr->name)), "~~") == 0)
      {
        group = PREC_GROUP_LIKE;
      }
      else
      {
        group = PREC_GROUP_NOT_LIKE;
      }
    }
    else if (aexpr->kind == AEXPR_ILIKE)
    {
      *nodename = "ILIKE";
      if (strcmp(strVal(linitial(aexpr->name)), "~~*") == 0)
      {
        group = PREC_GROUP_LIKE;
      }
      else
      {
        group = PREC_GROUP_NOT_LIKE;
      }
    }
    else if (aexpr->kind == AEXPR_SIMILAR)
    {
      *nodename = "SIMILAR";
      if (strcmp(strVal(linitial(aexpr->name)), "~") == 0)
      {
        group = PREC_GROUP_LIKE;
      }
      else
      {
        group = PREC_GROUP_NOT_LIKE;
      }
    }
    else if (aexpr->kind == AEXPR_BETWEEN || aexpr->kind == AEXPR_BETWEEN_SYM)
    {
      Assert(list_length(aexpr->name) == 1);
      *nodename = strVal(linitial(aexpr->name));
      group = PREC_GROUP_BETWEEN;
    }
    else if (aexpr->kind == AEXPR_NOT_BETWEEN || aexpr->kind == AEXPR_NOT_BETWEEN_SYM)
    {
      Assert(list_length(aexpr->name) == 1);
      *nodename = strVal(linitial(aexpr->name));
      group = PREC_GROUP_NOT_BETWEEN;
    }
  }
  else if (IsA(node, NullTest) || IsA(node, BooleanTest))
  {
    *nodename = "IS";
    group = PREC_GROUP_POSTFIX_IS;
  }
  else if (IsA(node, XmlExpr))
  {
    XmlExpr *x = (XmlExpr *)node;

    if (x->op == IS_DOCUMENT)
    {
      *nodename = "IS";
      group = PREC_GROUP_POSTFIX_IS;
    }
  }
  else if (IsA(node, SubLink))
  {
    SubLink *s = (SubLink *)node;

    if (s->subLinkType == ANY_SUBLINK || s->subLinkType == ALL_SUBLINK)
    {
      if (s->operName == NIL)
      {
        *nodename = "IN";
        group = PREC_GROUP_IN;
      }
      else
      {
        *nodename = strVal(llast(s->operName));
        group = PREC_GROUP_POSTFIX_OP;
      }
    }
  }
  else if (IsA(node, BoolExpr))
  {
       
                                                                          
                                                                         
                                                                      
                                                                   
       
                                                                           
                                                                      
                               
       
    BoolExpr *b = (BoolExpr *)node;

    if (b->boolop == NOT_EXPR)
    {
      Node *child = (Node *)linitial(b->args);

      if (IsA(child, XmlExpr))
      {
        XmlExpr *x = (XmlExpr *)child;

        if (x->op == IS_DOCUMENT && x->location == b->location)
        {
          *nodename = "IS";
          group = PREC_GROUP_POSTFIX_IS;
        }
      }
      else if (IsA(child, SubLink))
      {
        SubLink *s = (SubLink *)child;

        if (s->subLinkType == ANY_SUBLINK && s->operName == NIL && s->location == b->location)
        {
          *nodename = "IN";
          group = PREC_GROUP_NOT_IN;
        }
      }
    }
  }
  return group;
}

   
                                                                         
   
                                                      
                                                                         
   
                                                                         
                                                                               
                                                                             
                                                                          
                                                                         
   
static void
emit_precedence_warnings(ParseState *pstate, int opgroup, const char *opname, Node *lchild, Node *rchild, int location)
{
  int cgroup;
  const char *copname;

  Assert(opgroup > 0);

     
                                                                       
                                                              
     
                                                                       
                                                                          
                 
     
  cgroup = operator_precedence_group(lchild, &copname);
  if (cgroup > 0)
  {
    if (oldprecedence_l[cgroup] < oldprecedence_r[opgroup] && cgroup != PREC_GROUP_IN && cgroup != PREC_GROUP_NOT_IN && cgroup != PREC_GROUP_POSTFIX_OP && cgroup != PREC_GROUP_POSTFIX_IS)
    {
      ereport(WARNING, (errmsg("operator precedence change: %s is now lower precedence than %s", opname, copname), parser_errposition(pstate, location)));
    }
  }

     
                                                                             
                                                         
     
                                                                             
                                                                
     
  cgroup = operator_precedence_group(rchild, &copname);
  if (cgroup > 0)
  {
    if (oldprecedence_r[cgroup] <= oldprecedence_l[opgroup] && cgroup != PREC_GROUP_PREFIX_OP)
    {
      ereport(WARNING, (errmsg("operator precedence change: %s is now lower precedence than %s", opname, copname), parser_errposition(pstate, location)));
    }
  }
}

   
                                                       
   
                                                                           
                                                                           
                         
   
const char *
ParseExprKindName(ParseExprKind exprKind)
{
  switch (exprKind)
  {
  case EXPR_KIND_NONE:
    return "invalid expression context";
  case EXPR_KIND_OTHER:
    return "extension expression";
  case EXPR_KIND_JOIN_ON:
    return "JOIN/ON";
  case EXPR_KIND_JOIN_USING:
    return "JOIN/USING";
  case EXPR_KIND_FROM_SUBSELECT:
    return "sub-SELECT in FROM";
  case EXPR_KIND_FROM_FUNCTION:
    return "function in FROM";
  case EXPR_KIND_WHERE:
    return "WHERE";
  case EXPR_KIND_POLICY:
    return "POLICY";
  case EXPR_KIND_HAVING:
    return "HAVING";
  case EXPR_KIND_FILTER:
    return "FILTER";
  case EXPR_KIND_WINDOW_PARTITION:
    return "window PARTITION BY";
  case EXPR_KIND_WINDOW_ORDER:
    return "window ORDER BY";
  case EXPR_KIND_WINDOW_FRAME_RANGE:
    return "window RANGE";
  case EXPR_KIND_WINDOW_FRAME_ROWS:
    return "window ROWS";
  case EXPR_KIND_WINDOW_FRAME_GROUPS:
    return "window GROUPS";
  case EXPR_KIND_SELECT_TARGET:
    return "SELECT";
  case EXPR_KIND_INSERT_TARGET:
    return "INSERT";
  case EXPR_KIND_UPDATE_SOURCE:
  case EXPR_KIND_UPDATE_TARGET:
    return "UPDATE";
  case EXPR_KIND_GROUP_BY:
    return "GROUP BY";
  case EXPR_KIND_ORDER_BY:
    return "ORDER BY";
  case EXPR_KIND_DISTINCT_ON:
    return "DISTINCT ON";
  case EXPR_KIND_LIMIT:
    return "LIMIT";
  case EXPR_KIND_OFFSET:
    return "OFFSET";
  case EXPR_KIND_RETURNING:
    return "RETURNING";
  case EXPR_KIND_VALUES:
  case EXPR_KIND_VALUES_SINGLE:
    return "VALUES";
  case EXPR_KIND_CHECK_CONSTRAINT:
  case EXPR_KIND_DOMAIN_CHECK:
    return "CHECK";
  case EXPR_KIND_COLUMN_DEFAULT:
  case EXPR_KIND_FUNCTION_DEFAULT:
    return "DEFAULT";
  case EXPR_KIND_INDEX_EXPRESSION:
    return "index expression";
  case EXPR_KIND_INDEX_PREDICATE:
    return "index predicate";
  case EXPR_KIND_ALTER_COL_TRANSFORM:
    return "USING";
  case EXPR_KIND_EXECUTE_PARAMETER:
    return "EXECUTE";
  case EXPR_KIND_TRIGGER_WHEN:
    return "WHEN";
  case EXPR_KIND_PARTITION_BOUND:
    return "partition bound";
  case EXPR_KIND_PARTITION_EXPRESSION:
    return "PARTITION BY";
  case EXPR_KIND_CALL_ARGUMENT:
    return "CALL";
  case EXPR_KIND_COPY_WHERE:
    return "WHERE";
  case EXPR_KIND_GENERATED_COLUMN:
    return "GENERATED AS";

       
                                                                 
                                                                
                                                                     
                                                                 
       
  }
  return "unrecognized expression kind";
}
