                                                                            
   
                  
   
                                                                        
                    
   
                                                                         
                                                                        
   
   
                  
                                         
   
               
                                                         
                                                              
                                                                 
                                                                        
                                         
   
         
                                                                        
                       
                                         
                   
           
                       
                
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/sysattr.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "commands/proclang.h"
#include "executor/execdesc.h"
#include "executor/executor.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "pgstat.h"
#include "tcop/pquery.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

   
                                                                
                                                                      
   
                                                                            
                                                                               
                                                                             
                                                                           
                                                                          
                                                                           
               
   
static void
compute_return_type(TypeName *returnType, Oid languageOid, Oid *prorettype_p, bool *returnsSet_p)
{
  Oid rettype;
  Type typtup;
  AclResult aclresult;

  typtup = LookupTypeName(NULL, returnType, NULL, false);

  if (typtup)
  {
    if (!((Form_pg_type)GETSTRUCT(typtup))->typisdefined)
    {
      if (languageOid == SQLlanguageId)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("SQL function cannot return shell type %s", TypeNameToString(returnType))));
      }
      else
      {
        ereport(NOTICE, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("return type %s is only a shell", TypeNameToString(returnType))));
      }
    }
    rettype = typeTypeId(typtup);
    ReleaseSysCache(typtup);
  }
  else
  {
    char *typnam = TypeNameToString(returnType);
    Oid namespaceId;
    AclResult aclresult;
    char *typname;
    ObjectAddress address;

       
                                                                     
                                                                      
                                                                
                    
       
    if (languageOid != INTERNALlanguageId && languageOid != ClanguageId)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" does not exist", typnam)));
    }

                                                  
    if (returnType->typmods != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("type modifier cannot be specified for shell type \"%s\"", typnam)));
    }

                                                   
    ereport(NOTICE, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" is not yet defined", typnam), errdetail("Creating a shell type definition.")));
    namespaceId = QualifiedNameGetCreationNamespace(returnType->names, &typname);
    aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceId));
    }
    address = TypeShellMake(typname, namespaceId, GetUserId());
    rettype = address.objectId;
    Assert(OidIsValid(rettype));
  }

  aclresult = pg_type_aclcheck(rettype, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, rettype);
  }

  *prorettype_p = rettype;
  *returnsSet_p = returnType->setof;
}

   
                                                                 
                               
   
                     
                                                 
                                                                               
                                                                             
   
                                                                          
                                                                   
                                                                           
                                                               
                                                                           
                                                         
   
void
interpret_function_parameter_list(ParseState *pstate, List *parameters, Oid languageOid, ObjectType objtype, oidvector **parameterTypes, ArrayType **allParameterTypes, ArrayType **parameterModes, ArrayType **parameterNames, List **parameterDefaults, Oid *variadicArgType, Oid *requiredResultType)
{
  int parameterCount = list_length(parameters);
  Oid *inTypes;
  int inCount = 0;
  Datum *allTypes;
  Datum *paramModes;
  Datum *paramNames;
  int outCount = 0;
  int varCount = 0;
  bool have_names = false;
  bool have_defaults = false;
  ListCell *x;
  int i;

  *variadicArgType = InvalidOid;                        
  *requiredResultType = InvalidOid;                     

  inTypes = (Oid *)palloc(parameterCount * sizeof(Oid));
  allTypes = (Datum *)palloc(parameterCount * sizeof(Datum));
  paramModes = (Datum *)palloc(parameterCount * sizeof(Datum));
  paramNames = (Datum *)palloc0(parameterCount * sizeof(Datum));
  *parameterDefaults = NIL;

                                                       
  i = 0;
  foreach (x, parameters)
  {
    FunctionParameter *fp = (FunctionParameter *)lfirst(x);
    TypeName *t = fp->argType;
    bool isinput = false;
    Oid toid;
    Type typtup;
    AclResult aclresult;

    typtup = LookupTypeName(NULL, t, NULL, false);
    if (typtup)
    {
      if (!((Form_pg_type)GETSTRUCT(typtup))->typisdefined)
      {
                                                     
        if (languageOid == SQLlanguageId)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("SQL function cannot accept shell type %s", TypeNameToString(t))));
        }
                                                                      
        else if (objtype == OBJECT_AGGREGATE)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregate cannot accept shell type %s", TypeNameToString(t))));
        }
        else
        {
          ereport(NOTICE, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("argument type %s is only a shell", TypeNameToString(t))));
        }
      }
      toid = typeTypeId(typtup);
      ReleaseSysCache(typtup);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type %s does not exist", TypeNameToString(t))));
      toid = InvalidOid;                          
    }

    aclresult = pg_type_aclcheck(toid, GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error_type(aclresult, toid);
    }

    if (t->setof)
    {
      if (objtype == OBJECT_AGGREGATE)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregates cannot accept set arguments")));
      }
      else if (objtype == OBJECT_PROCEDURE)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("procedures cannot accept set arguments")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("functions cannot accept set arguments")));
      }
    }

    if (objtype == OBJECT_PROCEDURE)
    {
      if (fp->mode == FUNC_PARAM_OUT)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), (errmsg("procedures cannot have OUT arguments"), errhint("INOUT arguments are permitted."))));
      }
    }

                                 
    if (fp->mode != FUNC_PARAM_OUT && fp->mode != FUNC_PARAM_TABLE)
    {
                                                                    
      if (varCount > 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("VARIADIC parameter must be the last input parameter")));
      }
      inTypes[inCount++] = toid;
      isinput = true;
    }

                                  
    if (fp->mode != FUNC_PARAM_IN && fp->mode != FUNC_PARAM_VARIADIC)
    {
      if (objtype == OBJECT_PROCEDURE)
      {
        *requiredResultType = RECORDOID;
      }
      else if (outCount == 0)                                     
      {
        *requiredResultType = toid;
      }
      outCount++;
    }

    if (fp->mode == FUNC_PARAM_VARIADIC)
    {
      *variadicArgType = toid;
      varCount++;
                                            
      switch (toid)
      {
      case ANYARRAYOID:
      case ANYOID:
                  
        break;
      default:
        if (!OidIsValid(get_element_type(toid)))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("VARIADIC parameter must be an array")));
        }
        break;
      }
    }

    allTypes[i] = ObjectIdGetDatum(toid);

    paramModes[i] = CharGetDatum(fp->mode);

    if (fp->name && fp->name[0])
    {
      ListCell *px;

         
                                                                    
                                                                    
                                                                       
                                                                  
         
      foreach (px, parameters)
      {
        FunctionParameter *prevfp = (FunctionParameter *)lfirst(px);

        if (prevfp == fp)
        {
          break;
        }
                                                    
        if ((fp->mode == FUNC_PARAM_IN || fp->mode == FUNC_PARAM_VARIADIC) && (prevfp->mode == FUNC_PARAM_OUT || prevfp->mode == FUNC_PARAM_TABLE))
        {
          continue;
        }
        if ((prevfp->mode == FUNC_PARAM_IN || prevfp->mode == FUNC_PARAM_VARIADIC) && (fp->mode == FUNC_PARAM_OUT || fp->mode == FUNC_PARAM_TABLE))
        {
          continue;
        }
        if (prevfp->name && prevfp->name[0] && strcmp(prevfp->name, fp->name) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("parameter name \"%s\" used more than once", fp->name)));
        }
      }

      paramNames[i] = CStringGetTextDatum(fp->name);
      have_names = true;
    }

    if (fp->defexpr)
    {
      Node *def;

      if (!isinput)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("only input parameters can have default values")));
      }

      def = transformExpr(pstate, fp->defexpr, EXPR_KIND_FUNCTION_DEFAULT);
      def = coerce_to_specific_type(pstate, def, toid, "DEFAULT");
      assign_expr_collations(pstate, def);

         
                                                                       
                                                     
         
      if (list_length(pstate->p_rtable) != 0 || contain_var_clause(def))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("cannot use table references in parameter default value")));
      }

         
                                                                  
                                                                         
                             
         
                                                                      
                                      
         
                                                                    
                                                                        
                                                                        
                       
         

      *parameterDefaults = lappend(*parameterDefaults, def);
      have_defaults = true;
    }
    else
    {
      if (isinput && have_defaults)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("input parameters after one with a default value must also have defaults")));
      }
    }

    i++;
  }

                                                  
  *parameterTypes = buildoidvector(inTypes, inCount);

  if (outCount > 0 || varCount > 0)
  {
    *allParameterTypes = construct_array(allTypes, parameterCount, OIDOID, sizeof(Oid), true, 'i');
    *parameterModes = construct_array(paramModes, parameterCount, CHAROID, 1, true, 'c');
    if (outCount > 1)
    {
      *requiredResultType = RECORDOID;
    }
                                                             
  }
  else
  {
    *allParameterTypes = NULL;
    *parameterModes = NULL;
  }

  if (have_names)
  {
    for (i = 0; i < parameterCount; i++)
    {
      if (paramNames[i] == PointerGetDatum(NULL))
      {
        paramNames[i] = CStringGetTextDatum("");
      }
    }
    *parameterNames = construct_array(paramNames, parameterCount, TEXTOID, -1, false, 'i');
  }
  else
  {
    *parameterNames = NULL;
  }
}

   
                                                                  
                                                                
                                                                    
                                                                    
                                                                      
                                                                      
   
