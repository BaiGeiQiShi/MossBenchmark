                                                                            
   
               
                                                        
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/execnodes.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

static bool
expression_returns_set_walker(Node *node, void *context);
static int
leftmostLoc(int loc1, int loc2);
static bool
fix_opfuncids_walker(Node *node, void *context);
static bool
planstate_walk_subplans(List *plans, bool (*walker)(), void *context);
static bool
planstate_walk_members(PlanState **planstates, int nplans, bool (*walker)(), void *context);

   
              
                                                             
   
Oid
exprType(const Node *expr)
{
  Oid type;

  if (!expr)
  {
    return InvalidOid;
  }

  switch (nodeTag(expr))
  {
  case T_Var:
    type = ((const Var *)expr)->vartype;
    break;
  case T_Const:
    type = ((const Const *)expr)->consttype;
    break;
  case T_Param:
    type = ((const Param *)expr)->paramtype;
    break;
  case T_Aggref:
    type = ((const Aggref *)expr)->aggtype;
    break;
  case T_GroupingFunc:
    type = INT4OID;
    break;
  case T_WindowFunc:
    type = ((const WindowFunc *)expr)->wintype;
    break;
  case T_SubscriptingRef:
  {
    const SubscriptingRef *sbsref = (const SubscriptingRef *)expr;

                                                                
    if (sbsref->reflowerindexpr || sbsref->refassgnexpr)
    {
      type = sbsref->refcontainertype;
    }
    else
    {
      type = sbsref->refelemtype;
    }
  }
  break;
  case T_FuncExpr:
    type = ((const FuncExpr *)expr)->funcresulttype;
    break;
  case T_NamedArgExpr:
    type = exprType((Node *)((const NamedArgExpr *)expr)->arg);
    break;
  case T_OpExpr:
    type = ((const OpExpr *)expr)->opresulttype;
    break;
  case T_DistinctExpr:
    type = ((const DistinctExpr *)expr)->opresulttype;
    break;
  case T_NullIfExpr:
    type = ((const NullIfExpr *)expr)->opresulttype;
    break;
  case T_ScalarArrayOpExpr:
    type = BOOLOID;
    break;
  case T_BoolExpr:
    type = BOOLOID;
    break;
  case T_SubLink:
  {
    const SubLink *sublink = (const SubLink *)expr;

    if (sublink->subLinkType == EXPR_SUBLINK || sublink->subLinkType == ARRAY_SUBLINK)
    {
                                                               
      Query *qtree = (Query *)sublink->subselect;
      TargetEntry *tent;

      if (!qtree || !IsA(qtree, Query))
      {
        elog(ERROR, "cannot get type for untransformed sublink");
      }
      tent = linitial_node(TargetEntry, qtree->targetList);
      Assert(!tent->resjunk);
      type = exprType((Node *)tent->expr);
      if (sublink->subLinkType == ARRAY_SUBLINK)
      {
        type = get_promoted_array_type(type);
        if (!OidIsValid(type))
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(exprType((Node *)tent->expr)))));
        }
      }
    }
    else if (sublink->subLinkType == MULTIEXPR_SUBLINK)
    {
                                                           
      type = RECORDOID;
    }
    else
    {
                                                          
      type = BOOLOID;
    }
  }
  break;
  case T_SubPlan:
  {
    const SubPlan *subplan = (const SubPlan *)expr;

    if (subplan->subLinkType == EXPR_SUBLINK || subplan->subLinkType == ARRAY_SUBLINK)
    {
                                                               
      type = subplan->firstColType;
      if (subplan->subLinkType == ARRAY_SUBLINK)
      {
        type = get_promoted_array_type(type);
        if (!OidIsValid(type))
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(subplan->firstColType))));
        }
      }
    }
    else if (subplan->subLinkType == MULTIEXPR_SUBLINK)
    {
                                                           
      type = RECORDOID;
    }
    else
    {
                                                          
      type = BOOLOID;
    }
  }
  break;
  case T_AlternativeSubPlan:
  {
    const AlternativeSubPlan *asplan = (const AlternativeSubPlan *)expr;

                                                   
    type = exprType((Node *)linitial(asplan->subplans));
  }
  break;
  case T_FieldSelect:
    type = ((const FieldSelect *)expr)->resulttype;
    break;
  case T_FieldStore:
    type = ((const FieldStore *)expr)->resulttype;
    break;
  case T_RelabelType:
    type = ((const RelabelType *)expr)->resulttype;
    break;
  case T_CoerceViaIO:
    type = ((const CoerceViaIO *)expr)->resulttype;
    break;
  case T_ArrayCoerceExpr:
    type = ((const ArrayCoerceExpr *)expr)->resulttype;
    break;
  case T_ConvertRowtypeExpr:
    type = ((const ConvertRowtypeExpr *)expr)->resulttype;
    break;
  case T_CollateExpr:
    type = exprType((Node *)((const CollateExpr *)expr)->arg);
    break;
  case T_CaseExpr:
    type = ((const CaseExpr *)expr)->casetype;
    break;
  case T_CaseTestExpr:
    type = ((const CaseTestExpr *)expr)->typeId;
    break;
  case T_ArrayExpr:
    type = ((const ArrayExpr *)expr)->array_typeid;
    break;
  case T_RowExpr:
    type = ((const RowExpr *)expr)->row_typeid;
    break;
  case T_RowCompareExpr:
    type = BOOLOID;
    break;
  case T_CoalesceExpr:
    type = ((const CoalesceExpr *)expr)->coalescetype;
    break;
  case T_MinMaxExpr:
    type = ((const MinMaxExpr *)expr)->minmaxtype;
    break;
  case T_SQLValueFunction:
    type = ((const SQLValueFunction *)expr)->type;
    break;
  case T_XmlExpr:
    if (((const XmlExpr *)expr)->op == IS_DOCUMENT)
    {
      type = BOOLOID;
    }
    else if (((const XmlExpr *)expr)->op == IS_XMLSERIALIZE)
    {
      type = TEXTOID;
    }
    else
    {
      type = XMLOID;
    }
    break;
  case T_NullTest:
    type = BOOLOID;
    break;
  case T_BooleanTest:
    type = BOOLOID;
    break;
  case T_CoerceToDomain:
    type = ((const CoerceToDomain *)expr)->resulttype;
    break;
  case T_CoerceToDomainValue:
    type = ((const CoerceToDomainValue *)expr)->typeId;
    break;
  case T_SetToDefault:
    type = ((const SetToDefault *)expr)->typeId;
    break;
  case T_CurrentOfExpr:
    type = BOOLOID;
    break;
  case T_NextValueExpr:
    type = ((const NextValueExpr *)expr)->typeId;
    break;
  case T_InferenceElem:
  {
    const InferenceElem *n = (const InferenceElem *)expr;

    type = exprType((Node *)n->expr);
  }
  break;
  case T_PlaceHolderVar:
    type = exprType((Node *)((const PlaceHolderVar *)expr)->phexpr);
    break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(expr));
    type = InvalidOid;                          
    break;
  }
  return type;
}

   
                
                                                                         
                                                                         
   
int32
exprTypmod(const Node *expr)
{
  if (!expr)
  {
    return -1;
  }

  switch (nodeTag(expr))
  {
  case T_Var:
    return ((const Var *)expr)->vartypmod;
  case T_Const:
    return ((const Const *)expr)->consttypmod;
  case T_Param:
    return ((const Param *)expr)->paramtypmod;
  case T_SubscriptingRef:
                                                     
    return ((const SubscriptingRef *)expr)->reftypmod;
  case T_FuncExpr:
  {
    int32 coercedTypmod;

                                                     
    if (exprIsLengthCoercion(expr, &coercedTypmod))
    {
      return coercedTypmod;
    }
  }
  break;
  case T_NamedArgExpr:
    return exprTypmod((Node *)((const NamedArgExpr *)expr)->arg);
  case T_NullIfExpr:
  {
       
                                                                 
                                         
       
    const NullIfExpr *nexpr = (const NullIfExpr *)expr;

    return exprTypmod((Node *)linitial(nexpr->args));
  }
  break;
  case T_SubLink:
  {
    const SubLink *sublink = (const SubLink *)expr;

    if (sublink->subLinkType == EXPR_SUBLINK || sublink->subLinkType == ARRAY_SUBLINK)
    {
                                                                 
      Query *qtree = (Query *)sublink->subselect;
      TargetEntry *tent;

      if (!qtree || !IsA(qtree, Query))
      {
        elog(ERROR, "cannot get type for untransformed sublink");
      }
      tent = linitial_node(TargetEntry, qtree->targetList);
      Assert(!tent->resjunk);
      return exprTypmod((Node *)tent->expr);
                                                       
    }
                                                              
  }
  break;
  case T_SubPlan:
  {
    const SubPlan *subplan = (const SubPlan *)expr;

    if (subplan->subLinkType == EXPR_SUBLINK || subplan->subLinkType == ARRAY_SUBLINK)
    {
                                                                 
                                                       
      return subplan->firstColTypmod;
    }
                                                              
  }
  break;
  case T_AlternativeSubPlan:
  {
    const AlternativeSubPlan *asplan = (const AlternativeSubPlan *)expr;

                                                   
    return exprTypmod((Node *)linitial(asplan->subplans));
  }
  break;
  case T_FieldSelect:
    return ((const FieldSelect *)expr)->resulttypmod;
  case T_RelabelType:
    return ((const RelabelType *)expr)->resulttypmod;
  case T_ArrayCoerceExpr:
    return ((const ArrayCoerceExpr *)expr)->resulttypmod;
  case T_CollateExpr:
    return exprTypmod((Node *)((const CollateExpr *)expr)->arg);
  case T_CaseExpr:
  {
       
                                                                 
                           
       
    const CaseExpr *cexpr = (const CaseExpr *)expr;
    Oid casetype = cexpr->casetype;
    int32 typmod;
    ListCell *arg;

    if (!cexpr->defresult)
    {
      return -1;
    }
    if (exprType((Node *)cexpr->defresult) != casetype)
    {
      return -1;
    }
    typmod = exprTypmod((Node *)cexpr->defresult);
    if (typmod < 0)
    {
      return -1;                                
    }
    foreach (arg, cexpr->args)
    {
      CaseWhen *w = lfirst_node(CaseWhen, arg);

      if (exprType((Node *)w->result) != casetype)
      {
        return -1;
      }
      if (exprTypmod((Node *)w->result) != typmod)
      {
        return -1;
      }
    }
    return typmod;
  }
  break;
  case T_CaseTestExpr:
    return ((const CaseTestExpr *)expr)->typeMod;
  case T_ArrayExpr:
  {
       
                                                             
                           
       
    const ArrayExpr *arrayexpr = (const ArrayExpr *)expr;
    Oid commontype;
    int32 typmod;
    ListCell *elem;

    if (arrayexpr->elements == NIL)
    {
      return -1;
    }
    typmod = exprTypmod((Node *)linitial(arrayexpr->elements));
    if (typmod < 0)
    {
      return -1;                                
    }
    if (arrayexpr->multidims)
    {
      commontype = arrayexpr->array_typeid;
    }
    else
    {
      commontype = arrayexpr->element_typeid;
    }
    foreach (elem, arrayexpr->elements)
    {
      Node *e = (Node *)lfirst(elem);

      if (exprType(e) != commontype)
      {
        return -1;
      }
      if (exprTypmod(e) != typmod)
      {
        return -1;
      }
    }
    return typmod;
  }
  break;
  case T_CoalesceExpr:
  {
       
                                                                 
                           
       
    const CoalesceExpr *cexpr = (const CoalesceExpr *)expr;
    Oid coalescetype = cexpr->coalescetype;
    int32 typmod;
    ListCell *arg;

    if (exprType((Node *)linitial(cexpr->args)) != coalescetype)
    {
      return -1;
    }
    typmod = exprTypmod((Node *)linitial(cexpr->args));
    if (typmod < 0)
    {
      return -1;                                
    }
    for_each_cell(arg, lnext(list_head(cexpr->args)))
    {
      Node *e = (Node *)lfirst(arg);

      if (exprType(e) != coalescetype)
      {
        return -1;
      }
      if (exprTypmod(e) != typmod)
      {
        return -1;
      }
    }
    return typmod;
  }
  break;
  case T_MinMaxExpr:
  {
       
                                                                 
                           
       
    const MinMaxExpr *mexpr = (const MinMaxExpr *)expr;
    Oid minmaxtype = mexpr->minmaxtype;
    int32 typmod;
    ListCell *arg;

    if (exprType((Node *)linitial(mexpr->args)) != minmaxtype)
    {
      return -1;
    }
    typmod = exprTypmod((Node *)linitial(mexpr->args));
    if (typmod < 0)
    {
      return -1;                                
    }
    for_each_cell(arg, lnext(list_head(mexpr->args)))
    {
      Node *e = (Node *)lfirst(arg);

      if (exprType(e) != minmaxtype)
      {
        return -1;
      }
      if (exprTypmod(e) != typmod)
      {
        return -1;
      }
    }
    return typmod;
  }
  break;
  case T_SQLValueFunction:
    return ((const SQLValueFunction *)expr)->typmod;
  case T_CoerceToDomain:
    return ((const CoerceToDomain *)expr)->resulttypmod;
  case T_CoerceToDomainValue:
    return ((const CoerceToDomainValue *)expr)->typeMod;
  case T_SetToDefault:
    return ((const SetToDefault *)expr)->typeMod;
  case T_PlaceHolderVar:
    return exprTypmod((Node *)((const PlaceHolderVar *)expr)->phexpr);
  default:
    break;
  }
  return -1;
}

   
                        
                                                                        
                                                                       
   
                                                                              
                                                           
   
                                                                      
                                    
   
bool
exprIsLengthCoercion(const Node *expr, int32 *coercedTypmod)
{
  if (coercedTypmod != NULL)
  {
    *coercedTypmod = -1;                                
  }

     
                                                                             
                          
     
  if (expr && IsA(expr, FuncExpr))
  {
    const FuncExpr *func = (const FuncExpr *)expr;
    int nargs;
    Const *second_arg;

       
                                                          
       
    if (func->funcformat != COERCE_EXPLICIT_CAST && func->funcformat != COERCE_IMPLICIT_CAST)
    {
      return false;
    }

       
                                                                      
                                                                          
                                                                     
       
    nargs = list_length(func->args);
    if (nargs < 2 || nargs > 3)
    {
      return false;
    }

    second_arg = (Const *)lsecond(func->args);
    if (!IsA(second_arg, Const) || second_arg->consttype != INT4OID || second_arg->constisnull)
    {
      return false;
    }

       
                                                    
       
    if (coercedTypmod != NULL)
    {
      *coercedTypmod = DatumGetInt32(second_arg->constvalue);
    }

    return true;
  }

  if (expr && IsA(expr, ArrayCoerceExpr))
  {
    const ArrayCoerceExpr *acoerce = (const ArrayCoerceExpr *)expr;

                                                                       
    if (acoerce->resulttypmod < 0)
    {
      return false;
    }

       
                                                      
       
    if (coercedTypmod != NULL)
    {
      *coercedTypmod = acoerce->resulttypmod;
    }

    return true;
  }

  return false;
}

   
                    
                                                                   
                                               
   
                                                                              
                                                                          
                                                                          
                                                                          
                                                                           
                                                                
   
