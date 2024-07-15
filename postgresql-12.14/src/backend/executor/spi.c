                                                                            
   
         
                                   
   
                                                                         
                                                                        
   
   
                  
                                
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/printtup.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/heap.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/spi_priv.h"
#include "miscadmin.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

   
                                                                        
                                                                          
                                                                        
                                              
   
uint64 SPI_processed = 0;
SPITupleTable *SPI_tuptable = NULL;
int SPI_result = 0;

static _SPI_connection *_SPI_stack = NULL;
static _SPI_connection *_SPI_current = NULL;
static int _SPI_stack_depth = 0;                                   
static int _SPI_connected = -1;                           

static Portal
SPI_cursor_open_internal(const char *name, SPIPlanPtr plan, ParamListInfo paramLI, bool read_only);

static void
_SPI_prepare_plan(const char *src, SPIPlanPtr plan);

static void
_SPI_prepare_oneshot_plan(const char *src, SPIPlanPtr plan);

static int
_SPI_execute_plan(SPIPlanPtr plan, ParamListInfo paramLI, Snapshot snapshot, Snapshot crosscheck_snapshot, bool read_only, bool fire_triggers, uint64 tcount);

static ParamListInfo
_SPI_convert_params(int nargs, Oid *argtypes, Datum *Values, const char *Nulls);

static int
_SPI_pquery(QueryDesc *queryDesc, bool fire_triggers, uint64 tcount);

static void
_SPI_error_callback(void *arg);

static void
_SPI_cursor_operation(Portal portal, FetchDirection direction, long count, DestReceiver *dest);

static SPIPlanPtr
_SPI_make_plan_non_temp(SPIPlanPtr plan);
static SPIPlanPtr
_SPI_save_plan(SPIPlanPtr plan);

static int
_SPI_begin_call(bool use_exec);
static int
_SPI_end_call(bool use_exec);
static MemoryContext
_SPI_execmem(void);
static MemoryContext
_SPI_procmem(void);
static bool
_SPI_checktuples(void);

                                                                 

int
SPI_connect(void)
{
  return SPI_connect_ext(0);
}

int
SPI_connect_ext(int options)
{
  int newdepth;

                                  
  if (_SPI_stack == NULL)
  {
    if (_SPI_connected != -1 || _SPI_stack_depth != 0)
    {
      elog(ERROR, "SPI stack corrupted");
    }
    newdepth = 16;
    _SPI_stack = (_SPI_connection *)MemoryContextAlloc(TopMemoryContext, newdepth * sizeof(_SPI_connection));
    _SPI_stack_depth = newdepth;
  }
  else
  {
    if (_SPI_stack_depth <= 0 || _SPI_stack_depth <= _SPI_connected)
    {
      elog(ERROR, "SPI stack corrupted");
    }
    if (_SPI_stack_depth == _SPI_connected + 1)
    {
      newdepth = _SPI_stack_depth * 2;
      _SPI_stack = (_SPI_connection *)repalloc(_SPI_stack, newdepth * sizeof(_SPI_connection));
      _SPI_stack_depth = newdepth;
    }
  }

                             
  _SPI_connected++;
  Assert(_SPI_connected >= 0 && _SPI_connected < _SPI_stack_depth);

  _SPI_current = &(_SPI_stack[_SPI_connected]);
  _SPI_current->processed = 0;
  _SPI_current->tuptable = NULL;
  _SPI_current->execSubid = InvalidSubTransactionId;
  slist_init(&_SPI_current->tuptables);
  _SPI_current->procCxt = NULL;                                    
  _SPI_current->execCxt = NULL;
  _SPI_current->connectSubid = GetCurrentSubTransactionId();
  _SPI_current->queryEnv = NULL;
  _SPI_current->atomic = (options & SPI_OPT_NONATOMIC ? false : true);
  _SPI_current->internal_xact = false;
  _SPI_current->outer_processed = SPI_processed;
  _SPI_current->outer_tuptable = SPI_tuptable;
  _SPI_current->outer_result = SPI_result;

     
                                               
     
                                                                         
                                                                  
                 
     
                                                                          
                                                                             
                                                                             
                                                                            
                                  
     
  _SPI_current->procCxt = AllocSetContextCreate(_SPI_current->atomic ? TopTransactionContext : PortalContext, "SPI Proc", ALLOCSET_DEFAULT_SIZES);
  _SPI_current->execCxt = AllocSetContextCreate(_SPI_current->atomic ? TopTransactionContext : _SPI_current->procCxt, "SPI Exec", ALLOCSET_DEFAULT_SIZES);
                                             
  _SPI_current->savedcxt = MemoryContextSwitchTo(_SPI_current->procCxt);

     
                                                                           
                                         
     
  SPI_processed = 0;
  SPI_tuptable = NULL;
  SPI_result = 0;

  return SPI_OK_CONNECT;
}

int
SPI_finish(void)
{
  int res;

  res = _SPI_begin_call(false);                                 
  if (res < 0)
  {
    return res;
  }

                                                              
  MemoryContextSwitchTo(_SPI_current->savedcxt);

                                                                   
  MemoryContextDelete(_SPI_current->execCxt);
  _SPI_current->execCxt = NULL;
  MemoryContextDelete(_SPI_current->procCxt);
  _SPI_current->procCxt = NULL;

     
                                                                            
                                         
     
  SPI_processed = _SPI_current->outer_processed;
  SPI_tuptable = _SPI_current->outer_tuptable;
  SPI_result = _SPI_current->outer_result;

                        
  _SPI_connected--;
  if (_SPI_connected < 0)
  {
    _SPI_current = NULL;
  }
  else
  {
    _SPI_current = &(_SPI_stack[_SPI_connected]);
  }

  return SPI_OK_FINISH;
}

   
                                                                       
                                                  
   
void
SPI_start_transaction(void)
{
}

static void
_SPI_commit(bool chain)
{
  MemoryContext oldcontext = CurrentMemoryContext;

     
                                                                     
                                                                           
                                                                             
                                                                
     
  if (_SPI_current->atomic)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_TERMINATION), errmsg("invalid transaction termination")));
  }

     
                                                                          
                                                                            
                                                                    
                                                                            
                                                                         
                                                                        
                                                                             
                                           
     
  if (IsSubTransaction())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_TERMINATION), errmsg("cannot commit while a subtransaction is active")));
  }

                                                     
  if (chain)
  {
    SaveTransactionCharacteristics();
  }

                                                   
  PG_TRY();
  {
                                                          
    _SPI_current->internal_xact = true;

       
                                                                           
                                                                   
                                                    
       
    HoldPinnedPortals();

                                                   
    ForgetPortalSnapshots();

                     
    CommitTransactionCommand();

                                             
    StartTransactionCommand();
    if (chain)
    {
      RestoreTransactionCharacteristics();
    }

    MemoryContextSwitchTo(oldcontext);

    _SPI_current->internal_xact = false;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                                             
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

       
                                                                    
                                                                    
       
    AbortCurrentTransaction();

                                 
    StartTransactionCommand();
    if (chain)
    {
      RestoreTransactionCharacteristics();
    }

    MemoryContextSwitchTo(oldcontext);

    _SPI_current->internal_xact = false;

                                                                       
    ReThrowError(edata);
  }
  PG_END_TRY();
}

void
SPI_commit(void)
{
  _SPI_commit(false);
}

void
SPI_commit_and_chain(void)
{
  _SPI_commit(true);
}

static void
_SPI_rollback(bool chain)
{
  MemoryContext oldcontext = CurrentMemoryContext;

                              
  if (_SPI_current->atomic)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_TERMINATION), errmsg("invalid transaction termination")));
  }

                              
  if (IsSubTransaction())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_TERMINATION), errmsg("cannot roll back while a subtransaction is active")));
  }

                                                     
  if (chain)
  {
    SaveTransactionCharacteristics();
  }

                                                     
  PG_TRY();
  {
                                                          
    _SPI_current->internal_xact = true;

       
                                                                           
                                                                   
                                                                    
                                                          
       
    HoldPinnedPortals();

                                                   
    ForgetPortalSnapshots();

                     
    AbortCurrentTransaction();

                                             
    StartTransactionCommand();
    if (chain)
    {
      RestoreTransactionCharacteristics();
    }

    MemoryContextSwitchTo(oldcontext);

    _SPI_current->internal_xact = false;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                                             
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

       
                                                                      
                                                                           
           
       
    AbortCurrentTransaction();

                                 
    StartTransactionCommand();
    if (chain)
    {
      RestoreTransactionCharacteristics();
    }

    MemoryContextSwitchTo(oldcontext);

    _SPI_current->internal_xact = false;

                                                                       
    ReThrowError(edata);
  }
  PG_END_TRY();
}

void
SPI_rollback(void)
{
  _SPI_rollback(false);
}

