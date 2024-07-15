                                                                            
   
                                          
                           
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/transam.h"
#include "access/tupconvert.h"
#include "access/tuptoaster.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "executor/execExpr.h"
#include "executor/spi.h"
#include "executor/spi_priv.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_type.h"
#include "parser/scansup.h"
#include "storage/proc.h"
#include "tcop/pquery.h"
#include "tcop/tcopprot.h"
#include "tcop/utility.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

#include "plpgsql.h"

typedef struct
{
  int nargs;                              
  Oid *types;                            
  Datum *values;                                
  char *nulls;                                     
} PreparedParamsData;

   
                                                                              
                                                                            
                                                                      
                                                                              
                                                                           
                                                                           
                                                                           
                                                                            
                                                                         
                                                                           
   
                                                                             
                                                                               
                                                                         
                                   
   
                                                                           
                                                                             
                                                                           
                                                                            
                                                                               
                                                                          
                                                                             
                                                               
   
                                                                              
                                                                               
                                                                               
                                                                               
                                                                             
                                                            
   
typedef struct SimpleEcontextStackEntry
{
  ExprContext *stack_econtext;                                   
  SubTransactionId xact_subxid;                                      
  struct SimpleEcontextStackEntry *next;                          
} SimpleEcontextStackEntry;

static EState *shared_simple_eval_estate = NULL;
static SimpleEcontextStackEntry *simple_econtext_stack = NULL;

   
                                                                          
             
   
                                                                           
                                                                              
                                                                              
                                                                
                                                                 
   
                                                                           
                                                                            
                                                                             
                                                                        
                                                                                
   
                                                                         
                                                                               
                                                                        
                                                                         
   
                                                               
   
#define get_eval_mcontext(estate) ((estate)->eval_econtext->ecxt_per_tuple_memory)
#define eval_mcontext_alloc(estate, sz) MemoryContextAlloc(get_eval_mcontext(estate), sz)
#define eval_mcontext_alloc0(estate, sz) MemoryContextAllocZero(get_eval_mcontext(estate), sz)

   
                                                                  
   
                                                                            
                                                                          
                                                                    
   
                                                                              
                                                                            
   
                                                                             
                                                                          
                                                   
   
typedef struct                               
{
                                                           
  Oid srctype;                               
  Oid dsttype;                                    
  int32 srctypmod;                             
  int32 dsttypmod;                                  
} plpgsql_CastHashKey;

typedef struct                            
{
  plpgsql_CastHashKey key;                                      
  Expr *cast_expr;                                                          
  CachedExpression *cast_cexpr;                                          
                                                                   
  ExprState *cast_exprstate;                             
  bool cast_in_use;                                                    
  LocalTransactionId cast_lxid;
} plpgsql_CastHashEntry;

static MemoryContext shared_cast_context = NULL;
static HTAB *shared_cast_hash = NULL;

   
                                                                          
                                                                             
                                    
   
                            
              
      
         
                                          
                                             
         
      
               
   
                                                                              
                                                                             
                                                                             
                                                                          
   
                                                      
                                                       
   
#define LOOP_RC_PROCESSING(looplabel, exit_action)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  if (rc == PLPGSQL_RC_RETURN)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    exit_action;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else if (rc == PLPGSQL_RC_EXIT)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (estate->exitlabel == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      rc = PLPGSQL_RC_OK;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      exit_action;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else if ((looplabel) != NULL && strcmp(looplabel, estate->exitlabel) == 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      estate->exitlabel = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      rc = PLPGSQL_RC_OK;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      exit_action;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      exit_action;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else if (rc == PLPGSQL_RC_CONTINUE)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (estate->exitlabel == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      rc = PLPGSQL_RC_OK;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else if ((looplabel) != NULL && strcmp(looplabel, estate->exitlabel) == 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      estate->exitlabel = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      rc = PLPGSQL_RC_OK;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      exit_action;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    Assert(rc == PLPGSQL_RC_OK)

                                                              
                                       
                                                              
static void
coerce_function_result_tuple(PLpgSQL_execstate *estate, TupleDesc tupdesc);
static void
plpgsql_exec_error_callback(void *arg);
static void
copy_plpgsql_datums(PLpgSQL_execstate *estate, PLpgSQL_function *func);
static void
plpgsql_fulfill_promise(PLpgSQL_execstate *estate, PLpgSQL_var *var);
static MemoryContext
get_stmt_mcontext(PLpgSQL_execstate *estate);
static void
push_stmt_mcontext(PLpgSQL_execstate *estate);
static void
pop_stmt_mcontext(PLpgSQL_execstate *estate);

static int
exec_stmt_block(PLpgSQL_execstate *estate, PLpgSQL_stmt_block *block);
static int
exec_stmts(PLpgSQL_execstate *estate, List *stmts);
static int
exec_stmt(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt);
static int
exec_stmt_assign(PLpgSQL_execstate *estate, PLpgSQL_stmt_assign *stmt);
static int
exec_stmt_perform(PLpgSQL_execstate *estate, PLpgSQL_stmt_perform *stmt);
static int
exec_stmt_call(PLpgSQL_execstate *estate, PLpgSQL_stmt_call *stmt);
static int
exec_stmt_getdiag(PLpgSQL_execstate *estate, PLpgSQL_stmt_getdiag *stmt);
static int
exec_stmt_if(PLpgSQL_execstate *estate, PLpgSQL_stmt_if *stmt);
static int
exec_stmt_case(PLpgSQL_execstate *estate, PLpgSQL_stmt_case *stmt);
static int
exec_stmt_loop(PLpgSQL_execstate *estate, PLpgSQL_stmt_loop *stmt);
static int
exec_stmt_while(PLpgSQL_execstate *estate, PLpgSQL_stmt_while *stmt);
static int
exec_stmt_fori(PLpgSQL_execstate *estate, PLpgSQL_stmt_fori *stmt);
static int
exec_stmt_fors(PLpgSQL_execstate *estate, PLpgSQL_stmt_fors *stmt);
static int
exec_stmt_forc(PLpgSQL_execstate *estate, PLpgSQL_stmt_forc *stmt);
static int
exec_stmt_foreach_a(PLpgSQL_execstate *estate, PLpgSQL_stmt_foreach_a *stmt);
static int
exec_stmt_open(PLpgSQL_execstate *estate, PLpgSQL_stmt_open *stmt);
static int
exec_stmt_fetch(PLpgSQL_execstate *estate, PLpgSQL_stmt_fetch *stmt);
static int
exec_stmt_close(PLpgSQL_execstate *estate, PLpgSQL_stmt_close *stmt);
static int
exec_stmt_exit(PLpgSQL_execstate *estate, PLpgSQL_stmt_exit *stmt);
static int
exec_stmt_return(PLpgSQL_execstate *estate, PLpgSQL_stmt_return *stmt);
static int
exec_stmt_return_next(PLpgSQL_execstate *estate, PLpgSQL_stmt_return_next *stmt);
static int
exec_stmt_return_query(PLpgSQL_execstate *estate, PLpgSQL_stmt_return_query *stmt);
static int
exec_stmt_raise(PLpgSQL_execstate *estate, PLpgSQL_stmt_raise *stmt);
static int
exec_stmt_assert(PLpgSQL_execstate *estate, PLpgSQL_stmt_assert *stmt);
static int
exec_stmt_execsql(PLpgSQL_execstate *estate, PLpgSQL_stmt_execsql *stmt);
static int
exec_stmt_dynexecute(PLpgSQL_execstate *estate, PLpgSQL_stmt_dynexecute *stmt);
static int
exec_stmt_dynfors(PLpgSQL_execstate *estate, PLpgSQL_stmt_dynfors *stmt);
static int
exec_stmt_commit(PLpgSQL_execstate *estate, PLpgSQL_stmt_commit *stmt);
static int
exec_stmt_rollback(PLpgSQL_execstate *estate, PLpgSQL_stmt_rollback *stmt);
static int
exec_stmt_set(PLpgSQL_execstate *estate, PLpgSQL_stmt_set *stmt);

static void
plpgsql_estate_setup(PLpgSQL_execstate *estate, PLpgSQL_function *func, ReturnSetInfo *rsi, EState *simple_eval_estate);
static void
exec_eval_cleanup(PLpgSQL_execstate *estate);

static void
exec_prepare_plan(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, int cursorOptions, bool keepplan);
static void
exec_simple_check_plan(PLpgSQL_execstate *estate, PLpgSQL_expr *expr);
static void
exec_save_simple_expr(PLpgSQL_expr *expr, CachedPlan *cplan);
static void
exec_check_rw_parameter(PLpgSQL_expr *expr, int target_dno);
static bool
contains_target_param(Node *node, int *target_dno);
static bool
exec_eval_simple_expr(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, Datum *result, bool *isNull, Oid *rettype, int32 *rettypmod);

static void
exec_assign_expr(PLpgSQL_execstate *estate, PLpgSQL_datum *target, PLpgSQL_expr *expr);
static void
exec_assign_c_string(PLpgSQL_execstate *estate, PLpgSQL_datum *target, const char *str);
static void
exec_assign_value(PLpgSQL_execstate *estate, PLpgSQL_datum *target, Datum value, bool isNull, Oid valtype, int32 valtypmod);
static void
exec_eval_datum(PLpgSQL_execstate *estate, PLpgSQL_datum *datum, Oid *typeid, int32 *typetypmod, Datum *value, bool *isnull);
static int
exec_eval_integer(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, bool *isNull);
static bool
exec_eval_boolean(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, bool *isNull);
static Datum
exec_eval_expr(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, bool *isNull, Oid *rettype, int32 *rettypmod);
static int
exec_run_select(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, long maxtuples, Portal *portalP);
static int
exec_for_query(PLpgSQL_execstate *estate, PLpgSQL_stmt_forq *stmt, Portal portal, bool prefetch_ok);
static ParamListInfo
setup_param_list(PLpgSQL_execstate *estate, PLpgSQL_expr *expr);
static ParamExternData *
plpgsql_param_fetch(ParamListInfo params, int paramid, bool speculative, ParamExternData *workspace);
static void
plpgsql_param_compile(ParamListInfo params, Param *param, ExprState *state, Datum *resv, bool *resnull);
static void
plpgsql_param_eval_var(ExprState *state, ExprEvalStep *op, ExprContext *econtext);
static void
plpgsql_param_eval_var_ro(ExprState *state, ExprEvalStep *op, ExprContext *econtext);
static void
plpgsql_param_eval_recfield(ExprState *state, ExprEvalStep *op, ExprContext *econtext);
static void
plpgsql_param_eval_generic(ExprState *state, ExprEvalStep *op, ExprContext *econtext);
static void
plpgsql_param_eval_generic_ro(ExprState *state, ExprEvalStep *op, ExprContext *econtext);
static void
exec_move_row(PLpgSQL_execstate *estate, PLpgSQL_variable *target, HeapTuple tup, TupleDesc tupdesc);
static void
revalidate_rectypeid(PLpgSQL_rec *rec);
static ExpandedRecordHeader *
make_expanded_record_for_rec(PLpgSQL_execstate *estate, PLpgSQL_rec *rec, TupleDesc srctupdesc, ExpandedRecordHeader *srcerh);
static void
exec_move_row_from_fields(PLpgSQL_execstate *estate, PLpgSQL_variable *target, ExpandedRecordHeader *newerh, Datum *values, bool *nulls, TupleDesc tupdesc);
static bool
compatible_tupdescs(TupleDesc src_tupdesc, TupleDesc dst_tupdesc);
static HeapTuple
make_tuple_from_row(PLpgSQL_execstate *estate, PLpgSQL_row *row, TupleDesc tupdesc);
static TupleDesc
deconstruct_composite_datum(Datum value, HeapTupleData *tmptup);
static void
exec_move_row_from_datum(PLpgSQL_execstate *estate, PLpgSQL_variable *target, Datum value);
static void
instantiate_empty_record_variable(PLpgSQL_execstate *estate, PLpgSQL_rec *rec);
static char *
convert_value_to_string(PLpgSQL_execstate *estate, Datum value, Oid valtype);
static Datum
exec_cast_value(PLpgSQL_execstate *estate, Datum value, bool *isnull, Oid valtype, int32 valtypmod, Oid reqtype, int32 reqtypmod);
static plpgsql_CastHashEntry *
get_cast_hashentry(PLpgSQL_execstate *estate, Oid srctype, int32 srctypmod, Oid dsttype, int32 dsttypmod);
static void
exec_init_tuple_store(PLpgSQL_execstate *estate);
static void
exec_set_found(PLpgSQL_execstate *estate, bool state);
static void
plpgsql_create_econtext(PLpgSQL_execstate *estate);
static void
plpgsql_destroy_econtext(PLpgSQL_execstate *estate);
static void
assign_simple_var(PLpgSQL_execstate *estate, PLpgSQL_var *var, Datum newvalue, bool isnull, bool freeable);
static void
assign_text_var(PLpgSQL_execstate *estate, PLpgSQL_var *var, const char *str);
static void
assign_record_var(PLpgSQL_execstate *estate, PLpgSQL_rec *rec, ExpandedRecordHeader *erh);
static PreparedParamsData *
exec_eval_using_params(PLpgSQL_execstate *estate, List *params);
static Portal
exec_dynquery_with_params(PLpgSQL_execstate *estate, PLpgSQL_expr *dynquery, List *params, const char *portalname, int cursorOptions);
static char *
format_expr_params(PLpgSQL_execstate *estate, const PLpgSQL_expr *expr);
static char *
format_preparedparamsdata(PLpgSQL_execstate *estate, const PreparedParamsData *ppd);

              
                                                        
                          
   
                                                                          
                                                                         
                                                                          
                                                                      
                                                                         
                                                                       
                                 
              
   
Datum
plpgsql_exec_function(PLpgSQL_function *func, FunctionCallInfo fcinfo, EState *simple_eval_estate, bool atomic)
{
  PLpgSQL_execstate estate;
  ErrorContextCallback plerrcontext;
  int i;
  int rc;

     
                               
     
  plpgsql_estate_setup(&estate, func, (ReturnSetInfo *)fcinfo->resultinfo, simple_eval_estate);
  estate.atomic = atomic;

     
                                                 
     
  plerrcontext.callback = plpgsql_exec_error_callback;
  plerrcontext.arg = &estate;
  plerrcontext.previous = error_context_stack;
  error_context_stack = &plerrcontext;

     
                                                   
     
  estate.err_text = gettext_noop("during initialization of execution state");
  copy_plpgsql_datums(&estate, func);

     
                                                                          
     
  estate.err_text = gettext_noop("while storing call arguments into local variables");
  for (i = 0; i < func->fn_nargs; i++)
  {
    int n = func->fn_argvarnos[i];

    switch (estate.datums[n]->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    {
      PLpgSQL_var *var = (PLpgSQL_var *)estate.datums[n];

      assign_simple_var(&estate, var, fcinfo->args[i].value, fcinfo->args[i].isnull, false);

         
                                                          
                                                          
                                                                 
                                                            
                                                        
                                                              
         
                                                                 
                                                                 
                                                                
                                                                 
                                                               
                                  
         
      if (!var->isnull && var->datatype->typisarray)
      {
        if (VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(var->value)))
        {
                                            
          assign_simple_var(&estate, var, TransferExpandedObject(var->value, estate.datum_context), false, true);
        }
        else if (VARATT_IS_EXTERNAL_EXPANDED_RO(DatumGetPointer(var->value)))
        {
                                                            
        }
        else
        {
                                                     
          assign_simple_var(&estate, var, expand_array(var->value, estate.datum_context, NULL), false, true);
        }
      }
    }
    break;

    case PLPGSQL_DTYPE_REC:
    {
      PLpgSQL_rec *rec = (PLpgSQL_rec *)estate.datums[n];

      if (!fcinfo->args[i].isnull)
      {
                                                   
        exec_move_row_from_datum(&estate, (PLpgSQL_variable *)rec, fcinfo->args[i].value);
      }
      else
      {
                                                  
        exec_move_row(&estate, (PLpgSQL_variable *)rec, NULL, NULL);
      }
                                          
      exec_eval_cleanup(&estate);
    }
    break;

    default:
                                                            
      elog(ERROR, "unrecognized dtype: %d", func->datums[i]->dtype);
    }
  }

  estate.err_text = gettext_noop("during function entry");

     
                                           
     
  exec_set_found(&estate, false);

     
                                                          
     
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->func_beg)
  {
    ((*plpgsql_plugin_ptr)->func_beg)(&estate, func);
  }

     
                                               
     
  estate.err_text = NULL;
  estate.err_stmt = (PLpgSQL_stmt *)(func->action);
  rc = exec_stmt(&estate, (PLpgSQL_stmt *)func->action);
  if (rc != PLPGSQL_RC_RETURN)
  {
    estate.err_stmt = NULL;
    estate.err_text = NULL;
    ereport(ERROR, (errcode(ERRCODE_S_R_E_FUNCTION_EXECUTED_NO_RETURN_STATEMENT), errmsg("control reached end of function without RETURN")));
  }

     
                                        
     
  estate.err_stmt = NULL;
  estate.err_text = gettext_noop("while casting return value to function's return type");

  fcinfo->isnull = estate.retisnull;

  if (estate.retisset)
  {
    ReturnSetInfo *rsi = estate.rsi;

                                              
    if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
    }
    rsi->returnMode = SFRM_Materialize;

                                                         
    if (estate.tuple_store)
    {
      MemoryContext oldcxt;

      rsi->setResult = estate.tuple_store;
      oldcxt = MemoryContextSwitchTo(estate.tuple_store_cxt);
      rsi->setDesc = CreateTupleDescCopy(estate.tuple_store_desc);
      MemoryContextSwitchTo(oldcxt);
    }
    estate.retval = (Datum)0;
    fcinfo->isnull = true;
  }
  else if (!estate.retisnull)
  {
       
                                                                         
                                                                      
                                                                   
                                  
       
    if (estate.retistuple)
    {
                                                            
      if (func->fn_rettype == estate.rettype && func->fn_rettype != RECORDOID)
      {
           
                                                                     
                                                                 
                                                            
           
        estate.retval = SPI_datumTransfer(estate.retval, false, -1);
      }
      else
      {
           
                                                                   
                                                            
                                                                      
                                                             
                                                               
                                            
           
        Oid resultTypeId;
        TupleDesc tupdesc;

        switch (get_call_result_type(fcinfo, &resultTypeId, &tupdesc))
        {
        case TYPEFUNC_COMPOSITE:
                                                              
          coerce_function_result_tuple(&estate, tupdesc);
          break;
        case TYPEFUNC_COMPOSITE_DOMAIN:
                                                              
          coerce_function_result_tuple(&estate, tupdesc);
                                            
                                                            
          domain_check(estate.retval, false, resultTypeId, NULL, NULL);
          break;
        case TYPEFUNC_RECORD:

             
                                                            
                                                               
                                                              
                                                            
                                                                 
             
          estate.retval = SPI_datumTransfer(estate.retval, false, -1);
          break;
        default:
                                                            
          elog(ERROR, "return type must be a row type");
          break;
        }
      }
    }
    else
    {
                                            
      estate.retval = exec_cast_value(&estate, estate.retval, &fcinfo->isnull, estate.rettype, -1, func->fn_rettype, -1);

         
                                                                      
                                                                        
                                                                       
                                 
         
      if (!fcinfo->isnull && !func->fn_retbyval)
      {
        estate.retval = SPI_datumTransfer(estate.retval, false, func->fn_rettyplen);
      }
    }
  }
  else
  {
       
                                                                          
                                                                       
                                                                          
       
    if (func->fn_retisdomain)
    {
      estate.retval = exec_cast_value(&estate, estate.retval, &fcinfo->isnull, estate.rettype, -1, func->fn_rettype, -1);
    }
  }

  estate.err_text = gettext_noop("during function exit");

     
                                                          
     
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->func_end)
  {
    ((*plpgsql_plugin_ptr)->func_end)(&estate, func);
  }

                                              
  plpgsql_destroy_econtext(&estate);
  exec_eval_cleanup(&estate);
                                                                       

     
                                 
     
  error_context_stack = plerrcontext.previous;

     
                                  
     
  return estate.retval;
}

   
                                                                              
                                                                              
                                                                             
              
   
                                       
   
