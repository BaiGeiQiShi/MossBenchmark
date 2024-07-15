                                                                            
   
                                              
                           
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/regproc.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

#include "plpgsql.h"

              
                                      
              
   
PLpgSQL_stmt_block *plpgsql_parse_result;

static int datums_alloc;
int plpgsql_nDatums;
PLpgSQL_datum **plpgsql_Datums;
static int datums_last;

char *plpgsql_error_funcname;
bool plpgsql_DumpExecTree = false;
bool plpgsql_check_syntax = false;

PLpgSQL_function *plpgsql_curr_compile;

                                                                    
MemoryContext plpgsql_compile_tmp_cxt;

              
                                     
              
   
static HTAB *plpgsql_HashTable = NULL;

typedef struct plpgsql_hashent
{
  PLpgSQL_func_hashkey key;
  PLpgSQL_function *function;
} plpgsql_HashEnt;

#define FUNCS_PER_USER 128                         

              
                                              
              
   
typedef struct
{
  const char *label;
  int sqlerrstate;
} ExceptionLabelMap;

static const ExceptionLabelMap exception_label_map[] = {
#include "plerrcodes.h"                         
    {NULL, 0}};

              
                     
              
   
static PLpgSQL_function *
do_compile(FunctionCallInfo fcinfo, HeapTuple procTup, PLpgSQL_function *function, PLpgSQL_func_hashkey *hashkey, bool forValidator);
static void
plpgsql_compile_error_callback(void *arg);
static void
add_parameter_name(PLpgSQL_nsitem_type itemtype, int itemno, const char *name);
static void
add_dummy_return(PLpgSQL_function *function);
static Node *
plpgsql_pre_column_ref(ParseState *pstate, ColumnRef *cref);
static Node *
plpgsql_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var);
static Node *
plpgsql_param_ref(ParseState *pstate, ParamRef *pref);
static Node *
resolve_column_ref(ParseState *pstate, PLpgSQL_expr *expr, ColumnRef *cref, bool error_if_no_field);
static Node *
make_datum_param(PLpgSQL_expr *expr, int dno, int location);
static PLpgSQL_row *
build_row_from_vars(PLpgSQL_variable **vars, int numvars);
static PLpgSQL_type *
build_datatype(HeapTuple typeTup, int32 typmod, Oid collation, TypeName *origtypname);
static void
plpgsql_start_datums(void);
static void
plpgsql_finish_datums(PLpgSQL_function *function);
static void
compute_function_hashkey(FunctionCallInfo fcinfo, Form_pg_proc procStruct, PLpgSQL_func_hashkey *hashkey, bool forValidator);
static void
plpgsql_resolve_polymorphic_argtypes(int numargs, Oid *argtypes, char *argmodes, Node *call_expr, bool forValidator, const char *proname);
static PLpgSQL_function *
plpgsql_HashTableLookup(PLpgSQL_func_hashkey *func_key);
static void
plpgsql_HashTableInsert(PLpgSQL_function *function, PLpgSQL_func_hashkey *func_key);
static void
plpgsql_HashTableDelete(PLpgSQL_function *function);
static void
delete_function(PLpgSQL_function *func);

              
                                                                    
   
                                                                          
                                   
   
                                                                         
                              
              
   
PLpgSQL_function *
plpgsql_compile(FunctionCallInfo fcinfo, bool forValidator)
{
  Oid funcOid = fcinfo->flinfo->fn_oid;
  HeapTuple procTup;
  Form_pg_proc procStruct;
  PLpgSQL_function *function;
  PLpgSQL_func_hashkey hashkey;
  bool function_valid = false;
  bool hashkey_valid = false;

     
                                                                
     
  procTup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcOid));
  if (!HeapTupleIsValid(procTup))
  {
    elog(ERROR, "cache lookup failed for function %u", funcOid);
  }
  procStruct = (Form_pg_proc)GETSTRUCT(procTup);

     
                                                                            
                                        
     
  function = (PLpgSQL_function *)fcinfo->flinfo->fn_extra;

recheck:
  if (!function)
  {
                                                                       
    compute_function_hashkey(fcinfo, procStruct, &hashkey, forValidator);
    hashkey_valid = true;

                           
    function = plpgsql_HashTableLookup(&hashkey);
  }

  if (function)
  {
                                                             
    if (function->fn_xmin == HeapTupleHeaderGetRawXmin(procTup->t_data) && ItemPointerEquals(&function->fn_tid, &procTup->t_self))
    {
      function_valid = true;
    }
    else
    {
         
                                                                      
                                        
         
      delete_function(function);

         
                                                                       
                                                                         
                                                                       
                                                                      
                                                                  
                                                                    
                                                                      
                                      
         
                                                                         
                                                                         
                    
         
      if (function->use_count != 0)
      {
        function = NULL;
        if (!hashkey_valid)
        {
          goto recheck;
        }
      }
    }
  }

     
                                                                            
     
  if (!function_valid)
  {
       
                                                                          
                           
       
    if (!hashkey_valid)
    {
      compute_function_hashkey(fcinfo, procStruct, &hashkey, forValidator);
    }

       
                         
       
    function = do_compile(fcinfo, procTup, function, &hashkey, forValidator);
  }

  ReleaseSysCache(procTup);

     
                                                                  
     
  fcinfo->flinfo->fn_extra = (void *)function;

     
                                          
     
  return function;
}

   
                                               
   
                                                                           
                                 
   
                                                               
                                                                      
                                                                    
                        
   
                                                                       
                                                                  
                                                                      
                                                                 
                                                            
                                                             
                                               
   
                                                                             
                                                         
   
