                                                                            
   
               
                                                                         
                                    
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_class.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "utils/lsyscache.h"

   
                
                         
   
A_Expr *
makeA_Expr(A_Expr_Kind kind, List *name, Node *lexpr, Node *rexpr, int location)
{
  A_Expr *a = makeNode(A_Expr);

  a->kind = kind;
  a->name = name;
  a->lexpr = lexpr;
  a->rexpr = rexpr;
  a->location = location;
  return a;
}

   
                      
                                                         
   
A_Expr *
makeSimpleA_Expr(A_Expr_Kind kind, char *name, Node *lexpr, Node *rexpr, int location)
{
  A_Expr *a = makeNode(A_Expr);

  a->kind = kind;
  a->name = list_make1(makeString((char *)name));
  a->lexpr = lexpr;
  a->rexpr = rexpr;
  a->location = location;
  return a;
}

   
             
                        
   
Var *
makeVar(Index varno, AttrNumber varattno, Oid vartype, int32 vartypmod, Oid varcollid, Index varlevelsup)
{
  Var *var = makeNode(Var);

  var->varno = varno;
  var->varattno = varattno;
  var->vartype = vartype;
  var->vartypmod = vartypmod;
  var->varcollid = varcollid;
  var->varlevelsup = varlevelsup;

     
                                                                             
                                                                            
                                                                      
                                                                
     
  var->varnoold = varno;
  var->varoattno = varattno;

                                                        
  var->location = -1;

  return var;
}

   
                            
                                                                
                
   
Var *
makeVarFromTargetEntry(Index varno, TargetEntry *tle)
{
  return makeVar(varno, tle->resno, exprType((Node *)tle->expr), exprTypmod((Node *)tle->expr), exprCollation((Node *)tle->expr), 0);
}

   
                     
                                                                      
   
                                                                      
                                                                         
                                                                           
                                                                              
                                                                      
                                                                           
                                                                        
                               
   
                                                                                
                                                                              
                                                                          
                                                              
   
Var *
makeWholeRowVar(RangeTblEntry *rte, Index varno, Index varlevelsup, bool allowScalar)
{
  Var *result;
  Oid toid;
  Node *fexpr;

  switch (rte->rtekind)
  {
  case RTE_RELATION:
                                                         
    toid = get_rel_type_id(rte->relid);
    if (!OidIsValid(toid))
    {
      elog(ERROR, "could not find type OID for relation %u", rte->relid);
    }
    result = makeVar(varno, InvalidAttrNumber, toid, -1, InvalidOid, varlevelsup);
    break;

  case RTE_FUNCTION:

       
                                                                      
                                                                    
                                                           
       
    if (rte->funcordinality || list_length(rte->functions) != 1)
    {
                                                      
      result = makeVar(varno, InvalidAttrNumber, RECORDOID, -1, InvalidOid, varlevelsup);
      break;
    }

    fexpr = ((RangeTblFunction *)linitial(rte->functions))->funcexpr;
    toid = exprType(fexpr);
    if (type_is_rowtype(toid))
    {
                                                         
      result = makeVar(varno, InvalidAttrNumber, toid, -1, InvalidOid, varlevelsup);
    }
    else if (allowScalar)
    {
                                                             
      result = makeVar(varno, 1, toid, -1, exprCollation(fexpr), varlevelsup);
    }
    else
    {
                                                               
      result = makeVar(varno, InvalidAttrNumber, RECORDOID, -1, InvalidOid, varlevelsup);
    }
    break;

  default:

       
                                                                     
                                                                  
                                                                    
                                          
       
    result = makeVar(varno, InvalidAttrNumber, RECORDOID, -1, InvalidOid, varlevelsup);
    break;
  }

  return result;
}

   
                     
                                
   