static void
coerce_function_result_tuple(PLpgSQL_execstate *estate, TupleDesc tupdesc)
{
  HeapTuple rettup;
  TupleDesc retdesc;
  TupleConversionMap *tupmap;

                                                                    
  Assert(type_is_rowtype(estate->rettype));

                                                      
  if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(estate->retval)))
  {
    ExpandedRecordHeader *erh = (ExpandedRecordHeader *)DatumGetEOHP(estate->retval);

    Assert(erh->er_magic == ER_MAGIC);

                                    
    retdesc = expanded_record_get_tupdesc(erh);

                                     
    tupmap = convert_tuples_by_position(retdesc, tupdesc, gettext_noop("returned record type does not match expected record type"));

                                  
    if (tupmap)
    {
      rettup = expanded_record_get_tuple(erh);
      Assert(rettup);
      rettup = execute_attr_map_tuple(rettup, tupmap);

         
                                                                      
                                                                 
         
      estate->retval = PointerGetDatum(SPI_returntuple(rettup, tupdesc));
                                                             
    }
    else if (!(tupdesc->tdtypeid == erh->er_decltypeid || (tupdesc->tdtypeid == RECORDOID && !ExpandedRecordIsDomain(erh))))
    {
         
                                                                     
                                                                      
                                                                        
                                                                    
                                                                    
                                                                      
                                                                        
                                                                   
                                                 
         
      Size resultsize;
      HeapTupleHeader tuphdr;

      resultsize = EOH_get_flat_size(&erh->hdr);
      tuphdr = (HeapTupleHeader)SPI_palloc(resultsize);
      EOH_flatten_into(&erh->hdr, (void *)tuphdr, resultsize);
      HeapTupleHeaderSetTypeId(tuphdr, tupdesc->tdtypeid);
      HeapTupleHeaderSetTypMod(tuphdr, tupdesc->tdtypmod);
      estate->retval = PointerGetDatum(tuphdr);
    }
    else
    {
         
                                                                      
                                                                        
                                                          
         
      estate->retval = SPI_datumTransfer(estate->retval, false, -1);
    }
  }
  else
  {
                                                              
    HeapTupleData tmptup;

    retdesc = deconstruct_composite_datum(estate->retval, &tmptup);
    rettup = &tmptup;

                                     
    tupmap = convert_tuples_by_position(retdesc, tupdesc, gettext_noop("returned record type does not match expected record type"));

                                  
    if (tupmap)
    {
      rettup = execute_attr_map_tuple(rettup, tupmap);
    }

       
                                                                         
                                                          
       
    estate->retval = PointerGetDatum(SPI_returntuple(rettup, tupdesc));

                                                           

    ReleaseTupleDesc(retdesc);
  }
}

              
                                                        
                         
              
   
HeapTuple
plpgsql_exec_trigger(PLpgSQL_function *func, TriggerData *trigdata)
{
  PLpgSQL_execstate estate;
  ErrorContextCallback plerrcontext;
  int rc;
  TupleDesc tupdesc;
  PLpgSQL_rec *rec_new, *rec_old;
  HeapTuple rettup;

     
                               
     
  plpgsql_estate_setup(&estate, func, NULL, NULL);
  estate.trigdata = trigdata;

     
                                                 
     
  plerrcontext.callback = plpgsql_exec_error_callback;
  plerrcontext.arg = &estate;
  plerrcontext.previous = error_context_stack;
  error_context_stack = &plerrcontext;

     
                                                   
     
  estate.err_text = gettext_noop("during initialization of execution state");
  copy_plpgsql_datums(&estate, func);

     
                                                      
     
                                                                            
                                                                          
                                                                             
                                                                         
                                                                          
                                                             
     
  tupdesc = RelationGetDescr(trigdata->tg_relation);

  rec_new = (PLpgSQL_rec *)(estate.datums[func->new_varno]);
  rec_old = (PLpgSQL_rec *)(estate.datums[func->old_varno]);

  rec_new->erh = make_expanded_record_from_tupdesc(tupdesc, estate.datum_context);
  rec_old->erh = make_expanded_record_from_exprecord(rec_new->erh, estate.datum_context);

  if (!TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
  {
       
                                                          
       
  }
  else if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
  {
    expanded_record_set_tuple(rec_new->erh, trigdata->tg_trigtuple, false, false);
  }
  else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
  {
    expanded_record_set_tuple(rec_new->erh, trigdata->tg_newtuple, false, false);
    expanded_record_set_tuple(rec_old->erh, trigdata->tg_trigtuple, false, false);

       
                                                                         
                                                                         
                                                                           
                                                                           
                                                                           
                                                            
       
    if (tupdesc->constr && tupdesc->constr->has_generated_stored && TRIGGER_FIRED_BEFORE(trigdata->tg_event))
    {
      for (int i = 0; i < tupdesc->natts; i++)
      {
        if (TupleDescAttr(tupdesc, i)->attgenerated == ATTRIBUTE_GENERATED_STORED)
        {
          expanded_record_set_field_internal(rec_new->erh, i + 1, (Datum)0, true,             
              false, false);
        }
      }
    }
  }
  else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
  {
    expanded_record_set_tuple(rec_old->erh, trigdata->tg_trigtuple, false, false);
  }
  else
  {
    elog(ERROR, "unrecognized trigger action: not INSERT, DELETE, or UPDATE");
  }

                                                             
  rc = SPI_register_trigger_data(trigdata);
  Assert(rc >= 0);

  estate.err_text = gettext_noop("during function entry");

     
                                           
     
  exec_set_found(&estate, false);

     
                                                          
     
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->func_beg)
  {
    ((*plpgsql_plugin_ptr)->func_beg)(&estate, func);
  }

     
                                               
     
  estate.err_text = NULL;
  estate.err_stmt = (PLpgSQL_stmt *)(func->action);
  rc = exec_stmt(&estate, (PLpgSQL_stmt *)func->action);
  if (rc != PLPGSQL_RC_RETURN)
  {
    estate.err_stmt = NULL;
    estate.err_text = NULL;
    ereport(ERROR, (errcode(ERRCODE_S_R_E_FUNCTION_EXECUTED_NO_RETURN_STATEMENT), errmsg("control reached end of trigger procedure without RETURN")));
  }

  estate.err_stmt = NULL;
  estate.err_text = gettext_noop("during function exit");

  if (estate.retisset)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("trigger procedure cannot return a set")));
  }

     
                                                                          
                                                                         
                                                                             
                                          
     
                                                                         
                                                                          
                                                                  
     
  if (estate.retisnull || !TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
  {
    rettup = NULL;
  }
  else
  {
    TupleDesc retdesc;
    TupleConversionMap *tupmap;

                                                                      
    Assert(type_is_rowtype(estate.rettype));

                                                        
    if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(estate.retval)))
    {
      ExpandedRecordHeader *erh = (ExpandedRecordHeader *)DatumGetEOHP(estate.retval);

      Assert(erh->er_magic == ER_MAGIC);

                                           
      rettup = expanded_record_get_tuple(erh);
      Assert(rettup);
      retdesc = expanded_record_get_tupdesc(erh);

      if (retdesc != RelationGetDescr(trigdata->tg_relation))
      {
                                         
        tupmap = convert_tuples_by_position(retdesc, RelationGetDescr(trigdata->tg_relation), gettext_noop("returned row structure does not match the structure of the triggering table"));
                                      
        if (tupmap)
        {
          rettup = execute_attr_map_tuple(rettup, tupmap);
        }
                                                               
      }

         
                                                                    
                                                                         
                                                                       
                                                          
         
      if (rettup != trigdata->tg_newtuple && rettup != trigdata->tg_trigtuple)
      {
        rettup = SPI_copytuple(rettup);
      }
    }
    else
    {
                                                                
      HeapTupleData tmptup;

      retdesc = deconstruct_composite_datum(estate.retval, &tmptup);
      rettup = &tmptup;

                                       
      tupmap = convert_tuples_by_position(retdesc, RelationGetDescr(trigdata->tg_relation), gettext_noop("returned row structure does not match the structure of the triggering table"));
                                    
      if (tupmap)
      {
        rettup = execute_attr_map_tuple(rettup, tupmap);
      }

      ReleaseTupleDesc(retdesc);
                                                             

                                               
      rettup = SPI_copytuple(rettup);
    }
  }

     
                                                          
     
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->func_end)
  {
    ((*plpgsql_plugin_ptr)->func_end)(&estate, func);
  }

                                              
  plpgsql_destroy_econtext(&estate);
  exec_eval_cleanup(&estate);
                                                                       

     
                                 
     
  error_context_stack = plerrcontext.previous;

     
                                 
     
  return rettup;
}

              
                                                              
                               
              
   
void
plpgsql_exec_event_trigger(PLpgSQL_function *func, EventTriggerData *trigdata)
{
  PLpgSQL_execstate estate;
  ErrorContextCallback plerrcontext;
  int rc;

     
                               
     
  plpgsql_estate_setup(&estate, func, NULL, NULL);
  estate.evtrigdata = trigdata;

     
                                                 
     
  plerrcontext.callback = plpgsql_exec_error_callback;
  plerrcontext.arg = &estate;
  plerrcontext.previous = error_context_stack;
  error_context_stack = &plerrcontext;

     
                                                   
     
  estate.err_text = gettext_noop("during initialization of execution state");
  copy_plpgsql_datums(&estate, func);

     
                                                          
     
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->func_beg)
  {
    ((*plpgsql_plugin_ptr)->func_beg)(&estate, func);
  }

     
                                               
     
  estate.err_text = NULL;
  estate.err_stmt = (PLpgSQL_stmt *)(func->action);
  rc = exec_stmt(&estate, (PLpgSQL_stmt *)func->action);
  if (rc != PLPGSQL_RC_RETURN)
  {
    estate.err_stmt = NULL;
    estate.err_text = NULL;
    ereport(ERROR, (errcode(ERRCODE_S_R_E_FUNCTION_EXECUTED_NO_RETURN_STATEMENT), errmsg("control reached end of trigger procedure without RETURN")));
  }

  estate.err_stmt = NULL;
  estate.err_text = gettext_noop("during function exit");

     
                                                          
     
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->func_end)
  {
    ((*plpgsql_plugin_ptr)->func_end)(&estate, func);
  }

                                              
  plpgsql_destroy_econtext(&estate);
  exec_eval_cleanup(&estate);
                                                                       

     
                                 
     
  error_context_stack = plerrcontext.previous;

  return;
}

   
                                                                  
   
static void
plpgsql_exec_error_callback(void *arg)
{
  PLpgSQL_execstate *estate = (PLpgSQL_execstate *)arg;

  if (estate->err_text != NULL)
  {
       
                                                                         
                                                                        
                                                                    
                           
       
                                                                  
                                                                          
                                                                        
                                           
       
    if (estate->err_stmt != NULL)
    {
         
                                                                         
                                        
         
      errcontext("PL/pgSQL function %s line %d %s", estate->func->fn_signature, estate->err_stmt->lineno, _(estate->err_text));
    }
    else
    {
         
                                                                     
                                         
         
      errcontext("PL/pgSQL function %s %s", estate->func->fn_signature, _(estate->err_text));
    }
  }
  else if (estate->err_stmt != NULL)
  {
                                                              
    errcontext("PL/pgSQL function %s line %d at %s", estate->func->fn_signature, estate->err_stmt->lineno, plpgsql_stmt_typename(estate->err_stmt));
  }
  else
  {
    errcontext("PL/pgSQL function %s", estate->func->fn_signature);
  }
}

              
                                                               
              
   
static void
copy_plpgsql_datums(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
  int ndatums = estate->ndatums;
  PLpgSQL_datum **indatums;
  PLpgSQL_datum **outdatums;
  char *workspace;
  char *ws_next;
  int i;

                                          
  estate->datums = (PLpgSQL_datum **)palloc(sizeof(PLpgSQL_datum *) * ndatums);

     
                                                                            
                                                   
     
  workspace = palloc(func->copiable_size);
  ws_next = workspace;

                                                                         
  indatums = func->datums;
  outdatums = estate->datums;
  for (i = 0; i < ndatums; i++)
  {
    PLpgSQL_datum *indatum = indatums[i];
    PLpgSQL_datum *outdatum;

                                                                        
    switch (indatum->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    case PLPGSQL_DTYPE_PROMISE:
      outdatum = (PLpgSQL_datum *)ws_next;
      memcpy(outdatum, indatum, sizeof(PLpgSQL_var));
      ws_next += MAXALIGN(sizeof(PLpgSQL_var));
      break;

    case PLPGSQL_DTYPE_REC:
      outdatum = (PLpgSQL_datum *)ws_next;
      memcpy(outdatum, indatum, sizeof(PLpgSQL_rec));
      ws_next += MAXALIGN(sizeof(PLpgSQL_rec));
      break;

    case PLPGSQL_DTYPE_ROW:
    case PLPGSQL_DTYPE_RECFIELD:
    case PLPGSQL_DTYPE_ARRAYELEM:

         
                                                                     
                                                                
                                                                     
         
      outdatum = indatum;
      break;

    default:
      elog(ERROR, "unrecognized dtype: %d", indatum->dtype);
      outdatum = NULL;                          
      break;
    }

    outdatums[i] = outdatum;
  }

  Assert(ws_next == workspace + func->copiable_size);
}

   
                                                                      
                                  
                                                     
   
static void
plpgsql_fulfill_promise(PLpgSQL_execstate *estate, PLpgSQL_var *var)
{
  MemoryContext oldcontext;

  if (var->promise == PLPGSQL_PROMISE_NONE)
  {
    return;                    
  }

     
                                                                         
                                                                     
                                                                           
                                                                        
                                                                
     
  oldcontext = MemoryContextSwitchTo(estate->datum_context);

  switch (var->promise)
  {
  case PLPGSQL_PROMISE_TG_NAME:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    assign_simple_var(estate, var, DirectFunctionCall1(namein, CStringGetDatum(estate->trigdata->tg_trigger->tgname)), false, true);
    break;

  case PLPGSQL_PROMISE_TG_WHEN:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    if (TRIGGER_FIRED_BEFORE(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "BEFORE");
    }
    else if (TRIGGER_FIRED_AFTER(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "AFTER");
    }
    else if (TRIGGER_FIRED_INSTEAD(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "INSTEAD OF");
    }
    else
    {
      elog(ERROR, "unrecognized trigger execution time: not BEFORE, AFTER, or INSTEAD OF");
    }
    break;

  case PLPGSQL_PROMISE_TG_LEVEL:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    if (TRIGGER_FIRED_FOR_ROW(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "ROW");
    }
    else if (TRIGGER_FIRED_FOR_STATEMENT(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "STATEMENT");
    }
    else
    {
      elog(ERROR, "unrecognized trigger event type: not ROW or STATEMENT");
    }
    break;

  case PLPGSQL_PROMISE_TG_OP:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    if (TRIGGER_FIRED_BY_INSERT(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "INSERT");
    }
    else if (TRIGGER_FIRED_BY_UPDATE(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "UPDATE");
    }
    else if (TRIGGER_FIRED_BY_DELETE(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "DELETE");
    }
    else if (TRIGGER_FIRED_BY_TRUNCATE(estate->trigdata->tg_event))
    {
      assign_text_var(estate, var, "TRUNCATE");
    }
    else
    {
      elog(ERROR, "unrecognized trigger action: not INSERT, DELETE, UPDATE, or TRUNCATE");
    }
    break;

  case PLPGSQL_PROMISE_TG_RELID:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    assign_simple_var(estate, var, ObjectIdGetDatum(estate->trigdata->tg_relation->rd_id), false, false);
    break;

  case PLPGSQL_PROMISE_TG_TABLE_NAME:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    assign_simple_var(estate, var, DirectFunctionCall1(namein, CStringGetDatum(RelationGetRelationName(estate->trigdata->tg_relation))), false, true);
    break;

  case PLPGSQL_PROMISE_TG_TABLE_SCHEMA:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    assign_simple_var(estate, var, DirectFunctionCall1(namein, CStringGetDatum(get_namespace_name(RelationGetNamespace(estate->trigdata->tg_relation)))), false, true);
    break;

  case PLPGSQL_PROMISE_TG_NARGS:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    assign_simple_var(estate, var, Int16GetDatum(estate->trigdata->tg_trigger->tgnargs), false, false);
    break;

  case PLPGSQL_PROMISE_TG_ARGV:
    if (estate->trigdata == NULL)
    {
      elog(ERROR, "trigger promise is not in a trigger function");
    }
    if (estate->trigdata->tg_trigger->tgnargs > 0)
    {
         
                                                                    
                                                      
         
      int nelems = estate->trigdata->tg_trigger->tgnargs;
      Datum *elems;
      int dims[1];
      int lbs[1];
      int i;

      elems = palloc(sizeof(Datum) * nelems);
      for (i = 0; i < nelems; i++)
      {
        elems[i] = CStringGetTextDatum(estate->trigdata->tg_trigger->tgargs[i]);
      }
      dims[0] = nelems;
      lbs[0] = 0;

      assign_simple_var(estate, var, PointerGetDatum(construct_md_array(elems, NULL, 1, dims, lbs, TEXTOID, -1, false, 'i')), false, true);
    }
    else
    {
      assign_simple_var(estate, var, (Datum)0, true, false);
    }
    break;

  case PLPGSQL_PROMISE_TG_EVENT:
    if (estate->evtrigdata == NULL)
    {
      elog(ERROR, "event trigger promise is not in an event trigger function");
    }
    assign_text_var(estate, var, estate->evtrigdata->event);
    break;

  case PLPGSQL_PROMISE_TG_TAG:
    if (estate->evtrigdata == NULL)
    {
      elog(ERROR, "event trigger promise is not in an event trigger function");
    }
    assign_text_var(estate, var, estate->evtrigdata->tag);
    break;

  default:
    elog(ERROR, "unrecognized promise type: %d", var->promise);
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                         
                                                                           
                                                                            
   
static MemoryContext
get_stmt_mcontext(PLpgSQL_execstate *estate)
{
  if (estate->stmt_mcontext == NULL)
  {
    estate->stmt_mcontext = AllocSetContextCreate(estate->stmt_mcontext_parent, "PLpgSQL per-statement data", ALLOCSET_DEFAULT_SIZES);
  }
  return estate->stmt_mcontext;
}

   
                                                                               
                                                                              
                                                                              
                        
   
static void
push_stmt_mcontext(PLpgSQL_execstate *estate)
{
                                                  
  Assert(estate->stmt_mcontext != NULL);
                                                    
  Assert(MemoryContextGetParent(estate->stmt_mcontext) == estate->stmt_mcontext_parent);
                                                                     
  estate->stmt_mcontext_parent = estate->stmt_mcontext;
                                                  
  estate->stmt_mcontext = NULL;
}

   
                                                                           
                                                                            
                                                                             
                                             
   
static void
pop_stmt_mcontext(PLpgSQL_execstate *estate)
{
                                  
  estate->stmt_mcontext = estate->stmt_mcontext_parent;
  estate->stmt_mcontext_parent = MemoryContextGetParent(estate->stmt_mcontext);
}

   
                                                                            
                                
   
static bool
exception_matches_conditions(ErrorData *edata, PLpgSQL_condition *cond)
{
  for (; cond != NULL; cond = cond->next)
  {
    int sqlerrstate = cond->sqlerrstate;

       
                                                             
                                                                      
                   
       
    if (sqlerrstate == 0)
    {
      if (edata->sqlerrcode != ERRCODE_QUERY_CANCELED && edata->sqlerrcode != ERRCODE_ASSERT_FAILURE)
      {
        return true;
      }
    }
                      
    else if (edata->sqlerrcode == sqlerrstate)
    {
      return true;
    }
                         
    else if (ERRCODE_IS_CATEGORY(sqlerrstate) && ERRCODE_TO_CATEGORY(edata->sqlerrcode) == sqlerrstate)
    {
      return true;
    }
  }
  return false;
}

              
                                                   
              
   
static int
exec_stmt_block(PLpgSQL_execstate *estate, PLpgSQL_stmt_block *block)
{
  volatile int rc = -1;
  int i;

     
                                                           
     
  estate->err_text = gettext_noop("during statement block local variable initialization");

  for (i = 0; i < block->n_initvars; i++)
  {
    int n = block->initvarnos[i];
    PLpgSQL_datum *datum = estate->datums[n];

       
                                                                           
       
                                                                          
                                                                        
             
       
    switch (datum->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    {
      PLpgSQL_var *var = (PLpgSQL_var *)datum;

         
                                                            
                            
         
      assign_simple_var(estate, var, (Datum)0, true, false);

      if (var->default_val == NULL)
      {
           
                                                           
                                                           
                                                             
                                                    
           
        if (var->datatype->typtype == TYPTYPE_DOMAIN)
        {
          exec_assign_value(estate, (PLpgSQL_datum *)var, (Datum)0, true, UNKNOWNOID, -1);
        }

                                                  
        Assert(!var->notnull);
      }
      else
      {
        exec_assign_expr(estate, (PLpgSQL_datum *)var, var->default_val);
      }
    }
    break;

    case PLPGSQL_DTYPE_REC:
    {
      PLpgSQL_rec *rec = (PLpgSQL_rec *)datum;

         
                                                                
                                                            
                                                           
         
      if (rec->default_val == NULL)
      {
           
                                                           
                                                       
           
        exec_move_row(estate, (PLpgSQL_variable *)rec, NULL, NULL);

                                                  
        Assert(!rec->notnull);
      }
      else
      {
        exec_assign_expr(estate, (PLpgSQL_datum *)rec, rec->default_val);
      }
    }
    break;

    default:
      elog(ERROR, "unrecognized dtype: %d", datum->dtype);
    }
  }

  if (block->exceptions)
  {
       
                                                                           
       
    MemoryContext oldcontext = CurrentMemoryContext;
    ResourceOwner oldowner = CurrentResourceOwner;
    ExprContext *old_eval_econtext = estate->eval_econtext;
    ErrorData *save_cur_error = estate->cur_error;
    MemoryContext stmt_mcontext;

    estate->err_text = gettext_noop("during statement block entry");

       
                                                                       
                                                                       
                                                                          
                                                                         
                                                                        
                                                                        
                                                             
       
    stmt_mcontext = get_stmt_mcontext(estate);

    BeginInternalSubTransaction(NULL);
                                                                 
    MemoryContextSwitchTo(oldcontext);

    PG_TRY();
    {
         
                                                                        
                                                                      
                                                                        
                                    
         
      plpgsql_create_econtext(estate);

      estate->err_text = NULL;

                                      
      rc = exec_stmts(estate, block->body);

      estate->err_text = gettext_noop("during statement block exit");

         
                                                                        
                                                                       
                                                                         
         
      if (rc == PLPGSQL_RC_RETURN && !estate->retisset && !estate->retisnull)
      {
        int16 resTypLen;
        bool resTypByVal;

        get_typlenbyval(estate->rettype, &resTypLen, &resTypByVal);
        estate->retval = datumTransfer(estate->retval, resTypByVal, resTypLen);
      }

                                                                      
      ReleaseCurrentSubTransaction();
      MemoryContextSwitchTo(oldcontext);
      CurrentResourceOwner = oldowner;

                                                            
      Assert(stmt_mcontext == estate->stmt_mcontext);

         
                                                            
                                                        
         
      estate->eval_econtext = old_eval_econtext;
    }
    PG_CATCH();
    {
      ErrorData *edata;
      ListCell *e;

      estate->err_text = gettext_noop("during exception cleanup");

                                                
      MemoryContextSwitchTo(stmt_mcontext);
      edata = CopyErrorData();
      FlushErrorState();

                                       
      RollbackAndReleaseCurrentSubTransaction();
      MemoryContextSwitchTo(oldcontext);
      CurrentResourceOwner = oldowner;

         
                                                                      
                                                                         
                                                                  
                                                             
         
      estate->stmt_mcontext_parent = stmt_mcontext;
      estate->stmt_mcontext = NULL;

         
                                                                     
                                                                         
                                                                       
                                                                         
                                                                     
                                                                       
                                            
         
      MemoryContextDeleteChildren(stmt_mcontext);

                                         
      estate->eval_econtext = old_eval_econtext;

         
                                                                        
                                                                         
                                                                  
                        
         
      estate->eval_tuptable = NULL;
      exec_eval_cleanup(estate);

                                                 
      foreach (e, block->exceptions->exc_list)
      {
        PLpgSQL_exception *exception = (PLpgSQL_exception *)lfirst(e);

        if (exception_matches_conditions(edata, exception->conditions))
        {
             
                                                                     
                                                                  
                                                                 
                                                       
             
          PLpgSQL_var *state_var;
          PLpgSQL_var *errm_var;

          state_var = (PLpgSQL_var *)estate->datums[block->exceptions->sqlstate_varno];
          errm_var = (PLpgSQL_var *)estate->datums[block->exceptions->sqlerrm_varno];

          assign_text_var(estate, state_var, unpack_sql_state(edata->sqlerrcode));
          assign_text_var(estate, errm_var, edata->message);

             
                                                                   
                                 
             
          estate->cur_error = edata;

          estate->err_text = NULL;

          rc = exec_stmts(estate, exception->action);

          break;
        }
      }

         
                                                                         
                                                                     
                                               
         
      estate->cur_error = save_cur_error;

                                                 
      if (e == NULL)
      {
        ReThrowError(edata);
      }

                                                                  
      pop_stmt_mcontext(estate);
      MemoryContextReset(stmt_mcontext);
    }
    PG_END_TRY();

    Assert(save_cur_error == estate->cur_error);
  }
  else
  {
       
                                                       
       
    estate->err_text = NULL;

    rc = exec_stmts(estate, block->body);
  }

  estate->err_text = NULL;

     
                                                                   
                                                                            
                                             
     
  switch (rc)
  {
  case PLPGSQL_RC_OK:
  case PLPGSQL_RC_RETURN:
  case PLPGSQL_RC_CONTINUE:
    return rc;

  case PLPGSQL_RC_EXIT:
    if (estate->exitlabel == NULL)
    {
      return PLPGSQL_RC_EXIT;
    }
    if (block->label == NULL)
    {
      return PLPGSQL_RC_EXIT;
    }
    if (strcmp(block->label, estate->exitlabel) != 0)
    {
      return PLPGSQL_RC_EXIT;
    }
    estate->exitlabel = NULL;
    return PLPGSQL_RC_OK;

  default:
    elog(ERROR, "unrecognized rc: %d", rc);
  }

  return PLPGSQL_RC_OK;
}

              
                                                  
                                         
              
   
static int
exec_stmts(PLpgSQL_execstate *estate, List *stmts)
{
  ListCell *s;

  if (stmts == NIL)
  {
       
                                                                     
                                                                          
                                                     
       
    CHECK_FOR_INTERRUPTS();
    return PLPGSQL_RC_OK;
  }

  foreach (s, stmts)
  {
    PLpgSQL_stmt *stmt = (PLpgSQL_stmt *)lfirst(s);
    int rc = exec_stmt(estate, stmt);

    if (rc != PLPGSQL_RC_OK)
    {
      return rc;
    }
  }

  return PLPGSQL_RC_OK;
}

              
                                                          
                                        
              
   
static int
exec_stmt(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt)
{
  PLpgSQL_stmt *save_estmt;
  int rc = -1;

  save_estmt = estate->err_stmt;
  estate->err_stmt = stmt;

                                                                       
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->stmt_beg)
  {
    ((*plpgsql_plugin_ptr)->stmt_beg)(estate, stmt);
  }

  CHECK_FOR_INTERRUPTS();

  switch (stmt->cmd_type)
  {
  case PLPGSQL_STMT_BLOCK:
    rc = exec_stmt_block(estate, (PLpgSQL_stmt_block *)stmt);
    break;

  case PLPGSQL_STMT_ASSIGN:
    rc = exec_stmt_assign(estate, (PLpgSQL_stmt_assign *)stmt);
    break;

  case PLPGSQL_STMT_PERFORM:
    rc = exec_stmt_perform(estate, (PLpgSQL_stmt_perform *)stmt);
    break;

  case PLPGSQL_STMT_CALL:
    rc = exec_stmt_call(estate, (PLpgSQL_stmt_call *)stmt);
    break;

  case PLPGSQL_STMT_GETDIAG:
    rc = exec_stmt_getdiag(estate, (PLpgSQL_stmt_getdiag *)stmt);
    break;

  case PLPGSQL_STMT_IF:
    rc = exec_stmt_if(estate, (PLpgSQL_stmt_if *)stmt);
    break;

  case PLPGSQL_STMT_CASE:
    rc = exec_stmt_case(estate, (PLpgSQL_stmt_case *)stmt);
    break;

  case PLPGSQL_STMT_LOOP:
    rc = exec_stmt_loop(estate, (PLpgSQL_stmt_loop *)stmt);
    break;

  case PLPGSQL_STMT_WHILE:
    rc = exec_stmt_while(estate, (PLpgSQL_stmt_while *)stmt);
    break;

  case PLPGSQL_STMT_FORI:
    rc = exec_stmt_fori(estate, (PLpgSQL_stmt_fori *)stmt);
    break;

  case PLPGSQL_STMT_FORS:
    rc = exec_stmt_fors(estate, (PLpgSQL_stmt_fors *)stmt);
    break;

  case PLPGSQL_STMT_FORC:
    rc = exec_stmt_forc(estate, (PLpgSQL_stmt_forc *)stmt);
    break;

  case PLPGSQL_STMT_FOREACH_A:
    rc = exec_stmt_foreach_a(estate, (PLpgSQL_stmt_foreach_a *)stmt);
    break;

  case PLPGSQL_STMT_EXIT:
    rc = exec_stmt_exit(estate, (PLpgSQL_stmt_exit *)stmt);
    break;

  case PLPGSQL_STMT_RETURN:
    rc = exec_stmt_return(estate, (PLpgSQL_stmt_return *)stmt);
    break;

  case PLPGSQL_STMT_RETURN_NEXT:
    rc = exec_stmt_return_next(estate, (PLpgSQL_stmt_return_next *)stmt);
    break;

  case PLPGSQL_STMT_RETURN_QUERY:
    rc = exec_stmt_return_query(estate, (PLpgSQL_stmt_return_query *)stmt);
    break;

  case PLPGSQL_STMT_RAISE:
    rc = exec_stmt_raise(estate, (PLpgSQL_stmt_raise *)stmt);
    break;

  case PLPGSQL_STMT_ASSERT:
    rc = exec_stmt_assert(estate, (PLpgSQL_stmt_assert *)stmt);
    break;

  case PLPGSQL_STMT_EXECSQL:
    rc = exec_stmt_execsql(estate, (PLpgSQL_stmt_execsql *)stmt);
    break;

  case PLPGSQL_STMT_DYNEXECUTE:
    rc = exec_stmt_dynexecute(estate, (PLpgSQL_stmt_dynexecute *)stmt);
    break;

  case PLPGSQL_STMT_DYNFORS:
    rc = exec_stmt_dynfors(estate, (PLpgSQL_stmt_dynfors *)stmt);
    break;

  case PLPGSQL_STMT_OPEN:
    rc = exec_stmt_open(estate, (PLpgSQL_stmt_open *)stmt);
    break;

  case PLPGSQL_STMT_FETCH:
    rc = exec_stmt_fetch(estate, (PLpgSQL_stmt_fetch *)stmt);
    break;

  case PLPGSQL_STMT_CLOSE:
    rc = exec_stmt_close(estate, (PLpgSQL_stmt_close *)stmt);
    break;

  case PLPGSQL_STMT_COMMIT:
    rc = exec_stmt_commit(estate, (PLpgSQL_stmt_commit *)stmt);
    break;

  case PLPGSQL_STMT_ROLLBACK:
    rc = exec_stmt_rollback(estate, (PLpgSQL_stmt_rollback *)stmt);
    break;

  case PLPGSQL_STMT_SET:
    rc = exec_stmt_set(estate, (PLpgSQL_stmt_set *)stmt);
    break;

  default:
    estate->err_stmt = save_estmt;
    elog(ERROR, "unrecognized cmd_type: %d", stmt->cmd_type);
  }

                                                                          
  if (*plpgsql_plugin_ptr && (*plpgsql_plugin_ptr)->stmt_end)
  {
    ((*plpgsql_plugin_ptr)->stmt_end)(estate, stmt);
  }

  estate->err_stmt = save_estmt;

  return rc;
}

              
                                                 
                                       
              
   
static int
exec_stmt_assign(PLpgSQL_execstate *estate, PLpgSQL_stmt_assign *stmt)
{
  Assert(stmt->varno >= 0);

  exec_assign_expr(estate, estate->datums[stmt->varno], stmt->expr);

  return PLPGSQL_RC_OK;
}

              
                                                                 
                                                     
                        
              
   
static int
exec_stmt_perform(PLpgSQL_execstate *estate, PLpgSQL_stmt_perform *stmt)
{
  PLpgSQL_expr *expr = stmt->expr;

  (void)exec_run_select(estate, expr, 0, NULL);
  exec_set_found(estate, (estate->eval_processed != 0));
  exec_eval_cleanup(estate);

  return PLPGSQL_RC_OK;
}

   
                  
   
                                                       
   
static int
exec_stmt_call(PLpgSQL_execstate *estate, PLpgSQL_stmt_call *stmt)
{
  PLpgSQL_expr *expr = stmt->expr;
  SPIPlanPtr orig_plan = expr->plan;
  bool local_plan;
  PLpgSQL_variable *volatile cur_target = stmt->target;
  volatile LocalTransactionId before_lxid;
  LocalTransactionId after_lxid;
  volatile int rc;

     
                                                                            
                                                                             
                                               
     
                                                                    
                                                              
     
  local_plan = !estate->atomic;

                                                                      
  PG_TRY();
  {
    SPIPlanPtr plan = expr->plan;
    ParamListInfo paramLI;

       
                                                                          
                                                                         
                                                                 
       
    if (plan == NULL || local_plan)
    {
         
                                                                       
                                                         
         
      stmt->target = NULL;
      cur_target = NULL;

                                                                 
      exec_prepare_plan(estate, expr, 0, !local_plan);
      plan = expr->plan;
    }

       
                                                                        
                                                                        
                                                      
       
    Assert(!expr->expr_simple_expr);

       
                                                                     
                       
       
    plan->no_snapshots = true;

       
                                                                         
                                                                          
                                              
       
                                                                         
                                                                          
                                                                           
                                                                        
                                      
       
    if (stmt->is_call && cur_target == NULL)
    {
      Node *node;
      FuncExpr *funcexpr;
      HeapTuple func_tuple;
      List *funcargs;
      Oid *argtypes;
      char **argnames;
      char *argmodes;
      MemoryContext oldcontext;
      PLpgSQL_row *row;
      int nfields;
      int i;
      ListCell *lc;

                                                            
      oldcontext = MemoryContextSwitchTo(get_stmt_mcontext(estate));

         
                                                                   
         
      node = linitial_node(Query, ((CachedPlanSource *)linitial(plan->plancache_list))->query_list)->utilityStmt;
      if (node == NULL || !IsA(node, CallStmt))
      {
        elog(ERROR, "query for CALL statement is not a CallStmt");
      }

      funcexpr = ((CallStmt *)node)->funcexpr;

      func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcexpr->funcid));
      if (!HeapTupleIsValid(func_tuple))
      {
        elog(ERROR, "cache lookup failed for function %u", funcexpr->funcid);
      }

         
                                                                       
         
      funcargs = expand_function_arguments(funcexpr->args, funcexpr->funcresulttype, func_tuple);

         
                                               
         
      get_func_arg_info(func_tuple, &argtypes, &argnames, &argmodes);

      ReleaseSysCache(func_tuple);

         
                                                                       
                     
         
      if (!local_plan)
      {
        MemoryContextSwitchTo(estate->func->fn_cxt);
      }

      row = (PLpgSQL_row *)palloc0(sizeof(PLpgSQL_row));
      row->dtype = PLPGSQL_DTYPE_ROW;
      row->refname = "(unnamed row)";
      row->lineno = -1;
      row->varnos = (int *)palloc(sizeof(int) * list_length(funcargs));

      if (!local_plan)
      {
        MemoryContextSwitchTo(get_stmt_mcontext(estate));
      }

         
                                                                      
                                                                       
                                    
         
      nfields = 0;
      i = 0;
      foreach (lc, funcargs)
      {
        Node *n = lfirst(lc);

        if (argmodes && (argmodes[i] == PROARGMODE_INOUT || argmodes[i] == PROARGMODE_OUT))
        {
          if (IsA(n, Param))
          {
            Param *param = (Param *)n;

                                                                 
            row->varnos[nfields++] = param->paramid - 1;
          }
          else
          {
                                                                 
            if (argnames && argnames[i] && argnames[i][0])
            {
              ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("procedure parameter \"%s\" is an output parameter but corresponding argument is not writable", argnames[i])));
            }
            else
            {
              ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("procedure parameter %d is an output parameter but corresponding argument is not writable", i + 1)));
            }
          }
        }
        i++;
      }

      row->nfields = nfields;

      cur_target = (PLpgSQL_variable *)row;

                                                                      
      if (!local_plan)
      {
        stmt->target = cur_target;
      }

      MemoryContextSwitchTo(oldcontext);
    }

    paramLI = setup_param_list(estate, expr);

    before_lxid = MyProc->lxid;

    rc = SPI_execute_plan_with_paramlist(expr->plan, paramLI, estate->readonly_func, 0);
  }
  PG_CATCH();
  {
       
                                                                
       
    if (local_plan)
    {
      expr->plan = orig_plan;
    }
    PG_RE_THROW();
  }
  PG_END_TRY();

     
                                                                            
                                                                             
                                                                          
                                                                 
     
  if (local_plan)
  {
    SPIPlanPtr plan = expr->plan;

    expr->plan = orig_plan;
    SPI_freeplan(plan);
  }

  if (rc < 0)
  {
    elog(ERROR, "SPI_execute_plan_with_paramlist failed executing query \"%s\": %s", expr->query, SPI_result_code_string(rc));
  }

  after_lxid = MyProc->lxid;

  if (before_lxid != after_lxid)
  {
       
                                                                           
                                         
       
    estate->simple_eval_estate = NULL;
    plpgsql_create_econtext(estate);
  }

     
                                                                          
                                               
     
  if (SPI_processed == 1)
  {
    SPITupleTable *tuptab = SPI_tuptable;

    if (!cur_target)
    {
      elog(ERROR, "DO statement returned a row");
    }

    exec_move_row(estate, cur_target, tuptab->vals[0], tuptab->tupdesc);
  }
  else if (SPI_processed > 1)
  {
    elog(ERROR, "procedure call returned more than one row");
  }

  exec_eval_cleanup(estate);
  SPI_freetuptable(SPI_tuptable);

  return PLPGSQL_RC_OK;
}

              
                                                          
                                 
              
   
static int
exec_stmt_getdiag(PLpgSQL_execstate *estate, PLpgSQL_stmt_getdiag *stmt)
{
  ListCell *lc;

     
                                                                        
     
                                                                           
                                                        
     
  if (stmt->is_stacked && estate->cur_error == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_STACKED_DIAGNOSTICS_ACCESSED_WITHOUT_ACTIVE_HANDLER), errmsg("GET STACKED DIAGNOSTICS cannot be used outside an exception handler")));
  }

  foreach (lc, stmt->diag_items)
  {
    PLpgSQL_diag_item *diag_item = (PLpgSQL_diag_item *)lfirst(lc);
    PLpgSQL_datum *var = estate->datums[diag_item->target];

    switch (diag_item->kind)
    {
    case PLPGSQL_GETDIAG_ROW_COUNT:
      exec_assign_value(estate, var, UInt64GetDatum(estate->eval_processed), false, INT8OID, -1);
      break;

    case PLPGSQL_GETDIAG_ERROR_CONTEXT:
      exec_assign_c_string(estate, var, estate->cur_error->context);
      break;

    case PLPGSQL_GETDIAG_ERROR_DETAIL:
      exec_assign_c_string(estate, var, estate->cur_error->detail);
      break;

    case PLPGSQL_GETDIAG_ERROR_HINT:
      exec_assign_c_string(estate, var, estate->cur_error->hint);
      break;

    case PLPGSQL_GETDIAG_RETURNED_SQLSTATE:
      exec_assign_c_string(estate, var, unpack_sql_state(estate->cur_error->sqlerrcode));
      break;

    case PLPGSQL_GETDIAG_COLUMN_NAME:
      exec_assign_c_string(estate, var, estate->cur_error->column_name);
      break;

    case PLPGSQL_GETDIAG_CONSTRAINT_NAME:
      exec_assign_c_string(estate, var, estate->cur_error->constraint_name);
      break;

    case PLPGSQL_GETDIAG_DATATYPE_NAME:
      exec_assign_c_string(estate, var, estate->cur_error->datatype_name);
      break;

    case PLPGSQL_GETDIAG_MESSAGE_TEXT:
      exec_assign_c_string(estate, var, estate->cur_error->message);
      break;

    case PLPGSQL_GETDIAG_TABLE_NAME:
      exec_assign_c_string(estate, var, estate->cur_error->table_name);
      break;

    case PLPGSQL_GETDIAG_SCHEMA_NAME:
      exec_assign_c_string(estate, var, estate->cur_error->schema_name);
      break;

    case PLPGSQL_GETDIAG_CONTEXT:
    {
      char *contextstackstr;
      MemoryContext oldcontext;

                                                    
      oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
      contextstackstr = GetErrorContextStack();
      MemoryContextSwitchTo(oldcontext);

      exec_assign_c_string(estate, var, contextstackstr);
    }
    break;

    default:
      elog(ERROR, "unrecognized diagnostic item kind: %d", diag_item->kind);
    }
  }

  exec_eval_cleanup(estate);

  return PLPGSQL_RC_OK;
}

              
                                                  
                                      
                      
              
   