static bool
compute_common_attribute(ParseState *pstate, bool is_procedure, DefElem *defel, DefElem **volatility_item, DefElem **strict_item, DefElem **security_item, DefElem **leakproof_item, List **set_items, DefElem **cost_item, DefElem **rows_item, DefElem **support_item, DefElem **parallel_item)
{
  if (strcmp(defel->defname, "volatility") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*volatility_item)
    {
      goto duplicate_error;
    }

    *volatility_item = defel;
  }
  else if (strcmp(defel->defname, "strict") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*strict_item)
    {
      goto duplicate_error;
    }

    *strict_item = defel;
  }
  else if (strcmp(defel->defname, "security") == 0)
  {
    if (*security_item)
    {
      goto duplicate_error;
    }

    *security_item = defel;
  }
  else if (strcmp(defel->defname, "leakproof") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*leakproof_item)
    {
      goto duplicate_error;
    }

    *leakproof_item = defel;
  }
  else if (strcmp(defel->defname, "set") == 0)
  {
    *set_items = lappend(*set_items, defel->arg);
  }
  else if (strcmp(defel->defname, "cost") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*cost_item)
    {
      goto duplicate_error;
    }

    *cost_item = defel;
  }
  else if (strcmp(defel->defname, "rows") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*rows_item)
    {
      goto duplicate_error;
    }

    *rows_item = defel;
  }
  else if (strcmp(defel->defname, "support") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*support_item)
    {
      goto duplicate_error;
    }

    *support_item = defel;
  }
  else if (strcmp(defel->defname, "parallel") == 0)
  {
    if (is_procedure)
    {
      goto procedure_error;
    }
    if (*parallel_item)
    {
      goto duplicate_error;
    }

    *parallel_item = defel;
  }
  else
  {
    return false;
  }

                            
  return true;

duplicate_error:
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
  return false;                          

procedure_error:
  ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("invalid attribute in procedure definition"), parser_errposition(pstate, defel->location)));
  return false;
}

static char
interpret_func_volatility(DefElem *defel)
{
  char *str = strVal(defel->arg);

  if (strcmp(str, "immutable") == 0)
  {
    return PROVOLATILE_IMMUTABLE;
  }
  else if (strcmp(str, "stable") == 0)
  {
    return PROVOLATILE_STABLE;
  }
  else if (strcmp(str, "volatile") == 0)
  {
    return PROVOLATILE_VOLATILE;
  }
  else
  {
    elog(ERROR, "invalid volatility \"%s\"", str);
    return 0;                          
  }
}

static char
interpret_func_parallel(DefElem *defel)
{
  char *str = strVal(defel->arg);

  if (strcmp(str, "safe") == 0)
  {
    return PROPARALLEL_SAFE;
  }
  else if (strcmp(str, "unsafe") == 0)
  {
    return PROPARALLEL_UNSAFE;
  }
  else if (strcmp(str, "restricted") == 0)
  {
    return PROPARALLEL_RESTRICTED;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("parameter \"parallel\" must be SAFE, RESTRICTED, or UNSAFE")));
    return PROPARALLEL_UNSAFE;                          
  }
}

   
                                                                          
   
                                                             
   
static ArrayType *
update_proconfig_value(ArrayType *a, List *set_items)
{
  ListCell *l;

  foreach (l, set_items)
  {
    VariableSetStmt *sstmt = lfirst_node(VariableSetStmt, l);

    if (sstmt->kind == VAR_RESET_ALL)
    {
      a = NULL;
    }
    else
    {
      char *valuestr = ExtractSetVariableArgs(sstmt);

      if (valuestr)
      {
        a = GUCArrayAdd(a, sstmt->name, valuestr);
      }
      else            
      {
        a = GUCArrayDelete(a, sstmt->name);
      }
    }
  }

  return a;
}

static Oid
interpret_func_support(DefElem *defel)
{
  List *procName = defGetQualifiedName(defel);
  Oid procOid;
  Oid argList[1];

     
                                                                    
               
     
  argList[0] = INTERNALOID;

  procOid = LookupFuncName(procName, 1, argList, true);
  if (!OidIsValid(procOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(procName, 1, NIL, argList))));
  }

  if (get_func_rettype(procOid) != INTERNALOID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("support function %s must return type %s", NameListToString(procName), "internal")));
  }

     
                                                                          
                                                                         
                               
     
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to specify a support function")));
  }

  return procOid;
}

   
                                                                 
               
   