TargetEntry *
makeTargetEntry(Expr *expr, AttrNumber resno, char *resname, bool resjunk)
{
  TargetEntry *tle = makeNode(TargetEntry);

  tle->expr = expr;
  tle->resno = resno;
  tle->resname = resname;

     
                                                                            
                                                                    
                                            
     
  tle->ressortgroupref = 0;
  tle->resorigtbl = InvalidOid;
  tle->resorigcol = 0;

  tle->resjunk = resjunk;

  return tle;
}

   
                         
                                                          
   
                                                                             
                     
   
TargetEntry *
flatCopyTargetEntry(TargetEntry *src_tle)
{
  TargetEntry *tle = makeNode(TargetEntry);

  Assert(IsA(src_tle, TargetEntry));
  memcpy(tle, src_tle, sizeof(TargetEntry));
  return tle;
}

   
                  
                             
   
FromExpr *
makeFromExpr(List *fromlist, Node *quals)
{
  FromExpr *f = makeNode(FromExpr);

  f->fromlist = fromlist;
  f->quals = quals;
  return f;
}

   
               
                          
   
Const *
makeConst(Oid consttype, int32 consttypmod, Oid constcollid, int constlen, Datum constvalue, bool constisnull, bool constbyval)
{
  Const *cnst = makeNode(Const);

     
                                                                           
                                                                        
                                                                             
     
  if (!constisnull && constlen == -1)
  {
    constvalue = PointerGetDatum(PG_DETOAST_DATUM(constvalue));
  }

  cnst->consttype = consttype;
  cnst->consttypmod = consttypmod;
  cnst->constcollid = constcollid;
  cnst->constlen = constlen;
  cnst->constvalue = constvalue;
  cnst->constisnull = constisnull;
  cnst->constbyval = constbyval;
  cnst->location = -1;                

  return cnst;
}

   
                   
                                                                           
   
                                                                        
                       
   
Const *
makeNullConst(Oid consttype, int32 consttypmod, Oid constcollid)
{
  int16 typLen;
  bool typByVal;

  get_typlenbyval(consttype, &typLen, &typByVal);
  return makeConst(consttype, consttypmod, constcollid, (int)typLen, (Datum)0, true, typByVal);
}

   
                   
                                                                         
   
Node *
makeBoolConst(bool value, bool isnull)
{
                                                                        
  return (Node *)makeConst(BOOLOID, -1, InvalidOid, 1, BoolGetDatum(value), isnull, true);
}

   
                  
                             
   
Expr *
makeBoolExpr(BoolExprType boolop, List *args, int location)
{
  BoolExpr *b = makeNode(BoolExpr);

  b->boolop = boolop;
  b->args = args;
  b->location = location;

  return (Expr *)b;
}

   
               
                           
   
                                                                         
   
Alias *
makeAlias(const char *aliasname, List *colnames)
{
  Alias *a = makeNode(Alias);

  a->aliasname = pstrdup(aliasname);
  a->colnames = colnames;

  return a;
}

   
                     
                                
   
RelabelType *
makeRelabelType(Expr *arg, Oid rtype, int32 rtypmod, Oid rcollid, CoercionForm rformat)
{
  RelabelType *r = makeNode(RelabelType);

  r->arg = arg;
  r->resulttype = rtype;
  r->resulttypmod = rtypmod;
  r->resultcollid = rcollid;
  r->relabelformat = rformat;
  r->location = -1;

  return r;
}

   
                  
                                                          
   
RangeVar *
makeRangeVar(char *schemaname, char *relname, int location)
{
  RangeVar *r = makeNode(RangeVar);

  r->catalogname = NULL;
  r->schemaname = schemaname;
  r->relname = relname;
  r->inh = true;
  r->relpersistence = RELPERSISTENCE_PERMANENT;
  r->alias = NULL;
  r->location = location;

  return r;
}

   
                  
                                                  
   
                                                            
   
TypeName *
makeTypeName(char *typnam)
{
  return makeTypeNameFromNameList(list_make1(makeString(typnam)));
}

   
                              
                                                                          
   
                                                            
   
