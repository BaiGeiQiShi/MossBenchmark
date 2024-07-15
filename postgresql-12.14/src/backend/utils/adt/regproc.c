                                                                            
   
             
                                                                       
   
                                                                        
                                                                       
                                    
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_class.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "parser/parse_type.h"
#include "parser/scansup.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/acl.h"
#include "utils/regproc.h"
#include "utils/varlena.h"

static char *
format_operator_internal(Oid operator_oid, bool force_qualify);
static char *
format_procedure_internal(Oid procedure_oid, bool force_qualify);
static void
parseNameAndArgTypes(const char *string, bool allowNone, List **names, int *nargs, Oid *argtypes);

                                                                               
                                      
                                                                               

   
                                               
   
                                                                       
   
                                                                      
                                    
   
Datum
regprocin(PG_FUNCTION_ARGS)
{
  char *pro_name_or_oid = PG_GETARG_CSTRING(0);
  RegProcedure result = InvalidOid;
  List *names;
  FuncCandidateList clist;

             
  if (strcmp(pro_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (pro_name_or_oid[0] >= '0' && pro_name_or_oid[0] <= '9' && strspn(pro_name_or_oid, "0123456789") == strlen(pro_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(pro_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                   

     
                                                                          
                                      
     
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regproc values must be OIDs in bootstrap mode");
  }

     
                                                                           
                                                 
     
  names = stringToQualifiedNameList(pro_name_or_oid);
  clist = FuncnameGetCandidates(names, -1, NIL, false, false, false);

  if (clist == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function \"%s\" does not exist", pro_name_or_oid)));
  }
  else if (clist->next != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("more than one function named \"%s\"", pro_name_or_oid)));
  }

  result = clist->oid;

  PG_RETURN_OID(result);
}

   
                                               
   
                                             
   
Datum
to_regproc(PG_FUNCTION_ARGS)
{
  char *pro_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  List *names;
  FuncCandidateList clist;

     
                                                                      
                                         
     
  names = stringToQualifiedNameList(pro_name);
  clist = FuncnameGetCandidates(names, -1, NIL, false, false, true);

  if (clist == NULL || clist->next != NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_OID(clist->oid);
}

   
                                                 
   
Datum
regprocout(PG_FUNCTION_ARGS)
{
  RegProcedure proid = PG_GETARG_OID(0);
  char *result;
  HeapTuple proctup;

  if (proid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(proid));

  if (HeapTupleIsValid(proctup))
  {
    Form_pg_proc procform = (Form_pg_proc)GETSTRUCT(proctup);
    char *proname = NameStr(procform->proname);

       
                                                                         
                                                                      
                
       
    if (IsBootstrapProcessingMode())
    {
      result = pstrdup(proname);
    }
    else
    {
      char *nspname;
      FuncCandidateList clist;

         
                                                                    
                     
         
      clist = FuncnameGetCandidates(list_make1(makeString(proname)), -1, NIL, false, false, false);
      if (clist != NULL && clist->next == NULL && clist->oid == proid)
      {
        nspname = NULL;
      }
      else
      {
        nspname = get_namespace_name(procform->pronamespace);
      }

      result = quote_qualified_identifier(nspname, proname);
    }

    ReleaseSysCache(proctup);
  }
  else
  {
                                                                       
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", proid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                               
   
Datum
regprocrecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                      
   
Datum
regprocsend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                          
   
                                                                       
   
                                                                      
                                    
   
Datum
regprocedurein(PG_FUNCTION_ARGS)
{
  char *pro_name_or_oid = PG_GETARG_CSTRING(0);
  RegProcedure result = InvalidOid;
  List *names;
  int nargs;
  Oid argtypes[FUNC_MAX_ARGS];
  FuncCandidateList clist;

             
  if (strcmp(pro_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (pro_name_or_oid[0] >= '0' && pro_name_or_oid[0] <= '9' && strspn(pro_name_or_oid, "0123456789") == strlen(pro_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(pro_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regprocedure values must be OIDs in bootstrap mode");
  }

     
                                                                            
                                                                             
                                                                             
                           
     
  parseNameAndArgTypes(pro_name_or_oid, false, &names, &nargs, argtypes);

  clist = FuncnameGetCandidates(names, nargs, NIL, false, false, false);

  for (; clist; clist = clist->next)
  {
    if (memcmp(clist->args, argtypes, nargs * sizeof(Oid)) == 0)
    {
      break;
    }
  }

  if (clist == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function \"%s\" does not exist", pro_name_or_oid)));
  }

  result = clist->oid;

  PG_RETURN_OID(result);
}

   
                                                          
   
                                             
   
Datum
to_regprocedure(PG_FUNCTION_ARGS)
{
  char *pro_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  List *names;
  int nargs;
  Oid argtypes[FUNC_MAX_ARGS];
  FuncCandidateList clist;

     
                                                                            
                                                                          
                                                                       
     
  parseNameAndArgTypes(pro_name, false, &names, &nargs, argtypes);

  clist = FuncnameGetCandidates(names, nargs, NIL, false, false, true);

  for (; clist; clist = clist->next)
  {
    if (memcmp(clist->args, argtypes, nargs * sizeof(Oid)) == 0)
    {
      PG_RETURN_OID(clist->oid);
    }
  }

  PG_RETURN_NULL();
}

   
                                                             
   
                                                                    
                                                               
   
char *
format_procedure(Oid procedure_oid)
{
  return format_procedure_internal(procedure_oid, false);
}

char *
format_procedure_qualified(Oid procedure_oid)
{
  return format_procedure_internal(procedure_oid, true);
}

   
                                                                      
   
                                                                             
                                                                               
                                             
   
static char *
format_procedure_internal(Oid procedure_oid, bool force_qualify)
{
  char *result;
  HeapTuple proctup;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(procedure_oid));

  if (HeapTupleIsValid(proctup))
  {
    Form_pg_proc procform = (Form_pg_proc)GETSTRUCT(proctup);
    char *proname = NameStr(procform->proname);
    int nargs = procform->pronargs;
    int i;
    char *nspname;
    StringInfoData buf;

                                                
    Assert(!IsBootstrapProcessingMode());

    initStringInfo(&buf);

       
                                                                          
                                                                
       
    if (!force_qualify && FunctionIsVisible(procedure_oid))
    {
      nspname = NULL;
    }
    else
    {
      nspname = get_namespace_name(procform->pronamespace);
    }

    appendStringInfo(&buf, "%s(", quote_qualified_identifier(nspname, proname));
    for (i = 0; i < nargs; i++)
    {
      Oid thisargtype = procform->proargtypes.values[i];

      if (i > 0)
      {
        appendStringInfoChar(&buf, ',');
      }
      appendStringInfoString(&buf, force_qualify ? format_type_be_qualified(thisargtype) : format_type_be(thisargtype));
    }
    appendStringInfoChar(&buf, ')');

    result = buf.data;

    ReleaseSysCache(proctup);
  }
  else
  {
                                                                       
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", procedure_oid);
  }

  return result;
}

   
                                                                       
                                                        
   
                                                
   
void
format_procedure_parts(Oid procedure_oid, List **objnames, List **objargs)
{
  HeapTuple proctup;
  Form_pg_proc procform;
  int nargs;
  int i;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(procedure_oid));

  if (!HeapTupleIsValid(proctup))
  {
    elog(ERROR, "cache lookup failed for procedure with OID %u", procedure_oid);
  }

  procform = (Form_pg_proc)GETSTRUCT(proctup);
  nargs = procform->pronargs;

  *objnames = list_make2(get_namespace_name_or_temp(procform->pronamespace), pstrdup(NameStr(procform->proname)));
  *objargs = NIL;
  for (i = 0; i < nargs; i++)
  {
    Oid thisargtype = procform->proargtypes.values[i];

    *objargs = lappend(*objargs, format_type_be_qualified(thisargtype));
  }

  ReleaseSysCache(proctup);
}

   
                                                            
   
Datum
regprocedureout(PG_FUNCTION_ARGS)
{
  RegProcedure proid = PG_GETARG_OID(0);
  char *result;

  if (proid == InvalidOid)
  {
    result = pstrdup("-");
  }
  else
  {
    result = format_procedure(proid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                                         
   
Datum
regprocedurerecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                                
   
Datum
regproceduresend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                   
   
                                                                       
   
                                                                      
                                        
   
Datum
regoperin(PG_FUNCTION_ARGS)
{
  char *opr_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result = InvalidOid;
  List *names;
  FuncCandidateList clist;

             
  if (strcmp(opr_name_or_oid, "0") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (opr_name_or_oid[0] >= '0' && opr_name_or_oid[0] <= '9' && strspn(opr_name_or_oid, "0123456789") == strlen(opr_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(opr_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                   

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regoper values must be OIDs in bootstrap mode");
  }

     
                                                                           
                                                     
     
  names = stringToQualifiedNameList(opr_name_or_oid);
  clist = OpernameGetCandidates(names, '\0', false);

  if (clist == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator does not exist: %s", opr_name_or_oid)));
  }
  else if (clist->next != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("more than one operator named %s", opr_name_or_oid)));
  }

  result = clist->oid;

  PG_RETURN_OID(result);
}

   
                                                    
   
                                             
   
Datum
to_regoper(PG_FUNCTION_ARGS)
{
  char *opr_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  List *names;
  FuncCandidateList clist;

     
                                                                          
                                         
     
  names = stringToQualifiedNameList(opr_name);
  clist = OpernameGetCandidates(names, '\0', true);

  if (clist == NULL || clist->next != NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_OID(clist->oid);
}

   
                                                     
   
Datum
regoperout(PG_FUNCTION_ARGS)
{
  Oid oprid = PG_GETARG_OID(0);
  char *result;
  HeapTuple opertup;

  if (oprid == InvalidOid)
  {
    result = pstrdup("0");
    PG_RETURN_CSTRING(result);
  }

  opertup = SearchSysCache1(OPEROID, ObjectIdGetDatum(oprid));

  if (HeapTupleIsValid(opertup))
  {
    Form_pg_operator operform = (Form_pg_operator)GETSTRUCT(opertup);
    char *oprname = NameStr(operform->oprname);

       
                                                                         
                                                                      
                
       
    if (IsBootstrapProcessingMode())
    {
      result = pstrdup(oprname);
    }
    else
    {
      FuncCandidateList clist;

         
                                                                    
                     
         
      clist = OpernameGetCandidates(list_make1(makeString(oprname)), '\0', false);
      if (clist != NULL && clist->next == NULL && clist->oid == oprid)
      {
        result = pstrdup(oprname);
      }
      else
      {
        const char *nspname;

        nspname = get_namespace_name(operform->oprnamespace);
        nspname = quote_identifier(nspname);
        result = (char *)palloc(strlen(nspname) + strlen(oprname) + 2);
        sprintf(result, "%s.%s", nspname, oprname);
      }
    }

    ReleaseSysCache(opertup);
  }
  else
  {
       
                                                                         
       
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", oprid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                               
   
Datum
regoperrecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                      
   
Datum
regopersend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                             
   
                                                                       
   
                                                                      
                                        
   
Datum
regoperatorin(PG_FUNCTION_ARGS)
{
  char *opr_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result;
  List *names;
  int nargs;
  Oid argtypes[FUNC_MAX_ARGS];

             
  if (strcmp(opr_name_or_oid, "0") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (opr_name_or_oid[0] >= '0' && opr_name_or_oid[0] <= '9' && strspn(opr_name_or_oid, "0123456789") == strlen(opr_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(opr_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regoperator values must be OIDs in bootstrap mode");
  }

     
                                                                            
                                                                             
                                                                             
                           
     
  parseNameAndArgTypes(opr_name_or_oid, true, &names, &nargs, argtypes);
  if (nargs == 1)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("missing argument"), errhint("Use NONE to denote the missing argument of a unary operator.")));
  }
  if (nargs != 2)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg("too many arguments"), errhint("Provide two argument types for operator.")));
  }

  result = OpernameGetOprid(names, argtypes[0], argtypes[1]);

  if (!OidIsValid(result))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator does not exist: %s", opr_name_or_oid)));
  }

  PG_RETURN_OID(result);
}

   
                                                             
   
                                             
   
Datum
to_regoperator(PG_FUNCTION_ARGS)
{
  char *opr_name_or_oid = text_to_cstring(PG_GETARG_TEXT_PP(0));
  Oid result;
  List *names;
  int nargs;
  Oid argtypes[FUNC_MAX_ARGS];

     
                                                                            
                                                                          
                                                                       
     
  parseNameAndArgTypes(opr_name_or_oid, true, &names, &nargs, argtypes);
  if (nargs == 1)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("missing argument"), errhint("Use NONE to denote the missing argument of a unary operator.")));
  }
  if (nargs != 2)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg("too many arguments"), errhint("Provide two argument types for operator.")));
  }

  result = OpernameGetOprid(names, argtypes[0], argtypes[1]);

  if (!OidIsValid(result))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_OID(result);
}

   
                                                                
   
                                                                   
                                                               
   
