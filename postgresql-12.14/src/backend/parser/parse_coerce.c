                                                                            
   
                  
                                                 
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

static Node *
coerce_type_typmod(Node *node, Oid targetTypeId, int32 targetTypMod, CoercionContext ccontext, CoercionForm cformat, int location, bool hideInputCoercion);
static void
hide_coercion_node(Node *node);
static Node *
build_coercion_expression(Node *node, CoercionPathType pathtype, Oid funcId, Oid targetTypeId, int32 targetTypMod, CoercionContext ccontext, CoercionForm cformat, int location);
static Node *
coerce_record_to_complex(ParseState *pstate, Node *node, Oid targetTypeId, CoercionContext ccontext, CoercionForm cformat, int location);
static bool
is_complex_array(Oid typid);
static bool
typeIsOfTypedTable(Oid reltypeId, Oid reloftypeId);

   
                           
                                                       
   
                                                                       
                                                                        
                                                                       
                                                           
   
                                                                         
                                                                               
                                                                           
   
                                                       
                                                                       
                                  
                                    
                                        
                                                               
                                                                                
   
Node *
coerce_to_target_type(ParseState *pstate, Node *expr, Oid exprtype, Oid targettype, int32 targettypmod, CoercionContext ccontext, CoercionForm cformat, int location)
{
  Node *result;
  Node *origexpr;

  if (!can_coerce_type(1, &exprtype, &targettype, ccontext))
  {
    return NULL;
  }

     
                                                                          
                                                                     
                                                                            
                                                                        
                                                                      
                                                                             
                                                                         
                                                                            
     
  origexpr = expr;
  while (expr && IsA(expr, CollateExpr))
  {
    expr = (Node *)((CollateExpr *)expr)->arg;
  }

  result = coerce_type(pstate, expr, exprtype, targettype, targettypmod, ccontext, cformat, location);

     
                                                                            
                                                                           
                                                   
     
  result = coerce_type_typmod(result, targettype, targettypmod, ccontext, cformat, location, (result != expr && !IsA(result, Const)));

  if (expr != origexpr && type_is_collatable(targettype))
  {
                                   
    CollateExpr *coll = (CollateExpr *)origexpr;
    CollateExpr *newcoll = makeNode(CollateExpr);

    newcoll->arg = (Expr *)result;
    newcoll->collOid = coll->collOid;
    newcoll->location = coll->location;
    result = (Node *)newcoll;
  }

  return result;
}

   
                 
                                               
   
                                                                            
                        
   
                                                                             
                                                                           
                                                                    
                                                                      
                                                                      
                                                                       
                                                        
   
                                                                           
                                                                        
                                                             
   
                                                                           
                                                                         
   