Node *
applyRelabelType(Node *arg, Oid rtype, int32 rtypmod, Oid rcollid, CoercionForm rformat, int rlocation, bool overwrite_ok)
{
     
                                                                             
                                                                     
                                         
     
  while (arg && IsA(arg, RelabelType))
  {
    arg = (Node *)((RelabelType *)arg)->arg;
  }

  if (arg && IsA(arg, Const))
  {
                                                               
    Const *con = (Const *)arg;

    if (!overwrite_ok)
    {
      con = copyObject(con);
    }
    con->consttype = rtype;
    con->consttypmod = rtypmod;
    con->constcollid = rcollid;
                                                
    return (Node *)con;
  }
  else if (exprType(arg) == rtype && exprTypmod(arg) == rtypmod && exprCollation(arg) == rcollid)
  {
                                                                       
    return arg;
  }
  else
  {
                                         
    RelabelType *newrelabel = makeNode(RelabelType);

    newrelabel->arg = (Expr *)arg;
    newrelabel->resulttype = rtype;
    newrelabel->resulttypmod = rtypmod;
    newrelabel->resultcollid = rcollid;
    newrelabel->relabelformat = rformat;
    newrelabel->location = rlocation;
    return (Node *)newrelabel;
  }
}

   
                     
                                                                           
   
                                                                
   
Node *
relabel_to_typmod(Node *expr, int32 typmod)
{
  return applyRelabelType(expr, exprType(expr), typmod, exprCollation(expr), COERCE_EXPLICIT_CAST, -1, false);
}

   
                                                                            
   
                                                                        
                                          
   
                                                                        
                                                                          
   
Node *
strip_implicit_coercions(Node *node)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, FuncExpr))
  {
    FuncExpr *f = (FuncExpr *)node;

    if (f->funcformat == COERCE_IMPLICIT_CAST)
    {
      return strip_implicit_coercions(linitial(f->args));
    }
  }
  else if (IsA(node, RelabelType))
  {
    RelabelType *r = (RelabelType *)node;

    if (r->relabelformat == COERCE_IMPLICIT_CAST)
    {
      return strip_implicit_coercions((Node *)r->arg);
    }
  }
  else if (IsA(node, CoerceViaIO))
  {
    CoerceViaIO *c = (CoerceViaIO *)node;

    if (c->coerceformat == COERCE_IMPLICIT_CAST)
    {
      return strip_implicit_coercions((Node *)c->arg);
    }
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
    ArrayCoerceExpr *c = (ArrayCoerceExpr *)node;

    if (c->coerceformat == COERCE_IMPLICIT_CAST)
    {
      return strip_implicit_coercions((Node *)c->arg);
    }
  }
  else if (IsA(node, ConvertRowtypeExpr))
  {
    ConvertRowtypeExpr *c = (ConvertRowtypeExpr *)node;

    if (c->convertformat == COERCE_IMPLICIT_CAST)
    {
      return strip_implicit_coercions((Node *)c->arg);
    }
  }
  else if (IsA(node, CoerceToDomain))
  {
    CoerceToDomain *c = (CoerceToDomain *)node;

    if (c->coercionformat == COERCE_IMPLICIT_CAST)
    {
      return strip_implicit_coercions((Node *)c->arg);
    }
  }
  return node;
}

   
                          
                                                      
   
                                                                        
                                                                       
                  
   
bool
expression_returns_set(Node *clause)
{
  return expression_returns_set_walker(clause, NULL);
}

static bool
expression_returns_set_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, FuncExpr))
  {
    FuncExpr *expr = (FuncExpr *)node;

    if (expr->funcretset)
    {
      return true;
    }
                                         
  }
  if (IsA(node, OpExpr))
  {
    OpExpr *expr = (OpExpr *)node;

    if (expr->opretset)
    {
      return true;
    }
                                         
  }

                                                                             
  if (IsA(node, Aggref))
  {
    return false;
  }
  if (IsA(node, GroupingFunc))
  {
    return false;
  }
  if (IsA(node, WindowFunc))
  {
    return false;
  }

  return expression_tree_walker(node, expression_returns_set_walker, context);
}

   
                   
                                                                  
   
                                                                      
                                                                            
                                                                            
                                                                          
                                                                         
                                         
   
Oid
exprCollation(const Node *expr)
{
  Oid coll;

  if (!expr)
  {
    return InvalidOid;
  }

  switch (nodeTag(expr))
  {
  case T_Var:
    coll = ((const Var *)expr)->varcollid;
    break;
  case T_Const:
    coll = ((const Const *)expr)->constcollid;
    break;
  case T_Param:
    coll = ((const Param *)expr)->paramcollid;
    break;
  case T_Aggref:
    coll = ((const Aggref *)expr)->aggcollid;
    break;
  case T_GroupingFunc:
    coll = InvalidOid;
    break;
  case T_WindowFunc:
    coll = ((const WindowFunc *)expr)->wincollid;
    break;
  case T_SubscriptingRef:
    coll = ((const SubscriptingRef *)expr)->refcollid;
    break;
  case T_FuncExpr:
    coll = ((const FuncExpr *)expr)->funccollid;
    break;
  case T_NamedArgExpr:
    coll = exprCollation((Node *)((const NamedArgExpr *)expr)->arg);
    break;
  case T_OpExpr:
    coll = ((const OpExpr *)expr)->opcollid;
    break;
  case T_DistinctExpr:
    coll = ((const DistinctExpr *)expr)->opcollid;
    break;
  case T_NullIfExpr:
    coll = ((const NullIfExpr *)expr)->opcollid;
    break;
  case T_ScalarArrayOpExpr:
    coll = InvalidOid;                               
    break;
  case T_BoolExpr:
    coll = InvalidOid;                               
    break;
  case T_SubLink:
  {
    const SubLink *sublink = (const SubLink *)expr;

    if (sublink->subLinkType == EXPR_SUBLINK || sublink->subLinkType == ARRAY_SUBLINK)
    {
                                                                
      Query *qtree = (Query *)sublink->subselect;
      TargetEntry *tent;

      if (!qtree || !IsA(qtree, Query))
      {
        elog(ERROR, "cannot get collation for untransformed sublink");
      }
      tent = linitial_node(TargetEntry, qtree->targetList);
      Assert(!tent->resjunk);
      coll = exprCollation((Node *)tent->expr);
                                                               
    }
    else
    {
                                                  
      coll = InvalidOid;
    }
  }
  break;
  case T_SubPlan:
  {
    const SubPlan *subplan = (const SubPlan *)expr;

    if (subplan->subLinkType == EXPR_SUBLINK || subplan->subLinkType == ARRAY_SUBLINK)
    {
                                                                
      coll = subplan->firstColCollation;
                                                               
    }
    else
    {
                                                  
      coll = InvalidOid;
    }
  }
  break;
  case T_AlternativeSubPlan:
  {
    const AlternativeSubPlan *asplan = (const AlternativeSubPlan *)expr;

                                                   
    coll = exprCollation((Node *)linitial(asplan->subplans));
  }
  break;
  case T_FieldSelect:
    coll = ((const FieldSelect *)expr)->resultcollid;
    break;
  case T_FieldStore:
    coll = InvalidOid;                                 
    break;
  case T_RelabelType:
    coll = ((const RelabelType *)expr)->resultcollid;
    break;
  case T_CoerceViaIO:
    coll = ((const CoerceViaIO *)expr)->resultcollid;
    break;
  case T_ArrayCoerceExpr:
    coll = ((const ArrayCoerceExpr *)expr)->resultcollid;
    break;
  case T_ConvertRowtypeExpr:
    coll = InvalidOid;                                 
    break;
  case T_CollateExpr:
    coll = ((const CollateExpr *)expr)->collOid;
    break;
  case T_CaseExpr:
    coll = ((const CaseExpr *)expr)->casecollid;
    break;
  case T_CaseTestExpr:
    coll = ((const CaseTestExpr *)expr)->collation;
    break;
  case T_ArrayExpr:
    coll = ((const ArrayExpr *)expr)->array_collid;
    break;
  case T_RowExpr:
    coll = InvalidOid;                                 
    break;
  case T_RowCompareExpr:
    coll = InvalidOid;                               
    break;
  case T_CoalesceExpr:
    coll = ((const CoalesceExpr *)expr)->coalescecollid;
    break;
  case T_MinMaxExpr:
    coll = ((const MinMaxExpr *)expr)->minmaxcollid;
    break;
  case T_SQLValueFunction:
                                                      
    if (((const SQLValueFunction *)expr)->type == NAMEOID)
    {
      coll = C_COLLATION_OID;
    }
    else
    {
      coll = InvalidOid;
    }
    break;
  case T_XmlExpr:

       
                                                                    
                                                                       
                                      
       
    if (((const XmlExpr *)expr)->op == IS_XMLSERIALIZE)
    {
      coll = DEFAULT_COLLATION_OID;
    }
    else
    {
      coll = InvalidOid;
    }
    break;
  case T_NullTest:
    coll = InvalidOid;                               
    break;
  case T_BooleanTest:
    coll = InvalidOid;                               
    break;
  case T_CoerceToDomain:
    coll = ((const CoerceToDomain *)expr)->resultcollid;
    break;
  case T_CoerceToDomainValue:
    coll = ((const CoerceToDomainValue *)expr)->collation;
    break;
  case T_SetToDefault:
    coll = ((const SetToDefault *)expr)->collation;
    break;
  case T_CurrentOfExpr:
    coll = InvalidOid;                               
    break;
  case T_NextValueExpr:
    coll = InvalidOid;                                       
    break;
  case T_InferenceElem:
    coll = exprCollation((Node *)((const InferenceElem *)expr)->expr);
    break;
  case T_PlaceHolderVar:
    coll = exprCollation((Node *)((const PlaceHolderVar *)expr)->phexpr);
    break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(expr));
    coll = InvalidOid;                          
    break;
  }
  return coll;
}

   
                        
                                                                           
   
                                                                         
   
