                                                                            
   
             
                                                                      
                                                                
   
                                                                
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "nodes/nodeFuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/regproc.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

typedef struct polymorphic_actuals
{
  Oid anyelement_type;                                   
  Oid anyarray_type;                                   
  Oid anyrange_type;                                   
} polymorphic_actuals;

static void
shutdown_MultiFuncCall(Datum arg);
static TypeFuncClass
internal_get_result_type(Oid funcid, Node *call_expr, ReturnSetInfo *rsinfo, Oid *resultTypeId, TupleDesc *resultTupleDesc);
static void
resolve_anyelement_from_others(polymorphic_actuals *actuals);
static void
resolve_anyarray_from_others(polymorphic_actuals *actuals);
static void
resolve_anyrange_from_others(polymorphic_actuals *actuals);
static bool
resolve_polymorphic_tupdesc(TupleDesc tupdesc, oidvector *declared_args, Node *call_expr);
static TypeFuncClass
get_type_func_class(Oid typid, Oid *base_typeid);

   
                      
                                                  
                                                     
                      
   
FuncCallContext *
init_MultiFuncCall(PG_FUNCTION_ARGS)
{
  FuncCallContext *retval;

     
                                               
     
  if (fcinfo->resultinfo == NULL || !IsA(fcinfo->resultinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }

  if (fcinfo->flinfo->fn_extra == NULL)
  {
       
                  
       
    ReturnSetInfo *rsi = (ReturnSetInfo *)fcinfo->resultinfo;
    MemoryContext multi_call_ctx;

       
                                                                    
       
    multi_call_ctx = AllocSetContextCreate(fcinfo->flinfo->fn_mcxt, "SRF multi-call context", ALLOCSET_SMALL_SIZES);

       
                                                      
       
    retval = (FuncCallContext *)MemoryContextAllocZero(multi_call_ctx, sizeof(FuncCallContext));

       
                               
       
    retval->call_cntr = 0;
    retval->max_calls = 0;
    retval->user_fctx = NULL;
    retval->attinmeta = NULL;
    retval->tuple_desc = NULL;
    retval->multi_call_memory_ctx = multi_call_ctx;

       
                                           
       
    fcinfo->flinfo->fn_extra = retval;

       
                                                                          
                      
       
    RegisterExprContextCallback(rsi->econtext, shutdown_MultiFuncCall, PointerGetDatum(fcinfo->flinfo));
  }
  else
  {
                                     
    elog(ERROR, "init_MultiFuncCall cannot be called more than once");

                                                
    retval = NULL;
  }

  return retval;
}

   
                     
   
                                    
   
FuncCallContext *
per_MultiFuncCall(PG_FUNCTION_ARGS)
{
  FuncCallContext *retval = (FuncCallContext *)fcinfo->flinfo->fn_extra;

  return retval;
}

   
                     
                                     
   
void
end_MultiFuncCall(PG_FUNCTION_ARGS, FuncCallContext *funcctx)
{
  ReturnSetInfo *rsi = (ReturnSetInfo *)fcinfo->resultinfo;

                                        
  UnregisterExprContextCallback(rsi->econtext, shutdown_MultiFuncCall, PointerGetDatum(fcinfo->flinfo));

                                      
  shutdown_MultiFuncCall(PointerGetDatum(fcinfo->flinfo));
}

   
                          
                                                          
   
static void
shutdown_MultiFuncCall(Datum arg)
{
  FmgrInfo *flinfo = (FmgrInfo *)DatumGetPointer(arg);
  FuncCallContext *funcctx = (FuncCallContext *)flinfo->fn_extra;

                          
  flinfo->fn_extra = NULL;

     
                                                                  
                            
     
  MemoryContextDelete(funcctx->multi_call_memory_ctx);
}

   
                        
                                                                        
                                                                         
                                                                       
                                                                    
                                                                        
                                             
   
                                                                        
                                                                        
                                                                        
                                                                             
   
                                                                        
                                                                         
                                                           
   
                                                                          
                                               
   
TypeFuncClass
get_call_result_type(FunctionCallInfo fcinfo, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{
  return internal_get_result_type(fcinfo->flinfo->fn_oid, fcinfo->flinfo->fn_expr, (ReturnSetInfo *)fcinfo->resultinfo, resultTypeId, resultTupleDesc);
}

   
                        
                                                           
   
TypeFuncClass
get_expr_result_type(Node *expr, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{
  TypeFuncClass result;

  if (expr && IsA(expr, FuncExpr))
  {
    result = internal_get_result_type(((FuncExpr *)expr)->funcid, expr, NULL, resultTypeId, resultTupleDesc);
  }
  else if (expr && IsA(expr, OpExpr))
  {
    result = internal_get_result_type(get_opcode(((OpExpr *)expr)->opno), expr, NULL, resultTypeId, resultTupleDesc);
  }
  else
  {
                                                                     
    Oid typid = exprType(expr);
    Oid base_typid;

    if (resultTypeId)
    {
      *resultTypeId = typid;
    }
    if (resultTupleDesc)
    {
      *resultTupleDesc = NULL;
    }
    result = get_type_func_class(typid, &base_typid);
    if ((result == TYPEFUNC_COMPOSITE || result == TYPEFUNC_COMPOSITE_DOMAIN) && resultTupleDesc)
    {
      *resultTupleDesc = lookup_rowtype_tupdesc_copy(base_typid, -1);
    }
  }

  return result;
}

   
                        
                                                  
   
                                                                          
   
TypeFuncClass
get_func_result_type(Oid functionId, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{
  return internal_get_result_type(functionId, NULL, NULL, resultTypeId, resultTupleDesc);
}

   
                                                                         
   
                                                                            
                                                                   
                                                                          
                              
   
static TypeFuncClass
internal_get_result_type(Oid funcid, Node *call_expr, ReturnSetInfo *rsinfo, Oid *resultTypeId, TupleDesc *resultTupleDesc)
{
  TypeFuncClass result;
  HeapTuple tp;
  Form_pg_proc procform;
  Oid rettype;
  Oid base_rettype;
  TupleDesc tupdesc;

                                                                     
  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(tp);

  rettype = procform->prorettype;

                                                         
  tupdesc = build_function_result_tupdesc_t(tp);
  if (tupdesc)
  {
       
                                                                         
                                                                      
                   
       
    if (resultTypeId)
    {
      *resultTypeId = rettype;
    }

    if (resolve_polymorphic_tupdesc(tupdesc, &procform->proargtypes, call_expr))
    {
      if (tupdesc->tdtypeid == RECORDOID && tupdesc->tdtypmod < 0)
      {
        assign_record_type_typmod(tupdesc);
      }
      if (resultTupleDesc)
      {
        *resultTupleDesc = tupdesc;
      }
      result = TYPEFUNC_COMPOSITE;
    }
    else
    {
      if (resultTupleDesc)
      {
        *resultTupleDesc = NULL;
      }
      result = TYPEFUNC_RECORD;
    }

    ReleaseSysCache(tp);

    return result;
  }

     
                                                      
     
  if (IsPolymorphicType(rettype))
  {
    Oid newrettype = exprType(call_expr);

    if (newrettype == InvalidOid)                                      
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("could not determine actual result type for function \"%s\" declared to return type %s", NameStr(procform->proname), format_type_be(rettype))));
    }
    rettype = newrettype;
  }

  if (resultTypeId)
  {
    *resultTypeId = rettype;
  }
  if (resultTupleDesc)
  {
    *resultTupleDesc = NULL;                     
  }

                                
  result = get_type_func_class(rettype, &base_rettype);
  switch (result)
  {
  case TYPEFUNC_COMPOSITE:
  case TYPEFUNC_COMPOSITE_DOMAIN:
    if (resultTupleDesc)
    {
      *resultTupleDesc = lookup_rowtype_tupdesc_copy(base_rettype, -1);
    }
                                                                  
    break;
  case TYPEFUNC_SCALAR:
    break;
  case TYPEFUNC_RECORD:
                                                     
    if (rsinfo && IsA(rsinfo, ReturnSetInfo) && rsinfo->expectedDesc != NULL)
    {
      result = TYPEFUNC_COMPOSITE;
      if (resultTupleDesc)
      {
        *resultTupleDesc = rsinfo->expectedDesc;
      }
                                                      
    }
    break;
  default:
    break;
  }

  ReleaseSysCache(tp);

  return result;
}

   
                           
                                                                         
   
                                                                               
                                          
   
                                                                               
                                                      
   
TupleDesc
get_expr_result_tupdesc(Node *expr, bool noError)
{
  TupleDesc tupleDesc;
  TypeFuncClass functypclass;

  functypclass = get_expr_result_type(expr, NULL, &tupleDesc);

  if (functypclass == TYPEFUNC_COMPOSITE || functypclass == TYPEFUNC_COMPOSITE_DOMAIN)
  {
    return tupleDesc;
  }

  if (!noError)
  {
    Oid exprTypeId = exprType(expr);

    if (exprTypeId != RECORDOID)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(exprTypeId))));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("record type has not been registered")));
    }
  }

  return NULL;
}

   
                                                                   
   
                                                                         
                                                                          
                                                                       
                                                                            
   