static void
compute_function_attributes(ParseState *pstate, bool is_procedure, List *options, List **as, char **language, Node **transform, bool *windowfunc_p, char *volatility_p, bool *strict_p, bool *security_definer, bool *leakproof_p, ArrayType **proconfig, float4 *procost, float4 *prorows, Oid *prosupport, char *parallel_p)
{
  ListCell *option;
  DefElem *as_item = NULL;
  DefElem *language_item = NULL;
  DefElem *transform_item = NULL;
  DefElem *windowfunc_item = NULL;
  DefElem *volatility_item = NULL;
  DefElem *strict_item = NULL;
  DefElem *security_item = NULL;
  DefElem *leakproof_item = NULL;
  List *set_items = NIL;
  DefElem *cost_item = NULL;
  DefElem *rows_item = NULL;
  DefElem *support_item = NULL;
  DefElem *parallel_item = NULL;

  foreach (option, options)
  {
    DefElem *defel = (DefElem *)lfirst(option);

    if (strcmp(defel->defname, "as") == 0)
    {
      if (as_item)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      as_item = defel;
    }
    else if (strcmp(defel->defname, "language") == 0)
    {
      if (language_item)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      language_item = defel;
    }
    else if (strcmp(defel->defname, "transform") == 0)
    {
      if (transform_item)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      transform_item = defel;
    }
    else if (strcmp(defel->defname, "window") == 0)
    {
      if (windowfunc_item)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      if (is_procedure)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("invalid attribute in procedure definition"), parser_errposition(pstate, defel->location)));
      }
      windowfunc_item = defel;
    }
    else if (compute_common_attribute(pstate, is_procedure, defel, &volatility_item, &strict_item, &security_item, &leakproof_item, &set_items, &cost_item, &rows_item, &support_item, &parallel_item))
    {
                                    
      continue;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

                              
  if (as_item)
  {
    *as = (List *)as_item->arg;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("no function body specified")));
    *as = NIL;                          
  }

  if (language_item)
  {
    *language = strVal(language_item->arg);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("no language specified")));
    *language = NULL;                          
  }

                              
  if (transform_item)
  {
    *transform = transform_item->arg;
  }
  if (windowfunc_item)
  {
    *windowfunc_p = intVal(windowfunc_item->arg);
  }
  if (volatility_item)
  {
    *volatility_p = interpret_func_volatility(volatility_item);
  }
  if (strict_item)
  {
    *strict_p = intVal(strict_item->arg);
  }
  if (security_item)
  {
    *security_definer = intVal(security_item->arg);
  }
  if (leakproof_item)
  {
    *leakproof_p = intVal(leakproof_item->arg);
  }
  if (set_items)
  {
    *proconfig = update_proconfig_value(NULL, set_items);
  }
  if (cost_item)
  {
    *procost = defGetNumeric(cost_item);
    if (*procost <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COST must be positive")));
    }
  }
  if (rows_item)
  {
    *prorows = defGetNumeric(rows_item);
    if (*prorows <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ROWS must be positive")));
    }
  }
  if (support_item)
  {
    *prosupport = interpret_func_support(support_item);
  }
  if (parallel_item)
  {
    *parallel_p = interpret_func_parallel(parallel_item);
  }
}

   
                                                                         
   
                                                    
   
                      
   
                                         
   
static void
interpret_AS_clause(Oid languageOid, const char *languageName, char *funcname, List *as, char **prosrc_str_p, char **probin_str_p)
{
  Assert(as != NIL);

  if (languageOid == ClanguageId)
  {
       
                                                                        
                                                                   
                                                                   
                                                                        
                                                                         
                               
       
    *probin_str_p = strVal(linitial(as));
    if (list_length(as) == 1)
    {
      *prosrc_str_p = funcname;
    }
    else
    {
      *prosrc_str_p = strVal(lsecond(as));
      if (strcmp(*prosrc_str_p, "-") == 0)
      {
        *prosrc_str_p = funcname;
      }
    }
  }
  else
  {
                                                           
    *prosrc_str_p = strVal(linitial(as));
    *probin_str_p = NULL;

    if (list_length(as) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("only one AS item needed for language \"%s\"", languageName)));
    }

    if (languageOid == INTERNALlanguageId)
    {
         
                                                                        
                                                                     
                                                                     
                                                                       
                                                                      
                                                          
         
      if (strlen(*prosrc_str_p) == 0)
      {
        *prosrc_str_p = funcname;
      }
    }
  }
}

   
                  
                                                                       
   
ObjectAddress
CreateFunction(ParseState *pstate, CreateFunctionStmt *stmt)
{
  char *probin_str;
  char *prosrc_str;
  Oid prorettype;
  bool returnsSet;
  char *language;
  Oid languageOid;
  Oid languageValidator;
  Node *transformDefElem = NULL;
  char *funcname;
  Oid namespaceId;
  AclResult aclresult;
  oidvector *parameterTypes;
  ArrayType *allParameterTypes;
  ArrayType *parameterModes;
  ArrayType *parameterNames;
  List *parameterDefaults;
  Oid variadicArgType;
  List *trftypes_list = NIL;
  ArrayType *trftypes;
  Oid requiredResultType;
  bool isWindowFunc, isStrict, security, isLeakProof;
  char volatility;
  ArrayType *proconfig;
  float4 procost;
  float4 prorows;
  Oid prosupport;
  HeapTuple languageTuple;
  Form_pg_language languageStruct;
  List *as_clause;
  char parallel;

                                                     
  namespaceId = QualifiedNameGetCreationNamespace(stmt->funcname, &funcname);

                                                         
  aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceId));
  }

                              
  isWindowFunc = false;
  isStrict = false;
  security = false;
  isLeakProof = false;
  volatility = PROVOLATILE_VOLATILE;
  proconfig = NULL;
  procost = -1;                        
  prorows = -1;                        
  prosupport = InvalidOid;
  parallel = PROPARALLEL_UNSAFE;

                                                              
  compute_function_attributes(pstate, stmt->is_procedure, stmt->options, &as_clause, &language, &transformDefElem, &isWindowFunc, &volatility, &isStrict, &security, &isLeakProof, &proconfig, &procost, &prorows, &prosupport, &parallel);

                                                     
  languageTuple = SearchSysCache1(LANGNAME, PointerGetDatum(language));
  if (!HeapTupleIsValid(languageTuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("language \"%s\" does not exist", language), (PLTemplateExists(language) ? errhint("Use CREATE EXTENSION to load the language into the database.") : 0)));
  }

  languageStruct = (Form_pg_language)GETSTRUCT(languageTuple);
  languageOid = languageStruct->oid;

  if (languageStruct->lanpltrusted)
  {
                                                   
    AclResult aclresult;

    aclresult = pg_language_aclcheck(languageOid, GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_LANGUAGE, NameStr(languageStruct->lanname));
    }
  }
  else
  {
                                                  
    if (!superuser())
    {
      aclcheck_error(ACLCHECK_NO_PRIV, OBJECT_LANGUAGE, NameStr(languageStruct->lanname));
    }
  }

  languageValidator = languageStruct->lanvalidator;

  ReleaseSysCache(languageTuple);

     
                                                                     
                                                                             
                                                               
     
  if (isLeakProof && !superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("only superuser can define a leakproof function")));
  }

  if (transformDefElem)
  {
    ListCell *lc;

    foreach (lc, castNode(List, transformDefElem))
    {
      Oid typeid = typenameTypeId(NULL, lfirst_node(TypeName, lc));
      Oid elt = get_base_element_type(typeid);

      typeid = elt ? elt : typeid;

      get_transform_oid(typeid, languageOid, false);
      trftypes_list = lappend_oid(trftypes_list, typeid);
    }
  }

     
                                                              
                      
     
  interpret_function_parameter_list(pstate, stmt->parameters, languageOid, stmt->is_procedure ? OBJECT_PROCEDURE : OBJECT_FUNCTION, &parameterTypes, &allParameterTypes, &parameterModes, &parameterNames, &parameterDefaults, &variadicArgType, &requiredResultType);

  if (stmt->is_procedure)
  {
    Assert(!stmt->returnType);
    prorettype = requiredResultType ? requiredResultType : VOIDOID;
    returnsSet = false;
  }
  else if (stmt->returnType)
  {
                                 
    compute_return_type(stmt->returnType, languageOid, &prorettype, &returnsSet);
    if (OidIsValid(requiredResultType) && prorettype != requiredResultType)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("function result type must be %s because of OUT parameters", format_type_be(requiredResultType))));
    }
  }
  else if (OidIsValid(requiredResultType))
  {
                                                    
    prorettype = requiredResultType;
    returnsSet = false;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("function result type must be specified")));
                                                          
    prorettype = VOIDOID;
    returnsSet = false;
  }

  if (list_length(trftypes_list) > 0)
  {
    ListCell *lc;
    Datum *arr;
    int i;

    arr = palloc(list_length(trftypes_list) * sizeof(Datum));
    i = 0;
    foreach (lc, trftypes_list)
    {
      arr[i++] = ObjectIdGetDatum(lfirst_oid(lc));
    }
    trftypes = construct_array(arr, list_length(trftypes_list), OIDOID, sizeof(Oid), true, 'i');
  }
  else
  {
                                               
    trftypes = NULL;
  }

  interpret_AS_clause(languageOid, language, funcname, as_clause, &prosrc_str, &probin_str);

     
                                                                         
                                                                          
                                                 
     
  if (procost < 0)
  {
                                                                  
    if (languageOid == INTERNALlanguageId || languageOid == ClanguageId)
    {
      procost = 1;
    }
    else
    {
      procost = 100;
    }
  }
  if (prorows < 0)
  {
    if (returnsSet)
    {
      prorows = 1000;
    }
    else
    {
      prorows = 0;                                    
    }
  }
  else if (!returnsSet)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ROWS is not applicable when function does not return a set")));
  }

     
                                                                             
                                           
     
  return ProcedureCreate(funcname, namespaceId, stmt->replace, returnsSet, prorettype, GetUserId(), languageOid, languageValidator, prosrc_str,                              
      probin_str,                                                                                                                                                            
      stmt->is_procedure ? PROKIND_PROCEDURE : (isWindowFunc ? PROKIND_WINDOW : PROKIND_FUNCTION), security, isLeakProof, isStrict, volatility, parallel, parameterTypes, PointerGetDatum(allParameterTypes), PointerGetDatum(parameterModes), PointerGetDatum(parameterNames), parameterDefaults, PointerGetDatum(trftypes), PointerGetDatum(proconfig), prosupport, procost, prorows);
}

   
                              
   
                                                                     
                                                   
   