Node *
coerce_type(ParseState *pstate, Node *node, Oid inputTypeId, Oid targetTypeId, int32 targetTypeMod, CoercionContext ccontext, CoercionForm cformat, int location)
{
  Node *result;
  CoercionPathType pathtype;
  Oid funcId;

  if (targetTypeId == inputTypeId || node == NULL)
  {
                              
    return node;
  }
  if (targetTypeId == ANYOID || targetTypeId == ANYELEMENTOID || targetTypeId == ANYNONARRAYOID)
  {
       
                                                                       
       
                                                                       
                                                                   
                                                                           
                                                                           
       
                                                                      
                                                                      
                   
       
    return node;
  }
  if (targetTypeId == ANYARRAYOID || targetTypeId == ANYENUMOID || targetTypeId == ANYRANGEOID)
  {
       
                                                                       
       
                                                                         
                                                                      
                                                                         
                                                                
                                                                          
                                                                       
                                    
       
                                                                       
                                                                    
                                                                      
       
    if (inputTypeId != UNKNOWNOID)
    {
      Oid baseTypeId = getBaseType(inputTypeId);

      if (baseTypeId != inputTypeId)
      {
        RelabelType *r = makeRelabelType((Expr *)node, baseTypeId, -1, InvalidOid, cformat);

        r->location = location;
        return (Node *)r;
      }
                                                 
      return node;
    }
  }
  if (inputTypeId == UNKNOWNOID && IsA(node, Const))
  {
       
                                                                           
                                                                          
                        
       
                                                                
                                                                 
                                                                      
                                                                    
                                                           
       
                                                                         
                                                                         
                                                                        
                                                                    
                                                 
       
    Const *con = (Const *)node;
    Const *newcon = makeNode(Const);
    Oid baseTypeId;
    int32 baseTypeMod;
    int32 inputTypeMod;
    Type baseType;
    ParseCallbackState pcbstate;

       
                                                                       
                                                                           
                                                                        
                                                                          
                                                                     
                                  
       
    baseTypeMod = targetTypeMod;
    baseTypeId = getBaseTypeAndTypmod(targetTypeId, &baseTypeMod);

       
                                                                      
                                                                      
                                                                         
                                                                     
                                                                         
                                                                           
                                                   
       
    if (baseTypeId == INTERVALOID)
    {
      inputTypeMod = baseTypeMod;
    }
    else
    {
      inputTypeMod = -1;
    }

    baseType = typeidType(baseTypeId);

    newcon->consttype = baseTypeId;
    newcon->consttypmod = inputTypeMod;
    newcon->constcollid = typeTypeCollation(baseType);
    newcon->constlen = typeLen(baseType);
    newcon->constbyval = typeByVal(baseType);
    newcon->constisnull = con->constisnull;

       
                                                                         
                                                                          
                                             
       
    newcon->location = con->location;

       
                                                                          
                 
       
    setup_parser_errposition_callback(&pcbstate, pstate, con->location);

       
                                                                         
                   
       
    if (!con->constisnull)
    {
      newcon->constvalue = stringTypeDatum(baseType, DatumGetCString(con->constvalue), inputTypeMod);
    }
    else
    {
      newcon->constvalue = stringTypeDatum(baseType, NULL, inputTypeMod);
    }

       
                                                               
                                                                    
                                                                   
       
    if (!con->constisnull && newcon->constlen == -1)
    {
      newcon->constvalue = PointerGetDatum(PG_DETOAST_DATUM(newcon->constvalue));
    }

#ifdef RANDOMIZE_ALLOCATED_MEMORY

       
                                                                         
                                                                           
                                                                      
                                                                           
                                                                        
                                                                          
                                                                        
                                                                    
                                 
       
    if (!con->constisnull && !newcon->constbyval)
    {
      Datum val2;

      val2 = stringTypeDatum(baseType, DatumGetCString(con->constvalue), inputTypeMod);
      if (newcon->constlen == -1)
      {
        val2 = PointerGetDatum(PG_DETOAST_DATUM(val2));
      }
      if (!datumIsEqual(newcon->constvalue, val2, false, newcon->constlen))
      {
        elog(WARNING, "type %s has unstable input conversion for \"%s\"", typeTypeName(baseType), DatumGetCString(con->constvalue));
      }
    }
#endif

    cancel_parser_errposition_callback(&pcbstate);

    result = (Node *)newcon;

                                                   
    if (baseTypeId != targetTypeId)
    {
      result = coerce_to_domain(result, baseTypeId, baseTypeMod, targetTypeId, ccontext, cformat, location, false);
    }

    ReleaseSysCache(baseType);

    return result;
  }
  if (IsA(node, Param) && pstate != NULL && pstate->p_coerce_param_hook != NULL)
  {
       
                                                                          
                                                                       
                                                                
       
    result = pstate->p_coerce_param_hook(pstate, (Param *)node, targetTypeId, targetTypeMod, location);
    if (result)
    {
      return result;
    }
  }
  if (IsA(node, CollateExpr))
  {
       
                                                                 
                                                                         
                                                                          
                                                                    
                                                                         
       
    CollateExpr *coll = (CollateExpr *)node;

    result = coerce_type(pstate, (Node *)coll->arg, inputTypeId, targetTypeId, targetTypeMod, ccontext, cformat, location);
    if (type_is_collatable(targetTypeId))
    {
      CollateExpr *newcoll = makeNode(CollateExpr);

      newcoll->arg = (Expr *)result;
      newcoll->collOid = coll->collOid;
      newcoll->location = coll->location;
      result = (Node *)newcoll;
    }
    return result;
  }
  pathtype = find_coercion_pathway(targetTypeId, inputTypeId, ccontext, &funcId);
  if (pathtype != COERCION_PATH_NONE)
  {
    if (pathtype != COERCION_PATH_RELABELTYPE)
    {
         
                                                                       
                                                                      
                                                                        
                                                                   
                             
         
      Oid baseTypeId;
      int32 baseTypeMod;

      baseTypeMod = targetTypeMod;
      baseTypeId = getBaseTypeAndTypmod(targetTypeId, &baseTypeMod);

      result = build_coercion_expression(node, pathtype, funcId, baseTypeId, baseTypeMod, ccontext, cformat, location);

         
                                                                      
                                                     
         
      if (targetTypeId != baseTypeId)
      {
        result = coerce_to_domain(result, baseTypeId, baseTypeMod, targetTypeId, ccontext, cformat, location, true);
      }
    }
    else
    {
         
                                                                      
                                                                       
                                                                        
         
                                                                        
                                                                     
                                                
         
      result = coerce_to_domain(node, InvalidOid, -1, targetTypeId, ccontext, cformat, location, false);
      if (result == node)
      {
           
                                                                      
                                                                 
                                                                       
                                                    
           
        RelabelType *r = makeRelabelType((Expr *)result, targetTypeId, -1, InvalidOid, cformat);

        r->location = location;
        result = (Node *)r;
      }
    }
    return result;
  }
  if (inputTypeId == RECORDOID && ISCOMPLEX(targetTypeId))
  {
                                                    
    return coerce_record_to_complex(pstate, node, targetTypeId, ccontext, cformat, location);
  }
  if (targetTypeId == RECORDOID && ISCOMPLEX(inputTypeId))
  {
                                                  
                                               
    return node;
  }
#ifdef NOT_USED
  if (inputTypeId == RECORDARRAYOID && is_complex_array(targetTypeId))
  {
                                                          
                                 
  }
#endif
  if (targetTypeId == RECORDARRAYOID && is_complex_array(inputTypeId))
  {
                                                          
                                               
    return node;
  }
  if (typeInheritsFrom(inputTypeId, targetTypeId) || typeIsOfTypedTable(inputTypeId, targetTypeId))
  {
       
                                                                
                                                                     
                                                       
       
                                                                           
                                                                         
                                                                           
                                                                        
                                         
       
    Oid baseTypeId = getBaseType(inputTypeId);
    ConvertRowtypeExpr *r = makeNode(ConvertRowtypeExpr);

    if (baseTypeId != inputTypeId)
    {
      RelabelType *rt = makeRelabelType((Expr *)node, baseTypeId, -1, InvalidOid, COERCE_IMPLICIT_CAST);

      rt->location = location;
      node = (Node *)rt;
    }
    r->arg = (Expr *)node;
    r->resulttype = targetTypeId;
    r->convertformat = cformat;
    r->location = location;
    return (Node *)r;
  }
                                      
  elog(ERROR, "failed to find conversion function from %s to %s", format_type_be(inputTypeId), format_type_be(targetTypeId));
  return NULL;                          
}

   
                     
                                                    
   
                                                                               
                                                  
   
bool
can_coerce_type(int nargs, const Oid *input_typeids, const Oid *target_typeids, CoercionContext ccontext)
{
  bool have_generics = false;
  int i;

                                    
  for (i = 0; i < nargs; i++)
  {
    Oid inputTypeId = input_typeids[i];
    Oid targetTypeId = target_typeids[i];
    CoercionPathType pathtype;
    Oid funcId;

                                 
    if (inputTypeId == targetTypeId)
    {
      continue;
    }

                                 
    if (targetTypeId == ANYOID)
    {
      continue;
    }

                                                  
    if (IsPolymorphicType(targetTypeId))
    {
      have_generics = true;                             
      continue;
    }

       
                                                                           
                 
       
    if (inputTypeId == UNKNOWNOID)
    {
      continue;
    }

       
                                                                          
                                                           
       
    pathtype = find_coercion_pathway(targetTypeId, inputTypeId, ccontext, &funcId);
    if (pathtype != COERCION_PATH_NONE)
    {
      continue;
    }

       
                                                                        
                                               
       
    if (inputTypeId == RECORDOID && ISCOMPLEX(targetTypeId))
    {
      continue;
    }

       
                                                                 
       
    if (targetTypeId == RECORDOID && ISCOMPLEX(inputTypeId))
    {
      continue;
    }

#ifdef NOT_USED                          

       
                                                                         
                                                      
       
    if (inputTypeId == RECORDARRAYOID && is_complex_array(targetTypeId))
    {
      continue;
    }
#endif

       
                                                                         
       
    if (targetTypeId == RECORDARRAYOID && is_complex_array(inputTypeId))
    {
      continue;
    }

       
                                                                  
       
    if (typeInheritsFrom(inputTypeId, targetTypeId) || typeIsOfTypedTable(inputTypeId, targetTypeId))
    {
      continue;
    }

       
                                                     
       
    return false;
  }

                                                                
  if (have_generics)
  {
    if (!check_generic_type_consistency(input_typeids, target_typeids, nargs))
    {
      return false;
    }
  }

  return true;
}

   
                                                                     
   
                           
                                                                          
                                      
                                                                          
                                      
                                      
                                                      
                                      
                                         
                                                                         
   
                                                                         
   