static void
resolve_anyelement_from_others(polymorphic_actuals *actuals)
{
  if (OidIsValid(actuals->anyarray_type))
  {
                                                           
    Oid array_base_type = getBaseType(actuals->anyarray_type);
    Oid array_typelem = get_element_type(array_base_type);

    if (!OidIsValid(array_typelem))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not an array but type %s", "anyarray", format_type_be(array_base_type))));
    }
    actuals->anyelement_type = array_typelem;
  }
  else if (OidIsValid(actuals->anyrange_type))
  {
                                                           
    Oid range_base_type = getBaseType(actuals->anyrange_type);
    Oid range_typelem = get_range_subtype(range_base_type);

    if (!OidIsValid(range_typelem))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("argument declared %s is not a range type but type %s", "anyrange", format_type_be(range_base_type))));
    }
    actuals->anyelement_type = range_typelem;
  }
  else
  {
    elog(ERROR, "could not determine polymorphic type");
  }
}

   
                                                                 
   
static void
resolve_anyarray_from_others(polymorphic_actuals *actuals)
{
                                                       
  if (!OidIsValid(actuals->anyelement_type))
  {
    resolve_anyelement_from_others(actuals);
  }

  if (OidIsValid(actuals->anyelement_type))
  {
                                                         
    Oid array_typeid = get_array_type(actuals->anyelement_type);

    if (!OidIsValid(array_typeid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(actuals->anyelement_type))));
    }
    actuals->anyarray_type = array_typeid;
  }
  else
  {
    elog(ERROR, "could not determine polymorphic type");
  }
}

   
                                                                 
   
static void
resolve_anyrange_from_others(polymorphic_actuals *actuals)
{
     
                                                                         
                                                              
     
  elog(ERROR, "could not determine polymorphic type");
}

   
                                                                         
                                                                        
                                                              
                                                                            
                                                                          
                                                           
   