void
SPI_rollback_and_chain(void)
{
  _SPI_rollback(true);
}

   
                                                                       
                                                                            
                                
   
void
SPICleanup(void)
{
}

   
                                                      
   
void
AtEOXact_SPI(bool isCommit)
{
  bool found = false;

     
                                                                           
                                                            
     
  while (_SPI_connected >= 0)
  {
    _SPI_connection *connection = &(_SPI_stack[_SPI_connected]);

    if (connection->internal_xact)
    {
      break;
    }

    found = true;

       
                                                                          
                                                                         
                                 
       

       
                                                                       
                                                                           
                        
       
    SPI_processed = connection->outer_processed;
    SPI_tuptable = connection->outer_tuptable;
    SPI_result = connection->outer_result;

    _SPI_connected--;
    if (_SPI_connected < 0)
    {
      _SPI_current = NULL;
    }
    else
    {
      _SPI_current = &(_SPI_stack[_SPI_connected]);
    }
  }

                                                           
  if (found && isCommit)
  {
    ereport(WARNING, (errcode(ERRCODE_WARNING), errmsg("transaction left non-empty SPI stack"), errhint("Check for missing \"SPI_finish\" calls.")));
  }
}

   
                                                         
   
                                                                         
                                                                   
   
void
AtEOSubXact_SPI(bool isCommit, SubTransactionId mySubid)
{
  bool found = false;

  while (_SPI_connected >= 0)
  {
    _SPI_connection *connection = &(_SPI_stack[_SPI_connected]);

    if (connection->connectSubid != mySubid)
    {
      break;                                           
    }

    if (connection->internal_xact)
    {
      break;
    }

    found = true;

       
                                                                     
       
    if (connection->execCxt)
    {
      MemoryContextDelete(connection->execCxt);
      connection->execCxt = NULL;
    }
    if (connection->procCxt)
    {
      MemoryContextDelete(connection->procCxt);
      connection->procCxt = NULL;
    }

       
                                                                       
                                                                           
                        
       
    SPI_processed = connection->outer_processed;
    SPI_tuptable = connection->outer_tuptable;
    SPI_result = connection->outer_result;

    _SPI_connected--;
    if (_SPI_connected < 0)
    {
      _SPI_current = NULL;
    }
    else
    {
      _SPI_current = &(_SPI_stack[_SPI_connected]);
    }
  }

  if (found && isCommit)
  {
    ereport(WARNING, (errcode(ERRCODE_WARNING), errmsg("subtransaction left non-empty SPI stack"), errhint("Check for missing \"SPI_finish\" calls.")));
  }

     
                                                                          
                                                                  
     
  if (_SPI_current && !isCommit)
  {
    slist_mutable_iter siter;

       
                                                                           
                                                                          
       
    if (_SPI_current->execSubid >= mySubid)
    {
      _SPI_current->execSubid = InvalidSubTransactionId;
      MemoryContextResetAndDeleteChildren(_SPI_current->execCxt);
    }

                                                                    
    slist_foreach_modify(siter, &_SPI_current->tuptables)
    {
      SPITupleTable *tuptable;

      tuptable = slist_container(SPITupleTable, next, siter.cur);
      if (tuptable->subid >= mySubid)
      {
           
                                                                      
                                                                
                                                                  
                                               
           
        slist_delete_current(&siter);
        if (tuptable == _SPI_current->tuptable)
        {
          _SPI_current->tuptable = NULL;
        }
        if (tuptable == SPI_tuptable)
        {
          SPI_tuptable = NULL;
        }
        MemoryContextDelete(tuptable->tuptabcxt);
      }
    }
  }
}

   
                                                                           
   
bool
SPI_inside_nonatomic_context(void)
{
  if (_SPI_current == NULL)
  {
    return false;                                    
  }
  if (_SPI_current->atomic)
  {
    return false;                                              
  }
  return true;
}

                                             
int
SPI_execute(const char *src, bool read_only, long tcount)
{
  _SPI_plan plan;
  int res;

  if (src == NULL || tcount < 0)
  {
    return SPI_ERROR_ARGUMENT;
  }

  res = _SPI_begin_call(true);
  if (res < 0)
  {
    return res;
  }

  memset(&plan, 0, sizeof(_SPI_plan));
  plan.magic = _SPI_PLAN_MAGIC;
  plan.cursor_options = CURSOR_OPT_PARALLEL_OK;

  _SPI_prepare_oneshot_plan(src, &plan);

  res = _SPI_execute_plan(&plan, NULL, InvalidSnapshot, InvalidSnapshot, read_only, true, tcount);

  _SPI_end_call(true);
  return res;
}

                                     
int
SPI_exec(const char *src, long tcount)
{
  return SPI_execute(src, false, tcount);
}

                                        
int
SPI_execute_plan(SPIPlanPtr plan, Datum *Values, const char *Nulls, bool read_only, long tcount)
{
  int res;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC || tcount < 0)
  {
    return SPI_ERROR_ARGUMENT;
  }

  if (plan->nargs > 0 && Values == NULL)
  {
    return SPI_ERROR_PARAM;
  }

  res = _SPI_begin_call(true);
  if (res < 0)
  {
    return res;
  }

  res = _SPI_execute_plan(plan, _SPI_convert_params(plan->nargs, plan->argtypes, Values, Nulls), InvalidSnapshot, InvalidSnapshot, read_only, true, tcount);

  _SPI_end_call(true);
  return res;
}

                                          
int
SPI_execp(SPIPlanPtr plan, Datum *Values, const char *Nulls, long tcount)
{
  return SPI_execute_plan(plan, Values, Nulls, false, tcount);
}

                                        
int
SPI_execute_plan_with_paramlist(SPIPlanPtr plan, ParamListInfo params, bool read_only, long tcount)
{
  int res;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC || tcount < 0)
  {
    return SPI_ERROR_ARGUMENT;
  }

  res = _SPI_begin_call(true);
  if (res < 0)
  {
    return res;
  }

  res = _SPI_execute_plan(plan, params, InvalidSnapshot, InvalidSnapshot, read_only, true, tcount);

  _SPI_end_call(true);
  return res;
}

   
                                                                               
                                                                       
                                                                                
                                                                                
                       
   
                                                                            
                           
   
                                                                          
                                           
   
int
SPI_execute_snapshot(SPIPlanPtr plan, Datum *Values, const char *Nulls, Snapshot snapshot, Snapshot crosscheck_snapshot, bool read_only, bool fire_triggers, long tcount)
{
  int res;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC || tcount < 0)
  {
    return SPI_ERROR_ARGUMENT;
  }

  if (plan->nargs > 0 && Values == NULL)
  {
    return SPI_ERROR_PARAM;
  }

  res = _SPI_begin_call(true);
  if (res < 0)
  {
    return res;
  }

  res = _SPI_execute_plan(plan, _SPI_convert_params(plan->nargs, plan->argtypes, Values, Nulls), snapshot, crosscheck_snapshot, read_only, fire_triggers, tcount);

  _SPI_end_call(true);
  return res;
}

   
                                                                             
   
                                                              
                     
   
int
SPI_execute_with_args(const char *src, int nargs, Oid *argtypes, Datum *Values, const char *Nulls, bool read_only, long tcount)
{
  int res;
  _SPI_plan plan;
  ParamListInfo paramLI;

  if (src == NULL || nargs < 0 || tcount < 0)
  {
    return SPI_ERROR_ARGUMENT;
  }

  if (nargs > 0 && (argtypes == NULL || Values == NULL))
  {
    return SPI_ERROR_PARAM;
  }

  res = _SPI_begin_call(true);
  if (res < 0)
  {
    return res;
  }

  memset(&plan, 0, sizeof(_SPI_plan));
  plan.magic = _SPI_PLAN_MAGIC;
  plan.cursor_options = CURSOR_OPT_PARALLEL_OK;
  plan.nargs = nargs;
  plan.argtypes = argtypes;
  plan.parserSetup = NULL;
  plan.parserSetupArg = NULL;

  paramLI = _SPI_convert_params(nargs, argtypes, Values, Nulls);

  _SPI_prepare_oneshot_plan(src, &plan);

  res = _SPI_execute_plan(&plan, paramLI, InvalidSnapshot, InvalidSnapshot, read_only, true, tcount);

  _SPI_end_call(true);
  return res;
}

SPIPlanPtr
SPI_prepare(const char *src, int nargs, Oid *argtypes)
{
  return SPI_prepare_cursor(src, nargs, argtypes, 0);
}

SPIPlanPtr
SPI_prepare_cursor(const char *src, int nargs, Oid *argtypes, int cursorOptions)
{
  _SPI_plan plan;
  SPIPlanPtr result;

  if (src == NULL || nargs < 0 || (nargs > 0 && argtypes == NULL))
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return NULL;
  }

  SPI_result = _SPI_begin_call(true);
  if (SPI_result < 0)
  {
    return NULL;
  }

  memset(&plan, 0, sizeof(_SPI_plan));
  plan.magic = _SPI_PLAN_MAGIC;
  plan.cursor_options = cursorOptions;
  plan.nargs = nargs;
  plan.argtypes = argtypes;
  plan.parserSetup = NULL;
  plan.parserSetupArg = NULL;

  _SPI_prepare_plan(src, &plan);

                                      
  result = _SPI_make_plan_non_temp(&plan);

  _SPI_end_call(true);

  return result;
}