static int
exec_stmt_if(PLpgSQL_execstate *estate, PLpgSQL_stmt_if *stmt)
{
  bool value;
  bool isnull;
  ListCell *lc;

  value = exec_eval_boolean(estate, stmt->cond, &isnull);
  exec_eval_cleanup(estate);
  if (!isnull && value)
  {
    return exec_stmts(estate, stmt->then_body);
  }

  foreach (lc, stmt->elsif_list)
  {
    PLpgSQL_if_elsif *elif = (PLpgSQL_if_elsif *)lfirst(lc);

    value = exec_eval_boolean(estate, elif->cond, &isnull);
    exec_eval_cleanup(estate);
    if (!isnull && value)
    {
      return exec_stmts(estate, elif->stmts);
    }
  }

  return exec_stmts(estate, stmt->else_body);
}

              
                  
              
   
static int
exec_stmt_case(PLpgSQL_execstate *estate, PLpgSQL_stmt_case *stmt)
{
  PLpgSQL_var *t_var = NULL;
  bool isnull;
  ListCell *l;

  if (stmt->t_expr != NULL)
  {
                     
    Datum t_val;
    Oid t_typoid;
    int32 t_typmod;

    t_val = exec_eval_expr(estate, stmt->t_expr, &isnull, &t_typoid, &t_typmod);

    t_var = (PLpgSQL_var *)estate->datums[stmt->t_varno];

       
                                                                           
                                                                       
                                                                          
                                                                           
                                                                       
                                     
       
    if (t_var->datatype->typoid != t_typoid || t_var->datatype->atttypmod != t_typmod)
    {
      t_var->datatype = plpgsql_build_datatype(t_typoid, t_typmod, estate->func->fn_input_collation, NULL);
    }

                                           
    exec_assign_value(estate, (PLpgSQL_datum *)t_var, t_val, isnull, t_typoid, t_typmod);

    exec_eval_cleanup(estate);
  }

                                               
  foreach (l, stmt->case_when_list)
  {
    PLpgSQL_case_when *cwt = (PLpgSQL_case_when *)lfirst(l);
    bool value;

    value = exec_eval_boolean(estate, cwt->expr, &isnull);
    exec_eval_cleanup(estate);
    if (!isnull && value)
    {
                    

                                                                     
      if (t_var != NULL)
      {
        assign_simple_var(estate, t_var, (Datum)0, true, false);
      }

                                                     
      return exec_stmts(estate, cwt->stmts);
    }
  }

                                                                 
  if (t_var != NULL)
  {
    assign_simple_var(estate, t_var, (Datum)0, true, false);
  }

                                                               
  if (!stmt->have_else)
  {
    ereport(ERROR, (errcode(ERRCODE_CASE_NOT_FOUND), errmsg("case not found"), errhint("CASE statement is missing ELSE part.")));
  }

                                                    
  return exec_stmts(estate, stmt->else_stmts);
}

              
                                               
                       
              
   
static int
exec_stmt_loop(PLpgSQL_execstate *estate, PLpgSQL_stmt_loop *stmt)
{
  int rc = PLPGSQL_RC_OK;

  for (;;)
  {
    rc = exec_stmts(estate, stmt->body);

    LOOP_RC_PROCESSING(stmt->label, break);
  }

  return rc;
}

              
                                                  
                                     
                               
              
   
static int
exec_stmt_while(PLpgSQL_execstate *estate, PLpgSQL_stmt_while *stmt)
{
  int rc = PLPGSQL_RC_OK;

  for (;;)
  {
    bool value;
    bool isnull;

    value = exec_eval_boolean(estate, stmt->cond, &isnull);
    exec_eval_cleanup(estate);

    if (isnull || !value)
    {
      break;
    }

    rc = exec_stmts(estate, stmt->body);

    LOOP_RC_PROCESSING(stmt->label, break);
  }

  return rc;
}

              
                                                
                                      
                                                    
              
   
static int
exec_stmt_fori(PLpgSQL_execstate *estate, PLpgSQL_stmt_fori *stmt)
{
  PLpgSQL_var *var;
  Datum value;
  bool isnull;
  Oid valtype;
  int32 valtypmod;
  int32 loop_value;
  int32 end_value;
  int32 step_value;
  bool found = false;
  int rc = PLPGSQL_RC_OK;

  var = (PLpgSQL_var *)(estate->datums[stmt->var->dno]);

     
                                      
     
  value = exec_eval_expr(estate, stmt->lower, &isnull, &valtype, &valtypmod);
  value = exec_cast_value(estate, value, &isnull, valtype, valtypmod, var->datatype->typoid, var->datatype->atttypmod);
  if (isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("lower bound of FOR loop cannot be null")));
  }
  loop_value = DatumGetInt32(value);
  exec_eval_cleanup(estate);

     
                                      
     
  value = exec_eval_expr(estate, stmt->upper, &isnull, &valtype, &valtypmod);
  value = exec_cast_value(estate, value, &isnull, valtype, valtypmod, var->datatype->typoid, var->datatype->atttypmod);
  if (isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("upper bound of FOR loop cannot be null")));
  }
  end_value = DatumGetInt32(value);
  exec_eval_cleanup(estate);

     
                        
     
  if (stmt->step)
  {
    value = exec_eval_expr(estate, stmt->step, &isnull, &valtype, &valtypmod);
    value = exec_cast_value(estate, value, &isnull, valtype, valtypmod, var->datatype->typoid, var->datatype->atttypmod);
    if (isnull)
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("BY value of FOR loop cannot be null")));
    }
    step_value = DatumGetInt32(value);
    exec_eval_cleanup(estate);
    if (step_value <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("BY value of FOR loop must be greater than zero")));
    }
  }
  else
  {
    step_value = 1;
  }

     
                     
     
  for (;;)
  {
       
                                 
       
    if (stmt->reverse)
    {
      if (loop_value < end_value)
      {
        break;
      }
    }
    else
    {
      if (loop_value > end_value)
      {
        break;
      }
    }

    found = true;                           

       
                                        
       
    assign_simple_var(estate, var, Int32GetDatum(loop_value), false, false);

       
                              
       
    rc = exec_stmts(estate, stmt->body);

    LOOP_RC_PROCESSING(stmt->label, break);

       
                                                                        
                           
       
    if (stmt->reverse)
    {
      if (loop_value < (PG_INT32_MIN + step_value))
      {
        break;
      }
      loop_value -= step_value;
    }
    else
    {
      if (loop_value > (PG_INT32_MAX - step_value))
      {
        break;
      }
      loop_value += step_value;
    }
  }

     
                                                                         
                                                                             
                                                                            
                                 
     
  exec_set_found(estate, found);

  return rc;
}

              
                                                 
                                    
                                     
               
              
   
static int
exec_stmt_fors(PLpgSQL_execstate *estate, PLpgSQL_stmt_fors *stmt)
{
  Portal portal;
  int rc;

     
                                                                      
     
  exec_run_select(estate, stmt->query, 0, &portal);

     
                      
     
  rc = exec_for_query(estate, (PLpgSQL_stmt_forq *)stmt, portal, true);

     
                               
     
  SPI_cursor_close(portal);

  return rc;
}

              
                                                               
              
   
static int
exec_stmt_forc(PLpgSQL_execstate *estate, PLpgSQL_stmt_forc *stmt)
{
  PLpgSQL_var *curvar;
  MemoryContext stmt_mcontext = NULL;
  char *curname = NULL;
  PLpgSQL_expr *query;
  ParamListInfo paramLI;
  Portal portal;
  int rc;

                
                                                                   
                                     
                
     
  curvar = (PLpgSQL_var *)(estate->datums[stmt->curvar]);
  if (!curvar->isnull)
  {
    MemoryContext oldcontext;

                                                                   
    stmt_mcontext = get_stmt_mcontext(estate);
    oldcontext = MemoryContextSwitchTo(stmt_mcontext);
    curname = TextDatumGetCString(curvar->value);
    MemoryContextSwitchTo(oldcontext);

    if (SPI_cursor_find(curname) != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_CURSOR), errmsg("cursor \"%s\" already in use", curname)));
    }
  }

                
                                               
     
                                                                      
                                                                
                
     
  if (stmt->argquery != NULL)
  {
                  
                                                             
                                                           
                     
                  
       
    PLpgSQL_stmt_execsql set_args;

    if (curvar->cursor_explicit_argrow < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("arguments given for cursor without arguments")));
    }

    memset(&set_args, 0, sizeof(set_args));
    set_args.cmd_type = PLPGSQL_STMT_EXECSQL;
    set_args.lineno = stmt->lineno;
    set_args.sqlstmt = stmt->argquery;
    set_args.into = true;
                                                   
    set_args.target = (PLpgSQL_variable *)(estate->datums[curvar->cursor_explicit_argrow]);

    if (exec_stmt_execsql(estate, &set_args) != PLPGSQL_RC_OK)
    {
      elog(ERROR, "open cursor failed during argument processing");
    }
  }
  else
  {
    if (curvar->cursor_explicit_argrow >= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("arguments required for cursor")));
    }
  }

  query = curvar->cursor_explicit_expr;
  Assert(query);

  if (query->plan == NULL)
  {
    exec_prepare_plan(estate, query, curvar->cursor_options, true);
  }

     
                                         
     
  paramLI = setup_param_list(estate, query);

     
                                                                     
     
  portal = SPI_cursor_open_with_paramlist(curname, query->plan, paramLI, estate->readonly_func);
  if (portal == NULL)
  {
    elog(ERROR, "could not open cursor: %s", SPI_result_code_string(SPI_result));
  }

     
                                                                        
     
  if (curname == NULL)
  {
    assign_text_var(estate, curvar, portal->name);
  }

     
                                             
     
  exec_eval_cleanup(estate);
  if (stmt_mcontext)
  {
    MemoryContextReset(stmt_mcontext);
  }

     
                                                                           
                                                                            
     
  rc = exec_for_query(estate, (PLpgSQL_stmt_forq *)stmt, portal, false);

                
                                                                         
                
     
  SPI_cursor_close(portal);

  if (curname == NULL)
  {
    assign_simple_var(estate, curvar, (Datum)0, true, false);
  }

  return rc;
}

              
                                                                  
   
                                                                           
                                                                              
                                                                      
              
   
static int
exec_stmt_foreach_a(PLpgSQL_execstate *estate, PLpgSQL_stmt_foreach_a *stmt)
{
  ArrayType *arr;
  Oid arrtype;
  int32 arrtypmod;
  PLpgSQL_datum *loop_var;
  Oid loop_var_elem_type;
  bool found = false;
  int rc = PLPGSQL_RC_OK;
  MemoryContext stmt_mcontext;
  MemoryContext oldcontext;
  ArrayIterator array_iterator;
  Oid iterator_result_type;
  int32 iterator_result_typmod;
  Datum value;
  bool isnull;

                                             
  value = exec_eval_expr(estate, stmt->expr, &isnull, &arrtype, &arrtypmod);
  if (isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("FOREACH expression must not be null")));
  }

     
                                                                             
                                                                           
                                                
     
  stmt_mcontext = get_stmt_mcontext(estate);
  push_stmt_mcontext(estate);
  oldcontext = MemoryContextSwitchTo(stmt_mcontext);

                                                           
  if (!OidIsValid(get_element_type(arrtype)))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("FOREACH expression must yield an array, not type %s", format_type_be(arrtype))));
  }

     
                                                                          
                                                                             
                                                            
     
  arr = DatumGetArrayTypePCopy(value);

                                              
  exec_eval_cleanup(estate);

                                                                     
  if (stmt->slice < 0 || stmt->slice > ARR_NDIM(arr))
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("slice dimension (%d) is out of the valid range 0..%d", stmt->slice, ARR_NDIM(arr))));
  }

                                                                  
  loop_var = estate->datums[stmt->varno];
  if (loop_var->dtype == PLPGSQL_DTYPE_REC || loop_var->dtype == PLPGSQL_DTYPE_ROW)
  {
       
                                                                         
                                                               
       
    loop_var_elem_type = InvalidOid;
  }
  else
  {
    loop_var_elem_type = get_element_type(plpgsql_exec_get_datum_type(estate, loop_var));
  }

     
                                                                            
                                                                            
                                                                            
                                                          
     
  if (stmt->slice > 0 && loop_var_elem_type == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("FOREACH ... SLICE loop variable must be of an array type")));
  }
  if (stmt->slice == 0 && loop_var_elem_type != InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("FOREACH loop variable must not be of an array type")));
  }

                                                    
  array_iterator = array_create_iterator(arr, stmt->slice, NULL);

                                     
  if (stmt->slice > 0)
  {
                                                                    
    iterator_result_type = arrtype;
    iterator_result_typmod = arrtypmod;
  }
  else
  {
                                                                
    iterator_result_type = ARR_ELEMTYPE(arr);
    iterator_result_typmod = arrtypmod;
  }

                                                 
  while (array_iterate(array_iterator, &value, &isnull))
  {
    found = true;                           

                                                                       
    MemoryContextSwitchTo(oldcontext);

                                                           
    exec_assign_value(estate, loop_var, value, isnull, iterator_result_type, iterator_result_typmod);

                                                                          
    if (stmt->slice > 0)
    {
      pfree(DatumGetPointer(value));
    }

       
                              
       
    rc = exec_stmts(estate, stmt->body);

    LOOP_RC_PROCESSING(stmt->label, break);

    MemoryContextSwitchTo(stmt_mcontext);
  }

                                    
  MemoryContextSwitchTo(oldcontext);
  pop_stmt_mcontext(estate);

                                                           
  MemoryContextReset(stmt_mcontext);

     
                                                                         
                                                                             
                                                                            
                                 
     
  exec_set_found(estate, found);

  return rc;
}

              
                                                 
   
                                                           
              
   