static PLpgSQL_function *
do_compile(FunctionCallInfo fcinfo, HeapTuple procTup, PLpgSQL_function *function, PLpgSQL_func_hashkey *hashkey, bool forValidator)
{
  Form_pg_proc procStruct = (Form_pg_proc)GETSTRUCT(procTup);
  bool is_dml_trigger = CALLED_AS_TRIGGER(fcinfo);
  bool is_event_trigger = CALLED_AS_EVENT_TRIGGER(fcinfo);
  Datum prosrcdatum;
  bool isnull;
  char *proc_source;
  HeapTuple typeTup;
  Form_pg_type typeStruct;
  PLpgSQL_variable *var;
  PLpgSQL_rec *rec;
  int i;
  ErrorContextCallback plerrcontext;
  int parse_rc;
  Oid rettypeid;
  int numargs;
  int num_in_args = 0;
  int num_out_args = 0;
  Oid *argtypes;
  char **argnames;
  char *argmodes;
  int *in_arg_varnos = NULL;
  PLpgSQL_variable **out_arg_variables;
  MemoryContext func_cxt;

     
                                                                           
                                                                           
                                     
     
  prosrcdatum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isnull);
  if (isnull)
  {
    elog(ERROR, "null prosrc");
  }
  proc_source = TextDatumGetCString(prosrcdatum);
  plpgsql_scanner_init(proc_source);

  plpgsql_error_funcname = pstrdup(NameStr(procStruct->proname));

     
                                                 
     
  plerrcontext.callback = plpgsql_compile_error_callback;
  plerrcontext.arg = forValidator ? proc_source : NULL;
  plerrcontext.previous = error_context_stack;
  error_context_stack = &plerrcontext;

     
                                                                             
                                                                           
              
     
  plpgsql_check_syntax = forValidator;

     
                                                                        
                                                                      
     
  if (function == NULL)
  {
    function = (PLpgSQL_function *)MemoryContextAllocZero(TopMemoryContext, sizeof(PLpgSQL_function));
  }
  else
  {
                                                                
    memset(function, 0, sizeof(PLpgSQL_function));
  }
  plpgsql_curr_compile = function;

     
                                                                            
                                                                 
     
  func_cxt = AllocSetContextCreate(TopMemoryContext, "PL/pgSQL function", ALLOCSET_DEFAULT_SIZES);
  plpgsql_compile_tmp_cxt = MemoryContextSwitchTo(func_cxt);

  function->fn_signature = format_procedure(fcinfo->flinfo->fn_oid);
  MemoryContextSetIdentifier(func_cxt, function->fn_signature);
  function->fn_oid = fcinfo->flinfo->fn_oid;
  function->fn_xmin = HeapTupleHeaderGetRawXmin(procTup->t_data);
  function->fn_tid = procTup->t_self;
  function->fn_input_collation = fcinfo->fncollation;
  function->fn_cxt = func_cxt;
  function->out_param_varno = -1;                              
  function->resolve_option = plpgsql_variable_conflict;
  function->print_strict_params = plpgsql_print_strict_params;
                                                                      
  function->extra_warnings = forValidator ? plpgsql_extra_warnings : 0;
  function->extra_errors = forValidator ? plpgsql_extra_errors : 0;

  if (is_dml_trigger)
  {
    function->fn_is_trigger = PLPGSQL_DML_TRIGGER;
  }
  else if (is_event_trigger)
  {
    function->fn_is_trigger = PLPGSQL_EVENT_TRIGGER;
  }
  else
  {
    function->fn_is_trigger = PLPGSQL_NOT_TRIGGER;
  }

  function->fn_prokind = procStruct->prokind;

  function->nstatements = 0;

     
                                                                     
                                                                        
                                                                        
     
  plpgsql_ns_init();
  plpgsql_ns_push(NameStr(procStruct->proname), PLPGSQL_LABEL_BLOCK);
  plpgsql_DumpExecTree = false;
  plpgsql_start_datums();

  switch (function->fn_is_trigger)
  {
  case PLPGSQL_NOT_TRIGGER:

       
                                                                       
                                                    
       
                                                               
                                                                      
                                                        
       
    MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);

    numargs = get_func_arg_info(procTup, &argtypes, &argnames, &argmodes);

    plpgsql_resolve_polymorphic_argtypes(numargs, argtypes, argmodes, fcinfo->flinfo->fn_expr, forValidator, plpgsql_error_funcname);

    in_arg_varnos = (int *)palloc(numargs * sizeof(int));
    out_arg_variables = (PLpgSQL_variable **)palloc(numargs * sizeof(PLpgSQL_variable *));

    MemoryContextSwitchTo(func_cxt);

       
                                                            
       
    for (i = 0; i < numargs; i++)
    {
      char buf[32];
      Oid argtypeid = argtypes[i];
      char argmode = argmodes ? argmodes[i] : PROARGMODE_IN;
      PLpgSQL_type *argdtype;
      PLpgSQL_variable *argvariable;
      PLpgSQL_nsitem_type argitemtype;

                                       
      snprintf(buf, sizeof(buf), "$%d", i + 1);

                                
      argdtype = plpgsql_build_datatype(argtypeid, -1, function->fn_input_collation, NULL);

                                        
                                                        
                                                             
      if (argdtype->ttype == PLPGSQL_TTYPE_PSEUDO)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/pgSQL functions cannot accept type %s", format_type_be(argtypeid))));
      }

         
                                                                  
                                                                  
         
      argvariable = plpgsql_build_variable((argnames && argnames[i][0] != '\0') ? argnames[i] : buf, 0, argdtype, false);

      if (argvariable->dtype == PLPGSQL_DTYPE_VAR)
      {
        argitemtype = PLPGSQL_NSTYPE_VAR;
      }
      else
      {
        Assert(argvariable->dtype == PLPGSQL_DTYPE_REC);
        argitemtype = PLPGSQL_NSTYPE_REC;
      }

                                                    
      if (argmode == PROARGMODE_IN || argmode == PROARGMODE_INOUT || argmode == PROARGMODE_VARIADIC)
      {
        in_arg_varnos[num_in_args++] = argvariable->dno;
      }
      if (argmode == PROARGMODE_OUT || argmode == PROARGMODE_INOUT || argmode == PROARGMODE_TABLE)
      {
        out_arg_variables[num_out_args++] = argvariable;
      }

                                              
      add_parameter_name(argitemtype, argvariable->dno, buf);

                                                             
      if (argnames && argnames[i][0] != '\0')
      {
        add_parameter_name(argitemtype, argvariable->dno, argnames[i]);
      }
    }

       
                                                                 
                                                                   
                                                                    
                  
       
    if (num_out_args > 1 || (num_out_args == 1 && function->fn_prokind == PROKIND_PROCEDURE))
    {
      PLpgSQL_row *row = build_row_from_vars(out_arg_variables, num_out_args);

      plpgsql_adddatum((PLpgSQL_datum *)row);
      function->out_param_varno = row->dno;
    }
    else if (num_out_args == 1)
    {
      function->out_param_varno = out_arg_variables[0]->dno;
    }

       
                                                                    
                                                                   
                                                                      
                       
       
                                                                       
                                                                    
                           
       
    rettypeid = procStruct->prorettype;
    if (IsPolymorphicType(rettypeid))
    {
      if (forValidator)
      {
        if (rettypeid == ANYARRAYOID)
        {
          rettypeid = INT4ARRAYOID;
        }
        else if (rettypeid == ANYRANGEOID)
        {
          rettypeid = INT4RANGEOID;
        }
        else                                
        {
          rettypeid = INT4OID;
        }
                                                
      }
      else
      {
        rettypeid = get_fn_expr_rettype(fcinfo->flinfo);
        if (!OidIsValid(rettypeid))
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not determine actual return type "
                                                                         "for polymorphic function \"%s\"",
                                                                      plpgsql_error_funcname)));
        }
      }
    }

       
                                                
       
    function->fn_rettype = rettypeid;
    function->fn_retset = procStruct->proretset;

       
                                         
       
    typeTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(rettypeid));
    if (!HeapTupleIsValid(typeTup))
    {
      elog(ERROR, "cache lookup failed for type %u", rettypeid);
    }
    typeStruct = (Form_pg_type)GETSTRUCT(typeTup);

                                                           
                                                      
    if (typeStruct->typtype == TYPTYPE_PSEUDO)
    {
      if (rettypeid == VOIDOID || rettypeid == RECORDOID)
                  ;
      else if (rettypeid == TRIGGEROID || rettypeid == EVTTRIGGEROID)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("trigger functions can only be called as triggers")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/pgSQL functions cannot return type %s", format_type_be(rettypeid))));
      }
    }

    function->fn_retistuple = type_is_rowtype(rettypeid);
    function->fn_retisdomain = (typeStruct->typtype == TYPTYPE_DOMAIN);
    function->fn_retbyval = typeStruct->typbyval;
    function->fn_rettyplen = typeStruct->typlen;

       
                                                                    
                                                              
                  
       
    if (IsPolymorphicType(procStruct->prorettype) && num_out_args == 0)
    {
      (void)plpgsql_build_variable("$0", 0, build_datatype(typeTup, -1, function->fn_input_collation, NULL), true);
    }

    ReleaseSysCache(typeTup);
    break;

  case PLPGSQL_DML_TRIGGER:
                                                        
    function->fn_rettype = InvalidOid;
    function->fn_retbyval = false;
    function->fn_retistuple = true;
    function->fn_retisdomain = false;
    function->fn_retset = false;

                                             
    if (procStruct->pronargs != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("trigger functions cannot have declared arguments"), errhint("The arguments of the trigger can be accessed through TG_NARGS and TG_ARGV instead.")));
    }

                                                
    rec = plpgsql_build_record("new", 0, NULL, RECORDOID, true);
    function->new_varno = rec->dno;

                                                
    rec = plpgsql_build_record("old", 0, NULL, RECORDOID, true);
    function->old_varno = rec->dno;

                                  
    var = plpgsql_build_variable("tg_name", 0, plpgsql_build_datatype(NAMEOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_NAME;

                                  
    var = plpgsql_build_variable("tg_when", 0, plpgsql_build_datatype(TEXTOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_WHEN;

                                   
    var = plpgsql_build_variable("tg_level", 0, plpgsql_build_datatype(TEXTOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_LEVEL;

                                
    var = plpgsql_build_variable("tg_op", 0, plpgsql_build_datatype(TEXTOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_OP;

                                   
    var = plpgsql_build_variable("tg_relid", 0, plpgsql_build_datatype(OIDOID, -1, InvalidOid, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_RELID;

                                     
    var = plpgsql_build_variable("tg_relname", 0, plpgsql_build_datatype(NAMEOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_TABLE_NAME;

                                                      
    var = plpgsql_build_variable("tg_table_name", 0, plpgsql_build_datatype(NAMEOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_TABLE_NAME;

                                          
    var = plpgsql_build_variable("tg_table_schema", 0, plpgsql_build_datatype(NAMEOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_TABLE_SCHEMA;

                                   
    var = plpgsql_build_variable("tg_nargs", 0, plpgsql_build_datatype(INT4OID, -1, InvalidOid, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_NARGS;

                                  
    var = plpgsql_build_variable("tg_argv", 0, plpgsql_build_datatype(TEXTARRAYOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_ARGV;

    break;

  case PLPGSQL_EVENT_TRIGGER:
    function->fn_rettype = VOIDOID;
    function->fn_retbyval = false;
    function->fn_retistuple = true;
    function->fn_retisdomain = false;
    function->fn_retset = false;

                                             
    if (procStruct->pronargs != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("event trigger functions cannot have declared arguments")));
    }

                                   
    var = plpgsql_build_variable("tg_event", 0, plpgsql_build_datatype(TEXTOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_EVENT;

                                 
    var = plpgsql_build_variable("tg_tag", 0, plpgsql_build_datatype(TEXTOID, -1, function->fn_input_collation, NULL), true);
    Assert(var->dtype == PLPGSQL_DTYPE_VAR);
    var->dtype = PLPGSQL_DTYPE_PROMISE;
    ((PLpgSQL_var *)var)->promise = PLPGSQL_PROMISE_TG_TAG;

    break;

  default:
    elog(ERROR, "unrecognized function typecode: %d", (int)function->fn_is_trigger);
    break;
  }

                                                
  function->fn_readonly = (procStruct->provolatile != PROVOLATILE_VOLATILE);

     
                                      
     
  var = plpgsql_build_variable("found", 0, plpgsql_build_datatype(BOOLOID, -1, InvalidOid, NULL), true);
  function->found_varno = var->dno;

     
                                   
     
  parse_rc = plpgsql_yyparse();
  if (parse_rc != 0)
  {
    elog(ERROR, "plpgsql parser returned %d", parse_rc);
  }
  function->action = plpgsql_parse_result;

  plpgsql_scanner_finish();
  pfree(proc_source);

     
                                                                         
                                                                           
                                                                           
                                           
     
  if (num_out_args > 0 || function->fn_rettype == VOIDOID || function->fn_retset)
  {
    add_dummy_return(function);
  }

     
                                  
     
  function->fn_nargs = procStruct->pronargs;
  for (i = 0; i < function->fn_nargs; i++)
  {
    function->fn_argvarnos[i] = in_arg_varnos[i];
  }

  plpgsql_finish_datums(function);

                                          
  if (plpgsql_DumpExecTree)
  {
    plpgsql_dumptree(function);
  }

     
                              
     
  plpgsql_HashTableInsert(function, hashkey);

     
                                 
     
  error_context_stack = plerrcontext.previous;
  plpgsql_error_funcname = NULL;

  plpgsql_check_syntax = false;

  MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);
  plpgsql_compile_tmp_cxt = NULL;
  return function;
}

              
                                                                              
   
                                                                           
                  
   
                                                                              
                               
              
   
PLpgSQL_function *
plpgsql_compile_inline(char *proc_source)
{
  char *func_name = "inline_code_block";
  PLpgSQL_function *function;
  ErrorContextCallback plerrcontext;
  PLpgSQL_variable *var;
  int parse_rc;
  MemoryContext func_cxt;

     
                                                                           
                                                                           
                                     
     
  plpgsql_scanner_init(proc_source);

  plpgsql_error_funcname = func_name;

     
                                                 
     
  plerrcontext.callback = plpgsql_compile_error_callback;
  plerrcontext.arg = proc_source;
  plerrcontext.previous = error_context_stack;
  error_context_stack = &plerrcontext;

                                                               
  plpgsql_check_syntax = check_function_bodies;

                                                            
  function = (PLpgSQL_function *)palloc0(sizeof(PLpgSQL_function));

  plpgsql_curr_compile = function;

     
                                                                           
                                                            
     
  func_cxt = AllocSetContextCreate(CurrentMemoryContext, "PL/pgSQL inline code context", ALLOCSET_DEFAULT_SIZES);
  plpgsql_compile_tmp_cxt = MemoryContextSwitchTo(func_cxt);

  function->fn_signature = pstrdup(func_name);
  function->fn_is_trigger = PLPGSQL_NOT_TRIGGER;
  function->fn_input_collation = InvalidOid;
  function->fn_cxt = func_cxt;
  function->out_param_varno = -1;                              
  function->resolve_option = plpgsql_variable_conflict;
  function->print_strict_params = plpgsql_print_strict_params;

     
                                                                            
                
     
  function->extra_warnings = 0;
  function->extra_errors = 0;

  function->nstatements = 0;

  plpgsql_ns_init();
  plpgsql_ns_push(func_name, PLPGSQL_LABEL_BLOCK);
  plpgsql_DumpExecTree = false;
  plpgsql_start_datums();

                                                     
  function->fn_rettype = VOIDOID;
  function->fn_retset = false;
  function->fn_retistuple = false;
  function->fn_retisdomain = false;
  function->fn_prokind = PROKIND_FUNCTION;
                                                         
  function->fn_retbyval = true;
  function->fn_rettyplen = sizeof(int32);

     
                                                                          
                                                               
     
  function->fn_readonly = false;

     
                                      
     
  var = plpgsql_build_variable("found", 0, plpgsql_build_datatype(BOOLOID, -1, InvalidOid, NULL), true);
  function->found_varno = var->dno;

     
                                   
     
  parse_rc = plpgsql_yyparse();
  if (parse_rc != 0)
  {
    elog(ERROR, "plpgsql parser returned %d", parse_rc);
  }
  function->action = plpgsql_parse_result;

  plpgsql_scanner_finish();

     
                                                                         
                                                            
     
  if (function->fn_rettype == VOIDOID)
  {
    add_dummy_return(function);
  }

     
                                  
     
  function->fn_nargs = 0;

  plpgsql_finish_datums(function);

     
                                 
     
  error_context_stack = plerrcontext.previous;
  plpgsql_error_funcname = NULL;

  plpgsql_check_syntax = false;

  MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);
  plpgsql_compile_tmp_cxt = NULL;
  return function;
}

   
                                                                   
                                                                           
                                         
   
static void
plpgsql_compile_error_callback(void *arg)
{
  if (arg)
  {
       
                                                                          
                                      
       
    if (function_parse_error_transpose((const char *)arg))
    {
      return;
    }

       
                                                                          
                                            
       
  }

  if (plpgsql_error_funcname)
  {
    errcontext("compilation of PL/pgSQL function \"%s\" near line %d", plpgsql_error_funcname, plpgsql_latest_lineno());
  }
}

   
                                                                   
   
static void
add_parameter_name(PLpgSQL_nsitem_type itemtype, int itemno, const char *name)
{
     
                                                                             
                                                                      
                                                                        
                                                                          
                   
     
  if (plpgsql_ns_lookup(plpgsql_ns_top(), true, name, NULL, NULL, NULL) != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("parameter name \"%s\" used more than once", name)));
  }

                        
  plpgsql_ns_additem(itemtype, itemno, name);
}

   
                                                             
   
static void
add_dummy_return(PLpgSQL_function *function)
{
     
                                                                             
                                                                       
                                                                            
                                                 
     
  if (function->action->exceptions != NULL || function->action->label != NULL)
  {
    PLpgSQL_stmt_block *new;

    new = palloc0(sizeof(PLpgSQL_stmt_block));
    new->cmd_type = PLPGSQL_STMT_BLOCK;
    new->stmtid = ++function->nstatements;
    new->body = list_make1(function->action);

    function->action = new;
  }
  if (function->action->body == NIL || ((PLpgSQL_stmt *)llast(function->action->body))->cmd_type != PLPGSQL_STMT_RETURN)
  {
    PLpgSQL_stmt_return *new;

    new = palloc0(sizeof(PLpgSQL_stmt_return));
    new->cmd_type = PLPGSQL_STMT_RETURN;
    new->stmtid = ++function->nstatements;
    new->expr = NULL;
    new->retvarno = function->out_param_varno;

    function->action->body = lappend(function->action->body, new);
  }
}

   
                                                                    
   
                                                                             
                                                                              
                                                                        
                                       
   
void
plpgsql_parser_setup(struct ParseState *pstate, PLpgSQL_expr *expr)
{
  pstate->p_pre_columnref_hook = plpgsql_pre_column_ref;
  pstate->p_post_columnref_hook = plpgsql_post_column_ref;
  pstate->p_paramref_hook = plpgsql_param_ref;
                                          
  pstate->p_ref_hook_state = (void *)expr;
}

   
                                                                      
   
static Node *
plpgsql_pre_column_ref(ParseState *pstate, ColumnRef *cref)
{
  PLpgSQL_expr *expr = (PLpgSQL_expr *)pstate->p_ref_hook_state;

  if (expr->func->resolve_option == PLPGSQL_RESOLVE_VARIABLE)
  {
    return resolve_column_ref(pstate, expr, cref, false);
  }
  else
  {
    return NULL;
  }
}

   
                                                                      
   
static Node *
plpgsql_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var)
{
  PLpgSQL_expr *expr = (PLpgSQL_expr *)pstate->p_ref_hook_state;
  Node *myvar;

  if (expr->func->resolve_option == PLPGSQL_RESOLVE_VARIABLE)
  {
    return NULL;                                        
  }

  if (expr->func->resolve_option == PLPGSQL_RESOLVE_COLUMN && var != NULL)
  {
    return NULL;                                          
  }

     
                                                                          
                                                                         
                                                                    
                                                                             
                                                                        
                                                                             
                                                                         
                                      
     
  myvar = resolve_column_ref(pstate, expr, cref, (var == NULL));

  if (myvar != NULL && var != NULL)
  {
       
                                                                        
                                                                 
       
    ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("column reference \"%s\" is ambiguous", NameListToString(cref->fields)), errdetail("It could refer to either a PL/pgSQL variable or a table column."), parser_errposition(pstate, cref->location)));
  }

  return myvar;
}

   
                                                                 
   
static Node *
plpgsql_param_ref(ParseState *pstate, ParamRef *pref)
{
  PLpgSQL_expr *expr = (PLpgSQL_expr *)pstate->p_ref_hook_state;
  char pname[32];
  PLpgSQL_nsitem *nse;

  snprintf(pname, sizeof(pname), "$%d", pref->number);

  nse = plpgsql_ns_lookup(expr->ns, false, pname, NULL, NULL, NULL);

  if (nse == NULL)
  {
    return NULL;                                
  }

  return make_datum_param(expr, nse->itemno, pref->location);
}

   
                                                                       
   
                                                                    
   
                                                                            
                                                                             
   
static Node *
resolve_column_ref(ParseState *pstate, PLpgSQL_expr *expr, ColumnRef *cref, bool error_if_no_field)
{
  PLpgSQL_execstate *estate;
  PLpgSQL_nsitem *nse;
  const char *name1;
  const char *name2 = NULL;
  const char *name3 = NULL;
  const char *colname = NULL;
  int nnames;
  int nnames_scalar = 0;
  int nnames_wholerow = 0;
  int nnames_field = 0;

     
                                                                           
                                                                            
                                       
     
  estate = expr->func->cur_estate;

               
                               
     
                                                                  
                                                                       
                                             
                                      
                                                 
               
     
  switch (list_length(cref->fields))
  {
  case 1:
  {
    Node *field1 = (Node *)linitial(cref->fields);

    Assert(IsA(field1, String));
    name1 = strVal(field1);
    nnames_scalar = 1;
    nnames_wholerow = 1;
    break;
  }
  case 2:
  {
    Node *field1 = (Node *)linitial(cref->fields);
    Node *field2 = (Node *)lsecond(cref->fields);

    Assert(IsA(field1, String));
    name1 = strVal(field1);

                              
    if (IsA(field2, A_Star))
    {
                                                            
      name2 = "*";
      nnames_wholerow = 1;
      break;
    }

    Assert(IsA(field2, String));
    name2 = strVal(field2);
    colname = name2;
    nnames_scalar = 2;
    nnames_wholerow = 2;
    nnames_field = 1;
    break;
  }
  case 3:
  {
    Node *field1 = (Node *)linitial(cref->fields);
    Node *field2 = (Node *)lsecond(cref->fields);
    Node *field3 = (Node *)lthird(cref->fields);

    Assert(IsA(field1, String));
    name1 = strVal(field1);
    Assert(IsA(field2, String));
    name2 = strVal(field2);

                              
    if (IsA(field3, A_Star))
    {
                                                            
      name3 = "*";
      nnames_wholerow = 2;
      break;
    }

    Assert(IsA(field3, String));
    name3 = strVal(field3);
    colname = name3;
    nnames_field = 2;
    break;
  }
  default:
                                
    return NULL;
  }

  nse = plpgsql_ns_lookup(expr->ns, false, name1, name2, name3, &nnames);

  if (nse == NULL)
  {
    return NULL;                                
  }

  switch (nse->itemtype)
  {
  case PLPGSQL_NSTYPE_VAR:
    if (nnames == nnames_scalar)
    {
      return make_datum_param(expr, nse->itemno, cref->location);
    }
    break;
  case PLPGSQL_NSTYPE_REC:
    if (nnames == nnames_wholerow)
    {
      return make_datum_param(expr, nse->itemno, cref->location);
    }
    if (nnames == nnames_field)
    {
                                                   
      PLpgSQL_rec *rec = (PLpgSQL_rec *)estate->datums[nse->itemno];
      int i;

                                                     
      i = rec->firstfield;
      while (i >= 0)
      {
        PLpgSQL_recfield *fld = (PLpgSQL_recfield *)estate->datums[i];

        Assert(fld->dtype == PLPGSQL_DTYPE_RECFIELD && fld->recparentno == nse->itemno);
        if (strcmp(fld->fieldname, colname) == 0)
        {
          return make_datum_param(expr, i, cref->location);
        }
        i = fld->nextfield;
      }

         
                                                                 
                                                                    
                                                                   
                                                              
         
      if (error_if_no_field)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("record \"%s\" has no field \"%s\"", (nnames_field == 1) ? name1 : name2, colname), parser_errposition(pstate, cref->location)));
      }
    }
    break;
  default:
    elog(ERROR, "unrecognized plpgsql itemtype: %d", nse->itemtype);
  }

                                                           
  return NULL;
}

   
                                                                            
                                                                         
   
static Node *
make_datum_param(PLpgSQL_expr *expr, int dno, int location)
{
  PLpgSQL_execstate *estate;
  PLpgSQL_datum *datum;
  Param *param;
  MemoryContext oldcontext;

                                         
  estate = expr->func->cur_estate;
  Assert(dno >= 0 && dno < estate->ndatums);
  datum = estate->datums[dno];

     
                                                                        
     
  oldcontext = MemoryContextSwitchTo(expr->func->fn_cxt);
  expr->paramnos = bms_add_member(expr->paramnos, dno);
  MemoryContextSwitchTo(oldcontext);

  param = makeNode(Param);
  param->paramkind = PARAM_EXTERN;
  param->paramid = dno + 1;
  plpgsql_exec_get_datum_type_info(estate, datum, &param->paramtype, &param->paramtypmod, &param->paramcollid);
  param->location = location;

  return (Node *)param;
}

              
                                                           
                                                      
   
                                                                          
                                        
   
                                                                        
                                                               
   
                                                                          
                                                        
   
                                                                 
                                                      
                                                                          
                                                        
              
   
bool
plpgsql_parse_word(char *word1, const char *yytxt, bool lookup, PLwdatum *wdatum, PLword *word)
{
  PLpgSQL_nsitem *ns;

     
                                                                 
                                                                         
                                      
     
  if (lookup && plpgsql_IdentifierLookup == IDENTIFIER_LOOKUP_NORMAL)
  {
       
                                                  
       
    ns = plpgsql_ns_lookup(plpgsql_ns_top(), false, word1, NULL, NULL, NULL);

    if (ns != NULL)
    {
      switch (ns->itemtype)
      {
      case PLPGSQL_NSTYPE_VAR:
      case PLPGSQL_NSTYPE_REC:
        wdatum->datum = plpgsql_Datums[ns->itemno];
        wdatum->ident = word1;
        wdatum->quoted = (yytxt[0] == '"');
        wdatum->idents = NIL;
        return true;

      default:
                                                                 
        elog(ERROR, "unrecognized plpgsql itemtype: %d", ns->itemtype);
      }
    }
  }

     
                                                                           
         
     
  word->ident = word1;
  word->quoted = (yytxt[0] == '"');
  return false;
}

              
                                                    
                           
              
   
bool
plpgsql_parse_dblword(char *word1, char *word2, PLwdatum *wdatum, PLcword *cword)
{
  PLpgSQL_nsitem *ns;
  List *idents;
  int nnames;

  idents = list_make2(makeString(word1), makeString(word2));

     
                                                                       
                                                                         
             
     
  if (plpgsql_IdentifierLookup != IDENTIFIER_LOOKUP_DECLARE)
  {
       
                                                  
       
    ns = plpgsql_ns_lookup(plpgsql_ns_top(), false, word1, word2, NULL, &nnames);
    if (ns != NULL)
    {
      switch (ns->itemtype)
      {
      case PLPGSQL_NSTYPE_VAR:
                                                           
        wdatum->datum = plpgsql_Datums[ns->itemno];
        wdatum->ident = NULL;
        wdatum->quoted = false;               
        wdatum->idents = idents;
        return true;

      case PLPGSQL_NSTYPE_REC:
        if (nnames == 1)
        {
             
                                                               
                                                             
                                                              
                             
             
          PLpgSQL_rec *rec;
          PLpgSQL_recfield *new;

          rec = (PLpgSQL_rec *)(plpgsql_Datums[ns->itemno]);
          new = plpgsql_build_recfield(rec, word2);

          wdatum->datum = (PLpgSQL_datum *)new;
        }
        else
        {
                                                             
          wdatum->datum = plpgsql_Datums[ns->itemno];
        }
        wdatum->ident = NULL;
        wdatum->quoted = false;               
        wdatum->idents = idents;
        return true;

      default:
        break;
      }
    }
  }

                     
  cword->idents = idents;
  return false;
}

              
                                                       
                          
              
   
bool
plpgsql_parse_tripword(char *word1, char *word2, char *word3, PLwdatum *wdatum, PLcword *cword)
{
  PLpgSQL_nsitem *ns;
  List *idents;
  int nnames;

  idents = list_make3(makeString(word1), makeString(word2), makeString(word3));

     
                                                                       
                                                                         
             
     
  if (plpgsql_IdentifierLookup != IDENTIFIER_LOOKUP_DECLARE)
  {
       
                                                                         
                               
       
    ns = plpgsql_ns_lookup(plpgsql_ns_top(), false, word1, word2, word3, &nnames);
    if (ns != NULL && nnames == 2)
    {
      switch (ns->itemtype)
      {
      case PLPGSQL_NSTYPE_REC:
      {
           
                                                               
                                   
           
        PLpgSQL_rec *rec;
        PLpgSQL_recfield *new;

        rec = (PLpgSQL_rec *)(plpgsql_Datums[ns->itemno]);
        new = plpgsql_build_recfield(rec, word3);

        wdatum->datum = (PLpgSQL_datum *)new;
        wdatum->ident = NULL;
        wdatum->quoted = false;               
        wdatum->idents = idents;
        return true;
      }

      default:
        break;
      }
    }
  }

                     
  cword->idents = idents;
  return false;
}

              
                                                                   
                                     
   
                                                                
              
   
PLpgSQL_type *
plpgsql_parse_wordtype(char *ident)
{
  PLpgSQL_type *dtype;
  PLpgSQL_nsitem *nse;
  TypeName *typeName;
  HeapTuple typeTup;

     
                                                
     
  nse = plpgsql_ns_lookup(plpgsql_ns_top(), false, ident, NULL, NULL, NULL);

  if (nse != NULL)
  {
    switch (nse->itemtype)
    {
    case PLPGSQL_NSTYPE_VAR:
      return ((PLpgSQL_var *)(plpgsql_Datums[nse->itemno]))->datatype;

                                           

    default:
      return NULL;
    }
  }

     
                                                                            
                                                          
     
  typeName = makeTypeName(ident);
  typeTup = LookupTypeName(NULL, typeName, NULL, false);
  if (typeTup)
  {
    Form_pg_type typeStruct = (Form_pg_type)GETSTRUCT(typeTup);

    if (!typeStruct->typisdefined || typeStruct->typrelid != InvalidOid)
    {
      ReleaseSysCache(typeTup);
      return NULL;
    }

    dtype = build_datatype(typeTup, -1, plpgsql_curr_compile->fn_input_collation, typeName);

    ReleaseSysCache(typeTup);
    return dtype;
  }

     
                                                                           
         
     
  return NULL;
}

              
                                                               
              
   
PLpgSQL_type *
plpgsql_parse_cwordtype(List *idents)
{
  PLpgSQL_type *dtype = NULL;
  PLpgSQL_nsitem *nse;
  const char *fldname;
  Oid classOid;
  HeapTuple classtup = NULL;
  HeapTuple attrtup = NULL;
  HeapTuple typetup = NULL;
  Form_pg_class classStruct;
  Form_pg_attribute attrStruct;
  MemoryContext oldCxt;

                                                            
  oldCxt = MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);

  if (list_length(idents) == 2)
  {
       
                                                                          
                                                                     
                  
       
    nse = plpgsql_ns_lookup(plpgsql_ns_top(), false, strVal(linitial(idents)), strVal(lsecond(idents)), NULL, NULL);

    if (nse != NULL && nse->itemtype == PLPGSQL_NSTYPE_VAR)
    {
      dtype = ((PLpgSQL_var *)(plpgsql_Datums[nse->itemno]))->datatype;
      goto done;
    }

       
                                             
       
    classOid = RelnameGetRelid(strVal(linitial(idents)));
    if (!OidIsValid(classOid))
    {
      goto done;
    }
    fldname = strVal(lsecond(idents));
  }
  else if (list_length(idents) == 3)
  {
    RangeVar *relvar;

    relvar = makeRangeVar(strVal(linitial(idents)), strVal(lsecond(idents)), -1);
                                                             
    classOid = RangeVarGetRelid(relvar, NoLock, true);
    if (!OidIsValid(classOid))
    {
      goto done;
    }
    fldname = strVal(lthird(idents));
  }
  else
  {
    goto done;
  }

  classtup = SearchSysCache1(RELOID, ObjectIdGetDatum(classOid));
  if (!HeapTupleIsValid(classtup))
  {
    goto done;
  }
  classStruct = (Form_pg_class)GETSTRUCT(classtup);

     
                                                                         
                            
     
  if (classStruct->relkind != RELKIND_RELATION && classStruct->relkind != RELKIND_SEQUENCE && classStruct->relkind != RELKIND_VIEW && classStruct->relkind != RELKIND_MATVIEW && classStruct->relkind != RELKIND_COMPOSITE_TYPE && classStruct->relkind != RELKIND_FOREIGN_TABLE && classStruct->relkind != RELKIND_PARTITIONED_TABLE)
  {
    goto done;
  }

     
                                              
     
  attrtup = SearchSysCacheAttName(classOid, fldname);
  if (!HeapTupleIsValid(attrtup))
  {
    goto done;
  }
  attrStruct = (Form_pg_attribute)GETSTRUCT(attrtup);

  typetup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(attrStruct->atttypid));
  if (!HeapTupleIsValid(typetup))
  {
    elog(ERROR, "cache lookup failed for type %u", attrStruct->atttypid);
  }

     
                                                                       
                                                                       
                                                                           
     
  MemoryContextSwitchTo(oldCxt);
  dtype = build_datatype(typetup, attrStruct->atttypmod, attrStruct->attcollation, NULL);
  MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);

done:
  if (HeapTupleIsValid(classtup))
  {
    ReleaseSysCache(classtup);
  }
  if (HeapTupleIsValid(attrtup))
  {
    ReleaseSysCache(attrtup);
  }
  if (HeapTupleIsValid(typetup))
  {
    ReleaseSysCache(typetup);
  }

  MemoryContextSwitchTo(oldCxt);
  return dtype;
}

              
                                                          
                                     
              
   
PLpgSQL_type *
plpgsql_parse_wordrowtype(char *ident)
{
  Oid classOid;

     
                                                                         
                                                                           
                                                                         
                                                                            
                                                                             
     
  classOid = RelnameGetRelid(ident);
  if (!OidIsValid(classOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s\" does not exist", ident)));
  }

                                            
  return plpgsql_build_datatype(get_rel_type_id(classOid), -1, InvalidOid, makeTypeName(ident));
}

              
                                                                    
                                                       
              
   
PLpgSQL_type *
plpgsql_parse_cwordrowtype(List *idents)
{
  Oid classOid;
  RangeVar *relvar;
  MemoryContext oldCxt;

     
                                                                          
                                                             
     
  if (list_length(idents) != 2)
  {
    return NULL;
  }

                                                        
  oldCxt = MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);

                                                                             
  relvar = makeRangeVar(strVal(linitial(idents)), strVal(lsecond(idents)), -1);
  classOid = RangeVarGetRelid(relvar, NoLock, false);

  MemoryContextSwitchTo(oldCxt);

                                            
  return plpgsql_build_datatype(get_rel_type_id(classOid), -1, InvalidOid, makeTypeNameFromNameList(idents));
}

   
                                                                 
            
   
                                                           
                                                         
                                                                   
                                                   
   
PLpgSQL_variable *
plpgsql_build_variable(const char *refname, int lineno, PLpgSQL_type *dtype, bool add2namespace)
{
  PLpgSQL_variable *result;

  switch (dtype->ttype)
  {
  case PLPGSQL_TTYPE_SCALAR:
  {
                                  
    PLpgSQL_var *var;

    var = palloc0(sizeof(PLpgSQL_var));
    var->dtype = PLPGSQL_DTYPE_VAR;
    var->refname = pstrdup(refname);
    var->lineno = lineno;
    var->datatype = dtype;
                                                                

                        
    var->value = 0;
    var->isnull = true;
    var->freeval = false;

    plpgsql_adddatum((PLpgSQL_datum *)var);
    if (add2namespace)
    {
      plpgsql_ns_additem(PLPGSQL_NSTYPE_VAR, var->dno, refname);
    }
    result = (PLpgSQL_variable *)var;
    break;
  }
  case PLPGSQL_TTYPE_REC:
  {
                                                   
    PLpgSQL_rec *rec;

    rec = plpgsql_build_record(refname, lineno, dtype, dtype->typoid, add2namespace);
    result = (PLpgSQL_variable *)rec;
    break;
  }
  case PLPGSQL_TTYPE_PSEUDO:
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("variable \"%s\" has pseudo-type %s", refname, format_type_be(dtype->typoid))));
    result = NULL;                          
    break;
  default:
    elog(ERROR, "unrecognized ttype: %d", dtype->ttype);
    result = NULL;                          
    break;
  }

  return result;
}

   
                                                                         
   
PLpgSQL_rec *
plpgsql_build_record(const char *refname, int lineno, PLpgSQL_type *dtype, Oid rectypeid, bool add2namespace)
{
  PLpgSQL_rec *rec;

  rec = palloc0(sizeof(PLpgSQL_rec));
  rec->dtype = PLPGSQL_DTYPE_REC;
  rec->refname = pstrdup(refname);
  rec->lineno = lineno;
                                                              
  rec->datatype = dtype;
  rec->rectypeid = rectypeid;
  rec->firstfield = -1;
  rec->erh = NULL;
  plpgsql_adddatum((PLpgSQL_datum *)rec);
  if (add2namespace)
  {
    plpgsql_ns_additem(PLPGSQL_NSTYPE_REC, rec->dno, rec->refname);
  }

  return rec;
}

   
                                                                      
                                                                           
   