Oid
exprInputCollation(const Node *expr)
{
  Oid coll;

  if (!expr)
  {
    return InvalidOid;
  }

  switch (nodeTag(expr))
  {
  case T_Aggref:
    coll = ((const Aggref *)expr)->inputcollid;
    break;
  case T_WindowFunc:
    coll = ((const WindowFunc *)expr)->inputcollid;
    break;
  case T_FuncExpr:
    coll = ((const FuncExpr *)expr)->inputcollid;
    break;
  case T_OpExpr:
    coll = ((const OpExpr *)expr)->inputcollid;
    break;
  case T_DistinctExpr:
    coll = ((const DistinctExpr *)expr)->inputcollid;
    break;
  case T_NullIfExpr:
    coll = ((const NullIfExpr *)expr)->inputcollid;
    break;
  case T_ScalarArrayOpExpr:
    coll = ((const ScalarArrayOpExpr *)expr)->inputcollid;
    break;
  case T_MinMaxExpr:
    coll = ((const MinMaxExpr *)expr)->inputcollid;
    break;
  default:
    coll = InvalidOid;
    break;
  }
  return coll;
}

   
                      
                                                              
   
                                                                         
                                            
   
void
exprSetCollation(Node *expr, Oid collation)
{
  switch (nodeTag(expr))
  {
  case T_Var:
    ((Var *)expr)->varcollid = collation;
    break;
  case T_Const:
    ((Const *)expr)->constcollid = collation;
    break;
  case T_Param:
    ((Param *)expr)->paramcollid = collation;
    break;
  case T_Aggref:
    ((Aggref *)expr)->aggcollid = collation;
    break;
  case T_GroupingFunc:
    Assert(!OidIsValid(collation));
    break;
  case T_WindowFunc:
    ((WindowFunc *)expr)->wincollid = collation;
    break;
  case T_SubscriptingRef:
    ((SubscriptingRef *)expr)->refcollid = collation;
    break;
  case T_FuncExpr:
    ((FuncExpr *)expr)->funccollid = collation;
    break;
  case T_NamedArgExpr:
    Assert(collation == exprCollation((Node *)((NamedArgExpr *)expr)->arg));
    break;
  case T_OpExpr:
    ((OpExpr *)expr)->opcollid = collation;
    break;
  case T_DistinctExpr:
    ((DistinctExpr *)expr)->opcollid = collation;
    break;
  case T_NullIfExpr:
    ((NullIfExpr *)expr)->opcollid = collation;
    break;
  case T_ScalarArrayOpExpr:
    Assert(!OidIsValid(collation));                               
    break;
  case T_BoolExpr:
    Assert(!OidIsValid(collation));                               
    break;
  case T_SubLink:
#ifdef USE_ASSERT_CHECKING
  {
    SubLink *sublink = (SubLink *)expr;

    if (sublink->subLinkType == EXPR_SUBLINK || sublink->subLinkType == ARRAY_SUBLINK)
    {
                                                                
      Query *qtree = (Query *)sublink->subselect;
      TargetEntry *tent;

      if (!qtree || !IsA(qtree, Query))
      {
        elog(ERROR, "cannot set collation for untransformed sublink");
      }
      tent = linitial_node(TargetEntry, qtree->targetList);
      Assert(!tent->resjunk);
      Assert(collation == exprCollation((Node *)tent->expr));
    }
    else
    {
                                                  
      Assert(!OidIsValid(collation));
    }
  }
#endif                          
  break;
  case T_FieldSelect:
    ((FieldSelect *)expr)->resultcollid = collation;
    break;
  case T_FieldStore:
    Assert(!OidIsValid(collation));                                 
    break;
  case T_RelabelType:
    ((RelabelType *)expr)->resultcollid = collation;
    break;
  case T_CoerceViaIO:
    ((CoerceViaIO *)expr)->resultcollid = collation;
    break;
  case T_ArrayCoerceExpr:
    ((ArrayCoerceExpr *)expr)->resultcollid = collation;
    break;
  case T_ConvertRowtypeExpr:
    Assert(!OidIsValid(collation));                                 
    break;
  case T_CaseExpr:
    ((CaseExpr *)expr)->casecollid = collation;
    break;
  case T_ArrayExpr:
    ((ArrayExpr *)expr)->array_collid = collation;
    break;
  case T_RowExpr:
    Assert(!OidIsValid(collation));                                 
    break;
  case T_RowCompareExpr:
    Assert(!OidIsValid(collation));                               
    break;
  case T_CoalesceExpr:
    ((CoalesceExpr *)expr)->coalescecollid = collation;
    break;
  case T_MinMaxExpr:
    ((MinMaxExpr *)expr)->minmaxcollid = collation;
    break;
  case T_SQLValueFunction:
    Assert((((SQLValueFunction *)expr)->type == NAMEOID) ? (collation == C_COLLATION_OID) : (collation == InvalidOid));
    break;
  case T_XmlExpr:
    Assert((((XmlExpr *)expr)->op == IS_XMLSERIALIZE) ? (collation == DEFAULT_COLLATION_OID) : (collation == InvalidOid));
    break;
  case T_NullTest:
    Assert(!OidIsValid(collation));                               
    break;
  case T_BooleanTest:
    Assert(!OidIsValid(collation));                               
    break;
  case T_CoerceToDomain:
    ((CoerceToDomain *)expr)->resultcollid = collation;
    break;
  case T_CoerceToDomainValue:
    ((CoerceToDomainValue *)expr)->collation = collation;
    break;
  case T_SetToDefault:
    ((SetToDefault *)expr)->collation = collation;
    break;
  case T_CurrentOfExpr:
    Assert(!OidIsValid(collation));                               
    break;
  case T_NextValueExpr:
    Assert(!OidIsValid(collation));                                
                                              
    break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(expr));
    break;
  }
}

   
                           
                                                                    
   
                                                                          
                                                                       
                                           
   
void
exprSetInputCollation(Node *expr, Oid inputcollation)
{
  switch (nodeTag(expr))
  {
  case T_Aggref:
    ((Aggref *)expr)->inputcollid = inputcollation;
    break;
  case T_WindowFunc:
    ((WindowFunc *)expr)->inputcollid = inputcollation;
    break;
  case T_FuncExpr:
    ((FuncExpr *)expr)->inputcollid = inputcollation;
    break;
  case T_OpExpr:
    ((OpExpr *)expr)->inputcollid = inputcollation;
    break;
  case T_DistinctExpr:
    ((DistinctExpr *)expr)->inputcollid = inputcollation;
    break;
  case T_NullIfExpr:
    ((NullIfExpr *)expr)->inputcollid = inputcollation;
    break;
  case T_ScalarArrayOpExpr:
    ((ScalarArrayOpExpr *)expr)->inputcollid = inputcollation;
    break;
  case T_MinMaxExpr:
    ((MinMaxExpr *)expr)->inputcollid = inputcollation;
    break;
  default:
    break;
  }
}

   
                  
                                                                         
   
                                                       
   
                                                                     
                                                                           
                                                                         
                                                                             
                                                                        
                                                                          
                                                                              
   
                                                                          
                                                                           
                                                                        
                                                                     
   
                                                                            
                                                                           
                                                                           
                                                                       
                                                                            
                                                                            
                                                                         
                                                                            
                                          
   
int
exprLocation(const Node *expr)
{
  int loc;

  if (expr == NULL)
  {
    return -1;
  }
  switch (nodeTag(expr))
  {
  case T_RangeVar:
    loc = ((const RangeVar *)expr)->location;
    break;
  case T_TableFunc:
    loc = ((const TableFunc *)expr)->location;
    break;
  case T_Var:
    loc = ((const Var *)expr)->location;
    break;
  case T_Const:
    loc = ((const Const *)expr)->location;
    break;
  case T_Param:
    loc = ((const Param *)expr)->location;
    break;
  case T_Aggref:
                                                        
    loc = ((const Aggref *)expr)->location;
    break;
  case T_GroupingFunc:
    loc = ((const GroupingFunc *)expr)->location;
    break;
  case T_WindowFunc:
                                                        
    loc = ((const WindowFunc *)expr)->location;
    break;
  case T_SubscriptingRef:
                                                
    loc = exprLocation((Node *)((const SubscriptingRef *)expr)->refexpr);
    break;
  case T_FuncExpr:
  {
    const FuncExpr *fexpr = (const FuncExpr *)expr;

                                                      
    loc = leftmostLoc(fexpr->location, exprLocation((Node *)fexpr->args));
  }
  break;
  case T_NamedArgExpr:
  {
    const NamedArgExpr *na = (const NamedArgExpr *)expr;

                                               
    loc = leftmostLoc(na->location, exprLocation((Node *)na->arg));
  }
  break;
  case T_OpExpr:
  case T_DistinctExpr:                                  
  case T_NullIfExpr:                                    
  {
    const OpExpr *opexpr = (const OpExpr *)expr;

                                                      
    loc = leftmostLoc(opexpr->location, exprLocation((Node *)opexpr->args));
  }
  break;
  case T_ScalarArrayOpExpr:
  {
    const ScalarArrayOpExpr *saopexpr = (const ScalarArrayOpExpr *)expr;

                                                      
    loc = leftmostLoc(saopexpr->location, exprLocation((Node *)saopexpr->args));
  }
  break;
  case T_BoolExpr:
  {
    const BoolExpr *bexpr = (const BoolExpr *)expr;

       
                                                                
                                                              
                                   
       
    loc = leftmostLoc(bexpr->location, exprLocation((Node *)bexpr->args));
  }
  break;
  case T_SubLink:
  {
    const SubLink *sublink = (const SubLink *)expr;

                                                              
    loc = leftmostLoc(exprLocation(sublink->testexpr), sublink->location);
  }
  break;
  case T_FieldSelect:
                                      
    loc = exprLocation((Node *)((const FieldSelect *)expr)->arg);
    break;
  case T_FieldStore:
                                      
    loc = exprLocation((Node *)((const FieldStore *)expr)->arg);
    break;
  case T_RelabelType:
  {
    const RelabelType *rexpr = (const RelabelType *)expr;

                       
    loc = leftmostLoc(rexpr->location, exprLocation((Node *)rexpr->arg));
  }
  break;
  case T_CoerceViaIO:
  {
    const CoerceViaIO *cexpr = (const CoerceViaIO *)expr;

                       
    loc = leftmostLoc(cexpr->location, exprLocation((Node *)cexpr->arg));
  }
  break;
  case T_ArrayCoerceExpr:
  {
    const ArrayCoerceExpr *cexpr = (const ArrayCoerceExpr *)expr;

                       
    loc = leftmostLoc(cexpr->location, exprLocation((Node *)cexpr->arg));
  }
  break;
  case T_ConvertRowtypeExpr:
  {
    const ConvertRowtypeExpr *cexpr = (const ConvertRowtypeExpr *)expr;

                       
    loc = leftmostLoc(cexpr->location, exprLocation((Node *)cexpr->arg));
  }
  break;
  case T_CollateExpr:
                                      
    loc = exprLocation((Node *)((const CollateExpr *)expr)->arg);
    break;
  case T_CaseExpr:
                                                       
    loc = ((const CaseExpr *)expr)->location;
    break;
  case T_CaseWhen:
                                                       
    loc = ((const CaseWhen *)expr)->location;
    break;
  case T_ArrayExpr:
                                                                   
    loc = ((const ArrayExpr *)expr)->location;
    break;
  case T_RowExpr:
                                                                 
    loc = ((const RowExpr *)expr)->location;
    break;
  case T_RowCompareExpr:
                                               
    loc = exprLocation((Node *)((const RowCompareExpr *)expr)->largs);
    break;
  case T_CoalesceExpr:
                                                           
    loc = ((const CoalesceExpr *)expr)->location;
    break;
  case T_MinMaxExpr:
                                                                 
    loc = ((const MinMaxExpr *)expr)->location;
    break;
  case T_SQLValueFunction:
                                                           
    loc = ((const SQLValueFunction *)expr)->location;
    break;
  case T_XmlExpr:
  {
    const XmlExpr *xexpr = (const XmlExpr *)expr;

                                                      
    loc = leftmostLoc(xexpr->location, exprLocation((Node *)xexpr->args));
  }
  break;
  case T_NullTest:
  {
    const NullTest *nexpr = (const NullTest *)expr;

                       
    loc = leftmostLoc(nexpr->location, exprLocation((Node *)nexpr->arg));
  }
  break;
  case T_BooleanTest:
  {
    const BooleanTest *bexpr = (const BooleanTest *)expr;

                       
    loc = leftmostLoc(bexpr->location, exprLocation((Node *)bexpr->arg));
  }
  break;
  case T_CoerceToDomain:
  {
    const CoerceToDomain *cexpr = (const CoerceToDomain *)expr;

                       
    loc = leftmostLoc(cexpr->location, exprLocation((Node *)cexpr->arg));
  }
  break;
  case T_CoerceToDomainValue:
    loc = ((const CoerceToDomainValue *)expr)->location;
    break;
  case T_SetToDefault:
    loc = ((const SetToDefault *)expr)->location;
    break;
  case T_TargetEntry:
                                      
    loc = exprLocation((Node *)((const TargetEntry *)expr)->expr);
    break;
  case T_IntoClause:
                                                                
    loc = exprLocation((Node *)((const IntoClause *)expr)->rel);
    break;
  case T_List:
  {
                                                                  
    ListCell *lc;

    loc = -1;                                        
    foreach (lc, (const List *)expr)
    {
      loc = exprLocation((Node *)lfirst(lc));
      if (loc >= 0)
      {
        break;
      }
    }
  }
  break;
  case T_A_Expr:
  {
    const A_Expr *aexpr = (const A_Expr *)expr;

                                                           
                                                              
    loc = leftmostLoc(aexpr->location, exprLocation(aexpr->lexpr));
  }
  break;
  case T_ColumnRef:
    loc = ((const ColumnRef *)expr)->location;
    break;
  case T_ParamRef:
    loc = ((const ParamRef *)expr)->location;
    break;
  case T_A_Const:
    loc = ((const A_Const *)expr)->location;
    break;
  case T_FuncCall:
  {
    const FuncCall *fc = (const FuncCall *)expr;

                                                      
                                                                 
    loc = leftmostLoc(fc->location, exprLocation((Node *)fc->args));
  }
  break;
  case T_A_ArrayExpr:
                                                                   
    loc = ((const A_ArrayExpr *)expr)->location;
    break;
  case T_ResTarget:
                                                               
    loc = ((const ResTarget *)expr)->location;
    break;
  case T_MultiAssignRef:
    loc = exprLocation(((const MultiAssignRef *)expr)->source);
    break;
  case T_TypeCast:
  {
    const TypeCast *tc = (const TypeCast *)expr;

       
                                                                  
                                                
       
    loc = exprLocation(tc->arg);
    loc = leftmostLoc(loc, tc->typeName->location);
    loc = leftmostLoc(loc, tc->location);
  }
  break;
  case T_CollateClause:
                                      
    loc = exprLocation(((const CollateClause *)expr)->arg);
    break;
  case T_SortBy:
                                                                
    loc = exprLocation(((const SortBy *)expr)->node);
    break;
  case T_WindowDef:
    loc = ((const WindowDef *)expr)->location;
    break;
  case T_RangeTableSample:
    loc = ((const RangeTableSample *)expr)->location;
    break;
  case T_TypeName:
    loc = ((const TypeName *)expr)->location;
    break;
  case T_ColumnDef:
    loc = ((const ColumnDef *)expr)->location;
    break;
  case T_Constraint:
    loc = ((const Constraint *)expr)->location;
    break;
  case T_FunctionParameter:
                                      
    loc = exprLocation((Node *)((const FunctionParameter *)expr)->argType);
    break;
  case T_XmlSerialize:
                                                               
    loc = ((const XmlSerialize *)expr)->location;
    break;
  case T_GroupingSet:
    loc = ((const GroupingSet *)expr)->location;
    break;
  case T_WithClause:
    loc = ((const WithClause *)expr)->location;
    break;
  case T_InferClause:
    loc = ((const InferClause *)expr)->location;
    break;
  case T_OnConflictClause:
    loc = ((const OnConflictClause *)expr)->location;
    break;
  case T_CommonTableExpr:
    loc = ((const CommonTableExpr *)expr)->location;
    break;
  case T_PlaceHolderVar:
                                      
    loc = exprLocation((Node *)((const PlaceHolderVar *)expr)->phexpr);
    break;
  case T_InferenceElem:
                                         
    loc = exprLocation((Node *)((const InferenceElem *)expr)->expr);
    break;
  case T_PartitionElem:
    loc = ((const PartitionElem *)expr)->location;
    break;
  case T_PartitionSpec:
    loc = ((const PartitionSpec *)expr)->location;
    break;
  case T_PartitionBoundSpec:
    loc = ((const PartitionBoundSpec *)expr)->location;
    break;
  case T_PartitionRangeDatum:
    loc = ((const PartitionRangeDatum *)expr)->location;
    break;
  default:
                                                      
    loc = -1;
    break;
  }
  return loc;
}

   
                                          
   
                                                                      
   
static int
leftmostLoc(int loc1, int loc2)
{
  if (loc1 < 0)
  {
    return loc2;
  }
  else if (loc2 < 0)
  {
    return loc1;
  }
  else
  {
    return Min(loc1, loc2);
  }
}

   
                 
                                                                            
                                                                    
   
                                                                       
                                                                           
                      
   
void
fix_opfuncids(Node *node)
{
                                                                  
  fix_opfuncids_walker(node, NULL);
}