static int
exec_stmt_exit(PLpgSQL_execstate *estate, PLpgSQL_stmt_exit *stmt)
{
     
                                                         
     
  if (stmt->cond != NULL)
  {
    bool value;
    bool isnull;

    value = exec_eval_boolean(estate, stmt->cond, &isnull);
    exec_eval_cleanup(estate);
    if (isnull || value == false)
    {
      return PLPGSQL_RC_OK;
    }
  }

  estate->exitlabel = stmt->label;
  if (stmt->is_exit)
  {
    return PLPGSQL_RC_EXIT;
  }
  else
  {
    return PLPGSQL_RC_CONTINUE;
  }
}

              
                                                       
                                    
   
                                                                         
                                                           
              
   
static int
exec_stmt_return(PLpgSQL_execstate *estate, PLpgSQL_stmt_return *stmt)
{
     
                                                                       
                                                                            
                                             
     
  if (estate->retisset)
  {
    return PLPGSQL_RC_RETURN;
  }

                                  
  estate->retval = (Datum)0;
  estate->retisnull = true;
  estate->rettype = InvalidOid;

     
                                                                       
                                                                           
                                 
     
                                                                            
                                                                        
                                                                       
                                                                            
                                                                           
                                                                             
     
  if (stmt->retvarno >= 0)
  {
    PLpgSQL_datum *retvar = estate->datums[stmt->retvarno];

    switch (retvar->dtype)
    {
    case PLPGSQL_DTYPE_PROMISE:
                                                                   
      plpgsql_fulfill_promise(estate, (PLpgSQL_var *)retvar);

                     

    case PLPGSQL_DTYPE_VAR:
    {
      PLpgSQL_var *var = (PLpgSQL_var *)retvar;

      estate->retval = var->value;
      estate->retisnull = var->isnull;
      estate->rettype = var->datatype->typoid;

         
                                                          
                                                                
                                                                
                                                                 
                                                                
                                                                 
         
      if (estate->retistuple && !estate->retisnull)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot return non-composite value from function returning composite type")));
      }
    }
    break;

    case PLPGSQL_DTYPE_REC:
    {
      PLpgSQL_rec *rec = (PLpgSQL_rec *)retvar;

                                                                 
      if (rec->erh && !ExpandedRecordIsEmpty(rec->erh))
      {
        estate->retval = ExpandedRecordGetDatum(rec->erh);
        estate->retisnull = false;
        estate->rettype = rec->rectypeid;
      }
    }
    break;

    case PLPGSQL_DTYPE_ROW:
    {
      PLpgSQL_row *row = (PLpgSQL_row *)retvar;
      int32 rettypmod;

                                                            
      exec_eval_datum(estate, (PLpgSQL_datum *)row, &estate->rettype, &rettypmod, &estate->retval, &estate->retisnull);
    }
    break;

    default:
      elog(ERROR, "unrecognized dtype: %d", retvar->dtype);
    }

    return PLPGSQL_RC_RETURN;
  }

  if (stmt->expr != NULL)
  {
    int32 rettypmod;

    estate->retval = exec_eval_expr(estate, stmt->expr, &(estate->retisnull), &(estate->rettype), &rettypmod);

       
                                                                           
                                                                      
       
    if (estate->retistuple && !estate->retisnull && !type_is_rowtype(estate->rettype))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot return non-composite value from function returning composite type")));
    }

    return PLPGSQL_RC_RETURN;
  }

     
                                                                         
                                                                         
                                                                      
     
  if (estate->fn_rettype == VOIDOID && estate->func->fn_prokind != PROKIND_PROCEDURE)
  {
    estate->retval = (Datum)0;
    estate->retisnull = false;
    estate->rettype = VOIDOID;
  }

  return PLPGSQL_RC_RETURN;
}

              
                                                                   
                                                 
               
              
   
static int
exec_stmt_return_next(PLpgSQL_execstate *estate, PLpgSQL_stmt_return_next *stmt)
{
  TupleDesc tupdesc;
  int natts;
  HeapTuple tuple;
  MemoryContext oldcontext;

  if (!estate->retisset)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use RETURN NEXT in a non-SETOF function")));
  }

  if (estate->tuple_store == NULL)
  {
    exec_init_tuple_store(estate);
  }

                                                                
  tupdesc = estate->tuple_store_desc;
  natts = tupdesc->natts;

     
                                                                            
                                                                           
                                 
     
                                                                       
                                                                         
                                                                             
                                          
     
  if (stmt->retvarno >= 0)
  {
    PLpgSQL_datum *retvar = estate->datums[stmt->retvarno];

    switch (retvar->dtype)
    {
    case PLPGSQL_DTYPE_PROMISE:
                                                                   
      plpgsql_fulfill_promise(estate, (PLpgSQL_var *)retvar);

                     

    case PLPGSQL_DTYPE_VAR:
    {
      PLpgSQL_var *var = (PLpgSQL_var *)retvar;
      Datum retval = var->value;
      bool isNull = var->isnull;
      Form_pg_attribute attr = TupleDescAttr(tupdesc, 0);

      if (natts != 1)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("wrong result type supplied in RETURN NEXT")));
      }

                                                      
      retval = MakeExpandedObjectReadOnly(retval, isNull, var->datatype->typlen);

                                 
      retval = exec_cast_value(estate, retval, &isNull, var->datatype->typoid, var->datatype->atttypmod, attr->atttypid, attr->atttypmod);

      tuplestore_putvalues(estate->tuple_store, tupdesc, &retval, &isNull);
    }
    break;

    case PLPGSQL_DTYPE_REC:
    {
      PLpgSQL_rec *rec = (PLpgSQL_rec *)retvar;
      TupleDesc rec_tupdesc;
      TupleConversionMap *tupmap;

                                                               
      if (rec->erh == NULL)
      {
        instantiate_empty_record_variable(estate, rec);
      }
      if (ExpandedRecordIsEmpty(rec->erh))
      {
        deconstruct_expanded_record(rec->erh);
      }

                                                       
      oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
      rec_tupdesc = expanded_record_get_tupdesc(rec->erh);
      tupmap = convert_tuples_by_position(rec_tupdesc, tupdesc, gettext_noop("wrong record type supplied in RETURN NEXT"));
      tuple = expanded_record_get_tuple(rec->erh);
      if (tupmap)
      {
        tuple = execute_attr_map_tuple(tuple, tupmap);
      }
      tuplestore_puttuple(estate->tuple_store, tuple);
      MemoryContextSwitchTo(oldcontext);
    }
    break;

    case PLPGSQL_DTYPE_ROW:
    {
      PLpgSQL_row *row = (PLpgSQL_row *)retvar;

                                                            

                                                       
      oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
      tuple = make_tuple_from_row(estate, row, tupdesc);
      if (tuple == NULL)                        
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("wrong record type supplied in RETURN NEXT")));
      }
      tuplestore_puttuple(estate->tuple_store, tuple);
      MemoryContextSwitchTo(oldcontext);
    }
    break;

    default:
      elog(ERROR, "unrecognized dtype: %d", retvar->dtype);
      break;
    }
  }
  else if (stmt->expr)
  {
    Datum retval;
    bool isNull;
    Oid rettype;
    int32 rettypmod;

    retval = exec_eval_expr(estate, stmt->expr, &isNull, &rettype, &rettypmod);

    if (estate->retistuple)
    {
                                                            
      if (!isNull)
      {
        HeapTupleData tmptup;
        TupleDesc retvaldesc;
        TupleConversionMap *tupmap;

        if (!type_is_rowtype(rettype))
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot return non-composite value from function returning composite type")));
        }

                                                         
        oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
        retvaldesc = deconstruct_composite_datum(retval, &tmptup);
        tuple = &tmptup;
        tupmap = convert_tuples_by_position(retvaldesc, tupdesc, gettext_noop("returned record type does not match expected record type"));
        if (tupmap)
        {
          tuple = execute_attr_map_tuple(tuple, tupmap);
        }
        tuplestore_puttuple(estate->tuple_store, tuple);
        ReleaseTupleDesc(retvaldesc);
        MemoryContextSwitchTo(oldcontext);
      }
      else
      {
                                                     
        Datum *nulldatums;
        bool *nullflags;

        nulldatums = (Datum *)eval_mcontext_alloc0(estate, natts * sizeof(Datum));
        nullflags = (bool *)eval_mcontext_alloc(estate, natts * sizeof(bool));
        memset(nullflags, true, natts * sizeof(bool));
        tuplestore_putvalues(estate->tuple_store, tupdesc, nulldatums, nullflags);
      }
    }
    else
    {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, 0);

                                
      if (natts != 1)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("wrong result type supplied in RETURN NEXT")));
      }

                                 
      retval = exec_cast_value(estate, retval, &isNull, rettype, rettypmod, attr->atttypid, attr->atttypmod);

      tuplestore_putvalues(estate->tuple_store, tupdesc, &retval, &isNull);
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("RETURN NEXT must have a parameter")));
  }

  exec_eval_cleanup(estate);

  return PLPGSQL_RC_OK;
}

              
                                                              
                                                 
               
              
   
static int
exec_stmt_return_query(PLpgSQL_execstate *estate, PLpgSQL_stmt_return_query *stmt)
{
  Portal portal;
  uint64 processed = 0;
  TupleConversionMap *tupmap;
  MemoryContext oldcontext;

  if (!estate->retisset)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use RETURN QUERY in a non-SETOF function")));
  }

  if (estate->tuple_store == NULL)
  {
    exec_init_tuple_store(estate);
  }

  if (stmt->query != NULL)
  {
                      
    exec_run_select(estate, stmt->query, 0, &portal);
  }
  else
  {
                              
    Assert(stmt->dynquery != NULL);
    portal = exec_dynquery_with_params(estate, stmt->dynquery, stmt->params, NULL, 0);
  }

                                                   
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));

  tupmap = convert_tuples_by_position(portal->tupDesc, estate->tuple_store_desc, gettext_noop("structure of query does not match function result type"));

  while (true)
  {
    uint64 i;

    SPI_cursor_fetch(portal, true, 50);

                                                    
    MemoryContextSwitchTo(get_eval_mcontext(estate));

    if (SPI_processed == 0)
    {
      break;
    }

    for (i = 0; i < SPI_processed; i++)
    {
      HeapTuple tuple = SPI_tuptable->vals[i];

      if (tupmap)
      {
        tuple = execute_attr_map_tuple(tuple, tupmap);
      }
      tuplestore_puttuple(estate->tuple_store, tuple);
      if (tupmap)
      {
        heap_freetuple(tuple);
      }
      processed++;
    }

    SPI_freetuptable(SPI_tuptable);
  }

  SPI_freetuptable(SPI_tuptable);
  SPI_cursor_close(portal);

  MemoryContextSwitchTo(oldcontext);
  exec_eval_cleanup(estate);

  estate->eval_processed = processed;
  exec_set_found(estate, processed != 0);

  return PLPGSQL_RC_OK;
}

static void
exec_init_tuple_store(PLpgSQL_execstate *estate)
{
  ReturnSetInfo *rsi = estate->rsi;
  MemoryContext oldcxt;
  ResourceOwner oldowner;

     
                                                             
     
  if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0 || rsi->expectedDesc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }

     
                                                                           
                                                                            
                                                                             
                                                                            
                                               
     
  oldcxt = MemoryContextSwitchTo(estate->tuple_store_cxt);
  oldowner = CurrentResourceOwner;
  CurrentResourceOwner = estate->tuple_store_owner;

  estate->tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

  CurrentResourceOwner = oldowner;
  MemoryContextSwitchTo(oldcxt);

  estate->tuple_store_desc = rsi->expectedDesc;
}

#define SET_RAISE_OPTION_TEXT(opt, name)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (opt)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("RAISE option already specified: %s", name)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    opt = MemoryContextStrdup(stmt_mcontext, extval);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

              
                                                              
              
   
static int
exec_stmt_raise(PLpgSQL_execstate *estate, PLpgSQL_stmt_raise *stmt)
{
  int err_code = 0;
  char *condname = NULL;
  char *err_message = NULL;
  char *err_detail = NULL;
  char *err_hint = NULL;
  char *err_column = NULL;
  char *err_constraint = NULL;
  char *err_datatype = NULL;
  char *err_table = NULL;
  char *err_schema = NULL;
  MemoryContext stmt_mcontext;
  ListCell *lc;

                                                            
  if (stmt->condname == NULL && stmt->message == NULL && stmt->options == NIL)
  {
    if (estate->cur_error != NULL)
    {
      ReThrowError(estate->cur_error);
    }
                                          
    ereport(ERROR, (errcode(ERRCODE_STACKED_DIAGNOSTICS_ACCESSED_WITHOUT_ACTIVE_HANDLER), errmsg("RAISE without parameters cannot be used outside an exception handler")));
  }

                                                                     
  stmt_mcontext = get_stmt_mcontext(estate);

  if (stmt->condname)
  {
    err_code = plpgsql_recognize_err_condition(stmt->condname, true);
    condname = MemoryContextStrdup(stmt_mcontext, stmt->condname);
  }

  if (stmt->message)
  {
    StringInfoData ds;
    ListCell *current_param;
    char *cp;
    MemoryContext oldcontext;

                                       
    oldcontext = MemoryContextSwitchTo(stmt_mcontext);
    initStringInfo(&ds);
    MemoryContextSwitchTo(oldcontext);

    current_param = list_head(stmt->params);

    for (cp = stmt->message; *cp; cp++)
    {
         
                                                                        
                                                                     
         
      if (cp[0] == '%')
      {
        Oid paramtypeid;
        int32 paramtypmod;
        Datum paramvalue;
        bool paramisnull;
        char *extval;

        if (cp[1] == '%')
        {
          appendStringInfoChar(&ds, '%');
          cp++;
          continue;
        }

                                                      
        if (current_param == NULL)
        {
          elog(ERROR, "unexpected RAISE parameter list length");
        }

        paramvalue = exec_eval_expr(estate, (PLpgSQL_expr *)lfirst(current_param), &paramisnull, &paramtypeid, &paramtypmod);

        if (paramisnull)
        {
          extval = "<NULL>";
        }
        else
        {
          extval = convert_value_to_string(estate, paramvalue, paramtypeid);
        }
        appendStringInfoString(&ds, extval);
        current_param = lnext(current_param);
        exec_eval_cleanup(estate);
      }
      else
      {
        appendStringInfoChar(&ds, cp[0]);
      }
    }

                                                  
    if (current_param != NULL)
    {
      elog(ERROR, "unexpected RAISE parameter list length");
    }

    err_message = ds.data;
  }

  foreach (lc, stmt->options)
  {
    PLpgSQL_raise_option *opt = (PLpgSQL_raise_option *)lfirst(lc);
    Datum optionvalue;
    bool optionisnull;
    Oid optiontypeid;
    int32 optiontypmod;
    char *extval;

    optionvalue = exec_eval_expr(estate, opt->expr, &optionisnull, &optiontypeid, &optiontypmod);
    if (optionisnull)
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("RAISE statement option cannot be null")));
    }

    extval = convert_value_to_string(estate, optionvalue, optiontypeid);

    switch (opt->opt_type)
    {
    case PLPGSQL_RAISEOPTION_ERRCODE:
      if (err_code)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("RAISE option already specified: %s", "ERRCODE")));
      }
      err_code = plpgsql_recognize_err_condition(extval, true);
      condname = MemoryContextStrdup(stmt_mcontext, extval);
      break;
    case PLPGSQL_RAISEOPTION_MESSAGE:
      SET_RAISE_OPTION_TEXT(err_message, "MESSAGE");
      break;
    case PLPGSQL_RAISEOPTION_DETAIL:
      SET_RAISE_OPTION_TEXT(err_detail, "DETAIL");
      break;
    case PLPGSQL_RAISEOPTION_HINT:
      SET_RAISE_OPTION_TEXT(err_hint, "HINT");
      break;
    case PLPGSQL_RAISEOPTION_COLUMN:
      SET_RAISE_OPTION_TEXT(err_column, "COLUMN");
      break;
    case PLPGSQL_RAISEOPTION_CONSTRAINT:
      SET_RAISE_OPTION_TEXT(err_constraint, "CONSTRAINT");
      break;
    case PLPGSQL_RAISEOPTION_DATATYPE:
      SET_RAISE_OPTION_TEXT(err_datatype, "DATATYPE");
      break;
    case PLPGSQL_RAISEOPTION_TABLE:
      SET_RAISE_OPTION_TEXT(err_table, "TABLE");
      break;
    case PLPGSQL_RAISEOPTION_SCHEMA:
      SET_RAISE_OPTION_TEXT(err_schema, "SCHEMA");
      break;
    default:
      elog(ERROR, "unrecognized raise option: %d", opt->opt_type);
    }

    exec_eval_cleanup(estate);
  }

                                         
  if (err_code == 0 && stmt->elog_level >= ERROR)
  {
    err_code = ERRCODE_RAISE_EXCEPTION;
  }

                                                  
  if (err_message == NULL)
  {
    if (condname)
    {
      err_message = condname;
      condname = NULL;
    }
    else
    {
      err_message = MemoryContextStrdup(stmt_mcontext, unpack_sql_state(err_code));
    }
  }

     
                                                
     
  ereport(stmt->elog_level, (err_code ? errcode(err_code) : 0, errmsg_internal("%s", err_message), (err_detail != NULL) ? errdetail_internal("%s", err_detail) : 0, (err_hint != NULL) ? errhint("%s", err_hint) : 0, (err_column != NULL) ? err_generic_string(PG_DIAG_COLUMN_NAME, err_column) : 0, (err_constraint != NULL) ? err_generic_string(PG_DIAG_CONSTRAINT_NAME, err_constraint) : 0, (err_datatype != NULL) ? err_generic_string(PG_DIAG_DATATYPE_NAME, err_datatype) : 0, (err_table != NULL) ? err_generic_string(PG_DIAG_TABLE_NAME, err_table) : 0, (err_schema != NULL) ? err_generic_string(PG_DIAG_SCHEMA_NAME, err_schema) : 0));

                                  
  MemoryContextReset(stmt_mcontext);

  return PLPGSQL_RC_OK;
}

              
                                       
              
   
static int
exec_stmt_assert(PLpgSQL_execstate *estate, PLpgSQL_stmt_assert *stmt)
{
  bool value;
  bool isnull;

                                               
  if (!plpgsql_check_asserts)
  {
    return PLPGSQL_RC_OK;
  }

  value = exec_eval_boolean(estate, stmt->cond, &isnull);
  exec_eval_cleanup(estate);

  if (isnull || !value)
  {
    char *message = NULL;

    if (stmt->message != NULL)
    {
      Datum val;
      Oid typeid;
      int32 typmod;

      val = exec_eval_expr(estate, stmt->message, &isnull, &typeid, &typmod);
      if (!isnull)
      {
        message = convert_value_to_string(estate, val, typeid);
      }
                                                
    }

    ereport(ERROR, (errcode(ERRCODE_ASSERT_FAILURE), message ? errmsg_internal("%s", message) : errmsg("assertion failed")));
  }

  return PLPGSQL_RC_OK;
}

              
                                             
              
   