static char *
format_operator_internal(Oid operator_oid, bool force_qualify)
{
  char *result;
  HeapTuple opertup;

  opertup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operator_oid));

  if (HeapTupleIsValid(opertup))
  {
    Form_pg_operator operform = (Form_pg_operator)GETSTRUCT(opertup);
    char *oprname = NameStr(operform->oprname);
    char *nspname;
    StringInfoData buf;

                                                
    Assert(!IsBootstrapProcessingMode());

    initStringInfo(&buf);

       
                                                                         
                                                                           
       
    if (force_qualify || !OperatorIsVisible(operator_oid))
    {
      nspname = get_namespace_name(operform->oprnamespace);
      appendStringInfo(&buf, "%s.", quote_identifier(nspname));
    }

    appendStringInfo(&buf, "%s(", oprname);

    if (operform->oprleft)
    {
      appendStringInfo(&buf, "%s,", force_qualify ? format_type_be_qualified(operform->oprleft) : format_type_be(operform->oprleft));
    }
    else
    {
      appendStringInfoString(&buf, "NONE,");
    }

    if (operform->oprright)
    {
      appendStringInfo(&buf, "%s)", force_qualify ? format_type_be_qualified(operform->oprright) : format_type_be(operform->oprright));
    }
    else
    {
      appendStringInfoString(&buf, "NONE)");
    }

    result = buf.data;

    ReleaseSysCache(opertup);
  }
  else
  {
       
                                                                         
       
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", operator_oid);
  }

  return result;
}

