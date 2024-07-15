                                                                            
   
                  
                                                                   
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
lookup_agg_function(List *fnName, int nargs, Oid *input_types, Oid variadicArgType, Oid *rettype);

   
                   
   
ObjectAddress
AggregateCreate(const char *aggName, Oid aggNamespace, bool replace, char aggKind, int numArgs, int numDirectArgs, oidvector *parameterTypes, Datum allParameterTypes, Datum parameterModes, Datum parameterNames, List *parameterDefaults, Oid variadicArgType, List *aggtransfnName, List *aggfinalfnName, List *aggcombinefnName, List *aggserialfnName, List *aggdeserialfnName, List *aggmtransfnName, List *aggminvtransfnName, List *aggmfinalfnName, bool finalfnExtraArgs, bool mfinalfnExtraArgs, char finalfnModify, char mfinalfnModify, List *aggsortopName, Oid aggTransType, int32 aggTransSpace, Oid aggmTransType, int32 aggmTransSpace, const char *agginitval, const char *aggminitval, char proparallel)
{
  Relation aggdesc;
  HeapTuple tup;
  HeapTuple oldtup;
  bool nulls[Natts_pg_aggregate];
  Datum values[Natts_pg_aggregate];
  bool replaces[Natts_pg_aggregate];
  Form_pg_proc proc;
  Oid transfn;
  Oid finalfn = InvalidOid;                         
  Oid combinefn = InvalidOid;                       
  Oid serialfn = InvalidOid;                        
  Oid deserialfn = InvalidOid;                      
  Oid mtransfn = InvalidOid;                        
  Oid minvtransfn = InvalidOid;                     
  Oid mfinalfn = InvalidOid;                        
  Oid sortop = InvalidOid;                          
  Oid *aggArgTypes = parameterTypes->values;
  bool hasPolyArg;
  bool hasInternalArg;
  bool mtransIsStrict = false;
  Oid rettype;
  Oid finaltype;
  Oid fnArgs[FUNC_MAX_ARGS];
  int nargs_transfn;
  int nargs_finalfn;
  Oid procOid;
  TupleDesc tupDesc;
  int i;
  ObjectAddress myself, referenced;
  AclResult aclresult;

                                                       
  if (!aggName)
  {
    elog(ERROR, "no aggregate name supplied");
  }

  if (!aggtransfnName)
  {
    elog(ERROR, "aggregate must have a transition function");
  }

  if (numDirectArgs < 0 || numDirectArgs > numArgs)
  {
    elog(ERROR, "incorrect number of direct arguments for aggregate");
  }

     
                                                                        
                                                                           
                                                                         
     
  if (numArgs < 0 || numArgs > FUNC_MAX_ARGS - 1)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("aggregates cannot have more than %d argument", "aggregates cannot have more than %d arguments", FUNC_MAX_ARGS - 1, FUNC_MAX_ARGS - 1)));
  }

                                                    
  hasPolyArg = false;
  hasInternalArg = false;
  for (i = 0; i < numArgs; i++)
  {
    if (IsPolymorphicType(aggArgTypes[i]))
    {
      hasPolyArg = true;
    }
    else if (aggArgTypes[i] == INTERNALOID)
    {
      hasInternalArg = true;
    }
  }

     
                                                                            
                                                         
     
  if (IsPolymorphicType(aggTransType) && !hasPolyArg)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot determine transition data type"), errdetail("An aggregate using a polymorphic transition type must have at least one polymorphic argument.")));
  }

     
                                                     
     
  if (OidIsValid(aggmTransType) && IsPolymorphicType(aggmTransType) && !hasPolyArg)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot determine transition data type"), errdetail("An aggregate using a polymorphic transition type must have at least one polymorphic argument.")));
  }

     
                                                                         
                                                                          
                                                                            
                                                                           
                                                            
     
  if (AGGKIND_IS_ORDERED_SET(aggKind) && OidIsValid(variadicArgType) && variadicArgType != ANYOID)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("a variadic ordered-set aggregate must use VARIADIC type ANY")));
  }

     
                                                                          
                                                                          
                                                                           
                                                                         
                                                                            
                                                                            
                                                                             
                                                                             
                                                                            
                                                    
     
  if (aggKind == AGGKIND_HYPOTHETICAL && numDirectArgs < numArgs)
  {
    int numAggregatedArgs = numArgs - numDirectArgs;

    if (OidIsValid(variadicArgType) || numDirectArgs < numAggregatedArgs || memcmp(aggArgTypes + (numDirectArgs - numAggregatedArgs), aggArgTypes + numDirectArgs, numAggregatedArgs * sizeof(Oid)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("a hypothetical-set aggregate must have direct arguments matching its aggregated arguments")));
    }
  }

     
                                                                           
                                                                             
                                                                          
                                                                        
                                            
     
  if (AGGKIND_IS_ORDERED_SET(aggKind))
  {
    if (numDirectArgs < numArgs)
    {
      nargs_transfn = numArgs - numDirectArgs + 1;
    }
    else
    {
                                               
      Assert(variadicArgType != InvalidOid);
      nargs_transfn = 2;
    }
    fnArgs[0] = aggTransType;
    memcpy(fnArgs + 1, aggArgTypes + (numArgs - (nargs_transfn - 1)), (nargs_transfn - 1) * sizeof(Oid));
  }
  else
  {
    nargs_transfn = numArgs + 1;
    fnArgs[0] = aggTransType;
    memcpy(fnArgs + 1, aggArgTypes, numArgs * sizeof(Oid));
  }
  transfn = lookup_agg_function(aggtransfnName, nargs_transfn, fnArgs, variadicArgType, &rettype);

     
                                                          
                                                                            
                                       
     
                                                                        
                                                                     
                                                                   
                                                  
     
  if (rettype != aggTransType)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("return type of transition function %s is not %s", NameListToString(aggtransfnName), format_type_be(aggTransType))));
  }

  tup = SearchSysCache1(PROCOID, ObjectIdGetDatum(transfn));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for function %u", transfn);
  }
  proc = (Form_pg_proc)GETSTRUCT(tup);

     
                                                                             
                                                                         
                                                                          
     
  if (proc->proisstrict && agginitval == NULL)
  {
    if (numArgs < 1 || !IsBinaryCoercible(aggArgTypes[0], aggTransType))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("must not omit initial value when transition function is strict and transition type is not compatible with input type")));
    }
  }

  ReleaseSysCache(tup);

                                                    
  if (aggmtransfnName)
  {
       
                                                                          
                                                                          
                                                 
       
    Assert(OidIsValid(aggmTransType));
    fnArgs[0] = aggmTransType;

    mtransfn = lookup_agg_function(aggmtransfnName, nargs_transfn, fnArgs, variadicArgType, &rettype);

                                                                       
    if (rettype != aggmTransType)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("return type of transition function %s is not %s", NameListToString(aggmtransfnName), format_type_be(aggmTransType))));
    }

    tup = SearchSysCache1(PROCOID, ObjectIdGetDatum(mtransfn));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for function %u", mtransfn);
    }
    proc = (Form_pg_proc)GETSTRUCT(tup);

       
                                                                       
                                                        
       
    if (proc->proisstrict && aggminitval == NULL)
    {
      if (numArgs < 1 || !IsBinaryCoercible(aggArgTypes[0], aggmTransType))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("must not omit initial value when transition function is strict and transition type is not compatible with input type")));
      }
    }

                                                                
    mtransIsStrict = proc->proisstrict;

    ReleaseSysCache(tup);
  }

                                       
  if (aggminvtransfnName)
  {
       
                                                                          
                                                                        
       
    Assert(aggmtransfnName);

    minvtransfn = lookup_agg_function(aggminvtransfnName, nargs_transfn, fnArgs, variadicArgType, &rettype);

                                                                       
    if (rettype != aggmTransType)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("return type of inverse transition function %s is not %s", NameListToString(aggminvtransfnName), format_type_be(aggmTransType))));
    }

    tup = SearchSysCache1(PROCOID, ObjectIdGetDatum(minvtransfn));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for function %u", minvtransfn);
    }
    proc = (Form_pg_proc)GETSTRUCT(tup);

       
                                                                     
                                                                   
                                                 
       
    if (proc->proisstrict != mtransIsStrict)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("strictness of aggregate's forward and inverse transition functions must match")));
    }

    ReleaseSysCache(tup);
  }

                                   
  if (aggfinalfnName)
  {
       
                                                                         
                                                                      
                                                                      
                                                                      
                                                                          
       
    Oid ffnVariadicArgType = variadicArgType;

    fnArgs[0] = aggTransType;
    memcpy(fnArgs + 1, aggArgTypes, numArgs * sizeof(Oid));
    if (finalfnExtraArgs)
    {
      nargs_finalfn = numArgs + 1;
    }
    else
    {
      nargs_finalfn = numDirectArgs + 1;
      if (numDirectArgs < numArgs)
      {
                                                      
        ffnVariadicArgType = InvalidOid;
      }
    }

    finalfn = lookup_agg_function(aggfinalfnName, nargs_finalfn, fnArgs, ffnVariadicArgType, &finaltype);

       
                                                                         
                                                                      
                                                                           
                                                                       
       
    if (finalfnExtraArgs && func_strict(finalfn))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("final function with extra arguments must not be declared STRICT")));
    }
  }
  else
  {
       
                                                                       
       
    finaltype = aggTransType;
  }
  Assert(OidIsValid(finaltype));

                                         
  if (aggcombinefnName)
  {
    Oid combineType;

       
                                                                          
                                          
       
    fnArgs[0] = aggTransType;
    fnArgs[1] = aggTransType;

    combinefn = lookup_agg_function(aggcombinefnName, 2, fnArgs, InvalidOid, &combineType);

                                                                   
    if (combineType != aggTransType)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("return type of combine function %s is not %s", NameListToString(aggcombinefnName), format_type_be(aggTransType))));
    }

       
                                                                           
                                                                           
                                                                      
       
    if (aggTransType == INTERNALOID && func_strict(combinefn))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("combine function with transition type %s must not be declared STRICT", format_type_be(aggTransType))));
    }
  }

     
                                                      
     
  if (aggserialfnName)
  {
                                                               
    fnArgs[0] = INTERNALOID;

    serialfn = lookup_agg_function(aggserialfnName, 1, fnArgs, InvalidOid, &rettype);

    if (rettype != BYTEAOID)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("return type of serialization function %s is not %s", NameListToString(aggserialfnName), format_type_be(BYTEAOID))));
    }
  }

     
                                                        
     
  if (aggdeserialfnName)
  {
                                                                           
    fnArgs[0] = BYTEAOID;
    fnArgs[1] = INTERNALOID;                                     

    deserialfn = lookup_agg_function(aggdeserialfnName, 2, fnArgs, InvalidOid, &rettype);

    if (rettype != INTERNALOID)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("return type of deserialization function %s is not %s", NameListToString(aggdeserialfnName), format_type_be(INTERNALOID))));
    }
  }

     
                                                                           
                                                                       
                                                                         
                                                                             
                                                                      
                         
     
  if (IsPolymorphicType(finaltype) && !hasPolyArg)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot determine result data type"),
                       errdetail("An aggregate returning a polymorphic type "
                                 "must have at least one polymorphic argument.")));
  }

     
                                                                         
                                                                             
                                                                          
                                                                 
     
  if (finaltype == INTERNALOID && !hasInternalArg)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("unsafe use of pseudo-type \"internal\""), errdetail("A function returning \"internal\" must have at least one \"internal\" argument.")));
  }

     
                                                                           
                                                                          
                           
     
  if (OidIsValid(aggmTransType))
  {
                                     
    if (aggmfinalfnName)
    {
         
                                                                   
                                                                 
         
      Oid ffnVariadicArgType = variadicArgType;

      fnArgs[0] = aggmTransType;
      memcpy(fnArgs + 1, aggArgTypes, numArgs * sizeof(Oid));
      if (mfinalfnExtraArgs)
      {
        nargs_finalfn = numArgs + 1;
      }
      else
      {
        nargs_finalfn = numDirectArgs + 1;
        if (numDirectArgs < numArgs)
        {
                                                        
          ffnVariadicArgType = InvalidOid;
        }
      }

      mfinalfn = lookup_agg_function(aggmfinalfnName, nargs_finalfn, fnArgs, ffnVariadicArgType, &rettype);

                                                                    
      if (mfinalfnExtraArgs && func_strict(mfinalfn))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("final function with extra arguments must not be declared STRICT")));
      }
    }
    else
    {
         
                                                                         
         
      rettype = aggmTransType;
    }
    Assert(OidIsValid(rettype));
    if (rettype != finaltype)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("moving-aggregate implementation returns type %s, but plain implementation returns type %s", format_type_be(rettype), format_type_be(finaltype))));
    }
  }

                                  
  if (aggsortopName)
  {
    if (numArgs != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("sort operator can only be specified for single-argument aggregates")));
    }
    sortop = LookupOperName(NULL, aggsortopName, aggArgTypes[0], aggArgTypes[0], false, -1);
  }

     
                                     
     
  for (i = 0; i < numArgs; i++)
  {
    aclresult = pg_type_aclcheck(aggArgTypes[i], GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error_type(aclresult, aggArgTypes[i]);
    }
  }

  aclresult = pg_type_aclcheck(aggTransType, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, aggTransType);
  }

  if (OidIsValid(aggmTransType))
  {
    aclresult = pg_type_aclcheck(aggmTransType, GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error_type(aclresult, aggmTransType);
    }
  }

  aclresult = pg_type_aclcheck(finaltype, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, finaltype);
  }

     
                                                                     
                                                                           
     

  myself = ProcedureCreate(aggName, aggNamespace, replace,                        
      false,                                                                         
      finaltype,                                                           
      GetUserId(),                                                       
      INTERNALlanguageId,                                                        
      InvalidOid,                                                            
      "aggregate_dummy",                                                         
      NULL,                                                            
      PROKIND_AGGREGATE, false,                                                               
                                                                                   
      false,                                                                
      false,                                                                                  
      PROVOLATILE_IMMUTABLE,                                                         
                                                                         
      proparallel, parameterTypes,                                         
      allParameterTypes,                                                      
      parameterModes,                                                          
      parameterNames,                                                          
      parameterDefaults,                                                          
      PointerGetDatum(NULL),                                             
      PointerGetDatum(NULL),                                              
      InvalidOid,                                                             
      1,                                                                
      0);                                                               
  procOid = myself.objectId;

     
                                            
     
  aggdesc = table_open(AggregateRelationId, RowExclusiveLock);
  tupDesc = aggdesc->rd_att;

                                   
  for (i = 0; i < Natts_pg_aggregate; i++)
  {
    nulls[i] = false;
    values[i] = (Datum)NULL;
    replaces[i] = true;
  }
  values[Anum_pg_aggregate_aggfnoid - 1] = ObjectIdGetDatum(procOid);
  values[Anum_pg_aggregate_aggkind - 1] = CharGetDatum(aggKind);
  values[Anum_pg_aggregate_aggnumdirectargs - 1] = Int16GetDatum(numDirectArgs);
  values[Anum_pg_aggregate_aggtransfn - 1] = ObjectIdGetDatum(transfn);
  values[Anum_pg_aggregate_aggfinalfn - 1] = ObjectIdGetDatum(finalfn);
  values[Anum_pg_aggregate_aggcombinefn - 1] = ObjectIdGetDatum(combinefn);
  values[Anum_pg_aggregate_aggserialfn - 1] = ObjectIdGetDatum(serialfn);
  values[Anum_pg_aggregate_aggdeserialfn - 1] = ObjectIdGetDatum(deserialfn);
  values[Anum_pg_aggregate_aggmtransfn - 1] = ObjectIdGetDatum(mtransfn);
  values[Anum_pg_aggregate_aggminvtransfn - 1] = ObjectIdGetDatum(minvtransfn);
  values[Anum_pg_aggregate_aggmfinalfn - 1] = ObjectIdGetDatum(mfinalfn);
  values[Anum_pg_aggregate_aggfinalextra - 1] = BoolGetDatum(finalfnExtraArgs);
  values[Anum_pg_aggregate_aggmfinalextra - 1] = BoolGetDatum(mfinalfnExtraArgs);
  values[Anum_pg_aggregate_aggfinalmodify - 1] = CharGetDatum(finalfnModify);
  values[Anum_pg_aggregate_aggmfinalmodify - 1] = CharGetDatum(mfinalfnModify);
  values[Anum_pg_aggregate_aggsortop - 1] = ObjectIdGetDatum(sortop);
  values[Anum_pg_aggregate_aggtranstype - 1] = ObjectIdGetDatum(aggTransType);
  values[Anum_pg_aggregate_aggtransspace - 1] = Int32GetDatum(aggTransSpace);
  values[Anum_pg_aggregate_aggmtranstype - 1] = ObjectIdGetDatum(aggmTransType);
  values[Anum_pg_aggregate_aggmtransspace - 1] = Int32GetDatum(aggmTransSpace);
  if (agginitval)
  {
    values[Anum_pg_aggregate_agginitval - 1] = CStringGetTextDatum(agginitval);
  }
  else
  {
    nulls[Anum_pg_aggregate_agginitval - 1] = true;
  }
  if (aggminitval)
  {
    values[Anum_pg_aggregate_aggminitval - 1] = CStringGetTextDatum(aggminitval);
  }
  else
  {
    nulls[Anum_pg_aggregate_aggminitval - 1] = true;
  }

  if (replace)
  {
    oldtup = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(procOid));
  }
  else
  {
    oldtup = NULL;
  }

  if (HeapTupleIsValid(oldtup))
  {
    Form_pg_aggregate oldagg = (Form_pg_aggregate)GETSTRUCT(oldtup);

       
                                                                      
                                                                          
                                                                           
                                                    
       
    if (aggKind != oldagg->aggkind)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change routine kind"), (oldagg->aggkind == AGGKIND_NORMAL ? errdetail("\"%s\" is an ordinary aggregate function.", aggName) : oldagg->aggkind == AGGKIND_ORDERED_SET ? errdetail("\"%s\" is an ordered-set aggregate.", aggName) : oldagg->aggkind == AGGKIND_HYPOTHETICAL ? errdetail("\"%s\" is a hypothetical-set aggregate.", aggName) : 0)));
    }
    if (numDirectArgs != oldagg->aggnumdirectargs)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot change number of direct arguments of an aggregate function")));
    }

    replaces[Anum_pg_aggregate_aggfnoid - 1] = false;
    replaces[Anum_pg_aggregate_aggkind - 1] = false;
    replaces[Anum_pg_aggregate_aggnumdirectargs - 1] = false;

    tup = heap_modify_tuple(oldtup, tupDesc, values, nulls, replaces);
    CatalogTupleUpdate(aggdesc, &tup->t_self, tup);
    ReleaseSysCache(oldtup);
  }
  else
  {
    tup = heap_form_tuple(tupDesc, values, nulls);
    CatalogTupleInsert(aggdesc, tup);
  }

  table_close(aggdesc, RowExclusiveLock);

     
                                                                           
                                                                           
                                                                       
                                                                    
     
                                                                            
                                                                             
          
     

                                      
  referenced.classId = ProcedureRelationId;
  referenced.objectId = transfn;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                         
  if (OidIsValid(finalfn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = finalfn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                           
  if (OidIsValid(combinefn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = combinefn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                                 
  if (OidIsValid(serialfn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = serialfn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                                   
  if (OidIsValid(deserialfn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = deserialfn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                                      
  if (OidIsValid(mtransfn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = mtransfn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                                      
  if (OidIsValid(minvtransfn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = minvtransfn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                         
  if (OidIsValid(mfinalfn))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = mfinalfn;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                        
  if (OidIsValid(sortop))
  {
    referenced.classId = OperatorRelationId;
    referenced.objectId = sortop;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

  return myself;
}

   
                       
                                                       
   
                                                   
                                                        
                                                                      
   
                                                                     
   
                                                                  
   
static Oid
lookup_agg_function(List *fnName, int nargs, Oid *input_types, Oid variadicArgType, Oid *rettype)
{
  Oid fnOid;
  bool retset;
  int nvargs;
  Oid vatype;
  Oid *true_oid_array;
  FuncDetailCode fdresult;
  AclResult aclresult;
  int i;

     
                                                                 
                                                                        
                                                                    
                                                                          
                   
     
  fdresult = func_get_detail(fnName, NIL, NIL, nargs, input_types, false, false, &fnOid, rettype, &retset, &nvargs, &vatype, &true_oid_array, NULL);

                                                                
  if (fdresult != FUNCDETAIL_NORMAL || !OidIsValid(fnOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(fnName, nargs, NIL, input_types))));
  }
  if (retset)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("function %s returns a set", func_signature_string(fnName, nargs, NIL, input_types))));
  }

     
                                                                           
                                                                         
                                                                           
                                                                          
                                                                        
                                                                         
     
  if (variadicArgType == ANYOID && vatype != ANYOID)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("function %s must accept VARIADIC ANY to be used in this aggregate", func_signature_string(fnName, nargs, NIL, input_types))));
  }

     
                                                                           
                                                                      
                                        
     
  *rettype = enforce_generic_type_consistency(input_types, true_oid_array, nargs, *rettype, true);

     
                                                                          
                                                              
     
  for (i = 0; i < nargs; i++)
  {
    if (!IsBinaryCoercible(input_types[i], true_oid_array[i]))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("function %s requires run-time type coercion", func_signature_string(fnName, nargs, NIL, true_oid_array))));
    }
  }

                                                                   
  aclresult = pg_proc_aclcheck(fnOid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(fnOid));
  }

  return fnOid;
}
