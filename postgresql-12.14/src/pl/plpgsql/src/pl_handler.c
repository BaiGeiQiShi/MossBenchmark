                                                                            
   
                                            
                           
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

#include "plpgsql.h"

static bool
plpgsql_extra_checks_check_hook(char **newvalue, void **extra, GucSource source);
static void
plpgsql_extra_warnings_assign_hook(const char *newvalue, void *extra);
static void
plpgsql_extra_errors_assign_hook(const char *newvalue, void *extra);

PG_MODULE_MAGIC;

                         
static const struct config_enum_entry variable_conflict_options[] = {{"error", PLPGSQL_RESOLVE_ERROR, false}, {"use_variable", PLPGSQL_RESOLVE_VARIABLE, false}, {"use_column", PLPGSQL_RESOLVE_COLUMN, false}, {NULL, 0, false}};

int plpgsql_variable_conflict = PLPGSQL_RESOLVE_ERROR;

bool plpgsql_print_strict_params = false;

bool plpgsql_check_asserts = true;

char *plpgsql_extra_warnings_string = NULL;
char *plpgsql_extra_errors_string = NULL;
int plpgsql_extra_warnings;
int plpgsql_extra_errors;

                      
PLpgSQL_plugin **plpgsql_plugin_ptr = NULL;

static bool
plpgsql_extra_checks_check_hook(char **newvalue, void **extra, GucSource source)
{
  char *rawstring;
  List *elemlist;
  ListCell *l;
  int extrachecks = 0;
  int *myextra;

  if (pg_strcasecmp(*newvalue, "all") == 0)
  {
    extrachecks = PLPGSQL_XCHECK_ALL;
  }
  else if (pg_strcasecmp(*newvalue, "none") == 0)
  {
    extrachecks = PLPGSQL_XCHECK_NONE;
  }
  else
  {
                                          
    rawstring = pstrdup(*newvalue);

                                               
    if (!SplitIdentifierString(rawstring, ',', &elemlist))
    {
                                
      GUC_check_errdetail("List syntax is invalid.");
      pfree(rawstring);
      list_free(elemlist);
      return false;
    }

    foreach (l, elemlist)
    {
      char *tok = (char *)lfirst(l);

      if (pg_strcasecmp(tok, "shadowed_variables") == 0)
      {
        extrachecks |= PLPGSQL_XCHECK_SHADOWVAR;
      }
      else if (pg_strcasecmp(tok, "too_many_rows") == 0)
      {
        extrachecks |= PLPGSQL_XCHECK_TOOMANYROWS;
      }
      else if (pg_strcasecmp(tok, "strict_multi_assignment") == 0)
      {
        extrachecks |= PLPGSQL_XCHECK_STRICTMULTIASSIGNMENT;
      }
      else if (pg_strcasecmp(tok, "all") == 0 || pg_strcasecmp(tok, "none") == 0)
      {
        GUC_check_errdetail("Key word \"%s\" cannot be combined with other key words.", tok);
        pfree(rawstring);
        list_free(elemlist);
        return false;
      }
      else
      {
        GUC_check_errdetail("Unrecognized key word: \"%s\".", tok);
        pfree(rawstring);
        list_free(elemlist);
        return false;
      }
    }

    pfree(rawstring);
    list_free(elemlist);
  }

  myextra = (int *)malloc(sizeof(int));
  if (!myextra)
  {
    return false;
  }
  *myextra = extrachecks;
  *extra = (void *)myextra;

  return true;
}

static void
plpgsql_extra_warnings_assign_hook(const char *newvalue, void *extra)
{
  plpgsql_extra_warnings = *((int *)extra);
}