TypeName *
makeTypeNameFromNameList(List *names)
{
  TypeName *n = makeNode(TypeName);

  n->names = names;
  n->typmods = NIL;
  n->typemod = -1;
  n->location = -1;
  return n;
}

   
                         
                                                                          
   
TypeName *
makeTypeNameFromOid(Oid typeOid, int32 typmod)
{
  TypeName *n = makeNode(TypeName);

  n->typeOid = typeOid;
  n->typemod = typmod;
  n->location = -1;
  return n;
}

   
                   
                                                                   
   
                                            
                                                 
   
ColumnDef *
makeColumnDef(const char *colname, Oid typeOid, int32 typmod, Oid collOid)
{
  ColumnDef *n = makeNode(ColumnDef);

  n->colname = pstrdup(colname);
  n->typeName = makeTypeNameFromOid(typeOid, typmod);
  n->inhcount = 0;
  n->is_local = true;
  n->is_not_null = false;
  n->is_from_type = false;
  n->storage = 0;
  n->raw_default = NULL;
  n->cooked_default = NULL;
  n->collClause = NULL;
  n->collOid = collOid;
  n->constraints = NIL;
  n->fdwoptions = NIL;
  n->location = -1;

  return n;
}

   
                  
                                                          
   
                                                                
   
FuncExpr *
makeFuncExpr(Oid funcid, Oid rettype, List *args, Oid funccollid, Oid inputcollid, CoercionForm fformat)
{
  FuncExpr *funcexpr;

  funcexpr = makeNode(FuncExpr);
  funcexpr->funcid = funcid;
  funcexpr->funcresulttype = rettype;
  funcexpr->funcretset = false;                               
  funcexpr->funcvariadic = false;                             
  funcexpr->funcformat = fformat;
  funcexpr->funccollid = funccollid;
  funcexpr->inputcollid = inputcollid;
  funcexpr->args = args;
  funcexpr->location = -1;

  return funcexpr;
}

   
                 
                        
   
                                                                             
                          
   
DefElem *
makeDefElem(char *name, Node *arg, int location)
{
  DefElem *res = makeNode(DefElem);

  res->defnamespace = NULL;
  res->defname = name;
  res->arg = arg;
  res->defaction = DEFELEM_UNSPEC;
  res->location = location;

  return res;
}

   
                         
                                                                  
   
DefElem *
makeDefElemExtended(char *nameSpace, char *name, Node *arg, DefElemAction defaction, int location)
{
  DefElem *res = makeNode(DefElem);

  res->defnamespace = nameSpace;
  res->defname = name;
  res->arg = arg;
  res->defaction = defaction;
  res->location = location;

  return res;
}

   
                  
   
                                                                       
                                                                          
   
FuncCall *
makeFuncCall(List *name, List *args, int location)
{
  FuncCall *n = makeNode(FuncCall);

  n->funcname = name;
  n->args = args;
  n->agg_order = NIL;
  n->agg_filter = NULL;
  n->agg_within_group = false;
  n->agg_star = false;
  n->agg_distinct = false;
  n->func_variadic = false;
  n->over = NULL;
  n->location = location;
  return n;
}

   
                 
                                                                      
                                                                    
                         
   
Expr *
make_opclause(Oid opno, Oid opresulttype, bool opretset, Expr *leftop, Expr *rightop, Oid opcollid, Oid inputcollid)
{
  OpExpr *expr = makeNode(OpExpr);

  expr->opno = opno;
  expr->opfuncid = InvalidOid;
  expr->opresulttype = opresulttype;
  expr->opretset = opretset;
  expr->opcollid = opcollid;
  expr->inputcollid = inputcollid;
  if (rightop)
  {
    expr->args = list_make2(leftop, rightop);
  }
  else
  {
    expr->args = list_make1(leftop);
  }
  expr->location = -1;
  return (Expr *)expr;
}

   
                  
   
                                                           
   
Expr *
make_andclause(List *andclauses)
{
  BoolExpr *expr = makeNode(BoolExpr);

  expr->boolop = AND_EXPR;
  expr->args = andclauses;
  expr->location = -1;
  return (Expr *)expr;
}

   
                 
   
                                                          
   
Expr *
make_orclause(List *orclauses)
{
  BoolExpr *expr = makeNode(BoolExpr);

  expr->boolop = OR_EXPR;
  expr->args = orclauses;
  expr->location = -1;
  return (Expr *)expr;
}

   
                  
   
                                                             
   
Expr *
make_notclause(Expr *notclause)
{
  BoolExpr *expr = makeNode(BoolExpr);

  expr->boolop = NOT_EXPR;
  expr->args = list_make1(notclause);
  expr->location = -1;
  return (Expr *)expr;
}

   
                 
   
                                                                      
                                                                         
              
   
                                                                           
                                                                   
   