SPIPlanPtr
SPI_prepare_params(const char *src, ParserSetupHook parserSetup, void *parserSetupArg, int cursorOptions)
{
  _SPI_plan plan;
  SPIPlanPtr result;

  if (src == NULL)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return NULL;
  }

  SPI_result = _SPI_begin_call(true);
  if (SPI_result < 0)
  {
    return NULL;
  }

  memset(&plan, 0, sizeof(_SPI_plan));
  plan.magic = _SPI_PLAN_MAGIC;
  plan.cursor_options = cursorOptions;
  plan.nargs = 0;
  plan.argtypes = NULL;
  plan.parserSetup = parserSetup;
  plan.parserSetupArg = parserSetupArg;

  _SPI_prepare_plan(src, &plan);

                                      
  result = _SPI_make_plan_non_temp(&plan);

  _SPI_end_call(true);

  return result;
}

int
SPI_keepplan(SPIPlanPtr plan)
{
  ListCell *lc;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC || plan->saved || plan->oneshot)
  {
    return SPI_ERROR_ARGUMENT;
  }

     
                                                                           
                                                                      
                                                                      
     
  plan->saved = true;
  MemoryContextSetParent(plan->plancxt, CacheMemoryContext);

  foreach (lc, plan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc);

    SaveCachedPlan(plansource);
  }

  return 0;
}

SPIPlanPtr
SPI_saveplan(SPIPlanPtr plan)
{
  SPIPlanPtr newplan;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return NULL;
  }

  SPI_result = _SPI_begin_call(false);                           
  if (SPI_result < 0)
  {
    return NULL;
  }

  newplan = _SPI_save_plan(plan);

  SPI_result = _SPI_end_call(false);

  return newplan;
}

int
SPI_freeplan(SPIPlanPtr plan)
{
  ListCell *lc;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC)
  {
    return SPI_ERROR_ARGUMENT;
  }

                                     
  foreach (lc, plan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc);

    DropCachedPlan(plansource);
  }

                                                                       
  MemoryContextDelete(plan->plancxt);

  return 0;
}

HeapTuple
SPI_copytuple(HeapTuple tuple)
{
  MemoryContext oldcxt;
  HeapTuple ctuple;

  if (tuple == NULL)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return NULL;
  }

  if (_SPI_current == NULL)
  {
    SPI_result = SPI_ERROR_UNCONNECTED;
    return NULL;
  }

  oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);

  ctuple = heap_copytuple(tuple);

  MemoryContextSwitchTo(oldcxt);

  return ctuple;
}

HeapTupleHeader
SPI_returntuple(HeapTuple tuple, TupleDesc tupdesc)
{
  MemoryContext oldcxt;
  HeapTupleHeader dtup;

  if (tuple == NULL || tupdesc == NULL)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return NULL;
  }

  if (_SPI_current == NULL)
  {
    SPI_result = SPI_ERROR_UNCONNECTED;
    return NULL;
  }

                                                                
  if (tupdesc->tdtypeid == RECORDOID && tupdesc->tdtypmod < 0)
  {
    assign_record_type_typmod(tupdesc);
  }

  oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);

  dtup = DatumGetHeapTupleHeader(heap_copy_tuple_as_datum(tuple, tupdesc));

  MemoryContextSwitchTo(oldcxt);

  return dtup;
}

HeapTuple
SPI_modifytuple(Relation rel, HeapTuple tuple, int natts, int *attnum, Datum *Values, const char *Nulls)
{
  MemoryContext oldcxt;
  HeapTuple mtuple;
  int numberOfAttributes;
  Datum *v;
  bool *n;
  int i;

  if (rel == NULL || tuple == NULL || natts < 0 || attnum == NULL || Values == NULL)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return NULL;
  }

  if (_SPI_current == NULL)
  {
    SPI_result = SPI_ERROR_UNCONNECTED;
    return NULL;
  }

  oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);

  SPI_result = 0;

  numberOfAttributes = rel->rd_att->natts;
  v = (Datum *)palloc(numberOfAttributes * sizeof(Datum));
  n = (bool *)palloc(numberOfAttributes * sizeof(bool));

                                  
  heap_deform_tuple(tuple, rel->rd_att, v, n);

                                
  for (i = 0; i < natts; i++)
  {
    if (attnum[i] <= 0 || attnum[i] > numberOfAttributes)
    {
      break;
    }
    v[attnum[i] - 1] = Values[i];
    n[attnum[i] - 1] = (Nulls && Nulls[i] == 'n') ? true : false;
  }

  if (i == natts)                           
  {
    mtuple = heap_form_tuple(rel->rd_att, v, n);

       
                                                                          
                    
       
    mtuple->t_data->t_ctid = tuple->t_data->t_ctid;
    mtuple->t_self = tuple->t_self;
    mtuple->t_tableOid = tuple->t_tableOid;
  }
  else
  {
    mtuple = NULL;
    SPI_result = SPI_ERROR_NOATTRIBUTE;
  }

  pfree(v);
  pfree(n);

  MemoryContextSwitchTo(oldcxt);

  return mtuple;
}

int
SPI_fnumber(TupleDesc tupdesc, const char *fname)
{
  int res;
  const FormData_pg_attribute *sysatt;

  for (res = 0; res < tupdesc->natts; res++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, res);

    if (namestrcmp(&attr->attname, fname) == 0 && !attr->attisdropped)
    {
      return res + 1;
    }
  }

  sysatt = SystemAttributeByName(fname);
  if (sysatt != NULL)
  {
    return sysatt->attnum;
  }

                                                                      
  return SPI_ERROR_NOATTRIBUTE;
}

char *
SPI_fname(TupleDesc tupdesc, int fnumber)
{
  const FormData_pg_attribute *att;

  SPI_result = 0;

  if (fnumber > tupdesc->natts || fnumber == 0 || fnumber <= FirstLowInvalidHeapAttributeNumber)
  {
    SPI_result = SPI_ERROR_NOATTRIBUTE;
    return NULL;
  }

  if (fnumber > 0)
  {
    att = TupleDescAttr(tupdesc, fnumber - 1);
  }
  else
  {
    att = SystemAttributeDefinition(fnumber);
  }

  return pstrdup(NameStr(att->attname));
}

char *
SPI_getvalue(HeapTuple tuple, TupleDesc tupdesc, int fnumber)
{
  Datum val;
  bool isnull;
  Oid typoid, foutoid;
  bool typisvarlena;

  SPI_result = 0;

  if (fnumber > tupdesc->natts || fnumber == 0 || fnumber <= FirstLowInvalidHeapAttributeNumber)
  {
    SPI_result = SPI_ERROR_NOATTRIBUTE;
    return NULL;
  }

  val = heap_getattr(tuple, fnumber, tupdesc, &isnull);
  if (isnull)
  {
    return NULL;
  }

  if (fnumber > 0)
  {
    typoid = TupleDescAttr(tupdesc, fnumber - 1)->atttypid;
  }
  else
  {
    typoid = (SystemAttributeDefinition(fnumber))->atttypid;
  }

  getTypeOutputInfo(typoid, &foutoid, &typisvarlena);

  return OidOutputFunctionCall(foutoid, val);
}

Datum
SPI_getbinval(HeapTuple tuple, TupleDesc tupdesc, int fnumber, bool *isnull)
{
  SPI_result = 0;

  if (fnumber > tupdesc->natts || fnumber == 0 || fnumber <= FirstLowInvalidHeapAttributeNumber)
  {
    SPI_result = SPI_ERROR_NOATTRIBUTE;
    *isnull = true;
    return (Datum)NULL;
  }

  return heap_getattr(tuple, fnumber, tupdesc, isnull);
}

char *
SPI_gettype(TupleDesc tupdesc, int fnumber)
{
  Oid typoid;
  HeapTuple typeTuple;
  char *result;

  SPI_result = 0;

  if (fnumber > tupdesc->natts || fnumber == 0 || fnumber <= FirstLowInvalidHeapAttributeNumber)
  {
    SPI_result = SPI_ERROR_NOATTRIBUTE;
    return NULL;
  }

  if (fnumber > 0)
  {
    typoid = TupleDescAttr(tupdesc, fnumber - 1)->atttypid;
  }
  else
  {
    typoid = (SystemAttributeDefinition(fnumber))->atttypid;
  }

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typoid));

  if (!HeapTupleIsValid(typeTuple))
  {
    SPI_result = SPI_ERROR_TYPUNKNOWN;
    return NULL;
  }

  result = pstrdup(NameStr(((Form_pg_type)GETSTRUCT(typeTuple))->typname));
  ReleaseSysCache(typeTuple);
  return result;
}

   
                                       
   
                                                                            
                                                  
   