static void
plpgsql_estate_setup(PLpgSQL_execstate *estate, PLpgSQL_function *func, ReturnSetInfo *rsi, EState *simple_eval_estate)
{
  HASHCTL ctl;

                                                                    
  func->cur_estate = estate;

  estate->func = func;
  estate->trigdata = NULL;
  estate->evtrigdata = NULL;

  estate->retval = (Datum)0;
  estate->retisnull = true;
  estate->rettype = InvalidOid;

  estate->fn_rettype = func->fn_rettype;
  estate->retistuple = func->fn_retistuple;
  estate->retisset = func->fn_retset;

  estate->readonly_func = func->fn_readonly;
  estate->atomic = true;

  estate->exitlabel = NULL;
  estate->cur_error = NULL;

  estate->tuple_store = NULL;
  estate->tuple_store_desc = NULL;
  if (rsi)
  {
    estate->tuple_store_cxt = rsi->econtext->ecxt_per_query_memory;
    estate->tuple_store_owner = CurrentResourceOwner;
  }
  else
  {
    estate->tuple_store_cxt = NULL;
    estate->tuple_store_owner = NULL;
  }
  estate->rsi = rsi;

  estate->found_varno = func->found_varno;
  estate->ndatums = func->ndatums;
  estate->datums = NULL;
                                                                
  estate->datum_context = CurrentMemoryContext;

                                                                    
  estate->paramLI = makeParamList(0);
  estate->paramLI->paramFetch = plpgsql_param_fetch;
  estate->paramLI->paramFetchArg = (void *)estate;
  estate->paramLI->paramCompile = plpgsql_param_compile;
  estate->paramLI->paramCompileArg = NULL;                 
  estate->paramLI->parserSetup = (ParserSetupHook)plpgsql_parser_setup;
  estate->paramLI->parserSetupArg = NULL;                        
  estate->paramLI->numParams = estate->ndatums;

                                                                            
  if (simple_eval_estate)
  {
    estate->simple_eval_estate = simple_eval_estate;
                                                                 
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(plpgsql_CastHashKey);
    ctl.entrysize = sizeof(plpgsql_CastHashEntry);
    ctl.hcxt = CurrentMemoryContext;
    estate->cast_hash = hash_create("PLpgSQL private cast cache", 16,                             
        &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
    estate->cast_hash_context = CurrentMemoryContext;
  }
  else
  {
    estate->simple_eval_estate = shared_simple_eval_estate;
                                                                           
    if (shared_cast_hash == NULL)
    {
      shared_cast_context = AllocSetContextCreate(TopMemoryContext, "PLpgSQL cast info", ALLOCSET_DEFAULT_SIZES);
      memset(&ctl, 0, sizeof(ctl));
      ctl.keysize = sizeof(plpgsql_CastHashKey);
      ctl.entrysize = sizeof(plpgsql_CastHashEntry);
      ctl.hcxt = shared_cast_context;
      shared_cast_hash = hash_create("PLpgSQL cast cache", 16,                             
          &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
    }
    estate->cast_hash = shared_cast_hash;
    estate->cast_hash_context = shared_cast_context;
  }

     
                                                                         
                                                                          
                                                                             
     
  estate->stmt_mcontext = NULL;
  estate->stmt_mcontext_parent = CurrentMemoryContext;

  estate->eval_tuptable = NULL;
  estate->eval_processed = 0;
  estate->eval_econtext = NULL;

  estate->err_stmt = NULL;
  estate->err_text = NULL;

  estate->plugin_info = NULL;

     
                                                                            
     
  plpgsql_create_econtext(estate);

     
                                                                     
                                                                           
                                                                      
                                           
     
  if (*plpgsql_plugin_ptr)
  {
    (*plpgsql_plugin_ptr)->error_callback = plpgsql_exec_error_callback;
    (*plpgsql_plugin_ptr)->assign_expr = exec_assign_expr;

    if ((*plpgsql_plugin_ptr)->func_setup)
    {
      ((*plpgsql_plugin_ptr)->func_setup)(estate, func);
    }
  }
}

              
                                                                    
   
                                                                           
                                          
   
                                                                          
                                                                           
              
   
static void
exec_eval_cleanup(PLpgSQL_execstate *estate)
{
                                          
  if (estate->eval_tuptable != NULL)
  {
    SPI_freetuptable(estate->eval_tuptable);
  }
  estate->eval_tuptable = NULL;

     
                                                                          
                                                                         
     
  if (estate->eval_econtext != NULL)
  {
    ResetExprContext(estate->eval_econtext);
  }
}

              
                            
   
                                                                            
                                                                            
                                                                          
             
   
                            
     
                                   
                                           
                                       
     
   
                                                                            
                                                                             
                                                                          
              
   
static void
exec_prepare_plan(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, int cursorOptions, bool keepplan)
{
  SPIPlanPtr plan;

     
                                                                            
                                                              
     
  expr->func = estate->func;

     
                                
     
  plan = SPI_prepare_params(expr->query, (ParserSetupHook)plpgsql_parser_setup, (void *)expr, cursorOptions);
  if (plan == NULL)
  {
    elog(ERROR, "SPI_prepare_params failed for \"%s\": %s", expr->query, SPI_result_code_string(SPI_result));
  }
  if (keepplan)
  {
    SPI_keepplan(plan);
  }
  expr->plan = plan;

     
                                                                             
                                                                            
                                             
     
  expr->rwparam = -1;

                                                
  exec_simple_check_plan(estate, expr);
}

              
                                                                      
   
                                                                           
                                                                   
              
   
static int
exec_stmt_execsql(PLpgSQL_execstate *estate, PLpgSQL_stmt_execsql *stmt)
{
  ParamListInfo paramLI;
  long tcount;
  int rc;
  PLpgSQL_expr *expr = stmt->sqlstmt;
  int too_many_rows_level = 0;

  if (plpgsql_extra_errors & PLPGSQL_XCHECK_TOOMANYROWS)
  {
    too_many_rows_level = ERROR;
  }
  else if (plpgsql_extra_warnings & PLPGSQL_XCHECK_TOOMANYROWS)
  {
    too_many_rows_level = WARNING;
  }

     
                                                                        
                                                   
     
  if (expr->plan == NULL)
  {
    exec_prepare_plan(estate, expr, CURSOR_OPT_PARALLEL_OK, true);
  }

  if (!stmt->mod_stmt_set)
  {
    ListCell *l;

    stmt->mod_stmt = false;
    foreach (l, SPI_plan_get_plan_sources(expr->plan))
    {
      CachedPlanSource *plansource = (CachedPlanSource *)lfirst(l);

         
                                                                      
                                                                        
                                                                         
                                                           
         
      if (plansource->commandTag && (strcmp(plansource->commandTag, "INSERT") == 0 || strcmp(plansource->commandTag, "UPDATE") == 0 || strcmp(plansource->commandTag, "DELETE") == 0))
      {
        stmt->mod_stmt = true;
        break;
      }
    }
    stmt->mod_stmt_set = true;
  }

     
                                              
     
  paramLI = setup_param_list(estate, expr);

     
                                                                             
                                                                           
                                                                             
                                                                          
                   
     
                                                                          
                                                                            
                                                                             
                            
     
  if (stmt->into)
  {
    if (stmt->strict || stmt->mod_stmt || too_many_rows_level)
    {
      tcount = 2;
    }
    else
    {
      tcount = 1;
    }
  }
  else
  {
    tcount = 0;
  }

     
                      
     
  rc = SPI_execute_plan_with_paramlist(expr->plan, paramLI, estate->readonly_func, tcount);

     
                                                                           
                                                                      
                                                        
     
  switch (rc)
  {
  case SPI_OK_SELECT:
    Assert(!stmt->mod_stmt);
    exec_set_found(estate, (SPI_processed != 0));
    break;

  case SPI_OK_INSERT:
  case SPI_OK_UPDATE:
  case SPI_OK_DELETE:
  case SPI_OK_INSERT_RETURNING:
  case SPI_OK_UPDATE_RETURNING:
  case SPI_OK_DELETE_RETURNING:
    Assert(stmt->mod_stmt);
    exec_set_found(estate, (SPI_processed != 0));
    break;

  case SPI_OK_SELINTO:
  case SPI_OK_UTILITY:
    Assert(!stmt->mod_stmt);
    break;

  case SPI_OK_REWRITTEN:

       
                                                                    
                                                                     
                                                                    
                                                          
       
    exec_set_found(estate, false);
    break;

                                                         
  case SPI_ERROR_COPY:
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot COPY to/from client in PL/pgSQL")));
    break;

  case SPI_ERROR_TRANSACTION:
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unsupported transaction command in PL/pgSQL")));
    break;

  default:
    elog(ERROR, "SPI_execute_plan_with_paramlist failed executing query \"%s\": %s", expr->query, SPI_result_code_string(rc));
    break;
  }

                                                                
  estate->eval_processed = SPI_processed;

                               
  if (stmt->into)
  {
    SPITupleTable *tuptab = SPI_tuptable;
    uint64 n = SPI_processed;
    PLpgSQL_variable *target;

                                                                 
    if (tuptab == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("INTO used with a command that cannot return data")));
    }

                                    
    target = (PLpgSQL_variable *)estate->datums[stmt->target->dno];

       
                                                                      
                                                                           
                                                   
       
    if (n == 0)
    {
      if (stmt->strict)
      {
        char *errdetail;

        if (estate->func->print_strict_params)
        {
          errdetail = format_expr_params(estate, expr);
        }
        else
        {
          errdetail = NULL;
        }

        ereport(ERROR, (errcode(ERRCODE_NO_DATA_FOUND), errmsg("query returned no rows"), errdetail ? errdetail_internal("parameters: %s", errdetail) : 0));
      }
                                     
      exec_move_row(estate, target, NULL, tuptab->tupdesc);
    }
    else
    {
      if (n > 1 && (stmt->strict || stmt->mod_stmt || too_many_rows_level))
      {
        char *errdetail;
        int errlevel;

        if (estate->func->print_strict_params)
        {
          errdetail = format_expr_params(estate, expr);
        }
        else
        {
          errdetail = NULL;
        }

        errlevel = (stmt->strict || stmt->mod_stmt) ? ERROR : too_many_rows_level;

        ereport(errlevel, (errcode(ERRCODE_TOO_MANY_ROWS), errmsg("query returned more than one row"), errdetail ? errdetail_internal("parameters: %s", errdetail) : 0, errhint("Make sure the query returns a single row, or use LIMIT 1.")));
      }
                                                    
      exec_move_row(estate, target, tuptab->vals[0], tuptab->tupdesc);
    }

                  
    exec_eval_cleanup(estate);
    SPI_freetuptable(SPI_tuptable);
  }
  else
  {
                                                           
    if (SPI_tuptable != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("query has no destination for result data"), (rc == SPI_OK_SELECT) ? errhint("If you want to discard the results of a SELECT, use PERFORM instead.") : 0));
    }
  }

  return PLPGSQL_RC_OK;
}

              
                                                      
                             
              
   
static int
exec_stmt_dynexecute(PLpgSQL_execstate *estate, PLpgSQL_stmt_dynexecute *stmt)
{
  Datum query;
  bool isnull;
  Oid restype;
  int32 restypmod;
  char *querystr;
  int exec_res;
  PreparedParamsData *ppd = NULL;
  MemoryContext stmt_mcontext = get_stmt_mcontext(estate);

     
                                                                            
                                                   
     
  query = exec_eval_expr(estate, stmt->query, &isnull, &restype, &restypmod);
  if (isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("query string argument of EXECUTE is null")));
  }

                                       
  querystr = convert_value_to_string(estate, query, restype);

                                                         
  querystr = MemoryContextStrdup(stmt_mcontext, querystr);

  exec_eval_cleanup(estate);

     
                                                       
     
  if (stmt->params)
  {
    ppd = exec_eval_using_params(estate, stmt->params);
    exec_res = SPI_execute_with_args(querystr, ppd->nargs, ppd->types, ppd->values, ppd->nulls, estate->readonly_func, 0);
  }
  else
  {
    exec_res = SPI_execute(querystr, estate->readonly_func, 0);
  }

  switch (exec_res)
  {
  case SPI_OK_SELECT:
  case SPI_OK_INSERT:
  case SPI_OK_UPDATE:
  case SPI_OK_DELETE:
  case SPI_OK_INSERT_RETURNING:
  case SPI_OK_UPDATE_RETURNING:
  case SPI_OK_DELETE_RETURNING:
  case SPI_OK_UTILITY:
  case SPI_OK_REWRITTEN:
    break;

  case 0:

       
                                                               
                              
       
    break;

  case SPI_OK_SELINTO:

       
                                                                     
                                                                       
                                                                
                                                                       
                                                                   
       
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("EXECUTE of SELECT ... INTO is not implemented"), errhint("You might want to use EXECUTE ... INTO or EXECUTE CREATE TABLE ... AS instead.")));
    break;

                                                         
  case SPI_ERROR_COPY:
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot COPY to/from client in PL/pgSQL")));
    break;

  case SPI_ERROR_TRANSACTION:
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("EXECUTE of transaction commands is not implemented")));
    break;

  default:
    elog(ERROR, "SPI_execute failed executing query \"%s\": %s", querystr, SPI_result_code_string(exec_res));
    break;
  }

                                            
  estate->eval_processed = SPI_processed;

                               
  if (stmt->into)
  {
    SPITupleTable *tuptab = SPI_tuptable;
    uint64 n = SPI_processed;
    PLpgSQL_variable *target;

                                                                 
    if (tuptab == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("INTO used with a command that cannot return data")));
    }

                                    
    target = (PLpgSQL_variable *)estate->datums[stmt->target->dno];

       
                                                                      
                                                                           
                                                   
       
    if (n == 0)
    {
      if (stmt->strict)
      {
        char *errdetail;

        if (estate->func->print_strict_params)
        {
          errdetail = format_preparedparamsdata(estate, ppd);
        }
        else
        {
          errdetail = NULL;
        }

        ereport(ERROR, (errcode(ERRCODE_NO_DATA_FOUND), errmsg("query returned no rows"), errdetail ? errdetail_internal("parameters: %s", errdetail) : 0));
      }
                                     
      exec_move_row(estate, target, NULL, tuptab->tupdesc);
    }
    else
    {
      if (n > 1 && stmt->strict)
      {
        char *errdetail;

        if (estate->func->print_strict_params)
        {
          errdetail = format_preparedparamsdata(estate, ppd);
        }
        else
        {
          errdetail = NULL;
        }

        ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ROWS), errmsg("query returned more than one row"), errdetail ? errdetail_internal("parameters: %s", errdetail) : 0));
      }

                                                    
      exec_move_row(estate, target, tuptab->vals[0], tuptab->tupdesc);
    }
                                        
    exec_eval_cleanup(estate);
  }
  else
  {
       
                                                                       
                                                                        
             
       
  }

                                                                      
  SPI_freetuptable(SPI_tuptable);
  MemoryContextReset(stmt_mcontext);

  return PLPGSQL_RC_OK;
}

              
                                                            
                                    
                                     
               
              
   
static int
exec_stmt_dynfors(PLpgSQL_execstate *estate, PLpgSQL_stmt_dynfors *stmt)
{
  Portal portal;
  int rc;

  portal = exec_dynquery_with_params(estate, stmt->query, stmt->params, NULL, CURSOR_OPT_NO_SCROLL);

     
                      
     
  rc = exec_for_query(estate, (PLpgSQL_stmt_forq *)stmt, portal, true);

     
                               
     
  SPI_cursor_close(portal);

  return rc;
}

              
                                                     
              
   
static int
exec_stmt_open(PLpgSQL_execstate *estate, PLpgSQL_stmt_open *stmt)
{
  PLpgSQL_var *curvar;
  MemoryContext stmt_mcontext = NULL;
  char *curname = NULL;
  PLpgSQL_expr *query;
  Portal portal;
  ParamListInfo paramLI;

                
                                                                   
                                     
                
     
  curvar = (PLpgSQL_var *)(estate->datums[stmt->curvar]);
  if (!curvar->isnull)
  {
    MemoryContext oldcontext;

                                                                   
    stmt_mcontext = get_stmt_mcontext(estate);
    oldcontext = MemoryContextSwitchTo(stmt_mcontext);
    curname = TextDatumGetCString(curvar->value);
    MemoryContextSwitchTo(oldcontext);

    if (SPI_cursor_find(curname) != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_CURSOR), errmsg("cursor \"%s\" already in use", curname)));
    }
  }

                
                                              
                
     
  if (stmt->query != NULL)
  {
                  
                                                
       
                                                                
                        
                  
       
    query = stmt->query;
    if (query->plan == NULL)
    {
      exec_prepare_plan(estate, query, stmt->cursor_options, true);
    }
  }
  else if (stmt->dynquery != NULL)
  {
                  
                                                 
                  
       
    portal = exec_dynquery_with_params(estate, stmt->dynquery, stmt->params, curname, stmt->cursor_options);

       
                                                                           
                                                                           
                                                                          
           
       
    if (curname == NULL)
    {
      assign_text_var(estate, curvar, portal->name);
    }

    return PLPGSQL_RC_OK;
  }
  else
  {
                  
                              
       
                                                                        
                                                                  
                  
       
    if (stmt->argquery != NULL)
    {
                    
                                                               
                                                             
                       
                    
         
      PLpgSQL_stmt_execsql set_args;

      if (curvar->cursor_explicit_argrow < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("arguments given for cursor without arguments")));
      }

      memset(&set_args, 0, sizeof(set_args));
      set_args.cmd_type = PLPGSQL_STMT_EXECSQL;
      set_args.lineno = stmt->lineno;
      set_args.sqlstmt = stmt->argquery;
      set_args.into = true;
                                                     
      set_args.target = (PLpgSQL_variable *)(estate->datums[curvar->cursor_explicit_argrow]);

      if (exec_stmt_execsql(estate, &set_args) != PLPGSQL_RC_OK)
      {
        elog(ERROR, "open cursor failed during argument processing");
      }
    }
    else
    {
      if (curvar->cursor_explicit_argrow >= 0)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("arguments required for cursor")));
      }
    }

    query = curvar->cursor_explicit_expr;
    if (query->plan == NULL)
    {
      exec_prepare_plan(estate, query, curvar->cursor_options, true);
    }
  }

     
                                         
     
  paramLI = setup_param_list(estate, query);

     
                                                                     
     
  portal = SPI_cursor_open_with_paramlist(curname, query->plan, paramLI, estate->readonly_func);
  if (portal == NULL)
  {
    elog(ERROR, "could not open cursor: %s", SPI_result_code_string(SPI_result));
  }

     
                                                                        
     
  if (curname == NULL)
  {
    assign_text_var(estate, curvar, portal->name);
  }

                                                 
  exec_eval_cleanup(estate);
  if (stmt_mcontext)
  {
    MemoryContextReset(stmt_mcontext);
  }

  return PLPGSQL_RC_OK;
}

              
                                                                
                                                 
              
   
static int
exec_stmt_fetch(PLpgSQL_execstate *estate, PLpgSQL_stmt_fetch *stmt)
{
  PLpgSQL_var *curvar;
  long how_many = stmt->how_many;
  SPITupleTable *tuptab;
  Portal portal;
  char *curname;
  uint64 n;
  MemoryContext oldcontext;

                
                                          
                
     
  curvar = (PLpgSQL_var *)(estate->datums[stmt->curvar]);
  if (curvar->isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("cursor variable \"%s\" is null", curvar->refname)));
  }

                                                
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  curname = TextDatumGetCString(curvar->value);
  MemoryContextSwitchTo(oldcontext);

  portal = SPI_cursor_find(curname);
  if (portal == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("cursor \"%s\" does not exist", curname)));
  }

                                                               
  if (stmt->expr)
  {
    bool isnull;

                                                        
    how_many = exec_eval_integer(estate, stmt->expr, &isnull);

    if (isnull)
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("relative or absolute cursor position is null")));
    }

    exec_eval_cleanup(estate);
  }

  if (!stmt->is_move)
  {
    PLpgSQL_variable *target;

                  
                                     
                  
       
    SPI_scroll_cursor_fetch(portal, stmt->direction, how_many);
    tuptab = SPI_tuptable;
    n = SPI_processed;

                  
                                     
                  
       
    target = (PLpgSQL_variable *)estate->datums[stmt->target->dno];
    if (n == 0)
    {
      exec_move_row(estate, target, NULL, tuptab->tupdesc);
    }
    else
    {
      exec_move_row(estate, target, tuptab->vals[0], tuptab->tupdesc);
    }

    exec_eval_cleanup(estate);
    SPI_freetuptable(tuptab);
  }
  else
  {
                         
    SPI_scroll_cursor_move(portal, stmt->direction, how_many);
    n = SPI_processed;
  }

                                                                      
  estate->eval_processed = n;
  exec_set_found(estate, n != 0);

  return PLPGSQL_RC_OK;
}

              
                                    
              
   
static int
exec_stmt_close(PLpgSQL_execstate *estate, PLpgSQL_stmt_close *stmt)
{
  PLpgSQL_var *curvar;
  Portal portal;
  char *curname;
  MemoryContext oldcontext;

                
                                          
                
     
  curvar = (PLpgSQL_var *)(estate->datums[stmt->curvar]);
  if (curvar->isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("cursor variable \"%s\" is null", curvar->refname)));
  }

                                                
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  curname = TextDatumGetCString(curvar->value);
  MemoryContextSwitchTo(oldcontext);

  portal = SPI_cursor_find(curname);
  if (portal == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("cursor \"%s\" does not exist", curname)));
  }

                
                   
                
     
  SPI_cursor_close(portal);

  return PLPGSQL_RC_OK;
}

   
                    
   
                           
   
static int
exec_stmt_commit(PLpgSQL_execstate *estate, PLpgSQL_stmt_commit *stmt)
{
  if (stmt->chain)
  {
    SPI_commit_and_chain();
  }
  else
  {
    SPI_commit();
  }

     
                                                                          
                               
     
  estate->simple_eval_estate = NULL;
  plpgsql_create_econtext(estate);

  return PLPGSQL_RC_OK;
}

   
                      
   
                          
   
static int
exec_stmt_rollback(PLpgSQL_execstate *estate, PLpgSQL_stmt_rollback *stmt)
{
  if (stmt->chain)
  {
    SPI_rollback_and_chain();
  }
  else
  {
    SPI_rollback();
  }

     
                                                                          
                               
     
  estate->simple_eval_estate = NULL;
  plpgsql_create_econtext(estate);

  return PLPGSQL_RC_OK;
}

   
                 
   
                                
   
                                                                          
                                                                
                                                                              
   
static int
exec_stmt_set(PLpgSQL_execstate *estate, PLpgSQL_stmt_set *stmt)
{
  PLpgSQL_expr *expr = stmt->expr;
  int rc;

  if (expr->plan == NULL)
  {
    exec_prepare_plan(estate, expr, 0, true);
  }

  rc = SPI_execute_plan(expr->plan, NULL, NULL, estate->readonly_func, 0);

  if (rc != SPI_OK_UTILITY)
  {
    elog(ERROR, "SPI_execute_plan failed executing query \"%s\": %s", expr->query, SPI_result_code_string(rc));
  }

  return PLPGSQL_RC_OK;
}

              
                                                                  
              
   
static void
exec_assign_expr(PLpgSQL_execstate *estate, PLpgSQL_datum *target, PLpgSQL_expr *expr)
{
  Datum value;
  bool isnull;
  Oid valtype;
  int32 valtypmod;

     
                                                                            
                                                                         
                                                                            
                                                 
     
                                                                       
                                                                     
                                                                             
                                                                    
                                                                       
                                                                         
                                     
     
  if (expr->plan == NULL)
  {
    exec_prepare_plan(estate, expr, 0, true);
    if (target->dtype == PLPGSQL_DTYPE_VAR)
    {
      exec_check_rw_parameter(expr, target->dno);
    }
  }

  value = exec_eval_expr(estate, expr, &isnull, &valtype, &valtypmod);
  exec_assign_value(estate, target, value, isnull, valtype, valtypmod);
  exec_eval_cleanup(estate);
}

              
                                                              
   
                                                                    
   
                                                                      
                            
              
   
static void
exec_assign_c_string(PLpgSQL_execstate *estate, PLpgSQL_datum *target, const char *str)
{
  text *value;
  MemoryContext oldcontext;

                                                    
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  if (str != NULL)
  {
    value = cstring_to_text(str);
  }
  else
  {
    value = cstring_to_text("");
  }
  MemoryContextSwitchTo(oldcontext);

  exec_assign_value(estate, target, PointerGetDatum(value), false, TEXTOID, -1);
}

              
                                                       
   
                                                                         
                                                                            
                                                                             
              
   
static void
exec_assign_value(PLpgSQL_execstate *estate, PLpgSQL_datum *target, Datum value, bool isNull, Oid valtype, int32 valtypmod)
{
  switch (target->dtype)
  {
  case PLPGSQL_DTYPE_VAR:
  case PLPGSQL_DTYPE_PROMISE:
  {
       
                            
       
    PLpgSQL_var *var = (PLpgSQL_var *)target;
    Datum newvalue;

    newvalue = exec_cast_value(estate, value, &isNull, valtype, valtypmod, var->datatype->typoid, var->datatype->atttypmod);

    if (isNull && var->notnull)
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("null value cannot be assigned to variable \"%s\" declared NOT NULL", var->refname)));
    }

       
                                                             
                                                                
                                                                 
                                                                  
                                                        
       
                                                                
                                                                  
                                                                   
                                                               
                                                              
                                                                 
                      
       
    if (!var->datatype->typbyval && !isNull)
    {
      if (var->datatype->typisarray && !VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(newvalue)))
      {
                                                              
        newvalue = expand_array(newvalue, estate->datum_context, NULL);
      }
      else
      {
                                                             
        newvalue = datumTransfer(newvalue, false, var->datatype->typlen);
      }
    }

       
                                                                   
                                                               
                                                                  
                                                                  
                                                               
                               
       
                                                              
                                                               
                                                                
       
    if (var->value != newvalue || var->isnull || isNull)
    {
      assign_simple_var(estate, var, newvalue, isNull, (!var->datatype->typbyval && !isNull));
    }
    else
    {
      var->promise = PLPGSQL_PROMISE_NONE;
    }
    break;
  }

  case PLPGSQL_DTYPE_ROW:
  {
       
                                
       
    PLpgSQL_row *row = (PLpgSQL_row *)target;

    if (isNull)
    {
                                                           
      exec_move_row(estate, (PLpgSQL_variable *)row, NULL, NULL);
    }
    else
    {
                                                      
      if (!type_is_rowtype(valtype))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot assign non-composite value to a row variable")));
      }
      exec_move_row_from_datum(estate, (PLpgSQL_variable *)row, value);
    }
    break;
  }

  case PLPGSQL_DTYPE_REC:
  {
       
                                   
       
    PLpgSQL_rec *rec = (PLpgSQL_rec *)target;

    if (isNull)
    {
      if (rec->notnull)
      {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("null value cannot be assigned to variable \"%s\" declared NOT NULL", rec->refname)));
      }

                                         
      exec_move_row(estate, (PLpgSQL_variable *)rec, NULL, NULL);
    }
    else
    {
                                                      
      if (!type_is_rowtype(valtype))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot assign non-composite value to a record variable")));
      }
      exec_move_row_from_datum(estate, (PLpgSQL_variable *)rec, value);
    }
    break;
  }

  case PLPGSQL_DTYPE_RECFIELD:
  {
       
                                     
       
    PLpgSQL_recfield *recfield = (PLpgSQL_recfield *)target;
    PLpgSQL_rec *rec;
    ExpandedRecordHeader *erh;

    rec = (PLpgSQL_rec *)(estate->datums[recfield->recparentno]);
    erh = rec->erh;

       
                                                              
                                                                
                                                               
                                                                   
       
    if (erh == NULL)
    {
      instantiate_empty_record_variable(estate, rec);
      erh = rec->erh;
    }

       
                                                                 
                                                           
       
    if (unlikely(recfield->rectupledescid != erh->er_tupdesc_id))
    {
      if (!expanded_record_lookup_field(erh, recfield->fieldname, &recfield->finfo))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("record \"%s\" has no field \"%s\"", rec->refname, recfield->fieldname)));
      }
      recfield->rectupledescid = erh->er_tupdesc_id;
    }

                                                         
    if (recfield->finfo.fnumber <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot assign to system column \"%s\"", recfield->fieldname)));
    }

                                                          
    value = exec_cast_value(estate, value, &isNull, valtype, valtypmod, recfield->finfo.ftypeid, recfield->finfo.ftypmod);

                        
    expanded_record_set_field(erh, recfield->finfo.fnumber, value, isNull, !estate->atomic);
    break;
  }

  case PLPGSQL_DTYPE_ARRAYELEM:
  {
       
                                        
       
    PLpgSQL_arrayelem *arrayelem;
    int nsubscripts;
    int i;
    PLpgSQL_expr *subscripts[MAXDIM];
    int subscriptvals[MAXDIM];
    Datum oldarraydatum, newarraydatum, coerced_value;
    bool oldarrayisnull;
    Oid parenttypoid;
    int32 parenttypmod;
    SPITupleTable *save_eval_tuptable;
    MemoryContext oldcontext;

       
                                                               
                                                                 
                                                                   
                                                               
            
       
    save_eval_tuptable = estate->eval_tuptable;
    estate->eval_tuptable = NULL;

       
                                                                  
                                                                   
                                                                 
                                                                   
                                                            
                                                                  
                                                                
                                
       
    nsubscripts = 0;
    do
    {
      arrayelem = (PLpgSQL_arrayelem *)target;
      if (nsubscripts >= MAXDIM)
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("number of array dimensions (%d) exceeds the maximum allowed (%d)", nsubscripts + 1, MAXDIM)));
      }
      subscripts[nsubscripts++] = arrayelem->subscript;
      target = estate->datums[arrayelem->arrayparentno];
    } while (target->dtype == PLPGSQL_DTYPE_ARRAYELEM);

                                            
    exec_eval_datum(estate, target, &parenttypoid, &parenttypmod, &oldarraydatum, &oldarrayisnull);

                                              
    if (arrayelem->parenttypoid != parenttypoid || arrayelem->parenttypmod != parenttypmod)
    {
      Oid arraytypoid;
      int32 arraytypmod = parenttypmod;
      int16 arraytyplen;
      Oid elemtypoid;
      int16 elemtyplen;
      bool elemtypbyval;
      char elemtypalign;

                                                               
      arraytypoid = getBaseTypeAndTypmod(parenttypoid, &arraytypmod);

                                             
      elemtypoid = get_element_type(arraytypoid);
      if (!OidIsValid(elemtypoid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("subscripted object is not an array")));
      }

                                               
      arraytyplen = get_typlen(arraytypoid);

      get_typlenbyvalalign(elemtypoid, &elemtyplen, &elemtypbyval, &elemtypalign);

                                              
      arrayelem->parenttypoid = parenttypoid;
      arrayelem->parenttypmod = parenttypmod;
      arrayelem->arraytypoid = arraytypoid;
      arrayelem->arraytypmod = arraytypmod;
      arrayelem->arraytyplen = arraytyplen;
      arrayelem->elemtypoid = elemtypoid;
      arrayelem->elemtyplen = elemtyplen;
      arrayelem->elemtypbyval = elemtypbyval;
      arrayelem->elemtypalign = elemtypalign;
    }

       
                                                                 
                                                               
                                          
       
    for (i = 0; i < nsubscripts; i++)
    {
      bool subisnull;

      subscriptvals[i] = exec_eval_integer(estate, subscripts[nsubscripts - 1 - i], &subisnull);
      if (subisnull)
      {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("array subscript in assignment must not be null")));
      }

         
                                                          
                                                              
                                                                
                                                              
                                                                 
         
      if (estate->eval_tuptable != NULL)
      {
        SPI_freetuptable(estate->eval_tuptable);
      }
      estate->eval_tuptable = NULL;
    }

                                                                
    Assert(estate->eval_tuptable == NULL);
    estate->eval_tuptable = save_eval_tuptable;

                                                          
    coerced_value = exec_cast_value(estate, value, &isNull, valtype, valtypmod, arrayelem->elemtypoid, arrayelem->arraytypmod);

       
                                                                
                                                         
                                                         
                                                                   
                                                                  
                                                              
                                                                   
                                                           
       
    if (arrayelem->arraytyplen > 0 &&                          
        (oldarrayisnull || isNull))
    {
      return;
    }

                                                                
    oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));

    if (oldarrayisnull)
    {
      oldarraydatum = PointerGetDatum(construct_empty_array(arrayelem->elemtypoid));
    }

       
                                       
       
    newarraydatum = array_set_element(oldarraydatum, nsubscripts, subscriptvals, coerced_value, isNull, arrayelem->arraytyplen, arrayelem->elemtyplen, arrayelem->elemtypbyval, arrayelem->elemtypalign);

    MemoryContextSwitchTo(oldcontext);

       
                                                                   
                                                            
                                                               
                                        
       
    exec_assign_value(estate, target, newarraydatum, false, arrayelem->arraytypoid, arrayelem->arraytypmod);
    break;
  }

  default:
    elog(ERROR, "unrecognized dtype: %d", target->dtype);
  }
}

   
                                                           
   
                                                                            
   
                                                                            
                                                                             
   
                                                                            
                                                                           
                                                                             
                                                                     
                                                        
   
                                                                            
                                       
   
static void
exec_eval_datum(PLpgSQL_execstate *estate, PLpgSQL_datum *datum, Oid *typeid, int32 *typetypmod, Datum *value, bool *isnull)
{
  MemoryContext oldcontext;

  switch (datum->dtype)
  {
  case PLPGSQL_DTYPE_PROMISE:
                                                                 
    plpgsql_fulfill_promise(estate, (PLpgSQL_var *)datum);

                   

  case PLPGSQL_DTYPE_VAR:
  {
    PLpgSQL_var *var = (PLpgSQL_var *)datum;

    *typeid = var->datatype->typoid;
    *typetypmod = var->datatype->atttypmod;
    *value = var->value;
    *isnull = var->isnull;
    break;
  }

  case PLPGSQL_DTYPE_ROW:
  {
    PLpgSQL_row *row = (PLpgSQL_row *)datum;
    HeapTuple tup;

                                                          
    if (!row->rowtupdesc)                        
    {
      elog(ERROR, "row variable has no tupdesc");
    }
                                                       
    BlessTupleDesc(row->rowtupdesc);
    oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
    tup = make_tuple_from_row(estate, row, row->rowtupdesc);
    if (tup == NULL)                        
    {
      elog(ERROR, "row not compatible with its own tupdesc");
    }
    *typeid = row->rowtupdesc->tdtypeid;
    *typetypmod = row->rowtupdesc->tdtypmod;
    *value = HeapTupleGetDatum(tup);
    *isnull = false;
    MemoryContextSwitchTo(oldcontext);
    break;
  }

  case PLPGSQL_DTYPE_REC:
  {
    PLpgSQL_rec *rec = (PLpgSQL_rec *)datum;

    if (rec->erh == NULL)
    {
                                                        
      *value = (Datum)0;
      *isnull = true;
                                           
      *typeid = rec->rectypeid;
      *typetypmod = -1;
    }
    else
    {
      if (ExpandedRecordIsEmpty(rec->erh))
      {
                                         
        *value = (Datum)0;
        *isnull = true;
      }
      else
      {
        *value = ExpandedRecordGetDatum(rec->erh);
        *isnull = false;
      }
      if (rec->rectypeid != RECORDOID)
      {
                                                            
        *typeid = rec->rectypeid;
        *typetypmod = -1;
      }
      else
      {
                                                            
        *typeid = rec->erh->er_typeid;
        *typetypmod = rec->erh->er_typmod;
      }
    }
    break;
  }

  case PLPGSQL_DTYPE_RECFIELD:
  {
    PLpgSQL_recfield *recfield = (PLpgSQL_recfield *)datum;
    PLpgSQL_rec *rec;
    ExpandedRecordHeader *erh;

    rec = (PLpgSQL_rec *)(estate->datums[recfield->recparentno]);
    erh = rec->erh;

       
                                                              
                                                                
                                                          
       
    if (erh == NULL)
    {
      instantiate_empty_record_variable(estate, rec);
      erh = rec->erh;
    }

       
                                                                 
                                                           
       
    if (unlikely(recfield->rectupledescid != erh->er_tupdesc_id))
    {
      if (!expanded_record_lookup_field(erh, recfield->fieldname, &recfield->finfo))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("record \"%s\" has no field \"%s\"", rec->refname, recfield->fieldname)));
      }
      recfield->rectupledescid = erh->er_tupdesc_id;
    }

                           
    *typeid = recfield->finfo.ftypeid;
    *typetypmod = recfield->finfo.ftypmod;

                                    
    *value = expanded_record_get_field(erh, recfield->finfo.fnumber, isnull);
    break;
  }

  default:
    elog(ERROR, "unrecognized dtype: %d", datum->dtype);
  }
}

   
                                                                  
   
                                                                       
                                                                       
   
