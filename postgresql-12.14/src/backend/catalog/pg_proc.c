                                                                            
   
             
                                                              
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "executor/functions.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_type.h"
#include "tcop/pquery.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/regproc.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  char *proname;
  char *prosrc;
} parse_error_callback_arg;

static void
sql_function_parse_error_callback(void *arg);
static int
match_prosrc_to_query(const char *prosrc, const char *queryText, int cursorpos);
static bool
match_prosrc_to_literal(const char *prosrc, const char *literal, int cursorpos, int *newcursorpos);

                                                                    
                    
   
                                                                                    
                                                                          
                                                                 
                                                                    
   
ObjectAddress
ProcedureCreate(const char *procedureName, Oid procNamespace, bool replace, bool returnsSet, Oid returnType, Oid proowner, Oid languageObjectId, Oid languageValidator, const char *prosrc, const char *probin, char prokind, bool security_definer, bool isLeakProof, bool isStrict, char volatility, char parallel, oidvector *parameterTypes, Datum allParameterTypes, Datum parameterModes, Datum parameterNames, List *parameterDefaults, Datum trftypes, Datum proconfig, Oid prosupport, float4 procost, float4 prorows)
{
  Oid retval;
  int parameterCount;
  int allParamCount;
  Oid *allParams;
  char *paramModes = NULL;
  bool genericInParam = false;
  bool genericOutParam = false;
  bool anyrangeInParam = false;
  bool anyrangeOutParam = false;
  bool internalInParam = false;
  bool internalOutParam = false;
  Oid variadicType = InvalidOid;
  Acl *proacl = NULL;
  Relation rel;
  HeapTuple tup;
  HeapTuple oldtup;
  bool nulls[Natts_pg_proc];
  Datum values[Natts_pg_proc];
  bool replaces[Natts_pg_proc];
  NameData procname;
  TupleDesc tupDesc;
  bool is_update;
  ObjectAddress myself, referenced;
  int i;
  Oid trfid;

     
                   
     
  Assert(PointerIsValid(prosrc));

  parameterCount = parameterTypes->dim1;
  if (parameterCount < 0 || parameterCount > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("functions cannot have more than %d argument", "functions cannot have more than %d arguments", FUNC_MAX_ARGS, FUNC_MAX_ARGS)));
  }
                                                                    

                                
  if (allParameterTypes != PointerGetDatum(NULL))
  {
       
                                                                        
                                                                          
                                             
       
    ArrayType *allParamArray = (ArrayType *)DatumGetPointer(allParameterTypes);

    allParamCount = ARR_DIMS(allParamArray)[0];
    if (ARR_NDIM(allParamArray) != 1 || allParamCount <= 0 || ARR_HASNULL(allParamArray) || ARR_ELEMTYPE(allParamArray) != OIDOID)
    {
      elog(ERROR, "allParameterTypes is not a 1-D Oid array");
    }
    allParams = (Oid *)ARR_DATA_PTR(allParamArray);
    Assert(allParamCount >= parameterCount);
                                                 
  }
  else
  {
    allParamCount = parameterCount;
    allParams = parameterTypes->values;
  }

  if (parameterModes != PointerGetDatum(NULL))
  {
       
                                                                         
                                                                          
                                              
       
    ArrayType *modesArray = (ArrayType *)DatumGetPointer(parameterModes);

    if (ARR_NDIM(modesArray) != 1 || ARR_DIMS(modesArray)[0] != allParamCount || ARR_HASNULL(modesArray) || ARR_ELEMTYPE(modesArray) != CHAROID)
    {
      elog(ERROR, "parameterModes is not a 1-D char array");
    }
    paramModes = (char *)ARR_DATA_PTR(modesArray);
  }

     
                                                                          
                                                               
     
  for (i = 0; i < parameterCount; i++)
  {
    switch (parameterTypes->values[i])
    {
    case ANYARRAYOID:
    case ANYELEMENTOID:
    case ANYNONARRAYOID:
    case ANYENUMOID:
      genericInParam = true;
      break;
    case ANYRANGEOID:
      genericInParam = true;
      anyrangeInParam = true;
      break;
    case INTERNALOID:
      internalInParam = true;
      break;
    }
  }

  if (allParameterTypes != PointerGetDatum(NULL))
  {
    for (i = 0; i < allParamCount; i++)
    {
      if (paramModes == NULL || paramModes[i] == PROARGMODE_IN || paramModes[i] == PROARGMODE_VARIADIC)
      {
        continue;                               
      }

      switch (allParams[i])
      {
      case ANYARRAYOID:
      case ANYELEMENTOID:
      case ANYNONARRAYOID:
      case ANYENUMOID:
        genericOutParam = true;
        break;
      case ANYRANGEOID:
        genericOutParam = true;
        anyrangeOutParam = true;
        break;
      case INTERNALOID:
        internalOutParam = true;
        break;
      }
    }
  }

     
                                                                             
                                                                          
                                                                        
                                                                           
                                     
     
  if ((IsPolymorphicType(returnType) || genericOutParam) && !genericInParam)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot determine result data type"), errdetail("A function returning a polymorphic type must have at least one polymorphic argument.")));
  }

  if ((returnType == ANYRANGEOID || anyrangeOutParam) && !anyrangeInParam)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot determine result data type"), errdetail("A function returning \"anyrange\" must have at least one \"anyrange\" argument.")));
  }

  if ((returnType == INTERNALOID || internalOutParam) && !internalInParam)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("unsafe use of pseudo-type \"internal\""), errdetail("A function returning \"internal\" must have at least one \"internal\" argument.")));
  }

  if (paramModes != NULL)
  {
       
                                                                         
                                                                         
                             
       
    for (i = 0; i < allParamCount; i++)
    {
      switch (paramModes[i])
      {
      case PROARGMODE_IN:
      case PROARGMODE_INOUT:
        if (OidIsValid(variadicType))
        {
          elog(ERROR, "variadic parameter must be last");
        }
        break;
      case PROARGMODE_OUT:
      case PROARGMODE_TABLE:
                  
        break;
      case PROARGMODE_VARIADIC:
        if (OidIsValid(variadicType))
        {
          elog(ERROR, "variadic parameter must be last");
        }
        switch (allParams[i])
        {
        case ANYOID:
          variadicType = ANYOID;
          break;
        case ANYARRAYOID:
          variadicType = ANYELEMENTOID;
          break;
        default:
          variadicType = get_element_type(allParams[i]);
          if (!OidIsValid(variadicType))
          {
            elog(ERROR, "variadic parameter is not an array");
          }
          break;
        }
        break;
      default:
        elog(ERROR, "invalid parameter mode '%c'", paramModes[i]);
        break;
      }
    }
  }

     
                                                                 
     

  for (i = 0; i < Natts_pg_proc; ++i)
  {
    nulls[i] = false;
    values[i] = (Datum)0;
    replaces[i] = true;
  }

  namestrcpy(&procname, procedureName);
  values[Anum_pg_proc_proname - 1] = NameGetDatum(&procname);
  values[Anum_pg_proc_pronamespace - 1] = ObjectIdGetDatum(procNamespace);
  values[Anum_pg_proc_proowner - 1] = ObjectIdGetDatum(proowner);
  values[Anum_pg_proc_prolang - 1] = ObjectIdGetDatum(languageObjectId);
  values[Anum_pg_proc_procost - 1] = Float4GetDatum(procost);
  values[Anum_pg_proc_prorows - 1] = Float4GetDatum(prorows);
  values[Anum_pg_proc_provariadic - 1] = ObjectIdGetDatum(variadicType);
  values[Anum_pg_proc_prosupport - 1] = ObjectIdGetDatum(prosupport);
  values[Anum_pg_proc_prokind - 1] = CharGetDatum(prokind);
  values[Anum_pg_proc_prosecdef - 1] = BoolGetDatum(security_definer);
  values[Anum_pg_proc_proleakproof - 1] = BoolGetDatum(isLeakProof);
  values[Anum_pg_proc_proisstrict - 1] = BoolGetDatum(isStrict);
  values[Anum_pg_proc_proretset - 1] = BoolGetDatum(returnsSet);
  values[Anum_pg_proc_provolatile - 1] = CharGetDatum(volatility);
  values[Anum_pg_proc_proparallel - 1] = CharGetDatum(parallel);
  values[Anum_pg_proc_pronargs - 1] = UInt16GetDatum(parameterCount);
  values[Anum_pg_proc_pronargdefaults - 1] = UInt16GetDatum(list_length(parameterDefaults));
  values[Anum_pg_proc_prorettype - 1] = ObjectIdGetDatum(returnType);
  values[Anum_pg_proc_proargtypes - 1] = PointerGetDatum(parameterTypes);
  if (allParameterTypes != PointerGetDatum(NULL))
  {
    values[Anum_pg_proc_proallargtypes - 1] = allParameterTypes;
  }
  else
  {
    nulls[Anum_pg_proc_proallargtypes - 1] = true;
  }
  if (parameterModes != PointerGetDatum(NULL))
  {
    values[Anum_pg_proc_proargmodes - 1] = parameterModes;
  }
  else
  {
    nulls[Anum_pg_proc_proargmodes - 1] = true;
  }
  if (parameterNames != PointerGetDatum(NULL))
  {
    values[Anum_pg_proc_proargnames - 1] = parameterNames;
  }
  else
  {
    nulls[Anum_pg_proc_proargnames - 1] = true;
  }
  if (parameterDefaults != NIL)
  {
    values[Anum_pg_proc_proargdefaults - 1] = CStringGetTextDatum(nodeToString(parameterDefaults));
  }
  else
  {
    nulls[Anum_pg_proc_proargdefaults - 1] = true;
  }
  if (trftypes != PointerGetDatum(NULL))
  {
    values[Anum_pg_proc_protrftypes - 1] = trftypes;
  }
  else
  {
    nulls[Anum_pg_proc_protrftypes - 1] = true;
  }
  values[Anum_pg_proc_prosrc - 1] = CStringGetTextDatum(prosrc);
  if (probin)
  {
    values[Anum_pg_proc_probin - 1] = CStringGetTextDatum(probin);
  }
  else
  {
    nulls[Anum_pg_proc_probin - 1] = true;
  }
  if (proconfig != PointerGetDatum(NULL))
  {
    values[Anum_pg_proc_proconfig - 1] = proconfig;
  }
  else
  {
    nulls[Anum_pg_proc_proconfig - 1] = true;
  }
                                       

  rel = table_open(ProcedureRelationId, RowExclusiveLock);
  tupDesc = RelationGetDescr(rel);

                                         
  oldtup = SearchSysCache3(PROCNAMEARGSNSP, PointerGetDatum(procedureName), PointerGetDatum(parameterTypes), ObjectIdGetDatum(procNamespace));

  if (HeapTupleIsValid(oldtup))
  {
                                           
    Form_pg_proc oldproc = (Form_pg_proc)GETSTRUCT(oldtup);
    Datum proargnames;
    bool isnull;
    const char *dropcmd;

    if (!replace)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_FUNCTION), errmsg("function \"%s\" already exists with same argument types", procedureName)));
    }
    if (!pg_proc_ownercheck(oldproc->oid, proowner))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, procedureName);
    }

                                         
    if (oldproc->prokind != prokind)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change routine kind"), (oldproc->prokind == PROKIND_AGGREGATE ? errdetail("\"%s\" is an aggregate function.", procedureName) : oldproc->prokind == PROKIND_FUNCTION ? errdetail("\"%s\" is a function.", procedureName) : oldproc->prokind == PROKIND_PROCEDURE ? errdetail("\"%s\" is a procedure.", procedureName) : oldproc->prokind == PROKIND_WINDOW ? errdetail("\"%s\" is a window function.", procedureName) : 0)));
    }

    dropcmd = (prokind == PROKIND_PROCEDURE ? "DROP PROCEDURE" : prokind == PROKIND_AGGREGATE ? "DROP AGGREGATE" : "DROP FUNCTION");

       
                                                                      
                                                                 
       
                                                                         
                                                                           
                                                                           
       
    if (returnType != oldproc->prorettype || returnsSet != oldproc->proretset)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), prokind == PROKIND_PROCEDURE ? errmsg("cannot change whether a procedure has output parameters") : errmsg("cannot change return type of existing function"),

                            
                                                                                           
                                      
                            
                         errhint("Use %s %s first.", dropcmd, format_procedure(oldproc->oid))));
    }

       
                                                                      
                                 
       
    if (returnType == RECORDOID)
    {
      TupleDesc olddesc;
      TupleDesc newdesc;

      olddesc = build_function_result_tupdesc_t(oldtup);
      newdesc = build_function_result_tupdesc_d(prokind, allParameterTypes, parameterModes, parameterNames);
      if (olddesc == NULL && newdesc == NULL)
                                                  ;
      else if (olddesc == NULL || newdesc == NULL || !equalTupleDescs(olddesc, newdesc))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot change return type of existing function"), errdetail("Row type defined by OUT parameters is different."),
                                                                                        
                           errhint("Use %s %s first.", dropcmd, format_procedure(oldproc->oid))));
      }
    }

       
                                                                        
                                                                           
                                                                  
       
    proargnames = SysCacheGetAttr(PROCNAMEARGSNSP, oldtup, Anum_pg_proc_proargnames, &isnull);
    if (!isnull)
    {
      Datum proargmodes;
      char **old_arg_names;
      char **new_arg_names;
      int n_old_arg_names;
      int n_new_arg_names;
      int j;

      proargmodes = SysCacheGetAttr(PROCNAMEARGSNSP, oldtup, Anum_pg_proc_proargmodes, &isnull);
      if (isnull)
      {
        proargmodes = PointerGetDatum(NULL);                      
      }

      n_old_arg_names = get_func_input_arg_names(proargnames, proargmodes, &old_arg_names);
      n_new_arg_names = get_func_input_arg_names(parameterNames, parameterModes, &new_arg_names);
      for (j = 0; j < n_old_arg_names; j++)
      {
        if (old_arg_names[j] == NULL)
        {
          continue;
        }
        if (j >= n_new_arg_names || new_arg_names[j] == NULL || strcmp(old_arg_names[j], new_arg_names[j]) != 0)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot change name of input parameter \"%s\"", old_arg_names[j]),
                                                                                          
                             errhint("Use %s %s first.", dropcmd, format_procedure(oldproc->oid))));
        }
      }
    }

       
                                                                         
                                                                         
                                                                           
                                                                           
                                                                           
                                                    
       
    if (oldproc->pronargdefaults != 0)
    {
      Datum proargdefaults;
      List *oldDefaults;
      ListCell *oldlc;
      ListCell *newlc;

      if (list_length(parameterDefaults) < oldproc->pronargdefaults)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot remove parameter defaults from existing function"),
                                                                                        
                           errhint("Use %s %s first.", dropcmd, format_procedure(oldproc->oid))));
      }

      proargdefaults = SysCacheGetAttr(PROCNAMEARGSNSP, oldtup, Anum_pg_proc_proargdefaults, &isnull);
      Assert(!isnull);
      oldDefaults = castNode(List, stringToNode(TextDatumGetCString(proargdefaults)));
      Assert(list_length(oldDefaults) == oldproc->pronargdefaults);

                                                                      
      newlc = list_head(parameterDefaults);
      for (i = list_length(parameterDefaults) - oldproc->pronargdefaults; i > 0; i--)
      {
        newlc = lnext(newlc);
      }

      foreach (oldlc, oldDefaults)
      {
        Node *oldDef = (Node *)lfirst(oldlc);
        Node *newDef = (Node *)lfirst(newlc);

        if (exprType(oldDef) != exprType(newDef))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("cannot change data type of existing parameter default value"),
                                                                                          
                             errhint("Use %s %s first.", dropcmd, format_procedure(oldproc->oid))));
        }
        newlc = lnext(newlc);
      }
    }

       
                                                                           
                                                                     
       
    replaces[Anum_pg_proc_oid - 1] = false;
    replaces[Anum_pg_proc_proowner - 1] = false;
    replaces[Anum_pg_proc_proacl - 1] = false;

                        
    tup = heap_modify_tuple(oldtup, tupDesc, values, nulls, replaces);
    CatalogTupleUpdate(rel, &tup->t_self, tup);

    ReleaseSysCache(oldtup);
    is_update = true;
  }
  else
  {
                                  
    Oid newOid;

                                                          
    proacl = get_user_default_acl(OBJECT_FUNCTION, proowner, procNamespace);
    if (proacl != NULL)
    {
      values[Anum_pg_proc_proacl - 1] = PointerGetDatum(proacl);
    }
    else
    {
      nulls[Anum_pg_proc_proacl - 1] = true;
    }

    newOid = GetNewOidWithIndex(rel, ProcedureOidIndexId, Anum_pg_proc_oid);
    values[Anum_pg_proc_oid - 1] = ObjectIdGetDatum(newOid);
    tup = heap_form_tuple(tupDesc, values, nulls);
    CatalogTupleInsert(rel, tup);
    is_update = false;
  }

  retval = ((Form_pg_proc)GETSTRUCT(tup))->oid;

     
                                                                      
                                                                     
                                                                       
                                                                            
     
  if (is_update)
  {
    deleteDependencyRecordsFor(ProcedureRelationId, retval, true);
  }

  myself.classId = ProcedureRelationId;
  myself.objectId = retval;
  myself.objectSubId = 0;

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = procNamespace;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                             
  referenced.classId = LanguageRelationId;
  referenced.objectId = languageObjectId;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                 
  referenced.classId = TypeRelationId;
  referenced.objectId = returnType;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                           
  if ((trfid = get_transform_oid(returnType, languageObjectId, true)))
  {
    referenced.classId = TransformRelationId;
    referenced.objectId = trfid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                     
  for (i = 0; i < allParamCount; i++)
  {
    referenced.classId = TypeRelationId;
    referenced.objectId = allParams[i];
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                                
    if ((trfid = get_transform_oid(allParams[i], languageObjectId, true)))
    {
      referenced.classId = TransformRelationId;
      referenced.objectId = trfid;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }
  }

                                                   
  if (parameterDefaults)
  {
    recordDependencyOnExpr(&myself, (Node *)parameterDefaults, NIL, DEPENDENCY_NORMAL);
  }

                                              
  if (OidIsValid(prosupport))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = prosupport;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                           
  if (!is_update)
  {
    recordDependencyOnOwner(ProcedureRelationId, retval, proowner);
  }

                                                
  if (!is_update)
  {
    recordDependencyOnNewAcl(ProcedureRelationId, retval, 0, proowner, proacl);
  }

                               
  recordDependencyOnCurrentExtension(&myself, is_update);

  heap_freetuple(tup);

                                           
  InvokeObjectPostCreateHook(ProcedureRelationId, retval, 0);

  table_close(rel, RowExclusiveLock);

                            
  if (OidIsValid(languageValidator))
  {
    ArrayType *set_items = NULL;
    int save_nestlevel = 0;

                                                                       
    CommandCounterIncrement();

       
                                                                           
                                                                    
                                                                          
                                                                          
                                                                          
                                                                 
                                                                    
                                                    
       
    if (check_function_bodies)
    {
      set_items = (ArrayType *)DatumGetPointer(proconfig);
      if (set_items)                                   
      {
        save_nestlevel = NewGUCNestLevel();
        ProcessGUCArray(set_items, (superuser() ? PGC_SUSET : PGC_USERSET), PGC_S_SESSION, GUC_ACTION_SAVE);
      }
    }

    OidFunctionCall1(languageValidator, ObjectIdGetDatum(retval));

    if (set_items)
    {
      AtEOXact_GUC(true, save_nestlevel);
    }
  }

  return myself;
}

   
                                    
   
                                                                       
                             
   