static void
plpgsql_extra_errors_assign_hook(const char *newvalue, void *extra)
{
  plpgsql_extra_errors = *((int *)extra);
}

   
                                                   
   
                                                
   
void
_PG_init(void)
{
                                                                        
  static bool inited = false;

  if (inited)
  {
    return;
  }

  pg_bindtextdomain(TEXTDOMAIN);

  DefineCustomEnumVariable("plpgsql.variable_conflict", gettext_noop("Sets handling of conflicts between PL/pgSQL variable names and table column names."), NULL, &plpgsql_variable_conflict, PLPGSQL_RESOLVE_ERROR, variable_conflict_options, PGC_SUSET, 0, NULL, NULL, NULL);

  DefineCustomBoolVariable("plpgsql.print_strict_params", gettext_noop("Print information about parameters in the DETAIL part of the error messages generated on INTO ... STRICT failures."), NULL, &plpgsql_print_strict_params, false, PGC_USERSET, 0, NULL, NULL, NULL);

  DefineCustomBoolVariable("plpgsql.check_asserts", gettext_noop("Perform checks given in ASSERT statements."), NULL, &plpgsql_check_asserts, true, PGC_USERSET, 0, NULL, NULL, NULL);

  DefineCustomStringVariable("plpgsql.extra_warnings", gettext_noop("List of programming constructs that should produce a warning."), NULL, &plpgsql_extra_warnings_string, "none", PGC_USERSET, GUC_LIST_INPUT, plpgsql_extra_checks_check_hook, plpgsql_extra_warnings_assign_hook, NULL);

  DefineCustomStringVariable("plpgsql.extra_errors", gettext_noop("List of programming constructs that should produce an error."), NULL, &plpgsql_extra_errors_string, "none", PGC_USERSET, GUC_LIST_INPUT, plpgsql_extra_checks_check_hook, plpgsql_extra_errors_assign_hook, NULL);

  EmitWarningsOnPlaceholders("plpgsql");

  plpgsql_HashTableInit();
  RegisterXactCallback(plpgsql_xact_cb, NULL);
  RegisterSubXactCallback(plpgsql_subxact_cb, NULL);

                                                                      
  plpgsql_plugin_ptr = (PLpgSQL_plugin **)find_rendezvous_variable("PLpgSQL_plugin");

  inited = true;
}

              
                        
   
                                                       
                                                            
              
   
PG_FUNCTION_INFO_V1(plpgsql_call_handler);

Datum
plpgsql_call_handler(PG_FUNCTION_ARGS)
{
  bool nonatomic;
  PLpgSQL_function *func;
  PLpgSQL_execstate *save_cur_estate;
  Datum retval;
  int rc;

  nonatomic = fcinfo->context && IsA(fcinfo->context, CallContext) && !castNode(CallContext, fcinfo->context)->atomic;

     
                            
     
  if ((rc = SPI_connect_ext(nonatomic ? SPI_OPT_NONATOMIC : 0)) != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed: %s", SPI_result_code_string(rc));
  }

                                    
  func = plpgsql_compile(fcinfo, false);

                                                       
  save_cur_estate = func->cur_estate;

                                                                       
  func->use_count++;

  PG_TRY();
  {
       
                                                                       
                  
       
    if (CALLED_AS_TRIGGER(fcinfo))
    {
      retval = PointerGetDatum(plpgsql_exec_trigger(func, (TriggerData *)fcinfo->context));
    }
    else if (CALLED_AS_EVENT_TRIGGER(fcinfo))
    {
      plpgsql_exec_event_trigger(func, (EventTriggerData *)fcinfo->context);
      retval = (Datum)0;
    }
    else
    {
      retval = plpgsql_exec_function(func, fcinfo, NULL, !nonatomic);
    }
  }
  PG_CATCH();
  {
                                                                      
    func->use_count--;
    func->cur_estate = save_cur_estate;
    PG_RE_THROW();
  }
  PG_END_TRY();

  func->use_count--;

  func->cur_estate = save_cur_estate;

     
                                 
     
  if ((rc = SPI_finish()) != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed: %s", SPI_result_code_string(rc));
  }

  return retval;
}

              
                          
   
                                                           
              
   
PG_FUNCTION_INFO_V1(plpgsql_inline_handler);