static PLpgSQL_row *
build_row_from_vars(PLpgSQL_variable **vars, int numvars)
{
  PLpgSQL_row *row;
  int i;

  row = palloc0(sizeof(PLpgSQL_row));
  row->dtype = PLPGSQL_DTYPE_ROW;
  row->refname = "(unnamed row)";
  row->lineno = -1;
  row->rowtupdesc = CreateTemplateTupleDesc(numvars);
  row->nfields = numvars;
  row->fieldnames = palloc(numvars * sizeof(char *));
  row->varnos = palloc(numvars * sizeof(int));

  for (i = 0; i < numvars; i++)
  {
    PLpgSQL_variable *var = vars[i];
    Oid typoid;
    int32 typmod;
    Oid typcoll;

                                                    
    Assert(!var->isconst);

    switch (var->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    case PLPGSQL_DTYPE_PROMISE:
      typoid = ((PLpgSQL_var *)var)->datatype->typoid;
      typmod = ((PLpgSQL_var *)var)->datatype->atttypmod;
      typcoll = ((PLpgSQL_var *)var)->datatype->collation;
      break;

    case PLPGSQL_DTYPE_REC:
                                                             
      typoid = ((PLpgSQL_rec *)var)->rectypeid;
      typmod = -1;                                                      
      typcoll = InvalidOid;                                        
      break;

    default:
      elog(ERROR, "unrecognized dtype: %d", var->dtype);
      typoid = InvalidOid;                          
      typmod = 0;
      typcoll = InvalidOid;
      break;
    }

    row->fieldnames[i] = var->refname;
    row->varnos[i] = var->dno;

    TupleDescInitEntry(row->rowtupdesc, i + 1, var->refname, typoid, typmod, 0);
    TupleDescInitEntryCollation(row->rowtupdesc, i + 1, typcoll);
  }

  return row;
}

   
                                                                               
   
                                                                              
   
PLpgSQL_recfield *
plpgsql_build_recfield(PLpgSQL_rec *rec, const char *fldname)
{
  PLpgSQL_recfield *recfield;
  int i;

                                                           
  i = rec->firstfield;
  while (i >= 0)
  {
    PLpgSQL_recfield *fld = (PLpgSQL_recfield *)plpgsql_Datums[i];

    Assert(fld->dtype == PLPGSQL_DTYPE_RECFIELD && fld->recparentno == rec->dno);
    if (strcmp(fld->fieldname, fldname) == 0)
    {
      return fld;
    }
    i = fld->nextfield;
  }

                               
  recfield = palloc0(sizeof(PLpgSQL_recfield));
  recfield->dtype = PLPGSQL_DTYPE_RECFIELD;
  recfield->fieldname = pstrdup(fldname);
  recfield->recparentno = rec->dno;
  recfield->rectupledescid = INVALID_TUPLEDESC_IDENTIFIER;

  plpgsql_adddatum((PLpgSQL_datum *)recfield);

                                                  
  recfield->nextfield = rec->firstfield;
  rec->firstfield = recfield->dno;

  return recfield;
}

   
                          
                                                                 
                            
   
                                                                       
                                                                           
   
                                                                           
                                                                          
                                                                          
   