void
RemoveFunctionById(Oid funcOid)
{
  Relation relation;
  HeapTuple tup;
  char prokind;

     
                               
     
  relation = table_open(ProcedureRelationId, RowExclusiveLock);

  tup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for function %u", funcOid);
  }

  prokind = ((Form_pg_proc)GETSTRUCT(tup))->prokind;

  CatalogTupleDelete(relation, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(relation, RowExclusiveLock);

     
                                                       
     
  if (prokind == PROKIND_AGGREGATE)
  {
    relation = table_open(AggregateRelationId, RowExclusiveLock);

    tup = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(funcOid));
    if (!HeapTupleIsValid(tup))                        
    {
      elog(ERROR, "cache lookup failed for pg_aggregate tuple for function %u", funcOid);
    }

    CatalogTupleDelete(relation, &tup->t_self);

    ReleaseSysCache(tup);

    table_close(relation, RowExclusiveLock);
  }
}

   
                                                                 
                                                                      
                     
   
ObjectAddress
AlterFunction(ParseState *pstate, AlterFunctionStmt *stmt)
{
  HeapTuple tup;
  Oid funcOid;
  Form_pg_proc procForm;
  bool is_procedure;
  Relation rel;
  ListCell *l;
  DefElem *volatility_item = NULL;
  DefElem *strict_item = NULL;
  DefElem *security_def_item = NULL;
  DefElem *leakproof_item = NULL;
  List *set_items = NIL;
  DefElem *cost_item = NULL;
  DefElem *rows_item = NULL;
  DefElem *support_item = NULL;
  DefElem *parallel_item = NULL;
  ObjectAddress address;

  rel = table_open(ProcedureRelationId, RowExclusiveLock);

  funcOid = LookupFuncWithArgs(stmt->objtype, stmt->func, false);

  ObjectAddressSet(address, ProcedureRelationId, funcOid);

  tup = SearchSysCacheCopy1(PROCOID, ObjectIdGetDatum(funcOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for function %u", funcOid);
  }

  procForm = (Form_pg_proc)GETSTRUCT(tup);

                                           
  if (!pg_proc_ownercheck(funcOid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, stmt->objtype, NameListToString(stmt->func->objname));
  }

  if (procForm->prokind == PROKIND_AGGREGATE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an aggregate function", NameListToString(stmt->func->objname))));
  }

  is_procedure = (procForm->prokind == PROKIND_PROCEDURE);

                                  
  foreach (l, stmt->actions)
  {
    DefElem *defel = (DefElem *)lfirst(l);

    if (compute_common_attribute(pstate, is_procedure, defel, &volatility_item, &strict_item, &security_def_item, &leakproof_item, &set_items, &cost_item, &rows_item, &support_item, &parallel_item) == false)
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

  if (volatility_item)
  {
    procForm->provolatile = interpret_func_volatility(volatility_item);
  }
  if (strict_item)
  {
    procForm->proisstrict = intVal(strict_item->arg);
  }
  if (security_def_item)
  {
    procForm->prosecdef = intVal(security_def_item->arg);
  }
  if (leakproof_item)
  {
    procForm->proleakproof = intVal(leakproof_item->arg);
    if (procForm->proleakproof && !superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("only superuser can define a leakproof function")));
    }
  }
  if (cost_item)
  {
    procForm->procost = defGetNumeric(cost_item);
    if (procForm->procost <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COST must be positive")));
    }
  }
  if (rows_item)
  {
    procForm->prorows = defGetNumeric(rows_item);
    if (procForm->prorows <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ROWS must be positive")));
    }
    if (!procForm->proretset)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ROWS is not applicable when function does not return a set")));
    }
  }
  if (support_item)
  {
                                                            
    Oid newsupport = interpret_func_support(support_item);

                                                       
    if (OidIsValid(procForm->prosupport))
    {
      changeDependencyFor(ProcedureRelationId, funcOid, ProcedureRelationId, procForm->prosupport, newsupport);
    }
    else
    {
      ObjectAddress referenced;

      referenced.classId = ProcedureRelationId;
      referenced.objectId = newsupport;
      referenced.objectSubId = 0;
      recordDependencyOn(&address, &referenced, DEPENDENCY_NORMAL);
    }

    procForm->prosupport = newsupport;
  }
  if (parallel_item)
  {
    procForm->proparallel = interpret_func_parallel(parallel_item);
  }
  if (set_items)
  {
    Datum datum;
    bool isnull;
    ArrayType *a;
    Datum repl_val[Natts_pg_proc];
    bool repl_null[Natts_pg_proc];
    bool repl_repl[Natts_pg_proc];

                                            
    datum = SysCacheGetAttr(PROCOID, tup, Anum_pg_proc_proconfig, &isnull);
    a = isnull ? NULL : DatumGetArrayTypeP(datum);

                                                                   
    a = update_proconfig_value(a, set_items);

                          
    memset(repl_repl, false, sizeof(repl_repl));
    repl_repl[Anum_pg_proc_proconfig - 1] = true;

    if (a == NULL)
    {
      repl_val[Anum_pg_proc_proconfig - 1] = (Datum)0;
      repl_null[Anum_pg_proc_proconfig - 1] = true;
    }
    else
    {
      repl_val[Anum_pg_proc_proconfig - 1] = PointerGetDatum(a);
      repl_null[Anum_pg_proc_proconfig - 1] = false;
    }

    tup = heap_modify_tuple(tup, RelationGetDescr(rel), repl_val, repl_null, repl_repl);
  }
                                                                          

                     
  CatalogTupleUpdate(rel, &tup->t_self, tup);

  InvokeObjectPostAlterHook(ProcedureRelationId, funcOid, 0);

  table_close(rel, NoLock);
  heap_freetuple(tup);

  return address;
}

   
                                                                     
   
                                                                          
                                                                         
                                                                     
   
void
SetFunctionReturnType(Oid funcOid, Oid newRetType)
{
  Relation pg_proc_rel;
  HeapTuple tup;
  Form_pg_proc procForm;
  ObjectAddress func_address;
  ObjectAddress type_address;

  pg_proc_rel = table_open(ProcedureRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy1(PROCOID, ObjectIdGetDatum(funcOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for function %u", funcOid);
  }
  procForm = (Form_pg_proc)GETSTRUCT(tup);

  if (procForm->prorettype != OPAQUEOID)                       
  {
    elog(ERROR, "function %u doesn't return OPAQUE", funcOid);
  }

                                      
  procForm->prorettype = newRetType;

                                          
  CatalogTupleUpdate(pg_proc_rel, &tup->t_self, tup);

  table_close(pg_proc_rel, RowExclusiveLock);

     
                                                                             
                                                                            
     
  ObjectAddressSet(type_address, TypeRelationId, newRetType);
  ObjectAddressSet(func_address, ProcedureRelationId, funcOid);
  recordDependencyOn(&func_address, &type_address, DEPENDENCY_NORMAL);
}

   
                                                                    
   
                                            
   
void
SetFunctionArgType(Oid funcOid, int argIndex, Oid newArgType)
{
  Relation pg_proc_rel;
  HeapTuple tup;
  Form_pg_proc procForm;
  ObjectAddress func_address;
  ObjectAddress type_address;

  pg_proc_rel = table_open(ProcedureRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy1(PROCOID, ObjectIdGetDatum(funcOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for function %u", funcOid);
  }
  procForm = (Form_pg_proc)GETSTRUCT(tup);

  if (argIndex < 0 || argIndex >= procForm->pronargs || procForm->proargtypes.values[argIndex] != OPAQUEOID)
  {
    elog(ERROR, "function %u doesn't take OPAQUE", funcOid);
  }

                                      
  procForm->proargtypes.values[argIndex] = newArgType;

                                          
  CatalogTupleUpdate(pg_proc_rel, &tup->t_self, tup);

  table_close(pg_proc_rel, RowExclusiveLock);

     
                                                                             
                                                                            
     
  ObjectAddressSet(type_address, TypeRelationId, newArgType);
  ObjectAddressSet(func_address, ProcedureRelationId, funcOid);
  recordDependencyOn(&func_address, &type_address, DEPENDENCY_NORMAL);
}

   
               
   
ObjectAddress
CreateCast(CreateCastStmt *stmt)
{
  Oid sourcetypeid;
  Oid targettypeid;
  char sourcetyptype;
  char targettyptype;
  Oid funcid;
  Oid castid;
  int nargs;
  char castcontext;
  char castmethod;
  Relation relation;
  HeapTuple tuple;
  Datum values[Natts_pg_cast];
  bool nulls[Natts_pg_cast];
  ObjectAddress myself, referenced;
  AclResult aclresult;

  sourcetypeid = typenameTypeId(NULL, stmt->sourcetype);
  targettypeid = typenameTypeId(NULL, stmt->targettype);
  sourcetyptype = get_typtype(sourcetypeid);
  targettyptype = get_typtype(targettypeid);

                               
  if (sourcetyptype == TYPTYPE_PSEUDO)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("source data type %s is a pseudo-type", TypeNameToString(stmt->sourcetype))));
  }

  if (targettyptype == TYPTYPE_PSEUDO)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("target data type %s is a pseudo-type", TypeNameToString(stmt->targettype))));
  }

                        
  if (!pg_type_ownercheck(sourcetypeid, GetUserId()) && !pg_type_ownercheck(targettypeid, GetUserId()))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be owner of type %s or type %s", format_type_be(sourcetypeid), format_type_be(targettypeid))));
  }

  aclresult = pg_type_aclcheck(sourcetypeid, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, sourcetypeid);
  }

  aclresult = pg_type_aclcheck(targettypeid, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, targettypeid);
  }

                                                               
  if (sourcetyptype == TYPTYPE_DOMAIN)
  {
    ereport(WARNING, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cast will be ignored because the source data type is a domain")));
  }

  else if (targettyptype == TYPTYPE_DOMAIN)
  {
    ereport(WARNING, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cast will be ignored because the target data type is a domain")));
  }

                                 
  if (stmt->func != NULL)
  {
    castmethod = COERCION_METHOD_FUNCTION;
  }
  else if (stmt->inout)
  {
    castmethod = COERCION_METHOD_INOUT;
  }
  else
  {
    castmethod = COERCION_METHOD_BINARY;
  }

  if (castmethod == COERCION_METHOD_FUNCTION)
  {
    Form_pg_proc procstruct;

    funcid = LookupFuncWithArgs(OBJECT_FUNCTION, stmt->func, false);

    tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", funcid);
    }

    procstruct = (Form_pg_proc)GETSTRUCT(tuple);
    nargs = procstruct->pronargs;
    if (nargs < 1 || nargs > 3)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cast function must take one to three arguments")));
    }
    if (!IsBinaryCoercible(sourcetypeid, procstruct->proargtypes.values[0]))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("argument of cast function must match or be binary-coercible from source data type")));
    }
    if (nargs > 1 && procstruct->proargtypes.values[1] != INT4OID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("second argument of cast function must be type %s", "integer")));
    }
    if (nargs > 2 && procstruct->proargtypes.values[2] != BOOLOID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("third argument of cast function must be type %s", "boolean")));
    }
    if (!IsBinaryCoercible(procstruct->prorettype, targettypeid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("return data type of cast function must match or be binary-coercible to target data type")));
    }

       
                                                                         
                                                                    
                                                              
       
