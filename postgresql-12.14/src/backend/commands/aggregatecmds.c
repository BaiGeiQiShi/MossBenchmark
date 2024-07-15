                                                                            
   
                   
   
                                                  
   
                                                                         
                                                                        
   
   
                  
                                          
   
               
                                                                   
                                                             
                                                                 
                                                                        
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static char
extractModify(DefElem *defel);

   
                   
   
                                                                             
                                                                     
                                                                                
                                                                               
                                                                            
                                   
                                                                                
   
ObjectAddress
DefineAggregate(ParseState *pstate, List *name, List *args, bool oldstyle, List *parameters, bool replace)
{
  char *aggName;
  Oid aggNamespace;
  AclResult aclresult;
  char aggKind = AGGKIND_NORMAL;
  List *transfuncName = NIL;
  List *finalfuncName = NIL;
  List *combinefuncName = NIL;
  List *serialfuncName = NIL;
  List *deserialfuncName = NIL;
  List *mtransfuncName = NIL;
  List *minvtransfuncName = NIL;
  List *mfinalfuncName = NIL;
  bool finalfuncExtraArgs = false;
  bool mfinalfuncExtraArgs = false;
  char finalfuncModify = 0;
  char mfinalfuncModify = 0;
  List *sortoperatorName = NIL;
  TypeName *baseType = NULL;
  TypeName *transType = NULL;
  TypeName *mtransType = NULL;
  int32 transSpace = 0;
  int32 mtransSpace = 0;
  char *initval = NULL;
  char *minitval = NULL;
  char *parallel = NULL;
  int numArgs;
  int numDirectArgs = 0;
  oidvector *parameterTypes;
  ArrayType *allParameterTypes;
  ArrayType *parameterModes;
  ArrayType *parameterNames;
  List *parameterDefaults;
  Oid variadicArgType;
  Oid transTypeId;
  Oid mtransTypeId = InvalidOid;
  char transTypeType;
  char mtransTypeType = 0;
  char proparallel = PROPARALLEL_UNSAFE;
  ListCell *pl;

                                                     
  aggNamespace = QualifiedNameGetCreationNamespace(name, &aggName);

                                                         
  aclresult = pg_namespace_aclcheck(aggNamespace, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(aggNamespace));
  }

                                                                  
  if (!oldstyle)
  {
    Assert(list_length(args) == 2);
    numDirectArgs = intVal(lsecond(args));
    if (numDirectArgs >= 0)
    {
      aggKind = AGGKIND_ORDERED_SET;
    }
    else
    {
      numDirectArgs = 0;
    }
    args = linitial_node(List, args);
  }

                                              
  foreach (pl, parameters)
  {
    DefElem *defel = lfirst_node(DefElem, pl);

       
                                                                        
                                   
       
    if (strcmp(defel->defname, "sfunc") == 0)
    {
      transfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "sfunc1") == 0)
    {
      transfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "finalfunc") == 0)
    {
      finalfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "combinefunc") == 0)
    {
      combinefuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "serialfunc") == 0)
    {
      serialfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "deserialfunc") == 0)
    {
      deserialfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "msfunc") == 0)
    {
      mtransfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "minvfunc") == 0)
    {
      minvtransfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "mfinalfunc") == 0)
    {
      mfinalfuncName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "finalfunc_extra") == 0)
    {
      finalfuncExtraArgs = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "mfinalfunc_extra") == 0)
    {
      mfinalfuncExtraArgs = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "finalfunc_modify") == 0)
    {
      finalfuncModify = extractModify(defel);
    }
    else if (strcmp(defel->defname, "mfinalfunc_modify") == 0)
    {
      mfinalfuncModify = extractModify(defel);
    }
    else if (strcmp(defel->defname, "sortop") == 0)
    {
      sortoperatorName = defGetQualifiedName(defel);
    }
    else if (strcmp(defel->defname, "basetype") == 0)
    {
      baseType = defGetTypeName(defel);
    }
    else if (strcmp(defel->defname, "hypothetical") == 0)
    {
      if (defGetBoolean(defel))
      {
        if (aggKind == AGGKIND_NORMAL)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("only ordered-set aggregates can be hypothetical")));
        }
        aggKind = AGGKIND_HYPOTHETICAL;
      }
    }
    else if (strcmp(defel->defname, "stype") == 0)
    {
      transType = defGetTypeName(defel);
    }
    else if (strcmp(defel->defname, "stype1") == 0)
    {
      transType = defGetTypeName(defel);
    }
    else if (strcmp(defel->defname, "sspace") == 0)
    {
      transSpace = defGetInt32(defel);
    }
    else if (strcmp(defel->defname, "mstype") == 0)
    {
      mtransType = defGetTypeName(defel);
    }
    else if (strcmp(defel->defname, "msspace") == 0)
    {
      mtransSpace = defGetInt32(defel);
    }
    else if (strcmp(defel->defname, "initcond") == 0)
    {
      initval = defGetString(defel);
    }
    else if (strcmp(defel->defname, "initcond1") == 0)
    {
      initval = defGetString(defel);
    }
    else if (strcmp(defel->defname, "minitcond") == 0)
    {
      minitval = defGetString(defel);
    }
    else if (strcmp(defel->defname, "parallel") == 0)
    {
      parallel = defGetString(defel);
    }
    else
    {
      ereport(WARNING, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("aggregate attribute \"%s\" not recognized", defel->defname)));
    }
  }

     
                                                
     
  if (transType == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate stype must be specified")));
  }
  if (transfuncName == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate sfunc must be specified")));
  }

     
                                                                             
                                                                         
                 
     
  if (mtransType != NULL)
  {
    if (mtransfuncName == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate msfunc must be specified when mstype is specified")));
    }
    if (minvtransfuncName == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate minvfunc must be specified when mstype is specified")));
    }
  }
  else
  {
    if (mtransfuncName != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate msfunc must not be specified without mstype")));
    }
    if (minvtransfuncName != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate minvfunc must not be specified without mstype")));
    }
    if (mfinalfuncName != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate mfinalfunc must not be specified without mstype")));
    }
    if (mtransSpace != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate msspace must not be specified without mstype")));
    }
    if (minitval != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate minitcond must not be specified without mstype")));
    }
  }

     
                                                                             
              
     
  if (finalfuncModify == 0)
  {
    finalfuncModify = (aggKind == AGGKIND_NORMAL) ? AGGMODIFY_READ_ONLY : AGGMODIFY_READ_WRITE;
  }
  if (mfinalfuncModify == 0)
  {
    mfinalfuncModify = (aggKind == AGGKIND_NORMAL) ? AGGMODIFY_READ_ONLY : AGGMODIFY_READ_WRITE;
  }

     
                                                
     
  if (oldstyle)
  {
       
                                                                       
                                                                   
       
                                                                         
                                                                          
       
    Oid aggArgTypes[1];

    if (baseType == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate input type must be specified")));
    }

    if (pg_strcasecmp(TypeNameToString(baseType), "ANY") == 0)
    {
      numArgs = 0;
      aggArgTypes[0] = InvalidOid;
    }
    else
    {
      numArgs = 1;
      aggArgTypes[0] = typenameTypeId(NULL, baseType);
    }
    parameterTypes = buildoidvector(aggArgTypes, numArgs);
    allParameterTypes = NULL;
    parameterModes = NULL;
    parameterNames = NULL;
    parameterDefaults = NIL;
    variadicArgType = InvalidOid;
  }
  else
  {
       
                                                                         
                                                                  
       
    Oid requiredResultType;

    if (baseType != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("basetype is redundant with aggregate input type specification")));
    }

    numArgs = list_length(args);
    interpret_function_parameter_list(pstate, args, InvalidOid, OBJECT_AGGREGATE, &parameterTypes, &allParameterTypes, &parameterModes, &parameterNames, &parameterDefaults, &variadicArgType, &requiredResultType);
                                                                     
    Assert(parameterDefaults == NIL);
                                                              
    Assert(requiredResultType == InvalidOid);
  }

     
                                        
     
                                                                         
                                                                           
                                                                            
                                                                          
                                                                             
                                                                         
                
     
  transTypeId = typenameTypeId(NULL, transType);
  transTypeType = get_typtype(transTypeId);
  if (transTypeType == TYPTYPE_PSEUDO && !IsPolymorphicType(transTypeId))
  {
    if (transTypeId == INTERNALOID && superuser())
                ;
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate transition data type cannot be %s", format_type_be(transTypeId))));
    }
  }

  if (serialfuncName && deserialfuncName)
  {
       
                                                                    
       
    if (transTypeId != INTERNALOID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("serialization functions may be specified only when the aggregate transition data type is %s", format_type_be(INTERNALOID))));
    }
  }
  else if (serialfuncName || deserialfuncName)
  {
       
                                                      
       
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("must specify both or neither of serialization and deserialization functions")));
  }

     
                                                                       
                                    
     
  if (mtransType)
  {
    mtransTypeId = typenameTypeId(NULL, mtransType);
    mtransTypeType = get_typtype(mtransTypeId);
    if (mtransTypeType == TYPTYPE_PSEUDO && !IsPolymorphicType(mtransTypeId))
    {
      if (mtransTypeId == INTERNALOID && superuser())
                  ;
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate transition data type cannot be %s", format_type_be(mtransTypeId))));
      }
    }
  }

     
                                                                          
                                                                      
                                                                     
                                                                          
                                                                            
                                                                    
                                                         
     
  if (initval && transTypeType != TYPTYPE_PSEUDO)
  {
    Oid typinput, typioparam;

    getTypeInputInfo(transTypeId, &typinput, &typioparam);
    (void)OidInputFunctionCall(typinput, initval, typioparam, -1);
  }

     
                                            
     
  if (minitval && mtransTypeType != TYPTYPE_PSEUDO)
  {
    Oid typinput, typioparam;

    getTypeInputInfo(mtransTypeId, &typinput, &typioparam);
    (void)OidInputFunctionCall(typinput, minitval, typioparam, -1);
  }

  if (parallel)
  {
    if (strcmp(parallel, "safe") == 0)
    {
      proparallel = PROPARALLEL_SAFE;
    }
    else if (strcmp(parallel, "restricted") == 0)
    {
      proparallel = PROPARALLEL_RESTRICTED;
    }
    else if (strcmp(parallel, "unsafe") == 0)
    {
      proparallel = PROPARALLEL_UNSAFE;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("parameter \"parallel\" must be SAFE, RESTRICTED, or UNSAFE")));
    }
  }

     
                                                                     
     
  return AggregateCreate(aggName,                                                                                                                                                                                                            
      aggNamespace,                                                                                                                                                                                                                     
      replace, aggKind, numArgs, numDirectArgs, parameterTypes, PointerGetDatum(allParameterTypes), PointerGetDatum(parameterModes), PointerGetDatum(parameterNames), parameterDefaults, variadicArgType, transfuncName,                         
      finalfuncName,                                                                                                                                                                                                                              
      combinefuncName,                                                                                                                                                                                                                              
      serialfuncName,                                                                                                                                                                                                                              
      deserialfuncName,                                                                                                                                                                                                                              
      mtransfuncName,                                                                                                                                                                                                                                 
      minvtransfuncName,                                                                                                                                                                                                                              
      mfinalfuncName,                                                                                                                                                                                                                             
      finalfuncExtraArgs, mfinalfuncExtraArgs, finalfuncModify, mfinalfuncModify, sortoperatorName,                                                                                                                                              
      transTypeId,                                                                                                                                                                                                                                 
      transSpace,                                                                                                                                                                                                                              
      mtransTypeId,                                                                                                                                                                                                                                
      mtransSpace,                                                                                                                                                                                                                             
      initval,                                                                                                                                                                                                                                  
      minitval,                                                                                                                                                                                                                                 
      proparallel);                                                                                                                                                                                                                          
}

   
                                                                                
   
static char
extractModify(DefElem *defel)
{
  char *val = defGetString(defel);

  if (strcmp(val, "read_only") == 0)
  {
    return AGGMODIFY_READ_ONLY;
  }
  if (strcmp(val, "shareable") == 0)
  {
    return AGGMODIFY_SHAREABLE;
  }
  if (strcmp(val, "read_write") == 0)
  {
    return AGGMODIFY_READ_WRITE;
  }
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("parameter \"%s\" must be READ_ONLY, SHAREABLE, or READ_WRITE", defel->defname)));
  return 0;                          
}