PLpgSQL_type *
plpgsql_build_datatype(Oid typeOid, int32 typmod, Oid collation, TypeName *origtypname)
{
  HeapTuple typeTup;
  PLpgSQL_type *typ;

  typeTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeOid));
  if (!HeapTupleIsValid(typeTup))
  {
    elog(ERROR, "cache lookup failed for type %u", typeOid);
  }

  typ = build_datatype(typeTup, typmod, collation, origtypname);

  ReleaseSysCache(typeTup);

  return typ;
}

   
                                                                          
                                                                     
   
static PLpgSQL_type *
build_datatype(HeapTuple typeTup, int32 typmod, Oid collation, TypeName *origtypname)
{
  Form_pg_type typeStruct = (Form_pg_type)GETSTRUCT(typeTup);
  PLpgSQL_type *typ;

  if (!typeStruct->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" is only a shell", NameStr(typeStruct->typname))));
  }

  typ = (PLpgSQL_type *)palloc(sizeof(PLpgSQL_type));

  typ->typname = pstrdup(NameStr(typeStruct->typname));
  typ->typoid = typeStruct->oid;
  switch (typeStruct->typtype)
  {
  case TYPTYPE_BASE:
  case TYPTYPE_ENUM:
  case TYPTYPE_RANGE:
    typ->ttype = PLPGSQL_TTYPE_SCALAR;
    break;
  case TYPTYPE_COMPOSITE:
    typ->ttype = PLPGSQL_TTYPE_REC;
    break;
  case TYPTYPE_DOMAIN:
    if (type_is_rowtype(typeStruct->typbasetype))
    {
      typ->ttype = PLPGSQL_TTYPE_REC;
    }
    else
    {
      typ->ttype = PLPGSQL_TTYPE_SCALAR;
    }
    break;
  case TYPTYPE_PSEUDO:
    if (typ->typoid == RECORDOID)
    {
      typ->ttype = PLPGSQL_TTYPE_REC;
    }
    else
    {
      typ->ttype = PLPGSQL_TTYPE_PSEUDO;
    }
    break;
  default:
    elog(ERROR, "unrecognized typtype: %d", (int)typeStruct->typtype);
    break;
  }
  typ->typlen = typeStruct->typlen;
  typ->typbyval = typeStruct->typbyval;
  typ->typtype = typeStruct->typtype;
  typ->collation = typeStruct->typcollation;
  if (OidIsValid(collation) && OidIsValid(typ->collation))
  {
    typ->collation = collation;
  }
                                                       
                                                                     
  if (typeStruct->typtype == TYPTYPE_BASE)
  {
       
                                                                         
                                                                           
       
    typ->typisarray = (typeStruct->typlen == -1 && OidIsValid(typeStruct->typelem) && typeStruct->typstorage != 'p');
  }
  else if (typeStruct->typtype == TYPTYPE_DOMAIN)
  {
                                                                        
    typ->typisarray = (typeStruct->typlen == -1 && typeStruct->typstorage != 'p' && OidIsValid(get_base_element_type(typeStruct->typbasetype)));
  }
  else
  {
    typ->typisarray = false;
  }
  typ->atttypmod = typmod;

     
                                                                            
                                                                       
                                                                           
                                                                         
     
  if (typ->ttype == PLPGSQL_TTYPE_REC && typ->typoid != RECORDOID)
  {
    TypeCacheEntry *typentry;

    typentry = lookup_type_cache(typ->typoid, TYPECACHE_TUPDESC | TYPECACHE_DOMAIN_BASE_INFO);
    if (typentry->typtype == TYPTYPE_DOMAIN)
    {
      typentry = lookup_type_cache(typentry->domainBaseType, TYPECACHE_TUPDESC);
    }
    if (typentry->tupDesc == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(typ->typoid))));
    }

    typ->origtypname = origtypname;
    typ->tcache = typentry;
    typ->tupdesc_id = typentry->tupDesc_identifier;
  }
  else
  {
    typ->origtypname = NULL;
    typ->tcache = NULL;
    typ->tupdesc_id = 0;
  }

  return typ;
}

   
                                   
                                                       
   
                                                                         
                                                                 
   
int
plpgsql_recognize_err_condition(const char *condname, bool allow_sqlstate)
{
  int i;

  if (allow_sqlstate)
  {
    if (strlen(condname) == 5 && strspn(condname, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") == 5)
    {
      return MAKE_SQLSTATE(condname[0], condname[1], condname[2], condname[3], condname[4]);
    }
  }

  for (i = 0; exception_label_map[i].label != NULL; i++)
  {
    if (strcmp(condname, exception_label_map[i].label) == 0)
    {
      return exception_label_map[i].sqlerrstate;
    }
  }

  ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized exception condition \"%s\"", condname)));
  return 0;                          
}

   
                               
                                                                        
   
                                                                         
                                           
   
PLpgSQL_condition *
plpgsql_parse_err_condition(char *condname)
{
  int i;
  PLpgSQL_condition *new;
  PLpgSQL_condition *prev;

     
                                                                          
           
     

     
                                                                         
                                                                
     
  if (strcmp(condname, "others") == 0)
  {
    new = palloc(sizeof(PLpgSQL_condition));
    new->sqlerrstate = 0;
    new->condname = condname;
    new->next = NULL;
    return new;
  }

  prev = NULL;
  for (i = 0; exception_label_map[i].label != NULL; i++)
  {
    if (strcmp(condname, exception_label_map[i].label) == 0)
    {
      new = palloc(sizeof(PLpgSQL_condition));
      new->sqlerrstate = exception_label_map[i].sqlerrstate;
      new->condname = condname;
      new->next = prev;
      prev = new;
    }
  }

  if (!prev)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized exception condition \"%s\"", condname)));
  }

  return prev;
}

              
                                                                    
              
   