Oid
plpgsql_exec_get_datum_type(PLpgSQL_execstate *estate, PLpgSQL_datum *datum)
{
  Oid typeid;

  switch (datum->dtype)
  {
  case PLPGSQL_DTYPE_VAR:
  case PLPGSQL_DTYPE_PROMISE:
  {
    PLpgSQL_var *var = (PLpgSQL_var *)datum;

    typeid = var->datatype->typoid;
    break;
  }

  case PLPGSQL_DTYPE_REC:
  {
    PLpgSQL_rec *rec = (PLpgSQL_rec *)datum;

    if (rec->erh == NULL || rec->rectypeid != RECORDOID)
    {
                                           
      typeid = rec->rectypeid;
    }
    else
    {
                                                          
      typeid = rec->erh->er_typeid;
    }
    break;
  }

  case PLPGSQL_DTYPE_RECFIELD:
  {
    PLpgSQL_recfield *recfield = (PLpgSQL_recfield *)datum;
    PLpgSQL_rec *rec;

    rec = (PLpgSQL_rec *)(estate->datums[recfield->recparentno]);

       
                                                              
                                                                
                                                          
       
    if (rec->erh == NULL)
    {
      instantiate_empty_record_variable(estate, rec);
    }

       
                                                                 
                                                           
       
    if (unlikely(recfield->rectupledescid != rec->erh->er_tupdesc_id))
    {
      if (!expanded_record_lookup_field(rec->erh, recfield->fieldname, &recfield->finfo))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("record \"%s\" has no field \"%s\"", rec->refname, recfield->fieldname)));
      }
      recfield->rectupledescid = rec->erh->er_tupdesc_id;
    }

    typeid = recfield->finfo.ftypeid;
    break;
  }

  default:
    elog(ERROR, "unrecognized dtype: %d", datum->dtype);
    typeid = InvalidOid;                          
    break;
  }

  return typeid;
}

   
                                                                          
   
                                                                                
                                                                             
                                                                
   
void
plpgsql_exec_get_datum_type_info(PLpgSQL_execstate *estate, PLpgSQL_datum *datum, Oid *typeId, int32 *typMod, Oid *collation)
{
  switch (datum->dtype)
  {
  case PLPGSQL_DTYPE_VAR:
  case PLPGSQL_DTYPE_PROMISE:
  {
    PLpgSQL_var *var = (PLpgSQL_var *)datum;

    *typeId = var->datatype->typoid;
    *typMod = var->datatype->atttypmod;
    *collation = var->datatype->collation;
    break;
  }

  case PLPGSQL_DTYPE_REC:
  {
    PLpgSQL_rec *rec = (PLpgSQL_rec *)datum;

    if (rec->erh == NULL || rec->rectypeid != RECORDOID)
    {
                                           
      *typeId = rec->rectypeid;
      *typMod = -1;
    }
    else
    {
                                                          
      *typeId = rec->erh->er_typeid;
                                                                 
      *typMod = -1;
    }
                                              
    *collation = InvalidOid;
    break;
  }

  case PLPGSQL_DTYPE_RECFIELD:
  {
    PLpgSQL_recfield *recfield = (PLpgSQL_recfield *)datum;
    PLpgSQL_rec *rec;

    rec = (PLpgSQL_rec *)(estate->datums[recfield->recparentno]);

       
                                                              
                                                                
                                                          
       
    if (rec->erh == NULL)
    {
      instantiate_empty_record_variable(estate, rec);
    }

       
                                                                 
                                                           
       
    if (unlikely(recfield->rectupledescid != rec->erh->er_tupdesc_id))
    {
      if (!expanded_record_lookup_field(rec->erh, recfield->fieldname, &recfield->finfo))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("record \"%s\" has no field \"%s\"", rec->refname, recfield->fieldname)));
      }
      recfield->rectupledescid = rec->erh->er_tupdesc_id;
    }

    *typeId = recfield->finfo.ftypeid;
    *typMod = recfield->finfo.ftypmod;
    *collation = recfield->finfo.fcollation;
    break;
  }

  default:
    elog(ERROR, "unrecognized dtype: %d", datum->dtype);
    *typeId = InvalidOid;                          
    *typMod = -1;
    *collation = InvalidOid;
    break;
  }
}

              
                                                                    
   
                                                                      
                                                                        
                                                                        
                                      
              
   
static int
exec_eval_integer(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, bool *isNull)
{
  Datum exprdatum;
  Oid exprtypeid;
  int32 exprtypmod;

  exprdatum = exec_eval_expr(estate, expr, isNull, &exprtypeid, &exprtypmod);
  exprdatum = exec_cast_value(estate, exprdatum, isNull, exprtypeid, exprtypmod, INT4OID, -1);
  return DatumGetInt32(exprdatum);
}

              
                                                                    
   
                                                                      
                     
              
   
static bool
exec_eval_boolean(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, bool *isNull)
{
  Datum exprdatum;
  Oid exprtypeid;
  int32 exprtypmod;

  exprdatum = exec_eval_expr(estate, expr, isNull, &exprtypeid, &exprtypmod);
  exprdatum = exec_cast_value(estate, exprdatum, isNull, exprtypeid, exprtypmod, BOOLOID, -1);
  return DatumGetBool(exprdatum);
}

              
                                                      
                                                      
   
                                                                    
              
   
static Datum
exec_eval_expr(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, bool *isNull, Oid *rettype, int32 *rettypmod)
{
  Datum result = 0;
  int rc;
  Form_pg_attribute attr;

     
                                                               
     
  if (expr->plan == NULL)
  {
    exec_prepare_plan(estate, expr, CURSOR_OPT_PARALLEL_OK, true);
  }

     
                                                                     
              
     
  if (exec_eval_simple_expr(estate, expr, &result, isNull, rettype, rettypmod))
  {
    return result;
  }

     
                                                 
     
  rc = exec_run_select(estate, expr, 2, NULL);
  if (rc != SPI_OK_SELECT)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("query \"%s\" did not return data", expr->query)));
  }

     
                                                             
     
  if (estate->eval_tuptable->tupdesc->natts != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg_plural("query \"%s\" returned %d column", "query \"%s\" returned %d columns", estate->eval_tuptable->tupdesc->natts, expr->query, estate->eval_tuptable->tupdesc->natts)));
  }

     
                                        
     
  attr = TupleDescAttr(estate->eval_tuptable->tupdesc, 0);
  *rettype = attr->atttypid;
  *rettypmod = attr->atttypmod;

     
                                                                       
     
  if (estate->eval_processed == 0)
  {
    *isNull = true;
    return (Datum)0;
  }

     
                                                              
     
  if (estate->eval_processed != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("query \"%s\" returned more than one row", expr->query)));
  }

     
                                     
     
  return SPI_getbinval(estate->eval_tuptable->vals[0], estate->eval_tuptable->tupdesc, 1, isNull);
}

              
                                            
              
   
static int
exec_run_select(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, long maxtuples, Portal *portalP)
{
  ParamListInfo paramLI;
  int rc;

     
                                                              
     
                                                                           
                                                                             
                                                                       
                                                                           
                                                                             
                                                                             
                                                              
     
  if (expr->plan == NULL)
  {
    int cursorOptions = CURSOR_OPT_NO_SCROLL;

    if (portalP == NULL)
    {
      cursorOptions |= CURSOR_OPT_PARALLEL_OK;
    }
    exec_prepare_plan(estate, expr, cursorOptions, true);
  }

     
                                              
     
  paramLI = setup_param_list(estate, expr);

     
                                                                            
     
  if (portalP != NULL)
  {
    *portalP = SPI_cursor_open_with_paramlist(NULL, expr->plan, paramLI, estate->readonly_func);
    if (*portalP == NULL)
    {
      elog(ERROR, "could not open implicit cursor for query \"%s\": %s", expr->query, SPI_result_code_string(SPI_result));
    }
    exec_eval_cleanup(estate);
    return SPI_OK_CURSOR;
  }

     
                       
     
  rc = SPI_execute_plan_with_paramlist(expr->plan, paramLI, estate->readonly_func, maxtuples);
  if (rc != SPI_OK_SELECT)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("query \"%s\" is not a SELECT", expr->query)));
  }

                                               
  Assert(estate->eval_tuptable == NULL);
  estate->eval_tuptable = SPI_tuptable;
  estate->eval_processed = SPI_processed;

  return rc;
}

   
                                                                          
   
                                                                
   
static int
exec_for_query(PLpgSQL_execstate *estate, PLpgSQL_stmt_forq *stmt, Portal portal, bool prefetch_ok)
{
  PLpgSQL_variable *var;
  SPITupleTable *tuptab;
  bool found = false;
  int rc = PLPGSQL_RC_OK;
  uint64 previous_id = INVALID_TUPLEDESC_IDENTIFIER;
  bool tupdescs_match = true;
  uint64 n;

                                         
  var = (PLpgSQL_variable *)estate->datums[stmt->var->dno];

     
                                                                       
              
     
  PinPortal(portal);

     
                                                                     
                                                                         
                                                                       
                                                                          
                                                                        
                                                             
     
  if (!estate->atomic)
  {
    prefetch_ok = false;
  }

     
                                                                           
                                                                    
               
     
  SPI_cursor_fetch(portal, true, prefetch_ok ? 10 : 1);
  tuptab = SPI_tuptable;
  n = SPI_processed;

     
                                                                          
                                 
     
  if (n == 0)
  {
    exec_move_row(estate, var, NULL, tuptab->tupdesc);
    exec_eval_cleanup(estate);
  }
  else
  {
    found = true;                                   
  }

     
                     
     
  while (n > 0)
  {
    uint64 i;

    for (i = 0; i < n; i++)
    {
         
                                                                         
                                                                      
                                                                   
                                                                      
                                                                  
                                                                         
                                                                      
                                                                    
                                                                     
         
      if (var->dtype == PLPGSQL_DTYPE_REC)
      {
        PLpgSQL_rec *rec = (PLpgSQL_rec *)var;

        if (rec->erh && rec->erh->er_tupdesc_id == previous_id && tupdescs_match)
        {
                                                     
          expanded_record_set_tuple(rec->erh, tuptab->vals[i], true, !estate->atomic);
        }
        else
        {
             
                                                                   
                                                                    
                        
             
          exec_move_row(estate, var, tuptab->vals[i], tuptab->tupdesc);

             
                                                                  
                                                                   
                                                              
             
          if (tupdescs_match)
          {
            tupdescs_match = (rec->rectypeid == RECORDOID || rec->rectypeid == tuptab->tupdesc->tdtypeid || compatible_tupdescs(tuptab->tupdesc, expanded_record_get_tupdesc(rec->erh)));
          }
          previous_id = rec->erh->er_tupdesc_id;
        }
      }
      else
      {
        exec_move_row(estate, var, tuptab->vals[i], tuptab->tupdesc);
      }

      exec_eval_cleanup(estate);

         
                                
         
      rc = exec_stmts(estate, stmt->body);

      LOOP_RC_PROCESSING(stmt->label, goto loop_exit);
    }

    SPI_freetuptable(tuptab);

       
                                                                         
       
    SPI_cursor_fetch(portal, true, prefetch_ok ? 50 : 1);
    tuptab = SPI_tuptable;
    n = SPI_processed;
  }

loop_exit:

     
                                           
     
  SPI_freetuptable(tuptab);

  UnpinPortal(portal);

     
                                                                         
                                                                             
                                                                            
                                 
     
  exec_set_found(estate, found);

  return rc;
}

              
                                                                   
                                                      
   
                                                                            
                                                                               
                 
   
                                                                        
                                                                         
                                                                            
                                                                        
                                                                           
                                                                             
                                                                           
                                                                         
                             
   
                                                                             
                                                                               
                                                                               
                                                                        
                                                                              
                        
   
                                                                   
                                                    
              
   
static bool
exec_eval_simple_expr(PLpgSQL_execstate *estate, PLpgSQL_expr *expr, Datum *result, bool *isNull, Oid *rettype, int32 *rettypmod)
{
  ExprContext *econtext = estate->eval_econtext;
  LocalTransactionId curlxid = MyProc->lxid;
  CachedPlan *cplan;
  void *save_setup_arg;
  MemoryContext oldcontext;

     
                                                   
     
  if (expr->expr_simple_expr == NULL)
  {
    return false;
  }

     
                                                              
     
  if (expr->expr_simple_in_use && expr->expr_simple_lxid == curlxid)
  {
    return false;
  }

     
                                                                      
                                                                         
                                                                        
                                                                            
                                  
     
  EnsurePortalSnapshotExists();

     
                                                                            
                                                                           
                                                   
     
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  cplan = SPI_plan_get_cached_plan(expr->plan);
  MemoryContextSwitchTo(oldcontext);

     
                                                                             
                                                                            
                                                                  
     
  Assert(cplan != NULL);

                                                                     
  if (cplan->generation != expr->expr_simple_generation)
  {
    exec_save_simple_expr(expr, cplan);
                                                                       
    if (expr->rwparam >= 0)
    {
      exec_check_rw_parameter(expr, expr->rwparam);
    }
  }

     
                                                  
     
  *rettype = expr->expr_simple_type;
  *rettypmod = expr->expr_simple_typmod;

     
                                                                             
                                                                       
     
  save_setup_arg = estate->paramLI->parserSetupArg;

  econtext->ecxt_param_list_info = setup_param_list(estate, expr);

     
                                                                            
                                                                           
                                   
     
  if (expr->expr_simple_lxid != curlxid)
  {
    oldcontext = MemoryContextSwitchTo(estate->simple_eval_estate->es_query_cxt);
    expr->expr_simple_state = ExecInitExprWithParams(expr->expr_simple_expr, econtext->ecxt_param_list_info);
    expr->expr_simple_in_use = false;
    expr->expr_simple_lxid = curlxid;
    MemoryContextSwitchTo(oldcontext);
  }

     
                                                                    
                                                                            
                                                                            
                                              
     
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  if (!estate->readonly_func)
  {
    CommandCounterIncrement();
    PushActiveSnapshot(GetTransactionSnapshot());
  }

     
                                                                        
     
  expr->expr_simple_in_use = true;

     
                                                                 
     
  *result = ExecEvalExpr(expr->expr_simple_state, econtext, isNull);

                        
  expr->expr_simple_in_use = false;

  econtext->ecxt_param_list_info = NULL;

  estate->paramLI->parserSetupArg = save_setup_arg;

  if (!estate->readonly_func)
  {
    PopActiveSnapshot();
  }

  MemoryContextSwitchTo(oldcontext);

     
                                                         
     
  ReleaseCachedPlan(cplan, true);

     
                
     
  return true;
}

   
                                         
   
                                                                         
                                                                       
                                  
   
                                                                           
                                                                           
                                                                          
                                                                             
                                                                        
                                                                           
                                       
   
static ParamListInfo
setup_param_list(PLpgSQL_execstate *estate, PLpgSQL_expr *expr)
{
  ParamListInfo paramLI;

     
                                                                          
                                                                            
     
  Assert(expr->plan != NULL);

     
                                                                        
                                                                         
                                                                             
                                                                 
     
  if (expr->paramnos)
  {
                                      
    paramLI = estate->paramLI;

       
                                                                        
                                                                           
                                                               
       
    paramLI->parserSetupArg = (void *)expr;

       
                                                                         
                                                                           
                                                         
       
    expr->func = estate->func;
  }
  else
  {
       
                                                                          
                                                                      
                               
       
    paramLI = NULL;
  }
  return paramLI;
}

   
                                                                        
   
                                                                          
   
                                                                           
                                                                            
                                                                      
                           
   
static ParamExternData *
plpgsql_param_fetch(ParamListInfo params, int paramid, bool speculative, ParamExternData *prm)
{
  int dno;
  PLpgSQL_execstate *estate;
  PLpgSQL_expr *expr;
  PLpgSQL_datum *datum;
  bool ok = true;
  int32 prmtypmod;

                                                   
  dno = paramid - 1;
  Assert(dno >= 0 && dno < params->numParams);

                                
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  expr = (PLpgSQL_expr *)params->parserSetupArg;
  Assert(params->numParams == estate->ndatums);

                                          
  datum = estate->datums[dno];

     
                                                                           
                                                                         
                                                                       
                                                                      
                                             
     
  if (!bms_is_member(dno, expr->paramnos))
  {
    ok = false;
  }

     
                                                                           
                                                                    
     
  else if (speculative)
  {
    switch (datum->dtype)
    {
    case PLPGSQL_DTYPE_VAR:
    case PLPGSQL_DTYPE_PROMISE:
                       
      break;

    case PLPGSQL_DTYPE_ROW:
                                                   
      break;

    case PLPGSQL_DTYPE_REC:
                                                        
      break;

    case PLPGSQL_DTYPE_RECFIELD:
    {
      PLpgSQL_recfield *recfield = (PLpgSQL_recfield *)datum;
      PLpgSQL_rec *rec;

      rec = (PLpgSQL_rec *)(estate->datums[recfield->recparentno]);

         
                                                          
         
      if (rec->erh == NULL)
      {
        ok = false;
      }

         
                                                                
                                                                
         
      else if (unlikely(recfield->rectupledescid != rec->erh->er_tupdesc_id))
      {
        if (expanded_record_lookup_field(rec->erh, recfield->fieldname, &recfield->finfo))
        {
          recfield->rectupledescid = rec->erh->er_tupdesc_id;
        }
        else
        {
          ok = false;
        }
      }
      break;
    }

    default:
      ok = false;
      break;
    }
  }

                                            
  if (!ok)
  {
    prm->value = (Datum)0;
    prm->isnull = true;
    prm->pflags = 0;
    prm->ptype = InvalidOid;
    return prm;
  }

                                                               
  exec_eval_datum(estate, datum, &prm->ptype, &prmtypmod, &prm->value, &prm->isnull);
                                                                    
  prm->pflags = PARAM_FLAG_CONST;

     
                                                                          
                                             
     
  if (dno != expr->rwparam)
  {
    if (datum->dtype == PLPGSQL_DTYPE_VAR)
    {
      prm->value = MakeExpandedObjectReadOnly(prm->value, prm->isnull, ((PLpgSQL_var *)datum)->datatype->typlen);
    }
    else if (datum->dtype == PLPGSQL_DTYPE_REC)
    {
      prm->value = MakeExpandedObjectReadOnly(prm->value, prm->isnull, -1);
    }
  }

  return prm;
}

   
                                                                       
   
static void
plpgsql_param_compile(ParamListInfo params, Param *param, ExprState *state, Datum *resv, bool *resnull)
{
  PLpgSQL_execstate *estate;
  PLpgSQL_expr *expr;
  int dno;
  PLpgSQL_datum *datum;
  ExprEvalStep scratch;

                                
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  expr = (PLpgSQL_expr *)params->parserSetupArg;

                                                   
  dno = param->paramid - 1;
  Assert(dno >= 0 && dno < estate->ndatums);

                                          
  datum = estate->datums[dno];

  scratch.opcode = EEOP_PARAM_CALLBACK;
  scratch.resvalue = resv;
  scratch.resnull = resnull;

     
                                                                      
                                                                           
                                                                       
                                                                         
                       
     
  if (datum->dtype == PLPGSQL_DTYPE_VAR)
  {
    if (dno != expr->rwparam && ((PLpgSQL_var *)datum)->datatype->typlen == -1)
    {
      scratch.d.cparam.paramfunc = plpgsql_param_eval_var_ro;
    }
    else
    {
      scratch.d.cparam.paramfunc = plpgsql_param_eval_var;
    }
  }
  else if (datum->dtype == PLPGSQL_DTYPE_RECFIELD)
  {
    scratch.d.cparam.paramfunc = plpgsql_param_eval_recfield;
  }
  else if (datum->dtype == PLPGSQL_DTYPE_PROMISE)
  {
    if (dno != expr->rwparam && ((PLpgSQL_var *)datum)->datatype->typlen == -1)
    {
      scratch.d.cparam.paramfunc = plpgsql_param_eval_generic_ro;
    }
    else
    {
      scratch.d.cparam.paramfunc = plpgsql_param_eval_generic;
    }
  }
  else if (datum->dtype == PLPGSQL_DTYPE_REC && dno != expr->rwparam)
  {
    scratch.d.cparam.paramfunc = plpgsql_param_eval_generic_ro;
  }
  else
  {
    scratch.d.cparam.paramfunc = plpgsql_param_eval_generic;
  }

     
                                                                         
                                                                         
                                                                     
                                                       
     
  scratch.d.cparam.paramarg = NULL;
  scratch.d.cparam.paramid = param->paramid;
  scratch.d.cparam.paramtype = param->paramtype;
  ExprEvalPushStep(state, &scratch);
}

   
                                                                  
   
                                                                    
                                                        
   