static bool
fix_opfuncids_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, OpExpr))
  {
    set_opfuncid((OpExpr *)node);
  }
  else if (IsA(node, DistinctExpr))
  {
    set_opfuncid((OpExpr *)node);                                 
  }
  else if (IsA(node, NullIfExpr))
  {
    set_opfuncid((OpExpr *)node);                                 
  }
  else if (IsA(node, ScalarArrayOpExpr))
  {
    set_sa_opfuncid((ScalarArrayOpExpr *)node);
  }
  return expression_tree_walker(node, fix_opfuncids_walker, context);
}

   
                
                                                        
                                   
   
                                                            
                                      
   
void
set_opfuncid(OpExpr *opexpr)
{
  if (opexpr->opfuncid == InvalidOid)
  {
    opexpr->opfuncid = get_opcode(opexpr->opno);
  }
}

   
                   
                                           
   
void
set_sa_opfuncid(ScalarArrayOpExpr *opexpr)
{
  if (opexpr->opfuncid == InvalidOid)
  {
    opexpr->opfuncid = get_opcode(opexpr->opno);
  }
}

   
                             
                                                                             
   
                                                                            
                                                                          
                                                                          
                                                             
   
                                                                            
                                                                               
                                                                             
                                                                        
   
                                                                          
                                                                           
                                                                          
                                    
   
bool
check_functions_in_node(Node *node, check_function_callback checker, void *context)
{
  switch (nodeTag(node))
  {
  case T_Aggref:
  {
    Aggref *expr = (Aggref *)node;

    if (checker(expr->aggfnoid, context))
    {
      return true;
    }
  }
  break;
  case T_WindowFunc:
  {
    WindowFunc *expr = (WindowFunc *)node;

    if (checker(expr->winfnoid, context))
    {
      return true;
    }
  }
  break;
  case T_FuncExpr:
  {
    FuncExpr *expr = (FuncExpr *)node;

    if (checker(expr->funcid, context))
    {
      return true;
    }
  }
  break;
  case T_OpExpr:
  case T_DistinctExpr:                                  
  case T_NullIfExpr:                                    
  {
    OpExpr *expr = (OpExpr *)node;

                                               
    set_opfuncid(expr);
    if (checker(expr->opfuncid, context))
    {
      return true;
    }
  }
  break;
  case T_ScalarArrayOpExpr:
  {
    ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *)node;

    set_sa_opfuncid(expr);
    if (checker(expr->opfuncid, context))
    {
      return true;
    }
  }
  break;
  case T_CoerceViaIO:
  {
    CoerceViaIO *expr = (CoerceViaIO *)node;
    Oid iofunc;
    Oid typioparam;
    bool typisvarlena;

                                                
    getTypeInputInfo(expr->resulttype, &iofunc, &typioparam);
    if (checker(iofunc, context))
    {
      return true;
    }
                                                
    getTypeOutputInfo(exprType((Node *)expr->arg), &iofunc, &typisvarlena);
    if (checker(iofunc, context))
    {
      return true;
    }
  }
  break;
  case T_RowCompareExpr:
  {
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;
    ListCell *opid;

    foreach (opid, rcexpr->opnos)
    {
      Oid opfuncid = get_opcode(lfirst_oid(opid));

      if (checker(opfuncid, context))
      {
        return true;
      }
    }
  }
  break;
  default:
    break;
  }
  return false;
}

   
                                            
   
                                                                       
                                                                        
                                                                         
                                                                         
                                                                         
                                                                         
                                                                             
                                                                     
                                                                         
   

   
                                                                          
                                                                          
                                                                   
                                           
   
                                                   
     
                      
                   
                                                              
                        
      
                                          
      
                             
      
                                                 
      
                                                      
                                                                      
     
   
                                                                         
                                                                          
                                                                 
                                                                              
                                                                     
                                                                               
                                                    
   
                                                                          
                                                                           
                                                                          
                                                                            
                                                    
   
                                                                      
                                                                            
                                                                              
                                                                               
                                                                           
                                                                         
                                                                            
                            
   
                                                                          
                                                                              
                                                                             
                                                                            
                                                                       
                                                                             
                                                                              
                                                                               
                                                                             
                                                                             
                                           
   
                          
      
                                  
                                                                    
                                            
                                
                    
      
   
                                                                         
                                                                  
   
                                                                          
                                                                               
                                                                              
                                                                              
                                                                             
                                                 
   