static void
plpgsql_start_datums(void)
{
  datums_alloc = 128;
  plpgsql_nDatums = 0;
                                                                  
  plpgsql_Datums = MemoryContextAlloc(plpgsql_compile_tmp_cxt, sizeof(PLpgSQL_datum *) * datums_alloc);
                                                                       
  datums_last = 0;
}

              
                                                    
                                     
              
   
void
plpgsql_adddatum(PLpgSQL_datum *newdatum)
{
  if (plpgsql_nDatums == datums_alloc)
  {
    datums_alloc *= 2;
    plpgsql_Datums = repalloc(plpgsql_Datums, sizeof(PLpgSQL_datum *) * datums_alloc);
  }

  newdatum->dno = plpgsql_nDatums;
  plpgsql_Datums[plpgsql_nDatums++] = newdatum;
}

              
                                                                         
              
   
static void
plpgsql_finish_datums(PLpgSQL_function *function)
{
  Size copiable_size = 0;
  int i;

  function->ndatums = plpgsql_nDatums;
  function->datums = palloc(sizeof(PLpgSQL_datum *) * plpgsql_nDatums);
  for (i = 0; i < plpgsql_nDatums; i++)
  {
    function->datums[i] = plpgsql_Datums[i];

                                                                      
    switch (function->datums[i]->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    case PLPGSQL_DTYPE_PROMISE:
      copiable_size += MAXALIGN(sizeof(PLpgSQL_var));
      break;
    case PLPGSQL_DTYPE_REC:
      copiable_size += MAXALIGN(sizeof(PLpgSQL_rec));
      break;
    default:
      break;
    }
  }
  function->copiable_size = copiable_size;
}

              
                                                                 
                                                                
                         
   
                                                                         
              
   
                                                                        
                                                                          
                                                                       
                                                                  
              
   