Oid
SPI_gettypeid(TupleDesc tupdesc, int fnumber)
{
  SPI_result = 0;

  if (fnumber > tupdesc->natts || fnumber == 0 || fnumber <= FirstLowInvalidHeapAttributeNumber)
  {
    SPI_result = SPI_ERROR_NOATTRIBUTE;
    return InvalidOid;
  }

  if (fnumber > 0)
  {
    return TupleDescAttr(tupdesc, fnumber - 1)->atttypid;
  }
  else
  {
    return (SystemAttributeDefinition(fnumber))->atttypid;
  }
}

char *
SPI_getrelname(Relation rel)
{
  return pstrdup(RelationGetRelationName(rel));
}

char *
SPI_getnspname(Relation rel)
{
  return get_namespace_name(RelationGetNamespace(rel));
}

void *
SPI_palloc(Size size)
{
  if (_SPI_current == NULL)
  {
    elog(ERROR, "SPI_palloc called while not connected to SPI");
  }

  return MemoryContextAlloc(_SPI_current->savedcxt, size);
}

void *
SPI_repalloc(void *pointer, Size size)
{
                                                             
  return repalloc(pointer, size);
}

void
SPI_pfree(void *pointer)
{
                                                             
  pfree(pointer);
}

Datum
SPI_datumTransfer(Datum value, bool typByVal, int typLen)
{
  MemoryContext oldcxt;
  Datum result;

  if (_SPI_current == NULL)
  {
    elog(ERROR, "SPI_datumTransfer called while not connected to SPI");
  }

  oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);

  result = datumTransfer(value, typByVal, typLen);

  MemoryContextSwitchTo(oldcxt);

  return result;
}

void
SPI_freetuple(HeapTuple tuple)
{
                                                             
  heap_freetuple(tuple);
}

void
SPI_freetuptable(SPITupleTable *tuptable)
{
  bool found = false;

                                   
  if (tuptable == NULL)
  {
    return;
  }

     
                                                                     
     
  if (_SPI_current != NULL)
  {
    slist_mutable_iter siter;

                                                      
    slist_foreach_modify(siter, &_SPI_current->tuptables)
    {
      SPITupleTable *tt;

      tt = slist_container(SPITupleTable, next, siter.cur);
      if (tt == tuptable)
      {
        slist_delete_current(&siter);
        found = true;
        break;
      }
    }
  }

     
                                                                          
                                                                          
                                                                          
                                                                        
     
  if (!found)
  {
    elog(WARNING, "attempt to delete invalid SPITupleTable %p", tuptable);
    return;
  }

                                                                       
  if (tuptable == _SPI_current->tuptable)
  {
    _SPI_current->tuptable = NULL;
  }
  if (tuptable == SPI_tuptable)
  {
    SPI_tuptable = NULL;
  }

                                                
  MemoryContextDelete(tuptable->tuptabcxt);
}

   
                     
   
                                        
   
Portal
SPI_cursor_open(const char *name, SPIPlanPtr plan, Datum *Values, const char *Nulls, bool read_only)
{
  Portal portal;
  ParamListInfo paramLI;

                                                         
  paramLI = _SPI_convert_params(plan->nargs, plan->argtypes, Values, Nulls);

  portal = SPI_cursor_open_internal(name, plan, paramLI, read_only);

                                             
  if (paramLI)
  {
    pfree(paramLI);
  }

  return portal;
}

   
                               
   
                                                   
   
Portal
SPI_cursor_open_with_args(const char *name, const char *src, int nargs, Oid *argtypes, Datum *Values, const char *Nulls, bool read_only, int cursorOptions)
{
  Portal result;
  _SPI_plan plan;
  ParamListInfo paramLI;

  if (src == NULL || nargs < 0)
  {
    elog(ERROR, "SPI_cursor_open_with_args called with invalid arguments");
  }

  if (nargs > 0 && (argtypes == NULL || Values == NULL))
  {
    elog(ERROR, "SPI_cursor_open_with_args called with missing parameters");
  }

  SPI_result = _SPI_begin_call(true);
  if (SPI_result < 0)
  {
    elog(ERROR, "SPI_cursor_open_with_args called while not connected");
  }

  memset(&plan, 0, sizeof(_SPI_plan));
  plan.magic = _SPI_PLAN_MAGIC;
  plan.cursor_options = cursorOptions;
  plan.nargs = nargs;
  plan.argtypes = argtypes;
  plan.parserSetup = NULL;
  plan.parserSetupArg = NULL;

                                                         
  paramLI = _SPI_convert_params(nargs, argtypes, Values, Nulls);

  _SPI_prepare_plan(src, &plan);

                                                                     

  result = SPI_cursor_open_internal(name, &plan, paramLI, read_only);

                    
  _SPI_end_call(true);

  return result;
}

   
                                    
   
                                                                      
                                                                          
   
Portal
SPI_cursor_open_with_paramlist(const char *name, SPIPlanPtr plan, ParamListInfo params, bool read_only)
{
  return SPI_cursor_open_internal(name, plan, params, read_only);
}

   
                              
   
                                            
   
static Portal
SPI_cursor_open_internal(const char *name, SPIPlanPtr plan, ParamListInfo paramLI, bool read_only)
{
  CachedPlanSource *plansource;
  CachedPlan *cplan;
  List *stmt_list;
  char *query_string;
  Snapshot snapshot;
  MemoryContext oldcontext;
  Portal portal;
  ErrorContextCallback spierrcontext;

     
                                                                           
                             
     
  if (!SPI_is_cursor_plan(plan))
  {
                                          
    if (list_length(plan->plancache_list) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_DEFINITION), errmsg("cannot open multi-query plan as cursor")));
    }
    plansource = (CachedPlanSource *)linitial(plan->plancache_list);
    ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
                                                                               
                       errmsg("cannot open %s query as cursor", plansource->commandTag)));
  }

  Assert(list_length(plan->plancache_list) == 1);
  plansource = (CachedPlanSource *)linitial(plan->plancache_list);

                          
  if (_SPI_begin_call(true) < 0)
  {
    elog(ERROR, "SPI_cursor_open called while not connected");
  }

                                                                   
  SPI_processed = 0;
  SPI_tuptable = NULL;
  _SPI_current->processed = 0;
  _SPI_current->tuptable = NULL;

                         
  if (name == NULL || name[0] == '\0')
  {
                                          
    portal = CreateNewPortal();
  }
  else
  {
                                                                   
    portal = CreatePortal(name, false, false);
  }

                                                    
  query_string = MemoryContextStrdup(portal->portalContext, plansource->query_string);

     
                                                                        
                      
     
  spierrcontext.callback = _SPI_error_callback;
  spierrcontext.arg = unconstify(char *, plansource->query_string);
  spierrcontext.previous = error_context_stack;
  error_context_stack = &spierrcontext;

     
                                                                       
                                                                           
                         
     

                                                                
  cplan = GetCachedPlan(plansource, paramLI, false, _SPI_current->queryEnv);
  stmt_list = cplan->stmt_list;

  if (!plan->saved)
  {
       
                                                                          
                                                                       
                                                                       
                                                                      
       
    oldcontext = MemoryContextSwitchTo(portal->portalContext);
    stmt_list = copyObject(stmt_list);
    MemoryContextSwitchTo(oldcontext);
    ReleaseCachedPlan(cplan, false);
    cplan = NULL;                                       
  }

     
                        
     
  PortalDefineQuery(portal, NULL,                        
      query_string, plansource->commandTag, stmt_list, cplan);

     
                                                                            
                                   
     
  portal->cursorOptions = plan->cursor_options;
  if (!(portal->cursorOptions & (CURSOR_OPT_SCROLL | CURSOR_OPT_NO_SCROLL)))
  {
    if (list_length(stmt_list) == 1 && linitial_node(PlannedStmt, stmt_list)->commandType != CMD_UTILITY && linitial_node(PlannedStmt, stmt_list)->rowMarks == NIL && ExecSupportsBackwardScan(linitial_node(PlannedStmt, stmt_list)->planTree))
    {
      portal->cursorOptions |= CURSOR_OPT_SCROLL;
    }
    else
    {
      portal->cursorOptions |= CURSOR_OPT_NO_SCROLL;
    }
  }

     
                                                                             
                                                                          
                                  
     
  if (portal->cursorOptions & CURSOR_OPT_SCROLL)
  {
    if (list_length(stmt_list) == 1 && linitial_node(PlannedStmt, stmt_list)->commandType != CMD_UTILITY && linitial_node(PlannedStmt, stmt_list)->rowMarks != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DECLARE SCROLL CURSOR ... FOR UPDATE/SHARE is not supported"), errdetail("Scrollable cursors must be READ ONLY.")));
    }
  }

                                                                             
  portal->queryEnv = _SPI_current->queryEnv;

     
                                                                             
                                                                            
                                                                            
                                                                         
                                                                     
                
     
  if (read_only || IsInParallelMode())
  {
    ListCell *lc;

    foreach (lc, stmt_list)
    {
      PlannedStmt *pstmt = lfirst_node(PlannedStmt, lc);

      if (!CommandIsReadOnly(pstmt))
      {
        if (read_only)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                         
                             errmsg("%s is not allowed in a non-volatile function", CreateCommandTag((Node *)pstmt))));
        }
        else
        {
          PreventCommandIfParallelMode(CreateCommandTag((Node *)pstmt));
        }
      }
    }
  }

                                   
  if (read_only)
  {
    snapshot = GetActiveSnapshot();
  }
  else
  {
    CommandCounterIncrement();
    snapshot = GetTransactionSnapshot();
  }

     
                                                                            
                                                                            
                                                                       
     
  if (paramLI)
  {
    oldcontext = MemoryContextSwitchTo(portal->portalContext);
    paramLI = copyParamList(paramLI);
    MemoryContextSwitchTo(oldcontext);
  }

     
                             
     
  PortalStart(portal, paramLI, 0, snapshot);

  Assert(portal->strategy != PORTAL_MULTI_QUERY);

                                   
  error_context_stack = spierrcontext.previous;

                         
  _SPI_end_call(true);

                                 
  return portal;
}

   
                     
   
                                              
   
Portal
SPI_cursor_find(const char *name)
{
  return GetPortalByName(name);
}

   
                      
   
                          
   
void
SPI_cursor_fetch(Portal portal, bool forward, long count)
{
  _SPI_cursor_operation(portal, forward ? FETCH_FORWARD : FETCH_BACKWARD, count, CreateDestReceiver(DestSPI));
                                                                     
}

   
                     
   
                    
   
void
SPI_cursor_move(Portal portal, bool forward, long count)
{
  _SPI_cursor_operation(portal, forward ? FETCH_FORWARD : FETCH_BACKWARD, count, None_Receiver);
}

   
                             
   
                                     
   
void
SPI_scroll_cursor_fetch(Portal portal, FetchDirection direction, long count)
{
  _SPI_cursor_operation(portal, direction, count, CreateDestReceiver(DestSPI));
                                                                     
}

   
                            
   
                               
   
void
SPI_scroll_cursor_move(Portal portal, FetchDirection direction, long count)
{
  _SPI_cursor_operation(portal, direction, count, None_Receiver);
}

   
                      
   
                  
   
void
SPI_cursor_close(Portal portal)
{
  if (!PortalIsValid(portal))
  {
    elog(ERROR, "invalid portal in SPI cursor operation");
  }

  PortalDrop(portal, false);
}

   
                                                                            
                               
   