bool
expression_tree_walker(Node *node, bool (*walker)(), void *context)
{
  ListCell *temp;

     
                                                                          
                                        
     
                                                                          
                                                                    
                                   
     
  if (node == NULL)
  {
    return false;
  }

                                                                      
  check_stack_depth();

  switch (nodeTag(node))
  {
  case T_Var:
  case T_Const:
  case T_Param:
  case T_CaseTestExpr:
  case T_SQLValueFunction:
  case T_CoerceToDomainValue:
  case T_SetToDefault:
  case T_CurrentOfExpr:
  case T_NextValueExpr:
  case T_RangeTblRef:
  case T_SortGroupClause:
                                                          
    break;
  case T_WithCheckOption:
    return walker(((WithCheckOption *)node)->qual, context);
  case T_Aggref:
  {
    Aggref *expr = (Aggref *)node;

                                  
    if (expression_tree_walker((Node *)expr->aggdirectargs, walker, context))
    {
      return true;
    }
    if (expression_tree_walker((Node *)expr->args, walker, context))
    {
      return true;
    }
    if (expression_tree_walker((Node *)expr->aggorder, walker, context))
    {
      return true;
    }
    if (expression_tree_walker((Node *)expr->aggdistinct, walker, context))
    {
      return true;
    }
    if (walker((Node *)expr->aggfilter, context))
    {
      return true;
    }
  }
  break;
  case T_GroupingFunc:
  {
    GroupingFunc *grouping = (GroupingFunc *)node;

    if (expression_tree_walker((Node *)grouping->args, walker, context))
    {
      return true;
    }
  }
  break;
  case T_WindowFunc:
  {
    WindowFunc *expr = (WindowFunc *)node;

                                  
    if (expression_tree_walker((Node *)expr->args, walker, context))
    {
      return true;
    }
    if (walker((Node *)expr->aggfilter, context))
    {
      return true;
    }
  }
  break;
  case T_SubscriptingRef:
  {
    SubscriptingRef *sbsref = (SubscriptingRef *)node;

                                                                
    if (expression_tree_walker((Node *)sbsref->refupperindexpr, walker, context))
    {
      return true;
    }
    if (expression_tree_walker((Node *)sbsref->reflowerindexpr, walker, context))
    {
      return true;
    }
                                                               
    if (walker(sbsref->refexpr, context))
    {
      return true;
    }

    if (walker(sbsref->refassgnexpr, context))
    {
      return true;
    }
  }
  break;
  case T_FuncExpr:
  {
    FuncExpr *expr = (FuncExpr *)node;

    if (expression_tree_walker((Node *)expr->args, walker, context))
    {
      return true;
    }
  }
  break;
  case T_NamedArgExpr:
    return walker(((NamedArgExpr *)node)->arg, context);
  case T_OpExpr:
  case T_DistinctExpr:                                  
  case T_NullIfExpr:                                    
  {
    OpExpr *expr = (OpExpr *)node;

    if (expression_tree_walker((Node *)expr->args, walker, context))
    {
      return true;
    }
  }
  break;
  case T_ScalarArrayOpExpr:
  {
    ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *)node;

    if (expression_tree_walker((Node *)expr->args, walker, context))
    {
      return true;
    }
  }
  break;
  case T_BoolExpr:
  {
    BoolExpr *expr = (BoolExpr *)node;

    if (expression_tree_walker((Node *)expr->args, walker, context))
    {
      return true;
    }
  }
  break;
  case T_SubLink:
  {
    SubLink *sublink = (SubLink *)node;

    if (walker(sublink->testexpr, context))
    {
      return true;
    }

       
                                                                 
                                                      
       
    return walker(sublink->subselect, context);
  }
  break;
  case T_SubPlan:
  {
    SubPlan *subplan = (SubPlan *)node;

                                                          
    if (walker(subplan->testexpr, context))
    {
      return true;
    }
                                
    if (expression_tree_walker((Node *)subplan->args, walker, context))
    {
      return true;
    }
  }
  break;
  case T_AlternativeSubPlan:
    return walker(((AlternativeSubPlan *)node)->subplans, context);
  case T_FieldSelect:
    return walker(((FieldSelect *)node)->arg, context);
  case T_FieldStore:
  {
    FieldStore *fstore = (FieldStore *)node;

    if (walker(fstore->arg, context))
    {
      return true;
    }
    if (walker(fstore->newvals, context))
    {
      return true;
    }
  }
  break;
  case T_RelabelType:
    return walker(((RelabelType *)node)->arg, context);
  case T_CoerceViaIO:
    return walker(((CoerceViaIO *)node)->arg, context);
  case T_ArrayCoerceExpr:
  {
    ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;

    if (walker(acoerce->arg, context))
    {
      return true;
    }
    if (walker(acoerce->elemexpr, context))
    {
      return true;
    }
  }
  break;
  case T_ConvertRowtypeExpr:
    return walker(((ConvertRowtypeExpr *)node)->arg, context);
  case T_CollateExpr:
    return walker(((CollateExpr *)node)->arg, context);
  case T_CaseExpr:
  {
    CaseExpr *caseexpr = (CaseExpr *)node;

    if (walker(caseexpr->arg, context))
    {
      return true;
    }
                                                               
    foreach (temp, caseexpr->args)
    {
      CaseWhen *when = lfirst_node(CaseWhen, temp);

      if (walker(when->expr, context))
      {
        return true;
      }
      if (walker(when->result, context))
      {
        return true;
      }
    }
    if (walker(caseexpr->defresult, context))
    {
      return true;
    }
  }
  break;
  case T_ArrayExpr:
    return walker(((ArrayExpr *)node)->elements, context);
  case T_RowExpr:
                                           
    return walker(((RowExpr *)node)->args, context);
  case T_RowCompareExpr:
  {
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;

    if (walker(rcexpr->largs, context))
    {
      return true;
    }
    if (walker(rcexpr->rargs, context))
    {
      return true;
    }
  }
  break;
  case T_CoalesceExpr:
    return walker(((CoalesceExpr *)node)->args, context);
  case T_MinMaxExpr:
    return walker(((MinMaxExpr *)node)->args, context);
  case T_XmlExpr:
  {
    XmlExpr *xexpr = (XmlExpr *)node;

    if (walker(xexpr->named_args, context))
    {
      return true;
    }
                                                       
    if (walker(xexpr->args, context))
    {
      return true;
    }
  }
  break;
  case T_NullTest:
    return walker(((NullTest *)node)->arg, context);
  case T_BooleanTest:
    return walker(((BooleanTest *)node)->arg, context);
  case T_CoerceToDomain:
    return walker(((CoerceToDomain *)node)->arg, context);
  case T_TargetEntry:
    return walker(((TargetEntry *)node)->expr, context);
  case T_Query:
                                                           
    break;
  case T_WindowClause:
  {
    WindowClause *wc = (WindowClause *)node;

    if (walker(wc->partitionClause, context))
    {
      return true;
    }
    if (walker(wc->orderClause, context))
    {
      return true;
    }
    if (walker(wc->startOffset, context))
    {
      return true;
    }
    if (walker(wc->endOffset, context))
    {
      return true;
    }
  }
  break;
  case T_CommonTableExpr:
  {
    CommonTableExpr *cte = (CommonTableExpr *)node;

       
                                                            
                                                  
       
    return walker(cte->ctequery, context);
  }
  break;
  case T_List:
    foreach (temp, (List *)node)
    {
      if (walker((Node *)lfirst(temp), context))
      {
        return true;
      }
    }
    break;
  case T_FromExpr:
  {
    FromExpr *from = (FromExpr *)node;

    if (walker(from->fromlist, context))
    {
      return true;
    }
    if (walker(from->quals, context))
    {
      return true;
    }
  }
  break;
  case T_OnConflictExpr:
  {
    OnConflictExpr *onconflict = (OnConflictExpr *)node;

    if (walker((Node *)onconflict->arbiterElems, context))
    {
      return true;
    }
    if (walker(onconflict->arbiterWhere, context))
    {
      return true;
    }
    if (walker(onconflict->onConflictSet, context))
    {
      return true;
    }
    if (walker(onconflict->onConflictWhere, context))
    {
      return true;
    }
    if (walker(onconflict->exclRelTlist, context))
    {
      return true;
    }
  }
  break;
  case T_PartitionPruneStepOp:
  {
    PartitionPruneStepOp *opstep = (PartitionPruneStepOp *)node;

    if (walker((Node *)opstep->exprs, context))
    {
      return true;
    }
  }
  break;
  case T_PartitionPruneStepCombine:
                                
    break;
  case T_JoinExpr:
  {
    JoinExpr *join = (JoinExpr *)node;

    if (walker(join->larg, context))
    {
      return true;
    }
    if (walker(join->rarg, context))
    {
      return true;
    }
    if (walker(join->quals, context))
    {
      return true;
    }

       
                                                          
       
  }
  break;
  case T_SetOperationStmt:
  {
    SetOperationStmt *setop = (SetOperationStmt *)node;

    if (walker(setop->larg, context))
    {
      return true;
    }
    if (walker(setop->rarg, context))
    {
      return true;
    }

                                               
  }
  break;
  case T_IndexClause:
  {
    IndexClause *iclause = (IndexClause *)node;

    if (walker(iclause->rinfo, context))
    {
      return true;
    }
    if (expression_tree_walker((Node *)iclause->indexquals, walker, context))
    {
      return true;
    }
  }
  break;
  case T_PlaceHolderVar:
    return walker(((PlaceHolderVar *)node)->phexpr, context);
  case T_InferenceElem:
    return walker(((InferenceElem *)node)->expr, context);
  case T_AppendRelInfo:
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)node;

    if (expression_tree_walker((Node *)appinfo->translated_vars, walker, context))
    {
      return true;
    }
  }
  break;
  case T_PlaceHolderInfo:
    return walker(((PlaceHolderInfo *)node)->ph_var, context);
  case T_RangeTblFunction:
    return walker(((RangeTblFunction *)node)->funcexpr, context);
  case T_TableSampleClause:
  {
    TableSampleClause *tsc = (TableSampleClause *)node;

    if (expression_tree_walker((Node *)tsc->args, walker, context))
    {
      return true;
    }
    if (walker((Node *)tsc->repeatable, context))
    {
      return true;
    }
  }
  break;
  case T_TableFunc:
  {
    TableFunc *tf = (TableFunc *)node;

    if (walker(tf->ns_uris, context))
    {
      return true;
    }
    if (walker(tf->docexpr, context))
    {
      return true;
    }
    if (walker(tf->rowexpr, context))
    {
      return true;
    }
    if (walker(tf->colexprs, context))
    {
      return true;
    }
    if (walker(tf->coldefexprs, context))
    {
      return true;
    }
  }
  break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
  return false;
}

   
                                                                  
   
                                                                             
                                                                          
                                                                         
                                                                     
                                               
   
                                                                               
                                                                            
                                                                              
                                                                           
                                                              
   
bool
query_tree_walker(Query *query, bool (*walker)(), void *context, int flags)
{
  Assert(query != NULL && IsA(query, Query));

     
                                                                         
                                                                         
                                                                           
                       
     

  if (walker((Node *)query->targetList, context))
  {
    return true;
  }
  if (walker((Node *)query->withCheckOptions, context))
  {
    return true;
  }
  if (walker((Node *)query->onConflict, context))
  {
    return true;
  }
  if (walker((Node *)query->returningList, context))
  {
    return true;
  }
  if (walker((Node *)query->jointree, context))
  {
    return true;
  }
  if (walker(query->setOperations, context))
  {
    return true;
  }
  if (walker(query->havingQual, context))
  {
    return true;
  }
  if (walker(query->limitOffset, context))
  {
    return true;
  }
  if (walker(query->limitCount, context))
  {
    return true;
  }

     
                                                                         
                                                                          
                                              
     
  if ((flags & QTW_EXAMINE_SORTGROUP))
  {
    if (walker((Node *)query->groupClause, context))
    {
      return true;
    }
    if (walker((Node *)query->windowClause, context))
    {
      return true;
    }
    if (walker((Node *)query->sortClause, context))
    {
      return true;
    }
    if (walker((Node *)query->distinctClause, context))
    {
      return true;
    }
  }
  else
  {
       
                                                                         
                                                         
       
    ListCell *lc;

    foreach (lc, query->windowClause)
    {
      WindowClause *wc = lfirst_node(WindowClause, lc);

      if (walker(wc->startOffset, context))
      {
        return true;
      }
      if (walker(wc->endOffset, context))
      {
        return true;
      }
    }
  }

     
                                               
     
                                                                     
                                                                 
                                                                           
                                          
     
                                                                             
                                                                           
     

  if (!(flags & QTW_IGNORE_CTE_SUBQUERIES))
  {
    if (walker((Node *)query->cteList, context))
    {
      return true;
    }
  }
  if (!(flags & QTW_IGNORE_RANGE_TABLE))
  {
    if (range_table_walker(query->rtable, walker, context, flags))
    {
      return true;
    }
  }
  return false;
}

   
                                                                       
                                                                      
            
   
bool
range_table_walker(List *rtable, bool (*walker)(), void *context, int flags)
{
  ListCell *rt;

  foreach (rt, rtable)
  {
    RangeTblEntry *rte = lfirst_node(RangeTblEntry, rt);

    if (range_table_entry_walker(rte, walker, context, flags))
    {
      return true;
    }
  }
  return false;
}

   
                                                                      
   
bool
range_table_entry_walker(RangeTblEntry *rte, bool (*walker)(), void *context, int flags)
{
     
                                                                        
                                                                            
                                                                         
     
  if (flags & QTW_EXAMINE_RTES_BEFORE)
  {
    if (walker(rte, context))
    {
      return true;
    }
  }

  switch (rte->rtekind)
  {
  case RTE_RELATION:
    if (walker(rte->tablesample, context))
    {
      return true;
    }
    break;
  case RTE_SUBQUERY:
    if (!(flags & QTW_IGNORE_RT_SUBQUERIES))
    {
      if (walker(rte->subquery, context))
      {
        return true;
      }
    }
    break;
  case RTE_JOIN:
    if (!(flags & QTW_IGNORE_JOINALIASES))
    {
      if (walker(rte->joinaliasvars, context))
      {
        return true;
      }
    }
    break;
  case RTE_FUNCTION:
    if (walker(rte->functions, context))
    {
      return true;
    }
    break;
  case RTE_TABLEFUNC:
    if (walker(rte->tablefunc, context))
    {
      return true;
    }
    break;
  case RTE_VALUES:
    if (walker(rte->values_lists, context))
    {
      return true;
    }
    break;
  case RTE_CTE:
  case RTE_NAMEDTUPLESTORE:
  case RTE_RESULT:
                       
    break;
  }

  if (walker(rte->securityQuals, context))
  {
    return true;
  }

  if (flags & QTW_EXAMINE_RTES_AFTER)
  {
    if (walker(rte, context))
    {
      return true;
    }
  }

  return false;
}

   
                                                                         
                                                                     
                                                                          
                                                                             
                                                                        
                                            
   
                                                      
     
                      
                  
                                                              
                        
      
                                                     
      
                             
      
                                                        
      
                                                      
                                                                        
     
   
                                                                         
                                                                            
                                                                       
                                                                               
                                                                      
                                                                      
                                                                      
   
                                                                       
                                                                      
                                                                      
                                                                      
                                                                    
                               
   
                                                                 
                                                                            
                                                    
   
                                                                           
                                                                              
                                                                              
                                                                             
                                                                             
                                                                           
                                                                             
                                                                              
                                                                            
   
                                                                            
                                                                           
                                                                            
                                                                              
                                                                          
                              
   