int
plpgsql_add_initdatums(int **varnos)
{
  int i;
  int n = 0;

     
                                                                         
                                                  
     
  for (i = datums_last; i < plpgsql_nDatums; i++)
  {
    switch (plpgsql_Datums[i]->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    case PLPGSQL_DTYPE_REC:
      n++;
      break;

    default:
      break;
    }
  }

  if (varnos != NULL)
  {
    if (n > 0)
    {
      *varnos = (int *)palloc(sizeof(int) * n);

      n = 0;
      for (i = datums_last; i < plpgsql_nDatums; i++)
      {
        switch (plpgsql_Datums[i]->dtype)
        {
        case PLPGSQL_DTYPE_VAR:
        case PLPGSQL_DTYPE_REC:
          (*varnos)[n++] = plpgsql_Datums[i]->dno;

        default:
          break;
        }
      }
    }
    else
    {
      *varnos = NULL;
    }
  }

  datums_last = plpgsql_nDatums;
  return n;
}

   
                                                       
   
                                                                         
   
static void
compute_function_hashkey(FunctionCallInfo fcinfo, Form_pg_proc procStruct, PLpgSQL_func_hashkey *hashkey, bool forValidator)
{
                                                         
  MemSet(hashkey, 0, sizeof(PLpgSQL_func_hashkey));

                        
  hashkey->funcOid = fcinfo->flinfo->fn_oid;

                        
  hashkey->isTrigger = CALLED_AS_TRIGGER(fcinfo);
  hashkey->isEventTrigger = CALLED_AS_EVENT_TRIGGER(fcinfo);

     
                                                                             
                                                                             
                                                                            
                                                                            
                                                                         
                                
     
                                                                           
                                                                          
                        
     
  if (hashkey->isTrigger && !forValidator)
  {
    TriggerData *trigdata = (TriggerData *)fcinfo->context;

    hashkey->trigOid = trigdata->tg_trigger->tgoid;
  }

                                     
  hashkey->inputCollation = fcinfo->fncollation;

  if (procStruct->pronargs > 0)
  {
                                
    memcpy(hashkey->argtypes, procStruct->proargtypes.values, procStruct->pronargs * sizeof(Oid));

                                                
    plpgsql_resolve_polymorphic_argtypes(procStruct->pronargs, hashkey->argtypes, NULL, fcinfo->flinfo->fn_expr, forValidator, NameStr(procStruct->proname));
  }
}

   
                                                                             
                
                                                                      
                                                                        
                                                                         
                                                                            
                                                                           
                
                                                                       
                                                                         
   