#ifdef NOT_USED
    if (procstruct->provolatile == PROVOLATILE_VOLATILE)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cast function must not be volatile")));
    }
#endif
    if (procstruct->prokind != PROKIND_FUNCTION)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cast function must be a normal function")));
    }
    if (procstruct->proretset)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cast function must not return a set")));
    }

    ReleaseSysCache(tuple);
  }
  else
  {
    funcid = InvalidOid;
    nargs = 0;
  }

  if (castmethod == COERCION_METHOD_BINARY)
  {
    int16 typ1len;
    int16 typ2len;
    bool typ1byval;
    bool typ2byval;
    char typ1align;
    char typ2align;

       
                                                                  
                                                     
       
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create a cast WITHOUT FUNCTION")));
    }

       
                                                                    
                                                                           
                                                                          
                                             
       
    get_typlenbyvalalign(sourcetypeid, &typ1len, &typ1byval, &typ1align);
    get_typlenbyvalalign(targettypeid, &typ2len, &typ2byval, &typ2align);
    if (typ1len != typ2len || typ1byval != typ2byval || typ1align != typ2align)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("source and target data types are not physically compatible")));
    }

       
                                                                      
                                                                         
       
                                                                      
                                                                        
                                                                          
                                                                   
       
    if (sourcetyptype == TYPTYPE_COMPOSITE || targettyptype == TYPTYPE_COMPOSITE)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("composite data types are not binary-compatible")));
    }

    if (sourcetyptype == TYPTYPE_ENUM || targettyptype == TYPTYPE_ENUM)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("enum data types are not binary-compatible")));
    }

    if (OidIsValid(get_element_type(sourcetypeid)) || OidIsValid(get_element_type(targettypeid)))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("array data types are not binary-compatible")));
    }

       
                                                                      
                                                                   
                                                                     
                                                                           
                                                                         
       
                                                                       
                                                                       
                   
       
    if (sourcetyptype == TYPTYPE_DOMAIN || targettyptype == TYPTYPE_DOMAIN)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("domain data types must not be marked binary-compatible")));
    }
  }

     
                                                                       
                                                                      
     
  if (sourcetypeid == targettypeid && nargs < 2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("source data type and target data type are the same")));
  }

                                                                  
  switch (stmt->context)
  {
  case COERCION_IMPLICIT:
    castcontext = COERCION_CODE_IMPLICIT;
    break;
  case COERCION_ASSIGNMENT:
    castcontext = COERCION_CODE_ASSIGNMENT;
    break;
  case COERCION_EXPLICIT:
    castcontext = COERCION_CODE_EXPLICIT;
    break;
  default:
    elog(ERROR, "unrecognized CoercionContext: %d", stmt->context);
    castcontext = 0;                          
    break;
  }

  relation = table_open(CastRelationId, RowExclusiveLock);

     
                                                                          
                                                                            
                  
     
  tuple = SearchSysCache2(CASTSOURCETARGET, ObjectIdGetDatum(sourcetypeid), ObjectIdGetDatum(targettypeid));
  if (HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("cast from type %s to type %s already exists", format_type_be(sourcetypeid), format_type_be(targettypeid))));
  }

                   
  castid = GetNewOidWithIndex(relation, CastOidIndexId, Anum_pg_cast_oid);
  values[Anum_pg_cast_oid - 1] = ObjectIdGetDatum(castid);
  values[Anum_pg_cast_castsource - 1] = ObjectIdGetDatum(sourcetypeid);
  values[Anum_pg_cast_casttarget - 1] = ObjectIdGetDatum(targettypeid);
  values[Anum_pg_cast_castfunc - 1] = ObjectIdGetDatum(funcid);
  values[Anum_pg_cast_castcontext - 1] = CharGetDatum(castcontext);
  values[Anum_pg_cast_castmethod - 1] = CharGetDatum(castmethod);

  MemSet(nulls, false, sizeof(nulls));

  tuple = heap_form_tuple(RelationGetDescr(relation), values, nulls);

  CatalogTupleInsert(relation, tuple);

                               
  myself.classId = CastRelationId;
  myself.objectId = castid;
  myself.objectSubId = 0;

                                 
  referenced.classId = TypeRelationId;
  referenced.objectId = sourcetypeid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                 
  referenced.classId = TypeRelationId;
  referenced.objectId = targettypeid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                              
  if (OidIsValid(funcid))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = funcid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                               
  recordDependencyOnCurrentExtension(&myself, false);

                                       
  InvokeObjectPostCreateHook(CastRelationId, castid, 0);

  heap_freetuple(tuple);

  table_close(relation, RowExclusiveLock);

  return myself;
}

   
                                                          
   
                                                                        
                                 
   