Oid
SPI_getargtypeid(SPIPlanPtr plan, int argIndex)
{
  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC || argIndex < 0 || argIndex >= plan->nargs)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return InvalidOid;
  }
  return plan->argtypes[argIndex];
}

   
                                                          
   
int
SPI_getargcount(SPIPlanPtr plan)
{
  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return -1;
  }
  return plan->nargs;
}

   
                                                         
                                                                
                                                               
                                                                        
   
              
                                                        
   
bool
SPI_is_cursor_plan(SPIPlanPtr plan)
{
  CachedPlanSource *plansource;

  if (plan == NULL || plan->magic != _SPI_PLAN_MAGIC)
  {
    SPI_result = SPI_ERROR_ARGUMENT;
    return false;
  }

  if (list_length(plan->plancache_list) != 1)
  {
    SPI_result = 0;
    return false;                                        
  }
  plansource = (CachedPlanSource *)linitial(plan->plancache_list);

     
                                                                           
                                                                         
                                                                          
     
  SPI_result = 0;

                              
  if (plansource->resultDesc)
  {
    return true;
  }

  return false;
}

   
                                                                    
                                                           
   
                                                      
   
bool
SPI_plan_is_valid(SPIPlanPtr plan)
{
  ListCell *lc;

  Assert(plan->magic == _SPI_PLAN_MAGIC);

  foreach (lc, plan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc);

    if (!CachedPlanIsValid(plansource))
    {
      return false;
    }
  }
  return true;
}

   
                                                                      
   
                                                                       
                                                                          
                          
   
const char *
SPI_result_code_string(int code)
{
  static char buf[64];

  switch (code)
  {
  case SPI_ERROR_CONNECT:
    return "SPI_ERROR_CONNECT";
  case SPI_ERROR_COPY:
    return "SPI_ERROR_COPY";
  case SPI_ERROR_OPUNKNOWN:
    return "SPI_ERROR_OPUNKNOWN";
  case SPI_ERROR_UNCONNECTED:
    return "SPI_ERROR_UNCONNECTED";
  case SPI_ERROR_ARGUMENT:
    return "SPI_ERROR_ARGUMENT";
  case SPI_ERROR_PARAM:
    return "SPI_ERROR_PARAM";
  case SPI_ERROR_TRANSACTION:
    return "SPI_ERROR_TRANSACTION";
  case SPI_ERROR_NOATTRIBUTE:
    return "SPI_ERROR_NOATTRIBUTE";
  case SPI_ERROR_NOOUTFUNC:
    return "SPI_ERROR_NOOUTFUNC";
  case SPI_ERROR_TYPUNKNOWN:
    return "SPI_ERROR_TYPUNKNOWN";
  case SPI_ERROR_REL_DUPLICATE:
    return "SPI_ERROR_REL_DUPLICATE";
  case SPI_ERROR_REL_NOT_FOUND:
    return "SPI_ERROR_REL_NOT_FOUND";
  case SPI_OK_CONNECT:
    return "SPI_OK_CONNECT";
  case SPI_OK_FINISH:
    return "SPI_OK_FINISH";
  case SPI_OK_FETCH:
    return "SPI_OK_FETCH";
  case SPI_OK_UTILITY:
    return "SPI_OK_UTILITY";
  case SPI_OK_SELECT:
    return "SPI_OK_SELECT";
  case SPI_OK_SELINTO:
    return "SPI_OK_SELINTO";
  case SPI_OK_INSERT:
    return "SPI_OK_INSERT";
  case SPI_OK_DELETE:
    return "SPI_OK_DELETE";
  case SPI_OK_UPDATE:
    return "SPI_OK_UPDATE";
  case SPI_OK_CURSOR:
    return "SPI_OK_CURSOR";
  case SPI_OK_INSERT_RETURNING:
    return "SPI_OK_INSERT_RETURNING";
  case SPI_OK_DELETE_RETURNING:
    return "SPI_OK_DELETE_RETURNING";
  case SPI_OK_UPDATE_RETURNING:
    return "SPI_OK_UPDATE_RETURNING";
  case SPI_OK_REWRITTEN:
    return "SPI_OK_REWRITTEN";
  case SPI_OK_REL_REGISTER:
    return "SPI_OK_REL_REGISTER";
  case SPI_OK_REL_UNREGISTER:
    return "SPI_OK_REL_UNREGISTER";
  }
                                                         
  sprintf(buf, "Unrecognized SPI code %d", code);
  return buf;
}

   
                                                                     
                      
   
                                                                             
                                                                       
                                                                           
   