static void
plpgsql_resolve_polymorphic_argtypes(int numargs, Oid *argtypes, char *argmodes, Node *call_expr, bool forValidator, const char *proname)
{
  int i;

  if (!forValidator)
  {
    int inargno;

                                               
    if (!resolve_polymorphic_argtypes(numargs, argtypes, argmodes, call_expr))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not determine actual argument "
                                                                     "type for polymorphic function \"%s\"",
                                                                  proname)));
    }
                                                                    
    inargno = 0;
    for (i = 0; i < numargs; i++)
    {
      char argmode = argmodes ? argmodes[i] : PROARGMODE_IN;

      if (argmode == PROARGMODE_OUT || argmode == PROARGMODE_TABLE)
      {
        continue;
      }
      if (argtypes[i] == RECORDOID || argtypes[i] == RECORDARRAYOID)
      {
        Oid resolvedtype = get_call_expr_argtype(call_expr, inargno);

        if (OidIsValid(resolvedtype))
        {
          argtypes[i] = resolvedtype;
        }
      }
      inargno++;
    }
  }
  else
  {
                                                                     
    for (i = 0; i < numargs; i++)
    {
      switch (argtypes[i])
      {
      case ANYELEMENTOID:
      case ANYNONARRAYOID:
      case ANYENUMOID:                  
        argtypes[i] = INT4OID;
        break;
      case ANYARRAYOID:
        argtypes[i] = INT4ARRAYOID;
        break;
      case ANYRANGEOID:
        argtypes[i] = INT4RANGEOID;
        break;
      default:
        break;
      }
    }
  }
}

   
                                                                            
   
                                                                       
                                                                       
                                                                       
                                                                    
                                                                         
                                                            
   
                                                                              
                                                                           
          
   