Node *
coerce_to_domain(Node *arg, Oid baseTypeId, int32 baseTypeMod, Oid typeId, CoercionContext ccontext, CoercionForm cformat, int location, bool hideInputCoercion)
{
  CoerceToDomain *result;

                                                    
  if (baseTypeId == InvalidOid)
  {
    baseTypeId = getBaseTypeAndTypmod(typeId, &baseTypeMod);
  }

                                                                 
  if (baseTypeId == typeId)
  {
    return arg;
  }

                                                 
  if (hideInputCoercion)
  {
    hide_coercion_node(arg);
  }

     
                                                                            
                                                                             
                                                                             
                                                                          
                                                                         
                                                              
     
                                                                            
                                                                          
                                                                          
                                        
     
  arg = coerce_type_typmod(arg, baseTypeId, baseTypeMod, ccontext, COERCE_IMPLICIT_CAST, location, false);

     
                                                                            
                                                                             
                                                                
     
  result = makeNode(CoerceToDomain);
  result->arg = (Expr *)arg;
  result->resulttype = typeId;
  result->resulttypmod = -1;                                       
                                                   
  result->coercionformat = cformat;
  result->location = location;

  return (Node *)result;
}

   
                        
                                                                      
   
                                                                       
                                                                       
                                                                         
   
                                                                         
                                            
   
                                                                           
                                                               
   
                                                                             
   
                                                                            
                                                                               
                                              
   
                                                                        
                                                                       
                                                                               
   
static Node *
coerce_type_typmod(Node *node, Oid targetTypeId, int32 targetTypMod, CoercionContext ccontext, CoercionForm cformat, int location, bool hideInputCoercion)
{
  CoercionPathType pathtype;
  Oid funcId;

                                     
  if (targetTypMod == exprTypmod(node))
  {
    return node;
  }

                                                 
  if (hideInputCoercion)
  {
    hide_coercion_node(node);
  }

     
                                                                             
                                                                           
             
     
  if (targetTypMod < 0)
  {
    pathtype = COERCION_PATH_NONE;
  }
  else
  {
    pathtype = find_typmod_coercion_function(targetTypeId, &funcId);
  }

  if (pathtype != COERCION_PATH_NONE)
  {
    node = build_coercion_expression(node, pathtype, funcId, targetTypeId, targetTypMod, ccontext, cformat, location);
  }
  else
  {
       
                                                                        
                                                                     
                        
       
    node = applyRelabelType(node, targetTypeId, targetTypMod, exprCollation(node), cformat, location, false);
  }

  return node;
}

   
                                                                     
                                                                       
                                                                      
                                                                          
                          
   
                                                                    
                       
   