static bool
resolve_polymorphic_tupdesc(TupleDesc tupdesc, oidvector *declared_args, Node *call_expr)
{
  int natts = tupdesc->natts;
  int nargs = declared_args->dim1;
  bool have_polymorphic_result = false;
  bool have_anyelement_result = false;
  bool have_anyarray_result = false;
  bool have_anyrange_result = false;
  polymorphic_actuals poly_actuals;
  Oid anycollation = InvalidOid;
  int i;

                                                                  
  for (i = 0; i < natts; i++)
  {
    switch (TupleDescAttr(tupdesc, i)->atttypid)
    {
    case ANYELEMENTOID:
    case ANYNONARRAYOID:
    case ANYENUMOID:
      have_polymorphic_result = true;
      have_anyelement_result = true;
      break;
    case ANYARRAYOID:
      have_polymorphic_result = true;
      have_anyarray_result = true;
      break;
    case ANYRANGEOID:
      have_polymorphic_result = true;
      have_anyrange_result = true;
      break;
    default:
      break;
    }
  }
  if (!have_polymorphic_result)
  {
    return true;
  }

     
                                                                             
                                                                 
     
  if (!call_expr)
  {
    return false;              
  }

  memset(&poly_actuals, 0, sizeof(poly_actuals));

  for (i = 0; i < nargs; i++)
  {
    switch (declared_args->values[i])
    {
    case ANYELEMENTOID:
    case ANYNONARRAYOID:
    case ANYENUMOID:
      if (!OidIsValid(poly_actuals.anyelement_type))
      {
        poly_actuals.anyelement_type = get_call_expr_argtype(call_expr, i);
        if (!OidIsValid(poly_actuals.anyelement_type))
        {
          return false;
        }
      }
      break;
    case ANYARRAYOID:
      if (!OidIsValid(poly_actuals.anyarray_type))
      {
        poly_actuals.anyarray_type = get_call_expr_argtype(call_expr, i);
        if (!OidIsValid(poly_actuals.anyarray_type))
        {
          return false;
        }
      }
      break;
    case ANYRANGEOID:
      if (!OidIsValid(poly_actuals.anyrange_type))
      {
        poly_actuals.anyrange_type = get_call_expr_argtype(call_expr, i);
        if (!OidIsValid(poly_actuals.anyrange_type))
        {
          return false;
        }
      }
      break;
    default:
      break;
    }
  }

                                                          
  if (have_anyelement_result && !OidIsValid(poly_actuals.anyelement_type))
  {
    resolve_anyelement_from_others(&poly_actuals);
  }

  if (have_anyarray_result && !OidIsValid(poly_actuals.anyarray_type))
  {
    resolve_anyarray_from_others(&poly_actuals);
  }

  if (have_anyrange_result && !OidIsValid(poly_actuals.anyrange_type))
  {
    resolve_anyrange_from_others(&poly_actuals);
  }

     
                                                                          
                                                                           
                                                                             
                                        
     
  if (OidIsValid(poly_actuals.anyelement_type))
  {
    anycollation = get_typcollation(poly_actuals.anyelement_type);
  }
  else if (OidIsValid(poly_actuals.anyarray_type))
  {
    anycollation = get_typcollation(poly_actuals.anyarray_type);
  }

  if (OidIsValid(anycollation))
  {
       
                                                                         
                                                                        
                         
       
    Oid inputcollation = exprInputCollation(call_expr);

    if (OidIsValid(inputcollation))
    {
      anycollation = inputcollation;
    }
  }

                                                            
  for (i = 0; i < natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

    switch (att->atttypid)
    {
    case ANYELEMENTOID:
    case ANYNONARRAYOID:
    case ANYENUMOID:
      TupleDescInitEntry(tupdesc, i + 1, NameStr(att->attname), poly_actuals.anyelement_type, -1, 0);
      TupleDescInitEntryCollation(tupdesc, i + 1, anycollation);
      break;
    case ANYARRAYOID:
      TupleDescInitEntry(tupdesc, i + 1, NameStr(att->attname), poly_actuals.anyarray_type, -1, 0);
      TupleDescInitEntryCollation(tupdesc, i + 1, anycollation);
      break;
    case ANYRANGEOID:
      TupleDescInitEntry(tupdesc, i + 1, NameStr(att->attname), poly_actuals.anyrange_type, -1, 0);
                                                           
      break;
    default:
      break;
    }
  }

  return true;
}

   
                                                                           
                                                                             
                                                        
                                                           
   
                                                                               
                                                                            
   
                                                                                
   
bool
resolve_polymorphic_argtypes(int numargs, Oid *argtypes, char *argmodes, Node *call_expr)
{
  bool have_polymorphic_result = false;
  bool have_anyelement_result = false;
  bool have_anyarray_result = false;
  bool have_anyrange_result = false;
  polymorphic_actuals poly_actuals;
  int inargno;
  int i;

     
                                                                       
                                                                         
                       
     
  memset(&poly_actuals, 0, sizeof(poly_actuals));
  inargno = 0;
  for (i = 0; i < numargs; i++)
  {
    char argmode = argmodes ? argmodes[i] : PROARGMODE_IN;

    switch (argtypes[i])
    {
    case ANYELEMENTOID:
    case ANYNONARRAYOID:
    case ANYENUMOID:
      if (argmode == PROARGMODE_OUT || argmode == PROARGMODE_TABLE)
      {
        have_polymorphic_result = true;
        have_anyelement_result = true;
      }
      else
      {
        if (!OidIsValid(poly_actuals.anyelement_type))
        {
          poly_actuals.anyelement_type = get_call_expr_argtype(call_expr, inargno);
          if (!OidIsValid(poly_actuals.anyelement_type))
          {
            return false;
          }
        }
        argtypes[i] = poly_actuals.anyelement_type;
      }
      break;
    case ANYARRAYOID:
      if (argmode == PROARGMODE_OUT || argmode == PROARGMODE_TABLE)
      {
        have_polymorphic_result = true;
        have_anyarray_result = true;
      }
      else
      {
        if (!OidIsValid(poly_actuals.anyarray_type))
        {
          poly_actuals.anyarray_type = get_call_expr_argtype(call_expr, inargno);
          if (!OidIsValid(poly_actuals.anyarray_type))
          {
            return false;
          }
        }
        argtypes[i] = poly_actuals.anyarray_type;
      }
      break;
    case ANYRANGEOID:
      if (argmode == PROARGMODE_OUT || argmode == PROARGMODE_TABLE)
      {
        have_polymorphic_result = true;
        have_anyrange_result = true;
      }
      else
      {
        if (!OidIsValid(poly_actuals.anyrange_type))
        {
          poly_actuals.anyrange_type = get_call_expr_argtype(call_expr, inargno);
          if (!OidIsValid(poly_actuals.anyrange_type))
          {
            return false;
          }
        }
        argtypes[i] = poly_actuals.anyrange_type;
      }
      break;
    default:
      break;
    }
    if (argmode != PROARGMODE_OUT && argmode != PROARGMODE_TABLE)
    {
      inargno++;
    }
  }

             
  if (!have_polymorphic_result)
  {
    return true;
  }

                                                          
  if (have_anyelement_result && !OidIsValid(poly_actuals.anyelement_type))
  {
    resolve_anyelement_from_others(&poly_actuals);
  }

  if (have_anyarray_result && !OidIsValid(poly_actuals.anyarray_type))
  {
    resolve_anyarray_from_others(&poly_actuals);
  }

  if (have_anyrange_result && !OidIsValid(poly_actuals.anyrange_type))
  {
    resolve_anyrange_from_others(&poly_actuals);
  }

                                                             
  for (i = 0; i < numargs; i++)
  {
    switch (argtypes[i])
    {
    case ANYELEMENTOID:
    case ANYNONARRAYOID:
    case ANYENUMOID:
      argtypes[i] = poly_actuals.anyelement_type;
      break;
    case ANYARRAYOID:
      argtypes[i] = poly_actuals.anyarray_type;
      break;
    case ANYRANGEOID:
      argtypes[i] = poly_actuals.anyrange_type;
      break;
    default:
      break;
    }
  }

  return true;
}

   
                       
                                                            
                                                      
   
                                                                      
                                                                        
                                                   
   
static TypeFuncClass
get_type_func_class(Oid typid, Oid *base_typeid)
{
  *base_typeid = typid;

  switch (get_typtype(typid))
  {
  case TYPTYPE_COMPOSITE:
    return TYPEFUNC_COMPOSITE;
  case TYPTYPE_BASE:
  case TYPTYPE_ENUM:
  case TYPTYPE_RANGE:
    return TYPEFUNC_SCALAR;
  case TYPTYPE_DOMAIN:
    *base_typeid = typid = getBaseType(typid);
    if (get_typtype(typid) == TYPTYPE_COMPOSITE)
    {
      return TYPEFUNC_COMPOSITE_DOMAIN;
    }
    else                                             
    {
      return TYPEFUNC_SCALAR;
    }
  case TYPTYPE_PSEUDO:
    if (typid == RECORDOID)
    {
      return TYPEFUNC_RECORD;
    }

       
                                                                 
                                                                     
                                                                
                                 
       
    if (typid == VOIDOID || typid == CSTRINGOID)
    {
      return TYPEFUNC_SCALAR;
    }
    return TYPEFUNC_OTHER;
  }
                                    
  return TYPEFUNC_OTHER;
}

   
                     
   
                                                                         
                                                                  
                                                                     
                                                                     
                                                   
   
                                                                        
                                                          
   