Datum
fmgr_internal_validator(PG_FUNCTION_ARGS)
{
  Oid funcoid = PG_GETARG_OID(0);
  HeapTuple tuple;
  bool isnull;
  Datum tmp;
  char *prosrc;

  if (!CheckFunctionValidatorAccess(fcinfo->flinfo->fn_oid, funcoid))
  {
    PG_RETURN_VOID();
  }

     
                                                                            
                                                     
     

  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcoid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for function %u", funcoid);
  }

  tmp = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_prosrc, &isnull);
  if (isnull)
  {
    elog(ERROR, "null prosrc");
  }
  prosrc = TextDatumGetCString(tmp);

  if (fmgr_internal_function(prosrc) == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("there is no built-in function named \"%s\"", prosrc)));
  }

  ReleaseSysCache(tuple);

  PG_RETURN_VOID();
}

   
                                      
   
                                                                     
                                                              
                       
   
Datum
fmgr_c_validator(PG_FUNCTION_ARGS)
{
  Oid funcoid = PG_GETARG_OID(0);
  void *libraryhandle;
  HeapTuple tuple;
  bool isnull;
  Datum tmp;
  char *prosrc;
  char *probin;

  if (!CheckFunctionValidatorAccess(fcinfo->flinfo->fn_oid, funcoid))
  {
    PG_RETURN_VOID();
  }

     
                                                                          
                                                                          
                                                                
     

  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcoid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for function %u", funcoid);
  }

  tmp = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_prosrc, &isnull);
  if (isnull)
  {
    elog(ERROR, "null prosrc for C function %u", funcoid);
  }
  prosrc = TextDatumGetCString(tmp);

  tmp = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_probin, &isnull);
  if (isnull)
  {
    elog(ERROR, "null probin for C function %u", funcoid);
  }
  probin = TextDatumGetCString(tmp);

  (void)load_external_function(probin, prosrc, true, &libraryhandle);
  (void)fetch_finfo_record(libraryhandle, prosrc);

  ReleaseSysCache(tuple);

  PG_RETURN_VOID();
}

   
                                        
   
                                                                        
   
Datum
fmgr_sql_validator(PG_FUNCTION_ARGS)
{
  Oid funcoid = PG_GETARG_OID(0);
  HeapTuple tuple;
  Form_pg_proc proc;
  List *raw_parsetree_list;
  List *querytree_list;
  ListCell *lc;
  bool isnull;
  Datum tmp;
  char *prosrc;
  parse_error_callback_arg callback_arg;
  ErrorContextCallback sqlerrcontext;
  bool haspolyarg;
  int i;

  if (!CheckFunctionValidatorAccess(fcinfo->flinfo->fn_oid, funcoid))
  {
    PG_RETURN_VOID();
  }

  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcoid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for function %u", funcoid);
  }
  proc = (Form_pg_proc)GETSTRUCT(tuple);

                                  
                                               
  if (get_typtype(proc->prorettype) == TYPTYPE_PSEUDO && proc->prorettype != RECORDOID && proc->prorettype != VOIDOID && !IsPolymorphicType(proc->prorettype))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("SQL functions cannot return type %s", format_type_be(proc->prorettype))));
  }

                                         
                              
  haspolyarg = false;
  for (i = 0; i < proc->pronargs; i++)
  {
    if (get_typtype(proc->proargtypes.values[i]) == TYPTYPE_PSEUDO)
    {
      if (IsPolymorphicType(proc->proargtypes.values[i]))
      {
        haspolyarg = true;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("SQL functions cannot have arguments of type %s", format_type_be(proc->proargtypes.values[i]))));
      }
    }
  }

                                                      
  if (check_function_bodies)
  {
    tmp = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc");
    }

    prosrc = TextDatumGetCString(tmp);

       
                                                    
       
    callback_arg.proname = NameStr(proc->proname);
    callback_arg.prosrc = prosrc;

    sqlerrcontext.callback = sql_function_parse_error_callback;
    sqlerrcontext.arg = (void *)&callback_arg;
    sqlerrcontext.previous = error_context_stack;
    error_context_stack = &sqlerrcontext;

       
                                                                        
                                                                    
                                                                           
                        
       
                                                                       
                                           
       
    raw_parsetree_list = pg_parse_query(prosrc);

    if (!haspolyarg)
    {
         
                                                                       
                                 
         
      SQLFunctionParseInfoPtr pinfo;

                                                   
      pinfo = prepare_sql_fn_parse_info(tuple, NULL, InvalidOid);

      querytree_list = NIL;
      foreach (lc, raw_parsetree_list)
      {
        RawStmt *parsetree = lfirst_node(RawStmt, lc);
        List *querytree_sublist;

        querytree_sublist = pg_analyze_and_rewrite_params(parsetree, prosrc, (ParserSetupHook)sql_fn_parser_setup, pinfo, NULL);
        querytree_list = list_concat(querytree_list, querytree_sublist);
      }

      check_sql_fn_statements(querytree_list);
      (void)check_sql_fn_retval(funcoid, proc->prorettype, querytree_list, NULL, NULL);
    }

    error_context_stack = sqlerrcontext.previous;
  }

  ReleaseSysCache(tuple);

  PG_RETURN_VOID();
}

   
                                                                          
   
static void
sql_function_parse_error_callback(void *arg)
{
  parse_error_callback_arg *callback_arg = (parse_error_callback_arg *)arg;

                                                                       
  if (!function_parse_error_transpose(callback_arg->prosrc))
  {
                                                                  
    errcontext("SQL function \"%s\"", callback_arg->proname);
  }
}

   
                                                                        
                                                                          
                                                                 
                                                                             
                                                                             
                                                                           
                                                
   
                                                               
   
bool
function_parse_error_transpose(const char *prosrc)
{
  int origerrposition;
  int newerrposition;

     
                                                                        
                      
     
                                                                           
                                       
     
  origerrposition = geterrposition();
  if (origerrposition <= 0)
  {
    origerrposition = getinternalerrposition();
    if (origerrposition <= 0)
    {
      return false;
    }
  }

                                                                           
  if (ActivePortal && ActivePortal->status == PORTAL_ACTIVE)
  {
    const char *queryText = ActivePortal->sourceText;

                                                       
    newerrposition = match_prosrc_to_query(prosrc, queryText, origerrposition);
  }
  else
  {
       
                                                                         
                                                                
       
    newerrposition = -1;
  }

  if (newerrposition > 0)
  {
                                                                       
    errposition(newerrposition);
                                                                   
    internalerrposition(0);
    internalerrquery(NULL);
  }
  else
  {
       
                                                                     
                                                                
       
    errposition(0);
    internalerrposition(origerrposition);
    internalerrquery(prosrc);
  }

  return true;
}

   
                                                                        
                                                                           
                                                                          
                                                                           
   