List *
SPI_plan_get_plan_sources(SPIPlanPtr plan)
{
  Assert(plan->magic == _SPI_PLAN_MAGIC);
  return plan->plancache_list;
}

   
                                                                     
                                                                   
                                                                      
   
                                                                             
                                                                       
                                                                           
   
CachedPlan *
SPI_plan_get_cached_plan(SPIPlanPtr plan)
{
  CachedPlanSource *plansource;
  CachedPlan *cplan;
  ErrorContextCallback spierrcontext;

  Assert(plan->magic == _SPI_PLAN_MAGIC);

                                         
  if (plan->oneshot)
  {
    return NULL;
  }

                                              
  if (list_length(plan->plancache_list) != 1)
  {
    return NULL;
  }
  plansource = (CachedPlanSource *)linitial(plan->plancache_list);

                                                   
  spierrcontext.callback = _SPI_error_callback;
  spierrcontext.arg = unconstify(char *, plansource->query_string);
  spierrcontext.previous = error_context_stack;
  error_context_stack = &spierrcontext;

                                          
  cplan = GetCachedPlan(plansource, NULL, plan->saved, _SPI_current->queryEnv);
  Assert(cplan == plansource->gplan);

                                   
  error_context_stack = spierrcontext.previous;

  return cplan;
}

                                                               

   
                    
                                                                  
                             
   
void
spi_dest_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
  SPITupleTable *tuptable;
  MemoryContext oldcxt;
  MemoryContext tuptabcxt;

  if (_SPI_current == NULL)
  {
    elog(ERROR, "spi_dest_startup called while not connected to SPI");
  }

  if (_SPI_current->tuptable != NULL)
  {
    elog(ERROR, "improper call to spi_dest_startup");
  }

                                                               

  oldcxt = _SPI_procmem();                                         

  tuptabcxt = AllocSetContextCreate(CurrentMemoryContext, "SPI TupTable", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(tuptabcxt);

  _SPI_current->tuptable = tuptable = (SPITupleTable *)palloc0(sizeof(SPITupleTable));
  tuptable->tuptabcxt = tuptabcxt;
  tuptable->subid = GetCurrentSubTransactionId();

     
                                                                             
                                                                          
                                                                     
     
  slist_push_head(&_SPI_current->tuptables, &tuptable->next);

                                  
  tuptable->alloced = tuptable->free = 128;
  tuptable->vals = (HeapTuple *)palloc(tuptable->alloced * sizeof(HeapTuple));
  tuptable->tupdesc = CreateTupleDescCopy(typeinfo);

  MemoryContextSwitchTo(oldcxt);
}

   
                
                                                         
                             
   
bool
spi_printtup(TupleTableSlot *slot, DestReceiver *self)
{
  SPITupleTable *tuptable;
  MemoryContext oldcxt;

  if (_SPI_current == NULL)
  {
    elog(ERROR, "spi_printtup called while not connected to SPI");
  }

  tuptable = _SPI_current->tuptable;
  if (tuptable == NULL)
  {
    elog(ERROR, "improper call to spi_printtup");
  }

  oldcxt = MemoryContextSwitchTo(tuptable->tuptabcxt);

  if (tuptable->free == 0)
  {
                                              
    tuptable->free = tuptable->alloced;
    tuptable->alloced += tuptable->free;
    tuptable->vals = (HeapTuple *)repalloc_huge(tuptable->vals, tuptable->alloced * sizeof(HeapTuple));
  }

  tuptable->vals[tuptable->alloced - tuptable->free] = ExecCopySlotHeapTuple(slot);
  (tuptable->free)--;

  MemoryContextSwitchTo(oldcxt);

  return true;
}

   
                    
   

   
                                    
   
                                                                                
                                                                          
   
                                                                       
                                                                              
                                                                          
                                                                          
                                                 
   
static void
_SPI_prepare_plan(const char *src, SPIPlanPtr plan)
{
  List *raw_parsetree_list;
  List *plancache_list;
  ListCell *list_item;
  ErrorContextCallback spierrcontext;

     
                                                 
     
  spierrcontext.callback = _SPI_error_callback;
  spierrcontext.arg = unconstify(char *, src);
  spierrcontext.previous = error_context_stack;
  error_context_stack = &spierrcontext;

     
                                                              
     
  raw_parsetree_list = pg_parse_query(src);

     
                                                                            
                                             
     
  plancache_list = NIL;

  foreach (list_item, raw_parsetree_list)
  {
    RawStmt *parsetree = lfirst_node(RawStmt, list_item);
    List *stmt_list;
    CachedPlanSource *plansource;

       
                                                                         
                                                   
       
    plansource = CreateCachedPlan(parsetree, src, CreateCommandTag(parsetree->stmt));

       
                                                                       
                                                  
       
    if (plan->parserSetup != NULL)
    {
      Assert(plan->nargs == 0);
      stmt_list = pg_analyze_and_rewrite_params(parsetree, src, plan->parserSetup, plan->parserSetupArg, _SPI_current->queryEnv);
    }
    else
    {
      stmt_list = pg_analyze_and_rewrite(parsetree, src, plan->argtypes, plan->nargs, _SPI_current->queryEnv);
    }

                                                
    CompleteCachedPlan(plansource, stmt_list, NULL, plan->argtypes, plan->nargs, plan->parserSetup, plan->parserSetupArg, plan->cursor_options, false);                       

    plancache_list = lappend(plancache_list, plansource);
  }

  plan->plancache_list = plancache_list;
  plan->oneshot = false;

     
                                 
     
  error_context_stack = spierrcontext.previous;
}

   
                                            
   
                                                                           
                                                                 
                                                                   
   
                                                                          
                                                                          
                                                                       
                                                                     
                                                                   
   
                                                                       
                                                                              
                                                                          
                                                                          
                                                 
   
static void
_SPI_prepare_oneshot_plan(const char *src, SPIPlanPtr plan)
{
  List *raw_parsetree_list;
  List *plancache_list;
  ListCell *list_item;
  ErrorContextCallback spierrcontext;

     
                                                 
     
  spierrcontext.callback = _SPI_error_callback;
  spierrcontext.arg = unconstify(char *, src);
  spierrcontext.previous = error_context_stack;
  error_context_stack = &spierrcontext;

     
                                                              
     
  raw_parsetree_list = pg_parse_query(src);

     
                                                                   
     
  plancache_list = NIL;

  foreach (list_item, raw_parsetree_list)
  {
    RawStmt *parsetree = lfirst_node(RawStmt, list_item);
    CachedPlanSource *plansource;

    plansource = CreateOneShotCachedPlan(parsetree, src, CreateCommandTag(parsetree->stmt));

    plancache_list = lappend(plancache_list, plansource);
  }

  plan->plancache_list = plancache_list;
  plan->oneshot = true;

     
                                 
     
  error_context_stack = spierrcontext.previous;
}

   
                                                          
   
                                                                      
                                                      
                                                                    
                                                                        
                                                                             
                                                                       
                                                      
   
static int
_SPI_execute_plan(SPIPlanPtr plan, ParamListInfo paramLI, Snapshot snapshot, Snapshot crosscheck_snapshot, bool read_only, bool fire_triggers, uint64 tcount)
{
  int my_res = 0;
  uint64 my_processed = 0;
  SPITupleTable *my_tuptable = NULL;
  int res = 0;
  bool allow_nonatomic = plan->no_snapshots;                      
  bool pushed_active_snap = false;
  ErrorContextCallback spierrcontext;
  CachedPlan *cplan = NULL;
  ListCell *lc1;

     
                                                 
     
  spierrcontext.callback = _SPI_error_callback;
  spierrcontext.arg = NULL;                            
  spierrcontext.previous = error_context_stack;
  error_context_stack = &spierrcontext;

     
                                                             
     
                                                                          
               
     
                                                                             
                                                                 
     
                                                                       
                                                                           
     
                                                                     
                                                                            
                                   
     
                                                                           
                              
     
                                                                       
              
     
  if (snapshot != InvalidSnapshot)
  {
    Assert(!allow_nonatomic);
    if (read_only)
    {
      PushActiveSnapshot(snapshot);
      pushed_active_snap = true;
    }
    else
    {
                                                                      
      PushCopiedSnapshot(snapshot);
      pushed_active_snap = true;
    }
  }

  foreach (lc1, plan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc1);
    List *stmt_list;
    ListCell *lc2;

    spierrcontext.arg = unconstify(char *, plansource->query_string);

       
                                                                       
       
    if (plan->oneshot)
    {
      RawStmt *parsetree = plansource->raw_parse_tree;
      const char *src = plansource->query_string;
      List *stmt_list;

         
                                                                         
                                                    
         
      if (parsetree == NULL)
      {
        stmt_list = NIL;
      }
      else if (plan->parserSetup != NULL)
      {
        Assert(plan->nargs == 0);
        stmt_list = pg_analyze_and_rewrite_params(parsetree, src, plan->parserSetup, plan->parserSetupArg, _SPI_current->queryEnv);
      }
      else
      {
        stmt_list = pg_analyze_and_rewrite(parsetree, src, plan->argtypes, plan->nargs, _SPI_current->queryEnv);
      }

                                                  
      CompleteCachedPlan(plansource, stmt_list, NULL, plan->argtypes, plan->nargs, plan->parserSetup, plan->parserSetupArg, plan->cursor_options, false);                       
    }

       
                                                                       
                                                                      
       
    cplan = GetCachedPlan(plansource, paramLI, plan->saved, _SPI_current->queryEnv);
    stmt_list = cplan->stmt_list;

       
                                                                         
                                              
       
    if (snapshot == InvalidSnapshot && (list_length(stmt_list) > 1 || (list_length(stmt_list) == 1 && PlannedStmtRequiresSnapshot(linitial_node(PlannedStmt, stmt_list)))))
    {
         
                                                                         
                                                                        
                                                                         
                                                                        
                                                                        
                                                                    
                                        
         
      EnsurePortalSnapshotExists();

         
                                                                         
                                                                     
                                                                  
                                                        
         
      if (!read_only && !allow_nonatomic)
      {
        if (pushed_active_snap)
        {
          PopActiveSnapshot();
        }
        PushActiveSnapshot(GetTransactionSnapshot());
        pushed_active_snap = true;
      }
    }

    foreach (lc2, stmt_list)
    {
      PlannedStmt *stmt = lfirst_node(PlannedStmt, lc2);
      bool canSetTag = stmt->canSetTag;
      DestReceiver *dest;

      _SPI_current->processed = 0;
      _SPI_current->tuptable = NULL;

                                        
      if (stmt->utilityStmt)
      {
        if (IsA(stmt->utilityStmt, CopyStmt))
        {
          CopyStmt *cstmt = (CopyStmt *)stmt->utilityStmt;

          if (cstmt->filename == NULL)
          {
            my_res = SPI_ERROR_COPY;
            goto fail;
          }
        }
        else if (IsA(stmt->utilityStmt, TransactionStmt))
        {
          my_res = SPI_ERROR_TRANSACTION;
          goto fail;
        }
      }

      if (read_only && !CommandIsReadOnly(stmt))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                       
                           errmsg("%s is not allowed in a non-volatile function", CreateCommandTag((Node *)stmt))));
      }

      if (IsInParallelMode() && !CommandIsReadOnly(stmt))
      {
        PreventCommandIfParallelMode(CreateCommandTag((Node *)stmt));
      }

         
                                                                        
                                                                        
                                   
         
      if (!read_only && pushed_active_snap)
      {
        CommandCounterIncrement();
        UpdateActiveSnapshotCommandId();
      }

      dest = CreateDestReceiver(canSetTag ? DestSPI : DestNone);

      if (stmt->utilityStmt == NULL)
      {
        QueryDesc *qdesc;
        Snapshot snap;

        if (ActiveSnapshotSet())
        {
          snap = GetActiveSnapshot();
        }
        else
        {
          snap = InvalidSnapshot;
        }

        qdesc = CreateQueryDesc(stmt, plansource->query_string, snap, crosscheck_snapshot, dest, paramLI, _SPI_current->queryEnv, 0);
        res = _SPI_pquery(qdesc, fire_triggers, canSetTag ? tcount : 0);
        FreeQueryDesc(qdesc);
      }
      else
      {
        char completionTag[COMPLETION_TAG_BUFSIZE];
        ProcessUtilityContext context;

           
                                                                      
                                                                       
                              
           
        if (_SPI_current->atomic || !allow_nonatomic)
        {
          context = PROCESS_UTILITY_QUERY;
        }
        else
        {
          context = PROCESS_UTILITY_QUERY_NONATOMIC;
        }

        ProcessUtility(stmt, plansource->query_string, context, paramLI, _SPI_current->queryEnv, dest, completionTag);

                                                        
        if (_SPI_current->tuptable)
        {
          _SPI_current->processed = _SPI_current->tuptable->alloced - _SPI_current->tuptable->free;
        }

        res = SPI_OK_UTILITY;

           
                                                                       
                                                  
           
        if (IsA(stmt->utilityStmt, CreateTableAsStmt))
        {
          CreateTableAsStmt *ctastmt = (CreateTableAsStmt *)stmt->utilityStmt;

          if (strncmp(completionTag, "SELECT ", 7) == 0)
          {
            _SPI_current->processed = pg_strtouint64(completionTag + 7, NULL, 10);
          }
          else
          {
               
                                                               
                                        
               
            Assert(ctastmt->if_not_exists || ctastmt->into->skipData);
            _SPI_current->processed = 0;
          }

             
                                                                    
                                                           
             
          if (ctastmt->is_select_into)
          {
            res = SPI_OK_SELINTO;
          }
        }
        else if (IsA(stmt->utilityStmt, CopyStmt))
        {
          Assert(strncmp(completionTag, "COPY ", 5) == 0);
          _SPI_current->processed = pg_strtouint64(completionTag + 5, NULL, 10);
        }
      }

         
                                                                         
                                                                    
                                             
         
      if (canSetTag)
      {
        my_processed = _SPI_current->processed;
        SPI_freetuptable(my_tuptable);
        my_tuptable = _SPI_current->tuptable;
        my_res = res;
      }
      else
      {
        SPI_freetuptable(_SPI_current->tuptable);
        _SPI_current->tuptable = NULL;
      }
                                                                 
      if (res < 0)
      {
        my_res = res;
        goto fail;
      }
    }

                                                  
    ReleaseCachedPlan(cplan, plan->saved);
    cplan = NULL;

       
                                                                         
                                                                           
                                                        
       
    if (!read_only)
    {
      CommandCounterIncrement();
    }
  }