Datum
plpgsql_inline_handler(PG_FUNCTION_ARGS)
{
  LOCAL_FCINFO(fake_fcinfo, 0);
  InlineCodeBlock *codeblock = castNode(InlineCodeBlock, DatumGetPointer(PG_GETARG_DATUM(0)));
  PLpgSQL_function *func;
  FmgrInfo flinfo;
  EState *simple_eval_estate;
  Datum retval;
  int rc;

     
                            
     
  if ((rc = SPI_connect_ext(codeblock->atomic ? 0 : SPI_OPT_NONATOMIC)) != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed: %s", SPI_result_code_string(rc));
  }

                                        
  func = plpgsql_compile_inline(codeblock->source_text);

                                                 
  func->use_count++;

     
                                                           
                                                                           
                               
     
  MemSet(fake_fcinfo, 0, SizeForFunctionCallInfo(0));
  MemSet(&flinfo, 0, sizeof(flinfo));
  fake_fcinfo->flinfo = &flinfo;
  flinfo.fn_oid = InvalidOid;
  flinfo.fn_mcxt = CurrentMemoryContext;

     
                                                                           
                                                                          
                                                                          
                                                                        
                                                                          
                                
     
  simple_eval_estate = CreateExecutorState();

                            
  PG_TRY();
  {
    retval = plpgsql_exec_function(func, fake_fcinfo, simple_eval_estate, codeblock->atomic);
  }
  PG_CATCH();
  {
       
                                                                        
                                                                        
                                                                           
                                                                    
                       
       
                                                                 
                                                                          
                                                                         
                                                                         
                                                                          
                                                  
       
    plpgsql_subxact_cb(SUBXACT_EVENT_ABORT_SUB, GetCurrentSubTransactionId(), 0, NULL);

                                     
    FreeExecutorState(simple_eval_estate);

                                                              
    func->use_count--;
    Assert(func->use_count == 0);

                                               
    plpgsql_free_function_memory(func);

                                 
    PG_RE_THROW();
  }
  PG_END_TRY();

                                   
  FreeExecutorState(simple_eval_estate);

                                                            
  func->use_count--;
  Assert(func->use_count == 0);

                                             
  plpgsql_free_function_memory(func);

     
                                 
     
  if ((rc = SPI_finish()) != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed: %s", SPI_result_code_string(rc));
  }

  return retval;
}

              
                     
   
                                                             
                         
              
   
PG_FUNCTION_INFO_V1(plpgsql_validator);

Datum
plpgsql_validator(PG_FUNCTION_ARGS)
{
  Oid funcoid = PG_GETARG_OID(0);
  HeapTuple tuple;
  Form_pg_proc proc;
  char functyptype;
  int numargs;
  Oid *argtypes;
  char **argnames;
  char *argmodes;
  bool is_dml_trigger = false;
  bool is_event_trigger = false;
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

  functyptype = get_typtype(proc->prorettype);

                                  
                                                        
  if (functyptype == TYPTYPE_PSEUDO)
  {
                                                            
    if (proc->prorettype == TRIGGEROID || (proc->prorettype == OPAQUEOID && proc->pronargs == 0))
    {
      is_dml_trigger = true;
    }
    else if (proc->prorettype == EVTTRIGGEROID)
    {
      is_event_trigger = true;
    }
    else if (proc->prorettype != RECORDOID && proc->prorettype != VOIDOID && !IsPolymorphicType(proc->prorettype))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/pgSQL functions cannot return type %s", format_type_be(proc->prorettype))));
    }
  }

                                                            
                                         
  numargs = get_func_arg_info(tuple, &argtypes, &argnames, &argmodes);
  for (i = 0; i < numargs; i++)
  {
    if (get_typtype(argtypes[i]) == TYPTYPE_PSEUDO)
    {
      if (argtypes[i] != RECORDOID && !IsPolymorphicType(argtypes[i]))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/pgSQL functions cannot accept type %s", format_type_be(argtypes[i]))));
      }
    }
  }

                                                      
  if (check_function_bodies)
  {
    LOCAL_FCINFO(fake_fcinfo, 0);
    FmgrInfo flinfo;
    int rc;
    TriggerData trigdata;
    EventTriggerData etrigdata;

       
                                                                
       
    if ((rc = SPI_connect()) != SPI_OK_CONNECT)
    {
      elog(ERROR, "SPI_connect failed: %s", SPI_result_code_string(rc));
    }

       
                                                             
                          
       
    MemSet(fake_fcinfo, 0, SizeForFunctionCallInfo(0));
    MemSet(&flinfo, 0, sizeof(flinfo));
    fake_fcinfo->flinfo = &flinfo;
    flinfo.fn_oid = funcoid;
    flinfo.fn_mcxt = CurrentMemoryContext;
    if (is_dml_trigger)
    {
      MemSet(&trigdata, 0, sizeof(trigdata));
      trigdata.type = T_TriggerData;
      fake_fcinfo->context = (Node *)&trigdata;
    }
    else if (is_event_trigger)
    {
      MemSet(&etrigdata, 0, sizeof(etrigdata));
      etrigdata.type = T_EventTriggerData;
      fake_fcinfo->context = (Node *)&etrigdata;
    }

                                   
    plpgsql_compile(fake_fcinfo, true);

       
                                   
       
    if ((rc = SPI_finish()) != SPI_OK_FINISH)
    {
      elog(ERROR, "SPI_finish failed: %s", SPI_result_code_string(rc));
    }
  }

  ReleaseSysCache(tuple);

  PG_RETURN_VOID();
}