int
get_func_arg_info(HeapTuple procTup, Oid **p_argtypes, char ***p_argnames, char **p_argmodes)
{
  Form_pg_proc procStruct = (Form_pg_proc)GETSTRUCT(procTup);
  Datum proallargtypes;
  Datum proargmodes;
  Datum proargnames;
  bool isNull;
  ArrayType *arr;
  int numargs;
  Datum *elems;
  int nelems;
  int i;

                                                                         
  proallargtypes = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_proallargtypes, &isNull);
  if (!isNull)
  {
       
                                                                        
                                                                
                                                                           
                            
       
    arr = DatumGetArrayTypeP(proallargtypes);                         
    numargs = ARR_DIMS(arr)[0];
    if (ARR_NDIM(arr) != 1 || numargs < 0 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
    {
      elog(ERROR, "proallargtypes is not a 1-D Oid array");
    }
    Assert(numargs >= procStruct->pronargs);
    *p_argtypes = (Oid *)palloc(numargs * sizeof(Oid));
    memcpy(*p_argtypes, ARR_DATA_PTR(arr), numargs * sizeof(Oid));
  }
  else
  {
                                               
    numargs = procStruct->proargtypes.dim1;
    Assert(numargs == procStruct->pronargs);
    *p_argtypes = (Oid *)palloc(numargs * sizeof(Oid));
    memcpy(*p_argtypes, procStruct->proargtypes.values, numargs * sizeof(Oid));
  }

                                        
  proargnames = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_proargnames, &isNull);
  if (isNull)
  {
    *p_argnames = NULL;
  }
  else
  {
    deconstruct_array(DatumGetArrayTypeP(proargnames), TEXTOID, -1, false, 'i', &elems, NULL, &nelems);
    if (nelems != numargs)                        
    {
      elog(ERROR, "proargnames must have the same number of elements as the function has arguments");
    }
    *p_argnames = (char **)palloc(sizeof(char *) * numargs);
    for (i = 0; i < numargs; i++)
    {
      (*p_argnames)[i] = TextDatumGetCString(elems[i]);
    }
  }

                                        
  proargmodes = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_proargmodes, &isNull);
  if (isNull)
  {
    *p_argmodes = NULL;
  }
  else
  {
    arr = DatumGetArrayTypeP(proargmodes);                         
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numargs || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != CHAROID)
    {
      elog(ERROR, "proargmodes is not a 1-D char array");
    }
    *p_argmodes = (char *)palloc(numargs * sizeof(char));
    memcpy(*p_argmodes, ARR_DATA_PTR(arr), numargs * sizeof(char));
  }

  return numargs;
}

   
                     
   
                                                                 
                                                                   
                     
   
int
get_func_trftypes(HeapTuple procTup, Oid **p_trftypes)
{
  Datum protrftypes;
  ArrayType *arr;
  int nelems;
  bool isNull;

  protrftypes = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_protrftypes, &isNull);
  if (!isNull)
  {
       
                                                                        
                                                                
                                                                           
                            
       
    arr = DatumGetArrayTypeP(protrftypes);                         
    nelems = ARR_DIMS(arr)[0];
    if (ARR_NDIM(arr) != 1 || nelems < 0 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
    {
      elog(ERROR, "protrftypes is not a 1-D Oid array");
    }
    *p_trftypes = (Oid *)palloc(nelems * sizeof(Oid));
    memcpy(*p_trftypes, ARR_DATA_PTR(arr), nelems * sizeof(Oid));

    return nelems;
  }
  else
  {
    return 0;
  }
}

   
                            
   
                                                                 
                                                      
   
                                                                     
                                                                    
                                                                    
   