Oid
get_cast_oid(Oid sourcetypeid, Oid targettypeid, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid2(CASTSOURCETARGET, Anum_pg_cast_oid, ObjectIdGetDatum(sourcetypeid), ObjectIdGetDatum(targettypeid));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("cast from type %s to type %s does not exist", format_type_be(sourcetypeid), format_type_be(targettypeid))));
  }
  return oid;
}

void
DropCastById(Oid castOid)
{
  Relation relation;
  ScanKeyData scankey;
  SysScanDesc scan;
  HeapTuple tuple;

  relation = table_open(CastRelationId, RowExclusiveLock);

  ScanKeyInit(&scankey, Anum_pg_cast_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(castOid));
  scan = systable_beginscan(relation, CastOidIndexId, true, NULL, 1, &scankey);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for cast %u", castOid);
  }
  CatalogTupleDelete(relation, &tuple->t_self);

  systable_endscan(scan);
  table_close(relation, RowExclusiveLock);
}

static void
check_transform_function(Form_pg_proc procstruct)
{
  if (procstruct->provolatile == PROVOLATILE_VOLATILE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("transform function must not be volatile")));
  }
  if (procstruct->prokind != PROKIND_FUNCTION)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("transform function must be a normal function")));
  }
  if (procstruct->proretset)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("transform function must not return a set")));
  }
  if (procstruct->pronargs != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("transform function must take one argument")));
  }
  if (procstruct->proargtypes.values[0] != INTERNALOID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("first argument of transform function must be type %s", "internal")));
  }
}

   
                    
   
ObjectAddress
CreateTransform(CreateTransformStmt *stmt)
{
  Oid typeid;
  char typtype;
  Oid langid;
  Oid fromsqlfuncid;
  Oid tosqlfuncid;
  AclResult aclresult;
  Form_pg_proc procstruct;
  Datum values[Natts_pg_transform];
  bool nulls[Natts_pg_transform];
  bool replaces[Natts_pg_transform];
  Oid transformid;
  HeapTuple tuple;
  HeapTuple newtuple;
  Relation relation;
  ObjectAddress myself, referenced;
  bool is_replace;

     
                  
     
  typeid = typenameTypeId(NULL, stmt->type_name);
  typtype = get_typtype(typeid);

  if (typtype == TYPTYPE_PSEUDO)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("data type %s is a pseudo-type", TypeNameToString(stmt->type_name))));
  }

  if (typtype == TYPTYPE_DOMAIN)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("data type %s is a domain", TypeNameToString(stmt->type_name))));
  }

  if (!pg_type_ownercheck(typeid, GetUserId()))
  {
    aclcheck_error_type(ACLCHECK_NOT_OWNER, typeid);
  }

  aclresult = pg_type_aclcheck(typeid, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, typeid);
  }

     
                      
     
  langid = get_language_oid(stmt->lang, false);

  aclresult = pg_language_aclcheck(langid, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_LANGUAGE, stmt->lang);
  }

     
                       
     
  if (stmt->fromsql)
  {
    fromsqlfuncid = LookupFuncWithArgs(OBJECT_FUNCTION, stmt->fromsql, false);

    if (!pg_proc_ownercheck(fromsqlfuncid, GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, NameListToString(stmt->fromsql->objname));
    }

    aclresult = pg_proc_aclcheck(fromsqlfuncid, GetUserId(), ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_FUNCTION, NameListToString(stmt->fromsql->objname));
    }

    tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(fromsqlfuncid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", fromsqlfuncid);
    }
    procstruct = (Form_pg_proc)GETSTRUCT(tuple);
    if (procstruct->prorettype != INTERNALOID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("return data type of FROM SQL function must be %s", "internal")));
    }
    check_transform_function(procstruct);
    ReleaseSysCache(tuple);
  }
  else
  {
    fromsqlfuncid = InvalidOid;
  }

  if (stmt->tosql)
  {
    tosqlfuncid = LookupFuncWithArgs(OBJECT_FUNCTION, stmt->tosql, false);

    if (!pg_proc_ownercheck(tosqlfuncid, GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, NameListToString(stmt->tosql->objname));
    }

    aclresult = pg_proc_aclcheck(tosqlfuncid, GetUserId(), ACL_EXECUTE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_FUNCTION, NameListToString(stmt->tosql->objname));
    }

    tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(tosqlfuncid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", tosqlfuncid);
    }
    procstruct = (Form_pg_proc)GETSTRUCT(tuple);
    if (procstruct->prorettype != typeid)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("return data type of TO SQL function must be the transform data type")));
    }
    check_transform_function(procstruct);
    ReleaseSysCache(tuple);
  }
  else
  {
    tosqlfuncid = InvalidOid;
  }

     
                 
     
  values[Anum_pg_transform_trftype - 1] = ObjectIdGetDatum(typeid);
  values[Anum_pg_transform_trflang - 1] = ObjectIdGetDatum(langid);
  values[Anum_pg_transform_trffromsql - 1] = ObjectIdGetDatum(fromsqlfuncid);
  values[Anum_pg_transform_trftosql - 1] = ObjectIdGetDatum(tosqlfuncid);

  MemSet(nulls, false, sizeof(nulls));

  relation = table_open(TransformRelationId, RowExclusiveLock);

  tuple = SearchSysCache2(TRFTYPELANG, ObjectIdGetDatum(typeid), ObjectIdGetDatum(langid));
  if (HeapTupleIsValid(tuple))
  {
    Form_pg_transform form = (Form_pg_transform)GETSTRUCT(tuple);

    if (!stmt->replace)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("transform for type %s language \"%s\" already exists", format_type_be(typeid), stmt->lang)));
    }

    MemSet(replaces, false, sizeof(replaces));
    replaces[Anum_pg_transform_trffromsql - 1] = true;
    replaces[Anum_pg_transform_trftosql - 1] = true;

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);
    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

    transformid = form->oid;
    ReleaseSysCache(tuple);
    is_replace = true;
  }
  else
  {
    transformid = GetNewOidWithIndex(relation, TransformOidIndexId, Anum_pg_transform_oid);
    values[Anum_pg_transform_oid - 1] = ObjectIdGetDatum(transformid);
    newtuple = heap_form_tuple(RelationGetDescr(relation), values, nulls);
    CatalogTupleInsert(relation, newtuple);
    is_replace = false;
  }

  if (is_replace)
  {
    deleteDependencyRecordsFor(TransformRelationId, transformid, true);
  }

                               
  myself.classId = TransformRelationId;
  myself.objectId = transformid;
  myself.objectSubId = 0;

                              
  referenced.classId = LanguageRelationId;
  referenced.objectId = langid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                          
  referenced.classId = TypeRelationId;
  referenced.objectId = typeid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                 
  if (OidIsValid(fromsqlfuncid))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = fromsqlfuncid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }
  if (OidIsValid(tosqlfuncid))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = tosqlfuncid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                               
  recordDependencyOnCurrentExtension(&myself, is_replace);

                                            
  InvokeObjectPostCreateHook(TransformRelationId, transformid, 0);

  heap_freetuple(newtuple);

  table_close(relation, RowExclusiveLock);

  return myself;
}

   
                                                                                
   
                                                                             
                                 
   