static void
delete_function(PLpgSQL_function *func)
{
                                                               
  plpgsql_HashTableDelete(func);

                                                                   
  if (func->use_count == 0)
  {
    plpgsql_free_function_memory(func);
  }
}

                                                    
void
plpgsql_HashTableInit(void)
{
  HASHCTL ctl;

                                         
  Assert(plpgsql_HashTable == NULL);

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(PLpgSQL_func_hashkey);
  ctl.entrysize = sizeof(plpgsql_HashEnt);
  plpgsql_HashTable = hash_create("PLpgSQL function hash", FUNCS_PER_USER, &ctl, HASH_ELEM | HASH_BLOBS);
}

static PLpgSQL_function *
plpgsql_HashTableLookup(PLpgSQL_func_hashkey *func_key)
{
  plpgsql_HashEnt *hentry;

  hentry = (plpgsql_HashEnt *)hash_search(plpgsql_HashTable, (void *)func_key, HASH_FIND, NULL);
  if (hentry)
  {
    return hentry->function;
  }
  else
  {
    return NULL;
  }
}

static void
plpgsql_HashTableInsert(PLpgSQL_function *function, PLpgSQL_func_hashkey *func_key)
{
  plpgsql_HashEnt *hentry;
  bool found;

  hentry = (plpgsql_HashEnt *)hash_search(plpgsql_HashTable, (void *)func_key, HASH_ENTER, &found);
  if (found)
  {
    elog(WARNING, "trying to insert a function that already exists");
  }

  hentry->function = function;
                                                        
  function->fn_hashkey = &hentry->key;
}

static void
plpgsql_HashTableDelete(PLpgSQL_function *function)
{
  plpgsql_HashEnt *hentry;

                                  
  if (function->fn_hashkey == NULL)
  {
    return;
  }

  hentry = (plpgsql_HashEnt *)hash_search(plpgsql_HashTable, (void *)function->fn_hashkey, HASH_REMOVE, NULL);
  if (hentry == NULL)
  {
    elog(WARNING, "trying to delete function that does not exist");
  }

                                                                     
  function->fn_hashkey = NULL;
}