int
get_func_input_arg_names(Datum proargnames, Datum proargmodes, char ***arg_names)
{
  ArrayType *arr;
  int numargs;
  Datum *argnames;
  char *argmodes;
  char **inargnames;
  int numinargs;
  int i;

                                      
  if (proargnames == PointerGetDatum(NULL))
  {
    *arg_names = NULL;
    return 0;
  }

     
                                                                            
                                                                         
                                                                
     
  arr = DatumGetArrayTypeP(proargnames);                         
  if (ARR_NDIM(arr) != 1 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != TEXTOID)
  {
    elog(ERROR, "proargnames is not a 1-D text array");
  }
  deconstruct_array(arr, TEXTOID, -1, false, 'i', &argnames, NULL, &numargs);
  if (proargmodes != PointerGetDatum(NULL))
  {
    arr = DatumGetArrayTypeP(proargmodes);                         
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numargs || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != CHAROID)
    {
      elog(ERROR, "proargmodes is not a 1-D char array");
    }
    argmodes = (char *)ARR_DATA_PTR(arr);
  }
  else
  {
    argmodes = NULL;
  }

                                                                         
  if (numargs <= 0)
  {
    *arg_names = NULL;
    return 0;
  }

                                    
  inargnames = (char **)palloc(numargs * sizeof(char *));
  numinargs = 0;
  for (i = 0; i < numargs; i++)
  {
    if (argmodes == NULL || argmodes[i] == PROARGMODE_IN || argmodes[i] == PROARGMODE_INOUT || argmodes[i] == PROARGMODE_VARIADIC)
    {
      char *pname = TextDatumGetCString(argnames[i]);

      if (pname[0] != '\0')
      {
        inargnames[numinargs] = pname;
      }
      else
      {
        inargnames[numinargs] = NULL;
      }
      numinargs++;
    }
  }

  *arg_names = inargnames;
  return numinargs;
}

   
                        
   
                                                                        
                                                                        
   
                                                                          
                           
   
char *
get_func_result_name(Oid functionId)
{
  char *result;
  HeapTuple procTuple;
  Datum proargmodes;
  Datum proargnames;
  bool isnull;
  ArrayType *arr;
  int numargs;
  char *argmodes;
  Datum *argnames;
  int numoutargs;
  int nargnames;
  int i;

                                              
  procTuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(functionId));
  if (!HeapTupleIsValid(procTuple))
  {
    elog(ERROR, "cache lookup failed for function %u", functionId);
  }

                                                         
  if (heap_attisnull(procTuple, Anum_pg_proc_proargmodes, NULL) || heap_attisnull(procTuple, Anum_pg_proc_proargnames, NULL))
  {
    result = NULL;
  }
  else
  {
                                       
    proargmodes = SysCacheGetAttr(PROCOID, procTuple, Anum_pg_proc_proargmodes, &isnull);
    Assert(!isnull);
    proargnames = SysCacheGetAttr(PROCOID, procTuple, Anum_pg_proc_proargnames, &isnull);
    Assert(!isnull);

       
                                                                        
                                                                           
                                                                    
               
       
    arr = DatumGetArrayTypeP(proargmodes);                         
    numargs = ARR_DIMS(arr)[0];
    if (ARR_NDIM(arr) != 1 || numargs < 0 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != CHAROID)
    {
      elog(ERROR, "proargmodes is not a 1-D char array");
    }
    argmodes = (char *)ARR_DATA_PTR(arr);
    arr = DatumGetArrayTypeP(proargnames);                         
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numargs || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != TEXTOID)
    {
      elog(ERROR, "proargnames is not a 1-D text array");
    }
    deconstruct_array(arr, TEXTOID, -1, false, 'i', &argnames, NULL, &nargnames);
    Assert(nargnames == numargs);

                                     
    result = NULL;
    numoutargs = 0;
    for (i = 0; i < numargs; i++)
    {
      if (argmodes[i] == PROARGMODE_IN || argmodes[i] == PROARGMODE_VARIADIC)
      {
        continue;
      }
      Assert(argmodes[i] == PROARGMODE_OUT || argmodes[i] == PROARGMODE_INOUT || argmodes[i] == PROARGMODE_TABLE);
      if (++numoutargs > 1)
      {
                                             
        result = NULL;
        break;
      }
      result = TextDatumGetCString(argnames[i]);
      if (result == NULL || result[0] == '\0')
      {
                                                  
        result = NULL;
        break;
      }
    }
  }

  ReleaseSysCache(procTuple);

  return result;
}

   
                                   
   
                                                                         
                                                                         
   
                                                                   
                       
   
TupleDesc
build_function_result_tupdesc_t(HeapTuple procTuple)
{
  Form_pg_proc procform = (Form_pg_proc)GETSTRUCT(procTuple);
  Datum proallargtypes;
  Datum proargmodes;
  Datum proargnames;
  bool isnull;

                                                                   
  if (procform->prorettype != RECORDOID)
  {
    return NULL;
  }

                                                   
  if (heap_attisnull(procTuple, Anum_pg_proc_proallargtypes, NULL) || heap_attisnull(procTuple, Anum_pg_proc_proargmodes, NULL))
  {
    return NULL;
  }

                                     
  proallargtypes = SysCacheGetAttr(PROCOID, procTuple, Anum_pg_proc_proallargtypes, &isnull);
  Assert(!isnull);
  proargmodes = SysCacheGetAttr(PROCOID, procTuple, Anum_pg_proc_proargmodes, &isnull);
  Assert(!isnull);
  proargnames = SysCacheGetAttr(PROCOID, procTuple, Anum_pg_proc_proargnames, &isnull);
  if (isnull)
  {
    proargnames = PointerGetDatum(NULL);                      
  }

  return build_function_result_tupdesc_d(procform->prokind, proallargtypes, proargmodes, proargnames);
}

   
                                   
   
                                                                        
                                                                   
                                                                         
                                                    
   
                                                                            
                                     
   
TupleDesc
build_function_result_tupdesc_d(char prokind, Datum proallargtypes, Datum proargmodes, Datum proargnames)
{
  TupleDesc desc;
  ArrayType *arr;
  int numargs;
  Oid *argtypes;
  char *argmodes;
  Datum *argnames = NULL;
  Oid *outargtypes;
  char **outargnames;
  int numoutargs;
  int nargnames;
  int i;

                                                  
  if (proallargtypes == PointerGetDatum(NULL) || proargmodes == PointerGetDatum(NULL))
  {
    return NULL;
  }

     
                                                                            
                                                                           
                                                                          
     
  arr = DatumGetArrayTypeP(proallargtypes);                         
  numargs = ARR_DIMS(arr)[0];
  if (ARR_NDIM(arr) != 1 || numargs < 0 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
  {
    elog(ERROR, "proallargtypes is not a 1-D Oid array");
  }
  argtypes = (Oid *)ARR_DATA_PTR(arr);
  arr = DatumGetArrayTypeP(proargmodes);                         
  if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numargs || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != CHAROID)
  {
    elog(ERROR, "proargmodes is not a 1-D char array");
  }
  argmodes = (char *)ARR_DATA_PTR(arr);
  if (proargnames != PointerGetDatum(NULL))
  {
    arr = DatumGetArrayTypeP(proargnames);                         
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numargs || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != TEXTOID)
    {
      elog(ERROR, "proargnames is not a 1-D text array");
    }
    deconstruct_array(arr, TEXTOID, -1, false, 'i', &argnames, NULL, &nargnames);
    Assert(nargnames == numargs);
  }

                                                                         
  if (numargs <= 0)
  {
    return NULL;
  }

                                               
  outargtypes = (Oid *)palloc(numargs * sizeof(Oid));
  outargnames = (char **)palloc(numargs * sizeof(char *));
  numoutargs = 0;
  for (i = 0; i < numargs; i++)
  {
    char *pname;

    if (argmodes[i] == PROARGMODE_IN || argmodes[i] == PROARGMODE_VARIADIC)
    {
      continue;
    }
    Assert(argmodes[i] == PROARGMODE_OUT || argmodes[i] == PROARGMODE_INOUT || argmodes[i] == PROARGMODE_TABLE);
    outargtypes[numoutargs] = argtypes[i];
    if (argnames)
    {
      pname = TextDatumGetCString(argnames[i]);
    }
    else
    {
      pname = NULL;
    }
    if (pname == NULL || pname[0] == '\0')
    {
                                                           
      pname = psprintf("column%d", numoutargs + 1);
    }
    outargnames[numoutargs] = pname;
    numoutargs++;
  }

     
                                                                        
                    
     
  if (numoutargs < 2 && prokind != PROKIND_PROCEDURE)
  {
    return NULL;
  }

  desc = CreateTemplateTupleDesc(numoutargs);
  for (i = 0; i < numoutargs; i++)
  {
    TupleDescInitEntry(desc, i + 1, outargnames[i], outargtypes[i], -1, 0);
  }

  return desc;
}

   
                            
   
                                                                  
   
                                                                     
                                                                   
                                                                     
   
TupleDesc
RelationNameGetTupleDesc(const char *relname)
{
  RangeVar *relvar;
  Relation rel;
  TupleDesc tupdesc;
  List *relname_list;

                                                    
  relname_list = stringToQualifiedNameList(relname);
  relvar = makeRangeVarFromNameList(relname_list);
  rel = relation_openrv(relvar, AccessShareLock);
  tupdesc = CreateTupleDescCopy(RelationGetDescr(rel));
  relation_close(rel, AccessShareLock);

  return tupdesc;
}

   
                    
   
                                                                      
                                                                     
                                                                       
                             
   
                                                                        
                                                                        
                                                                   
                                                    
   
                                                                     
   
TupleDesc
TypeGetTupleDesc(Oid typeoid, List *colaliases)
{
  Oid base_typeoid;
  TypeFuncClass functypclass = get_type_func_class(typeoid, &base_typeoid);
  TupleDesc tupdesc = NULL;

     
                                                                  
                                                                          
                                                                     
                                           
     
  if (functypclass == TYPEFUNC_COMPOSITE)
  {
                                                      
    tupdesc = lookup_rowtype_tupdesc_copy(base_typeoid, -1);

    if (colaliases != NIL)
    {
      int natts = tupdesc->natts;
      int varattno;

                                                                
      if (list_length(colaliases) != natts)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("number of aliases does not match number of columns")));
      }

                                       
      for (varattno = 0; varattno < natts; varattno++)
      {
        char *label = strVal(list_nth(colaliases, varattno));
        Form_pg_attribute attr = TupleDescAttr(tupdesc, varattno);

        if (label != NULL)
        {
          namestrcpy(&(attr->attname), label);
        }
      }

                                                          
      tupdesc->tdtypeid = RECORDOID;
      tupdesc->tdtypmod = -1;
    }
  }
  else if (functypclass == TYPEFUNC_SCALAR)
  {
                                     
    char *attname;

                                                   
    if (colaliases == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("no column alias was provided")));
    }

                                         
    if (list_length(colaliases) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("number of aliases does not match number of columns")));
    }

                                  
    attname = strVal(linitial(colaliases));

    tupdesc = CreateTemplateTupleDesc(1);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, attname, typeoid, -1, 0);
  }
  else if (functypclass == TYPEFUNC_RECORD)
  {
                                                                    
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("could not determine row description for function returning record")));
  }
  else
  {
                                                                  
    elog(ERROR, "function in FROM has unsupported return type");
  }

  return tupdesc;
}

   
                         
   
                                                                        
                                                                          
                                                                         
                                                                            
                                                                           
                                                                          
                  
   
                                                                         
                            
   