Node *
make_and_qual(Node *qual1, Node *qual2)
{
  if (qual1 == NULL)
  {
    return qual2;
  }
  if (qual2 == NULL)
  {
    return qual1;
  }
  return (Node *)make_andclause(list_make2(qual1, qual2));
}

   
                                                                        
                                                                
   
                                                                            
                                                    
   
                                                             
   
Expr *
make_ands_explicit(List *andclauses)
{
  if (andclauses == NIL)
  {
    return (Expr *)makeBoolConst(true, false);
  }
  else if (list_length(andclauses) == 1)
  {
    return (Expr *)linitial(andclauses);
  }
  else
  {
    return make_andclause(andclauses);
  }
}

List *
make_ands_implicit(Expr *clause)
{
     
                                                                            
                                                                         
                                                      
     
  if (clause == NULL)
  {
    return NIL;                               
  }
  else if (is_andclause(clause))
  {
    return ((BoolExpr *)clause)->args;
  }
  else if (IsA(clause, Const) && !((Const *)clause)->constisnull && DatumGetBool(((Const *)clause)->constvalue))
  {
    return NIL;                                      
  }
  else
  {
    return list_make1(clause);
  }
}

   
                 
                              
   
IndexInfo *
makeIndexInfo(int numattrs, int numkeyattrs, Oid amoid, List *expressions, List *predicates, bool unique, bool isready, bool concurrent)
{
  IndexInfo *n = makeNode(IndexInfo);

  n->ii_NumIndexAttrs = numattrs;
  n->ii_NumIndexKeyAttrs = numkeyattrs;
  Assert(n->ii_NumIndexKeyAttrs != 0);
  Assert(n->ii_NumIndexKeyAttrs <= n->ii_NumIndexAttrs);
  n->ii_Unique = unique;
  n->ii_ReadyForInserts = isready;
  n->ii_Concurrent = concurrent;

                   
  n->ii_Expressions = expressions;
  n->ii_ExpressionsState = NIL;

                   
  n->ii_Predicate = predicates;
  n->ii_PredicateState = NULL;

                             
  n->ii_ExclusionOps = NULL;
  n->ii_ExclusionProcs = NULL;
  n->ii_ExclusionStrats = NULL;

                           
  n->ii_UniqueOps = NULL;
  n->ii_UniqueProcs = NULL;
  n->ii_UniqueStrats = NULL;

                                               
  n->ii_BrokenHotChain = false;
  n->ii_ParallelWorkers = 0;

                                           
  n->ii_Am = amoid;
  n->ii_AmCache = NULL;
  n->ii_Context = CurrentMemoryContext;

  return n;
}

   
                   
   
   
GroupingSet *
makeGroupingSet(GroupingSetKind kind, List *content, int location)
{
  GroupingSet *n = makeNode(GroupingSet);

  n->kind = kind;
  n->content = content;
  n->location = location;
  return n;
}

   
                        
                                  
   
VacuumRelation *
makeVacuumRelation(RangeVar *relation, Oid oid, List *va_cols)
{
  VacuumRelation *v = makeNode(VacuumRelation);

  v->relation = relation;
  v->oid = oid;
  v->va_cols = va_cols;
  return v;
}