Oid
get_transform_oid(Oid type_id, Oid lang_id, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid2(TRFTYPELANG, Anum_pg_transform_oid, ObjectIdGetDatum(type_id), ObjectIdGetDatum(lang_id));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("transform for type %s language \"%s\" does not exist", format_type_be(type_id), get_language_name(lang_id, false))));
  }
  return oid;
}

void
DropTransformById(Oid transformOid)
{
  Relation relation;
  ScanKeyData scankey;
  SysScanDesc scan;
  HeapTuple tuple;

  relation = table_open(TransformRelationId, RowExclusiveLock);

  ScanKeyInit(&scankey, Anum_pg_transform_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(transformOid));
  scan = systable_beginscan(relation, TransformOidIndexId, true, NULL, 1, &scankey);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for transform %u", transformOid);
  }
  CatalogTupleDelete(relation, &tuple->t_self);

  systable_endscan(scan);
  table_close(relation, RowExclusiveLock);
}

   
                                                             
   
                                                                              
                                                          
   
void
IsThereFunctionInNamespace(const char *proname, int pronargs, oidvector *proargtypes, Oid nspOid)
{
                                                                          
  if (SearchSysCacheExists3(PROCNAMEARGSNSP, CStringGetDatum(proname), PointerGetDatum(proargtypes), ObjectIdGetDatum(nspOid)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_FUNCTION), errmsg("function %s already exists in schema \"%s\"", funcname_signature_string(proname, pronargs, NIL, proargtypes->values), get_namespace_name(nspOid))));
  }
}

   
                 
                                            
   
                                                       
   