char *
format_operator(Oid operator_oid)
{
  return format_operator_internal(operator_oid, false);
}

char *
format_operator_qualified(Oid operator_oid)
{
  return format_operator_internal(operator_oid, true);
}

void
format_operator_parts(Oid operator_oid, List **objnames, List **objargs)
{
  HeapTuple opertup;
  Form_pg_operator oprForm;

  opertup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operator_oid));
  if (!HeapTupleIsValid(opertup))
  {
    elog(ERROR, "cache lookup failed for operator with OID %u", operator_oid);
  }

  oprForm = (Form_pg_operator)GETSTRUCT(opertup);
  *objnames = list_make2(get_namespace_name_or_temp(oprForm->oprnamespace), pstrdup(NameStr(oprForm->oprname)));
  *objargs = NIL;
  if (oprForm->oprleft)
  {
    *objargs = lappend(*objargs, format_type_be_qualified(oprForm->oprleft));
  }
  if (oprForm->oprright)
  {
    *objargs = lappend(*objargs, format_type_be_qualified(oprForm->oprright));
  }

  ReleaseSysCache(opertup);
}

   
                                                               
   
Datum
regoperatorout(PG_FUNCTION_ARGS)
{
  Oid oprid = PG_GETARG_OID(0);
  char *result;

  if (oprid == InvalidOid)
  {
    result = pstrdup("0");
  }
  else
  {
    result = format_operator(oprid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                                       
   
Datum
regoperatorrecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                              
   
Datum
regoperatorsend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                   
   
                                                                       
   
                                                                      
                                     
   
Datum
regclassin(PG_FUNCTION_ARGS)
{
  char *class_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result = InvalidOid;
  List *names;

             
  if (strcmp(class_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (class_name_or_oid[0] >= '0' && class_name_or_oid[0] <= '9' && strspn(class_name_or_oid, "0123456789") == strlen(class_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(class_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                   

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regclass values must be OIDs in bootstrap mode");
  }

     
                                                                           
                                                  
     
  names = stringToQualifiedNameList(class_name_or_oid);

                                                                           
  result = RangeVarGetRelid(makeRangeVarFromNameList(names), NoLock, false);

  PG_RETURN_OID(result);
}

   
                                                    
   
                                             
   
Datum
to_regclass(PG_FUNCTION_ARGS)
{
  char *class_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  Oid result;
  List *names;

     
                                                                       
                                         
     
  names = stringToQualifiedNameList(class_name);

                                                                           
  result = RangeVarGetRelid(makeRangeVarFromNameList(names), NoLock, true);

  if (OidIsValid(result))
  {
    PG_RETURN_OID(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                     
   
Datum
regclassout(PG_FUNCTION_ARGS)
{
  Oid classid = PG_GETARG_OID(0);
  char *result;
  HeapTuple classtup;

  if (classid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  classtup = SearchSysCache1(RELOID, ObjectIdGetDatum(classid));

  if (HeapTupleIsValid(classtup))
  {
    Form_pg_class classform = (Form_pg_class)GETSTRUCT(classtup);
    char *classname = NameStr(classform->relname);

       
                                                                         
                                                                       
                
       
    if (IsBootstrapProcessingMode())
    {
      result = pstrdup(classname);
    }
    else
    {
      char *nspname;

         
                                                                      
         
      if (RelationIsVisible(classid))
      {
        nspname = NULL;
      }
      else
      {
        nspname = get_namespace_name(classform->relnamespace);
      }

      result = quote_qualified_identifier(nspname, classname);
    }

    ReleaseSysCache(classtup);
  }
  else
  {
                                                                        
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", classid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                                 
   
Datum
regclassrecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                        
   
Datum
regclasssend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                
   
                                                                           
                                                                            
                                                                     
                                      
   
                                                                       
                                           
   
                                                                      
                                    
   
Datum
regtypein(PG_FUNCTION_ARGS)
{
  char *typ_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result = InvalidOid;
  int32 typmod;

             
  if (strcmp(typ_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (typ_name_or_oid[0] >= '0' && typ_name_or_oid[0] <= '9' && strspn(typ_name_or_oid, "0123456789") == strlen(typ_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(typ_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                                     

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regtype values must be OIDs in bootstrap mode");
  }

     
                                                                            
                   
     
  parseTypeString(typ_name_or_oid, &result, &typmod, false);

  PG_RETURN_OID(result);
}

   
                                                 
   
                                             
   
Datum
to_regtype(PG_FUNCTION_ARGS)
{
  char *typ_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  Oid result;
  int32 typmod;

     
                                                                             
     
  parseTypeString(typ_name, &result, &typmod, true);

  if (OidIsValid(result))
  {
    PG_RETURN_OID(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                 
   
Datum
regtypeout(PG_FUNCTION_ARGS)
{
  Oid typid = PG_GETARG_OID(0);
  char *result;
  HeapTuple typetup;

  if (typid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  typetup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));

  if (HeapTupleIsValid(typetup))
  {
    Form_pg_type typeform = (Form_pg_type)GETSTRUCT(typetup);

       
                                                                         
                                                                      
                
       
    if (IsBootstrapProcessingMode())
    {
      char *typname = NameStr(typeform->typname);

      result = pstrdup(typname);
    }
    else
    {
      result = format_type_be(typid);
    }

    ReleaseSysCache(typetup);
  }
  else
  {
                                                                       
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", typid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                               
   
Datum
regtyperecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                      
   
Datum
regtypesend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                          
   
                                                                       
   
                                                                      
                                         
   
Datum
regconfigin(PG_FUNCTION_ARGS)
{
  char *cfg_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result;
  List *names;

             
  if (strcmp(cfg_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (cfg_name_or_oid[0] >= '0' && cfg_name_or_oid[0] <= '9' && strspn(cfg_name_or_oid, "0123456789") == strlen(cfg_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(cfg_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regconfig values must be OIDs in bootstrap mode");
  }

     
                                                                           
                                                      
     
  names = stringToQualifiedNameList(cfg_name_or_oid);

  result = get_ts_config_oid(names, false);

  PG_RETURN_OID(result);
}

   
                                                           
   
Datum
regconfigout(PG_FUNCTION_ARGS)
{
  Oid cfgid = PG_GETARG_OID(0);
  char *result;
  HeapTuple cfgtup;

  if (cfgid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  cfgtup = SearchSysCache1(TSCONFIGOID, ObjectIdGetDatum(cfgid));

  if (HeapTupleIsValid(cfgtup))
  {
    Form_pg_ts_config cfgform = (Form_pg_ts_config)GETSTRUCT(cfgtup);
    char *cfgname = NameStr(cfgform->cfgname);
    char *nspname;

       
                                                                      
       
    if (TSConfigIsVisible(cfgid))
    {
      nspname = NULL;
    }
    else
    {
      nspname = get_namespace_name(cfgform->cfgnamespace);
    }

    result = quote_qualified_identifier(nspname, cfgname);

    ReleaseSysCache(cfgtup);
  }
  else
  {
                                                                          
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", cfgid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                                   
   
Datum
regconfigrecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                          
   
Datum
regconfigsend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                                      
   
                                                                       
   
                                                                      
                                       
   
Datum
regdictionaryin(PG_FUNCTION_ARGS)
{
  char *dict_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result;
  List *names;

             
  if (strcmp(dict_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (dict_name_or_oid[0] >= '0' && dict_name_or_oid[0] <= '9' && strspn(dict_name_or_oid, "0123456789") == strlen(dict_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(dict_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regdictionary values must be OIDs in bootstrap mode");
  }

     
                                                                           
                                                    
     
  names = stringToQualifiedNameList(dict_name_or_oid);

  result = get_ts_dict_oid(names, false);

  PG_RETURN_OID(result);
}

   
                                                                       
   
Datum
regdictionaryout(PG_FUNCTION_ARGS)
{
  Oid dictid = PG_GETARG_OID(0);
  char *result;
  HeapTuple dicttup;

  if (dictid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  dicttup = SearchSysCache1(TSDICTOID, ObjectIdGetDatum(dictid));

  if (HeapTupleIsValid(dicttup))
  {
    Form_pg_ts_dict dictform = (Form_pg_ts_dict)GETSTRUCT(dicttup);
    char *dictname = NameStr(dictform->dictname);
    char *nspname;

       
                                                                          
           
       
    if (TSDictionaryIsVisible(dictid))
    {
      nspname = NULL;
    }
    else
    {
      nspname = get_namespace_name(dictform->dictnamespace);
    }

    result = quote_qualified_identifier(nspname, dictname);

    ReleaseSysCache(dicttup);
  }
  else
  {
                                                                        
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", dictid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                                         
   
Datum
regdictionaryrecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                                
   
Datum
regdictionarysend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                               
   
                                                                       
   
                                                                      
                                      
   
Datum
regrolein(PG_FUNCTION_ARGS)
{
  char *role_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result;
  List *names;

             
  if (strcmp(role_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (role_name_or_oid[0] >= '0' && role_name_or_oid[0] <= '9' && strspn(role_name_or_oid, "0123456789") == strlen(role_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(role_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regrole values must be OIDs in bootstrap mode");
  }

                                                                 
  names = stringToQualifiedNameList(role_name_or_oid);

  if (list_length(names) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  result = get_role_oid(strVal(linitial(names)), false);

  PG_RETURN_OID(result);
}

   
                                                 
   
                                             
   
Datum
to_regrole(PG_FUNCTION_ARGS)
{
  char *role_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  Oid result;
  List *names;

  names = stringToQualifiedNameList(role_name);

  if (list_length(names) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  result = get_role_oid(strVal(linitial(names)), true);

  if (OidIsValid(result))
  {
    PG_RETURN_OID(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                  
   
Datum
regroleout(PG_FUNCTION_ARGS)
{
  Oid roleoid = PG_GETARG_OID(0);
  char *result;

  if (roleoid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  result = GetUserNameFromId(roleoid, true);

  if (result)
  {
                                                                           
    result = pstrdup(quote_identifier(result));
  }
  else
  {
                                                              
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", roleoid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                             
   
Datum
regrolerecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                    
   
Datum
regrolesend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                                         
   
                                                                       
   
                                                                      
                                         
   
Datum
regnamespacein(PG_FUNCTION_ARGS)
{
  char *nsp_name_or_oid = PG_GETARG_CSTRING(0);
  Oid result;
  List *names;

             
  if (strcmp(nsp_name_or_oid, "-") == 0)
  {
    PG_RETURN_OID(InvalidOid);
  }

                    
  if (nsp_name_or_oid[0] >= '0' && nsp_name_or_oid[0] <= '9' && strspn(nsp_name_or_oid, "0123456789") == strlen(nsp_name_or_oid))
  {
    result = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(nsp_name_or_oid)));
    PG_RETURN_OID(result);
  }

                                                        
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "regnamespace values must be OIDs in bootstrap mode");
  }

                                                                    
  names = stringToQualifiedNameList(nsp_name_or_oid);

  if (list_length(names) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  result = get_namespace_oid(strVal(linitial(names)), false);

  PG_RETURN_OID(result);
}

   
                                                          
   
                                             
   
Datum
to_regnamespace(PG_FUNCTION_ARGS)
{
  char *nsp_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  Oid result;
  List *names;

  names = stringToQualifiedNameList(nsp_name);

  if (list_length(names) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  result = get_namespace_oid(strVal(linitial(names)), true);

  if (OidIsValid(result))
  {
    PG_RETURN_OID(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                           
   
Datum
regnamespaceout(PG_FUNCTION_ARGS)
{
  Oid nspid = PG_GETARG_OID(0);
  char *result;

  if (nspid == InvalidOid)
  {
    result = pstrdup("-");
    PG_RETURN_CSTRING(result);
  }

  result = get_namespace_name(nspid);

  if (result)
  {
                                                                           
    result = pstrdup(quote_identifier(result));
  }
  else
  {
                                                                   
    result = (char *)palloc(NAMEDATALEN);
    snprintf(result, NAMEDATALEN, "%u", nspid);
  }

  PG_RETURN_CSTRING(result);
}

   
                                                                       
   
Datum
regnamespacerecv(PG_FUNCTION_ARGS)
{
                                                  
  return oidrecv(fcinfo);
}

   
                                                               
   
Datum
regnamespacesend(PG_FUNCTION_ARGS)
{
                                                  
  return oidsend(fcinfo);
}

   
                                           
   
                                                                       
                                                                             
                          
   
Datum
text_regclass(PG_FUNCTION_ARGS)
{
  text *relname = PG_GETARG_TEXT_PP(0);
  Oid result;
  RangeVar *rv;

  rv = makeRangeVarFromNameList(textToQualifiedNameList(relname));

                                                                           
  result = RangeVarGetRelid(rv, NoLock, false);

  PG_RETURN_OID(result);
}

   
                                                          
   
List *
stringToQualifiedNameList(const char *string)
{
  char *rawname;
  List *result = NIL;
  List *namelist;
  ListCell *l;

                                                      
  rawname = pstrdup(string);

  if (!SplitIdentifierString(rawname, '.', &namelist))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  if (namelist == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  foreach (l, namelist)
  {
    char *curname = (char *)lfirst(l);

    result = lappend(result, makeString(pstrdup(curname)));
  }

  pfree(rawname);
  list_free(namelist);

  return result;
}

                                                                               
                                     
                                                                               

   
                                                                         
                                                               
                                                                       
                                                                         
                                                             
   
                                                                            
                         
   
static void
parseNameAndArgTypes(const char *string, bool allowNone, List **names, int *nargs, Oid *argtypes)
{
  char *rawname;
  char *ptr;
  char *ptr2;
  char *typename;
  bool in_quote;
  bool had_comma;
  int paren_count;
  Oid typeid;
  int32 typmod;

                                                      
  rawname = pstrdup(string);

                                                               
  in_quote = false;
  for (ptr = rawname; *ptr; ptr++)
  {
    if (*ptr == '"')
    {
      in_quote = !in_quote;
    }
    else if (*ptr == '(' && !in_quote)
    {
      break;
    }
  }
  if (*ptr == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected a left parenthesis")));
  }

                                                  
  *ptr++ = '\0';
  *names = stringToQualifiedNameList(rawname);

                                                              
  ptr2 = ptr + strlen(ptr);
  while (--ptr2 > ptr)
  {
    if (!scanner_isspace(*ptr2))
    {
      break;
    }
  }
  if (*ptr2 != ')')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected a right parenthesis")));
  }

  *ptr2 = '\0';

                                                                     
  *nargs = 0;
  had_comma = false;

  for (;;)
  {
                                  
    while (scanner_isspace(*ptr))
    {
      ptr++;
    }
    if (*ptr == '\0')
    {
                                                              
      if (had_comma)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected a type name")));
      }
      break;
    }
    typename = ptr;
                                                          
                                                     
    in_quote = false;
    paren_count = 0;
    for (; *ptr; ptr++)
    {
      if (*ptr == '"')
      {
        in_quote = !in_quote;
      }
      else if (*ptr == ',' && !in_quote && paren_count == 0)
      {
        break;
      }
      else if (!in_quote)
      {
        switch (*ptr)
        {
        case '(':
        case '[':
          paren_count++;
          break;
        case ')':
        case ']':
          paren_count--;
          break;
        }
      }
    }
    if (in_quote || paren_count != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("improper type name")));
    }

    ptr2 = ptr;
    if (*ptr == ',')
    {
      had_comma = true;
      *ptr++ = '\0';
    }
    else
    {
      had_comma = false;
      Assert(*ptr == '\0');
    }
                                     
    while (--ptr2 >= typename)
    {
      if (!scanner_isspace(*ptr2))
      {
        break;
      }
      *ptr2 = '\0';
    }

    if (allowNone && pg_strcasecmp(typename, "none") == 0)
    {
                                 
      typeid = InvalidOid;
      typmod = -1;
    }
    else
    {
                                                    
      parseTypeString(typename, &typeid, &typmod, false);
    }
    if (*nargs >= FUNC_MAX_ARGS)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg("too many arguments")));
    }

    argtypes[*nargs] = typeid;
    (*nargs)++;
  }

  pfree(rawname);
}