static void
hide_coercion_node(Node *node)
{
  if (IsA(node, FuncExpr))
  {
    ((FuncExpr *)node)->funcformat = COERCE_IMPLICIT_CAST;
  }
  else if (IsA(node, RelabelType))
  {
    ((RelabelType *)node)->relabelformat = COERCE_IMPLICIT_CAST;
  }
  else if (IsA(node, CoerceViaIO))
  {
    ((CoerceViaIO *)node)->coerceformat = COERCE_IMPLICIT_CAST;
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
    ((ArrayCoerceExpr *)node)->coerceformat = COERCE_IMPLICIT_CAST;
  }
  else if (IsA(node, ConvertRowtypeExpr))
  {
    ((ConvertRowtypeExpr *)node)->convertformat = COERCE_IMPLICIT_CAST;
  }
  else if (IsA(node, RowExpr))
  {
    ((RowExpr *)node)->row_format = COERCE_IMPLICIT_CAST;
  }
  else if (IsA(node, CoerceToDomain))
  {
    ((CoerceToDomain *)node)->coercionformat = COERCE_IMPLICIT_CAST;
  }
  else
  {
    elog(ERROR, "unsupported node type: %d", (int)nodeTag(node));
  }
}

   
                               
                                                               
   
                                                                       
                                                                    
   
static Node *
build_coercion_expression(Node *node, CoercionPathType pathtype, Oid funcId, Oid targetTypeId, int32 targetTypMod, CoercionContext ccontext, CoercionForm cformat, int location)
{
  int nargs = 0;

  if (OidIsValid(funcId))
  {
    HeapTuple tp;
    Form_pg_proc procstruct;

    tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcId));
    if (!HeapTupleIsValid(tp))
    {
      elog(ERROR, "cache lookup failed for function %u", funcId);
    }
    procstruct = (Form_pg_proc)GETSTRUCT(tp);

       
                                                                         
                                                                          
                                                                           
                                           
       
                                                         
    Assert(!procstruct->proretset);
    Assert(procstruct->prokind == PROKIND_FUNCTION);
    nargs = procstruct->pronargs;
    Assert(nargs >= 1 && nargs <= 3);
                                                                      
    Assert(nargs < 2 || procstruct->proargtypes.values[1] == INT4OID);
    Assert(nargs < 3 || procstruct->proargtypes.values[2] == BOOLOID);

    ReleaseSysCache(tp);
  }

  if (pathtype == COERCION_PATH_FUNC)
  {
                                                              
    FuncExpr *fexpr;
    List *args;
    Const *cons;

    Assert(OidIsValid(funcId));

    args = list_make1(node);

    if (nargs >= 2)
    {
                                                  
      cons = makeConst(INT4OID, -1, InvalidOid, sizeof(int32), Int32GetDatum(targetTypMod), false, true);

      args = lappend(args, cons);
    }

    if (nargs == 3)
    {
                                                       
      cons = makeConst(BOOLOID, -1, InvalidOid, sizeof(bool), BoolGetDatum(ccontext == COERCION_EXPLICIT), false, true);

      args = lappend(args, cons);
    }

    fexpr = makeFuncExpr(funcId, targetTypeId, args, InvalidOid, InvalidOid, cformat);
    fexpr->location = location;
    return (Node *)fexpr;
  }
  else if (pathtype == COERCION_PATH_ARRAYCOERCE)
  {
                                             
    ArrayCoerceExpr *acoerce = makeNode(ArrayCoerceExpr);
    CaseTestExpr *ctest = makeNode(CaseTestExpr);
    Oid sourceBaseTypeId;
    int32 sourceBaseTypeMod;
    Oid targetElementType;
    Node *elemexpr;

       
                                                                          
                                                                          
                                                                          
       
    sourceBaseTypeMod = exprTypmod(node);
    sourceBaseTypeId = getBaseTypeAndTypmod(exprType(node), &sourceBaseTypeMod);

       
                                                                           
                                                                      
                                                                     
                 
       
    ctest->typeId = get_element_type(sourceBaseTypeId);
    Assert(OidIsValid(ctest->typeId));
    ctest->typeMod = sourceBaseTypeMod;
    ctest->collation = InvalidOid;                                  

                                                  
    targetElementType = get_element_type(targetTypeId);
    Assert(OidIsValid(targetElementType));

    elemexpr = coerce_to_target_type(NULL, (Node *)ctest, ctest->typeId, targetElementType, targetTypMod, ccontext, cformat, location);
    if (elemexpr == NULL)                       
    {
      elog(ERROR, "failed to coerce array element type as expected");
    }

    acoerce->arg = (Expr *)node;
    acoerce->elemexpr = (Expr *)elemexpr;
    acoerce->resulttype = targetTypeId;

       
                                                                         
                                                                        
       
    acoerce->resulttypmod = exprTypmod(elemexpr);
                                                     
    acoerce->coerceformat = cformat;
    acoerce->location = location;

    return (Node *)acoerce;
  }
  else if (pathtype == COERCION_PATH_COERCEVIAIO)
  {
                                             
    CoerceViaIO *iocoerce = makeNode(CoerceViaIO);

    Assert(!OidIsValid(funcId));

    iocoerce->arg = (Expr *)node;
    iocoerce->resulttype = targetTypeId;
                                                     
    iocoerce->coerceformat = cformat;
    iocoerce->location = location;

    return (Node *)iocoerce;
  }
  else
  {
    elog(ERROR, "unsupported pathtype %d in build_coercion_expression", (int)pathtype);
    return NULL;                          
  }
}

   
                            
                                                  
   
                                                                            
         
   
static Node *
coerce_record_to_complex(ParseState *pstate, Node *node, Oid targetTypeId, CoercionContext ccontext, CoercionForm cformat, int location)
{
  RowExpr *rowexpr;
  Oid baseTypeId;
  int32 baseTypeMod = -1;
  TupleDesc tupdesc;
  List *args = NIL;
  List *newargs;
  int i;
  int ucolno;
  ListCell *arg;

  if (node && IsA(node, RowExpr))
  {
       
                                                                           
                                       
       
    args = ((RowExpr *)node)->args;
  }
  else if (node && IsA(node, Var) && ((Var *)node)->varattno == InvalidAttrNumber)
  {
    int rtindex = ((Var *)node)->varno;
    int sublevels_up = ((Var *)node)->varlevelsup;
    int vlocation = ((Var *)node)->location;
    RangeTblEntry *rte;

    rte = GetRTEByRangeTablePosn(pstate, rtindex, sublevels_up);
    expandRTE(rte, rtindex, sublevels_up, vlocation, false, NULL, &args);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(RECORDOID), format_type_be(targetTypeId)), parser_coercion_errposition(pstate, location, node)));
  }

     
                                                                             
                                       
     
  baseTypeId = getBaseTypeAndTypmod(targetTypeId, &baseTypeMod);
  tupdesc = lookup_rowtype_tupdesc(baseTypeId, baseTypeMod);

                          
  newargs = NIL;
  ucolno = 1;
  arg = list_head(args);
  for (i = 0; i < tupdesc->natts; i++)
  {
    Node *expr;
    Node *cexpr;
    Oid exprtype;
    Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

                                                      
    if (attr->attisdropped)
    {
         
                                                                         
                                 
         
      newargs = lappend(newargs, makeNullConst(INT4OID, -1, InvalidOid));
      continue;
    }

    if (arg == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(RECORDOID), format_type_be(targetTypeId)), errdetail("Input has too few columns."), parser_coercion_errposition(pstate, location, node)));
    }
    expr = (Node *)lfirst(arg);
    exprtype = exprType(expr);

    cexpr = coerce_to_target_type(pstate, expr, exprtype, attr->atttypid, attr->atttypmod, ccontext, COERCE_IMPLICIT_CAST, -1);
    if (cexpr == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(RECORDOID), format_type_be(targetTypeId)), errdetail("Cannot cast type %s to %s in column %d.", format_type_be(exprtype), format_type_be(attr->atttypid), ucolno), parser_coercion_errposition(pstate, location, expr)));
    }
    newargs = lappend(newargs, cexpr);
    ucolno++;
    arg = lnext(arg);
  }
  if (arg != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE), errmsg("cannot cast type %s to %s", format_type_be(RECORDOID), format_type_be(targetTypeId)), errdetail("Input has too many columns."), parser_coercion_errposition(pstate, location, node)));
  }

  ReleaseTupleDesc(tupdesc);

  rowexpr = makeNode(RowExpr);
  rowexpr->args = newargs;
  rowexpr->row_typeid = baseTypeId;
  rowexpr->row_format = cformat;
  rowexpr->colnames = NIL;                                       
  rowexpr->location = location;

                                                
  if (baseTypeId != targetTypeId)
  {
    rowexpr->row_format = COERCE_IMPLICIT_CAST;
    return coerce_to_domain((Node *)rowexpr, baseTypeId, baseTypeMod, targetTypeId, ccontext, cformat, location, false);
  }

  return (Node *)rowexpr;
}

   
                       
                                                                  
                                                              
   
                                               
   
                                                                       
                         
   
Node *
coerce_to_boolean(ParseState *pstate, Node *node, const char *constructName)
{
  Oid inputTypeId = exprType(node);

  if (inputTypeId != BOOLOID)
  {
    Node *newnode;

    newnode = coerce_to_target_type(pstate, node, inputTypeId, BOOLOID, -1, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (newnode == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                        
                         errmsg("argument of %s must be type %s, not type %s", constructName, "boolean", format_type_be(inputTypeId)), parser_errposition(pstate, exprLocation(node))));
    }
    node = newnode;
  }

  if (expression_returns_set(node))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                
                       errmsg("argument of %s must not return a set", constructName), parser_errposition(pstate, exprLocation(node))));
  }

  return node;
}

   
                                    
                                                                          
                                                                 
   
                                               
   
                                                                       
                         
   
Node *
coerce_to_specific_type_typmod(ParseState *pstate, Node *node, Oid targetTypeId, int32 targetTypmod, const char *constructName)
{
  Oid inputTypeId = exprType(node);

  if (inputTypeId != targetTypeId)
  {
    Node *newnode;

    newnode = coerce_to_target_type(pstate, node, inputTypeId, targetTypeId, targetTypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (newnode == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                        
                         errmsg("argument of %s must be type %s, not type %s", constructName, format_type_be(targetTypeId), format_type_be(inputTypeId)), parser_errposition(pstate, exprLocation(node))));
    }
    node = newnode;
  }

  if (expression_returns_set(node))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                
                       errmsg("argument of %s must not return a set", constructName), parser_errposition(pstate, exprLocation(node))));
  }

  return node;
}

   
                             
                                                                          
                                        
   
                                               
   
                                                                       
                         
   