fail:

                                                       
  if (pushed_active_snap)
  {
    PopActiveSnapshot();
  }

                                                          
  if (cplan)
  {
    ReleaseCachedPlan(cplan, plan->saved);
  }

     
                                 
     
  error_context_stack = spierrcontext.previous;

                               
  SPI_processed = my_processed;
  SPI_tuptable = my_tuptable;

                                                          
  _SPI_current->tuptable = NULL;

     
                                                                             
                                                                             
                                    
     
  if (my_res == 0)
  {
    my_res = SPI_OK_REWRITTEN;
  }

  return my_res;
}

   
                                                                             
   
static ParamListInfo
_SPI_convert_params(int nargs, Oid *argtypes, Datum *Values, const char *Nulls)
{
  ParamListInfo paramLI;

  if (nargs > 0)
  {
    paramLI = makeParamList(nargs);

    for (int i = 0; i < nargs; i++)
    {
      ParamExternData *prm = &paramLI->params[i];

      prm->value = Values[i];
      prm->isnull = (Nulls && Nulls[i] == 'n');
      prm->pflags = PARAM_FLAG_CONST;
      prm->ptype = argtypes[i];
    }
  }
  else
  {
    paramLI = NULL;
  }
  return paramLI;
}

static int
_SPI_pquery(QueryDesc *queryDesc, bool fire_triggers, uint64 tcount)
{
  int operation = queryDesc->operation;
  int eflags;
  int res;

  switch (operation)
  {
  case CMD_SELECT:
    if (queryDesc->dest->mydest != DestSPI)
    {
                                                                 
      res = SPI_OK_UTILITY;
    }
    else
    {
      res = SPI_OK_SELECT;
    }
    break;
  case CMD_INSERT:
    if (queryDesc->plannedstmt->hasReturning)
    {
      res = SPI_OK_INSERT_RETURNING;
    }
    else
    {
      res = SPI_OK_INSERT;
    }
    break;
  case CMD_DELETE:
    if (queryDesc->plannedstmt->hasReturning)
    {
      res = SPI_OK_DELETE_RETURNING;
    }
    else
    {
      res = SPI_OK_DELETE;
    }
    break;
  case CMD_UPDATE:
    if (queryDesc->plannedstmt->hasReturning)
    {
      res = SPI_OK_UPDATE_RETURNING;
    }
    else
    {
      res = SPI_OK_UPDATE;
    }
    break;
  default:
    return SPI_ERROR_OPUNKNOWN;
  }

#ifdef SPI_EXECUTOR_STATS
  if (ShowExecutorStats)
  {
    ResetUsage();
  }
#endif

                                
  if (fire_triggers)
  {
    eflags = 0;                                      
  }
  else
  {
    eflags = EXEC_FLAG_SKIP_TRIGGERS;
  }

  ExecutorStart(queryDesc, eflags);

  ExecutorRun(queryDesc, ForwardScanDirection, tcount, true);

  _SPI_current->processed = queryDesc->estate->es_processed;

  if ((res == SPI_OK_SELECT || queryDesc->plannedstmt->hasReturning) && queryDesc->dest->mydest == DestSPI)
  {
    if (_SPI_checktuples())
    {
      elog(ERROR, "consistency check on SPI tuple count failed");
    }
  }

  ExecutorFinish(queryDesc);
  ExecutorEnd(queryDesc);
                                           

#ifdef SPI_EXECUTOR_STATS
  if (ShowExecutorStats)
  {
    ShowUsage("SPI EXECUTOR STATS");
  }
#endif

  return res;
}

   
                       
   
                                                              
   
static void
_SPI_error_callback(void *arg)
{
  const char *query = (const char *)arg;
  int syntaxerrposition;

  if (query == NULL)                                 
  {
    return;
  }

     
                                                                            
                                                           
     
  syntaxerrposition = geterrposition();
  if (syntaxerrposition > 0)
  {
    errposition(0);
    internalerrposition(syntaxerrposition);
    internalerrquery(query);
  }
  else
  {
    errcontext("SQL statement \"%s\"", query);
  }
}

   
                           
   
                                  
   
static void
_SPI_cursor_operation(Portal portal, FetchDirection direction, long count, DestReceiver *dest)
{
  uint64 nfetched;

                                      
  if (!PortalIsValid(portal))
  {
    elog(ERROR, "invalid portal in SPI cursor operation");
  }

                          
  if (_SPI_begin_call(true) < 0)
  {
    elog(ERROR, "SPI cursor operation called while not connected");
  }

                                                                       
  SPI_processed = 0;
  SPI_tuptable = NULL;
  _SPI_current->processed = 0;
  _SPI_current->tuptable = NULL;

                      
  nfetched = PortalRunFetch(portal, direction, count, dest);

     
                                                                          
                                                                            
                                                                         
                                                                           
                                                                             
                                                    
     
  _SPI_current->processed = nfetched;

  if (dest->mydest == DestSPI && _SPI_checktuples())
  {
    elog(ERROR, "consistency check on SPI tuple count failed");
  }

                                                      
  SPI_processed = _SPI_current->processed;
  SPI_tuptable = _SPI_current->tuptable;

                                                          
  _SPI_current->tuptable = NULL;

                         
  _SPI_end_call(true);
}