static void
plpgsql_param_eval_var(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{
  ParamListInfo params;
  PLpgSQL_execstate *estate;
  int dno = op->d.cparam.paramid - 1;
  PLpgSQL_var *var;

                                
  params = econtext->ecxt_param_list_info;
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  Assert(dno >= 0 && dno < estate->ndatums);

                                          
  var = (PLpgSQL_var *)estate->datums[dno];
  Assert(var->dtype == PLPGSQL_DTYPE_VAR);

                                            
  *op->resvalue = var->value;
  *op->resnull = var->isnull;

                                                         
  Assert(var->datatype->typoid == op->d.cparam.paramtype);
}

   
                                                                     
   
                                                                    
                                                 
   
static void
plpgsql_param_eval_var_ro(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{
  ParamListInfo params;
  PLpgSQL_execstate *estate;
  int dno = op->d.cparam.paramid - 1;
  PLpgSQL_var *var;

                                
  params = econtext->ecxt_param_list_info;
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  Assert(dno >= 0 && dno < estate->ndatums);

                                          
  var = (PLpgSQL_var *)estate->datums[dno];
  Assert(var->dtype == PLPGSQL_DTYPE_VAR);

     
                                                                           
                                   
     
  *op->resvalue = MakeExpandedObjectReadOnly(var->value, var->isnull, -1);
  *op->resnull = var->isnull;

                                                         
  Assert(var->datatype->typoid == op->d.cparam.paramtype);
}

   
                                                                       
   
                                                                          
                                                       
   
static void
plpgsql_param_eval_recfield(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{
  ParamListInfo params;
  PLpgSQL_execstate *estate;
  int dno = op->d.cparam.paramid - 1;
  PLpgSQL_recfield *recfield;
  PLpgSQL_rec *rec;
  ExpandedRecordHeader *erh;

                                
  params = econtext->ecxt_param_list_info;
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  Assert(dno >= 0 && dno < estate->ndatums);

                                          
  recfield = (PLpgSQL_recfield *)estate->datums[dno];
  Assert(recfield->dtype == PLPGSQL_DTYPE_RECFIELD);

                                                   
  rec = (PLpgSQL_rec *)(estate->datums[recfield->recparentno]);
  erh = rec->erh;

     
                                                                            
                                                                       
                               
     
  if (erh == NULL)
  {
    instantiate_empty_record_variable(estate, rec);
    erh = rec->erh;
  }

     
                                                                            
                                            
     
  if (unlikely(recfield->rectupledescid != erh->er_tupdesc_id))
  {
    if (!expanded_record_lookup_field(erh, recfield->fieldname, &recfield->finfo))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("record \"%s\" has no field \"%s\"", rec->refname, recfield->fieldname)));
    }
    recfield->rectupledescid = erh->er_tupdesc_id;
  }

                                    
  *op->resvalue = expanded_record_get_field(erh, recfield->finfo.fnumber, op->resnull);

                                                     
  if (unlikely(recfield->finfo.ftypeid != op->d.cparam.paramtype))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type of parameter %d (%s) does not match that when preparing the plan (%s)", op->d.cparam.paramid, format_type_be(recfield->finfo.ftypeid), format_type_be(op->d.cparam.paramtype))));
  }
}

   
                                                                      
   
                                                                         
                               
   
static void
plpgsql_param_eval_generic(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{
  ParamListInfo params;
  PLpgSQL_execstate *estate;
  int dno = op->d.cparam.paramid - 1;
  PLpgSQL_datum *datum;
  Oid datumtype;
  int32 datumtypmod;

                                
  params = econtext->ecxt_param_list_info;
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  Assert(dno >= 0 && dno < estate->ndatums);

                                          
  datum = estate->datums[dno];

                           
  exec_eval_datum(estate, datum, &datumtype, &datumtypmod, op->resvalue, op->resnull);

                                                     
  if (unlikely(datumtype != op->d.cparam.paramtype))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type of parameter %d (%s) does not match that when preparing the plan (%s)", op->d.cparam.paramid, format_type_be(datumtype), format_type_be(op->d.cparam.paramtype))));
  }
}

   
                                                                        
   
                                                                  
                                                                           
   
static void
plpgsql_param_eval_generic_ro(ExprState *state, ExprEvalStep *op, ExprContext *econtext)
{
  ParamListInfo params;
  PLpgSQL_execstate *estate;
  int dno = op->d.cparam.paramid - 1;
  PLpgSQL_datum *datum;
  Oid datumtype;
  int32 datumtypmod;

                                
  params = econtext->ecxt_param_list_info;
  estate = (PLpgSQL_execstate *)params->paramFetchArg;
  Assert(dno >= 0 && dno < estate->ndatums);

                                          
  datum = estate->datums[dno];

                           
  exec_eval_datum(estate, datum, &datumtype, &datumtypmod, op->resvalue, op->resnull);

                                                     
  if (unlikely(datumtype != op->d.cparam.paramtype))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type of parameter %d (%s) does not match that when preparing the plan (%s)", op->d.cparam.paramid, format_type_be(datumtype), format_type_be(op->d.cparam.paramtype))));
  }

                                    
  *op->resvalue = MakeExpandedObjectReadOnly(*op->resvalue, *op->resnull, -1);
}

   
                                                                
   
                                                                             
                                                                          
                                                                           
   
                                                                             
                                                        
   
static void
exec_move_row(PLpgSQL_execstate *estate, PLpgSQL_variable *target, HeapTuple tup, TupleDesc tupdesc)
{
  ExpandedRecordHeader *newerh = NULL;

     
                                                                             
     
  if (target->dtype == PLPGSQL_DTYPE_REC)
  {
    PLpgSQL_rec *rec = (PLpgSQL_rec *)target;

       
                                                                           
                                                                   
                                                                      
                              
       
    if (tupdesc == NULL)
    {
      if (rec->datatype && rec->datatype->typtype == TYPTYPE_DOMAIN)
      {
           
                                                                 
                                                                      
                                                                     
                                                                    
           
        newerh = make_expanded_record_for_rec(estate, rec, NULL, rec->erh);
        expanded_record_set_tuple(newerh, NULL, false, false);
        assign_record_var(estate, rec, newerh);
      }
      else
      {
                                   
        if (rec->erh)
        {
          DeleteExpandedObject(ExpandedRecordGetDatum(rec->erh));
        }
        rec->erh = NULL;
      }
      return;
    }

       
                                                             
       
    newerh = make_expanded_record_for_rec(estate, rec, tupdesc, NULL);

       
                                                                    
                                                                  
       
                                                                          
                                                                          
                                                                           
                                                                          
                                                                         
                                                                      
       
    if (rec->rectypeid == RECORDOID || rec->rectypeid == tupdesc->tdtypeid || !HeapTupleIsValid(tup) || compatible_tupdescs(tupdesc, expanded_record_get_tupdesc(newerh)))
    {
      if (!HeapTupleIsValid(tup))
      {
                                                               
        deconstruct_expanded_record(newerh);
      }
      else
      {
                                                                 
        expanded_record_set_tuple(newerh, tup, true, !estate->atomic);
      }

                                   
      assign_record_var(estate, rec, newerh);

      return;
    }
  }

     
                                                                        
                                      
     
  if (tupdesc && HeapTupleIsValid(tup))
  {
    int td_natts = tupdesc->natts;
    Datum *values;
    bool *nulls;
    Datum values_local[64];
    bool nulls_local[64];

       
                                                                      
                                                                      
                                                                      
       
    if (td_natts <= lengthof(values_local))
    {
      values = values_local;
      nulls = nulls_local;
    }
    else
    {
      char *chunk;

      chunk = eval_mcontext_alloc(estate, td_natts * (sizeof(Datum) + sizeof(bool)));
      values = (Datum *)chunk;
      nulls = (bool *)(chunk + td_natts * sizeof(Datum));
    }

    heap_deform_tuple(tup, tupdesc, values, nulls);

    exec_move_row_from_fields(estate, target, newerh, values, nulls, tupdesc);
  }
  else
  {
       
                         
       
    exec_move_row_from_fields(estate, target, newerh, NULL, NULL, NULL);
  }
}

   
                                                        
   
static void
revalidate_rectypeid(PLpgSQL_rec *rec)
{
  PLpgSQL_type *typ = rec->datatype;
  TypeCacheEntry *typentry;

  if (rec->rectypeid == RECORDOID)
  {
    return;                                    
  }
  Assert(typ != NULL);
  if (typ->tcache && typ->tcache->tupDesc_identifier == typ->tupdesc_id)
  {
       
                                                                       
                                                                         
                                                                          
                                   
       
    rec->rectypeid = typ->typoid;
    return;
  }

     
                                                                           
                                                                     
                                                                       
     
  if (typ->origtypname != NULL)
  {
                                                             
    typenameTypeIdAndMod(NULL, typ->origtypname, &typ->typoid, &typ->atttypmod);
  }

                                                           
  typentry = lookup_type_cache(typ->typoid, TYPECACHE_TUPDESC | TYPECACHE_DOMAIN_BASE_INFO);
  if (typentry->typtype == TYPTYPE_DOMAIN)
  {
    typentry = lookup_type_cache(typentry->domainBaseType, TYPECACHE_TUPDESC);
  }
  if (typentry->tupDesc == NULL)
  {
       
                                                                     
                                                         
       
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(typ->typoid))));
  }

     
                                                                         
                                                                   
     
  typ->tcache = typentry;
  typ->tupdesc_id = typentry->tupDesc_identifier;

     
                                                                          
     
  rec->rectypeid = typ->typoid;
}

   
                                                                     
   
                                                                            
                                                                        
                                                                            
                                                                    
   
                                                                       
   
                                                                               
                                                   
   
static ExpandedRecordHeader *
make_expanded_record_for_rec(PLpgSQL_execstate *estate, PLpgSQL_rec *rec, TupleDesc srctupdesc, ExpandedRecordHeader *srcerh)
{
  ExpandedRecordHeader *newerh;
  MemoryContext mcontext = get_eval_mcontext(estate);

  if (rec->rectypeid != RECORDOID)
  {
       
                                                               
       
    revalidate_rectypeid(rec);

       
                                                                        
                                  
       
    if (srcerh && rec->rectypeid == srcerh->er_decltypeid)
    {
      newerh = make_expanded_record_from_exprecord(srcerh, mcontext);
    }
    else
    {
      newerh = make_expanded_record_from_typeid(rec->rectypeid, -1, mcontext);
    }
  }
  else
  {
       
                                                        
                                                                        
                                                                
       
    if (srcerh && !ExpandedRecordIsDomain(srcerh))
    {
      newerh = make_expanded_record_from_exprecord(srcerh, mcontext);
    }
    else
    {
      if (!srctupdesc)
      {
        srctupdesc = expanded_record_get_tupdesc(srcerh);
      }
      newerh = make_expanded_record_from_tupdesc(srctupdesc, mcontext);
    }
  }

  return newerh;
}

   
                                                                              
   
                                                                               
                                                                           
   
                                                                      
                                                                    
                                                                           
                                    
   
                                                                             
                                                        
   
static void
exec_move_row_from_fields(PLpgSQL_execstate *estate, PLpgSQL_variable *target, ExpandedRecordHeader *newerh, Datum *values, bool *nulls, TupleDesc tupdesc)
{
  int td_natts = tupdesc ? tupdesc->natts : 0;
  int fnum;
  int anum;
  int strict_multiassignment_level = 0;

     
                                                                             
                                 
     
  if (tupdesc != NULL)
  {
    if (plpgsql_extra_errors & PLPGSQL_XCHECK_STRICTMULTIASSIGNMENT)
    {
      strict_multiassignment_level = ERROR;
    }
    else if (plpgsql_extra_warnings & PLPGSQL_XCHECK_STRICTMULTIASSIGNMENT)
    {
      strict_multiassignment_level = WARNING;
    }
  }

                                 
  if (target->dtype == PLPGSQL_DTYPE_REC)
  {
    PLpgSQL_rec *rec = (PLpgSQL_rec *)target;
    TupleDesc var_tupdesc;
    Datum newvalues_local[64];
    bool newnulls_local[64];

    Assert(newerh != NULL);                                        

    var_tupdesc = expanded_record_get_tupdesc(newerh);

       
                                                                       
                                                                           
                                                                           
                                                                           
                                                                      
                                                                      
                                                                        
       
    if (var_tupdesc != tupdesc)
    {
      int vtd_natts = var_tupdesc->natts;
      Datum *newvalues;
      bool *newnulls;

         
                                                                         
                                                                        
                                                                        
         
      if (vtd_natts <= lengthof(newvalues_local))
      {
        newvalues = newvalues_local;
        newnulls = newnulls_local;
      }
      else
      {
        char *chunk;

        chunk = eval_mcontext_alloc(estate, vtd_natts * (sizeof(Datum) + sizeof(bool)));
        newvalues = (Datum *)chunk;
        newnulls = (bool *)(chunk + vtd_natts * sizeof(Datum));
      }

                                         
      anum = 0;
      for (fnum = 0; fnum < vtd_natts; fnum++)
      {
        Form_pg_attribute attr = TupleDescAttr(var_tupdesc, fnum);
        Datum value;
        bool isnull;
        Oid valtype;
        int32 valtypmod;

        if (attr->attisdropped)
        {
                                                                    
          continue;                                    
        }

        while (anum < td_natts && TupleDescAttr(tupdesc, anum)->attisdropped)
        {
          anum++;                                   
        }

        if (anum < td_natts)
        {
          value = values[anum];
          isnull = nulls[anum];
          valtype = TupleDescAttr(tupdesc, anum)->atttypid;
          valtypmod = TupleDescAttr(tupdesc, anum)->atttypmod;
          anum++;
        }
        else
        {
                                                
          value = (Datum)0;
          isnull = true;
          valtype = UNKNOWNOID;
          valtypmod = -1;

                                            
          if (strict_multiassignment_level)
          {
            ereport(strict_multiassignment_level, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("number of source and target fields in assignment does not match"),
                                                                                                              
                                                      errdetail("%s check of %s is active.", "strict_multi_assignment", strict_multiassignment_level == ERROR ? "extra_errors" : "extra_warnings"), errhint("Make sure the query returns the exact list of columns.")));
          }
        }

                                                              
        newvalues[fnum] = exec_cast_value(estate, value, &isnull, valtype, valtypmod, attr->atttypid, attr->atttypmod);
        newnulls[fnum] = isnull;
      }

         
                                                                        
                                                    
         
      if (strict_multiassignment_level && anum < td_natts)
      {
                                                           
        while (anum < td_natts && TupleDescAttr(tupdesc, anum)->attisdropped)
        {
          anum++;
        }

        if (anum < td_natts)
        {
          ereport(strict_multiassignment_level, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("number of source and target fields in assignment does not match"),
                                                                                                            
                                                    errdetail("%s check of %s is active.", "strict_multi_assignment", strict_multiassignment_level == ERROR ? "extra_errors" : "extra_warnings"), errhint("Make sure the query returns the exact list of columns.")));
        }
      }

      values = newvalues;
      nulls = newnulls;
    }

                                                                      
    expanded_record_set_fields(newerh, values, nulls, !estate->atomic);

                                 
    assign_record_var(estate, rec, newerh);

    return;
  }

                                                              
  Assert(newerh == NULL);

     
                                                                           
                    
     
                                                                          
                                                                         
                                                      
     
                                                                          
                       
     
  if (target->dtype == PLPGSQL_DTYPE_ROW)
  {
    PLpgSQL_row *row = (PLpgSQL_row *)target;

    anum = 0;
    for (fnum = 0; fnum < row->nfields; fnum++)
    {
      PLpgSQL_var *var;
      Datum value;
      bool isnull;
      Oid valtype;
      int32 valtypmod;

      var = (PLpgSQL_var *)(estate->datums[row->varnos[fnum]]);

      while (anum < td_natts && TupleDescAttr(tupdesc, anum)->attisdropped)
      {
        anum++;                                   
      }

      if (anum < td_natts)
      {
        value = values[anum];
        isnull = nulls[anum];
        valtype = TupleDescAttr(tupdesc, anum)->atttypid;
        valtypmod = TupleDescAttr(tupdesc, anum)->atttypmod;
        anum++;
      }
      else
      {
                                              
        value = (Datum)0;
        isnull = true;
        valtype = UNKNOWNOID;
        valtypmod = -1;

        if (strict_multiassignment_level)
        {
          ereport(strict_multiassignment_level, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("number of source and target fields in assignment does not match"),
                                                                                                            
                                                    errdetail("%s check of %s is active.", "strict_multi_assignment", strict_multiassignment_level == ERROR ? "extra_errors" : "extra_warnings"), errhint("Make sure the query returns the exact list of columns.")));
        }
      }

      exec_assign_value(estate, (PLpgSQL_datum *)var, value, isnull, valtype, valtypmod);
    }

       
                                                                           
                                        
       
    if (strict_multiassignment_level && anum < td_natts)
    {
      while (anum < td_natts && TupleDescAttr(tupdesc, anum)->attisdropped)
      {
        anum++;                                   
      }

      if (anum < td_natts)
      {
        ereport(strict_multiassignment_level, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("number of source and target fields in assignment does not match"),
                                                                                                          
                                                  errdetail("%s check of %s is active.", "strict_multi_assignment", strict_multiassignment_level == ERROR ? "extra_errors" : "extra_warnings"), errhint("Make sure the query returns the exact list of columns.")));
      }
    }

    return;
  }

  elog(ERROR, "unsupported target type: %d", target->dtype);
}

   
                                                                              
   
                                                                              
                                                       
   
static bool
compatible_tupdescs(TupleDesc src_tupdesc, TupleDesc dst_tupdesc)
{
  int i;

                                                                  
  if (dst_tupdesc->natts != src_tupdesc->natts)
  {
    return false;
  }

  for (i = 0; i < dst_tupdesc->natts; i++)
  {
    Form_pg_attribute dattr = TupleDescAttr(dst_tupdesc, i);
    Form_pg_attribute sattr = TupleDescAttr(src_tupdesc, i);

    if (dattr->attisdropped != sattr->attisdropped)
    {
      return false;
    }
    if (!dattr->attisdropped)
    {
                                                        
      if (dattr->atttypid != sattr->atttypid || (dattr->atttypmod >= 0 && dattr->atttypmod != sattr->atttypmod))
      {
        return false;
      }
    }
    else
    {
                                                                    
      if (dattr->attlen != sattr->attlen || dattr->attalign != sattr->attalign)
      {
        return false;
      }
    }
  }
  return true;
}

              
                                                                     
   
                                                                              
   
                                                                        
                                             
              
   
static HeapTuple
make_tuple_from_row(PLpgSQL_execstate *estate, PLpgSQL_row *row, TupleDesc tupdesc)
{
  int natts = tupdesc->natts;
  HeapTuple tuple;
  Datum *dvalues;
  bool *nulls;
  int i;

  if (natts != row->nfields)
  {
    return NULL;
  }

  dvalues = (Datum *)eval_mcontext_alloc0(estate, natts * sizeof(Datum));
  nulls = (bool *)eval_mcontext_alloc(estate, natts * sizeof(bool));

  for (i = 0; i < natts; i++)
  {
    Oid fieldtypeid;
    int32 fieldtypmod;

    if (TupleDescAttr(tupdesc, i)->attisdropped)
    {
      nulls[i] = true;                               
      continue;
    }

    exec_eval_datum(estate, estate->datums[row->varnos[i]], &fieldtypeid, &fieldtypmod, &dvalues[i], &nulls[i]);
    if (fieldtypeid != TupleDescAttr(tupdesc, i)->atttypid)
    {
      return NULL;
    }
                                                    
  }

  tuple = heap_form_tuple(tupdesc, dvalues, nulls);

  return tuple;
}

   
                                                                           
   
                                                                         
                                                                           
                                                                             
                                                                   
   
                                                              
                                                                         
   
                                                                             
                                                                              
   
static TupleDesc
deconstruct_composite_datum(Datum value, HeapTupleData *tmptup)
{
  HeapTupleHeader td;
  Oid tupType;
  int32 tupTypmod;

                                                           
  td = DatumGetHeapTupleHeader(value);

                                                     
  tmptup->t_len = HeapTupleHeaderGetDatumLength(td);
  ItemPointerSetInvalid(&(tmptup->t_self));
  tmptup->t_tableOid = InvalidOid;
  tmptup->t_data = td;

                                               
  tupType = HeapTupleHeaderGetTypeId(td);
  tupTypmod = HeapTupleHeaderGetTypMod(td);
  return lookup_rowtype_tupdesc(tupType, tupTypmod);
}

   
                                                                         
   
                                                                   
                                                                  
                              
   
                                                                             
   
static void
exec_move_row_from_datum(PLpgSQL_execstate *estate, PLpgSQL_variable *target, Datum value)
{
                                                    
  if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(value)))
  {
    ExpandedRecordHeader *erh = (ExpandedRecordHeader *)DatumGetEOHP(value);
    ExpandedRecordHeader *newerh = NULL;

    Assert(erh->er_magic == ER_MAGIC);

                                                              
    if (target->dtype == PLPGSQL_DTYPE_REC)
    {
      PLpgSQL_rec *rec = (PLpgSQL_rec *)target;

         
                                                                    
                                                                        
                                                                         
                                                                        
                           
         
      if (erh == rec->erh)
      {
        return;
      }

         
                                                                 
         
      revalidate_rectypeid(rec);

         
                                                                    
                                                                         
                                                                       
                                                                   
                                                                    
                                                         
         
      if (VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(value)) && (rec->rectypeid == erh->er_decltypeid || (rec->rectypeid == RECORDOID && !ExpandedRecordIsDomain(erh))))
      {
        assign_record_var(estate, rec, erh);
        return;
      }

         
                                                                    
                                                                
                                                                        
                                                              
                                                                       
                                                                      
                                                                  
                                                                     
                                                                    
                                                              
         
      if (rec->erh && (erh->flags & ER_FLAG_FVALUE_VALID) && erh->er_typeid == rec->erh->er_typeid && (erh->er_typeid != RECORDOID || (erh->er_typmod == rec->erh->er_typmod && erh->er_typmod >= 0)))
      {
        expanded_record_set_tuple(rec->erh, erh->fvalue, true, !estate->atomic);
        return;
      }

         
                                                                        
                                                                 
                                   
         
      newerh = make_expanded_record_for_rec(estate, rec, NULL, erh);

         
                                                                       
                                                                     
                                                                         
                                                                     
                                                                         
         
      if ((erh->flags & ER_FLAG_FVALUE_VALID) && (rec->rectypeid == RECORDOID || rec->rectypeid == erh->er_typeid))
      {
        expanded_record_set_tuple(newerh, erh->fvalue, true, !estate->atomic);
        assign_record_var(estate, rec, newerh);
        return;
      }

         
                                                                         
                      
         
      if (ExpandedRecordIsEmpty(erh))
      {
                                          
        deconstruct_expanded_record(newerh);
        assign_record_var(estate, rec, newerh);
        return;
      }
    }                                      

       
                                                                           
                                                                          
                                                                       
                                        
       
    if (ExpandedRecordIsEmpty(erh))
    {
      exec_move_row(estate, target, NULL, expanded_record_get_tupdesc(erh));
      return;
    }

       
                                                                      
                                     
       
    deconstruct_expanded_record(erh);
    exec_move_row_from_fields(estate, target, newerh, erh->dvalues, erh->dnulls, expanded_record_get_tupdesc(erh));
  }
  else
  {
       
                                                                        
                                                                          
                                              
       
    HeapTupleHeader td;
    HeapTupleData tmptup;
    Oid tupType;
    int32 tupTypmod;
    TupleDesc tupdesc;
    MemoryContext oldcontext;

                                                                      
    oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
                                                             
    td = DatumGetHeapTupleHeader(value);
    MemoryContextSwitchTo(oldcontext);

                                                       
    tmptup.t_len = HeapTupleHeaderGetDatumLength(td);
    ItemPointerSetInvalid(&(tmptup.t_self));
    tmptup.t_tableOid = InvalidOid;
    tmptup.t_data = td;

                              
    tupType = HeapTupleHeaderGetTypeId(td);
    tupTypmod = HeapTupleHeaderGetTypMod(td);

                                                                         
    if (target->dtype == PLPGSQL_DTYPE_REC)
    {
      PLpgSQL_rec *rec = (PLpgSQL_rec *)target;

         
                                                                    
                                                                        
                                                                         
                                                                     
                                                                     
                                                                    
                                                               
         
      if (rec->erh && tupType == rec->erh->er_typeid && (tupType != RECORDOID || (tupTypmod == rec->erh->er_typmod && tupTypmod >= 0)))
      {
        expanded_record_set_tuple(rec->erh, &tmptup, true, !estate->atomic);
        return;
      }

         
                                                                      
                                                                         
                                                                       
                                                         
         
      if (rec->rectypeid == RECORDOID || rec->rectypeid == tupType)
      {
        ExpandedRecordHeader *newerh;
        MemoryContext mcontext = get_eval_mcontext(estate);

        newerh = make_expanded_record_from_typeid(tupType, tupTypmod, mcontext);
        expanded_record_set_tuple(newerh, &tmptup, true, !estate->atomic);
        assign_record_var(estate, rec, newerh);
        return;
      }

         
                                                                       
                             
         
    }

       
                                                                          
                                                    
       
    tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

                     
    exec_move_row(estate, target, &tmptup, tupdesc);

                                     
    ReleaseTupleDesc(tupdesc);
  }
}

   
                                                                           
                                                                        
                                                                     
                                                                            
   