Node *
coerce_to_specific_type(ParseState *pstate, Node *node, Oid targetTypeId, const char *constructName)
{
  return coerce_to_specific_type_typmod(pstate, node, targetTypeId, -1, constructName);
}

   
                                                                             
   
                                                                           
                                                                          
                                               
   
                                                                    
                                                           
   
int
parser_coercion_errposition(ParseState *pstate, int coerce_location, Node *input_expr)
{
  if (coerce_location >= 0)
  {
    return parser_errposition(pstate, coerce_location);
  }
  else
  {
    return parser_errposition(pstate, exprLocation(input_expr));
  }
}

   
                        
                                                                   
                                                                 
                            
   
                                                                         
                                                    
                                                                          
                                                                   
                                             
                                                                         
                                                    
   
Oid
select_common_type(ParseState *pstate, List *exprs, const char *context, Node **which_expr)
{
  Node *pexpr;
  Oid ptype;
  TYPCATEGORY pcategory;
  bool pispreferred;
  ListCell *lc;

  Assert(exprs != NIL);
  pexpr = (Node *)linitial(exprs);
  lc = lnext(list_head(exprs));
  ptype = exprType(pexpr);

     
                                                                             
                                                                            
                                                                             
     
  if (ptype != UNKNOWNOID)
  {
    for_each_cell(lc, lc)
    {
      Node *nexpr = (Node *)lfirst(lc);
      Oid ntype = exprType(nexpr);

      if (ntype != ptype)
      {
        break;
      }
    }
    if (lc == NULL)                                  
    {
      if (which_expr)
      {
        *which_expr = pexpr;
      }
      return ptype;
    }
  }

     
                                                                          
                                                                             
                                                               
     
  ptype = getBaseType(ptype);
  get_type_category_preferred(ptype, &pcategory, &pispreferred);

  for_each_cell(lc, lc)
  {
    Node *nexpr = (Node *)lfirst(lc);
    Oid ntype = getBaseType(exprType(nexpr));

                                                      
    if (ntype != UNKNOWNOID && ntype != ptype)
    {
      TYPCATEGORY ncategory;
      bool nispreferred;

      get_type_category_preferred(ntype, &ncategory, &nispreferred);
      if (ptype == UNKNOWNOID)
      {
                                                       
        pexpr = nexpr;
        ptype = ntype;
        pcategory = ncategory;
        pispreferred = nispreferred;
      }
      else if (ncategory != pcategory)
      {
           
                                                                     
           
        if (context == NULL)
        {
          return InvalidOid;
        }
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                    
                                                                                        
                           errmsg("%s types %s and %s cannot be matched", context, format_type_be(ptype), format_type_be(ntype)), parser_errposition(pstate, exprLocation(nexpr))));
      }
      else if (!pispreferred && can_coerce_type(1, &ptype, &ntype, COERCION_IMPLICIT) && !can_coerce_type(1, &ntype, &ptype, COERCION_IMPLICIT))
      {
           
                                                                    
                                                                   
           
        pexpr = nexpr;
        ptype = ntype;
        pcategory = ncategory;
        pispreferred = nispreferred;
      }
    }
  }

     
                                                                           
                                                                         
                                                                         
                                                                          
                                                                        
                                                                          
                                                                        
                                                                          
          
     
  if (ptype == UNKNOWNOID)
  {
    ptype = TEXTOID;
  }

  if (which_expr)
  {
    *which_expr = pexpr;
  }
  return ptype;
}

   
                           
                                            
   
                                                                        
                                                                         
                                       
   
                                                                       
                         
   
Node *
coerce_to_common_type(ParseState *pstate, Node *node, Oid targetTypeId, const char *context)
{
  Oid inputTypeId = exprType(node);

  if (inputTypeId == targetTypeId)
  {
    return node;              
  }
  if (can_coerce_type(1, &inputTypeId, &targetTypeId, COERCION_IMPLICIT))
  {
    node = coerce_type(pstate, node, inputTypeId, targetTypeId, -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_CANNOT_COERCE),
                                                                                     
                       errmsg("%s could not convert type %s to %s", context, format_type_be(inputTypeId), format_type_be(targetTypeId)), parser_errposition(pstate, exprLocation(node))));
  }
  return node;
}

   
                                    
                                                           
                          
   
                                       
   
                                                                     
                                                                   
                                         
                                                                   
                                 
                                                                            
                                                                           
                        
                                                                         
                                                                         
                               
                                                                          
                                                                       
                                                         
                                                                               
                                                                               
                                                                           
                                      
   
                                                                              
                                                                               
                                                                            
                                                           
   
                                                                      
                                 
   
                                                                   
                                        
   
                                                                         
                                
   
                                                                       
                                                                            
                                                                       
                                                                          
                                                                  
   
                                                                        
   
bool
check_generic_type_consistency(const Oid *actual_arg_types, const Oid *declared_arg_types, int nargs)
{
  int j;
  Oid elem_typeid = InvalidOid;
  Oid array_typeid = InvalidOid;
  Oid array_typelem;
  Oid range_typeid = InvalidOid;
  Oid range_typelem;
  bool have_anyelement = false;
  bool have_anynonarray = false;
  bool have_anyenum = false;

     
                                                                            
                                                       
     
  for (j = 0; j < nargs; j++)
  {
    Oid decl_type = declared_arg_types[j];
    Oid actual_type = actual_arg_types[j];

    if (decl_type == ANYELEMENTOID || decl_type == ANYNONARRAYOID || decl_type == ANYENUMOID)
    {
      have_anyelement = true;
      if (decl_type == ANYNONARRAYOID)
      {
        have_anynonarray = true;
      }
      else if (decl_type == ANYENUMOID)
      {
        have_anyenum = true;
      }
      if (actual_type == UNKNOWNOID)
      {
        continue;
      }
      if (OidIsValid(elem_typeid) && actual_type != elem_typeid)
      {
        return false;
      }
      elem_typeid = actual_type;
    }
    else if (decl_type == ANYARRAYOID)
    {
      if (actual_type == UNKNOWNOID)
      {
        continue;
      }
      actual_type = getBaseType(actual_type);                      
      if (OidIsValid(array_typeid) && actual_type != array_typeid)
      {
        return false;
      }
      array_typeid = actual_type;
    }
    else if (decl_type == ANYRANGEOID)
    {
      if (actual_type == UNKNOWNOID)
      {
        continue;
      }
      actual_type = getBaseType(actual_type);                      
      if (OidIsValid(range_typeid) && actual_type != range_typeid)
      {
        return false;
      }
      range_typeid = actual_type;
    }
  }

                                                                    
  if (OidIsValid(array_typeid))
  {
    if (array_typeid == ANYARRAYOID)
    {
                                                                   
      if (have_anyelement)
      {
        return false;
      }
      return true;
    }

    array_typelem = get_element_type(array_typeid);
    if (!OidIsValid(array_typelem))
    {
      return false;                                    
    }

    if (!OidIsValid(elem_typeid))
    {
         
                                                                       
         
      elem_typeid = array_typelem;
    }
    else if (array_typelem != elem_typeid)
    {
                                        
      return false;
    }
  }

                                                                    
  if (OidIsValid(range_typeid))
  {
    range_typelem = get_range_subtype(range_typeid);
    if (!OidIsValid(range_typelem))
    {
      return false;                                   
    }

    if (!OidIsValid(elem_typeid))
    {
         
                                                                       
         
      elem_typeid = range_typelem;
    }
    else if (range_typelem != elem_typeid)
    {
                                        
      return false;
    }
  }

  if (have_anynonarray)
  {
                                                                          
    if (type_is_array_domain(elem_typeid))
    {
      return false;
    }
  }

  if (have_anyenum)
  {
                                                
    if (!type_is_enum(elem_typeid))
    {
      return false;
    }
  }

                   
  return true;
}

   
                                      
                                                              
                                             
   
                                                                      
                                                                       
                                                                   
                                     
   
                                                                         
                                                                      
                                                                      
                                                                 
   
                                                                          
                                            
   
                                                                        
                                                           
                                                                           
                                                                   
                                                                               
                                                                      
                                                                         
                                                                              
                                                                     
                                                                             
                                                                       
                                                                     
                                                                        
                                                                            
                                                           
                                                                                
                                                                               
                                                                             
                                                                            
                       
                                                                            
                                                                          
                                                   
                                                                          
                                                                       
                                                         
                                                                               
                                                                               
                                                                           
                                      
   
                                                                       
                                                                       
                                                                             
                                          
   
                                                                              
                                                                         
                                                                             
                                                                         
                                                                           
                                                         
   
                                                                           
                                                                            
                                                                         
                                                                           
                                                                           
                                                                           
                                                                         
                                                                      
   