static int
match_prosrc_to_query(const char *prosrc, const char *queryText, int cursorpos)
{
     
                                                                      
                                                                             
                                                                           
            
     
  int prosrclen = strlen(prosrc);
  int querylen = strlen(queryText);
  int matchpos = 0;
  int curpos;
  int newcursorpos;

  for (curpos = 0; curpos < querylen - prosrclen; curpos++)
  {
    if (queryText[curpos] == '$' && strncmp(prosrc, &queryText[curpos + 1], prosrclen) == 0 && queryText[curpos + 1 + prosrclen] == '$')
    {
         
                                                                   
                                                                        
                                                                 
         
      if (matchpos)
      {
        return 0;                             
      }
      matchpos = pg_mbstrlen_with_len(queryText, curpos + 1) + cursorpos;
    }
    else if (queryText[curpos] == '\'' && match_prosrc_to_literal(prosrc, &queryText[curpos + 1], cursorpos, &newcursorpos))
    {
         
                                                                      
                                                                
         
      if (matchpos)
      {
        return 0;                             
      }
      matchpos = pg_mbstrlen_with_len(queryText, curpos + 1) + newcursorpos;
    }
  }

  return matchpos;
}

   
                                                                  
                                                                     
                                                                   
   
                                                                            
                   
   
static bool
match_prosrc_to_literal(const char *prosrc, const char *literal, int cursorpos, int *newcursorpos)
{
  int newcp = cursorpos;
  int chlen;

     
                                                                       
                                                                     
                                       
     
                                                                          
                                                
     
  while (*prosrc)
  {
    cursorpos--;                                    

       
                                                                       
                                                  
       
    if (*literal == '\\')
    {
      literal++;
      if (cursorpos > 0)
      {
        newcp++;
      }
    }
    else if (*literal == '\'')
    {
      if (literal[1] != '\'')
      {
        goto fail;
      }
      literal++;
      if (cursorpos > 0)
      {
        newcp++;
      }
    }
    chlen = pg_mblen(prosrc);
    if (strncmp(prosrc, literal, chlen) != 0)
    {
      goto fail;
    }
    prosrc += chlen;
    literal += chlen;
  }

  if (*literal == '\'' && literal[1] != '\'')
  {
                 
    *newcursorpos = newcp;
    return true;
  }

fail:
                                                           
  *newcursorpos = newcp;
  return false;
}

List *
oid_array_to_list(Datum datum)
{
  ArrayType *array = DatumGetArrayTypeP(datum);
  Datum *values;
  int nelems;
  int i;
  List *result = NIL;

  deconstruct_array(array, OIDOID, sizeof(Oid), true, 'i', &values, NULL, &nelems);
  for (i = 0; i < nelems; i++)
  {
    result = lappend_oid(result, values[i]);
  }
  return result;
}