static void
instantiate_empty_record_variable(PLpgSQL_execstate *estate, PLpgSQL_rec *rec)
{
  Assert(rec->erh == NULL);                        

                                                        
  if (rec->rectypeid == RECORDOID)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("record \"%s\" is not assigned yet", rec->refname), errdetail("The tuple structure of a not-yet-assigned record is indeterminate.")));
  }

                                                              
  revalidate_rectypeid(rec);

                 
  rec->erh = make_expanded_record_from_typeid(rec->rectypeid, -1, estate->datum_context);
}

              
                                                                  
   
                                                                          
                                                                            
                                                                             
                                                                            
                                                                        
                                                                           
   
                                                                            
                                                                            
                                                
              
   
static char *
convert_value_to_string(PLpgSQL_execstate *estate, Datum value, Oid valtype)
{
  char *result;
  MemoryContext oldcontext;
  Oid typoutput;
  bool typIsVarlena;

  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  getTypeOutputInfo(valtype, &typoutput, &typIsVarlena);
  result = OidOutputFunctionCall(typoutput, value);
  MemoryContextSwitchTo(oldcontext);

  return result;
}

              
                                              
   
                                                                           
                                                                           
                                          
   
                                                                           
                                                                          
                                                                           
                         
              
   
static Datum
exec_cast_value(PLpgSQL_execstate *estate, Datum value, bool *isnull, Oid valtype, int32 valtypmod, Oid reqtype, int32 reqtypmod)
{
     
                                                                        
     
  if (valtype != reqtype || (valtypmod != reqtypmod && reqtypmod != -1))
  {
    plpgsql_CastHashEntry *cast_entry;

    cast_entry = get_cast_hashentry(estate, valtype, valtypmod, reqtype, reqtypmod);
    if (cast_entry)
    {
      ExprContext *econtext = estate->eval_econtext;
      MemoryContext oldcontext;

      oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));

      econtext->caseValue_datum = value;
      econtext->caseValue_isNull = *isnull;

      cast_entry->cast_in_use = true;

      value = ExecEvalExpr(cast_entry->cast_exprstate, econtext, isnull);

      cast_entry->cast_in_use = false;

      MemoryContextSwitchTo(oldcontext);
    }
  }

  return value;
}

              
                                                           
   
                                                                         
                                                                          
                                                                         
                                                                          
                            
              
   
static plpgsql_CastHashEntry *
get_cast_hashentry(PLpgSQL_execstate *estate, Oid srctype, int32 srctypmod, Oid dsttype, int32 dsttypmod)
{
  plpgsql_CastHashKey cast_key;
  plpgsql_CastHashEntry *cast_entry;
  bool found;
  LocalTransactionId curlxid;
  MemoryContext oldcontext;

                               
  cast_key.srctype = srctype;
  cast_key.dsttype = dsttype;
  cast_key.srctypmod = srctypmod;
  cast_key.dsttypmod = dsttypmod;
  cast_entry = (plpgsql_CastHashEntry *)hash_search(estate->cast_hash, (void *)&cast_key, HASH_ENTER, &found);
  if (!found)                              
  {
    cast_entry->cast_cexpr = NULL;
  }

  if (cast_entry->cast_cexpr == NULL || !cast_entry->cast_cexpr->is_valid)
  {
       
                                                                           
                                        
       
    Node *cast_expr;
    CachedExpression *cast_cexpr;
    CaseTestExpr *placeholder;

       
                                                   
       
    if (cast_entry->cast_cexpr)
    {
      FreeCachedExpression(cast_entry->cast_cexpr);
      cast_entry->cast_cexpr = NULL;
    }

       
                                                                  
                                                             
                                                                        
       
    oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));

       
                                                                          
                                                       
       
    placeholder = makeNode(CaseTestExpr);
    placeholder->typeId = srctype;
    placeholder->typeMod = srctypmod;
    placeholder->collation = get_typcollation(srctype);

       
                                                                      
                                                                      
                                                                        
                                                      
       
                                                                           
                                                                           
                                                                           
                                                                 
       
    if (srctype == UNKNOWNOID || srctype == RECORDOID)
    {
      cast_expr = NULL;
    }
    else
    {
      cast_expr = coerce_to_target_type(NULL, (Node *)placeholder, srctype, dsttype, dsttypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    }

       
                                                                           
                                                                           
                                                                     
                                     
       
    if (cast_expr == NULL)
    {
      CoerceViaIO *iocoerce = makeNode(CoerceViaIO);

      iocoerce->arg = (Expr *)placeholder;
      iocoerce->resulttype = dsttype;
      iocoerce->resultcollid = InvalidOid;
      iocoerce->coerceformat = COERCE_IMPLICIT_CAST;
      iocoerce->location = -1;
      cast_expr = (Node *)iocoerce;
      if (dsttypmod != -1)
      {
        cast_expr = coerce_to_target_type(NULL, cast_expr, dsttype, dsttype, dsttypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
      }
    }

                                                                           

                                                          
    cast_cexpr = GetCachedExpression(cast_expr);
    cast_expr = cast_cexpr->expr;

                                                               
    if (IsA(cast_expr, RelabelType) && ((RelabelType *)cast_expr)->arg == (Expr *)placeholder)
    {
      cast_expr = NULL;
    }

                                                 
    cast_entry->cast_cexpr = cast_cexpr;
    cast_entry->cast_expr = (Expr *)cast_expr;
    cast_entry->cast_exprstate = NULL;
    cast_entry->cast_in_use = false;
    cast_entry->cast_lxid = InvalidLocalTransactionId;

    MemoryContextSwitchTo(oldcontext);
  }

                                                             
  if (cast_entry->cast_expr == NULL)
  {
    return NULL;
  }

     
                                                                            
                                                                       
                                                                             
                                                                         
                                                                      
                                                                         
                                                                         
                                                                          
                                                                           
                                        
     
  curlxid = MyProc->lxid;
  if (cast_entry->cast_lxid != curlxid || cast_entry->cast_in_use)
  {
    oldcontext = MemoryContextSwitchTo(estate->simple_eval_estate->es_query_cxt);
    cast_entry->cast_exprstate = ExecInitExpr(cast_entry->cast_expr, NULL);
    cast_entry->cast_in_use = false;
    cast_entry->cast_lxid = curlxid;
    MemoryContextSwitchTo(oldcontext);
  }

  return cast_entry;
}

              
                                                                 
                                                 
                  
   
                                                                            
                                                                               
                                                                      
              
   
static void
exec_simple_check_plan(PLpgSQL_execstate *estate, PLpgSQL_expr *expr)
{
  List *plansources;
  CachedPlanSource *plansource;
  Query *query;
  CachedPlan *cplan;
  MemoryContext oldcontext;

     
                                 
     
  expr->expr_simple_expr = NULL;

     
                                                                             
                                                                           
                                                                         
                                        
     

     
                                                                            
     
  plansources = SPI_plan_get_plan_sources(expr->plan);
  if (list_length(plansources) != 1)
  {
    return;
  }
  plansource = (CachedPlanSource *)linitial(plansources);

     
                                            
     
  if (list_length(plansource->query_list) != 1)
  {
    return;
  }
  query = (Query *)linitial(plansource->query_list);

     
                                                                 
     
  if (!IsA(query, Query))
  {
    return;
  }
  if (query->commandType != CMD_SELECT)
  {
    return;
  }
  if (query->rtable != NIL)
  {
    return;
  }

     
                                                                          
                                                                       
                                                                   
                                                                             
     
  if (query->hasAggs || query->hasWindowFuncs || query->hasTargetSRFs || query->hasSubLinks || query->cteList || query->jointree->fromlist || query->jointree->quals || query->groupClause || query->groupingSets || query->havingQual || query->windowClause || query->distinctClause || query->sortClause || query->limitOffset || query->limitCount || query->setOperations)
  {
    return;
  }

     
                                                         
     
  if (list_length(query->targetList) != 1)
  {
    return;
  }

     
                                           
     
                                                                           
                                                                             
                                                                         
     
  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));
  cplan = SPI_plan_get_cached_plan(expr->plan);
  MemoryContextSwitchTo(oldcontext);

                                                                          
  Assert(cplan != NULL);

                                                      
  exec_save_simple_expr(expr, cplan);

                                 
  ReleaseCachedPlan(cplan, true);
}

   
                                                                       
   
static void
exec_save_simple_expr(PLpgSQL_expr *expr, CachedPlan *cplan)
{
  PlannedStmt *stmt;
  Plan *plan;
  Expr *tle_expr;

     
                                                                           
                            
     

                                      
  Assert(list_length(cplan->stmt_list) == 1);
  stmt = linitial_node(PlannedStmt, cplan->stmt_list);
  Assert(stmt->commandType == CMD_SELECT);

     
                                                                       
                                                                         
                                                                           
                                                                        
                                                                           
                                                      
     
  plan = stmt->planTree;
  for (;;)
  {
                                             
    Assert(list_length(plan->targetlist) == 1);
    tle_expr = castNode(TargetEntry, linitial(plan->targetlist))->expr;

    if (IsA(plan, Result))
    {
      Assert(plan->lefttree == NULL && plan->righttree == NULL && plan->initPlan == NULL && plan->qual == NULL && ((Result *)plan)->resconstantqual == NULL);
      break;
    }
    else if (IsA(plan, Gather))
    {
      Assert(plan->lefttree != NULL && plan->righttree == NULL && plan->initPlan == NULL && plan->qual == NULL);
                                                                   
      if (IsA(tle_expr, Const))
      {
        break;
      }
                                                               
      Assert(IsA(tle_expr, Param) || (IsA(tle_expr, Var) && ((Var *)tle_expr)->varno == OUTER_VAR));
                                     
      plan = plan->lefttree;
    }
    else
    {
      elog(ERROR, "unexpected plan node type: %d", (int)nodeTag(plan));
    }
  }

     
                                                                       
                           
     
  expr->expr_simple_expr = tle_expr;
  expr->expr_simple_generation = cplan->generation;
  expr->expr_simple_state = NULL;
  expr->expr_simple_in_use = false;
  expr->expr_simple_lxid = InvalidLocalTransactionId;
                                                  
  expr->expr_simple_type = exprType((Node *)tle_expr);
  expr->expr_simple_typmod = exprTypmod((Node *)tle_expr);
}

   
                                                                                
   
                                                                          
                                                                           
                                                                              
                                                                       
             
   
                                                                             
                                                              
   
static void
exec_check_rw_parameter(PLpgSQL_expr *expr, int target_dno)
{
  Oid funcid;
  List *fargs;
  ListCell *lc;

                     
  expr->rwparam = -1;

     
                                                                            
                                                                             
                                                                             
     
  if (expr->expr_simple_expr == NULL)
  {
    return;
  }

     
                                                                        
              
     
  if (!bms_is_member(target_dno, expr->paramnos))
  {
    return;
  }

     
                                                                  
     
  if (IsA(expr->expr_simple_expr, FuncExpr))
  {
    FuncExpr *fexpr = (FuncExpr *)expr->expr_simple_expr;

    funcid = fexpr->funcid;
    fargs = fexpr->args;
  }
  else if (IsA(expr->expr_simple_expr, OpExpr))
  {
    OpExpr *opexpr = (OpExpr *)expr->expr_simple_expr;

    funcid = opexpr->opfuncid;
    fargs = opexpr->args;
  }
  else
  {
    return;
  }

     
                                                                    
                                                                        
                                                          
     
  if (!(funcid == F_ARRAY_APPEND || funcid == F_ARRAY_PREPEND))
  {
    return;
  }

     
                                                                        
                                                
     
  foreach (lc, fargs)
  {
    Node *arg = (Node *)lfirst(lc);

                                                                
    if (arg && IsA(arg, Param))
    {
      continue;
    }
                                                                  
    if (contains_target_param(arg, &target_dno))
    {
      return;
    }
  }

                                                        
  expr->rwparam = target_dno;
}

   
                                                                 
   
static bool
contains_target_param(Node *node, int *target_dno)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_EXTERN && param->paramid == *target_dno + 1)
    {
      return true;
    }
    return false;
  }
  return expression_tree_walker(node, contains_target_param, (void *)target_dno);
}

              
                                                                
              
   
static void
exec_set_found(PLpgSQL_execstate *estate, bool state)
{
  PLpgSQL_var *var;

  var = (PLpgSQL_var *)(estate->datums[estate->found_varno]);
  assign_simple_var(estate, var, BoolGetDatum(state), false, false);
}

   
                                                                                
   
                                                                             
                                                                              
                    
   
static void
plpgsql_create_econtext(PLpgSQL_execstate *estate)
{
  SimpleEcontextStackEntry *entry;

     
                                                                           
                                                                            
                                                               
     
                                                                       
                                                                           
                                                                           
                                                                           
                                                                           
                                                                            
                                                                            
     
  if (estate->simple_eval_estate == NULL)
  {
    MemoryContext oldcontext;

    if (shared_simple_eval_estate == NULL)
    {
      oldcontext = MemoryContextSwitchTo(TopTransactionContext);
      shared_simple_eval_estate = CreateExecutorState();
      MemoryContextSwitchTo(oldcontext);
    }
    estate->simple_eval_estate = shared_simple_eval_estate;
  }

     
                                                       
     
  estate->eval_econtext = CreateExprContext(estate->simple_eval_estate);

     
                                                                        
                                                                     
     
  entry = (SimpleEcontextStackEntry *)MemoryContextAlloc(TopTransactionContext, sizeof(SimpleEcontextStackEntry));

  entry->stack_econtext = estate->eval_econtext;
  entry->xact_subxid = GetCurrentSubTransactionId();

  entry->next = simple_econtext_stack;
  simple_econtext_stack = entry;
}

   
                                                            
   
                                                                       
                                 
   
static void
plpgsql_destroy_econtext(PLpgSQL_execstate *estate)
{
  SimpleEcontextStackEntry *next;

  Assert(simple_econtext_stack != NULL);
  Assert(simple_econtext_stack->stack_econtext == estate->eval_econtext);

  next = simple_econtext_stack->next;
  pfree(simple_econtext_stack);
  simple_econtext_stack = next;

  FreeExprContext(estate->eval_econtext, true);
  estate->eval_econtext = NULL;
}

   
                                                                
   
                                                                         
                            
   
void
plpgsql_xact_cb(XactEvent event, void *arg)
{
     
                                                                            
                                                                          
                                                                           
               
     
  if (event == XACT_EVENT_COMMIT || event == XACT_EVENT_PARALLEL_COMMIT || event == XACT_EVENT_PREPARE)
  {
    simple_econtext_stack = NULL;

    if (shared_simple_eval_estate)
    {
      FreeExecutorState(shared_simple_eval_estate);
    }
    shared_simple_eval_estate = NULL;
  }
  else if (event == XACT_EVENT_ABORT || event == XACT_EVENT_PARALLEL_ABORT)
  {
    simple_econtext_stack = NULL;
    shared_simple_eval_estate = NULL;
  }
}

   
                                                                      
   
                                                                    
                                                                         
                                                                         
   
void
plpgsql_subxact_cb(SubXactEvent event, SubTransactionId mySubid, SubTransactionId parentSubid, void *arg)
{
  if (event == SUBXACT_EVENT_COMMIT_SUB || event == SUBXACT_EVENT_ABORT_SUB)
  {
    while (simple_econtext_stack != NULL && simple_econtext_stack->xact_subxid == mySubid)
    {
      SimpleEcontextStackEntry *next;

      FreeExprContext(simple_econtext_stack->stack_econtext, (event == SUBXACT_EVENT_COMMIT_SUB));
      next = simple_econtext_stack->next;
      pfree(simple_econtext_stack);
      simple_econtext_stack = next;
    }
  }
}

   
                                                              
   
                                                                         
                                                                       
                             
   
static void
assign_simple_var(PLpgSQL_execstate *estate, PLpgSQL_var *var, Datum newvalue, bool isnull, bool freeable)
{
  Assert(var->dtype == PLPGSQL_DTYPE_VAR || var->dtype == PLPGSQL_DTYPE_PROMISE);

     
                                                                       
                                                                         
                                                                         
                                                                        
                                                                         
                                                                          
                                                                            
                                                                  
     
  if (!estate->atomic && !isnull && var->datatype->typlen == -1 && VARATT_IS_EXTERNAL_NON_EXPANDED(DatumGetPointer(newvalue)))
  {
    MemoryContext oldcxt;
    Datum detoasted;

       
                                                                         
                                                                           
                                                                      
                                        
       
    oldcxt = MemoryContextSwitchTo(get_eval_mcontext(estate));
    detoasted = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(newvalue)));
    MemoryContextSwitchTo(oldcxt);
                                                                        
    if (freeable)
    {
      pfree(DatumGetPointer(newvalue));
    }
                                                          
    newvalue = datumCopy(detoasted, false, -1);
    freeable = true;
                                                                         
  }

                                    
  if (var->freeval)
  {
    if (DatumIsReadWriteExpandedObject(var->value, var->isnull, var->datatype->typlen))
    {
      DeleteExpandedObject(var->value);
    }
    else
    {
      pfree(DatumGetPointer(var->value));
    }
  }
                                 
  var->value = newvalue;
  var->isnull = isnull;
  var->freeval = freeable;

     
                                                                           
                                                                         
                              
     
  var->promise = PLPGSQL_PROMISE_NONE;
}

   
                                                                        
   
static void
assign_text_var(PLpgSQL_execstate *estate, PLpgSQL_var *var, const char *str)
{
  assign_simple_var(estate, var, CStringGetTextDatum(str), false, true);
}

   
                                                              
   
static void
assign_record_var(PLpgSQL_execstate *estate, PLpgSQL_rec *rec, ExpandedRecordHeader *erh)
{
  Assert(rec->dtype == PLPGSQL_DTYPE_REC);

                                                     
  TransferExpandedRecord(erh, estate->datum_context);

                              
  if (rec->erh)
  {
    DeleteExpandedObject(ExpandedRecordGetDatum(rec->erh));
  }

                               
  rec->erh = erh;
}

   
                                                              
   
                                                                         
                                       
   
static PreparedParamsData *
exec_eval_using_params(PLpgSQL_execstate *estate, List *params)
{
  PreparedParamsData *ppd;
  MemoryContext stmt_mcontext = get_stmt_mcontext(estate);
  int nargs;
  int i;
  ListCell *lc;

  ppd = (PreparedParamsData *)MemoryContextAlloc(stmt_mcontext, sizeof(PreparedParamsData));
  nargs = list_length(params);

  ppd->nargs = nargs;
  ppd->types = (Oid *)MemoryContextAlloc(stmt_mcontext, nargs * sizeof(Oid));
  ppd->values = (Datum *)MemoryContextAlloc(stmt_mcontext, nargs * sizeof(Datum));
  ppd->nulls = (char *)MemoryContextAlloc(stmt_mcontext, nargs * sizeof(char));

  i = 0;
  foreach (lc, params)
  {
    PLpgSQL_expr *param = (PLpgSQL_expr *)lfirst(lc);
    bool isnull;
    int32 ppdtypmod;
    MemoryContext oldcontext;

    ppd->values[i] = exec_eval_expr(estate, param, &isnull, &ppd->types[i], &ppdtypmod);
    ppd->nulls[i] = isnull ? 'n' : ' ';

    oldcontext = MemoryContextSwitchTo(stmt_mcontext);

    if (ppd->types[i] == UNKNOWNOID)
    {
         
                                                                    
                                                                       
                                                                      
                                                                      
                                                                      
         
      ppd->types[i] = TEXTOID;
      if (!isnull)
      {
        ppd->values[i] = CStringGetTextDatum(DatumGetCString(ppd->values[i]));
      }
    }
                                                                       
    else if (!isnull)
    {
      int16 typLen;
      bool typByVal;

      get_typlenbyval(ppd->types[i], &typLen, &typByVal);
      if (!typByVal)
      {
        ppd->values[i] = datumCopy(ppd->values[i], typByVal, typLen);
      }
    }

    MemoryContextSwitchTo(oldcontext);

    exec_eval_cleanup(estate);

    i++;
  }

  return ppd;
}

   
                                 
   
                                                                             
                                                                             
                                                                         
                          
   
static Portal
exec_dynquery_with_params(PLpgSQL_execstate *estate, PLpgSQL_expr *dynquery, List *params, const char *portalname, int cursorOptions)
{
  Portal portal;
  Datum query;
  bool isnull;
  Oid restype;
  int32 restypmod;
  char *querystr;
  MemoryContext stmt_mcontext = get_stmt_mcontext(estate);

     
                                                                             
                                         
     
  query = exec_eval_expr(estate, dynquery, &isnull, &restype, &restypmod);
  if (isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("query string argument of EXECUTE is null")));
  }

                                       
  querystr = convert_value_to_string(estate, query, restype);

                                                         
  querystr = MemoryContextStrdup(stmt_mcontext, querystr);

  exec_eval_cleanup(estate);

     
                                                    
                                                                           
                                                     
     
  if (params)
  {
    PreparedParamsData *ppd;

    ppd = exec_eval_using_params(estate, params);
    portal = SPI_cursor_open_with_args(portalname, querystr, ppd->nargs, ppd->types, ppd->values, ppd->nulls, estate->readonly_func, cursorOptions);
  }
  else
  {
    portal = SPI_cursor_open_with_args(portalname, querystr, 0, NULL, NULL, NULL, estate->readonly_func, cursorOptions);
  }

  if (portal == NULL)
  {
    elog(ERROR, "could not open implicit cursor for query \"%s\": %s", querystr, SPI_result_code_string(SPI_result));
  }

                              
  MemoryContextReset(stmt_mcontext);

  return portal;
}

   
                                                                                
                                                           
                                       
   
static char *
format_expr_params(PLpgSQL_execstate *estate, const PLpgSQL_expr *expr)
{
  int paramno;
  int dno;
  StringInfoData paramstr;
  MemoryContext oldcontext;

  if (!expr->paramnos)
  {
    return NULL;
  }

  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));

  initStringInfo(&paramstr);
  paramno = 0;
  dno = -1;
  while ((dno = bms_next_member(expr->paramnos, dno)) >= 0)
  {
    Datum paramdatum;
    Oid paramtypeid;
    bool paramisnull;
    int32 paramtypmod;
    PLpgSQL_var *curvar;

    curvar = (PLpgSQL_var *)estate->datums[dno];

    exec_eval_datum(estate, (PLpgSQL_datum *)curvar, &paramtypeid, &paramtypmod, &paramdatum, &paramisnull);

    appendStringInfo(&paramstr, "%s%s = ", paramno > 0 ? ", " : "", curvar->refname);

    if (paramisnull)
    {
      appendStringInfoString(&paramstr, "NULL");
    }
    else
    {
      char *value = convert_value_to_string(estate, paramdatum, paramtypeid);
      char *p;

      appendStringInfoCharMacro(&paramstr, '\'');
      for (p = value; *p; p++)
      {
        if (*p == '\'')                           
        {
          appendStringInfoCharMacro(&paramstr, *p);
        }
        appendStringInfoCharMacro(&paramstr, *p);
      }
      appendStringInfoCharMacro(&paramstr, '\'');
    }

    paramno++;
  }

  MemoryContextSwitchTo(oldcontext);

  return paramstr.data;
}

   
                                                                                
                               
                                       
   
static char *
format_preparedparamsdata(PLpgSQL_execstate *estate, const PreparedParamsData *ppd)
{
  int paramno;
  StringInfoData paramstr;
  MemoryContext oldcontext;

  if (!ppd)
  {
    return NULL;
  }

  oldcontext = MemoryContextSwitchTo(get_eval_mcontext(estate));

  initStringInfo(&paramstr);
  for (paramno = 0; paramno < ppd->nargs; paramno++)
  {
    appendStringInfo(&paramstr, "%s$%d = ", paramno > 0 ? ", " : "", paramno + 1);

    if (ppd->nulls[paramno] == 'n')
    {
      appendStringInfoString(&paramstr, "NULL");
    }
    else
    {
      char *value = convert_value_to_string(estate, ppd->values[paramno], ppd->types[paramno]);
      char *p;

      appendStringInfoCharMacro(&paramstr, '\'');
      for (p = value; *p; p++)
      {
        if (*p == '\'')                           
        {
          appendStringInfoCharMacro(&paramstr, *p);
        }
        appendStringInfoCharMacro(&paramstr, *p);
      }
      appendStringInfoCharMacro(&paramstr, '\'');
    }
  }

  MemoryContextSwitchTo(oldcontext);

  return paramstr.data;
}