Oid
enforce_generic_type_consistency(const Oid *actual_arg_types, Oid *declared_arg_types, int nargs, Oid rettype, bool allow_poly)
{
  int j;
  bool have_generics = false;
  bool have_unknowns = false;
  Oid elem_typeid = InvalidOid;
  Oid array_typeid = InvalidOid;
  Oid range_typeid = InvalidOid;
  Oid array_typelem;
  Oid range_typelem;
  bool have_anyelement = (rettype == ANYELEMENTOID || rettype == ANYNONARRAYOID || rettype == ANYENUMOID);
  bool have_anynonarray = (rettype == ANYNONARRAYOID);
  bool have_anyenum = (rettype == ANYENUMOID);

     
                                                                            
                                                       
     
  for (j = 0; j < nargs; j++)
  {
    Oid decl_type = declared_arg_types[j];
    Oid actual_type = actual_arg_types[j];

    if (decl_type == ANYELEMENTOID || decl_type == ANYNONARRAYOID || decl_type == ANYENUMOID)
    {
      have_generics = have_anyelement = true;
      if (decl_type == ANYNONARRAYOID)
      {
        have_anynonarray = true;
      }
      else if (decl_type == ANYENUMOID)
      {
        have_anyenum = true;
      }
      if (actual_type == UNKNOWNOID)
      {
        have_unknowns = true;
        continue;
      }
      if (allow_poly && decl_type == actual_type)
      {
        continue;                              
      }
      if (OidIsValid(elem_typeid) && actual_type != elem_typeid)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("arguments declared \"anyelement\" are not all alike"), errdetail("%s versus %s", format_type_be(elem_typeid), format_type_be(actual_type))));
      }
      elem_typeid = actual_type;
    }
    else if (decl_type == ANYARRAYOID)
    {
      have_generics = true;
      if (actual_type == UNKNOWNOID)
      {
        have_unknowns = true;
        continue;
      }
      if (allow_poly && decl_type == actual_type)
      {
        continue;                              
      }
      actual_type = getBaseType(actual_type);                      
      if (OidIsValid(array_typeid) && actual_type != array_typeid)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("arguments declared \"anyarray\" are not all alike"), errdetail("%s versus %s", format_type_be(array_typeid), format_type_be(actual_type))));
      }
      array_typeid = actual_type;
    }
    else if (decl_type == ANYRANGEOID)
    {
      have_generics = true;
      if (actual_type == UNKNOWNOID)
      {
        have_unknowns = true;
        continue;
      }
      if (allow_poly && decl_type == actual_type)
      {
        continue;                              
      }
      actual_type = getBaseType(actual_type);                      
      if (OidIsValid(range_typeid) && actual_type != range_typeid)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("arguments declared \"anyrange\" are not all alike"), errdetail("%s versus %s", format_type_be(range_typeid), format_type_be(actual_type))));
      }
      range_typeid = actual_type;
    }
  }

     
                                                                      
                                                                    
     
  if (!have_generics)
  {
    return rettype;
  }

                                                                    
  if (OidIsValid(array_typeid))
  {
    if (array_typeid == ANYARRAYOID && !have_anyelement)
    {
                                                                   
      array_typelem = ANYELEMENTOID;
    }
    else
    {
      array_typelem = get_element_type(array_typeid);
      if (!OidIsValid(array_typelem))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not an array but type %s", "anyarray", format_type_be(array_typeid))));
      }
    }

    if (!OidIsValid(elem_typeid))
    {
         
                                                                       
         
      elem_typeid = array_typelem;
    }
    else if (array_typelem != elem_typeid)
    {
                                        
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not consistent with argument declared %s", "anyarray", "anyelement"), errdetail("%s versus %s", format_type_be(array_typeid), format_type_be(elem_typeid))));
    }
  }

                                                                    
  if (OidIsValid(range_typeid))
  {
    if (range_typeid == ANYRANGEOID && !have_anyelement)
    {
                                                                   
      range_typelem = ANYELEMENTOID;
    }
    else
    {
      range_typelem = get_range_subtype(range_typeid);
      if (!OidIsValid(range_typelem))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not a range type but type %s", "anyrange", format_type_be(range_typeid))));
      }
    }

    if (!OidIsValid(elem_typeid))
    {
         
                                                                       
         
      elem_typeid = range_typelem;
    }
    else if (range_typelem != elem_typeid)
    {
                                        
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not consistent with argument declared %s", "anyrange", "anyelement"), errdetail("%s versus %s", format_type_be(range_typeid), format_type_be(elem_typeid))));
    }
  }

  if (!OidIsValid(elem_typeid))
  {
    if (allow_poly)
    {
      elem_typeid = ANYELEMENTOID;
      array_typeid = ANYARRAYOID;
      range_typeid = ANYRANGEOID;
    }
    else
    {
                                                                       
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("could not determine polymorphic type because input has type %s", "unknown")));
    }
  }

  if (have_anynonarray && elem_typeid != ANYELEMENTOID)
  {
                                                                          
    if (type_is_array_domain(elem_typeid))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type matched to anynonarray is an array type: %s", format_type_be(elem_typeid))));
    }
  }

  if (have_anyenum && elem_typeid != ANYELEMENTOID)
  {
                                                
    if (!type_is_enum(elem_typeid))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type matched to anyenum is not an enum type: %s", format_type_be(elem_typeid))));
    }
  }

     
                                                                   
     
  if (have_unknowns)
  {
    for (j = 0; j < nargs; j++)
    {
      Oid decl_type = declared_arg_types[j];
      Oid actual_type = actual_arg_types[j];

      if (actual_type != UNKNOWNOID)
      {
        continue;
      }

      if (decl_type == ANYELEMENTOID || decl_type == ANYNONARRAYOID || decl_type == ANYENUMOID)
      {
        declared_arg_types[j] = elem_typeid;
      }
      else if (decl_type == ANYARRAYOID)
      {
        if (!OidIsValid(array_typeid))
        {
          array_typeid = get_array_type(elem_typeid);
          if (!OidIsValid(array_typeid))
          {
            ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(elem_typeid))));
          }
        }
        declared_arg_types[j] = array_typeid;
      }
      else if (decl_type == ANYRANGEOID)
      {
        if (!OidIsValid(range_typeid))
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find range type for data type %s", format_type_be(elem_typeid))));
        }
        declared_arg_types[j] = range_typeid;
      }
    }
  }

                                                               
  if (rettype == ANYARRAYOID)
  {
    if (!OidIsValid(array_typeid))
    {
      array_typeid = get_array_type(elem_typeid);
      if (!OidIsValid(array_typeid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(elem_typeid))));
      }
    }
    return array_typeid;
  }

                                                               
  if (rettype == ANYRANGEOID)
  {
    if (!OidIsValid(range_typeid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find range type for data type %s", format_type_be(elem_typeid))));
    }
    return range_typeid;
  }

                                                                 
  if (rettype == ANYELEMENTOID || rettype == ANYNONARRAYOID || rettype == ANYENUMOID)
  {
    return elem_typeid;
  }

                                                                          
  return rettype;
}

   
                          
                                                                
                                                        
   
                                                              
                                                                     
                                                     
   
                                                                      
                                                                        
                                                                       
   
Oid
resolve_generic_type(Oid declared_type, Oid context_actual_type, Oid context_declared_type)
{
  if (declared_type == ANYARRAYOID)
  {
    if (context_declared_type == ANYARRAYOID)
    {
         
                                                                       
                                              
         
      Oid context_base_type = getBaseType(context_actual_type);
      Oid array_typelem = get_element_type(context_base_type);

      if (!OidIsValid(array_typelem))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not an array but type %s", "anyarray", format_type_be(context_base_type))));
      }
      return context_base_type;
    }
    else if (context_declared_type == ANYELEMENTOID || context_declared_type == ANYNONARRAYOID || context_declared_type == ANYENUMOID || context_declared_type == ANYRANGEOID)
    {
                                                           
      Oid array_typeid = get_array_type(context_actual_type);

      if (!OidIsValid(array_typeid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(context_actual_type))));
      }
      return array_typeid;
    }
  }
  else if (declared_type == ANYELEMENTOID || declared_type == ANYNONARRAYOID || declared_type == ANYENUMOID || declared_type == ANYRANGEOID)
  {
    if (context_declared_type == ANYARRAYOID)
    {
                                                             
      Oid context_base_type = getBaseType(context_actual_type);
      Oid array_typelem = get_element_type(context_base_type);

      if (!OidIsValid(array_typelem))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not an array but type %s", "anyarray", format_type_be(context_base_type))));
      }
      return array_typelem;
    }
    else if (context_declared_type == ANYRANGEOID)
    {
                                                             
      Oid context_base_type = getBaseType(context_actual_type);
      Oid range_typelem = get_range_subtype(context_base_type);

      if (!OidIsValid(range_typelem))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not a range type but type %s", "anyrange", format_type_be(context_base_type))));
      }
      return range_typelem;
    }
    else if (context_declared_type == ANYELEMENTOID || context_declared_type == ANYNONARRAYOID || context_declared_type == ANYENUMOID)
    {
                                                                  
      return context_actual_type;
    }
  }
  else
  {
                                                             
    return declared_type;
  }
                                                                      
                                                                
  elog(ERROR, "could not determine polymorphic type because context isn't polymorphic");
  return InvalidOid;                          
}

                  
                                                 
   
                                                 
   
TYPCATEGORY
TypeCategory(Oid type)
{
  char typcategory;
  bool typispreferred;

  get_type_category_preferred(type, &typcategory, &typispreferred);
  Assert(typcategory != TYPCATEGORY_INVALID);
  return (TYPCATEGORY)typcategory;
}

                     
                                                                   
   
                                                                            
                                                                      
             
   
bool
IsPreferredType(TYPCATEGORY category, Oid type)
{
  char typcategory;
  bool typispreferred;

  get_type_category_preferred(type, &typcategory, &typispreferred);
  if (category == typcategory || category == TYPCATEGORY_INVALID)
  {
    return typispreferred;
  }
  else
  {
    return false;
  }
}

                       
                                                        
   
                                                                       
                                                                          
                                                                            
                                                                            
                                               
   
                                                                         
                                                                    
                                                                          
                                                                            
                                                                           
                                                                           
          
   
                                                                        
                                                                            
                                                 
   
bool
IsBinaryCoercible(Oid srctype, Oid targettype)
{
  HeapTuple tuple;
  Form_pg_cast castForm;
  bool result;

                              
  if (srctype == targettype)
  {
    return true;
  }

                                                  
  if (targettype == ANYOID || targettype == ANYELEMENTOID)
  {
    return true;
  }

                                                       
  if (OidIsValid(srctype))
  {
    srctype = getBaseType(srctype);
  }

                                                       
  if (srctype == targettype)
  {
    return true;
  }

                                                           
  if (targettype == ANYARRAYOID)
  {
    if (type_is_array(srctype))
    {
      return true;
    }
  }

                                                                  
  if (targettype == ANYNONARRAYOID)
  {
    if (!type_is_array(srctype))
    {
      return true;
    }
  }

                                                         
  if (targettype == ANYENUMOID)
  {
    if (type_is_enum(srctype))
    {
      return true;
    }
  }

                                                           
  if (targettype == ANYRANGEOID)
  {
    if (type_is_range(srctype))
    {
      return true;
    }
  }

                                                             
  if (targettype == RECORDOID)
  {
    if (ISCOMPLEX(srctype))
    {
      return true;
    }
  }

                                                                     
  if (targettype == RECORDARRAYOID)
  {
    if (is_complex_array(srctype))
    {
      return true;
    }
  }

                            
  tuple = SearchSysCache2(CASTSOURCETARGET, ObjectIdGetDatum(srctype), ObjectIdGetDatum(targettype));
  if (!HeapTupleIsValid(tuple))
  {
    return false;              
  }
  castForm = (Form_pg_cast)GETSTRUCT(tuple);

  result = (castForm->castmethod == COERCION_METHOD_BINARY && castForm->castcontext == COERCION_CODE_IMPLICIT);

  ReleaseSysCache(tuple);

  return result;
}

   
                         
                                                   
   
                                                                           
                                                                       
                      
   
                                                   
   
                                  
                                                           
                                   
                                                                       
                                                                         
                                   
                                                           
                                   
                                                      
                                   
   
                                                                             
                                                                            
                                                                             
                                            
   