void
ExecuteDoStmt(DoStmt *stmt, bool atomic)
{
  InlineCodeBlock *codeblock = makeNode(InlineCodeBlock);
  ListCell *arg;
  DefElem *as_item = NULL;
  DefElem *language_item = NULL;
  char *language;
  Oid laninline;
  HeapTuple languageTuple;
  Form_pg_language languageStruct;

                                          
  foreach (arg, stmt->args)
  {
    DefElem *defel = (DefElem *)lfirst(arg);

    if (strcmp(defel->defname, "as") == 0)
    {
      if (as_item)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      as_item = defel;
    }
    else if (strcmp(defel->defname, "language") == 0)
    {
      if (language_item)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      language_item = defel;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

  if (as_item)
  {
    codeblock->source_text = strVal(as_item->arg);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("no inline code specified")));
  }

                                                            
  if (language_item)
  {
    language = strVal(language_item->arg);
  }
  else
  {
    language = "plpgsql";
  }

                                                     
  languageTuple = SearchSysCache1(LANGNAME, PointerGetDatum(language));
  if (!HeapTupleIsValid(languageTuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("language \"%s\" does not exist", language), (PLTemplateExists(language) ? errhint("Use CREATE EXTENSION to load the language into the database.") : 0)));
  }

  languageStruct = (Form_pg_language)GETSTRUCT(languageTuple);
  codeblock->langOid = languageStruct->oid;
  codeblock->langIsTrusted = languageStruct->lanpltrusted;
  codeblock->atomic = atomic;

  if (languageStruct->lanpltrusted)
  {
                                                   
    AclResult aclresult;

    aclresult = pg_language_aclcheck(codeblock->langOid, GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_LANGUAGE, NameStr(languageStruct->lanname));
    }
  }
  else
  {
                                                  
    if (!superuser())
    {
      aclcheck_error(ACLCHECK_NO_PRIV, OBJECT_LANGUAGE, NameStr(languageStruct->lanname));
    }
  }

                                      
  laninline = languageStruct->laninline;
  if (!OidIsValid(laninline))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("language \"%s\" does not support inline code execution", NameStr(languageStruct->lanname))));
  }

  ReleaseSysCache(languageTuple);

                                  
  OidFunctionCall1(laninline, PointerGetDatum(codeblock));
}

   
                          
   
                                                                               
                                                                               
                                                                           
                                                                              
                                                                             
                                                                          
                                                                              
                                                                          
                                                                          
   
                                                                               
                                                                        
                                                                        
                                                                    
                                                                            
                                                              
                                                                             
                                                  
   
                                                                       
                                                                        
                                                                        
                                                                       
                                                  
   
void
ExecuteCallStmt(CallStmt *stmt, ParamListInfo params, bool atomic, DestReceiver *dest)
{
  LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
  ListCell *lc;
  FuncExpr *fexpr;
  int nargs;
  int i;
  AclResult aclresult;
  FmgrInfo flinfo;
  CallContext *callcontext;
  EState *estate;
  ExprContext *econtext;
  HeapTuple tp;
  PgStat_FunctionCallUsage fcusage;
  Datum retval;

  fexpr = stmt->funcexpr;
  Assert(fexpr);
  Assert(IsA(fexpr, FuncExpr));

  aclresult = pg_proc_aclcheck(fexpr->funcid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_PROCEDURE, get_func_name(fexpr->funcid));
  }

                                                           
  callcontext = makeNode(CallContext);
  callcontext->atomic = atomic;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(fexpr->funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", fexpr->funcid);
  }

     
                                                                            
                                                                            
                                                                            
                                                     
     
  if (!heap_attisnull(tp, Anum_pg_proc_proconfig, NULL))
  {
    callcontext->atomic = true;
  }

     
                                                                          
                                                                          
                                                                        
                                                 
     
  if (((Form_pg_proc)GETSTRUCT(tp))->prosecdef)
  {
    callcontext->atomic = true;
  }

     
                                                                           
                                                                           
                                                                         
                                                           
     
  {
    FuncExpr *nexpr = makeNode(FuncExpr);

    memcpy(nexpr, fexpr, sizeof(FuncExpr));
    fexpr = nexpr;
  }

  fexpr->args = expand_function_arguments(fexpr->args, fexpr->funcresulttype, tp);
  nargs = list_length(fexpr->args);

  ReleaseSysCache(tp);

                                        
  if (nargs > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("cannot pass more than %d argument to a procedure", "cannot pass more than %d arguments to a procedure", FUNC_MAX_ARGS, FUNC_MAX_ARGS)));
  }

                                          
  InvokeFunctionExecuteHook(fexpr->funcid);
  fmgr_info(fexpr->funcid, &flinfo);
  fmgr_info_set_expr((Node *)fexpr, &flinfo);
  InitFunctionCallInfoData(*fcinfo, &flinfo, nargs, fexpr->inputcollid, (Node *)callcontext, NULL);

     
                                                                             
                                                            
     
  estate = CreateExecutorState();
  estate->es_param_list_info = params;
  econtext = CreateExprContext(estate);

     
                                                                            
                                                                            
                                                                     
                                                                       
                                                                            
     
  if (!atomic)
  {
    PushActiveSnapshot(GetTransactionSnapshot());
  }

  i = 0;
  foreach (lc, fexpr->args)
  {
    ExprState *exprstate;
    Datum val;
    bool isnull;

    exprstate = ExecPrepareExpr(lfirst(lc), estate);

    val = ExecEvalExprSwitchContext(exprstate, econtext, &isnull);

    fcinfo->args[i].value = val;
    fcinfo->args[i].isnull = isnull;

    i++;
  }

                                                                   
  if (!atomic)
  {
    PopActiveSnapshot();
  }

                                           
  pgstat_init_function_usage(fcinfo, &fcusage);
  retval = FunctionCallInvoke(fcinfo);
  pgstat_end_function_usage(&fcusage, true);

                                      
  if (fexpr->funcresulttype == VOIDOID)
  {
                    
  }
  else if (fexpr->funcresulttype == RECORDOID)
  {
                              
    HeapTupleHeader td;
    Oid tupType;
    int32 tupTypmod;
    TupleDesc retdesc;
    HeapTupleData rettupdata;
    TupOutputState *tstate;
    TupleTableSlot *slot;

    if (fcinfo->isnull)
    {
      elog(ERROR, "procedure returned null record");
    }

       
                                                                      
                                                                      
                                                                          
                                                                       
                                                                           
                                                                        
                                                        
       
                                                                         
                         
       
    EnsurePortalSnapshotExists();

    td = DatumGetHeapTupleHeader(retval);
    tupType = HeapTupleHeaderGetTypeId(td);
    tupTypmod = HeapTupleHeaderGetTypMod(td);
    retdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

    tstate = begin_tup_output_tupdesc(dest, retdesc, &TTSOpsHeapTuple);

    rettupdata.t_len = HeapTupleHeaderGetDatumLength(td);
    ItemPointerSetInvalid(&(rettupdata.t_self));
    rettupdata.t_tableOid = InvalidOid;
    rettupdata.t_data = td;

    slot = ExecStoreHeapTuple(&rettupdata, tstate->slot, false);
    tstate->dest->receiveSlot(slot, tstate->dest);

    end_tup_output(tstate);

    ReleaseTupleDesc(retdesc);
  }
  else
  {
    elog(ERROR, "unexpected result type for procedure: %u", fexpr->funcresulttype);
  }

  FreeExecutorState(estate);
}

   
                                                              
   
TupleDesc
CallStmtResultDesc(CallStmt *stmt)
{
  FuncExpr *fexpr;
  HeapTuple tuple;
  TupleDesc tupdesc;

  fexpr = stmt->funcexpr;

  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(fexpr->funcid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for procedure %u", fexpr->funcid);
  }

  tupdesc = build_function_result_tupdesc_t(tuple);

  ReleaseSysCache(tuple);

  return tupdesc;
}