static MemoryContext
_SPI_execmem(void)
{
  return MemoryContextSwitchTo(_SPI_current->execCxt);
}

static MemoryContext
_SPI_procmem(void)
{
  return MemoryContextSwitchTo(_SPI_current->procCxt);
}

   
                                                                       
   
                                                                        
                                                                           
                                                                   
   
static int
_SPI_begin_call(bool use_exec)
{
  if (_SPI_current == NULL)
  {
    return SPI_ERROR_UNCONNECTED;
  }

  if (use_exec)
  {
                                                      
    _SPI_current->execSubid = GetCurrentSubTransactionId();
                                               
    _SPI_execmem();
  }

  return 0;
}

   
                                                                   
   
                                                                
   
                                                                            
   
static int
_SPI_end_call(bool use_exec)
{
  if (use_exec)
  {
                                                
    _SPI_procmem();
                                                
    _SPI_current->execSubid = InvalidSubTransactionId;
                                  
    MemoryContextResetAndDeleteChildren(_SPI_current->execCxt);
  }

  return 0;
}

static bool
_SPI_checktuples(void)
{
  uint64 processed = _SPI_current->processed;
  SPITupleTable *tuptable = _SPI_current->tuptable;
  bool failed = false;

  if (tuptable == NULL)                                      
  {
    failed = true;
  }
  else if (processed != (tuptable->alloced - tuptable->free))
  {
    failed = true;
  }

  return failed;
}

   
                                                         
   
                                                                            
                                                                            
                                                                          
                                                                           
                                                                           
   
static SPIPlanPtr
_SPI_make_plan_non_temp(SPIPlanPtr plan)
{
  SPIPlanPtr newplan;
  MemoryContext parentcxt = _SPI_current->procCxt;
  MemoryContext plancxt;
  MemoryContext oldcxt;
  ListCell *lc;

                                               
  Assert(plan->magic == _SPI_PLAN_MAGIC);
  Assert(plan->plancxt == NULL);
                                     
  Assert(!plan->oneshot);

     
                                                                             
                                                
     
  plancxt = AllocSetContextCreate(parentcxt, "SPI Plan", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(plancxt);

                                                                         
  newplan = (SPIPlanPtr)palloc0(sizeof(_SPI_plan));
  newplan->magic = _SPI_PLAN_MAGIC;
  newplan->plancxt = plancxt;
  newplan->cursor_options = plan->cursor_options;
  newplan->nargs = plan->nargs;
  if (plan->nargs > 0)
  {
    newplan->argtypes = (Oid *)palloc(plan->nargs * sizeof(Oid));
    memcpy(newplan->argtypes, plan->argtypes, plan->nargs * sizeof(Oid));
  }
  else
  {
    newplan->argtypes = NULL;
  }
  newplan->parserSetup = plan->parserSetup;
  newplan->parserSetupArg = plan->parserSetupArg;

     
                                                                        
                                                                             
                                                                             
                             
     
  foreach (lc, plan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc);

    CachedPlanSetParentContext(plansource, parentcxt);

                                                    
    newplan->plancache_list = lappend(newplan->plancache_list, plansource);
  }

  MemoryContextSwitchTo(oldcxt);

                                                                        
  plan->plancache_list = NIL;

  return newplan;
}

   
                                          
   
static SPIPlanPtr
_SPI_save_plan(SPIPlanPtr plan)
{
  SPIPlanPtr newplan;
  MemoryContext plancxt;
  MemoryContext oldcxt;
  ListCell *lc;

                                     
  Assert(!plan->oneshot);

     
                                                                           
                                                                       
                                                           
     
  plancxt = AllocSetContextCreate(CurrentMemoryContext, "SPI Plan", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(plancxt);

                                              
  newplan = (SPIPlanPtr)palloc0(sizeof(_SPI_plan));
  newplan->magic = _SPI_PLAN_MAGIC;
  newplan->plancxt = plancxt;
  newplan->cursor_options = plan->cursor_options;
  newplan->nargs = plan->nargs;
  if (plan->nargs > 0)
  {
    newplan->argtypes = (Oid *)palloc(plan->nargs * sizeof(Oid));
    memcpy(newplan->argtypes, plan->argtypes, plan->nargs * sizeof(Oid));
  }
  else
  {
    newplan->argtypes = NULL;
  }
  newplan->parserSetup = plan->parserSetup;
  newplan->parserSetupArg = plan->parserSetupArg;

                                      
  foreach (lc, plan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc);
    CachedPlanSource *newsource;

    newsource = CopyCachedPlan(plansource);
    newplan->plancache_list = lappend(newplan->plancache_list, newsource);
  }

  MemoryContextSwitchTo(oldcxt);

     
                                                                           
                                                                      
                                                                      
     
  newplan->saved = true;
  MemoryContextSetParent(newplan->plancxt, CacheMemoryContext);

  foreach (lc, newplan->plancache_list)
  {
    CachedPlanSource *plansource = (CachedPlanSource *)lfirst(lc);

    SaveCachedPlan(plansource);
  }

  return newplan;
}

   
                                                        
   
static EphemeralNamedRelation
_SPI_find_ENR_by_name(const char *name)
{
                                                                
  Assert(name != NULL);

                                                   
  if (_SPI_current->queryEnv == NULL)
  {
    return NULL;
  }

  return get_ENR(_SPI_current->queryEnv, name);
}

   
                                                                               
                                               
   
int
SPI_register_relation(EphemeralNamedRelation enr)
{
  EphemeralNamedRelation match;
  int res;

  if (enr == NULL || enr->md.name == NULL)
  {
    return SPI_ERROR_ARGUMENT;
  }

  res = _SPI_begin_call(false);                                  
  if (res < 0)
  {
    return res;
  }

  match = _SPI_find_ENR_by_name(enr->md.name);
  if (match)
  {
    res = SPI_ERROR_REL_DUPLICATE;
  }
  else
  {
    if (_SPI_current->queryEnv == NULL)
    {
      _SPI_current->queryEnv = create_queryEnv();
    }

    register_ENR(_SPI_current->queryEnv, enr);
    res = SPI_OK_REL_REGISTER;
  }

  _SPI_end_call(false);

  return res;
}

   
                                                                            
                                                                       
   
int
SPI_unregister_relation(const char *name)
{
  EphemeralNamedRelation match;
  int res;

  if (name == NULL)
  {
    return SPI_ERROR_ARGUMENT;
  }

  res = _SPI_begin_call(false);                                  
  if (res < 0)
  {
    return res;
  }

  match = _SPI_find_ENR_by_name(name);
  if (match)
  {
    unregister_ENR(_SPI_current->queryEnv, match->md.name);
    res = SPI_OK_REL_UNREGISTER;
  }
  else
  {
    res = SPI_ERROR_REL_NOT_FOUND;
  }

  _SPI_end_call(false);

  return res;
}

   
                                                                            
                                                                       
                                                                             
                       
   
int
SPI_register_trigger_data(TriggerData *tdata)
{
  if (tdata == NULL)
  {
    return SPI_ERROR_ARGUMENT;
  }

  if (tdata->tg_newtable)
  {
    EphemeralNamedRelation enr = palloc(sizeof(EphemeralNamedRelationData));
    int rc;

    enr->md.name = tdata->tg_trigger->tgnewtable;
    enr->md.reliddesc = tdata->tg_relation->rd_id;
    enr->md.tupdesc = NULL;
    enr->md.enrtype = ENR_NAMED_TUPLESTORE;
    enr->md.enrtuples = tuplestore_tuple_count(tdata->tg_newtable);
    enr->reldata = tdata->tg_newtable;
    rc = SPI_register_relation(enr);
    if (rc != SPI_OK_REL_REGISTER)
    {
      return rc;
    }
  }

  if (tdata->tg_oldtable)
  {
    EphemeralNamedRelation enr = palloc(sizeof(EphemeralNamedRelationData));
    int rc;

    enr->md.name = tdata->tg_trigger->tgoldtable;
    enr->md.reliddesc = tdata->tg_relation->rd_id;
    enr->md.tupdesc = NULL;
    enr->md.enrtype = ENR_NAMED_TUPLESTORE;
    enr->md.enrtuples = tuplestore_tuple_count(tdata->tg_oldtable);
    enr->reldata = tdata->tg_oldtable;
    rc = SPI_register_relation(enr);
    if (rc != SPI_OK_REL_REGISTER)
    {
      return rc;
    }
  }

  return SPI_OK_TD_REGISTER;
}