CoercionPathType
find_coercion_pathway(Oid targetTypeId, Oid sourceTypeId, CoercionContext ccontext, Oid *funcid)
{
  CoercionPathType result = COERCION_PATH_NONE;
  HeapTuple tuple;

  *funcid = InvalidOid;

                                                                      
  if (OidIsValid(sourceTypeId))
  {
    sourceTypeId = getBaseType(sourceTypeId);
  }
  if (OidIsValid(targetTypeId))
  {
    targetTypeId = getBaseType(targetTypeId);
  }

                                                                
  if (sourceTypeId == targetTypeId)
  {
    return COERCION_PATH_RELABELTYPE;
  }

                       
  tuple = SearchSysCache2(CASTSOURCETARGET, ObjectIdGetDatum(sourceTypeId), ObjectIdGetDatum(targetTypeId));

  if (HeapTupleIsValid(tuple))
  {
    Form_pg_cast castForm = (Form_pg_cast)GETSTRUCT(tuple);
    CoercionContext castcontext;

                                                                    
    switch (castForm->castcontext)
    {
    case COERCION_CODE_IMPLICIT:
      castcontext = COERCION_IMPLICIT;
      break;
    case COERCION_CODE_ASSIGNMENT:
      castcontext = COERCION_ASSIGNMENT;
      break;
    case COERCION_CODE_EXPLICIT:
      castcontext = COERCION_EXPLICIT;
      break;
    default:
      elog(ERROR, "unrecognized castcontext: %d", (int)castForm->castcontext);
      castcontext = 0;                          
      break;
    }

                                                            
    if (ccontext >= castcontext)
    {
      switch (castForm->castmethod)
      {
      case COERCION_METHOD_FUNCTION:
        result = COERCION_PATH_FUNC;
        *funcid = castForm->castfunc;
        break;
      case COERCION_METHOD_INOUT:
        result = COERCION_PATH_COERCEVIAIO;
        break;
      case COERCION_METHOD_BINARY:
        result = COERCION_PATH_RELABELTYPE;
        break;
      default:
        elog(ERROR, "unrecognized castmethod: %d", (int)castForm->castmethod);
        break;
      }
    }

    ReleaseSysCache(tuple);
  }
  else
  {
       
                                                                          
                                                                         
                                                                   
       
                                                                   
                                                                          
                                                                           
                                                                 
                                                                      
                                                      
       
    if (targetTypeId != OIDVECTOROID && targetTypeId != INT2VECTOROID)
    {
      Oid targetElem;
      Oid sourceElem;

      if ((targetElem = get_element_type(targetTypeId)) != InvalidOid && (sourceElem = get_element_type(sourceTypeId)) != InvalidOid)
      {
        CoercionPathType elempathtype;
        Oid elemfuncid;

        elempathtype = find_coercion_pathway(targetElem, sourceElem, ccontext, &elemfuncid);
        if (elempathtype != COERCION_PATH_NONE)
        {
          result = COERCION_PATH_ARRAYCOERCE;
        }
      }
    }

       
                                                                           
                                                                           
                                                                     
                                                                          
                                                                         
                                                                          
                                                                     
                                                                     
       
    if (result == COERCION_PATH_NONE)
    {
      if (ccontext >= COERCION_ASSIGNMENT && TypeCategory(targetTypeId) == TYPCATEGORY_STRING)
      {
        result = COERCION_PATH_COERCEVIAIO;
      }
      else if (ccontext >= COERCION_EXPLICIT && TypeCategory(sourceTypeId) == TYPCATEGORY_STRING)
      {
        result = COERCION_PATH_COERCEVIAIO;
      }
    }
  }

  return result;
}

   
                                                                              
   
                                                                          
                                 
   
                                                                    
   
                                                                            
                                                                          
                                                                          
                                                                       
                                                                          
                                                                     
   
                                                                               
                     
                                                 
                                                              
                                                                       
   
CoercionPathType
find_typmod_coercion_function(Oid typeId, Oid *funcid)
{
  CoercionPathType result;
  Type targetType;
  Form_pg_type typeForm;
  HeapTuple tuple;

  *funcid = InvalidOid;
  result = COERCION_PATH_FUNC;

  targetType = typeidType(typeId);
  typeForm = (Form_pg_type)GETSTRUCT(targetType);

                                      
  if (typeForm->typelem != InvalidOid && typeForm->typlen == -1)
  {
                                                       
    typeId = typeForm->typelem;
    result = COERCION_PATH_ARRAYCOERCE;
  }
  ReleaseSysCache(targetType);

                       
  tuple = SearchSysCache2(CASTSOURCETARGET, ObjectIdGetDatum(typeId), ObjectIdGetDatum(typeId));

  if (HeapTupleIsValid(tuple))
  {
    Form_pg_cast castForm = (Form_pg_cast)GETSTRUCT(tuple);

    *funcid = castForm->castfunc;
    ReleaseSysCache(tuple);
  }

  if (!OidIsValid(*funcid))
  {
    result = COERCION_PATH_NONE;
  }

  return result;
}

   
                    
                                        
   
                                                                          
                         
   
static bool
is_complex_array(Oid typid)
{
  Oid elemtype = get_element_type(typid);

  return (OidIsValid(elemtype) && ISCOMPLEX(elemtype));
}

   
                                                                    
                                                                            
                                                                       
   
static bool
typeIsOfTypedTable(Oid reltypeId, Oid reloftypeId)
{
  Oid relid = typeOrDomainTypeRelid(reltypeId);
  bool result = false;

  if (relid)
  {
    HeapTuple tp;
    Form_pg_class reltup;

    tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(tp))
    {
      elog(ERROR, "cache lookup failed for relation %u", relid);
    }

    reltup = (Form_pg_class)GETSTRUCT(tp);
    if (reltup->reloftype == reloftypeId)
    {
      result = true;
    }

    ReleaseSysCache(tp);
  }

  return result;
}