int
extract_variadic_args(FunctionCallInfo fcinfo, int variadic_start, bool convert_unknown, Datum **args, Oid **types, bool **nulls)
{
  bool variadic = get_fn_expr_variadic(fcinfo->flinfo);
  Datum *args_res;
  bool *nulls_res;
  Oid *types_res;
  int nargs, i;

  *args = NULL;
  *types = NULL;
  *nulls = NULL;

  if (variadic)
  {
    ArrayType *array_in;
    Oid element_type;
    bool typbyval;
    char typalign;
    int16 typlen;

    Assert(PG_NARGS() == variadic_start + 1);

    if (PG_ARGISNULL(variadic_start))
    {
      return -1;
    }

    array_in = PG_GETARG_ARRAYTYPE_P(variadic_start);
    element_type = ARR_ELEMTYPE(array_in);

    get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);
    deconstruct_array(array_in, element_type, typlen, typbyval, typalign, &args_res, &nulls_res, &nargs);

                                                          
    types_res = (Oid *)palloc0(nargs * sizeof(Oid));
    for (i = 0; i < nargs; i++)
    {
      types_res[i] = element_type;
    }
  }
  else
  {
    nargs = PG_NARGS() - variadic_start;
    Assert(nargs > 0);
    nulls_res = (bool *)palloc0(nargs * sizeof(bool));
    args_res = (Datum *)palloc0(nargs * sizeof(Datum));
    types_res = (Oid *)palloc0(nargs * sizeof(Oid));

    for (i = 0; i < nargs; i++)
    {
      nulls_res[i] = PG_ARGISNULL(i + variadic_start);
      types_res[i] = get_fn_expr_argtype(fcinfo->flinfo, i + variadic_start);

         
                                                                        
                                                                   
                                                                         
                                                                         
                                                  
         
      if (convert_unknown && types_res[i] == UNKNOWNOID && get_fn_expr_arg_stable(fcinfo->flinfo, i + variadic_start))
      {
        types_res[i] = TEXTOID;

        if (PG_ARGISNULL(i + variadic_start))
        {
          args_res[i] = (Datum)0;
        }
        else
        {
          args_res[i] = CStringGetTextDatum(PG_GETARG_POINTER(i + variadic_start));
        }
      }
      else
      {
                                                                
        args_res[i] = PG_GETARG_DATUM(i + variadic_start);
      }

      if (!OidIsValid(types_res[i]) || (convert_unknown && types_res[i] == UNKNOWNOID))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not determine data type for argument %d", i + 1)));
      }
    }
  }

                       
  *args = args_res;
  *nulls = nulls_res;
  *types = types_res;

  return nargs;
}