Node *
expression_tree_mutator(Node *node, Node *(*mutator)(), void *context)
{
     
                                                                            
                                              
     

#define FLATCOPY(newnode, node, nodetype) ((newnode) = (nodetype *)palloc(sizeof(nodetype)), memcpy((newnode), (node), sizeof(nodetype)))

#define CHECKFLATCOPY(newnode, node, nodetype) (AssertMacro(IsA((node), nodetype)), (newnode) = (nodetype *)palloc(sizeof(nodetype)), memcpy((newnode), (node), sizeof(nodetype)))

#define MUTATE(newfield, oldfield, fieldtype) ((newfield) = (fieldtype)mutator((Node *)(oldfield), context))

  if (node == NULL)
  {
    return NULL;
  }

                                                                      
  check_stack_depth();

  switch (nodeTag(node))
  {
       
                                                                  
                                                                      
                                   
       
  case T_Var:
  {
    Var *var = (Var *)node;
    Var *newnode;

    FLATCOPY(newnode, var, Var);
    return (Node *)newnode;
  }
  break;
  case T_Const:
  {
    Const *oldnode = (Const *)node;
    Const *newnode;

    FLATCOPY(newnode, oldnode, Const);
                                                        
    return (Node *)newnode;
  }
  break;
  case T_Param:
  case T_CaseTestExpr:
  case T_SQLValueFunction:
  case T_CoerceToDomainValue:
  case T_SetToDefault:
  case T_CurrentOfExpr:
  case T_NextValueExpr:
  case T_RangeTblRef:
  case T_SortGroupClause:
    return (Node *)copyObject(node);
  case T_WithCheckOption:
  {
    WithCheckOption *wco = (WithCheckOption *)node;
    WithCheckOption *newnode;

    FLATCOPY(newnode, wco, WithCheckOption);
    MUTATE(newnode->qual, wco->qual, Node *);
    return (Node *)newnode;
  }
  case T_Aggref:
  {
    Aggref *aggref = (Aggref *)node;
    Aggref *newnode;

    FLATCOPY(newnode, aggref, Aggref);
                                                           
    newnode->aggargtypes = list_copy(aggref->aggargtypes);
    MUTATE(newnode->aggdirectargs, aggref->aggdirectargs, List *);
    MUTATE(newnode->args, aggref->args, List *);
    MUTATE(newnode->aggorder, aggref->aggorder, List *);
    MUTATE(newnode->aggdistinct, aggref->aggdistinct, List *);
    MUTATE(newnode->aggfilter, aggref->aggfilter, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_GroupingFunc:
  {
    GroupingFunc *grouping = (GroupingFunc *)node;
    GroupingFunc *newnode;

    FLATCOPY(newnode, grouping, GroupingFunc);
    MUTATE(newnode->args, grouping->args, List *);

       
                                                                  
                                                                   
                                                             
                                                               
       
                                                                
                                                             
       
    newnode->refs = list_copy(grouping->refs);
    newnode->cols = list_copy(grouping->cols);

    return (Node *)newnode;
  }
  break;
  case T_WindowFunc:
  {
    WindowFunc *wfunc = (WindowFunc *)node;
    WindowFunc *newnode;

    FLATCOPY(newnode, wfunc, WindowFunc);
    MUTATE(newnode->args, wfunc->args, List *);
    MUTATE(newnode->aggfilter, wfunc->aggfilter, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_SubscriptingRef:
  {
    SubscriptingRef *sbsref = (SubscriptingRef *)node;
    SubscriptingRef *newnode;

    FLATCOPY(newnode, sbsref, SubscriptingRef);
    MUTATE(newnode->refupperindexpr, sbsref->refupperindexpr, List *);
    MUTATE(newnode->reflowerindexpr, sbsref->reflowerindexpr, List *);
    MUTATE(newnode->refexpr, sbsref->refexpr, Expr *);
    MUTATE(newnode->refassgnexpr, sbsref->refassgnexpr, Expr *);

    return (Node *)newnode;
  }
  break;
  case T_FuncExpr:
  {
    FuncExpr *expr = (FuncExpr *)node;
    FuncExpr *newnode;

    FLATCOPY(newnode, expr, FuncExpr);
    MUTATE(newnode->args, expr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_NamedArgExpr:
  {
    NamedArgExpr *nexpr = (NamedArgExpr *)node;
    NamedArgExpr *newnode;

    FLATCOPY(newnode, nexpr, NamedArgExpr);
    MUTATE(newnode->arg, nexpr->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_OpExpr:
  {
    OpExpr *expr = (OpExpr *)node;
    OpExpr *newnode;

    FLATCOPY(newnode, expr, OpExpr);
    MUTATE(newnode->args, expr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_DistinctExpr:
  {
    DistinctExpr *expr = (DistinctExpr *)node;
    DistinctExpr *newnode;

    FLATCOPY(newnode, expr, DistinctExpr);
    MUTATE(newnode->args, expr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_NullIfExpr:
  {
    NullIfExpr *expr = (NullIfExpr *)node;
    NullIfExpr *newnode;

    FLATCOPY(newnode, expr, NullIfExpr);
    MUTATE(newnode->args, expr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_ScalarArrayOpExpr:
  {
    ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *)node;
    ScalarArrayOpExpr *newnode;

    FLATCOPY(newnode, expr, ScalarArrayOpExpr);
    MUTATE(newnode->args, expr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_BoolExpr:
  {
    BoolExpr *expr = (BoolExpr *)node;
    BoolExpr *newnode;

    FLATCOPY(newnode, expr, BoolExpr);
    MUTATE(newnode->args, expr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_SubLink:
  {
    SubLink *sublink = (SubLink *)node;
    SubLink *newnode;

    FLATCOPY(newnode, sublink, SubLink);
    MUTATE(newnode->testexpr, sublink->testexpr, Node *);

       
                                                                  
                                                      
       
    MUTATE(newnode->subselect, sublink->subselect, Node *);
    return (Node *)newnode;
  }
  break;
  case T_SubPlan:
  {
    SubPlan *subplan = (SubPlan *)node;
    SubPlan *newnode;

    FLATCOPY(newnode, subplan, SubPlan);
                            
    MUTATE(newnode->testexpr, subplan->testexpr, Node *);
                                                              
    MUTATE(newnode->args, subplan->args, List *);
                                                                
    return (Node *)newnode;
  }
  break;
  case T_AlternativeSubPlan:
  {
    AlternativeSubPlan *asplan = (AlternativeSubPlan *)node;
    AlternativeSubPlan *newnode;

    FLATCOPY(newnode, asplan, AlternativeSubPlan);
    MUTATE(newnode->subplans, asplan->subplans, List *);
    return (Node *)newnode;
  }
  break;
  case T_FieldSelect:
  {
    FieldSelect *fselect = (FieldSelect *)node;
    FieldSelect *newnode;

    FLATCOPY(newnode, fselect, FieldSelect);
    MUTATE(newnode->arg, fselect->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_FieldStore:
  {
    FieldStore *fstore = (FieldStore *)node;
    FieldStore *newnode;

    FLATCOPY(newnode, fstore, FieldStore);
    MUTATE(newnode->arg, fstore->arg, Expr *);
    MUTATE(newnode->newvals, fstore->newvals, List *);
    newnode->fieldnums = list_copy(fstore->fieldnums);
    return (Node *)newnode;
  }
  break;
  case T_RelabelType:
  {
    RelabelType *relabel = (RelabelType *)node;
    RelabelType *newnode;

    FLATCOPY(newnode, relabel, RelabelType);
    MUTATE(newnode->arg, relabel->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_CoerceViaIO:
  {
    CoerceViaIO *iocoerce = (CoerceViaIO *)node;
    CoerceViaIO *newnode;

    FLATCOPY(newnode, iocoerce, CoerceViaIO);
    MUTATE(newnode->arg, iocoerce->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_ArrayCoerceExpr:
  {
    ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;
    ArrayCoerceExpr *newnode;

    FLATCOPY(newnode, acoerce, ArrayCoerceExpr);
    MUTATE(newnode->arg, acoerce->arg, Expr *);
    MUTATE(newnode->elemexpr, acoerce->elemexpr, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_ConvertRowtypeExpr:
  {
    ConvertRowtypeExpr *convexpr = (ConvertRowtypeExpr *)node;
    ConvertRowtypeExpr *newnode;

    FLATCOPY(newnode, convexpr, ConvertRowtypeExpr);
    MUTATE(newnode->arg, convexpr->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_CollateExpr:
  {
    CollateExpr *collate = (CollateExpr *)node;
    CollateExpr *newnode;

    FLATCOPY(newnode, collate, CollateExpr);
    MUTATE(newnode->arg, collate->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_CaseExpr:
  {
    CaseExpr *caseexpr = (CaseExpr *)node;
    CaseExpr *newnode;

    FLATCOPY(newnode, caseexpr, CaseExpr);
    MUTATE(newnode->arg, caseexpr->arg, Expr *);
    MUTATE(newnode->args, caseexpr->args, List *);
    MUTATE(newnode->defresult, caseexpr->defresult, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_CaseWhen:
  {
    CaseWhen *casewhen = (CaseWhen *)node;
    CaseWhen *newnode;

    FLATCOPY(newnode, casewhen, CaseWhen);
    MUTATE(newnode->expr, casewhen->expr, Expr *);
    MUTATE(newnode->result, casewhen->result, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_ArrayExpr:
  {
    ArrayExpr *arrayexpr = (ArrayExpr *)node;
    ArrayExpr *newnode;

    FLATCOPY(newnode, arrayexpr, ArrayExpr);
    MUTATE(newnode->elements, arrayexpr->elements, List *);
    return (Node *)newnode;
  }
  break;
  case T_RowExpr:
  {
    RowExpr *rowexpr = (RowExpr *)node;
    RowExpr *newnode;

    FLATCOPY(newnode, rowexpr, RowExpr);
    MUTATE(newnode->args, rowexpr->args, List *);
                                               
    return (Node *)newnode;
  }
  break;
  case T_RowCompareExpr:
  {
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;
    RowCompareExpr *newnode;

    FLATCOPY(newnode, rcexpr, RowCompareExpr);
    MUTATE(newnode->largs, rcexpr->largs, List *);
    MUTATE(newnode->rargs, rcexpr->rargs, List *);
    return (Node *)newnode;
  }
  break;
  case T_CoalesceExpr:
  {
    CoalesceExpr *coalesceexpr = (CoalesceExpr *)node;
    CoalesceExpr *newnode;

    FLATCOPY(newnode, coalesceexpr, CoalesceExpr);
    MUTATE(newnode->args, coalesceexpr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_MinMaxExpr:
  {
    MinMaxExpr *minmaxexpr = (MinMaxExpr *)node;
    MinMaxExpr *newnode;

    FLATCOPY(newnode, minmaxexpr, MinMaxExpr);
    MUTATE(newnode->args, minmaxexpr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_XmlExpr:
  {
    XmlExpr *xexpr = (XmlExpr *)node;
    XmlExpr *newnode;

    FLATCOPY(newnode, xexpr, XmlExpr);
    MUTATE(newnode->named_args, xexpr->named_args, List *);
                                                      
    MUTATE(newnode->args, xexpr->args, List *);
    return (Node *)newnode;
  }
  break;
  case T_NullTest:
  {
    NullTest *ntest = (NullTest *)node;
    NullTest *newnode;

    FLATCOPY(newnode, ntest, NullTest);
    MUTATE(newnode->arg, ntest->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_BooleanTest:
  {
    BooleanTest *btest = (BooleanTest *)node;
    BooleanTest *newnode;

    FLATCOPY(newnode, btest, BooleanTest);
    MUTATE(newnode->arg, btest->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_CoerceToDomain:
  {
    CoerceToDomain *ctest = (CoerceToDomain *)node;
    CoerceToDomain *newnode;

    FLATCOPY(newnode, ctest, CoerceToDomain);
    MUTATE(newnode->arg, ctest->arg, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_TargetEntry:
  {
    TargetEntry *targetentry = (TargetEntry *)node;
    TargetEntry *newnode;

    FLATCOPY(newnode, targetentry, TargetEntry);
    MUTATE(newnode->expr, targetentry->expr, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_Query:
                                                           
    return node;
  case T_WindowClause:
  {
    WindowClause *wc = (WindowClause *)node;
    WindowClause *newnode;

    FLATCOPY(newnode, wc, WindowClause);
    MUTATE(newnode->partitionClause, wc->partitionClause, List *);
    MUTATE(newnode->orderClause, wc->orderClause, List *);
    MUTATE(newnode->startOffset, wc->startOffset, Node *);
    MUTATE(newnode->endOffset, wc->endOffset, Node *);
    return (Node *)newnode;
  }
  break;
  case T_CommonTableExpr:
  {
    CommonTableExpr *cte = (CommonTableExpr *)node;
    CommonTableExpr *newnode;

    FLATCOPY(newnode, cte, CommonTableExpr);

       
                                                                  
                                                  
       
    MUTATE(newnode->ctequery, cte->ctequery, Node *);
    return (Node *)newnode;
  }
  break;
  case T_List:
  {
       
                                                                
                                                                  
                                                         
       
    List *resultlist;
    ListCell *temp;

    resultlist = NIL;
    foreach (temp, (List *)node)
    {
      resultlist = lappend(resultlist, mutator((Node *)lfirst(temp), context));
    }
    return (Node *)resultlist;
  }
  break;
  case T_FromExpr:
  {
    FromExpr *from = (FromExpr *)node;
    FromExpr *newnode;

    FLATCOPY(newnode, from, FromExpr);
    MUTATE(newnode->fromlist, from->fromlist, List *);
    MUTATE(newnode->quals, from->quals, Node *);
    return (Node *)newnode;
  }
  break;
  case T_OnConflictExpr:
  {
    OnConflictExpr *oc = (OnConflictExpr *)node;
    OnConflictExpr *newnode;

    FLATCOPY(newnode, oc, OnConflictExpr);
    MUTATE(newnode->arbiterElems, oc->arbiterElems, List *);
    MUTATE(newnode->arbiterWhere, oc->arbiterWhere, Node *);
    MUTATE(newnode->onConflictSet, oc->onConflictSet, List *);
    MUTATE(newnode->onConflictWhere, oc->onConflictWhere, Node *);
    MUTATE(newnode->exclRelTlist, oc->exclRelTlist, List *);

    return (Node *)newnode;
  }
  break;
  case T_PartitionPruneStepOp:
  {
    PartitionPruneStepOp *opstep = (PartitionPruneStepOp *)node;
    PartitionPruneStepOp *newnode;

    FLATCOPY(newnode, opstep, PartitionPruneStepOp);
    MUTATE(newnode->exprs, opstep->exprs, List *);

    return (Node *)newnode;
  }
  break;
  case T_PartitionPruneStepCombine:
                                 
    return (Node *)copyObject(node);
  case T_JoinExpr:
  {
    JoinExpr *join = (JoinExpr *)node;
    JoinExpr *newnode;

    FLATCOPY(newnode, join, JoinExpr);
    MUTATE(newnode->larg, join->larg, Node *);
    MUTATE(newnode->rarg, join->rarg, Node *);
    MUTATE(newnode->quals, join->quals, Node *);
                                                    
    return (Node *)newnode;
  }
  break;
  case T_SetOperationStmt:
  {
    SetOperationStmt *setop = (SetOperationStmt *)node;
    SetOperationStmt *newnode;

    FLATCOPY(newnode, setop, SetOperationStmt);
    MUTATE(newnode->larg, setop->larg, Node *);
    MUTATE(newnode->rarg, setop->rarg, Node *);
                                                  
    return (Node *)newnode;
  }
  break;
  case T_IndexClause:
  {
    IndexClause *iclause = (IndexClause *)node;
    IndexClause *newnode;

    FLATCOPY(newnode, iclause, IndexClause);
    MUTATE(newnode->rinfo, iclause->rinfo, RestrictInfo *);
    MUTATE(newnode->indexquals, iclause->indexquals, List *);
    return (Node *)newnode;
  }
  break;
  case T_PlaceHolderVar:
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;
    PlaceHolderVar *newnode;

    FLATCOPY(newnode, phv, PlaceHolderVar);
    MUTATE(newnode->phexpr, phv->phexpr, Expr *);
                                                      
    return (Node *)newnode;
  }
  break;
  case T_InferenceElem:
  {
    InferenceElem *inferenceelemdexpr = (InferenceElem *)node;
    InferenceElem *newnode;

    FLATCOPY(newnode, inferenceelemdexpr, InferenceElem);
    MUTATE(newnode->expr, newnode->expr, Node *);
    return (Node *)newnode;
  }
  break;
  case T_AppendRelInfo:
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)node;
    AppendRelInfo *newnode;

    FLATCOPY(newnode, appinfo, AppendRelInfo);
    MUTATE(newnode->translated_vars, appinfo->translated_vars, List *);
    return (Node *)newnode;
  }
  break;
  case T_PlaceHolderInfo:
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)node;
    PlaceHolderInfo *newnode;

    FLATCOPY(newnode, phinfo, PlaceHolderInfo);
    MUTATE(newnode->ph_var, phinfo->ph_var, PlaceHolderVar *);
                                                       
    return (Node *)newnode;
  }
  break;
  case T_RangeTblFunction:
  {
    RangeTblFunction *rtfunc = (RangeTblFunction *)node;
    RangeTblFunction *newnode;

    FLATCOPY(newnode, rtfunc, RangeTblFunction);
    MUTATE(newnode->funcexpr, rtfunc->funcexpr, Node *);
                                                       
    return (Node *)newnode;
  }
  break;
  case T_TableSampleClause:
  {
    TableSampleClause *tsc = (TableSampleClause *)node;
    TableSampleClause *newnode;

    FLATCOPY(newnode, tsc, TableSampleClause);
    MUTATE(newnode->args, tsc->args, List *);
    MUTATE(newnode->repeatable, tsc->repeatable, Expr *);
    return (Node *)newnode;
  }
  break;
  case T_TableFunc:
  {
    TableFunc *tf = (TableFunc *)node;
    TableFunc *newnode;

    FLATCOPY(newnode, tf, TableFunc);
    MUTATE(newnode->ns_uris, tf->ns_uris, List *);
    MUTATE(newnode->docexpr, tf->docexpr, Node *);
    MUTATE(newnode->rowexpr, tf->rowexpr, Node *);
    MUTATE(newnode->colexprs, tf->colexprs, List *);
    MUTATE(newnode->coldefexprs, tf->coldefexprs, List *);
    return (Node *)newnode;
  }
  break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
                                               
  return NULL;
}

   
                                                                         
   
                                                                             
                                                                          
                                                                         
                                                                      
                                                
   
                                                                         
                                                                            
                                                                              
                                                                  
                                                              
   
                                                                            
                                                                        
                                                       
   
Query *
query_tree_mutator(Query *query, Node *(*mutator)(), void *context, int flags)
{
  Assert(query != NULL && IsA(query, Query));

  if (!(flags & QTW_DONT_COPY_QUERY))
  {
    Query *newquery;

    FLATCOPY(newquery, query, Query);
    query = newquery;
  }

  MUTATE(query->targetList, query->targetList, List *);
  MUTATE(query->withCheckOptions, query->withCheckOptions, List *);
  MUTATE(query->onConflict, query->onConflict, OnConflictExpr *);
  MUTATE(query->returningList, query->returningList, List *);
  MUTATE(query->jointree, query->jointree, FromExpr *);
  MUTATE(query->setOperations, query->setOperations, Node *);
  MUTATE(query->havingQual, query->havingQual, Node *);
  MUTATE(query->limitOffset, query->limitOffset, Node *);
  MUTATE(query->limitCount, query->limitCount, Node *);

     
                                                                         
                                                                           
                                          
     

  if ((flags & QTW_EXAMINE_SORTGROUP))
  {
    MUTATE(query->groupClause, query->groupClause, List *);
    MUTATE(query->windowClause, query->windowClause, List *);
    MUTATE(query->sortClause, query->sortClause, List *);
    MUTATE(query->distinctClause, query->distinctClause, List *);
  }
  else
  {
       
                                                                           
                                                         
       
    List *resultlist;
    ListCell *temp;

    resultlist = NIL;
    foreach (temp, query->windowClause)
    {
      WindowClause *wc = lfirst_node(WindowClause, temp);
      WindowClause *newnode;

      FLATCOPY(newnode, wc, WindowClause);
      MUTATE(newnode->startOffset, wc->startOffset, Node *);
      MUTATE(newnode->endOffset, wc->endOffset, Node *);

      resultlist = lappend(resultlist, (Node *)newnode);
    }
    query->windowClause = resultlist;
  }

     
                                                
     
                                                                      
                                                                            
                                                                            
                 
     
                                                                    
                                                           
     

  if (!(flags & QTW_IGNORE_CTE_SUBQUERIES))
  {
    MUTATE(query->cteList, query->cteList, List *);
  }
  else                               
  {
    query->cteList = copyObject(query->cteList);
  }
  query->rtable = range_table_mutator(query->rtable, mutator, context, flags);
  return query;
}

   
                                                                             
                                                                      
            
   
List *
range_table_mutator(List *rtable, Node *(*mutator)(), void *context, int flags)
{
  List *newrt = NIL;
  ListCell *rt;

  foreach (rt, rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(rt);
    RangeTblEntry *newrte;

    FLATCOPY(newrte, rte, RangeTblEntry);
    switch (rte->rtekind)
    {
    case RTE_RELATION:
      MUTATE(newrte->tablesample, rte->tablesample, TableSampleClause *);
                                                           
      break;
    case RTE_SUBQUERY:
      if (!(flags & QTW_IGNORE_RT_SUBQUERIES))
      {
        CHECKFLATCOPY(newrte->subquery, rte->subquery, Query);
        MUTATE(newrte->subquery, newrte->subquery, Query *);
      }
      else
      {
                                            
        newrte->subquery = copyObject(rte->subquery);
      }
      break;
    case RTE_JOIN:
      if (!(flags & QTW_IGNORE_JOINALIASES))
      {
        MUTATE(newrte->joinaliasvars, rte->joinaliasvars, List *);
      }
      else
      {
                                           
        newrte->joinaliasvars = copyObject(rte->joinaliasvars);
      }
      break;
    case RTE_FUNCTION:
      MUTATE(newrte->functions, rte->functions, List *);
      break;
    case RTE_TABLEFUNC:
      MUTATE(newrte->tablefunc, rte->tablefunc, TableFunc *);
      break;
    case RTE_VALUES:
      MUTATE(newrte->values_lists, rte->values_lists, List *);
      break;
    case RTE_CTE:
    case RTE_NAMEDTUPLESTORE:
    case RTE_RESULT:
                         
      break;
    }
    MUTATE(newrte->securityQuals, rte->securityQuals, List *);
    newrt = lappend(newrt, newrte);
  }
  return newrt;
}

   
                                                   
   
                                                                         
                                                                           
                                                                             
                                 
   
bool
query_or_expression_tree_walker(Node *node, bool (*walker)(), void *context, int flags)
{
  if (node && IsA(node, Query))
  {
    return query_tree_walker((Query *)node, walker, context, flags);
  }
  else
  {
    return walker(node, context);
  }
}

   
                                                    
   
                                                                          
                                                                            
                                                                              
                                 
   
Node *
query_or_expression_tree_mutator(Node *node, Node *(*mutator)(), void *context, int flags)
{
  if (node && IsA(node, Query))
  {
    return (Node *)query_tree_mutator((Query *)node, mutator, context, flags);
  }
  else
  {
    return mutator(node, context);
  }
}

   
                                                       
   
                                                                           
                                                                          
                                                                        
                                                                         
                                                                       
                                                                     
   
                                                                         
                                                                            
                                                                            
                   
   
bool
raw_expression_tree_walker(Node *node, bool (*walker)(), void *context)
{
  ListCell *temp;

     
                                                                          
                                        
     
  if (node == NULL)
  {
    return false;
  }

                                                                      
  check_stack_depth();

  switch (nodeTag(node))
  {
  case T_SetToDefault:
  case T_CurrentOfExpr:
  case T_SQLValueFunction:
  case T_Integer:
  case T_Float:
  case T_String:
  case T_BitString:
  case T_Null:
  case T_ParamRef:
  case T_A_Const:
  case T_A_Star:
                                               
    break;
  case T_Alias:
                                                       
    break;
  case T_RangeVar:
    return walker(((RangeVar *)node)->alias, context);
  case T_GroupingFunc:
    return walker(((GroupingFunc *)node)->args, context);
  case T_SubLink:
  {
    SubLink *sublink = (SubLink *)node;

    if (walker(sublink->testexpr, context))
    {
      return true;
    }
                                                   
    if (walker(sublink->subselect, context))
    {
      return true;
    }
  }
  break;
  case T_CaseExpr:
  {
    CaseExpr *caseexpr = (CaseExpr *)node;

    if (walker(caseexpr->arg, context))
    {
      return true;
    }
                                                               
    foreach (temp, caseexpr->args)
    {
      CaseWhen *when = lfirst_node(CaseWhen, temp);

      if (walker(when->expr, context))
      {
        return true;
      }
      if (walker(when->result, context))
      {
        return true;
      }
    }
    if (walker(caseexpr->defresult, context))
    {
      return true;
    }
  }
  break;
  case T_RowExpr:
                                           
    return walker(((RowExpr *)node)->args, context);
  case T_CoalesceExpr:
    return walker(((CoalesceExpr *)node)->args, context);
  case T_MinMaxExpr:
    return walker(((MinMaxExpr *)node)->args, context);
  case T_XmlExpr:
  {
    XmlExpr *xexpr = (XmlExpr *)node;

    if (walker(xexpr->named_args, context))
    {
      return true;
    }
                                                       
    if (walker(xexpr->args, context))
    {
      return true;
    }
  }
  break;
  case T_NullTest:
    return walker(((NullTest *)node)->arg, context);
  case T_BooleanTest:
    return walker(((BooleanTest *)node)->arg, context);
  case T_JoinExpr:
  {
    JoinExpr *join = (JoinExpr *)node;

    if (walker(join->larg, context))
    {
      return true;
    }
    if (walker(join->rarg, context))
    {
      return true;
    }
    if (walker(join->quals, context))
    {
      return true;
    }
    if (walker(join->alias, context))
    {
      return true;
    }
                                            
  }
  break;
  case T_IntoClause:
  {
    IntoClause *into = (IntoClause *)node;

    if (walker(into->rel, context))
    {
      return true;
    }
                                                    
                                                                 
    if (walker(into->viewQuery, context))
    {
      return true;
    }
  }
  break;
  case T_List:
    foreach (temp, (List *)node)
    {
      if (walker((Node *)lfirst(temp), context))
      {
        return true;
      }
    }
    break;
  case T_InsertStmt:
  {
    InsertStmt *stmt = (InsertStmt *)node;

    if (walker(stmt->relation, context))
    {
      return true;
    }
    if (walker(stmt->cols, context))
    {
      return true;
    }
    if (walker(stmt->selectStmt, context))
    {
      return true;
    }
    if (walker(stmt->onConflictClause, context))
    {
      return true;
    }
    if (walker(stmt->returningList, context))
    {
      return true;
    }
    if (walker(stmt->withClause, context))
    {
      return true;
    }
  }
  break;
  case T_DeleteStmt:
  {
    DeleteStmt *stmt = (DeleteStmt *)node;

    if (walker(stmt->relation, context))
    {
      return true;
    }
    if (walker(stmt->usingClause, context))
    {
      return true;
    }
    if (walker(stmt->whereClause, context))
    {
      return true;
    }
    if (walker(stmt->returningList, context))
    {
      return true;
    }
    if (walker(stmt->withClause, context))
    {
      return true;
    }
  }
  break;
  case T_UpdateStmt:
  {
    UpdateStmt *stmt = (UpdateStmt *)node;

    if (walker(stmt->relation, context))
    {
      return true;
    }
    if (walker(stmt->targetList, context))
    {
      return true;
    }
    if (walker(stmt->whereClause, context))
    {
      return true;
    }
    if (walker(stmt->fromClause, context))
    {
      return true;
    }
    if (walker(stmt->returningList, context))
    {
      return true;
    }
    if (walker(stmt->withClause, context))
    {
      return true;
    }
  }
  break;
  case T_SelectStmt:
  {
    SelectStmt *stmt = (SelectStmt *)node;

    if (walker(stmt->distinctClause, context))
    {
      return true;
    }
    if (walker(stmt->intoClause, context))
    {
      return true;
    }
    if (walker(stmt->targetList, context))
    {
      return true;
    }
    if (walker(stmt->fromClause, context))
    {
      return true;
    }
    if (walker(stmt->whereClause, context))
    {
      return true;
    }
    if (walker(stmt->groupClause, context))
    {
      return true;
    }
    if (walker(stmt->havingClause, context))
    {
      return true;
    }
    if (walker(stmt->windowClause, context))
    {
      return true;
    }
    if (walker(stmt->valuesLists, context))
    {
      return true;
    }
    if (walker(stmt->sortClause, context))
    {
      return true;
    }
    if (walker(stmt->limitOffset, context))
    {
      return true;
    }
    if (walker(stmt->limitCount, context))
    {
      return true;
    }
    if (walker(stmt->lockingClause, context))
    {
      return true;
    }
    if (walker(stmt->withClause, context))
    {
      return true;
    }
    if (walker(stmt->larg, context))
    {
      return true;
    }
    if (walker(stmt->rarg, context))
    {
      return true;
    }
  }
  break;
  case T_A_Expr:
  {
    A_Expr *expr = (A_Expr *)node;

    if (walker(expr->lexpr, context))
    {
      return true;
    }
    if (walker(expr->rexpr, context))
    {
      return true;
    }
                                               
  }
  break;
  case T_BoolExpr:
  {
    BoolExpr *expr = (BoolExpr *)node;

    if (walker(expr->args, context))
    {
      return true;
    }
  }
  break;
  case T_ColumnRef:
                                                          
    break;
  case T_FuncCall:
  {
    FuncCall *fcall = (FuncCall *)node;

    if (walker(fcall->args, context))
    {
      return true;
    }
    if (walker(fcall->agg_order, context))
    {
      return true;
    }
    if (walker(fcall->agg_filter, context))
    {
      return true;
    }
    if (walker(fcall->over, context))
    {
      return true;
    }
                                               
  }
  break;
  case T_NamedArgExpr:
    return walker(((NamedArgExpr *)node)->arg, context);
  case T_A_Indices:
  {
    A_Indices *indices = (A_Indices *)node;

    if (walker(indices->lidx, context))
    {
      return true;
    }
    if (walker(indices->uidx, context))
    {
      return true;
    }
  }
  break;
  case T_A_Indirection:
  {
    A_Indirection *indir = (A_Indirection *)node;

    if (walker(indir->arg, context))
    {
      return true;
    }
    if (walker(indir->indirection, context))
    {
      return true;
    }
  }
  break;
  case T_A_ArrayExpr:
    return walker(((A_ArrayExpr *)node)->elements, context);
  case T_ResTarget:
  {
    ResTarget *rt = (ResTarget *)node;

    if (walker(rt->indirection, context))
    {
      return true;
    }
    if (walker(rt->val, context))
    {
      return true;
    }
  }
  break;
  case T_MultiAssignRef:
    return walker(((MultiAssignRef *)node)->source, context);
  case T_TypeCast:
  {
    TypeCast *tc = (TypeCast *)node;

    if (walker(tc->arg, context))
    {
      return true;
    }
    if (walker(tc->typeName, context))
    {
      return true;
    }
  }
  break;
  case T_CollateClause:
    return walker(((CollateClause *)node)->arg, context);
  case T_SortBy:
    return walker(((SortBy *)node)->node, context);
  case T_WindowDef:
  {
    WindowDef *wd = (WindowDef *)node;

    if (walker(wd->partitionClause, context))
    {
      return true;
    }
    if (walker(wd->orderClause, context))
    {
      return true;
    }
    if (walker(wd->startOffset, context))
    {
      return true;
    }
    if (walker(wd->endOffset, context))
    {
      return true;
    }
  }
  break;
  case T_RangeSubselect:
  {
    RangeSubselect *rs = (RangeSubselect *)node;

    if (walker(rs->subquery, context))
    {
      return true;
    }
    if (walker(rs->alias, context))
    {
      return true;
    }
  }
  break;
  case T_RangeFunction:
  {
    RangeFunction *rf = (RangeFunction *)node;

    if (walker(rf->functions, context))
    {
      return true;
    }
    if (walker(rf->alias, context))
    {
      return true;
    }
    if (walker(rf->coldeflist, context))
    {
      return true;
    }
  }
  break;
  case T_RangeTableSample:
  {
    RangeTableSample *rts = (RangeTableSample *)node;

    if (walker(rts->relation, context))
    {
      return true;
    }
                                             
    if (walker(rts->args, context))
    {
      return true;
    }
    if (walker(rts->repeatable, context))
    {
      return true;
    }
  }
  break;
  case T_RangeTableFunc:
  {
    RangeTableFunc *rtf = (RangeTableFunc *)node;

    if (walker(rtf->docexpr, context))
    {
      return true;
    }
    if (walker(rtf->rowexpr, context))
    {
      return true;
    }
    if (walker(rtf->namespaces, context))
    {
      return true;
    }
    if (walker(rtf->columns, context))
    {
      return true;
    }
    if (walker(rtf->alias, context))
    {
      return true;
    }
  }
  break;
  case T_RangeTableFuncCol:
  {
    RangeTableFuncCol *rtfc = (RangeTableFuncCol *)node;

    if (walker(rtfc->colexpr, context))
    {
      return true;
    }
    if (walker(rtfc->coldefexpr, context))
    {
      return true;
    }
  }
  break;
  case T_TypeName:
  {
    TypeName *tn = (TypeName *)node;

    if (walker(tn->typmods, context))
    {
      return true;
    }
    if (walker(tn->arrayBounds, context))
    {
      return true;
    }
                                                  
  }
  break;
  case T_ColumnDef:
  {
    ColumnDef *coldef = (ColumnDef *)node;

    if (walker(coldef->typeName, context))
    {
      return true;
    }
    if (walker(coldef->raw_default, context))
    {
      return true;
    }
    if (walker(coldef->collClause, context))
    {
      return true;
    }
                                          
  }
  break;
  case T_IndexElem:
  {
    IndexElem *indelem = (IndexElem *)node;

    if (walker(indelem->expr, context))
    {
      return true;
    }
                                                              
  }
  break;
  case T_GroupingSet:
    return walker(((GroupingSet *)node)->content, context);
  case T_LockingClause:
    return walker(((LockingClause *)node)->lockedRels, context);
  case T_XmlSerialize:
  {
    XmlSerialize *xs = (XmlSerialize *)node;

    if (walker(xs->expr, context))
    {
      return true;
    }
    if (walker(xs->typeName, context))
    {
      return true;
    }
  }
  break;
  case T_WithClause:
    return walker(((WithClause *)node)->ctes, context);
  case T_InferClause:
  {
    InferClause *stmt = (InferClause *)node;

    if (walker(stmt->indexElems, context))
    {
      return true;
    }
    if (walker(stmt->whereClause, context))
    {
      return true;
    }
  }
  break;
  case T_OnConflictClause:
  {
    OnConflictClause *stmt = (OnConflictClause *)node;

    if (walker(stmt->infer, context))
    {
      return true;
    }
    if (walker(stmt->targetList, context))
    {
      return true;
    }
    if (walker(stmt->whereClause, context))
    {
      return true;
    }
  }
  break;
  case T_CommonTableExpr:
    return walker(((CommonTableExpr *)node)->ctequery, context);
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
  return false;
}

   
                                                   
   
                                                                        
                                      
   
bool
planstate_tree_walker(PlanState *planstate, bool (*walker)(), void *context)
{
  Plan *plan = planstate->plan;
  ListCell *lc;

                                                                     
  check_stack_depth();

                  
  if (planstate_walk_subplans(planstate->initPlan, walker, context))
  {
    return true;
  }

                
  if (outerPlanState(planstate))
  {
    if (walker(outerPlanState(planstate), context))
    {
      return true;
    }
  }

                 
  if (innerPlanState(planstate))
  {
    if (walker(innerPlanState(planstate), context))
    {
      return true;
    }
  }

                           
  switch (nodeTag(plan))
  {
  case T_ModifyTable:
    if (planstate_walk_members(((ModifyTableState *)planstate)->mt_plans, ((ModifyTableState *)planstate)->mt_nplans, walker, context))
    {
      return true;
    }
    break;
  case T_Append:
    if (planstate_walk_members(((AppendState *)planstate)->appendplans, ((AppendState *)planstate)->as_nplans, walker, context))
    {
      return true;
    }
    break;
  case T_MergeAppend:
    if (planstate_walk_members(((MergeAppendState *)planstate)->mergeplans, ((MergeAppendState *)planstate)->ms_nplans, walker, context))
    {
      return true;
    }
    break;
  case T_BitmapAnd:
    if (planstate_walk_members(((BitmapAndState *)planstate)->bitmapplans, ((BitmapAndState *)planstate)->nplans, walker, context))
    {
      return true;
    }
    break;
  case T_BitmapOr:
    if (planstate_walk_members(((BitmapOrState *)planstate)->bitmapplans, ((BitmapOrState *)planstate)->nplans, walker, context))
    {
      return true;
    }
    break;
  case T_SubqueryScan:
    if (walker(((SubqueryScanState *)planstate)->subplan, context))
    {
      return true;
    }
    break;
  case T_CustomScan:
    foreach (lc, ((CustomScanState *)planstate)->custom_ps)
    {
      if (walker((PlanState *)lfirst(lc), context))
      {
        return true;
      }
    }
    break;
  default:
    break;
  }

                 
  if (planstate_walk_subplans(planstate->subPlan, walker, context))
  {
    return true;
  }

  return false;
}

   
                                                                         
   
static bool
planstate_walk_subplans(List *plans, bool (*walker)(), void *context)
{
  ListCell *lc;

  foreach (lc, plans)
  {
    SubPlanState *sps = lfirst_node(SubPlanState, lc);

    if (walker(sps->planstate, context))
    {
      return true;
    }
  }

  return false;
}

   
                                                                     
                                
   
static bool
planstate_walk_members(PlanState **planstates, int nplans, bool (*walker)(), void *context)
{
  int j;

  for (j = 0; j < nplans; j++)
  {
    if (walker(planstates[j], context))
    {
      return true;
    }
  }

  return false;
}
